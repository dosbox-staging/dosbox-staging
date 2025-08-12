// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CLAP_LIBRARY_H
#define DOSBOX_CLAP_LIBRARY_H

#include <memory>
#include <string>
#include <vector>

#include "clap/all.h"

#include "utils/dynlib.h"
#include "misc/std_filesystem.h"

namespace Clap {

struct PluginInfo {
	std_fs::path library_path = {};

	std::string id          = {};
	std::string name        = {};
	std::string description = {};
	std::string version     = {};
};

// Encapsulates a dynamically-loaded CLAP library and manages its lifecycle.
// CLAP libraries are uniquely identified by their filesystem paths.
//
class Library {
public:
	// Loads and initialises a CLAP plugin. `library_path` must be:
	//
	// - On Windows: the path of the `.clap` file (renamed DLL file)
	//
	// - On Linux: the path of the `.clap`  file (renamed .so file)
	//
	// - On macOS: the path of the application bundle. The actual
	// dynamic-link library (.dynlib file) is in `Contents/MacOS` within the
	// bundle. Passing in
	//
	// Throws `std::runtime_error` if the library cannot be loaded and
	// initialiased.
	//
	Library(const std_fs::path& library_path);

	// Deinits the library and unloads the dynamic-link library
	~Library();

	// Returns the path passed into the constructor
	std_fs::path GetPath() const;

	// Get a list of all plugins in the library
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
