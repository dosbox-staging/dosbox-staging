// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SHADER_H
#define DOSBOX_SHADER_H

#include <string>

#include "gui/private/common.h"

// Glad must be included before SDL
#include "glad/gl.h"

namespace SymbolicShaderName {

constexpr auto AutoGraphicsStandard = "crt-auto";
constexpr auto AutoMachine          = "crt-auto-machine";
constexpr auto AutoArcade           = "crt-auto-arcade";
constexpr auto AutoArcadeSharp      = "crt-auto-arcade-sharp";
} // namespace SymbolicShaderName

namespace ShaderName {

constexpr auto CrtHyllian = "crt/crt-hyllian";
constexpr auto Sharp      = "interpolation/sharp";
} // namespace ShaderName

enum class ShaderMode {
	// No shader auto-switching; the 'shader' setting always contains the
	// name of the shader in use.
	//
	Single,

	// Graphics-standard-based adaptive CRT shader mode.
	// Enabled with the 'crt-auto' magic 'shader' setting.
	//
	// The most appropriate shader is auto-selected based on the graphic
	// standard of the current video mode and the viewport resolution. E.g.,
	// CGA modes will always use the 'crt/cga-*' shaders, EGA modes the
	// 'crt/ega-*' shaders, and so on, regardless of the machine type. In
	// other words, the choice of the shader is governed by the graphics
	// standard of the current video mode, *not* the emulated video adapter.
	//
	// As most users leave the 'machine' setting at the 'svga_s3' default,
	// this mode gives them single-scanned CRT emulation in CGA and EGA modes,
	// providing a more authentic out-of-the-box experience (authentic as in
	// "how people experienced the game at the time of release", and
	// prioritising the most probable developer intent.)
	//
	// For CGA and EGA modes that reprogram the 18-bit DAC palette on VGA
	// adapters, a double-scanned VGA shader is selected. This is authentic as
	// these games require a VGA adapter, therefore they were designed with
	// double scanning in mind. In other words, no one could have experienced
	// them on single scanning CGA and EGA monitors without special hardware
	// hacks.
	//
	AutoGraphicsStandard,

	// Machine-based adaptive CRT shader mode.
	// Enabled via the 'crt-machine-auto' magic 'shader' setting.
	//
	// This mode emulates a computer (machine) equipped with the configured
	// video adapter and a matching monitor. The auto-switching picks the most
	// approriate shader variant for the adapter & monitor combo (Hercules,
	// CGA, EGA, (S)VGA, etc.) for a given viewport resolution.
	//
	// E.g., CGA and EGA modes on an emulated VGA adapter type will always use
	// 'crt/vga-*' shaders, on an EGA adapter always the 'crt/ega-*' shaders,
	// and so on.
	//
	AutoMachine,

	// 15 kHz arcade / home computer monitor adaptive CRT shader mode.
	// Enabled via the 'crt-machine-arcade' magic 'shader' setting.
	//
	// This basically forces single scanning of all double-scanned VGA modes
	// and no pixel doubling in all modes to achieve a somewhat less sharp
	// look with more blending and "rounder" pixels than what you'd get on a
	// typical sharp EGA/VGA PC monitor.
	//
	// This is by no means "authentic", but a lot of fun with certain games,
	// plus it allows you to play DOS ports of Amiga games or other 16-bit
	// home computers with a single-scanned 15 kHz monitor look.
	//
	AutoArcade,

	// A sharper variant of the arcade shader. It's the exact same shader but
	// with pixel doubling enabled.
	AutoArcadeSharp
};

/*
 * The shader descriptor is in the `SHADER_NAME[:SHADER_PRESET]` format
 * where `SHADER_NAME` can refer to the filename of an actual shader on
 * the filesystem, a symbolic alias, or a "meta-shader". Specifying
 * `SHADER_PRESET` after a colon is optional (the default preset is used
 * if it's not provided).
 *
 * These are the various use-cases in more detail:
 *
 * 1. Referring to an actual shader file in the standard resource lookup
 *    paths. The .glsl extension can be omitted. A shader preset can be
 *    optionally specified in the `SHADER_NAME:PRESET_NAME` format. If the
 *    preset is not specified, the default preset will be used. Examples:
 *
 *    - interpolation/catmull-rom.glsl
 *    - interpolation/catmull-rom
 *    - crt/crt-hyllian
 *    - crt/crt-hyllian:vga-4k
 *
 * 2. Referring to an actual shader file on the filesystem via relative
 * or absolute paths. The .glsl extension can be omitted. Examples:
 *
 *    - ../my-shaders/custom-shader
 *    - D:\Emulators\DOSBox\shaders\custom-shader.glsl
 *
 * 3. Aliased symbolic shader names, e.g.:
 *
 *    - bilinear (alias of 'interpolation/bilinear')
 *    - sharp    (alias of 'interpolation/sharp')
 *
 * 4. "Meta-shader" symbolic shader names. Currently, these are the CRT
 *    shaders that automatically switch presets depending on the machine
 *    type and the viewport resolution. This is the full list of meta-
 *    shaders:
 *
 *    - crt-auto
 *    - crt-auto-machine
 *    - crt-auto-arcade
 *    - crt-auto-arcade-sharp
 */
struct ShaderDescriptor {
	std::string shader_name = {};
	std::string preset_name = {};

	auto operator<=>(const ShaderDescriptor&) const = default;

	std::string ToString() const
	{
		if (preset_name.empty()) {
			return shader_name;
		} else {
			return format_str("%s:%s",
			                  shader_name.c_str(),
			                  preset_name.c_str());
		}
	}
};

ShaderDescriptor from_string(const std::string& descriptor,
                             const std::string& extension);

// The default setttings are important; these are the settings we get if the
// shader doesn't override them via custom pragmas.
struct ShaderSettings {
	bool force_single_scan       = false;
	bool force_no_pixel_doubling = false;

	TextureFilterMode texture_filter_mode = TextureFilterMode::Bilinear;

	auto operator<=>(const ShaderSettings&) const = default;
};

using ShaderParameters = std::unordered_map<std::string, float>;

struct ShaderPreset {
	std::string name        = {};
	ShaderSettings settings = {};
	ShaderParameters params = {};
};

struct ShaderInfo {
	// The mapped shader name without the file extension. The name might
	// optionally contain a relative or absolute directory path.
	std::string name = {};

	ShaderPreset default_preset = {};

	bool is_adaptive = false;
};

struct Shader {
	ShaderInfo info       = {};
	GLuint program_object = 0;
};

#endif // DOSBOX_SHADER_H
