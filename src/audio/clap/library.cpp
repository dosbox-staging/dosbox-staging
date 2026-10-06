// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "library.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string_view>

#include "clap/all.h"

#include "misc/logging.h"
#include "misc/support.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

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
	// The dynamic-link library is inside the application bundle on macOS,
	// so we need to resolve its path, but we must always report the bundle
	// path to the plugin instance.
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

	lib_handle.reset(dynlib_open(dynlib_path));
	if (!lib_handle) {
		const auto msg = format_str("CLAP: Error loading plugin library '%s'",
		                            dynlib_path.string().c_str());
		LOG_ERR("%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	plugin_entry = reinterpret_cast<const clap_plugin_entry_t*>(
	        dynlib_get_symbol(lib_handle.get(), "clap_entry"));

	if (!plugin_entry) {
		const auto msg = format_str(
		        "CLAP: Invalid plugin library '%s', "
		        "cannot find 'clap_entry' symbol",
		        dynlib_path.string().c_str());

		LOG_ERR("%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	if (!clap_version_is_compatible(plugin_entry->clap_version)) {
		const auto version = plugin_entry->clap_version;
		const auto msg = format_str("CLAP: Incompatible API version %u.%u.%u in '%s'",
		                            version.major,
		                            version.minor,
		                            version.revision,
		                            library_path.string().c_str());

		LOG_ERR("%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	// A failed init forbids further CLAP calls, including deinit().
	if (!plugin_entry->init(reported_plugin_path.string().c_str())) {
		const auto msg = format_str("CLAP: Failed to initialise plugin library '%s'",
		                            library_path.string().c_str());

		LOG_ERR("%s", msg.c_str());
		throw std::runtime_error(msg);
	}
}

Library::~Library()
{
	// Global library deinit
	assert(plugin_entry);
	plugin_entry->deinit();
}

std_fs::path Library::GetPath() const
{
	assert(!library_path.empty());
	return library_path;
}

const clap_plugin_factory_t* Library::GetPluginFactory() const
{
	assert(plugin_entry);

	const auto factory = static_cast<const clap_plugin_factory_t*>(
	        plugin_entry->get_factory(CLAP_PLUGIN_FACTORY_ID));

	if (!factory) {
		LOG_ERR("CLAP: Plugin library '%s' does not provide a plugin factory",
		        library_path.string().c_str());
	}

	return factory;
}

static bool has_synth_features(const char* const* features)
{
	if (!features) {
		return false;
	}

	bool has_instrument  = false;
	bool has_synthesizer = false;
	bool has_stereo      = false;

	for (auto feature = features; *feature; ++feature) {
		const auto feature_name = std::string_view(*feature);

		has_instrument |= feature_name == CLAP_PLUGIN_FEATURE_INSTRUMENT;
		has_synthesizer |= feature_name == CLAP_PLUGIN_FEATURE_SYNTHESIZER;
		has_stereo |= feature_name == CLAP_PLUGIN_FEATURE_STEREO;
	}

	return has_instrument && has_synthesizer && has_stereo;
}

std::vector<PluginInfo> Library::GetPluginInfos() const
{
	const auto factory = GetPluginFactory();
	if (!factory) {
		return {};
	}

	const auto num_plugins = factory->get_plugin_count(factory);
	std::vector<PluginInfo> plugin_infos = {};

	for (uint32_t plugin_index = 0; plugin_index < num_plugins; ++plugin_index) {
		const auto desc = factory->get_plugin_descriptor(factory,
		                                                 plugin_index);

		if (!desc) {
			continue;
		}

		if (!clap_version_is_compatible(desc->clap_version)) {
			const auto version = desc->clap_version;

			LOG_WARNING("CLAP: Skipping plugin at index %u in '%s': incompatible API version %u.%u.%u",
			            plugin_index,
			            library_path.string().c_str(),
			            version.major,
			            version.minor,
			            version.revision);
			continue;
		}

		if (!desc->id || !*desc->id || !desc->name || !*desc->name) {
			LOG_WARNING("CLAP: Skipping plugin at index %u in '%s': missing ID or name",
			            plugin_index,
			            library_path.string().c_str());
			continue;
		}

		if (!has_synth_features(desc->features)) {
			LOG_DEBUG("CLAP: Skipping plugin '%s': instrument, synthesizer, and stereo features are required",
			          desc->name);
			continue;
		}

		LOG_INFO("CLAP: Found plugin '%s'", desc->name);

		const PluginInfo info = {library_path,
		                         plugin_index,
		                         desc->id,
		                         desc->name,
		                         desc->description ? desc->description : "",
		                         desc->version ? desc->version : ""};

		plugin_infos.emplace_back(info);
	}

	return plugin_infos;
}

} // namespace Clap
