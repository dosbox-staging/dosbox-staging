// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_AUTO_SHADER_SWITCHER_H
#define DOSBOX_AUTO_SHADER_SWITCHER_H

#include <string>
#include <unordered_map>

#include "gui/private/common.h"
#include "shader_common.h"

#include "misc/video.h"
#include "utils/rect.h"

// Shader switcher for handling shader auto-switching for the adaptive CRT
// shaders.
//
// Usage:
//
// - Notify the shader manager about changes that could potentially trigger
//   shader switching with the `Notify*` methods.
//
// - Query the name of the new shader shader with `GetCurrentShaderDescriptor()`.
//   The caller is responsible for implementing lazy shader switching (only
//   activate the new shader if the current shader has changed).
//
// - Load the shader and preset, or switch to a new shader preset via
//   `ShaderManager`.
//
class AutoShaderSwitcher {
public:
	static AutoShaderSwitcher& GetInstance()
	{
		static AutoShaderSwitcher instance;
		return instance;
	}

	/*
	 * Notify the shader manager that the current shader has been changed
	 * by the user.
	 */
	ShaderDescriptor NotifyShaderChanged(const std::string& symbolic_shader_descriptor);

	/*
	 * Notify the shader manager that the viewport area or the
	 * current video mode has been changed.
	 */
	ShaderDescriptor NotifyRenderParametersChanged(
	        const ShaderDescriptor& curr_shader_descriptor,
	        const DosBox::Rect canvas_size_px, const VideoMode& video_mode);

	ShaderDescriptor GetCurrentShaderDescriptor() const;
	ShaderMode GetCurrentShaderMode() const;

private:
	AutoShaderSwitcher()  = default;
	~AutoShaderSwitcher() = default;

	// prevent copying
	AutoShaderSwitcher(const AutoShaderSwitcher&) = delete;
	// prevent assignment
	AutoShaderSwitcher& operator=(const AutoShaderSwitcher&) = delete;

	std::string MapShaderName(const std::string& name) const;

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

#endif // DOSBOX_AUTO_SHADER_SWITCHER_H
