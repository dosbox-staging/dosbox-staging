// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// The MSVC min/max macro imported by clap/all.h collides with
// std::numeric_limits which makes the MSCV Debug x64 build fail.
// Defining NOMINMAX is the workaround recommended by the MSCV team here:
// https://github.com/microsoft/GSL/issues/816
//
#define NOMINMAX 1

#include "plugin_manager.h"

#include <cassert>
#include <optional>

#include "clap/all.h"

#include "checks.h"
#include "cross.h"
#include "logging.h"
#include "support.h"

CHECK_NARROWING();

namespace Clap {

static const clap_host_t dosbox_clap_host = {
        .clap_version = CLAP_VERSION,

        .host_data = nullptr,

        .name    = "DOSBox Staging",
        .vendor  = "The DOSBox Staging Team",
        .url     = "http://www.dosbox-staging.org",
        .version = "1.0",

        .get_extension = []([[maybe_unused]] const clap_host_t* host,
                            [[maybe_unused]] const char* extension_id) -> const void* {
	        return nullptr;
        },

        .request_restart  = []([[maybe_unused]] const clap_host_t* host) {},
        .request_process  = []([[maybe_unused]] const clap_host_t* host) {},
        .request_callback = []([[maybe_unused]] const clap_host_t* host) {}};

void PluginManager::EnumeratePlugins()
{
	plugin_info_cache.clear();

	constexpr auto OnlyRegularFiles = false;

	for (const auto& dir : get_plugin_paths()) {
		LOG_DEBUG("CLAP: Enumerating CLAP plugins in '%s'", dir.string().c_str());
		for (const auto& name : get_directory_entries(dir, ".clap", OnlyRegularFiles)) {
			auto library_path = dir / name;

			LOG_DEBUG("CLAP: Trying to load plugin library '%s'",
			          library_path.string().c_str());

			const auto library = GetOrLoadLibrary(library_path);
			if (!library) {
				continue;
			}

			const auto pi = library->GetPluginInfos();

			plugin_info_cache.insert(plugin_info_cache.end(),
			                         pi.begin(),
			                         pi.end());
		}
	}
}

std::vector<PluginInfo> PluginManager::GetPluginInfos()
{
	if (!plugins_enumerated) {
		EnumeratePlugins();
		plugins_enumerated = true;
	}

	return plugin_info_cache;
}

static bool validate_note_ports(const clap_plugin_t* plugin)
{
	const auto note_ports = static_cast<const clap_plugin_note_ports_t*>(
	        plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS));

	if (!note_ports) {
		LOG_DEBUG("CLAP: Only plugins that implement the note ports extension are supported");
		return false;
	}

	constexpr auto InputPort  = true;
	constexpr auto OutputPort = false;

	const auto num_in_ports = note_ports->count(plugin, InputPort);
	if (num_in_ports != 1) {
		LOG_DEBUG("CLAP: Only plugins with a single MIDI input ports are supported");
		return false;
	}

	const auto num_out_ports = note_ports->count(plugin, OutputPort);
	if (num_out_ports != 0) {
		LOG_DEBUG("CLAP: Only plugins with no MIDI output ports are supported");
		return false;
	}

	clap_note_port_info info = {};
	const auto PortIndex     = 0;
	note_ports->get(plugin, PortIndex, InputPort, &info);

	if ((info.supported_dialects & CLAP_NOTE_DIALECT_MIDI) == 0) {
		LOG_DEBUG("CLAP: Only plugins with MIDI dialect support are supported");
		return false;
	}

	return true;
}

static bool validate_audio_ports(const clap_plugin_t* plugin)
{
	const auto audio_ports = static_cast<const clap_plugin_audio_ports_t*>(
	        plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));

	if (!audio_ports) {
		LOG_DEBUG("CLAP: Only plugins that implement the audio ports extension are supported");
		return false;
	}

	constexpr auto InputPort  = true;
	constexpr auto OutputPort = false;

	const auto num_in_ports = audio_ports->count(plugin, InputPort);
	if (num_in_ports != 0) {
		LOG_DEBUG("CLAP: Only instrument plugins with no audio input ports are supported");
		return false;
	}

	const auto num_out_ports = audio_ports->count(plugin, OutputPort);
	if (num_out_ports != 1) {
		LOG_DEBUG("CLAP: Only plugins with a single audio output port are supported");
		return false;
	}

	clap_audio_port_info info = {};
	const auto PortIndex      = 0;
	audio_ports->get(plugin, PortIndex, OutputPort, &info);

	if (!(info.channel_count == 2 &&
	      strcmp(info.port_type, CLAP_PORT_STEREO) == 0)) {
		LOG_DEBUG("CLAP: Only stereo plugins with two audio output channels are supported");
		return false;
	}

	return true;
}

std::shared_ptr<Library> PluginManager::GetOrLoadLibrary(const std_fs::path& library_path)
{
	// CLAP libraries are uniquely identified by their filesystem paths
	const auto it = std::find_if(library_cache.cbegin(),
	                             library_cache.cend(),
	                             [&](const auto lib) {
		                             if (auto l = lib.lock()) {
			                             return l->GetPath() ==
			                                    library_path;
		                             }
		                             return false;
	                             });

	if (it != library_cache.end()) {
		// Library found in the cache (meaning a plugin instance holds a
		// shared_ptr to it).

		// Create a shared_ptr & return it
		return it->lock();

	} else {
		// Library not found in the cache; we'll need to load it and
		// store a weak_ptr in the cache.
		try {
			const auto lib = std::make_shared<Library>(library_path);
			library_cache.emplace_back(lib);
			return lib;

		} catch (const std::runtime_error& ex) {
			return nullptr;
		}
	}
}

std::unique_ptr<Plugin> PluginManager::LoadPlugin(const PluginInfo& plugin_info)
{
	LOG_DEBUG("CLAP: Loading plugin with ID '%s' from library '%s'",
	          plugin_info.id.c_str(),
	          plugin_info.library_path.c_str());

	const auto library = GetOrLoadLibrary(plugin_info.library_path);
	if (!library) {
		return {};
	}

	auto factory = static_cast<const clap_plugin_factory*>(
	        library->GetPluginEntry()->get_factory(CLAP_PLUGIN_FACTORY_ID));
	assert(factory);

	const clap_plugin_t* plugin = factory->create_plugin(factory,
	                                                     &dosbox_clap_host,
	                                                     plugin_info.id.c_str());
	if (!plugin) {
		LOG_ERR("CLAP: Error creating plugin with ID '%s' from library '%s'",
		        plugin_info.id.c_str(),
		        plugin_info.library_path.string().c_str());
		return {};
	}
	if (!plugin->init(plugin)) {
		LOG_DEBUG("CLAP: Error initialising plugin with ID '%s' from library '%s'",
		          plugin_info.id.c_str(),
		          plugin_info.library_path.string().c_str());
		return {};
	}

	if (!validate_note_ports(plugin)) {
		return {};
	}
	if (!validate_audio_ports(plugin)) {
		return {};
	}

	LOG_INFO("CLAP: Plugin '%s' loaded (version %s)",
	         plugin_info.name.c_str(),
	         plugin_info.version.c_str());

	return std::make_unique<Plugin>(library, plugin);
}

} // namespace Clap
