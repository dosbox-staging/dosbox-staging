// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CLAP_LIBRARY_H
#define DOSBOX_CLAP_LIBRARY_H

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "clap/all.h"

#include "misc/std_filesystem.h"
#include "utils/dynlib.h"

namespace Clap {

struct PluginInfo {
	std_fs::path library_path = {};

	// The descriptor index within the library of the plugin to load.
	uint32_t index = 0;

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
	// Loads and initialises a CLAP plugin library. `library_path` must be:
	//
	// - On Windows: the path of the `.clap` file (renamed DLL file)
	//
	// - On Linux: the path of the `.clap` file (renamed .so file)
	//
	// - On macOS: the path of the application bundle. The dynamic-link
	//   library is in `Contents/MacOS` within the bundle.
	//
	// Throws `std::runtime_error` if the library cannot be loaded or
	// initialised, or its CLAP API version is incompatible.
	//
	Library(const std_fs::path& library_path);

	// Deinits the library and unloads the dynamic-link library
	~Library();

	// Returns the path passed into the constructor
	std_fs::path GetPath() const;

	// Returns compatible plugins declaring the instrument, synthesizer,
	// and stereo features, with non-empty IDs and names. Missing
	// descriptions and versions become empty strings. Returns an empty
	// list if the library does not provide a plugin factory.
	std::vector<PluginInfo> GetPluginInfos() const;

	// Returns the plugin factory, or nullptr if the library provides none.
	const clap_plugin_factory_t* GetPluginFactory() const;

	// prevent copying
	Library(const Library&) = delete;
	// prevent assignment
	Library& operator=(const Library&) = delete;

private:
	using LibraryHandle = std::unique_ptr<std::remove_pointer_t<dynlib_handle>,
	                                      decltype(&dynlib_close)>;

	std_fs::path library_path = {};
	LibraryHandle lib_handle  = {nullptr, dynlib_close};

	const clap_plugin_entry_t* plugin_entry = nullptr;
};

} // namespace Clap

#endif // DOSBOX_CLAP_LIBRARY_H
