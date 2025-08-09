// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "library.h"

#include <algorithm>
#include <cassert>

#include "clap/all.h"

#include "util/checks.h"
#include "logging.h"
#include "util/string_utils.h"
#include "support.h"

CHECK_NARROWING();

namespace Clap {

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

	if (files.empty()) {
		return {};
	}

	std::sort(files.begin(), files.end());
	return files.front();
}

Library::Library(const std_fs::path& _library_path)
        : library_path(_library_path)
{
	const auto reported_plugin_path = library_path;

#ifdef MACOSX
	// The dynamic-link library is inside the application bundle on macOS, so
	// we need to resolve its path, but we must always report the bundle path
	// to the plugin instance.
	const auto dynlib_path = [&] {
		if (const auto f = find_first_file(_library_path / "Contents" / "MacOS");
		    f) {
			return *f;
		}
		return _library_path;
	}();

#else // Windows, Linux

	// We use the path of the dynamic-link library directly on Windows and
	// Linux.
	const auto dynlib_path = library_path;
#endif

	lib_handle = dynlib_open(dynlib_path);
	if (!lib_handle) {
		const auto msg = format_str("CLAP: Error loading plugin library '%s'",
		                            dynlib_path.string().c_str());
		LOG_ERR("%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	plugin_entry = reinterpret_cast<const clap_plugin_entry_t*>(
	        dynlib_get_symbol(lib_handle, "clap_entry"));

	if (!plugin_entry) {
		dynlib_close(lib_handle);

		const auto msg = format_str(
		        "CLAP: Invalid plugin library '%s', "
		        "cannot find 'clap_entry' symbol",
		        dynlib_path.string().c_str());

		LOG_ERR("%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	// Global library init
	plugin_entry->init(reported_plugin_path.string().c_str());
}

Library::~Library()
{
	// Global library deinit
	assert(plugin_entry);
	plugin_entry->deinit();

	// Unload library from memory
	assert(lib_handle);
	dynlib_close(lib_handle);
}

std_fs::path Library::GetPath() const
{
	assert(!library_path.empty());
	return library_path;
}

const clap_plugin_entry_t* Library::GetPluginEntry() const
{
	assert(plugin_entry);
	return plugin_entry;
}

std::vector<PluginInfo> Library::GetPluginInfos() const
{
	assert(plugin_entry);

	const auto factory = static_cast<const clap_plugin_factory*>(
	        plugin_entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
	assert(factory);

	const auto num_plugins = factory->get_plugin_count(factory);
	std::vector<PluginInfo> plugin_infos = {};

	for (size_t plugin_index = 0; plugin_index < num_plugins; ++plugin_index) {
		const auto desc = factory->get_plugin_descriptor(
		        factory, check_cast<uint32_t>(plugin_index));

		if (desc) {
			LOG_INFO("CLAP: Found plugin '%s'", desc->name);

			const PluginInfo info = {library_path,
			                         desc->id,
			                         desc->name,
			                         desc->description,
			                         desc->version};

			plugin_infos.emplace_back(info);
		}
	}

	return plugin_infos;
}

} // namespace Clap
