// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SHADER_MANAGER_H
#define DOSBOX_SHADER_MANAGER_H

#include <string>
#include <vector>

#include "hardware/video/vga.h"
#include "util/rect.h"
#include "gui/render.h"

// forward references
class Fraction;

constexpr auto BilinearShaderName = "interpolation/bilinear";
constexpr auto SharpShaderName    = "interpolation/sharp";
constexpr auto FallbackShaderName = BilinearShaderName;

constexpr auto AutoGraphicsStandardShaderName = "crt-auto";
constexpr auto AutoMachineShaderName          = "crt-auto-machine";
constexpr auto AutoArcadeShaderName           = "crt-auto-arcade";
constexpr auto AutoArcadeSharpShaderName      = "crt-auto-arcade-sharp";

enum class ShaderMode {
	// No shader auto-switching; the 'glshader' setting always contains the
	// name of the shader in use.
	//
	Single,

	// Graphics-standard-based adaptive CRT shader mode.
	// Enabled with the 'crt-auto' magic 'glshader' setting.
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
	// Enabled via the 'crt-machine-auto' magic 'glshader' setting.
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
	// Enabled via the 'crt-machine-arcade' magic 'glshader' setting.
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

enum TextureFilterMode {
	// Nearest-neighbour interpolation
	Nearest,

	// Bilinear interpolation
	Linear
};

// The default setttings are important; these are the settings we get if the
// shader doesn't override them via custom pragmas.
struct ShaderSettings {
	bool use_srgb_texture     = false;
	bool use_srgb_framebuffer = false;

	bool force_single_scan       = false;
	bool force_no_pixel_doubling = false;

	TextureFilterMode texture_filter_mode = TextureFilterMode::Linear;
};

struct ShaderInfo {
	std::string name        = {};
	ShaderSettings settings = {};
	bool is_adaptive        = false;
};

// Shader manager that picks the best shader to use based on various criteria.
// Its main function is to handle shader auto-switching in the adaptive CRT
// shader modes.
//
// Usage:
//
// - Notify the shader manager about changes that could potentially trigger
//   shader switching with the `Notify*` methods.
//
// - Query information about the current shader with `GetCurrentShaderInfo()`.
//
// - Fetch the source of the current shader with `GetCurrentShaderSource()`.
//
// The caller is responsible for compiling the shader source to the OpenGL
// backend and to implement lazy shader switching (only when the shader has
// really changed).
//
class ShaderManager {
public:
	static ShaderManager& GetInstance()
	{
		static ShaderManager instance;
		return instance;
	}

	// Generate a human-readable shader inventory message (one list element
	// per line).
	std::deque<std::string> GenerateShaderInventoryMessage() const;
	static void AddMessages();

	std::string MapShaderName(const std::string& name) const;

	void NotifyGlshaderSettingChanged(const std::string& shader_name);

	void NotifyRenderParametersChanged(const DosBox::Rect canvas_size_px,
	                                   const VideoMode& video_mode);

	const ShaderInfo& GetCurrentShaderInfo() const;
	const std::string& GetCurrentShaderSource() const;

	void ReloadCurrentShader();

private:
	ShaderManager()  = default;
	~ShaderManager() = default;

	// prevent copying
	ShaderManager(const ShaderManager&) = delete;
	// prevent assignment
	ShaderManager& operator=(const ShaderManager&) = delete;

	void LoadShader(const std::string& shader_name);
	bool ReadShaderSource(const std::string& shader_name, std::string& source);

	ShaderSettings ParseShaderSettings(const std::string& shader_name,
	                                   const std::string& source) const;

	void MaybeAutoSwitchShader();

	std::string FindShaderAutoGraphicsStandard() const;
	std::string FindShaderAutoMachine() const;
	std::string FindShaderAutoArcade() const;
	std::string FindShaderAutoArcadeSharp() const;

	std::string GetHerculesShader() const;
	std::string GetCgaShader() const;
	std::string GetCompositeShader() const;
	std::string GetEgaShader() const;
	std::string GetVgaShader() const;

	ShaderMode mode = ShaderMode::Single;

	struct {
		ShaderInfo info    = {};
		std::string source = {};
	} current_shader = {};

	std::string shader_name_from_config = {};

	int pixels_per_scanline                   = 1;
	int pixels_per_scanline_force_single_scan = 1;

	VideoMode video_mode = {};
};

#endif // DOSBOX_SHADER_MANAGER_H
