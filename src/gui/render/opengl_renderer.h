// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_OPENGL_RENDERER_H
#define DOSBOX_OPENGL_RENDERER_H

#include "render_backend.h"

#include "gui/private/shader_manager.h"

#include <optional>
#include <string>
#include <vector>

#include "dosbox_config.h"
#include "render.h"
#include "utils/rect.h"

#if C_OPENGL
// Glad must be included before SDL
#include "glad/gl.h"
#include <SDL.h>
#include <SDL_opengl.h>

#else

#include "SDL.h"
#endif // C_OPENGL

#include "utils/rect.h"

class OpenGlRenderer : public RenderBackend {

public:
	OpenGlRenderer(const int x, const int y, const int width,
	               const int height, uint32_t sdl_window_flags);

	~OpenGlRenderer() override;

	SDL_Window* GetWindow() override;
	uint8_t GetGfxFlags() override;

	DosBox::Rect GetCanvasSizeInPixels() override;
	void UpdateViewport(const DosBox::Rect draw_rect_px) override;

	bool UpdateRenderSize(const int new_render_width_px,
	                      const int new_render_height_px) override;

	void StartFrame(uint8_t*& pixels_out, int& pitch_out) override;
	void EndFrame() override;

	void PrepareFrame() override;
	void PresentFrame() override;

	void SetShader(const ShaderInfo& shader_info,
	               const std::string& shader_source) override;

	void SetVsync(const bool is_enabled) override;

	bool ReadPixelsPostShader(const DosBox::Rect output_rect_px,
	                          RenderedImage& image_out) override;

	uint32_t GetRgb(const uint8_t red, const uint8_t green,
	                const uint8_t blue) override;

private:
	SDL_Window* CreateSdlWindow(const int x, const int y, const int width,
	                            const int height,
	                            const uint32_t sdl_window_flags);

	bool InitRenderer();

	void GetUniformLocations();
	void UpdateUniforms();

	GLuint BuildShader(const GLenum type, const std::string& source);
	GLuint BuildShaderProgram(const std::string& source);

	SDL_Window* window    = {};
	SDL_GLContext context = {};
	uint8_t gfx_flags     = 0;

	int pitch = 0;

	// The current framebuffer we render the emulated video output into
	// (contains the "work-in-progress" next frame).
	std::vector<uint8_t> curr_framebuf = {};

	// Contains the last fully rendered frame, waiting to be presented.
	std::vector<uint8_t> last_framebuf = {};

	int render_width_px  = 0;
	int render_height_px = 0;

	DosBox::Rect draw_rect_px = {};

	GLuint texture            = 0;
	GLint max_texture_size_px = 0;

	bool is_framebuffer_srgb_capable = false;

	ShaderInfo shader_info    = {};
	std::string shader_source = {};

	GLuint program_object = 0;

	struct {
		GLint texture_size = 0;
		GLint input_size   = 0;
		GLint output_size  = 0;
		GLint frame_count  = 0;
	} uniform = {};

	GLuint actual_frame_count  = 0;
	GLfloat vertex_data[2 * 3] = {};
};

#endif // DOSBOX_OPENGL_RENDERER_H
