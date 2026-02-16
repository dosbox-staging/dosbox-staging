// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sdl_renderer.h"

#include "gui/private/common.h"

#include "capture/capture.h"
#include "config/setup.h"
#include "misc/video.h"
#include "utils/checks.h"
#include "utils/math_utils.h"

CHECK_NARROWING();

static constexpr uint32_t SdlPixelFormat = SDL_PIXELFORMAT_ARGB8888;

SdlRenderer::SdlRenderer(const int x, const int y, const int width,
                         const int height, const uint32_t sdl_window_flags,
                         const std::string& render_driver,
                         TextureFilterMode texture_filter_mode)
        : texture_filter_mode(texture_filter_mode)
{
	auto flags = sdl_window_flags | OpenGlDriverCrashWorkaround(render_driver);

#ifdef MACOSX
	SDL_SetHint(SDL_HINT_MAC_COLOR_SPACE, "srgb");
#endif

	window = SDL_CreateWindow(DOSBOX_NAME, x, y, width, height, flags);

	if (!window && (flags & SDL_WINDOW_OPENGL)) {
		// The opengl_driver_crash_workaround() call above conditionally
		// sets SDL_WINDOW_OPENGL. It sometimes gets this wrong (e.g.,
		// SDL_VIDEODRIVER=dummy). This can only be determined reliably
		// by trying SDL_CreateWindow(). If we failed to create the
		// window, try again without it.
		flags &= ~SDL_WINDOW_OPENGL;

		window = SDL_CreateWindow(DOSBOX_NAME, x, y, width, height, flags);
	}
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
uint32_t SdlRenderer::OpenGlDriverCrashWorkaround(const std::string_view render_driver) const
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

	// According to SDL2 documentation, the first driver in the list is the
	// default one.
	int i = 0;
	SDL_RendererInfo info;

	while (SDL_GetRenderDriverInfo(i, &info) == 0) {
		if (info.flags & SDL_RENDERER_TARGETTEXTURE) {
			break;
		}
		++i;
	}

	default_driver_is_opengl = std::string_view(info.name).starts_with("opengl");

	return (default_driver_is_opengl ? SDL_WINDOW_OPENGL : 0);
}

bool SdlRenderer::InitRenderer(const std::string& render_driver)
{
	if (render_driver != "auto") {
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, render_driver.c_str());
	}

	constexpr uint32_t Flags = 0;

	renderer = SDL_CreateRenderer(window, -1, Flags);
	if (!renderer) {
		LOG_ERR("SDL: Error creating renderer: %s", SDL_GetError());
		return false;
	}

	// Log the rendering driver
	SDL_RendererInfo info;

	if (SDL_GetRendererInfo(renderer, &info) < 0) {
		LOG_ERR("SDL: Error retrieving SDL renderer info: %s",
		        SDL_GetError());
		SDL_DestroyRenderer(renderer);
		return false;
	}
	LOG_MSG("SDL: Using '%s' SDL render driver", info.name);

	if (SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE) < 0) {
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
	if (curr_framebuf) {
		SDL_FreeSurface(curr_framebuf);
		curr_framebuf = {};
	}
	if (last_framebuf) {
		SDL_FreeSurface(last_framebuf);
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
#if SDL_VERSION_ATLEAST(2, 26, 0)
	SDL_GetWindowSizeInPixels(window, &canvas_size_px.w, &canvas_size_px.h);
#else
	if (SDL_GetRendererOutputSize(renderer,
	                              &canvas_size_px.w,
	                              &canvas_size_px.h) < 0) {
		LOG_ERR("SDL: Error retrieving output size: %s", SDL_GetError());
	}
#endif

	const auto r = to_rect(canvas_size_px);
	assert(r.HasPositiveSize());

	return r;
}

void SdlRenderer::NotifyViewportSizeChanged(const DosBox::Rect draw_rect_px)
{
	const auto sdl_draw_rect_px = to_sdl_rect(draw_rect_px);

	if (SDL_RenderSetViewport(renderer, &sdl_draw_rect_px) < 0) {
		LOG_ERR("SDL: Error setting viewport: %s", SDL_GetError());
	}
}

void SdlRenderer::NotifyRenderSizeChanged(const int render_width_px,
                                          const int render_height_px)
{
	if (texture) {
		SDL_DestroyTexture(texture);
	}

	texture = SDL_CreateTexture(renderer,
	                            SdlPixelFormat,
	                            SDL_TEXTUREACCESS_STREAMING,
	                            render_width_px,
	                            render_height_px);
	if (!texture) {
		LOG_ERR("SDL: Error creating SDL texture: %s", SDL_GetError());
		return;
	}

	switch (texture_filter_mode) {
	case TextureFilterMode::NearestNeighbour:
		if (SDL_SetTextureScaleMode(texture,
		                            SDL_ScaleMode::SDL_ScaleModeNearest) < 0) {
			LOG_ERR("SDL: Error setting texture filtering mode: %s",
			        SDL_GetError());
		}
		break;

	case TextureFilterMode::Bilinear:
		if (SDL_SetTextureScaleMode(texture,
		                            SDL_ScaleMode::SDL_ScaleModeLinear) < 0) {
			LOG_ERR("SDL: Error setting texture filtering mode: %s",
			        SDL_GetError());
		}
		break;

	default: assertm(false, "Invalid TextureFilterMode"); return;
	}

	// unused; must be 0
	constexpr auto Flags    = 0;
	constexpr auto BitDepth = 32;

	if (curr_framebuf) {
		SDL_FreeSurface(curr_framebuf);
	}
	if (last_framebuf) {
		SDL_FreeSurface(last_framebuf);
	}

	curr_framebuf = SDL_CreateRGBSurfaceWithFormat(Flags,
	                                               render_width_px,
	                                               render_height_px,
	                                               BitDepth,
	                                               SdlPixelFormat);

	last_framebuf = SDL_CreateRGBSurfaceWithFormat(Flags,
	                                               render_width_px,
	                                               render_height_px,
	                                               BitDepth,
	                                               SdlPixelFormat);

	if (!curr_framebuf || !last_framebuf) {
		SDL_DestroyTexture(texture);
		LOG_ERR("SDL: Error creating input surface: %s", SDL_GetError());
		return;
	}
}

SdlRenderer::SetShaderResult SdlRenderer::SetShader(
        [[maybe_unused]] const std::string& symbolic_shader_descriptor)
{
	// no-op; always report success (no shader support)
	//
	// If we didn't, the rendering backend agnostic fallback mechanism would
	// fail and we'd hard exit.
	return SetShaderResult::Ok;
}

void SdlRenderer::NotifyVideoModeChanged([[maybe_unused]] const VideoMode& video_mode)
{
	// no-op (no shader support)
	return;
}

void SdlRenderer::ForceReloadCurrentShader()
{
	// no-op
}

ShaderInfo SdlRenderer::GetCurrentShaderInfo()
{
	// no-op (no shader support)
	return {};
}

ShaderPreset SdlRenderer::GetCurrentShaderPreset()
{
	// no-op (no shader support)
	return {};
}

std::string SdlRenderer::GetCurrentSymbolicShaderDescriptor()
{
	// no-op (no shader support)
	return {};
}

void SdlRenderer::StartFrame(uint32_t*& pixels_out, int& pitch_out)
{
	assert(curr_framebuf);

	// Some surfaces must be locked for direct access
	if (SDL_MUSTLOCK(curr_framebuf)) {
		SDL_LockSurface(curr_framebuf);
	}

	pixels_out = reinterpret_cast<uint32_t*>(curr_framebuf->pixels);
	pitch_out  = curr_framebuf->pitch;
}

void SdlRenderer::EndFrame()
{
	assert(curr_framebuf);
	assert(last_framebuf);

	if (SDL_MUSTLOCK(last_framebuf)) {
		SDL_LockSurface(last_framebuf);
	}

	// We need to copy the buffers. We can't just swap them because the VGA
	// emulation only writes the changed pixels to the framebuffer in each
	// frame.

	// TODO Couldn't get SDL_BlitSurface to work... If you can, feel free to
	// use that here, but this works perfectly fine.
	std::memcpy(last_framebuf->pixels,
	            curr_framebuf->pixels,
	            (curr_framebuf->h * curr_framebuf->pitch));

	last_framebuf_dirty = true;
}

void SdlRenderer::PrepareFrame()
{
	assert(texture);
	assert(last_framebuf);

	if (SDL_MUSTLOCK(last_framebuf)) {
		SDL_UnlockSurface(last_framebuf);
	}

	if (last_framebuf_dirty) {
		SDL_UpdateTexture(texture,
		                  nullptr, // entire texture
		                  last_framebuf->pixels,
		                  last_framebuf->pitch);

		last_framebuf_dirty = false;
	}
}

void SdlRenderer::PresentFrame()
{
	assert(texture);

	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, nullptr, nullptr);

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
	if (SDL_RenderSetVSync(renderer, (is_enabled ? 1 : 0))) {
		LOG_ERR("SDL: Error %s vsync: %s",
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

	// SDL2 pixel formats are a bit weird coming from OpenGL...
	// You would think SDL_PIXELFORMAT_BGR888 is an alias of
	// SDL_PIXELFORMAT_BGR24, but the two are actually very
	// different:
	//
	// - SDL_PIXELFORMAT_BGR24 is an "array format"; it specifies
	//   the endianness-agnostic memory layout just like OpenGL
	//   pixel formats.
	//
	// - SDL_PIXELFORMAT_BGR888 is a "packed format" which uses
	//   native types, therefore its memory layout depends on the
	//   endianness.
	//
	// More info: https://afrantzis.com/pixel-format-guide/sdl2.html
	//
	const SDL_Rect read_rect_px = to_sdl_rect(output_rect_px);

	if (SDL_RenderReadPixels(renderer,
	                         &read_rect_px,
	                         SDL_PIXELFORMAT_BGR24,
	                         image.image_data,
	                         image.pitch) != 0) {

		LOG_ERR("SDL: Error reading pixels from the texture renderer: %s",
		        SDL_GetError());
	}

	return image;
}

uint32_t SdlRenderer::MakePixel(const uint8_t red, const uint8_t green,
                                const uint8_t blue)
{
	static_assert(SdlPixelFormat == SDL_PIXELFORMAT_ARGB8888);
	return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
}
