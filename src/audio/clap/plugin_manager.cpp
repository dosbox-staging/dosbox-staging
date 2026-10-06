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

#include "misc/cross.h"
#include "misc/logging.h"
#include "misc/support.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

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
		LOG_DEBUG("CLAP: Enumerating CLAP plugins in '%s'",
		          dir.string().c_str());
		for (const auto& name :
		     get_directory_entries(dir, ".clap", OnlyRegularFiles)) {
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

static std::optional<PluginInfo> find_plugin(const std::vector<PluginInfo>& plugin_infos,
                                             const std::string& library_name,
                                             const std::string& plugin_name_or_index)
{
	std::optional<uint32_t> plugin_index = {};
	if (!plugin_name_or_index.empty() && is_digits(plugin_name_or_index)) {
		const auto index = parse_int(plugin_name_or_index);
		if (!index) {
			return {};
		}
		plugin_index = check_cast<uint32_t>(*index);
	}

	for (const auto& info : plugin_infos) {
		if (!library_name.empty() &&
		    !find_in_case_insensitive(library_name,
		                              info.library_path.filename().string())) {
			continue;
		}
		if (plugin_index) {
			if (info.index != *plugin_index) {
				continue;
			}
		} else if (!plugin_name_or_index.empty() &&
		           !find_in_case_insensitive(plugin_name_or_index, info.name)) {
			continue;
		}
		return info;
	}
	return {};
}

std::optional<PluginInfo> PluginManager::FindPlugin(const std::string& library_name,
                                                    const std::string& plugin_name_or_index)
{
	return find_plugin(GetPluginInfos(), library_name, plugin_name_or_index);
}

static bool validate_note_ports(const clap_plugin_t* plugin)
{
	const auto note_ports = static_cast<const clap_plugin_note_ports_t*>(
	        plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS));

	if (!note_ports) {
		LOG_DEBUG("CLAP: Only plugins that implement the note ports extension are supported");
		return false;
	}

	constexpr auto InputPort = true;

	const auto num_in_ports = note_ports->count(plugin, InputPort);
	if (num_in_ports == 0) {
		LOG_DEBUG("CLAP: Plugins must have at least one MIDI input port");
		return false;
	}

	clap_note_port_info info = {};
	const auto PortIndex     = 0;
	if (!note_ports->get(plugin, PortIndex, InputPort, &info)) {
		LOG_DEBUG("CLAP: Cannot get the first note input port");
		return false;
	}

	if ((info.supported_dialects & CLAP_NOTE_DIALECT_MIDI) == 0) {
		LOG_DEBUG("CLAP: The first note input port must support the MIDI dialect");
		return false;
	}

	return true;
}

static bool validate_audio_ports(const clap_plugin_t* plugin,
                                 std::vector<uint32_t>& input_channel_counts,
                                 std::vector<uint32_t>& output_channel_counts)
{
	const auto audio_ports = static_cast<const clap_plugin_audio_ports_t*>(
	        plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));

	if (!audio_ports) {
		LOG_DEBUG("CLAP: Only plugins that implement the audio ports extension are supported");
		return false;
	}

	constexpr auto InputPort  = true;
	constexpr auto OutputPort = false;

	const auto scan_ports = [&](const bool is_input,
	                            std::vector<uint32_t>& channel_counts) {
		const auto num_ports = audio_ports->count(plugin, is_input);
		if (!is_input && num_ports == 0) {
			LOG_DEBUG("CLAP: Plugins must have at least one audio output port");
			return false;
		}

		channel_counts.clear();
		channel_counts.reserve(num_ports);
		for (uint32_t i = 0; i < num_ports; ++i) {
			clap_audio_port_info port = {};
			if (!audio_ports->get(plugin, i, is_input, &port) ||
			    port.channel_count == 0) {
				LOG_DEBUG("CLAP: Invalid audio %s port %u",
				          is_input ? "input" : "output",
				          i);
				return false;
			}
			if (!is_input && i == 0 &&
			    !(port.channel_count == 2 && port.port_type &&
			      strcmp(port.port_type, CLAP_PORT_STEREO) == 0)) {
				LOG_DEBUG("CLAP: The first audio output port must be stereo with two channels");
				return false;
			}
			channel_counts.push_back(port.channel_count);
		}
		return true;
	};

	return scan_ports(InputPort, input_channel_counts) &&
	       scan_ports(OutputPort, output_channel_counts);
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
	const auto destroy_plugin = [](const clap_plugin_t* instance) {
		instance->destroy(instance);
	};
	std::unique_ptr<const clap_plugin_t, decltype(destroy_plugin)> plugin_guard(
	        plugin, destroy_plugin);

	if (!plugin->init(plugin)) {
		LOG_DEBUG("CLAP: Error initialising plugin with ID '%s' from library '%s'",
		          plugin_info.id.c_str(),
		          plugin_info.library_path.string().c_str());
		return {};
	}

	if (!validate_note_ports(plugin)) {
		return {};
	}
	std::vector<uint32_t> input_channel_counts  = {};
	std::vector<uint32_t> output_channel_counts = {};
	if (!validate_audio_ports(plugin, input_channel_counts, output_channel_counts)) {
		return {};
	}

	LOG_INFO("CLAP: Plugin '%s' loaded (version %s)",
	         plugin_info.name.c_str(),
	         plugin_info.version.c_str());

	auto instance = std::make_unique<Plugin>(library,
	                                         plugin,
	                                         input_channel_counts,
	                                         output_channel_counts);
	// The wrapper now owns the plugin; disarm the temporary guard.
	(void)plugin_guard.release();
	return instance;
}

} // namespace Clap
