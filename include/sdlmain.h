/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_SDLMAIN_H
#define DOSBOX_SDLMAIN_H
 
#include <string>
#include <string_view>
#include <string.h>
#include "SDL.h"

#if C_OPENGL
#include <SDL_opengl.h>
#endif

#include "fraction.h"
#include "render.h"
#include "shader_manager.h"
#include "video.h"

#define SDL_NOFRAME 0x00000020

// Texture buffer and presentation functions and type-defines
using update_frame_buffer_f = void(const uint16_t *);
using present_frame_f = bool();
constexpr void update_frame_noop([[maybe_unused]] const uint16_t *) { /* no-op */ }
static inline bool present_frame_noop() { return true; }

enum SCREEN_TYPES	{
	SCREEN_SURFACE,
	SCREEN_TEXTURE,
#if C_OPENGL
	SCREEN_OPENGL
#endif
};

enum class FrameMode {
	Unset,
	Cfr,          // constant frame rate, as defined by the emulated system
	Vfr,          // variable frame rate, as defined by the emulated system
	ThrottledVfr, // variable frame rate, throttled to the display's rate
};

enum class HostRateMode {
	Auto,
	Sdi, // serial digital interface
	Vrr, // variable refresh rate
	Custom,
};

enum class VsyncState {
	Unset    = -2,
	Adaptive = -1,
	Off      = 0,
	On       = 1,
	Yield    = 2,
};

// The vsync settings consists of three parts:
//  - What the user asked for.
//  - What the measured state is after setting the requested vsync state.
//    The video driver may honor the requested vsync state, ignore it, change
//    it, or be outright buggy.
//  - The benchmarked rate is the actual frame rate after setting the requested
//    stated, and is used to determined the measured state.
//
struct VsyncSettings {
	VsyncState requested = VsyncState::Unset;
	VsyncState measured  = VsyncState::Unset;
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

struct SDL_Block {
	bool initialized = false;
	bool active = false; // If this isn't set don't draw
	bool updating = false;
	bool update_display_contents = true;
	bool resizing_window = false;
	bool wait_on_error = false;

	InterpolationMode interpolation_mode    = InterpolationMode::Bilinear;
	IntegerScalingMode integer_scaling_mode = IntegerScalingMode::Off;

	struct {
		int width = 0;
		int height = 0;
		Fraction render_pixel_aspect_ratio = {1};

		bool has_changed = false;
		GFX_CallBack_t callback = nullptr;
		bool width_was_doubled = false;
		bool height_was_doubled = false;
	} draw = {};

	VideoMode video_mode = {};

	struct {
		struct {
			int width = 0;
			int height = 0;
			bool fixed = false;
			bool display_res = false;
		} full = {};
		struct {
			// user-configured window size
			int width = 0;
			int height = 0;
			bool resizable = false;
			bool show_decorations = true;
			bool adjusted_initial_size = false;
			int initial_x_pos = -1;
			int initial_y_pos = -1;
			// instantaneous canvas size of the window
			SDL_Rect canvas_size = {};
		} window = {};
		struct {
			int width = 0;
			int height = 0;
		} requested_window_bounds = {};

		PixelFormat pixel_format = {};
		double dpi_scale = 1.0;
		bool fullscreen = false;

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
		double preferred_host_rate = 0.0;
		bool want_resizable_window = false;
		SCREEN_TYPES type = SCREEN_SURFACE;
		SCREEN_TYPES want_type = SCREEN_SURFACE;
	} desktop = {};
	struct {
		int num_cycles = 0;
		std::string hint_mouse_str  = {};
		std::string hint_paused_str = {};
		std::string cycles_ms_str   = {};
	} title_bar = {};

	struct {
		VsyncSettings when_windowed   = {};
		VsyncSettings when_fullscreen = {};
		int skip_us                   = 0;
	} vsync = {};

#if C_OPENGL
	struct {
		SDL_GLContext context;
		int pitch = 0;
		void *framebuf = nullptr;
		GLuint buffer;
		GLuint texture;
		GLuint displaylist;
		GLint max_texsize;
		bool bilinear;
		bool pixel_buffer_object = false;
		bool npot_textures_supported = false;
		bool use_shader;
		bool framebuffer_is_srgb_encoded;
		GLuint program_object;

		ShaderInfo shader_info    = {};
		std::string shader_source = {};

		struct {
			GLint texture_size;
			GLint input_size;
			GLint output_size;
			GLint frame_count;
		} ruby = {};
		GLuint actual_frame_count;
		GLfloat vertex_data[2*3];
	} opengl = {};
#endif // C_OPENGL
	struct {
		PRIORITY_LEVELS active   = PRIORITY_LEVEL_AUTO;
		PRIORITY_LEVELS inactive = PRIORITY_LEVEL_AUTO;
	} priority = {};

	bool mute_when_inactive  = false;
	bool pause_when_inactive = false;

	SDL_Rect clip = {0, 0, 0, 0};
	SDL_Surface *surface = nullptr;
	SDL_Window *window = nullptr;
	SDL_Renderer *renderer = nullptr;
	std::string render_driver = "";
	int display_number = 0;

	struct {
		SDL_Surface *input_surface = nullptr;
		SDL_Texture *texture = nullptr;
		SDL_PixelFormat *pixelFormat = nullptr;
	} texture = {};

	struct {
		present_frame_f* present      = present_frame_noop;
		update_frame_buffer_f* update = update_frame_noop;
		FrameMode desired_mode        = FrameMode::Unset;
		FrameMode mode                = FrameMode::Unset;
		double period_ms    = 0.0; // in ms, for use with PIC timers
		int period_us       = 0; // same but in us, for use with chrono
		int period_us_early = 0;
		int period_us_late  = 0;
		int8_t vfr_dupe_countdown = 0;
	} frame = {};

	SDL_Rect updateRects[1024] = {};

	bool use_exact_window_resolution = false;
	bool use_viewport_limits = false;

	SDL_Point viewport_resolution = {-1, -1};
#if defined (WIN32)
	// Time when sdl regains focus (Alt+Tab) in windowed mode
	int64_t focus_ticks = 0;
#endif
	// State of Alt keys for certain special handlings
	SDL_EventType laltstate = SDL_KEYUP;
	SDL_EventType raltstate = SDL_KEYUP;
};

extern SDL_Block sdl;

#endif
