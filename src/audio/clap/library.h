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

#ifndef DOSBOX_CLAP_LIBRARY_H
#define DOSBOX_CLAP_LIBRARY_H

#include <memory>
#include <string>
#include <vector>

#include "clap/all.h"

#include "dynlib.h"
#include "std_filesystem.h"

namespace Clap {

struct PluginInfo {
	std_fs::path library_path = {};

	std::string id          = {};
	std::string name        = {};
	std::string description = {};
};

// Encapsulates and manages the lifecycle of a CLAP library. CLAP libraries
// are uniquely identified by their filesystem paths.
class Library {
public:
	Library(const std_fs::path& library_path);
	~Library();

	// Returns the path passed into the constructor
	std_fs::path GetPath() const;

	std::vector<PluginInfo> GetPluginInfos() const;

	const clap_plugin_entry_t* GetPluginEntry() const;

	// prevent copying
	Library(const Library&) = delete;
	// prevent assignment
	Library& operator=(const Library&) = delete;

private:
	std_fs::path library_path = {};
	dynlib_handle lib_handle  = nullptr;

	const clap_plugin_entry_t* plugin_entry = nullptr;
};

} // namespace Clap

#endif // DOSBOX_CLAP_LIBRARY_H
