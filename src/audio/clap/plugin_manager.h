// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CLAP_PLUGIN_MANAGER_H
#define DOSBOX_CLAP_PLUGIN_MANAGER_H

#include <memory>
#include <string>
#include <vector>

#include "library.h"
#include "plugin.h"
#include "misc/std_filesystem.h"

namespace Clap {

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
	// lifecycle of the program, then it returns the cached results.
	//
	// Only supported plugins having a single MIDI input port and a single
	// stereo audio output port and are enumerated.
	//
	std::vector<PluginInfo> GetPluginInfos();

	// Loads and initialises a CLAP plugin.
	std::unique_ptr<Plugin> LoadPlugin(const PluginInfo& plugin_info);

private:
	PluginManager()  = default;
	~PluginManager() = default;

	// prevent copying
	PluginManager(const PluginManager&) = delete;
	// prevent assignment
	PluginManager& operator=(const PluginManager&) = delete;

	// Plugin enumeration
	void EnumeratePlugins();

	std::vector<PluginInfo> plugin_info_cache = {};
	bool plugins_enumerated                   = false;

	// Library handling
	std::shared_ptr<Library> GetOrLoadLibrary(const std_fs::path& library_path);

	// Plugin instances hold shared_ptrs to the loaded library instances.
	// We need to look up the library in the cache first whenever we
	// instantiate a new plugin.
	std::vector<std::weak_ptr<Library>> library_cache = {};
};

} // namespace Clap

#endif // DOSBOX_CLAP_PLUGIN_MANAGER_H
