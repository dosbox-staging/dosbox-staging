// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weffc++"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#include <simpleini/SimpleIni.h>
#pragma GCC diagnostic pop
#pragma clang diagnostic pop

#include "dosbox.h"
#include "gui/render/render_backend.h"
#include "misc/cross.h"
#include "misc/notifications.h"
#include "misc/video.h"
#include "utils/checks.h"
#include "utils/math_utils.h"
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

ShaderDescriptor ShaderManager::NotifyShaderChanged(const std::string& symbolic_shader_descriptor,
                                                    const std::string& extension)
{
	assert(!symbolic_shader_descriptor.empty());
	assert(!extension.empty());

	const auto shader_descriptor = ShaderDescriptor::FromString(symbolic_shader_descriptor,
	                                                            extension);

	const auto mapped_shader_name = MapShaderName(shader_descriptor.shader_name);

	const ShaderDescriptor new_shader_descriptor = {mapped_shader_name,
	                                                shader_descriptor.preset_name};

	using enum ShaderMode;

	if (mapped_shader_name == SymbolicShaderName::AutoGraphicsStandard) {
		if (current_shader_mode != AutoGraphicsStandard) {

			current_shader_mode = AutoGraphicsStandard;
			LOG_MSG("RENDER: Using adaptive CRT shader based on the graphics "
			        "standard of the video mode");
		}
	} else if (mapped_shader_name == SymbolicShaderName::AutoMachine) {
		if (current_shader_mode != AutoMachine) {

			current_shader_mode = AutoMachine;
			LOG_MSG("RENDER: Using adaptive CRT shader based on the "
			        "configured graphics adapter");
		}
	} else if (mapped_shader_name == SymbolicShaderName::AutoArcade) {
		if (current_shader_mode != AutoArcade) {

			current_shader_mode = AutoArcade;
			LOG_MSG("RENDER: Using adaptive arcade monitor emulation "
			        "CRT shader (normal variant)");
		}
	} else if (mapped_shader_name == SymbolicShaderName::AutoArcadeSharp) {
		if (current_shader_mode != AutoArcadeSharp) {

			current_shader_mode = AutoArcadeSharp;
			LOG_MSG("RENDER: Using adaptive arcade monitor emulation "
			        "CRT shader (sharp variant)");
		}
	} else {
		current_shader_mode = Single;

		LOG_MSG("RENDER: Using shader '%s'",
		        new_shader_descriptor.ToString().c_str());
	}

	last_shader_descriptor = MaybeAutoSwitchShader(new_shader_descriptor);
	return last_shader_descriptor;
}

ShaderDescriptor ShaderManager::NotifyRenderParametersChanged(
        const ShaderDescriptor& curr_shader_descriptor,
        const DosBox::Rect new_canvas_size_px, const VideoMode& new_video_mode)
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

	last_shader_descriptor = MaybeAutoSwitchShader(curr_shader_descriptor);
	return last_shader_descriptor;
}

std::optional<std::pair<ShaderInfo, std::string>> ShaderManager::LoadShader(
        const std::string& shader_name, const std::string& extension)
{
	auto maybe_shader_source = FindShaderAndReadSource(shader_name + extension);
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
	const auto default_preset = ParseDefaultShaderPreset(shader_name,
	                                                     shader_source);

	const bool is_adaptive = [&] {
		if (current_shader_mode == ShaderMode::Single) {
			return false;

		} else {
			// This will turn off vertical integer scaling for the
			// 'sharp' shader in 'integer_scaling = auto' mode
			return (shader_name != ShaderName::Sharp);
		}
	}();

	const ShaderInfo shader_info = {shader_name, default_preset, is_adaptive};

	return std::pair{shader_info, shader_source};
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

ShaderMode ShaderManager::GetCurrentShaderMode() const
{
	return current_shader_mode;
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

std::string ShaderManager::MapShaderName(const std::string& name) const
{
	// Map shader aliases
	if (name == "sharp") {
		return ShaderName::Sharp;

	} else if (name == "bilinear" || name == "none") {
		return "interpolation/bilinear";

	} else if (name == "nearest") {
		return "interpolation/nearest";
	} else if (name == "jinc2") {
		return "interpolation/jinc2";
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

	const auto it = legacy_name_mappings.find(name);
	if (it != legacy_name_mappings.end()) {
		const auto new_name = it->second;

		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "RENDER",
		                      "RENDER_SHADER_RENAMED",
		                      name.c_str(),
		                      new_name.c_str(),
		                      new_name.c_str());
		return new_name;
	}

	// No mapping required
	return name;
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

ShaderPreset ShaderManager::ParseDefaultShaderPreset(const std::string& shader_name,
                                                     const std::string& shader_source) const
{
	ShaderPreset preset = {};

	// The default preset has no name
	preset.name.clear();

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
		        shader_name.c_str(),
		        e.code());
	}

	return preset;
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

ShaderDescriptor ShaderManager::MaybeAutoSwitchShader(
        const ShaderDescriptor& new_shader_descriptor) const
{
	using enum ShaderMode;

	const auto new_descriptor = [&]() -> ShaderDescriptor {
		switch (current_shader_mode) {
		case Single: return new_shader_descriptor;

		case AutoGraphicsStandard:
			return FindShaderAutoGraphicsStandard();

		case AutoMachine: return FindShaderAutoMachine();

		case AutoArcade: return FindShaderAutoArcade();
		case AutoArcadeSharp: return FindShaderAutoArcadeSharp();

		default: assertm(false, "Invalid ShaderMode value"); return {};
		}
	}();

	if (last_shader_descriptor != new_descriptor) {
		if (current_shader_mode == ShaderMode::AutoGraphicsStandard &&
		    video_mode.has_vga_colors) {

			LOG_MSG("RENDER: EGA mode with custom 18-bit VGA palette "
			        "detected; auto-switching to VGA shader");
		}
	}
	if (current_shader_mode != Single) {
		LOG_MSG("RENDER: Auto-switched to shader '%s'",
		        new_descriptor.ToString().c_str());
	}

	return new_descriptor;
}

ShaderDescriptor ShaderManager::GetHerculesShader() const
{
	return {ShaderName::CrtHyllian, "hercules"};
}

ShaderDescriptor ShaderManager::GetCgaShader() const
{
	using namespace ShaderName;

	if (video_mode.color_depth == ColorDepth::Monochrome) {
		if (video_mode.width < 640) {
			return {CrtHyllian, "monochrome-lowres"};
		} else {
			return {CrtHyllian, "monochrome-hires"};
		}
	}
	if (pixels_per_scanline_force_single_scan >= 8) {
		return {CrtHyllian, "cga-4k"};
	}
	if (pixels_per_scanline_force_single_scan >= 5) {
		return {CrtHyllian, "cga-1440p"};
	}
	if (pixels_per_scanline_force_single_scan >= 4) {
		return {CrtHyllian, "cga-1080p"};
	}
	if (pixels_per_scanline_force_single_scan >= 3) {
		return {CrtHyllian, "cga-720p"};
	}
	return {Sharp, ""};
}

ShaderDescriptor ShaderManager::GetCompositeShader() const
{
	using namespace ShaderName;

	if (pixels_per_scanline >= 8) {
		return {CrtHyllian, "composite-4k"};
	}
	if (pixels_per_scanline >= 5) {
		return {CrtHyllian, "composite-1440p"};
	}
	if (pixels_per_scanline >= 3) {
		return {CrtHyllian, "composite-1080p"};
	}
	return {Sharp, ""};
}

ShaderDescriptor ShaderManager::GetEgaShader() const
{
	using namespace ShaderName;

	if (pixels_per_scanline_force_single_scan >= 8) {
		return {CrtHyllian, "ega-4k"};
	}
	if (pixels_per_scanline_force_single_scan >= 5) {
		return {CrtHyllian, "ega-1440p"};
	}
	if (pixels_per_scanline_force_single_scan >= 4) {
		return {CrtHyllian, "ega-1080p"};
	}
	if (pixels_per_scanline_force_single_scan >= 3) {
		return {CrtHyllian, "ega-720p"};
	}
	return {Sharp, ""};
}

ShaderDescriptor ShaderManager::GetVgaShader() const
{
	using namespace ShaderName;

	if (pixels_per_scanline >= 4) {
		return {CrtHyllian, "vga-4k"};
	}
	if (pixels_per_scanline >= 3) {
		return {CrtHyllian, "vga-1440p"};
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
	return {Sharp, ""};
}

ShaderDescriptor ShaderManager::FindShaderAutoGraphicsStandard() const
{
	if (video_mode.color_depth == ColorDepth::Composite) {
		return GetCompositeShader();
	}

	using enum GraphicsStandard;

	switch (video_mode.graphics_standard) {
	case Hercules: return GetHerculesShader();

	case Cga:
	case Pcjr: return GetCgaShader();

	case Tga: return GetEgaShader();

	case Ega:
		// Use VGA shaders for VGA games that use EGA modes with an
		// 18-bit VGA palette (these games won't even work on an EGA
		// card).
		return video_mode.has_vga_colors ? GetVgaShader() : GetEgaShader();

	case Vga:
	case Svga:
	case Vesa: return GetVgaShader();

	default: assertm(false, "Invalid GraphicsStandard value"); return {};
	}
}

ShaderDescriptor ShaderManager::FindShaderAutoMachine() const
{
	if (video_mode.color_depth == ColorDepth::Composite) {
		return GetCompositeShader();
	}

	using enum MachineType;

	switch (machine) {
	case Hercules: return GetHerculesShader();

	case CgaMono:
	case CgaColor:
	case Pcjr: return GetCgaShader();

	case Tandy:
	case Ega: return GetEgaShader();

	case Vga: return GetVgaShader();
	default: assertm(false, "Invalid MachineType value"); return {};
	};
}

ShaderDescriptor ShaderManager::FindShaderAutoArcade() const
{
	using namespace ShaderName;

	if (pixels_per_scanline_force_single_scan >= 8) {
		return {CrtHyllian, "arcade-4k"};
	}
	if (pixels_per_scanline_force_single_scan >= 5) {
		return {CrtHyllian, "arcade-1440p"};
	}
	if (pixels_per_scanline_force_single_scan >= 3) {
		return {CrtHyllian, "arcade-1080p"};
	}
	return {Sharp, ""};
}

ShaderDescriptor ShaderManager::FindShaderAutoArcadeSharp() const
{
	using namespace ShaderName;

	if (pixels_per_scanline_force_single_scan >= 8) {
		return {CrtHyllian, "arcade-sharp-4k"};
	}
	if (pixels_per_scanline_force_single_scan >= 5) {
		return {CrtHyllian, "arcade-sharp-1440p"};
	}
	if (pixels_per_scanline_force_single_scan >= 3) {
		return {CrtHyllian, "arcade-sharp-1080p"};
	}
	return {Sharp, ""};
}
