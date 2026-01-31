// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_AUTO_SHADER_SWITCHER_H
#define DOSBOX_AUTO_SHADER_SWITCHER_H

#include <string>
#include <unordered_map>

#include "gui/private/common.h"
#include "shader_pass.h"

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
	 * Notify the auto shader switcher that the current shader has been
	 * changed by the user.
	 */
	void NotifyShaderChanged(const std::string& shader_descriptor,
	                         const std::string& extension);

	/*
	 * Notify the auto shader switcher that the viewport area or the current
	 * video mode has been changed.
	 */
	void NotifyRenderParametersChanged(const DosBox::Rect canvas_size_px,
	                                   const VideoMode& video_mode);

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

	void MaybeAutoSwitchShader();

	ShaderDescriptor FindShaderAutoGraphicsStandard() const;
	ShaderDescriptor FindShaderAutoMachine() const;
	ShaderDescriptor FindShaderAutoArcade() const;
	ShaderDescriptor FindShaderAutoArcadeSharp() const;

	ShaderDescriptor GetHerculesShader() const;
	ShaderDescriptor GetCgaShader() const;
	ShaderDescriptor GetCompositeShader() const;
	ShaderDescriptor GetEgaShader() const;
	ShaderDescriptor GetVgaShader() const;

	struct {
		ShaderDescriptor descriptor = {};

		ShaderMode mode = ShaderMode::Single;
	} current_shader = {};

	VideoMode video_mode = {};

	int pixels_per_scanline                   = 1;
	int pixels_per_scanline_force_single_scan = 1;
};

#endif // DOSBOX_AUTO_SHADER_SWITCHER_H
