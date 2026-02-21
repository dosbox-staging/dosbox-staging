// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/shader_manager.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <regex>
#include <unordered_map>

#include <SDL.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weffc++"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#include <simpleini/SimpleIni.h>
#pragma GCC diagnostic pop
#pragma clang diagnostic pop

#include "misc/notifications.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

// #define DEBUG_SHADER_MANAGER

ShaderDescriptor ShaderDescriptor::FromString(const std::string& descriptor,
                                              const std::string& extension)
{
	const auto parts        = split(descriptor, ":");
	const auto& shader_name = parts[0];

	// Drop optional shader file extension (e.g., '.glsl')
	std_fs::path path = shader_name;
	if (path.extension() == extension) {
		path.replace_extension("");
	}

	const auto preset_name = (parts.size() > 1) ? parts[1] : "";

	return {path.string(), preset_name};
}

ShaderManager::~ShaderManager()
{
	for (auto& [_, shader] : shader_cache) {
		glDeleteProgram(shader.program_object);
	}
	shader_cache.clear();
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

std::optional<Shader> ShaderManager::ForceReloadShader(const std::string& shader_name)
{
	if (shader_cache.contains(shader_name)) {
		glDeleteProgram(shader_cache[shader_name].program_object);
		shader_cache.erase(shader_name);
	}
	return LoadAndBuildShader(shader_name);
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
	const auto result = ParseShaderPragmas(shader_name, shader_source);

	const bool is_adaptive = [&] {
		// TODO why?
		//		if (current_shader.mode == ShaderMode::Single) {
		//			return false;
		//		} else {
		// This will turn off vertical integer scaling for the
		// 'sharp' shader in 'integer_scaling = auto' mode
		return (shader_name != ShaderName::Sharp);
		//		}
	}();

	Shader shader = {};

	shader.info = {shader_name,
	               result.pass_name,
	               result.input_ids,
	               result.output_size,
	               result.preset,
	               is_adaptive};

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
			LOG_WARNING("OPENGL: Error loading shader preset '%s'; using default preset",
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
		LOG_WARNING("RENDER: Cannot locate shader preset '%s'",
		            descriptor.ToString().c_str());
		return {};
	}

	const auto file_exists = std_fs::exists(path) &&
	                         (std_fs::is_regular_file(path) ||
	                          std_fs::is_symlink(path));
	if (!file_exists) {
		LOG_WARNING(
		        "RENDER: Error loading shader preset file '%s'; "
		        "file does not exist or not a regular file",
		        path.string().c_str());
		return {};
	}

	CSimpleIniA ini;
	ini.SetUnicode();

#ifdef DEBUG_SHADER_MANAGER
	LOG_DEBUG("RENDER: Loading shader preset '%s'", path.string().c_str());
#endif

	const auto result = ini.LoadFile(path.string().c_str());
	if (result < 0) {
		LOG_WARNING("RENDER: Error loading shader preset '%s'; invalid file format",
		            path.string().c_str());
		return {};
	}

	ShaderPreset preset = default_preset;

	preset.name = descriptor.preset_name;

	if (const auto settings = ini.GetSection("settings"); settings) {
		for (const auto& [name, value] : *settings) {
			SetShaderSetting(name.pItem, value, preset.settings);
		}
	}

	if (const auto params = ini.GetSection("parameters"); params) {
		for (const auto& [key, value_str] : *params) {
			const auto name = key.pItem;

			if (!default_preset.params.contains(name)) {
				LOG_WARNING("RENDER: Invalid shader parameter name: '%s'",
				            name);
			} else {
				const auto maybe_float = parse_float(value_str);
				if (!maybe_float) {
					LOG_WARNING(
					        "RENDER: Invalid value for shader parameter '%s' "
					        "(must be float): '%s'",
					        name,
					        value_str);
				} else {
					preset.params[name] = *maybe_float;
				}
			}
		}
	} else {
		LOG_WARNING("RENDER: Invalid preset file; [parameters] section not found");
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
		inventory.emplace_back(format_str(pattern, dir.u8string().c_str()));

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

ShaderManager::ParseShaderPragmaResult ShaderManager::ParseShaderPragmas(
        const std::string& shader_name, const std::string& shader_source) const
{
	ParseShaderPragmaResult result = {};

	// The default preset has no name
	result.preset.name.clear();

	std::unordered_map<int, std::string> input_ids = {};
	auto highest_input_index                       = -1;

	try {
		const std::regex re("\\s*#pragma\\s+(.+)");

		std::sregex_iterator next(shader_source.begin(),
		                          shader_source.end(),
		                          re);
		const std::sregex_iterator end;

		while (next != end) {
			std::smatch match = *next;

			auto pragma = match[1].str();

			if (pragma.starts_with("parameter")) {
				if (const auto maybe_result = ParseParameterPragma(pragma);
				    maybe_result) {

					const auto [param_name,
					            default_value] = *maybe_result;

					result.preset.params[param_name] = default_value;
				} else {
					LOG_ERR("RENDER: Invalid shader parameter pragma: '%s'",
					        pragma.c_str());
				}

			} else if (pragma.starts_with("name")) {
				if (const auto maybe_name = ParseNamePragma(pragma);
				    maybe_name) {

					result.pass_name = *maybe_name;
				} else {
					LOG_ERR("RENDER: Invalid shader pass name pragma: '%s'",
					        pragma.c_str());
				}

			} else if (pragma.starts_with("input")) {
				if (const auto maybe_result = ParseInputPragma(pragma);
				    maybe_result) {

					auto [input_index, input_name] = *maybe_result;
					input_ids[input_index] = input_name;

					highest_input_index = std::max(
					        input_index, highest_input_index);
				} else {
					LOG_ERR("RENDER: Invalid input name pragme: '%s'",
					        pragma.c_str());
				}

			} else if (pragma.starts_with("output_size")) {
				if (const auto maybe_output_size = ParseOutputSizePragma(
				            pragma);
				    maybe_output_size) {

					result.output_size = *maybe_output_size;
				} else {
					LOG_ERR("RENDER: Invalid output size pragma: '%s'",
					        pragma.c_str());
				}

			} else {
				SetShaderSetting(pragma, "on", result.preset.settings);
			}

			++next;
		}
	} catch (std::regex_error& e) {
		LOG_ERR("RENDER: Regex error while parsing shader '%s' for pragmas: %d",
		        shader_name.c_str(),
		        e.code());
	}

	// Validate and store texture input IDs
	const auto num_inputs = check_cast<int>(input_ids.size());

	if (num_inputs != (highest_input_index + 1)) {
		LOG_ERR("RENDER: Shader input indices are invalid "
		        "(%d inputs but highest input index is %d)",
		        num_inputs,
		        highest_input_index);
	}
	if (num_inputs == 0) {
		result.input_ids = {"Previous"};
	} else {
		for (auto i = 0; i <= highest_input_index; ++i) {
			result.input_ids.emplace_back(input_ids[i]);
		}
	}

	return result;
}

void ShaderManager::SetShaderSetting(const std::string& name, const std::string& value,
                                     ShaderSettings& settings) const
{
	assert(!name.empty());

	const auto is_true = (value == "1") || has_true(value);

	if (name == "force_single_scan") {
		settings.force_single_scan = is_true;

	} else if (name == "force_no_pixel_doubling") {
		settings.force_no_pixel_doubling = is_true;

	} else if (name == "use_nearest_texture_filter") {
		settings.texture_filter_mode = is_true ? TextureFilterMode::NearestNeighbour
		                                       : TextureFilterMode::Bilinear;

	} else {
		LOG_WARNING("RENDER: Unknown shader setting pragma: '%s'",
		            name.c_str());
	}
}

std::optional<std::pair<std::string, float>> ShaderManager::ParseParameterPragma(
        const std::string& pragma) const
{
	assert(!pragma.empty());

	// Parameter format example:
	//
	//   #pragma parameter OUTPUT_GAMMA "OUTPUT GAMMA" 2.2 0.0 5.0 0.1
	//
	const auto parts = split(strip_prefix(pragma, "parameter"), "\"");
	if (parts.size() != 3) {
		return {};
	}
	// parts[0] - param (variable) name (OUTPUT_GAMMA)
	// parts[1] - display name (OUTPUT GAMMA)
	// parts[2] - values (4 space-separated floats)

	auto param_name = parts[0];
	trim(param_name);

	const auto params = split(parts[2]);
	if (params.size() != 4) {
		return {};
	}
	// params[0] - default value (2.2)
	// params[1] - min value     (0.0)
	// params[2] - max value     (5.0)
	// params[3] - value step    (0.1)

	const auto maybe_default_val = parse_float(params[0]);
	if (!maybe_default_val) {
		return {};
	}

	return {
	        {param_name, *maybe_default_val}
        };
}

std::optional<std::string> ShaderManager::ParseNamePragma(const std::string& pragma) const
{
	// Shader pass name format:
	//
	//   #pragma name Main_Pass1
	//
	const auto parts = split(strip_prefix(pragma, "name"));
	if (parts.size() != 1) {
		return {};
	}
	return parts[0];
}

std::optional<ShaderOutputSize> ShaderManager::ParseOutputSizePragma(
        const std::string& pragma) const
{
	// Output size format:
	//
	//   #pragma output_size VideoMode
	//
	// Valid values are: Rendered, VideoMode, Viewport
	//
	const auto parts = split(strip_prefix(pragma, "output_size"));
	if (parts.size() != 1) {
		return {};
	}
	const auto& type = parts[0];

	using enum ShaderOutputSize;

	if (type == "Rendered") {
		return Rendered;

	} else if (type == "VideoMode") {
		return VideoMode;

	} else if (type == "Viewport") {
		return Viewport;
	}
	return {};
}

std::optional<std::pair<int, std::string>> ShaderManager::ParseInputPragma(
        const std::string& pragma) const
{
	// Shader input format:
	//
	//   #pragma input0 Main_Pass2
	//   #pragma input1 Previous
	//   ...
	//
	const auto parts = split(strip_prefix(pragma, "input"));
	if (parts.size() != 2) {
		return {};
	}

	const auto& index_str = parts[0];

	if (auto maybe_int = parse_int(index_str); maybe_int) {
		const auto& name = parts[1];

		return {
		        {*maybe_int, name}
                };
	}
	return {};
}
