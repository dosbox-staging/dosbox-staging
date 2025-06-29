// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include <array>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <sys/types.h>
#include <tuple>
#include <unistd.h>

#if C_DEBUG
#include <queue>
#endif

#ifdef WIN32
#include <process.h>
#include <windows.h>
#endif // WIN32

#if C_OPENGL
// Glad must be included before SDL
#include "glad/gl.h"
#include <SDL.h>
#include <SDL_opengl.h>

#else
#include <SDL.h>
#endif // C_OPENGL

#include "../capture/capture.h"
#include "../dos/dos_locale.h"
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
#include "pic.h"
#include "rect.h"
#include "render.h"
#include "sdlmain.h"
#include "setup.h"
#include "string_utils.h"
#include "timer.h"
#include "titlebar.h"
#include "tracy.h"
#include "vga.h"
#include "video.h"

constexpr uint32_t sdl_version_to_uint32(const SDL_version version)
{
	return (version.major << 16) + (version.minor << 8) + version.patch;
}

static bool is_runtime_sdl_version_at_least(const SDL_version min_version)
{
	SDL_version version = {};
	SDL_GetVersion(&version);
	const auto curr_version = sdl_version_to_uint32(version);

	return curr_version >= sdl_version_to_uint32(min_version);
}

static void switch_console_to_utf8()
{
#ifdef WIN32
	constexpr uint16_t CodePageUtf8 = 65001;
	if (!sdl.original_code_page) {
		sdl.original_code_page = GetConsoleOutputCP();
		// Don't do anything if we couldn't get the original code page
		if (sdl.original_code_page) {
			SetConsoleOutputCP(CodePageUtf8);
		}
	}
#endif
}

static void restore_console_encoding()
{
#ifdef WIN32
	if (sdl.original_code_page) {
		SetConsoleOutputCP(sdl.original_code_page);
		sdl.original_code_page = 0;
	}
#endif
}

#if C_OPENGL
// Define to report opengl errors
// #define DB_OPENGL_ERROR

#ifdef DB_OPENGL_ERROR
static void maybe_log_opengl_error(const char* message)
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
static void maybe_log_opengl_error(const char*)
{
	return;
}
#endif

// SDL allows pixels sizes (colour-depth) from 1 to 4 bytes
constexpr uint8_t MaxBytesPerPixel = 4;

// A safe wrapper around that returns the default result on failure
static const char* safe_gl_get_string(const GLenum requested_name,
                                      const char* default_result = "")
{
	// the result points to a static string but can be null
	const auto result = glGetString(requested_name);

	// the default, however, needs to be valid
	assert(default_result);

	return result ? reinterpret_cast<const char*>(result) : default_result;
}

// Create a GLSL shader object, load the shader source, and compile the
// shader.
//
// `type` is an OpenGL shader stage enum, either GL_VERTEX_SHADER or
// GL_FRAGMENT_SHADER. Other shader types are not supported.
//
// Returns the compiled shader object, or zero on failure.
//
static GLuint build_shader_gl(GLenum type, const std::string& source)
{
	GLuint shader            = 0;
	GLint is_shader_compiled = 0;

	assert(source.length());

	const char* shaderSrc      = source.c_str();
	const char* src_strings[2] = {nullptr, nullptr};
	std::string top;

	// look for "#version" because it has to occur first
	const char* ver = strstr(shaderSrc, "#version ");
	if (ver) {
		const char* endline = strchr(ver + 9, '\n');
		if (endline) {
			top.assign(shaderSrc, endline - shaderSrc + 1);
			shaderSrc = endline + 1;
		}
	}

	top += (type == GL_VERTEX_SHADER) ? "#define VERTEX 1\n"
	                                  : "#define FRAGMENT 1\n";

	src_strings[0] = top.c_str();
	src_strings[1] = shaderSrc;

	// Create the shader object
	shader = glCreateShader(type);
	if (shader == 0) {
		return 0;
	}

	// Load the shader source
	glShaderSource(shader, 2, src_strings, nullptr);

	// Compile the shader
	glCompileShader(shader);

	// Check the compile status
	glGetShaderiv(shader, GL_COMPILE_STATUS, &is_shader_compiled);

	GLint log_length_bytes = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length_bytes);

	// The info log might contain warnings and info messages even if the
	// compilation was successful, so we'll always log it if it's non-empty.
	if (log_length_bytes > 1) {
		std::vector<GLchar> info_log(log_length_bytes);
		glGetShaderInfoLog(shader, log_length_bytes, nullptr, info_log.data());

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

// Build a OpenGL shader program.
//
// Input GLSL source must contain both vertex and fragment stages inside their
// respective preprocessor definitions.
//
// Returns a ready to use OpenGL shader program, or zero on failure.
//
static GLuint build_shader_program(const std::string& source)
{
	if (source.empty()) {
		LOG_ERR("OPENGL: No shader source present");
		return 0;
	}

	auto vertex_shader = build_shader_gl(GL_VERTEX_SHADER, source);
	if (!vertex_shader) {
		LOG_ERR("OPENGL: Failed compiling vertex shader");
		return 0;
	}

	auto fragment_shader = build_shader_gl(GL_FRAGMENT_SHADER, source);
	if (!fragment_shader) {
		LOG_ERR("OPENGL: Failed compiling fragment shader");
		glDeleteShader(vertex_shader);
		return 0;
	}

	const GLuint shader_program = glCreateProgram();

	if (!shader_program) {
		LOG_ERR("OPENGL: Failed creating shader program");
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);
		return 0;
	}

	glAttachShader(shader_program, vertex_shader);
	glAttachShader(shader_program, fragment_shader);

	glLinkProgram(shader_program);

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	// Check the link status
	GLint is_program_linked = GL_FALSE;
	glGetProgramiv(shader_program, GL_LINK_STATUS, &is_program_linked);

	// The info log might contain warnings and info messages even if the
	// linking was successful, so we'll always log it if it's non-empty.
	GLint log_length_bytes = 0;
	glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &log_length_bytes);

	if (log_length_bytes > 1) {
		std::vector<GLchar> info_log(static_cast<size_t>(log_length_bytes));

		glGetProgramInfoLog(shader_program,
		                    log_length_bytes,
		                    nullptr,
		                    info_log.data());

		if (is_program_linked) {
			LOG_WARNING("OPENGL: Program info log:\n %s",
			            info_log.data());
		} else {
			LOG_ERR("OPENGL: Failed linking shader program:\n %s",
			        info_log.data());
		}
	}

	if (!is_program_linked) {
		glDeleteProgram(shader_program);
		return 0;
	}

	glUseProgram(shader_program);

	// Set vertex data. Vertices in counter-clockwise order.
	const GLint vertex_attrib_location = glGetAttribLocation(shader_program,
	                                                         "a_position");

	if (vertex_attrib_location == -1) {
		LOG_ERR("OPENGL: Failed to retrieve vertex position attribute location");
		glDeleteProgram(shader_program);
		return 0;
	}

	// Lower left
	sdl.opengl.vertex_data[0] = -1.0f;
	sdl.opengl.vertex_data[1] = -1.0f;

	// Lower right
	sdl.opengl.vertex_data[2] = 3.0f;
	sdl.opengl.vertex_data[3] = -1.0f;

	// Upper left
	sdl.opengl.vertex_data[4] = -1.0f;
	sdl.opengl.vertex_data[5] = 3.0f;

	// Load the vertices' positions
	constexpr GLint NumComponents           = 2; // vec2(x, y)
	constexpr GLenum ComponentDataType      = GL_FLOAT;
	constexpr GLboolean NormalizeFixedPoint = GL_FALSE;
	constexpr GLsizei DataStride            = 0;

	glVertexAttribPointer(static_cast<GLuint>(vertex_attrib_location),
	                      NumComponents,
	                      ComponentDataType,
	                      NormalizeFixedPoint,
	                      DataStride,
	                      sdl.opengl.vertex_data);

	glEnableVertexAttribArray(static_cast<GLuint>(vertex_attrib_location));

	// Set texture slot
	const GLint texture_uniform = glGetUniformLocation(shader_program,
	                                                   "rubyTexture");
	glUniform1i(texture_uniform, 0);

	return shader_program;
}

static void get_uniform_locations_gl()
{
	sdl.opengl.ruby.texture_size = glGetUniformLocation(sdl.opengl.program_object,
	                                                    "rubyTextureSize");

	sdl.opengl.ruby.input_size = glGetUniformLocation(sdl.opengl.program_object,
	                                                  "rubyInputSize");

	sdl.opengl.ruby.output_size = glGetUniformLocation(sdl.opengl.program_object,
	                                                   "rubyOutputSize");

	sdl.opengl.ruby.frame_count = glGetUniformLocation(sdl.opengl.program_object,
	                                                   "rubyFrameCount");
}

static void update_uniforms_gl()
{
	glUniform2f(sdl.opengl.ruby.texture_size,
	            (GLfloat)sdl.opengl.texture_width_px,
	            (GLfloat)sdl.opengl.texture_height_px);

	glUniform2f(sdl.opengl.ruby.input_size,
	            (GLfloat)sdl.draw.render_width_px,
	            (GLfloat)sdl.draw.render_height_px);

	glUniform2f(sdl.opengl.ruby.output_size,
	            (GLfloat)sdl.draw_rect_px.w,
	            (GLfloat)sdl.draw_rect_px.h);

	glUniform1i(sdl.opengl.ruby.frame_count, sdl.opengl.actual_frame_count);
}

static bool init_shader_gl()
{
	GLuint prog = 0;

	// reset error
	glGetError();
	glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&prog);

	// if there was an error this context doesn't support shaders
	if (glGetError() == GL_NO_ERROR &&
	    (sdl.opengl.program_object == 0 || prog != sdl.opengl.program_object)) {

		// check if existing program is valid
		if (sdl.opengl.program_object) {
			glUseProgram(sdl.opengl.program_object);

			if (glGetError() != GL_NO_ERROR) {
				// program is not usable (probably new context),
				// purge it
				glDeleteProgram(sdl.opengl.program_object);
				sdl.opengl.program_object = 0;
			}
		}

		// does program need to be rebuilt?
		if (sdl.opengl.program_object == 0) {
			sdl.opengl.program_object = build_shader_program(
			        sdl.opengl.shader_source);

			if (sdl.opengl.program_object == 0) {
				LOG_ERR("OPENGL: Failed to compile shader");
				return false;
			}

			glUseProgram(sdl.opengl.program_object);

			get_uniform_locations_gl();
		}
	}

	return true;
}

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

static SDL_Point FallbackWindowSize = {640, 480};

static bool first_window = true;

static DosBox::Rect to_rect(const SDL_Rect r)
{
	return {r.x, r.y, r.w, r.h};
}

static SDL_Rect to_sdl_rect(const DosBox::Rect& r)
{
	return {iroundf(r.x), iroundf(r.y), iroundf(r.w), iroundf(r.h)};
}

#if C_DEBUG
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
	}
	return false;
}

SDL_Window* GFX_GetSDLWindow()
{
	return sdl.window;
}
#endif

static void QuitSDL()
{
	if (sdl.initialized) {
#if !C_DEBUG
		SDL_Quit();
#endif
	}
	restore_console_encoding();
}

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

double GFX_GetHostRefreshRate()
{
	auto refresh_rate = [] {
		SDL_DisplayMode mode = {};

		auto& sdl_rate = mode.refresh_rate;

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

// Reset and populate the vsync settings from the config. This is called
// on-demand after startup and on output mode changes (e.g., switching from
// the 'texture' backend to 'opengl').
//
static void initialize_vsync_settings()
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

void GFX_RequestExit(const bool pressed)
{
	shutdown_requested = pressed;
	if (shutdown_requested) {
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
	GFX_RefreshTitle();

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
	while (sdl.is_paused && !shutdown_requested) {
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
				GFX_RefreshTitle();
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

// Let the presentation layer safely call no-op functions.
// Useful during output initialization or transitions.
void GFX_DisengageRendering()
{
	sdl.frame.update  = update_frame_noop;
	sdl.frame.present = present_frame_noop;
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

[[maybe_unused]] static int int_log2(int val)
{
	int log = 0;
	while ((val >>= 1) != 0) {
		log++;
	}
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

	if (sdl.render_driver.starts_with("opengl")) {
		return SDL_WINDOW_OPENGL;
	}

	if (sdl.render_driver != "auto") {
		return 0;
	}

	static int default_driver_is_opengl = -1;
	if (default_driver_is_opengl >= 0) {
		return (default_driver_is_opengl ? SDL_WINDOW_OPENGL : 0);
	}

	// According to SDL2 documentation, the first driver
	// in the list is the default one.
	int i = 0;
	SDL_RendererInfo info;
	while (SDL_GetRenderDriverInfo(i++, &info) == 0) {
		if (info.flags & SDL_RENDERER_TARGETTEXTURE) {
			break;
		}
	}
	default_driver_is_opengl = std::string_view(info.name).starts_with("opengl");
	return (default_driver_is_opengl ? SDL_WINDOW_OPENGL : 0);
}

static DosBox::Rect calc_draw_rect_in_pixels(const DosBox::Rect& canvas_size_px)
{
	const DosBox::Rect render_size_px = {sdl.draw.render_width_px,
	                                     sdl.draw.render_height_px};

	const auto r = RENDER_CalcDrawRectInPixels(canvas_size_px,
	                                           render_size_px,
	                                           sdl.draw.render_pixel_aspect_ratio);

	return {iroundf(r.x), iroundf(r.y), iroundf(r.w), iroundf(r.h)};
}

// Returns the actual output size in pixels.
// Needed for DPI-scaled windows, when logical window and actual output sizes
// might not match.
static DosBox::Rect get_canvas_size_in_pixels(
        [[maybe_unused]] const RenderingBackend rendering_backend)
{
	SDL_Rect canvas_size_px = {};
#if SDL_VERSION_ATLEAST(2, 26, 0)
	SDL_GetWindowSizeInPixels(sdl.window, &canvas_size_px.w, &canvas_size_px.h);
#else
	switch (rendering_backend) {
	case RenderingBackend::Texture:
		if (SDL_GetRendererOutputSize(sdl.renderer,
		                              &canvas_size_px.w,
		                              &canvas_size_px.h) != 0) {
			LOG_ERR("SDL: Failed to retrieve output size: %s",
			        SDL_GetError());
		}
		break;
#if C_OPENGL
	case RenderingBackend::OpenGl:
		SDL_GL_GetDrawableSize(sdl.window,
		                       &canvas_size_px.w,
		                       &canvas_size_px.h);
		break;
#endif
	default:
		SDL_GetWindowSize(sdl.window, &canvas_size_px.w, &canvas_size_px.h);
	}
#endif
	const auto r = to_rect(canvas_size_px);
	assert(r.HasPositiveSize());
	return r;
}

static void maybe_log_display_properties()
{
	assert(sdl.draw.render_width_px > 0 && sdl.draw.render_height_px > 0);

	const auto canvas_size_px = get_canvas_size_in_pixels(sdl.rendering_backend);
	const auto draw_size_px = calc_draw_rect_in_pixels(canvas_size_px);

	assert(draw_size_px.HasPositiveSize());

	const auto scale_x = static_cast<double>(draw_size_px.w) / sdl.draw.render_width_px;
	const auto scale_y = static_cast<double>(draw_size_px.h) / sdl.draw.render_height_px;

	[[maybe_unused]] const auto one_per_render_pixel_aspect = scale_y / scale_x;

	const auto refresh_rate = VGA_GetRefreshRate();

	if (sdl.maybe_video_mode) {
		const auto video_mode = *sdl.maybe_video_mode;

		static VideoMode last_video_mode      = {};
		static double last_refresh_rate       = 0.0;
		static FrameMode last_frame_mode      = {};
		static DosBox::Rect last_draw_size_px = {};

		if (last_video_mode != video_mode ||
		    last_refresh_rate != refresh_rate ||
		    last_frame_mode != sdl.frame.mode ||
		    last_draw_size_px.w != draw_size_px.w ||
		    last_draw_size_px.h != draw_size_px.h) {

			const auto frame_mode = [] {
				switch (sdl.frame.mode) {
				case FrameMode::Cfr: return "CFR";
				case FrameMode::Vfr: return "VFR";
				default:
					assertm(false, "Invalid FrameMode");
					return "";
				}
			}();

			const auto& par = video_mode.pixel_aspect_ratio;

			LOG_MSG("DISPLAY: %s at %2.5g Hz %s, "
			        "scaled to %dx%d pixels with 1:%1.6g (%d:%d) pixel aspect ratio",
			        to_string(video_mode).c_str(),
			        refresh_rate,
			        frame_mode,
			        iroundf(draw_size_px.w),
			        iroundf(draw_size_px.h),
			        par.Inverse().ToDouble(),
			        static_cast<int32_t>(par.Num()),
			        static_cast<int32_t>(par.Denom()));

			last_video_mode   = video_mode;
			last_refresh_rate = refresh_rate;
			last_frame_mode   = sdl.frame.mode;
			last_draw_size_px = draw_size_px;
		}

	} else {
		LOG_MSG("SDL: Window size initialized to %dx%d pixels",
		        iroundf(draw_size_px.w),
		        iroundf(draw_size_px.h));
	}

#if 0
	LOG_MSG("DISPLAY: render_width_px: %d, render_height_px: %d, "
	        "render pixel aspect ratio: 1:%1.3g",
	        render_width_px,
	        render_height_px,
	        one_per_render_pixel_aspect);
#endif
}

static SDL_Point get_initial_window_position_or_default(const int default_val)
{
	int x, y;
	if (sdl.desktop.window.x_pos != SDL_WINDOWPOS_UNDEFINED &&
	    sdl.desktop.window.y_pos != SDL_WINDOWPOS_UNDEFINED) {
		x = sdl.desktop.window.x_pos;
		y = sdl.desktop.window.y_pos;
	} else {
		x = default_val;
		y = default_val;
	}
	return {x, y};
}

// A safer way to call SDL_SetWindowSize because it ensures the event-callback
// is disabled during the resize event. This prevents the event callback from
// firing before the window is resized in which case an endless loop can occur
//
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

static bool is_vsync_enabled()
{
	return sdl.desktop.is_fullscreen ? sdl.vsync.fullscreen : sdl.vsync.windowed;
}

static void save_rate_to_frame_period(const double rate_hz)
{
	assert(rate_hz > 0);

	// Back off by one-onethousandth to avoid hitting the vsync edge
	sdl.frame.period_ms  = 1'001.0 / rate_hz;
	const auto period_us = sdl.frame.period_ms * 1'000;
	sdl.frame.period_us  = ifloor(period_us);

	// Permit the frame period to be off by up to 90% before "out of sync"
	sdl.frame.period_us_early = ifloor(55 * period_us / 100);
	sdl.frame.period_us_late  = ifloor(145 * period_us / 100);
}

static void remove_window()
{
	if (sdl.window) {
		SDL_DestroyWindow(sdl.window);
		sdl.window = nullptr;
	}
}

static void maybe_present_synced(const bool present_if_last_skipped)
{
	// State tracking across runs
	static bool last_frame_presented = false;
	static int64_t last_sync_time    = 0;

	const auto now = GetTicksUs();

	const auto scheduler_arrival = GetTicksDiff(now, last_sync_time);

	const auto on_time = scheduler_arrival > sdl.frame.period_us_early &&
	                     scheduler_arrival < sdl.frame.period_us_late;

	const auto should_present = on_time || (present_if_last_skipped &&
	                                        !last_frame_presented);

	if (should_present) {
		sdl.frame.present();
		last_frame_presented = true;
		last_sync_time       = GetTicksUs();
	} else {
		last_frame_presented = false;
		last_sync_time       = now;
	}
}

static void setup_presentation_mode()
{
	const auto refresh_rate = VGA_GetRefreshRate();

	// Calculate the maximum number of duplicate frames before presenting.
	constexpr uint16_t MinRateHz = 10;
	sdl.frame.max_dupe_frames    = static_cast<float>(refresh_rate) / MinRateHz;

	const auto vsync_is_on = is_vsync_enabled();

	FrameMode mode = {};

	// Manual CFR or VFR modes
	if (sdl.frame.desired_mode == FrameMode::Cfr ||
	    sdl.frame.desired_mode == FrameMode::Vfr) {
		mode = sdl.frame.desired_mode;

		// Frames will be presented at the DOS rate.
		save_rate_to_frame_period(refresh_rate);
	}
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

	params.x_abs = check_cast<int32_t>(abs_x);
	params.y_abs = check_cast<int32_t>(abs_y);

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

	// TODO pixels or logical unit?
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

	// TODO pixels or logical unit?
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

	const auto canvas_size_px = get_canvas_size_in_pixels(rendering_backend);

	assert(width_in_logical_units > 0);
	const auto new_dpi_scale = canvas_size_px.w / width_in_logical_units;

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

static void clean_up_sdl_resources()
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

static SDL_Window* create_window(const RenderingBackend rendering_backend,
                                 const int width, const int height)
{
	uint32_t flags = 0;
#if C_OPENGL
	if (rendering_backend == RenderingBackend::OpenGl) {
		// TODO Maybe check for failures?

		// Request 24-bits sRGB framebuffer, don't care about depth buffer
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		if (SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1)) {
			LOG_ERR("OPENGL: Failed requesting an sRGB framebuffer: %s",
					SDL_GetError());
		}

		// Explicitly request an OpenGL 2.1 compatibility context
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
							SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

		// Request an OpenGL-ready window
		flags |= SDL_WINDOW_OPENGL;

	}
#endif
	flags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
	flags |= opengl_driver_crash_workaround(rendering_backend);

	if (!sdl.desktop.window.show_decorations) {
		flags |= SDL_WINDOW_BORDERLESS;
	}

	// Using undefined position will take care of placing and
	// restoring the window by WM.
	const auto default_val = SDL_WINDOWPOS_UNDEFINED_DISPLAY(
			sdl.display_number);

	const auto pos = get_initial_window_position_or_default(default_val);

	// ensure we don't leak
	assert(sdl.window == nullptr);

	sdl.window = SDL_CreateWindow(DOSBOX_NAME, pos.x, pos.y, width, height, flags);

	if (!sdl.window && rendering_backend == RenderingBackend::Texture &&
	    (flags & SDL_WINDOW_OPENGL)) {
		// opengl_driver_crash_workaround() call above
		// conditionally sets SDL_WINDOW_OPENGL. It sometimes
		// gets this wrong (ex. SDL_VIDEODRIVER=dummy). This can
		// only be determined reliably by trying
		// SDL_CreateWindow(). If we failed to create the
		// window, try again without it.
		flags &= ~SDL_WINDOW_OPENGL;

		sdl.window = SDL_CreateWindow(
		        DOSBOX_NAME, pos.x, pos.y, width, height, flags);
	}

	if (!sdl.window) {
		LOG_ERR("SDL: Failed to create window: %s", SDL_GetError());
		return nullptr;
	}

	return sdl.window;
}

static void enter_fullscreen(const int width, const int height)
{
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

		safe_set_window_size(sdl.desktop.fullscreen.width + 1,
		                     sdl.desktop.fullscreen.height);

		sdl.desktop.fullscreen.is_forced_borderless_fullscreen = true;

		maybe_log_display_properties();

	} else {
		SDL_DisplayMode display_mode;
		SDL_GetWindowDisplayMode(sdl.window, &display_mode);

		display_mode.w = width;
		display_mode.h = height;

		// TODO pixels or logical unit?
		if (SDL_SetWindowDisplayMode(sdl.window, &display_mode) != 0) {
			LOG_WARNING("SDL: Failed setting fullscreen mode to %dx%d at %d Hz",
			            display_mode.w,
			            display_mode.h,
			            display_mode.refresh_rate);
		}
		SDL_SetWindowFullscreen(sdl.window,
		                        sdl.desktop.fullscreen.mode ==
		                                        FullscreenMode::Standard
		                                ? enum_val(SDL_WINDOW_FULLSCREEN_DESKTOP)
		                                : enum_val(SDL_WINDOW_FULLSCREEN));
	}
}

static void exit_fullscreen()
{
	if (sdl.desktop.switching_fullscreen) {
		if (sdl.desktop.fullscreen.mode == FullscreenMode::ForcedBorderless) {
			// Restore the previous window state when exiting our "fake"
			// borderless fullscreen mode.
			if (sdl.desktop.window.show_decorations) {
				SDL_SetWindowBordered(sdl.window, SDL_TRUE);
			}
			SDL_SetWindowResizable(sdl.window, SDL_TRUE);

			safe_set_window_size(sdl.desktop.fullscreen.prev_window.width,
			                     sdl.desktop.fullscreen.prev_window.height);

			SDL_SetWindowPosition(sdl.window,
			                      sdl.desktop.fullscreen.prev_window.x_pos,
			                      sdl.desktop.fullscreen.prev_window.y_pos);

			sdl.desktop.fullscreen.is_forced_borderless_fullscreen = false;

			maybe_log_display_properties();

		} else {
			// Let SDL restore the previous window size
			constexpr auto WindowedMode = 0;
			SDL_SetWindowFullscreen(sdl.window, WindowedMode);
		}
	}
}

// Callers:
//
//   setup_scaled_window()
//	   init_gl_renderer()
//	     GFX_SetSize()
//     init_sdl_texture_renderer()
//       GFX_SetSize()
//
//   set_default_window_mode()
//     set_output()
//       sdl_section_init()
//         init_sdl_config_section()
//           sdl_main()
//       GFX_RegenerateWindow()
//         sdl_main()
//		   MAPPER_StartUp() (from sdl_mapper.cpp)
//
static SDL_Window* set_window_mode(const RenderingBackend rendering_backend,
                                   const int width, const int height,
                                   const bool is_fullscreen)
{
	if (sdl.window && sdl.resizing_window) {
		return sdl.window;
	}

	clean_up_sdl_resources();

	if (!sdl.window || (sdl.rendering_backend != rendering_backend)) {
		remove_window();

		sdl.window = create_window(rendering_backend, width, height);
		if (!sdl.window) {
			return nullptr;
		}

		// Certain functionality (like setting viewport) doesn't work
		// properly before initial window events are received.
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

		SDL_RaiseWindow(sdl.window);

		if (!is_fullscreen) {
			goto finish;
		}
	}

	// Fullscreen mode switching has its limits, and is also problematic on
	// some window managers. For now, the following may work up to some
	// level. On X11, SDL_VIDEO_X11_LEGACY_FULLSCREEN=1 can also help,
	// although it has its own issues.
	//
	if (is_fullscreen) {
		enter_fullscreen(width, height);
	} else {
		exit_fullscreen();
	}

	// Maybe some requested fullscreen resolution is unsupported?
finish:

	if (sdl.draw.has_changed) {
		setup_presentation_mode();
	}

	// Force redraw after changing the window
	if (sdl.draw.callback) {
		sdl.draw.callback(GFX_CallbackRedraw);
	}

	sdl.rendering_backend = rendering_backend;
	return sdl.window;
}

// Returns the current window; used for mapper UI.
SDL_Window* GFX_GetWindow()
{
	return sdl.window;
}

DosBox::Rect GFX_GetCanvasSizeInPixels()
{
	return get_canvas_size_in_pixels(sdl.rendering_backend);
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
	const auto canvas_size_px = get_canvas_size_in_pixels(sdl.rendering_backend);

	return RENDER_CalcRestrictedViewportSizeInPixels(canvas_size_px);
}

float GFX_GetDpiScaleFactor()
{
	return sdl.desktop.dpi_scale;
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

// Callers:
//
//   init_gl_renderer()
//	   GFX_SetSize()
//
//   init_sdl_texture_renderer()
//	   GFX_SetSize()
//
static SDL_Window* setup_scaled_window(const RenderingBackend rendering_backend)
{
	int window_width  = 0;
	int window_height = 0;

	switch (sdl.desktop.fullscreen.mode) {
	case FullscreenMode::Standard:
	case FullscreenMode::ForcedBorderless:
		if (sdl.desktop.is_fullscreen) {
			window_width  = sdl.desktop.fullscreen.width;
			window_height = sdl.desktop.fullscreen.height;
		} else {
			window_width  = sdl.desktop.window.width;
			window_height = sdl.desktop.window.height;
		}
		break;

	case FullscreenMode::Original: {
		const auto [draw_scale_x, draw_scale_y] = get_scale_factors_from_pixel_aspect_ratio(
		        sdl.draw.render_pixel_aspect_ratio);

		window_width = iround(sdl.draw.render_width_px * draw_scale_x);
		window_height = iround(sdl.draw.render_height_px * draw_scale_y);
	} break;

	default: assertm(false, "Invalid FullscreenMode value");
	}

	sdl.window = set_window_mode(rendering_backend,
	                             window_width,
	                             window_height,
	                             sdl.desktop.is_fullscreen);

	return sdl.window;
}

static bool is_using_kmsdrm_driver()
{
	const bool is_initialized = SDL_WasInit(SDL_INIT_VIDEO);
	const auto driver         = is_initialized ? SDL_GetCurrentVideoDriver()
	                                           : getenv("SDL_VIDEODRIVER");
	if (!driver) {
		return false;
	}

	std::string driver_str = driver;
	lowcase(driver_str);
	return driver_str == "kmsdrm";
}

bool operator!=(const SDL_Point lhs, const SDL_Point rhs)
{
	return lhs.x != rhs.x || lhs.y != rhs.y;
}

// Callers:
//
//   initialize_sdl_window_size()
//   finalise_window_state()
//
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
		// TODO pixels or logical unit?
		// LOG_MSG("SDL: Initialized the window size to %dx%d",
		//         bounded_size.x,
		//         bounded_size.y);
	}
}

// Texture update and presentation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static void update_frame_texture([[maybe_unused]] const uint16_t* changedLines)
{
	SDL_UpdateTexture(sdl.texture.texture,
	                  nullptr, // update entire texture
	                  sdl.texture.input_surface->pixels,
	                  sdl.texture.input_surface->pitch);
}

static std::optional<RenderedImage> get_rendered_output_from_backbuffer()
{
	// This should be impossible, but maybe the user is hitting the screen
	// capture hotkey on startup even before DOS comes alive.
	if (!sdl.maybe_video_mode) {
		LOG_WARNING(
		        "SDL: The DOS video mode needs to be set "
		        "before we can get the rendered image");
		return {};
	}

	RenderedImage image = {};

	// The draw rect can extends beyond the bounds of the window or the
	// screen in fullscreen when we're "zooming into" the DOS content in
	// `relative` viewport mode. But rendered captures should always capture
	// what we see on the screen, so only the visible part of the enlarged
	// image. Therefore, we need to clip the draw rect to the bounds of the
	// canvas (the total visible area of the window or screen), and only
	// capture the resulting output rectangle.

	auto canvas_rect_px = get_canvas_size_in_pixels(sdl.rendering_backend);
	canvas_rect_px.x    = 0.0f;
	canvas_rect_px.y    = 0.0f;

	const auto output_rect_px = canvas_rect_px.Copy().Intersect(
	        to_rect(sdl.draw_rect_px));

	auto allocate_image = [&]() {
		image.params.width              = iroundf(output_rect_px.w);
		image.params.height             = iroundf(output_rect_px.h);
		image.params.double_width       = false;
		image.params.double_height      = false;
		image.params.pixel_aspect_ratio = {1};
		image.params.pixel_format       = PixelFormat::BGR24_ByteArray;

		assert(sdl.maybe_video_mode);
		image.params.video_mode = *sdl.maybe_video_mode;

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

		glReadPixels(iroundf(output_rect_px.x),
		             iroundf(output_rect_px.y),
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
	const SDL_Rect read_rect_px = to_sdl_rect(output_rect_px);

	if (SDL_RenderReadPixels(renderer,
	                         &read_rect_px,
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

static void present_frame_texture()
{
	SDL_RenderClear(sdl.renderer);
	SDL_RenderCopy(sdl.renderer, sdl.texture.texture, nullptr, nullptr);

	if (CAPTURE_IsCapturingPostRenderImage()) {
		// glReadPixels() implicitly blocks until all pipelined
		// rendering commands have finished, so we're guaranteed
		// to read the contents of the up-to-date backbuffer
		// here right before the buffer swap.
		//
		const auto image = get_rendered_output_from_backbuffer();
		if (image) {
			CAPTURE_AddPostRenderImage(*image);
		}
	}

	SDL_RenderPresent(sdl.renderer);
}

#if C_OPENGL

// OpenGL frame-based update and presentation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void update_frame_gl(const uint16_t* changedLines)
{
	if (changedLines) {
		const auto framebuf = static_cast<uint8_t*>(sdl.opengl.framebuf);
		const auto pitch = sdl.opengl.pitch;

		int y        = 0;
		size_t index = 0;

		while (y < sdl.draw.render_height_px) {
			if (!(index & 1)) {
				y += changedLines[index];
			} else {
				const uint8_t* pixels = framebuf + y * pitch;
				const int height_px   = changedLines[index];

				glTexSubImage2D(GL_TEXTURE_2D,
				                0,
				                0,
				                y,
				                sdl.draw.render_width_px,
				                height_px,
				                GL_BGRA_EXT,
				                GL_UNSIGNED_INT_8_8_8_8_REV,
				                pixels);
				y += height_px;
			}
			index++;
		}
	} else {
		sdl.opengl.actual_frame_count++;
	}
}

static void present_frame_gl()
{
	glClear(GL_COLOR_BUFFER_BIT);

	sdl.opengl.actual_frame_count++;
	update_uniforms_gl();

	glDrawArrays(GL_TRIANGLES, 0, 3);

	if (CAPTURE_IsCapturingPostRenderImage()) {
		// glReadPixels() implicitly blocks until all pipelined
		// rendering commands have finished, so we're
		// guaranateed to read the contents of the up-to-date
		// backbuffer here right before the buffer swap.
		//
		const auto image = get_rendered_output_from_backbuffer();
		if (image) {
			CAPTURE_AddPostRenderImage(*image);
		}
	}

	SDL_GL_SwapWindow(sdl.window);
}

static void set_vsync_gl(const bool is_enabled)
{
	assert(sdl.opengl.context);

	const auto swap_interval = is_enabled ? 1 : 0;

	if (SDL_GL_SetSwapInterval(swap_interval) != 0) {
		// The requested swap_interval is not supported
		LOG_WARNING("OPENGL: Failed %s vsync: %s",
		            (is_enabled ? "enabling" : "disabling"),
		            SDL_GetError());
		return;
	}

	if (is_enabled) {
		LOG_INFO("OPENGL: Enabled vsync");
	} else {
		LOG_INFO("OPENGL: Disabled vsync");
	}
}

// Callers:
//
//   GFX_SetSize()
//
std::optional<uint8_t> init_gl_renderer(const uint8_t flags, const int render_width_px,
                                        const int render_height_px)
{
	free(sdl.opengl.framebuf);
	sdl.opengl.framebuf = nullptr;

	if (!(flags & GFX_CAN_32)) {
		return {};
	}

	sdl.opengl.texture_width_px  = render_width_px;
	sdl.opengl.texture_height_px = render_height_px;

	if (sdl.opengl.texture_width_px > sdl.opengl.max_texsize ||
	    sdl.opengl.texture_height_px > sdl.opengl.max_texsize) {
		LOG_WARNING(
		        "OPENGL: No support for texture size of %dx%d pixels, "
		        "falling back to texture",
		        sdl.opengl.texture_width_px,
		        sdl.opengl.texture_height_px);
		return {};
	}

	// Re-apply the minimum bounds prior to clipping the OpenGL
	// window because SDL invalidates the prior bounds in the above
	// window changes.
	SDL_SetWindowMinimumSize(sdl.window,
	                         FallbackWindowSize.x,
	                         FallbackWindowSize.y);

	setup_scaled_window(RenderingBackend::OpenGl);

	// We may simply use SDL_BYTESPERPIXEL here rather than
	// SDL_BITSPERPIXEL
	if (!sdl.window ||
	    SDL_BYTESPERPIXEL(SDL_GetWindowPixelFormat(sdl.window)) < 2) {
		LOG_WARNING("OPENGL: Can't open drawing window, are you running in 16bpp (or higher) mode?");
		return {};
	}

	if (!init_shader_gl()) {
		return {};
	}

	// Create the texture
	const auto framebuffer_bytes = static_cast<size_t>(render_width_px) *
	                               render_height_px * MaxBytesPerPixel;

	sdl.opengl.framebuf = malloc(framebuffer_bytes); // 32 bit colour
	sdl.opengl.pitch    = render_width_px * 4;

	// One-time initialize the window size
	if (!sdl.desktop.window.adjusted_initial_size) {
		initialize_sdl_window_size(sdl.window,
		                           FallbackWindowSize,
		                           {sdl.desktop.window.width,
		                            sdl.desktop.window.height});

		sdl.desktop.window.adjusted_initial_size = true;
	}

	const auto canvas_size_px = get_canvas_size_in_pixels(
	        sdl.want_rendering_backend);

	// LOG_MSG("Attempting to fix the centering to %d %d %d %d",
	//         (canvas_size_px.w - sdl.clip.w) / 2,
	//         (canvas_size_px.h - sdl.clip.h) / 2,
	//         sdl.clip.w,
	//         sdl.clip.h);

	sdl.draw_rect_px = to_sdl_rect(calc_draw_rect_in_pixels(canvas_size_px));

	glViewport(sdl.draw_rect_px.x,
	           sdl.draw_rect_px.y,
	           sdl.draw_rect_px.w,
	           sdl.draw_rect_px.h);

	if (sdl.opengl.texture > 0) {
		glDeleteTextures(1, &sdl.opengl.texture);
	}
	glGenTextures(1, &sdl.opengl.texture);
	glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);

	// No borders
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	const int filter_mode = [&] {
		switch (sdl.opengl.shader_info.settings.texture_filter_mode) {
		case TextureFilterMode::Nearest: return GL_NEAREST;
		case TextureFilterMode::Linear: return GL_LINEAR;
		default: assertm(false, "Invalid TextureFilterMode"); return 0;
		}
	}();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_mode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_mode);

	const auto texture_area_bytes = static_cast<size_t>(
	                                        sdl.opengl.texture_width_px) *
	                                sdl.opengl.texture_height_px *
	                                MaxBytesPerPixel;

	uint8_t* emptytex = new uint8_t[texture_area_bytes];
	assert(emptytex);

	memset(emptytex, 0, texture_area_bytes);

	int is_double_buffering_enabled = 0;
	if (SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &is_double_buffering_enabled)) {

		LOG_WARNING("OPENGL: Failed getting double buffering status: %s",
		            SDL_GetError());
	} else {
		if (!is_double_buffering_enabled) {
			LOG_WARNING("OPENGL: Double buffering not enabled");
		}
	}

	int is_framebuffer_srgb_capable = 0;
	if (SDL_GL_GetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,
	                        &is_framebuffer_srgb_capable)) {

		LOG_WARNING("OPENGL: Failed getting the framebuffer's sRGB status: %s",
		            SDL_GetError());
	}

	const auto use_srgb_framebuffer = [&] {
		if (sdl.opengl.shader_info.settings.use_srgb_framebuffer) {
			if (is_framebuffer_srgb_capable > 0) {
				return true;
			} else {
				LOG_WARNING("OPENGL: sRGB framebuffer not supported");
			}
		}
		return false;
	}();

	// Using GL_SRGB8_ALPHA8 because GL_SRGB8 doesn't work properly
	// with Mesa drivers on certain integrated Intel GPUs
	const auto texformat = sdl.opengl.shader_info.settings.use_srgb_texture &&
	                                       use_srgb_framebuffer
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
	             sdl.opengl.texture_width_px,
	             sdl.opengl.texture_height_px,
	             0,
	             GL_BGRA_EXT,
	             GL_UNSIGNED_BYTE,
	             emptytex);

	delete[] emptytex;

	if (use_srgb_framebuffer) {
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

	sdl.opengl.actual_frame_count = 0;

	// Set shader variables
	update_uniforms_gl();

	set_vsync_gl(is_vsync_enabled());

	maybe_log_opengl_error("End of setsize");

	sdl.frame.update  = update_frame_gl;
	sdl.frame.present = present_frame_gl;

	return GFX_CAN_32 | GFX_CAN_RANDOM;
}

#endif

static void set_vsync_sdl_texture(const bool is_enabled)
{
	// https://wiki.libsdl.org/SDL_HINT_RENDER_VSYNC - can only be
	// set to "1", "0", adapative is currently not supported, so we
	// also treat it as "1"
	const auto hint_str = is_enabled ? "1" : "0";

	if (SDL_SetHint(SDL_HINT_RENDER_VSYNC, hint_str) == SDL_FALSE) {
		LOG_WARNING("SDL: Failed %s vsync: %s",
		            (is_enabled ? "enabling" : "disabling"),
		            SDL_GetError());
		return;
	}

	if (is_enabled) {
		LOG_INFO("SDL: Enabled vsync");
	} else {
		LOG_INFO("SDL: Disabled vsync");
	}
}

// Callers:
//
//   GFX_SetSize()
//
uint8_t init_sdl_texture_renderer()
{
	uint8_t flags = 0;

	if (!setup_scaled_window(RenderingBackend::Texture)) {
		LOG_ERR("DISPLAY: Can't initialise 'texture' window");
		E_Exit("SDL: Failed to create window");
	}

	// Use renderer's default format
	SDL_RendererInfo rinfo;
	SDL_GetRendererInfo(sdl.renderer, &rinfo);

	const auto texture_format = rinfo.texture_formats[0];

	sdl.texture.texture = SDL_CreateTexture(sdl.renderer,
	                                        texture_format,
	                                        SDL_TEXTUREACCESS_STREAMING,
	                                        sdl.draw.render_width_px,
	                                        sdl.draw.render_height_px);

	if (!sdl.texture.texture) {
		SDL_DestroyRenderer(sdl.renderer);
		sdl.renderer = nullptr;
		E_Exit("SDL: Failed to create texture");
	}

	// release the existing surface if needed
	auto& texture_input_surface = sdl.texture.input_surface;
	if (texture_input_surface) {
		SDL_FreeSurface(texture_input_surface);
		texture_input_surface = nullptr;
	}

	// ensure we don't leak
	assert(texture_input_surface == nullptr);

	texture_input_surface = SDL_CreateRGBSurfaceWithFormat(
	        0, sdl.draw.render_width_px, sdl.draw.render_height_px, 32, texture_format);

	if (!texture_input_surface) {
		E_Exit("SDL: Error while preparing texture input");
	}

	SDL_SetRenderDrawColor(sdl.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);

	uint32_t pixel_format;
	assert(sdl.texture.texture);
	SDL_QueryTexture(sdl.texture.texture, &pixel_format, nullptr, nullptr, nullptr);

	// ensure we don't leak
	assert(sdl.texture.pixelFormat == nullptr);
	sdl.texture.pixelFormat = SDL_AllocFormat(pixel_format);

	switch (SDL_BITSPERPIXEL(pixel_format)) {
	case 8: flags = GFX_CAN_8; break;
	case 15: flags = GFX_CAN_15; break;
	case 16: flags = GFX_CAN_16; break;
	case 24: // SDL_BYTESPERPIXEL is probably 4, though.
	case 32: flags = GFX_CAN_32; break;
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
		flags |= GFX_CAN_RANDOM;
	}

	// Re-apply the minimum bounds prior to clipping the
	// texture-based window because SDL invalidates the prior bounds
	// in the above changes.
	SDL_SetWindowMinimumSize(sdl.window,
	                         FallbackWindowSize.x,
	                         FallbackWindowSize.y);

	// One-time intialize the window size
	if (!sdl.desktop.window.adjusted_initial_size) {
		initialize_sdl_window_size(sdl.window,
		                           FallbackWindowSize,
		                           {sdl.desktop.window.width,
		                            sdl.desktop.window.height});

		sdl.desktop.window.adjusted_initial_size = true;
	}

	const auto canvas_size_px = get_canvas_size_in_pixels(
	        sdl.want_rendering_backend);

	// LOG_MSG("Attempting to fix the centering to %d %d %d %d",
	//         (canvas.w - sdl.clip.w) / 2,
	//         (canvas.h - sdl.clip.h) / 2,
	//         sdl.clip.w,
	//         sdl.clip.h);
	sdl.draw_rect_px = to_sdl_rect(calc_draw_rect_in_pixels(canvas_size_px));

	if (SDL_RenderSetViewport(sdl.renderer, &sdl.draw_rect_px) != 0) {
		LOG_ERR("SDL: Failed to set viewport: %s", SDL_GetError());
	}

	set_vsync_sdl_texture(is_vsync_enabled());

	sdl.frame.update  = update_frame_texture;
	sdl.frame.present = present_frame_texture;

	return flags;
}

uint8_t GFX_SetSize(const int render_width_px, const int render_height_px,
                    const Fraction& render_pixel_aspect_ratio, const uint8_t flags,
                    const VideoMode& video_mode, GFX_Callback_t callback)
{
	uint8_t retFlags = 0;
	if (sdl.updating) {
		GFX_EndUpdate(nullptr);
	}

	GFX_DisengageRendering();
	// The rendering objects are recreated below with new sizes, after which
	// frame rendering is re-engaged with the output-type specific calls.

	const bool double_width  = flags & GFX_DBL_W;
	const bool double_height = flags & GFX_DBL_H;

	sdl.draw.has_changed = (sdl.maybe_video_mode != video_mode ||
	                        sdl.draw.render_width_px != render_width_px ||
	                        sdl.draw.render_height_px != render_height_px ||
	                        sdl.draw.width_was_doubled != double_width ||
	                        sdl.draw.height_was_doubled != double_height ||
	                        sdl.draw.render_pixel_aspect_ratio !=
	                                render_pixel_aspect_ratio);

	sdl.draw.render_width_px           = render_width_px;
	sdl.draw.render_height_px          = render_height_px;
	sdl.draw.width_was_doubled         = double_width;
	sdl.draw.height_was_doubled        = double_height;
	sdl.draw.render_pixel_aspect_ratio = render_pixel_aspect_ratio;

	sdl.maybe_video_mode = video_mode;

	sdl.draw.callback = callback;

	if (sdl.want_rendering_backend == RenderingBackend::OpenGl) {
#if C_OPENGL
		if (const auto result = init_gl_renderer(flags,
		                                         render_width_px,
		                                         render_height_px);
		    result) {
			retFlags = *result;
		} else {
			// Initialising OpenGL renderer failed; fallback to SDL
			// texture
			sdl.want_rendering_backend = RenderingBackend::Texture;
		}
#else
		// Should never occur, but fallback to texture in release builds
		assertm(false, "OpenGL is not supported by this executable");
		LOG_ERR("SDL: OpenGL is not supported by this executable, "
		        "falling back to 'texture' output");

		sdl.want_rendering_backend = RenderingBackend::Texture;
#endif
	}

	if (sdl.want_rendering_backend == RenderingBackend::Texture) {
		retFlags = init_sdl_texture_renderer();
	}

	// Ensure mouse emulation knows the current parameters
	notify_new_mouse_screen_params();

	if (sdl.draw.has_changed) {
		maybe_log_display_properties();
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

	if (sdl.opengl.program_object) {
		glDeleteProgram(sdl.opengl.program_object);
		sdl.opengl.program_object = 0;
	}
#endif
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
		const auto canvas_size_px = get_canvas_size_in_pixels(
		        sdl.rendering_backend);

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
#endif

static void switch_fullscreen()
{
	sdl.desktop.switching_fullscreen = true;

	// Record the window's current canvas size if we're departing window-mode
	if (!sdl.desktop.is_fullscreen) {
		sdl.desktop.window.canvas_size = to_sdl_rect(
		        get_canvas_size_in_pixels(sdl.rendering_backend));
	}

#if defined(WIN32)
	// We are about to switch to the opposite of our current mode
	// (ie: opposite of whatever sdl.desktop.is_fullscreen holds).
	// Sticky-keys should be set to the opposite of fullscreen,
	// so we simply apply the bool of the mode we're switching out-of.
	sticky_keys(sdl.desktop.is_fullscreen);
#endif
	sdl.desktop.is_fullscreen = !sdl.desktop.is_fullscreen;

	set_section_property_value("sdl",
	                           "fullscreen",
	                           sdl.desktop.is_fullscreen ? "on" : "off");

	GFX_ResetScreen();

	focus_input();
	setup_presentation_mode();

	sdl.desktop.switching_fullscreen = false;
}

static void switch_fullscreen_handler(bool pressed)
{
	if (pressed) {
		switch_fullscreen();
	}
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
bool GFX_StartUpdate(uint8_t*& pixels, int& pitch)
{
	if (!sdl.active || sdl.updating) {
		return false;
	}

	switch (sdl.rendering_backend) {
	case RenderingBackend::Texture:
		assert(sdl.texture.input_surface);

		pixels = static_cast<uint8_t*>(sdl.texture.input_surface->pixels);
		pitch = sdl.texture.input_surface->pitch;

		sdl.updating = true;
		return true;

	case RenderingBackend::OpenGl:
#if C_OPENGL
		pixels = static_cast<uint8_t*>(sdl.opengl.framebuf);
		maybe_log_opengl_error("end of start update");

		if (pixels == nullptr) {
			return false;
		}

		static_assert(std::is_same<decltype(pitch), decltype((sdl.opengl.pitch))>::value,
		              "Our internal pitch types should be the same.");

		pitch        = sdl.opengl.pitch;
		sdl.updating = true;
		return true;
#else
		// Should never occur
		E_Exit("SDL: OpenGL is not supported by this executable");
#endif // C_OPENGL
	}
	return false;
}

void GFX_EndUpdate(const uint16_t* changedLines)
{
	static int64_t cumulative_time_rendered_us = 0;

	const auto start_us = GetTicksUs();

	sdl.frame.update(changedLines);

	if (CAPTURE_IsCapturingPostRenderImage()) {
		// Always present the frame if we want to capture the next
		// rendered frame, regardless of the presentation mode. This is
		// necessary to keep the contents of rendered and raw/upscaled
		// screenshots in sync (so they capture the exact same frame) in
		// multi-output image capture modes.
		sdl.frame.present();

	} else {
		// Helper lambda indicating whether the frame should be
		// presented. Returns true if the frame has been updated or if
		// the limit of sequentially skipped duplicate frames has been
		// reached.
		auto vfr_should_present = []() {
			static uint16_t dupe_tally = 0;
			if (sdl.updating || ++dupe_tally > sdl.frame.max_dupe_frames) {
				dupe_tally = 0;
				return true;
			}
			return false;
		};

		switch (sdl.frame.mode) {
		case FrameMode::Cfr: maybe_present_synced(sdl.updating); break;
		case FrameMode::Vfr:
			if (vfr_should_present()) {
				sdl.frame.present();
			}
			break;
		}
	}

	const auto elapsed_us = GetTicksUsSince(start_us);
	cumulative_time_rendered_us += elapsed_us;

	// Update "ticks done" with the rendering time
	constexpr auto MicrosInMillisecond = 1000;

	if (cumulative_time_rendered_us >= MicrosInMillisecond) {
		// 1 tick == 1 millisecond
		const auto cumulative_ticks_rendered = cumulative_time_rendered_us /
		                                       MicrosInMillisecond;

		DOSBOX_SetTicksDone(DOSBOX_GetTicksDone() - cumulative_ticks_rendered);

		// Keep the fractional microseconds part
		cumulative_time_rendered_us %= MicrosInMillisecond;
	}

	sdl.updating = false;

	FrameMark;
}

uint32_t GFX_GetRGB(const uint8_t red, const uint8_t green, const uint8_t blue)
{
	switch (sdl.rendering_backend) {
	case RenderingBackend::Texture:
		assert(sdl.texture.pixelFormat);
		return SDL_MapRGB(sdl.texture.pixelFormat, red, green, blue);

	case RenderingBackend::OpenGl:
		return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
	}
	return 0;
}

void GFX_Stop()
{
	if (sdl.updating) {
		GFX_EndUpdate(nullptr);
	}
	sdl.active = false;
}

void GFX_Start()
{
	sdl.active = true;
}

static void shutdown_gui(Section*)
{
	GFX_Stop();

	if (sdl.draw.callback) {
		(sdl.draw.callback)(GFX_CallbackStop);
	}

	GFX_SetMouseCapture(false);
	GFX_SetMouseVisibility(true);

	clean_up_sdl_resources();

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

static void set_priority(PRIORITY_LEVELS level)
{
	// Just let the OS scheduler manage priority
	if (level == PRIORITY_LEVEL_AUTO) {
		return;
	}

	// TODO replace platform-specific API with SDL_SetThreadPriority
#if defined(HAVE_SETPRIORITY)
	// If the priorities are different, do nothing unless the user is root,
	// since priority can always be lowered but requires elevated rights
	// to increase

	if ((sdl.priority.active != sdl.priority.inactive) && (getuid() != 0)) {
		return;
	}

#endif
	switch (level) {
#ifdef WIN32
	case PRIORITY_LEVEL_LOWEST:
		SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
		break;

	case PRIORITY_LEVEL_LOWER:
		SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
		break;

	case PRIORITY_LEVEL_NORMAL:
		SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
		break;

	case PRIORITY_LEVEL_HIGHER:
		SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
		break;

	case PRIORITY_LEVEL_HIGHEST:
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
		break;

#elif defined(HAVE_SETPRIORITY)
	// Linux use group as dosbox has mulitple threads under Linux
	case PRIORITY_LEVEL_LOWEST: setpriority(PRIO_PGRP, 0, PRIO_MAX); break;
	case PRIORITY_LEVEL_LOWER:
		setpriority(PRIO_PGRP, 0, PRIO_MAX - (PRIO_TOTAL / 3));
		break;

	case PRIORITY_LEVEL_NORMAL:
		setpriority(PRIO_PGRP, 0, PRIO_MAX - (PRIO_TOTAL / 2));
		break;

	case PRIORITY_LEVEL_HIGHER:
		setpriority(PRIO_PGRP, 0, PRIO_MAX - ((3 * PRIO_TOTAL) / 5));
		break;

	case PRIORITY_LEVEL_HIGHEST:
		setpriority(PRIO_PGRP, 0, PRIO_MAX - ((3 * PRIO_TOTAL) / 4));
		break;
#endif
	default: break;
	}
}

// Callers:
//
//   set_output()
//     sdl_section_init()
//       init_sdl_config_section()
//         sdl_main()
//     GFX_RegenerateWindow()
//       sdl_main()
//	   MAPPER_StartUp() (from sdl_mapper.cpp)
//
static SDL_Window* set_default_window_mode()
{
	if (sdl.window) {
		return sdl.window;
	}

	// TODO: this cannot be correct; at least it would need conversion to
	// pixels, and we can't correlate render and window dimensions like this
	sdl.draw.render_width_px  = FallbackWindowSize.x;
	sdl.draw.render_height_px = FallbackWindowSize.y;

	if (sdl.desktop.is_fullscreen) {
		sdl.desktop.lazy_init_window_size = true;

		return set_window_mode(sdl.want_rendering_backend,
		                       sdl.desktop.fullscreen.width,
		                       sdl.desktop.fullscreen.height,
		                       sdl.desktop.is_fullscreen);
	}

	sdl.desktop.lazy_init_window_size = false;

	return set_window_mode(sdl.want_rendering_backend,
	                       sdl.desktop.window.width,
	                       sdl.desktop.window.height,
	                       sdl.desktop.is_fullscreen);
}

static SDL_Point refine_window_size(const SDL_Point size,
                                    const bool wants_aspect_ratio_correction)
{
	// TODO This only works for 320x200 games. We cannot make hardcoded
	// assumptions about aspect ratios in general, e.g. the pixel aspect
	// ratio is 1:1 for 640x480 games both with 'aspect = on` and 'aspect =
	// off'.
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

	return FallbackWindowSize;
}

// Callers:
//
//   parse_window_resolution_from_conf()
//     setup_window_sizes_from_conf()
//       set_output()
//         sdl_section_init()
//           init_sdl_config_section()
//             sdl_main()
//         GFX_RegenerateWindow()
//           sdl_main()
//		     MAPPER_StartUp() (from sdl_mapper.cpp)
//
//   sdl_section_init()
//     init_sdl_config_section()
//       sdl_main()
//
static void maybe_limit_requested_resolution(int& w, int& h,
                                             const char* size_description)
{
	const auto desktop = get_desktop_size();
	if (w <= desktop.w && h <= desktop.h) {
		return;
	}

	bool was_limited = false;

	// Add any driver / platform / operating system limits in succession:

	// SDL KMSDRM limitations:
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

// Callers:
//
//   setup_window_sizes_from_conf()
//
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

	LOG_WARNING(
	        "DISPLAY: Invalid 'window_size' setting: '%s', "
	        "using 'default'",
	        pref.c_str());

	return FallbackWindowSize;
}

// Callers:
//
//    setup_window_sizes_from_conf()
//    set_output()
//       sdl_section_init()
//         init_sdl_config_section()
//           sdl_main()
//       GFX_RegenerateWindow()
//         sdl_main()
//	         MAPPER_StartUp() (from sdl_mapper.cpp)
//
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

static void setup_initial_window_position_from_conf(const std::string& window_position_val)
{
	sdl.desktop.window.x_pos = SDL_WINDOWPOS_UNDEFINED;
	sdl.desktop.window.y_pos = SDL_WINDOWPOS_UNDEFINED;

	if (window_position_val == "auto") {
		return;
	}

	int x, y;
	const auto was_parsed = (sscanf(window_position_val.c_str(), "%d,%d", &x, &y) ==
	                         2);
	if (!was_parsed) {
		LOG_WARNING(
		        "DISPLAY: Invalid 'window_position' setting: '%s'. "
		        "Must be in X,Y format, using 'auto'.",
		        window_position_val.c_str());
		return;
	}

	const auto desktop = get_desktop_size();

	const bool is_out_of_bounds = x < 0 || x > desktop.w || y < 0 ||
	                              y > desktop.h;
	if (is_out_of_bounds) {
		LOG_WARNING(
		        "DISPLAY: Invalid 'window_position' setting: '%s'. "
		        "Requested position is outside the bounds of the %dx%d "
		        "desktop, using 'auto'.",
		        window_position_val.c_str(),
		        desktop.w,
		        desktop.h);
		return;
	}

	sdl.desktop.window.x_pos = x;
	sdl.desktop.window.y_pos = y;
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
static void setup_window_sizes_from_conf(const bool wants_aspect_ratio_correction)
{

	const auto window_size_pref = [&]() {
		const auto legacy_pref = get_sdl_section()->GetString("windowresolution");
		if (!legacy_pref.empty()) {
			set_section_property_value("sdl", "windowresolution", "");
			set_section_property_value("sdl", "window_size", legacy_pref);
		}
		return get_sdl_section()->GetString("window_size");
	}();

	// Get the coarse resolution from the users setting, and adjust
	// refined scaling mode if an exact resolution is desired.
	SDL_Point coarse_size  = FallbackWindowSize;

	sdl.use_exact_window_resolution = window_size_pref.find('x') != std::string::npos;

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
		                                  wants_aspect_ratio_correction);
	}

	assert(refined_size.x <= UINT16_MAX && refined_size.y <= UINT16_MAX);

	save_window_size(refined_size.x, refined_size.y);

	// Let the user know the resulting window properties
	LOG_MSG("DISPLAY: Using %dx%d window size in windowed mode on display-%d",
	        refined_size.x,
	        refined_size.y,
	        sdl.display_number);
}

InterpolationMode GFX_GetTextureInterpolationMode()
{
	return sdl.texture.interpolation_mode;
}

// Callers:
//
//   sdl_section_init()
//     init_sdl_config_section()
//       sdl_main()
//
//   GFX_RegenerateWindow()
//     sdl_main()
//     MAPPER_StartUp() (from sdl_mapper.cpp)
//
static void set_output(Section* sec, const bool wants_aspect_ratio_correction)
{
	const auto section = static_cast<const SectionProp*>(sec);
	std::string output = section->GetString("output");

	GFX_DisengageRendering();
	// it's the job of everything after this to re-engage it.

	if (output == "texture") {
		sdl.want_rendering_backend     = RenderingBackend::Texture;
		sdl.texture.interpolation_mode = InterpolationMode::Bilinear;

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	} else if (output == "texturenb") {
		sdl.want_rendering_backend = RenderingBackend::Texture;
		sdl.texture.interpolation_mode = InterpolationMode::NearestNeighbour;

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

#if C_OPENGL
	} else if (output == "opengl") {
		sdl.want_rendering_backend = RenderingBackend::OpenGl;
#endif

	} else {
		LOG_WARNING("SDL: Unsupported output device '%s', using 'texture' output mode",
		            output.c_str());

		sdl.want_rendering_backend = RenderingBackend::Texture;
	}

	sdl.render_driver = section->GetString("texture_renderer");
	lowcase(sdl.render_driver);

	if (sdl.render_driver != "auto") {
		if (SDL_SetHint(SDL_HINT_RENDER_DRIVER,
		                sdl.render_driver.c_str()) == SDL_FALSE) {
			LOG_WARNING(
			        "SDL: Failed to set '%s' texture renderer driver; "
			        "falling back to automatic selection",
			        sdl.render_driver.c_str());
		}
	}

	sdl.desktop.window.show_decorations = section->GetBool("window_decorations");

	setup_initial_window_position_from_conf(
	        section->GetString("window_position"));

	setup_window_sizes_from_conf(wants_aspect_ratio_correction);

#if C_OPENGL
	if (sdl.want_rendering_backend == RenderingBackend::OpenGl) {
		if (!set_default_window_mode()) {
			LOG_WARNING(
			        "OPENGL: Could not create OpenGL window, "
			        "using 'texture' output mode");

			sdl.want_rendering_backend = RenderingBackend::Texture;

		} else {
			sdl.opengl.context = SDL_GL_CreateContext(sdl.window);

			if (sdl.opengl.context == nullptr) {
				LOG_WARNING(
				        "OPENGL: Could not create context, "
				        "using 'texture' output mode");

				sdl.want_rendering_backend = RenderingBackend::Texture;
			}
		}

		if (sdl.want_rendering_backend == RenderingBackend::OpenGl) {
			sdl.opengl.program_object = 0;

			const auto version = gladLoadGL(
			        (GLADloadfunc)SDL_GL_GetProcAddress);

			LOG_INFO("OPENGL: Version: %d.%d, GLSL version: %s, vendor: %s",
			         GLAD_VERSION_MAJOR(version),
			         GLAD_VERSION_MINOR(version),
			         safe_gl_get_string(GL_SHADING_LANGUAGE_VERSION,
			                            "unknown"),
			         safe_gl_get_string(GL_VENDOR, "unknown"));

			glGetIntegerv(GL_MAX_TEXTURE_SIZE, &sdl.opengl.max_texsize);

			sdl.opengl.framebuf = nullptr;
			sdl.opengl.texture  = 0;
		}
	}
#endif // OPENGL

	if (!set_default_window_mode()) {
		E_Exit("SDL: Could not initialize video: %s", SDL_GetError());
	}

	const auto transparency = clamp(section->GetInt("transparency"), 0, 90);
	const auto alpha = static_cast<float>(100 - transparency) / 100.0f;

	SDL_SetWindowOpacity(sdl.window, alpha);
	GFX_RefreshTitle();

	RENDER_Reinit();
}

static void apply_active_settings()
{
	set_priority(sdl.priority.active);
	MOUSE_NotifyWindowActive(true);

	if (sdl.mute_when_inactive && !MIXER_IsManuallyMuted()) {
		MIXER_Unmute();
	}

	// At least on some platforms grabbing the keyboard has to be repeated
	// each time we regain focus
	if (sdl.window) {
		const auto capture_keyboard = get_sdl_section()->GetBool(
		        "keyboard_capture");

		SDL_SetWindowKeyboardGrab(sdl.window,
		                          capture_keyboard ? SDL_TRUE : SDL_FALSE);
	}
}

static void apply_inactive_settings()
{
	set_priority(sdl.priority.inactive);
	MOUSE_NotifyWindowActive(false);

	if (sdl.mute_when_inactive) {
		MIXER_Mute();
	}
}

static void set_priority_levels(const std::string& active_pref,
                                const std::string& inactive_pref)
{
	auto to_level = [](const std::string& pref) {
		if (pref == "auto") {
			return PRIORITY_LEVEL_AUTO;
		}
		if (pref == "lowest") {
			return PRIORITY_LEVEL_LOWEST;
		}
		if (pref == "lower") {
			return PRIORITY_LEVEL_LOWER;
		}
		if (pref == "normal") {
			return PRIORITY_LEVEL_NORMAL;
		}
		if (pref == "higher") {
			return PRIORITY_LEVEL_HIGHER;
		}
		if (pref == "highest") {
			return PRIORITY_LEVEL_HIGHEST;
		}
		LOG_WARNING("SDL: Invalid 'priority' setting: '%s', using 'auto'",
		            pref.c_str());

		return PRIORITY_LEVEL_AUTO;
	};

	sdl.priority.active   = to_level(active_pref);
	sdl.priority.inactive = to_level(inactive_pref);
}

static void restart_hotkey_handler([[maybe_unused]] bool pressed)
{
	DOSBOX_Restart();
}

static void set_fullscreen_mode()
{
	const auto fullscreen_mode_pref = [&] {
		auto legacy_pref = get_sdl_section()->GetString("fullresolution");
		if (!legacy_pref.empty()) {
			set_section_property_value("sdl", "fullresolution", "");
			set_section_property_value("sdl", "fullscreen_mode", legacy_pref);
		}
		return get_sdl_section()->GetString("fullscreen_mode");
	}();

	auto set_screen_bounds = [&] {
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

static void sdl_section_init(Section* sec)
{
	assert(sec);

	const SectionProp* conf = dynamic_cast<SectionProp*>(sec);
	assert(conf);

	if (!conf) {
		return;
	}

	sec->AddDestroyFunction(&shutdown_gui);
	SectionProp* section = static_cast<SectionProp*>(sec);

	sdl.active          = false;
	sdl.updating        = false;
	sdl.resizing_window = false;
	sdl.wait_on_error   = section->GetBool("waitonerror");

	sdl.desktop.is_fullscreen = control->arguments.fullscreen ||
	                            section->GetBool("fullscreen");

	auto priority_conf = section->GetMultiVal("priority")->GetSection();
	set_priority_levels(priority_conf->GetString("active"),
	                    priority_conf->GetString("inactive"));

	sdl.pause_when_inactive = section->GetBool("pause_when_inactive");

	sdl.mute_when_inactive = (!sdl.pause_when_inactive) &&
	                         section->GetBool("mute_when_inactive");

	// Assume focus on startup
	apply_active_settings();

	set_fullscreen_mode();

	const int display = section->GetInt("display");

	if ((display >= 0) && (display < SDL_GetNumVideoDisplays())) {
		sdl.display_number = display;
	} else {
		LOG_WARNING("SDL: Display number out of bounds, using display 0");
		sdl.display_number = 0;
	}

	const std::string presentation_mode_pref = section->GetString(
	        "presentation_mode");

	if (presentation_mode_pref == "cfr") {
		sdl.frame.desired_mode = FrameMode::Cfr;

	} else if (presentation_mode_pref == "vfr") {
		sdl.frame.desired_mode = FrameMode::Vfr;
	}

	initialize_vsync_settings();

	set_output(section, is_aspect_ratio_correction_enabled());

	const std::string screensaver = section->GetString("screensaver");
	if (screensaver == "allow") {
		SDL_EnableScreenSaver();
	}
	if (screensaver == "block") {
		SDL_DisableScreenSaver();
	}

	MAPPER_AddHandler(GFX_RequestExit, SDL_SCANCODE_F9, PRIMARY_MOD, "shutdown", "Shutdown");

	MAPPER_AddHandler(switch_fullscreen_handler, SDL_SCANCODE_RETURN, MMOD2, "fullscr", "Fullscreen");
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

#if C_DEBUG
// Pause binds with activate-debugger

#elif defined(MACOSX)
	// Pause/unpause is hardcoded to Command+P on macOS
	MAPPER_AddHandler(&pause_emulation, SDL_SCANCODE_P, PRIMARY_MOD, "pause", "Pause Emu.");
#else
	// Pause/unpause is hardcoded to Alt+Pause on Window & Linux
	MAPPER_AddHandler(&pause_emulation, SDL_SCANCODE_PAUSE, MMOD2, "pause", "Pause Emu.");
#endif
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

	TITLEBAR_ReadConfig(*conf);
}

static void handle_mouse_motion(SDL_MouseMotionEvent* motion)
{
	MOUSE_EventMoved(static_cast<float>(motion->xrel),
	                 static_cast<float>(motion->yrel),
	                 check_cast<int32_t>(motion->x),
	                 check_cast<int32_t>(motion->y));
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

void GFX_RegenerateWindow(Section* sec)
{
	if (first_window) {
		first_window = false;
		return;
	}
	remove_window();
	set_output(sec, is_aspect_ratio_correction_enabled());
	GFX_ResetScreen();
}

// TODO check if this workaround is still needed
#if defined(MACOSX)
#define DB_POLLSKIP 3
#else
// Not used yet, see comment below
#define DB_POLLSKIP 1
#endif

static void handle_video_resize(int width, int height)
{
	// Maybe a screen rotation has just occurred, so we simply resize.
	// There may be a different cause for a forced resized, though.
	if (sdl.desktop.is_fullscreen) {

		// Note: We should not use get_display_dimensions()
		// (SDL_GetDisplayBounds) on Android after a screen rotation:
		// The older values from application startup are returned.
		sdl.desktop.fullscreen.width  = width;
		sdl.desktop.fullscreen.height = height;
	}

	const auto canvas_size_px = get_canvas_size_in_pixels(sdl.rendering_backend);

	sdl.draw_rect_px = to_sdl_rect(calc_draw_rect_in_pixels(canvas_size_px));

	if (sdl.rendering_backend == RenderingBackend::Texture) {
		SDL_RenderSetViewport(sdl.renderer, &sdl.draw_rect_px);
	}
#if C_OPENGL
	if (sdl.rendering_backend == RenderingBackend::OpenGl) {
		glViewport(sdl.draw_rect_px.x,
		           sdl.draw_rect_px.y,
		           sdl.draw_rect_px.w,
		           sdl.draw_rect_px.h);

		update_uniforms_gl();
	}
#endif // C_OPENGL

	if (!sdl.desktop.is_fullscreen) {
		// If the window was resized, it might have been
		// triggered by the OS setting DPI scale, so recalculate
		// that based on the incoming logical width.
		check_and_handle_dpi_change(sdl.window, sdl.rendering_backend, width);
	}

	// Ensure mouse emulation knows the current parameters
	notify_new_mouse_screen_params();
}

// TODO: Properly set window parameters and remove this routine.
//
// This function is triggered after window is shown to fixup sdl.window
// properties in predictable manner on all platforms.
//
// In specific usecases, certain sdl.window properties might be left unitialized
// when starting in fullscreen, which might trigger severe problems for end
// users (e.g. placing window partially off-screen, or using fullscreen
// resolution for window size).
//
static void finalise_window_state()
{
	assert(sdl.window);

	if (!sdl.desktop.lazy_init_window_size) {
		return;
	}

	// Don't change window position or size when state changed to
	// fullscreen.
	if (sdl.desktop.is_fullscreen) {
		return;
	}

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

static bool maybe_auto_switch_shader()
{
	// The shaders need the OpenGL backend
	if (sdl.rendering_backend != RenderingBackend::OpenGl) {
		return false;
	}

	// The shaders need a canvas size as their target resolution
	const auto canvas_size_px = get_canvas_size_in_pixels(sdl.rendering_backend);
	if (canvas_size_px.IsEmpty()) {
		return false;
	}

	// The shaders need the DOS mode to be set as their source resolution
	if (!sdl.maybe_video_mode) {
		return false;
	}

	constexpr auto reinit_render = true;
	return RENDER_MaybeAutoSwitchShader(canvas_size_px,
	                                    *sdl.maybe_video_mode,
	                                    reinit_render);
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
		GFX_RefreshAnimatedTitle();
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
		SDL_Event ev;

		KEYBOARD_ClrBuffer();

		bool paused = true;

		// Prevent the mixer from running while in our pause loop.
		// Muting is not ideal for some sound devices such as GUS that
		// loop samples. This also saves CPU time by not rendering
		// samples we're not going to play anyway.
		MIXER_LockMixerThread();

		while (paused && !shutdown_requested) {
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
						GFX_RefreshTitle();

						if (we == SDL_WINDOWEVENT_FOCUS_GAINED) {
							paused = false;
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

static bool handle_sdl_windowevent(const SDL_Event& event)
{
	switch (event.window.event) {
	case SDL_WINDOWEVENT_RESTORED:
		// LOG_DEBUG("SDL: Window has been restored");

		// We may need to re-create a texture and more on Android.
		// Another case: Update surface while using X11.
		//
		GFX_ResetScreen();

#if C_OPENGL && defined(MACOSX)
		// TODO check if this workaround is still needed

		// LOG_DEBUG("SDL: Reset macOS's GL viewport after
		// window-restore");
		if (sdl.rendering_backend == RenderingBackend::OpenGl) {
			glViewport(sdl.draw_rect_px.x,
			           sdl.draw_rect_px.y,
			           sdl.draw_rect_px.w,
			           sdl.draw_rect_px.h);
		}
#endif
		focus_input();
		return true;

	case SDL_WINDOWEVENT_RESIZED: {
		// TODO pixels or logical unit?
		// LOG_DEBUG("SDL: Window has been resized to %dx%d",
		// event.window.data1, event.window.data2);

		// When going from an initial fullscreen to windowed state, this
		// event will be called moments before SDL's windowed mode is
		// engaged, so simply ensure the window size has already been
		// established:
		assert(sdl.desktop.window.width > 0 && sdl.desktop.window.height > 0);

		// SDL_WINDOWEVENT_RESIZED events are sent twice when resizing
		// the window, but maybe_log_display_properties() will only output
		// a log entry if the image dimensions have actually changed.
		maybe_log_display_properties();
		return true;
	}

	case SDL_WINDOWEVENT_FOCUS_GAINED:
		apply_active_settings();
		[[fallthrough]];

	case SDL_WINDOWEVENT_EXPOSED:
		// LOG_DEBUG("SDL: Window has been exposed and should be redrawn");

		// TODO: below is not consistently true :( seems incorrect on
		// KDE and sometimes on MATE
		//
		// Note that on Windows/Linux-X11/Wayland/macOS, event is fired
		// after toggling between full vs windowed modes. However this
		// is never fired on the Raspberry Pi (when rendering to the
		// Framebuffer); therefore we rely on the FOCUS_GAINED event to
		// catch window startup and size toggles.

		// LOG_DEBUG("SDL: Window has gained keyboard focus");

		if (sdl.draw.callback) {
			sdl.draw.callback(GFX_CallbackRedraw);
		}
		focus_input();
		return true;

	case SDL_WINDOWEVENT_FOCUS_LOST:
		// LOG_DEBUG("SDL: Window has lost keyboard
		// focus");
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
		// LOG_DEBUG("SDL: Window has gained mouse focus");
		return true;

	case SDL_WINDOWEVENT_LEAVE:
		// LOG_DEBUG("SDL: Window has lost mouse focus");
		return true;

	case SDL_WINDOWEVENT_SHOWN:
		// LOG_DEBUG("SDL: Window has been shown");
		maybe_auto_switch_shader();
		return true;

	case SDL_WINDOWEVENT_HIDDEN:
		// LOG_DEBUG("SDL: Window has been hidden");
		return true;

#if C_OPENGL && defined(MACOSX)
	// TODO check if this workaround is still needed
	case SDL_WINDOWEVENT_MOVED:
		// LOG_DEBUG("SDL: Window has been moved to %d, %d",
		// event.window.data1, event.window.data2);

		if (sdl.rendering_backend == RenderingBackend::OpenGl) {
			glViewport(sdl.draw_rect_px.x,
			           sdl.draw_rect_px.y,
			           sdl.draw_rect_px.w,
			           sdl.draw_rect_px.h);
		}
		return true;
#endif

	case SDL_WINDOWEVENT_DISPLAY_CHANGED: {
		// New display might have a different resolution and DPI scaling
		// set, so recalculate that and set viewport
		check_and_handle_dpi_change(sdl.window, sdl.rendering_backend);

		SDL_Rect display_bounds = {};
		SDL_GetDisplayBounds(event.window.data1, &display_bounds);
		sdl.desktop.fullscreen.width  = display_bounds.w;
		sdl.desktop.fullscreen.height = display_bounds.h;

		sdl.display_number = event.window.data1;

		const auto canvas_size_px = get_canvas_size_in_pixels(
		        sdl.rendering_backend);

		sdl.draw_rect_px = to_sdl_rect(
		        calc_draw_rect_in_pixels(canvas_size_px));

		if (sdl.rendering_backend == RenderingBackend::Texture) {
			SDL_RenderSetViewport(sdl.renderer, &sdl.draw_rect_px);
		}
#if C_OPENGL
		if (sdl.rendering_backend == RenderingBackend::OpenGl) {
			glViewport(sdl.draw_rect_px.x,
			           sdl.draw_rect_px.y,
			           sdl.draw_rect_px.w,
			           sdl.draw_rect_px.h);
		}

		maybe_auto_switch_shader();
#endif
		notify_new_mouse_screen_params();
		return true;
	}

	case SDL_WINDOWEVENT_SIZE_CHANGED: {
		// LOG_DEBUG("SDL: The window size has changed");

		// The window size has changed either as a result of an API call
		// or through the system or user changing the window size.
		const auto new_width  = event.window.data1;
		const auto new_height = event.window.data2;

		handle_video_resize(new_width, new_height);
		finalise_window_state();
		maybe_auto_switch_shader();
		return true;
	}

	case SDL_WINDOWEVENT_MINIMIZED:
		// LOG_DEBUG("SDL: Window has been minimized");
		apply_inactive_settings();
		return false;

	case SDL_WINDOWEVENT_MAXIMIZED:
		// LOG_DEBUG("SDL: Window has been maximized");
		return true;

	case SDL_WINDOWEVENT_CLOSE:
		// LOG_DEBUG("SDL: The window manager requests that the window
		// be closed");
		GFX_RequestExit(true);
		return false;

	case SDL_WINDOWEVENT_TAKE_FOCUS:
		focus_input();
		apply_active_settings();
		return true;

	case SDL_WINDOWEVENT_HIT_TEST:
		// LOG_DEBUG("SDL: Window had a hit test that wasn't
		// SDL_HITTEST_NORMAL");
		return true;

	default: return false;
	}
}

bool GFX_Events()
{
#if defined(MACOSX)
	// TODO check if this workaround is still needed
	//
	// Don't poll too often. This can be heavy on the OS, especially Macs.
	// In idle mode 3000-4000 polls are done per second without this check.
	// Macs, with this code,  max 250 polls per second. (non-macs unused
	// default max 500). Currently not implemented for all platforms, given
	// the ALT-TAB stuff for WIN32.
	static auto last_check = GetTicks();

	auto current_check = GetTicks();

	if (GetTicksDiff(current_check, last_check) <= DB_POLLSKIP) {
		return true;
	}
	last_check = current_check;
#endif

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
#if C_DEBUG
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
	return !shutdown_requested;
}

#if defined(WIN32)
static BOOL WINAPI console_event_handler(DWORD event)
{
	switch (event) {
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT: raise(SIGTERM); return TRUE;
	case CTRL_C_EVENT:
	default:
		// pass to the next handler
		return FALSE;
	}
}
#endif

// Static variable to show wether there is not a valid stdout.
// Fixes some bugs when -noconsole is used in a read only directory
static bool no_stdout = false;

void GFX_ShowMsg(const char* format, ...)
{
	char buf[512];

	va_list msg;
	va_start(msg, format);
	safe_sprintf(buf, format, msg);
	va_end(msg);

	buf[sizeof(buf) - 1] = '\0';
	if (!no_stdout) {
		puts(buf); // Else buf is parsed again. (puts adds end of line)
	}
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

static void add_command_line_help_message()
{
	MSG_Add("DOSBOX_HELP",
	        "Usage: %s [OPTION]... [PATH]\n"
	        "\n"
	        "PATH                       If PATH is a directory, it's mounted as C:.\n"
	        "                           If PATH is a bootable disk image (IMA/IMG), it's booted.\n"
	        "                           If PATH is a CD-ROM image (CUE/ISO), it's mounted as D:.\n"
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
	        "  --list-countries         List all supported countries with their numeric codes.\n"
	        "                           Codes are to be used in the 'country' config setting.\n"
	        "\n"
	        "  --list-layouts           List all supported keyboard layouts with their codes.\n"
	        "                           Codes are to be used in the 'keyboard_layout' config setting.\n"
	        "\n"
	        "  --list-code-pages        List all bundled code pages (screen fonts).\n"
	        "\n"
	        "  --list-glshaders         List all available OpenGL shaders and their paths.\n"
	        "                           Shaders are to be used in the 'glshader' config setting.\n"
	        "\n"
	        "  --fullscreen             Start in fullscreen mode.\n"
	        "\n"
	        "  --lang <lang_file>       Start with the language specified in <lang_file>. If set to\n"
	        "                           'auto', tries to detect the language from the host OS.\n"
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
}

// Callers:
//
//   sdl_main()
//
static void init_sdl_config_section()
{
	constexpr bool changeable_at_runtime = true;

	SectionProp* sdl_sec = control->AddSectionProp("sdl",
	                                               &sdl_section_init,
	                                               changeable_at_runtime);
	sdl_sec->AddInitFunction(&MAPPER_StartUp);

	constexpr auto always     = Property::Changeable::Always;
	constexpr auto deprecated = Property::Changeable::Deprecated;
	constexpr auto deprecated_but_allowed = Property::Changeable::DeprecatedButAllowed;
	constexpr auto on_start = Property::Changeable::OnlyAtStart;

#if C_OPENGL
	const std::string default_output = "opengl";
#else
	const std::string default_output = "texture";
#endif
	auto pstring = sdl_sec->AddString("output", always, default_output.c_str());

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

	pstring = sdl_sec->AddString("texture_renderer", always, "auto");
	pstring->SetHelp(
	        "Render driver to use in 'texture' output mode ('auto' by default).\n"
	        "Use 'texture_renderer = auto' for an automatic choice.");
	pstring->SetValues(get_sdl_texture_renderers());

	auto pint = sdl_sec->AddInt("display", on_start, 0);
	pint->SetHelp(
	        "Number of display to use; values depend on OS and user "
	        "settings (0 by default).");

	auto pbool = sdl_sec->AddBool("fullscreen", always, false);
	pbool->SetHelp("Start in fullscreen mode ('off' by default).");

	pstring = sdl_sec->AddString("fullresolution", deprecated_but_allowed, "");
	pstring->SetHelp(
	        "The [color=light-green]'fullresolution'[reset] setting is deprecated but still accepted;\n"
	        "please use [color=light-green]'fullscreen_mode'[reset] instead.");

	pstring = sdl_sec->AddString("fullscreen_mode", always, "standard");
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
	        "                      and slightly worse frame pacing.");

	pstring->SetOptionHelp("original",
	                       "  original:           Exclusive fullscreen mode at the game's original\n"
		                   "                      resolution, or at the closest available resolution. This\n"
		                   "                      is a niche option for using DOSBox Staging with a CRT\n"
		                   "                      monitor. Toggling fullscreen might result in janky\n"
		                   "                      behaviour in this mode.");
	
	pstring->SetValues({"standard",
#if WIN32
	                     "forced-borderless",
#endif
	                     "original"});

	pstring->SetDeprecatedWithAlternateValue("desktop", "standard");


	pstring = sdl_sec->AddString("windowresolution", deprecated_but_allowed, "");
	pstring->SetHelp(
	        "The [color=light-green]'windowresolution'[reset] setting is deprecated but still accepted;\n"
	        "please use [color=light-green]'window_size'[reset] instead.");

	pstring = sdl_sec->AddString("window_size", on_start, "default");
	pstring->SetHelp(
	        "Set initial window size for windowed mode. You can still resize the window\n"
	        "after startup.\n"
	        "  default:   Select the best option based on your environment and other\n"
	        "             settings (such as whether aspect ratio correction is enabled).\n"
	        "  small, medium, large (s, m, l):\n"
	        "             Size the window relative to the desktop.\n"
	        "  WxH:       Specify window size in WxH format in logical units\n"
	        "             (e.g., 1024x768).");

	pstring = sdl_sec->AddString("window_position", always, "auto");
	pstring->SetHelp(
	        "Set initial window position for windowed mode:\n"
	        "  auto:      Let the window manager decide the position (default).\n"
	        "  X,Y:       Set window position in X,Y format in logical units (e.g., 250,100).\n"
	        "             0,0 is the top-left corner of the screen.");

	pbool = sdl_sec->AddBool("window_decorations", always, true);
	pbool->SetHelp("Enable window decorations in windowed mode ('on' by default).");

	TITLEBAR_AddConfig(*sdl_sec);

	pint = sdl_sec->AddInt("transparency", always, 0);
	pint->SetHelp(
	        "Set the transparency of the DOSBox Staging screen (0 by default).\n"
	        "From 0 (no transparency) to 90 (high transparency).");

	pstring = sdl_sec->AddString("max_resolution", deprecated, "");
	pstring->SetHelp(
	        "Moved to [color=light-cyan][render][reset] section "
	        "and renamed to [color=light-green]'viewport'[reset].");

	pstring = sdl_sec->AddString("viewport_resolution", deprecated, "");
	pstring->SetHelp(
	        "Moved to [color=light-cyan][render][reset] section "
	        "and renamed to [color=light-green]'viewport'[reset].");

	pstring = sdl_sec->AddString("vsync", always, "off");
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
	        "                    lag. It might also impact performance (e.g., introduce audio\n"
	        "                    glitches in some 70 Hz VGA games running on a 60 Hz fixed\n"
	        "                    refresh rate monitor).\n"
	        "  fullscreen-only:  Enable vsync in fullscreen mode only. This might be useful\n"
	        "                    if your operating system enforces vsync in windowed mode and\n"
	        "                    the 'on' setting causes audio glitches or other issues in\n"
	        "                    windowed mode only.\n"
	        "\n"
	        "Notes:\n"
	        "  - For perfectly smooth scrolling in 2D games (e.g., in Pinball Dreams\n"
	        "    and Epic Pinball), you'll need a VRR monitor running in VRR mode and vsync\n"
	        "    disabled. The scrolling in 70 Hz VGA games will always appear juddery on\n"
	        "    60 Hz fixed refresh rate monitors even with vsync enabled.\n"
	        "  - Usually, you'll only get perfectly smooth 2D scrolling in fullscreen mode,\n"
	        "    even on a VRR monitor.\n"
	        "  - For the best results, disable all frame cappers and global vsync overrides\n"
	        "    in your video driver settings.");
	pstring->SetValues({"off", "on", "fullscreen-only"});

	pstring = sdl_sec->AddString("presentation_mode", always, "vfr");
	pstring->SetHelp(
	        "Select the frame presentation mode:\n"
	        "  cfr:   Always present DOS frames at a constant frame rate.\n"
	        "  vfr:   Always present changed DOS frames at a variable frame rate (default).");
	pstring->SetValues({"auto", "cfr"});

	auto pmulti = sdl_sec->AddMultiVal("capture_mouse", deprecated, ",");
	pmulti->SetHelp(
	        "Moved to [color=light-cyan][mouse][reset] section and "
	        "renamed to [color=light-green]'mouse_capture'[reset].");

	pmulti = sdl_sec->AddMultiVal("sensitivity", deprecated, ",");
	pmulti->SetHelp(
	        "Moved to [color=light-cyan][mouse][reset] section and "
	        "renamed to [color=light-green]'mouse_sensitivity'[reset].");

	pbool = sdl_sec->AddBool("raw_mouse_input", deprecated, false);
	pbool->SetHelp(
	        "Moved to [color=light-cyan][mouse][reset] section and "
	        "renamed to [color=light-green]'mouse_raw_input'[reset].");

	pbool = sdl_sec->AddBool("waitonerror", always, true);
	pbool->SetHelp("Keep the console open if an error has occurred ('on' by default).");

	pmulti = sdl_sec->AddMultiVal("priority", always, " ");
	pmulti->SetValue("auto auto");
	pmulti->SetHelp(
	        "Priority levels to apply when active and inactive, respectively.\n"
	        "('auto auto' by default)\n"
	        "'auto' lets the host operating system manage the priority.");

	auto psection = pmulti->GetSection();
	psection->AddString("active", always, "auto")
	        ->SetValues({"auto", "lowest", "lower", "normal", "higher", "highest"});
	psection->AddString("inactive", always, "auto")
	        ->SetValues({"auto", "lowest", "lower", "normal", "higher", "highest"});

	pbool = sdl_sec->AddBool("mute_when_inactive", on_start, false);
	pbool->SetHelp("Mute the sound when the window is inactive ('off' by default).");

	pbool = sdl_sec->AddBool("pause_when_inactive", on_start, false);
	pbool->SetHelp("Pause emulation when the window is inactive ('off' by default).");

	pbool = sdl_sec->AddBool("keyboard_capture", always, false);
	pbool->SetHelp(
	        "Capture system keyboard shortcuts ('off' by default).\n"
	        "When enabled, most system shortcuts such as Alt+Tab are captured and sent to\n"
	        "DOSBox Staging. This is useful for Windows 3.1x and some DOS programs with\n"
	        "unchangeable keyboard shortcuts that conflict with system shortcuts.");

	pstring = sdl_sec->AddPath("mapperfile", always, MAPPERFILE);
	pstring->SetHelp(
	        "Path to the mapper file ('mapper-sdl2-XYZ.map' by default, where XYZ is the\n"
	        "current version). Pre-configured maps are bundled in 'resources/mapperfiles'.\n"
	        "These can be loaded by name, e.g., with 'mapperfile = xbox/xenon2.map'.\n"
	        "Note: The '--resetmapper' command line option only deletes the default mapper\n"
	        "      file.");

	pstring = sdl_sec->AddString("screensaver", on_start, "auto");
	pstring->SetHelp(
	        "Use 'allow' or 'block' to override the SDL_VIDEO_ALLOW_SCREENSAVER environment\n"
	        "variable which usually blocks the OS screensaver while the emulator is\n"
	        "running ('auto' by default).");
	pstring->SetValues({"auto", "allow", "block"});
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
#ifdef WIN32
		_execlp(prog.c_str(), prog.c_str(), path.string().c_str(), (char*)nullptr);
#else
		execlp(prog.c_str(), prog.c_str(), path.string().c_str(), (char*)nullptr);
#endif
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
extern void DEBUG_ShutDown(Section* /*sec*/);
#endif

static void remove_waitpid(std::vector<std::string>& parameters)
{
	auto it = parameters.begin();
	while (it != parameters.end()) {
		if (*it == "-waitpid" || *it == "--waitpid") {
			it = parameters.erase(it);
			if (it != parameters.end()) {
				auto& pid = *it;
				if (!pid.empty() && std::isdigit(pid[0])) {
					// The integer value should be the next
					// element following "--waitpid". If we
					// found an integer, remove it as well.
					it = parameters.erase(it);
				}
			}
		} else {
			++it;
		}
	}
}

void DOSBOX_Restart(std::vector<std::string>& parameters)
{

	control->ApplyQueuedValuesToCli(parameters);

	// Remove any existing --waitpid parameters.
	// This can happen with multiple restarts.
	remove_waitpid(parameters);

#ifdef WIN32
	parameters.emplace_back("--waitpid");
	parameters.emplace_back(std::to_string(GetCurrentProcessId()));
	std::string command_line = {};

	bool first = true;

	for (const auto& arg : parameters) {
		if (!first) {
			command_line.push_back(' ');
		}
		command_line.append(arg);
		first = false;
	}
#else
	parameters.emplace_back("--waitpid");
	parameters.emplace_back(std::to_string(getpid()));

	char** newargs = new char*[parameters.size() + 1];

	// parameter 0 is the executable path
	// contents of the vector follow
	// last one is NULL
	for (size_t i = 0; i < parameters.size(); i++) {
		newargs[i] = parameters[i].data();
	}
	newargs[parameters.size()] = nullptr;
#endif // WIN32

	GFX_RequestExit(true);

#if C_DEBUG
	// shutdown curses
	DEBUG_ShutDown(nullptr);
#endif // C_DEBUG

#ifdef WIN32
	// nullptr to parse from command line
	const LPCSTR application_name = nullptr;

	// nullptr for default
	const LPSECURITY_ATTRIBUTES process_attributes = nullptr;

	// nullptr for default
	const LPSECURITY_ATTRIBUTES thread_attributes = nullptr;

	const BOOL inherit_handles = FALSE;

	// CREATE_NEW_CONSOLE fixes a bug where the parent process exiting kills
	// the child process. This can manifest when you use the "restart"
	// hotkey action to restart DOSBox.
	// https://github.com/dosbox-staging/dosbox-staging/issues/3346
	const DWORD creation_flags = CREATE_NEW_CONSOLE;

	// nullptr to use parent's environment
	const LPVOID environment_variables = nullptr;

	// nullptr to use parent's current directory
	const LPCSTR current_directory = nullptr;

	// Input structure with a bunch of stuff we probably don't care about.
	// Zero it out to use defaults save for the size field.
	STARTUPINFO startup_info = {};
	startup_info.cb          = sizeof(startup_info);

	// Output structure we also don't care about.
	PROCESS_INFORMATION process_information = {};

	if (!CreateProcess(application_name,
	                   const_cast<char*>(command_line.c_str()),
	                   process_attributes,
	                   thread_attributes,
	                   inherit_handles,
	                   creation_flags,
	                   environment_variables,
	                   current_directory,
	                   &startup_info,
	                   &process_information)) {
		LOG_ERR("Restart failed: CreateProcess failed");
	}
#else
	int ret = fork();
	switch (ret) {
	case -1:
		LOG_ERR("Restart failed: fork failed: %s", strerror(errno));
		break;
	case 0:
		// Newly created child process immediately executes a new
		// instance of DOSBox. It is not safe for a child process in a
		// multi-threaded program to do much else.
		if (execvp(newargs[0], newargs) == -1) {
			E_Exit("Restart failed: execvp failed: %s", strerror(errno));
		}
		break;
	}
	// Original (parent) process continues execution here.
	// It will proceed to do a normal shutdown due to GFX_RequestExit(true);
	// above.
	delete[] newargs;
#endif // WIN32
}

static void list_glshaders()
{
#if C_OPENGL
	for (const auto& line : RENDER_GenerateShaderInventoryMessage()) {
		printf("%s\n", line.c_str());
	}
#else
	fprintf(stderr,
	        "OpenGL is not supported by this executable "
	        "and is missing the functionality to list shaders.");
#endif
}

static void list_countries()
{
	const auto message_utf8 = DOS_GenerateListCountriesMessage();
	printf("%s\n", message_utf8.c_str());
}

static void list_keyboard_layouts()
{
	const auto message_utf8 = DOS_GenerateListKeyboardLayoutsMessage();
	printf("%s\n", message_utf8.c_str());
}

static void list_code_pages()
{
	const auto message_utf8 = DOS_GenerateListCodePagesMessage();
	printf("%s\n", message_utf8.c_str());
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

	if (!delete_file(path)) {
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

	if (!delete_file(path)) {
		fprintf(stderr,
		        "Cannot delete mapper file '%s'",
		        path.string().c_str());
		return 1;
	}

	printf("Mapper file '%s' deleted.\n", path.string().c_str());
	return 0;
}

static void set_wm_class()
{
#if (SDL_VERSION_ATLEAST(3, 0, 0))
	SDL_SetHint(SDL_HINT_APP_ID, DOSBOX_APP_ID);
#else
#if !defined(WIN32) && !defined(MACOSX)
	constexpr int overwrite = 0;

	setenv("SDL_VIDEO_X11_WMCLASS", DOSBOX_APP_ID, overwrite);
	setenv("SDL_VIDEO_WAYLAND_WMCLASS", DOSBOX_APP_ID, overwrite);
#endif
#endif
}

static void init_logger(const CommandLineArguments& args, int argc, char* argv[])
{
	loguru::g_preamble_date    = true;
	loguru::g_preamble_time    = true;
	loguru::g_preamble_uptime  = false;
	loguru::g_preamble_thread  = false;
	loguru::g_preamble_file    = false;
	loguru::g_preamble_verbose = false;
	loguru::g_preamble_pipe    = true;

	if (args.version || args.help || args.printconf || args.editconf ||
	    args.eraseconf || args.list_countries || args.list_layouts ||
	    args.list_code_pages || args.list_glshaders || args.erasemapper) {

		loguru::g_stderr_verbosity = loguru::Verbosity_WARNING;
	}

	loguru::init(argc, argv);
}

static void maybe_write_primary_config(const CommandLineArguments& args)
{
	// Before loading any configs, write the default primary config if it
	// doesn't exist when:
	//
	// - secure mode is NOT enabled with the '--securemode' option,
	//
	// - AND we're not in portable mode (portable mode is enabled when
	//   'dosbox-staging.conf' exists in the executable directory)
	//
	// - AND the primary config is NOT disabled with the
	//   '--noprimaryconf' option.
	//
	if (!args.securemode && !args.noprimaryconf) {
		const auto primary_config_path = GetPrimaryConfigPath();

		if (!config_file_is_valid(primary_config_path)) {
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
}

static std::optional<int> maybe_handle_command_line_output_only_actions(
        const CommandLineArguments& args, const char* program_name)
{
	if (args.version) {
		printf(version_msg, DOSBOX_PROJECT_NAME, DOSBOX_GetDetailedVersion());
		return 0;
	}
	if (args.help) {
		const auto help = format_str(MSG_GetTranslatedRaw("DOSBOX_HELP"),
		                             program_name);
		printf("%s", help.c_str());
		return 0;
	}
	if (args.editconf) {
		return edit_primary_config();
	}
	if (args.eraseconf) {
		return erase_primary_config_file();
	}
	if (args.erasemapper) {
		return erase_mapper_file();
	}
	if (args.printconf) {
		return print_primary_config_location();
	}
	if (args.list_countries) {
		list_countries();
		return 0;
	}
	if (args.list_layouts) {
		list_keyboard_layouts();
		return 0;
	}
	if (args.list_code_pages) {
		list_code_pages();
		return 0;
	}
	if (args.list_glshaders) {
		list_glshaders();
		return 0;
	}
	return {};
}

static bool check_kmsdrm_setting()
{
	// Simple pre-check to see if we're using kmsdrm
	if (!is_using_kmsdrm_driver()) {
		return true;
	}

	// Do we have read access to the event subsystem
	if (auto f = fopen("/dev/input/event0", "r"); f) {
		fclose(f);
		return true;
	}

	// We're using KMSDRM, but we don't have read access to the event subsystem
	return false;
}

static void init_sdl()
{
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

	if (!check_kmsdrm_setting()) {
		E_Exit("SDL: /dev/input/event0 is not readable, quitting early to prevent TTY input lockup.\n"
		       "Please run: 'sudo usermod -aG input $(whoami)', then re-login and try again.");
	}

	// Timer is needed for title bar animations
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		E_Exit("SDL: Can't init SDL %s", SDL_GetError());
	}
	sdl.start_event_id = SDL_RegisterEvents(enum_val(SDL_DosBoxEvents::NumEvents));
	if (sdl.start_event_id == UINT32_MAX) {
		E_Exit("SDL: Failed to alocate event IDs");
	}

	sdl.initialized = true;

	SDL_version sdl_version = {};
	SDL_GetVersion(&sdl_version);

	LOG_MSG("SDL: Version %d.%d.%d initialised (%s video and %s audio)",
	        sdl_version.major,
	        sdl_version.minor,
	        sdl_version.patch,
	        SDL_GetCurrentVideoDriver(),
	        SDL_GetCurrentAudioDriver());

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

static void handle_cli_set_commands(const std::vector<std::string>& set_args)
{
	for (auto command : set_args) {
		trim(command);

		if (command.empty() || command[0] == '%' || command[0] == '\0' ||
		    command[0] == '#' || command[0] == '\n') {
			continue;
		}

		std::vector<std::string> pvars(1, std::move(command));

		const char* result = control->SetProp(pvars);

		if (strlen(result)) {
			LOG_WARNING("CONFIG: %s", result);
		} else {
			Section* tsec = control->GetSection(pvars[0]);
			std::string value(pvars[2]);

			// Due to parsing, there can be a '=' at the
			// start of the value.
			while (value.size() &&
			       (value.at(0) == ' ' || value.at(0) == '=')) {
				value.erase(0, 1);
			}

			for (size_t i = 3; i < pvars.size(); i++) {
				value += (std::string(" ") + pvars[i]);
			}

			std::string inputline = pvars[1] + "=" + value;

			bool change_success = tsec->HandleInputline(inputline);

			if (!change_success && !value.empty()) {
				LOG_WARNING("CONFIG: Cannot set '%s'",
				            inputline.c_str());
			}
		}
	}
}

#if defined(WIN32) && !(C_DEBUG)
static void apply_windows_debugger_workaround(const bool is_console_disabled)
{
	// Can't disable the console with debugger enabled
	if (is_console_disabled) {
		FreeConsole();
		// Redirect standard input and standard output
		//
		if (freopen(STDOUT_FILE, "w", stdout) == NULL) {
			// No stdout so don't write messages
			no_stdout = true;
		}
		freopen(STDERR_FILE, "w", stderr);

		// Line buffered
		setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
		// No buffering
		setvbuf(stderr, NULL, _IONBF, BUFSIZ);

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
}
#endif

static void maybe_create_resource_directories()
{
	const auto plugins_dir = GetConfigDir() / PluginsDir;

	if (create_dir(plugins_dir, 0700, OK_IF_EXISTS) != 0) {
		LOG_WARNING("CONFIG: Can't create directory '%s': %s",
		            plugins_dir.string().c_str(),
		            safe_strerror(errno).c_str());
	}

#if C_OPENGL
	const auto glshaders_dir = GetConfigDir() / GlShadersDir;

	if (create_dir(glshaders_dir, 0700, OK_IF_EXISTS) != 0) {
		LOG_WARNING("CONFIG: Can't create directory '%s': %s",
		            glshaders_dir.string().c_str(),
		            safe_strerror(errno).c_str());
	}
#endif

	const auto soundfonts_dir = GetConfigDir() / DefaultSoundfontsDir;

	if (create_dir(soundfonts_dir, 0700, OK_IF_EXISTS) != 0) {
		LOG_WARNING("CONFIG: Can't create directory '%s': %s",
		            soundfonts_dir.string().c_str(),
		            safe_strerror(errno).c_str());
	}

#if C_MT32EMU
	const auto mt32_rom_dir = GetConfigDir() / DefaultMt32RomsDir;

	if (create_dir(mt32_rom_dir, 0700, OK_IF_EXISTS) != 0) {
		LOG_WARNING("CONFIG: Can't create directory '%s': %s",
		            mt32_rom_dir.string().c_str(),
		            safe_strerror(errno).c_str());
	}
#endif
}

int sdl_main(int argc, char* argv[])
{
	// Ensure we perform SDL cleanup and restore console settings
	atexit(QuitSDL);

	CommandLine command_line(argc, argv);
	control = std::make_unique<Config>(&command_line);

	const auto arguments = &control->arguments;

	if (arguments->wait_pid) {
#ifdef WIN32
		// Synchronize permission is all we need for WaitForSingleObject()
		constexpr DWORD DesiredAccess = SYNCHRONIZE;
		constexpr BOOL InheritHandles = FALSE;
		HANDLE process                = OpenProcess(DesiredAccess,
                                             InheritHandles,
                                             *arguments->wait_pid);
		if (process) {
			// Waits for the process to terminate.
			// If we failed to open it, it's probably already dead.
			constexpr DWORD Timeout = INFINITE;
			WaitForSingleObject(process, Timeout);
			CloseHandle(process);
		}
#else
		for (;;) {
			// Signal of 0 does not actually send a signal.
			// It only checks for existance and permissions of the PID.
			constexpr int Signal = 0;
			int ret = kill(*arguments->wait_pid, Signal);
			// ESRCH means PID does not exist.
			if (ret == -1 && errno == ESRCH) {
				break;
			}
			Delay(50);
		}
#endif
	}

	switch_console_to_utf8();

	// Set up logging after command line was parsed and trivial arguments
	// have been handled
	init_logger(*arguments, argc, argv);

	LOG_MSG("%s version %s", DOSBOX_PROJECT_NAME, DOSBOX_GetDetailedVersion());
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

		// Before SDL2/SDL3 video subsystem is initialised
		set_wm_class();

		// Create or determine the location of the config directory
		// (e.g., in portable mode, the config directory is the
		// executable dir).
		//
		// TODO Consider forcing portable mode in secure mode (this
		// could be accomplished by passing a flag to InitConfigDir);.
		//
		InitConfigDir();

		// Register essential DOS messages needed by some command line
		// switches and during startup or reboot
		add_command_line_help_message();

		DOS_Locale_AddMessages();
		RENDER_AddMessages();
		TITLEBAR_AddMessages();

		// Add [sdl] config section
		init_sdl_config_section();

		// Register the config sections and messages of all the other
		// modules
		DOSBOX_InitAllModuleConfigsAndMessages();

		maybe_write_primary_config(*arguments);

		// After DOSBOX_InitAllModuleConfigsAndMessages() is done, all
		// the config sections have been registered, so we're ready to
		// parse the config files.
		//
		control->ParseConfigFiles(GetConfigDir());

		// Handle command line options that don't start the emulator but
		// only perform some actions and print the results to the console.
		assert(argv && argv[0]);
		const auto program_name = argv[0];

		if (const auto return_code = maybe_handle_command_line_output_only_actions(
		            *arguments, program_name);
		    return_code) {
			return *return_code;
		}

#if defined(WIN32) && !(C_DEBUG)
		apply_windows_debugger_workaround(arguments->noconsole);
#endif

#if defined(WIN32)
		SetConsoleCtrlHandler((PHANDLER_ROUTINE)console_event_handler, TRUE);
#endif

		init_sdl();

		// Handle configuration settings passed with `--set` commands
		// from the CLI.
		handle_cli_set_commands(arguments->set);

		maybe_create_resource_directories();

		control->ParseEnv();

		// Execute all registered section init functions
		control->Init();

		// All subsystems' hotkeys need to be registered at this point
		// to ensure their hotkeys appear in the graphical mapper.
		MAPPER_BindKeys(get_sdl_section());
		GFX_RegenerateWindow(get_sdl_section());

		if (arguments->startmapper) {
			MAPPER_DisplayUI();
		}

		// Start emulation and run it until shutdown
		control->StartUp();

		// Shutdown and release
		control.reset();

	} catch (char* error) {
		return_code = 1;

		// TODO Show warning popup dialog with the error (use the tiny
		// osdialog lib) with console log fallback
		GFX_ShowMsg("Exit to error: %s", error);

		fflush(nullptr);

		if (sdl.wait_on_error) {
			// TODO Maybe look for some way to show message in linux?
#if (C_DEBUG)
			GFX_ShowMsg("Press enter to continue");

			fflush(nullptr);
			fgetc(stdin);

#elif defined(WIN32)
			// TODO not needed once we should the popup dialog
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
	// TODO is this still needed on current SDL?
	sticky_keys(true);
#endif

	// We already do this at exit, but do cleanup earlier in case of normal
	// exit; this works around problems when atexit order clashes with SDL2
	// cleanup order. Happens with SDL_VIDEODRIVER=wayland as of SDL 2.0.12.
	QuitSDL();

	return return_code;
}

