// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/shader_manager.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <unordered_map>

#include <SDL.h>

#include "simpleini/SimpleIni.h"

#include "misc/notifications.h"
#include "private/shader_pragma_parser.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

// #define DEBUG_SHADER_MANAGER

ShaderDescriptor ShaderDescriptor::FromString(const std::string& descriptor,
                                              const std::string& extension)
{
	const auto parts = split(descriptor, ":");

	// Drop optional shader file extension (e.g., '.glsl')
	std_fs::path path = parts[0];
	if (path.extension() == extension) {
		path.replace_extension("");
	}

	const auto shader_name = path.string();
	const auto preset_name = (parts.size() > 1) ? parts[1] : "";

	const auto shader_mode = [&] {
		// clang-format off
		static const std::unordered_map<std::string, ShaderMode> auto_shader_map = {
			{SymbolicShaderName::AutoGraphicsStandard, ShaderMode::AutoGraphicsStandard},
			{SymbolicShaderName::AutoMachine,          ShaderMode::AutoMachine},
			{SymbolicShaderName::AutoArcade,           ShaderMode::AutoArcade},
			{SymbolicShaderName::AutoArcadeSharp,      ShaderMode::AutoArcadeSharp},
		};
		// clang-format on

		if (const auto it = auto_shader_map.find(descriptor);
		    it != auto_shader_map.end()) {
			return it->second;
		}
		return ShaderMode::Single;
	}();

	return {shader_name, preset_name, shader_mode};
}

bool ShaderDescriptor::EnforceAutoIntegerScaling() const
{
	// 'integer_scaling = auto' enables integer scaling only for the CRT
	// shaders (either set via the adaptive meta-shaders, e.g., `crt-auto`, or
	// directly via `crt/crt-hyllian:4k` or similar).
	//
	return shader_name.starts_with("crt/");
}

std::optional<Shader> ShaderManager::LoadShader(const std::string& shader_name)
{
	if (!shader_cache.contains(shader_name)) {
		const auto maybe_shader = LoadAndBuildShader(shader_name);
		if (!maybe_shader) {
			return {};
		}
		const auto& shader = *maybe_shader;

		shader_cache[shader.info.name] = shader;

#ifdef DEBUG_SHADER_MANAGER
		LOG_DEBUG("OPENGL: Built and cached shader '%s'",
		          shader_name.c_str());
#endif

	} else {
#ifdef DEBUG_SHADER_MANAGER
		LOG_DEBUG("OPENGL: Using cached shader '%s'", shader_name.c_str());
#endif
	}

	return shader_cache[shader_name];
}

std::optional<std::pair<Shader, ShaderPreset>> ShaderManager::ForceReloadShader(
        const ShaderDescriptor& descriptor)
{
	const auto maybe_shader = LoadAndBuildShader(descriptor.shader_name);
	if (!maybe_shader) {
		LOG_ERR("RENDER: Error reloading shader '%s'",
		        descriptor.shader_name.c_str());
		return {};
	}
	const auto shader = *maybe_shader;

	auto replace_cached_shader = [&] {
		// Replace reloaded shader in the cache
		const auto& shader_key = descriptor.shader_name;
		assert(shader_cache.contains(shader_key));

		glDeleteProgram(shader_cache[shader_key].program_object);
		shader_cache[shader_key] = shader;
	};

	if (descriptor.preset_name.empty()) {
		// No preset specified; use default preset
		replace_cached_shader();

		return {
		        {shader, shader.info.default_preset}
                };
	}

	const auto maybe_preset = ShaderManager::LoadShaderPreset(
	        descriptor, shader.info.default_preset);
	if (!maybe_preset) {
		LOG_ERR("RENDER: Error reloading shader preset '%s'",
		        descriptor.ToString().c_str());
		return {};
	}
	const auto& preset = *maybe_preset;

	replace_cached_shader();

	// Replace reloaded shader preset in the cache
	const auto& preset_key = descriptor.ToString();
	assert(shader_preset_cache.contains(preset_key));

	shader_preset_cache[preset_key] = preset;

	return {
	        {shader, preset}
        };
}

std::optional<Shader> ShaderManager::LoadAndBuildShader(const std::string& shader_name)
{
	constexpr auto GlslExtension = ".glsl";

	auto maybe_shader_source = FindShaderAndReadSource(shader_name + GlslExtension);
	if (!maybe_shader_source) {
		// List all the existing shaders for the user
		// TODO convert to notification
		LOG_ERR("RENDER: Shader file '%s' not found", shader_name.c_str());

		for (const auto& line : GenerateShaderInventoryMessage()) {
			// TODO convert to notification (maybe?)
			LOG_WARNING("RENDER: %s", line.c_str());
		}
		return {};
	}

	const auto& shader_source = *maybe_shader_source;

	const auto maybe_pragmas = ShaderPragma::Parse(shader_name, shader_source);
	if (!maybe_pragmas) {
		LOG_ERR("RENDER: Error parsing pragmas of shader '%s'",
		        shader_name.c_str());
		return {};
	}
	const auto pragmas = *maybe_pragmas;

	Shader shader = {};

	shader.info = {shader_name,
	               pragmas.pass_name,
	               pragmas.input_ids,
	               pragmas.output_size,
	               pragmas.preset};

	if (!shader.BuildShaderProgram(shader_source)) {
		LOG_ERR("RENDER: Error loading shader '%s'", shader_name.c_str());
		return {};
	}
	return shader;
}

ShaderPreset ShaderManager::LoadShaderPresetOrDefault(const ShaderDescriptor& descriptor)
{
	assert(!descriptor.shader_name.empty());

	assert(shader_cache.contains(descriptor.shader_name));

	const auto& default_preset = shader_cache[descriptor.shader_name].info.default_preset;
	if (descriptor.preset_name.empty()) {
#ifdef DEBUG_SHADER_MANAGER
		LOG_DEBUG("OPENGL: Using default shader preset",
		          descriptor.ToString().c_str());
#endif
		return default_preset;
	}

	const auto cache_key = descriptor.ToString();

	if (!shader_preset_cache.contains(cache_key)) {
		const auto maybe_preset = LoadShaderPreset(descriptor, default_preset);
		if (!maybe_preset) {
			LOG_ERR("OPENGL: Error loading shader preset '%s'; using default preset",
			        descriptor.ToString().c_str());
			return default_preset;
		}

		shader_preset_cache[cache_key] = *maybe_preset;

#ifdef DEBUG_SHADER_MANAGER
		LOG_DEBUG("OPENGL: Loaded and cached shader preset '%s'",
		          descriptor.ToString().c_str());
#endif

	} else {
#ifdef DEBUG_SHADER_MANAGER
		LOG_DEBUG("OPENGL: Using cached shader preset '%s'",
		          descriptor.ToString().c_str());
#endif
	}

	return shader_preset_cache[cache_key];
}

std::optional<ShaderPreset> ShaderManager::LoadShaderPreset(
        const ShaderDescriptor& descriptor, const ShaderPreset& default_preset) const
{
	assert(!descriptor.shader_name.empty());
	assert(!descriptor.preset_name.empty());

	constexpr auto ShaderPresetExtension = ".preset";

	const auto path = get_resource_path(ShaderPresetsDir,
	                                    std_fs::path{descriptor.shader_name} /
	                                            (descriptor.preset_name +
	                                             ShaderPresetExtension));

	// TODO get_resource_path() should return optional
	if (path.empty()) {
		LOG_ERR("RENDER: Cannot locate shader preset '%s'",
		        descriptor.ToString().c_str());
		return {};
	}

	const auto file_exists = std_fs::exists(path) &&
	                         (std_fs::is_regular_file(path) ||
	                          std_fs::is_symlink(path));
	if (!file_exists) {
		LOG_ERR("RENDER: Error loading shader preset file '%s'; "
		        "file does not exist or not a regular file",
		        path.string().c_str());
		return {};
	}

	CSimpleIniA ini;
	ini.SetUnicode();

#ifdef DEBUG_SHADER_MANAGER
	LOG_DEBUG("RENDER: Loading shader preset '%s'", path.string().c_str());
#endif

	if (ini.LoadFile(path.string().c_str()) < 0) {
		LOG_ERR("RENDER: Error loading shader preset '%s'; invalid file format",
		        path.string().c_str());
		return {};
	}

	ShaderPreset preset = default_preset;
	preset.name         = descriptor.preset_name;

	if (const auto settings = ini.GetSection("settings"); settings) {
		for (const auto& [name, value] : *settings) {

			if (!ShaderPragma::SetSetting(name.pItem, value, preset.settings)) {
				LOG_ERR("RENDER: Invalid shader setting, name: '%s', value: '%s'",
				        name.pItem,
				        value);
				return {};
			}
		}
	}

	if (const auto params = ini.GetSection("parameters"); params) {
		for (const auto& [key, value_str] : *params) {
			const auto name = key.pItem;

			if (!default_preset.params.contains(name)) {
				LOG_ERR("RENDER: Invalid shader parameter name: '%s'",
				        name);
			} else {
				const auto maybe_float = parse_float(value_str);
				if (!maybe_float) {
					LOG_ERR("RENDER: Invalid value for shader parameter '%s' "
					        "(must be float): '%s'",
					        name,
					        value_str);
				} else {
					preset.params[name] = *maybe_float;
				}
			}
		}
	} else {
		LOG_ERR("RENDER: Invalid shader preset file; [parameters] section not found");
		return {};
	}

	return preset;
}

std::deque<std::string> ShaderManager::GenerateShaderInventoryMessage() const
{
	std::deque<std::string> inventory;
	inventory.emplace_back("");
	inventory.emplace_back(MSG_GetTranslatedRaw("DOSBOX_HELP_LIST_SHADERS_1"));
	inventory.emplace_back("");

	const std::string file_prefix = "        ";

	std::error_code ec = {};

	constexpr auto OnlyRegularFiles = true;

	for (const auto& parent : get_resource_parent_paths()) {
		// TODO Handling the optional shader file extension should be
		// handled by the render backend once we add more backends with
		// shader support.
		const auto dir = parent / ShadersDir;
		auto shaders = get_directory_entries(dir, ".glsl", OnlyRegularFiles);

		const auto dir_exists      = std_fs::is_directory(dir, ec);
		auto shader                = shaders.begin();
		const auto dir_has_shaders = shader != shaders.end();

		std::string pattern = {};
		if (!dir_exists) {
			pattern = MSG_GetTranslatedRaw(
			        "DOSBOX_HELP_LIST_SHADERS_NOT_EXISTS");

		} else if (!dir_has_shaders) {
			pattern = MSG_GetTranslatedRaw(
			        "DOSBOX_HELP_LIST_SHADERS_NO_SHADERS");

		} else {
			pattern = MSG_GetTranslatedRaw("DOSBOX_HELP_LIST_SHADERS_LIST");
		}
		inventory.emplace_back(format_str(pattern, dir.string().c_str()));

		while (shader != shaders.end()) {
			if (shader->string().starts_with("_internal/")) {
				++shader;
				continue;
			}
			shader->replace_extension("");
			const auto is_last = (shader + 1 == shaders.end());
			inventory.emplace_back(file_prefix +
			                       (is_last ? "`- " : "|- ") +
			                       shader->string());
			++shader;
		}
		inventory.emplace_back("");
	}

	inventory.emplace_back(MSG_GetTranslatedRaw("DOSBOX_HELP_LIST_SHADERS_2"));

	return inventory;
}

void ShaderManager::AddMessages()
{
	MSG_Add("DOSBOX_HELP_LIST_SHADERS_1",
	        "List of available GLSL shaders\n"
	        "------------------------------");
	MSG_Add("DOSBOX_HELP_LIST_SHADERS_2",
	        "The above shaders can be used exactly as listed in the 'shader'\n"
	        "config setting, without the need for the resource path or .glsl extension.");

	MSG_Add("DOSBOX_HELP_LIST_SHADERS_NOT_EXISTS", "Path '%s' does not exist.");
	MSG_Add("DOSBOX_HELP_LIST_SHADERS_NO_SHADERS", "Path '%s' has no shaders.");
	MSG_Add("DOSBOX_HELP_LIST_SHADERS_LIST", "Path '%s' has:");
}

std::optional<std::string> ShaderManager::FindShaderAndReadSource(
        const std::string& shader_name_with_extension)
{
	auto read_shader = [&](const std_fs::path& path) -> std::optional<std::string> {
		std::ifstream fshader(path, std::ios_base::binary);
		if (!fshader.is_open()) {
			return {};
		}
		std::stringstream buf;
		buf << fshader.rdbuf();
		fshader.close();

		return buf.str() + '\n';
	};

	// Start with the provided path...
	std::vector<std_fs::path> candidate_paths = {shader_name_with_extension};

	// ...and then try from resources
	const auto resource_shader_path = get_resource_path(ShadersDir,
	                                                    shader_name_with_extension);

	// TODO get_resource_path() should return optional
	if (!resource_shader_path.empty()) {
		candidate_paths.emplace_back(resource_shader_path);
	}

	for (const auto& path : candidate_paths) {
		if (std_fs::exists(path) &&
		    (std_fs::is_regular_file(path) || std_fs::is_symlink(path))) {
			return read_shader(path);
		}
	}
	return {};
}
