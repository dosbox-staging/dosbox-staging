// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_OPENGL_RENDERER_H
#define DOSBOX_OPENGL_RENDERER_H

#include "render_backend.h"

#if C_OPENGL

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "private/shader_common.h"
#include "private/shader_pipeline.h"

#include "dosbox_config.h"
#include "misc/video.h"
#include "utils/rect.h"

// Glad must be included before SDL
#include "glad/gl.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

class OpenGlRenderer : public RenderBackend {

public:
	OpenGlRenderer(const int x, const int y, const int width,
	               const int height, SDL_WindowFlags sdl_window_flags);

	~OpenGlRenderer() override;

	SDL_Window* GetWindow() override;

	DosBox::Rect GetCanvasSizeInPixels() override;

	void NotifyViewportSizeChanged(const DosBox::Rect viewport_size) override;

	void NotifyRenderSizeChanged(const int new_render_width_px,
	                             const int new_render_height_px) override;

	void NotifyVideoModeChanged(const VideoMode& video_mode) override;

	SetShaderResult SetShader(const std::string& symbolic_shader_descriptor) override;

	void ForceReloadCurrentShader() override;

	ShaderInfo GetCurrentShaderInfo() override;
	ShaderPreset GetCurrentShaderPreset() override;

	std::string GetCurrentSymbolicShaderDescriptor() override;
	ShaderDescriptor GetCurrentShaderDescriptor() override;

	void UploadFrame(const uint32_t* pixels, const int width_px,
	                 const int height_px, const int pitch_bytes,
	                 const bool double_width, const bool double_height,
	                 const VideoMode& video_mode) override;

	void PresentFrame() override;

	void SetVsync(const bool is_enabled) override;

	void SetColorSpace(const ColorSpace color_space) override;
	void EnableImageAdjustments(const bool enable) override;
	void SetImageAdjustmentSettings(const ImageAdjustmentSettings& settings) override;
	void SetDeditheringStrength(const float strength) override;

	RenderedImage ReadPixelsPostShader(const DosBox::Rect output_rect_px) override;

	uint32_t MakePixel(const uint8_t red, const uint8_t green,
	                   const uint8_t blue) override;

	// prevent copying
	OpenGlRenderer(const OpenGlRenderer&) = delete;
	// prevent assignment
	OpenGlRenderer& operator=(const OpenGlRenderer&) = delete;

private:
	SDL_Window* CreateSdlWindow(const int x, const int y, const int width,
	                            const int height,
	                            const SDL_WindowFlags sdl_window_flags);

	bool InitRenderer();
	void RecreateInputTexture();

	void MaybeUpdateRenderSize(const int new_width_px, const int new_height_px,
	                           const bool new_double_width,
	                           const bool new_double_height);

	SetShaderResult SetShaderInternal(const std::string& symbolic_shader_descriptor);

	void HandleShaderAndPresetChangeViaNotify(const ShaderDescriptor& new_descriptor);

	SetShaderResult MaybeSwitchShaderAndPreset(const ShaderDescriptor& curr_descriptor,
	                                           const ShaderDescriptor& new_descriptor);

	bool SwitchShader(const std::string& shader_name);

	SDL_Window* window    = {};
	SDL_GLContext context = {};

	GLint max_texture_size_px = 0;

	struct {
		int width      = 0;
		int height     = 0;
		GLuint texture = 0;

		// Additional doubling requested on top of the texture
		// dimensions, performed by the shader pipeline's
		// integer-upscale pre-pass
		bool double_width  = false;
		bool double_height = false;
	} input_texture = {};

	// Vertex buffer object
	GLuint vbo = 0;

	// Vertex array object
	GLuint vao = 0;

	// Vertex data for an oversized triangle
	std::array<GLfloat, 2 * 3> vertex_data = {};

	// Current shader descriptor string as set by the user (e.g., if the
	// user set `crt-auto`, this will stay `crt-auto`; it won't be synced to
	// the actual shader & preset combo in use, such as
	// `crt/crt-hyllian:vga-4k`).
	//
	// Might contain the .glsl file extension if set by the user.
	//
	std::string curr_symbolic_shader_descriptor = {};

	ShaderDescriptor curr_shader_descriptor = {};

	ShaderInfo main_shader_info     = {};
	ShaderPreset main_shader_preset = {};

	DosBox::Rect curr_viewport_size_px = {};
	VideoMode curr_video_mode          = {};

	std::unique_ptr<ShaderPipeline> shader_pipeline = {};
};

#endif // C_OPENGL

#endif // DOSBOX_OPENGL_RENDERER_H
