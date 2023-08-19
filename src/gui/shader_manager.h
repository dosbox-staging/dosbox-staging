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

#ifndef DOSBOX_SHADER_MANAGER_H
#define DOSBOX_SHADER_MANAGER_H

#include <string>
#include <vector>

#include "vga.h"

// forward references
class Fraction;

const auto FallbackShaderName             = "none";
const auto DefaultShaderName              = "default";
const auto SharpShaderName                = "interpolation/sharp";
const auto AutoGraphicsStandardShaderName = "crt-auto";
const auto AutoMachineShaderName          = "crt-auto-machine";
const auto AutoArcadeShaderName           = "crt-auto-arcade";

enum class ShaderMode {
	// No shader auto-switching; the 'glshader' setting always contains the
	// name of the shader in use.
	Single,

	// Graphics-standard-based shader auto-switching enabled via the
	// 'crt-auto' magic 'glshader' setting.
	//
	// CGA modes will always use the 'crt/cga-*' shaders, EGA modes always the
	// 'crt/ega-*' shaders, etc. regardless of machine type. In other words,
	// the choice of the shader is governed by the graphics standard of the
	// active screen mode, *not* the emulated video adapter.
	//
	// As most users leave the machine type at the 'svga_s3' default, this
	// mode gives them single-scanned CRT emulation in CGA and EGA modes,
	// providing a more authentic out-of-the-box experience (authentic as in
	// "how people experienced the game at the time of release", and
	// prioritising the most probable developer intent.)
	AutoGraphicsStandard,

	// Machine-based shader auto-switching enabled via the 'crt-machine-auto'
	// magic 'glshader' setting.
	//
	// CGA and EGA modes on a VGA machine type will always use 'crt/vga-*'
	// shaders, on the EGA machine type always the 'crt/ega-*' shaders, and so
	// on.
	//
	// This mode emulates a computer (machine) equipped with the configured
	// video adapter and matching monitor. The auto-switching only picks the
	// most approriate shader variant for the chosen adapter/monitor combo
	// (Hercules, CGA, EGA, (S)VGA, etc.) for a given viewport resolution.
	AutoMachine
};

struct ShaderSettings {
	bool use_srgb_texture           = false;
	bool use_srgb_framebuffer       = false;
	bool force_single_scan          = false;
	bool force_no_pixel_doubling    = false;
	float min_vertical_scale_factor = 0.0f;
};

struct ShaderInfo {
	std::string name        = {};
	ShaderSettings settings = {};
};

using ShaderSet = std::vector<ShaderInfo>;

class ShaderManager {
public:
	ShaderManager() noexcept;
	~ShaderManager() noexcept;

	std::deque<std::string> InventoryShaders() const;

	void NotifyGlshaderSettingChanged(const std::string& shader_name);

	void NotifyRenderParametersChanged(const uint16_t canvas_width,
	                                   const uint16_t canvas_height,
	                                   const uint16_t draw_width,
	                                   const uint16_t draw_height,
	                                   const Fraction& render_pixel_aspect_ratio,
	                                   const VideoMode& video_mode);

	ShaderInfo GetCurrentShaderInfo() const;
	std::string GetCurrentShaderSource() const;

	void ReloadCurrentShader();

	// prevent copying
	ShaderManager(const ShaderManager&) = delete;
	// prevent assignment
	ShaderManager& operator=(const ShaderManager&) = delete;

private:
	std::optional<std::string> MapLegacyShaderName(const std::string& name) const;

	bool ReadShaderSource(const std::string& shader_name, std::string& source);

	ShaderSettings ParseShaderSettings(const std::string& shader_name,
	                                   const std::string& source) const;

	void LoadShader(const std::string& shader_name);

	void MaybeAutoSwitchShader();

	const ShaderSet& GetShaderSetForGraphicsStandard(const VideoMode& video_mode) const;

	const ShaderSet& GetShaderSetForMachineType(const MachineType machine_type,
	                                            const VideoMode& video_mode) const;

	std::optional<std::string> FindBestShader(
	        const ShaderSet& shader_set, const double scale_y,
	        const std::optional<bool> force_single_scan_filter = {});

	struct {
		// Shader sets are sorted by 'min_vertical_scale_factor' in
		// descending order
		ShaderSet monochrome = {};
		ShaderSet composite  = {};
		ShaderSet cga        = {};
		ShaderSet ega        = {};
		ShaderSet vga        = {};
		ShaderSet arcade     = {};
	} shader_set = {};

	ShaderMode mode = ShaderMode::Single;

	struct {
		ShaderInfo info    = {};
		std::string source = {};
	} current_shader = {};

	// Shader name from config, resolved to its actual name
	std::string shader_name_from_config = {};

	double scale_y             = 1.0;
	double scale_y_single_scan = 1.0;
	VideoMode video_mode       = {};
};

#endif // DOSBOX_SHADER_MANAGER_H
