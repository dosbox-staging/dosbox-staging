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

#ifndef DOSBOX_CLAP_PLUGIN_MANAGER_H
#define DOSBOX_CLAP_PLUGIN_MANAGER_H

#include <string>
#include <vector>

#include "dynlib.h"
#include "plugin.h"
#include "std_filesystem.h"

namespace Clap {

struct PluginInfo {
	std_fs::path library_path = {};

	std::string id          = {};
	std::string name        = {};
	std::string description = {};
};

// Plugin manager to discover and load (instantiate) CLAP plugins.
//
// This is a singleton that only enumerates the list of available plugins once
// during the lifecycle of the program.
//
class PluginManager {
public:
	static PluginManager& GetInstance()
	{
		static PluginManager instance;
		return instance;
	}

	std::vector<PluginInfo> GetPluginInfos();

	// Use the `path` and `id` from `PluginInfo` retrieved via
	// `GetPluginInfos()`.
	std::unique_ptr<Plugin> LoadPlugin(const std_fs::path& library_path,
	                                   const std::string& plugin_id) const;

private:
	PluginManager()  = default;
	~PluginManager() = default;

	// prevent copying
	PluginManager(const PluginManager&) = delete;
	// prevent assignment
	PluginManager& operator=(const PluginManager&) = delete;

	void EnumeratePlugins();

	std::vector<PluginInfo> plugin_infos = {};

	bool plugins_enumerated = false;
};

} // namespace Clap

#endif // DOSBOX_CLAP_PLUGIN_MANAGER_H
