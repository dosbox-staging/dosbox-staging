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
	            TextureFilterMode texture_filter_mode);

	~SdlRenderer() override;

	SDL_Window* GetWindow() override;

	DosBox::Rect GetCanvasSizeInPixels() override;

	void NotifyViewportSizeChanged(const DosBox::Rect draw_rect_px) override;

	void NotifyRenderSizeChanged(const int new_render_width_px,
	                             const int new_render_height_px) override;

	void NotifyVideoModeChanged(const VideoMode& video_mode) override;

	SetShaderResult SetShader(const std::string& symbolic_shader_descriptor) override;

	void ForceReloadCurrentShader() override;

	ShaderInfo GetCurrentShaderInfo() override;
	ShaderPreset GetCurrentShaderPreset() override;

	std::string GetCurrentSymbolicShaderDescriptor() override;

	void StartFrame(uint32_t*& pixels_out, int& pitch_out) override;
	void EndFrame() override;

	void PrepareFrame() override;
	void PresentFrame() override;

	void SetVsync(const bool is_enabled) override;

	void SetColorSpace(const ColorSpace color_space) override;
	void SetImageAdjustmentSettings(const ImageAdjustmentSettings& settings) override;
	void EnableImageAdjustments(const bool enable) override;

	RenderedImage ReadPixelsPostShader(const DosBox::Rect output_rect_px) override;

	uint32_t MakePixel(const uint8_t red, const uint8_t green,
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

	// The current framebuffer we render the emulated video output into
	// (contains the "work-in-progress" next frame).
	//
	// The framebuffers contain 32-bit pixel data stored as a sequence of
	// four packed 8-bit values in BGRX byte order (that's in memory order,
	// so byte N is B, byte N+1 is G, byte N+2 is R).
	//
	SDL_Surface* curr_framebuf = {};

	// Contains the last fully rendered frame, waiting to be presented.
	SDL_Surface* last_framebuf = {};

	// True if the last framebuffer has been updated since the last present
	bool last_framebuf_dirty = false;

	SDL_Texture* texture = {};

	TextureFilterMode texture_filter_mode = TextureFilterMode::Bilinear;
};

#endif // DOSBOX_SDL_RENDERER_H
