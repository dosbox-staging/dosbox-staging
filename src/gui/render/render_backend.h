// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_BACKEND_H
#define DOSBOX_RENDER_BACKEND_H

#include "gui/private/shader_manager.h"

#include <string>

#include "utils/rect.h"

struct SDL_Window;

class RenderBackend {

public:
	// Destroys the renderer and associated resources, including the SDL
	// window
	virtual ~RenderBackend() = default;

	// Return the SDL window.
	virtual SDL_Window* GetWindow() = 0;

	// Return the "gfx flags".
	virtual uint8_t GetGfxFlags() = 0;

	// Get the unrestricted total available drawing area of the emulator
	// window or the screen in fullscreen in pixels.
	virtual DosBox::Rect GetCanvasSizeInPixels() = 0;

	// Update the drawing area (viewport) of the renderer.
	virtual void UpdateViewport(const DosBox::Rect draw_rect_px) = 0;

	// Update the size of the image rendered by the video emulation (the
	// size of the DOS framebuffer). Always called at least once before the
	// first StartUpdate() call.
	virtual bool UpdateRenderSize(const int new_render_width_px,
	                              const int new_render_height_px) = 0;

	// Called at the start of every unique frame (when there have been
	// changes to the DOS framebuffer).
	//
	// Should return a writeable buffer for the video emulation to render
	// the framebuffer image into. The buffer was sized for the current DOS
	// video mode by a preceding `UpdateRenderSize()` call.
	//
	// If a renderer implements a double buffering scheme, this call should
	// return a pointer to the current render buffer.
	//
	// `pitch_out` is the number of bytes used to store a single row of
	// pixel data (can be larger than actual width).
	//
	virtual void StartFrame(uint8_t*& pixels_out, int& pitch_out) = 0;

	// Called at the end of every frame. There is a matching EndUpdate()
	// call for every StartUpdate() call.
	//
	// If a renderer implements a double buffering scheme, this call should
	// swap the "current" and "last" buffers.
	//
	virtual void EndFrame() = 0;

	// Prepares the frame for presentation (e.g., by uploading it to
	// GPU memory).
	//
	// If a renderer implements a double buffering scheme, this call should
	// prepare the "last" buffer for presentation.
	//
	virtual void PrepareFrame() = 0;

	// Presents the frame prepared for presentation by PrepareFrame().
	virtual void PresentFrame() = 0;

	// Sets the current shader (can be a no-op on backends that don't
	// support shaders).
	virtual void SetShader(const ShaderInfo& shader_info,
	                       const std::string& shader_source) = 0;

	// Enables or disables vsync.
	virtual void SetVsync(const bool is_enabled) = 0;

	// Read the specified rectangle of the post-shader from the window's
	// framebuffer.
	virtual RenderedImage ReadPixelsPostShader(const DosBox::Rect output_rect_px) = 0;

	// TODO
	virtual uint32_t GetRgb(const uint8_t red, const uint8_t green,
	                        const uint8_t blue) = 0;
};

#endif // DOSBOX_RENDER_BACKEND_H
