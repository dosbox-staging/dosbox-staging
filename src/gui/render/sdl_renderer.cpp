// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sdl_renderer.h"

#include <memory>

#include <SDL3/SDL.h>

#include "gui/private/common.h"

#include "capture/capture.h"
#include "config/setup.h"
#include "gui/render/format_convert.h"
#include "misc/video.h"
#include "utils/checks.h"
#include "utils/math_utils.h"

CHECK_NARROWING();

static constexpr SDL_PixelFormat SdlPixelFormat = SDL_PIXELFORMAT_XRGB8888;

SdlRenderer::SdlRenderer(const int x, const int y, const int width,
                         const int height, const SDL_WindowFlags sdl_window_flags,
                         const std::string& render_driver,
                         TextureFilterMode texture_filter_mode)
        : texture_filter_mode(texture_filter_mode)
{
	SDL_WindowFlags flags = sdl_window_flags |
	                        OpenGlDriverCrashWorkaround(render_driver);

	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, DOSBOX_NAME);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, x);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);

	// For window flags you should use separate window creation properties,
	// but for easier migration from SDL2 you can use the following:
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, flags);
	window = SDL_CreateWindowWithProperties(props);

	if (!window && (flags & SDL_WINDOW_OPENGL)) {
		// The opengl_driver_crash_workaround() call above conditionally
		// sets SDL_WINDOW_OPENGL. It sometimes gets this wrong (e.g.,
		// SDL_VIDEODRIVER=dummy). This can only be determined reliably
		// by trying SDL_CreateWindow(). If we failed to create the
		// window, try again without it.
		flags &= ~SDL_WINDOW_OPENGL;

		SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, flags);
		window = SDL_CreateWindowWithProperties(props);
	}

	SDL_DestroyProperties(props);

	if (!window) {
		const auto msg = format_str("SDL: Error creating window: %s",
		                            SDL_GetError());
		LOG_ERR("%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	if (!InitRenderer(render_driver)) {
		SDL_DestroyWindow(window);

		const auto msg = "SDL: Error creating SDL renderer";
		LOG_ERR("%s", msg);
		throw std::runtime_error(msg);
	}
}

// This is a hack to prevent SDL2 from re-creating the window internally. It
// prevents crashes on Windows and Linux, and prevents the initial window from
// being visibly destroyed for window managers that show animations while
// creating the window (e.g., Gnome 3).
//
SDL_WindowFlags SdlRenderer::OpenGlDriverCrashWorkaround(const std::string_view render_driver) const
{
	if (render_driver.starts_with("opengl")) {
		return SDL_WINDOW_OPENGL;
	}
	if (render_driver != "auto") {
		return 0;
	}

	static int default_driver_is_opengl = -1;
	if (default_driver_is_opengl >= 0) {
		return (default_driver_is_opengl ? SDL_WINDOW_OPENGL : 0);
	}

	// According to SDL3 all renderers support target textures; the default
	// driver is the first in the list, so just query its name.
	const char* name         = SDL_GetRenderDriver(0);
	default_driver_is_opengl = (name &&
	                            std::string_view(name).starts_with("opengl"));

	return (default_driver_is_opengl ? SDL_WINDOW_OPENGL : 0);
}

bool SdlRenderer::InitRenderer(const std::string& render_driver)
{
	const auto has_specific_driver = (render_driver != "auto");

	if (has_specific_driver &&
	    !SDL_SetHint(SDL_HINT_RENDER_DRIVER, render_driver.c_str())) {
		LOG_WARNING("SDL: Error setting '%s' SDL render driver hint: %s",
		            render_driver.c_str(),
		            SDL_GetError());
	}

	renderer = SDL_CreateRenderer(window, nullptr);
	if (!renderer && has_specific_driver) {
		if (!SDL_ResetHint(SDL_HINT_RENDER_DRIVER)) {
			LOG_WARNING("SDL: Error resetting SDL render driver hint: %s",
			            SDL_GetError());
		}

		renderer = SDL_CreateRenderer(window, nullptr);
		if (renderer) {
			const auto initial_error = std::string(SDL_GetError());

			LOG_WARNING(
			        "SDL: Error creating '%s' SDL render driver (%s); "
			        "falling back to automatic selection",
			        render_driver.c_str(),
			        initial_error.c_str());

			set_section_property_value("sdl", "texture_renderer", "auto");
		}
	}

	if (!renderer) {
		LOG_ERR("SDL: Error creating renderer: %s", SDL_GetError());
		return false;
	}

	LOG_MSG("SDL: Using '%s' SDL render driver", SDL_GetRendererName(renderer));

	if (!SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE)) {
		LOG_ERR("SDL: Error setting render clear color: %s", SDL_GetError());
	}

	return true;
}

SdlRenderer::~SdlRenderer()
{
	if (renderer) {
		// Frees associated textures automatically.
		SDL_DestroyRenderer(renderer);
		renderer = {};
		texture  = {};
	}
	if (curr_framebuf.surface) {
		SDL_DestroySurface(curr_framebuf.surface);
		curr_framebuf = {};
	}
	if (last_framebuf.surface) {
		SDL_DestroySurface(last_framebuf.surface);
		last_framebuf = {};
	}
	if (window) {
		SDL_DestroyWindow(window);
		window = {};
	}
}

SDL_Window* SdlRenderer::GetWindow()
{
	return window;
}

DosBox::Rect SdlRenderer::GetCanvasSizeInPixels()
{
	SDL_Rect canvas_size_px = {};
	SDL_GetWindowSizeInPixels(window, &canvas_size_px.w, &canvas_size_px.h);

	const auto r = to_rect(canvas_size_px);
	assert(r.HasPositiveSize());

	return r;
}

void SdlRenderer::NotifyViewportSizeChanged(const DosBox::Rect draw_rect_px)
{
	const auto sdl_draw_rect_px = to_sdl_rect(draw_rect_px);

	if (!SDL_SetRenderViewport(renderer, &sdl_draw_rect_px)) {
		LOG_ERR("SDL: Error setting viewport: %s", SDL_GetError());
	}
}

void SdlRenderer::NotifyRenderSizeChanged([[maybe_unused]] const int render_width_px,
                                          [[maybe_unused]] const int render_height_px)
{
	// No-op: the texture is sized to the CPU-doubled frame dimensions
	// from within `UploadFrame()`.
}

void SdlRenderer::MaybeRecreateTexture(const int width_px, const int height_px)
{
	if (texture) {
		float curr_width_px  = 0.0f;
		float curr_height_px = 0.0f;

		SDL_GetTextureSize(texture, &curr_width_px, &curr_height_px);

		if (static_cast<int>(curr_width_px) == width_px &&
		    static_cast<int>(curr_height_px) == height_px) {
			return;
		}
		SDL_DestroyTexture(texture);
	}

	texture = SDL_CreateTexture(renderer,
	                            SdlPixelFormat,
	                            SDL_TEXTUREACCESS_STREAMING,
	                            width_px,
	                            height_px);
	if (!texture) {
		LOG_ERR("SDL: Error creating SDL texture: %s", SDL_GetError());
		return;
	}

	switch (texture_filter_mode) {
	case TextureFilterMode::NearestNeighbour:
		if (!SDL_SetTextureScaleMode(texture,
		                             SDL_ScaleMode::SDL_SCALEMODE_NEAREST)) {
			LOG_ERR("SDL: Error setting texture filtering mode: %s",
			        SDL_GetError());
		}
		break;

	case TextureFilterMode::Bilinear:
		if (!SDL_SetTextureScaleMode(texture,
		                             SDL_ScaleMode::SDL_SCALEMODE_LINEAR)) {
			LOG_ERR("SDL: Error setting texture filtering mode: %s",
			        SDL_GetError());
		}
		break;

	default: assertm(false, "Invalid TextureFilterMode"); return;
	}
}

void SdlRenderer::UploadFrame(const uint32_t* pixels, const int width_px,
                              const int height_px, const int pitch_bytes,
                              const bool double_width, const bool double_height,
                              [[maybe_unused]] const VideoMode& video_mode)
{
	// The SDL renderer has no shader pipeline to promote the source-size
	// frame to the rendered size, so the doubling is done on the CPU,
	// in-flight while writing the rows into the texture.
	const auto x_scale = double_width ? 2 : 1;
	const auto y_scale = double_height ? 2 : 1;

	MaybeRecreateTexture(width_px * x_scale, height_px * y_scale);

	if (!texture) {
		return;
	}

	void* dst           = nullptr;
	int dst_pitch_bytes = 0;

	// A streaming texture's previous contents are undefined after
	// locking, but every upload writes the complete frame
	if (!SDL_LockTexture(texture, nullptr, &dst, &dst_pitch_bytes)) {
		LOG_ERR("SDL: Error locking texture: %s", SDL_GetError());
		return;
	}

	const auto src_pitch_px = pitch_bytes / static_cast<int>(sizeof(uint32_t));

	for (auto out_y = 0; out_y < height_px * y_scale; ++out_y) {
		const auto* src_row = pixels + (out_y / y_scale) * src_pitch_px;

		auto* dst_row = reinterpret_cast<uint32_t*>(
		        static_cast<uint8_t*>(dst) +
		        static_cast<size_t>(out_y) * dst_pitch_bytes);

		ExpandBgrx32Row(src_row, dst_row, width_px, double_width);
	}

	SDL_UnlockTexture(texture);
}

SdlRenderer::SetShaderResult SdlRenderer::SetShader(
        [[maybe_unused]] const std::string& symbolic_shader_descriptor)
{
	// no shader support; always report success
	//
	// If we didn't, the rendering backend agnostic fallback mechanism would
	// fail and we'd hard exit.
	return SetShaderResult::Ok;
}

void SdlRenderer::NotifyVideoModeChanged([[maybe_unused]] const VideoMode& video_mode)
{
	// no shader support
	return;
}

void SdlRenderer::ForceReloadCurrentShader()
{
	// no shader support
	return;
}

ShaderInfo SdlRenderer::GetCurrentShaderInfo()
{
	// no shader support
	return {};
}

ShaderPreset SdlRenderer::GetCurrentShaderPreset()
{
	// no shader support
	return {};
}

std::string SdlRenderer::GetCurrentSymbolicShaderDescriptor()
{
	// no shader support
	return {};
}

ShaderDescriptor SdlRenderer::GetCurrentShaderDescriptor()
{
	// no-op (no shader support)
	return {};
}

void SdlRenderer::StartFrame(uint32_t*& pixels_out, int& pitch_out)
{
	assert(curr_framebuf.surface);

	// Some surfaces must be locked for direct access
	curr_framebuf.LockSurface();

	pixels_out = static_cast<uint32_t*>(curr_framebuf.surface->pixels);
	pitch_out  = curr_framebuf.surface->pitch;
}

void SdlRenderer::EndFrame()
{
	assert(curr_framebuf.surface);
	assert(last_framebuf.surface);

	last_framebuf.LockSurface();

	// We need to copy the buffers. We can't just swap them because the VGA
	// emulation only writes the changed pixels to the framebuffer in each
	// frame.

	// TODO Couldn't get SDL_BlitSurface to work... If you
	// can, feel free to use that here, but this works
	// perfectly fine.
	std::memcpy(last_framebuf.surface->pixels,
	            curr_framebuf.surface->pixels,
	            (curr_framebuf.surface->h * curr_framebuf.surface->pitch));

	last_framebuf_dirty = true;
}

void SdlRenderer::PrepareFrame()
{
	// `UploadFrame()` writes directly into the texture, so there is
	// nothing to prepare. (This remains part of the interface until the
	// StartFrame/EndFrame protocol is removed.)
}

void SdlRenderer::PresentFrame()
{
	SDL_RenderClear(renderer);

	// Null until the first frame has been uploaded
	if (texture) {
		SDL_RenderTexture(renderer, texture, nullptr, nullptr);
	}

	if (CAPTURE_IsCapturingPostRenderImage()) {
		// glReadPixels() implicitly blocks until all pipelined
		// rendering commands have finished, so we're guaranteed to
		// read the contents of the up-to-date backbuffer here right
		// before the buffer swap.
		//
		GFX_CaptureRenderedImage();
	}

	SDL_RenderPresent(renderer);
}

void SdlRenderer::SetVsync(const bool is_enabled)
{
	if (!SDL_SetRenderVSync(renderer, (is_enabled ? 1 : 0))) {
		LOG_WARNING("SDL: Error %s vsync: %s",
		            (is_enabled ? "enabling" : "disabling"),
		            SDL_GetError());
	}
}

void SdlRenderer::SetColorSpace([[maybe_unused]] const ColorSpace color_space)
{
	// no-op (no colour space support)
}

void SdlRenderer::SetImageAdjustmentSettings(
        [[maybe_unused]] const ImageAdjustmentSettings& settings)
{
	// no-op (no image adjustment support)
}

void SdlRenderer::SetDeditheringStrength([[maybe_unused]] const float strength)
{
	// no-op (no image adjustment support)
}

void SdlRenderer::EnableImageAdjustments([[maybe_unused]] const bool enable)
{
	// no-op (no image adjustment support)
}

RenderedImage SdlRenderer::ReadPixelsPostShader(const DosBox::Rect output_rect_px)
{
	// Create new image
	RenderedImage image = {};

	image.params.width              = iroundf(output_rect_px.w);
	image.params.height             = iroundf(output_rect_px.h);
	image.params.double_width       = false;
	image.params.double_height      = false;
	image.params.pixel_aspect_ratio = {1};
	image.params.pixel_format       = PixelFormat::BGR24_ByteArray;

	image.pitch = image.params.width *
	              (get_bits_per_pixel(image.params.pixel_format) / 8);

	const auto image_size_bytes = check_cast<uint32_t>(image.params.height *
	                                                   image.pitch);

	image.image_data = new uint8_t[image_size_bytes];

	image.is_flipped_vertically = false;

	// SDL pixel formats are a bit weird coming from OpenGL...
	// You would think SDL_PIXELFORMAT_XBGR8888 is an alias of
	// SDL_PIXELFORMAT_BGR24, but the two are actually very
	// different:
	//
	// - SDL_PIXELFORMAT_BGR24 is an "array format"; it specifies
	//   the endianness-agnostic memory layout just like OpenGL
	//   pixel formats.
	//
	// - SDL_PIXELFORMAT_XBGR8888 is a "packed format" which uses
	//   native types, therefore its memory layout depends on the
	//   endianness.
	//
	// More info: https://afrantzis.com/pixel-format-guide/sdl2.html
	//
	const SDL_Rect read_rect_px = to_sdl_rect(output_rect_px);

	const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
	        SDL_RenderReadPixels(renderer, &read_rect_px), SDL_DestroySurface);
	if (!surface) {
		LOG_ERR("SDL: Error reading pixels from the texture renderer: %s",
		        SDL_GetError());
		return image;
	}

	// Unlike SDL2, SDL3 returns the pixels in the renderer's native format,
	// so convert them into the BGR24 destination buffer in a single step.
	if (!SDL_ConvertPixels(image.params.width,
	                       image.params.height,
	                       surface->format,
	                       surface->pixels,
	                       surface->pitch,
	                       SDL_PIXELFORMAT_BGR24,
	                       image.image_data,
	                       image.pitch)) {
		LOG_ERR("SDL: Error converting texture renderer pixels: %s",
		        SDL_GetError());
	}

	return image;
}

uint32_t SdlRenderer::MakePixel(const uint8_t red, const uint8_t green,
                                const uint8_t blue)
{
	static_assert(SdlPixelFormat == SDL_PIXELFORMAT_XRGB8888);
	return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
}

void SdlSurface::LockSurface()
{
	assert(surface);
	if (SDL_MUSTLOCK(surface) && !is_locked) {
		SDL_LockSurface(surface);
		is_locked = true;
	}
}

void SdlSurface::UnlockSurface()
{
	assert(surface);
	if (SDL_MUSTLOCK(surface) && is_locked) {
		SDL_UnlockSurface(surface);
		is_locked = false;
	}
}
