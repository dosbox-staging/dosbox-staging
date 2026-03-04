// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SHADER_PIPELINE_H
#define DOSBOX_SHADER_PIPELINE_H

#include <iterator>
#include <optional>
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
	DosBox::Rect out_size = {};

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

	bool IsPipelineComplete() const;

	void NotifyRenderSizeChanged(const int input_texture_width,
	                             const int input_texture_height,
	                             const GLuint input_texture);

	void NotifyViewportSizeChanged(const DosBox::Rect& viewport);
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
	void CreateSamplers();
	void DestroySamplers();

	void LoadAndAddInternalPasses();
	void LoadAndAddInternalPassOrExit(const std::string& shader_name);

	void SetPassOutputSizes();
	void CreatePassOutputTextures();

	void CreatePipeline();
	void DestroyPipeline();

	ShaderPass& GetShaderPass(const std::string& name);

	std::pair<GLuint, DosBox::Rect> GetPreviousPassOutputTexture(
	        const std::vector<ShaderPass>::iterator pass) const;

	GLuint CreateTexture(const DosBox::Rect& size, const bool float_texture) const;

	void UpdatePassTextureUniforms();
	void UpdateTextureUniforms(const std::vector<ShaderPass>::iterator pass) const;

	void RenderPass(const ShaderPass& pass, const GLuint vertex_array_object) const;

	struct {
		DosBox::Rect size = {};
		GLuint texture    = 0;
	} input_texture = {};

	VideoMode video_mode  = {};
	DosBox::Rect viewport = {};

	std::optional<Shader> main_shader = {};

	GLuint nearest_sampler = 0;
	GLuint linear_sampler  = 0;

	// ---------------------------------------------------------------------
	// Shader passes
	// ---------------------------------------------------------------------
	std::vector<ShaderPass> shader_passes = {};

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
