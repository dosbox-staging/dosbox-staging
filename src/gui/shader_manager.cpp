/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2024  The DOSBox Staging Team
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
#include <fstream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <utility>

#include <SDL.h>

#include "checks.h"
#include "cross.h"
#include "dosbox.h"
#include "math_utils.h"
#include "string_utils.h"
#include "video.h"

CHECK_NARROWING();

void ShaderManager::NotifyGlshaderSettingChanged(const std::string& shader_name)
{
	if (shader_name == AutoGraphicsStandardShaderName) {
		if (mode != ShaderMode::AutoGraphicsStandard) {
			mode = ShaderMode::AutoGraphicsStandard;

			LOG_MSG("RENDER: Using adaptive CRT shader based on the graphics "
			        "standard of the video mode");
		}
	} else if (shader_name == AutoMachineShaderName) {
		if (mode != ShaderMode::AutoMachine) {
			mode = ShaderMode::AutoMachine;

			LOG_MSG("RENDER: Using adaptive CRT shader based on the "
			        "configured graphics adapter");
		}
	} else if (shader_name == AutoArcadeShaderName) {
		if (mode != ShaderMode::AutoArcade) {
			mode = ShaderMode::AutoArcade;

			LOG_MSG("RENDER: Using adaptive arcade monitor emulation "
			        "CRT shader (normal variant)");
		}
	} else if (shader_name == AutoArcadeSharpShaderName) {
		if (mode != ShaderMode::AutoArcadeSharp) {
			mode = ShaderMode::AutoArcadeSharp;

			LOG_MSG("RENDER: Using adaptive arcade monitor emulation "
			        "CRT shader (sharp variant)");
		}
	} else {
		mode = ShaderMode::Single;
	}

	shader_name_from_config = shader_name;

	MaybeAutoSwitchShader();
}

void ShaderManager::NotifyRenderParametersChanged(const DosBox::Rect new_canvas_size_px,
                                                  const VideoMode& new_video_mode)
{
	// We need to calculate the scale factors for two eventualities: 1)
	// potentially double-scanned, and 2) forced single-scanned output. Then
	// we need to pick the best outcome based on shader availability for the
	// given screen mode.
	//
	// We need to derive the potentially double-scanned dimensions from the
	// video mode, *not* the current render dimensions! That's because we
	// might be in forced single scanning and/or no pixel doubling mode
	// already in the renderer, but that's actually irrelevant for the
	// shader auto-switching algorithm. All in all, it's easiest to start
	// from a fixed, unchanging starting point, which is the "nominal"
	// dimensions of the current video made.

	// 1) Calculate vertical scale factor for the standard output resolution
	// (i.e., always double scanning on VGA).
	//
	pixels_per_scanline = [&] {
		const auto double_scan = new_video_mode.is_double_scanned_mode ? 2 : 1;

		const DosBox::Rect render_size_px = {new_video_mode.width * double_scan,
		                                     new_video_mode.height *
		                                             double_scan};

		const auto draw_rect_px = GFX_CalcDrawRectInPixels(
		        new_canvas_size_px,
		        render_size_px,
		        new_video_mode.pixel_aspect_ratio);

		return iroundf(draw_rect_px.h) / iroundf(render_size_px.h);
	}();

	// 2) Calculate vertical scale factor for forced single scanning on VGA
	// for double-scanned modes.
	if (new_video_mode.is_double_scanned_mode) {
		const DosBox::Rect render_size_px = {new_video_mode.width,
		                                     new_video_mode.height};

		const auto draw_rect_px = GFX_CalcDrawRectInPixels(
		        new_canvas_size_px,
		        render_size_px,
		        new_video_mode.pixel_aspect_ratio);

		pixels_per_scanline_force_single_scan = iroundf(draw_rect_px.h) /
		                                        iroundf(render_size_px.h);
	} else {
		pixels_per_scanline_force_single_scan = pixels_per_scanline;
	}

	video_mode = new_video_mode;

	MaybeAutoSwitchShader();
}

void ShaderManager::LoadShader(const std::string& shader_name)
{
	auto new_shader_name = shader_name;

	if (!ReadShaderSource(new_shader_name, current_shader.source)) {
		current_shader.source.clear();

		// List all the existing shaders for the user
		LOG_ERR("RENDER: Shader file '%s' not found",
		        new_shader_name.c_str());

		for (const auto& line : GenerateShaderInventoryMessage()) {
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

	const bool is_adaptive = [&] {
		if (mode == ShaderMode::Single) {
			return false;

		} else {
			// This will turn off vertical integer scaling for the
			// 'sharp' shader in 'integer_scaling = auto' mode
			return (new_shader_name != SharpShaderName);
		}
	}();

	current_shader.info = {new_shader_name, settings, is_adaptive};
}

const ShaderInfo& ShaderManager::GetCurrentShaderInfo() const
{
	return current_shader.info;
}

const std::string& ShaderManager::GetCurrentShaderSource() const
{
	return current_shader.source;
}

void ShaderManager::ReloadCurrentShader()
{
	LoadShader(current_shader.info.name);
	LOG_MSG("RENDER: Reloaded current shader '%s'",
	        current_shader.info.name.c_str());
}

std::deque<std::string> ShaderManager::GenerateShaderInventoryMessage() const
{
	std::deque<std::string> inventory;
	inventory.emplace_back("");
	inventory.emplace_back(MSG_GetRaw("DOSBOX_HELP_LIST_GLSHADERS_1"));
	inventory.emplace_back("");

	const std::string file_prefix = "        ";
	std::error_code ec            = {};

	constexpr auto OnlyRegularFiles = true;

	for (auto& [dir, shaders] :
	     GetFilesInResource(GlShadersDir, ".glsl", OnlyRegularFiles)) {

		const auto dir_exists      = std_fs::is_directory(dir, ec);
		auto shader                = shaders.begin();
		const auto dir_has_shaders = shader != shaders.end();

		const char* pattern = nullptr;
		if (!dir_exists) {
			pattern = MSG_GetRaw("DOSBOX_HELP_LIST_GLSHADERS_NOT_EXISTS");
		} else if (!dir_has_shaders) {
			pattern = MSG_GetRaw("DOSBOX_HELP_LIST_GLSHADERS_NO_SHADERS");
		} else {
			pattern = MSG_GetRaw("DOSBOX_HELP_LIST_GLSHADERS_LIST");
		}
		inventory.emplace_back(format_str(pattern, dir.u8string().c_str()));

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
	inventory.emplace_back(MSG_GetRaw("DOSBOX_HELP_LIST_GLSHADERS_2"));

	return inventory;
}

void ShaderManager::AddMessages()
{
	MSG_Add("DOSBOX_HELP_LIST_GLSHADERS_1",
	        "List of available GLSL shaders\n"
	        "------------------------------");
	MSG_Add("DOSBOX_HELP_LIST_GLSHADERS_2",
	        "The above shaders can be used exactly as listed in the 'glshader'\n"
	        "config setting, without the need for the resource path or .glsl extension.");

	MSG_Add("DOSBOX_HELP_LIST_GLSHADERS_NOT_EXISTS", "Path '%s' does not exist.");
	MSG_Add("DOSBOX_HELP_LIST_GLSHADERS_NO_SHADERS", "Path '%s' has no shaders.");
	MSG_Add("DOSBOX_HELP_LIST_GLSHADERS_LIST", "Path '%s' has:");
}

std::string ShaderManager::MapShaderName(const std::string& name) const
{
	// Handle empty glshader setting case
	if (name.empty()) {
		return FallbackShaderName;
	}

	// Map shader aliases
	if (name == "sharp") {
		return SharpShaderName;

	} else if (name == "bilinear" || name == "none") {
		return BilinearShaderName;

	} else if (name == "nearest") {
		return "interpolation/nearest";
	}

	// Map legacy shader names
	// clang-format off
	static const std::map<std::string, std::string> legacy_name_mappings = {
		{"advinterp2x", "scaler/advinterp2x"},
		{"advinterp3x", "scaler/advinterp3x"},
		{"advmame2x",   "scaler/advmame2x"},
		{"advmame3x",   "scaler/advmame3x"},
		{"default",     "interpolation/sharp"},
		{"rgb2x",       "scaler/rgb2x"},
		{"rgb3x",       "scaler/rgb3x"},
		{"scan2x",      "scaler/scan2x"},
		{"scan3x",      "scaler/scan3x"},
		{"tv2x",        "scaler/tv2x"},
		{"tv3x",        "scaler/tv3x"}};
	// clang-format on

	std_fs::path shader_path = name;
	std_fs::path ext         = shader_path.extension();

	if (ext == "" || ext == ".glsl") {
		shader_path.replace_extension("");

		const auto old_name = shader_path.string();
		const auto it       = legacy_name_mappings.find(old_name);
		if (it != legacy_name_mappings.end()) {
			const auto new_name = it->second;

			LOG_WARNING(
			        "RENDER: Built-in shader '%s' has been renamed to '%s'; "
			        "using '%s' instead.",
			        old_name.c_str(),
			        new_name.c_str(),
			        new_name.c_str());

			return new_name;
		}
	}

	// No mapping required
	return name;
}

bool ShaderManager::ReadShaderSource(const std::string& shader_name, std::string& source)
{
	auto read_shader = [&](const std_fs::path& path) {
		std::ifstream fshader(path, std::ios_base::binary);
		if (!fshader.is_open()) {
			return false;
		}
		std::stringstream buf;
		buf << fshader.rdbuf();
		fshader.close();

		source = buf.str() + '\n';
		return true;
	};

	constexpr auto glsl_ext = ".glsl";

	// Start with the name as-is and then try from resources
	const auto candidate_paths = {std_fs::path(shader_name),
	                              std_fs::path(shader_name + glsl_ext),
	                              GetResourcePath(GlShadersDir, shader_name),
	                              GetResourcePath(GlShadersDir,
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
		const std::regex re("\\s*#pragma\\s+(\\w+)");
		std::sregex_iterator next(source.begin(), source.end(), re);
		const std::sregex_iterator end;

		while (next != end) {
			std::smatch match = *next;
			auto pragma       = match[1].str();

			if (pragma == "use_npot_texture") {
				settings.use_npot_texture = true;

			} else if (pragma == "use_srgb_texture") {
				settings.use_srgb_texture = true;

			} else if (pragma == "use_srgb_framebuffer") {
				settings.use_srgb_framebuffer = true;

			} else if (pragma == "force_single_scan") {
				settings.force_single_scan = true;

			} else if (pragma == "force_no_pixel_doubling") {
				settings.force_no_pixel_doubling = true;

			} else if (pragma == "use_nearest_texture_filter") {
				settings.texture_filter_mode = TextureFilterMode::Nearest;
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

void ShaderManager::MaybeAutoSwitchShader()
{
	auto maybe_load_shader = [&](const std::string& shader_name) {
		if (current_shader.info.name == shader_name) {
			return false;
		}
		LoadShader(shader_name);
		return true;
	};

	if (mode == ShaderMode::Single) {
		const auto shader_changed = maybe_load_shader(shader_name_from_config);
		if (shader_changed) {
			LOG_MSG("RENDER: Using shader '%s'",
			        current_shader.info.name.c_str());
		}

	} else {
		auto shader_changed = false;

		switch (mode) {
		case ShaderMode::AutoGraphicsStandard:
			shader_changed = maybe_load_shader(
			        FindShaderAutoGraphicsStandard());
			break;

		case ShaderMode::AutoMachine:
			shader_changed = maybe_load_shader(FindShaderAutoMachine());
			break;

		case ShaderMode::AutoArcade:
			shader_changed = maybe_load_shader(FindShaderAutoArcade());
			break;

		case ShaderMode::AutoArcadeSharp:
			shader_changed = maybe_load_shader(
			        FindShaderAutoArcadeSharp());
			break;

		default: assertm(false, "Invalid ShaderMode value");
		}

		if (shader_changed) {
			if (video_mode.has_vga_colors) {
				LOG_MSG("RENDER: EGA mode with custom 18-bit VGA palette "
				        "detected; auto-switching to VGA shader");
			}
			LOG_MSG("RENDER: Auto-switched to shader '%s'",
			        current_shader.info.name.c_str());
		}
	}
}

std::string ShaderManager::GetHerculesShader() const
{
	return "crt/hercules";
}

std::string ShaderManager::GetCgaShader() const
{
	if (video_mode.color_depth == ColorDepth::Monochrome) {
		if (video_mode.width < 640) {
			return "crt/monochrome-lowres";
		} else {
			return "crt/monochrome-hires";
		}
	}
	if (pixels_per_scanline_force_single_scan >= 8) {
		return "crt/cga-4k";
	}
	if (pixels_per_scanline_force_single_scan >= 5) {
		return "crt/cga-1440p";
	}
	if (pixels_per_scanline_force_single_scan >= 4) {
		return "crt/cga-1080p";
	}
	if (pixels_per_scanline_force_single_scan >= 3) {
		return "crt/cga-720p";
	}
	return SharpShaderName;
}

std::string ShaderManager::GetCompositeShader() const
{
	if (pixels_per_scanline >= 8) {
		return "crt/composite-4k";
	}
	if (pixels_per_scanline >= 5) {
		return "crt/composite-1440p";
	}
	if (pixels_per_scanline >= 3) {
		return "crt/composite-1080p";
	}
	return SharpShaderName;
}

std::string ShaderManager::GetEgaShader() const
{
	if (pixels_per_scanline_force_single_scan >= 8) {
		return "crt/ega-4k";
	}
	if (pixels_per_scanline_force_single_scan >= 5) {
		return "crt/ega-1440p";
	}
	if (pixels_per_scanline_force_single_scan >= 4) {
		return "crt/ega-1080p";
	}
	if (pixels_per_scanline_force_single_scan >= 3) {
		return "crt/ega-720p";
	}
	return SharpShaderName;
}

std::string ShaderManager::GetVgaShader() const
{
	if (pixels_per_scanline >= 4) {
		return "crt/vga-4k";
	}
	if (pixels_per_scanline >= 3) {
		return "crt/vga-1440p";
	}
	if (pixels_per_scanline >= 2) {
		// Up to 1080/5 = 216-line double-scanned VGA modes can be
		// displayed with 5x vertical scaling on 1080p screens in
		// fullscreen with forced single scanning and a "fake
		// double scanning" shader that gives the *impression* of
		// double scanning (clearly, our options at 1080p are limited as
		// we'd need 3 pixels per emulated scanline at the very minimum
		// for a somewhat convincing scanline emulation).
		//
		// Without this fake double scanning trick, 320x200 content
		// would be auto-scaled to 1067x800 in fullscreen, which is too
		// small and would not please most users.
		//
		constexpr auto MaxFakeDoubleScanVideoModeHeight = 1080 / 5;

		if (video_mode.is_double_scanned_mode &&
		    video_mode.height <= MaxFakeDoubleScanVideoModeHeight) {
			return "crt/vga-1080p-fake-double-scan";
		} else {
			// This shader works correctly only with exact 2x
			// vertical scaling to make the best out of the very
			// constrained 1080p situation. Luckily, the most common
			// non-double-scanned VGA modes used by games are the
			// 640x480 VGA mode (most common) and the 640x400 mode
			// (much rarer) -- both fit into 1080 pixels of vertical
			// resolution with 2x vertical scaling.
			//
			// Double-scanned 216 to 270 line modes are also handled
			// by this shader.
			//
			return "crt/vga-1080p";
		}
	}
	return SharpShaderName;
}

std::string ShaderManager::FindShaderAutoGraphicsStandard() const
{
	if (video_mode.color_depth == ColorDepth::Composite) {
		return GetCompositeShader();
	}

	switch (video_mode.graphics_standard) {
	case GraphicsStandard::Hercules: return GetHerculesShader();

	case GraphicsStandard::Cga:
	case GraphicsStandard::Pcjr: return GetCgaShader();

	case GraphicsStandard::Tga: return GetEgaShader();

	case GraphicsStandard::Ega:
		// Use VGA shaders for VGA games that use EGA modes with an
		// 18-bit VGA palette (these games won't even work on an EGA
		// card).
		return video_mode.has_vga_colors ? GetVgaShader() : GetEgaShader();

	case GraphicsStandard::Vga:
	case GraphicsStandard::Svga:
	case GraphicsStandard::Vesa: return GetVgaShader();

	default: assertm(false, "Invalid GraphicsStandard value"); return {};
	}
}

std::string ShaderManager::FindShaderAutoMachine() const
{
	if (video_mode.color_depth == ColorDepth::Composite) {
		return GetCompositeShader();
	}

	// DOSBOX_RealInit may have not been run yet.
	// If not, go ahead and set the globals from the config.
	if (machine == MCH_INVALID) {
		DOSBOX_SetMachineTypeFromConfig(
		        static_cast<Section_prop*>(control->GetSection("dosbox")));
	}

	switch (machine) {
	case MCH_HERC: return GetHerculesShader();

	case MCH_CGA:
	case MCH_PCJR: return GetCgaShader();

	case MCH_TANDY:
	case MCH_EGA: return GetEgaShader();

	case MCH_VGA: return GetVgaShader();
	default: assertm(false, "Invalid MachineType value"); return {};
	};
}

std::string ShaderManager::FindShaderAutoArcade() const
{
	if (pixels_per_scanline_force_single_scan >= 8) {
		return "crt/arcade-4k";
	}
	if (pixels_per_scanline_force_single_scan >= 5) {
		return "crt/arcade-1440p";
	}
	if (pixels_per_scanline_force_single_scan >= 3) {
		return "crt/arcade-1080p";
	}
	return SharpShaderName;
}

std::string ShaderManager::FindShaderAutoArcadeSharp() const
{
	if (pixels_per_scanline_force_single_scan >= 8) {
		return "crt/arcade-sharp-4k";
	}
	if (pixels_per_scanline_force_single_scan >= 5) {
		return "crt/arcade-sharp-1440p";
	}
	if (pixels_per_scanline_force_single_scan >= 3) {
		return "crt/arcade-sharp-1080p";
	}
	return SharpShaderName;
}
