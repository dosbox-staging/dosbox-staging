/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
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
#include <cstdlib>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <tuple>
#include <math.h>

#if C_DEBUG
#include <queue>
#endif

#ifdef WIN32
#include <signal.h>
#include <process.h>
#endif

#include <SDL.h>
#if C_OPENGL
#include <SDL_opengl.h>
#endif
#if C_SDL_IMAGE
#	include <SDL_image.h>
#endif

#include "../ints/int10.h"
#include "control.h"
#include "cpu.h"
#include "cross.h"
#include "debug.h"
#include "fs_utils.h"
#include "gui_msgs.h"
#include "hardware.h"
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

#ifndef GL_ARB_pixel_buffer_object
#define GL_ARB_pixel_buffer_object 1
#define GL_PIXEL_PACK_BUFFER_ARB           0x88EB
#define GL_PIXEL_UNPACK_BUFFER_ARB         0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING_ARB   0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING_ARB 0x88EF
#endif

#ifndef GL_ARB_vertex_buffer_object
#define GL_ARB_vertex_buffer_object 1
typedef void (APIENTRYP PFNGLGENBUFFERSARBPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLBINDBUFFERARBPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRYP PFNGLDELETEBUFFERSARBPROC) (GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP PFNGLBUFFERDATAARBPROC) (GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef GLvoid* (APIENTRYP PFNGLMAPBUFFERARBPROC) (GLenum target, GLenum access);
typedef GLboolean (APIENTRYP PFNGLUNMAPBUFFERARBPROC) (GLenum target);
#endif

PFNGLGENBUFFERSARBPROC glGenBuffersARB = NULL;
PFNGLBINDBUFFERARBPROC glBindBufferARB = NULL;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB = NULL;
PFNGLBUFFERDATAARBPROC glBufferDataARB = NULL;
PFNGLMAPBUFFERARBPROC glMapBufferARB = NULL;
PFNGLUNMAPBUFFERARBPROC glUnmapBufferARB = NULL;

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
PFNGLATTACHSHADERPROC glAttachShader = NULL;
PFNGLCOMPILESHADERPROC glCompileShader = NULL;
PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
PFNGLCREATESHADERPROC glCreateShader = NULL;
PFNGLDELETEPROGRAMPROC glDeleteProgram = NULL;
PFNGLDELETESHADERPROC glDeleteShader = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = NULL;
PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
PFNGLGETSHADERIVPROC glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
PFNGLSHADERSOURCEPROC_NP glShaderSource = NULL;
PFNGLUNIFORM2FPROC glUniform2f = NULL;
PFNGLUNIFORM1IPROC glUniform1i = NULL;
PFNGLUSEPROGRAMPROC glUseProgram = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = NULL;
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
constexpr uint32_t RMASK = 0xff000000;
constexpr uint32_t GMASK = 0x00ff0000;
constexpr uint32_t BMASK = 0x0000ff00;
constexpr uint32_t AMASK = 0x000000ff;
#else
constexpr uint32_t RMASK = 0x000000ff;
constexpr uint32_t GMASK = 0x0000ff00;
constexpr uint32_t BMASK = 0x00ff0000;
constexpr uint32_t AMASK = 0xff000000;
#endif

// Size and ratio constants
// ------------------------
constexpr int SMALL_WINDOW_PERCENT = 50;
constexpr int MEDIUM_WINDOW_PERCENT = 74;
constexpr int LARGE_WINDOW_PERCENT = 90;
constexpr int DEFAULT_WINDOW_PERCENT = MEDIUM_WINDOW_PERCENT;
static SDL_Point FALLBACK_WINDOW_DIMENSIONS = {640, 480};
constexpr SDL_Point RATIOS_FOR_STRETCHED_PIXELS = {4, 3};
constexpr SDL_Point RATIOS_FOR_SQUARE_PIXELS = {8, 5};

/* Alias for indicating, that new window should not be user-resizable: */
constexpr bool FIXED_SIZE = false;

static bool first_window = true;

static SDL_Point restrict_to_viewport_resolution(int width, int height);
static PPScale calc_pp_scale(int width, int heigth);
static SDL_Rect calc_viewport(int width, int height);
static SDL_Rect calc_viewport_fit(const int width, const int height);

static void CleanupSDLResources();
static void HandleVideoResize(int width, int height);

static const char *vsync_state_as_string(const VSYNC_STATE state)
{
	switch (state) {
	case VSYNC_STATE::UNSET: return "unset";
	case VSYNC_STATE::ADAPTIVE: return "adaptive";
	case VSYNC_STATE::OFF: return "off";
	case VSYNC_STATE::ON: return "on";
	default: return "unknown";
	}
}

#if C_DEBUG
extern SDL_Window *pdc_window;
extern std::queue<SDL_Event> pdc_event_queue;

static bool isDebuggerEvent(const SDL_Event &event)
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

// SDL allows pixels sizes (color-depth) from 1 to 4 bytes
constexpr uint8_t MAX_BYTES_PER_PIXEL = 4;


#ifdef DB_OPENGL_ERROR
void OPENGL_ERROR(const char* message) {
	GLenum r = glGetError();
	if (r == GL_NO_ERROR) return;
	LOG_ERR("errors from %s",message);
	do {
		LOG_ERR("%X",r);
	} while ( (r=glGetError()) != GL_NO_ERROR);
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
	#define APP_NAME_STR "DOSBox Staging (debug build)"
#else
	#define APP_NAME_STR "DOSBox Staging"
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
			return REFRESH_RATE_HOST_DEFAULT;
		}
		if (SDL_GetCurrentDisplayMode(display_in_use, &mode) != 0) {
			LOG_ERR("SDL: Could not get the current display mode: %s",
			        SDL_GetError());
			return REFRESH_RATE_HOST_DEFAULT;
		}
		if (sdl_rate < REFRESH_RATE_MIN) {
			LOG_WARNING("SDL: Got a strange refresh rate of %d Hz",
			            sdl_rate);
			return REFRESH_RATE_HOST_DEFAULT;
		}
		assert(sdl_rate >= REFRESH_RATE_MIN);
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
	case HOST_RATE_MODE::AUTO:
		if (const auto sdl_rate = get_sdl_rate();
		    sdl.desktop.fullscreen && sdl_rate >= REFRESH_RATE_HOST_VRR_MIN) {
			rate = get_vrr_rate(sdl_rate);
			rate_description = "VRR-adjusted (auto)";
		} else {
			rate = get_sdi_rate(sdl_rate);
			rate_description = "standard SDI (auto)";
		}
		break;
	case HOST_RATE_MODE::SDI:
		rate = get_sdi_rate(get_sdl_rate());
		rate_description = "standard SDI";
		break;
	case HOST_RATE_MODE::VRR:
		rate = get_vrr_rate(get_sdl_rate());
		rate_description = "VRR-adjusted";
		break;
	case HOST_RATE_MODE::CUSTOM:
		assert(sdl.desktop.preferred_host_rate >= REFRESH_RATE_MIN);
		rate = sdl.desktop.preferred_host_rate;
		rate_description = "custom";
		break;
	}
	assert(rate >= REFRESH_RATE_MIN);

	// Log if changed
	static auto last_int_rate = 0;
	const auto int_rate = static_cast<int>(rate);
	if (last_int_rate != int_rate) {
		last_int_rate = int_rate;
		LOG_MSG("SDL: Using %s display refresh rate of %2.5g Hz",
		        rate_description, rate);
	}
	return rate;
}

/* This function is SDL_EventFilter which is being called when event is
 * pushed into the SDL event queue.
 *
 * WARNING: Be very careful of what you do in this function, as it may run in
 * a different thread!
 *
 * Read documentation for SDL_AddEventWatch for more details.
 */
static int watch_sdl_events(void *userdata, SDL_Event *e)
{
	/* There's a significant difference in handling of window resize
	 * events in different OSes. When handling resize in main event loop
	 * we receive continuous stream of events (as expected) on Linux,
	 * but only single event after user stopped dragging cursor on Windows
	 * and macOS.
	 *
	 * Watching resize events here gives us continuous stream on
	 * every OS.
	 */
	if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_RESIZED) {
		SDL_Window *win = SDL_GetWindowFromID(e->window.windowID);
		if (win == (SDL_Window *)userdata) {
			const int w = e->window.data1;
			const int h = e->window.data2;
			// const int id = e->window.windowID;
			// DEBUG_LOG_MSG("SDL: Resizing window %d to %dx%d", id, w, h);
			HandleVideoResize(w, h);
		}
	}
	return 0;
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
		DEBUG_LOG_MSG("SDL: Exit requested");
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
		case SDL_KEYUP:
			if (event.key.keysym.sym == SDLK_PAUSE ||
			    event.key.keysym.sym == SDLK_ESCAPE) {
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

Bitu GFX_GetBestMode(Bitu flags)
{
	switch (sdl.desktop.want_type) {
	case SCREEN_SURFACE:
	check_surface:
		switch (sdl.desktop.bpp) {
		case 8:
			if (flags & GFX_CAN_8) flags&=~(GFX_CAN_15|GFX_CAN_16|GFX_CAN_32);
			break;
		case 15:
			if (flags & GFX_CAN_15) flags&=~(GFX_CAN_8|GFX_CAN_16|GFX_CAN_32);
			break;
		case 16:
			if (flags & GFX_CAN_16) flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_32);
			break;
		case 24:
		case 32:
			if (flags & GFX_CAN_32) flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
			break;
		}
		flags |= GFX_CAN_RANDOM;
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
#endif
	case SCREEN_TEXTURE:
		// We only accept 32bit output from the scalers here
		if (!(flags & GFX_CAN_32)) {
			goto check_surface;
		}
		flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
		break;
	default:
		goto check_surface;
		break;
	}
	return flags;
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
static uint32_t opengl_driver_crash_workaround(SCREEN_TYPES type)
{
	if (type != SCREEN_TEXTURE)
		return 0;

	if (starts_with("opengl", sdl.render_driver))
		return SDL_WINDOW_OPENGL;

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
	default_driver_is_opengl = starts_with("opengl", info.name);
	return (default_driver_is_opengl ? SDL_WINDOW_OPENGL : 0);
}

static SDL_Point refine_window_size(const SDL_Point &size,
                                    const SCALING_MODE scaling_mode,
                                    const bool should_stretch_pixels);

static SDL_Rect get_canvas_size(const SCREEN_TYPES screen_type);

static SDL_Rect calc_viewport_pp(int win_width, int win_height);

// Logs the source and target resolution including describing scaling method
// and pixel-aspect ratios. Note that this function deliberately doesn't use
// any global structs to disentangle it from the existing sdl-main design.
static void log_display_properties(int source_w, int source_h,
                                   const std::optional<SDL_Rect> &target_size_override,
                                   const SCALING_MODE scaling_mode,
                                   const SCREEN_TYPES screen_type,
                                   const PPScale &pp_scale)
{
	// The pixel perfect object holds its effective source dimensions
	if (scaling_mode == SCALING_MODE::PERFECT) {
		source_w = pp_scale.effective_source_w;
		source_h = pp_scale.effective_source_h;
	}
	// Get the target dimentions, with consideration for possible override
	// values and pixel-perfect handling
	auto get_target_dims = [&]() -> std::pair<int, int> {
		if (target_size_override) {
			auto calc_vp  = (scaling_mode == SCALING_MODE::PERFECT)
			                      ? calc_viewport_pp
			                      : calc_viewport_fit;
			const auto vp = calc_vp(target_size_override->w,
			                        target_size_override->h);
			return {vp.w, vp.h};
		}
		if (scaling_mode == SCALING_MODE::PERFECT) {
			return {pp_scale.output_w, pp_scale.output_h};
		}
		const auto canvas   = get_canvas_size(screen_type);
		const auto viewport = calc_viewport_fit(canvas.w, canvas.h);
		return {viewport.w, viewport.h};
	};

	const auto [target_w, target_h] = get_target_dims();

	// Check expectations
	assert(source_w > 0 && source_h > 0);
	assert(target_w > 0 && target_h > 0);

	const auto scale_x = static_cast<double>(target_w) / source_w;
	const auto scale_y = static_cast<double>(target_h) / source_h;
	const auto out_par = scale_y / scale_x;

	const auto [type_name, type_colours] = VGA_DescribeType(CurMode->type,
	                                                        CurMode->mode);

	const char *frame_mode = nullptr;
	switch (sdl.frame.mode) {
	case FRAME_MODE::CFR: frame_mode = "CFR"; break;
	case FRAME_MODE::VFR: frame_mode = "VFR"; break;
	case FRAME_MODE::SYNCED_CFR: frame_mode = "synced CFR"; break;
	case FRAME_MODE::THROTTLED_VFR: frame_mode = "throttled VFR"; break;
	case FRAME_MODE::UNSET: break;
	}
	assert(frame_mode);

	// Some DOS FPS rates are double-scanned in hardware, so multiply them
	// up to avoid confusion (ie: 30 Hz should actually be shown at 60Hz)
	auto refresh_rate = VGA_GetPreferredRate();
	const auto double_scanned_str = (refresh_rate <= REFRESH_RATE_DOS_DOUBLED_MAX)
	                                        ? "double-scanned "
	                                        : "";
	LOG_MSG("DISPLAY: %s %dx%d%s (%Xh) at %s%2.5g Hz %s, scaled"
	        " by %.1fx%.1f to %dx%d with %#.2g pixel-aspect",
	        type_name,
	        source_w,
	        source_h,
	        type_colours,
	        CurMode->mode,
	        double_scanned_str,
	        refresh_rate,
	        frame_mode,
	        scale_x,
	        scale_y,
	        target_w,
	        target_h,
	        out_par);
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

static Pacer render_pacer("Render", 7000, Pacer::LogLevel::TIMEOUTS);

static const VsyncPreference &get_vsync_preference()
{
	return sdl.desktop.fullscreen ? sdl.vsync.when_fullscreen
	                              : sdl.vsync.when_windowed;
}

static void save_rate_to_frame_period(const double rate_hz)
{
	assert(rate_hz > 0);
	// backoff by one-onethousandth to avoid hitting the vsync edge
	sdl.frame.period_ms = 1'001.0 / rate_hz;
	const auto period_us = sdl.frame.period_ms * 1'000;
	sdl.frame.period_us = static_cast<int>(period_us);
	// Permit the frame period to be off by up to 90% before "out of sync"
	sdl.frame.period_us_early = static_cast<int>(55 * period_us / 100);
	sdl.frame.period_us_late = static_cast<int>(145 * period_us / 100);
}

static int benchmark_presentation_rate()
{
	// If the presentation function is empty, then we can't benchmark
	assert(sdl.frame.present != present_frame_noop ||
	       sdl.frame.update != update_frame_noop);

	// Number of frames to benchmark
	const auto warmup_frames = 10;
	const auto bench_frames = 4;
	// Disable the pacer because we need every frame presented and measured
	// so we can hit the vsync wall (if it exists)
	render_pacer.SetTimeout(0);
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
	const auto elapsed_us = std::max(1, GetTicksUsSince(start_us));
	return (bench_frames * 1'000'000) / elapsed_us;
}

static VSYNC_STATE get_reported_vsync()
{
	auto state = VSYNC_STATE::UNSET;
#if C_OPENGL
	if (sdl.desktop.type == SCREEN_OPENGL) {
		assert(sdl.opengl.context);
		const auto retval = SDL_GL_GetSwapInterval();
		switch (retval) {
		case 1: state = VSYNC_STATE::ON; break;
		case 0: state = VSYNC_STATE::OFF; break;
		case -1: state = VSYNC_STATE::ADAPTIVE; break;
		default:
			LOG_WARNING("SDL:OPENGL: Reported an unknown vsync state: %d",
			            retval);
			break;
		}
	}
#endif
	if (sdl.desktop.type == SCREEN_TEXTURE || sdl.desktop.type == SCREEN_SURFACE) {
		const std::string_view retstr = SDL_GetHint(SDL_HINT_RENDER_VSYNC);
		if (retstr == "1")
			state = VSYNC_STATE::ON;
		else if (retstr == "0")
			state = VSYNC_STATE::OFF;
		else if (retstr == "-1")
			state = VSYNC_STATE::ADAPTIVE;
		else
			LOG_WARNING("SDL: Reported an unknown vsync state: %s",
			            retstr.data());
	}
	assert(state != VSYNC_STATE::UNSET);
	return state;
}

static VSYNC_STATE get_resultant_vsync(int &bench_rate)
{
	bench_rate = benchmark_presentation_rate();
	const auto host_rate = get_host_refresh_rate();

	// Notify the user if the machine is prensetation-starved.
	if (bench_rate < host_rate * 0.5) {
		LOG_WARNING("SDL: The benchmarked rendering rate of %d FPS, which"
		            " is well below the host's refresh rate of %2.5g Hz.",
		            bench_rate, host_rate);
		LOG_WARNING(
		        "SDL: You will experience rendering lag and stuttering."
		        " Consider updating your video drivers and try disabling"
		        " vsync in your host and drivers, set [sdl] vsync = false");
	}

	if (bench_rate < host_rate * 1.5)
		return VSYNC_STATE::ON;
	else if (bench_rate < host_rate * 2.5)
		return VSYNC_STATE::ADAPTIVE;
	else
		return VSYNC_STATE::OFF;
}

static void set_vsync(const VSYNC_STATE state)
{
#if C_OPENGL
	if (sdl.desktop.type == SCREEN_OPENGL) {
		assert(sdl.opengl.context);
		const auto interval = static_cast<int>(state);
		// -1=adaptive, 0=off, 1=on
		assert(interval >= -1 && interval <= 1);
		if (SDL_GL_SetSwapInterval(interval) == 0)
			return;

		// the requested interval is not supported
		LOG_WARNING("SDL: Failed setting the OpenGL vsync state to %s (%d): %s",
		            vsync_state_as_string(state), interval, SDL_GetError());

		// Per SDL's recommendation: If an application requests adaptive
		// vsync and the system does not support it, this function will
		// fail and return -1. In such a case, you should probably retry
		// the call with 1 for the interval.
		if (interval == -1 && SDL_GL_SetSwapInterval(1) != 0) {
			LOG_WARNING("SDL: Tried enabling non-adaptive OpenGL vsync, but it still failed: %s",
			            SDL_GetError());
		}
		return;
	}
#endif
	if (sdl.desktop.type == SCREEN_TEXTURE || sdl.desktop.type == SCREEN_SURFACE) {
		// https://wiki.libsdl.org/SDL_HINT_RENDER_VSYNC - can only be
		// set to "1", "0", adapative is currently not supported, so we
		// also treat it as "1"
		const auto hint_str = (state == VSYNC_STATE::ON ||
		                       state == VSYNC_STATE::ADAPTIVE)
		                              ? "1"
		                              : "0";
		if (SDL_SetHint(SDL_HINT_RENDER_VSYNC, hint_str) == SDL_TRUE)
			return;
		LOG_WARNING("SDL: Failed setting SDL's vsync state to to %s (%s): %s",
		            vsync_state_as_string(state), hint_str, SDL_GetError());
		return;
	}
	// Unhandled screen type
	LOG_WARNING("SDL: Failed setting the vsync state to %s: unsupported screen type",
	            vsync_state_as_string(state));
}

static void update_vsync_state()
{
	// Hosts can have different vsync constraints between window modes
	auto &vsync_pref = sdl.desktop.fullscreen ? sdl.vsync.when_fullscreen
	                                          : sdl.vsync.when_windowed;
	// Short-hand aliases
	auto &requested = vsync_pref.requested;
	auto &reported = vsync_pref.reported;
	auto &resultant = vsync_pref.resultant;

	assert(requested != VSYNC_STATE::UNSET);

	// We haven't assessed the reported and resultant states yet
	if (resultant == VSYNC_STATE::UNSET) {
		set_vsync(requested);
		resultant = get_resultant_vsync(vsync_pref.benchmarked_rate);
		reported = get_reported_vsync();
		if (requested != resultant) {
			DEBUG_LOG_MSG("SDL: Set the %s-mode vsync to %s, after which the driver told us vsync"
			              " was %s, but measurements proved it to be %s (benchmarked %d FPS)",
			              sdl.desktop.fullscreen ? "fullscreen" : "window",
			              vsync_state_as_string(requested),
			              vsync_state_as_string(reported),
			              vsync_state_as_string(resultant),
			              vsync_pref.benchmarked_rate);
		}
	}
	// Do we need to set the state?
	if (sdl.vsync.current != resultant) {
		set_vsync(requested);
		const auto new_reported = get_reported_vsync();
		if (new_reported != reported) {
			DEBUG_LOG_MSG("SDL: Set the %s-mode vsync to %s like before, but now the driver tells"
			              " us vsync was %s when previously it said vsync was %s",
			              sdl.desktop.fullscreen ? "fullscreen" : "window",
			              vsync_state_as_string(requested),
			              vsync_state_as_string(new_reported),
			              vsync_state_as_string(reported));
			resultant = get_resultant_vsync(vsync_pref.benchmarked_rate);
		}
		sdl.vsync.current = resultant;
	}
}

static void remove_window()
{
	if (sdl.window) {
		SDL_DestroyWindow(sdl.window);
		sdl.window = nullptr;
	}
}

// The throttled presenter skip frames that have an inter-frame spaces more
// narrow than the allowed frame period.
static void maybe_present_throttled(const bool frame_is_new)
{
	static bool last_frame_shown = false;
	if (!frame_is_new && last_frame_shown)
		return;

	const auto now = GetTicksUs();
	static int64_t last_present_time = 0;
	const auto elapsed = now - last_present_time;
	if (elapsed >= sdl.frame.period_us) {
		// If we waited beyond this frame's refresh period, then credit
		// this extra wait back by deducting it from the recorded time.
		const auto wait_overage = elapsed % sdl.frame.period_us;
		last_present_time = now - (9 * wait_overage / 10);
		last_frame_shown = sdl.frame.present();
	} else {
		last_frame_shown = false;
	}
}

static void maybe_present_synced(const bool present_if_last_skipped)
{
	// state tracking across runs
	static bool last_frame_shown = false;
	static int64_t last_sync_time = 0;

	const auto now = GetTicksUs();

	const auto scheduler_arrival = GetTicksDiff(now, last_sync_time);

	const auto on_time = scheduler_arrival > sdl.frame.period_us_early &&
	                     scheduler_arrival < sdl.frame.period_us_late;

	const auto should_present = on_time ||
	                            (present_if_last_skipped && !last_frame_shown);

	last_frame_shown = should_present ? sdl.frame.present() : false;

	last_sync_time = should_present ? GetTicksUs() : now;
}

static void schedule_synced([[maybe_unused]] const uint32_t event_id = 0)
{
	if (sdl.frame.mode != FRAME_MODE::SYNCED_CFR)
		return;

	constexpr bool present_if_last_skipped = false;
	maybe_present_synced(present_if_last_skipped);
	PIC_RemoveEvents(schedule_synced);
	PIC_AddEvent(schedule_synced, sdl.frame.period_ms);
}

static void setup_presentation_mode(FRAME_MODE &previous_mode)
{
	// Always get the reported refresh rate and hint the VGA side with it
	// This ensures the VGA side always has the host's rate to prior to
	// its next mode change.
	const auto host_rate = get_host_refresh_rate();
	if (host_rate >= REFRESH_RATE_MIN)
		VGA_SetHostRate(host_rate);
	const auto dos_rate = VGA_GetPreferredRate();

	// Frame rates are defined up to the 3rd decimal place, so compare on
	// the fourth.
	auto atleast_as_fast = [](const double a, const double b) {
		constexpr auto threshold = 0.0001;
		return a > b - threshold;
	};

	auto configure_cfr_mode = [&]() -> FRAME_MODE {
		if (atleast_as_fast(dos_rate, REFRESH_RATE_HOST_VRR_LFC) ||
		    !atleast_as_fast(host_rate, REFRESH_RATE_HOST_VRR_MIN)) {
			const auto lesser_rate = std::min(host_rate, dos_rate);
			save_rate_to_frame_period(lesser_rate);
			return atleast_as_fast(host_rate, dos_rate)
			               ? FRAME_MODE::CFR
			               : FRAME_MODE::SYNCED_CFR;
		}
		assert(!atleast_as_fast(dos_rate, REFRESH_RATE_HOST_VRR_LFC));
		const auto doubled_dos_rate = dos_rate * 2;
		const auto lesser_rate = std::min(doubled_dos_rate, host_rate);
		save_rate_to_frame_period(lesser_rate);
		return atleast_as_fast(host_rate, doubled_dos_rate)
		               ? FRAME_MODE::CFR
		               : FRAME_MODE::SYNCED_CFR;
	};

	auto configure_vfr_mode = [&]() {
		const auto bench_rate = get_vsync_preference().benchmarked_rate;
		const auto lesser_rate = std::min(host_rate, dos_rate);
		save_rate_to_frame_period(lesser_rate);
		const auto is_fast_enough = sdl.vsync.current != VSYNC_STATE::ON &&
		                            atleast_as_fast(bench_rate, dos_rate);
		const auto wont_hit_vsync_wall = sdl.vsync.current != VSYNC_STATE::OFF &&
		                                 atleast_as_fast(host_rate, dos_rate);
		return (is_fast_enough || wont_hit_vsync_wall)
		               ? FRAME_MODE::VFR
		               : FRAME_MODE::THROTTLED_VFR;
	};

	const bool wants_vsync = sdl.vsync.current == VSYNC_STATE::ON ||
	                         get_vsync_preference().requested == VSYNC_STATE::ON;

	// to be set below
	auto mode = FRAME_MODE::UNSET;

	// Manual full CFR
	if (sdl.frame.desired_mode == FRAME_MODE::CFR) {
		if (configure_cfr_mode() != FRAME_MODE::CFR && wants_vsync) {
			LOG_WARNING("SDL: CFR performance warning: the DOS rate of %2.5g"
			            " Hz exceeds the host's %2.5g Hz vsynced rate",
			            dos_rate, host_rate);
		}
		mode = sdl.frame.desired_mode;

	}
	// Manual full VFR
	else if (sdl.frame.desired_mode == FRAME_MODE::VFR) {
		if (configure_vfr_mode() != FRAME_MODE::VFR) {
			LOG_WARNING("SDL: VFR performance warning: the DOS rate of %2.5g"
			            " Hz exceeds the host's %2.5g Hz handling rate",
			            dos_rate, host_rate);
		}
		mode = sdl.frame.desired_mode;
	}
	// Auto VFR, if in a text mode with a non-VRR display
	else if (CurMode->type & M_TEXT_MODES &&
	         !atleast_as_fast(host_rate, REFRESH_RATE_HOST_VRR_MIN)) {
		mode = FRAME_MODE::VFR;
		save_rate_to_frame_period(dos_rate);
	}
	// Auto CFR
	else if (wants_vsync) {
		mode = configure_cfr_mode();
	}
	// Auto VFR
	else {
		mode = configure_vfr_mode();
	}

	// If the mode is unchanged, do nothing
	assert(mode != FRAME_MODE::UNSET);
	if (previous_mode == mode)
		return;
	previous_mode = mode;

	// Configure the pacer. We only use it for VFR modes because CFR modes
	// determine if the frame is presented based on the scheduler's accuracy.
	const auto is_vfr_mode = mode == FRAME_MODE::VFR ||
	                         mode == FRAME_MODE::THROTTLED_VFR;
	render_pacer.SetTimeout(is_vfr_mode ? sdl.vsync.skip_us : 0);

	// Start synced presentation, if applicable
	if (mode == FRAME_MODE::SYNCED_CFR)
		schedule_synced();
}

static void NewMouseScreenParams()
{
	if (sdl.clip.w <= 0 || sdl.clip.h <= 0 ||
	    sdl.clip.x < 0 || sdl.clip.y < 0) {
		// Filter out unusual parameters, which can be the result
		// of window minimized due to ALT+TAB, for example
		return;
	}

	int abs_x = 0;
	int abs_y = 0;
	SDL_GetMouseState(&abs_x, &abs_y);

	// When DPI scaling is enabled, mouse coordinates are reported on
	// "client points" grid, not physical pixels.
	MOUSE_NewScreenParams(
	        check_cast<uint32_t>(lround(sdl.clip.x / sdl.desktop.dpi_scale)),
	        check_cast<uint32_t>(lround(sdl.clip.y / sdl.desktop.dpi_scale)),
	        check_cast<uint32_t>(lround(sdl.clip.w / sdl.desktop.dpi_scale)),
	        check_cast<uint32_t>(lround(sdl.clip.h / sdl.desktop.dpi_scale)),
	        check_cast<int32_t>(abs_x),
	        check_cast<int32_t>(abs_y),
	        sdl.desktop.fullscreen);
}

static SDL_Window *SetWindowMode(SCREEN_TYPES screen_type,
                                 int width,
                                 int height,
                                 bool fullscreen,
                                 bool resizable)
{
	CleanupSDLResources();

	if (sdl.window && sdl.resizing_window) {
		int currWidth, currHeight;
		SDL_GetWindowSize(sdl.window, &currWidth, &currHeight);
		sdl.update_display_contents = ((width <= currWidth) && (height <= currHeight));
		return sdl.window;
	}

	static SCREEN_TYPES last_type = SCREEN_SURFACE;
	if (!sdl.window || (last_type != screen_type)) {

		last_type = screen_type;

		remove_window();

		uint32_t flags = opengl_driver_crash_workaround(screen_type);
		flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#if C_OPENGL
		if (screen_type == SCREEN_OPENGL)
			flags |= SDL_WINDOW_OPENGL;
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

		if (screen_type == SCREEN_TEXTURE) {
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
		if (screen_type == SCREEN_OPENGL) {
			if (sdl.opengl.context) {
				SDL_GL_DeleteContext(sdl.opengl.context);
				sdl.opengl.context = nullptr;
			}

			assert(sdl.opengl.context == nullptr);
			sdl.opengl.context = SDL_GL_CreateContext(sdl.window);
			if (sdl.opengl.context == nullptr) {
				LOG_ERR("SDL: OPENGL: Can't create OpenGL context: %s",
				        SDL_GetError());
				return nullptr;
			}
			if (SDL_GL_MakeCurrent(sdl.window, sdl.opengl.context) < 0) {
				LOG_ERR("SDL: OPENGL: Can't make OpenGL context current: %s",
				        SDL_GetError());
				return nullptr;
			}
		}
#endif
		if (resizable) {
			SDL_AddEventWatch(watch_sdl_events, sdl.window);
			SDL_SetWindowResizable(sdl.window, SDL_TRUE);
		}
		sdl.desktop.window.resizable = resizable;

		int window_width = 0;
		SDL_GetWindowSize(sdl.window, &window_width, nullptr);
		const auto canvas = get_canvas_size(screen_type);

		sdl.desktop.dpi_scale = static_cast<double>(canvas.w) / window_width;

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
			LOG_WARNING("SDL: Failed setting fullscreen mode to %dx%d at %d Hz", displayMode.w, displayMode.h, displayMode.refresh_rate);
		}
		SDL_SetWindowFullscreen(sdl.window,
		                        sdl.desktop.full.display_res
		                                ? SDL_WINDOW_FULLSCREEN_DESKTOP
		                                : SDL_WINDOW_FULLSCREEN);
	} else { // Windowd mode

		// Does our window still need sizing?
		int current_w, current_h;
		SDL_GetWindowSize(sdl.window, &current_w, &current_h);

		const bool window_is_too_small = (current_w < width ||
		                                  current_h < height);

		const bool window_dimensions_not_exact = (current_w != width ||
		                                          current_h != height);

		// Adjust the window dimensions if our window isn't sized yet or
		// we're in PP mode
		if (window_is_too_small || (sdl.scaling_mode == SCALING_MODE::PERFECT &&
		                            window_dimensions_not_exact)) {
			safe_set_window_size(width, height);
		}
		// If we're switching down from fullscreen, then it will use the set window size
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
	render_pacer.Reset();

	sdl.update_display_contents = true;
	return sdl.window;
}

// Used for the mapper UI and more: Creates a fullscreen window with desktop res
// on Android, and a non-fullscreen window with the input dimensions otherwise.
SDL_Window * GFX_SetSDLSurfaceWindow(uint16_t width, uint16_t height)
{
	constexpr bool fullscreen = false;
	return SetWindowMode(SCREEN_SURFACE, width, height, fullscreen, FIXED_SIZE);
}

// Returns the rectangle in the current window to be used for scaling a
// sub-window with the given dimensions, like the mapper UI.
SDL_Rect GFX_GetSDLSurfaceSubwindowDims(uint16_t width, uint16_t height)
{
	SDL_Rect rect;
	rect.x = rect.y = 0;
	rect.w = width;
	rect.h = height;
	return rect;
}

// Returns the actual output size in pixels, when possible.
// Needed for DPI-scaled windows, when logical window and actual output sizes
// might not match.
static SDL_Rect get_canvas_size(const SCREEN_TYPES screen_type)
{
	SDL_Rect canvas = {};

	switch (screen_type) {
	case SCREEN_SURFACE:
		SDL_GetWindowSize(sdl.window, &canvas.w, &canvas.h);
		break;
	case SCREEN_TEXTURE:
		if (SDL_GetRendererOutputSize(sdl.renderer, &canvas.w, &canvas.h) != 0)
			LOG_ERR("SDL: Failed to retrieve output size: %s",
			        SDL_GetError());
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
		SDL_GL_GetDrawableSize(sdl.window, &canvas.w, &canvas.h);
		break;
#endif
	}

	assert(canvas.w > 0 && canvas.h > 0);
	return canvas;
}

static SDL_Window *setup_window_pp(SCREEN_TYPES screen_type, bool resizable)
{
	assert(sdl.window);

	int window_width  = 0;
	int window_height = 0;
	SDL_GetWindowSize(sdl.window, &window_width, &window_height);
	assert(window_width > 0 && window_height > 0);

	if (!sdl.desktop.fullscreen) {
		window_width  = sdl.desktop.requested_window_bounds.width;
		window_height = sdl.desktop.requested_window_bounds.height;
	}

	const auto render_resolution = restrict_to_viewport_resolution(
	        iround(window_width * sdl.desktop.dpi_scale),
	        iround(window_height * sdl.desktop.dpi_scale));

	sdl.pp_scale = calc_pp_scale(render_resolution.x, render_resolution.y);

	const int img_width = sdl.pp_scale.output_w;
	const int img_height = sdl.pp_scale.output_h;

	int win_width;
	int win_height;
	if (sdl.use_exact_window_resolution && sdl.use_viewport_limits) {
		win_width  = window_width;
		win_height = window_height;
	} else {
		win_width  = (sdl.desktop.fullscreen
		                      ? window_width
		                      : iround(img_width / sdl.desktop.dpi_scale));
		win_height = (sdl.desktop.fullscreen
		                      ? window_height
		                      : iround(img_height / sdl.desktop.dpi_scale));
	}

	sdl.window = SetWindowMode(screen_type, win_width, win_height,
	                           sdl.desktop.fullscreen, resizable);
	return sdl.window;
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

static SDL_Rect calc_viewport_fit(int win_width, int win_height);

static SDL_Window *SetupWindowScaled(SCREEN_TYPES screen_type, bool resizable)
{
	if (sdl.scaling_mode == SCALING_MODE::PERFECT)
		return setup_window_pp(screen_type, resizable);

	int window_width;
	int window_height;
	if (sdl.desktop.fullscreen) {
		window_width = sdl.desktop.full.fixed ? sdl.desktop.full.width : 0;
		window_height = sdl.desktop.full.fixed ? sdl.desktop.full.height : 0;
	} else {
		window_width  = sdl.desktop.window.width;
		window_height = sdl.desktop.window.height;
	}

	if (window_width == 0 && window_height == 0) {
		window_width  = iround(sdl.draw.width * sdl.draw.scalex);
		window_height = iround(sdl.draw.height * sdl.draw.scaley);
	}

	sdl.window = SetWindowMode(screen_type,
	                           window_width,
	                           window_height,
	                           sdl.desktop.fullscreen,
	                           resizable);

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
static GLuint BuildShader(GLenum type, const std::string_view source_sv)
{
	GLuint shader = 0;
	GLint compiled = 0;

	assert(source_sv.length());
	const char *shaderSrc = source_sv.data();
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
	glShaderSource(shader, 2, src_strings, NULL);

	// Compile the shader
	glCompileShader(shader);

	// Check the compile status
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (!compiled) {
		GLint info_len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);

		if (info_len > 1) {
			std::vector<GLchar> info_log(info_len);
			glGetShaderInfoLog(shader, info_len, NULL, info_log.data());
			LOG_ERR("Error compiling shader: %s", info_log.data());
		}

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

static bool LoadGLShaders(const std::string_view source_sv, GLuint *vertex,
                          GLuint *fragment)
{
	if (source_sv.empty())
		return false;

	assert(vertex);
	assert(fragment);

	GLuint s = BuildShader(GL_VERTEX_SHADER, source_sv);
	if (s) {
		*vertex = s;
		s = BuildShader(GL_FRAGMENT_SHADER, source_sv);
		if (s) {
			*fragment = s;
			return true;
		}
		glDeleteShader(*vertex);
	}
	return false;
}
#endif


[[maybe_unused]] static std::string get_glshader_value()
{
#if C_OPENGL
	if (control) {
		const Section *rs = control->GetSection("render");
		assert(rs);
		return rs->GetPropValue("glshader");
	}
#endif // C_OPENGL
	return "";
}

[[maybe_unused]] static bool is_shader_flexible()
{
	constexpr std::array<std::string_view, 3> flexible_shader_names{{
	        "sharp",
	        "none",
	        "default",
	}};
	return contains(flexible_shader_names, get_glshader_value());
}

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

static void check_kmsdrm_setting()
{
	// Simple pre-check to see if we're using kmsdrm
	if (!is_using_kmsdrm_driver())
		return;

	// Do we have read access to the event subsystem
	if (auto f = fopen("/dev/input/event0", "r"); f) {
		fclose(f);
		return;
	}

	// We're using KMSDRM, but we don't have read access to the event subsystem
	LOG_WARNING("SDL: /dev/input/event0 is not readable, quitting early to prevent TTY input lockup.");
	LOG_WARNING("SDL: Please run: \"sudo usermod -aG input $(whoami)\", then re-login and try again.");
	exit(1);
}

// Some video modes are effectively doubled in resolution but only have half the
// unique pixel resolution.
static bool is_draw_size_doubled()
{
	// Has either dimension been doubled?
	const bool is_doubled = (sdl.draw.width_was_doubled ||
	                         sdl.draw.height_was_doubled);

	// Are the dimensions divisible by 2?
	const bool is_divisible = (sdl.draw.width % 2 == 0 &&
	                           sdl.draw.height % 2 == 0);

	// Are the dimensions beyond low-res?
	const bool is_large = (sdl.draw.width > 500 && sdl.draw.height > 350);

	return is_doubled && is_divisible && is_large;
}

static PPScale calc_pp_scale(const int avw, const int avh)
{
	return PPScale(sdl.draw.width, sdl.draw.height, sdl.draw.pixel_aspect,
	               is_draw_size_doubled(), avw, avh);
}

Bitu GFX_SetSize(int width,
                 int height,
                 const Bitu flags,
                 double scalex,
                 double scaley,
                 GFX_CallBack_t callback,
                 double pixel_aspect)
{
	Bitu retFlags = 0;
	if (sdl.updating)
		GFX_EndUpdate(nullptr);

	GFX_DisengageRendering();
	// The rendering objects are recreated below with new sizes, after which
	// frame rendering is re-engaged with the output-type specific calls.

	const bool double_width = flags & GFX_DBL_W;
	const bool double_height = flags & GFX_DBL_H;

	sdl.draw.has_changed = (sdl.draw.width != width || sdl.draw.height != height ||
	                        sdl.draw.width_was_doubled != double_width ||
	                        sdl.draw.height_was_doubled != double_height ||
	                        sdl.draw.scalex != scalex ||
	                        sdl.draw.scaley != scaley ||
	                        sdl.draw.pixel_aspect != pixel_aspect ||
	                        sdl.draw.previous_mode != CurMode->type);

	sdl.draw.width = width;
	sdl.draw.height = height;
	sdl.draw.width_was_doubled = double_width;
	sdl.draw.height_was_doubled = double_height;
	sdl.draw.scalex = scalex;
	sdl.draw.scaley = scaley;
	sdl.draw.pixel_aspect = pixel_aspect;
	sdl.draw.callback = callback;
	sdl.draw.previous_mode = CurMode->type;

	const auto wants_vsync = get_vsync_preference().requested != VSYNC_STATE::OFF;

	switch (sdl.desktop.want_type) {
dosurface:
	case SCREEN_SURFACE:
		if (sdl.desktop.fullscreen) {
			if (sdl.desktop.full.fixed) {
				sdl.clip.w = sdl.desktop.full.width;
				sdl.clip.h = sdl.desktop.full.height;
				sdl.clip.x = (sdl.desktop.full.width - width) / 2;
				sdl.clip.y = (sdl.desktop.full.height - height) / 2;
				sdl.window = SetWindowMode(SCREEN_SURFACE,
				                           sdl.clip.w, sdl.clip.h,
				                           sdl.desktop.fullscreen,
				                           FIXED_SIZE);
				if (sdl.window == nullptr) {
					E_Exit("Could not set fullscreen video mode %ix%i-%i: %s",
					       sdl.clip.w, sdl.clip.h, sdl.desktop.bpp,
					       SDL_GetError());
				}

				/* This may be required after an ALT-TAB leading to a window
				minimize, which further effectively shrinks its size */
				if ((sdl.clip.x < 0) || (sdl.clip.y < 0)) {
					sdl.update_display_contents = false;
				}
			} else {
				sdl.clip.w = width;
				sdl.clip.h = height;
				sdl.clip.x = 0;
				sdl.clip.y = 0;
				sdl.window = SetWindowMode(SCREEN_SURFACE,
				                           sdl.clip.w, sdl.clip.h,
				                           sdl.desktop.fullscreen,
				                           FIXED_SIZE);
				if (sdl.window == nullptr) {
					E_Exit("Could not set fullscreen video mode %ix%i-%i: %s",
					       sdl.clip.w, sdl.clip.h, sdl.desktop.bpp,
					       SDL_GetError());
				}
			}
		} else {
			sdl.clip.w = width;
			sdl.clip.h = height;
			sdl.clip.x = 0;
			sdl.clip.y = 0;
			sdl.window = SetWindowMode(SCREEN_SURFACE,
			                           sdl.clip.w, sdl.clip.h,
			                           sdl.desktop.fullscreen,
			                           FIXED_SIZE);
			if (sdl.window == nullptr) {
				E_Exit("Could not set windowed video mode %ix%i-%i: %s",
				       sdl.clip.w, sdl.clip.h, sdl.desktop.bpp,
				       SDL_GetError());
			}
		}
		sdl.surface = SDL_GetWindowSurface(sdl.window);
		if (sdl.surface == nullptr) {
			E_Exit("Could not retrieve window surface: %s",SDL_GetError());
		}
		switch (sdl.surface->format->BitsPerPixel) {
			case 8:
				retFlags = GFX_CAN_8;
				break;
			case 15:
				retFlags = GFX_CAN_15;
				break;
			case 16:
				retFlags = GFX_CAN_16;
				break;
			case 32:
				retFlags = GFX_CAN_32;
				break;
		}
		/* Fix a glitch with aspect=true occuring when
		changing between modes with different dimensions */
		SDL_FillRect(sdl.surface, NULL, SDL_MapRGB(sdl.surface->format, 0, 0, 0));
		SDL_UpdateWindowSurface(sdl.window);

		sdl.frame.update = update_frame_surface;
		sdl.frame.present = present_frame_noop; // surface presents during the update

		sdl.desktop.type = SCREEN_SURFACE;
		break; // SCREEN_SURFACE

	case SCREEN_TEXTURE: {
		SDL_SetHint(SDL_HINT_RENDER_VSYNC, wants_vsync ? "1" : "0");

		if (!SetupWindowScaled(SCREEN_TEXTURE, false)) {
			LOG_ERR("DISPLAY: Can't initialise 'texture' window");
			E_Exit("Creating window failed");
		}

		/* Use renderer's default format */
		SDL_RendererInfo rinfo;
		SDL_GetRendererInfo(sdl.renderer, &rinfo);
		const auto texture_format = rinfo.texture_formats[0];
		sdl.texture.texture = SDL_CreateTexture(sdl.renderer, texture_format,
		                                        SDL_TEXTUREACCESS_STREAMING, width, height);

		if (!sdl.texture.texture) {
			SDL_DestroyRenderer(sdl.renderer);
			sdl.renderer = nullptr;
			LOG_WARNING("SDL: Can't create texture, falling back to surface");
			goto dosurface;
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
			LOG_WARNING("SDL: Error while preparing texture input");
			goto dosurface;
		}

		SDL_SetRenderDrawColor(sdl.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		uint32_t pixel_format;
		assert(sdl.texture.texture);
		SDL_QueryTexture(sdl.texture.texture, &pixel_format, NULL, NULL, NULL);

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
			LOG_MSG("SDL: Using driver \"%s\" for texture renderer", rinfo.name);
			render_driver = rinfo.name;
		}

		if (!(rinfo.flags & SDL_RENDERER_ACCELERATED)) {
			retFlags |= GFX_CAN_RANDOM;
		}

		// Copied from the OpenGL path below; used to center texturepp output
		int window_width  = 0;
		int window_height = 0;
		SDL_GetWindowSize(sdl.window, &window_width, &window_height);

		const auto &desired_w = sdl.desktop.window.width;
		const auto &desired_h = sdl.desktop.window.height;
		const bool window_doesnt_match_desired = (desired_w != window_width ||
		                                          desired_h != window_height);
		const bool desired_size_is_valid = (desired_w > 0 && desired_h > 0);

		// Adjust the window size if needed and permitted
		if (sdl.scaling_mode != SCALING_MODE::PERFECT &&
		    window_doesnt_match_desired && desired_size_is_valid &&
		    !sdl.desktop.window.adjusted_initial_size) {
			sdl.desktop.window.adjusted_initial_size = true;
			safe_set_window_size(desired_w, desired_h);
		}

		const auto canvas = get_canvas_size(sdl.desktop.want_type);
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

		sdl.desktop.type = SCREEN_TEXTURE;
		break; // SCREEN_TEXTURE
	}
#if C_OPENGL
	case SCREEN_OPENGL: {
		if (sdl.opengl.pixel_buffer_object) {
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
			if (sdl.opengl.buffer) glDeleteBuffersARB(1, &sdl.opengl.buffer);
		} else {
			free(sdl.opengl.framebuf);
		}
		sdl.opengl.framebuf=0;
		if (!(flags & GFX_CAN_32))
			goto dosurface;

		int texsize_w, texsize_h;
		if (sdl.opengl.npot_textures_supported) {
			texsize_w = width;
			texsize_h = height;
		} else {
			texsize_w = 2 << int_log2(width);
			texsize_h = 2 << int_log2(height);
		}

		if (texsize_w > sdl.opengl.max_texsize ||
		    texsize_h > sdl.opengl.max_texsize) {
			LOG_WARNING("SDL:OPENGL: No support for texture size of %dx%d, falling back to surface",
			            texsize_w, texsize_h);
			goto dosurface;
		}
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		const std::string_view gl_vendor = safe_gl_get_string(GL_VENDOR,
		                                                      "unknown vendor");
#	if WIN32
		const auto is_vendors_srgb_unreliable = (gl_vendor == "Intel");
#	else
		constexpr auto is_vendors_srgb_unreliable = false;
#	endif
		if (is_vendors_srgb_unreliable) {
			LOG_WARNING("SDL:OPENGL: Not requesting an sRGB framebuffer"
			            " because %s's driver is unreliable",
			            gl_vendor.data());
		} else if (SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1)) {
			LOG_ERR("SDL:OPENGL: Failed requesting an sRGB framebuffer: %s",
			        SDL_GetError());
		}

		SetupWindowScaled(SCREEN_OPENGL, sdl.desktop.want_resizable_window);

		/* We may simply use SDL_BYTESPERPIXEL
		here rather than SDL_BITSPERPIXEL   */
		if (!sdl.window || SDL_BYTESPERPIXEL(SDL_GetWindowPixelFormat(sdl.window))<2) {
			LOG_WARNING("SDL:OPENGL: Can't open drawing window, are you running in 16bpp (or higher) mode?");
			goto dosurface;
		}

		SDL_GL_SetSwapInterval(wants_vsync);

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

					if (!LoadGLShaders(sdl.opengl.shader_source_sv,
					                   &vertexShader,
					                   &fragmentShader)) {
						LOG_ERR("SDL:OPENGL: Failed to compile shader!");
						goto dosurface;
					}

					sdl.opengl.program_object = glCreateProgram();
					if (!sdl.opengl.program_object) {
						glDeleteShader(vertexShader);
						glDeleteShader(fragmentShader);
						LOG_WARNING("SDL:OPENGL: Can't create program object, falling back to surface");
						goto dosurface;
					}
					glAttachShader(sdl.opengl.program_object, vertexShader);
					glAttachShader(sdl.opengl.program_object, fragmentShader);
					// Link the program
					glLinkProgram(sdl.opengl.program_object);
					// Even if we *are* successful, we may delete the shader objects
					glDeleteShader(vertexShader);
					glDeleteShader(fragmentShader);

					// Check the link status
					GLint isProgramLinked;
					glGetProgramiv(sdl.opengl.program_object, GL_LINK_STATUS, &isProgramLinked);
					if (!isProgramLinked) {
						GLint info_len = 0;
						glGetProgramiv(sdl.opengl.program_object, GL_INFO_LOG_LENGTH, &info_len);

						if (info_len > 1) {
							std::vector<GLchar> info_log(info_len);
							glGetProgramInfoLog(sdl.opengl.program_object, info_len, NULL, info_log.data());
							LOG_ERR("SDL:OPENGL: Error link program:\n %s", info_log.data());
						}
						glDeleteProgram(sdl.opengl.program_object);
						sdl.opengl.program_object = 0;
						goto dosurface;
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
		const auto framebuffer_bytes = static_cast<size_t>(width) *  height * MAX_BYTES_PER_PIXEL;
		if (sdl.opengl.pixel_buffer_object) {
			glGenBuffersARB(1, &sdl.opengl.buffer);
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, sdl.opengl.buffer);
			glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_EXT, framebuffer_bytes, NULL, GL_STREAM_DRAW_ARB);
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
		} else {
			sdl.opengl.framebuf = malloc(framebuffer_bytes); // 32 bit color
		}
		sdl.opengl.pitch=width*4;

		int window_width  = 0;
		int window_height = 0;
		SDL_GetWindowSize(sdl.window, &window_width, &window_height);

		const auto &desired_w = sdl.desktop.window.width;
		const auto &desired_h = sdl.desktop.window.height;
		const bool window_doesnt_match_desired = (desired_w != window_width ||
		                                          desired_h != window_height);
		const bool desired_size_is_valid = (desired_w > 0 && desired_h > 0);

		// Adjust the window size if needed and permitted
		if (sdl.scaling_mode != SCALING_MODE::PERFECT &&
		    window_doesnt_match_desired && desired_size_is_valid &&
		    !sdl.desktop.window.adjusted_initial_size) {
			sdl.desktop.window.adjusted_initial_size = true;
			safe_set_window_size(desired_w, desired_h);
		}

		const auto canvas = get_canvas_size(sdl.desktop.want_type);
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
		const auto wrap_parameter = is_shader_flexible()
		                                    ? GL_CLAMP_TO_EDGE
		                                    : GL_CLAMP_TO_BORDER;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_parameter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_parameter);

		const bool use_nearest_neighbour =
		        (sdl.scaling_mode == SCALING_MODE::NEAREST ||
		         sdl.scaling_mode == SCALING_MODE::PERFECT);
		const GLint filter = (use_nearest_neighbour ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);

		const auto texture_area_bytes = static_cast<size_t>(texsize_w) *
		                                texsize_h * MAX_BYTES_PER_PIXEL;
		uint8_t *emptytex = new uint8_t[texture_area_bytes];
		assert(emptytex);

		memset(emptytex, 0, texture_area_bytes);

		int is_framebuffer_srgb_capable = 0;
		if (SDL_GL_GetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,
		                        &is_framebuffer_srgb_capable))
			LOG_WARNING("OPENGL: Failed getting the framebuffer's sRGB status: %s",
			            SDL_GetError());

		sdl.opengl.framebuffer_is_srgb_encoded = RENDER_UseSRGBFramebuffer() && is_framebuffer_srgb_capable > 0;

		if (RENDER_UseSRGBFramebuffer() && !sdl.opengl.framebuffer_is_srgb_encoded)
			LOG_WARNING("OPENGL: sRGB framebuffer not supported");

		// Using GL_SRGB8_ALPHA8 because GL_SRGB8 doesn't work properly with Mesa drivers on certain integrated Intel GPUs
		const auto texformat = RENDER_UseSRGBTexture() && sdl.opengl.framebuffer_is_srgb_encoded
		                             ? GL_SRGB8_ALPHA8
		                             : GL_RGB8;
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
		}

		glClearColor (0.0f, 0.0f, 0.0f, 1.0f);

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
			glUniform2f(sdl.opengl.ruby.input_size, (GLfloat)width, (GLfloat)height);
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

		retFlags = GFX_CAN_32;
		if (sdl.opengl.pixel_buffer_object) {
			sdl.frame.update = update_frame_gl_pbo;
		} else {
			sdl.frame.update = update_frame_gl_fb;
			retFlags |= GFX_CAN_RANDOM;
		}
		// Both update mechanisms use the same presentation call
		sdl.frame.present = present_frame_gl;

		sdl.desktop.type = SCREEN_OPENGL;
		break; // SCREEN_OPENGL
	}
#endif // C_OPENGL
	}

	// Ensure mouse emulation knows the current parameters
	NewMouseScreenParams();
	update_vsync_state();

	if (sdl.draw.has_changed)
		log_display_properties(sdl.draw.width,
		                       sdl.draw.height,
		                       {},
		                       sdl.scaling_mode,
		                       sdl.desktop.type,
		                       sdl.pp_scale);

	if (retFlags)
		GFX_Start();
	return retFlags;
}

void GFX_SetShader([[maybe_unused]] const std::string &source)
{
#if C_OPENGL
	if (sdl.opengl.shader_source_sv != source)
		sdl.opengl.shader_source_sv = source;

	if (!sdl.opengl.use_shader)
		return;

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
	// Ensure auto-cycles are enabled
	CPU_Disable_SkipAutoAdjust();

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
		window_canvas_size = get_canvas_size(sdl.desktop.type);

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
	                         ? get_canvas_size(sdl.desktop.type)
	                         : window_canvas_size;

	log_display_properties(sdl.draw.width,
	                       sdl.draw.height,
	                       canvas_size,
	                       sdl.scaling_mode,
	                       sdl.desktop.type,
	                       sdl.pp_scale);

	sdl.desktop.switching_fullscreen = false;
}

static void SwitchFullScreen(bool pressed)
{
	if (pressed)
		GFX_SwitchFullScreen();
}

// This function returns write'able buffer for user to draw upon. Successful
// return depends on properly initialized SDL_Block structure (which generally
// can be achieved via GFX_SetSize call), and specifically - properly initialized
// output-specific bits (sdl.surface, sdl.texture, sdl.opengl.framebuf, or
// sdl.openg.pixel_buffer_object fields).
//
// If everything is prepared correctly, this function returns true, assigns
// 'pixels' output parameter to to a buffer (with format specified via earlier
// GFX_SetSize call), and assigns 'pitch' to a number of bytes used for a single
// pixels row in 'pixels' buffer.
//
bool GFX_StartUpdate(uint8_t * &pixels, int &pitch)
{
	if (!sdl.update_display_contents)
		return false;
	if (!sdl.active || sdl.updating)
		return false;

	switch (sdl.desktop.type) {
	case SCREEN_TEXTURE:
		assert(sdl.texture.input_surface);
		pixels = static_cast<uint8_t *>(sdl.texture.input_surface->pixels);
		pitch = sdl.texture.input_surface->pitch;
		sdl.updating = true;
		return true;
#if C_OPENGL
	case SCREEN_OPENGL:
		if (sdl.opengl.pixel_buffer_object) {
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, sdl.opengl.buffer);
			pixels = static_cast<uint8_t *>(glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, GL_WRITE_ONLY));
		} else {
			pixels = static_cast<uint8_t *>(sdl.opengl.framebuf);
		}
		OPENGL_ERROR("end of start update");
		if (pixels == nullptr)
			return false;
		static_assert(std::is_same<decltype(pitch), decltype((sdl.opengl.pitch))>::value,
		              "Our internal pitch types should be the same.");
		pitch = sdl.opengl.pitch;
		sdl.updating = true;
		return true;
#endif
	case SCREEN_SURFACE:
		assert(sdl.surface);
		pixels = static_cast<uint8_t *>(sdl.surface->pixels);
		pixels += sdl.clip.y * sdl.surface->pitch;
		pixels += sdl.clip.x * sdl.surface->format->BytesPerPixel;
		static_assert(std::is_same<decltype(pitch), decltype((sdl.surface->pitch))>::value,
		              "SDL internal surface pitch type should match our type.");
		pitch = sdl.surface->pitch;
		sdl.updating = true;
		return true;
	}
	return false;
}

void GFX_EndUpdate(const uint16_t *changedLines)
{
	sdl.frame.update(changedLines);

	const auto frame_is_new = sdl.update_display_contents && sdl.updating;

	switch (sdl.frame.mode) {
	case FRAME_MODE::CFR:
		maybe_present_synced(frame_is_new);
		break;
	case FRAME_MODE::VFR:
		if (frame_is_new)
			sdl.frame.present();
		break;
	case FRAME_MODE::THROTTLED_VFR:
		maybe_present_throttled(frame_is_new);
		break;
	// Synced CFR is started when the presetation mode is setup
	case FRAME_MODE::SYNCED_CFR:
	case FRAME_MODE::UNSET:
		break;
	}
	sdl.updating = false;
	FrameMark;
}

// Texture update and presentation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void update_frame_texture([[maybe_unused]] const uint16_t *changedLines)
{
	if (sdl.update_display_contents) {
		SDL_UpdateTexture(sdl.texture.texture,
		                  nullptr, // update entire texture
		                  sdl.texture.input_surface->pixels,
		                  sdl.texture.input_surface->pitch);
	}
}

static bool present_frame_texture()
{
	const auto is_presenting = render_pacer.CanRun();
	if (is_presenting) {
		SDL_RenderClear(sdl.renderer);
		SDL_RenderCopy(sdl.renderer, sdl.texture.texture, nullptr, nullptr);
		SDL_RenderPresent(sdl.renderer);
	}
	render_pacer.Checkpoint();
	return is_presenting;
}

// OpenGL PBO-based update, frame-based update, and presentation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#if C_OPENGL
static void update_frame_gl_pbo([[maybe_unused]] const uint16_t *changedLines)
{
	if (sdl.updating) {
		glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sdl.draw.width,
		                sdl.draw.height, GL_BGRA_EXT,
		                GL_UNSIGNED_INT_8_8_8_8_REV, 0);
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
	} else {
		sdl.opengl.actual_frame_count++;
	}
}

static void update_frame_gl_fb(const uint16_t *changedLines)
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
	const auto is_presenting = render_pacer.CanRun();
	if (is_presenting) {
		glClear(GL_COLOR_BUFFER_BIT);
		if (sdl.opengl.program_object) {
			glUniform1i(sdl.opengl.ruby.frame_count,
			            sdl.opengl.actual_frame_count++);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		} else {
			glCallList(sdl.opengl.displaylist);
		}
		SDL_GL_SwapWindow(sdl.window);
	}
	render_pacer.Checkpoint();
	return is_presenting;
}
#endif

// Surface update & presentation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void update_frame_surface([[maybe_unused]] const uint16_t *changedLines)
{
	// Changed lines are "streamed in" over multiple ticks - so important
	// we don't ignore or skip this content (otherwise parts of the image
	// won't be updated). So no matter, we always processed these changes,
	// even if we don't have to render a frame this pass.  The content is
	// updated in a persistent buffer.
	if (changedLines) {
		int y = 0;
		size_t index = 0;
		int16_t rect_count = 0;
		auto *rect = sdl.updateRects;
		assert(rect);
		while (y < sdl.draw.height) {
			if (index & 1) {
				rect->x = sdl.clip.x;
				rect->y = sdl.clip.y + y;
				rect->w = sdl.draw.width;
				rect->h = changedLines[index];
				rect++;
				rect_count++;
			}
			y += changedLines[index];
			index++;
		}
		if (rect_count) {
			SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects,
			                             rect_count);
		}
	}
}

Bitu GFX_GetRGB(uint8_t red,uint8_t green,uint8_t blue) {
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		return SDL_MapRGB(sdl.surface->format,red,green,blue);
	case SCREEN_TEXTURE:
		assert(sdl.texture.pixelFormat);
		return SDL_MapRGB(sdl.texture.pixelFormat, red, green, blue);
#if C_OPENGL
	case SCREEN_OPENGL:
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
		return SetWindowMode(sdl.desktop.want_type, sdl.desktop.full.width,
		                     sdl.desktop.full.height, sdl.desktop.fullscreen,
		                     sdl.desktop.want_resizable_window);
	}
	sdl.desktop.lazy_init_window_size = false;
	return SetWindowMode(sdl.desktop.want_type, sdl.desktop.window.width,
	                     sdl.desktop.window.height, sdl.desktop.fullscreen,
	                     sdl.desktop.want_resizable_window);
}

static bool detect_resizable_window()
{
	if (sdl.desktop.want_type == SCREEN_SURFACE) {
		LOG_WARNING("DISPLAY: Disabled resizable window, not compatible with surface output");
		return false;
	}
	return true;
}

static bool wants_stretched_pixels()
{
	const auto render_section = static_cast<Section_prop *>(
	        control->GetSection("render"));
	assert(render_section);

	return render_section->Get_bool("aspect");
}

static SDL_Point remove_stretched_aspect(const SDL_Point &size)
{
	return {size.x, ceil_sdivide(size.y * 5, 6)};
}

static SDL_Point refine_window_size(const SDL_Point &size,
                                    const SCALING_MODE scaling_mode,
                                    const bool should_stretch_pixels)
{
	switch (scaling_mode) {
	case (SCALING_MODE::NONE): {
		const auto game_ratios = should_stretch_pixels
		                                 ? RATIOS_FOR_STRETCHED_PIXELS
		                                 : RATIOS_FOR_SQUARE_PIXELS;

		const auto window_aspect = static_cast<double>(size.x) / size.y;
		const auto game_aspect = static_cast<double>(game_ratios.x) / game_ratios.y;

		// screen is wider than the game, so constrain horizonal
		if (window_aspect > game_aspect) {
			const int x = ceil_sdivide(size.y * game_ratios.x, game_ratios.y);
			return {x, size.y};
		} else {
			// screen is narrower than the game, so constrain vertical
			const int y = ceil_sdivide(size.x * game_ratios.y,
			                           game_ratios.x);
			return {size.x, y};
		}
	}
	case (SCALING_MODE::NEAREST): {
		constexpr SDL_Point resolutions[] = {
		        {7680, 5760}, // 8K  at 4:3 aspect
		        {7360, 5520}, //
		        {7040, 5280}, //
		        {6720, 5040}, //
		        {6400, 4800}, // HUXGA
		        {6080, 4560}, //
		        {5760, 4320}, // 8K "Full Format" at 4:3 aspect
		        {5440, 4080}, //
		        {5120, 3840}, // HSXGA at 4:3 aspect
		        {4800, 3600}, //
		        {4480, 3360}, //
		        {4160, 3120}, //
		        {3840, 2880}, // 4K UHD at 4:3 aspect
		        {3520, 2640}, //
		        {3200, 2400}, // QUXGA
		        {2880, 2160}, // 3K UHD at 4:3 aspect
		        {2560, 1920}, // 4.92M3 (Max CRT, Viewsonic P225f)
		        {2400, 1800}, //
		        {1920, 1440}, // 1080p in 4:3
		        {1600, 1200}, // UXGA
		        {1280, 960},  // 720p in 4:3
		        {1024, 768},  // XGA
		        {800, 600},   // SVGA
		};
		// Pick the biggest window size that fits inside the bounds.
		for (const auto &candidate : resolutions)
			if (candidate.x <= size.x && candidate.y <= size.y)
				return (should_stretch_pixels
				                ? candidate
				                : remove_stretched_aspect(candidate));
		break;
	}
	case (SCALING_MODE::PERFECT): {
		constexpr double aspect_weight = 1.14;
		constexpr SDL_Point pre_draw_size = {320, 200};
		const double pixel_aspect_ratio = should_stretch_pixels ? 1.2 : 1;

		int scale_x = 0;
		int scale_y = 0;
		const int err = pp_getscale(pre_draw_size.x, pre_draw_size.y,
		                            pixel_aspect_ratio, size.x, size.y,
		                            aspect_weight, &scale_x, &scale_y);
		if (err == 0)
			return {pre_draw_size.x * scale_x, pre_draw_size.y * scale_y};
		// else use the fallback below
		break;
	}
	}; // end-switch

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

	// SDL KMSDRM limitstaions:
	if (is_using_kmsdrm_driver()) {
		w = desktop.w;
		h = desktop.h;
		was_limited = true;
		LOG_WARNING("DISPLAY: Limitting %s resolution to '%dx%d' to avoid kmsdrm issues",
		            size_description, w, h);
	}

	if (!was_limited)
		LOG_INFO("DISPLAY: Accepted %s resolution %dx%d despite exceeding the %dx%d display",
		         size_description, w, h, desktop.w, desktop.h);
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

	LOG_WARNING("DISPLAY: Requested windowresolution '%s' is not valid, falling back to '%dx%d' instead",
	            pref.c_str(), FALLBACK_WINDOW_DIMENSIONS.x,
	            FALLBACK_WINDOW_DIMENSIONS.y);

	return FALLBACK_WINDOW_DIMENSIONS;
}

static SDL_Point window_bounds_from_label(const std::string &pref,
                                          const SDL_Rect &desktop)
{
	int percent = DEFAULT_WINDOW_PERCENT;
	if (starts_with("s", pref))
		percent = SMALL_WINDOW_PERCENT;
	else if (starts_with("m", pref) || pref == "default" || pref.empty())
		percent = MEDIUM_WINDOW_PERCENT;
	else if (starts_with("l", pref))
		percent = LARGE_WINDOW_PERCENT;
	else if (pref == "desktop")
		percent = 100;
	else
		LOG_WARNING("DISPLAY: Requested windowresolution '%s' is invalid, using 'default' instead",
		            pref.c_str());

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

	// TODO: Deprecate SURFACE output and remove this.
	if (sdl.desktop.want_type == SCREEN_SURFACE)
		return;

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
		LOG_WARNING("DISPLAY: Requested window_position '%s' was not in X,Y format, using 'auto' instead",
		            window_position_val.c_str());
		return;
	}

	const auto desktop = get_desktop_resolution();	
	const bool is_out_of_bounds = x < 0 || x > desktop.w || y < 0 ||
	                              y > desktop.h;
	if (is_out_of_bounds) {
		LOG_WARNING("DISPLAY: Requested window_position '%d,%d' is outside the bounds of the desktop '%dx%d', "
		            "using 'auto' instead",
		            x, y, desktop.w, desktop.h);
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
//  - The previously configured scaling mode: NONE, NEAREST, or PERFECT.
//  - If aspect correction (stretched pixels) is requested.

// Except for SURFACE rendering, this function returns a refined size and
// additionally populates the following struct members:
//  - 'sdl.desktop.requested_window_bounds', with the coarse bounds, which do
//     not take into account scaling or aspect correction.
//  - 'sdl.desktop.window', with the refined size.
//  - 'sdl.desktop.want_resizable_window', if the window can be resized.

// Refined sizes:
//  - If the user requested an exact resolution or 'desktop' size, then the
//    requested resolution is only trimed for aspect (4:3 or 8:5), unless
//    pixel-perfect is requested, in which case the resolution is adjusted down
//    toward to nearest exact integer multiple.

//  - If the user requested a relative size: small, medium, or large, then either
//    the closest fixed resolution is picked from a list or a pixel-perfect
//    resolution is calculated. In both cases, aspect correction is factored in.

static void setup_window_sizes_from_conf(const char *windowresolution_val,
                                         const SCALING_MODE scaling_mode,
                                         const bool should_stretch_pixels)
{
	// TODO: Deprecate SURFACE output and remove this.
	// For now, let the DOS-side determine the window's resolution.
	if (sdl.desktop.want_type == SCREEN_SURFACE) {
		// ensure our window sizes are populated
		save_window_size(FALLBACK_WINDOW_DIMENSIONS.x,
		                 FALLBACK_WINDOW_DIMENSIONS.y);
		return;
	}

	// Can the window be resized?
	sdl.desktop.want_resizable_window = detect_resizable_window();

	const auto desktop = get_desktop_resolution();

	// The refined scaling mode tell the refiner how it should adjust
	// the coarse-resolution.
	SCALING_MODE refined_scaling_mode = scaling_mode;
	auto drop_nearest = [scaling_mode]() {
		return (scaling_mode == SCALING_MODE::NEAREST) ? SCALING_MODE::NONE
		                                               : scaling_mode;
	};

	// Get the coarse resolution from the users setting, and adjust
	// refined scaling mode if an exact resolution is desired.
	const std::string pref = windowresolution_val;
	SDL_Point coarse_size = FALLBACK_WINDOW_DIMENSIONS;

	sdl.use_exact_window_resolution = pref.find('x') != std::string::npos;
	if (sdl.use_exact_window_resolution) {
		coarse_size = parse_window_resolution_from_conf(pref);
		refined_scaling_mode = drop_nearest();
	} else {
		coarse_size = window_bounds_from_label(pref, desktop);
		if (pref == "desktop") {
			refined_scaling_mode = drop_nearest();
		}
	}
	// Save the coarse bounds in the SDL struct for future sizing events
	sdl.desktop.requested_window_bounds = {coarse_size.x, coarse_size.y};

	// Refine the coarse resolution and save it in the SDL struct.
	auto refined_size = coarse_size;
	if (sdl.use_exact_window_resolution) {
		refined_size = clamp_to_minimum_window_dimensions(coarse_size);
	} else {
		refined_size = refine_window_size(coarse_size, refined_scaling_mode,
		                                  should_stretch_pixels);
	}
	assert(refined_size.x <= UINT16_MAX && refined_size.y <= UINT16_MAX);
	save_window_size(refined_size.x, refined_size.y);

	auto describe_scaling_mode = [scaling_mode]() -> const char * {
		switch (scaling_mode) {
		case SCALING_MODE::NONE: return "Bilinear";
		case SCALING_MODE::NEAREST: return "Nearest-neighbour";
		case SCALING_MODE::PERFECT: return "Pixel-perfect";
		}
		return "Unknown mode!";
	};

	// Let the user know the resulting window properties
	LOG_MSG("DISPLAY: Initialized %dx%d window-mode using %s scaling on %dp display-%d",
	        refined_size.x, refined_size.y, describe_scaling_mode(), desktop.h,
	        sdl.display_number);
}

static SDL_Rect calc_viewport_fit(const int win_w, const int win_h)
{
	assert(sdl.draw.width > 0);
	assert(sdl.draw.height > 0);
	assert(sdl.draw.scalex > 0.0);
	assert(sdl.draw.scaley > 0.0);
	assert(std::isfinite(sdl.draw.scalex));
	assert(std::isfinite(sdl.draw.scaley));

	// limit the window to the user's desired viewport, if configured
	const auto [lwin_w, lwin_h] = restrict_to_viewport_resolution(win_w, win_h);

	// calculate the aspect ratios of the draw buffer and the limited window
	const auto draw_aspect = (sdl.draw.width * sdl.draw.scalex) /
	                         (sdl.draw.height * sdl.draw.scaley);
	const auto lwin_aspect = static_cast<double>(lwin_w) / lwin_h;

	// calculate the viewport contingent on the aspect ratio of the limited
	// window versus draw buffer
	const auto lwin_is_wider = lwin_aspect > draw_aspect;
	const auto view_w = lwin_is_wider ? iround(lwin_h * draw_aspect) : lwin_w;
	const auto view_h = lwin_is_wider ? lwin_h : iround(lwin_w / draw_aspect);

	const int view_x = (win_w - view_w) / 2;
	const int view_y = (win_h - view_h) / 2;

	/*
	LOG_MSG("DISPLAY: %s win %4dx%4d, lwin %4dx%4d %1.2f"
	        " vs draw %1.2f => viewport=%3d,%3d %4dx%4d",
	        __FUNCTION__, win_w, win_h, lwin_w, lwin_h, lwin_aspect,
	        draw_aspect, view_x, view_y, view_w, view_h);
	*/
	return {view_x, view_y, view_w, view_h};
}

static SDL_Rect calc_viewport_pp(int win_width, int win_height)
{
	const auto render_resolution = restrict_to_viewport_resolution(win_width, win_height);
	sdl.pp_scale = calc_pp_scale(render_resolution.x, render_resolution.y);

	const int w = sdl.pp_scale.output_w;
	const int h = sdl.pp_scale.output_h;
	const int x = (win_width - w) / 2;
	const int y = (win_height - h) / 2;

	return {x, y, w, h};
}

[[maybe_unused]] static SDL_Rect calc_viewport(int width, int height)
{
	if (sdl.scaling_mode == SCALING_MODE::PERFECT)
		return calc_viewport_pp(width, height);
	else
		return calc_viewport_fit(width, height);
}

static void set_output(Section *sec, bool should_stretch_pixels)
{
	// Apply the user's mouse settings
	const auto section = static_cast<const Section_prop *>(sec);
	std::string output = section->Get_string("output");

	GFX_DisengageRendering();
	// it's the job of everything after this to re-engage it.

	if (output == "surface") {
		sdl.desktop.want_type = SCREEN_SURFACE;
	} else if (output == "texture") {
		sdl.desktop.want_type = SCREEN_TEXTURE;
		sdl.scaling_mode = SCALING_MODE::NONE;
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	} else if (output == "texturenb") {
		sdl.desktop.want_type = SCREEN_TEXTURE;
		sdl.scaling_mode = SCALING_MODE::NEAREST;
		// Currently the default, but... oh well
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	} else if (output == "texturepp") {
		sdl.desktop.want_type = SCREEN_TEXTURE;
		sdl.scaling_mode = SCALING_MODE::PERFECT;
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
#if C_OPENGL
	} else if (starts_with("opengl", output)) {
		RENDER_InitShaderSource(sec);
		if (output == "opengl") {
			sdl.desktop.want_type = SCREEN_OPENGL;
			sdl.scaling_mode      = SCALING_MODE::NONE;
			sdl.opengl.bilinear   = true;
		} else if (output == "openglnb") {
			sdl.desktop.want_type = SCREEN_OPENGL;
			sdl.scaling_mode      = SCALING_MODE::NEAREST;
			sdl.opengl.bilinear   = false;
		} else if (output == "openglpp") {
			sdl.desktop.want_type = SCREEN_OPENGL;
			sdl.scaling_mode      = SCALING_MODE::PERFECT;
			sdl.opengl.bilinear   = false;
		}
#endif
	} else {
		LOG_WARNING("SDL: Unsupported output device %s, switching back to surface",
		            output.c_str());
		sdl.desktop.want_type = SCREEN_SURFACE; // SHOULDN'T BE POSSIBLE
		                                        // anymore
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
			LOG_WARNING("SDL: Failed to set '%s' texture renderer driver; falling back to automatic selection",
			            sdl.render_driver.c_str());
		}
	}

	sdl.desktop.window.show_decorations = section->Get_bool("window_decorations");

	setup_initial_window_position_from_conf(
	        section->Get_string("window_position"));

	setup_viewport_resolution_from_conf(section->Get_string("viewport_resolution"));

	setup_window_sizes_from_conf(section->Get_string("windowresolution"),
	                             sdl.scaling_mode, should_stretch_pixels);

#if C_OPENGL
	if (sdl.desktop.want_type == SCREEN_OPENGL) { /* OPENGL is requested */
		if (!SetDefaultWindowMode()) {
			LOG_WARNING("Could not create OpenGL window, switching back to surface");
			sdl.desktop.want_type = SCREEN_SURFACE;
		} else {
			sdl.opengl.context = SDL_GL_CreateContext(sdl.window);
			if (sdl.opengl.context == 0) {
				LOG_WARNING("Could not create OpenGL context, switching back to surface");
				sdl.desktop.want_type = SCREEN_SURFACE;
			}
		}
		if (sdl.desktop.want_type == SCREEN_OPENGL) {
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

			sdl.opengl.buffer = 0;
			sdl.opengl.framebuf = 0;
			sdl.opengl.texture = 0;
			sdl.opengl.displaylist = 0;
			glGetIntegerv(GL_MAX_TEXTURE_SIZE, &sdl.opengl.max_texsize);

			glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)SDL_GL_GetProcAddress(
			        "glGenBuffersARB");
			glBindBufferARB = (PFNGLBINDBUFFERARBPROC)SDL_GL_GetProcAddress(
			        "glBindBufferARB");
			glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)
			        SDL_GL_GetProcAddress("glDeleteBuffersARB");
			glBufferDataARB = (PFNGLBUFFERDATAARBPROC)SDL_GL_GetProcAddress(
			        "glBufferDataARB");
			glMapBufferARB = (PFNGLMAPBUFFERARBPROC)SDL_GL_GetProcAddress(
			        "glMapBufferARB");
			glUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)SDL_GL_GetProcAddress(
			        "glUnmapBufferARB");
			const bool have_arb_buffers = glGenBuffersARB &&
			                              glBindBufferARB &&
			                              glDeleteBuffersARB &&
			                              glBufferDataARB &&
			                              glMapBufferARB &&
			                              glUnmapBufferARB;

			const auto gl_version_string = safe_gl_get_string(GL_VERSION,
			                                                  "0.0.0");
			const int gl_version_major = gl_version_string[0] - '0';

			sdl.opengl.pixel_buffer_object =
			        have_arb_buffers &&
			        SDL_GL_ExtensionSupported("GL_ARB_pixel_buffer_object");

			sdl.opengl.npot_textures_supported =
			        gl_version_major >= 2 ||
			        SDL_GL_ExtensionSupported(
			                "GL_ARB_texture_non_power_of_two");

			std::string npot_support_msg = sdl.opengl.npot_textures_supported
			                                       ? "supported"
			                                       : "not supported";
			if (sdl.opengl.npot_textures_supported &&
			    !is_shader_flexible()) {
				sdl.opengl.npot_textures_supported = false;
				npot_support_msg = "disabled to maximize compatibility with custom shader";
			}

			LOG_INFO("OPENGL: Vendor: %s",
			         safe_gl_get_string(GL_VENDOR, "unknown"));

			LOG_INFO("OPENGL: Version: %s", gl_version_string);

			LOG_INFO("OPENGL: GLSL version: %s",
			         safe_gl_get_string(GL_SHADING_LANGUAGE_VERSION,
			                            "unknown"));

			LOG_INFO("OPENGL: Pixel buffer object: %s",
			         sdl.opengl.pixel_buffer_object ? "available"
			                                        : "missing");
			LOG_INFO("OPENGL: NPOT textures: %s",
			         npot_support_msg.c_str());
		}
	} /* OPENGL is requested end */
#endif    // OPENGL

	if (!SetDefaultWindowMode())
		E_Exit("Could not initialize video: %s", SDL_GetError());

	const auto transparency = clamp(section->Get_int("transparency"), 0, 90);
	const auto alpha = static_cast<float>(100 - transparency) / 100.0f;
	SDL_SetWindowOpacity(sdl.window, alpha);
}

static std::optional<SDL_Surface *> get_rendered_surface()
{
	// Variables common to all screen-modes
	const auto renderer = SDL_GetRenderer(sdl.window);
	const auto canvas   = get_canvas_size(sdl.desktop.type);

#if C_OPENGL
	// Get the OpenGL-renderer surface
	// -------------------------------
	if (sdl.desktop.type == SCREEN_OPENGL) {
		// Setup our OpenGL image properties
		constexpr int gl_channels       = 3; // RBG (no alpha)
		constexpr int gl_bits_per_pixel = gl_channels * 8; // 8-bpp
		const size_t bytes_per_row      = gl_channels * canvas.w;

		// Allocate a 24-bit surface to be populated
		const auto surface = SDL_CreateRGBSurface(SDL_SWSURFACE,
		                                          canvas.w,
		                                          canvas.h,
		                                          gl_bits_per_pixel,
		                                          RMASK,
		                                          GMASK,
		                                          BMASK,
		                                          AMASK);
		if (!surface) {
			LOG_WARNING("SDL: Failed creating a surface for OpenGL because %s",
			            SDL_GetError());
			return {};
		}
		// Per OpenGL's documentation:
		// The glReadPixels function starts at the lower left corner: (x
		// + i, y + j) and iterates for 0 <= i < width and 0 <= j <
		// height. It describes the pxiels as being "the i'th pixel in
		// the j'th row". Pixels are returned in row-order from the
		// lowest to the highest row, left to right in each row.
		std::vector<uint8_t> pixels(bytes_per_row * canvas.h);
		glReadPixels(0, 0, canvas.w, canvas.h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

		// To match SDL's surface ordering, we invert the rows (outer)
		// and lines (inner):
		auto surface_pixels = static_cast<char *>(surface->pixels);
		for (int j = 0; j < canvas.h; ++j) {
			auto target_row = surface_pixels + surface->pitch * j;
			const auto source_row = pixels.data() +
			                        bytes_per_row * (canvas.h - j - 1);
			memcpy(target_row, source_row, bytes_per_row);
		}
		return surface;
	}
#endif

	// Get the SDL texture-renderer surface
	// ------------------------------------
	if (sdl.desktop.type == SCREEN_TEXTURE) {
		// Get the renderer's pixel format
		SDL_RendererInfo rinfo;
		if (SDL_GetRendererInfo(renderer, &rinfo) != 0) {
			LOG_MSG("SDL: Failed to get a rendering info because %s",
			        SDL_GetError());
			return {};
		}
		const auto pixel_format = rinfo.texture_formats[0];

		// Create a 32-bit surface with color format matching the renderer
		const auto surface = SDL_CreateRGBSurfaceWithFormat(
		        SDL_SWSURFACE, canvas.w, canvas.h, 32, pixel_format);
		if (!surface || !renderer) {
			LOG_WARNING("SDL: Failed creating a surface because %s",
			            SDL_GetError());
			return {};
		}
		// Copy the pixels from the renderer into our new surface
		if (SDL_RenderReadPixels(renderer,
		                         nullptr,
		                         surface->format->format,
		                         surface->pixels,
		                         surface->pitch) != 0) {
			LOG_WARNING("SDL: Failed rendering to the surface because %s",
			            SDL_GetError());
			SDL_FreeSurface(surface);
			return {};
		}
		return surface;
	}

	// We're already in surface-mode, how convenient ;)
	// -----------------------------------------------
	else if (sdl.desktop.type == SCREEN_SURFACE) {
		assert(sdl.surface);
		// Simply return a copy
		return SDL_ConvertSurfaceFormat(sdl.surface,
		                                sdl.surface->format->format,
		                                0);
	}

	LOG_WARNING("SDL: unhandled screen-type (bug)");
	return {};
}

static void screenshot_rendered_surface(bool pressed)
{
	if (!pressed)
		return;

	const auto surface = get_rendered_surface();
	if (!surface)
		return;

#if C_SDL_IMAGE
	const auto filename = CAPTURE_GetScreenshotFilename("Screenshot", ".png");
	const auto is_saved = IMG_SavePNG(*surface, filename.c_str()) == 0;
#else
	const auto filename = CAPTURE_GetScreenshotFilename("Screenshot", ".bmp");
	const auto is_saved = SDL_SaveBMP(*surface, filename.c_str()) == 0;
#endif
	SDL_FreeSurface(*surface);

	if (is_saved)
		LOG_MSG("SDL: Captured rendered output to %s", filename.c_str());
	else
		LOG_MSG("SDL: Failed capturing rendered output to %s because %s",
		        filename.c_str(),
		        SDL_GetError());
}

// extern void UI_Run(bool);
void Restart(bool pressed);

static void ApplyActiveSettings()
{
	SetPriority(sdl.priority.active);

	if (sdl.mute_when_inactive && !MIXER_IsManuallyMuted())
		MIXER_SetState(MixerState::On);
}

static void ApplyInactiveSettings()
{
	SetPriority(sdl.priority.inactive);

	if (sdl.mute_when_inactive && !MIXER_IsManuallyMuted())
		MIXER_SetState(MixerState::Off);
}

static void SetPriorityLevels(const std::string_view active_pref,
                              const std::string_view inactive_pref)
{
	auto to_level = [](const std::string_view pref) {
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
	sdl.updating = false;
	sdl.update_display_contents = true;
	sdl.resizing_window = false;
	sdl.wait_on_error = section->Get_bool("waitonerror");

	sdl.desktop.fullscreen=section->Get_bool("fullscreen");

	auto priority_conf = section->GetMultiVal("priority")->GetSection();
	SetPriorityLevels(priority_conf->Get_string("active"),
	                  priority_conf->Get_string("inactive"));

	sdl.mute_when_inactive  = section->Get_bool("mute_when_inactive");
	sdl.pause_when_inactive = section->Get_bool("pause_when_inactive");

	// Adjust the fallback resolution based on the user's aspect-correction
	const auto should_stretch_pixels = wants_stretched_pixels();
	FALLBACK_WINDOW_DIMENSIONS = should_stretch_pixels ? SDL_Point{640, 480}
	                                                   : SDL_Point{640, 400};

	ApplyActiveSettings(); // Assume focus on startup
	sdl.desktop.full.fixed=false;
	const char* fullresolution=section->Get_string("fullresolution");
	sdl.desktop.full.width  = 0;
	sdl.desktop.full.height = 0;
	if(fullresolution && *fullresolution) {
		char res[100];
		safe_strcpy(res, fullresolution);
		fullresolution = lowcase (res);//so x and X are allowed
		if (strcmp(fullresolution,"original")) {
			sdl.desktop.full.fixed = true;
			if (strcmp(fullresolution,"desktop")) { // desktop uses 0x0, below sets a custom WxH
				char* height = const_cast<char*>(strchr(fullresolution,'x'));
				if (height && * height) {
					*height = 0;
					sdl.desktop.full.height = atoi(height + 1);
					sdl.desktop.full.width  = atoi(res);
					maybe_limit_requested_resolution(
					        sdl.desktop.full.width,
					        sdl.desktop.full.height,
					        "fullscreen");
				}
			}
		}
	}

	const std::string host_rate_pref = section->Get_string("host_rate");
	if (host_rate_pref == "auto")
		sdl.desktop.host_rate_mode = HOST_RATE_MODE::AUTO;
	else if (host_rate_pref == "sdi")
		sdl.desktop.host_rate_mode = HOST_RATE_MODE::SDI;
	else if (host_rate_pref == "vrr")
		sdl.desktop.host_rate_mode = HOST_RATE_MODE::VRR;
	else {
		const auto rate = to_finite<double>(host_rate_pref);
		if (std::isfinite(rate) && rate >= REFRESH_RATE_MIN) {
			sdl.desktop.host_rate_mode = HOST_RATE_MODE::CUSTOM;
			sdl.desktop.preferred_host_rate = rate;
		} else {
			LOG_WARNING("SDL: Invalid 'host_rate' value: '%s', using 'auto'",
			            host_rate_pref.c_str());
			sdl.desktop.host_rate_mode = HOST_RATE_MODE::AUTO;
		}
	}

	sdl.vsync.when_windowed.requested = VSYNC_STATE::OFF;
	sdl.vsync.when_fullscreen.requested = section->Get_bool("vsync")
	                                              ? VSYNC_STATE::ON
	                                              : VSYNC_STATE::OFF;
	sdl.vsync.skip_us = section->Get_int("vsync_skip");


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
		sdl.frame.desired_mode = FRAME_MODE::UNSET;
	else if (presentation_mode_pref == "cfr")
		sdl.frame.desired_mode = FRAME_MODE::CFR;
	else if (presentation_mode_pref == "vfr")
		sdl.frame.desired_mode = FRAME_MODE::VFR;
	else {
		sdl.frame.desired_mode = FRAME_MODE::UNSET;
		LOG_WARNING("SDL: Invalid 'presentation_mode' value: '%s'",
		            presentation_mode_pref.c_str());
	}

	sdl.desktop.full.display_res = sdl.desktop.full.fixed && (!sdl.desktop.full.width || !sdl.desktop.full.height);
	if (sdl.desktop.full.display_res) {
		GFX_ObtainDisplayDimensions();
	}

	set_output(section, should_stretch_pixels);

	SDL_SetWindowTitle(sdl.window, "DOSBox Staging");
	SetIcon();

	/* Get some Event handlers */
	MAPPER_AddHandler(GFX_RequestExit, SDL_SCANCODE_F9, PRIMARY_MOD,
	                  "shutdown", "Shutdown");
	MAPPER_AddHandler(screenshot_rendered_surface,
	                  SDL_SCANCODE_F5,
	                  MMOD2,
	                  "rendshot",
	                  "Rend Screenshot");
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
		switch (button) {
		case SDL_BUTTON_LEFT:   MOUSE_EventButton(0, pressed); break;
		case SDL_BUTTON_RIGHT:  MOUSE_EventButton(1, pressed); break;
		case SDL_BUTTON_MIDDLE: MOUSE_EventButton(2, pressed); break;
		case SDL_BUTTON_X1:     MOUSE_EventButton(3, pressed); break;
		case SDL_BUTTON_X2:     MOUSE_EventButton(4, pressed); break;
		}
	};

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
	const auto section = static_cast<const Section_prop *>(sec);
	if (strcmp(section->Get_string("output"), "surface"))
		remove_window();
	set_output(sec, wants_stretched_pixels());
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

	if (sdl.desktop.window.resizable) {
		const auto canvas = get_canvas_size(sdl.desktop.type);
		sdl.clip          = calc_viewport(canvas.w, canvas.h);
		if (sdl.desktop.type == SCREEN_TEXTURE) {
			SDL_RenderSetViewport(sdl.renderer, &sdl.clip);
		}
#if C_OPENGL
		if (sdl.desktop.type == SCREEN_OPENGL) {
			glViewport(sdl.clip.x, sdl.clip.y, sdl.clip.w, sdl.clip.h);
			glUniform2f(sdl.opengl.ruby.output_size,
			            (GLfloat)sdl.clip.w,
			            (GLfloat)sdl.clip.h);
		}
#endif // C_OPENGL

		if (!sdl.desktop.fullscreen)
			save_window_size(width, height);

		// Window resize might have been triggered by the OS setting DPI scale,
		// so recalculate that.
		sdl.desktop.dpi_scale = static_cast<double>(canvas.w) / width;

		// Ensure mouse emulation knows the current parameters
		NewMouseScreenParams();

		return;
	}

	/* Even if the new window's dimensions are actually the desired ones
	 * we may still need to re-obtain a new window surface or do
	 * a different thing. So we basically reset the screen, but without
	 * touching the window itself (or else we may end in an infinite loop).
	 *
	 * Furthermore, if the new dimensions are *not* the desired ones, we
	 * don't fight it. Rather than attempting to resize it back, we simply
	 * keep the window as-is and disable screen updates. This is done
	 * in SDL_SetSDLWindowSurface by setting sdl.update_display_contents
	 * to false.
	 */
	sdl.resizing_window = true;
	GFX_ResetScreen();
	sdl.resizing_window = false;

	// Ensure mouse emulation knows the current parameters
	NewMouseScreenParams();
}

/* This function is triggered after window is shown to fixup sdl.window
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
	if (MAPPER_IsUsingJoysticks()) {
		static auto last_check_joystick = GetTicks();
		auto current_check_joystick = GetTicks();
		if (GetTicksDiff(current_check_joystick, last_check_joystick) > 20) {
			last_check_joystick = current_check_joystick;
			SDL_JoystickUpdate();
			MAPPER_UpdateJoysticks();
		}
	}
#endif
	while (SDL_PollEvent(&event)) {
#if C_DEBUG
		if (isDebuggerEvent(event)) {
			pdc_event_queue.push(event);
			continue;
		}
#endif
		switch (event.type) {
		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_RESTORED:
				// DEBUG_LOG_MSG("SDL: Window has been restored");
				/* We may need to re-create a texture
				 * and more on Android. Another case:
				 * Update surface while using X11.
				 */
				GFX_ResetScreen();
				ApplyActiveSettings();
				FocusInput();
				continue;

			case SDL_WINDOWEVENT_RESIZED:
				// DEBUG_LOG_MSG("SDL: Window has been resized to %dx%d",
				//               event.window.data1,
				//               event.window.data2);
				HandleVideoResize(event.window.data1,
				                  event.window.data2);
				sdl.desktop.last_size_event = SDL_WINDOWEVENT_RESIZED;
				continue;

			case SDL_WINDOWEVENT_FOCUS_GAINED:
			case SDL_WINDOWEVENT_EXPOSED:
				// DEBUG_LOG_MSG("SDL: Window has been exposed "
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
				// DEBUG_LOG_MSG("SDL: Window has gained
				// keyboard focus");
				if (sdl.draw.callback)
					sdl.draw.callback(GFX_CallBackRedraw);
				MOUSE_NotifyHasFocus(true);
				ApplyActiveSettings();
				FocusInput();
				continue;

			case SDL_WINDOWEVENT_FOCUS_LOST:
				// DEBUG_LOG_MSG("SDL: Window has lost keyboard focus");
#ifdef WIN32
				if (sdl.desktop.fullscreen) {
					VGA_KillDrawing();
					GFX_ForceFullscreenExit();
				}
#endif
				ApplyInactiveSettings();
				GFX_LosingFocus();
				CPU_Enable_SkipAutoAdjust();
				MOUSE_NotifyHasFocus(false);
				break;

			case SDL_WINDOWEVENT_ENTER:
				// DEBUG_LOG_MSG("SDL: Window has gained mouse focus");
				continue;

			case SDL_WINDOWEVENT_LEAVE:
				// DEBUG_LOG_MSG("SDL: Window has lost mouse focus");
				continue;

			case SDL_WINDOWEVENT_SHOWN:
				// DEBUG_LOG_MSG("SDL: Window has been shown");
				continue;

			case SDL_WINDOWEVENT_HIDDEN:
				// DEBUG_LOG_MSG("SDL: Window has been hidden");
				continue;

#if C_OPENGL && defined(MACOSX)
			case SDL_WINDOWEVENT_MOVED:
				// DEBUG_LOG_MSG("SDL: Window has been moved to %d, %d",
				//               event.window.data1,
				//               event.window.data2);
				if (sdl.desktop.type == SCREEN_OPENGL) {
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
				int win_w = 0;
				SDL_GetWindowSize(sdl.window, &win_w, nullptr);
				const auto canvas = get_canvas_size(sdl.desktop.type);
				assert(win_w > 0 && canvas.w > 0 && canvas.h > 0);

				sdl.desktop.dpi_scale = static_cast<double>(canvas.w) /
				                        win_w;

				SDL_Rect display_bounds = {};
				SDL_GetDisplayBounds(event.window.data1,
				                     &display_bounds);
				sdl.desktop.full.width  = display_bounds.w;
				sdl.desktop.full.height = display_bounds.h;

				sdl.display_number = event.window.data1;

				sdl.clip = calc_viewport(canvas.w, canvas.h);
				if (sdl.desktop.type == SCREEN_TEXTURE) {
					SDL_RenderSetViewport(sdl.renderer,
					                      &sdl.clip);
				}
#	if C_OPENGL
				if (sdl.desktop.type == SCREEN_OPENGL) {
					glViewport(sdl.clip.x,
					           sdl.clip.y,
					           sdl.clip.w,
					           sdl.clip.h);
				}
#	endif
				NewMouseScreenParams();
				continue;
			}
#endif

			case SDL_WINDOWEVENT_SIZE_CHANGED:
				// DEBUG_LOG_MSG("SDL: The window size has changed");

				// differentiate this size change versus resizing.
				sdl.desktop.last_size_event = SDL_WINDOWEVENT_SIZE_CHANGED;

				// The window size has changed either as a
				// result of an API call or through the system
				// or user changing the window size.
				FinalizeWindowState();
				continue;

			case SDL_WINDOWEVENT_MINIMIZED:
				// DEBUG_LOG_MSG("SDL: Window has been minimized");
				ApplyInactiveSettings();
				break;

			case SDL_WINDOWEVENT_MAXIMIZED:
				// DEBUG_LOG_MSG("SDL: Window has been maximized");
				continue;

			case SDL_WINDOWEVENT_CLOSE:
				// DEBUG_LOG_MSG("SDL: The window manager "
				//               "requests that the window be "
				//               "closed");
				GFX_RequestExit(true);
				break;

			case SDL_WINDOWEVENT_TAKE_FOCUS:
				FocusInput();
				ApplyActiveSettings();
				continue;

			case SDL_WINDOWEVENT_HIT_TEST:
				// DEBUG_LOG_MSG("SDL: Window had a hit test that "
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

					while (paused) {
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
									paused = false;
									GFX_RefreshTitle();
									ApplyActiveSettings();
									CPU_Disable_SkipAutoAdjust();
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
static BOOL WINAPI ConsoleEventHandler(DWORD event) {
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

static std::vector<std::string> Get_SDL_TextureRenderers()
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

void config_add_sdl() {
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
	Pbool->Set_help("Start directly in fullscreen.\n"
	                "Run INTRO and see Special Keys for window control hotkeys.");

	pint = sdl_sec->Add_int("display", on_start, 0);
	pint->Set_help("Number of display to use; values depend on OS and user "
	               "settings.");

	Pstring = sdl_sec->Add_string("fullresolution", always, "desktop");
	Pstring->Set_help("What resolution to use for fullscreen: 'original', 'desktop'\n"
	                  "or a fixed size (e.g. 1024x768).");

	pstring = sdl_sec->Add_string("windowresolution", on_start, "default");
	pstring->Set_help(
	        "Set window size when running in windowed mode:\n"
	        "  default:   Select the best option based on your\n"
	        "             environment and other settings.\n"
	        "  small, medium, or large (or s, m, l):\n"
	        "             Size the window relative to the desktop.\n"
	        "  <custom>:  Scale the window to the given dimensions in\n"
	        "             WxH format. For example: 1024x768.\n"
	        "             Scaling is not performed for output=surface.");

	pstring = sdl_sec->Add_path("viewport_resolution", always, "fit");
	pstring->Set_help(
	        "Set the viewport size (drawable area) within the window/screen:\n"
	        "  fit:       Fit the viewport to the available window/screen (default).\n"
	        "  <custom>:  Limit the viewport within to a custom resolution or percentage of\n"
	        "             the desktop. Specified in WxH, N%, N.M%. Examples: 960x720 or 50%");

	pstring = sdl_sec->Add_string("window_position", always, "auto");
	pstring->Set_help(
	        "Set initial window position when running in windowed mode:\n"
	        "  auto:      Let the window manager decide the position.\n"
	        "  <custom>:  Set window position in X,Y format. For example: 250,100\n"
	        "             0,0 is the top-left corner of the screen.");

	Pbool = sdl_sec->Add_bool("window_decorations", always, true);
	Pbool->Set_help("Controls whether to display window decorations in windowed mode.");

	Pint = sdl_sec->Add_int("transparency", always, 0);
	Pint->Set_help("Set the transparency of the DOSBox Staging screen.\n"
	               "From 0 (no transparency) to 90 (high transparency).");

	pstring = sdl_sec->Add_path("max_resolution", deprecated, "");
	pstring->Set_help("This setting has been renamed to viewport_resolution.");

	pstring = sdl_sec->Add_string("host_rate", on_start, "auto");
	pstring->Set_help(
	        "Set the host's refresh rate:\n"
	        "  auto:      Use SDI rates, or VRR rates when fullscreen on a high-refresh\n"
	        "             display.\n"
	        "  sdi:       Use serial device interface (SDI) rates, without further\n"
	        "             adjustment.\n"
	        "  vrr:       Deduct 3 Hz from the reported rate (best-practice for VRR\n"
	        "             displays).\n"
	        "  <custom>:  Specify a custom rate as a whole or decimal value greater than\n"
	        "             23.000.");

	Pbool = sdl_sec->Add_bool("vsync", on_start, false);
	Pbool->Set_help(
	        "Synchronize with display refresh rate if supported. This can\n"
	        "reduce flickering and tearing, but may also impact performance.");

	pint = sdl_sec->Add_int("vsync_skip", on_start, 7000);
	pint->Set_help("Number of microseconds to allow rendering to block before skipping the next\n"
	               "frame. 0 disables this and will always render.");
	pint->SetMinMax(0, 14000);

	const char *presentation_modes[] = {"auto", "cfr", "vfr", 0};
	pstring = sdl_sec->Add_string("presentation_mode", always, "auto");
	pstring->Set_help(
	        "Optionally select the frame presentation mode:\n"
	        "  auto:  Intelligently time and drop frames to prevent\n"
	        "         emulation stalls, based on host and DOS frame rates.\n"
	        "  cfr:   Always present DOS frames at a constant frame rate.\n"
	        "  vfr:   Always present changed DOS frames at a variable frame rate.");
	pstring->Set_values(presentation_modes);

	const char *outputs[] =
	{ "surface",
	  "texture",
	  "texturenb",
	  "texturepp",
#if C_OPENGL
	  "opengl",
	  "openglnb",
	  "openglpp",
#endif
	  0 };

#if C_OPENGL
	Pstring = sdl_sec->Add_string("output", always, "opengl");
#else
	Pstring = sdl_sec->Add_string("output", always, "texture");
#endif
	Pstring->Set_help("What video system to use for output.");
	Pstring->Set_values(outputs);

	pstring = sdl_sec->Add_string("texture_renderer", always, "auto");
	pstring->Set_help("Choose a renderer driver when using a texture output mode.\n"
	                  "Use texture_renderer=auto for an automatic choice.");
	pstring->Set_values(Get_SDL_TextureRenderers());

	Pmulti = sdl_sec->AddMultiVal("capture_mouse", deprecated, ",");
	Pmulti->Set_help("Moved to [mouse] section.");
	Pmulti = sdl_sec->AddMultiVal("sensitivity", deprecated, ",");
	Pmulti->Set_help("Moved to [mouse] section.");
	pbool = sdl_sec->Add_bool("raw_mouse_input", deprecated, false);
	pbool->Set_help("Moved to [mouse] section.");

	Pbool = sdl_sec->Add_bool("waitonerror", always, true);
	Pbool->Set_help("Keep the console open if an error has occurred.");

	Pmulti = sdl_sec->AddMultiVal("priority", always, " ");
	Pmulti->SetValue("auto auto");
	Pmulti->Set_help("Priority levels to apply when active and inactive, respectively. \n"
	                 "   auto:  Let the host operating system manage the priority (valid for both).\n"
	                 "Default is: 'auto auto'");

	const char *priority_level_choices[] = {
	        "auto",
	        "lowest",
	        "lower",
	        "normal",
	        "higher",
	        "highest",
	        0,
	};
	psection = Pmulti->GetSection();
	psection->Add_string("active", always, priority_level_choices[0])
	        ->Set_values(priority_level_choices);
	psection->Add_string("inactive", always, priority_level_choices[0])
	        ->Set_values(priority_level_choices);

	pbool = sdl_sec->Add_bool("mute_when_inactive", on_start, false);
	pbool->Set_help("Mute the sound when the window is inactive.");

	pbool = sdl_sec->Add_bool("pause_when_inactive", on_start, false);
	pbool->Set_help("Pause emulation when the window is inactive.");

	pstring = sdl_sec->Add_path("mapperfile", always, MAPPERFILE);
	pstring->Set_help(
	        "File used to load/save the key/event mappings.\n"
	        "Pre-configured maps are bundled in the 'resources/mapperfiles' directory.\n"
	        "They can be loaded by name, for example: mapperfile = xbox/xenon2.map\n"
	        "Note: the -resetmapper commandline flag only deletes the default mapperfile.");

	pstring = sdl_sec->Add_string("screensaver", on_start, "auto");
	pstring->Set_help(
	        "Use 'allow' or 'block' to override the SDL_VIDEO_ALLOW_SCREENSAVER\n"
	        "environment variable (which usually blocks the OS screensaver\n"
	        "while the emulator is running).");
	const char *ssopts[] = {"auto", "allow", "block", 0};
	pstring->Set_values(ssopts);
}

static int LaunchEditor()
{
	std::string path, file;
	Cross::CreatePlatformConfigDir(path);
	Cross::GetPlatformConfigName(file);
	path += file;

	FILE *f = fopen(path.c_str(), "r");
	if (!f && !control->PrintConfig(path)) {
		fprintf(stderr, "Tried creating '%s', but failed.\n", path.c_str());
		return 1;
	}
	if (f)
		fclose(f);

	auto replace_with_process = [path](const std::string &prog) {
		execlp(prog.c_str(), prog.c_str(), path.c_str(), (char *)nullptr);
	};

	std::string editor;
	constexpr bool remove_arg = true;
	// Loop until one succeeds
	while (control->cmdline->FindString("--editconf", editor, remove_arg))
		replace_with_process(editor);

	while (control->cmdline->FindString("-editconf", editor, remove_arg))
		replace_with_process(editor);

	const char *env_editor = getenv("EDITOR");
	if (env_editor)
		replace_with_process(env_editor);

	replace_with_process("nano");
	replace_with_process("vim");
	replace_with_process("vi");
	replace_with_process("notepad++.exe");
	replace_with_process("notepad.exe");

	fprintf(stderr, "Can't find any text editors.\n"
	                "You can set the EDITOR env variable to your preferred "
	                "text editor.\n");
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
	for(Bitu i = 0; i < parameters.size(); i++) newargs[i] = (char*)parameters[i].c_str();
	newargs[parameters.size()] = NULL;
	MIXER_CloseAudioDevice();
	Delay(50);
	QuitSDL();
#if C_DEBUG
	// shutdown curses
	DEBUG_ShutDown(NULL);
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

static void launchcaptures(const std::string& edit)
{
	std::string path,file;
	Section* t = control->GetSection("dosbox");
	if(t) file = t->GetPropValue("captures");
	if(!t || file == NO_SUCH_PROPERTY) {
		printf("Config system messed up.\n");
		exit(1);
	}
	Cross::CreatePlatformConfigDir(path);
	path += file;

	if (create_dir(path.c_str(), 0700, OK_IF_EXISTS) != 0) {
		fprintf(stderr, "Can't access capture dir '%s': %s\n",
		        path.c_str(), safe_strerror(errno).c_str());
		exit(1);
	}

	execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);
	//if you get here the launching failed!
	fprintf(stderr, "can't find filemanager %s\n", edit.c_str());
	exit(1);
}

static void ListGlShaders()
{
#if C_OPENGL
	for (const auto &line : RENDER_InventoryShaders())
		printf("%s\n", line.c_str());
#else
	LOG_ERR("OpenGL is not supported by this executable "
	        "and is missing the functionality to list shaders");
#endif
}

static int PrintConfigLocation()
{
	std::string path, file;
	Cross::CreatePlatformConfigDir(path);
	Cross::GetPlatformConfigName(file);
	path += file;

	FILE *f = fopen(path.c_str(), "r");
	if (!f && !control->PrintConfig(path)) {
		fprintf(stderr, "Tried creating '%s', but failed.\n", path.c_str());
		return 1;
	}
	if (f)
		fclose(f);
	printf("%s\n", path.c_str());
	return 0;
}

static void eraseconfigfile() {
	FILE* f = fopen("dosbox.conf","r");
	if(f) {
		fclose(f);
		LOG_WARNING("Warning: dosbox.conf exists in current working directory.\nThis will override the configuration file at runtime.\n");
	}
	std::string path,file;
	Cross::GetPlatformConfigDir(path);
	Cross::GetPlatformConfigName(file);
	path += file;
	f = fopen(path.c_str(),"r");
	if(!f) exit(0);
	fclose(f);
	unlink(path.c_str());
	exit(0);
}

static void erasemapperfile() {
	FILE* g = fopen("dosbox.conf","r");
	if(g) {
		fclose(g);
		LOG_WARNING("Warning: dosbox.conf exists in current working directory.\nKeymapping might not be properly reset.\n"
		             "Please reset configuration as well and delete the dosbox.conf.\n");
	}

	std::string path,file=MAPPERFILE;
	Cross::GetPlatformConfigDir(path);
	path += file;
	FILE* f = fopen(path.c_str(),"r");
	if(!f) exit(0);
	fclose(f);
	unlink(path.c_str());
	exit(0);
}

void OverrideWMClass()
{
#if !defined(WIN32)
	constexpr int overwrite = 0; // don't overwrite
	setenv("SDL_VIDEO_X11_WMCLASS", CANONICAL_PROJECT_NAME, overwrite);
#endif
}

void GFX_GetSize(int &width, int &height, bool &fullscreen)
{
	width = sdl.draw.width;
	height = sdl.draw.height;
	fullscreen = sdl.desktop.fullscreen;
}

extern "C" int SDL_CDROMInit(void);
int sdl_main(int argc, char *argv[])
{
	CommandLine com_line(argc, argv);
	control = std::make_unique<Config>(&com_line);

	if (control->cmdline->FindExist("--version") ||
	    control->cmdline->FindExist("-version") ||
	    control->cmdline->FindExist("-v")) {
		printf(version_msg, CANONICAL_PROJECT_NAME, DOSBOX_GetDetailedVersion());
		return 0;
	}

	if (control->cmdline->FindExist("--help") ||
	    control->cmdline->FindExist("-help") ||
	    control->cmdline->FindExist("-h")) {
		printf(help_msg); // -V618
		return 0;
	}

	// Setup logging after commandline is parsed and trivial arguments handled
	loguru::g_preamble_date    = true; // The date field
	loguru::g_preamble_time    = true; // The time of the current day
	loguru::g_preamble_uptime  = false; // The time since init call
	loguru::g_preamble_thread  = false; // The logging thread
	loguru::g_preamble_file    = false; // The file from which the log originates from
	loguru::g_preamble_verbose = false; // The verbosity field
	loguru::g_preamble_pipe    = true; // The pipe symbol right before the message	

	loguru::init(argc, argv);

	LOG_MSG("%s version %s", CANONICAL_PROJECT_NAME, DOSBOX_GetDetailedVersion());
	LOG_MSG("---");

	LOG_MSG("LOG: Loguru version %d.%d.%d initialized", LOGURU_VERSION_MAJOR,
	        LOGURU_VERSION_MINOR, LOGURU_VERSION_PATCH);

	int rcode = 0; // assume good until proven otherwise
	try {
		std::string working_dir;
		constexpr bool remove_arg = true;
		if (control->cmdline->FindString("--working-dir", working_dir, remove_arg) ||
		    control->cmdline->FindString("-working-dir", working_dir, remove_arg)) {
			std::error_code ec;
			std::filesystem::current_path(working_dir, ec);
			if (ec) {
				LOG_ERR("Cannot set working directory to %s",
				        working_dir.c_str());
			}
		}

		OverrideWMClass(); // Before SDL2 video subsystem is initialized

		CROSS_DetermineConfigPaths();

		/* Init the configuration system and add default values */
		messages_add_sdl();
		config_add_sdl();
		DOSBOX_Init();

		if (control->cmdline->FindExist("--editconf") ||
		    control->cmdline->FindExist("-editconf")) {
			const int err = LaunchEditor();
			return err;
		}

		std::string editor;
		if(control->cmdline->FindString("-opencaptures",editor,true)) launchcaptures(editor);
		if(control->cmdline->FindExist("-eraseconf")) eraseconfigfile();
		if(control->cmdline->FindExist("-resetconf")) eraseconfigfile();
		if(control->cmdline->FindExist("-erasemapper")) erasemapperfile();
		if(control->cmdline->FindExist("-resetmapper")) erasemapperfile();

		/* Can't disable the console with debugger enabled */
#if defined(WIN32) && !(C_DEBUG)
		if (control->cmdline->FindExist("-noconsole")) {
			FreeConsole();
			/* Redirect standard input and standard output */
			if(freopen(STDOUT_FILE, "w", stdout) == NULL)
				no_stdout = true; // No stdout so don't write messages
			freopen(STDERR_FILE, "w", stderr);
			setvbuf(stdout, NULL, _IOLBF, BUFSIZ); // Line buffered
			setvbuf(stderr, NULL, _IONBF, BUFSIZ); // No buffering
		} else {
			if (AllocConsole()) {
				fclose(stdin);
				fclose(stdout);
				fclose(stderr);
				freopen("CONIN$","r",stdin);
				freopen("CONOUT$","w",stdout);
				freopen("CONOUT$","w",stderr);
			}
			SetConsoleTitle("DOSBox Status Window");
		}
#endif  //defined(WIN32) && !(C_DEBUG)

		if (control->cmdline->FindExist("--printconf") ||
		    control->cmdline->FindExist("-printconf")) {
			const int err = PrintConfigLocation();
			return err;
		}

		if (control->cmdline->FindExist("--list-glshaders") ||
		    control->cmdline->FindExist("-list-glshaders")) {
			ListGlShaders();
			return 0;
		}

#if defined(WIN32)
	SetConsoleCtrlHandler((PHANDLER_ROUTINE) ConsoleEventHandler,TRUE);

#	if SDL_VERSION_ATLEAST(2, 23, 0)
	if (SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2") == SDL_FALSE)
		LOG_WARNING("SDL: Failed to set DPI awareness flag");
	if (SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1") == SDL_FALSE)
		LOG_WARNING("SDL: Failed to set DPI scaling flag");
#	endif
#endif

	check_kmsdrm_setting();

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0)
		E_Exit("Can't init SDL %s", SDL_GetError());
	if (SDL_CDROMInit() < 0)
		LOG_WARNING("Failed to init CD-ROM support");

	sdl.initialized = true;
	// Once initialized, ensure we clean up SDL for all exit conditions
	atexit(QuitSDL);

	LOG_MSG("SDL: version %d.%d.%d initialized (%s video and %s audio)",
		SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL,
		SDL_GetCurrentVideoDriver(), SDL_GetCurrentAudioDriver());

	const auto config_path = CROSS_GetPlatformConfigDir();
	SETUP_ParseConfigFiles(config_path);

	MSG_Add("PROGRAM_CONFIG_PROPERTY_ERROR", "No such section or property: %s\n");
	MSG_Add("PROGRAM_CONFIG_NO_PROPERTY",
		"There is no property \"%s\" in section \"%s\".\n");
	MSG_Add("PROGRAM_CONFIG_SET_SYNTAX",
		"Correct syntax: config -set \"[section] property=value\".\n");
	std::string line;
	while (control->cmdline->FindString("-set", line, true)) {
		trim(line);
		if (line.empty()
		    || line[0] == '%' || line[0] == '\0'
		    || line[0] == '#' || line[0] == '\n')
			continue;
		std::vector<std::string> pvars(1, std::move(line));
		const char *result = SetProp(pvars);
		if (strlen(result))
			LOG_WARNING("%s", result);
		else {
			Section *tsec = control->GetSection(pvars[0]);
			std::string value(pvars[2]);
			// Due to parsing there can be a = at the start of value.
			while (value.size() &&
			       (value.at(0) == ' ' || value.at(0) == '='))
				value.erase(0, 1);
			for (Bitu i = 3; i < pvars.size(); i++)
				value += (std::string(" ") + pvars[i]);
			std::string inputline = pvars[1] + "=" + value;
			bool change_success = tsec->HandleInputline(
				inputline.c_str());
			if (!change_success && !value.empty())
				LOG_WARNING("Cannot set \"%s\"\n",
					    inputline.c_str());
		}
	}

#if C_OPENGL
	const std::string glshaders_dir = config_path + "glshaders";
	if (create_dir(glshaders_dir.c_str(), 0700, OK_IF_EXISTS) != 0)
		LOG_WARNING("CONFIG: Can't create dir '%s': %s",
			    glshaders_dir.c_str(), safe_strerror(errno).c_str());
#endif // C_OPENGL
#if C_FLUIDSYNTH
	const std::string soundfonts_dir = config_path + "soundfonts";
	if (create_dir(soundfonts_dir.c_str(), 0700, OK_IF_EXISTS) != 0)
		LOG_WARNING("CONFIG: Can't create dir '%s': %s",
			    soundfonts_dir.c_str(), safe_strerror(errno).c_str());
#endif // C_FLUIDSYNTH
#if C_MT32EMU
	const std::string mt32_rom_dir = config_path + "mt32-roms";
	if (create_dir(mt32_rom_dir.c_str(), 0700, OK_IF_EXISTS) != 0)
		LOG_WARNING("CONFIG: Can't create dir '%s': %s",
			    mt32_rom_dir.c_str(), safe_strerror(errno).c_str());
#endif // C_MT32EMU

		control->ParseEnv();
//		UI_Init();
//		if (control->cmdline->FindExist("-startui")) UI_Run(false);
		/* Init all the sections */
		control->Init();
		/* Some extra SDL Functions */
		Section_prop * sdl_sec=static_cast<Section_prop *>(control->GetSection("sdl"));

		if (control->cmdline->FindExist("-fullscreen") ||
		    sdl_sec->Get_bool("fullscreen")) {
			if(!sdl.desktop.fullscreen) { //only switch if not already in fullscreen
				GFX_SwitchFullScreen();
			}
		}

		// All subsystems' hotkeys need to be registered at this point
		// to ensure their hotkeys appear in the graphical mapper.
		MAPPER_BindKeys(sdl_sec);
		if (control->cmdline->FindExist("-startmapper"))
			MAPPER_DisplayUI();

		control->StartUp(); // Run the machine until shutdown
		control.reset();  // Shutdown and release

	} catch (char *error) {
		rcode = 1;
		GFX_ShowMsg("Exit to error: %s",error);
		fflush(NULL);
		if(sdl.wait_on_error) {
			//TODO Maybe look for some way to show message in linux?
#if (C_DEBUG)
			GFX_ShowMsg("Press enter to continue");
			fflush(NULL);
			fgetc(stdin);
#elif defined(WIN32)
			Sleep(5000);
#endif
		}
	} catch (const std::exception &e) {
		// catch all exceptions that derive from the standard library
		LOG_ERR("EXCEPTION: Standard library exception: %s", e.what());
		rcode = 1;
	} catch (...) {
		// just exit
		rcode = 1;
	}

#if defined (WIN32)
	sticky_keys(true); //Might not be needed if the shutdown function switches to windowed mode, but it doesn't hurt
#endif

	// We already do this at exit, but do cleanup earlier in case of normal
	// exit; this works around problems when atexit order clashes with SDL2
	// cleanup order. Happens with SDL_VIDEODRIVER=wayland as of SDL 2.0.12.
	QuitSDL();

	return rcode;
}
