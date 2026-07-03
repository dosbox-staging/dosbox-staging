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
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

struct ShaderPass {
	Shader shader = {};

	// Input texture; contains at least one element
	std::vector<GLuint> in_textures = {};

	// Output texture size of the shader pass (width & height only; the
	// final pass renders at viewport size, positioned at the origin).
	DosBox::Rect out_size = {};

	// Output texture and FBO of the shader pass. The final pass owns a
	// viewport-sized FBO that caches the pipeline output for clean
	// presents and rendered captures; presents of changed frames bypass
	// it (see `RenderToBackbuffer()`).
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
	                             const bool double_width, const bool double_height,
	                             const GLuint input_texture);

	void NotifyViewportSizeChanged(const DosBox::Rect& viewport);
	void NotifyVideoModeChanged(const VideoMode& video_mode);

	void SetMainShader(const Shader& shader);
	void SetMainShaderPreset(const ShaderPreset& preset);

	void SetColorSpace(const ColorSpace color_space);
	void EnableImageAdjustments(const bool enable);
	void SetImageAdjustmentSettings(const ImageAdjustmentSettings& settings);
	void SetDeditheringStrength(const float strength);

	// Render the pipeline into the output FBO (see `GetOutputFbo()`).
	void Render(const GLuint vertex_array_object);

	// Render the pipeline with the final pass drawing straight into the
	// viewport rectangle of the window's framebuffer, bypassing the
	// output FBO (which then goes stale). This is the cheap path for
	// presenting changed frames: it skips a full-viewport FBO write plus
	// the blit to the window's framebuffer.
	void RenderToBackbuffer(const GLuint vertex_array_object);

	// The input texture's contents have changed; the next Render() call
	// must re-run the pipeline.
	void NotifyInputTextureUpdated();

	// True when the last rendered output (whatever its target) is out of
	// date with the input texture contents or pipeline settings. Cleared
	// by Render() and RenderToBackbuffer().
	bool IsOutputStale() const;

	// True when the output FBO does not hold the current pipeline
	// output: the input or settings changed since the last render, or
	// the last render went to the backbuffer. Consumers of the FBO must
	// call Render() first when this is set.
	bool IsOutputFboStale() const;

	// The FBO holding the final pipeline output at viewport size,
	// positioned at the origin. Only valid when the pipeline is complete.
	GLuint GetOutputFbo() const;

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

	void RenderPass(const ShaderPass& pass, const GLuint vertex_array_object,
	                const GLuint target_fbo,
	                const DosBox::Rect& target_rect) const;

	struct {
		bool dedithering_enabled = false;
	} config = {};

	// Pass output is a pure function of the input texture and the
	// pipeline settings (there are no per-frame uniforms), so presents
	// can skip re-running the pipeline and re-blit the cached output FBO
	// when nothing changed. If a shader ever needs per-frame animation
	// (e.g. a time or frame-count uniform), it must opt out of this
	// optimisation.
	bool output_stale = true;

	// The output FBO doesn't hold the current pipeline output:
	// `RenderToBackbuffer()` bypassed it, or the pipeline (and thus the
	// FBO) was freshly (re)created and nothing has been rendered into it
	// yet. Only meaningful in combination with `output_stale` -- see
	// `IsOutputFboStale()`.
	bool output_fbo_stale = true;

	struct {
		DosBox::Rect size = {};
		GLuint texture    = 0;

		// Additional doubling requested on top of the input texture
		// dimensions; the `Rendered` output size is the input texture
		// size multiplied by these.
		bool double_width  = false;
		bool double_height = false;
	} input_texture = {};

	VideoMode video_mode  = {};
	DosBox::Rect viewport = {};

	std::optional<Shader> main_shader = {};

	// Pre-created OpenGL sampler objects, one per (filter, wrap)
	// combination. Indexed by [TextureFilterMode][TextureWrapMode].
	static constexpr size_t NumFilterModes        = 2;
	static constexpr size_t NumWrapModes          = 4;
	GLuint samplers[NumFilterModes][NumWrapModes] = {};

	GLuint GetSampler(const TextureFilterMode filter,
	                  const TextureWrapMode wrap) const;

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
