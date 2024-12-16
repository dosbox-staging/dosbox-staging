/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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

#include "cross.h"
#include "logging.h"
#include "support.h"

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

[[maybe_unused]] static std::optional<std_fs::path> find_first_file(const std_fs::path& path)
{
	std::vector<std_fs::path> files = {};

	std::error_code ec;
	for (const auto& entry : std_fs::directory_iterator(path, ec)) {
		if (ec) {
			return {};
		}
		if (entry.is_regular_file(ec)) {
			files.emplace_back(entry.path());
		}
	}

	if (files.size() == 0) {
		return {};
	}

	std::sort(files.begin(), files.end());
	return files.front();
}

static std::pair<dynlib_handle, const clap_plugin_entry_t*> load_plugin_library(
        const std_fs::path& _library_path)
{
	const auto reported_plugin_path = _library_path;

#ifdef MACOSX
	// The dynamic-link library is usually inside the application bundle on
	// macOS, so we need to resolve its path, but we must always report the
	// bundle path to the plugin instance.
	const auto library_path = [&] {
		if (const auto f = find_first_file(_library_path / "Contents" / "MacOS");
		    f) {
			return *f;
		}
		return _library_path;
	}();

#else // Windows, Linux

	// We use the path of the dynamic-link library directly on Windows and
	// Linux.
	const auto library_path = _library_path;
#endif

	const auto lib = dynlib_open(library_path);
	if (!lib) {
		LOG_ERR("CLAP: Error loading plugin library '%s'",
		        library_path.string().c_str());
		return {};
	}

	const auto plugin_entry = reinterpret_cast<const clap_plugin_entry_t*>(
	        dynlib_get_symbol(lib, "clap_entry"));

	if (!plugin_entry) {
		dynlib_close(lib);
		return {};
	}

	plugin_entry->init(reported_plugin_path.string().c_str());
	return {lib, plugin_entry};
}

static std::vector<PluginInfo> get_plugin_infos(const std_fs::path& path,
                                                const clap_plugin_entry_t* plugin_entry)
{
	const auto factory = static_cast<const clap_plugin_factory*>(
	        plugin_entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
	assert(factory);

	const auto num_plugins = factory->get_plugin_count(factory);
	std::vector<PluginInfo> plugin_infos = {};

	for (size_t plugin_index = 0; plugin_index < num_plugins; ++plugin_index) {
		const auto desc = factory->get_plugin_descriptor(factory,
		                                                 plugin_index);
		if (desc) {
			LOG_INFO("CLAP: Found plugin '%s'", desc->name);

			const PluginInfo info = {path,
			                         desc->id,
			                         desc->name,
			                         desc->description};

			plugin_infos.emplace_back(info);
		}
	}

	return plugin_infos;
}

void PluginManager::EnumeratePlugins()
{
	plugin_infos.clear();

	constexpr auto OnlyRegularFiles = false;

	for (const auto& [dir, plugin_names] :
	     get_files_in_resource(PluginsDir, ".clap", OnlyRegularFiles)) {

		LOG_DEBUG("CLAP: Enumerating CLAP plugins in '%s'",
		          dir.string().c_str());

		for (const auto& name : plugin_names) {
			auto library_path = dir / name;

			LOG_DEBUG("CLAP: Trying to load plugin library '%s'",
			          library_path.string().c_str());

			const auto [_, plugin_entry] = load_plugin_library(library_path);

			if (!plugin_entry) {
				LOG_WARNING("CLAP: Invalid plugin library '%s'",
				            library_path.string().c_str());
				continue;
			}
			const auto pi = get_plugin_infos(library_path, plugin_entry);

			plugin_infos.insert(plugin_infos.end(), pi.begin(), pi.end());
		}
	}

	plugins_enumerated = true;
}

std::vector<PluginInfo> PluginManager::GetPluginInfos()
{
	if (!plugins_enumerated) {
		EnumeratePlugins();
	}
	return plugin_infos;
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

std::unique_ptr<Plugin> PluginManager::LoadPlugin(const std_fs::path& library_path,
                                                  const std::string& plugin_id) const
{
	LOG_DEBUG("CLAP: Loading plugin with ID '%s' from library '%s'",
	          plugin_id.c_str(),
	          library_path.c_str());

	const auto [lib, plugin_entry] = load_plugin_library(library_path);

	auto factory = static_cast<const clap_plugin_factory*>(
	        plugin_entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
	assert(factory);

	const clap_plugin_t* plugin = factory->create_plugin(factory,
	                                                     &dosbox_clap_host,
	                                                     plugin_id.c_str());
	if (!plugin) {
		LOG_ERR("CLAP: Error creating plugin with ID '%s' from library '%s'",
		        plugin_id.c_str(),
		        library_path.string().c_str());
		return {};
	}
	if (!plugin->init(plugin)) {
		LOG_DEBUG("CLAP: Error initialising plugin with ID '%s' from library '%s'",
		          plugin_id.c_str(),
		          library_path.string().c_str());
		return {};
	}

	if (!validate_note_ports(plugin)) {
		return {};
	}
	if (!validate_audio_ports(plugin)) {
		return {};
	}

	return std::make_unique<Plugin>(lib, plugin_entry, plugin);
}

} // namespace Clap
