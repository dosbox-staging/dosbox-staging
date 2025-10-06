// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SDL_RENDERER_H
#define DOSBOX_SDL_RENDERER_H

#include "render_backend.h"

#include "dosbox_config.h"
#include "gui/private/common.h"
#include "gui/render/render.h"
#include "utils/rect.h"

#include <string>

// must be included after dosbox_config.h
#include "SDL.h"

class SdlRenderer : public RenderBackend {

public:
	SdlRenderer(const int x, const int y, const int width, const int height,
	            const uint32_t sdl_window_flags, const std::string& render_driver,
	            InterpolationMode interpolation_mode);

	~SdlRenderer() override;

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

	RenderedImage ReadPixelsPostShader(const DosBox::Rect output_rect_px) override;

	uint32_t GetRgb(const uint8_t red, const uint8_t green,
	                const uint8_t blue) override;

	// prevent copying
	SdlRenderer(const SdlRenderer&) = delete;
	// prevent assignment
	SdlRenderer& operator=(const SdlRenderer&) = delete;

private:
	bool InitRenderer(const std::string& render_driver);

	uint32_t OpenGlDriverCrashWorkaround(const std::string_view render_driver) const;

	SDL_Window* window     = {};
	SDL_Renderer* renderer = {};
	uint8_t gfx_flags      = 0;

	// The current framebuffer we render the emulated video output into
	// (contains the "work-in-progress" next frame).
	SDL_Surface* curr_framebuf = {};

	// Contains the last fully rendered frame, waiting to be presented.
	SDL_Surface* last_framebuf = {};

	SDL_PixelFormat* pixel_format = {};
	SDL_Texture* texture          = {};

	InterpolationMode interpolation_mode = InterpolationMode::Bilinear;
};

#endif // DOSBOX_SDL_RENDERER_H
