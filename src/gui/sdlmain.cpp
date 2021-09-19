/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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
#ifdef WIN32
#include <signal.h>
#include <process.h>
#endif

#include <SDL.h>
#if C_OPENGL
#include <SDL_opengl.h>
#endif

#include "control.h"
#include "cpu.h"
#include "cross.h"
#include "debug.h"
#include "fs_utils.h"
#include "gui_msgs.h"
#include "../ints/int10.h"
#include "joystick.h"
#include "keyboard.h"
#include "mapper.h"
#include "mouse.h"
#include "pacer.h"
#include "pic.h"
#include "render.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"
#include "timer.h"
#include "vga.h"
#include "video.h"

#include "../libs/ppscale/ppscale.h"

#define MAPPERFILE "mapper-sdl2-" VERSION ".map"

#if C_OPENGL
//Define to disable the usage of the pixel buffer object
//#define DB_DISABLE_DBO
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

SDL_bool mouse_is_captured = SDL_FALSE; // global for mapper

// SDL allows pixels sizes (color-depth) from 1 to 4 bytes
constexpr uint8_t MAX_BYTES_PER_PIXEL = 4;

// Masks to be passed when creating SDL_Surface.
// Remove ifndef if they'll be needed for MacOS X builds.
#ifndef MACOSX
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
#endif // !MACOSX

enum MouseControlType {
	CaptureOnClick = 1 << 0,
	CaptureOnStart = 1 << 1,
	Seamless       = 1 << 2,
	NoMouse        = 1 << 3
};

enum SCREEN_TYPES	{
	SCREEN_SURFACE,
	SCREEN_TEXTURE,
#if C_OPENGL
	SCREEN_OPENGL
#endif
};

enum class SCALING_MODE { NONE, NEAREST, PERFECT };

// Size and ratio constants
// ------------------------
constexpr int SMALL_WINDOW_PERCENT = 50;
constexpr int MEDIUM_WINDOW_PERCENT = 70;
constexpr int LARGE_WINDOW_PERCENT = 90;
constexpr int DEFAULT_WINDOW_PERCENT = MEDIUM_WINDOW_PERCENT;
constexpr SDL_Point FALLBACK_WINDOW_DIMENSIONS = {640, 480};
constexpr SDL_Point RATIOS_FOR_STRETCHED_PIXELS = {4, 3};
constexpr SDL_Point RATIOS_FOR_SQUARE_PIXELS = {8, 5};

enum PRIORITY_LEVELS {
	PRIORITY_LEVEL_AUTO,
	PRIORITY_LEVEL_PAUSE,
	PRIORITY_LEVEL_LOWEST,
	PRIORITY_LEVEL_LOWER,
	PRIORITY_LEVEL_NORMAL,
	PRIORITY_LEVEL_HIGHER,
	PRIORITY_LEVEL_HIGHEST
};

/* Alias for indicating, that new window should not be user-resizable: */
constexpr bool FIXED_SIZE = false;

struct SDL_Block {
	bool initialized = false;
	bool active = false; // If this isn't set don't draw
	bool updating = false;
	bool update_display_contents = true;
	bool resizing_window = false;
	bool wait_on_error = false;
	SCALING_MODE scaling_mode = SCALING_MODE::NONE;
	struct {
		int width = 0;
		int height = 0;
		double scalex = 1.0;
		double scaley = 1.0;
		bool has_changed = false;
		double pixel_aspect = 1.0;
		GFX_CallBack_t callback = nullptr;
	} draw = {};
	struct {
		struct {
			Bit16u width = 0;
			Bit16u height = 0;
			bool fixed = false;
			bool display_res = false;
		} full = {};
		struct {
			uint16_t width = 0; // TODO convert to int
			uint16_t height = 0; // TODO convert to int
			bool resizable = false;
		} window = {};
		struct {
			int width = 0;
			int height = 0;
		} requested_window_bounds = {};

		Bit8u bpp = 0;
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
		bool vsync = false;
		int vsync_skip = 0;
		bool want_resizable_window = false;
		SDL_WindowEventID last_size_event = {};
		SCREEN_TYPES type;
		SCREEN_TYPES want_type;
	} desktop = {};
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
		bool packed_pixel;
		bool paletted_texture;
		bool pixel_buffer_object = false;
		bool use_shader;
		GLuint program_object;
		const char *shader_src;
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
		PRIORITY_LEVELS focus;
		PRIORITY_LEVELS nofocus;
	} priority = {};
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
		int xsensitivity = 0;
		int ysensitivity = 0;
		int sensitivity = 0;
		MouseControlType control_choice = Seamless;
		bool middle_will_release = true;
		bool has_focus = false;
	} mouse = {};
	SDL_Point pp_scale = {1, 1};
	SDL_Rect updateRects[1024];
#if defined (WIN32)
	// Time when sdl regains focus (Alt+Tab) in windowed mode
	int64_t focus_ticks = 0;
#endif
	// State of Alt keys for certain special handlings
	SDL_EventType laltstate = SDL_KEYUP;
	SDL_EventType raltstate = SDL_KEYUP;
};

static SDL_Block sdl;

static SDL_Point calc_pp_scale(int width, int heigth);
static SDL_Rect calc_viewport(int width, int height);

static void CleanupSDLResources();
static void HandleVideoResize(int width, int height);

#if C_OPENGL
static char const shader_src_default[] = R"GLSL(
varying vec2 v_texCoord;
#if defined(VERTEX)
uniform vec2 rubyTextureSize;
uniform vec2 rubyInputSize;
attribute vec4 a_position;
void main() {
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize/rubyTextureSize;
}
#elif defined(FRAGMENT)
uniform sampler2D rubyTexture;
void main() {
	gl_FragColor = texture2D(rubyTexture, v_texCoord);
}
#endif
)GLSL";

#ifdef DB_OPENGL_ERROR
void OPENGL_ERROR(const char* message) {
	GLenum r = glGetError();
	if (r == GL_NO_ERROR) return;
	LOG_MSG("errors from %s",message);
	do {
		LOG_MSG("%X",r);
	} while ( (r=glGetError()) != GL_NO_ERROR);
}
#else
void OPENGL_ERROR(const char*) {
	return;
}
#endif
#endif

static void QuitSDL()
{
	if (sdl.initialized)
		SDL_Quit();
}

extern const char* RunningProgram;
extern bool CPU_CycleAutoAdjust;
//Globals for keyboard initialisation
bool startup_state_numlock=false;
bool startup_state_capslock=false;

void GFX_SetTitle(Bit32s cycles, int /*frameskip*/, bool paused)
{
	char title[200] = {0};

#if !defined(NDEBUG)
	const char* build_type = " (debug build)";
#else
	const char* build_type = "";
#endif

	static Bit32s internal_cycles = 0;
	if (cycles != -1)
		internal_cycles = cycles;

	const char *msg = CPU_CycleAutoAdjust
	                          ? "%8s - max %d%% - DOSBox Staging%s%s"
	                          : "%8s - %d cycles/ms - DOSBox Staging%s%s";
	snprintf(title, sizeof(title), msg, RunningProgram, internal_cycles,
	         build_type, paused ? " (PAUSED)" : "");
	SDL_SetWindowTitle(sdl.window, title);
}

int GFX_GetDisplayRefreshRate()
{
	constexpr auto invalid_refresh = -1;
	assert(sdl.window);
	const int display_in_use = SDL_GetWindowDisplayIndex(sdl.window);
	if (display_in_use < 0) {
		LOG_ERR("SDL: Could not get the current window index: %s", SDL_GetError());
		return invalid_refresh;
	}

	SDL_DisplayMode mode;
	if (SDL_GetCurrentDisplayMode(display_in_use, &mode) != 0) {
		LOG_ERR("SDL: Could not get the current display mode: %s", SDL_GetError());
		return invalid_refresh;
	}
	if (mode.refresh_rate < 23 || mode.refresh_rate > 500) {
		LOG_ERR("SDL: Got an unexpected refresh rate of %d Hz; not using it", mode.refresh_rate);
		return invalid_refresh;
	}

	return mode.refresh_rate;
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
	SDL_SetWindowIcon(sdl.window, s);
	SDL_FreeSurface(s);
}

#endif

static void RequestExit(bool pressed)
{
	shutdown_requested = pressed;
	if (shutdown_requested)
		DEBUG_LOG_MSG("SDL: Exit requested");
}

MAYBE_UNUSED static void PauseDOSBox(bool pressed)
{
	if (!pressed)
		return;
	const auto inkeymod = static_cast<uint16_t>(SDL_GetModState());

	GFX_SetTitle(-1,-1,true);
	bool paused = true;
	Delay(500);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		// flush event queue.
	}
	/* NOTE: This is one of the few places where we use SDL key codes
	with SDL 2.0, rather than scan codes. Is that the correct behavior? */
	while (paused) {
		SDL_WaitEvent(&event);    // since we're not polling, cpu usage drops to 0.
		switch (event.type) {
		case SDL_QUIT: RequestExit(true); break;

		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESTORED) {
				// We may need to re-create a texture and more
				GFX_ResetScreen();
			}
			break;
		case SDL_KEYDOWN: // Must use Pause/Break Key to resume.
		case SDL_KEYUP:
			if(event.key.keysym.sym == SDLK_PAUSE) {
				const uint16_t outkeymod = event.key.keysym.mod;
				if (inkeymod != outkeymod) {
					KEYBOARD_ClrBuffer();
					MAPPER_LosingFocus();
					//Not perfect if the pressed alt key is switched, but then we have to
					//insert the keys into the mapper or create/rewrite the event and push it.
					//Which is tricky due to possible use of scancodes.
				}
				paused = false;
				GFX_SetTitle(-1,-1,false);
				break;
			}
#if defined (MACOSX)
			if (event.key.keysym.sym == SDLK_q &&
			   (event.key.keysym.mod == KMOD_RGUI || event.key.keysym.mod == KMOD_LGUI)) {
				/* On macs, all aps exit when pressing cmd-q */
				RequestExit(true);
				break;
			}
#endif
		}
	}
}

/* Reset the screen with current values in the sdl structure */
Bitu GFX_GetBestMode(Bitu flags)
{
	if (sdl.scaling_mode == SCALING_MODE::PERFECT)
		flags |= GFX_UNITY_SCALE;
	switch (sdl.desktop.want_type) {
	case SCREEN_SURFACE:
check_surface:
		flags &= ~GFX_LOVE_8;		//Disable love for 8bpp modes
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
		if (!(flags&GFX_CAN_32)) goto check_surface;
		flags|=GFX_SCALING;
		flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
		break;
	default:
		goto check_surface;
		break;
	}
	return flags;
}


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

MAYBE_UNUSED static int int_log2(int val)
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
                                    const bool wants_stretched_pixels);

// Logs the source and target resolution including describing scaling method
// and pixel-aspect ratios. Note that this function deliberately doesn't use
// any global structs to disentangle it from the existing sdl-main design.
static void log_display_properties(const int in_x,
                                   const int in_y,
                                   const double in_par,
                                   const SCALING_MODE scaling_mode,
                                   const SDL_Point pp_scale,
                                   const bool is_fullscreen,
                                   int out_x,
                                   int out_y)
{
	// Sanity check expectations
	assert(in_x > 0 && in_y > 0 && in_par > 0);
	assert(out_x > 0 && out_y > 0);

	if (scaling_mode == SCALING_MODE::PERFECT) {
		// If we're using pixel perfect, then the incoming'out_x' and
		// 'height' arguments only represent the total drawing area as
		// opposed to the internal clipped area, so use this approach to
		// get the actual scaled dimentions.
		out_x = pp_scale.x * in_x;
		out_y = pp_scale.y * in_y;
	} else if (is_fullscreen) {
		// If we're fullscreen and using a non-pixel-perfect scaling
		// mode, then the incoming'out_x' and 'out_y' arguments only
		// represent the total display resolution as opposed to the 4:3
		// or 8:5 area, so use this approach to get the actual scaled
		// dimentions.
		const auto fs = refine_window_size({out_x, out_y},
		                                   SCALING_MODE::NONE, in_par > 1);
		out_x = fs.x;
		out_y = fs.y;
	}
	const auto scale_x = static_cast<double>(out_x) / in_x;
	const auto scale_y = static_cast<double>(out_y) / in_y;
	const auto out_par = scale_y / scale_x;

	auto describe_scaling_mode = [scaling_mode]() {
		switch (scaling_mode) {
		case SCALING_MODE::NONE: return "Bilinear";
		case SCALING_MODE::NEAREST: return "Nearest-neighbour";
		case SCALING_MODE::PERFECT: return "Pixel-perfect";
		}
		return "Unknown mode!";
	};

	LOG_MSG("DISPLAY: %s scaling source %dx%d (PAR %#.3g) by %.1fx%.1f -> %dx%d (PAR %#.3g)",
	        describe_scaling_mode(), in_x, in_y, in_par, scale_x, scale_y,
	        out_x, out_y, out_par);
}

static SDL_Window *SetWindowMode(SCREEN_TYPES screen_type,
                                 int width,
                                 int height,
                                 bool fullscreen,
                                 bool resizable)
{
	static SCREEN_TYPES last_type = SCREEN_SURFACE;

	CleanupSDLResources();

	int currWidth, currHeight;
	if (sdl.window && sdl.resizing_window) {
		SDL_GetWindowSize(sdl.window, &currWidth, &currHeight);
		sdl.update_display_contents = ((width <= currWidth) && (height <= currHeight));
		return sdl.window;
	}

	if (!sdl.window || (last_type != screen_type)) {

		last_type = screen_type;

		if (sdl.window) {
			SDL_DestroyWindow(sdl.window);
		}

		uint32_t flags = opengl_driver_crash_workaround(screen_type);
#if C_OPENGL
		if (screen_type == SCREEN_OPENGL)
			flags |= SDL_WINDOW_OPENGL;
#endif

		// Using undefined position will take care of placing and
		// restoring the window by WM.
		const int pos = SDL_WINDOWPOS_UNDEFINED_DISPLAY(sdl.display_number);
		sdl.window = SDL_CreateWindow("", pos, pos, width, height, flags);
		if (!sdl.window) {
			LOG_MSG("SDL: %s", SDL_GetError());
			return nullptr;
		}

		if (resizable) {
			SDL_AddEventWatch(watch_sdl_events, sdl.window);
			SDL_SetWindowResizable(sdl.window, SDL_TRUE);
		}
		sdl.desktop.window.resizable = resizable;

		GFX_SetTitle(-1, -1, false); // refresh title.

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
	} else {
		// If you feel inclined to remove this SDL_SetWindowSize call
		// (or further modify when it is being invoked), then make sure
		// window size is updated AND content is correctly clipped when
		// emulated program changes resolution with various
		// windowresolution settings (!).
		if (!sdl.desktop.window.resizable && !sdl.desktop.switching_fullscreen)
			SDL_SetWindowSize(sdl.window, width, height);
		SDL_SetWindowFullscreen(sdl.window, 0);
	}
	// Maybe some requested fullscreen resolution is unsupported?
finish:

	if (sdl.desktop.window.resizable && !sdl.desktop.switching_fullscreen) {
		const int w = iround(sdl.draw.width * sdl.draw.scalex);
		const int h = iround(sdl.draw.height * sdl.draw.scaley);
		SDL_SetWindowMinimumSize(sdl.window, w, h);
	}

	if (sdl.draw.has_changed)
		log_display_properties(sdl.draw.width, sdl.draw.height,
		                       sdl.draw.pixel_aspect, sdl.scaling_mode,
		                       sdl.pp_scale, fullscreen, width, height);

	sdl.update_display_contents = true;
	return sdl.window;
}

// Used for the mapper UI and more: Creates a fullscreen window with desktop res
// on Android, and a non-fullscreen window with the input dimensions otherwise.
SDL_Window * GFX_SetSDLSurfaceWindow(Bit16u width, Bit16u height)
{
	constexpr bool fullscreen = false;
	return SetWindowMode(SCREEN_SURFACE, width, height, fullscreen, FIXED_SIZE);
}

// Returns the rectangle in the current window to be used for scaling a
// sub-window with the given dimensions, like the mapper UI.
SDL_Rect GFX_GetSDLSurfaceSubwindowDims(Bit16u width, Bit16u height)
{
	SDL_Rect rect;
	rect.x = rect.y = 0;
	rect.w = width;
	rect.h = height;
	return rect;
}

static SDL_Window *setup_window_pp(SCREEN_TYPES screen_type, bool resizable)
{
	assert(sdl.window);

	int w = 0;
	int h = 0;

	if (sdl.desktop.fullscreen) {
		SDL_GetWindowSize(sdl.window, &w, &h);
	} else {
		w = sdl.desktop.requested_window_bounds.width;
		h = sdl.desktop.requested_window_bounds.height;
	}
	assert(w > 0 && h > 0);

	sdl.pp_scale = calc_pp_scale(w, h);

	const int imgw = sdl.pp_scale.x * sdl.draw.width;
	const int imgh = sdl.pp_scale.y * sdl.draw.height;

	const int wndw = (sdl.desktop.fullscreen ? w : imgw);
	const int wndh = (sdl.desktop.fullscreen ? h : imgh);

	sdl.clip.w = imgw;
	sdl.clip.h = imgh;
	sdl.clip.x = (wndw - imgw) / 2;
	sdl.clip.y = (wndh - imgh) / 2;

	sdl.window = SetWindowMode(screen_type, wndw, wndh,
	                           sdl.desktop.fullscreen, resizable);
	return sdl.window;
}

static SDL_Window *SetupWindowScaled(SCREEN_TYPES screen_type, bool resizable)
{
	if (sdl.scaling_mode == SCALING_MODE::PERFECT)
		return setup_window_pp(screen_type, resizable);

	Bit16u fixedWidth;
	Bit16u fixedHeight;

	if (sdl.desktop.fullscreen) {
		fixedWidth = sdl.desktop.full.fixed ? sdl.desktop.full.width : 0;
		fixedHeight = sdl.desktop.full.fixed ? sdl.desktop.full.height : 0;
	} else {
		fixedWidth = sdl.desktop.window.width;
		fixedHeight = sdl.desktop.window.height;
	}

	if (fixedWidth && fixedHeight) {
		double ratio_w=(double)fixedWidth/(sdl.draw.width*sdl.draw.scalex);
		double ratio_h=(double)fixedHeight/(sdl.draw.height*sdl.draw.scaley);
		if ( ratio_w < ratio_h) {
			sdl.clip.w = fixedWidth;
			sdl.clip.h = (Bit16u)(sdl.draw.height * sdl.draw.scaley*ratio_w + 0.1); //possible rounding issues
		} else {
			/*
			 * The 0.4 is there to correct for rounding issues.
			 * (partly caused by the rounding issues fix in RENDER_SetSize)
			 */
			sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex*ratio_h + 0.4);
			sdl.clip.h = fixedHeight;
		}

		if (sdl.desktop.fullscreen) {
			sdl.window = SetWindowMode(screen_type, fixedWidth,
			                           fixedHeight, sdl.desktop.fullscreen,
			                           resizable);
		} else {
			sdl.window = SetWindowMode(screen_type, sdl.clip.w,
			                           sdl.clip.h, sdl.desktop.fullscreen,
			                           resizable);
		}

		if (sdl.window && SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_FULLSCREEN) {
			int windowWidth;
			SDL_GetWindowSize(sdl.window, &windowWidth, NULL);
			sdl.clip.x = (Sint16)((windowWidth - sdl.clip.w) / 2);
			sdl.clip.y = (Sint16)((fixedHeight - sdl.clip.h) / 2);
		} else {
			sdl.clip.x = 0;
			sdl.clip.y = 0;
		}

		return sdl.window;

	} else {
		sdl.clip.x = 0;
		sdl.clip.y = 0;
		sdl.clip.w = iround(sdl.draw.width * sdl.draw.scalex);
		sdl.clip.h = iround(sdl.draw.height * sdl.draw.scaley);
		sdl.window = SetWindowMode(screen_type, sdl.clip.w, sdl.clip.h,
		                           sdl.desktop.fullscreen, resizable);
		return sdl.window;
	}
}

#if C_OPENGL
/* Create a GLSL shader object, load the shader source, and compile the shader. */
static GLuint BuildShader ( GLenum type, const char *shaderSrc ) {
	GLuint shader;
	GLint compiled;
	const char* src_strings[2];
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
			LOG_MSG("Error compiling shader: %s", info_log.data());
		}

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

static bool LoadGLShaders(const char *src, GLuint *vertex, GLuint *fragment) {
	GLuint s = BuildShader(GL_VERTEX_SHADER, src);
	if (s) {
		*vertex = s;
		s = BuildShader(GL_FRAGMENT_SHADER, src);
		if (s) {
			*fragment = s;
			return true;
		}
		glDeleteShader(*vertex);
	}
	return false;
}
#endif

static SDL_Point calc_pp_scale(int avw, int avh)
{
	assert(sdl.draw.width > 0);
	assert(sdl.draw.height > 0);
	assert(sdl.draw.pixel_aspect > 0.0);

	int x = 0;
	int y = 0;
	constexpr double aspect_weight = 1.14;
	const int err = pp_getscale(sdl.draw.width, sdl.draw.height,
	                            sdl.draw.pixel_aspect, avw, avh,
	                            aspect_weight, &x, &y);
	if (err == 0)
		return {x, y};
	else
		return {1, 1};
}

Bitu GFX_SetSize(Bitu width,
                 Bitu height,
                 MAYBE_UNUSED Bitu flags,
                 double scalex,
                 double scaley,
                 GFX_CallBack_t callback,
                 double pixel_aspect)
{
	Bitu retFlags = 0;
	if (sdl.updating)
		GFX_EndUpdate( 0 );

	sdl.draw.has_changed = (sdl.draw.width != static_cast<int>(width) ||
	                        sdl.draw.height != static_cast<int>(height) ||
	                        sdl.draw.scalex != scalex ||
	                        sdl.draw.scaley != scaley ||
	                        sdl.draw.pixel_aspect != pixel_aspect);

	sdl.draw.width = static_cast<int>(width);
	sdl.draw.height = static_cast<int>(height);
	sdl.draw.scalex = scalex;
	sdl.draw.scaley = scaley;
	sdl.draw.pixel_aspect = pixel_aspect;
	sdl.draw.callback = callback;

	switch (sdl.desktop.want_type) {
dosurface:
	case SCREEN_SURFACE:
		sdl.clip.w = width;
		sdl.clip.h = height;
		if (sdl.desktop.fullscreen) {
			if (sdl.desktop.full.fixed) {
				sdl.clip.x = (sdl.desktop.full.width - width) / 2;
				sdl.clip.y = (sdl.desktop.full.height - height) / 2;
				sdl.window = SetWindowMode(SCREEN_SURFACE,
				                           sdl.desktop.full.width,
				                           sdl.desktop.full.height,
				                           sdl.desktop.fullscreen,
				                           FIXED_SIZE);
				if (sdl.window == NULL)
					E_Exit("Could not set fullscreen video mode %ix%i-%i: %s",
					       sdl.desktop.full.width,
					       sdl.desktop.full.height,
					       sdl.desktop.bpp,
					       SDL_GetError());

				/* This may be required after an ALT-TAB leading to a window
				minimize, which further effectively shrinks its size */
				if ((sdl.clip.x < 0) || (sdl.clip.y < 0)) {
					sdl.update_display_contents = false;
				}
			} else {
				sdl.clip.x = 0;
				sdl.clip.y = 0;
				sdl.window = SetWindowMode(SCREEN_SURFACE,
				                           width, height,
				                           sdl.desktop.fullscreen,
				                           FIXED_SIZE);
				if (sdl.window == NULL)
					E_Exit("Could not set fullscreen video mode %ix%i-%i: %s",
					       (int)width,
					       (int)height,
					       sdl.desktop.bpp,
					       SDL_GetError());
			}
		} else {
			sdl.clip.x = 0;
			sdl.clip.y = 0;
			sdl.window = SetWindowMode(SCREEN_SURFACE, width, height,
			                           sdl.desktop.fullscreen,
			                           FIXED_SIZE);
			if (sdl.window == NULL)
				E_Exit("Could not set windowed video mode %ix%i-%i: %s",
				       (int)width,
				       (int)height,
				       sdl.desktop.bpp,
				       SDL_GetError());
		}
		sdl.surface = SDL_GetWindowSurface(sdl.window);
		if (sdl.surface == NULL)
			E_Exit("Could not retrieve window surface: %s",SDL_GetError());
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
		sdl.desktop.type = SCREEN_SURFACE;
		break; // SCREEN_SURFACE

	case SCREEN_TEXTURE: {
		if (!SetupWindowScaled(SCREEN_TEXTURE, false)) {
			LOG_MSG("DISPLAY: Can't initialise 'texture' window");
			E_Exit("Creating window failed");
		}

		if (sdl.render_driver != "auto")
			SDL_SetHint(SDL_HINT_RENDER_DRIVER, sdl.render_driver.c_str());
		sdl.renderer = SDL_CreateRenderer(sdl.window, -1,
		                                  SDL_RENDERER_ACCELERATED |
		                                  (sdl.desktop.vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
		if (!sdl.renderer) {
			LOG_MSG("%s\n", SDL_GetError());
			LOG_MSG("SDL:Can't create renderer, falling back to surface");
			goto dosurface;
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
			LOG_MSG("SDL:Can't create texture, falling back to surface");
			goto dosurface;
		}

		sdl.texture.input_surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, texture_format);
		if (!sdl.texture.input_surface) {
			LOG_MSG("SDL: Error while preparing texture input");
			goto dosurface;
		}

		SDL_SetRenderDrawColor(sdl.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		uint32_t pixel_format;
		SDL_QueryTexture(sdl.texture.texture, &pixel_format, NULL, NULL, NULL);
		sdl.texture.pixelFormat = SDL_AllocFormat(pixel_format);
		switch (SDL_BITSPERPIXEL(pixel_format)) {
			case 8:  retFlags = GFX_CAN_8;  break;
			case 15: retFlags = GFX_CAN_15; break;
			case 16: retFlags = GFX_CAN_16; break;
			case 24: /* SDL_BYTESPERPIXEL is probably 4, though. */
			case 32: retFlags = GFX_CAN_32; break;
		}
		retFlags |= GFX_SCALING;

		// Log changes to the rendering driver
		static std::string render_driver = {};
		if (render_driver != rinfo.name) {
			LOG_MSG("SDL: Using driver \"%s\" for texture renderer", rinfo.name);
			render_driver = rinfo.name;
		}
		
		if (rinfo.flags & SDL_RENDERER_ACCELERATED)
			retFlags |= GFX_HARDWARE;

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
		int texsize = 2 << int_log2(width > height ? width : height);
		if (texsize>sdl.opengl.max_texsize) {
			LOG_MSG("SDL:OPENGL: No support for texturesize of %d, falling back to surface",texsize);
			goto dosurface;
		}
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SetupWindowScaled(SCREEN_OPENGL, sdl.desktop.want_resizable_window);

		/* We may simply use SDL_BYTESPERPIXEL
		here rather than SDL_BITSPERPIXEL   */
		if (!sdl.window || SDL_BYTESPERPIXEL(SDL_GetWindowPixelFormat(sdl.window))<2) {
			LOG_MSG("SDL:OPENGL:Can't open drawing window, are you running in 16bpp(or higher) mode?");
			goto dosurface;
		}
		sdl.opengl.context = SDL_GL_CreateContext(sdl.window);
		if (sdl.opengl.context == NULL) {
			LOG_MSG("SDL:OPENGL:Can't create OpenGL context, falling back to surface");
			goto dosurface;
		}
		/* Sync to VBlank if desired */
		SDL_GL_SetSwapInterval(sdl.desktop.vsync ? 1 : 0);

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
					const char *src = sdl.opengl.shader_src;
					if (src && !LoadGLShaders(src, &vertexShader, &fragmentShader)) {
						LOG_MSG("SDL:OPENGL:Failed to compile shader, falling back to default");
						src = NULL;
					}
					if (src == NULL && !LoadGLShaders(shader_src_default, &vertexShader, &fragmentShader)) {
						LOG_MSG("SDL:OPENGL:Failed to compile default shader!");
						goto dosurface;
					}

					sdl.opengl.program_object = glCreateProgram();
					if (!sdl.opengl.program_object) {
						glDeleteShader(vertexShader);
						glDeleteShader(fragmentShader);
						LOG_MSG("SDL:OPENGL:Can't create program object, falling back to surface");
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
							LOG_MSG("SDL:OPENGL:Error link program:\n %s", info_log.data());
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
					// Don't force updating unless a shader depends on frame_count
					RENDER_SetForceUpdate(sdl.opengl.ruby.frame_count != -1);
				}
			}
		}

		/* Create the texture and display list */
		if (sdl.opengl.pixel_buffer_object) {
			glGenBuffersARB(1, &sdl.opengl.buffer);
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, sdl.opengl.buffer);
			glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_EXT, width*height*4, NULL, GL_STREAM_DRAW_ARB);
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
		} else {
			sdl.opengl.framebuf = malloc(width * height * 4); // 32 bit color
		}
		sdl.opengl.pitch=width*4;

		int windowWidth, windowHeight;
		SDL_GetWindowSize(sdl.window, &windowWidth, &windowHeight);
		if (sdl.clip.x == 0 && sdl.clip.y == 0 &&
		    sdl.desktop.fullscreen &&
		    !sdl.desktop.full.fixed &&
		    (sdl.clip.w != windowWidth || sdl.clip.h != windowHeight)) {
			// LOG_MSG("attempting to fix the centering to %d %d %d %d",(windowWidth-sdl.clip.w)/2,(windowHeight-sdl.clip.h)/2,sdl.clip.w,sdl.clip.h);
			glViewport((windowWidth - sdl.clip.w) / 2,
			           (windowHeight - sdl.clip.h) / 2, sdl.clip.w,
			           sdl.clip.h);
		} else if (sdl.desktop.window.resizable) {
			sdl.clip = calc_viewport(windowWidth, windowHeight);
			glViewport(sdl.clip.x, sdl.clip.y, sdl.clip.w, sdl.clip.h);
		} else {
			/* We don't just pass sdl.clip.y as-is, so we cover the case of non-vertical
			 * centering on Android (in order to leave room for the on-screen keyboard)
			 */
			glViewport(sdl.clip.x,
			           windowHeight - (sdl.clip.y + sdl.clip.h),
			           sdl.clip.w,
			           sdl.clip.h);
		}

		if (sdl.opengl.texture > 0) {
			glDeleteTextures(1,&sdl.opengl.texture);
		}
		glGenTextures(1, &sdl.opengl.texture);
		glBindTexture(GL_TEXTURE_2D,sdl.opengl.texture);
		// No borders
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		const bool use_nearest = (sdl.scaling_mode != SCALING_MODE::NONE) ||
		                         ((sdl.clip.h % height == 0) &&
		                          (sdl.clip.w % width == 0));
		const GLint filter = (use_nearest ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);

		const auto texture_area_bytes = static_cast<size_t>(texsize) *
		                                texsize * MAX_BYTES_PER_PIXEL;
		uint8_t *emptytex = new uint8_t[texture_area_bytes];
		assert(emptytex);

		memset(emptytex, 0, texture_area_bytes);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texsize, texsize, 0,
		             GL_BGRA_EXT, GL_UNSIGNED_BYTE, emptytex);
		delete[] emptytex;

		glClearColor (0.0f, 0.0f, 0.0f, 1.0f);

		glClear(GL_COLOR_BUFFER_BIT);
		SDL_GL_SwapWindow(sdl.window);
		glClear(GL_COLOR_BUFFER_BIT);

		glDisable (GL_DEPTH_TEST);
		glDisable (GL_LIGHTING);
		glDisable(GL_CULL_FACE);
		glEnable(GL_TEXTURE_2D);

		if (sdl.opengl.program_object) {
			// Set shader variables
			glUniform2f(sdl.opengl.ruby.texture_size, (float)texsize, (float)texsize);
			glUniform2f(sdl.opengl.ruby.input_size, (float)width, (float)height);
			glUniform2f(sdl.opengl.ruby.output_size, sdl.clip.w, sdl.clip.h);
			// The following uniform is *not* set right now
			sdl.opengl.actual_frame_count = 0;
		} else {
			GLfloat tex_width=((GLfloat)(width)/(GLfloat)texsize);
			GLfloat tex_height=((GLfloat)(height)/(GLfloat)texsize);

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

		retFlags = GFX_CAN_32 | GFX_SCALING;
		if (sdl.opengl.pixel_buffer_object)
			retFlags |= GFX_HARDWARE;

		sdl.desktop.type = SCREEN_OPENGL;
		break; // SCREEN_OPENGL
	}
#endif // C_OPENGL
	}

	if (retFlags)
		GFX_Start();
	return retFlags;
}

void GFX_SetShader(MAYBE_UNUSED const char *src)
{
#if C_OPENGL
	if (!sdl.opengl.use_shader || src == sdl.opengl.shader_src)
		return;

	sdl.opengl.shader_src = src;
	if (sdl.opengl.program_object) {
		glDeleteProgram(sdl.opengl.program_object);
		sdl.opengl.program_object = 0;
	}
#endif
}

void GFX_ToggleMouseCapture(void) {
	/*
	 * Only process mouse events when we have focus.
	 * This protects against out-of-order event issues such
	 * as acting on clicks before the window is drawn.
	 */
	if (!sdl.mouse.has_focus)
		return;

	assertm(sdl.mouse.control_choice != NoMouse,
	        "SDL: Mouse capture is invalid when NoMouse is configured [Logic Bug]");

	mouse_is_captured = mouse_is_captured ? SDL_FALSE : SDL_TRUE;
	if (SDL_SetRelativeMouseMode(mouse_is_captured) != 0) {
		SDL_ShowCursor(SDL_ENABLE);
		E_Exit("SDL: failed to %s relative-mode [SDL Bug]",
		       mouse_is_captured ? "put the mouse in"
		                         : "take the mouse out of");
	}
	LOG_MSG("SDL: %s the mouse", mouse_is_captured ? "captured" : "released");
}

static void ToggleMouseCapture(bool pressed) {
	if (!pressed || sdl.desktop.fullscreen)
		return;
	GFX_ToggleMouseCapture();
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

/*
 *  Assesses the following:
 *   - current window size (full or not),
 *   - mouse capture state, (yes or no).
 *   - desired capture type (start, click, seamless), and
 *   - if we're starting up for the first time,
 *  to determine if the mouse-capture state should be toggled.
 *  Note that this also acts a filter: we don't want to repeatedly
 *  re-apply the same mouse capture state over and over again, so most
 *  of the time this function will (or should) decide to do nothing.
 */
void GFX_UpdateMouseState(void) {

	// This function is only be run when the window is shown or has focus
	sdl.mouse.has_focus = true;

	// Used below
	static bool has_run_once = false;

	/*
	 *  We've switched to or started in fullscreen, so capture the mouse
	 *  This is valid for all modes except for nomouse.
	 */
	if (sdl.desktop.fullscreen
	    && !mouse_is_captured
	    && sdl.mouse.control_choice != NoMouse) {
		GFX_ToggleMouseCapture();

	/*
	 * If we've switched-back from fullscreen, then release the mouse
	 * if it's captured and in seamless-mode.
	 */
	} else if (!sdl.desktop.fullscreen
	           && mouse_is_captured
	           && sdl.mouse.control_choice == Seamless) {
			GFX_ToggleMouseCapture();
			SDL_ShowCursor(SDL_DISABLE);

	/*
	 *  If none of the above are true /and/ we're starting
	 *  up the first time, then:
	 *  - Capture the mouse if configured onstart is set.
	 *  - Hide the mouse if seamless or nomouse are set.
	 */
	} else if (!has_run_once) {
		if (sdl.mouse.control_choice == CaptureOnStart) {
			SDL_RaiseWindow(sdl.window);
			GFX_ToggleMouseCapture();
		} else if (sdl.mouse.control_choice & (Seamless | NoMouse)) {
			SDL_ShowCursor(SDL_DISABLE);
		}
	}
	has_run_once = true;
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
#if defined (WIN32)
	// We are about to switch to the opposite of our current mode
	// (ie: opposite of whatever sdl.desktop.fullscreen holds).
	// Sticky-keys should be set to the opposite of fullscreen,
	// so we simply apply the bool of the mode we're switching out-of.
	sticky_keys(sdl.desktop.fullscreen);
#endif
	sdl.desktop.fullscreen = !sdl.desktop.fullscreen;
	GFX_ResetScreen();
	FocusInput();
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

static Pacer render_pacer("Render", 7000);

void GFX_EndUpdate(const Bit16u *changedLines)
{
	if (!sdl.update_display_contents)
		return;
#if C_OPENGL
	const bool using_opengl = (sdl.desktop.type == SCREEN_OPENGL);
#else
	const bool using_opengl = false;
#endif
	if ((!using_opengl || !RENDER_GetForceUpdate()) && !sdl.updating)
		return;
	MAYBE_UNUSED bool actually_updating = sdl.updating;
	sdl.updating = false;
	switch (sdl.desktop.type) {
	case SCREEN_TEXTURE: {
		assert(sdl.texture.input_surface);
		if (render_pacer.CanRun()) {
			SDL_UpdateTexture(sdl.texture.texture,
			                  nullptr, // update entire texture
			                  sdl.texture.input_surface->pixels,
			                  sdl.texture.input_surface->pitch);
			SDL_RenderClear(sdl.renderer);
			SDL_RenderCopy(sdl.renderer, sdl.texture.texture,
			               nullptr, &sdl.clip);
			SDL_RenderPresent(sdl.renderer);
		}
		render_pacer.Checkpoint();
	} break;
#if C_OPENGL
	case SCREEN_OPENGL:
		// Clear drawing area. Some drivers (on Linux) have more than 2 buffers and the screen might
		// be dirty because of other programs.
		if (!actually_updating) {
			/* Don't really update; Just increase the frame counter.
			 * If we tried to update it may have not worked so well
			 * with VSync...
			 * (Think of 60Hz on the host with 70Hz on the client.)
			 */
			sdl.opengl.actual_frame_count++;
			return;
		}
		glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		if (sdl.opengl.pixel_buffer_object) {
			glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sdl.draw.width,
			                sdl.draw.height, GL_BGRA_EXT,
			                GL_UNSIGNED_INT_8_8_8_8_REV, 0);
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
		} else if (changedLines) {
			int y = 0;
			size_t index = 0;
			while (y < sdl.draw.height) {
				if (!(index & 1)) {
					y += changedLines[index];
				} else {
					Bit8u *pixels = (Bit8u *)sdl.opengl.framebuf + y * sdl.opengl.pitch;
					int height = changedLines[index];
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y,
					                sdl.draw.width, height,
					                GL_BGRA_EXT,
					                GL_UNSIGNED_INT_8_8_8_8_REV,
					                pixels);
					y += height;
				}
				index++;
			}
		} else {
			return;
		}

		if (render_pacer.CanRun()) {
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
		break;
#endif
	case SCREEN_SURFACE:
		if (changedLines) {
			int y = 0;
			size_t index = 0;
			size_t rect_count = 0;
			while (y < sdl.draw.height) {
				if (!(index & 1)) {
					y += changedLines[index];
				} else {
					SDL_Rect *rect = &sdl.updateRects[rect_count++];
					rect->x = sdl.clip.x;
					rect->y = sdl.clip.y + y;
					rect->w = sdl.draw.width;
					rect->h = changedLines[index];
					y += changedLines[index];
				}
				index++;
			}
			if (rect_count) {
				if (render_pacer.CanRun()) {
					SDL_UpdateWindowSurfaceRects(sdl.window,
					                             sdl.updateRects,
					                             rect_count);
				}
				render_pacer.Checkpoint();
			}
		}
		break;
	}
}

Bitu GFX_GetRGB(Bit8u red,Bit8u green,Bit8u blue) {
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		return SDL_MapRGB(sdl.surface->format,red,green,blue);
	case SCREEN_TEXTURE:
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
		GFX_EndUpdate( 0 );
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
	if (sdl.renderer) {
		SDL_DestroyRenderer(sdl.renderer);
		sdl.renderer = nullptr;
	}
#if C_OPENGL
	if (sdl.opengl.context) {
		SDL_GL_DeleteContext(sdl.opengl.context);
		sdl.opengl.context = 0;
	}
#endif
}

static void GUI_ShutDown(Section *)
{
	GFX_Stop();
	if (sdl.draw.callback)
		(sdl.draw.callback)( GFX_CallBackStop );
	if (sdl.desktop.fullscreen)
		GFX_SwitchFullScreen();
	if (mouse_is_captured)
		GFX_ToggleMouseCapture();
	CleanupSDLResources();
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

	if((sdl.priority.focus != sdl.priority.nofocus ) &&
		(getuid()!=0) ) return;

#endif
	switch (level) {
#ifdef WIN32
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
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
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
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

#ifdef WIN32
extern Bit8u int10_font_14[256 * 14];
static void OutputString(Bitu x,Bitu y,const char * text,Bit32u color,Bit32u color2,SDL_Surface * output_surface) {
	Bit32u * draw=(Bit32u*)(((Bit8u *)output_surface->pixels)+((y)*output_surface->pitch))+x;
	while (*text) {
		Bit8u * font=&int10_font_14[(*text)*14];
		Bitu i,j;
		Bit32u * draw_line=draw;
		for (i=0;i<14;i++) {
			Bit8u map=*font++;
			for (j=0;j<8;j++) {
				*(draw_line + j) = map & 0x80 ? color : color2;
				map<<=1;
			}
			draw_line+=output_surface->pitch/4;
		}
		text++;
		draw+=8;
	}
}
#endif

#include "dosbox_staging_splash.c"

static SDL_Window *SetDefaultWindowMode()
{
	if (sdl.window)
		return sdl.window;

	sdl.draw.width = splash_image.width;
	sdl.draw.height = splash_image.height;

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

/*
 * Please leave the Splash screen stuff in working order.
 * We spend a lot of time making DOSBox.
 */
static void DisplaySplash(int time_ms)
{
	assert(sdl.window);

	constexpr int src_w = splash_image.width;
	constexpr int src_h = splash_image.height;
	constexpr int src_bpp = splash_image.bytes_per_pixel;
	static_assert(src_bpp == 3, "Source image expected in RGB format.");

	const auto flags = GFX_SetSize(src_w, src_h, GFX_CAN_32, 1.0, 1.0, nullptr, 1.0);
	if (!(flags & GFX_CAN_32)) {
		LOG_MSG("Can't show 32bpp splash.");
		return;
	}

	uint8_t *out = nullptr;
	int pitch = 0;
	if (!GFX_StartUpdate(out, pitch))
		E_Exit("%s", SDL_GetError());

	uint32_t *pixels = reinterpret_cast<uint32_t *>(out);
	assertm(pixels, "GFX_StartUpdate is supposed to give us buffer.");
	const int buf_width = pitch / 4;
	assertm(buf_width >= src_w, "Row length needs to be big enough.");

	std::array<uint8_t, (src_w * src_h * src_bpp)> splash;
	GIMP_IMAGE_RUN_LENGTH_DECODE(splash.data(), splash_image.rle_pixel_data,
	                             src_w * src_h, src_bpp);
	size_t i = 0;
	size_t j = 0;

	static_assert(splash.size() % 3 == 0, "Reading 3 bytes at a time.");
	for (int y = 0; y < src_h; y++) {
		// copy a row of pixels to output buffer
		for (int x = 0; x < src_w; x++) {
			const uint32_t r = splash[i++];
			const uint32_t g = splash[i++];
			const uint32_t b = splash[i++];
			pixels[j++] = (r << 16) | (g << 8) | b;
		}
		// pad with black until the end of row
		// only output=surface actually needs this
		for (int x = src_w; x < buf_width; x++)
			pixels[j++] = 0;
	}

	const uint16_t lines[2] = {0, src_h}; // output=surface won't work otherwise
	GFX_EndUpdate(lines);
	Delay(time_ms);
}

/* For some preference values, we can safely remove comment after # character
 */
static std::string NormalizeConfValue(const char *val)
{
	std::string pref = val;
	const auto comment_pos = pref.find('#');
	if (comment_pos != std::string::npos)
		pref.erase(comment_pos);
	trim(pref);
	lowcase(pref);
	return pref;
}

MAYBE_UNUSED static std::string get_glshader_value()
{
#if C_OPENGL
	assert(control);
	const Section *rs = control->GetSection("render");
	assert(rs);
	return rs->GetPropValue("glshader");
#else
	return "";
#endif // C_OPENGL
}



static bool detect_resizable_window()
{
#if C_OPENGL
	if (sdl.desktop.want_type != SCREEN_OPENGL) {
		LOG_MSG("DISPLAY: Disabled resizable window, only compatible with OpenGL output");
		return false;
	}

	const std::string sname = get_glshader_value();

	if (sname != "sharp" && sname != "none" && sname != "default") {
		LOG_MSG("DISPLAY: Disabled resizable window, only compatible with 'sharp' and 'none' glshaders");
		return false;
	}

	return true;
#else
	return false;
#endif // C_OPENGL
}

static SDL_Point remove_stretched_aspect(const SDL_Point &size)
{
	return {size.x, ceil_sdivide(size.y * 5, 6)};
}

static SDL_Point refine_window_size(const SDL_Point &size,
                                    const SCALING_MODE scaling_mode,
                                    const bool wants_stretched_pixels)
{
	switch (scaling_mode) {
	case (SCALING_MODE::NONE): {
		const auto game_ratios = wants_stretched_pixels
		                                 ? RATIOS_FOR_STRETCHED_PIXELS
		                                 : RATIOS_FOR_SQUARE_PIXELS;

		const auto window_aspect = static_cast<double>(size.x) / size.y;
		const auto game_aspect = static_cast<double>(game_ratios.x) / game_ratios.y;

		// screen is wider than the game, so constrain horizonal
		if (window_aspect > game_aspect) {
			const int x = ceil_sdivide(size.y * game_ratios.x, game_ratios.y);
			return {x, size.y};
		}
		// screen is narrower than the game, so constrain vertical
		const int y = ceil_sdivide(size.x * game_ratios.y, game_ratios.x);
		return {size.x, y};
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
				return (wants_stretched_pixels
				                ? candidate
				                : remove_stretched_aspect(candidate));
		break;
	}
	case (SCALING_MODE::PERFECT): {
		constexpr double aspect_weight = 1.14;
		constexpr SDL_Point pre_draw_size = {320, 200};
		const double pixel_aspect_ratio = wants_stretched_pixels ? 1.2 : 1;

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

	// Fallback
	return (wants_stretched_pixels
	                ? FALLBACK_WINDOW_DIMENSIONS
	                : remove_stretched_aspect(FALLBACK_WINDOW_DIMENSIONS));
}

static SDL_Point window_bounds_from_resolution(const std::string &pref,
                                               const SDL_Rect &desktop)
{
	int w = 0;
	int h = 0;
	const bool was_parsed = sscanf(pref.c_str(), "%dx%d", &w, &h) == 2;

	const bool is_out_of_bounds = (w > desktop.w || h > desktop.h);
	if (was_parsed && is_out_of_bounds)
		LOG_MSG("DISPLAY: Requested windowresolution '%dx%d' is larger than the desktop '%dx%d'",
		        w, h, desktop.w, desktop.h);

	const bool is_valid = (w > 0 && h > 0);
	if (was_parsed && is_valid)
		return {w, h};

	LOG_MSG("DISPLAY: Requested windowresolution '%s' is not valid, falling back to '%dx%d' instead",
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
		LOG_MSG("DISPLAY: Requested windowresolution '%s' is invalid, using 'default' instead",
		        pref.c_str());

	const int w = ceil_sdivide(desktop.w * percent, 100);
	const int h = ceil_sdivide(desktop.h * percent, 100);
	return {w, h};
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
                                         const bool wants_stretched_pixels)
{
	// TODO: Deprecate SURFACE output and remove this.
	// For now, let the DOS-side determine the window's resolution.
	if (sdl.desktop.want_type == SCREEN_SURFACE)
		return;

	// Can the window be resized?
	sdl.desktop.want_resizable_window = detect_resizable_window();

	// Get the total desktop resolution
	SDL_Rect desktop;
	assert(sdl.display_number >= 0);
	SDL_GetDisplayBounds(sdl.display_number, &desktop);
	assert(desktop.w >= FALLBACK_WINDOW_DIMENSIONS.x);
	assert(desktop.h >= FALLBACK_WINDOW_DIMENSIONS.y);

	// The refined scaling mode tell the refiner how it should adjust
	// the coarse-resolution.
	SCALING_MODE refined_scaling_mode = scaling_mode;
	auto drop_nearest = [scaling_mode]() {
		return (scaling_mode == SCALING_MODE::NEAREST) ? SCALING_MODE::NONE
		                                               : scaling_mode;
	};

	// Get the coarse resolution from the users setting, and adjust
	// refined scaling mode if an exact resolution is desired.
	const std::string pref = NormalizeConfValue(windowresolution_val);
	SDL_Point coarse_size = FALLBACK_WINDOW_DIMENSIONS;
	if (pref.find('x') != std::string::npos) {
		coarse_size = window_bounds_from_resolution(pref, desktop);
		refined_scaling_mode = drop_nearest();
	} else {
		coarse_size = window_bounds_from_label(pref, desktop);
		if (pref == "desktop") {
			refined_scaling_mode = drop_nearest();
		}
	}
	// Save the coarse bounds in the SDL struct for future sizing events
	sdl.desktop.requested_window_bounds = {coarse_size.x, coarse_size.y};

	// Refine the coarse resolution and save it in the SDL struct
	const auto refined_size = refine_window_size(coarse_size, refined_scaling_mode,
	                                             wants_stretched_pixels);
	assert(refined_size.x <= UINT16_MAX && refined_size.y <= UINT16_MAX);
	sdl.desktop.window.width = static_cast<uint16_t>(refined_size.x);
	sdl.desktop.window.height = static_cast<uint16_t>(refined_size.y);

	// Let the user know the resulting window properties
	LOG_MSG("DISPLAY: Initialized %dx%d window-mode on %dx%d display-%d",
	        refined_size.x, refined_size.y, desktop.w, desktop.h,
	        sdl.display_number);
}

static SDL_Rect calc_viewport_fit(int win_width, int win_height)
{
	assert(sdl.draw.width > 0);
	assert(sdl.draw.height > 0);
	assert(sdl.draw.scalex > 0.0);
	assert(sdl.draw.scaley > 0.0);
	assert(std::isfinite(sdl.draw.scalex));
	assert(std::isfinite(sdl.draw.scaley));

	const double prog_aspect_ratio = (sdl.draw.width * sdl.draw.scalex) /
	                                 (sdl.draw.height * sdl.draw.scaley);
	const double win_aspect_ratio = double(win_width) / double(win_height);

	if (prog_aspect_ratio > win_aspect_ratio) {
		// match window width
		const int w = win_width;
		const int h = iround(win_width / prog_aspect_ratio);
		assert(win_height >= h);
		const int y = (win_height - h) / 2;
		return {0, y, w, h};
	} else {
		// match window height
		const int w = iround(win_height * prog_aspect_ratio);
		const int h = win_height;
		assert(win_width >= w);
		const int x = (win_width - w) / 2;
		return {x, 0, w, h};
	}
}

static SDL_Rect calc_viewport_pp(int win_width, int win_height)
{
	sdl.pp_scale = calc_pp_scale(win_width, win_height);

	const int w = sdl.pp_scale.x * sdl.draw.width;
	const int h = sdl.pp_scale.y * sdl.draw.height;
	const int x = (win_width - w) / 2;
	const int y = (win_height - h) / 2;

	return {x, y, w, h};
}

MAYBE_UNUSED static SDL_Rect calc_viewport(int width, int height)
{
	if (sdl.scaling_mode == SCALING_MODE::PERFECT)
		return calc_viewport_pp(width, height);
	else
		return calc_viewport_fit(width, height);
}

//extern void UI_Run(bool);
void Restart(bool pressed);

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

	Prop_multival* p=section->Get_multival("priority");
	std::string focus = p->GetSection()->Get_string("active");
	std::string notfocus = p->GetSection()->Get_string("inactive");

	if (focus == "auto" || notfocus == "auto") {
		sdl.priority.focus = PRIORITY_LEVEL_AUTO;
		sdl.priority.nofocus = PRIORITY_LEVEL_AUTO;
		if (focus != "auto" || notfocus != "auto")
			LOG_MSG("MAIN: \"priority\" can't be \"auto\" for just one value, overriding");
	} else {
		if (focus == "lowest")
			sdl.priority.focus = PRIORITY_LEVEL_LOWEST;
		else if (focus == "lower")
			sdl.priority.focus = PRIORITY_LEVEL_LOWER;
		else if (focus == "normal")
			sdl.priority.focus = PRIORITY_LEVEL_NORMAL;
		else if (focus == "higher")
			sdl.priority.focus = PRIORITY_LEVEL_HIGHER;
		else if (focus == "highest")
			sdl.priority.focus = PRIORITY_LEVEL_HIGHEST;

		if (notfocus == "lowest")
			sdl.priority.nofocus = PRIORITY_LEVEL_LOWEST;
		else if (notfocus == "lower")
			sdl.priority.nofocus = PRIORITY_LEVEL_LOWER;
		else if (notfocus == "normal")
			sdl.priority.nofocus = PRIORITY_LEVEL_NORMAL;
		else if (notfocus == "higher")
			sdl.priority.nofocus = PRIORITY_LEVEL_HIGHER;
		else if (notfocus == "highest")
			sdl.priority.nofocus = PRIORITY_LEVEL_HIGHEST;
		else if (notfocus == "pause")
			/* we only check for pause here, because it makes no
			 * sense for DOSBox to be paused while it has focus
			 */
			sdl.priority.nofocus = PRIORITY_LEVEL_PAUSE;
	}

	SetPriority(sdl.priority.focus); //Assume focus on startup
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
			if (strcmp(fullresolution,"desktop")) { //desktop = 0x0
				char* height = const_cast<char*>(strchr(fullresolution,'x'));
				if (height && * height) {
					*height = 0;
					sdl.desktop.full.height = (Bit16u)atoi(height+1);
					sdl.desktop.full.width  = (Bit16u)atoi(res);
				}
			}
		}
	}

	// TODO vsync option is disabled for the time being, as it does not work
	//      correctly and is causing serious bugs.
	sdl.desktop.vsync = section->Get_bool("vsync");
	sdl.desktop.vsync_skip = section->Get_int("vsync_skip");

	const int display = section->Get_int("display");
	if ((display >= 0) && (display < SDL_GetNumVideoDisplays())) {
		sdl.display_number = display;
	} else {
		LOG_MSG("SDL: Display number out of bounds, using display 0");
		sdl.display_number = 0;
	}

	sdl.desktop.full.display_res = sdl.desktop.full.fixed && (!sdl.desktop.full.width || !sdl.desktop.full.height);
	if (sdl.desktop.full.display_res) {
		GFX_ObtainDisplayDimensions();
	}

	std::string output=section->Get_string("output");

	if (output == "surface") {
		sdl.desktop.want_type=SCREEN_SURFACE;
	} else if (output == "texture") {
		sdl.desktop.want_type=SCREEN_TEXTURE;
		sdl.scaling_mode = SCALING_MODE::NONE;
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	} else if (output == "texturenb") {
		sdl.desktop.want_type=SCREEN_TEXTURE;
		sdl.scaling_mode = SCALING_MODE::NEAREST;
		// Currently the default, but... oh well
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	} else if (output == "texturepp") {
		sdl.desktop.want_type=SCREEN_TEXTURE;
		sdl.scaling_mode = SCALING_MODE::PERFECT;
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
#if C_OPENGL
	} else if (output == "opengl") {
		sdl.desktop.want_type=SCREEN_OPENGL;
		sdl.scaling_mode = SCALING_MODE::NONE;
		sdl.opengl.bilinear = true;
	} else if (output == "openglnb") {
		sdl.desktop.want_type=SCREEN_OPENGL;
		sdl.scaling_mode = SCALING_MODE::NEAREST;
		sdl.opengl.bilinear = false;
	} else if (output == "openglpp") {
		sdl.desktop.want_type = SCREEN_OPENGL;
		sdl.scaling_mode = SCALING_MODE::PERFECT;
		sdl.opengl.bilinear = false;
#endif
	} else {
		LOG_MSG("SDL: Unsupported output device %s, switching back to surface",output.c_str());
		sdl.desktop.want_type=SCREEN_SURFACE;//SHOULDN'T BE POSSIBLE anymore
	}

	const std::string screensaver = section->Get_string("screensaver");
	if (screensaver == "allow")
		SDL_EnableScreenSaver();
	if (screensaver == "block")
		SDL_DisableScreenSaver();

	sdl.texture.texture = 0;
	sdl.texture.pixelFormat = 0;
	sdl.render_driver = section->Get_string("texture_renderer");
	lowcase(sdl.render_driver);

	const auto render_section = static_cast<Section_prop *>(
	        control->GetSection("render"));
	assert(render_section);

	setup_window_sizes_from_conf(section->Get_string("windowresolution"),
	                             sdl.scaling_mode,
	                             render_section->Get_bool("aspect"));

#if C_OPENGL
	if (sdl.desktop.want_type == SCREEN_OPENGL) { /* OPENGL is requested */
		if (!SetDefaultWindowMode()) {
			LOG_MSG("Could not create OpenGL window, switching back to surface");
			sdl.desktop.want_type = SCREEN_SURFACE;
		} else {
			sdl.opengl.context = SDL_GL_CreateContext(sdl.window);
			if (sdl.opengl.context == 0) {
				LOG_MSG("Could not create OpenGL context, switching back to surface");
				sdl.desktop.want_type = SCREEN_SURFACE;
			}
		}
		if (sdl.desktop.want_type == SCREEN_OPENGL) {
			sdl.opengl.program_object = 0;
			glAttachShader = (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader");
			glCompileShader = (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader");
			glCreateProgram = (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram");
			glCreateShader = (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader");
			glDeleteProgram = (PFNGLDELETEPROGRAMPROC)SDL_GL_GetProcAddress("glDeleteProgram");
			glDeleteShader = (PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader");
			glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
			glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)SDL_GL_GetProcAddress("glGetAttribLocation");
			glGetProgramiv = (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv");
			glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog");
			glGetShaderiv = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv");
			glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog");
			glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress("glGetUniformLocation");
			glLinkProgram = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram");
			glShaderSource = (PFNGLSHADERSOURCEPROC_NP)SDL_GL_GetProcAddress("glShaderSource");
			glUniform2f = (PFNGLUNIFORM2FPROC)SDL_GL_GetProcAddress("glUniform2f");
			glUniform1i = (PFNGLUNIFORM1IPROC)SDL_GL_GetProcAddress("glUniform1i");
			glUseProgram = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");
			glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)SDL_GL_GetProcAddress("glVertexAttribPointer");
			sdl.opengl.use_shader = (glAttachShader && glCompileShader && glCreateProgram && glDeleteProgram && glDeleteShader && \
				glEnableVertexAttribArray && glGetAttribLocation && glGetProgramiv && glGetProgramInfoLog && \
				glGetShaderiv && glGetShaderInfoLog && glGetUniformLocation && glLinkProgram && glShaderSource && \
				glUniform2f && glUniform1i && glUseProgram && glVertexAttribPointer);

			sdl.opengl.buffer=0;
			sdl.opengl.framebuf=0;
			sdl.opengl.texture=0;
			sdl.opengl.displaylist=0;
			glGetIntegerv (GL_MAX_TEXTURE_SIZE, &sdl.opengl.max_texsize);
			glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)SDL_GL_GetProcAddress("glGenBuffersARB");
			glBindBufferARB = (PFNGLBINDBUFFERARBPROC)SDL_GL_GetProcAddress("glBindBufferARB");
			glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)SDL_GL_GetProcAddress("glDeleteBuffersARB");
			glBufferDataARB = (PFNGLBUFFERDATAARBPROC)SDL_GL_GetProcAddress("glBufferDataARB");
			glMapBufferARB = (PFNGLMAPBUFFERARBPROC)SDL_GL_GetProcAddress("glMapBufferARB");
			glUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)SDL_GL_GetProcAddress("glUnmapBufferARB");

			// TODO According to Khronos documentation, the correct
			// way to query GL_EXTENSIONS is using glGetStringi from
			// OpenGL 3.0; replace with OpenGL loading library (such
			// as GLEW or libepoxy)
			const char * gl_ext = (const char *)glGetString (GL_EXTENSIONS);
			if(gl_ext && *gl_ext){
				sdl.opengl.packed_pixel=(strstr(gl_ext,"EXT_packed_pixels") != NULL);
				sdl.opengl.paletted_texture=(strstr(gl_ext,"EXT_paletted_texture") != NULL);
				sdl.opengl.pixel_buffer_object=(strstr(gl_ext,"GL_ARB_pixel_buffer_object") != NULL ) &&
				    glGenBuffersARB && glBindBufferARB && glDeleteBuffersARB && glBufferDataARB &&
				    glMapBufferARB && glUnmapBufferARB;
			} else {
				sdl.opengl.packed_pixel = false;
				sdl.opengl.paletted_texture = false;
				sdl.opengl.pixel_buffer_object = false;
			}
#ifdef DB_DISABLE_DBO
			sdl.opengl.pixel_buffer_object = false;
#endif
			LOG_MSG("OPENGL: Pixel buffer object extension: %s",
			        sdl.opengl.pixel_buffer_object ? "available"
			                                       : "missing");
		}
	} /* OPENGL is requested end */
#endif	//OPENGL

	if (!SetDefaultWindowMode())
		E_Exit("Could not initialize video: %s", SDL_GetError());

	SDL_SetWindowTitle(sdl.window, "DOSBox Staging");
	SetIcon();

	const bool tiny_fullresolution = splash_image.width > sdl.desktop.full.width ||
	                                 splash_image.height > sdl.desktop.full.height;
	if ((control->GetStartupVerbosity() == Verbosity::High ||
	     control->GetStartupVerbosity() == Verbosity::SplashOnly) &&
	    !(sdl.desktop.fullscreen && tiny_fullresolution)) {
		GFX_Start();
		DisplaySplash(1000);
		GFX_Stop();
	}

	// Apply the user's mouse settings
	Section_prop* s = section->Get_multival("capture_mouse")->GetSection();
	const std::string control_choice = s->Get_string("capture_mouse_first_value");
	std::string mouse_control_msg;
	if (control_choice == "onclick") {
		sdl.mouse.control_choice = CaptureOnClick;
		mouse_control_msg = "will be captured after clicking";
	} else if (control_choice == "onstart") {
		sdl.mouse.control_choice = CaptureOnStart;
		mouse_control_msg = "will be captured immediately on start";
	} else if (control_choice == "seamless") {
		sdl.mouse.control_choice = Seamless;
		mouse_control_msg = "will move seamlessly without being captured";
	} else if (control_choice == "nomouse") {
		sdl.mouse.control_choice = NoMouse;
		mouse_control_msg = "is disabled";
	} else {
		assert(sdl.mouse.control_choice == CaptureOnClick);
	}

	std::string middle_control_msg;

	if (sdl.mouse.control_choice != NoMouse) {
		const std::string mclick_choice = s->Get_string("capture_mouse_second_value");

		// release the mouse is the default; this logic handles an empty 2nd value
		sdl.mouse.middle_will_release = (mclick_choice != "middlegame");


		middle_control_msg = sdl.mouse.middle_will_release
		                             ? " and middle-click will uncapture the mouse"
		                             : " and middle-clicks will be sent to the game";

		// Only setup the Ctrl/Cmd+F10 handler if the mouse is capturable
		if (sdl.mouse.control_choice & (CaptureOnClick | CaptureOnStart))
			MAPPER_AddHandler(ToggleMouseCapture, SDL_SCANCODE_F10,
			                  PRIMARY_MOD, "capmouse", "Cap Mouse");

		// Apply the user's mouse sensitivity settings
		Prop_multival *p3 = section->Get_multival("sensitivity");
		sdl.mouse.xsensitivity = p3->GetSection()->Get_int("xsens");
		sdl.mouse.ysensitivity = p3->GetSection()->Get_int("ysens");

		// Apply raw mouse input setting
		SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP,
		                        section->Get_bool("raw_mouse_input") ? "0" : "1",
		                        SDL_HINT_OVERRIDE);
	}
	LOG_MSG("SDL: Mouse %s%s.", mouse_control_msg.c_str(), middle_control_msg.c_str());

	/* Get some Event handlers */
	MAPPER_AddHandler(RequestExit, SDL_SCANCODE_F9, PRIMARY_MOD, "shutdown",
	                  "Shutdown");
	MAPPER_AddHandler(SwitchFullScreen, SDL_SCANCODE_RETURN, MMOD2,
	                  "fullscr", "Fullscreen");
	MAPPER_AddHandler(Restart, SDL_SCANCODE_HOME, MMOD1 | MMOD2, "restart",
	                  "Restart");
#if C_DEBUG
/* Pause binds with activate-debugger */
#else
	MAPPER_AddHandler(&PauseDOSBox, SDL_SCANCODE_PAUSE, MMOD2,
	                  "pause", "Pause Emu.");
#endif
	/* Get Keyboard state of numlock and capslock */
	SDL_Keymod keystate = SDL_GetModState();
	if (keystate & KMOD_NUM)
		startup_state_numlock = true;
	if (keystate & KMOD_CAPS)
		startup_state_capslock = true;
}

static void HandleMouseMotion(SDL_MouseMotionEvent * motion) {
	if (mouse_is_captured || sdl.mouse.control_choice == Seamless)
		Mouse_CursorMoved((float)motion->xrel*sdl.mouse.xsensitivity/100.0f,
						  (float)motion->yrel*sdl.mouse.ysensitivity/100.0f,
						  (float)(motion->x-sdl.clip.x)/(sdl.clip.w-1)*sdl.mouse.xsensitivity/100.0f,
						  (float)(motion->y-sdl.clip.y)/(sdl.clip.h-1)*sdl.mouse.ysensitivity/100.0f,
						  mouse_is_captured);
}

static void HandleMouseButton(SDL_MouseButtonEvent * button) {
	switch (button->state) {
	case SDL_PRESSED:
		if (!sdl.desktop.fullscreen
		    && sdl.mouse.control_choice & (CaptureOnStart | CaptureOnClick)
		    && ((sdl.mouse.middle_will_release && button->button == SDL_BUTTON_MIDDLE)
		        || !mouse_is_captured)) {

			GFX_ToggleMouseCapture();
			break; // Don't pass click to mouse handler
		}
		switch (button->button) {
		case SDL_BUTTON_LEFT:
			Mouse_ButtonPressed(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonPressed(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonPressed(2);
			break;
		}
		break;
	case SDL_RELEASED:
		switch (button->button) {
		case SDL_BUTTON_LEFT:
			Mouse_ButtonReleased(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonReleased(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonReleased(2);
			break;
		}
		break;
	}
}

void GFX_LosingFocus()
{
	sdl.laltstate = SDL_KEYUP;
	sdl.raltstate = SDL_KEYUP;
	MAPPER_LosingFocus();
}

bool GFX_IsFullscreen(void) {
	return sdl.desktop.fullscreen;
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

#if C_OPENGL
	if (sdl.desktop.window.resizable && sdl.desktop.type == SCREEN_OPENGL) {
		sdl.clip = calc_viewport(width, height);
		glViewport(sdl.clip.x, sdl.clip.y, sdl.clip.w, sdl.clip.h);
		return;
	}
#endif

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

	SDL_SetWindowSize(sdl.window, sdl.desktop.window.width,
	                  sdl.desktop.window.height);

	// Force window position when dosbox is configured to start in
	// fullscreen.  Otherwise SDL will reset window position to 0,0 when
	// switching to a window for the first time. This is happening on every
	// OS, but only on Windows it's a really big problem (because window
	// decorations are rendered off-screen).
	//
	// To work around this problem, tell SDL to center the window on current
	// screen (we want center, as undefined position could still result in
	// placing part of the window off-screen).
	const int pos = SDL_WINDOWPOS_CENTERED_DISPLAY(sdl.display_number);
	SDL_SetWindowPosition(sdl.window, pos, pos);

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
		switch (event.type) {
		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_RESTORED:
				DEBUG_LOG_MSG("SDL: Window has been restored");
				/* We may need to re-create a texture
				 * and more on Android. Another case:
				 * Update surface while using X11.
				 */
				GFX_ResetScreen();
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
				// DEBUG_LOG_MSG("SDL: Window has gained keyboard focus");
				SetPriority(sdl.priority.focus);
				if (sdl.draw.callback)
					sdl.draw.callback(GFX_CallBackRedraw);
				GFX_UpdateMouseState();
				FocusInput();

				// When we're fullscreen, the DOS program might
				// change underlying draw resolutions, which can
				// pose a problem when switching back to
				// window-mode (beacuse the prior window-size
				// may not longer accomodate the new draw-size).
				// So in this case, we want to reset the screen
				// size - but only if we're not in the middle of
				// resizing the window (which also fires EXPOSED
				// events).
				if (!sdl.desktop.fullscreen &&
					sdl.desktop.last_size_event != SDL_WINDOWEVENT_RESIZED)
					GFX_ResetScreen();

				continue;

			case SDL_WINDOWEVENT_FOCUS_LOST:
				DEBUG_LOG_MSG("SDL: Window has lost keyboard focus");
#ifdef WIN32
				if (sdl.desktop.fullscreen) {
					VGA_KillDrawing();
					GFX_ForceFullscreenExit();
				}
#endif
				SetPriority(sdl.priority.nofocus);
				GFX_LosingFocus();
				CPU_Enable_SkipAutoAdjust();
				sdl.mouse.has_focus = false;
				break;

			case SDL_WINDOWEVENT_ENTER:
				// DEBUG_LOG_MSG("SDL: Window has gained mouse focus");
				continue;

			case SDL_WINDOWEVENT_LEAVE:
				// DEBUG_LOG_MSG("SDL: Window has lost mouse focus");
				continue;

			case SDL_WINDOWEVENT_SHOWN:
				DEBUG_LOG_MSG("SDL: Window has been shown");
				continue;

			case SDL_WINDOWEVENT_HIDDEN:
				DEBUG_LOG_MSG("SDL: Window has been hidden");
				continue;

#if 0 // ifdefed out only because it's too noisy
			case SDL_WINDOWEVENT_MOVED:
				DEBUG_LOG_MSG("SDL: Window has been moved to %d, %d",
				              event.window.data1,
				              event.window.data2);
				continue;
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
				DEBUG_LOG_MSG("SDL: Window has been minimized");
				break;

			case SDL_WINDOWEVENT_MAXIMIZED:
				DEBUG_LOG_MSG("SDL: Window has been maximized");
				continue;

			case SDL_WINDOWEVENT_CLOSE:
				DEBUG_LOG_MSG("SDL: The window manager "
				              "requests that the window be "
				              "closed");
				RequestExit(true);
				break;

			case SDL_WINDOWEVENT_TAKE_FOCUS:
				// DEBUG_LOG_MSG("SDL: Window is being offered a focus");
				// should SetWindowInputFocus() on itself or a
				// subwindow, or ignore
				continue;

			case SDL_WINDOWEVENT_HIT_TEST:
				DEBUG_LOG_MSG("SDL: Window had a hit test that "
				              "wasn't SDL_HITTEST_NORMAL");
				continue;

			default: break;
			}

			/* Non-focus priority is set to pause; check to see if we've lost window or input focus
			 * i.e. has the window been minimised or made inactive?
			 */
			if (sdl.priority.nofocus == PRIORITY_LEVEL_PAUSE) {
				if ((event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) || (event.window.event == SDL_WINDOWEVENT_MINIMIZED)) {
					/* Window has lost focus, pause the emulator.
					 * This is similar to what PauseDOSBox() does, but the exit criteria is different.
					 * Instead of waiting for the user to hit Alt+Break, we wait for the window to
					 * regain window or input focus.
					 */
					bool paused = true;
					SDL_Event ev;

					GFX_SetTitle(-1,-1,true);
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
							RequestExit(true);
							break;
						case SDL_WINDOWEVENT:     // wait until we get window focus back
							if ((ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) || (ev.window.event == SDL_WINDOWEVENT_MINIMIZED) || (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) || (ev.window.event == SDL_WINDOWEVENT_RESTORED) || (ev.window.event == SDL_WINDOWEVENT_EXPOSED)) {
								// We've got focus back, so unpause and break out of the loop
								if ((ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) || (ev.window.event == SDL_WINDOWEVENT_RESTORED) || (ev.window.event == SDL_WINDOWEVENT_EXPOSED)) {
									paused = false;
									GFX_SetTitle(-1,-1,false);
									SetPriority(sdl.priority.focus);
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

		case SDL_MOUSEMOTION:
			HandleMouseMotion(&event.motion);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if (sdl.mouse.control_choice != NoMouse)
				HandleMouseButton(&event.button);
			break;
		case SDL_QUIT: RequestExit(true); break;
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
			FALLTHROUGH;
#endif
#if defined (MACOSX)
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			/* On macs CMD-Q is the default key to close an application */
			if (event.key.keysym.sym == SDLK_q &&
			    (event.key.keysym.mod == KMOD_RGUI || event.key.keysym.mod == KMOD_LGUI)) {
				RequestExit(true);
				break;
			}
			FALLTHROUGH;
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
void GFX_ShowMsg(char const* format,...) {
	char buf[512];

	va_list msg;
	va_start(msg,format);
	vsnprintf(buf,sizeof(buf),format,msg);
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

void Config_Add_SDL() {
	Section_prop * sdl_sec=control->AddSection_prop("sdl",&GUI_StartUp);
	sdl_sec->AddInitFunction(&MAPPER_StartUp);
	Prop_bool *Pbool; // use pbool for new properties
	Prop_bool *pbool;
	Prop_string *Pstring; // use pstring for new properties
	Prop_string *pstring;
	Prop_int *Pint; // use pint for new properties
	Prop_int *pint;
	Prop_multival* Pmulti;
	Section_prop* psection;

	constexpr auto always = Property::Changeable::Always;
	// constexpr auto deprecated = Property::Changeable::Deprecated;
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

	Pbool = sdl_sec->Add_bool("vsync", on_start, false);
	Pbool->Set_help(
	        "Synchronize with display refresh rate if supported. This can\n"
	        "reduce flickering and tearing, but may also impact performance.");

	pint = sdl_sec->Add_int("vsync_skip", on_start, 7000);
	pint->Set_help("Number of microseconds to allow rendering to block before skipping "
	               "the next frame.");
	pint->SetMinMax(1, 14000);

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
	Pstring = sdl_sec->Add_string("output", Property::Changeable::Always, "opengl");
#else
	Pstring = sdl_sec->Add_string("output", Property::Changeable::Always, "texture");
#endif
	Pstring->Set_help("What video system to use for output.");
	Pstring->Set_values(outputs);

	pstring = sdl_sec->Add_string("texture_renderer", always, "auto");
	pstring->Set_help("Choose a renderer driver when using a texture output mode.\n"
	                  "Use texture_renderer=auto for an automatic choice.");
	pstring->Set_values(Get_SDL_TextureRenderers());

	// Define mouse control settings
	Pmulti = sdl_sec->Add_multi("capture_mouse", always, " ");
	const char *mouse_controls[] = {
	        "seamless", // default
	        "onclick",  "onstart", "nomouse", 0,
	};
	const char *middle_controls[] = {
	        "middlerelease", // default
	        "middlegame",
	        "", // allow empty second value for 'nomouse'
	        0,
	};
	// Generate and set the mouse control defaults from above arrays
	std::string mouse_control_defaults(mouse_controls[0]);
	mouse_control_defaults += " ";
	mouse_control_defaults += middle_controls[0];
	Pmulti->SetValue(mouse_control_defaults);

	// Add the mouse and middle control as sub-sections
	psection = Pmulti->GetSection();
	psection->Add_string("capture_mouse_first_value", always, mouse_controls[0])
	        ->Set_values(mouse_controls);
	psection->Add_string("capture_mouse_second_value", always, middle_controls[0])
	        ->Set_values(middle_controls);

	// Construct and set the help block using defaults set above
	std::string mouse_control_help("Choose a mouse control method:\n"
	                               "   onclick:        Capture the mouse when clicking inside the window.\n"
	                               "   onstart:        Capture the mouse immediately on start.\n"
	                               "   seamless:       Never capture the mouse; let it move seamlessly.\n"
	                               "   nomouse:        Hide the mouse and don't send input to the game.\n"
	                               "Choose how middle-clicks are handled (second parameter):\n"
	                               "   middlegame:     Middle-clicks are sent to the game.\n"
	                               "   middlerelease:  Middle-click will release the captured mouse.\n"
	                               "Defaults (if not present or incorrect): ");
	mouse_control_help += mouse_control_defaults;
	Pmulti->Set_help(mouse_control_help);

	Pmulti = sdl_sec->Add_multi("sensitivity",Property::Changeable::Always, ",");
	Pmulti->Set_help("Mouse sensitivity. The optional second parameter specifies vertical sensitivity (e.g. 100,-50).");
	Pmulti->SetValue("100");
	Pint = Pmulti->GetSection()->Add_int("xsens",Property::Changeable::Always,100);
	Pint->SetMinMax(-1000,1000);
	Pint = Pmulti->GetSection()->Add_int("ysens",Property::Changeable::Always,100);
	Pint->SetMinMax(-1000,1000);

	pbool = sdl_sec->Add_bool("raw_mouse_input", on_start, false);
	pbool->Set_help(
	        "Enable this setting to bypass your operating system's mouse\n"
	        "acceleration and sensitivity settings. This works in\n"
	        "fullscreen or when the mouse is captured in window mode.");

	Pbool = sdl_sec->Add_bool("waitonerror",Property::Changeable::Always, true);
	Pbool->Set_help("Wait before closing the console if dosbox has an error.");

	Pmulti = sdl_sec->Add_multi("priority", Property::Changeable::Always, ",");
	Pmulti->SetValue("auto,auto");
	Pmulti->Set_help(
	        "Priority levels for dosbox. Second entry behind the comma is for when dosbox is not focused/minimized.\n"
	        "pause is only valid for the second entry. auto disables priority levels and uses OS defaults");

	const char *actt[] = {"auto",   "lowest",  "lower", "normal",
	                      "higher", "highest", "pause", 0};
	Pstring = Pmulti->GetSection()->Add_string("active",
	                                           Property::Changeable::Always,
	                                           "higher");
	Pstring->Set_values(actt);

	const char *inactt[] = {"auto",   "lowest",  "lower", "normal",
	                        "higher", "highest", "pause", 0};
	Pstring = Pmulti->GetSection()->Add_string("inactive",
	                                           Property::Changeable::Always,
	                                           "normal");
	Pstring->Set_values(inactt);

	pstring = sdl_sec->Add_path("mapperfile", always, MAPPERFILE);
	pstring->Set_help("File used to load/save the key/event mappings from.\n"
	                  "Resetmapper only works with the default value.");

	pstring = sdl_sec->Add_string("screensaver", on_start, "auto");
	pstring->Set_help(
	        "Use 'allow' or 'block' to override the SDL_VIDEO_ALLOW_SCREENSAVER\n"
	        "environment variable (which usually blocks the OS screensaver\n"
	        "while the emulator is running).");
	const char *ssopts[] = {"auto", "allow", "block", 0};
	pstring->Set_values(ssopts);
}

static void show_warning(char const * const message) {
#ifndef WIN32
	fprintf(stderr, "%s", message);
	return;
#else
	if (!sdl.initialized && SDL_Init(SDL_INIT_VIDEO) < 0) {
		sdl.initialized = true;
		fprintf(stderr, "%s", message);
		return;
	}

	if (!sdl.window && !GFX_SetSDLSurfaceWindow(640, 400))
		return;

	sdl.surface = SDL_GetWindowSurface(sdl.window);
	if (!sdl.surface)
		return;

	SDL_Surface *splash_surf = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 400, 32,
	                                                RMASK, GMASK, BMASK, 0);
	if (!splash_surf) return;

	int x = 120,y = 20;
	std::string m(message),m2;
	std::string::size_type a,b,c,d;

	while(m.size()) { //Max 50 characters. break on space before or on a newline
		c = m.find('\n');
		d = m.rfind(' ',50);
		if(c>d) a=b=d; else a=b=c;
		if( a != std::string::npos) b++;
		m2 = m.substr(0,a); m.erase(0,b);
		OutputString(x,y,m2.c_str(),0xffffffff,0,splash_surf);
		y += 20;
	}

	SDL_BlitSurface(splash_surf, NULL, sdl.surface, NULL);
	SDL_UpdateWindowSurface(sdl.window);
	Delay(12000);
#endif // WIN32
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

static void launchcaptures(std::string const& edit) {
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
		show_warning("Warning: dosbox.conf exists in current working directory.\nThis will override the configuration file at runtime.\n");
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
		show_warning("Warning: dosbox.conf exists in current working directory.\nKeymapping might not be properly reset.\n"
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

void Disable_OS_Scaling() {
#if defined (WIN32)
	typedef BOOL (*function_set_dpi_pointer)();
	function_set_dpi_pointer function_set_dpi;
	function_set_dpi = (function_set_dpi_pointer) GetProcAddress(LoadLibrary("user32.dll"), "SetProcessDPIAware");
	if (function_set_dpi) {
		function_set_dpi();
	}
#endif
}

void OverrideWMClass()
{
#if !defined(WIN32)
	constexpr int overwrite = 0; // don't overwrite
	setenv("SDL_VIDEO_X11_WMCLASS", "dosbox-staging", overwrite);
#endif
}

void GFX_GetSize(int &width, int &height, bool &fullscreen)
{
	width = sdl.draw.width;
	height = sdl.draw.height;
	fullscreen = sdl.desktop.fullscreen;
}

int sdl_main(int argc, char *argv[])
{
	int rcode = 0; // assume good until proven otherwise
	
	// Setup logging right away
	loguru::g_preamble_date    = true; // The date field
	loguru::g_preamble_time    = true; // The time of the current day
	loguru::g_preamble_uptime  = false; // The time since init call
	loguru::g_preamble_thread  = false; // The logging thread
	loguru::g_preamble_file    = false; // The file from which the log originates from
	loguru::g_preamble_verbose = false; // The verbosity field
	loguru::g_preamble_pipe    = true; // The pipe symbol right before the message	

	loguru::init(argc, argv);

	try {
		Disable_OS_Scaling(); //Do this early on, maybe override it through some parameter.
		OverrideWMClass(); // Before SDL2 video subsystem is initialized

		CROSS_DetermineConfigPaths();

		CommandLine com_line(argc,argv);
		Config myconf(&com_line);
		control=&myconf;
		/* Init the configuration system and add default values */
		Config_Add_SDL();
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

		if (control->cmdline->FindExist("--version") ||
		    control->cmdline->FindExist("-version") ||
		    control->cmdline->FindExist("-v")) {
			printf(version_msg, DOSBOX_GetDetailedVersion());
			return 0;
		}

		//If command line includes --help or -h, print help message and exit.
		if (control->cmdline->FindExist("--help") ||
		    control->cmdline->FindExist("-h")) {
			printf(help_msg); // -V618
			return 0;
		}

		if (control->cmdline->FindExist("--printconf") ||
		    control->cmdline->FindExist("-printconf")) {
			const int err = PrintConfigLocation();
			return err;
		}

#if C_DEBUG
		DEBUG_SetupConsole();
#endif

#if defined(WIN32)
	SetConsoleCtrlHandler((PHANDLER_ROUTINE) ConsoleEventHandler,TRUE);
#endif

	LOG_MSG("dosbox-staging version %s", DOSBOX_GetDetailedVersion());
	LOG_MSG("---");

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0)
		E_Exit("Can't init SDL %s", SDL_GetError());
	sdl.initialized = true;
	// Once initialized, ensure we clean up SDL for all exit conditions
	atexit(QuitSDL);

	const auto config_path = CROSS_GetPlatformConfigDir();
	SETUP_ParseConfigFiles(config_path);

#if C_OPENGL
	const std::string glshaders_dir = config_path + "glshaders";
	if (create_dir(glshaders_dir.c_str(), 0700, OK_IF_EXISTS) != 0)
		LOG_MSG("CONFIG: Can't create dir '%s': %s",
			glshaders_dir.c_str(), safe_strerror(errno).c_str());
#endif // C_OPENGL
#if C_FLUIDSYNTH
	const std::string soundfonts_dir = config_path + "soundfonts";
	if (create_dir(soundfonts_dir.c_str(), 0700, OK_IF_EXISTS) != 0)
		LOG_MSG("CONFIG: Can't create dir '%s': %s",
			soundfonts_dir.c_str(), safe_strerror(errno).c_str());
#endif // C_FLUIDSYNTH
#if C_MT32EMU
	const std::string mt32_rom_dir = config_path + "mt32-roms";
	if (create_dir(mt32_rom_dir.c_str(), 0700, OK_IF_EXISTS) != 0)
		LOG_MSG("CONFIG: Can't create dir '%s': %s",
			mt32_rom_dir.c_str(), safe_strerror(errno).c_str());
#endif // C_MT32EMU

		control->ParseEnv();
//		UI_Init();
//		if (control->cmdline->FindExist("-startui")) UI_Run(false);
		/* Init all the sections */
		control->Init();
		/* Some extra SDL Functions */
		Section_prop * sdl_sec=static_cast<Section_prop *>(control->GetSection("sdl"));

		render_pacer.SetTimeout(sdl.desktop.vsync_skip);

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

		/* Start up main machine */
		control->StartUp();
		/* Shutdown everything */
	} catch (char * error) {
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
	}
	catch (...) {
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
