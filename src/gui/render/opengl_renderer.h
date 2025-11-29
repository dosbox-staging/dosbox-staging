// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_OPENGL_RENDERER_H
#define DOSBOX_OPENGL_RENDERER_H

#include "render_backend.h"

#if C_OPENGL

#include "gui/private/shader_manager.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "dosbox_config.h"
#include "gui/render/render.h"
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
	void UpdateViewport(const DosBox::Rect draw_rect_px) override;

	bool UpdateRenderSize(const int new_render_width_px,
	                      const int new_render_height_px) override;

	void NotifyVideoModeChanged(const VideoMode& video_mode) override;

	SetShaderResult SetShader(const std::string& shader_descriptor) override;

	bool ForceReloadCurrentShader() override;

	ShaderInfo GetCurrentShaderInfo() override;
	ShaderPreset GetCurrentShaderPreset() override;

	std::string GetCurrentShaderDescriptorString() override;

	void StartFrame(uint8_t*& pixels_out, int& pitch_out) override;
	void EndFrame() override;

	void PrepareFrame() override;
	void PresentFrame() override;

	void SetVsync(const bool is_enabled) override;

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

	SetShaderResult SetShaderInternal(const std::string& shader_descriptor,
	                                  const bool force_reload = false);

	bool MaybeSwitchShaderAndPreset(const ShaderDescriptor& curr_descriptor,
	                                const ShaderDescriptor& new_descriptor);

	bool SwitchShader(const std::string& shader_name);

	std::optional<Shader> GetOrLoadAndCacheShader(const std::string& shader_name);

	void SwitchShaderPresetOrSetDefault(const ShaderDescriptor& descriptor);

	std::optional<ShaderPreset> GetOrLoadAndCacheShaderPreset(
	        const ShaderDescriptor& descriptor);

	void GetUniformLocations(const ShaderParameters& params);
	void UpdateUniforms();

	std::optional<GLuint> BuildShaderProgram(const std::string& source);

	std::optional<GLuint> BuildShader(const GLenum type,
	                                  const std::string& source) const;

	SDL_Window* window    = {};
	SDL_GLContext context = {};

	int pitch = 0;

	// The current framebuffer we render the emulated video output into
	// (contains the "work-in-progress" next frame).
	std::vector<uint8_t> curr_framebuf = {};

	// Contains the last fully rendered frame, waiting to be presented.
	std::vector<uint8_t> last_framebuf = {};

	// True if the last framebuffer has been updated since the last present
	bool last_framebuf_dirty = false;

	int render_width_px  = 0;
	int render_height_px = 0;

	DosBox::Rect draw_rect_px = {};

	GLuint texture            = 0;
	GLint max_texture_size_px = 0;

	struct {
		GLint texture_size  = -1;
		GLint input_size    = -1;
		GLint output_size   = -1;
		GLint frame_count   = -1;
		GLint input_texture = -1;

		std::unordered_map<std::string, GLint> params = {};
	} uniforms = {};

	GLuint frame_count = 0;

	// Vertex buffer object
	GLuint vbo = 0;

	// Vertex array object
	GLuint vao = 0;

	// Vertex data for an oversized triangle
	GLfloat vertex_data[2 * 3] = {};

	// Keys are the shader names including the path part but without the
	// .glsl file extension
	std::unordered_map<std::string, Shader> shader_cache = {};

	// Keys are the shader names including the path part but without the
	// .glsl file extension
	std::unordered_map<std::string, ShaderPreset> shader_preset_cache = {};

	Shader current_shader              = {};
	ShaderPreset current_shader_preset = {};

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
