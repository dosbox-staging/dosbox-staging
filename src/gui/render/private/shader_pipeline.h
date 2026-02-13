// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SHADER_PIPELINE_H
#define DOSBOX_SHADER_PIPELINE_H

#include <iterator>
#include <list>
#include <string>
#include <vector>

#include "gui/render/render.h"
#include "misc/std_filesystem.h"
#include "misc/video.h"
#include "shader.h"
#include "shader_common.h"
#include "utils/rect.h"

// Glad must be included before SDL
#include "glad/gl.h"
#include <SDL.h>
#include <SDL_opengl.h>

struct ShaderPass {
	Shader shader = {};

	// Input texture; contains at least one element
	std::vector<GLuint> in_textures = {};

	// Output texture size for intermediate shader passes (width & height
	// only), or the position and size of the viewport for the final pass.
	DosBox::Rect out_texture_size = {};

	// Textures and FBOs for intermediate shader passes. Both are 0 for the
	// final pass that's rendered directly to the window's framebuffer.
	GLuint out_fbo     = 0;
	GLuint out_texture = 0;

	std::string ToString() const;
};

class ShaderPipeline {

public:
	ShaderPipeline();
	~ShaderPipeline();

	void NotifyRenderSizeChanged(const int input_texture_width,
	                             const int input_texture_height,
	                             const GLuint _input_texture);

	void NotifyViewportSizeChanged(const DosBox::Rect draw_rect_px);
	void NotifyVideoModeChanged(const VideoMode& video_mode);

	void SetMainShader(const Shader& shader);
	void SetMainShaderPreset(const ShaderPreset& preset);

	void SetColorSpace(const ColorSpace color_space);
	void EnableImageAdjustments(const bool enable);
	void SetImageAdjustmentSettings(const ImageAdjustmentSettings& settings);
	void SetDeditheringStrength(const float strength);

	void Render(const GLuint vertex_array_object) const;

	// prevent copying
	ShaderPipeline(const ShaderPipeline&) = delete;
	// prevent assignment
	ShaderPipeline& operator=(const ShaderPipeline&) = delete;

private:
	void LoadInternalShaderPassOrExit(const std::string shader_name);

	void CreatePipeline();
	void DestroyPipeline();

	ShaderPass& GetShaderPass(const std::string& name);

	GLuint CreateTexture(const int width, const int height,
	                     const TextureFilterMode filter_mode) const;

	void SetTextureFiltering(const GLuint texture,
	                         const TextureFilterMode filter_mode) const;

	void UpdateTextureUniforms(const std::list<ShaderPass>::iterator pass) const;

	void RenderPass(const ShaderPass& pass, const GLuint vertex_array_object) const;

	struct {
		int width      = 0;
		int height     = 0;
		GLuint texture = 0;
	} input_texture = {};

	VideoMode video_mode  = {};
	DosBox::Rect viewport = {};

	// ---------------------------------------------------------------------
	// Shader passes
	// ---------------------------------------------------------------------
	std::list<ShaderPass> shader_passes = {};

	// Image adjustments pass params
	// -----------------------------
	ColorSpace color_space = {};

	ImageAdjustmentSettings image_adjustment_settings = {};

	bool enable_image_adjustments = false;

	void UpdateImageAdjustmentsPassUniforms();

	// Dedither pass params
	// --------------------
	float dedithering_strength = {};

	void UpdateDeditherPassUniforms();

	// Main shader pass params
	// -----------------------
	ShaderPreset main_shader_preset = {};

	void UpdateMainShaderPassUniforms();
};

#endif // DOSBOX_SHADER_PIPELINE_H
