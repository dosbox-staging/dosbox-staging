/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "dosbox.h"

#include <array>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <tuple>
#include <unistd.h>

#if C_DEBUG
#include <queue>
#endif

#ifdef WIN32
#include <process.h>
#include <signal.h>
#include <windows.h>
#endif

#include <SDL.h>
#if C_OPENGL
#include <SDL_opengl.h>
#endif

#include "../capture/capture.h"
#include "../ints/int10.h"
#include "control.h"
#include "cpu.h"
#include "cross.h"
#include "debug.h"
#include "fs_utils.h"
#include "gui_msgs.h"
#include "joystick.h"
#include "keyboard.h"
#include "mapper.h"
#include "math_utils.h"
#include "mixer.h"
#include "mouse.h"
#include "pacer.h"
#include "pic.h"
#include "render.h"
#include "sdlmain.h"
#include "setup.h"
#include "string_utils.h"
#include "timer.h"
#include "tracy.h"
#include "vga.h"
#include "video.h"

#if C_OPENGL
//Define to report opengl errors
//#define DB_OPENGL_ERROR

#ifndef APIENTRY
#define APIENTRY
#endif

#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

/* Don't guard these with GL_VERSION_2_0 - Apple defines it but not these typedefs.
 * If they're already defined they should match these definitions, so no conflicts.
 */
typedef void (APIENTRYP PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (APIENTRYP PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef GLuint (APIENTRYP PFNGLCREATEPROGRAMPROC) (void);
typedef GLuint (APIENTRYP PFNGLCREATESHADERPROC) (GLenum type);
typedef void (APIENTRYP PFNGLDELETEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP PFNGLDELETESHADERPROC) (GLuint shader);
typedef void (APIENTRYP PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef GLint (APIENTRYP PFNGLGETATTRIBLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRYP PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLint (APIENTRYP PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRYP PFNGLLINKPROGRAMPROC) (GLuint program);
//Change to NP, as Khronos changes include guard :(
typedef void (APIENTRYP PFNGLSHADERSOURCEPROC_NP) (GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
typedef void (APIENTRYP PFNGLUNIFORM2FPROC) (GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRYP PFNGLUNIFORM1IPROC) (GLint location, GLint v0);
typedef void (APIENTRYP PFNGLUSEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP PFNGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);

/* Apple defines these functions in their GL header (as core functions)
 * so we can't use their names as function pointers. We can't link
 * directly as some platforms may not have them. So they get their own
 * namespace here to keep the official names but avoid collisions.
 */
namespace gl2 {
PFNGLATTACHSHADERPROC glAttachShader = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
PFNGLCREATESHADERPROC glCreateShader = nullptr;
PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
PFNGLDELETESHADERPROC glDeleteShader = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = nullptr;
PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
PFNGLSHADERSOURCEPROC_NP glShaderSource = nullptr;
PFNGLUNIFORM2FPROC glUniform2f = nullptr;
PFNGLUNIFORM1IPROC glUniform1i = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;
}

/* "using" is meant to hide identical names declared in outer scope
 * but is unreliable, so just redefine instead.
 */
#define glAttachShader            gl2::glAttachShader
#define glCompileShader           gl2::glCompileShader
#define glCreateProgram           gl2::glCreateProgram
#define glCreateShader            gl2::glCreateShader
#define glDeleteProgram           gl2::glDeleteProgram
#define glDeleteShader            gl2::glDeleteShader
#define glEnableVertexAttribArray gl2::glEnableVertexAttribArray
#define glGetAttribLocation       gl2::glGetAttribLocation
#define glGetProgramiv            gl2::glGetProgramiv
#define glGetProgramInfoLog       gl2::glGetProgramInfoLog
#define glGetShaderiv             gl2::glGetShaderiv
#define glGetShaderInfoLog        gl2::glGetShaderInfoLog
#define glGetUniformLocation      gl2::glGetUniformLocation
#define glLinkProgram             gl2::glLinkProgram
#define glShaderSource            gl2::glShaderSource
#define glUniform2f               gl2::glUniform2f
#define glUniform1i               gl2::glUniform1i
#define glUseProgram              gl2::glUseProgram
#define glVertexAttribPointer     gl2::glVertexAttribPointer

#endif // C_OPENGL

#ifdef WIN32
#include <winuser.h>
#define STDOUT_FILE "stdout.txt"
#define STDERR_FILE "stderr.txt"
#endif

#if defined(HAVE_SETPRIORITY)
#include <sys/resource.h>

#define PRIO_TOTAL (PRIO_MAX - PRIO_MIN)
#endif

SDL_Block sdl;

// Masks to be passed when creating SDL_Surface.
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
[[maybe_unused]] constexpr uint32_t RMASK = 0xff000000;
[[maybe_unused]] constexpr uint32_t GMASK = 0x00ff0000;
[[maybe_unused]] constexpr uint32_t BMASK = 0x0000ff00;
[[maybe_unused]] constexpr uint32_t AMASK = 0x000000ff;
#else
[[maybe_unused]] constexpr uint32_t RMASK = 0x000000ff;
[[maybe_unused]] constexpr uint32_t GMASK = 0x0000ff00;
[[maybe_unused]] constexpr uint32_t BMASK = 0x00ff0000;
[[maybe_unused]] constexpr uint32_t AMASK = 0xff000000;
#endif

// Size and ratio constants
// ------------------------
static SDL_Point FALLBACK_WINDOW_DIMENSIONS = {640, 480};

static bool first_window = true;

static SDL_Point restrict_to_viewport_resolution(int width, int height);
static SDL_Rect calc_viewport(const int width, const int height);

static void CleanupSDLResources();
static void HandleVideoResize(int width, int height);

static void update_frame_texture([[maybe_unused]] const uint16_t* changedLines);
static bool present_frame_texture();
#if C_OPENGL
static void update_frame_gl(const uint16_t *changedLines);
static bool present_frame_gl();
#endif

static const char* vsync_state_as_string(const VsyncState state)
{
	switch (state) {
	case VsyncState::Unset: return "unset";
	case VsyncState::Adaptive: return "adaptive";
	case VsyncState::Off: return "off";
	case VsyncState::On: return "on";
	default: return "unknown";
	}
}

#if C_DEBUG
extern SDL_Window *pdc_window;
extern std::queue<SDL_Event> pdc_event_queue;

static bool is_debugger_event(const SDL_Event &event)
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
	}
	return false;
}

SDL_Window *GFX_GetSDLWindow(void)
{
	return sdl.window;
}
#endif

#if C_OPENGL

// SDL allows pixels sizes (colour-depth) from 1 to 4 bytes
constexpr uint8_t MAX_BYTES_PER_PIXEL = 4;


#ifdef DB_OPENGL_ERROR
void OPENGL_ERROR(const char* message)
{
	GLenum r = glGetError();
	if (r == GL_NO_ERROR) {
		return;
	}
	LOG_ERR("OPENGL: Errors from %s", message);
	do {
		LOG_ERR("OPENGL: %X", r);
	} while ((r = glGetError()) != GL_NO_ERROR);
}
#else
void OPENGL_ERROR(const char*) {
	return;
}
#endif
#endif

extern "C" void SDL_CDROMQuit(void);
static void QuitSDL()
{
	if (sdl.initialized) {
		SDL_CDROMQuit();
#if !C_DEBUG
		SDL_Quit();
#endif
	}
}

extern const char* RunningProgram;
extern bool CPU_CycleAutoAdjust;
//Globals for keyboard initialisation
bool startup_state_numlock=false;
bool startup_state_capslock=false;

void GFX_SetTitle(const int32_t new_num_cycles, const bool is_paused = false)
{
	char title_buf[200] = {0};

#if !defined(NDEBUG)
	#define APP_NAME_STR DOSBOX_NAME " (debug build)"
#else
	#define APP_NAME_STR DOSBOX_NAME
#endif

	auto &num_cycles      = sdl.title_bar.num_cycles;
	auto &cycles_ms_str   = sdl.title_bar.cycles_ms_str;
	auto &hint_mouse_str  = sdl.title_bar.hint_mouse_str;
	auto &hint_paused_str = sdl.title_bar.hint_paused_str;

	if (new_num_cycles != -1)
		num_cycles = new_num_cycles;

	if (cycles_ms_str.empty()) {
		cycles_ms_str   = MSG_GetRaw("TITLEBAR_CYCLES_MS");
		hint_paused_str = std::string(" ") + MSG_GetRaw("TITLEBAR_HINT_PAUSED");
	}

	if (CPU_CycleAutoAdjust)
		safe_sprintf(title_buf, "%8s - max %d%% - " APP_NAME_STR "%s",
		             RunningProgram, num_cycles,
		             is_paused ? hint_paused_str.c_str() : hint_mouse_str.c_str());
	else
		safe_sprintf(title_buf, "%8s - %d %s - " APP_NAME_STR "%s",
		             RunningProgram, num_cycles, cycles_ms_str.c_str(),
		             is_paused ? hint_paused_str.c_str() : hint_mouse_str.c_str());

	SDL_SetWindowTitle(sdl.window, title_buf);
}

void GFX_RefreshTitle()
{
	constexpr int8_t refresh_cycle_count = -1;
	GFX_SetTitle(refresh_cycle_count);
}

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
		constexpr const char* vars[] = {"XDG_CURRENT_DESKTOP",
		                                "XDG_SESSION_DESKTOP",
		                                "DESKTOP_SESSION",
		                                "GDMSESSION"};

		have_desktop_environment = std::any_of(std::begin(vars),
		                                       std::end(vars),
		                                       std::getenv);
		already_checked = true;
	}

	return have_desktop_environment;
#else
	// Assume we have a desktop environment on all other systems
	return true;
#endif
}

static double get_host_refresh_rate()
{
	auto get_sdl_rate = []() {
		SDL_DisplayMode mode = {};
		auto &sdl_rate = mode.refresh_rate;
		assert(sdl.window);
		const auto display_in_use = SDL_GetWindowDisplayIndex(sdl.window);
		if (display_in_use < 0) {
			LOG_ERR("SDL: Could not get the current window index: %s",
			        SDL_GetError());
			return RefreshRateHostDefault;
		}
		if (SDL_GetCurrentDisplayMode(display_in_use, &mode) != 0) {
			LOG_ERR("SDL: Could not get the current display mode: %s",
			        SDL_GetError());
			return RefreshRateHostDefault;
		}
		if (sdl_rate < RefreshRateMin) {
			LOG_WARNING("SDL: Got a strange refresh rate of %d Hz",
			            sdl_rate);
			return RefreshRateHostDefault;
		}
		assert(sdl_rate >= RefreshRateMin);
		return sdl_rate;
	};

	auto get_vrr_rate = [](const int sdl_rate) {
		constexpr auto vrr_backoff_hz = 3;
		return sdl_rate - vrr_backoff_hz;
	};

	auto get_sdi_rate = [](const int sdl_rate) {
		const auto is_odd = sdl_rate % 2 != 0;
		const auto not_div_by_5 = sdl_rate % 5 != 0;
		const auto next_is_div_by_3 = (sdl_rate + 1) % 3 == 0;
		const bool should_adjust = is_odd && not_div_by_5 && next_is_div_by_3;
		constexpr auto sdi_factor = 1.0 - 1.0 / 1000.0;
		return should_adjust ? (sdl_rate + 1) * sdi_factor : sdl_rate;
	};

	// To be populated in the switch
	auto rate = 0.0;                   // refresh rate as a floating point number
	const char *rate_description = ""; // description of the refresh rate

	switch (sdl.desktop.host_rate_mode) {
	case HostRateMode::Auto:
		if (const auto sdl_rate = get_sdl_rate();
		    sdl.desktop.fullscreen && sdl_rate >= InterpolatingVrrMinRateHz) {
			rate = get_vrr_rate(sdl_rate);
			rate_description = "VRR-adjusted (auto)";
		} else {
			rate = get_sdi_rate(sdl_rate);
			rate_description = "standard SDI (auto)";
		}
		break;
	case HostRateMode::Sdi:
		rate = get_sdi_rate(get_sdl_rate());
		rate_description = "standard SDI";
		break;
	case HostRateMode::Vrr:
		rate = get_vrr_rate(get_sdl_rate());
		rate_description = "VRR-adjusted";
		break;
	case HostRateMode::Custom:
		assert(sdl.desktop.preferred_host_rate >= RefreshRateMin);
		rate = sdl.desktop.preferred_host_rate;
		rate_description = "custom";
		break;
	}
	assert(rate >= RefreshRateMin);

	// Log if changed
	static auto last_int_rate = 0;
	const auto int_rate = ifloor(rate);
	if (last_int_rate != int_rate) {
		last_int_rate = int_rate;
		LOG_MSG("SDL: Using %s display refresh rate of %2.5g Hz",
		        rate_description, rate);
	}
	return rate;
}

// Reset and populate the vsync settings from the user's conf setting. This is
// called on-demand after startup and on output-mode changes (ie: switching from
// an SDL-based drawing context to an OpenGL-based context).
//
static void initialize_vsync_settings()
{
	sdl.vsync = {};

	const auto section = dynamic_cast<Section_prop*>(control->GetSection("sdl"));
	const std::string user_pref = (section ? section->Get_string("vsync") : "auto");

	if (has_true(user_pref)) {
		sdl.vsync.when_windowed.requested   = VsyncState::On;
		sdl.vsync.when_fullscreen.requested = VsyncState::On;
	} else if (user_pref == "adaptive") {
		sdl.vsync.when_windowed.requested   = VsyncState::Adaptive;
		sdl.vsync.when_fullscreen.requested = VsyncState::Adaptive;
	} else if (has_false(user_pref)) {
		sdl.vsync.when_windowed.requested   = VsyncState::Off;
		sdl.vsync.when_fullscreen.requested = VsyncState::Off;
	} else if (user_pref == "yield") {
		sdl.vsync.when_windowed.requested   = VsyncState::Yield;
		sdl.vsync.when_fullscreen.requested = VsyncState::Yield;
	} else {
		assert(user_pref == "auto");

		// In 'vsync = auto' windowed-mode, we try to disable vsync for
		// our Window because enabling vsync both at the program and
		// compositor levels (which might be enforced) may add latency
		// (i.e,: extra time blocked inside rendering or presentation
		// calls), which stalls DOSBox.
		//
		sdl.vsync.when_windowed.requested = VsyncState::Off;

		// In fullscreen-mode, the above still applies however we add
		// handling for VRR displays that perform frame interpolation.
		// as tehy need vsync enabled to lock onto the content:
		const bool prefers_vsync_when_fullscreen =
		        (get_host_refresh_rate() >= InterpolatingVrrMinRateHz);

		sdl.vsync.when_fullscreen.requested = prefers_vsync_when_fullscreen
		                                            ? VsyncState::On
		                                            : VsyncState::Off;

		// In 'vsync = auto', we also /assume/ vsync is enabled
		// (regardless how the above requests actually played out) by
		// overriding the measured state, as follows:
		//
		sdl.vsync.when_windowed.measured   = VsyncState::On;
		sdl.vsync.when_fullscreen.measured = VsyncState::On;
		//
		// A 60 Hz display can only show 60 complete frames per second.
		// To achieve a higher frame rate, the host must drop or tear
		// frames. This creates two layers of tearing when combined with
		// DOS's (potentially) torn frames, which is common in games
		// that don't use vblank. In "auto" mode, we aim for the best
		// user experience, and dual-tearing isn't it. However, users
		// can always set 'vsync = off'.
		//
		// There is no downside to making this assumption when the host
		// display is faster than the DOS rate because all frames will
		// be presented.
	}
}

/* On macOS, as we use a nicer external icon packaged in App bundle.
 *
 * Visual Studio bundles .ico file specified in winres.rc into the
 * dosbox.exe file.
 *
 * Other OSes will either use svg icon bundled in the native package, or
 * the window uploaded via SDL_SetWindowIcon below.
 */
#if defined(MACOSX) || defined(_MSC_VER)
static void SetIcon() {}
#else

#include "icon.c"

static void SetIcon()
{
	constexpr int src_w = icon_data.width;
	constexpr int src_h = icon_data.height;
	constexpr int src_bpp = icon_data.bytes_per_pixel;
	static_assert(src_bpp == 4, "Source image expected in RGBA format.");
	std::array<uint8_t, (src_w * src_h * src_bpp)> icon;
	ICON_DATA_RUN_LENGTH_DECODE(icon.data(), icon_data.rle_pixel_data,
	                            src_w * src_h, src_bpp);
	SDL_Surface *s = SDL_CreateRGBSurfaceFrom(icon.data(), src_w, src_h,
	                                          src_bpp * 8, src_w * src_bpp,
	                                          RMASK, GMASK, BMASK, AMASK);
	assert(s);
	SDL_SetWindowIcon(sdl.window, s);
	SDL_FreeSurface(s);
}

#endif

void GFX_RequestExit(const bool pressed)
{
	shutdown_requested = pressed;
	if (shutdown_requested) {
		LOG_DEBUG("SDL: Exit requested");
	}
}

[[maybe_unused]] static void PauseDOSBox(bool pressed)
{
	if (!pressed)
		return;
	const auto inkeymod = static_cast<uint16_t>(SDL_GetModState());

	GFX_RefreshTitle();
	bool paused = true;
	Delay(500);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		// flush event queue.
	}
	/* NOTE: This is one of the few places where we use SDL key codes
	with SDL 2.0, rather than scan codes. Is that the correct behavior? */
	while (paused && !shutdown_requested) {
		SDL_WaitEvent(&event);    // since we're not polling, cpu usage drops to 0.
		switch (event.type) {
		case SDL_QUIT: GFX_RequestExit(true); break;

		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESTORED) {
				// We may need to re-create a texture and more
				GFX_ResetScreen();
			}
			break;
		case SDL_KEYDOWN: // Must use Pause/Break Key to resume.
			if (event.key.keysym.sym == SDLK_PAUSE) {
				const uint16_t outkeymod = event.key.keysym.mod;
				if (inkeymod != outkeymod) {
					KEYBOARD_ClrBuffer();
					MAPPER_LosingFocus();
					//Not perfect if the pressed alt key is switched, but then we have to
					//insert the keys into the mapper or create/rewrite the event and push it.
					//Which is tricky due to possible use of scancodes.
				}
				paused = false;
				GFX_RefreshTitle();
				break;
			}
#if defined (MACOSX)
			if (event.key.keysym.sym == SDLK_q &&
			   (event.key.keysym.mod == KMOD_RGUI || event.key.keysym.mod == KMOD_LGUI)) {
				/* On macs, all aps exit when pressing cmd-q */
				GFX_RequestExit(true);
				break;
			}
#endif
		}
	}
}

uint8_t GFX_GetBestMode(const uint8_t flags)
{
	return (flags & GFX_CAN_32) & ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_16);
}

// Let the presentation layer safely call no-op functions.
// Useful during output initialization or transitions.
void GFX_DisengageRendering()
{
	sdl.frame.update  = update_frame_noop;
	sdl.frame.present = present_frame_noop;
}

/* Reset the screen with current values in the sdl structure */
void GFX_ResetScreen(void) {
	GFX_Stop();
	if (sdl.draw.callback)
		(sdl.draw.callback)( GFX_CallBackReset );
	GFX_Start();
	CPU_Reset_AutoAdjust();
}

void GFX_ForceFullscreenExit()
{
	sdl.desktop.fullscreen = false;
	GFX_ResetScreen();
}

[[maybe_unused]] static int int_log2(int val)
{
	int log = 0;
	while ((val >>= 1) != 0)
		log++;
	return log;
}

// This is a hack to prevent SDL2 from re-creating window internally. Prevents
// crashes on Windows and Linux, and prevents initial window from being visibly
// destroyed (for window managers that show animations while creating window,
// e.g. Gnome 3).
static uint32_t opengl_driver_crash_workaround(const RenderingBackend rendering_backend)
{
	if (rendering_backend != RenderingBackend::Texture) {
		return 0;
	}

	if (starts_with(sdl.render_driver, "opengl")) {
		return SDL_WINDOW_OPENGL;
	}

	if (sdl.render_driver != "auto")
		return 0;

	static int default_driver_is_opengl = -1;
	if (default_driver_is_opengl >= 0)
		return (default_driver_is_opengl ? SDL_WINDOW_OPENGL : 0);

	// According to SDL2 documentation, the first driver
	// in the list is the default one.
	int i = 0;
	SDL_RendererInfo info;
	while (SDL_GetRenderDriverInfo(i++, &info) == 0) {
		if (info.flags & SDL_RENDERER_TARGETTEXTURE)
			break;
	}
	default_driver_is_opengl = starts_with(info.name, "opengl");
	return (default_driver_is_opengl ? SDL_WINDOW_OPENGL : 0);
}

static SDL_Rect get_canvas_size(const RenderingBackend rendering_backend);

// Logs the source and target resolution including describing scaling method
// and pixel aspect ratio. Note that this function deliberately doesn't use
// any global structs to disentangle it from the existing sdl-main design.
static void log_display_properties(const int width, const int height,
                                   const VideoMode& video_mode,
                                   const std::optional<SDL_Rect>& viewport_size_override,
                                   const RenderingBackend rendering_backend)
{
	// Get the viewport dimensions, with consideration for possible override
	// values
	auto get_viewport_size = [&]() -> std::pair<int, int> {
		if (viewport_size_override) {
			const auto vp = calc_viewport(viewport_size_override->w,
			                              viewport_size_override->h);
			return {vp.w, vp.h};
		}
		const auto canvas   = get_canvas_size(rendering_backend);
		const auto viewport = calc_viewport(canvas.w, canvas.h);
		return {viewport.w, viewport.h};
	};

	const auto [viewport_w, viewport_h] = get_viewport_size();

	// Check expectations
	assert(width > 0 && height > 0);
	assert(viewport_w > 0 && viewport_h > 0);

	const auto scale_x = static_cast<double>(viewport_w) / width;
	const auto scale_y = static_cast<double>(viewport_h) / height;

	[[maybe_unused]] const auto one_per_render_pixel_aspect = scale_y / scale_x;

	const auto video_mode_desc = to_string(video_mode);

	const char* frame_mode = nullptr;
	switch (sdl.frame.mode) {
	case FrameMode::Cfr: frame_mode = "CFR"; break;
	case FrameMode::Vfr: frame_mode = "VFR"; break;
	case FrameMode::ThrottledVfr: frame_mode = "throttled VFR"; break;
	case FrameMode::Unset: frame_mode = "Unset frame mode"; break;
	default: assertm(false, "Invalid FrameMode");
	}

	const auto refresh_rate = VGA_GetPreferredRate();

	LOG_MSG("DISPLAY: %s at %2.5g Hz %s, "
	        "scaled to %dx%d with 1:%1.6g (%d:%d) pixel aspect ratio",
	        video_mode_desc.c_str(),
	        refresh_rate,
	        frame_mode,
	        viewport_w,
	        viewport_h,
	        video_mode.pixel_aspect_ratio.Inverse().ToDouble(),
	        static_cast<int32_t>(video_mode.pixel_aspect_ratio.Num()),
	        static_cast<int32_t>(video_mode.pixel_aspect_ratio.Denom()));

#if 0
	LOG_MSG("DISPLAY: render width: %d, render height: %d, "
	        "render pixel aspect ratio: 1:%1.3g",
	        width,
	        height,
	        one_per_render_pixel_aspect);
#endif
}

static SDL_Point get_initial_window_position_or_default(int default_val)
{
	int x, y;
	if (sdl.desktop.window.initial_x_pos >= 0 &&
	    sdl.desktop.window.initial_y_pos >= 0) {
		x = sdl.desktop.window.initial_x_pos;
		y = sdl.desktop.window.initial_y_pos;
	} else {
		x = y = default_val;
	}
	return {x, y};
}

// A safer way to call SDL_SetWindowSize because it ensures the event-callback
// is disabled during the resize event. This prevents the event callback from
// firing before the window is resized in which case an endless loop can occur
static void safe_set_window_size(const int w, const int h)
{
	decltype(sdl.draw.callback) saved_callback = nullptr;
	// Swap and save the callback with a a no-op
	std::swap(sdl.draw.callback, saved_callback);

	assert(sdl.window);
	SDL_SetWindowSize(sdl.window, w, h);

	// Swap the saved callback back in
	std::swap(sdl.draw.callback, saved_callback);
}

static VsyncSettings& get_vsync_settings()
{
	if (sdl.vsync.when_fullscreen.requested == VsyncState::Unset ||
	    sdl.vsync.when_windowed.requested == VsyncState::Unset) {
		initialize_vsync_settings();
	}
	return sdl.desktop.fullscreen ? sdl.vsync.when_fullscreen
	                              : sdl.vsync.when_windowed;
}

// Benchmarks are run in each vsync'd mode as part of the vsync detection
// process. This routine returns the vsync mode's current benchmark rate
// if available.
static std::optional<int> get_benchmarked_vsync_rate()
{
	const auto bench_rate = get_vsync_settings().benchmarked_rate;
	return (bench_rate != 0 ? std::optional<int>(bench_rate) : std::nullopt);
}

static void save_rate_to_frame_period(const double rate_hz)
{
	assert(rate_hz > 0);
	// backoff by one-onethousandth to avoid hitting the vsync edge
	sdl.frame.period_ms = 1'001.0 / rate_hz;
	const auto period_us = sdl.frame.period_ms * 1'000;
	sdl.frame.period_us = ifloor(period_us);
	// Permit the frame period to be off by up to 90% before "out of sync"
	sdl.frame.period_us_early = ifloor(55 * period_us / 100);
	sdl.frame.period_us_late = ifloor(145 * period_us / 100);
}

static std::unique_ptr<Pacer> render_pacer = {};

static int benchmark_presentation_rate()
{
	// If the presentation function is empty, then we can't benchmark
	assert(sdl.frame.present != present_frame_noop ||
	       sdl.frame.update != update_frame_noop);

	// Number of frames to benchmark
	const auto ten_percent_of_fps = get_host_refresh_rate() / 10;

	const auto warmup_frames = ten_percent_of_fps;
	const auto bench_frames  = ten_percent_of_fps;

	// Disable the pacer because we need every frame presented and measured
	// so we can hit the vsync wall (if it exists).
	render_pacer->SetTimeout(0);

	// Warmup round
	for (auto i = 0; i < warmup_frames; ++i) {
		sdl.frame.update(nullptr);
		sdl.frame.present();
	}
	// Measured round
	const auto start_us = GetTicksUs();
	for (auto frame = 0; frame < bench_frames; ++frame) {
		sdl.frame.update(nullptr);
		sdl.frame.present();
	}
	const auto elapsed_us = std::max(static_cast<int64_t>(1L), GetTicksUsSince(start_us));
	return iround(static_cast<int>((bench_frames * 1'000'000) / elapsed_us));
}

static int nearest_common_rate(const double rate)
{
	constexpr int common_rates[] = {
	        24, 30, 50, 60, 70, 71, 72, 75, 80, 85, 90, 100, 120, 144, 165, 240};

	int nearest_rate   = 0;
	int min_difference = INT_MAX;

	// Find the nearest refresh rate
	for (int common_rate : common_rates) {
		int difference = std::abs(iround(rate) - common_rate);
		if (difference <= min_difference) {
			min_difference = difference;
			nearest_rate   = common_rate;
			continue;
		}
		break;
	}
	assert(nearest_rate != 0);
	return nearest_rate;
}

static VsyncState measure_vsync_state(int& bench_rate)
{
	bench_rate = benchmark_presentation_rate();
	const auto host_rate = get_host_refresh_rate();

	// Notify the user if the machine is prensetation-starved.
	if (bench_rate < host_rate * 0.8) {
		LOG_WARNING("SDL: We can only render %d FPS, which is well below"
		            " the host's reported refresh rate of %2.5g Hz.",
		            bench_rate,
		            host_rate);
		LOG_WARNING(
		        "SDL: You will experience rendering lag and stuttering."
		        " Consider updating your video drivers and try disabling"
		        " vsync in your host and drivers, set [sdl] vsync = false");
	}

	if (bench_rate < host_rate * 1.5)
		return VsyncState::On;
	else if (bench_rate < host_rate * 2.5)
		return VsyncState::Adaptive;
	else
		return VsyncState::Off;
}

static void set_vsync(const VsyncState state)
{
	if (state == VsyncState::Yield) {
		return;
	}
#if C_OPENGL
	if (sdl.rendering_backend == RenderingBackend::OpenGl) {
		assert(sdl.opengl.context);
		const auto interval = static_cast<int>(state);
		// -1=adaptive, 0=off, 1=on
		assert(interval >= -1 && interval <= 1);
		if (SDL_GL_SetSwapInterval(interval) == 0)
			return;

		// the requested interval is not supported
		LOG_WARNING("OPENGL: Failed setting the vsync state to %s (%d): %s",
		            vsync_state_as_string(state), interval, SDL_GetError());

		// Per SDL's recommendation: If an application requests adaptive
		// vsync and the system does not support it, this function will
		// fail and return -1. In such a case, you should probably retry
		// the call with 1 for the interval.
		if (interval == -1 && SDL_GL_SetSwapInterval(1) != 0) {
			LOG_WARNING("OPENGL: Tried enabling non-adaptive vsync, "
			            "but it still failed: %s",
			            SDL_GetError());
		}
		return;
	}
#endif
	assert(sdl.rendering_backend == RenderingBackend::Texture);
	// https://wiki.libsdl.org/SDL_HINT_RENDER_VSYNC - can only be
	// set to "1", "0", adapative is currently not supported, so we
	// also treat it as "1"
	const auto hint_str = (state == VsyncState::On ||
							state == VsyncState::Adaptive)
									? "1"
									: "0";
	if (SDL_SetHint(SDL_HINT_RENDER_VSYNC, hint_str) == SDL_TRUE)
		return;
	LOG_WARNING("SDL: Failed setting vsync state to to %s (%s): %s",
				vsync_state_as_string(state), hint_str, SDL_GetError());
}

static void update_vsync_state()
{
	// Hosts can have different vsync constraints between window modes
	auto& vsync_pref = get_vsync_settings();

	// Short-hand aliases
	auto &requested = vsync_pref.requested;
	auto& measured  = vsync_pref.measured;

	assert(requested != VsyncState::Unset);

	// Do we still need to measure the vsync state?
	if (measured == VsyncState::Unset) {
		set_vsync(requested);
		measured = measure_vsync_state(vsync_pref.benchmarked_rate);
	}
}

static void remove_window()
{
	if (sdl.window) {
		SDL_DestroyWindow(sdl.window);
		sdl.window = nullptr;
	}
}

// The throttled presenter skips frames that have inter-frame spacing narrower
// than the allowed frame period (sdl.frame.period_us). When a frame is skipped,
// the presenter still tries to present it at its next oppourtunity.
//
static void maybe_present_throttled(const bool frame_is_new)
{
	static int64_t last_present_time = 0;
	static auto was_new_and_throttled = false;

	const auto now     = GetTicksUs();
	const auto elapsed = now - last_present_time;

	if (elapsed >= sdl.frame.period_us) {
		// If we waited beyond this frame's refresh period, then credit
		// this extra wait back by deducting it from the recorded time.
		const auto wait_overage = elapsed % sdl.frame.period_us;
		last_present_time = now - (9 * wait_overage / 10);

		if (frame_is_new || was_new_and_throttled) {
			sdl.frame.present();
		}
	}
	// Otherwise we've had to throttle the frame, however if the frame was
	// new, we'll record it as such and try to present it next time.
	else {
		was_new_and_throttled = frame_is_new;
	}
}

static void maybe_present_synced(const bool present_if_last_skipped)
{
	// state tracking across runs
	static bool last_frame_presented = false;
	static int64_t last_sync_time = 0;

	const auto now = GetTicksUs();

	const auto scheduler_arrival = GetTicksDiff(now, last_sync_time);

	const auto on_time = scheduler_arrival > sdl.frame.period_us_early &&
	                     scheduler_arrival < sdl.frame.period_us_late;

	const auto should_present = on_time || (present_if_last_skipped &&
	                                        !last_frame_presented);

	last_frame_presented = should_present ? sdl.frame.present() : false;

	last_sync_time = should_present ? GetTicksUs() : now;
}

static void setup_presentation_mode(FrameMode &previous_mode)
{
	// Always get the reported refresh rate and hint the VGA side with it.
	// This ensures the VGA side always has the host's rate to prior to its
	// next mode change.
	const auto host_rate = get_host_refresh_rate();
	VGA_SetHostRate(host_rate);
	const auto dos_rate = VGA_GetPreferredRate();

	// Calculate the maximum number of duplicate frames before presenting
	constexpr uint16_t min_rate = 10;
	sdl.frame.max_dupe_frames   = static_cast<float>(dos_rate) / min_rate;

	// Consider any vsync state that isn't explicitly off as having some
	// level of vsync enforcement as "on"
	const auto vsync_is_on = (get_vsync_settings().requested != VsyncState::Off);

	// to be set below
	auto mode = FrameMode::Unset;

	// Manual CFR or VFR modes
	if (sdl.frame.desired_mode == FrameMode::Cfr ||
	    sdl.frame.desired_mode == FrameMode::Vfr) {
		mode = sdl.frame.desired_mode;

		// Frames will be presented at the DOS rate.
		save_rate_to_frame_period(dos_rate);

		// Because we don't have proof that the host system actually
		// supports the requested rates, we use the frame pacer to
		// inform the user when the host is hitting the vsync wall.
		render_pacer->SetTimeout(vsync_is_on ? sdl.vsync.skip_us : 0);
	}
	// Automatic CFR or VFR modes
	else {
		const auto has_bench_rate = get_benchmarked_vsync_rate();

		auto get_supported_rate = [=]() -> double {
			if (!has_bench_rate) {
				return host_rate;
			}
			const double bench_rate = *has_bench_rate;

			return vsync_is_on ? std::min(bench_rate, host_rate)
			                   : std::max(bench_rate, host_rate);
		};
		const auto supported_rate = get_supported_rate();

		const auto display_might_be_interpolating = (host_rate >=
		                                             InterpolatingVrrMinRateHz);

		const auto conditions_demand_constant_host_rate = (
#if defined(MACOSX)
		        // Normally OpenGL drivers with a swap-interval of 1
		        // only block for the gap in time after the application
		        // requests a buffer swap through to presentation of the
		        // current frame.
		        //
		        // However, circa-2023 macOS OpenGL drivers impose
		        // additional per-frame block penalities if frames
		        // /aren't/ presented. Therefore, this condition demands
		        // we present at a constant host rate.
		        //
		        sdl.rendering_backend == RenderingBackend::OpenGl &&
		        get_vsync_settings().requested == VsyncState::On);
#else
		        false);
#endif

		// If we're fullscreen, vsynced, and using a VRR display that
		// performs frame interpolation, then we prefer to use a
		// constant rate.
		const auto conditions_prefer_constant_rate =
		        (sdl.desktop.fullscreen && vsync_is_on &&
		         display_might_be_interpolating);

#if 0
		LOG_MSG("SDL: Auto presentation mode conditions:");
		LOG_MSG("SDL:   - DOS rate is %2.5g Hz", dos_rate);
		if (has_bench_rate) {
		        LOG_MSG("SDL:   - Host renders at %d FPS", *has_bench_rate);
		}
		LOG_MSG("SDL:   - Display refresh rate is %.3f Hz", host_rate);
		LOG_MSG("SDL:   - %s",
		        supported_rate >= dos_rate
		                ? "Host can handle the full DOS rate"
		                : "Host cannot handle the DOS rate");
		if (conditions_demand_constant_host_rate) {
			LOG_MSG("SDL:   - CFR selected because we're on macOS using"
			        " OpenGL with vsync on");
		} else {
			LOG_MSG("SDL:   - %s",
					conditions_prefer_constant_rate
							? "CFR selected because we're fullscreen, "
							"vsync'd, and display is 140+Hz"
							: "VFR selected because we're not "
							"fullscreen, nor vsync'd, nor < 140Hz");
		}
#endif
		if (conditions_demand_constant_host_rate) {
			mode = FrameMode::Cfr;
			save_rate_to_frame_period(host_rate);
		} else if (supported_rate >= dos_rate) {
			mode = conditions_prefer_constant_rate ? FrameMode::Cfr
			                                       : FrameMode::Vfr;
			save_rate_to_frame_period(dos_rate);
		} else {
			mode = FrameMode::ThrottledVfr;
			save_rate_to_frame_period(nearest_common_rate(supported_rate));
		}
		// In auto-mode, the presentation rate doesn't exceed supported
		// rate so we disable the pacer.
		render_pacer->SetTimeout(0);
	}

	// If the mode is unchanged, do nothing
	assert(mode != FrameMode::Unset);
	if (previous_mode == mode)
		return;
	previous_mode = mode;
}

static void NewMouseScreenParams()
{
	if (sdl.clip.w <= 0 || sdl.clip.h <= 0 ||
	    sdl.clip.x < 0  || sdl.clip.y < 0) {
		// Filter out unusual parameters, which can be the result
		// of window minimized due to ALT+TAB, for example
		return;
	}

	MouseScreenParams params = {};

	// When DPI scaling is enabled, mouse coordinates are reported on
	// "client points" grid, not physical pixels.
	auto adapt = [](const int value) {
		return lround(value / sdl.desktop.dpi_scale);
	};

	params.clip_x = check_cast<uint32_t>(adapt(sdl.clip.x));
	params.clip_y = check_cast<uint32_t>(adapt(sdl.clip.y));
	params.res_x  = check_cast<uint32_t>(adapt(sdl.clip.w));
	params.res_y  = check_cast<uint32_t>(adapt(sdl.clip.h));

	int abs_x = 0;
	int abs_y = 0;
	SDL_GetMouseState(&abs_x, &abs_y);

	params.x_abs = check_cast<int32_t>(abs_x);
	params.y_abs = check_cast<int32_t>(abs_y);

	params.is_fullscreen    = sdl.desktop.fullscreen;
	params.is_multi_display = (SDL_GetNumVideoDisplays() > 1);

	MOUSE_NewScreenParams(params);
}

static void update_fallback_dimensions(const double dpi_scale)
{
	// TODO This only works for 320x200 games. We cannot make hardcoded
	// assumptions about aspect ratios in general, e.g. the pixel aspect ratio
	// is 1:1 for 640x480 games both with 'aspect = on` and 'aspect = off'.
	const auto fallback_height =
	        (RENDER_IsAspectRatioCorrectionEnabled() ? 480 : 400) / dpi_scale;

	assert(dpi_scale > 0);
	const auto fallback_width = 640 / dpi_scale;

	FALLBACK_WINDOW_DIMENSIONS = {iround(fallback_width),
	                              iround(fallback_height)};

	// LOG_INFO("SDL: Updated fallback dimensions to %dx%d",
	//          FALLBACK_WINDOW_DIMENSIONS.x,
	//          FALLBACK_WINDOW_DIMENSIONS.y);

	// Keep the SDL minimum allowed window size in lock-step with the
	// fallback dimensions. If these aren't linked, the window can obscure
	// the content (or vice-versa).
	if (!sdl.window) {
		return;
		// LOG_WARNING("SDL: Tried setting window minimum size,"
		//             " but the SDL window is not available yet");
	}
	SDL_SetWindowMinimumSize(sdl.window,
	                         FALLBACK_WINDOW_DIMENSIONS.x,
	                         FALLBACK_WINDOW_DIMENSIONS.y);
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
                                        const RenderingBackend rendering_backend,
                                        int width_in_logical_units = 0)
{
	if (width_in_logical_units <= 0) {
		SDL_GetWindowSize(sdl_window, &width_in_logical_units, nullptr);
	}

	const auto canvas = get_canvas_size(rendering_backend);
	const auto width_in_physical_pixels = static_cast<double>(canvas.w);

	assert(width_in_logical_units > 0);
	const auto new_dpi_scale = width_in_physical_pixels / width_in_logical_units;

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

static SDL_Window* SetWindowMode(const RenderingBackend rendering_backend,
                                 const int width, const int height,
                                 const bool fullscreen)
{
	if (sdl.window && sdl.resizing_window) {
		return sdl.window;
	}

	CleanupSDLResources();

	if (!sdl.window || (sdl.rendering_backend != rendering_backend)) {
		remove_window();

		uint32_t flags = opengl_driver_crash_workaround(rendering_backend);
		flags |= SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
#if C_OPENGL
		if (rendering_backend == RenderingBackend::OpenGl) {
			flags |= SDL_WINDOW_OPENGL;
		}
#endif
		if (!sdl.desktop.window.show_decorations) {
			flags |= SDL_WINDOW_BORDERLESS;
		}

		// Using undefined position will take care of placing and
		// restoring the window by WM.
		const auto default_val = SDL_WINDOWPOS_UNDEFINED_DISPLAY(sdl.display_number);
		const auto pos = get_initial_window_position_or_default(default_val);

		assert(sdl.window == nullptr); // enusre we don't leak
		sdl.window = SDL_CreateWindow("", pos.x, pos.y, width, height, flags);
		if (!sdl.window) {
			LOG_ERR("SDL: Failed to create window: %s", SDL_GetError());
			return nullptr;
		}

		// Certain functionality (like setting viewport) doesn't work properly
		// before initial window events are received.
		SDL_PumpEvents();

		if (rendering_backend == RenderingBackend::Texture) {
			if (sdl.renderer) {
				SDL_DestroyRenderer(sdl.renderer);
				sdl.renderer = nullptr;
			}

			assert(sdl.renderer == nullptr);
			sdl.renderer = SDL_CreateRenderer(sdl.window, -1, 0);
			if (!sdl.renderer) {
				LOG_ERR("SDL: Failed to create renderer: %s",
				        SDL_GetError());
				return nullptr;
			}
		}
#if C_OPENGL
		if (rendering_backend == RenderingBackend::OpenGl) {
			if (sdl.opengl.context) {
				SDL_GL_DeleteContext(sdl.opengl.context);
				sdl.opengl.context = nullptr;
			}

			assert(sdl.opengl.context == nullptr);
			sdl.opengl.context = SDL_GL_CreateContext(sdl.window);
			if (sdl.opengl.context == nullptr) {
				LOG_ERR("OPENGL: Can't create context: %s",
				        SDL_GetError());
				return nullptr;
			}
			if (SDL_GL_MakeCurrent(sdl.window, sdl.opengl.context) < 0) {
				LOG_ERR("OPENGL: Can't make context current: %s",
				        SDL_GetError());
				return nullptr;
			}
		}
#endif
		check_and_handle_dpi_change(sdl.window, rendering_backend);

		GFX_RefreshTitle();

		if (!fullscreen) {
			goto finish;
		}
	}
	/* Fullscreen mode switching has its limits, and is also problematic on
	 * some window managers. For now, the following may work up to some
	 * level. On X11, SDL_VIDEO_X11_LEGACY_FULLSCREEN=1 can also help,
	 * although it has its own issues.
	 */
	if (fullscreen) {
		SDL_DisplayMode displayMode;
		SDL_GetWindowDisplayMode(sdl.window, &displayMode);
		displayMode.w = width;
		displayMode.h = height;

		if (SDL_SetWindowDisplayMode(sdl.window, &displayMode) != 0) {
			LOG_WARNING("SDL: Failed setting fullscreen mode to %dx%d at %d Hz",
			            displayMode.w,
			            displayMode.h,
			            displayMode.refresh_rate);
		}
		SDL_SetWindowFullscreen(sdl.window,
		                        sdl.desktop.full.display_res
		                                ? SDL_WINDOW_FULLSCREEN_DESKTOP
		                                : SDL_WINDOW_FULLSCREEN);
	} else {
		// we're switching down from fullscreen, so let SDL use the
		// previously-set window size
		if (sdl.desktop.switching_fullscreen) {
			SDL_SetWindowFullscreen(sdl.window, 0);
		}
	}

	// Maybe some requested fullscreen resolution is unsupported?
finish:

	if (sdl.draw.has_changed)
		setup_presentation_mode(sdl.frame.mode);

	// Force redraw after changing the window
	if (sdl.draw.callback)
		sdl.draw.callback(GFX_CallBackRedraw);

	// Ensure the time to change window modes isn't counted against
	// our paced timing. This is a rare event that depends on host
	// latency (and not the rendering pipeline).
	render_pacer->Reset();

	sdl.rendering_backend = rendering_backend;
	return sdl.window;
}

// Returns the current window; used for mapper UI.
SDL_Window* GFX_GetWindow()
{
	return sdl.window;
}

// Returns the actual output size in pixels, when possible.
// Needed for DPI-scaled windows, when logical window and actual output sizes
// might not match.
static SDL_Rect get_canvas_size([[maybe_unused]] const RenderingBackend rendering_backend)
{
	SDL_Rect canvas = {};
#if SDL_VERSION_ATLEAST(2, 26, 0)
	SDL_GetWindowSizeInPixels(sdl.window, &canvas.w, &canvas.h);
#else
	switch (rendering_backend) {
	case RenderingBackend::Texture:
		if (SDL_GetRendererOutputSize(sdl.renderer, &canvas.w, &canvas.h) !=
		    0) {
			LOG_ERR("SDL: Failed to retrieve output size: %s",
			        SDL_GetError());
		}
		break;
#if C_OPENGL
	case RenderingBackend::OpenGl:
		SDL_GL_GetDrawableSize(sdl.window, &canvas.w, &canvas.h);
		break;
#endif
	default: SDL_GetWindowSize(sdl.window, &canvas.w, &canvas.h);
	}
#endif
	assert(canvas.w > 0 && canvas.h > 0);
	return canvas;
}

SDL_Rect GFX_GetCanvasSize()
{
	return get_canvas_size(sdl.rendering_backend);
}

RenderingBackend GFX_GetRenderingBackend()
{
	return sdl.rendering_backend;
}

static SDL_Point restrict_to_viewport_resolution(const int w, const int h)
{
	return sdl.use_viewport_limits
	             ? SDL_Point{std::min(iround(sdl.viewport_resolution.x *
	                                         sdl.desktop.dpi_scale),
	                                  w),
	                         std::min(iround(sdl.viewport_resolution.y *
	                                         sdl.desktop.dpi_scale),
	                                  h)}
	             : SDL_Point{w, h};
}

static std::pair<double, double> get_scale_factors_from_pixel_aspect_ratio(
        const Fraction& pixel_aspect_ratio)
{
       const auto one_per_pixel_aspect = pixel_aspect_ratio.Inverse().ToDouble();

       if (one_per_pixel_aspect > 1.0) {
               const auto scale_x = 1;
               const auto scale_y = one_per_pixel_aspect;
               return {scale_x, scale_y};
       } else {
               const auto scale_x = pixel_aspect_ratio.ToDouble();
               const auto scale_y = 1;
               return {scale_x, scale_y};
       }
}

static SDL_Window* SetupWindowScaled(const RenderingBackend rendering_backend)
{
	int window_width;
	int window_height;
	if (sdl.desktop.fullscreen) {
		window_width = sdl.desktop.full.fixed ? sdl.desktop.full.width : 0;
		window_height = sdl.desktop.full.fixed ? sdl.desktop.full.height : 0;
	} else {
		window_width  = sdl.desktop.window.width;
		window_height = sdl.desktop.window.height;
	}

	const auto [draw_scale_x, draw_scale_y] = get_scale_factors_from_pixel_aspect_ratio(
	        sdl.draw.render_pixel_aspect_ratio);

	if (window_width == 0 && window_height == 0) {
		window_width  = iround(sdl.draw.width * draw_scale_x);
		window_height = iround(sdl.draw.height * draw_scale_y);
	}

	sdl.window = SetWindowMode(rendering_backend,
	                           window_width,
	                           window_height,
	                           sdl.desktop.fullscreen);

	return sdl.window;
}

#if C_OPENGL

// A safe wrapper around that returns the default result on failure
static const char *safe_gl_get_string(const GLenum requested_name,
                                      const char *default_result = "")
{
	// the result points to a static string but can be null
	const auto result = glGetString(requested_name);

	// the default, howeever, needs to be valid
	assert(default_result);

	return result ? reinterpret_cast<const char *>(result) : default_result;
}

/* Create a GLSL shader object, load the shader source, and compile the shader. */
static GLuint BuildShader(GLenum type, const std::string& source)
{
	GLuint shader = 0;
	GLint is_shader_compiled = 0;

	assert(source.length());
	const char *shaderSrc = source.c_str();
	const char *src_strings[2] = {nullptr, nullptr};
	std::string top;

	// look for "#version" because it has to occur first
	const char *ver = strstr(shaderSrc, "#version ");
	if (ver) {
		const char *endline = strchr(ver+9, '\n');
		if (endline) {
			top.assign(shaderSrc, endline-shaderSrc+1);
			shaderSrc = endline+1;
		}
	}

	top += (type==GL_VERTEX_SHADER) ? "#define VERTEX 1\n":"#define FRAGMENT 1\n";
	if (!sdl.opengl.bilinear)
		top += "#define OPENGLNB 1\n";

	src_strings[0] = top.c_str();
	src_strings[1] = shaderSrc;

	// Create the shader object
	shader = glCreateShader(type);
	if (shader == 0) return 0;

	// Load the shader source
	glShaderSource(shader, 2, src_strings, nullptr);

	// Compile the shader
	glCompileShader(shader);

	// Check the compile status
	glGetShaderiv(shader, GL_COMPILE_STATUS, &is_shader_compiled);

	GLint info_len = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);

	// The info log might contain warnings and info messages even if the
	// compilation was successful, so we'll always log it if it's non-empty.
	if (info_len > 1) {
		std::vector<GLchar> info_log(info_len);
		glGetShaderInfoLog(shader, info_len, nullptr, info_log.data());

		if (is_shader_compiled) {
			LOG_WARNING("OPENGL: Shader info log: %s", info_log.data());
		} else {
			LOG_ERR("OPENGL: Error compiling shader: %s",
			        info_log.data());
		}
	}

	if (is_shader_compiled) {
		return shader;
	} else {
		glDeleteShader(shader);
		return 0;
	}
}

static bool LoadGLShaders(const std::string& source, GLuint *vertex,
                          GLuint *fragment)
{
	if (source.empty())
		return false;

	assert(vertex);
	assert(fragment);

	GLuint s = BuildShader(GL_VERTEX_SHADER, source);
	if (s) {
		*vertex = s;
		s = BuildShader(GL_FRAGMENT_SHADER, source);
		if (s) {
			*fragment = s;
			return true;
		}
		glDeleteShader(*vertex);
	}
	return false;
}
#endif

static bool is_using_kmsdrm_driver()
{
	const bool is_initialized = SDL_WasInit(SDL_INIT_VIDEO);
	const auto driver = is_initialized ? SDL_GetCurrentVideoDriver()
	                                   : getenv("SDL_VIDEODRIVER");
	if (!driver)
		return false;

	std::string driver_str = driver;
	lowcase(driver_str);
	return driver_str == "kmsdrm";
}

static int check_kmsdrm_setting()
{
	// Simple pre-check to see if we're using kmsdrm
	if (!is_using_kmsdrm_driver())
		return 0;

	// Do we have read access to the event subsystem
	if (auto f = fopen("/dev/input/event0", "r"); f) {
		fclose(f);
		return 0;
	}

	// We're using KMSDRM, but we don't have read access to the event subsystem
	LOG_WARNING("SDL: /dev/input/event0 is not readable, quitting early to prevent TTY input lockup.");
	LOG_WARNING("SDL: Please run: \"sudo usermod -aG input $(whoami)\", then re-login and try again.");
	return 1;
}

bool operator!=(const SDL_Point lhs, const SDL_Point rhs)
{
	return lhs.x != rhs.x || lhs.y != rhs.y;
}

static void initialize_sdl_window_size(SDL_Window* sdl_window,
                                       const SDL_Point requested_min_size,
                                       const SDL_Point requested_size)
{
	assert(sdl_window);

	// Set the (bounded) window size, if not already matching
	SDL_Point current_size = {};
	SDL_GetWindowSize(sdl_window, &current_size.x, &current_size.y);
	SDL_Point bounded_size = {std::max(requested_size.x, requested_min_size.x),
	                          std::max(requested_size.y, requested_min_size.y)};
	if (current_size != bounded_size) {
		safe_set_window_size(bounded_size.x, bounded_size.y);
		// LOG_MSG("SDL: Initialized the window size to %dx%d",
		//         bounded_size.x,
		//         bounded_size.y);
	}
}

uint8_t GFX_SetSize(const int width, const int height,
                    const Fraction& render_pixel_aspect_ratio, const uint8_t flags,
                    const VideoMode& video_mode, GFX_CallBack_t callback)
{
	uint8_t retFlags = 0;
	if (sdl.updating)
		GFX_EndUpdate(nullptr);

	GFX_DisengageRendering();
	// The rendering objects are recreated below with new sizes, after which
	// frame rendering is re-engaged with the output-type specific calls.

	const bool double_width  = flags & GFX_DBL_W;
	const bool double_height = flags & GFX_DBL_H;

	sdl.draw.has_changed = (sdl.video_mode != video_mode ||
	                        sdl.draw.width != width || sdl.draw.height != height ||
	                        sdl.draw.width_was_doubled != double_width ||
	                        sdl.draw.height_was_doubled != double_height ||
	                        sdl.draw.render_pixel_aspect_ratio != render_pixel_aspect_ratio);

	sdl.draw.width                     = width;
	sdl.draw.height                    = height;
	sdl.draw.width_was_doubled         = double_width;
	sdl.draw.height_was_doubled        = double_height;
	sdl.draw.render_pixel_aspect_ratio = render_pixel_aspect_ratio;

	sdl.video_mode = video_mode;

	sdl.draw.callback = callback;

	// If we're changing the SDL output type (ie: going from 'output =
	// texture' to 'output = OpenGL'), then re-initialize our vsync settings
	// because how the OS's compositor (if one exists) handles (or doesn't
	// handle) this new type may be different, and thus warrants new
	// measurements.
	if (sdl.want_rendering_backend  != sdl.rendering_backend) {
		initialize_vsync_settings();
	}

	switch (sdl.want_rendering_backend ) {
	case RenderingBackend::Texture: {
	fallback_texture: // FIXME: Must be replaced with a proper fallback system.

		if (!SetupWindowScaled(RenderingBackend::Texture)) {
			LOG_ERR("DISPLAY: Can't initialise 'texture' window");
			E_Exit("SDL: Failed to create window");
		}

		/* Use renderer's default format */
		SDL_RendererInfo rinfo;
		SDL_GetRendererInfo(sdl.renderer, &rinfo);
		const auto texture_format = rinfo.texture_formats[0];

		sdl.texture.texture = SDL_CreateTexture(sdl.renderer,
		                                        texture_format,
		                                        SDL_TEXTUREACCESS_STREAMING,
		                                        width,
		                                        height);

		if (!sdl.texture.texture) {
			SDL_DestroyRenderer(sdl.renderer);
			sdl.renderer = nullptr;
			E_Exit("SDL: Failed to create texture");
		}

		// release the existing surface if needed
		auto &texture_input_surface = sdl.texture.input_surface;
		if (texture_input_surface) {
			SDL_FreeSurface(texture_input_surface);
			texture_input_surface = nullptr;
		}
		assert(texture_input_surface == nullptr); // ensure we don't leak
		texture_input_surface = SDL_CreateRGBSurfaceWithFormat(
		        0, width, height, 32, texture_format);
		if (!texture_input_surface) {
			E_Exit("SDL: Error while preparing texture input");
		}

		SDL_SetRenderDrawColor(sdl.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		uint32_t pixel_format;
		assert(sdl.texture.texture);
		SDL_QueryTexture(sdl.texture.texture, &pixel_format, nullptr, nullptr, nullptr);

		assert(sdl.texture.pixelFormat == nullptr); // ensure we don't leak
		sdl.texture.pixelFormat = SDL_AllocFormat(pixel_format);
		switch (SDL_BITSPERPIXEL(pixel_format)) {
			case 8:  retFlags = GFX_CAN_8;  break;
			case 15: retFlags = GFX_CAN_15; break;
			case 16: retFlags = GFX_CAN_16; break;
			case 24: /* SDL_BYTESPERPIXEL is probably 4, though. */
			case 32: retFlags = GFX_CAN_32; break;
		        }

		// Log changes to the rendering driver
		static std::string render_driver = {};
		if (render_driver != rinfo.name) {
			LOG_MSG("SDL: Using '%s' driver for %d-bit texture rendering",
			        rinfo.name,
			        SDL_BITSPERPIXEL(pixel_format));
			render_driver = rinfo.name;
		}

		if (!(rinfo.flags & SDL_RENDERER_ACCELERATED)) {
			retFlags |= GFX_CAN_RANDOM;
		}

		// Re-apply the minimum bounds prior to clipping the
		// texture-based window because SDL invalidates the prior bounds
		// in the above changes.
		SDL_SetWindowMinimumSize(sdl.window,
		                         FALLBACK_WINDOW_DIMENSIONS.x,
		                         FALLBACK_WINDOW_DIMENSIONS.y);

		// One-time intialize the window size
		if (!sdl.desktop.window.adjusted_initial_size) {
			initialize_sdl_window_size(sdl.window,
			                           FALLBACK_WINDOW_DIMENSIONS,
			                           {sdl.desktop.window.width,
			                            sdl.desktop.window.height});
			sdl.desktop.window.adjusted_initial_size = true;
		}
		const auto canvas = get_canvas_size(sdl.want_rendering_backend);
		// LOG_MSG("Attempting to fix the centering to %d %d %d %d",
		//         (canvas.w - sdl.clip.w) / 2,
		//         (canvas.h - sdl.clip.h) / 2,
		//         sdl.clip.w,
		//         sdl.clip.h);
		sdl.clip = calc_viewport(canvas.w, canvas.h);
		if (SDL_RenderSetViewport(sdl.renderer, &sdl.clip) != 0)
			LOG_ERR("SDL: Failed to set viewport: %s", SDL_GetError());

		sdl.frame.update = update_frame_texture;
		sdl.frame.present = present_frame_texture;

		break; // RenderingBackend::Texture
	}
#if C_OPENGL
	case RenderingBackend::OpenGl: {
		free(sdl.opengl.framebuf);
		sdl.opengl.framebuf = nullptr;
		if (!(flags & GFX_CAN_32)) {
			goto fallback_texture;
		}

		int texsize_w, texsize_h;

		const auto use_npot_texture = sdl.opengl.npot_textures_supported &&
		                              sdl.opengl.shader_info.settings.use_npot_texture;
#if 0
		if (use_npot_texture) {
			LOG_MSG("OPENGL: Using NPOT texture");
		} else {
			LOG_MSG("OPENGL: Not using NPOT texture");
		}
#endif

		if (use_npot_texture) {
			texsize_w = width;
			texsize_h = height;
		} else {
			texsize_w = 2 << int_log2(width);
			texsize_h = 2 << int_log2(height);
		}

		if (texsize_w > sdl.opengl.max_texsize ||
		    texsize_h > sdl.opengl.max_texsize) {
			LOG_WARNING("OPENGL: No support for texture size of %dx%d, "
			            "falling back to texture",
			            texsize_w,
			            texsize_h);
			goto fallback_texture;
		}
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		const std::string gl_vendor = safe_gl_get_string(GL_VENDOR,
		                                                 "unknown vendor");
#	if WIN32
		const auto is_vendors_srgb_unreliable = (gl_vendor == "Intel");
#	else
		constexpr auto is_vendors_srgb_unreliable = false;
#	endif
		if (is_vendors_srgb_unreliable) {
			LOG_WARNING("OPENGL: Not requesting an sRGB framebuffer "
			            "because %s's driver is unreliable",
			            gl_vendor.data());

		} else if (SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1)) {
			LOG_ERR("OPENGL: Failed requesting an sRGB framebuffer: %s",
			        SDL_GetError());
		}

		// Re-apply the minimum bounds prior to clipping the OpenGL
		// window because SDL invalidates the prior bounds in the above
		// window changes.
		SDL_SetWindowMinimumSize(sdl.window,
		                         FALLBACK_WINDOW_DIMENSIONS.x,
		                         FALLBACK_WINDOW_DIMENSIONS.y);

		SetupWindowScaled(RenderingBackend::OpenGl);

		/* We may simply use SDL_BYTESPERPIXEL
		here rather than SDL_BITSPERPIXEL   */
		if (!sdl.window ||
		    SDL_BYTESPERPIXEL(SDL_GetWindowPixelFormat(sdl.window)) < 2) {
			LOG_WARNING("OPENGL: Can't open drawing window, are you running in 16bpp (or higher) mode?");
			goto fallback_texture;
		}

		if (sdl.opengl.use_shader) {
			GLuint prog=0;
			// reset error
			glGetError();
			glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&prog);
			// if there was an error this context doesn't support shaders
			if (glGetError()==GL_NO_ERROR && (sdl.opengl.program_object==0 || prog!=sdl.opengl.program_object)) {
				// check if existing program is valid
				if (sdl.opengl.program_object) {
					glUseProgram(sdl.opengl.program_object);
					if (glGetError() != GL_NO_ERROR) {
						// program is not usable (probably new context), purge it
						glDeleteProgram(sdl.opengl.program_object);
						sdl.opengl.program_object = 0;
					}
				}

				// does program need to be rebuilt?
				if (sdl.opengl.program_object == 0) {
					GLuint vertexShader, fragmentShader;

					if (!LoadGLShaders(sdl.opengl.shader_source,
					                   &vertexShader,
					                   &fragmentShader)) {
						LOG_ERR("OPENGL: Failed to compile shader");
						goto fallback_texture;
					}

					sdl.opengl.program_object = glCreateProgram();
					if (!sdl.opengl.program_object) {
						glDeleteShader(vertexShader);
						glDeleteShader(fragmentShader);

						LOG_WARNING("OPENGL: Can't create program object, "
						            "falling back to texture");
						goto fallback_texture;
					}
					glAttachShader(sdl.opengl.program_object, vertexShader);
					glAttachShader(sdl.opengl.program_object, fragmentShader);

					// Link the program
					glLinkProgram(sdl.opengl.program_object);

					// Even if we *are* successful, we may delete the shader objects
					glDeleteShader(vertexShader);
					glDeleteShader(fragmentShader);

					// Check the link status
					GLint is_program_linked = 0;
					glGetProgramiv(sdl.opengl.program_object, GL_LINK_STATUS, &is_program_linked);

					// The info log might contain warnings and info messages
					// even if the linking was successful, so we'll always log
					// it if it's non-empty.
					GLint info_len = 0;

					glGetProgramiv(sdl.opengl.program_object,
					               GL_INFO_LOG_LENGTH,
					               &info_len);

					if (info_len > 1) {
						std::vector<GLchar> info_log(info_len);

						glGetProgramInfoLog(sdl.opengl.program_object,
						                    info_len,
						                    nullptr,
						                    info_log.data());

						if (is_program_linked) {
							LOG_WARNING("OPENGL: Program info log:\n %s",
							            info_log.data());
						} else {
							LOG_ERR("OPENGL: Error linking program:\n %s",
							        info_log.data());
						}
					}

					if (!is_program_linked) {
						glDeleteProgram(sdl.opengl.program_object);
						sdl.opengl.program_object = 0;
						goto fallback_texture;
					}

					glUseProgram(sdl.opengl.program_object);

					GLint u = glGetAttribLocation(sdl.opengl.program_object, "a_position");
					// upper left
					sdl.opengl.vertex_data[0] = -1.0f;
					sdl.opengl.vertex_data[1] = 1.0f;
					// lower left
					sdl.opengl.vertex_data[2] = -1.0f;
					sdl.opengl.vertex_data[3] = -3.0f;
					// upper right
					sdl.opengl.vertex_data[4] = 3.0f;
					sdl.opengl.vertex_data[5] = 1.0f;
					// Load the vertex positions
					glVertexAttribPointer(u, 2, GL_FLOAT, GL_FALSE, 0, sdl.opengl.vertex_data);
					glEnableVertexAttribArray(u);

					u = glGetUniformLocation(sdl.opengl.program_object, "rubyTexture");
					glUniform1i(u, 0);

					sdl.opengl.ruby.texture_size = glGetUniformLocation(sdl.opengl.program_object, "rubyTextureSize");
					sdl.opengl.ruby.input_size = glGetUniformLocation(sdl.opengl.program_object, "rubyInputSize");
					sdl.opengl.ruby.output_size = glGetUniformLocation(sdl.opengl.program_object, "rubyOutputSize");
					sdl.opengl.ruby.frame_count = glGetUniformLocation(sdl.opengl.program_object, "rubyFrameCount");
				}
			}
		}

		/* Create the texture and display list */
		const auto framebuffer_bytes = static_cast<size_t>(width) *
		                               height * MAX_BYTES_PER_PIXEL;
		sdl.opengl.framebuf = malloc(framebuffer_bytes); // 32 bit colour
		sdl.opengl.pitch = width * 4;

		// One-time initialize the window size
		if (!sdl.desktop.window.adjusted_initial_size) {
			initialize_sdl_window_size(sdl.window,
			                           FALLBACK_WINDOW_DIMENSIONS,
			                           {sdl.desktop.window.width,
			                            sdl.desktop.window.height});
			sdl.desktop.window.adjusted_initial_size = true;
		}

		const auto canvas = get_canvas_size(sdl.want_rendering_backend);
		// LOG_MSG("Attempting to fix the centering to %d %d %d %d",
		//         (canvas.w - sdl.clip.w) / 2,
		//         (canvas.h - sdl.clip.h) / 2,
		//         sdl.clip.w,
		//         sdl.clip.h);
		sdl.clip = calc_viewport(canvas.w, canvas.h);
		glViewport(sdl.clip.x, sdl.clip.y, sdl.clip.w, sdl.clip.h);

		if (sdl.opengl.texture > 0) {
			glDeleteTextures(1,&sdl.opengl.texture);
		}
		glGenTextures(1, &sdl.opengl.texture);
		glBindTexture(GL_TEXTURE_2D,sdl.opengl.texture);

		// No borders
		const auto wrap_parameter = use_npot_texture ? GL_CLAMP_TO_EDGE
		                                             : GL_CLAMP_TO_BORDER;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_parameter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_parameter);

		const GLint filter = (sdl.interpolation_mode == InterpolationMode::NearestNeighbour
		                              ? GL_NEAREST
		                              : GL_LINEAR);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);

		const auto texture_area_bytes = static_cast<size_t>(texsize_w) *
		                                texsize_h * MAX_BYTES_PER_PIXEL;
		uint8_t *emptytex = new uint8_t[texture_area_bytes];
		assert(emptytex);

		memset(emptytex, 0, texture_area_bytes);

		int is_framebuffer_srgb_capable = 0;
		if (SDL_GL_GetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,
		                        &is_framebuffer_srgb_capable)) {
			LOG_WARNING("OPENGL: Failed getting the framebuffer's sRGB status: %s",
			            SDL_GetError());
		}

		sdl.opengl.framebuffer_is_srgb_encoded =
		        sdl.opengl.shader_info.settings.use_srgb_framebuffer &&
		        (is_framebuffer_srgb_capable > 0);

		if (sdl.opengl.shader_info.settings.use_srgb_framebuffer &&
		    !sdl.opengl.framebuffer_is_srgb_encoded) {
			LOG_WARNING("OPENGL: sRGB framebuffer not supported");
		}

		// Using GL_SRGB8_ALPHA8 because GL_SRGB8 doesn't work properly
		// with Mesa drivers on certain integrated Intel GPUs
		const auto texformat = sdl.opengl.shader_info.settings.use_srgb_texture &&
		                                       sdl.opengl.framebuffer_is_srgb_encoded
		                             ? GL_SRGB8_ALPHA8
		                             : GL_RGB8;

#if 0
		if (texformat == GL_SRGB8_ALPHA8) {
			LOG_MSG("OPENGL: Using sRGB texture");
		}
#endif

		glTexImage2D(GL_TEXTURE_2D,
		             0,
		             texformat,
		             texsize_w,
		             texsize_h,
		             0,
		             GL_BGRA_EXT,
		             GL_UNSIGNED_BYTE,
		             emptytex);
		delete[] emptytex;

		if (sdl.opengl.framebuffer_is_srgb_encoded) {
			glEnable(GL_FRAMEBUFFER_SRGB);
#if 0
			LOG_MSG("OPENGL: Using sRGB framebuffer");
#endif
		} else {
			glDisable(GL_FRAMEBUFFER_SRGB);
		}

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		glClear(GL_COLOR_BUFFER_BIT);
		SDL_GL_SwapWindow(sdl.window);
		glClear(GL_COLOR_BUFFER_BIT);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_LIGHTING);
		glDisable(GL_CULL_FACE);
		glEnable(GL_TEXTURE_2D);

		if (sdl.opengl.program_object) {
			// Set shader variables
			glUniform2f(sdl.opengl.ruby.texture_size,
			            (GLfloat)texsize_w, (GLfloat)texsize_h);
			glUniform2f(sdl.opengl.ruby.input_size,
			            (GLfloat)width,
			            (GLfloat)height);
			glUniform2f(sdl.opengl.ruby.output_size, (GLfloat)sdl.clip.w, (GLfloat)sdl.clip.h);
			// The following uniform is *not* set right now
			sdl.opengl.actual_frame_count = 0;
		} else {
			GLfloat tex_width = ((GLfloat)width / (GLfloat)texsize_w);
			GLfloat tex_height = ((GLfloat)height / (GLfloat)texsize_h);

			glShadeModel(GL_FLAT);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();

			if (glIsList(sdl.opengl.displaylist)) glDeleteLists(sdl.opengl.displaylist, 1);
			sdl.opengl.displaylist = glGenLists(1);
			glNewList(sdl.opengl.displaylist, GL_COMPILE);

			//Create one huge triangle and only display a portion.
			//When using a quad, there was scaling bug (certain resolutions on Nvidia chipsets) in the seam
			glBegin(GL_TRIANGLES);
			// upper left
			glTexCoord2f(0,0); glVertex2f(-1.0f, 1.0f);
			// lower left
			glTexCoord2f(0,tex_height*2); glVertex2f(-1.0f,-3.0f);
			// upper right
			glTexCoord2f(tex_width*2,0); glVertex2f(3.0f, 1.0f);
			glEnd();

			glEndList();
		}

		OPENGL_ERROR("End of setsize");

		retFlags          = GFX_CAN_32 | GFX_CAN_RANDOM;
		sdl.frame.update  = update_frame_gl;
		sdl.frame.present = present_frame_gl;
		break; // RenderingBackend::OpenGl
	}
#endif // C_OPENGL
	}
	// Ensure mouse emulation knows the current parameters
	NewMouseScreenParams();
	update_vsync_state();

	if (sdl.draw.has_changed) {
		log_display_properties(sdl.draw.width,
		                       sdl.draw.height,
		                       sdl.video_mode,
		                       {},
		                       sdl.rendering_backend);
	}

	if (retFlags) {
		GFX_Start();
	}
	return retFlags;
}

void GFX_SetShader([[maybe_unused]] const ShaderInfo& shader_info,
                   [[maybe_unused]] const std::string& shader_source)
{
#if C_OPENGL
	sdl.opengl.shader_info   = shader_info;
	sdl.opengl.shader_source = shader_source;

	if (!sdl.opengl.use_shader) {
		return;
	}
	if (sdl.opengl.program_object) {
		glDeleteProgram(sdl.opengl.program_object);
		sdl.opengl.program_object = 0;
	}
#endif
}

void GFX_SetMouseHint(const MouseHint hint_id)
{
	static const std::string prexix = " - ";

	auto create_hint_str = [](const char *requested_name) {
		char hint_buffer[200] = {0};

		safe_sprintf(hint_buffer,
		             MSG_GetRaw(requested_name),
		             PRIMARY_MOD_NAME);
		return prexix + hint_buffer;
	};

	auto &hint_str = sdl.title_bar.hint_mouse_str;
	switch (hint_id) {
	case MouseHint::None:
		hint_str.clear();
		break;
	case MouseHint::NoMouse:
		hint_str = prexix + MSG_GetRaw("TITLEBAR_HINT_NOMOUSE");
		break;
	case MouseHint::CapturedHotkey:
		hint_str = create_hint_str("TITLEBAR_HINT_CAPTURED_HOTKEY");
		break;
	case MouseHint::CapturedHotkeyMiddle:
		hint_str = create_hint_str("TITLEBAR_HINT_CAPTURED_HOTKEY_MIDDLE");
		break;
	case MouseHint::ReleasedHotkey:
		hint_str = create_hint_str("TITLEBAR_HINT_RELEASED_HOTKEY");
		break;
	case MouseHint::ReleasedHotkeyMiddle:
		hint_str = create_hint_str("TITLEBAR_HINT_RELEASED_HOTKEY_MIDDLE");
		break;
	case MouseHint::ReleasedHotkeyAnyButton:
		hint_str = create_hint_str("TITLEBAR_HINT_RELEASED_HOTKEY_ANY_BUTTON");
		break;
	case MouseHint::SeamlessHotkey:
		hint_str = create_hint_str("TITLEBAR_HINT_SEAMLESS_HOTKEY");
		break;
	case MouseHint::SeamlessHotkeyMiddle:
		hint_str = create_hint_str("TITLEBAR_HINT_SEAMLESS_HOTKEY_MIDDLE");
		break;
	default:
		assert(false);
		hint_str.clear();
		break;
	}

	GFX_RefreshTitle();
}

void GFX_CenterMouse()
{
	assert(sdl.window);

	int width  = 0;
	int height = 0;

#if defined(WIN32) && !SDL_VERSION_ATLEAST(2, 28, 1)
	const auto window_canvas_size = get_canvas_size(sdl.rendering_backend);

	width  = window_canvas_size.w;
	height = window_canvas_size.h;
#else
	SDL_GetWindowSize(sdl.window, &width, &height);
#endif

	SDL_WarpMouseInWindow(sdl.window, width / 2, height / 2);
}

void GFX_SetMouseRawInput(const bool requested_raw_input)
{
	if (SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP,
	                            requested_raw_input ? "0" : "1",
	                            SDL_HINT_OVERRIDE) != SDL_TRUE)
		LOG_WARNING("SDL: Mouse raw input %s failed",
		            requested_raw_input ? "enable" : "disable");
}

void GFX_SetMouseCapture(const bool requested_capture)
{
	const auto param = requested_capture ? SDL_TRUE : SDL_FALSE;
	if (SDL_SetRelativeMouseMode(param) != 0) {
		SDL_ShowCursor(SDL_ENABLE);
		E_Exit("SDL: Failed to %s relative-mode [SDL Bug]",
		       requested_capture ? "put the mouse in" :
		                           "take the mouse out of");
	}
}

void GFX_SetMouseVisibility(const bool requested_visible)
{
	const auto param = requested_visible ? SDL_ENABLE : SDL_DISABLE;
	if (SDL_ShowCursor(param) < 0)
		E_Exit("SDL: Failed to make mouse cursor %s [SDL Bug]",
		       requested_visible ? "visible" : "invisible");
}

static void FocusInput()
{
#if defined(WIN32)
	sdl.focus_ticks = GetTicks();
#endif

	// Ensure we have input focus when in fullscreen
	if (!sdl.desktop.fullscreen)
		return;

	// Do we already have focus?
	if (SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_INPUT_FOCUS)
		return;

	// If not, raise-and-focus to prevent stranding the window
	SDL_RaiseWindow(sdl.window);
	SDL_SetWindowInputFocus(sdl.window);
}

#if defined (WIN32)
STICKYKEYS stick_keys = {sizeof(STICKYKEYS), 0};
void sticky_keys(bool restore){
	static bool inited = false;
	if (!inited){
		inited = true;
		SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
	}
	if (restore) {
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
		return;
	}
	//Get current sticky keys layout:
	STICKYKEYS s = {sizeof(STICKYKEYS), 0};
	SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
	if ( !(s.dwFlags & SKF_STICKYKEYSON)) { //Not on already
		s.dwFlags &= ~SKF_HOTKEYACTIVE;
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
	}
}
#endif

void GFX_SwitchFullScreen()
{
	sdl.desktop.switching_fullscreen = true;

	// Record the window's current canvas size if we're departing window-mode
	auto &window_canvas_size = sdl.desktop.window.canvas_size;
	if (!sdl.desktop.fullscreen)
		window_canvas_size = get_canvas_size(sdl.rendering_backend);

#if defined(WIN32)
	// We are about to switch to the opposite of our current mode
	// (ie: opposite of whatever sdl.desktop.fullscreen holds).
	// Sticky-keys should be set to the opposite of fullscreen,
	// so we simply apply the bool of the mode we're switching out-of.
	sticky_keys(sdl.desktop.fullscreen);
#endif
	sdl.desktop.fullscreen = !sdl.desktop.fullscreen;
	GFX_ResetScreen();
	FocusInput();
	setup_presentation_mode(sdl.frame.mode);

	// After switching modes, get the current canvas size, which might be
	// the windowed size or full screen size.
	auto canvas_size = sdl.desktop.fullscreen
	                         ? get_canvas_size(sdl.rendering_backend)
	                         : window_canvas_size;

	log_display_properties(sdl.draw.width,
	                       sdl.draw.height,
	                       sdl.video_mode,
	                       canvas_size,
	                       sdl.rendering_backend);

	sdl.desktop.switching_fullscreen = false;
}

static void SwitchFullScreen(bool pressed)
{
	if (pressed)
		GFX_SwitchFullScreen();
}

// This function returns write'able buffer for user to draw upon. Successful
// return depends on properly initialized SDL_Block structure (which generally
// can be achieved via GFX_SetSize call), and specifically - properly
// initialized output-specific bits (sdl.texture or sdl.opengl.framebuf fields).
//
// If everything is prepared correctly, this function returns true, assigns
// 'pixels' output parameter to to a buffer (with format specified via earlier
// GFX_SetSize call), and assigns 'pitch' to a number of bytes used for a single
// pixels row in 'pixels' buffer.
//
bool GFX_StartUpdate(uint8_t * &pixels, int &pitch)
{
	if (!sdl.active || sdl.updating)
		return false;

	switch (sdl.rendering_backend) {
	case RenderingBackend::Texture:
		assert(sdl.texture.input_surface);
		pixels = static_cast<uint8_t *>(sdl.texture.input_surface->pixels);
		pitch = sdl.texture.input_surface->pitch;
		sdl.updating = true;
		return true;
#if C_OPENGL
	case RenderingBackend::OpenGl:
		pixels = static_cast<uint8_t*>(sdl.opengl.framebuf);
		OPENGL_ERROR("end of start update");
		if (pixels == nullptr) {
			return false;
		}
		static_assert(std::is_same<decltype(pitch), decltype((sdl.opengl.pitch))>::value,
		              "Our internal pitch types should be the same.");
		pitch        = sdl.opengl.pitch;
		sdl.updating = true;
		return true;
#endif
	}
	return false;
}

void GFX_EndUpdate(const uint16_t *changedLines)
{
	sdl.frame.update(changedLines);

	if (CAPTURE_IsCapturingPostRenderImage()) {
		// Always present the frame if we want to capture the next rendered
		// frame, regardless of the presentation mode. This is necessary to
		// keep the contents of rendered and raw/upscaled screenshots in sync
		// (so they capture the exact same frame) in multi-output image
		// capture modes.
		sdl.frame.present();
	} else {
		// Helper lambda indicating whether the frame should be presented.
		// Returns true if the frame has been updated or if the limit of
		// sequentially skipped duplicate frames has been reached.
		auto vfr_should_present = []() {
			static uint16_t dupe_tally = 0;
			if (sdl.updating || ++dupe_tally > sdl.frame.max_dupe_frames) {
				dupe_tally = 0;
				return true;
			}
			return false;
		};

		switch (sdl.frame.mode) {
		case FrameMode::Cfr:
			maybe_present_synced(sdl.updating);
			break;
		case FrameMode::Vfr:
			if (vfr_should_present()) {
				sdl.frame.present();
			}
			break;
		case FrameMode::ThrottledVfr:
			maybe_present_throttled(vfr_should_present());
			break;
		case FrameMode::Unset:
			break;
		}
	}
	sdl.updating = false;
	FrameMark;
}

// Texture update and presentation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void update_frame_texture([[maybe_unused]] const uint16_t *changedLines)
{
	SDL_UpdateTexture(sdl.texture.texture,
	                  nullptr, // update entire texture
	                  sdl.texture.input_surface->pixels,
	                  sdl.texture.input_surface->pitch);
}

static std::optional<RenderedImage> get_rendered_output_from_backbuffer()
{
	RenderedImage image = {};

	auto allocate_image = [&]() {
		image.params.width              = sdl.clip.w;
		image.params.height             = sdl.clip.h;
		image.params.double_width       = false;
		image.params.double_height      = false;
		image.params.pixel_aspect_ratio = {1};
		image.params.pixel_format       = PixelFormat::BGR24_ByteArray;
		image.params.video_mode         = sdl.video_mode;

		image.is_flipped_vertically = false;

		image.pitch = image.params.width *
		              (get_bits_per_pixel(image.params.pixel_format) / 8);

		image.palette_data = nullptr;

		const auto image_size_bytes = check_cast<uint32_t>(
		        image.params.height * image.pitch);
		image.image_data = new uint8_t[image_size_bytes];
	};

#if C_OPENGL
	// Get the OpenGL-renderer surface
	// -------------------------------
	if (sdl.rendering_backend == RenderingBackend::OpenGl) {
		glReadBuffer(GL_BACK);

		// Alignment is 4 by default which works fine when using the
		// GL_BGRA pixel format with glReadPixels(). We need to set it 1
		// to be able to use the GL_BGR format in order to conserve
		// memory. This should not cause any slowdowns whatsoever.
		glPixelStorei(GL_PACK_ALIGNMENT, 1);

		allocate_image();

		glReadPixels(sdl.clip.x,
		             sdl.clip.y,
		             image.params.width,
		             image.params.height,
		             GL_BGR,
		             GL_UNSIGNED_BYTE,
		             image.image_data);

		image.is_flipped_vertically = true;
		return image;
	}
#endif

	// Get the SDL texture-renderer surface
	// ------------------------------------
	const auto renderer = SDL_GetRenderer(sdl.window);
	if (!renderer) {
		LOG_WARNING("SDL: Failed retrieving texture renderer surface: %s",
		            SDL_GetError());
		return {};
	}

	allocate_image();

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
	if (SDL_RenderReadPixels(renderer,
	                         &sdl.clip,
	                         SDL_PIXELFORMAT_BGR24,
	                         image.image_data,
	                         image.pitch) != 0) {
		LOG_WARNING("SDL: Failed reading pixels from the texture renderer: %s",
		            SDL_GetError());
		delete[] image.image_data;
		return {};
	}
	return image;
}

static bool present_frame_texture()
{
	const auto is_presenting = render_pacer->CanRun();
	if (is_presenting) {
		SDL_RenderClear(sdl.renderer);
		SDL_RenderCopy(sdl.renderer, sdl.texture.texture, nullptr, nullptr);

		if (CAPTURE_IsCapturingPostRenderImage()) {
			// glReadPixels() implicitly blocks until all pipelined rendering
			// commands have finished, so we're guaranteed to read the
			// contents of the up-to-date backbuffer here right before the
			// buffer swap.
			const auto image = get_rendered_output_from_backbuffer();
			if (image) {
				CAPTURE_AddPostRenderImage(*image);
			}
		}

		SDL_RenderPresent(sdl.renderer);
	}
	render_pacer->Checkpoint();
	return is_presenting;
}

// OpenGL frame-based update and presentation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#if C_OPENGL
static void update_frame_gl(const uint16_t* changedLines)
{
	if (changedLines) {
		const auto framebuf = static_cast<uint8_t *>(sdl.opengl.framebuf);
		const auto pitch = sdl.opengl.pitch;
		int y = 0;
		size_t index = 0;
		while (y < sdl.draw.height) {
			if (!(index & 1)) {
				y += changedLines[index];
			} else {
				const uint8_t *pixels = framebuf + y * pitch;
				const int height = changedLines[index];
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y,
				                sdl.draw.width, height, GL_BGRA_EXT,
				                GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
				y += height;
			}
			index++;
		}
	} else {
		sdl.opengl.actual_frame_count++;
	}
}

static bool present_frame_gl()
{
	const auto is_presenting = render_pacer->CanRun();
	if (is_presenting) {
		glClear(GL_COLOR_BUFFER_BIT);
		if (sdl.opengl.program_object) {
			glUniform1i(sdl.opengl.ruby.frame_count,
			            sdl.opengl.actual_frame_count++);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		} else {
			glCallList(sdl.opengl.displaylist);
		}

		if (CAPTURE_IsCapturingPostRenderImage()) {
			// glReadPixels() implicitly blocks until all pipelined rendering
			// commands have finished, so we're guaranateed to read the
			// contents of the up-to-date backbuffer here right before the
			// buffer swap.
			const auto image = get_rendered_output_from_backbuffer();
			if (image) {
				CAPTURE_AddPostRenderImage(*image);
			}
		}

		SDL_GL_SwapWindow(sdl.window);
	}
	render_pacer->Checkpoint();
	return is_presenting;
}
#endif

uint32_t GFX_GetRGB(const uint8_t red, const uint8_t green, const uint8_t blue)
{
	switch (sdl.rendering_backend) {
	case RenderingBackend::Texture:
		assert(sdl.texture.pixelFormat);
		return SDL_MapRGB(sdl.texture.pixelFormat, red, green, blue);
#if C_OPENGL
	case RenderingBackend::OpenGl:
		return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
#endif
	}
	return 0;
}

void GFX_Stop() {
	if (sdl.updating)
		GFX_EndUpdate(nullptr);
	sdl.active=false;
}

void GFX_Start() {
	sdl.active=true;
}

void GFX_ObtainDisplayDimensions() {
	SDL_Rect displayDimensions;
	SDL_GetDisplayBounds(sdl.display_number, &displayDimensions);
	sdl.desktop.full.width = displayDimensions.w;
	sdl.desktop.full.height = displayDimensions.h;
}

/* Manually update display dimensions in case of a window resize,
 * IF there is the need for that ("yes" on Android, "no" otherwise).
 * Used for the mapper UI on Android.
 * Reason is the usage of GFX_GetSDLSurfaceSubwindowDims, as well as a
 * mere notification of the fact that the window's dimensions are modified.
 */
void GFX_UpdateDisplayDimensions(int width, int height)
{
	if (sdl.desktop.full.display_res && sdl.desktop.fullscreen) {
		/* Note: We should not use GFX_ObtainDisplayDimensions
		(SDL_GetDisplayBounds) on Android after a screen rotation:
		The older values from application startup are returned. */
		sdl.desktop.full.width = width;
		sdl.desktop.full.height = height;
	}
}

static void CleanupSDLResources()
{
	if (sdl.texture.pixelFormat) {
		SDL_FreeFormat(sdl.texture.pixelFormat);
		sdl.texture.pixelFormat = nullptr;
	}
	if (sdl.texture.texture) {
		SDL_DestroyTexture(sdl.texture.texture);
		sdl.texture.texture = nullptr;
	}
	if (sdl.texture.input_surface) {
		SDL_FreeSurface(sdl.texture.input_surface);
		sdl.texture.input_surface = nullptr;
	}
}

static void GUI_ShutDown(Section *)
{
	GFX_Stop();
	if (sdl.draw.callback)
		(sdl.draw.callback)( GFX_CallBackStop );
	if (sdl.desktop.fullscreen)
		GFX_SwitchFullScreen();

	GFX_SetMouseCapture(false);
	GFX_SetMouseVisibility(true);

	CleanupSDLResources();
	if (sdl.renderer) {
		SDL_DestroyRenderer(sdl.renderer);
		sdl.renderer = nullptr;
	}
#if C_OPENGL
	if (sdl.opengl.context) {
		SDL_GL_DeleteContext(sdl.opengl.context);
		sdl.opengl.context = nullptr;
	}
#endif

	remove_window();
}

static void SetPriority(PRIORITY_LEVELS level)
{
	// Just let the OS scheduler manage priority
	if (level == PRIORITY_LEVEL_AUTO)
		return;

		// TODO replace platform-specific API with SDL_SetThreadPriority
#if defined(HAVE_SETPRIORITY)
	// If the priorities are different, do nothing unless the user is root,
	// since priority can always be lowered but requires elevated rights
	// to increase

	if ((sdl.priority.active != sdl.priority.inactive) && (getuid() != 0))
		return;

#endif
	switch (level) {
#ifdef WIN32
	case PRIORITY_LEVEL_LOWEST:
		SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_LOWER:
		SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_NORMAL:
		SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHER:
		SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHEST:
		SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
		break;
#elif defined(HAVE_SETPRIORITY)
		/* Linux use group as dosbox has mulitple threads under linux */
	case PRIORITY_LEVEL_LOWEST:
		setpriority (PRIO_PGRP, 0,PRIO_MAX);
		break;
	case PRIORITY_LEVEL_LOWER:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/3));
		break;
	case PRIORITY_LEVEL_NORMAL:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/2));
		break;
	case PRIORITY_LEVEL_HIGHER:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/5) );
		break;
	case PRIORITY_LEVEL_HIGHEST:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/4) );
		break;
#endif
	default:
		break;
	}
}

static SDL_Window *SetDefaultWindowMode()
{
	if (sdl.window)
		return sdl.window;

	sdl.draw.width = FALLBACK_WINDOW_DIMENSIONS.x;
	sdl.draw.height = FALLBACK_WINDOW_DIMENSIONS.y;

	if (sdl.desktop.fullscreen) {
		sdl.desktop.lazy_init_window_size = true;
		return SetWindowMode(sdl.want_rendering_backend,
		                     sdl.desktop.full.width,
		                     sdl.desktop.full.height,
		                     sdl.desktop.fullscreen);
	}
	sdl.desktop.lazy_init_window_size = false;
	return SetWindowMode(sdl.want_rendering_backend,
	                     sdl.desktop.window.width,
	                     sdl.desktop.window.height,
	                     sdl.desktop.fullscreen);
}

static SDL_Point refine_window_size(const SDL_Point size,
                                    const bool wants_aspect_ratio_correction)
{
	// TODO This only works for 320x200 games. We cannot make hardcoded
	// assumptions about aspect ratios in general, e.g. the pixel aspect ratio
	// is 1:1 for 640x480 games both with 'aspect = on` and 'aspect = off'.
	constexpr SDL_Point ratios_for_stretched_pixels = {4, 3};
	constexpr SDL_Point ratios_for_square_pixels    = {8, 5};

	const auto image_aspect = wants_aspect_ratio_correction
	                                ? ratios_for_stretched_pixels
	                                : ratios_for_square_pixels;

	const auto window_aspect = static_cast<double>(size.x) / size.y;
	const auto game_aspect   = static_cast<double>(image_aspect.x) /
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
	return FALLBACK_WINDOW_DIMENSIONS;
}

static SDL_Rect get_desktop_resolution()
{
	SDL_Rect desktop;
	assert(sdl.display_number >= 0);
	SDL_GetDisplayBounds(sdl.display_number, &desktop);

	// Deduct the border decorations from the desktop size
	int top = 0;
	int left = 0;
	int bottom = 0;
	int right = 0;
	(void)SDL_GetWindowBordersSize(SDL_GetWindowFromID(sdl.display_number),
	                               &top, &left, &bottom, &right);
	// If SDL_GetWindowBordersSize fails, it populates the values with 0.
	desktop.w -= (left + right);
	desktop.h -= (top + bottom);

	assert(desktop.w >= FALLBACK_WINDOW_DIMENSIONS.x);
	assert(desktop.h >= FALLBACK_WINDOW_DIMENSIONS.y);
	return desktop;
}

static void maybe_limit_requested_resolution(int &w, int &h, const char *size_description)
{
	const auto desktop = get_desktop_resolution();
	if (w <= desktop.w && h <= desktop.h)
		return;

	bool was_limited = false;

	// Add any driver / platform / operating system limits in succession:

	// SDL KMSDRM limitations:
	if (is_using_kmsdrm_driver()) {
		w = desktop.w;
		h = desktop.h;
		was_limited = true;
		LOG_WARNING("DISPLAY: Limiting %s resolution to '%dx%d' to avoid kmsdrm issues",
		            size_description,
		            w,
		            h);
	}

	if (!was_limited)
		LOG_INFO("DISPLAY: Accepted %s resolution %dx%d despite exceeding "
		         "the %dx%d display",
		         size_description,
		         w,
		         h,
		         desktop.w,
		         desktop.h);
}

static SDL_Point parse_window_resolution_from_conf(const std::string &pref)
{
	int w = 0;
	int h = 0;
	const bool was_parsed = sscanf(pref.c_str(), "%dx%d", &w, &h) == 2;

	const bool is_valid = (w >= FALLBACK_WINDOW_DIMENSIONS.x &&
	                       h >= FALLBACK_WINDOW_DIMENSIONS.y);

	if (was_parsed && is_valid) {
		maybe_limit_requested_resolution(w, h, "window");
		return {w, h};
	}

	LOG_WARNING("DISPLAY: Requested windowresolution '%s' is not valid, "
	            "falling back to '%dx%d' instead",
	            pref.c_str(),
	            FALLBACK_WINDOW_DIMENSIONS.x,
	            FALLBACK_WINDOW_DIMENSIONS.y);

	return FALLBACK_WINDOW_DIMENSIONS;
}

static SDL_Point window_bounds_from_label(const std::string& pref,
                                          const SDL_Rect desktop)
{
	constexpr int SmallPercent  = 50;
	constexpr int MediumPercent = 74;
	constexpr int LargePercent  = 90;

	const int percent = [&] {
		if (starts_with(pref, "s")) {
			return SmallPercent;
		} else if (starts_with(pref, "m") || pref == "default" ||
		           pref.empty()) {
			return MediumPercent;
		} else if (starts_with(pref, "l")) {
			return LargePercent;
		} else if (pref == "desktop") {
			return 100;
		} else {
			LOG_WARNING("DISPLAY: Requested windowresolution '%s' is invalid, "
			            "using 'default' instead",
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
	const auto w = std::max(size.x, FALLBACK_WINDOW_DIMENSIONS.x);
	const auto h = std::max(size.y, FALLBACK_WINDOW_DIMENSIONS.y);
	return {w, h};
}

// Takes in:
//  - The 'viewport_resolution' config value: 'fit', 'WxH', 'N[.M]%', or an invalid setting.
//
// Except for SURFACE and TEXTURE rendering, the function populates the following struct members:
//  - 'sdl.desktop.use_viewport_limits', true if the viewport_resolution feature is enabled.
//  - 'sdl.desktop.viewport_resolution', with the refined size.

static void setup_viewport_resolution_from_conf(const std::string &viewport_resolution_val)
{
	sdl.use_viewport_limits = false;
	sdl.viewport_resolution = {-1, -1};

	constexpr auto default_val = "fit";
	if (viewport_resolution_val == default_val)
		return;

	int w = 0;
	int h = 0;
	float p = 0.0f;
	const auto was_parsed = sscanf(viewport_resolution_val.c_str(), "%dx%d", &w, &h) == 2 ||
	                        sscanf(viewport_resolution_val.c_str(), "%f%%", &p) == 1;

	if (!was_parsed) {
		LOG_WARNING("DISPLAY: Requested viewport_resolution '%s' was not in WxH"
		            " or N%% format, using the default setting ('%s') instead",
		            viewport_resolution_val.c_str(), default_val);
		return;
	}

	const auto desktop = get_desktop_resolution();
	const bool is_out_of_bounds = (w <= 0 || w > desktop.w || h <= 0 ||
	                               h > desktop.h) &&
	                              (p <= 0.0f || p > 100.0f);
	if (is_out_of_bounds) {
		LOG_WARNING("DISPLAY: Requested viewport_resolution of '%s' is outside"
		            " the desktop '%dx%d' bounds or the 1-100%% range, "
		            " using the default setting ('%s') instead",
		            viewport_resolution_val.c_str(), desktop.w,
		            desktop.h, default_val);
		return;
	}

	sdl.use_viewport_limits = true;
	if (p > 0.0f) {
		sdl.viewport_resolution.x = iround(desktop.w * static_cast<double>(p) / 100.0);
		sdl.viewport_resolution.y = iround(desktop.h * static_cast<double>(p) / 100.0);
		LOG_MSG("DISPLAY: Limiting viewport resolution to %2.4g%% (%dx%d) of the desktop",
		        static_cast<double>(p), sdl.viewport_resolution.x, sdl.viewport_resolution.y);

	} else {
		sdl.viewport_resolution = {w, h};
		LOG_MSG("DISPLAY: Limiting viewport resolution to %dx%d",
		        sdl.viewport_resolution.x, sdl.viewport_resolution.y);
	}
}

static void setup_initial_window_position_from_conf(const std::string &window_position_val)
{
	sdl.desktop.window.initial_x_pos = -1;
	sdl.desktop.window.initial_y_pos = -1;

	if (window_position_val == "auto")
		return;

	int x, y;
	const auto was_parsed = sscanf(window_position_val.c_str(), "%d,%d", &x, &y) == 2;
	if (!was_parsed) {
		LOG_WARNING("DISPLAY: Requested window_position '%s' was not in X,Y format; "
		            "using 'auto' instead",
		            window_position_val.c_str());
		return;
	}

	const auto desktop = get_desktop_resolution();
	const bool is_out_of_bounds = x < 0 || x > desktop.w || y < 0 ||
	                              y > desktop.h;
	if (is_out_of_bounds) {
		LOG_WARNING("DISPLAY: Requested window_position '%d,%d' is outside "
		            "the bounds of the desktop '%dx%d', "
		            "using 'auto' instead",
		            x,
		            y,
		            desktop.w,
		            desktop.h);
		return;
	}

	sdl.desktop.window.initial_x_pos = x;
	sdl.desktop.window.initial_y_pos = y;
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
	auto &window_canvas_size = sdl.desktop.window.canvas_size;
	if (window_canvas_size.w <= 0 || window_canvas_size.h <= 0) {
		window_canvas_size.w = w;
		window_canvas_size.h = h;
	}
}

// Takes in:
//  - The user's windowresolution: default, WxH, small, medium, large,
//    desktop, or an invalid setting.
//  - The previously configured scaling mode: Bilinear or NearestNeighbour.
//  - If aspect correction is requested.
//
// Except for SURFACE rendering, this function returns a refined size and
// additionally populates the following struct members:
//
//  - 'sdl.desktop.requested_window_bounds', with the coarse bounds, which do
//     not take into account scaling or aspect correction.
//  - 'sdl.desktop.window', with the refined size.
//  - 'sdl.desktop.want_resizable_window', if the window can be resized.
//
static void setup_window_sizes_from_conf(const char* windowresolution_val,
                                         const InterpolationMode interpolation_mode,
                                         const bool wants_aspect_ratio_correction)
{
	// Get the coarse resolution from the users setting, and adjust
	// refined scaling mode if an exact resolution is desired.
	const std::string pref = windowresolution_val;
	SDL_Point coarse_size  = FALLBACK_WINDOW_DIMENSIONS;

	sdl.use_exact_window_resolution = pref.find('x') != std::string::npos;
	if (sdl.use_exact_window_resolution) {
		coarse_size = parse_window_resolution_from_conf(pref);
	} else {
		const auto desktop = get_desktop_resolution();

		coarse_size = window_bounds_from_label(pref, desktop);
	}

	// Save the coarse bounds in the SDL struct for future sizing events
	sdl.desktop.requested_window_bounds = {coarse_size.x, coarse_size.y};

	// Refine the coarse resolution and save it in the SDL struct.
	auto refined_size = coarse_size;
	if (sdl.use_exact_window_resolution) {
		refined_size = clamp_to_minimum_window_dimensions(coarse_size);
	} else {
		refined_size = refine_window_size(coarse_size,
		                                  wants_aspect_ratio_correction);
	}
	assert(refined_size.x <= UINT16_MAX && refined_size.y <= UINT16_MAX);
	save_window_size(refined_size.x, refined_size.y);

	auto describe_interpolation_mode = [interpolation_mode]() -> const char* {
		switch (interpolation_mode) {
		case InterpolationMode::Bilinear: return "bilinear";
		case InterpolationMode::NearestNeighbour:
			return "nearest-neighbour";
		}
		return "unknown";
	};

	// Let the user know the resulting window properties
	LOG_MSG("DISPLAY: Initialised %dx%d windowed mode using %s scaling on display-%d",
	        refined_size.x,
	        refined_size.y,
	        describe_interpolation_mode(),
	        sdl.display_number);
}

SDL_Rect GFX_CalcViewport(const int canvas_width, const int canvas_height,
                          const int draw_width, const int draw_height,
                          const Fraction& render_pixel_aspect_ratio)
{
	assert(draw_width > 0);
	assert(draw_height > 0);

	const auto [draw_scale_x, draw_scale_y] = get_scale_factors_from_pixel_aspect_ratio(
	        render_pixel_aspect_ratio);

	assert(draw_scale_x > 0.0);
	assert(draw_scale_y > 0.0);
	assert(std::isfinite(draw_scale_x));
	assert(std::isfinite(draw_scale_y));

	// Limit the window to the user's desired viewport, if configured
	const auto restricted_dims = restrict_to_viewport_resolution(canvas_width,
	                                                             canvas_height);

	const auto bounds_w = restricted_dims.x;
	const auto bounds_h = restricted_dims.y;

	// It is important to calculate the image aspect ratio like this because
	// the aspect ratio of the *image* itself it not always 4:3, in which
	// case it would appear letter and/or pillarboxed on a real 4:3 display
	// aspect ratio CRT monitor.
	const auto image_aspect_ratio = (draw_width * draw_scale_x) /
	                                (draw_height * draw_scale_y);

	auto calculate_bounded_dims = [&]() -> std::pair<int, int> {
		// Calculate the viewport contingent on the aspect ratio of the
		// viewport bounds versus display mode.
		const auto bounds_aspect = static_cast<double>(bounds_w) / bounds_h;

		if (bounds_aspect > image_aspect_ratio) {
			const auto w = iround(bounds_h * image_aspect_ratio);
			const auto h = bounds_h;
			return {w, h};
		} else {
			const auto w = bounds_w;
			const auto h = iround(bounds_w / image_aspect_ratio);
			return {w, h};
		}
	};

	auto calculate_vertical_integer_scaling_dims = [&]() -> std::pair<int, int> {
		auto integer_scale_factor = std::min(
		        bounds_h / draw_height,
		        ifloor(bounds_w / (draw_height * image_aspect_ratio)));

		if (integer_scale_factor < 1) {
			// Revert to fit to viewport
			return calculate_bounded_dims();
		} else {
			const auto w = iround(draw_height * integer_scale_factor *
			                      image_aspect_ratio);
			const auto h = draw_height * integer_scale_factor;

			return {w, h};
		}
	};

	int view_w = 0;
	int view_h = 0;

	switch (sdl.integer_scaling_mode) {
	case IntegerScalingMode::Off: {
		std::tie(view_w, view_h) = calculate_bounded_dims();
		break;
	}
	case IntegerScalingMode::Auto:
#if C_OPENGL
		if (sdl.rendering_backend == RenderingBackend::OpenGl &&
		    sdl.opengl.shader_info.is_adaptive) {
			std::tie(view_w,
			         view_h) = calculate_vertical_integer_scaling_dims();
		} else {
			std::tie(view_w, view_h) = calculate_bounded_dims();
		}
#else
		std::tie(view_w, view_h) = calculate_bounded_dims();
#endif
		break;

	case IntegerScalingMode::Horizontal: {
		// Calculate scaling multiplier that will allow to fit the whole
		// image into the window or viewport bounds.
		const auto pixel_aspect = round(draw_height * draw_scale_y) /
		                          draw_height;
		auto integer_scale_factor =
		        std::min(bounds_w / draw_width,
		                 ifloor(bounds_h / (draw_height * pixel_aspect)));

		if (integer_scale_factor < 1) {
			// Revert to fit to viewport
			std::tie(view_w, view_h) = calculate_bounded_dims();
		} else {
			// Calculate the final viewport.
			view_w = draw_width * integer_scale_factor;
			view_h = iround(draw_height * integer_scale_factor *
			                pixel_aspect);
		}
		break;
	}
	case IntegerScalingMode::Vertical:
		std::tie(view_w, view_h) = calculate_vertical_integer_scaling_dims();
		break;

	default: assertm(false, "Invalid IntegerScalingMode value");
	}

	// Calculate centered viewport position.
	const int view_x = (canvas_width - view_w) / 2;
	const int view_y = (canvas_height - view_h) / 2;

	return {view_x, view_y, view_w, view_h};
}

static SDL_Rect calc_viewport(const int canvas_width, const int canvas_height)
{
	return GFX_CalcViewport(canvas_width,
	                        canvas_height,
	                        sdl.draw.width,
	                        sdl.draw.height,
	                        sdl.draw.render_pixel_aspect_ratio);
}

IntegerScalingMode GFX_GetIntegerScalingMode()
{
	return sdl.integer_scaling_mode;
}

void GFX_SetIntegerScalingMode(const IntegerScalingMode mode)
{
	sdl.integer_scaling_mode = mode;
}

InterpolationMode GFX_GetInterpolationMode()
{
	return sdl.interpolation_mode;
}

static void set_output(Section* sec, const bool wants_aspect_ratio_correction)
{
	// Apply the user's mouse settings
	const auto section = static_cast<const Section_prop *>(sec);
	std::string output = section->Get_string("output");

	GFX_DisengageRendering();
	// it's the job of everything after this to re-engage it.

	if (output == "texture") {
		sdl.want_rendering_backend = RenderingBackend::Texture;
		sdl.interpolation_mode = InterpolationMode::Bilinear;
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	} else if (output == "texturenb") {
		sdl.want_rendering_backend = RenderingBackend::Texture;
		sdl.interpolation_mode = InterpolationMode::NearestNeighbour;
		// Currently the default, but... oh well
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

#if C_OPENGL
	} else if (starts_with(output, "opengl")) {
		if (output == "opengl") {
			sdl.want_rendering_backend = RenderingBackend::OpenGl;
			sdl.interpolation_mode = InterpolationMode::Bilinear;
			sdl.opengl.bilinear    = true;

		} else if (output == "openglnb") {
			sdl.want_rendering_backend = RenderingBackend::OpenGl;
			sdl.interpolation_mode = InterpolationMode::NearestNeighbour;
			sdl.opengl.bilinear = false;
		}
#endif

	} else {
		LOG_WARNING("SDL: Unsupported output device '%s', using 'texture' output mode",
		            output.c_str());
		sdl.want_rendering_backend = RenderingBackend::Texture;
	}

	const std::string screensaver = section->Get_string("screensaver");
	if (screensaver == "allow")
		SDL_EnableScreenSaver();
	if (screensaver == "block")
		SDL_DisableScreenSaver();

	sdl.render_driver = section->Get_string("texture_renderer");
	lowcase(sdl.render_driver);
	if (sdl.render_driver != "auto") {
		if (SDL_SetHint(SDL_HINT_RENDER_DRIVER,
		                sdl.render_driver.c_str()) == SDL_FALSE) {
			LOG_WARNING("SDL: Failed to set '%s' texture renderer driver; "
			            "falling back to automatic selection",
			            sdl.render_driver.c_str());
		}
	}

	sdl.desktop.window.show_decorations = section->Get_bool("window_decorations");

	setup_initial_window_position_from_conf(
	        section->Get_string("window_position"));

	setup_viewport_resolution_from_conf(section->Get_string("viewport_resolution"));

	setup_window_sizes_from_conf(section->Get_string("windowresolution").c_str(),
	                             sdl.interpolation_mode,
	                             wants_aspect_ratio_correction);

#if C_OPENGL
	if (sdl.want_rendering_backend == RenderingBackend::OpenGl) { /* OPENGL is requested */
		if (!SetDefaultWindowMode()) {
			LOG_WARNING("OPENGL: Could not create OpenGL window, "
			            "using 'texture' output mode");
			sdl.want_rendering_backend = RenderingBackend::Texture;
		} else {
			sdl.opengl.context = SDL_GL_CreateContext(sdl.window);
			if (sdl.opengl.context == nullptr) {
				LOG_WARNING("OPENGL: Could not create context, "
				            "using 'texture' output mode");
				sdl.want_rendering_backend = RenderingBackend::Texture;
			}
		}

		if (sdl.want_rendering_backend == RenderingBackend::OpenGl) {
			sdl.opengl.program_object = 0;
			glAttachShader = (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress(
			        "glAttachShader");
			glCompileShader = (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress(
			        "glCompileShader");
			glCreateProgram = (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress(
			        "glCreateProgram");
			glCreateShader = (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress(
			        "glCreateShader");
			glDeleteProgram = (PFNGLDELETEPROGRAMPROC)SDL_GL_GetProcAddress(
			        "glDeleteProgram");
			glDeleteShader = (PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress(
			        "glDeleteShader");
			glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)
			        SDL_GL_GetProcAddress("glEnableVertexAttribArray");
			glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)
			        SDL_GL_GetProcAddress("glGetAttribLocation");
			glGetProgramiv = (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress(
			        "glGetProgramiv");
			glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)
			        SDL_GL_GetProcAddress("glGetProgramInfoLog");
			glGetShaderiv = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress(
			        "glGetShaderiv");
			glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)
			        SDL_GL_GetProcAddress("glGetShaderInfoLog");
			glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)
			        SDL_GL_GetProcAddress("glGetUniformLocation");
			glLinkProgram = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress(
			        "glLinkProgram");
			glShaderSource = (PFNGLSHADERSOURCEPROC_NP)SDL_GL_GetProcAddress(
			        "glShaderSource");
			glUniform2f = (PFNGLUNIFORM2FPROC)SDL_GL_GetProcAddress(
			        "glUniform2f");
			glUniform1i = (PFNGLUNIFORM1IPROC)SDL_GL_GetProcAddress(
			        "glUniform1i");
			glUseProgram = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress(
			        "glUseProgram");
			glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)
			        SDL_GL_GetProcAddress("glVertexAttribPointer");
			sdl.opengl.use_shader =
			        (glAttachShader && glCompileShader &&
			         glCreateProgram && glDeleteProgram &&
			         glDeleteShader && glEnableVertexAttribArray &&
			         glGetAttribLocation && glGetProgramiv &&
			         glGetProgramInfoLog && glGetShaderiv &&
			         glGetShaderInfoLog && glGetUniformLocation &&
			         glLinkProgram && glShaderSource &&
			         glUniform2f && glUniform1i && glUseProgram &&
			         glVertexAttribPointer);

			sdl.opengl.framebuf = nullptr;
			sdl.opengl.texture = 0;
			sdl.opengl.displaylist = 0;
			glGetIntegerv(GL_MAX_TEXTURE_SIZE, &sdl.opengl.max_texsize);

			const auto gl_version_string = safe_gl_get_string(GL_VERSION,
			                                                  "0.0.0");
			const int gl_version_major = gl_version_string[0] - '0';

			sdl.opengl.npot_textures_supported =
			        gl_version_major >= 2 ||
			        SDL_GL_ExtensionSupported(
			                "GL_ARB_texture_non_power_of_two");

			std::string npot_support_msg = sdl.opengl.npot_textures_supported
			                                     ? "supported"
			                                     : "not supported";

			LOG_INFO("OPENGL: Vendor: %s",
			         safe_gl_get_string(GL_VENDOR, "unknown"));

			LOG_INFO("OPENGL: Version: %s", gl_version_string);

			LOG_INFO("OPENGL: GLSL version: %s",
			         safe_gl_get_string(GL_SHADING_LANGUAGE_VERSION,
			                            "unknown"));

			LOG_INFO("OPENGL: NPOT textures %s",
			         npot_support_msg.c_str());
		}
	} /* OPENGL is requested end */
#endif    // OPENGL

	if (!SetDefaultWindowMode())
		E_Exit("SDL: Could not initialize video: %s", SDL_GetError());

	const auto transparency = clamp(section->Get_int("transparency"), 0, 90);
	const auto alpha = static_cast<float>(100 - transparency) / 100.0f;
	SDL_SetWindowOpacity(sdl.window, alpha);
	SDL_SetWindowTitle(sdl.window, APP_NAME_STR);
	SetIcon();

	RENDER_Reinit();
}

// extern void UI_Run(bool);
void Restart(bool pressed);

static void ApplyActiveSettings()
{
	SetPriority(sdl.priority.active);
	MOUSE_NotifyWindowActive(true);

	if (sdl.mute_when_inactive && !MIXER_IsManuallyMuted()) {
		MIXER_Unmute();
	}
}

static void ApplyInactiveSettings()
{
	SetPriority(sdl.priority.inactive);
	MOUSE_NotifyWindowActive(false);

	if (sdl.mute_when_inactive) {
		MIXER_Mute();
	}
}

static void SetPriorityLevels(const std::string& active_pref,
                              const std::string& inactive_pref)
{
	auto to_level = [](const std::string& pref) {
		if (pref == "auto")
			return PRIORITY_LEVEL_AUTO;
		if (pref == "lowest")
			return PRIORITY_LEVEL_LOWEST;
		if (pref == "lower")
			return PRIORITY_LEVEL_LOWER;
		if (pref == "normal")
			return PRIORITY_LEVEL_NORMAL;
		if (pref == "higher")
			return PRIORITY_LEVEL_HIGHER;
		if (pref == "highest")
			return PRIORITY_LEVEL_HIGHEST;

		LOG_WARNING("Invalid priority level: %s, using 'auto'", pref.data());
		return PRIORITY_LEVEL_AUTO;
	};

	sdl.priority.active   = to_level(active_pref);
	sdl.priority.inactive = to_level(inactive_pref);
}

static void GUI_StartUp(Section *sec)
{
	sec->AddDestroyFunction(&GUI_ShutDown);
	Section_prop *section = static_cast<Section_prop *>(sec);

	sdl.active = false;
	sdl.updating        = false;
	sdl.resizing_window = false;
	sdl.wait_on_error = section->Get_bool("waitonerror");

	sdl.desktop.fullscreen = control->arguments.fullscreen ||
	                         section->Get_bool("fullscreen");

	auto priority_conf = section->GetMultiVal("priority")->GetSection();
	SetPriorityLevels(priority_conf->Get_string("active"),
	                  priority_conf->Get_string("inactive"));

	sdl.pause_when_inactive = section->Get_bool("pause_when_inactive");

	sdl.mute_when_inactive = section->Get_bool("mute_when_inactive") ||
	                         sdl.pause_when_inactive;

	ApplyActiveSettings(); // Assume focus on startup
	sdl.desktop.full.fixed = false;
	std::string fullresolution = section->Get_string("fullresolution");
	sdl.desktop.full.width  = 0;
	sdl.desktop.full.height = 0;
	if(!fullresolution.empty()) {
		lowcase(fullresolution); //so x and X are allowed
		if (fullresolution != "original") {
			sdl.desktop.full.fixed = true;
			if (fullresolution != "desktop") { // desktop uses 0x0, below sets a custom WxH
				std::vector<std::string> dimensions = split_with_empties(fullresolution, 'x');
				if (dimensions.size() == 2) {
					sdl.desktop.full.width = parse_int(dimensions[0]).value_or(0);
					sdl.desktop.full.height = parse_int(dimensions[1]).value_or(0);
					maybe_limit_requested_resolution(
					        sdl.desktop.full.width,
					        sdl.desktop.full.height,
					        "fullscreen");
				}
			}
		}
	}

	const std::string host_rate_pref = section->Get_string("host_rate");
	if (host_rate_pref == "auto") {
		sdl.desktop.host_rate_mode = HostRateMode::Auto;
	} else if (host_rate_pref == "sdi") {
		sdl.desktop.host_rate_mode = HostRateMode::Sdi;
	} else if (host_rate_pref == "vrr") {
		sdl.desktop.host_rate_mode = HostRateMode::Vrr;
	} else {
		const auto rate = to_finite<double>(host_rate_pref);
		if (std::isfinite(rate) && rate >= RefreshRateMin) {
			sdl.desktop.host_rate_mode      = HostRateMode::Custom;
			sdl.desktop.preferred_host_rate = rate;
		} else {
			LOG_WARNING("SDL: Invalid 'host_rate' value: '%s', using 'auto'",
			            host_rate_pref.c_str());
			sdl.desktop.host_rate_mode = HostRateMode::Auto;
		}
	}

	sdl.vsync.skip_us = section->Get_int("vsync_skip");

	render_pacer = std::make_unique<Pacer>("Render",
	                                       sdl.vsync.skip_us,
	                                       Pacer::LogLevel::TIMEOUTS);

	const int display = section->Get_int("display");
	if ((display >= 0) && (display < SDL_GetNumVideoDisplays())) {
		sdl.display_number = display;
	} else {
		LOG_WARNING("SDL: Display number out of bounds, using display 0");
		sdl.display_number = 0;
	}

	const std::string presentation_mode_pref = section->Get_string(
	        "presentation_mode");
	if (presentation_mode_pref == "auto")
		sdl.frame.desired_mode = FrameMode::Unset;
	else if (presentation_mode_pref == "cfr")
		sdl.frame.desired_mode = FrameMode::Cfr;
	else if (presentation_mode_pref == "vfr")
		sdl.frame.desired_mode = FrameMode::Vfr;
	else {
		sdl.frame.desired_mode = FrameMode::Unset;
		LOG_WARNING("SDL: Invalid 'presentation_mode' value: '%s'",
		            presentation_mode_pref.c_str());
	}

	sdl.desktop.full.display_res = sdl.desktop.full.fixed && (!sdl.desktop.full.width || !sdl.desktop.full.height);
	if (sdl.desktop.full.display_res) {
		GFX_ObtainDisplayDimensions();
	}

	set_output(section, RENDER_IsAspectRatioCorrectionEnabled());

	/* Get some Event handlers */
	MAPPER_AddHandler(GFX_RequestExit, SDL_SCANCODE_F9, PRIMARY_MOD,
	                  "shutdown", "Shutdown");

	MAPPER_AddHandler(SwitchFullScreen, SDL_SCANCODE_RETURN, MMOD2, "fullscr", "Fullscreen");
	MAPPER_AddHandler(Restart, SDL_SCANCODE_HOME, MMOD1 | MMOD2, "restart", "Restart");
	MAPPER_AddHandler(MOUSE_ToggleUserCapture,
	                  SDL_SCANCODE_F10,
	                  PRIMARY_MOD,
	                  "capmouse",
	                  "Cap Mouse");

#if C_DEBUG
/* Pause binds with activate-debugger */
#else
	MAPPER_AddHandler(&PauseDOSBox, SDL_SCANCODE_PAUSE, MMOD2,
	                  "pause", "Pause Emu.");
#endif
	/* Get Keyboard state of numlock and capslock */
	SDL_Keymod keystate = SDL_GetModState();

	// A long-standing SDL1 and SDL2 bug prevents it from detecting the
	// numlock and capslock states on startup. Instead, these states must
	// be toggled by the user /after/ starting DOSBox.
	startup_state_numlock = keystate & KMOD_NUM;
	startup_state_capslock = keystate & KMOD_CAPS;

	// Notify MOUSE subsystem that it can start now
	MOUSE_NotifyReadyGFX();
}

static void HandleMouseMotion(SDL_MouseMotionEvent *motion)
{
	MOUSE_EventMoved(static_cast<float>(motion->xrel),
	                 static_cast<float>(motion->yrel),
	                 check_cast<int32_t>(motion->x),
	                 check_cast<int32_t>(motion->y));
}

static void HandleMouseWheel(SDL_MouseWheelEvent *wheel)
{
    const auto tmp = (wheel->direction == SDL_MOUSEWHEEL_NORMAL) ? -wheel->y : wheel->y;
	MOUSE_EventWheel(check_cast<int16_t>(tmp));
}

static void HandleMouseButton(SDL_MouseButtonEvent * button)
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

bool GFX_IsFullscreen() {
	return sdl.desktop.fullscreen;
}

void GFX_RegenerateWindow(Section *sec) {
	if (first_window) {
		first_window = false;
		return;
	}
	remove_window();
	set_output(sec, RENDER_IsAspectRatioCorrectionEnabled());
	GFX_ResetScreen();
}

#if defined(MACOSX)
#define DB_POLLSKIP 3
#else
//Not used yet, see comment below
#define DB_POLLSKIP 1
#endif

static void HandleVideoResize(int width, int height)
{
	/* Maybe a screen rotation has just occurred, so we simply resize.
	There may be a different cause for a forced resized, though.    */
	if (sdl.desktop.full.display_res && sdl.desktop.fullscreen) {
		/* Note: We should not use GFX_ObtainDisplayDimensions
		(SDL_GetDisplayBounds) on Android after a screen rotation:
		The older values from application startup are returned. */
		sdl.desktop.full.width = width;
		sdl.desktop.full.height = height;
	}

	const auto canvas = get_canvas_size(sdl.rendering_backend);
	sdl.clip          = calc_viewport(canvas.w, canvas.h);
	if (sdl.rendering_backend == RenderingBackend::Texture) {
		SDL_RenderSetViewport(sdl.renderer, &sdl.clip);
	}
#if C_OPENGL
	if (sdl.rendering_backend == RenderingBackend::OpenGl) {
		glViewport(sdl.clip.x, sdl.clip.y, sdl.clip.w, sdl.clip.h);
		glUniform2f(sdl.opengl.ruby.output_size,
		            (GLfloat)sdl.clip.w,
		            (GLfloat)sdl.clip.h);
	}
#endif // C_OPENGL

	if (!sdl.desktop.fullscreen) {
		// If the window was resized, it might have been
		// triggered by the OS setting DPI scale, so recalculate
		// that based on the incoming logical width.
		check_and_handle_dpi_change(sdl.window, sdl.rendering_backend, width);
	}

	// Ensure mouse emulation knows the current parameters
	NewMouseScreenParams();
}

/* TODO: Properly set window parameters and remove this routine.
 *
 * This function is triggered after window is shown to fixup sdl.window
 * properties in predictable manner on all platforms.
 *
 * In specific usecases, certain sdl.window properties might be left unitialized
 * when starting in fullscreen, which might trigger severe problems for end
 * users (e.g. placing window partially off-screen, or using fullscreen
 * resolution for window size).
 */
static void FinalizeWindowState()
{
	assert(sdl.window);

	if (!sdl.desktop.lazy_init_window_size)
		return;

	// Don't change window position or size when state changed to
	// fullscreen.
	if (sdl.desktop.fullscreen)
		return;

	// Once window is fixed up once, OS or Window Manager will deal with it
	// on future expose events.
	sdl.desktop.lazy_init_window_size = false;

	safe_set_window_size(sdl.desktop.window.width, sdl.desktop.window.height);

	// Force window position when dosbox is configured to start in
	// fullscreen.  Otherwise SDL will reset window position to 0,0 when
	// switching to a window for the first time. This is happening on every
	// OS, but only on Windows it's a really big problem (because window
	// decorations are rendered off-screen).
	const auto default_val = SDL_WINDOWPOS_CENTERED_DISPLAY(sdl.display_number);
	const auto pos = get_initial_window_position_or_default(default_val);
	SDL_SetWindowPosition(sdl.window, pos.x, pos.y);

	// In some cases (not always), leaving fullscreen breaks window content,
	// screen reset restores rendering to the working state.
	GFX_ResetScreen();
}

static void maybe_auto_switch_shader()
{
#if C_OPENGL
	if (sdl.rendering_backend != RenderingBackend::OpenGl) {
		return;
	}

	const auto canvas = get_canvas_size(sdl.rendering_backend);

	constexpr auto reinit_render = true;
	RENDER_MaybeAutoSwitchShader(canvas.w, canvas.h, sdl.video_mode, reinit_render);
#endif
}

bool GFX_Events()
{
#if defined(MACOSX)
	// Don't poll too often. This can be heavy on the OS, especially Macs.
	// In idle mode 3000-4000 polls are done per second without this check.
	// Macs, with this code,  max 250 polls per second. (non-macs unused
	// default max 500). Currently not implemented for all platforms, given
	// the ALT-TAB stuff for WIN32.
	static auto last_check = GetTicks();
	auto current_check = GetTicks();
	if (GetTicksDiff(current_check, last_check) <= DB_POLLSKIP)
		return true;
	last_check = current_check;
#endif

	SDL_Event event;
#if defined (REDUCE_JOYSTICK_POLLING)
	static auto last_check_joystick = GetTicks();
	auto current_check_joystick = GetTicks();
	if (GetTicksDiff(current_check_joystick, last_check_joystick) > 20) {
		last_check_joystick = current_check_joystick;
		if (MAPPER_IsUsingJoysticks()) SDL_JoystickUpdate();
		MAPPER_UpdateJoysticks();
	}
#endif
	while (SDL_PollEvent(&event)) {
#if C_DEBUG
		if (is_debugger_event(event)) {
			pdc_event_queue.push(event);
			continue;
		}
#endif
		switch (event.type) {
		case SDL_DISPLAYEVENT:
			switch (event.display.event) {
#if (SDL_MAJOR_VERSION > 2 || SDL_MINOR_VERSION > 0 || SDL_PATCHLEVEL >= 14)
			// Events added in SDL 2.0.14
			case SDL_DISPLAYEVENT_CONNECTED:
			case SDL_DISPLAYEVENT_DISCONNECTED:
				NewMouseScreenParams();
				break;
#endif
			default:
				break;
			};
			break;
		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_RESTORED:
				// LOG_DEBUG("SDL: Window has been restored");
				/* We may need to re-create a texture
				 * and more on Android. Another case:
				 * Update surface while using X11.
				 */
				GFX_ResetScreen();
#if C_OPENGL && defined(MACOSX)
				// LOG_DEBUG("SDL: Reset macOS's GL viewport
				// after window-restore");
				if (sdl.rendering_backend == RenderingBackend::OpenGl) {
					glViewport(sdl.clip.x,
					           sdl.clip.y,
					           sdl.clip.w,
					           sdl.clip.h);
				}
#endif
				FocusInput();
				continue;

			case SDL_WINDOWEVENT_RESIZED:
				// LOG_DEBUG("SDL: Window has been resized to %dx%d",
				//               event.window.data1,
				//               event.window.data2);
				//
				// When going from an initial fullscreen to
				// windowed state, this event will be called
				// moments before SDL's windowed mode is
				// engaged, so simply ensure the window size has
				// already been established:
				assert(sdl.desktop.window.width > 0 &&
				       sdl.desktop.window.height > 0);
				continue;

			case SDL_WINDOWEVENT_FOCUS_GAINED:
				ApplyActiveSettings();
				[[fallthrough]];
			case SDL_WINDOWEVENT_EXPOSED:
				// LOG_DEBUG("SDL: Window has been exposed "
				//               "and should be redrawn");

				/* TODO: below is not consistently true :(
				 * seems incorrect on KDE and sometimes on MATE
				 *
				 * Note that on Windows/Linux-X11/Wayland/macOS,
				 * event is fired after toggling between full vs
				 * windowed modes. However this is never fired
				 * on the Raspberry Pi (when rendering to the
				 * Framebuffer); therefore we rely on the
				 * FOCUS_GAINED event to catch window startup
				 * and size toggles.
				 */
				// LOG_DEBUG("SDL: Window has gained
				// keyboard focus");
				if (sdl.draw.callback)
					sdl.draw.callback(GFX_CallBackRedraw);
				FocusInput();
				continue;

			case SDL_WINDOWEVENT_FOCUS_LOST:
				// LOG_DEBUG("SDL: Window has lost keyboard focus");
#ifdef WIN32
				if (sdl.desktop.fullscreen) {
					VGA_KillDrawing();
					GFX_ForceFullscreenExit();
				}
#endif
				ApplyInactiveSettings();
				GFX_LosingFocus();
				break;

			case SDL_WINDOWEVENT_ENTER:
				// LOG_DEBUG("SDL: Window has gained mouse focus");
				continue;

			case SDL_WINDOWEVENT_LEAVE:
				// LOG_DEBUG("SDL: Window has lost mouse focus");
				continue;

			case SDL_WINDOWEVENT_SHOWN:
				// LOG_DEBUG("SDL: Window has been shown");
				maybe_auto_switch_shader();
				continue;

			case SDL_WINDOWEVENT_HIDDEN:
				// LOG_DEBUG("SDL: Window has been hidden");
				continue;

#if C_OPENGL && defined(MACOSX)
			case SDL_WINDOWEVENT_MOVED:
				// LOG_DEBUG("SDL: Window has been moved to %d, %d",
				//               event.window.data1,
				//               event.window.data2);
				if (sdl.rendering_backend == RenderingBackend::OpenGl) {
					glViewport(sdl.clip.x,
					           sdl.clip.y,
					           sdl.clip.w,
					           sdl.clip.h);
				}
				continue;
#endif

#if SDL_VERSION_ATLEAST(2, 0, 18)
			case SDL_WINDOWEVENT_DISPLAY_CHANGED: {
				// New display might have a different resolution
				// and DPI scaling set, so recalculate that and
				// set viewport
				check_and_handle_dpi_change(sdl.window,
				                            sdl.rendering_backend);

				SDL_Rect display_bounds = {};
				SDL_GetDisplayBounds(event.window.data1,
				                     &display_bounds);
				sdl.desktop.full.width  = display_bounds.w;
				sdl.desktop.full.height = display_bounds.h;

				sdl.display_number = event.window.data1;

				const auto canvas = get_canvas_size(sdl.rendering_backend);
				sdl.clip = calc_viewport(canvas.w, canvas.h);
				if (sdl.rendering_backend == RenderingBackend::Texture) {
					SDL_RenderSetViewport(sdl.renderer,
					                      &sdl.clip);
				}
#	if C_OPENGL
				if (sdl.rendering_backend == RenderingBackend::OpenGl) {
					glViewport(sdl.clip.x,
					           sdl.clip.y,
					           sdl.clip.w,
					           sdl.clip.h);
				}

				maybe_auto_switch_shader();
#	endif
				NewMouseScreenParams();
				continue;
			}
#endif

			case SDL_WINDOWEVENT_SIZE_CHANGED:
				// LOG_DEBUG("SDL: The window size has changed");

				// The window size has changed either as a
				// result of an API call or through the system
				// or user changing the window size.
				HandleVideoResize(event.window.data1, event.window.data2);
				FinalizeWindowState();
				maybe_auto_switch_shader();
				continue;

			case SDL_WINDOWEVENT_MINIMIZED:
				// LOG_DEBUG("SDL: Window has been minimized");
				ApplyInactiveSettings();
				break;

			case SDL_WINDOWEVENT_MAXIMIZED:
				// LOG_DEBUG("SDL: Window has been maximized");
				continue;

			case SDL_WINDOWEVENT_CLOSE:
				// LOG_DEBUG("SDL: The window manager "
				//               "requests that the window be "
				//               "closed");
				GFX_RequestExit(true);
				break;

			case SDL_WINDOWEVENT_TAKE_FOCUS:
				FocusInput();
				ApplyActiveSettings();
				continue;

			case SDL_WINDOWEVENT_HIT_TEST:
				// LOG_DEBUG("SDL: Window had a hit test that "
				//               "wasn't SDL_HITTEST_NORMAL");
				continue;

			default: break;
			}

			/* Non-focus priority is set to pause; check to see if we've lost window or input focus
			 * i.e. has the window been minimised or made inactive?
			 */
			if (sdl.pause_when_inactive) {
				if ((event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) || (event.window.event == SDL_WINDOWEVENT_MINIMIZED)) {
					/* Window has lost focus, pause the emulator.
					 * This is similar to what PauseDOSBox() does, but the exit criteria is different.
					 * Instead of waiting for the user to hit Alt+Break, we wait for the window to
					 * regain window or input focus.
					 */
					bool paused = true;
					ApplyInactiveSettings();
					SDL_Event ev;

					GFX_RefreshTitle();
					KEYBOARD_ClrBuffer();
//					Delay(500);
//					while (SDL_PollEvent(&ev)) {
						// flush event queue.
//					}

					while (paused && !shutdown_requested) {
						// WaitEvent waits for an event rather than polling, so CPU usage drops to zero
						SDL_WaitEvent(&ev);

						switch (ev.type) {
						case SDL_QUIT:
							GFX_RequestExit(true);
							break;
						case SDL_WINDOWEVENT:     // wait until we get window focus back
							if ((ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) || (ev.window.event == SDL_WINDOWEVENT_MINIMIZED) || (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) || (ev.window.event == SDL_WINDOWEVENT_RESTORED) || (ev.window.event == SDL_WINDOWEVENT_EXPOSED)) {
								// We've got focus back, so unpause and break out of the loop
								if ((ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) || (ev.window.event == SDL_WINDOWEVENT_RESTORED) || (ev.window.event == SDL_WINDOWEVENT_EXPOSED)) {
									GFX_RefreshTitle();
									if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
										paused = false;
										ApplyActiveSettings();
									}
								}

								/* Now poke a "release ALT" command into the keyboard buffer
								 * we have to do this, otherwise ALT will 'stick' and cause
								 * problems with the app running in the DOSBox.
								 */
								KEYBOARD_AddKey(KBD_leftalt, false);
								KEYBOARD_AddKey(KBD_rightalt, false);
								if (ev.window.event == SDL_WINDOWEVENT_RESTORED) {
									// We may need to re-create a texture and more
									GFX_ResetScreen();
								}
							}
							break;
						}
					}
				}
			}
			break; // end of SDL_WINDOWEVENT

		case SDL_MOUSEMOTION: HandleMouseMotion(&event.motion); break;
		case SDL_MOUSEWHEEL: HandleMouseWheel(&event.wheel); break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP: HandleMouseButton(&event.button); break;

		case SDL_QUIT: GFX_RequestExit(true); break;
#ifdef WIN32
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			// ignore event alt+tab
			if (event.key.keysym.sym == SDLK_LALT)
				sdl.laltstate = (SDL_EventType)event.key.type;
			if (event.key.keysym.sym == SDLK_RALT)
				sdl.raltstate = (SDL_EventType)event.key.type;
			if (((event.key.keysym.sym==SDLK_TAB)) && ((sdl.laltstate==SDL_KEYDOWN) || (sdl.raltstate==SDL_KEYDOWN)))
				break;
			// This can happen as well.
			if (((event.key.keysym.sym == SDLK_TAB )) && (event.key.keysym.mod & KMOD_ALT)) break;
			// Ignore tab events that arrive just after regaining
			// focus. Likely the result of Alt+Tab.
			if ((event.key.keysym.sym == SDLK_TAB) &&
			    (GetTicksSince(sdl.focus_ticks) < 2))
				break;
			[[fallthrough]];
#endif
#if defined (MACOSX)
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			/* On macs CMD-Q is the default key to close an application */
			if (event.key.keysym.sym == SDLK_q &&
			    (event.key.keysym.mod == KMOD_RGUI || event.key.keysym.mod == KMOD_LGUI)) {
				GFX_RequestExit(true);
				break;
			}
			[[fallthrough]];
#endif
		default: MAPPER_CheckEvent(&event);
		}
	}
	return !shutdown_requested;
}

#if defined (WIN32)
static BOOL WINAPI console_event_handler(DWORD event) {
	switch (event) {
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
		raise(SIGTERM);
		return TRUE;
	case CTRL_C_EVENT:
	default: //pass to the next handler
		return FALSE;
	}
}
#endif


/* static variable to show wether there is not a valid stdout.
 * Fixes some bugs when -noconsole is used in a read only directory */
static bool no_stdout = false;

void GFX_ShowMsg(const char* format, ...)
{
	char buf[512];

	va_list msg;
	va_start(msg,format);
	safe_sprintf(buf, format, msg);
	va_end(msg);

	buf[sizeof(buf) - 1] = '\0';
	if (!no_stdout) puts(buf); //Else buf is parsed again. (puts adds end of line)
}

static std::vector<std::string> get_sdl_texture_renderers()
{
	const int n = SDL_GetNumRenderDrivers();
	std::vector<std::string> drivers;
	drivers.reserve(n + 1);
	drivers.push_back("auto");
	SDL_RendererInfo info;
	for (int i = 0; i < n; i++) {
		if (SDL_GetRenderDriverInfo(i, &info))
			continue;
		if (info.flags & SDL_RENDERER_TARGETTEXTURE)
			drivers.push_back(info.name);
	}
	return drivers;
}

static void messages_add_sdl()
{
	MSG_Add("DOSBOX_HELP",
	        "Usage: %s [OPTION]... [PATH]\n"
	        "\n"
	        "PATH                       If PATH is a directory, it's mounted as C:.\n"
	        "                           If PATH is a bootable disk image (IMA/IMG), it's booted.\n"
	        "                           If PATH is a CDROM image (CUE/ISO), it's mounted as D:.\n"
	        "                           If PATH is a DOS executable (BAT/COM/EXE), it's parent\n"
	        "                           path is mounted as C: and the executable is run. When\n"
	        "                           the executable exits, DOSBox Staging quits.\n"
	        "\n"
	        "List of available options:\n"
	        "\n"
	        "  --conf <config_file>     Start with the options specified in <config_file>.\n"
	        "                           Multiple configuration files can be specified.\n"
	        "                           Example: --conf conf1.conf --conf conf2.conf\n"
	        "\n"
	        "  --printconf              Print the location of the primary configuration file.\n"
	        "\n"
	        "  --editconf               Open the primary configuration file in a text editor.\n"
	        "\n"
	        "  --eraseconf              Delete the primary configuration file.\n"
	        "\n"
	        "  --noprimaryconf          Don't load or create the primary configuration file.\n"
	        "\n"
	        "  --nolocalconf            Don't load the local 'dosbox.conf' configuration file\n"
	        "                           if present in the current working directory.\n"
	        "\n"
	        "  --set <setting>=<value>  Set a configuration setting. Multiple configuration\n"
	        "                           settings can be specified. Example:\n"
	        "                           --set mididevice=fluidsynth --set soundfont=mysoundfont.sf2\n"
	        "\n"
	        "  --working-dir <path>     Set working directory to <path>. DOSBox will act as if\n"
	        "                           started from this directory.\n"
	        "\n"
	        "  --list-glshaders         List all available OpenGL shaders and their paths.\n"
	        "                           Results are useable in the 'glshader = ' config setting.\n"
	        "\n"
	        "  --fullscreen             Start in fullscreen mode.\n"
	        "\n"
	        "  --lang <lang_file>       Start with the language specified in <lang_file>.\n"
	        "\n"
	        "  --machine <type>         Emulate a specific type of machine. The machine type has\n"
	        "                           influence on both the emulated video and sound cards.\n"
	        "                           Valid choices are: hercules, cga, cga_mono, tandy,\n"
	        "                           pcjr, ega, svga_s3 (default), svga_et3000, svga_et4000,\n"
	        "                           svga_paradise, vesa_nolfb, vesa_oldvbe.\n"
	        "\n"
	        "  -c <command>             Run the specified DOS command before handling the PATH.\n"
	        "                           Multiple commands can be specified.\n"
	        "\n"
	        "  --noautoexec             Don't run DOS commands from any [autoexec] sections.\n"
	        "\n"
	        "  --exit                   Exit after running '-c <command>'s and [autoexec] sections.\n"
	        "\n"
	        "  --startmapper            Run the mapper GUI.\n"
	        "\n"
	        "  --erasemapper            Delete the default mapper file.\n"
	        "\n"
	        "  --securemode             Enable secure mode by disabling the MOUNT and IMGMOUNT\n"
	        "                           commands.\n"
	        "\n"
	        "  --socket <num>           Run nullmodem on the specified socket number.\n"
	        "\n"
	        "  -h, -?, --help           Print help message and exit.\n"
	        "\n"
	        "  -V, --version            Print version information and exit.\n");

	MSG_Add("PROGRAM_CONFIG_PROPERTY_ERROR", "No such section or property: %s\n");

	MSG_Add("PROGRAM_CONFIG_NO_PROPERTY",
	        "There is no property '%s' in section [%s]\n");

	MSG_Add("PROGRAM_CONFIG_SET_SYNTAX",
	        "Usage: [color=light-green]config [reset]-set [color=light-cyan][SECTION][reset] "
	        "[color=white]PROPERTY[reset][=][color=white]VALUE[reset]\n");

	MSG_Add("TITLEBAR_CYCLES_MS",    "cycles/ms");
	MSG_Add("TITLEBAR_HINT_PAUSED",  "(PAUSED)");
	MSG_Add("TITLEBAR_HINT_NOMOUSE", "no-mouse mode");
	MSG_Add("TITLEBAR_HINT_CAPTURED_HOTKEY",
	        "mouse captured, %s+F10 to release");
	MSG_Add("TITLEBAR_HINT_CAPTURED_HOTKEY_MIDDLE",
	        "mouse captured, %s+F10 or middle-click to release");
	MSG_Add("TITLEBAR_HINT_RELEASED_HOTKEY",
	        "to capture the mouse press %s+F10");
	MSG_Add("TITLEBAR_HINT_RELEASED_HOTKEY_MIDDLE",
	        "to capture the mouse press %s+F10 or middle-click");
	MSG_Add("TITLEBAR_HINT_RELEASED_HOTKEY_ANY_BUTTON",
	        "to capture the mouse press %s+F10 or click any button");
	MSG_Add("TITLEBAR_HINT_SEAMLESS_HOTKEY",
	        "seamless mouse, %s+F10 to capture");
	MSG_Add("TITLEBAR_HINT_SEAMLESS_HOTKEY_MIDDLE",
	        "seamless mouse, %s+F10 or middle-click to capture");
}

static void config_add_sdl() {
	Section_prop *sdl_sec=control->AddSection_prop("sdl", &GUI_StartUp);
	sdl_sec->AddInitFunction(&MAPPER_StartUp);
	Prop_bool *Pbool; // use pbool for new properties
	Prop_bool *pbool;
	Prop_string *Pstring; // use pstring for new properties
	Prop_string *pstring;
	Prop_int *Pint; // use pint for new properties
	Prop_int *pint;
	PropMultiVal* Pmulti;
	Section_prop* psection;

	constexpr auto always = Property::Changeable::Always;
	constexpr auto deprecated = Property::Changeable::Deprecated;
	constexpr auto on_start = Property::Changeable::OnlyAtStart;

	Pbool = sdl_sec->Add_bool("fullscreen", always, false);
	Pbool->Set_help("Start directly in fullscreen (disabled by default).\n"
	                "Run INTRO and see Special Keys for window control hotkeys.");

	pint = sdl_sec->Add_int("display", on_start, 0);
	pint->Set_help("Number of display to use; values depend on OS and user "
	               "settings (0 by default).");

	Pstring = sdl_sec->Add_string("fullresolution", always, "desktop");
	Pstring->Set_help("What resolution to use for fullscreen: 'original', 'desktop'\n"
	                  "or a fixed size, e.g. 1024x768 ('desktop' by default).");

	pstring = sdl_sec->Add_string("windowresolution", on_start, "default");
	pstring->Set_help(
	        "Set window size when running in windowed mode:\n"
	        "  default:   Select the best option based on your environment and\n"
	        "             other settings.\n"
	        "  small, medium, large (s, m, l):\n"
	        "             Size the window relative to the desktop.\n"
	        "  <custom>:  Scale the window to the given dimensions in WxH format.\n"
	        "             For example: 1024x768.");

	pstring = sdl_sec->Add_path("viewport_resolution", always, "fit");
	pstring->Set_help(
	        "Set the viewport size (drawable area) within the window/screen:\n"
	        "  fit:       Fit the viewport to the available window/screen (default).\n"
	        "  <custom>:  Limit the viewport within to a custom resolution or percentage of\n"
	        "             the desktop. Specified in WxH, N%, N.M%. Examples: 960x720 or 50%");

	pstring = sdl_sec->Add_string("window_position", always, "auto");
	pstring->Set_help(
	        "Set initial window position when running in windowed mode:\n"
	        "  auto:      Let the window manager decide the position (default).\n"
	        "  <custom>:  Set window position in X,Y format. For example: 250,100\n"
	        "             0,0 is the top-left corner of the screen.");

	Pbool = sdl_sec->Add_bool("window_decorations", always, true);
	Pbool->Set_help("Enable window decorations in windowed mode (enabled by default).");

	Pint = sdl_sec->Add_int("transparency", always, 0);
	Pint->Set_help("Set the transparency of the DOSBox Staging screen (0 by default).\n"
	               "From 0 (no transparency) to 90 (high transparency).");

	pstring = sdl_sec->Add_path("max_resolution", deprecated, "");
	pstring->Set_help("Renamed to 'viewport_resolution'.");

	pstring = sdl_sec->Add_path("viewport_resolution", deprecated, "");
	pstring->Set_help("Moved to [render] section.");

	pstring = sdl_sec->Add_string("host_rate", on_start, "auto");
	pstring->Set_help(
	        "Set the host's refresh rate:\n"
	        "  auto:      Use SDI rates, or VRR rates when fullscreen on a high-refresh\n"
	        "             display (default).\n"
	        "  sdi:       Use serial device interface (SDI) rates, without further\n"
	        "             adjustment.\n"
	        "  vrr:       Deduct 3 Hz from the reported rate (best practice for VRR\n"
	        "             displays).\n"
	        "  <custom>:  Specify a custom rate as an integer or decimal Hz value\n"
	        "             (23.000 is the allowed minimum).");

	const char* vsync_prefs[] = {"auto", "on", "adaptive", "off", "yield", nullptr};
	pstring = sdl_sec->Add_string("vsync", always, "auto");

	pstring->Set_help(
	        "Set the host video driver's vertical synchronization (vsync) mode:\n"
	        "  auto:      Limit vsync to beneficial cases, such as when using an\n"
	        "             interpolating VRR display in fullscreen (default).\n"
	        "  on:        Enable vsync. This can prevent tearing in some games but will\n"
	        "             impact performance or drop frames when the DOS rate exceeds the\n"
	        "             host rate (e.g. 70 Hz DOS rate vs 60 Hz host rate).\n"
	        "  adaptive:  Enables vsync when the frame rate is higher than the host rate,\n"
	        "             but disables it when the frame rate drops below the host rate.\n"
	        "             Note: only valid in OpenGL output modes; otherwise treated as 'on'.\n"
	        "  off:       Attempt to disable vsync to allow quicker frame presentation at\n"
	        "             the risk of tearing in some games.\n"
	        "  yield:     Let the host's video driver control video synchronization.");
	pstring->Set_values(vsync_prefs);

	pint = sdl_sec->Add_int("vsync_skip", on_start, 0);
	pint->Set_help("Number of microseconds to allow rendering to block before skipping the\n"
	               "next frame. For example, a value of 7000 is roughly half the frame time\n"
	               "at 70 Hz. 0 disables this and will always render (default).");
	pint->SetMinMax(0, 14000);

	const char *presentation_modes[] = {"auto", "cfr", "vfr", nullptr};
	pstring = sdl_sec->Add_string("presentation_mode", always, "auto");
	pstring->Set_help(
	        "Select the frame presentation mode:\n"
	        "  auto:  Intelligently time and drop frames to prevent emulation stalls,\n"
	        "         based on host and DOS frame rates (default).\n"
	        "  cfr:   Always present DOS frames at a constant frame rate.\n"
	        "  vfr:   Always present changed DOS frames at a variable frame rate.");
	pstring->Set_values(presentation_modes);

	const char* outputs[] =
	{ "texture",
	  "texturenb",
#if C_OPENGL
	  "opengl",
	  "openglnb",
#endif
	  nullptr };

#if C_OPENGL
	Pstring = sdl_sec->Add_string("output", always, "opengl");
	Pstring->Set_help(
	        "Video system to use for output ('opengl' by default).\n"
	        "'texture' and 'opengl' use bilinear interpolation, 'texturenb' and\n"
	        "'openglnb' use nearest-neighbour (no-bilinear). Some shaders require\n"
	        "bilinear interpolation, making that the safest choice.");
	Pstring->SetDeprecatedWithAlternateValue("openglpp", "opengl");
	Pstring->SetDeprecatedWithAlternateValue("surface", "opengl");
#else
	Pstring = sdl_sec->Add_string("output", always, "texture");
	Pstring->Set_help("Video system to use for output ('texture' by default).");
	Pstring->SetDeprecatedWithAlternateValue("surface", "texture");
#endif
	Pstring->SetDeprecatedWithAlternateValue("texturepp", "texture");
	Pstring->Set_values(outputs);

	pstring = sdl_sec->Add_string("texture_renderer", always, "auto");
	pstring->Set_help("Render driver to use in 'texture' output mode ('auto' by default).\n"
	                  "Use 'texture_renderer = auto' for an automatic choice.");
	pstring->Set_values(get_sdl_texture_renderers());

	Pmulti = sdl_sec->AddMultiVal("capture_mouse", deprecated, ",");
	Pmulti->Set_help("Moved to [mouse] section and renamed to 'mouse_capture'.");

	Pmulti = sdl_sec->AddMultiVal("sensitivity", deprecated, ",");
	Pmulti->Set_help("Moved to [mouse] section and renamed to 'mouse_sensitivity'.");

	pbool = sdl_sec->Add_bool("raw_mouse_input", deprecated, false);
	pbool->Set_help("Moved to [mouse] section and renamed to 'mouse_raw_input'.");

	Pbool = sdl_sec->Add_bool("waitonerror", always, true);
	Pbool->Set_help("Keep the console open if an error has occurred (enabled by default).");

	Pmulti = sdl_sec->AddMultiVal("priority", always, " ");
	Pmulti->SetValue("auto auto");
	Pmulti->Set_help("Priority levels to apply when active and inactive, respectively.\n"
	                 "('auto auto' by default)\n"
	                 "'auto' lets the host operating system manage the priority.");

	const char *priority_level_choices[] = {
	        "auto",
	        "lowest",
	        "lower",
	        "normal",
	        "higher",
	        "highest",
	        nullptr,
	};
	psection = Pmulti->GetSection();
	psection->Add_string("active", always, priority_level_choices[0])
	        ->Set_values(priority_level_choices);
	psection->Add_string("inactive", always, priority_level_choices[0])
	        ->Set_values(priority_level_choices);

	pbool = sdl_sec->Add_bool("mute_when_inactive", on_start, false);
	pbool->Set_help("Mute the sound when the window is inactive (disabled by default).");

	pbool = sdl_sec->Add_bool("pause_when_inactive", on_start, false);
	pbool->Set_help("Pause emulation when the window is inactive (disabled by default).");

	pstring = sdl_sec->Add_path("mapperfile", always, MAPPERFILE);
	pstring->Set_help(
	        "File used to load/save the key/event mappings.\n"
	        "Pre-configured maps are bundled in the 'resources/mapperfiles' directory.\n"
	        "They can be loaded by name, for example: mapperfile = xbox/xenon2.map\n"
	        "Note: The --resetmapper command line flag only deletes the default mapperfile.");

	pstring = sdl_sec->Add_string("screensaver", on_start, "auto");
	pstring->Set_help(
	        "Use 'allow' or 'block' to override the SDL_VIDEO_ALLOW_SCREENSAVER environment\n"
	        "variable which usually blocks the OS screensaver while the emulator is\n"
	        "running ('auto' by default).");
	const char *ssopts[] = {"auto", "allow", "block", nullptr};
	pstring->Set_values(ssopts);
}

static int edit_primary_config()
{
	const auto path = GetPrimaryConfigPath();

	if (!path_exists(path)) {
		printf("Primary config does not exist at path '%s'\n",
		       path.string().c_str());
		return 1;
	}

	auto replace_with_process = [&](const std::string& prog) {
		execlp(prog.c_str(), prog.c_str(), path.string().c_str(), (char*)nullptr);
	};

	// Loop until one succeeds
	const auto arguments = &control->arguments;
	if (arguments->editconf) {
		for (const auto& editor : *arguments->editconf) {
			replace_with_process(editor);
		}
	}

	const char* env_editor = getenv("EDITOR");
	if (env_editor) {
		replace_with_process(env_editor);
	}

	replace_with_process("nano");
	replace_with_process("vim");
	replace_with_process("vi");
	replace_with_process("notepad++.exe");
	replace_with_process("notepad.exe");

	LOG_ERR("Can't find any text editors; please set the EDITOR env variable "
	        "to your preferred text editor.\n");

	return 1;
}

#if C_DEBUG
extern void DEBUG_ShutDown(Section * /*sec*/);
#endif

void MIXER_CloseAudioDevice(void);

void restart_program(std::vector<std::string> & parameters) {
	char** newargs = new char* [parameters.size() + 1];
	// parameter 0 is the executable path
	// contents of the vector follow
	// last one is NULL
	for (size_t i = 0; i < parameters.size(); i++) {
		newargs[i] = parameters[i].data();
	}
	newargs[parameters.size()] = nullptr;
	MIXER_CloseAudioDevice();
	Delay(50);
	QuitSDL();
#if C_DEBUG
	// shutdown curses
	DEBUG_ShutDown(nullptr);
#endif

	if(execvp(newargs[0], newargs) == -1) {
#ifdef WIN32
		if(newargs[0][0] == '\"') {
			//everything specifies quotes around it if it contains a space, however my system disagrees
			std::string edit = parameters[0];
			edit.erase(0,1);edit.erase(edit.length() - 1,1);
			//However keep the first argument of the passed argv (newargs) with quotes, as else repeated restarts go wrong.
			if(execvp(edit.c_str(), newargs) == -1) E_Exit("Restarting failed");
		}
#endif
		E_Exit("Restarting failed");
	}
	delete [] newargs;
}
void Restart(bool pressed) { // mapper handler
	(void) pressed; // deliberately unused but required for API compliance
	restart_program(control->startup_params);
}

static int list_glshaders()
{
#if C_OPENGL
	for (const auto& line : RENDER_GenerateShaderInventoryMessage()) {
		printf("%s\n", line.c_str());
	}
	return 0;
#else
	fprintf(stderr,
	        "OpenGL is not supported by this executable "
	        "and is missing the functionality to list shaders.");

	return 1;
#endif
}

static int print_primary_config_location()
{
	const auto path = GetPrimaryConfigPath();

	if (!path_exists(path)) {
		printf("Primary config does not exist at path '%s'\n",
		       path.string().c_str());
		return 1;
	}

	printf("%s\n", path.string().c_str());
	return 0;
}

static int erase_primary_config_file()
{
	const auto path = GetPrimaryConfigPath();

	if (!path_exists(path)) {
		printf("Primary config does not exist at path '%s'\n",
		       path.string().c_str());
		return 1;
	}

	constexpr auto success = 0;
	if (unlink(path.string().c_str()) != success) {
		fprintf(stderr,
		        "Cannot delete primary config '%s'",
		        path.string().c_str());
		return 1;
	}

	printf("Primary config '%s' deleted.\n", path.string().c_str());
	return 0;
}

static int erase_mapper_file()
{
	const auto path = GetConfigDir() / MAPPERFILE;

	if (!path_exists(path)) {
		printf("Default mapper file does not exist at path '%s'\n",
		       path.string().c_str());
		return 1;
	}

	if (path_exists("dosbox.conf")) {
		printf("Local 'dosbox.conf' exists in current working directory; "
		       "mappings will not be reset if the local config specifies "
		       "a custom mapper file.\n");
	}

	constexpr auto success = 0;
	if (unlink(path.string().c_str()) != success) {
		fprintf(stderr,
		        "Cannot delete mapper file '%s'",
		        path.string().c_str());
		return 1;
	}
	printf("Mapper file '%s' deleted.\n", path.string().c_str());
	return 0;
}

static void override_wm_class()
{
#if !defined(WIN32)
	constexpr int overwrite = 0; // don't overwrite
	setenv("SDL_VIDEO_X11_WMCLASS", CANONICAL_PROJECT_NAME, overwrite);
#endif
}

extern "C" int SDL_CDROMInit(void);

int sdl_main(int argc, char* argv[])
{
	CommandLine command_line(argc, argv);
	control = std::make_unique<Config>(&command_line);

	const auto arguments = &control->arguments;

	// Set up logging after command line was parsed and trivial arguments have
	// been handled
	loguru::g_preamble_date    = true;
	loguru::g_preamble_time    = true;
	loguru::g_preamble_uptime  = false;
	loguru::g_preamble_thread  = false;
	loguru::g_preamble_file    = false;
	loguru::g_preamble_verbose = false;
	loguru::g_preamble_pipe    = true;

	if (arguments->version || arguments->help || arguments->printconf ||
	    arguments->editconf || arguments->eraseconf ||
	    arguments->list_glshaders || arguments->erasemapper) {
		loguru::g_stderr_verbosity = loguru::Verbosity_WARNING;
	}

	loguru::init(argc, argv);

	LOG_MSG("%s version %s", CANONICAL_PROJECT_NAME, DOSBOX_GetDetailedVersion());
	LOG_MSG("---");

	LOG_MSG("LOG: Loguru version %d.%d.%d initialised",
	        LOGURU_VERSION_MAJOR,
	        LOGURU_VERSION_MINOR,
	        LOGURU_VERSION_PATCH);

	int return_code = 0;

	try {
		if (!arguments->working_dir.empty()) {
			std::error_code ec;
			std_fs::current_path(arguments->working_dir, ec);
			if (ec) {
				LOG_ERR("Cannot set working directory to %s",
				        arguments->working_dir.c_str());
			}
		}

		// Before SDL2 video subsystem is initialised
		override_wm_class();

		// Create or determine the location of the config directory
		// (e.g., in portable mode, the config directory is the
		// executable dir).
		//
		// TODO Consider forcing portable mode in secure mode (this
		// could be accomplished by passing a flag to InitConfigDir);.
		//
		InitConfigDir();

		// Register sdlmain's messages and conf sections
		messages_add_sdl();
		config_add_sdl();

		// Register DOSBox's (and all modules) messages and conf sections
		DOSBOX_Init();

		// Before loading any configs, write the default primary config if it
		// doesn't exist when:
		// - secure mode is NOT enabled with the '--securemode' option,
		// - AND we're not in portable mode (portable mode is enabled when
		//   'dosbox-staging.conf' exists in the executable directory)
		// - AND the primary config is NOT disabled with the
		//   '--noprimaryconf' option.
		if (!arguments->securemode && !arguments->noprimaryconf) {
			const auto primary_config_path = GetPrimaryConfigPath();

			if (!path_exists(primary_config_path)) {
				// No config is loaded at this point, so we're
				// writing the default settings to the primary
				// config.
				if (control->WriteConfig(primary_config_path)) {
					LOG_MSG("CONFIG: Created primary config file '%s'",
					        primary_config_path.string().c_str());
				} else {
					LOG_WARNING("CONFIG: Unable to create primary config file '%s'",
					            primary_config_path.string().c_str());
				}
			}
		}

		// After DOSBOX_Init() is done, all the conf sections have been
		// registered, so we're ready to parse the conf files.
		//
		ParseConfigFiles(GetConfigDir());

		// Handle command line options that don't start the emulator but only
		// perform some actions and print the results to the console.
		if (arguments->version) {
			printf(version_msg,
			       CANONICAL_PROJECT_NAME,
			       DOSBOX_GetDetailedVersion());
			return 0;
		}
		if (arguments->help) {
			assert(argv && argv[0]);
			const auto program_name = argv[0];
			const auto help_utf8 = format_string(MSG_GetRaw("DOSBOX_HELP"),
			                                     program_name);
#ifdef WIN32
			const auto old_code_page = GetConsoleOutputCP();

			constexpr uint16_t CodePageUtf8 = 65001;
			SetConsoleOutputCP(CodePageUtf8);
			printf("%s", help_utf8.c_str());
			SetConsoleOutputCP(old_code_page);
#else
			// Assume any OS without special support uses UTF-8 as
			// console encoding
			printf("%s", help_utf8.c_str());
#endif
			return 0;
		}
		if (arguments->editconf) {
			return edit_primary_config();
		}
		if (arguments->eraseconf) {
			return erase_primary_config_file();
		}
		if (arguments->erasemapper) {
			return erase_mapper_file();
		}
		if (arguments->printconf) {
			return print_primary_config_location();
		}
		if (arguments->list_glshaders) {
			return list_glshaders();
		}

		// Can't disable the console with debugger enabled
#if defined(WIN32) && !(C_DEBUG)
		if (arguments->noconsole) {
			FreeConsole();
			// Redirect standard input and standard output
			//
			if (freopen(STDOUT_FILE, "w", stdout) == NULL) {
				// No stdout so don't write messages
				no_stdout = true;
			}
			freopen(STDERR_FILE, "w", stderr);

			setvbuf(stdout, NULL, _IOLBF, BUFSIZ); // Line buffered
			setvbuf(stderr, NULL, _IONBF, BUFSIZ); // No buffering
		} else {
			if (AllocConsole()) {
				fclose(stdin);
				fclose(stdout);
				fclose(stderr);
				freopen("CONIN$", "r", stdin);
				freopen("CONOUT$", "w", stdout);
				freopen("CONOUT$", "w", stderr);
			}
			SetConsoleTitle("DOSBox Status Window");
		}
#endif // defined(WIN32) && !(C_DEBUG)

#if defined(WIN32)
		SetConsoleCtrlHandler((PHANDLER_ROUTINE)console_event_handler, TRUE);

#if SDL_VERSION_ATLEAST(2, 24, 0)
		if (SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2") ==
		    SDL_FALSE) {
			LOG_WARNING("SDL: Failed to set DPI awareness flag");
		}
		if (SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1") == SDL_FALSE) {
			LOG_WARNING("SDL: Failed to set DPI scaling flag");
		}
#endif
#endif // WIN32

		if (const auto err = check_kmsdrm_setting(); err != 0) {
			return err;
		}

		if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0) {
			E_Exit("SDL: Can't init SDL %s", SDL_GetError());
		}
		if (SDL_CDROMInit() < 0) {
			LOG_WARNING("SDL: Failed to init CD-ROM support");
		}

		sdl.initialized = true;

		// Once initialised, ensure we clean up SDL for all exit conditions
		atexit(QuitSDL);

		SDL_version sdl_version;
		SDL_GetVersion(&sdl_version);

		LOG_MSG("SDL: version %d.%d.%d initialised (%s video and %s audio)",
		        sdl_version.major,
		        sdl_version.minor,
		        sdl_version.patch,
		        SDL_GetCurrentVideoDriver(),
		        SDL_GetCurrentAudioDriver());

		for (auto line : arguments->set) {
			trim(line);

			if (line.empty() || line[0] == '%' || line[0] == '\0' ||
			    line[0] == '#' || line[0] == '\n') {
				continue;
			}

			std::vector<std::string> pvars(1, std::move(line));

			const char* result = SetProp(pvars);

			if (strlen(result)) {
				LOG_WARNING("CONFIG: %s", result);
			} else {
				Section* tsec = control->GetSection(pvars[0]);
				std::string value(pvars[2]);

				// Due to parsing, there can be a '=' at the
				// start of the value.
				while (value.size() && (value.at(0) == ' ' ||
				                        value.at(0) == '=')) {
					value.erase(0, 1);
				}

				for (Bitu i = 3; i < pvars.size(); i++) {
					value += (std::string(" ") + pvars[i]);
				}

				std::string inputline = pvars[1] + "=" + value;

				bool change_success = tsec->HandleInputline(
				        inputline.c_str());

				if (!change_success && !value.empty()) {
					LOG_WARNING("CONFIG: Cannot set '%s'",
					            inputline.c_str());
				}
			}
		}

#if C_OPENGL
		const auto glshaders_dir = GetConfigDir() / GlShadersDir;

		if (create_dir(glshaders_dir, 0700, OK_IF_EXISTS) != 0) {
			LOG_WARNING("CONFIG: Can't create directory '%s': %s",
			            glshaders_dir.string().c_str(),
			            safe_strerror(errno).c_str());
		}
#endif

#if C_FLUIDSYNTH
		const auto soundfonts_dir = GetConfigDir() / DefaultSoundfontsDir;

		if (create_dir(soundfonts_dir, 0700, OK_IF_EXISTS) != 0) {
			LOG_WARNING("CONFIG: Can't create directory '%s': %s",
			            soundfonts_dir.string().c_str(),
			            safe_strerror(errno).c_str());
		}
#endif

#if C_MT32EMU
		const auto mt32_rom_dir = GetConfigDir() / DefaultMt32RomsDir;

		if (create_dir(mt32_rom_dir, 0700, OK_IF_EXISTS) != 0) {
			LOG_WARNING("CONFIG: Can't create directory '%s': %s",
			            mt32_rom_dir.string().c_str(),
			            safe_strerror(errno).c_str());
		}
#endif

		control->ParseEnv();
		//		UI_Init();
		//		if (control->cmdline->FindExist("-startui"))
		// UI_Run(false);

		// Init all the sections
		control->Init();

		// Some extra SDL Functions
		Section_prop* sdl_sec = static_cast<Section_prop*>(
		        control->GetSection("sdl"));

		// All subsystems' hotkeys need to be registered at this point
		// to ensure their hotkeys appear in the graphical mapper.
		MAPPER_BindKeys(sdl_sec);

		if (arguments->startmapper) {
			MAPPER_DisplayUI();
		}

		// Run the machine until shutdown
		control->StartUp();

		// Shutdown and release
		control.reset();

	} catch (char* error) {
		return_code = 1;

		GFX_ShowMsg("Exit to error: %s", error);

		fflush(nullptr);

		if (sdl.wait_on_error) {
			// TODO Maybe look for some way to show message in linux?
#if (C_DEBUG)
			GFX_ShowMsg("Press enter to continue");

			fflush(nullptr);
			fgetc(stdin);

#elif defined(WIN32)
			Sleep(5000);
#endif
		}
	} catch (const std::exception& e) {
		// Catch all exceptions that derive from the standard library
		LOG_ERR("EXCEPTION: Standard library exception: %s", e.what());
		return_code = 1;
	} catch (...) {
		// Just exit
		return_code = 1;
	}

#if defined(WIN32)
	// Might not be needed if the shutdown function switches to windowed
	// mode, but it doesn't hurt
	sticky_keys(true);
#endif

	// We already do this at exit, but do cleanup earlier in case of normal
	// exit; this works around problems when atexit order clashes with SDL2
	// cleanup order. Happens with SDL_VIDEODRIVER=wayland as of SDL 2.0.12.
	QuitSDL();

	return return_code;
}

