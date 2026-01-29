// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_OPENGL_RENDERER_H
#define DOSBOX_OPENGL_RENDERER_H

#include "render_backend.h"

#if C_OPENGL

#include "gui/private/shader_manager.h"

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "dosbox_config.h"
#include "gui/render/render.h"
#include "misc/video.h"
#include "utils/rect.h"

// Glad must be included before SDL
#include "glad/gl.h"
#include <SDL.h>
#include <SDL_opengl.h>

class OpenGlRenderer : public RenderBackend {

public:
	OpenGlRenderer(const int x, const int y, const int width,
	               const int height, uint32_t sdl_window_flags);

	~OpenGlRenderer() override;

	SDL_Window* GetWindow() override;

	DosBox::Rect GetCanvasSizeInPixels() override;

	void NotifyViewportSizeChanged(const DosBox::Rect draw_rect_px) override;

	void NotifyRenderSizeChanged(const int new_render_width_px,
	                             const int new_render_height_px) override;

	void NotifyVideoModeChanged(const VideoMode& video_mode) override;

	SetShaderResult SetShader(const std::string& shader_descriptor) override;

	bool ForceReloadCurrentShader() override;

	ShaderInfo GetCurrentShaderInfo() override;
	ShaderPreset GetCurrentShaderPreset() override;

	std::string GetCurrentShaderDescriptorString() override;

	void StartFrame(uint32_t*& pixels_out, int& pitch_out) override;
	void EndFrame() override;

	void PrepareFrame() override;
	void PresentFrame() override;

	void SetVsync(const bool is_enabled) override;

	void SetColorSpace(const ColorSpace color_space) override;
	void EnableImageAdjustments(const bool enable) override;
	void SetImageAdjustmentSettings(const ImageAdjustmentSettings& settings) override;

	RenderedImage ReadPixelsPostShader(const DosBox::Rect output_rect_px) override;

	uint32_t MakePixel(const uint8_t red, const uint8_t green,
	                   const uint8_t blue) override;

	// prevent copying
	OpenGlRenderer(const OpenGlRenderer&) = delete;
	// prevent assignment
	OpenGlRenderer& operator=(const OpenGlRenderer&) = delete;

private:
	struct Shader {
		ShaderInfo info       = {};
		GLuint program_object = 0;
	};

	SDL_Window* CreateSdlWindow(const int x, const int y, const int width,
	                            const int height,
	                            const uint32_t sdl_window_flags);

	bool InitRenderer();

	void MaybeUpdateRenderSize(const int new_render_width_px,
	                           const int new_render_height_px);

	SetShaderResult SetShaderInternal(const std::string& shader_descriptor,
	                                  const bool force_reload = false);

	bool MaybeSwitchShaderAndPreset(const ShaderDescriptor& curr_descriptor,
	                                const ShaderDescriptor& new_descriptor);

	bool SwitchShader(const std::string& shader_name);

	std::optional<Shader> LoadAndBuildShader(const std::string& shader_name);
	std::optional<Shader> GetOrLoadAndCacheShader(const std::string& shader_name);

	void SwitchShaderPresetOrSetDefault(const ShaderDescriptor& descriptor);

	std::optional<ShaderPreset> GetOrLoadAndCacheShaderPreset(
	        const ShaderDescriptor& descriptor);

	std::optional<GLuint> BuildShaderProgram(const std::string& source);

	std::optional<GLuint> BuildShader(const GLenum type,
	                                  const std::string& source) const;

	void SetUniform1i(const GLint program_object, const std::string& name,
	                  const int val);

	void SetUniform1f(const GLint program_object, const std::string& name,
	                  const float val);

	void SetUniform2f(const GLint program_object, const std::string& name,
	                  const float val1, const float val2);

	void SetUniform3f(const GLint program_object, const std::string& name,
	                  const float val1, const float val2, const float val3);

	void UpdatePass1Uniforms();
	void UpdatePass2Uniforms();

	void RecreatePass1InputTextureAndRenderBuffer();
	void RecreatePass1OutputTexture();
	void SetPass1OutputTextureFiltering();

	void RenderPass1();
	void RenderPass2();

	// ---------------------------------------------------------------------
	// Common
	// ---------------------------------------------------------------------

	SDL_Window* window    = {};
	SDL_GLContext context = {};

	GLint max_texture_size_px = 0;

	// The current framebuffer we render the emulated video output into
	// (contains the "work-in-progress" next frame).
	//
	// The framebuffers contain 32-bit pixel data stored as a sequence of
	// four packed 8-bit values in BGRX byte order (that's in memory order,
	// so byte N is B, byte N+1 is G, byte N+2 is R).
	//
	std::vector<uint32_t> curr_framebuf = {};

	// Contains the last fully rendered frame, waiting to be presented.
	std::vector<uint32_t> last_framebuf = {};

	// True if the last framebuffer has been updated since the last present
	bool last_framebuf_dirty = false;

	struct {
		int width      = 0;
		int height     = 0;
		int pitch      = 0;
		GLuint texture = 0;
	} input_texture = {};

	DosBox::Rect viewport_rect_px = {};

	GLuint frame_count = 0;

	// Vertex buffer object
	GLuint vbo = 0;

	// Vertex array object
	GLuint vao = 0;

	// Vertex data for an oversized triangle
	std::array<GLfloat, 2 * 3> vertex_data = {};

	// Image adjustments pass
	// ----------------------
	ColorSpace color_space = {};

	ImageAdjustmentSettings image_adjustment_settings = {};

	bool enable_image_adjustments = false;

	ShaderPreset main_shader_preset = {};

	// ---------------------------------------------------------------------
	// Render passes
	// ---------------------------------------------------------------------
	struct {
		Shader shader = {};

		GLuint out_fbo     = 0;
		GLuint out_texture = 0;
	} pass1 = {};

	struct {
		Shader shader              = {};
	} pass2 = {};

	// ---------------------------------------------------------------------
	// Shader caching & preset management
	// ---------------------------------------------------------------------

	// Keys are the shader names including the path part but without the
	// .glsl file extension
	std::unordered_map<std::string, Shader> shader_cache = {};

	// Keys are the shader names including the path part but without the
	// .glsl file extension
	std::unordered_map<std::string, ShaderPreset> shader_preset_cache = {};

	ShaderDescriptor current_shader_descriptor = {};

	// Current shader descriptor string as set by the user (e.g., if the
	// user set `crt-auto`, this will stay `crt-auto`; it won't be synced to
	// the actual shader & preset combo in use, such as
	// `crt/crt-hyllian:vga-4k`).
	//
	// Might contain the .glsl file extension if set by the user.
	//
	std::string current_shader_descriptor_string = {};
};

#endif // C_OPENGL

#endif // DOSBOX_OPENGL_RENDERER_H
