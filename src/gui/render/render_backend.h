// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_BACKEND_H
#define DOSBOX_RENDER_BACKEND_H

#include "gui/private/common.h"
#include "gui/private/shader_manager.h"

#include <string>

#include "dosbox_config.h"
#include "gui/render/render.h"
#include "misc/rendered_image.h"
#include "misc/video.h"
#include "utils/rect.h"

// forward declaration
struct SDL_Window;

class RenderBackend {

public:
	// Destroys the renderer and associated resources, including the SDL
	// window
	virtual ~RenderBackend() = default;

	// Return the SDL window.
	virtual SDL_Window* GetWindow() = 0;

	// Get the unrestricted total available drawing area of the emulator
	// window or the screen in fullscreen in pixels.
	virtual DosBox::Rect GetCanvasSizeInPixels() = 0;

	// Notify the renderer that the drawing area (viewport) size has changed.
	virtual void NotifyViewportSizeChanged(const DosBox::Rect draw_rect_px) = 0;

	// Notify the renderer that the size of the image rendered by the video
	// emulation has changed (the size of the DOS framebuffer). Always
	// called at least once before the first StartUpdate() call.
	virtual void NotifyRenderSizeChanged(const int new_render_width_px,
	                                     const int new_render_height_px) = 0;

	// Notify the renderer of video mode changes.
	virtual void NotifyVideoModeChanged(const VideoMode& video_mode) = 0;

	// Set a shader by its symbolic shader descriptor. The render backend
	// should load the shader via the `ShaderManager` if it's not in its
	// shader cache (caching is optional but recommended).
	//
	// E.g., `crt-auto-machine` is a symbolic "meta shader" name that will
	// get resolved to actual physical shaders on disk that implement the
	// Hercules, CGA, EGA, and VGA CRT emulations, respectively (see
	// `ShaderManager::NotifyShaderChanged()`.
	//
	// Similarly, `sharp` is mapped to `interpolation/sharp.glsl` on disk,
	// etc.
	//
	enum class SetShaderResult { Ok, ShaderError, PresetError };

	virtual SetShaderResult SetShader(const std::string& symbolic_shader_descriptor) = 0;

	// Reload the currently active shader from disk. If this fails (e.g.,
	// the shader cannot be loaded, or the compilation fails), the current
	// shader should stay active.
	virtual void ForceReloadCurrentShader() = 0;

	// Get information about the currently active shader.
	virtual ShaderInfo GetCurrentShaderInfo() = 0;

	// Get current shader preset.
	virtual ShaderPreset GetCurrentShaderPreset() = 0;

	// Get the symbolic shader descriptor of the currently active shader
	// (see `ShaderManager::NotifyShaderChanged()`.
	virtual std::string GetCurrentSymbolicShaderDescriptor() = 0;

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
	// pixel data, including optional padding bytes at the end of the row.
	//
	virtual void StartFrame(uint32_t*& pixels_out, int& pitch_out) = 0;

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

	// Enables or disables vsync.
	virtual void SetVsync(const bool is_enabled) = 0;

	// Sets the colour space of the video output.
	virtual void SetColorSpace(const ColorSpace color_space) = 0;

	// Enables or disable the application of image adjustments.
	virtual void EnableImageAdjustments(const bool enable) = 0;

	// Sets image adjustment settings.
	virtual void SetImageAdjustmentSettings(const ImageAdjustmentSettings& settings) = 0;

	// Read the specified rectangle of the post-shader from the window's
	// framebuffer.
	virtual RenderedImage ReadPixelsPostShader(const DosBox::Rect output_rect_px) = 0;

	// Create an RGB pixel in the internal format of the render backend.
	virtual uint32_t MakePixel(const uint8_t red, const uint8_t green,
	                           const uint8_t blue) = 0;
};

#endif // DOSBOX_RENDER_BACKEND_H
