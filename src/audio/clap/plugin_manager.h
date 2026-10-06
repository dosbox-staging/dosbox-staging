// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CLAP_PLUGIN_MANAGER_H
#define DOSBOX_CLAP_PLUGIN_MANAGER_H

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "library.h"
#include "misc/std_filesystem.h"
#include "plugin.h"

namespace Clap {

enum class PluginFilter { All, GenericSynthOnly };

// CLAP plugin manager singleton to discover and load (instantiate) CLAP
// libraries and plugins. Technically, it implements some parts of a very
// basic CLAP host.
//
class PluginManager {
public:
	static PluginManager& GetInstance()
	{
		static PluginManager instance;
		return instance;
	}

	// Enumerates the list of available plugins only once during the
	// lifecycle of the program, then returns a copy of the cached results.
	// GenericSynthOnly excludes plugins handled by a dedicated MIDI synth.
	std::vector<PluginInfo> GetPluginInfos(PluginFilter filter = PluginFilter::All);

	// Returns the first plugin in GetPluginInfos(filter) that matches the
	// supplied library and plugin selectors. Returns no value if no plugin
	// matches, both selectors are empty, an index is supplied without a
	// library name, or the index is out of range.
	//
	// wanted_library_name uses case-insensitive partial matching against
	// the library filename. The .clap extension can be omitted. An empty
	// library name searches all libraries by plugin name.
	//
	// wanted_plugin_name_or_index uses case-insensitive partial matching
	// against the plugin name. An all-digit string is always treated as the
	// zero-based descriptor index within a library; for example, "1"
	// selects the second plugin descriptor. An index requires a non-empty
	// wanted_library_name. An empty plugin selector matches the first
	// plugin in a matching library.
	std::optional<PluginInfo> FindPlugin(
	        const std::string& wanted_library_name         = {},
	        const std::string& wanted_plugin_name_or_index = {},
	        PluginFilter filter = PluginFilter::All);

	// Like FindPlugin(), but wanted_library_name matches the complete,
	// case-sensitive library filename, including .clap. Plugin names still
	// use case-insensitive partial matching, and indices behave as above.
	// Requires a non-empty library filename. Only matching libraries are
	// discovered; their plugin metadata is cached independently.
	std::optional<PluginInfo> FindPluginInLibrary(
	        const std::string& wanted_library_name,
	        const std::string& wanted_plugin_name_or_index = {});

	// Loads and initialises a CLAP plugin. The first note input port must
	// support MIDI and the first audio output port must be stereo.
	std::unique_ptr<Plugin> LoadPlugin(const PluginInfo& plugin_info);

private:
	PluginManager()  = default;
	~PluginManager() = default;

	// prevent copying
	PluginManager(const PluginManager&) = delete;
	// prevent assignment
	PluginManager& operator=(const PluginManager&) = delete;

	// Plugin enumeration
	std::vector<PluginInfo> EnumeratePlugins(
	        const std::string& wanted_library_name = {});

	std::vector<PluginInfo> plugin_info_cache = {};
	bool plugins_enumerated                   = false;

	std::unordered_map<std::string, std::vector<PluginInfo>> library_plugin_info_cache = {};

	// Library handling
	std::shared_ptr<Library> GetOrLoadLibrary(const std_fs::path& library_path);

	// Plugin instances hold shared_ptrs to the loaded library instances.
	// We need to look up the library in the cache first whenever we
	// instantiate a new plugin.
	std::vector<std::weak_ptr<Library>> library_cache = {};
};

} // namespace Clap

#endif // DOSBOX_CLAP_PLUGIN_MANAGER_H
