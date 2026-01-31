// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SHADER_MANAGER_H
#define DOSBOX_SHADER_MANAGER_H

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gui/private/common.h"

#include "gui/render/render.h"
#include "hardware/video/vga.h"
#include "utils/rect.h"

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
	// this mode gives them single-scanned CRT emulation in CGA and EGA
	// modes, providing a more authentic out-of-the-box experience
	// (authentic as in "how people experienced these games at the time of
	// release", and prioritising the "most probable" developer intent.)
	//
	// For CGA and EGA modes that reprogram the 18-bit DAC palette on VGA
	// adapters, a double-scanned VGA shader is selected. This is authentic
	// as these games require a VGA adapter, therefore they were designed with
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
 * A symbolic shader descriptor is a `std::string` in the
 * `SHADER_NAME[:SHADER_PRESET]` format where `SHADER_NAME` can refer to the
 * filename of an actual shader on the filesystem, a symbolic alias, or a
 * "meta-shader". Specifying `SHADER_PRESET` after a colon is optional (the
 * default preset is used if it's not provided).
 *
 * Once a symbolic shader description is turned into a `ShaderDescriptor`, the
 * symbolic and "meta-shader" shader names are resolved to actual physical
 * shader names on disk.
 *
 * E.g. the `crt-auto` symbolic shader descriptions can be potentially
 * resolved to `crt/crt-hyllian:vga-4k`.
 *
 * Although `ShaderDescriptor`s could contain symbolic and meta shader names,
 * too, we use them only for resolved descriptors in the codebase for clarity.
 *
 * ---
 *
 * These are the various shader descriptor use-cases in more detail:
 *
 * 1. Referring to an actual shader file in the standard resource lookup
 *    paths. The .glsl extension can be omitted. A shader preset can be
 *    optionally specified in the `SHADER_NAME:PRESET_NAME` format. If the
 *    preset is not specified, the default preset will be used. Examples:
 *
 *    - interpolation/catmull-rom.glsl
 *    - interpolation/catmull-rom
 *    - crt/crt-hyllian  (the ":" must be omitted if no preset is specified)
 *    - crt/crt-hyllian:vga-4k
 *
 * 2. Referring to an actual shader file on the filesystem via relative
 *    or absolute paths. The .glsl extension can be omitted. Examples:
 *
 *    - ../my-shaders/custom-shader
 *    - D:\Emulators\DOSBox\shaders\custom-shader.glsl
 *
 * 3. Aliased symbolic shader names, e.g.:
 *
 *    - bilinear (alias of 'interpolation/bilinear')
 *    - sharp    (alias of 'interpolation/sharp')
 *
 * 4. "Meta-shader" names. Currently, only the auto-CRT shader fall into this
 *    category that automatically switch presets depending on the machine
 *    type and the viewport resolution. This is the full list of the meta-
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

	static ShaderDescriptor FromString(const std::string& descriptor,
	                                   const std::string& extension);
};

// The default shader settings are important; we'll get these if the shader
// doesn't override them via custom pragmas.
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
	// Resolved shader name without the file extension. The name might
	// optionally contain a relative or absolute directory path.
	std::string name = {};

	ShaderPreset default_preset = {};

	bool is_adaptive = false;
};

// Shader manager for loading shader sources, parsing shader metadata, and
// handling shader auto-switching for the adaptive CRT shaders.
//
// Usage:
//
// - Notify the shader manager about changes that could potentially trigger
//   shader switching with the `Notify*` methods.
//
// - Query the name of the new shader shader with `GetCurrentShaderName()`.
//   The caller is responsible for implementing lazy shader switching (only
//   activate the new shader if the current shader has changed).
//
// - Read the shader source code with `LoadShader()`, then compile and
//   activate it in the rendering backend.
//
class ShaderManager {
public:
	static ShaderManager& GetInstance()
	{
		static ShaderManager instance;
		return instance;
	}

	static void AddMessages();

	/*
	 * Generate a human-readable shader inventory message (one list element
	 * per line).
	 */
	std::deque<std::string> GenerateShaderInventoryMessage() const;

	std::optional<std::pair<ShaderInfo, std::string>> LoadShader(
	        const std::string& mapped_name);

	std::optional<std::pair<ShaderInfo, std::string>> LoadShader(
	        const std::string& shader_name, const std::string& extension);

	std::optional<ShaderPreset> LoadShaderPreset(
	        const ShaderDescriptor& descriptor,
	        const ShaderPreset& default_preset) const;

	/*
	 * Notify the shader manager that the current shader has been changed
	 * by the user.
	 */
	ShaderDescriptor NotifyShaderChanged(const std::string& symbolic_shader_descriptor,
	                                     const std::string& extension);
	/*
	 * Notify the shader manager that the viewport area or the
	 * current video mode has been changed.
	 */
	ShaderDescriptor NotifyRenderParametersChanged(
	        const ShaderDescriptor& curr_shader_descriptor,
	        const DosBox::Rect canvas_size_px, const VideoMode& video_mode);

	ShaderMode GetCurrentShaderMode() const;

private:
	ShaderManager()  = default;
	~ShaderManager() = default;

	// prevent copying
	ShaderManager(const ShaderManager&) = delete;
	// prevent assignment
	ShaderManager& operator=(const ShaderManager&) = delete;

	ShaderDescriptor ParseShaderDescriptor(const std::string& descriptor,
	                                       const std::string& extension) const;

	std::string MapShaderName(const std::string& name) const;

	std::optional<std::string> FindShaderAndReadSource(const std::string& shader_name);

	ShaderPreset ParseDefaultShaderPreset(const std::string& shader_name,
	                                      const std::string& shader_source) const;

	void SetShaderSetting(const std::string& name, const std::string& value,
	                      ShaderSettings& settings) const;

	std::optional<std::pair<std::string, float>> ParseParameterPragma(
	        const std::string& pragma_value) const;

	ShaderDescriptor MaybeAutoSwitchShader(
	        const ShaderDescriptor& new_mapped_descriptor) const;

	ShaderDescriptor FindShaderAutoGraphicsStandard() const;
	ShaderDescriptor FindShaderAutoMachine() const;
	ShaderDescriptor FindShaderAutoArcade() const;
	ShaderDescriptor FindShaderAutoArcadeSharp() const;

	ShaderDescriptor GetHerculesShader() const;
	ShaderDescriptor GetCgaShader() const;
	ShaderDescriptor GetCompositeShader() const;
	ShaderDescriptor GetEgaShader() const;
	ShaderDescriptor GetVgaShader() const;

	ShaderDescriptor last_shader_descriptor = {};
	ShaderMode current_shader_mode          = {};

	VideoMode video_mode = {};

	int pixels_per_scanline                   = 1;
	int pixels_per_scanline_force_single_scan = 1;
};

#endif // DOSBOX_SHADER_MANAGER_H
