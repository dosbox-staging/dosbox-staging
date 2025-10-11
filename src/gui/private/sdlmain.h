// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SDLMAIN_H
#define DOSBOX_SDLMAIN_H

#include "gui/private/shader_manager.h"

#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dosbox_config.h"
#include "gui/common.h"
#include "gui/render/render.h"
#include "gui/render/render_backend.h"
#include "misc/video.h"
#include "utils/fraction.h"

// must be included after dosbox_config.h
#include "SDL.h"

// The image rendered in the emulated computer's raw framebuffer as raw pixels
// goes through a number of transformations until it gets shown on the host
// display. It is important to use a common vocabulary for the terms involved
// in these various stages and to apply them consistently. To understand
// the difference between logical units and pixels, please see `video.h`.
//
// Video mode dimensions
// ---------------------
//   The dimensions of the DOS video mode in raw pixels as stored on disk or
//   in the emulated video card's framebuffer (e.g., 320x200 = 64000 pixels).
//
// Rendered image size
// -------------------
//   Size of the final rendered image in pixels *after* width and height
//   doubling has been applied (e.g. 320x200 VGA is width and height doubled
//   (scan-doubled) to 640x400; 320x200 CGA composite output is quadrupled in
//   width to 1280x200, etc.). The rendered image size is more or less
//   analogous to the actual video signal the CRT monitor "sees" (e.g., a
//   monitor cannot differentiate between 320x200 double-scanned to 640x400,
//   or an actual 640x400 video mode, as they're identical at the analog VGA
//   signal level). In OpenGL mode, this is the size of the input image in
//   pixels sent to GLSL shaders.
//
// Canvas size
// -----------
//   The unrestricted total available drawing area of the emulator window or
//   the screen in fullscreen. This is reported by SDL as logical units.
//
// Viewport rectangle
// ------------------
//   The maximum area we can *potentially* draw into in logical units.
//   Normally, it's smaller than the canvas, but it can also be larger in
//   certain viewport modes where we "zoom into" the image, or when we
//   simulate the horiz/vert stretch controls of CRT monitors. In these cases,
//   the canvas effectively acts as our "window" into the oversized viewport,
//   and one or both coordinates of the viewport rectangle's start point are
//   negative.
//
//   IMPORTANT: Note that this viewport concept is different to what SDL &
//   OpenGL calls the "viewport". Technically, we set the SDL/OpenGL viewport
//   to the draw rectangle described below.
//
// Draw rectangle
// --------------
//   The actual draw rectangle in pixels after applying all rendering
//   constraints such as integer scaling. It's always 100% filled with the
//   final output image, so its ratio is equal to the output display aspect
//   ratio. The draw rectangle is always equal to or is contained within the
//   viewport rectangle.
//
//   We set the SDL/OpenGL viewport (which is different to our *our* viewport
//   concept) to the draw rectangle without any further transforms. In OpenGL
//   mode, this is the size of the final output image coming out of the
//   shaders, which is the image that is displayed on the host monitor with
//   1:1 physical pixel mapping.
//
//   Because the viewport can be larger than the canvas, the draw area can be
//   larger too. In other words, the draw rectangle can extend beyond the
//   edges of the window or the screen in fullscreen mode, in which case the
//   image is centered and the overhanging areas are clipped.
//

enum class FullscreenMode { Standard, Original, ForcedBorderless };

enum class SDL_DosBoxEvents : uint8_t {
	RefreshAnimatedTitle,
	NumEvents // dummy, keep last, do not use
};

struct SDL_Block {
	uint32_t start_event_id = UINT32_MAX;

	SDL_Window* window = {};
	int display_number = 0;

	float dpi_scale    = 1.0f;
	bool is_fullscreen = false;

	// True when the contents of the framebuffer has been changed in the
	// current frame. We only need to upload new texture data when this flag
	// is true in GFX_EndUpdate().
	bool updating_framebuffer = false;

	bool is_paused           = false;
	bool mute_when_inactive  = false;
	bool pause_when_inactive = false;

	SDL_Rect draw_rect_px = {};

	// Key state for certain special handlings
	struct {
		SDL_EventType left_alt_state  = SDL_KEYUP;
		SDL_EventType right_alt_state = SDL_KEYUP;
	} key = {};

	// TODO rename to RenderBackend, move intoe `render_backend.h`
	RenderingBackend rendering_backend = RenderingBackend::Texture;
	RenderingBackend want_rendering_backend = RenderingBackend::Texture;
	InterpolationMode interpolation_mode   = {};

	std::unique_ptr<RenderBackend> renderer = {};

	struct {
		int render_width_px                = 0;
		int render_height_px               = 0;
		Fraction render_pixel_aspect_ratio = {1};

		GFX_Callback_t callback = {};
		bool width_was_doubled  = false;
		bool height_was_doubled = false;

		bool active = false;
	} draw = {};

	// The DOS video mode is populated after we set up the SDL window.
	std::optional<VideoMode> maybe_video_mode = {};

	struct {
		int width  = 0;
		int height = 0;
		int x_pos  = SDL_WINDOWPOS_UNDEFINED;
		int y_pos  = SDL_WINDOWPOS_UNDEFINED;

		// Instantaneous canvas size of the window
		SDL_Rect canvas_size = {};
	} windowed = {};

	struct {
		FullscreenMode mode = {};

		int width  = 0;
		int height = 0;

		bool is_forced_borderless_fullscreen = false;

		struct {
			int width  = 0;
			int height = 0;
			int x_pos  = 0;
			int y_pos  = 0;
		} prev_window;
	} fullscreen = {};

	struct {
		PresentationMode windowed_mode   = {};
		PresentationMode fullscreen_mode = {};

		int frame_time_us            = 0;
		int early_present_window_us  = 0;
		int64_t last_present_time_us = 0;
	} presentation = {};

	struct {
		bool windowed   = false;
		bool fullscreen = false;
	} vsync = {};

#if defined(WIN32)
	// TODO check of this workaround is still needed
	// Time when sdl regains focus (Alt+Tab) in windowed mode
	int64_t focus_ticks = 0;
#endif

	// State of Alt keys for certain special handlings
	SDL_EventType laltstate = SDL_KEYUP;
	SDL_EventType raltstate = SDL_KEYUP;
};

// TODO should be private; introduce SDL_* API calls instead
extern SDL_Block sdl;

#endif
