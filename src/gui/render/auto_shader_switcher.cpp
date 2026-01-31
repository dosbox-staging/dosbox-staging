// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/auto_shader_switcher.h"

#include <algorithm>
#include <cassert>
#include <fstream>

#include <SDL.h>

#include "dosbox.h"
#include "gui/render/render_backend.h"
#include "misc/notifications.h"
#include "utils/checks.h"
#include "utils/math_utils.h"

CHECK_NARROWING();

static ShaderDescriptor parse_shader_descriptor(const std::string& descriptor,
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

void AutoShaderSwitcher::NotifyShaderChanged(const std::string& shader_descriptor,
                                             const std::string& extension)
{
	assert(!shader_descriptor.empty());
	assert(!extension.empty());

	const auto descriptor = parse_shader_descriptor(shader_descriptor, extension);

	const auto shader_name = MapShaderName(descriptor.shader_name);

	auto store_descriptor_and_maybe_autoswitch = [&]() {
		current_shader.descriptor = {shader_name, descriptor.preset_name};
		MaybeAutoSwitchShader();
	};

	using enum ShaderMode;

	if (shader_name == SymbolicShaderName::AutoGraphicsStandard) {
		if (current_shader.mode != AutoGraphicsStandard) {
			current_shader.mode = AutoGraphicsStandard;
			LOG_MSG("RENDER: Using adaptive CRT shader based on the graphics "
			        "standard of the video mode");

			store_descriptor_and_maybe_autoswitch();
		}
	} else if (shader_name == SymbolicShaderName::AutoMachine) {
		if (current_shader.mode != AutoMachine) {
			current_shader.mode = AutoMachine;

			LOG_MSG("RENDER: Using adaptive CRT shader based on the "
			        "configured graphics adapter");

			store_descriptor_and_maybe_autoswitch();
		}
	} else if (shader_name == SymbolicShaderName::AutoArcade) {
		if (current_shader.mode != AutoArcade) {
			current_shader.mode = AutoArcade;

			LOG_MSG("RENDER: Using adaptive arcade monitor emulation "
			        "CRT shader (normal variant)");

			store_descriptor_and_maybe_autoswitch();
		}
	} else if (shader_name == SymbolicShaderName::AutoArcadeSharp) {
		if (current_shader.mode != AutoArcadeSharp) {
			current_shader.mode = AutoArcadeSharp;

			LOG_MSG("RENDER: Using adaptive arcade monitor emulation "
			        "CRT shader (sharp variant)");

			store_descriptor_and_maybe_autoswitch();
		}
	} else {
		current_shader.mode = Single;
		current_shader.descriptor = {shader_name, descriptor.preset_name};

		LOG_MSG("RENDER: Using shader '%s'",
		        current_shader.descriptor.ToString().c_str());
	}
}

void AutoShaderSwitcher::NotifyRenderParametersChanged(
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

	MaybeAutoSwitchShader();
}

std::string AutoShaderSwitcher::MapShaderName(const std::string& name) const
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
	static const std::unordered_map<std::string, std::string> legacy_name_mappings = {
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

ShaderDescriptor AutoShaderSwitcher::GetCurrentShaderDescriptor() const
{
	return current_shader.descriptor;
}

ShaderMode AutoShaderSwitcher::GetCurrentShaderMode() const
{
	return current_shader.mode;
}

void AutoShaderSwitcher::MaybeAutoSwitchShader()
{
	using enum ShaderMode;

	const auto new_shader_descriptor = [&]() -> ShaderDescriptor {
		switch (current_shader.mode) {
		case Single: return current_shader.descriptor;

		case AutoGraphicsStandard:
			return FindShaderAutoGraphicsStandard();

		case AutoMachine: return FindShaderAutoMachine();

		case AutoArcade: return FindShaderAutoArcade();
		case AutoArcadeSharp: return FindShaderAutoArcadeSharp();

		default: assertm(false, "Invalid ShaderMode value"); return {};
		}
	}();

	if (current_shader.descriptor == new_shader_descriptor) {
		return;
	}

	current_shader.descriptor = new_shader_descriptor;

	if (video_mode.has_vga_colors) {
		LOG_MSG("RENDER: EGA mode with custom 18-bit VGA palette "
		        "detected; auto-switching to VGA shader");
	}
	LOG_MSG("RENDER: Auto-switched to shader '%s'",
	        current_shader.descriptor.ToString().c_str());
}

ShaderDescriptor AutoShaderSwitcher::GetHerculesShader() const
{
	return {ShaderName::CrtHyllian, "hercules"};
}

ShaderDescriptor AutoShaderSwitcher::GetCgaShader() const
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

ShaderDescriptor AutoShaderSwitcher::GetCompositeShader() const
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

ShaderDescriptor AutoShaderSwitcher::GetEgaShader() const
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

ShaderDescriptor AutoShaderSwitcher::GetVgaShader() const
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

ShaderDescriptor AutoShaderSwitcher::FindShaderAutoGraphicsStandard() const
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

ShaderDescriptor AutoShaderSwitcher::FindShaderAutoMachine() const
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

ShaderDescriptor AutoShaderSwitcher::FindShaderAutoArcade() const
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

ShaderDescriptor AutoShaderSwitcher::FindShaderAutoArcadeSharp() const
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
