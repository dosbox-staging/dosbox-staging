// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SDLMAIN_H
#define DOSBOX_SDLMAIN_H

#include "SDL.h"

#include <cstring>
#include <optional>
#include <string>
#include <string_view>

#if C_OPENGL
#include <SDL_opengl.h>
#endif

#include "fraction.h"
#include "rect.h"
#include "render.h"
#include "shader_manager.h"
#include "video.h"

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

#define SDL_NOFRAME 0x00000020

// Texture buffer and presentation functions and type-defines
using update_frame_buffer_f = void(const uint16_t*);
using present_frame_f       = bool();

constexpr void update_frame_noop([[maybe_unused]] const uint16_t*)
{
	// no-op
}

static inline bool present_frame_noop()
{
	return true;
}

enum class FrameMode {
	Unset,

	// Constant frame rate, as defined by the emulated system
	Cfr,

	// Variable frame rate, as defined by the emulated system
	Vfr,

	// Variable frame rate, throttled to the display's rate
	ThrottledVfr,
};

enum class HostRateMode {
	Auto,

	// Serial digital interface
	Sdi,

	// Variable refresh rate
	Vrr,

	Custom,
};

enum class VsyncMode { Unset, Off, On, Adaptive, Yield };

enum class FullscreenMode { Standard, Original, ForcedBorderless };

struct VsyncSettings {
	// The vsync mode the user asked for.
	VsyncMode requested = VsyncMode::Unset;

	// What the auto-determined state is after setting the requested vsync state.
	// The video driver may honor the requested vsync mode, ignore it, change
	// it, or be outright buggy.
	VsyncMode auto_determined  = VsyncMode::Unset;

	// The actual frame rate after setting the requested vsync mode; it's used
	// to select the auto-determined vsync mode.
	int benchmarked_rate = 0;
};

enum PRIORITY_LEVELS {
	PRIORITY_LEVEL_AUTO,
	PRIORITY_LEVEL_LOWEST,
	PRIORITY_LEVEL_LOWER,
	PRIORITY_LEVEL_NORMAL,
	PRIORITY_LEVEL_HIGHER,
	PRIORITY_LEVEL_HIGHEST
};

enum class SDL_DosBoxEvents : uint8_t {
	RefreshAnimatedTitle,
	NumEvents // dummy, keep last, do not use
};

struct SDL_Block {
	bool initialized     = false;
	bool active          = false; // If this isn't set don't draw
	bool updating        = false;
	bool resizing_window = false;
	bool wait_on_error   = false;

	uint32_t start_event_id = UINT32_MAX;

#ifdef WIN32
	uint16_t original_code_page = 0;
#endif

	bool is_paused = false;

	RenderingBackend rendering_backend      = RenderingBackend::Texture;
	RenderingBackend want_rendering_backend = RenderingBackend::Texture;

	struct {
		int render_width_px                = 0;
		int render_height_px               = 0;
		Fraction render_pixel_aspect_ratio = {1};

		bool has_changed        = false;
		GFX_Callback_t callback = nullptr;
		bool width_was_doubled  = false;
		bool height_was_doubled = false;
	} draw = {};

	// The DOS video mode is populated after we set up the SDL window.
	std::optional<VideoMode> maybe_video_mode = {};

	struct {
		struct {
			FullscreenMode mode = {};

			int width  = 0;
			int height = 0;

			bool is_forced_borderless_fullscreen = false;

			struct {
				int width     = 0;
				int height    = 0;
				int x_pos     = 0;
				int y_pos     = 0;
			} prev_window;
		} fullscreen = {};

		struct {
			// User-configured window size
			int width  = 0;
			int height = 0;
			int x_pos  = SDL_WINDOWPOS_UNDEFINED;
			int y_pos  = SDL_WINDOWPOS_UNDEFINED;

			bool show_decorations      = true;
			bool adjusted_initial_size = false;

			// Instantaneous canvas size of the window
			SDL_Rect canvas_size = {};
		} window = {};

		struct {
			int width  = 0;
			int height = 0;
		} requested_window_bounds = {};

		PixelFormat pixel_format = {};

		float dpi_scale = 1.0f;

		bool is_fullscreen = false;

		// This flag indicates, that we are in the process of switching
		// between fullscreen or window (as oppososed to changing
		// rendering size due to rotating screen, emulation state, or
		// user resizing the window).
		bool switching_fullscreen = false;

		// Lazy window size init triggers updating window size and
		// position when leaving fullscreen for the first time.
		// See FinalizeWindowState function for details.
		bool lazy_init_window_size = false;

		HostRateMode host_rate_mode = HostRateMode::Auto;
		double preferred_host_rate  = 0.0;
	} desktop = {};

	struct {
		VsyncSettings when_windowed   = {};
		VsyncSettings when_fullscreen = {};
		int skip_us                   = 0;
	} vsync = {};

#if C_OPENGL
	struct {
		SDL_GLContext context;
		int pitch      = 0;
		void* framebuf = nullptr;
		GLuint texture;
		GLint max_texsize;
		GLuint program_object;

		int texture_width_px  = 0;
		int texture_height_px = 0;

		ShaderInfo shader_info    = {};
		std::string shader_source = {};

		struct {
			GLint texture_size;
			GLint input_size;
			GLint output_size;
			GLint frame_count;
		} ruby = {};

		GLuint actual_frame_count;
		GLfloat vertex_data[2 * 3];
	} opengl = {};
#endif // C_OPENGL

	struct {
		PRIORITY_LEVELS active   = PRIORITY_LEVEL_AUTO;
		PRIORITY_LEVELS inactive = PRIORITY_LEVEL_AUTO;
	} priority = {};

	bool mute_when_inactive  = false;
	bool pause_when_inactive = false;

	SDL_Rect draw_rect_px     = {};
	SDL_Window* window        = nullptr;
	SDL_Renderer* renderer    = nullptr;
	std::string render_driver = "";
	int display_number        = 0;

	struct {
		SDL_Surface* input_surface   = nullptr;
		SDL_Texture* texture         = nullptr;
		SDL_PixelFormat* pixelFormat = nullptr;

		InterpolationMode interpolation_mode = InterpolationMode::Bilinear;
	} texture = {};

	struct {
		present_frame_f* present      = present_frame_noop;
		update_frame_buffer_f* update = update_frame_noop;
		FrameMode desired_mode        = FrameMode::Unset;
		FrameMode mode                = FrameMode::Unset;

		// in ms, for use with PIC timers
		double period_ms      = 0.0;
		float max_dupe_frames = 0.0f;

		// same but in us, for use with chrono
		int period_us       = 0;
		int period_us_early = 0;
		int period_us_late  = 0;
	} frame = {};

	bool use_exact_window_resolution = false;

#if defined(WIN32)
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
