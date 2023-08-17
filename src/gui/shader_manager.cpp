/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "shader_manager.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <utility>

#include <SDL.h>

#include "checks.h"
#include "cross.h"
#include "string_utils.h"
#include "video.h"

CHECK_NARROWING();

ShaderManager::ShaderManager() noexcept
{
	static const std::vector<const char*> cga_shader_names = {"crt/cga-1080p",
	                                                          "crt/cga-1440p",
	                                                          "crt/cga-4k"};

	static const std::vector<const char*> ega_shader_names = {"crt/ega-1080p",
	                                                          "crt/ega-1440p",
	                                                          "crt/ega-4k"};

	static const std::vector<const char*> vga_shader_names = {"crt/vga-1080p",
	                                                          "crt/vga-1440p",
	                                                          "crt/vga-4k"};

	static const std::vector<const char*> composite_shader_names = {
	        "crt/composite-1080p", "crt/composite-1440p", "crt/composite-4k"};

	static const std::vector<const char*> monochrome_shader_names = {
	        "crt/monochrome"};

	auto build_set = [&](const std::vector<const char*>& shader_names) {
		std::string source = {};

		ShaderSet shader_set = {};
		for (const char* name : shader_names) {
			if (!(ReadShaderSource(name, source))) {
				LOG_ERR("RENDER: Cannot load built-in shader '%s'",
				        name);
			}
			const auto settings = ParseShaderSettings(name, source);
			shader_set.push_back({name, settings});
		}

		auto compare = [](const ShaderInfo& a, const ShaderInfo& b) {
			const auto fa = a.settings.min_vertical_scale_factor;
			const auto fb = b.settings.min_vertical_scale_factor;
			return (fa == fb) ? (a.name < b.name) : (fa > fb);
		};
		std::sort(shader_set.begin(), shader_set.end(), compare);

		LOG_WARNING("-----------------------------------------");
		for (auto info : shader_set) {
			LOG_WARNING("  shader name: %s, min_vertical_scale_factor: %g",
			            info.name.c_str(),
			            info.settings.min_vertical_scale_factor);
		}

		return shader_set;
	};

	const auto start = std::chrono::high_resolution_clock::now();

	shader_set.monochrome = build_set(monochrome_shader_names);
	shader_set.composite  = build_set(composite_shader_names);
	shader_set.cga        = build_set(cga_shader_names);
	shader_set.ega        = build_set(ega_shader_names);
	shader_set.vga        = build_set(vga_shader_names);

	const auto stop = std::chrono::high_resolution_clock::now();
	const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
	        stop - start);

	LOG_INFO("RENDER: Building shader sets took %lld ms", duration.count());
}

ShaderManager::~ShaderManager() noexcept = default;

void ShaderManager::NotifyGlshaderSettingChanged(const std::string& shader_name)
{
	LOG_ERR("NotifyGlshaderSetting: %s", shader_name.c_str());

	if (shader_name == AutoGraphicsStandardShaderName) {
		if (mode != ShaderMode::AutoGraphicsStandard) {
			mode = ShaderMode::AutoGraphicsStandard;
			LOG_MSG("RENDER: Using auto-switching graphics standard shader");
		}
	} else if (shader_name == AutoMachineShaderName) {
		if (mode != ShaderMode::AutoMachine) {
			mode = ShaderMode::AutoMachine;
			LOG_MSG("RENDER: Using auto-switching machine shader");
		}
	} else {
		mode = ShaderMode::Single;
	}

	shader_name_from_config = shader_name;

	MaybeUpdateCurrentShader();
}

void ShaderManager::NotifyRenderParametersChanged(
        const uint16_t canvas_width, const uint16_t canvas_height,
        const uint16_t draw_width, const uint16_t draw_height,
        const Fraction& render_pixel_aspect_ratio, const VideoMode& video_mode)
{
	LOG_ERR("NotifyRenderParameters");
	LOG_ERR("  canvas_width: %d, height: %d", canvas_width, canvas_height);
	LOG_ERR("  draw_width: %d, draw_height: %d", draw_width, draw_height);

	const auto viewport = GFX_CalcViewport(canvas_width,
	                                       canvas_height,
	                                       draw_width,
	                                       draw_height,
	                                       render_pixel_aspect_ratio);

	vertical_scale_factor = static_cast<double>(viewport.h) / draw_height;
	LOG_ERR("    viewport.h:   %d", viewport.h);
	LOG_ERR("    vertical_scale_factor: %g", vertical_scale_factor);

	// Force single-scanned
	auto render_height = draw_height;
	if (video_mode.is_double_scanned_mode) {
		const auto double_scanned = draw_height > video_mode.height;
		if (double_scanned) {
			const auto viewport_ss = GFX_CalcViewport(
			        canvas_width,
			        canvas_height,
			        draw_width,
			        video_mode.height,
			        video_mode.pixel_aspect_ratio / 2);

			vertical_scale_factor_ss = static_cast<double>(
			                                   viewport_ss.h) /
			                           video_mode.height;

			LOG_ERR("    viewport_ss.h:   %d", viewport_ss.h);
			LOG_ERR("    vertical_scale_factor_ss: %g",
			        vertical_scale_factor_ss);
		}
	}

	this->video_mode = video_mode;

	MaybeUpdateCurrentShader();
}

bool ShaderManager::MaybeLoadShader(const std::string& shader_name)
{
	LOG_ERR("current_shader.info.name: %s, shader_name: %s",
	        current_shader.info.name.c_str(),
	        shader_name.c_str());

	if (current_shader.info.name == shader_name) {
		return false;
	}

	auto new_shader_name = shader_name;

	if (shader_name == DefaultShaderName) {
		new_shader_name = SharpShaderName;
	} else {
		const auto mapped_name = MapLegacyShaderName(shader_name);
		if (mapped_name) {
			LOG_WARNING("RENDER: Built-in shader '%s' has been renamed to '%s'; "
			            "using '%s' instead.",
			            shader_name.c_str(),
			            mapped_name->c_str(),
			            mapped_name->c_str());

			new_shader_name = *mapped_name;
		}
	}

	if (!ReadShaderSource(new_shader_name, current_shader.source)) {
		current_shader.source.clear();

		// List all the existing shaders for the user
		LOG_ERR("RENDER: Shader file '%s' not found",
		        new_shader_name.c_str());

		for (const auto& line : InventoryShaders()) {
			LOG_WARNING("RENDER: %s", line.c_str());
		}

		// Fallback to the 'none' shader and otherwise fail
		if (ReadShaderSource(FallbackShaderName, current_shader.source)) {
			new_shader_name = FallbackShaderName;
		} else {
			E_Exit("RENDER: Fallback shader file '%s' not found and is mandatory",
			       FallbackShaderName);
		}
	}

	const auto settings = ParseShaderSettings(new_shader_name,
	                                          current_shader.source);

	current_shader.info = {new_shader_name, settings};
	return true;
}

ShaderInfo ShaderManager::GetCurrentShaderInfo() const
{
	return current_shader.info;
}

std::string ShaderManager::GetCurrentShaderSource() const
{
	return current_shader.source;
}

std::deque<std::string> ShaderManager::InventoryShaders() const
{
	std::deque<std::string> inventory;
	inventory.emplace_back("");
	inventory.emplace_back("List of available GLSL shaders");
	inventory.emplace_back("------------------------------");

	const std::string dir_prefix  = "Path '";
	const std::string file_prefix = "        ";

	std::error_code ec = {};
	for (auto& [dir, shaders] : GetFilesInResource("glshaders", ".glsl")) {
		const auto dir_exists      = std_fs::is_directory(dir, ec);
		auto shader                = shaders.begin();
		const auto dir_has_shaders = shader != shaders.end();
		const auto dir_postfix     = dir_exists
		                                   ? (dir_has_shaders ? "' has:"
		                                                      : "' has no shaders")
		                                   : "' does not exist";

		inventory.emplace_back(dir_prefix + dir.string() + dir_postfix);

		while (shader != shaders.end()) {
			shader->replace_extension("");
			const auto is_last = (shader + 1 == shaders.end());
			inventory.emplace_back(file_prefix +
			                       (is_last ? "`- " : "|- ") +
			                       shader->string());
			++shader;
		}
		inventory.emplace_back("");
	}
	inventory.emplace_back(
	        "The above shaders can be used exactly as listed in the 'glshader'");
	inventory.emplace_back(
	        "conf setting, without the need for the resource path or .glsl extension.");
	inventory.emplace_back("");

	return inventory;
}

std::optional<std::string> ShaderManager::MapLegacyShaderName(const std::string& name) const
{
	static const std::map<std::string, std::string> legacy_name_mappings = {
	        {"advinterp2x", "scaler/advinterp2x"},
	        {"advinterp3x", "scaler/advinterp3x"},
	        {"advmame2x", "scaler/advmame2x"},
	        {"advmame3x", "scaler/advmame3x"},
	        {"rgb2x", "scaler/rgb2x"},
	        {"rgb3x", "scaler/rgb3x"},
	        {"scan2x", "scaler/scan2x"},
	        {"scan3x", "scaler/scan3x"},
	        {"sharp", SharpShaderName},
	        {"tv2x", "scaler/tv2x"},
	        {"tv3x", "scaler/tv3x"}};

	std_fs::path shader_path = name;
	std_fs::path ext         = shader_path.extension();

	if (!(ext == "" || ext == ".glsl")) {
		return {};
	}

	shader_path.replace_extension("");

	const auto it = legacy_name_mappings.find(shader_path.string());
	if (it != legacy_name_mappings.end()) {
		const auto new_name = it->second;
		return new_name;
	}
	return {};
}

bool ShaderManager::ReadShaderSource(const std::string& shader_name, std::string& source)
{
	auto read_shader = [&](const std_fs::path& path) {
		std::ifstream fshader(path, std::ios_base::binary);
		if (!fshader.is_open()) {
			return false;
		}
		//		LOG_WARNING("*** Reading shader from
		//'%s'", std_fs::absolute(path).string().c_str());
		std::stringstream buf;
		buf << fshader.rdbuf();
		fshader.close();

		source = buf.str() + '\n';
		return true;
	};

	constexpr auto glsl_ext      = ".glsl";
	constexpr auto glshaders_dir = "glshaders";

	// Start with the name as-is and then try from resources
	const auto candidate_paths = {std_fs::path(shader_name),
	                              std_fs::path(shader_name + glsl_ext),
	                              GetResourcePath(glshaders_dir, shader_name),
	                              GetResourcePath(glshaders_dir,
	                                              shader_name + glsl_ext)};

	for (const auto& path : candidate_paths) {
		if (std_fs::exists(path) &&
		    (std_fs::is_regular_file(path) || std_fs::is_symlink(path))) {
			return read_shader(path);
		}
	}
	return false;
}

ShaderSettings ShaderManager::ParseShaderSettings(const std::string& shader_name,
                                                  const std::string& source) const
{
	ShaderSettings settings = {};
	try {
		const std::regex re("\\s*#pragma\\s+(\\w+)\\s*([\\w\\.]+)?");
		std::sregex_iterator next(source.begin(), source.end(), re);
		const std::sregex_iterator end;

		while (next != end) {
			std::smatch match = *next;
			auto pragma       = match[1].str();

			if (pragma == "use_srgb_texture") {
				settings.use_srgb_texture = true;

			} else if (pragma == "use_srgb_framebuffer") {
				settings.use_srgb_framebuffer = true;

			} else if (pragma == "force_single_scan") {
				settings.force_single_scan = true;

			} else if (pragma == "force_no_pixel_doubling") {
				settings.force_no_pixel_doubling = true;

			} else if (pragma == "min_vertical_scale_factor") {
				auto value_str = match[2].str();
				if (value_str.empty()) {
					LOG_WARNING("RENDER: No value specified for "
					            "'min_vertical_scale_factor' pragma in shader '%s'",
					            shader_name.c_str());
				} else {
					constexpr auto MinValidScaleFactor = 0.0f;
					constexpr auto MaxValidScaleFactor = 100.0f;
					const auto value =
					        parse_value(value_str,
					                    MinValidScaleFactor,
					                    MaxValidScaleFactor);

					if (value) {
						settings.min_vertical_scale_factor = *value;
					} else {
						LOG_WARNING("RENDER: Invalid 'min_vertical_scale_factor' "
						            "pragma value of '%s' in shader '%s'",
						            value_str.c_str(),
						            shader_name.c_str());
					}
				}
			}
			++next;
		}
	} catch (std::regex_error& e) {
		LOG_ERR("RENDER: Regex error while parsing shader '%s' for pragmas: %d",
		        shader_name.c_str(),
		        e.code());
	}
	return settings;
}

void ShaderManager::MaybeUpdateCurrentShader()
{
	auto auto_switch_shader = [&](const ShaderSet& shader_set) {
		const auto shader_name = [&] {
			for (auto shader : shader_set) {
				LOG_WARNING("  >>> vert: %g, shader: %g",
				            vertical_scale_factor,
				            shader.settings.min_vertical_scale_factor);
				if (vertical_scale_factor >=
				    shader.settings.min_vertical_scale_factor) {
					return shader.name;
				}
			}
			return std::string(SharpShaderName);
		}();

		LOG_WARNING("  vertical_scale_factor: %g", vertical_scale_factor);
		LOG_WARNING("  shader_name: %s", shader_name.c_str());

		const auto shader_changed = MaybeLoadShader(shader_name);
		if (shader_changed) {
			LOG_MSG("RENDER: Auto-switched to shader '%s'",
			        current_shader.info.name.c_str());
		}
	};

	switch (mode) {
	case ShaderMode::Single: {
		const auto shader_changed = MaybeLoadShader(shader_name_from_config);
		if (shader_changed) {
			// TODO explain
			shader_name_from_config = current_shader.info.name;

			LOG_MSG("RENDER: Using shader '%s'",
			        current_shader.info.name.c_str());
		}
	} break;

	case ShaderMode::AutoGraphicsStandard:
		return auto_switch_shader(GetShaderSetForGraphicsStandard(video_mode));

	case ShaderMode::AutoMachine:
		return auto_switch_shader(
		        GetShaderSetForMachineType(machine, video_mode));

	default: assertm(false, "Invalid ShaderMode value");
	}
}

const ShaderSet& ShaderManager::GetShaderSetForGraphicsStandard(const VideoMode& video_mode) const
{
	if (video_mode.color_depth == ColorDepth::Monochrome) {
		return shader_set.monochrome;
	}
	if (video_mode.color_depth == ColorDepth::Composite) {
		return shader_set.composite;
	}

	switch (video_mode.graphics_standard) {
	case GraphicsStandard::Hercules: return shader_set.monochrome;

	case GraphicsStandard::Cga: return shader_set.cga;

	case GraphicsStandard::Pcjr:
	case GraphicsStandard::Tga:
	case GraphicsStandard::Ega: return shader_set.ega;

	case GraphicsStandard::Vga:
	case GraphicsStandard::Svga:
	case GraphicsStandard::Vesa: return shader_set.vga;

	default:
		assertm(false, "Invalid GraphicsStandard value");
		// dummy return value
		return shader_set.monochrome;
	}
}

const ShaderSet& ShaderManager::GetShaderSetForMachineType(
        const MachineType machine_type, const VideoMode& video_mode) const
{
	if (video_mode.color_depth == ColorDepth::Composite) {
		return shader_set.composite;
	}
	switch (machine_type) {
	case MCH_HERC: return shader_set.monochrome;
	case MCH_CGA: return shader_set.cga;
	case MCH_TANDY: return shader_set.ega;
	case MCH_PCJR: return shader_set.ega;
	case MCH_EGA: return shader_set.ega;
	case MCH_VGA: return shader_set.vga;
	default:
		assertm(false, "Invalid MachineType value");
		// dummy return value
		return shader_set.monochrome;
	};
}
