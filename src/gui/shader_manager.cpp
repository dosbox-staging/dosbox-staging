// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/shader_manager.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <utility>

#include <SDL.h>
#include <simpleini/SimpleIni.h>

#include "dosbox.h"
#include "gui/render/render_backend.h"
#include "misc/cross.h"
#include "misc/video.h"
#include "utils/checks.h"
#include "utils/math_utils.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

void ShaderManager::NotifyShaderNameChanged(const std::string& _shader_name)
{
	const auto [shader_name, preset_name] = SplitShaderName(_shader_name);

	// TODO
	constexpr auto GlslExtension = ".glsl";
	const auto symbolic_name = MapShaderName(symbolic_name, GlslExtension);

	if (symbolic_name == SymbolicShaderName::AutoGraphicsStandard) {
		if (current_shader.mode != ShaderMode::AutoGraphicsStandard) {
			current_shader.mode = ShaderMode::AutoGraphicsStandard;
			LOG_MSG("RENDER: Using adaptive CRT shader based on the graphics "
			        "standard of the video mode");
		}
	} else if (symbolic_name == SymbolicShaderName::AutoMachine) {
		if (current_shader.mode != ShaderMode::AutoMachine) {
			current_shader.mode = ShaderMode::AutoMachine;

			LOG_MSG("RENDER: Using adaptive CRT shader based on the "
			        "configured graphics adapter");
		}
	} else if (symbolic_name == SymbolicShaderName::AutoArcade) {
		if (current_shader.mode != ShaderMode::AutoArcade) {
			current_shader.mode = ShaderMode::AutoArcade;

			LOG_MSG("RENDER: Using adaptive arcade monitor emulation "
			        "CRT shader (normal variant)");
		}
	} else if (symbolic_name == SymbolicShaderName::AutoArcadeSharp) {
		if (current_shader.mode != ShaderMode::AutoArcadeSharp) {
			current_shader.mode = ShaderMode::AutoArcadeSharp;

			LOG_MSG("RENDER: Using adaptive arcade monitor emulation "
			        "CRT shader (sharp variant)");
		}
	} else {
		current_shader.mode = ShaderMode::Single;
	}

	current_shader.symbolic_name = symbolic_name;
	current_shader.preset_name   = preset_name;

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

		const auto draw_rect_px = RENDER_CalcDrawRectInPixels(
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

		const auto draw_rect_px = RENDER_CalcDrawRectInPixels(
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

std::optional<std::pair<ShaderInfo, std::string>> ShaderManager::LoadShader(
        const std::string& _mapped_name)
{
	auto mapped_name = _mapped_name;

	auto maybe_source = FindShaderAndReadSource(mapped_name);
	if (!maybe_source) {
		// List all the existing shaders for the user
		// TODO convert to notification
		LOG_ERR("RENDER: Shader file '%s' not found", mapped_name.c_str());

		for (const auto& line : GenerateShaderInventoryMessage()) {
			// TODO convert to notification (maybe?)
			LOG_WARNING("RENDER: %s", line.c_str());
		}
		return {};
	}

	const auto source = *maybe_source;
	const auto default_preset = ParseDefaultShaderPreset(mapped_name, source);

	const bool is_adaptive = [&] {
		if (current_shader.mode == ShaderMode::Single) {
			return false;

		} else {
			// This will turn off vertical integer scaling for the
			// 'sharp' shader in 'integer_scaling = auto' mode
			return (mapped_name != MappedShaderName::Sharp);
		}
	}();

	const ShaderInfo shader_info = {mapped_name, default_preset, is_adaptive};

	return std::pair{shader_info, source};
}

std::optional<ShaderPreset> ShaderManager::LoadShaderPreset(
        const std::string& mapped_shader_name, const std::string& preset_name,
        const ShaderPreset& default_preset) const
{
	assert(!mapped_shader_name.empty());
	assert(!preset_name.empty());

	const auto path = get_resource_path(ShaderPresetsDir,
	                                    std_fs::path{mapped_shader_name} /
	                                            preset_name);

	// TODO get_resource_path() should return optional
	if (path.empty()) {
		return {};
	}

	const auto file_exists = std_fs::exists(path) &&
	                         (std_fs::is_regular_file(path) ||
	                          std_fs::is_symlink(path));
	if (!file_exists) {
		return {};
	}

	CSimpleIniA ini;
	ini.SetUnicode();

	const auto result = ini.LoadFile(path.string().c_str());
	if (result < 0) {
		return {};
	}

	ShaderPreset preset = default_preset;

	const auto settings = ini.GetSection("settings");
	if (settings) {
		for (auto it = settings->begin(); it != settings->end(); ++it) {
			const auto [name, value] = *it;
			SetShaderSetting(name.pItem, value, preset.settings);
		}
	}

	const auto params = ini.GetSection("parameters");
	if (params) {
		for (const auto& [key, value_str] : params) {
			const auto name = key.pItem;
			SetShaderSetting(name, value_str, preset.settings);

			if (!default_preset.params.contains(name)) {
				LOG_WARNING("RENDER: Invalid shader parameter name: '%s'",
				            name);
			} else {
				const auto value = parse_float(value_str);
				if (!value) {
					LOG_WARNING(
					        "RENDER: Invalid value for shader parameter '%s' "
					        "(must be float): '%s'",
					        name,
					        value_str.c_str());
				} else {
					shader.params[name] = value;
				}
			}
		}
	}

	return preset;
}

std::string ShaderManager::GetCurrentMappedShaderName() const
{
	return current_shader.mapped_name;
}

std::deque<std::string> ShaderManager::GenerateShaderInventoryMessage() const
{
	std::deque<std::string> inventory;
	inventory.emplace_back("");
	inventory.emplace_back(MSG_GetTranslatedRaw("DOSBOX_HELP_LIST_GLSHADERS_1"));
	inventory.emplace_back("");

	const std::string file_prefix = "        ";

	std::error_code ec = {};

	constexpr auto OnlyRegularFiles = true;

	for (const auto& parent : get_resource_parent_paths()) {
		// TODO Handling the optional extensions should be probably
		// handled by the render backend once we add more backends with
		// shader support.
		auto shaders = get_directory_entries(dir, ".glsl", OnlyRegularFiles);

		const auto dir_exists      = std_fs::is_directory(dir, ec);
		auto shader                = shaders.begin();
		const auto dir_has_shaders = shader != shaders.end();

		std::string pattern = {};
		if (!dir_exists) {
			pattern = MSG_GetTranslatedRaw(
			        "DOSBOX_HELP_LIST_GLSHADERS_NOT_EXISTS");

		} else if (!dir_has_shaders) {
			pattern = MSG_GetTranslatedRaw(
			        "DOSBOX_HELP_LIST_GLSHADERS_NO_SHADERS");

		} else {
			pattern = MSG_GetTranslatedRaw(
			        "DOSBOX_HELP_LIST_GLSHADERS_LIST");
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

	inventory.emplace_back(MSG_GetTranslatedRaw("DOSBOX_HELP_LIST_GLSHADERS_2"));

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

std::pair<std::string, std::string> ShaderManager::SplitShaderName(
        const std::string& shader_name) const
{
	const auto parts = split(shader_name, ":");

	const auto shader_name  = parts[0];
	std::string preset_name = "";

	if (parts.size() > 1) {
		preset_name = parts[1];
	}
	return {shader_name, preset_name};
}

std::string ShaderManager::MapShaderName(const std::string& symbolic_name,
                                         const std::string& file_extension) const
{
	// Handle empty glshader setting case
	if (symbolic_name.empty()) {
		// Fallback to the sharp shader
		return MappedShaderName::Sharp;
	}

	// Map shader aliases
	if (symbolic_name == "sharp") {
		return MappedShaderName::Sharp;

	} else if (symbolic_name == "bilinear" || symbolic_name == "none") {
		return "interpolation/bilinear";

	} else if (symbolic_name == "nearest") {
		return "interpolation/nearest";
	}

	// Map legacy shader names
	// clang-format off
	static const std::map<std::string, std::string> legacy_name_mappings = {
		{"advinterp2x", "scaler/advinterp2x"},
		{"advinterp3x", "scaler/advinterp3x"},
		{"advmame2x",   "scaler/advmame2x"},
		{"advmame3x",   "scaler/advmame3x"},
		{"default",     "interpolation/sharp"}
	};
	// clang-format on

	std_fs::path shader_path = symbolic_name;
	if (shader_path.extension() == file_extension) {
		shader_path.replace_extension("");
	}

	const auto legacy_name = shader_path.string();

	const auto it = legacy_name_mappings.find(legacy_name);
	if (it != legacy_name_mappings.end()) {
		const auto new_name = it->second;

		// TODO convert to notification
		LOG_WARNING(
		        "RENDER: Built-in shader '%s' has been renamed to '%s'; "
		        "using '%s' instead.",
		        legacy_name.c_str(),
		        new_name.c_str(),
		        new_name.c_str());

		return new_name;
	}

	// No mapping required
	return symbolic_name;
}

std::optional<std::string> ShaderManager::FindShaderAndReadSource(const std::string& mapped_name)
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
	std::vector<std_fs::path> candidate_paths = {mapped_name};

	// ...and then try from resources
	const auto resource_shader_path = get_resource_path(GlShadersDir, mapped_name);

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

ShaderPreset ShaderManager::ParseDefaultShaderPreset(const std::string& mapped_name,
                                                     const std::string& shader_source) const
{
	ShaderPreset preset = {};

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

					preset.params[param_name] = default_value;
				} else {
					LOG_ERR("RENDER: Invalid shader parameter: '%s'",
					        pragma.c_str());
				}
			} else {
				SetShaderSetting(pragma, "on", preset.settings);
			}

			++next;
		}
	} catch (std::regex_error& e) {
		LOG_ERR("RENDER: Regex error while parsing shader '%s' for pragmas: %d",
		        mapped_name.c_str(),
		        e.code());
	}

	return preset;
}

void ShaderManager::SetShaderSetting(const std::string& name, const std::string& value,
                                     ShaderSettings& settings) const
{
	assert(!name.empty());

	const auto is_true = (value == "1") || has_true(value);

	if (name == "use_srgb_texture") {
		settings.use_srgb_texture = is_true;

	} else if (name == "use_srgb_framebuffer") {
		settings.use_srgb_framebuffer = is_true;

	} else if (name == "force_single_scan") {
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

	const auto maybe_default_val = parse_float(params[1].c_str());
	if (!maybe_default_val) {
		return {};
	}

	return {{param_name, *maybe_default_val}};
}

void ShaderManager::MaybeAutoSwitchShader()
{
	const auto new_shader = [&] {
		switch (current_shader.mode) {
		case ShaderMode::Single:
			return {current_shader.symbolic_name, ""};

		case ShaderMode::AutoGraphicsStandard:
			return FindShaderAutoGraphicsStandard();

		case ShaderMode::AutoMachine: return FindShaderAutoMachine();

		case ShaderMode::AutoArcade: return FindShaderAutoArcade();

		case ShaderMode::AutoArcadeSharp:
			return FindShaderAutoArcadeSharp();

		default:
			assertm(false, "Invalid ShaderMode value");
			return std::string{""};
		}
	}();

	if (current_shader.mapped_name == new_shader.mapped_name &&
	    current_shader.preset_name == new_shader.preset_name) {
		return;
	}

	current_shader.mapped_name = new_shader.mapped_name;
	current_shader.preset_name = new_shader.preset_name;

	if (current_shader.mode == ShaderMode::Single) {
		LOG_MSG("RENDER: Using shader '%s'",
		        current_shader.mapped_name.c_str());
	} else {
		if (video_mode.has_vga_colors) {
			LOG_MSG("RENDER: EGA mode with custom 18-bit VGA palette "
			        "detected; auto-switching to VGA shader");
		}
		LOG_MSG("RENDER: Auto-switched to shader '%s'",
		        current_shader.mapped_name.c_str());
	}
}

ShaderAndPreset ShaderManager::GetHerculesShader() const
{
	return {MappedShaderName::CrtHyllian, "hercules"};
}

ShaderAndPreset ShaderManager::GetCgaShader() const
{
	if (video_mode.color_depth == ColorDepth::Monochrome) {
		if (video_mode.width < 640) {
			return {MappedShaderName::CrtHyllian, "monochrome-lowres"};
		} else {
			return {MappedShaderName::CrtHyllian, "monochrome-hires"};
		}
	}
	if (pixels_per_scanline_force_single_scan >= 8) {
		return {MappedShaderName::CrtHyllian, "cga-4k"};
	}
	if (pixels_per_scanline_force_single_scan >= 5) {
		return {MappedShaderName::CrtHyllian, "cga-1440p"};
	}
	if (pixels_per_scanline_force_single_scan >= 4) {
		return {MappedShaderName::CrtHyllian, "cga-1080p"};
	}
	if (pixels_per_scanline_force_single_scan >= 3) {
		return {MappedShaderName::CrtHyllian, "cga-720p"};
	}
	return {MappedShaderName::Sharp, ""};
}

ShaderAndPreset ShaderManager::GetCompositeShader() const
{
	if (pixels_per_scanline >= 8) {
		return {MappedShaderName::CrtHyllian, "composite-4k"};
	}
	if (pixels_per_scanline >= 5) {
		return {MappedShaderName::CrtHyllian, "composite-1440p"};
	}
	if (pixels_per_scanline >= 3) {
		return {MappedShaderName::CrtHyllian, "composite-1080p"};
	}
	return {MappedShaderName::Sharp, ""};
}

ShaderAndPreset ShaderManager::GetEgaShader() const
{
	if (pixels_per_scanline_force_single_scan >= 8) {
		return {MappedShaderName::CrtHyllian, "ega-4k"};
	}
	if (pixels_per_scanline_force_single_scan >= 5) {
		return {MappedShaderName::CrtHyllian, "ega-1440p"};
	}
	if (pixels_per_scanline_force_single_scan >= 4) {
		return {MappedShaderName::CrtHyllian, "ega-1080p"};
	}
	if (pixels_per_scanline_force_single_scan >= 3) {
		return {MappedShaderName::CrtHyllian, "ega-720p"};
	}
	return {MappedShaderName::Sharp, ""};
}

ShaderAndPreset ShaderManager::GetVgaShader() const
{
	if (pixels_per_scanline >= 4) {
		return {MappedShaderName::CrtHyllian, "vga-4k"};
	}
	if (pixels_per_scanline >= 3) {
		return {MappedShaderName::CrtHyllian, "vga-1440p"};
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
			return {"crt/vga-1080p-fake-double-scan", ""};

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
			return {"crt/vga-1080p", ""};
		}
	}
	return {MappedShaderName::Sharp, ""};
}

ShaderAndPreset ShaderManager::FindShaderAutoGraphicsStandard() const
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

ShaderAndPreset ShaderManager::FindShaderAutoMachine() const
{
	if (video_mode.color_depth == ColorDepth::Composite) {
		return GetCompositeShader();
	}

	// TODO still needed?
	// DOSBOX_RealInit may have not been run yet.
	// If not, go ahead and set the globals from the config.
	if (machine == MachineType::None) {
		DOSBOX_SetMachineTypeFromConfig(*get_section("dosbox"));
	}

	switch (machine) {
	case MachineType::Hercules: return GetHerculesShader();

	case MachineType::CgaMono:
	case MachineType::CgaColor:
	case MachineType::Pcjr: return GetCgaShader();

	case MachineType::Tandy:
	case MachineType::Ega: return GetEgaShader();

	case MachineType::Vga: return GetVgaShader();
	default: assertm(false, "Invalid MachineType value"); return {};
	};
}

ShaderAndPreset ShaderManager::FindShaderAutoArcade() const
{
	if (pixels_per_scanline_force_single_scan >= 8) {
		return {MappedShaderName::CrtHyllian, "arcade-4k"};
	}
	if (pixels_per_scanline_force_single_scan >= 5) {
		return {MappedShaderName::CrtHyllian, "arcade-1440p"};
	}
	if (pixels_per_scanline_force_single_scan >= 3) {
		return {MappedShaderName::CrtHyllian, "arcade-1080p"};
	}
	return {MappedShaderName::Sharp, ""};
}

ShaderAndPreset ShaderManager::FindShaderAutoArcadeSharp() const
{
	if (pixels_per_scanline_force_single_scan >= 8) {
		return {MappedShaderName::CrtHyllian, "arcade-sharp-4k"};
	}
	if (pixels_per_scanline_force_single_scan >= 5) {
		return {MappedShaderName::CrtHyllian, "arcade-sharp-1440p"};
	}
	if (pixels_per_scanline_force_single_scan >= 3) {
		return {MappedShaderName::CrtHyllian, "arcade-sharp-1080p"};
	}
	return {MappedShaderName::Sharp, ""};
}
