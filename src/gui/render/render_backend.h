// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_BACKEND_H
#define DOSBOX_RENDER_BACKEND_H

#include <cstdint>
#include <cstring>
#include <string>

#include "gui/private/common.h"
#include "private/shader_common.h"

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

	// Get the shader descriptor of the currently active shader.
	virtual ShaderDescriptor GetCurrentShaderDescriptor() = 0;

	// Uploads one complete frame. `pixels` holds tightly packed 32-bit
	// BGRX pixels at the source dimensions (baked-in VGA scaling
	// included, no render-layer doubling); `double_width` and
	// `double_height` request the additional doubling on top of that.
	//
	// Called once per present when the source frame has changed since the
	// last upload.
	//
	virtual void UploadFrame(const uint32_t* pixels, const int width_px,
	                         const int height_px, const int pitch_bytes,
	                         const bool double_width, const bool double_height,
	                         const VideoMode& video_mode) = 0;

	// Presents the last uploaded frame.
	virtual void PresentFrame() = 0;

	// Enables or disables vsync.
	virtual void SetVsync(const bool is_enabled) = 0;

	// Sets the colour space of the video output.
	virtual void SetColorSpace(const ColorSpace color_space) = 0;

	// Enables or disable the application of image adjustments.
	virtual void EnableImageAdjustments(const bool enable) = 0;

	// Sets image adjustment settings.
	virtual void SetImageAdjustmentSettings(const ImageAdjustmentSettings& settings) = 0;

	// Sets dedithering strength.
	virtual void SetDeditheringStrength(const float strength) = 0;

	// Read the specified rectangle of the post-shader from the window's
	// framebuffer.
	virtual RenderedImage ReadPixelsPostShader(const DosBox::Rect output_rect_px) = 0;

	// Create an RGB pixel in the internal format of the render backend.
	virtual uint32_t MakePixel(const uint8_t red, const uint8_t green,
	                           const uint8_t blue) = 0;
};

#endif // DOSBOX_RENDER_BACKEND_H
