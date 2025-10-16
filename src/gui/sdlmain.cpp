// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/sdlmain.h"

#include "private/common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sys/types.h>
#include <unistd.h>

#if C_DEBUGGER
#include <queue>
#endif

#include "audio/mixer.h"
#include "capture/capture.h"
#include "config/config.h"
#include "config/setup.h"
#include "cpu/cpu.h"
#include "dosbox.h"
#include "gui/mapper.h"
#include "gui/render/opengl_renderer.h"
#include "gui/render/sdl_renderer.h"
#include "gui/titlebar.h"
#include "hardware/input/keyboard.h"
#include "hardware/input/mouse.h"
#include "hardware/timer.h"
#include "hardware/video/vga.h"
#include "misc/cross.h"
#include "misc/notifications.h"
#include "misc/support.h"
#include "misc/tracy.h"
#include "misc/video.h"
#include "utils/checks.h"
#include "utils/math_utils.h"
#include "utils/rect.h"
#include "utils/string_utils.h"

// must be included after dosbox_config.h
#include <SDL.h>

CHECK_NARROWING();

// #define DEBUG_WINDOW_EVENTS

constexpr uint32_t sdl_version_to_uint32(const SDL_version version)
{
	return (version.major << 16) + (version.minor << 8) + version.patch;
}

[[maybe_unused]] static bool is_runtime_sdl_version_at_least(const SDL_version min_version)
{
	SDL_version version = {};
	SDL_GetVersion(&version);
	const auto curr_version = sdl_version_to_uint32(version);

	return curr_version >= sdl_version_to_uint32(min_version);
}

SDL_Block sdl;

static SDL_Point FallbackWindowSize = {640, 480};

DosBox::Rect to_rect(const SDL_Rect r)
{
	return {r.x, r.y, r.w, r.h};
}

SDL_Rect to_sdl_rect(const DosBox::Rect& r)
{
	return {iroundf(r.x), iroundf(r.y), iroundf(r.w), iroundf(r.h)};
}

#if C_DEBUGGER

extern SDL_Window* pdc_window;
extern std::queue<SDL_Event> pdc_event_queue;

static bool is_debugger_event(const SDL_Event& event)
{
	switch (event.type) {
	case SDL_KEYDOWN:
	case SDL_KEYUP:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEMOTION:
	case SDL_MOUSEWHEEL:
	case SDL_TEXTINPUT:
	case SDL_TEXTEDITING:
	case SDL_USEREVENT:
	case SDL_WINDOWEVENT:
		return event.window.windowID == SDL_GetWindowID(pdc_window);

	default: return false;
	}
}

SDL_Window* GFX_GetSDLWindow()
{
	return sdl.window;
}
#endif

// Globals for keyboard initialisation
static bool startup_state_numlock  = false;
static bool startup_state_capslock = false;

// Detects if we're running within a desktop environment (or window manager).
bool GFX_HaveDesktopEnvironment()
{
// On BSD and Linux, it's possible that the user is running directly on the
// console without a windowing environment. For example, SDL can directly
// interface with the host's OpenGL/GLES drivers, the console's frame buffer, or
// the Raspberry Pi's DISPMANX driver.
//
#if defined(BSD) || defined(LINUX)
	// The presence of any of the following variables set by either the
	// login manager, display manager, or window manager itself is
	// sufficient evidence to say the user has a desktop session.
	//
	// References:
	// https://www.freedesktop.org/software/systemd/man/pam_systemd.html#desktop=
	// https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#recognized-keys
	// https://askubuntu.com/questions/72549/how-to-determine-which-window-manager-and-desktop-environment-is-running
	// https://unix.stackexchange.com/questions/116539/how-to-detect-the-desktop-environment-in-a-bash-script

	static bool already_checked          = false;
	static bool have_desktop_environment = false;

	if (!already_checked) {
		constexpr const char* EnvVars[] = {"XDG_CURRENT_DESKTOP",
		                                   "XDG_SESSION_DESKTOP",
		                                   "DESKTOP_SESSION",
		                                   "GDMSESSION"};

		have_desktop_environment = std::any_of(std::begin(EnvVars),
		                                       std::end(EnvVars),
		                                       std::getenv);

		already_checked = true;
	}

	return have_desktop_environment;
#else
	// Assume we have a desktop environment on all other systems
	return true;
#endif
}

double GFX_GetHostRefreshRate()
{
	auto refresh_rate = [] {
		SDL_DisplayMode mode = {};

		const auto sdl_rate = mode.refresh_rate;

		assert(sdl.window);
		const auto display_in_use = SDL_GetWindowDisplayIndex(sdl.window);

		constexpr auto DefaultHostRefreshRateHz = 60;

		if (display_in_use < 0) {
			LOG_ERR("SDL: Could not get the current window index: %s",
			        SDL_GetError());
			return DefaultHostRefreshRateHz;
		}
		if (SDL_GetCurrentDisplayMode(display_in_use, &mode) != 0) {
			LOG_ERR("SDL: Could not get the current display mode: %s",
			        SDL_GetError());
			return DefaultHostRefreshRateHz;
		}
		if (sdl_rate < RefreshRateMin) {
			LOG_WARNING("SDL: Got a strange refresh rate of %d Hz",
			            sdl_rate);
			return DefaultHostRefreshRateHz;
		}

		assert(sdl_rate >= RefreshRateMin);
		return sdl_rate;
	}();

	return refresh_rate;
}

static void validate_vsync_and_presentation_mode_settings()
{
	const std::string vsync_pref = get_sdl_section()->GetString("vsync");

	const std::string presentation_mode_pref = get_sdl_section()->GetString(
	        "presentation_mode");

	if (presentation_mode_pref == "dos-rate" &&
	    (vsync_pref == "on" || vsync_pref == "fullscreen-only")) {

		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "DISPLAY",
		                      "DISPLAY_INVALID_VSYNC_SETTING",
		                      vsync_pref.c_str());

		set_section_property_value("sdl", "vsync", "off");
	}
}

// Reset and populate the vsync settings from the config. This is called
// on-demand after startup and on output mode changes (e.g., switching from
// the 'texture' backend to 'opengl').
//
static void configure_vsync()
{
	const std::string vsync_pref = get_sdl_section()->GetString("vsync");

	if (has_false(vsync_pref)) {
		sdl.vsync.windowed   = false;
		sdl.vsync.fullscreen = false;

	} else if (has_true(vsync_pref)) {
		sdl.vsync.windowed   = true;
		sdl.vsync.fullscreen = true;

	} else {
		assert(vsync_pref == "fullscreen-only");

		sdl.vsync.windowed   = false;
		sdl.vsync.fullscreen = true;
	}
}

static void configure_presentation_mode()
{
	using enum PresentationMode;

	const std::string presentation_mode_pref = get_sdl_section()->GetString(
	        "presentation_mode");

	if (presentation_mode_pref == "dos-rate") {
		sdl.presentation.windowed_mode   = DosRate;
		sdl.presentation.fullscreen_mode = DosRate;

	} else if (presentation_mode_pref == "host-rate") {
		sdl.presentation.windowed_mode   = HostRate;
		sdl.presentation.fullscreen_mode = HostRate;

	} else {
		assert(presentation_mode_pref == "auto");

		sdl.presentation.windowed_mode = sdl.vsync.windowed ? HostRate
		                                                    : DosRate;

		sdl.presentation.fullscreen_mode = sdl.vsync.fullscreen ? HostRate
		                                                        : DosRate;
	}
}

static void configure_renderer()
{
	const std::string output = get_sdl_section()->GetString("output");

	if (output == "texture") {
		sdl.want_rendering_backend = RenderingBackend::Texture;
		sdl.interpolation_mode     = InterpolationMode::Bilinear;

	} else if (output == "texturenb") {
		sdl.want_rendering_backend = RenderingBackend::Texture;
		sdl.interpolation_mode = InterpolationMode::NearestNeighbour;

#if C_OPENGL
	} else if (output == "opengl") {
		sdl.want_rendering_backend = RenderingBackend::OpenGl;
#endif

	} else {
		// TODO convert to notification
		LOG_WARNING("SDL: Unsupported output device '%s', using 'texture' output mode",
		            output.c_str());

		sdl.want_rendering_backend = RenderingBackend::Texture;
	}
}

void GFX_RequestExit(const bool pressed)
{
	if (pressed) {
		DOSBOX_RequestShutdown();
		LOG_DEBUG("SDL: Exit requested");
	}
}

#if defined(MACOSX)
static bool is_command_pressed(const SDL_Event event)
{
	return (event.key.keysym.mod == KMOD_RGUI ||
	        event.key.keysym.mod == KMOD_LGUI);
}
#endif

[[maybe_unused]] static void pause_emulation(bool pressed)
{
	if (!pressed) {
		return;
	}
	const auto inkeymod = static_cast<uint16_t>(SDL_GetModState());

	sdl.is_paused = true;
	TITLEBAR_RefreshTitle();

	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		// flush event queue.
	}

	// Prevent the mixer from running while in our pause loop
	// Muting is not ideal for some sound devices such as GUS that loop
	// samples This also saves CPU time by not rendering samples we're not
	// going to play anyway
	MIXER_LockMixerThread();

	// NOTE: This is one of the few places where we use SDL key codes with
	// SDL 2.0, rather than scan codes. Is that the correct behavior?
	while (sdl.is_paused && !DOSBOX_IsShutdownRequested()) {
		// since we're not polling, CPU usage drops to 0.
		SDL_WaitEvent(&event);

		switch (event.type) {
		case SDL_QUIT: GFX_RequestExit(true); break;

		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESTORED) {
				// We may need to re-create a texture and more
				GFX_ResetScreen();
			}
			break;

		case SDL_KEYDOWN:
#if defined(MACOSX)
			// Pause/unpause is hardcoded to Command+P on macOS
			if (is_command_pressed(event) &&
			    event.key.keysym.sym == SDLK_p) {
#else
			// Pause/unpause is hardcoded to Alt+Pause on Window &
			// Linux
			if (event.key.keysym.sym == SDLK_PAUSE) {
#endif
				const uint16_t outkeymod = event.key.keysym.mod;
				if (inkeymod != outkeymod) {
					KEYBOARD_ClrBuffer();
					MAPPER_LosingFocus();
					// Not perfect if the pressed Alt key is
					// switched, but then we have to insert
					// the keys into the mapper or
					// create/rewrite the event and push it.
					// Which is tricky due to possible use
					// of scancodes.
				}
				sdl.is_paused = false;
				TITLEBAR_RefreshTitle();
				break;
			}

#if defined(MACOSX)
			if (is_command_pressed(event) &&
			    event.key.keysym.sym == SDLK_q) {
				// On macOS, Command+Q is the default key to
				// close an application
				GFX_RequestExit(true);
				break;
			}
#endif
		}
	}
	MIXER_UnlockMixerThread();
}

void GFX_Stop()
{
	if (sdl.updating_framebuffer) {
		GFX_EndUpdate();
	}
	sdl.active = false;
}

void GFX_ResetScreen()
{
	GFX_Stop();
	if (sdl.draw.callback) {
		(sdl.draw.callback)(GFX_CallbackReset);
	}
	GFX_Start();

	CPU_ResetAutoAdjust();

	VGA_SetupDrawing(0);
}

static bool is_vsync_enabled()
{
	return sdl.desktop.is_fullscreen ? sdl.vsync.fullscreen : sdl.vsync.windowed;
}

PresentationMode GFX_GetPresentationMode()
{
	return sdl.desktop.is_fullscreen ? sdl.presentation.fullscreen_mode
	                                 : sdl.presentation.windowed_mode;
}

DosBox::Rect GFX_CalcDrawRectInPixels(const DosBox::Rect& canvas_size_px)
{
	const DosBox::Rect render_size_px = {sdl.draw.render_width_px,
	                                     sdl.draw.render_height_px};

	const auto r = RENDER_CalcDrawRectInPixels(canvas_size_px,
	                                           render_size_px,
	                                           sdl.draw.render_pixel_aspect_ratio);

	return {iroundf(r.x), iroundf(r.y), iroundf(r.w), iroundf(r.h)};
}

static void log_presentation_and_vsync_mode()
{
	const auto presentation_rate = []() -> std::string {
		switch (GFX_GetPresentationMode()) {
		case PresentationMode::DosRate: return "DOS rate";

		case PresentationMode::HostRate:
			return format_str("%2.5g Hz host rate",
			                  GFX_GetHostRefreshRate());

		default: assertm(false, "Invalid PresentationMode"); return "";
		}
	}();

	LOG_MSG("DISPLAY: Presenting at %s %s vsync",
	        presentation_rate.c_str(),
	        (is_vsync_enabled() ? "with" : "without"));
}

static void maybe_log_display_properties()
{
	assert(sdl.draw.render_width_px > 0 && sdl.draw.render_height_px > 0);

	const auto canvas_size_px = sdl.renderer->GetCanvasSizeInPixels();
	const auto draw_size_px   = GFX_CalcDrawRectInPixels(canvas_size_px);

	assert(draw_size_px.HasPositiveSize());

	const auto refresh_rate = VGA_GetRefreshRate();

	if (sdl.maybe_video_mode) {
		const auto video_mode = *sdl.maybe_video_mode;

		static VideoMode last_video_mode               = {};
		static double last_refresh_rate                = 0.0;
		static PresentationMode last_presentation_mode = {};
		static DosBox::Rect last_draw_size_px          = {};
		static bool last_width_was_doubled             = false;
		static bool last_height_was_doubled            = false;
		static Fraction last_pixel_aspect_ratio        = {};

		if (last_video_mode != video_mode ||
		    last_refresh_rate != refresh_rate ||
		    last_presentation_mode != GFX_GetPresentationMode() ||
		    last_draw_size_px.w != draw_size_px.w ||
		    last_draw_size_px.h != draw_size_px.h ||
		    last_width_was_doubled != sdl.draw.width_was_doubled ||
		    last_height_was_doubled != sdl.draw.height_was_doubled ||
		    last_pixel_aspect_ratio != sdl.draw.render_pixel_aspect_ratio) {

			const auto& par = video_mode.pixel_aspect_ratio;

			LOG_MSG("DISPLAY: %s at %2.5g Hz, scaled to %dx%d pixels "
			        "with 1:%1.6g (%d:%d) pixel aspect ratio",
			        to_string(video_mode).c_str(),
			        refresh_rate,
			        iroundf(draw_size_px.w),
			        iroundf(draw_size_px.h),
			        par.Inverse().ToDouble(),
			        static_cast<int32_t>(par.Num()),
			        static_cast<int32_t>(par.Denom()));

			log_presentation_and_vsync_mode();

			last_video_mode         = video_mode;
			last_refresh_rate       = refresh_rate;
			last_presentation_mode  = GFX_GetPresentationMode();
			last_draw_size_px       = draw_size_px;
			last_width_was_doubled  = sdl.draw.width_was_doubled;
			last_height_was_doubled = sdl.draw.height_was_doubled;
			last_pixel_aspect_ratio = sdl.draw.render_pixel_aspect_ratio;
		}

	} else {
		LOG_MSG("SDL: Window size initialized to %dx%d pixels",
		        iroundf(draw_size_px.w),
		        iroundf(draw_size_px.h));
	}

#if 0
	const auto scale_x = static_cast<double>(draw_size_px.w) / render_width_px;
	const auto scale_y = static_cast<double>(draw_size_px.h) / render_height_px;

	const auto one_per_render_pixel_aspect = scale_y / scale_x;

	LOG_MSG("DISPLAY: render_width_px: %d, render_height_px: %d, "
	        "render pixel aspect ratio: 1:%1.3g",
	        render_width_px,
	        render_height_px,
	        one_per_render_pixel_aspect);
#endif
}

static void setup_presentation_mode()
{
	auto update_frame_time = [](const double rate_hz) {
		assert(rate_hz > 0);

		const auto frame_time_ms       = 1000.0 / rate_hz;
		sdl.presentation.frame_time_us = ifloor(frame_time_ms * 1000.0);
	};

	switch (GFX_GetPresentationMode()) {
	case PresentationMode::DosRate: {
		update_frame_time(VGA_GetRefreshRate());

		// In 'dos-rate' mode, we just present the frame whenever it's
		// ready, so the duration of the window doesn't matter if it's
		// large enough to allow for the frame time jitter.
		//
		sdl.presentation.early_present_window_us = sdl.presentation.frame_time_us;
	} break;

	case PresentationMode::HostRate: {
		update_frame_time(GFX_GetHostRefreshRate());

		// The primary use case for the 'host-rate' mode is a fixed
		// refresh rate monitor running at 60 Hz with vsync enabled
		// (with vsync off, we might as well just use 'dos-rate'). In
		// this scenario, we need to present the frame a bit before the
		// vsync happens, otherwise we'd "miss the train" and would have
		// to wait for an extra frame period. This would increase
		// latency and possibly cause audio glitches because it's a
		// blocking wait, so it's better to be a bit generous with the
		// time window.
		//
		// This value was determined by experimentation on our supported
		// OSes. We might turn this a config setting if there's enough
		// evidence that no single single value works well on all
		// systems, but so far it seems to do the job.
		//
		sdl.presentation.early_present_window_us = 3000;
	} break;
	}

	sdl.presentation.last_present_time_us = 0;
}

static void notify_new_mouse_screen_params()
{
	if (sdl.draw_rect_px.w <= 0 || sdl.draw_rect_px.h <= 0) {
		// Filter out unusual parameters, which can be the result
		// of window minimized due to ALT+TAB, for example
		return;
	}

	MouseScreenParams params = {};

	// It is important to scale not just the size of the rectangle but also
	// its starting point by the inverse of the DPI scale factor.
	params.draw_rect =
	        to_rect(sdl.draw_rect_px).Copy().Scale(1.0f / sdl.desktop.dpi_scale);

	int abs_x = 0;
	int abs_y = 0;
	SDL_GetMouseState(&abs_x, &abs_y);

	params.x_abs = static_cast<float>(abs_x);
	params.y_abs = static_cast<float>(abs_y);

	params.is_fullscreen    = sdl.desktop.is_fullscreen;
	params.is_multi_display = (SDL_GetNumVideoDisplays() > 1);

	MOUSE_NewScreenParams(params);
}

static bool is_aspect_ratio_correction_enabled()
{
	return (RENDER_GetAspectRatioCorrectionMode() ==
	        AspectRatioCorrectionMode::Auto);
}

static void update_fallback_dimensions(const double dpi_scale)
{
	// TODO This only works for 320x200 games. We cannot make hardcoded
	// assumptions about aspect ratios in general, e.g. the pixel aspect
	// ratio is 1:1 for 640x480 games both with 'aspect = on` and 'aspect =
	// off'.
	const auto fallback_height = (is_aspect_ratio_correction_enabled() ? 480
	                                                                   : 400) /
	                             dpi_scale;

	assert(dpi_scale > 0);
	const auto fallback_width = 640 / dpi_scale;

	FallbackWindowSize = {iround(fallback_width), iround(fallback_height)};

	// TODO pixels or logical units?
	// LOG_INFO("SDL: Updated fallback dimensions to %dx%d",
	//          FallbackWindowSize.x,
	//          FallbackWindowSize.y);

	// Keep the SDL minimum allowed window size in lock-step with the
	// fallback dimensions. If these aren't linked, the window can obscure
	// the content (or vice-versa).
	if (!sdl.window) {
		return;
		// LOG_WARNING("SDL: Tried setting window minimum size,"
		//             " but the SDL window is not available yet");
	}
	SDL_SetWindowMinimumSize(sdl.window,
	                         FallbackWindowSize.x,
	                         FallbackWindowSize.y);

	// TODO pixels or logical units?
	// LOG_INFO("SDL: Updated window minimum size to %dx%d", width, height);
}

// This is a collection point for things affected by DPI changes, instead of
// duplicating these calls at every point in the code where we save a new DPI
static void apply_new_dpi_scale(const double dpi_scale)
{
	update_fallback_dimensions(dpi_scale);

	// add more functions here
}

static void check_and_handle_dpi_change(SDL_Window* sdl_window,
                                        int width_in_logical_units = 0)
{
	if (width_in_logical_units <= 0) {
		SDL_GetWindowSize(sdl_window, &width_in_logical_units, nullptr);
	}

	const auto canvas_size_px = sdl.renderer->GetCanvasSizeInPixels();

	assert(width_in_logical_units > 0);

	const auto new_dpi_scale = canvas_size_px.w /
	                           static_cast<float>(width_in_logical_units);

	if (std::abs(new_dpi_scale - sdl.desktop.dpi_scale) < DBL_EPSILON) {
		// LOG_MSG("SDL: DPI scale hasn't changed (still %f)",
		//         sdl.desktop.dpi_scale);
		return;
	}
	sdl.desktop.dpi_scale = new_dpi_scale;
	// LOG_MSG("SDL: DPI scale updated from %f to %f",
	//         sdl.desktop.dpi_scale,
	//         new_dpi_scale);

	apply_new_dpi_scale(new_dpi_scale);
}

static void maybe_handle_screen_rotation(const int new_width, const int new_height)
{
	// Maybe a screen rotation has just occurred, so we simply resize.
	// There may be a different cause for a forced resized, though.
	if (sdl.desktop.is_fullscreen) {

		// Note: We should not use get_display_dimensions()
		// (SDL_GetDisplayBounds) on Android after a screen rotation:
		// The older values from application startup are returned.
		sdl.desktop.fullscreen.width  = new_width;
		sdl.desktop.fullscreen.height = new_height;
	}
}

static void configure_window_transparency()
{
	const auto transparency = get_sdl_section()->GetInt("window_transparency");
	const auto alpha = static_cast<float>(100 - transparency) / 100.0f;

	SDL_SetWindowOpacity(sdl.window, alpha);
}

static void configure_window_decorations()
{
	SDL_SetWindowBordered(sdl.window,
	                      get_sdl_section()->GetBool("window_decorations")
	                              ? SDL_TRUE
	                              : SDL_FALSE);
}

static void enter_fullscreen()
{
	sdl.desktop.is_fullscreen = true;

	if (sdl.desktop.fullscreen.mode == FullscreenMode::ForcedBorderless) {

		// enter_fullscreen() can be called multiple times in a row by
		// the existing code while still in fullscreen mode. That would
		// throw off the window state restoring logic of the forced
		// borderless mode. This a long-standing issue and needs to be
		// fixed, eventually, but it's not so simple. In the meantime,
		// the easy solution to make borderless fullscreen work
		// correctly is to maintain its own fullscreen state flag.
		//
		// Once enter_fullscreen() and exit_fullscreen() are always
		// called in pairs, this workaround can be removed.
		//
		if (sdl.desktop.fullscreen.is_forced_borderless_fullscreen) {
			return;
		}

		// "Emulate" SDL's built-in borderless fullscreen mode by
		// turning off window decorations and resizing the window to
		// cover the entire desktop. But this would trigger exclusive
		// fullscreen on Windows so we'd be no better off -- the trick
		// is to size the window one pixel wider than the desktop so
		// fullscreen optimisation won't kick in.
		//
		SDL_GetWindowSize(sdl.window,
		                  &sdl.desktop.fullscreen.prev_window.width,
		                  &sdl.desktop.fullscreen.prev_window.height);

		SDL_GetWindowPosition(sdl.window,
		                      &sdl.desktop.fullscreen.prev_window.x_pos,
		                      &sdl.desktop.fullscreen.prev_window.y_pos);

		SDL_SetWindowBordered(sdl.window, SDL_FALSE);
		SDL_SetWindowResizable(sdl.window, SDL_FALSE);
		SDL_SetWindowPosition(sdl.window, 0, 0);

		SDL_SetWindowSize(sdl.window,
		                  sdl.desktop.fullscreen.width + 1,
		                  sdl.desktop.fullscreen.height);

		sdl.desktop.fullscreen.is_forced_borderless_fullscreen = true;

		// Disable transparency in fullscreen mode
		SDL_SetWindowOpacity(sdl.window, 100);

		maybe_log_display_properties();

	} else {
		const auto mode = (sdl.desktop.fullscreen.mode ==
		                   FullscreenMode::Standard)
		                        ? SDL_WINDOW_FULLSCREEN_DESKTOP
		                        : SDL_WINDOW_FULLSCREEN;

		SDL_SetWindowFullscreen(sdl.window, mode); //-V2006
	}

	// We need to disable transparency in fullscreen on macOS
	SDL_SetWindowOpacity(sdl.window, 100);
}

static void exit_fullscreen()
{
	sdl.desktop.is_fullscreen = false;

	if (sdl.desktop.fullscreen.mode == FullscreenMode::ForcedBorderless) {
		// Restore the previous window state when exiting our "fake"
		// borderless fullscreen mode.
		//
		configure_window_decorations();

		SDL_SetWindowResizable(sdl.window, SDL_TRUE);

		SDL_SetWindowSize(sdl.window,
		                  sdl.desktop.fullscreen.prev_window.width,
		                  sdl.desktop.fullscreen.prev_window.height);

		SDL_SetWindowPosition(sdl.window,
		                      sdl.desktop.fullscreen.prev_window.x_pos,
		                      sdl.desktop.fullscreen.prev_window.y_pos);

		configure_window_transparency();

		sdl.desktop.fullscreen.is_forced_borderless_fullscreen = false;

		maybe_log_display_properties();

	} else {
		constexpr auto WindowedMode = 0;
		SDL_SetWindowFullscreen(sdl.window, WindowedMode);

		// On macOS, SDL_SetWindowSize() and SDL_SetWindowPosition()
		// calls in fullscreen mode are no-ops, so we need to set the
		// potentially changed window size and position when exiting
		// fullscreen mode.
		SDL_SetWindowSize(sdl.window,
		                  sdl.desktop.window.width,
		                  sdl.desktop.window.height);

		SDL_SetWindowPosition(sdl.window,
		                      sdl.desktop.window.x_pos,
		                      sdl.desktop.window.y_pos);
	}

	// We need to disable transparency in fullscreen on macOS
	configure_window_transparency();

	configure_window_decorations();
}

// Returns the current window; used for mapper UI.
SDL_Window* GFX_GetWindow()
{
	return sdl.window;
}

DosBox::Rect GFX_GetCanvasSizeInPixels()
{
	return sdl.renderer->GetCanvasSizeInPixels();
}

RenderingBackend GFX_GetRenderingBackend()
{
	return sdl.rendering_backend;
}

static SDL_Rect get_desktop_size()
{
	SDL_Rect desktop = {};
	assert(sdl.display_number >= 0);

	SDL_GetDisplayBounds(sdl.display_number, &desktop);

	// Deduct the border decorations from the desktop size
	int top    = 0;
	int left   = 0;
	int bottom = 0;
	int right  = 0;

	SDL_GetWindowBordersSize(SDL_GetWindowFromID(sdl.display_number),
	                         &top,
	                         &left,
	                         &bottom,
	                         &right);

	// If SDL_GetWindowBordersSize fails, it populates the values with 0.
	desktop.w -= (left + right);
	desktop.h -= (top + bottom);

	assert(desktop.w >= FallbackWindowSize.x);
	assert(desktop.h >= FallbackWindowSize.y);
	return desktop;
}

DosBox::Rect GFX_GetDesktopSize()
{
	return to_rect(get_desktop_size());
}

DosBox::Rect GFX_GetViewportSizeInPixels()
{
	const auto canvas_size_px = sdl.renderer->GetCanvasSizeInPixels();

	return RENDER_CalcRestrictedViewportSizeInPixels(canvas_size_px);
}

float GFX_GetDpiScaleFactor()
{
	return sdl.desktop.dpi_scale;
}

static bool is_using_kmsdrm_driver()
{
	const bool is_initialized = SDL_WasInit(SDL_INIT_VIDEO);

	const auto driver = is_initialized ? SDL_GetCurrentVideoDriver()
	                                   : getenv("SDL_VIDEODRIVER");
	if (!driver) {
		return false;
	}

	std::string driver_str = driver;
	lowcase(driver_str);
	return driver_str == "kmsdrm";
}

static bool check_kmsdrm_setting()
{
	// Do we have read access to the event subsystem
	if (auto f = fopen("/dev/input/event0", "r"); f) {
		fclose(f);
		return true;
	}

	// We're using KMSDRM, but we don't have read access to the event
	// subsystem
	return false;
}

[[maybe_unused]] static bool operator!=(const SDL_Point lhs, const SDL_Point rhs)
{
	return lhs.x != rhs.x || lhs.y != rhs.y;
}

static void update_viewport()
{
	const auto canvas_size_px = sdl.renderer->GetCanvasSizeInPixels();
	const auto draw_rect_px   = GFX_CalcDrawRectInPixels(canvas_size_px);

	sdl.draw_rect_px = to_sdl_rect(draw_rect_px);

	sdl.renderer->UpdateViewport(draw_rect_px);
}

uint8_t GFX_SetSize(const int render_width_px, const int render_height_px,
                    const Fraction& render_pixel_aspect_ratio, const uint8_t flags,
                    const VideoMode& video_mode, GFX_Callback_t callback)
{
	if (sdl.updating_framebuffer) {
		GFX_EndUpdate();
	}

	GFX_Stop();
	// The rendering objects are recreated below with new sizes, after which
	// frame rendering is re-engaged with the output-type specific calls.

	const bool double_width  = flags & GFX_DBL_W;
	const bool double_height = flags & GFX_DBL_H;

	sdl.draw.render_width_px           = render_width_px;
	sdl.draw.render_height_px          = render_height_px;
	sdl.draw.width_was_doubled         = double_width;
	sdl.draw.height_was_doubled        = double_height;
	sdl.draw.render_pixel_aspect_ratio = render_pixel_aspect_ratio;

	sdl.maybe_video_mode = video_mode;

	sdl.draw.callback = callback;

	// Update fullscreen display mode.
	if (!sdl.desktop.is_fullscreen) {
		SDL_DisplayMode desired = {};
		SDL_GetDesktopDisplayMode(sdl.display_number, &desired);

		desired.w = sdl.draw.render_width_px;
		desired.h = sdl.draw.render_height_px;

		SDL_DisplayMode closest = {};
		SDL_GetClosestDisplayMode(sdl.display_number, &desired, &closest);
		SDL_SetWindowDisplayMode(sdl.window, &closest);
	}

	if (!sdl.renderer->UpdateRenderSize(sdl.draw.render_width_px,
	                                    sdl.draw.render_height_px)) {
		LOG_ERR("SDL: Failed to update texture");
	}

	update_viewport();
	setup_presentation_mode();

	// Ensure mouse emulation knows the current parameters
	notify_new_mouse_screen_params();

	maybe_log_display_properties();

	const auto gfx_flags = sdl.renderer->GetGfxFlags();
	if (gfx_flags) {
		GFX_Start();
	}

	return gfx_flags;
}

void GFX_SetShader([[maybe_unused]] const ShaderInfo& shader_info,
                   [[maybe_unused]] const std::string& shader_source)
{
	sdl.renderer->SetShader(shader_info, shader_source);
}

void GFX_CenterMouse()
{
	assert(sdl.window);

	int width  = 0;
	int height = 0;

#if defined(WIN32)
	if (is_runtime_sdl_version_at_least({2, 28, 1})) {
		SDL_GetWindowSize(sdl.window, &width, &height);

	} else {
		const auto canvas_size_px = sdl.renderer->GetCanvasSizeInPixels();

		width  = iroundf(canvas_size_px.w);
		height = iroundf(canvas_size_px.h);
	}
#else
	SDL_GetWindowSize(sdl.window, &width, &height);
#endif

	SDL_WarpMouseInWindow(sdl.window, width / 2, height / 2);
}

void GFX_SetMouseRawInput(const bool requested_raw_input)
{
	if (SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP,
	                            requested_raw_input ? "0" : "1",
	                            SDL_HINT_OVERRIDE) != SDL_TRUE) {

		LOG_WARNING("SDL: Failed to %s raw mouse input",
		            requested_raw_input ? "enable" : "disable");
	}
}

void GFX_SetMouseCapture(const bool requested_capture)
{
	const auto param = requested_capture ? SDL_TRUE : SDL_FALSE;
	if (SDL_SetRelativeMouseMode(param) != 0) {
		SDL_ShowCursor(SDL_ENABLE);

		E_Exit("SDL: Failed to %s relative-mode [SDL Bug]",
		       requested_capture ? "put the mouse in"
		                         : "take the mouse out of");
	}
}

void GFX_SetMouseVisibility(const bool requested_visible)
{
	const auto param = requested_visible ? SDL_ENABLE : SDL_DISABLE;
	if (SDL_ShowCursor(param) < 0) {
		E_Exit("SDL: Failed to make mouse cursor %s [SDL Bug]",
		       requested_visible ? "visible" : "invisible");
	}
}

static void focus_input()
{
#if defined(WIN32)
	sdl.focus_ticks = GetTicks();
#endif

	// Ensure we have input focus when in fullscreen
	if (!sdl.desktop.is_fullscreen) {
		return;
	}
	// Do we already have focus?
	if (SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_INPUT_FOCUS) {
		return;
	}

	// If not, raise-and-focus to prevent stranding the window
	SDL_RaiseWindow(sdl.window);
	SDL_SetWindowInputFocus(sdl.window);
}

#if defined(WIN32)
STICKYKEYS stick_keys = {sizeof(STICKYKEYS), 0};

static void sticky_keys(bool restore)
{
	static bool inited = false;

	if (!inited) {
		inited = true;
		SystemParametersInfo(SPI_GETSTICKYKEYS,
		                     sizeof(STICKYKEYS),
		                     &stick_keys,
		                     0);
	}
	if (restore) {
		SystemParametersInfo(SPI_SETSTICKYKEYS,
		                     sizeof(STICKYKEYS),
		                     &stick_keys,
		                     0);
		return;
	}
	// Get current sticky keys layout:
	STICKYKEYS s = {sizeof(STICKYKEYS), 0};
	SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);

	if (!(s.dwFlags & SKF_STICKYKEYSON)) { // Not on already
		s.dwFlags &= ~SKF_HOTKEYACTIVE;
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
	}
}
#endif // WIN32

static void switch_fullscreen()
{
	// Record the window's current canvas size if we're departing window-mode
	if (!sdl.desktop.is_fullscreen) {
		sdl.desktop.window.canvas_size = to_sdl_rect(
		        sdl.renderer->GetCanvasSizeInPixels());
	}

#if defined(WIN32)
	// We are about to switch to the opposite of our current mode
	// (ie: opposite of whatever sdl.desktop.is_fullscreen holds).
	// Sticky-keys should be set to the opposite of fullscreen,
	// so we simply apply the bool of the mode we're switching out-of.
	sticky_keys(sdl.desktop.is_fullscreen);
#endif
	if (sdl.desktop.is_fullscreen) {
		exit_fullscreen();
	} else {
		enter_fullscreen();
	}

	set_section_property_value("sdl",
	                           "fullscreen",
	                           sdl.desktop.is_fullscreen ? "on" : "off");

	focus_input();
	setup_presentation_mode();

	maybe_log_display_properties();
}

static void switch_fullscreen_handler(bool pressed)
{
	if (pressed) {
		switch_fullscreen();
	}
}

// Returns a writeable buffer for the VGA emulation to render the framebuffer
// image into. The buffer was sized for the current DOS video mode by a
// preceding `GFX_SetSize()` call.
//
// `pitch_out` is the number of bytes used to store a single row of pixel data
// (can be larger than actual width).
//
bool GFX_StartUpdate(uint8_t*& pixels, int& pitch)
{
	if (!sdl.active || sdl.updating_framebuffer) {
		return false;
	}

	sdl.renderer->StartFrame(pixels, pitch);

	sdl.updating_framebuffer = true;
	return true;
}

void GFX_EndUpdate()
{
	if (sdl.updating_framebuffer) {
		// `sdl.updating_framebuffer` is true when the contents of the
		// framebuffer has been changed in the current frame.
		//
		// We're making a copy of the framebuffer as we might present it
		// a bit later in 'host-rate' mode, otherwise the VGA emulation
		// could partially overwrite it by the time we present it (this
		// would introduce tearing even with vsync enabled!)
		//
		// Also, we're not updating the texture here yet because if
		// frames are skiped due to host vs DOS refresh mismatch, we
		// don't want to upload the texture for the skipped frames.
		//
		sdl.renderer->EndFrame();
	}

	if (GFX_GetPresentationMode() == PresentationMode::DosRate) {

		// In 'dos-rate' presentation mode, we present the frames as
		// soon as they're ready. This caters for the VRR monitor use
		// case where effectively our present rate controls the refresh
		// rate of the monitor.
		//
		// The `GFX_EndUpdate` is called at the end of each frame, so at
		// regular intervals close to the refresh rate of the emulated
		// DOS video mode. There is some jitter in the 1-5 ms range, but
		// the timing seems to work well enough in practice on VRR
		// monitors (we can certainly achieve 100% smooth scrolling on
		// better VRR displays).
		//
		// However, this jitter might cause flicker and less than
		// perfect smooth scrolling on some VRR monitor & driver
		// combinations. We should try to get as close as possible to
		// the DOS rate in the future, down to microsecond accuracy
		// (e.g., by tightening the timing accuracy of the PIC timers as
		// VGA updates are timed by abusing the emulated PIC timers,
		// then we might need to do some additional sleep & busy waiting
		// before present to hit the exact time).
		//
		// Updating the new texture to the GPU also takes some non-zero
		// time, so we'll probably need to introduce an extra fixed
		// latency to account for this delay, and possibly make
		// adjustments in the audio emulation layer to keep the video
		// and audio in perfect sync.
		//
		GFX_MaybePresentFrame();
	}

	// 'host-rate' present is handled in `normal_loop()` in `dosbox.cpp` in
	// a "cooperative-multitasking" fashion at the end of each emulated 1ms
	// tick.
	sdl.updating_framebuffer = false;

	FrameMark;
}

// TODO rename to GFX_GetRgb
uint32_t GFX_GetRGB(const uint8_t red, const uint8_t green, const uint8_t blue)
{
	return sdl.renderer->GetRgb(red, green, blue);
}

void GFX_Start()
{
	sdl.active = true;
}

static void gui_destroy()
{
	GFX_Stop();

	if (sdl.draw.callback) {
		(sdl.draw.callback)(GFX_CallbackStop);
	}

	// Let SDL cleanup up after itself (exit fullscreen, restore mouse
	// state, etc.) instead of us attempting to do it manually. That is
	// unnecessary and a moving target with each SDL library upgrade.
	//
	// Similarly, all GPU resources (textures, compiled shaders, etc.) will
	// be cleaned up by the driver automatically. That's the best way, we
	// don't need to attempt to do a manual cleanup.
}

void GFX_Destroy()
{
	gui_destroy();
	MAPPER_Destroy();
}

static SDL_Point refine_window_size(const SDL_Point size,
                                    const bool wants_aspect_ratio_correction)
{
	// TODO This only works for 320x200 games. We cannot make hardcoded
	// assumptions about aspect ratios in general, e.g. the pixel aspect
	// ratio is 1:1 for 640x480 games both with 'aspect = on' and 'aspect =
	// off'.
	constexpr SDL_Point RatiosForStretchedPixels = {4, 3};
	constexpr SDL_Point RatiosForSquarePixels    = {8, 5};

	const auto image_aspect = wants_aspect_ratio_correction
	                                ? RatiosForStretchedPixels
	                                : RatiosForSquarePixels;

	const auto window_aspect = static_cast<double>(size.x) / size.y;

	const auto game_aspect = static_cast<double>(image_aspect.x) /
	                         image_aspect.y;

	// Window is wider than the emulated image, so constrain horizonally
	if (window_aspect > game_aspect) {
		const int x = ceil_sdivide(size.y * image_aspect.x, image_aspect.y);
		return {x, size.y};
	} else {
		// Window is narrower than the emulated image, so constrain
		// vertically
		const int y = ceil_sdivide(size.x * image_aspect.y, image_aspect.x);
		return {size.x, y};
	}

	return FallbackWindowSize;
}

static void maybe_limit_requested_resolution(int& w, int& h,
                                             const char* size_description)
{
	const auto desktop = get_desktop_size();
	if (w <= desktop.w && h <= desktop.h) {
		return;
	}

	bool was_limited = false;

	// Add any driver / platform / operating system limits in succession:

	// SDL KMSDRM limitations
	if (is_using_kmsdrm_driver()) {
		w = desktop.w;
		h = desktop.h;

		was_limited = true;

		LOG_WARNING("DISPLAY: Limiting '%s' resolution to %dx%d to avoid kmsdrm issues",
		            size_description,
		            w,
		            h);
	}

	if (!was_limited) {
		// TODO shouldn't we log the display resolution in physical
		// pixels instead?
		LOG_INFO(
		        "DISPLAY: Accepted '%s' resolution %dx%d despite exceeding "
		        "the %dx%d display",
		        size_description,
		        w,
		        h,
		        desktop.w,
		        desktop.h);
	}
}

static SDL_Point parse_window_resolution_from_conf(const std::string& pref)
{
	int w = 0;
	int h = 0;

	const bool was_parsed = sscanf(pref.c_str(), "%dx%d", &w, &h) == 2;

	const bool is_valid = (w >= FallbackWindowSize.x &&
	                       h >= FallbackWindowSize.y);

	if (was_parsed && is_valid) {
		maybe_limit_requested_resolution(w, h, "window");
		return {w, h};
	}

	// TODO convert to notification
	LOG_WARNING(
	        "DISPLAY: Invalid 'window_size' setting: '%s', "
	        "using 'default'",
	        pref.c_str());

	return FallbackWindowSize;
}

static SDL_Point window_bounds_from_label(const std::string& pref,
                                          const SDL_Rect desktop)
{
	constexpr int SmallPercent  = 50;
	constexpr int MediumPercent = 74;
	constexpr int LargePercent  = 90;

	const int percent = [&] {
		if (pref.starts_with('s')) {
			return SmallPercent;

		} else if (pref.starts_with('m') || pref == "default" ||
		           pref.empty()) {
			return MediumPercent;

		} else if (pref.starts_with('l')) {
			return LargePercent;

		} else if (pref == "desktop") {
			return 100;

		} else {
			// TODO convert to notification
			LOG_WARNING(
			        "DISPLAY: Invalid 'window_size' setting: '%s', "
			        "using 'default'",
			        pref.c_str());
			return MediumPercent;
		}
	}();

	const int w = ceil_sdivide(desktop.w * percent, 100);
	const int h = ceil_sdivide(desktop.h * percent, 100);

	return {w, h};
}

static SDL_Point clamp_to_minimum_window_dimensions(SDL_Point size)
{
	const auto w = std::max(size.x, FallbackWindowSize.x);
	const auto h = std::max(size.y, FallbackWindowSize.y);
	return {w, h};
}

static std::optional<SDL_Point> parse_window_position_conf(const std::string& window_position_val)
{
	if (window_position_val == "auto") {
		return {};
	}

	int x, y;
	const auto was_parsed = (sscanf(window_position_val.c_str(), "%d,%d", &x, &y) ==
	                         2);
	if (!was_parsed) {
		// TODO convert to notification
		LOG_WARNING(
		        "DISPLAY: Invalid 'window_position' setting: '%s'. "
		        "Must be in X,Y format, using 'auto'.",
		        window_position_val.c_str());
		return {};
	}

	const auto desktop = get_desktop_size();

	const bool is_out_of_bounds = x < 0 || x > desktop.w || y < 0 ||
	                              y > desktop.h;
	if (is_out_of_bounds) {
		// TODO convert to notification
		LOG_WARNING(
		        "DISPLAY: Invalid 'window_position' setting: '%s'. "
		        "Requested position is outside the bounds of the %dx%d "
		        "desktop, using 'auto'.",
		        window_position_val.c_str(),
		        desktop.w,
		        desktop.h);
		return {};
	}

	return SDL_Point{x, y};
}

static void save_window_position(const std::optional<SDL_Point> pos)
{
	if (pos) {
		if (sdl.desktop.fullscreen.mode == FullscreenMode::ForcedBorderless) {
			sdl.desktop.fullscreen.prev_window.x_pos = pos->x;
			sdl.desktop.fullscreen.prev_window.y_pos = pos->y;
		} else {
			sdl.desktop.window.x_pos = pos->x;
			sdl.desktop.window.y_pos = pos->y;
		}
	} else {
		if (sdl.desktop.fullscreen.mode == FullscreenMode::ForcedBorderless) {
			sdl.desktop.fullscreen.prev_window.x_pos =
			        SDL_WINDOWPOS_UNDEFINED_DISPLAY(sdl.display_number);

			sdl.desktop.fullscreen.prev_window.y_pos =
			        SDL_WINDOWPOS_UNDEFINED_DISPLAY(sdl.display_number);
		} else {
			sdl.desktop.window.x_pos = SDL_WINDOWPOS_UNDEFINED_DISPLAY(
			        sdl.display_number);

			sdl.desktop.window.y_pos = SDL_WINDOWPOS_UNDEFINED_DISPLAY(
			        sdl.display_number);
		}
	}
}

// Writes to the window-size member should be done via this function
static void save_window_size(const int w, const int h)
{
	assert(w > 0 && h > 0);

	// The desktop.window size stores the user-configured window size.
	// During runtime, the actual SDL window size might differ from this
	// depending on the aspect ratio, window DPI, or manual resizing.
	sdl.desktop.window.width  = w;
	sdl.desktop.window.height = h;

	// Initialize the window's canvas size if it hasn't yet been set.
	auto& window_canvas_size = sdl.desktop.window.canvas_size;

	if (window_canvas_size.w <= 0 || window_canvas_size.h <= 0) {
		window_canvas_size.w = w;
		window_canvas_size.h = h;
	}
}

// Takes in:
//  - The user's window_size setting: default, WxH, small, medium, large,
//    desktop, or an invalid setting.
//  - If aspect correction is requested.
//
// This function returns a refined size and additionally populates the
// following struct members:
//
//  - 'sdl.desktop.window', with the refined size.
//
static void configure_window_size()
{
	const auto window_size_pref = []() {
		const auto legacy_pref = get_sdl_section()->GetString(
		        "windowresolution");
		if (!legacy_pref.empty()) {
			set_section_property_value("sdl", "windowresolution", "");
			set_section_property_value("sdl", "window_size", legacy_pref);
		}
		return get_sdl_section()->GetString("window_size");
	}();

	// Get the coarse resolution from the users setting, and adjust
	// refined scaling mode if an exact resolution is desired.
	SDL_Point coarse_size = FallbackWindowSize;

	sdl.use_exact_window_resolution = window_size_pref.find('x') !=
	                                  std::string::npos;

	if (sdl.use_exact_window_resolution) {
		coarse_size = parse_window_resolution_from_conf(window_size_pref);
	} else {
		const auto desktop = get_desktop_size();

		coarse_size = window_bounds_from_label(window_size_pref, desktop);
	}

	// Refine the coarse resolution and save it in the SDL struct.
	auto refined_size = coarse_size;

	if (sdl.use_exact_window_resolution) {
		refined_size = clamp_to_minimum_window_dimensions(coarse_size);
	} else {
		refined_size = refine_window_size(coarse_size,
		                                  is_aspect_ratio_correction_enabled());
	}

	assert(refined_size.x <= UINT16_MAX && refined_size.y <= UINT16_MAX);

	save_window_size(refined_size.x, refined_size.y);

	// Let the user know the resulting window properties
	LOG_MSG("DISPLAY: Using %dx%d window size in windowed mode on display-%d",
	        refined_size.x,
	        refined_size.y,
	        sdl.display_number);
}

static void configure_window()
{
	configure_window_decorations();

	save_window_position(parse_window_position_conf(
	        get_sdl_section()->GetString("window_position")));

	configure_window_size();
}

InterpolationMode GFX_GetTextureInterpolationMode()
{
	return sdl.interpolation_mode;
}

static int get_sdl_window_flags()
{
	auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

	if (!get_sdl_section()->GetBool("window_decorations")) {
		flags |= SDL_WINDOW_BORDERLESS;
	}

	if (sdl.desktop.is_fullscreen) {
		switch (sdl.desktop.fullscreen.mode) {
		case FullscreenMode::Standard:
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
			break;
		case FullscreenMode::Original:
			flags |= SDL_WINDOW_FULLSCREEN;
			break;
		case FullscreenMode::ForcedBorderless:
			// no-op
			break;
		default: assertm(false, "Invalid FullscreenMode");
		}
	}

	return check_cast<int>(flags);
}

static void setup_fullscreen_mode()
{
	// Set fullscreen display mode
	if (sdl.desktop.fullscreen.mode != FullscreenMode::ForcedBorderless) {
		SDL_DisplayMode fullscreen_mode = {};

		const SDL_DisplayMode requested_mode = {0,
		                                        sdl.desktop.fullscreen.width,
		                                        sdl.desktop.fullscreen.height,
		                                        0,
		                                        nullptr};

		if (!SDL_GetClosestDisplayMode(sdl.display_number,
		                               &requested_mode,
		                               &fullscreen_mode)) {

			LOG_WARNING(
			        "SDL: Failed to set fullscreen mode to %dx%d, "
			        "falling back to desktop mode",
			        requested_mode.w,
			        requested_mode.h);

			if (SDL_GetDesktopDisplayMode(sdl.display_number,
			                              &fullscreen_mode) < 0) {
				LOG_WARNING("SDL: Failed to retrieve desktop display mode: %s",
				            SDL_GetError());
			}
		}

		if (SDL_SetWindowDisplayMode(sdl.window, &fullscreen_mode) < 0) {
			LOG_ERR("SDL: Failed to set fullscreen display mode: %s",
			        SDL_GetError());
		}
	}
}

static void create_window_and_renderer()
{
#if C_OPENGL
	if (sdl.want_rendering_backend == RenderingBackend::OpenGl) {
		sdl.rendering_backend = RenderingBackend::OpenGl;

		try {
			sdl.renderer = std::make_unique<OpenGlRenderer>(
			        sdl.desktop.window.x_pos,
			        sdl.desktop.window.y_pos,
			        sdl.desktop.window.width,
			        sdl.desktop.window.height,
			        get_sdl_window_flags());

		} catch (const std::runtime_error& ex) {
			LOG_WARNING(
			        "OPENGL: Could not create OpenGL context, "
			        "falling back to SDL renderer");

			sdl.want_rendering_backend = RenderingBackend::Texture;
		}
	}
#endif

	if (sdl.want_rendering_backend == RenderingBackend::Texture) {
		sdl.rendering_backend = RenderingBackend::Texture;

		try {
			std::string render_driver = get_sdl_section()->GetString(
			        "texture_renderer");
			lowcase(render_driver);

			sdl.renderer = std::make_unique<SdlRenderer>(
			        sdl.desktop.window.x_pos,
			        sdl.desktop.window.y_pos,
			        sdl.desktop.window.width,
			        sdl.desktop.window.height,
			        get_sdl_window_flags(),
			        render_driver,
			        sdl.interpolation_mode);

		} catch (const std::runtime_error& ex) {
			E_Exit("SDL: Could not initialize SDL render backend");
		}
	}

	sdl.window = sdl.renderer->GetWindow();
}

static void configure_keyboard_capture()
{
	const auto capture_keyboard = get_sdl_section()->GetBool("keyboard_capture");

	SDL_SetWindowKeyboardGrab(sdl.window, capture_keyboard ? SDL_TRUE : SDL_FALSE);
}

static void apply_active_settings()
{
	MOUSE_NotifyWindowActive(true);

	if (sdl.mute_when_inactive && !MIXER_IsManuallyMuted()) {
		MIXER_Unmute();
	}

	// At least on some platforms grabbing the keyboard has to be repeated
	// each time we regain focus
	if (sdl.window) {
		configure_keyboard_capture();
	}
}

static void apply_inactive_settings()
{
	MOUSE_NotifyWindowActive(false);

	if (sdl.mute_when_inactive) {
		MIXER_Mute();
	}
}

static void restart_hotkey_handler([[maybe_unused]] bool pressed)
{
	DOSBOX_Restart();
}

static void configure_fullscreen_mode()
{
	const auto section = get_sdl_section();

	sdl.desktop.is_fullscreen = control->arguments.fullscreen ||
	                            section->GetBool("fullscreen");

	const auto fullscreen_mode_pref = [&] {
		auto legacy_pref = section->GetString("fullresolution");
		if (!legacy_pref.empty()) {
			set_section_property_value("sdl", "fullresolution", "");
			set_section_property_value("sdl", "fullscreen_mode", legacy_pref);
		}
		return section->GetString("fullscreen_mode");
	}();

	auto set_screen_bounds = [] {
		SDL_Rect bounds;
		SDL_GetDisplayBounds(sdl.display_number, &bounds);

		sdl.desktop.fullscreen.width  = bounds.w;
		sdl.desktop.fullscreen.height = bounds.h;
	};

	if (fullscreen_mode_pref == "standard") {
		set_screen_bounds();
		sdl.desktop.fullscreen.mode = FullscreenMode::Standard;

	} else if (fullscreen_mode_pref == "forced-borderless") {
		set_screen_bounds();
		sdl.desktop.fullscreen.mode = FullscreenMode::ForcedBorderless;

	} else if (fullscreen_mode_pref == "original") {
		set_screen_bounds();
		sdl.desktop.fullscreen.mode = FullscreenMode::Original;
	}
}

static void configure_display()
{
	const int display = get_sdl_section()->GetInt("display");

	if ((display >= 0) && (display < SDL_GetNumVideoDisplays())) {
		sdl.display_number = display;
	} else {
		// TODO convert to notification
		LOG_WARNING("SDL: Display number out of bounds, using display 0");
		sdl.display_number = 0;
	}
}

static void configure_allow_screensaver()
{
	const std::string screensaver = get_sdl_section()->GetString("screensaver");
	if (screensaver == "allow") {
		SDL_EnableScreenSaver();
	} else {
		SDL_DisableScreenSaver();
	}
}

static void configure_pause_and_mute_when_inactive()
{
	sdl.pause_when_inactive = get_sdl_section()->GetBool("pause_when_inactive");

	sdl.mute_when_inactive = (!sdl.pause_when_inactive) &&
	                         get_sdl_section()->GetBool("mute_when_inactive");
}

static void set_sdl_hints()
{
#if (SDL_VERSION_ATLEAST(3, 0, 0))
	SDL_SetHint(SDL_HINT_APP_ID, DOSBOX_APP_ID);
#else
#if !defined(WIN32) && !defined(MACOSX)
	constexpr int Overwrite = 0;

	setenv("SDL_VIDEO_X11_WMCLASS", DOSBOX_APP_ID, Overwrite);
	setenv("SDL_VIDEO_WAYLAND_WMCLASS", DOSBOX_APP_ID, Overwrite);
#endif
#endif

#if defined(WIN32)
	if (SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2") == SDL_FALSE) {
		LOG_WARNING("SDL: Failed to set DPI awareness flag");
	}
	if (SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1") == SDL_FALSE) {
		LOG_WARNING("SDL: Failed to set DPI scaling flag");
	}
#endif

	// Seamless mouse integration feels more 'seamless' if mouse
	// clicks on unfocused windows are passed to the guest.
	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

	// We have a keyboard shortcut to exit the fullscreen mode,
	// so we don't necessary need the ALT+TAB shortcut
	SDL_SetHint(SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED, "0");

#if defined SDL_HINT_APP_NAME
	// For KDE 6 volume applet and PipeWire audio driver; further
	// SetHint calls have no effect in the GUI, only the first
	// advertised name is used.
	SDL_SetHint(SDL_HINT_APP_NAME, DOSBOX_NAME);
#endif

#if defined SDL_HINT_AUDIO_DEVICE_STREAM_NAME
	// Useful for 'pw-top' and possibly other PipeWire CLI tools.
	SDL_SetHint(SDL_HINT_AUDIO_DEVICE_STREAM_NAME, DOSBOX_NAME);
#endif
}

static void add_default_sdl_section_mapper_bindings()
{
	MAPPER_AddHandler(MAPPER_Run, SDL_SCANCODE_F1, PRIMARY_MOD, "mapper", "Mapper");

	MAPPER_AddHandler(GFX_RequestExit, SDL_SCANCODE_F9, PRIMARY_MOD, "shutdown", "Shutdown");

	MAPPER_AddHandler(switch_fullscreen_handler,
	                  SDL_SCANCODE_RETURN,
	                  MMOD2,
	                  "fullscr",
	                  "Fullscreen");
	MAPPER_AddHandler(restart_hotkey_handler,
	                  SDL_SCANCODE_HOME,
	                  PRIMARY_MOD | MMOD2,
	                  "restart",
	                  "Restart");

	MAPPER_AddHandler(MOUSE_ToggleUserCapture,
	                  SDL_SCANCODE_F10,
	                  PRIMARY_MOD,
	                  "capmouse",
	                  "Cap Mouse");

#if C_DEBUGGER
	// Pause binds with activate-debugger

#elif defined(MACOSX)
	// Pause/unpause is hardcoded to Command+P on macOS
	MAPPER_AddHandler(&pause_emulation, SDL_SCANCODE_P, PRIMARY_MOD, "pause", "Pause Emu.");
#else
	// Pause/unpause is hardcoded to Alt+Pause on Window & Linux
	MAPPER_AddHandler(&pause_emulation, SDL_SCANCODE_PAUSE, MMOD2, "pause", "Pause Emu.");
#endif
}

void GFX_Init()
{
	set_sdl_hints();

	if (is_using_kmsdrm_driver() && !check_kmsdrm_setting()) {
		E_Exit("SDL: /dev/input/event0 is not readable, quitting early to prevent TTY input lockup.\n"
		       "Please run: 'sudo usermod -aG input $(whoami)', then re-login and try again.");
	}

	// Initialise SDL (timer is needed for title bar animations)
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		E_Exit("SDL: Can't init SDL %s", SDL_GetError());
	}

	// Register custom SDL events
	sdl.start_event_id = SDL_RegisterEvents(enum_val(SDL_DosBoxEvents::NumEvents));
	if (sdl.start_event_id == UINT32_MAX) {
		E_Exit("SDL: Failed to allocate event IDs");
	}

	// Log runtime SDL version
	SDL_version sdl_version = {};
	SDL_GetVersion(&sdl_version);

	LOG_MSG("SDL: Version %d.%d.%d initialised (%s video and %s audio)",
	        sdl_version.major,
	        sdl_version.minor,
	        sdl_version.patch,
	        SDL_GetCurrentVideoDriver(),
	        SDL_GetCurrentAudioDriver());

	// Start GUI init
	auto section = get_sdl_section();

	configure_pause_and_mute_when_inactive();

	// Assume focus on startup
	apply_active_settings();

	configure_fullscreen_mode();
	configure_display();

	validate_vsync_and_presentation_mode_settings();
	configure_vsync();
	configure_presentation_mode();
	configure_renderer();
	configure_window();

	sdl.draw.render_width_px  = FallbackWindowSize.x;
	sdl.draw.render_height_px = FallbackWindowSize.y;

	create_window_and_renderer();

#if defined(MACOSX)
	// Setting the SDL_WINDOW_BORDERLESS flag on window creation doesn't
	// work on macOS.
	//
	// TODO Remove workaround when the SDL issue
	// https://github.com/libsdl-org/SDL/issues/6172 is resolved.
	//
	configure_window_decorations();
#endif

	setup_fullscreen_mode();

	SDL_SetWindowMinimumSize(sdl.window,
	                         FallbackWindowSize.x,
	                         FallbackWindowSize.y);

	sdl.renderer->SetVsync(is_vsync_enabled());

	configure_window_transparency();

	check_and_handle_dpi_change(sdl.window);
	configure_allow_screensaver();

	add_default_sdl_section_mapper_bindings();

	// Get keyboard state of NumLock and CapsLock
	SDL_Keymod keystate = SDL_GetModState();

	// TODO is this still needed on current SDL?
	//
	// A long-standing SDL1 and SDL2 bug prevents it from detecting the
	// NumLock and CapsLock states on startup. Instead, these states must
	// be toggled by the user /after/ starting DOSBox.
	startup_state_numlock  = keystate & KMOD_NUM;
	startup_state_capslock = keystate & KMOD_CAPS;

	// Notify MOUSE subsystem that it can start now
	MOUSE_NotifyReadyGFX();

	TITLEBAR_ReadConfig(*section);

	if (sdl.desktop.is_fullscreen &&
	    sdl.desktop.fullscreen.mode == FullscreenMode::ForcedBorderless) {
		enter_fullscreen();
	}
}

static void regenerate_window()
{
	GFX_Stop();

	sdl.renderer = {};
	sdl.window   = nullptr;

	create_window_and_renderer();

	GFX_ResetScreen();
}

static void notify_sdl_setting_updated(SectionProp& section,
                                       const std::string& prop_name)
{
	if (prop_name == "fullscreen") {
		auto fullscreen_requested = section.GetBool("fullscreen");

		if (sdl.desktop.is_fullscreen && !fullscreen_requested) {
			exit_fullscreen();
		} else if (!sdl.desktop.is_fullscreen && fullscreen_requested) {
			enter_fullscreen();
		}

	} else if (prop_name == "fullscreen_mode") {
		const auto was_in_fullscreen = sdl.desktop.is_fullscreen;
		if (sdl.desktop.is_fullscreen) {
			exit_fullscreen();
		}

		configure_fullscreen_mode();

		if (was_in_fullscreen) {
			enter_fullscreen();
		}

	} else if (prop_name == "keyboard_capture") {
		configure_keyboard_capture();

	} else if (prop_name == "mapperfile") {
		MAPPER_BindKeys(&section);

	} else if (prop_name == "mute_when_inactive") {
		configure_pause_and_mute_when_inactive();

	} else if (prop_name == "pause_when_inactive") {
		configure_pause_and_mute_when_inactive();

	} else if (prop_name == "presentation_mode") {
		validate_vsync_and_presentation_mode_settings();
		configure_vsync();
		configure_presentation_mode();
		regenerate_window();

	} else if (prop_name == "screensaver") {
		configure_allow_screensaver();

	} else if (prop_name == "vsync") {
		validate_vsync_and_presentation_mode_settings();
		configure_vsync();

		sdl.renderer->SetVsync(is_vsync_enabled());
		log_presentation_and_vsync_mode();

	} else if (prop_name == "window_decorations") {
		configure_window_decorations();

#if C_OPENGL && defined(MACOSX)
		update_viewport();
#endif

	} else if (prop_name == "window_position") {
		save_window_position(parse_window_position_conf(
		        section.GetString("window_position")));

		if (!sdl.desktop.is_fullscreen) {
			SDL_SetWindowPosition(sdl.window,
			                      sdl.desktop.window.x_pos,
			                      sdl.desktop.window.y_pos);
		}

	} else if (prop_name == "window_size") {
		configure_window_size();

		if (sdl.desktop.fullscreen.mode == FullscreenMode::ForcedBorderless &&
		    sdl.desktop.is_fullscreen) {

			sdl.desktop.fullscreen.prev_window.width =
			        sdl.desktop.window.width;

			sdl.desktop.fullscreen.prev_window.height =
			        sdl.desktop.window.height;
		} else {
			SDL_SetWindowSize(sdl.window,
			                  sdl.desktop.window.width,
			                  sdl.desktop.window.height);
		}

	} else if (prop_name == "window_titlebar") {
		TITLEBAR_ReadConfig(section);

	} else if (prop_name == "window_transparency") {
		if (!sdl.desktop.is_fullscreen) {
			configure_window_transparency();
		}

	} else {
		assertm(false, "Unhandled [sdl] section setting");
	}
}

static void handle_mouse_motion(SDL_MouseMotionEvent* motion)
{
	MOUSE_EventMoved(static_cast<float>(motion->xrel),
	                 static_cast<float>(motion->yrel),
	                 static_cast<float>(motion->x),
	                 static_cast<float>(motion->y));
}

static void handle_mouse_wheel(SDL_MouseWheelEvent* wheel)
{
	const auto tmp = (wheel->direction == SDL_MOUSEWHEEL_NORMAL) ? -wheel->y
	                                                             : wheel->y;
	MOUSE_EventWheel(check_cast<int16_t>(tmp));
}

static void handle_mouse_button(SDL_MouseButtonEvent* button)
{
	auto notify_button = [](const uint8_t button, const bool pressed) {
		// clang-format off
		switch (button) {
		case SDL_BUTTON_LEFT:   MOUSE_EventButton(MouseButtonId::Left,   pressed); break;
		case SDL_BUTTON_RIGHT:  MOUSE_EventButton(MouseButtonId::Right,  pressed); break;
		case SDL_BUTTON_MIDDLE: MOUSE_EventButton(MouseButtonId::Middle, pressed); break;
		case SDL_BUTTON_X1:     MOUSE_EventButton(MouseButtonId::Extra1, pressed); break;
		case SDL_BUTTON_X2:     MOUSE_EventButton(MouseButtonId::Extra2, pressed); break;
		}
		// clang-format on
	};
	assert(button);
	notify_button(button->button, button->state == SDL_PRESSED);
}

void GFX_LosingFocus()
{
	sdl.laltstate = SDL_KEYUP;
	sdl.raltstate = SDL_KEYUP;
	MAPPER_LosingFocus();
}

bool GFX_IsFullscreen()
{
	return sdl.desktop.is_fullscreen;
}

static bool maybe_autoswitch_shader()
{
	// The shaders need a canvas size as their target resolution
	const auto canvas_size_px = sdl.renderer->GetCanvasSizeInPixels();
	if (canvas_size_px.IsEmpty()) {
		return false;
	}

	// The shaders need the DOS mode to be set as their source resolution
	if (!sdl.maybe_video_mode) {
		return false;
	}

	constexpr auto ReinitRender = true;
	return RENDER_MaybeAutoSwitchShader(canvas_size_px,
	                                    *sdl.maybe_video_mode,
	                                    ReinitRender);
}

static bool is_user_event(const SDL_Event& event)
{
	const auto start_id = sdl.start_event_id;
	const auto end_id   = start_id + enum_val(SDL_DosBoxEvents::NumEvents);

	return (event.common.type >= start_id && event.common.type < end_id);
}

static void handle_user_event(const SDL_Event& event)
{
	const auto id = event.common.type - sdl.start_event_id;

	switch (static_cast<SDL_DosBoxEvents>(id)) {
	case SDL_DosBoxEvents::RefreshAnimatedTitle:
		TITLEBAR_RefreshAnimatedTitle();
		break;

	default: assert(false);
	}
}

static void handle_pause_when_inactive(const SDL_Event& event)
{
	// Non-focus priority is set to pause; check to see if we've lost window
	// or input focus i.e. has the window been minimised or made inactive?
	//
	if ((event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) ||
	    (event.window.event == SDL_WINDOWEVENT_MINIMIZED)) {
		// Window has lost focus, pause the emulator. This is similar to
		// what PauseDOSBox() does, but the exit criteria is different.
		// Instead of waiting for the user to hit Alt+Break, we wait for
		// the window to regain window or input focus.
		//
		apply_inactive_settings();

		KEYBOARD_ClrBuffer();

		sdl.is_paused = true;
		TITLEBAR_RefreshTitle();

		// Prevent the mixer from running while in our pause loop.
		// Muting is not ideal for some sound devices such as GUS that
		// loop samples. This also saves CPU time by not rendering
		// samples we're not going to play anyway.
		MIXER_LockMixerThread();

		SDL_Event ev;

		while (sdl.is_paused && !DOSBOX_IsShutdownRequested()) {
			// WaitEvent() waits for an event rather than
			// polling, so CPU usage drops to zero.
			SDL_WaitEvent(&ev);

			switch (ev.type) {
			case SDL_QUIT: GFX_RequestExit(true); break;
			case SDL_WINDOWEVENT: {
				const auto we = ev.window.event;

				// wait until we get window focus back
				if ((we == SDL_WINDOWEVENT_FOCUS_LOST) ||
				    (we == SDL_WINDOWEVENT_MINIMIZED) ||
				    (we == SDL_WINDOWEVENT_FOCUS_GAINED) ||
				    (we == SDL_WINDOWEVENT_RESTORED) ||
				    (we == SDL_WINDOWEVENT_EXPOSED)) {

					// We've got focus back, so unpause and
					// break out of the loop
					if ((we == SDL_WINDOWEVENT_FOCUS_GAINED) ||
					    (we == SDL_WINDOWEVENT_RESTORED) ||
					    (we == SDL_WINDOWEVENT_EXPOSED)) {

						sdl.is_paused = false;
						TITLEBAR_RefreshTitle();

						if (we == SDL_WINDOWEVENT_FOCUS_GAINED) {
							sdl.is_paused = false;
							apply_active_settings();
						}
					}

					// Now poke a "release ALT" command into
					// the keyboard buffer we have to do
					// this, otherwise ALT will 'stick' and
					// cause problems with the app running
					// in the DOSBox.
					//
					KEYBOARD_AddKey(KBD_leftalt, false);
					KEYBOARD_AddKey(KBD_rightalt, false);

					if (we == SDL_WINDOWEVENT_RESTORED) {
						// We may need to re-create a
						// texture and more
						GFX_ResetScreen();
					}
				}
			} break;
			}
		}
		MIXER_UnlockMixerThread();
	}
}

template <typename... Args>
void log_window_event([[maybe_unused]] const char* message,
                      [[maybe_unused]] const Args&... args) noexcept
{
#ifdef DEBUG_WINDOW_EVENTS
	LOG_DEBUG(message, args...);
#endif
}

static bool handle_sdl_windowevent(const SDL_Event& event)
{
	switch (event.window.event) {
	case SDL_WINDOWEVENT_RESTORED:
		log_window_event("SDL: Window has been restored");

		// We may need to re-create a texture and more on Android.
		// Another case: Update surface while using X11.
		//
		GFX_ResetScreen();

#if C_OPENGL && defined(MACOSX)
		// TODO check if this workaround is still needed

		log_window_event("SDL: Reset macOS's GL viewport after window-restore");

		if (sdl.rendering_backend == RenderingBackend::OpenGl) {
			update_viewport();
		}
#endif
		focus_input();
		return true;

	case SDL_WINDOWEVENT_RESIZED: {
		// Window dimensions in logical coordinates
		const auto width  = event.window.data1;
		const auto height = event.window.data2;

		log_window_event("SDL: Window has been resized to %dx%d", width, height);

		static int last_width  = 0;
		static int last_height = 0;

		// SDL_WINDOWEVENT_RESIZED events are sent twice when resizing
		// the window, but maybe_log_display_properties() will only
		// output a log entry if the image dimensions have actually
		// changed.
		maybe_log_display_properties();

		if (!sdl.desktop.is_fullscreen) {
			save_window_size(width, height);

			set_section_property_value("sdl",
			                           "window_size",
			                           format_str("%dx%d", width, height));
		}

		if (width != last_width && height != last_height) {
			maybe_log_display_properties();

			// Needed for aspect & viewport mode combinations where
			// the pixel aspect ratio or viewport size is sized
			// relatively to the window size.
			VGA_SetupDrawing(0);

			last_width  = width;
			last_height = height;
		}
		return true;
	}

	case SDL_WINDOWEVENT_FOCUS_GAINED:
		log_window_event("SDL: Window has gained keyboard focus");

		apply_active_settings();
		[[fallthrough]];

	case SDL_WINDOWEVENT_EXPOSED:
		log_window_event("SDL: Window has been exposed and should be redrawn");

		// TODO: below is not consistently true :( seems incorrect on
		// KDE and sometimes on MATE
		//
		// Note that on Windows/Linux-X11/Wayland/macOS, event is fired
		// after toggling between full vs windowed modes. However this
		// is never fired on the Raspberry Pi (when rendering to the
		// Framebuffer); therefore we rely on the FOCUS_GAINED event to
		// catch window startup and size toggles.

		if (sdl.draw.callback) {
			sdl.draw.callback(GFX_CallbackRedraw);
		}
		focus_input();
		return true;

	case SDL_WINDOWEVENT_FOCUS_LOST:
		log_window_event("SDL: Window has lost keyboard focus");

#ifdef WIN32
		// TODO is this still needed?
		if (sdl.desktop.is_fullscreen) {
			VGA_KillDrawing();

			// Force-exit fullscreen
			sdl.desktop.is_fullscreen = false;
			set_section_property_value("sdl", "fullscreen", "off");
			GFX_ResetScreen();
		}
#endif
		apply_inactive_settings();
		GFX_LosingFocus();
		return false;

	case SDL_WINDOWEVENT_ENTER:
		log_window_event("SDL: Window has gained mouse focus");
		return true;

	case SDL_WINDOWEVENT_LEAVE:
		log_window_event("SDL: Window has lost mouse focus");
		return true;

	case SDL_WINDOWEVENT_SHOWN:
		log_window_event("SDL: Window has been shown");
		maybe_autoswitch_shader();
		return true;

	case SDL_WINDOWEVENT_HIDDEN:
		log_window_event("SDL: Window has been hidden");
		return true;

	case SDL_WINDOWEVENT_MOVED: {
		const auto x = event.window.data1;
		const auto y = event.window.data2;

		log_window_event("SDL: Window has been moved to %d, %d", x, y);

#if C_OPENGL && defined(MACOSX)
		// TODO This workaround is still needed on macOS 15.6. We'll be
		// able to remove it once we always set the viewport to covert
		// the full window (supporting overlay images and the OSD will
		// necessitate this).
		//
		if (sdl.rendering_backend == RenderingBackend::OpenGl) {
			update_viewport();
		}
#endif
		// We don't allow negative values for 'window_position', so this
		// is the best we can do to keep things in sync.
		const auto new_x = std::max(x, 0);
		const auto new_y = std::max(y, 0);

		if (!sdl.desktop.is_fullscreen) {
			save_window_position(SDL_Point{new_x, new_y});

			set_section_property_value("sdl",
			                           "window_position",
			                           format_str("%d,%d", new_x, new_y));
		}
		return true;
	}

	case SDL_WINDOWEVENT_DISPLAY_CHANGED: {
		const auto new_display_number = event.window.data1;
		log_window_event("SDL: Window has been moved to display %d",
		                 new_display_number);

		// New display might have a different resolution and DPI scaling
		// set, so recalculate that and set viewport
		check_and_handle_dpi_change(sdl.window);

		SDL_Rect display_bounds = {};
		SDL_GetDisplayBounds(new_display_number, &display_bounds);
		sdl.desktop.fullscreen.width  = display_bounds.w;
		sdl.desktop.fullscreen.height = display_bounds.h;

		sdl.display_number = new_display_number;

		update_viewport();
		maybe_autoswitch_shader();
		notify_new_mouse_screen_params();
		return true;
	}

	case SDL_WINDOWEVENT_SIZE_CHANGED: {
		log_window_event("SDL: The window size has changed");

		// The window size has changed either as a result of an API call
		// or through the system or user changing the window size.
		const auto new_width  = event.window.data1;
		const auto new_height = event.window.data2;

		check_and_handle_dpi_change(sdl.window, new_width);

		maybe_handle_screen_rotation(new_width, new_height);
		update_viewport();
		maybe_autoswitch_shader();

		notify_new_mouse_screen_params();
		return true;
	}

	case SDL_WINDOWEVENT_MINIMIZED:
		log_window_event("SDL: Window has been minimized");

		apply_inactive_settings();
		return false;

	case SDL_WINDOWEVENT_MAXIMIZED:
		log_window_event("SDL: Window has been maximized");
		return true;

	case SDL_WINDOWEVENT_CLOSE:
		log_window_event(
		        "SDL: The window manager requests that the window be closed");

		GFX_RequestExit(true);
		return false;

	case SDL_WINDOWEVENT_TAKE_FOCUS:
		log_window_event("SDL: Window is being offered a focus");

		focus_input();
		apply_active_settings();
		return true;

	case SDL_WINDOWEVENT_HIT_TEST:
		log_window_event(
		        "SDL: Window had a hit test that wasn't SDL_HITTEST_NORMAL");
		return true;

	default: return false;
	}
}

static void adjust_ticks_after_present_frame(int64_t elapsed_us)
{
	static int64_t cumulative_time_rendered_us = 0;
	cumulative_time_rendered_us += elapsed_us;

	constexpr auto MicrosInMillisecond = 1000;

	if (cumulative_time_rendered_us >= MicrosInMillisecond) {
		// 1 tick == 1 millisecond
		const auto cumulative_ticks_rendered = cumulative_time_rendered_us /
		                                       MicrosInMillisecond;

		DOSBOX_SetTicksDone(DOSBOX_GetTicksDone() - cumulative_ticks_rendered);

		// Keep the fractional microseconds part
		cumulative_time_rendered_us %= MicrosInMillisecond;
	}
}

void GFX_CaptureRenderedImage()
{
	// The draw rect can extends beyond the bounds of the window or the
	// screen in fullscreen when we're "zooming into" the DOS content in
	// `relative` viewport mode. But rendered captures should always capture
	// what we see on the screen, so only the visible part of the enlarged
	// image. Therefore, we need to clip the draw rect to the bounds of the
	// canvas (the total visible area of the window or screen), and only
	// capture the resulting output rectangle.

	auto canvas_rect_px = sdl.renderer->GetCanvasSizeInPixels();
	canvas_rect_px.x    = 0.0f;
	canvas_rect_px.y    = 0.0f;

	const auto output_rect_px = canvas_rect_px.Copy().Intersect(
	        to_rect(sdl.draw_rect_px));

	auto image = sdl.renderer->ReadPixelsPostShader(output_rect_px);

	assert(sdl.maybe_video_mode);
	image.params.video_mode = *sdl.maybe_video_mode;

	CAPTURE_AddPostRenderImage(image);
}

void GFX_MaybePresentFrame()
{
	const auto start_us = GetTicksUs();

	// Always present the frame if we want to capture the next
	// rendered frame, regardless of the presentation mode. This is
	// necessary to keep the contents of rendered and raw/upscaled
	// screenshots in sync (so they capture the exact same frame) in
	// multi-output image capture modes.
	const auto force_present = CAPTURE_IsCapturingPostRenderImage();

	const auto curr_frame_time_us =
	        GetTicksDiff(start_us, sdl.presentation.last_present_time_us);

	const auto present_window_start_us = sdl.presentation.frame_time_us -
	                                     sdl.presentation.early_present_window_us;

	if (force_present || (curr_frame_time_us >= present_window_start_us)) {

		if (sdl.active) {
			sdl.renderer->PrepareFrame();
			sdl.renderer->PresentFrame();
		}

		const auto end_us = GetTicksUs();
#if 0
		LOG_TRACE("DISPLAY: present took %2.4f ms", 0.001 * GetTicksDiff(end_us, start_us));

		const auto measured_frame_time_us = GetTicksDiff(
			end_us, sdl.presentation.last_present_time_us);

		LOG_TRACE("DISPLAY: frame time: %2.4f ms", 0.001 * measured_frame_time_us);

		if (measured_frame_time_us >
			sdl.presentation.frame_time_us * 1.5) {
			LOG_WARNING("DISPLAY: missed vsync (long frame)");
		}
#endif
		sdl.presentation.last_present_time_us = end_us;

		// Adjust "ticks done" counter by the time it took to present
		// the frame
		adjust_ticks_after_present_frame(GetTicksDiff(end_us, start_us));
	}
}

// Returns:
//   true  - event loop can keep running
//   false - event loop wants to quit
bool GFX_PollAndHandleEvents()
{
	SDL_Event event;

	static auto last_check_joystick = GetTicks();
	auto current_check_joystick     = GetTicks();

	if (GetTicksDiff(current_check_joystick, last_check_joystick) > 20) {
		last_check_joystick = current_check_joystick;

		if (MAPPER_IsUsingJoysticks()) {
			SDL_JoystickUpdate();
		}
		MAPPER_UpdateJoysticks();
	}

	while (SDL_PollEvent(&event)) {
#if C_DEBUGGER
		if (is_debugger_event(event)) {
			pdc_event_queue.push(event);
			continue;
		}
#endif
		if (is_user_event(event)) {
			handle_user_event(event);
			continue;
		}

		switch (event.type) {
		case SDL_DISPLAYEVENT:
			switch (event.display.event) {
			case SDL_DISPLAYEVENT_CONNECTED:
			case SDL_DISPLAYEVENT_DISCONNECTED:
				notify_new_mouse_screen_params();
				break;
			default: break;
			};
			break;

		case SDL_WINDOWEVENT: {
			auto handling_finished = handle_sdl_windowevent(event);
			if (handling_finished) {
				continue;
			}

			if (sdl.pause_when_inactive) {
				handle_pause_when_inactive(event);
			}
		} break;

		case SDL_MOUSEMOTION: handle_mouse_motion(&event.motion); break;
		case SDL_MOUSEWHEEL: handle_mouse_wheel(&event.wheel); break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			handle_mouse_button(&event.button);
			break;

		case SDL_QUIT: GFX_RequestExit(true); break;
#ifdef WIN32
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			// ignore event alt+tab
			if (event.key.keysym.sym == SDLK_LALT) {
				sdl.laltstate = (SDL_EventType)event.key.type;
			}
			if (event.key.keysym.sym == SDLK_RALT) {
				sdl.raltstate = (SDL_EventType)event.key.type;
			}
			if (((event.key.keysym.sym == SDLK_TAB)) &&
			    ((sdl.laltstate == SDL_KEYDOWN) ||
			     (sdl.raltstate == SDL_KEYDOWN))) {
				break;
			}
			// This can happen as well.
			if (((event.key.keysym.sym == SDLK_TAB)) &&
			    (event.key.keysym.mod & KMOD_ALT)) {
				break;
			}
			// Ignore tab events that arrive just after regaining
			// focus. Likely the result of Alt+Tab.
			if ((event.key.keysym.sym == SDLK_TAB) &&
			    (GetTicksSince(sdl.focus_ticks) < 2)) {
				break;
			}
			[[fallthrough]];
#endif
#if defined(MACOSX)
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			// On macOS, Command+Q is the default key to close an
			// application
			if (is_command_pressed(event) &&
			    event.key.keysym.sym == SDLK_q) {
				GFX_RequestExit(true);
				break;
			}
			[[fallthrough]];
#endif
		default: MAPPER_CheckEvent(&event);
		}
	}
	return !DOSBOX_IsShutdownRequested();
}

static std::vector<std::string> get_sdl_texture_renderers()
{
	const int n = SDL_GetNumRenderDrivers();

	std::vector<std::string> drivers;
	drivers.reserve(n + 1);
	drivers.push_back("auto");

	SDL_RendererInfo info;

	for (int i = 0; i < n; i++) {
		if (SDL_GetRenderDriverInfo(i, &info)) {
			continue;
		}
		if (info.flags & SDL_RENDERER_TARGETTEXTURE) {
			drivers.push_back(info.name);
		}
	}
	return drivers;
}

static void register_sdl_text_messages()
{
	MSG_Add("DISPLAY_INVALID_VSYNC_SETTING",
	        "Invalid [color=light-green]'vsync'[reset] setting: [color=white]'%s'[reset];\n"
	        "vsync cannot be enabled in [color=white]'dos-rate'[reset] presentation mode, "
	        "using [color=white]'off'[reset]");
}

static void init_sdl_config_settings(SectionProp& section)
{
	using enum Property::Changeable::Value;

#if C_OPENGL
	const std::string default_output = "opengl";
#else
	const std::string default_output = "texture";
#endif
	auto pstring = section.AddString("output", OnlyAtStart, default_output.c_str());

	pstring->SetOptionHelp(
	        "opengl_default",
	        "Rendering backend to use for graphics output ('opengl' by default).\n"
	        "Only the 'opengl' backend has shader support and is thus the preferred option.\n"
	        "The 'texture' backend is only provided as a last resort fallback for buggy or\n"
	        "non-existent OpenGL 2.1+ drivers (this is extremely rare).");

	pstring->SetOptionHelp("texture_default",
	                       "Rendering backend to use for graphics output ('texture' by default).");

	pstring->SetOptionHelp("opengl",
	                       "  opengl:     OpenGL backend with shader support (default).");
	pstring->SetOptionHelp("texture",
	                       "  texture:    SDL's texture backend with bilinear interpolation.");
	pstring->SetOptionHelp("texturenb",
	                       "  texturenb:  SDL's texture backend with nearest-neighbour interpolation\n"
	                       "              (no bilinear).");
#if C_OPENGL
	pstring->SetDeprecatedWithAlternateValue("surface", "opengl");
	pstring->SetDeprecatedWithAlternateValue("openglpp", "opengl");
	pstring->SetDeprecatedWithAlternateValue("openglnb", "opengl");
#else
	pstring->SetDeprecatedWithAlternateValue("surface", "texture");
#endif
	pstring->SetDeprecatedWithAlternateValue("texturepp", "texture");

	pstring->SetValues({
#if C_OPENGL
	        "opengl",
#endif
	        "texture",
	        "texturenb",
	});
	pstring->SetEnabledOptions({
#if C_OPENGL
	        "opengl_default",
	        "opengl",
#else
	        "texture_default",
#endif
	        "texture",
	        "texturenb",
	});

	pstring = section.AddString("texture_renderer", OnlyAtStart, "auto");
	pstring->SetHelp(
	        "Render driver to use in 'texture' output mode ('auto' by default).\n"
	        "Use 'texture_renderer = auto' for an automatic choice.");
	pstring->SetValues(get_sdl_texture_renderers());

	auto pint = section.AddInt("display", OnlyAtStart, 0);
	pint->SetHelp(
	        "Number of display to use; values depend on OS and user "
	        "settings (0 by default).");

	auto pbool = section.AddBool("fullscreen", Always, false);
	pbool->SetHelp("Start in fullscreen mode ('off' by default).");

	pstring = section.AddString("fullresolution", DeprecatedButAllowed, "");
	pstring->SetHelp(
	        "The [color=light-green]'fullresolution'[reset] setting is deprecated but still accepted;\n"
	        "please use [color=light-green]'fullscreen_mode'[reset] instead.");

	pstring = section.AddString("fullscreen_mode", Always, "standard");
	pstring->SetHelp("Set the fullscreen mode ('standard' by default):");

	pstring->SetOptionHelp("standard",
	                       "  standard:           Use the standard fullscreen mode of your operating system\n"
	                       "                      (default).");

	pstring->SetOptionHelp(
	        "forced-borderless",
	        "  forced-borderless:  Force borderless fullscreen operation if your graphics\n"
	        "                      card driver decides to disable fullscreen optimisation\n"
	        "                      on Windows, resulting in exclusive fullscreen. Forcing\n"
	        "                      borderless mode might result in decreased performance\n"
	        "                      and slightly worse frame pacing (e.g., scrolling in 2D\n"
	        "                      games not appearing perfectly smooth).");

	pstring->SetOptionHelp(
	        "original",
	        "  original:           Exclusive fullscreen mode at the game's original\n"
	        "                      resolution, or at the closest available resolution. This\n"
	        "                      is a niche option for using DOSBox Staging with a CRT\n"
	        "                      monitor. Toggling fullscreen might result in janky\n"
	        "                      behaviour in this mode.");
	pstring->SetValues({"standard",
#ifdef WIN32
	                    "forced-borderless",
#endif
	                    "original"});

	pstring->SetDeprecatedWithAlternateValue("desktop", "standard");

	pstring = section.AddString("windowresolution", DeprecatedButAllowed, "");
	pstring->SetHelp(
	        "The [color=light-green]'windowresolution'[reset] setting is deprecated but still accepted;\n"
	        "please use [color=light-green]'window_size'[reset] instead.");

	pstring = section.AddString("window_size", Always, "default");
	pstring->SetHelp(
	        "Set initial window size for windowed mode. You can still resize the window\n"
	        "after startup.\n"
	        "  default:   Select the best option based on your environment and other\n"
	        "             settings (such as whether aspect ratio correction is enabled).\n"
	        "  small, medium, large (s, m, l):\n"
	        "             Size the window relative to the desktop.\n"
	        "  WxH:       Specify window size in WxH format in logical units\n"
	        "             (e.g., 1024x768).");

	pstring = section.AddString("window_position", Always, "auto");
	pstring->SetHelp(
	        "Set initial window position for windowed mode:\n"
	        "  auto:      Let the window manager decide the position (default).\n"
	        "  X,Y:       Set window position in X,Y format in logical units (e.g., 250,100).\n"
	        "             0,0 is the top-left corner of the screen.");

	pbool = section.AddBool("window_decorations", Always, true);
	pbool->SetHelp("Enable window decorations in windowed mode ('on' by default).");

	TITLEBAR_AddConfigSettings(section);

	pint = section.AddInt("transparency", Deprecated, 0);
	pint->SetHelp("Renamed to [color=light-green]'window_transparency'[reset].");

	pint = section.AddInt("window_transparency", Always, 0);
	pint->SetHelp(
	        "Set the transparency of the DOSBox Staging window (0 by default).\n"
	        "From 0 (no transparency) to 90 (high transparency).");
	pint->SetMinMax(0, 90);

	pstring = section.AddString("max_resolution", Deprecated, "");
	pstring->SetHelp(
	        "Moved to [color=light-cyan][render][reset] section "
	        "and renamed to [color=light-green]'viewport'[reset].");

	pstring = section.AddString("viewport_resolution", Deprecated, "");
	pstring->SetHelp(
	        "Moved to [color=light-cyan][render][reset] section "
	        "and renamed to [color=light-green]'viewport'[reset].");

	pstring = section.AddString("vsync", Always, "off");
	pstring->SetHelp(
	        "Set the host video driver's vertical synchronization (vsync) mode:\n"
	        "  off:              Disable vsync in both windowed and fullscreen mode\n"
	        "                    (default). This is the best option on variable refresh rate\n"
	        "                    (VRR) monitors running in VRR mode to get perfect frame\n"
	        "                    pacing, no tearing, and low input lag. On fixed refresh rate\n"
	        "                    monitors (or VRR monitors in fixed refresh mode), disabling\n"
	        "                    vsync might cause visible tearing in fast-paced games.\n"
	        "  on:               Enable vsync in both windowed and fullscreen mode. This can\n"
	        "                    prevent tearing in fast-paced games but will increase input\n"
	        "                    lag. Vsync is only available with 'host-rate' presentation\n"
	        "                    (see 'presentation_mode').\n"
	        "  fullscreen-only:  Enable vsync in fullscreen mode only. This might be useful\n"
	        "                    if your operating system enforces vsync in windowed mode and\n"
	        "                    the 'on' setting causes audio glitches or other issues in\n"
	        "                    windowed mode only. Vsync is only available with 'host-rate'\n"
	        "                    presentation (see 'presentation_mode').\n"
	        "\n"
	        "Notes:\n"
	        "  - For perfectly smooth scrolling in 2D games (e.g., in Pinball Dreams\n"
	        "    and Epic Pinball), you'll need a VRR monitor running in VRR mode and 'vsync'\n"
	        "    disabled. The scrolling in 70 Hz VGA games will always appear juddery on\n"
	        "    60 Hz fixed refresh rate monitors even with vsync enabled.\n"
	        "  - Usually, you'll only get perfectly smooth 2D scrolling in fullscreen mode,\n"
	        "    even on a VRR monitor.\n"
	        "  - For the best results, disable all frame cappers and global vsync overrides\n"
	        "    in your video driver settings.");
	pstring->SetValues({"off", "on", "fullscreen-only"});

	pstring = section.AddString("presentation_mode", Always, "auto");
	pstring->SetHelp(
	        "Select the frame presentation mode ('auto' by default):\n"
	        "  auto:       Use 'host-rate' if 'vsync' is enabled, otherwise use 'dos-rate'\n"
	        "              (default). See 'vsync' for further details.\n"
	        "  dos-rate:   Present frames at the refresh rate of the emulated DOS video mode.\n"
	        "              This is the best option on variable refresh rate (VRR) monitors.\n"
	        "              'vsync' is not availabe with 'dos-rate' presentation.\n"
	        "  host-rate:  Present frames at the refresh rate of the host display. Use this\n"
	        "              with 'vsync' enabled on fixed refresh rate monitors for fast-paced\n"
	        "              games where tearing is a problem. 'host-rate' combined with\n"
	        "              'vsync' disabled can be a good workaround on systems that always\n"
	        "              enforce blocking vsync at the OS level (e.g., forced 60 Hz vsync\n"
	        "              could cause problems with VGA games presenting frames at 70 Hz).");
	pstring->SetValues({"auto", "dos-rate", "host-rate"});

	auto pmulti = section.AddMultiVal("capture_mouse", Deprecated, ",");
	pmulti->SetHelp(
	        "Moved to [color=light-cyan][mouse][reset] section and "
	        "renamed to [color=light-green]'mouse_capture'[reset].");

	pmulti = section.AddMultiVal("sensitivity", Deprecated, ",");
	pmulti->SetHelp(
	        "Moved to [color=light-cyan][mouse][reset] section and "
	        "renamed to [color=light-green]'mouse_sensitivity'[reset].");

	pbool = section.AddBool("raw_mouse_input", Deprecated, false);
	pbool->SetHelp(
	        "Moved to [color=light-cyan][mouse][reset] section and "
	        "renamed to [color=light-green]'mouse_raw_input'[reset].");

	pbool = section.AddBool("waitonerror", Deprecated, true);
	pbool->SetHelp("The [color=light-green]'waitonerror'[reset] setting has been removed.");

	pstring = section.AddString("priority", Deprecated, "");
	pstring->SetHelp(
	        "The [color=light-green]'priority'[reset] setting has been removed.");

	pbool = section.AddBool("mute_when_inactive", Always, false);
	pbool->SetHelp("Mute the sound when the window is inactive ('off' by default).");

	pbool = section.AddBool("pause_when_inactive", Always, false);
	pbool->SetHelp("Pause emulation when the window is inactive ('off' by default).");

	pbool = section.AddBool("keyboard_capture", Always, false);
	pbool->SetHelp(
	        "Capture system keyboard shortcuts ('off' by default).\n"
	        "When enabled, most system shortcuts such as Alt+Tab are captured and sent to\n"
	        "DOSBox Staging. This is useful for Windows 3.1x and some DOS programs with\n"
	        "unchangeable keyboard shortcuts that conflict with system shortcuts.");

	pstring = section.AddPath("mapperfile", Always, MAPPERFILE);
	pstring->SetHelp(
	        "Path to the mapper file ('mapper-sdl2-XYZ.map' by default, where XYZ is the\n"
	        "current version). Pre-configured maps are bundled in 'resources/mapperfiles'.\n"
	        "These can be loaded by name, e.g., with 'mapperfile = xbox/xenon2.map'.\n"
	        "Note: The '--resetmapper' command line option only deletes the default mapper\n"
	        "      file.");

	pstring = section.AddString("screensaver", Always, "auto");
	pstring->SetHelp(
	        "Use 'allow' or 'block' to override the SDL_VIDEO_ALLOW_SCREENSAVER environment\n"
	        "variable which usually blocks the OS screensaver while the emulator is\n"
	        "running ('auto' by default).");
	pstring->SetValues({"auto", "allow", "block"});
}

void GFX_AddConfigSection()
{
	auto section = control->AddSection("sdl");
	section->AddUpdateHandler(notify_sdl_setting_updated);

	init_sdl_config_settings(*section);

	TITLEBAR_AddMessages();
	register_sdl_text_messages();
}

void GFX_Quit()
{
#if !C_DEBUGGER
	SDL_Quit();
#endif

#if defined(WIN32)
	// Might not be needed if the shutdown function switches to windowed
	// mode, but it doesn't hurt.
	// TODO is this still needed on current SDL?
	sticky_keys(true);
#endif
}

