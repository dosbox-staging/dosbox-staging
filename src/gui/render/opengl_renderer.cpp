// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "opengl_renderer.h"

#if C_OPENGL

#include "gui/private/common.h"
#include "gui/private/shader_manager.h"

#include "capture/capture.h"
#include "dosbox_config.h"
#include "misc/support.h"
#include "misc/video.h"
#include "utils/checks.h"
#include "utils/math_utils.h"

// Glad must be included before SDL
#include "glad/gl.h"

// must be included after dosbox_config.h
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_syswm.h>

CHECK_NARROWING();

// #define DEBUG_OPENGL
// #define USE_DEBUG_CONTEXT

constexpr auto GlslExtension = ".glsl";

constexpr auto ImageAdjustmentsShaderName = "_internal/image-adjustments-pass";

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

OpenGlRenderer::OpenGlRenderer(const int x, const int y, const int width,
                               const int height, uint32_t sdl_window_flags)
{
	window = CreateSdlWindow(x, y, width, height, sdl_window_flags);
	if (!window) {
		const auto msg = format_str("OPENGL: Error creating window: %s",
		                            SDL_GetError());
		LOG_ERR("%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	if (!InitRenderer()) {
		SDL_DestroyWindow(window);

		const auto msg = format_str("OPENGL: Error creating renderer");
		LOG_ERR("%s", msg.c_str());
		throw std::runtime_error(msg);
	}
}

SDL_Window* OpenGlRenderer::CreateSdlWindow(const int x, const int y,
                                            const int width, const int height,
                                            const uint32_t sdl_window_flags)
{
	// Request 24-bits framebuffer, don't care about depth buffer
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

#ifdef USE_DEBUG_CONTEXT
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

	// Request an OpenGL 4.3 core profile context (debug output became a
	// core part of OpenGL since version 4.3). Note the macOS doesn't seem
	// to support debug contexts at all.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#else
	// Request an OpenGL 3.3 core profile context
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	// Request an OpenGL-ready window
	auto flags = sdl_window_flags;
	flags |= SDL_WINDOW_OPENGL;

#ifdef MACOSX
	SDL_SetHint(SDL_HINT_MAC_COLOR_SPACE, "displayp3");
#endif

	return SDL_CreateWindow(DOSBOX_NAME, x, y, width, height, flags);
}

#ifdef USE_DEBUG_CONTEXT
void glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity,
                   GLsizei length, const char* message, const void* userParam)
{
	LOG_DEBUG("OPENGL: %s", message);
}
#endif

bool OpenGlRenderer::InitRenderer()
{
	context = SDL_GL_CreateContext(window);
	if (!context) {
		LOG_ERR("SDL: Error creating OpenGL context: %s", SDL_GetError());
		return false;
	}

	const auto version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);

#ifdef USE_DEBUG_CONTEXT
	GLint flags;
	glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
	if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(glDebugOutput, nullptr);
		glDebugMessageControl(
		        GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
	} else {
		LOG_ERR("OPENGL: Could not initialise debug context");
	}
#endif

	GLint size = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);

	max_texture_size_px = size;

	LOG_INFO("OPENGL: Version: %d.%d, GLSL version: %s, vendor: %s",
	         GLAD_VERSION_MAJOR(version),
	         GLAD_VERSION_MINOR(version),
	         safe_gl_get_string(GL_SHADING_LANGUAGE_VERSION, "unknown"),
	         safe_gl_get_string(GL_VENDOR, "unknown"));

	// Vertex data of a single oversized triangle encompassing the viewport
	// Lower left
	vertex_data[0] = -1.0f;
	vertex_data[1] = -1.0f;

	// Lower right
	vertex_data[2] = 3.0f;
	vertex_data[3] = -1.0f;

	// Upper left
	vertex_data[4] = -1.0f;
	vertex_data[5] = 3.0f;

	// Create VBO & VAO
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);

	glVertexAttribPointer(0,        // attribute index,
	                      2,        // num components (vec2),
	                      GL_FLOAT, // component data type,
	                      GL_FALSE, // do not normalise fixed point,
	                      0,        // data stride (0=packed),
	                      static_cast<GLvoid*>(nullptr));

	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	// We don't care about depth testing; we'll just apply the shaders in 2D
	// space
	glDisable(GL_DEPTH_TEST);

	// Create off-screen framebuffer
	glGenFramebuffers(1, &pass1.out_fbo);

	const auto maybe_shader = LoadAndBuildShader(ImageAdjustmentsShaderName);
	if (!maybe_shader) {
		E_Exit("OPENGL: Cannot load '%s' shader, exiting",
		       ImageAdjustmentsShaderName);
	}
	pass1.shader = *maybe_shader;

	glUseProgram(pass1.shader.program_object);
	GetPass1UniformLocations();

	return true;
}

OpenGlRenderer::~OpenGlRenderer()
{
	SDL_GL_ResetAttributes();

	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);

	if (pass1.in_texture) {
		glDeleteTextures(1, &pass1.in_texture);
		pass1.in_texture = 0;
	}

	if (pass1.out_texture) {
		glDeleteTextures(1, &pass1.out_texture);
		pass1.out_texture = 0;
	}
	glDeleteFramebuffers(1, &pass1.out_fbo);

	for (auto& [_, shader] : shader_cache) {
		glDeleteProgram(shader.program_object);
	}
	shader_cache.clear();

	if (context) {
		SDL_GL_DeleteContext(context);
		context = {};
	}
	if (window) {
		SDL_DestroyWindow(window);
		window = {};
	}
}

SDL_Window* OpenGlRenderer::GetWindow()
{
	return window;
}

DosBox::Rect OpenGlRenderer::GetCanvasSizeInPixels()
{
	SDL_Rect canvas_size_px = {};
#if SDL_VERSION_ATLEAST(2, 26, 0)
	SDL_GetWindowSizeInPixels(window, &canvas_size_px.w, &canvas_size_px.h);
#else
	SDL_GL_GetDrawableSize(window, &canvas_size_px.w, &canvas_size_px.h);
#endif

	const auto r = to_rect(canvas_size_px);
	assert(r.HasPositiveSize());

	return r;
}

void OpenGlRenderer::NotifyViewportSizeChanged(const DosBox::Rect draw_rect_px)
{
	viewport_rect_px = draw_rect_px;

	// If the viewport size has changed, the canvas size might have changed
	// too.
	const auto canvas_size_px = GetCanvasSizeInPixels();

	// We always expect a valid canvas
	assert(!canvas_size_px.IsEmpty());

	// The video mode hasn't changed, but the ShaderManager call expects it.
	const auto video_mode = VGA_GetCurrentVideoMode();

	if (video_mode.width <= 0 && video_mode.height <= 0) {
		// On Windows at least, this method gets called before the video
		// mode is initialised if fullscreen is enabled in the config.
		// The problem is that the exact invocation order of the
		// graphics init code is still somewhat platform dependent as
		// certain actions are triggered in response to windowing events
		// which are not 100% standardised across platforms in SDL2.
		// SDL3 promises more cross-platform consistency in this regard,
		// so let's hope that will get us closer to the "test once, run
		// anywhere" paradigm.
		//
		// It's rather tricky to solve this in a more elegant way, so
		// this solution will do for now. This method will get called a
		// second time after the video mode has been initialised which
		// won't be a no-op.
		return;
	}

	const auto new_descriptor = ShaderManager::GetInstance().NotifyRenderParametersChanged(
	        curr_shader_descriptor, canvas_size_px, video_mode);

	HandleShaderAndPresetChangeViaNotify(new_descriptor);
}

void OpenGlRenderer::HandleShaderAndPresetChangeViaNotify(const ShaderDescriptor& new_descriptor)
{
	using enum OpenGlRenderer::SetShaderResult;

	switch (MaybeSetShaderAndPreset(curr_shader_descriptor, new_descriptor)) {
	case Ok: curr_shader_descriptor = new_descriptor; break;
	case PresetError:
		LOG_ERR("RENDER: Error loading shader preset '%s'; using default preset",
		        new_descriptor.ToString().c_str());

		curr_shader_descriptor = {new_descriptor.shader_name, ""};
		break;

	case ShaderError:
		LOG_ERR("RENDER: Error setting shader '%s'",
		        new_descriptor.ToString().c_str());
		break;

	default: assertm(false, "Invalid SetShaderResult value");
	}
}

void OpenGlRenderer::NotifyRenderSizeChanged(const int new_render_width_px,
                                             const int new_render_height_px)
{
	MaybeUpdateRenderSize(new_render_width_px, new_render_height_px);
}

void OpenGlRenderer::MaybeUpdateRenderSize(const int new_render_width_px,
                                           const int new_render_height_px)
{
	// At startup we set the shader before receiving the first
	// `UpdateRenderSize()` call.  This will result in
	// `MaybeSetShaderAndPreset()` calling `UpdateRenderSize()` with zero
	// render dimensions.
	//
	// It's hard to solve this in a more "elegant" way, so the simplest
	// solution is just to bail out in the zero dimension case and let the
	// subsequent `UpdateRenderSize()` call set up the correct render
	// dimensions.
	//
	if (new_render_width_px == 0 && new_render_height_px == 0) {
		// no-op
		return;
	}

	// Size hasn't changed, don't recreate the texture
	if (new_render_width_px == pass1.width &&
	    new_render_height_px == pass1.height) {
		// no-op
		return;
	}

	if (new_render_width_px > max_texture_size_px ||
	    new_render_height_px > max_texture_size_px) {

		LOG_ERR("OPENGL: No support for texture size of %dx%d pixels",
		        new_render_width_px,
		        new_render_height_px);
		return;
	}

	pass1.width  = new_render_width_px;
	pass1.height = new_render_height_px;

	RecreatePass1InputTextureAndRenderBuffer();
	RecreatePass1OutputTexture();

	// Set up off-screen framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, pass1.out_fbo);

	glFramebufferTexture2D(GL_FRAMEBUFFER,
	                       GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D,
	                       pass1.out_texture,
	                       0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LOG_ERR("OPENGL: Framebuffer is not complete");
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGlRenderer::RecreatePass1InputTextureAndRenderBuffer()
{
	if (pass1.in_texture) {
		glDeleteTextures(1, &pass1.in_texture);
	}

	glGenTextures(1, &pass1.in_texture);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, pass1.in_texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Just create the texture; we'll copy the image data later with
	// `glTexSubImage2D()`
	//
	glTexImage2D(GL_TEXTURE_2D,
	             0,                // mimap level (0 = base image)
	             GL_RGB8,          // internal format
	             pass1.width,      // width
	             pass1.height,     // height
	             0,                // border (must be always 0)
	             GL_BGRA,          // pixel data format
	             GL_UNSIGNED_BYTE, // pixel data type
	             nullptr           // pointer to image data
	);

	glBindTexture(GL_TEXTURE_2D, 0);

	// Allocate host memory buffers for the texture data. The video card
	// emulation will write to these buffers, then we'll copy the data to
	// the texture in GPU memory with `glTexSubImage2D()` before presenting
	// the frame.
	const auto pitch_pixels = pass1.width;
	const auto num_pixels = static_cast<size_t>(pitch_pixels) * pass1.height;

	curr_framebuf.resize(num_pixels);
	last_framebuf.resize(num_pixels);

	constexpr auto BytesPerPixel = sizeof(uint32_t);
	const auto pitch_bytes       = pitch_pixels * BytesPerPixel;

	pass1.in_texture_pitch = check_cast<int>(pitch_bytes);
}

void OpenGlRenderer::RecreatePass1OutputTexture()
{
	if (pass1.out_texture) {
		glDeleteTextures(1, &pass1.out_texture);
	}
	glGenTextures(1, &pass1.out_texture);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, pass1.out_texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	SetPass1OutputTextureFiltering();

	glTexImage2D(GL_TEXTURE_2D,
	             0,            // mimap level (0 = base image)
	             GL_RGB32F,    // internal format
	             pass1.width,  // width
	             pass1.height, // height
	             0,            // border (must be always 0)
	             GL_BGRA,      // pixel data format
	             GL_FLOAT,     // pixel data type
	             nullptr);     // pointer to image data

	glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGlRenderer::SetPass1OutputTextureFiltering()
{
	glBindTexture(GL_TEXTURE_2D, pass1.out_texture);

	const auto& shader_settings = pass2.shader_preset.settings;

	const int filter_param = [&] {
		switch (shader_settings.texture_filter_mode) {
		case TextureFilterMode::NearestNeighbour: return GL_NEAREST;
		case TextureFilterMode::Bilinear: return GL_LINEAR;
		default: assertm(false, "Invalid TextureFilterMode"); return 0;
		}
	}();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_param);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_param);
}

void OpenGlRenderer::StartFrame(uint32_t*& pixels_out, int& pitch_out)
{
	assert(!curr_framebuf.empty());

	pixels_out = curr_framebuf.data();
	if (pixels_out == nullptr) {
		return;
	}
	pitch_out = pass1.in_texture_pitch;
}

void OpenGlRenderer::EndFrame()
{
	assert(!curr_framebuf.empty());
	assert(!last_framebuf.empty());

	// We need to copy the buffers. We can't just swap them because the VGA
	// emulation only writes the changed pixels to the framebuffer in each
	// frame.

	last_framebuf       = curr_framebuf;
	last_framebuf_dirty = true;
}

void OpenGlRenderer::PrepareFrame()
{
	assert(!last_framebuf.empty());

	if (last_framebuf_dirty) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, pass1.in_texture);

		glTexSubImage2D(GL_TEXTURE_2D,
		                0,            // mimap level (0 = base image)
		                0,            // x offset
		                0,            // y offset
		                pass1.width,  // width
		                pass1.height, // height
		                GL_BGRA,      // pixel data format
		                GL_UNSIGNED_INT_8_8_8_8_REV, // pixel data type
		                last_framebuf.data() // pointer to image data
		);

		glBindTexture(GL_TEXTURE_2D, 0);

		++frame_count;

		last_framebuf_dirty = false;
	}
}

void OpenGlRenderer::RenderPass1()
{
	// Pass 1
	// ------
	// Apply image adjustment shader and render into an off-screen buffer

	glBindFramebuffer(GL_FRAMEBUFFER, pass1.out_fbo);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(pass1.shader.program_object);

	// Bind input texture containing the raw framebuffer data of the
	// emulated video card
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, pass1.in_texture);

	// Set up viewport
	glViewport(0, 0, pass1.width, pass1.height);

	// Apply shader by drawing an oversized triangle
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// Reset binds
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
}

void OpenGlRenderer::RenderPass2()
{
	// Pass 2
	// ------
	// Apply user-configured shader and render the output into default
	// framebuffer visible on the screen.

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(pass2.shader.program_object);
	UpdatePass2Uniforms();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, pass1.out_texture);

	// Set up viewport
	glViewport(static_cast<GLsizei>(viewport_rect_px.x),
	           static_cast<GLsizei>(viewport_rect_px.y),
	           static_cast<GLsizei>(viewport_rect_px.w),
	           static_cast<GLsizei>(viewport_rect_px.h));

	// Apply shader by drawing an oversized triangle
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// Reset binds
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
}

void OpenGlRenderer::PresentFrame()
{
	RenderPass1();
	RenderPass2();

	// Optionally capture frame
	if (CAPTURE_IsCapturingPostRenderImage()) {
		// glReadPixels() implicitly blocks until all pipelined
		// rendering commands have finished, so we're guaranteed to
		// read the contents of the up-to-date backbuffer here right
		// before the buffer swap.
		//
		GFX_CaptureRenderedImage();
	}

	// Present frame
	SDL_GL_SwapWindow(window);
}

std::optional<GLuint> OpenGlRenderer::BuildShader(const GLenum type,
                                                  const std::string& source) const
{
	GLuint shader            = 0;
	GLint is_shader_compiled = 0;

	assert(!source.empty());

	const char* shaderSrc      = source.c_str();
	const char* src_strings[2] = {nullptr, nullptr};
	std::string top;

	// Look for "#version" because it has to occur first
	if (const char* ver = strstr(shaderSrc, "#version "); ver) {

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
		return {};
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

	if (!is_shader_compiled) {
		glDeleteShader(shader);
		return {};
	}

	return shader;
}

// Build a OpenGL shader program.
//
// Input GLSL source must contain both vertex and fragment stages inside their
// respective preprocessor definitions.
//
// Returns a ready to use OpenGL shader program on success.
//
std::optional<GLuint> OpenGlRenderer::BuildShaderProgram(const std::string& shader_source)
{
	if (shader_source.empty()) {
		LOG_ERR("OPENGL: No shader source present");
		return {};
	}

	const auto maybe_vertex_shader = BuildShader(GL_VERTEX_SHADER, shader_source);
	if (!maybe_vertex_shader) {
		LOG_ERR("OPENGL: Error compiling vertex shader");
		return {};
	}
	const auto vertex_shader = *maybe_vertex_shader;

	const auto maybe_fragment_shader = BuildShader(GL_FRAGMENT_SHADER,
	                                               shader_source);
	if (!maybe_fragment_shader) {
		LOG_ERR("OPENGL: Error compiling fragment shader");
		glDeleteShader(vertex_shader);
		return {};
	}
	const auto fragment_shader = *maybe_fragment_shader;

	const GLuint shader_program = glCreateProgram();

	if (!shader_program) {
		LOG_ERR("OPENGL: Error creating shader program");
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);
		return {};
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
			LOG_ERR("OPENGL: Error linking shader program:\n %s",
			        info_log.data());
		}
	}

	if (!is_program_linked) {
		glDeleteProgram(shader_program);
		return {};
	}

	return shader_program;
}

OpenGlRenderer::SetShaderResult OpenGlRenderer::SetShader(const std::string& symbolic_shader_descriptor)
{
	return SetShaderInternal(symbolic_shader_descriptor);
}

OpenGlRenderer::SetShaderResult OpenGlRenderer::SetShaderInternal(
        const std::string& new_symbolic_shader_descriptor, const bool force_reload)
{
	using enum OpenGlRenderer::SetShaderResult;

	const auto new_descriptor = ShaderManager::GetInstance().NotifyShaderChanged(
	        new_symbolic_shader_descriptor, GlslExtension);

	const auto curr_descriptor = force_reload ? ShaderDescriptor{"", ""}
	                                          : curr_shader_descriptor;

	const auto result = MaybeSetShaderAndPreset(curr_descriptor, new_descriptor);

	switch (result) {
	case Ok:
		curr_shader_descriptor = new_descriptor;
		curr_symbolic_shader_descriptor = new_symbolic_shader_descriptor;

		// The texture filtering mode might have changed
		SetPass1OutputTextureFiltering();
		break;

	case PresetError: {
		curr_shader_descriptor = {new_descriptor.shader_name, ""};

		auto descriptor = ShaderDescriptor::FromString(new_symbolic_shader_descriptor,
		                                               GlslExtension);
		descriptor.preset_name.clear();

		curr_symbolic_shader_descriptor = descriptor.ToString();

		// The texture filtering mode might have changed
		SetPass1OutputTextureFiltering();
		break;
	}
	case ShaderError: break;

	default:
		assertm(false, "Invalid SetShaderResult value");
		return ShaderError;
	}

	return result;
}

void OpenGlRenderer::ForceReloadCurrentShader()
{
	// Save shader & shader preset for later
	const auto shader_key = curr_shader_descriptor.shader_name;
	assert(shader_cache.contains(shader_key));
	const auto saved_shader = shader_cache[shader_key];

	const auto shader_preset_key     = curr_shader_descriptor.ToString();
	ShaderPreset saved_shader_preset = {};

	if (!curr_shader_descriptor.preset_name.empty()) {
		assert(shader_preset_cache.contains(shader_preset_key));
		saved_shader_preset = shader_preset_cache[shader_preset_key];
	}

	// Delete them from the caches
	shader_cache.erase(shader_key);
	shader_preset_cache.erase(shader_preset_key);

	LOG_MSG("RENDER: Reloading shader '%s'",
	        curr_shader_descriptor.ToString().c_str());

	constexpr auto ForceReload = true;

	const auto result = SetShaderInternal(curr_symbolic_shader_descriptor,
	                                      ForceReload);

	using enum OpenGlRenderer::SetShaderResult;

	switch (result) {
	case Ok:
		// Delete saved shader program
		glDeleteProgram(saved_shader.program_object);
		break;

	case PresetError:
		// Restore saved preset
		if (!curr_shader_descriptor.preset_name.empty()) {
			shader_preset_cache[shader_preset_key] = saved_shader_preset;
		}
		break;

	case ShaderError:
		// Restore saved shader
		shader_cache[shader_key] = saved_shader;

		// Restore saved preset
		if (!curr_shader_descriptor.preset_name.empty()) {
			shader_preset_cache[shader_preset_key] = saved_shader_preset;
		}
		break;

	default: assertm(false, "Invalid SetShaderResult value");
	}
}

void OpenGlRenderer::NotifyVideoModeChanged(const VideoMode& video_mode)
{
	const auto canvas_size_px = GetCanvasSizeInPixels();

	// We always expect a valid canvas and DOS video mode
	assert(!canvas_size_px.IsEmpty());
	assert(video_mode.width > 0 && video_mode.height > 0);

	// Handle shader auto-switching
	const auto new_descriptor = ShaderManager::GetInstance().NotifyRenderParametersChanged(
	        curr_shader_descriptor, canvas_size_px, video_mode);

	HandleShaderAndPresetChangeViaNotify(new_descriptor);
}

OpenGlRenderer::SetShaderResult OpenGlRenderer::MaybeSetShaderAndPreset(
        const ShaderDescriptor& curr_descriptor, const ShaderDescriptor& new_descriptor)
{
	const auto changed_shader = (curr_descriptor.shader_name !=
	                             new_descriptor.shader_name);

	const auto changed_preset = (curr_descriptor.preset_name !=
	                             new_descriptor.preset_name);

	using enum OpenGlRenderer::SetShaderResult;

	if (!changed_shader && !changed_preset) {
		// No change; report success
		return Ok;
	}

	if (changed_shader) {
		if (!SwitchShader(new_descriptor.shader_name)) {
			// Loading shader failed; report error
			return ShaderError;
		}
	}

	MaybeUpdateRenderSize(pass1.width, pass1.height);

	if (SwitchShaderPresetOrSetDefault(new_descriptor)) {
		return Ok;
	} else {
		return PresetError;
	}
}

bool OpenGlRenderer::SwitchShader(const std::string& shader_name)
{
	const auto maybe_shader = GetOrLoadAndCacheShader(shader_name);
	if (!maybe_shader) {
		return false;
	}

	pass2.shader = *maybe_shader;

	glUseProgram(pass2.shader.program_object);
	GetPass2UniformLocations(pass2.shader.info.default_preset.params);

	return true;
}

bool OpenGlRenderer::SwitchShaderPresetOrSetDefault(const ShaderDescriptor& descriptor)
{
	assert(!descriptor.shader_name.empty());

	auto set_default_preset = [&]() {
		assert(shader_cache.contains(descriptor.shader_name));
		auto& default_preset = shader_cache[descriptor.shader_name].info.default_preset;

		pass2.shader_preset = default_preset;
	};

	if (descriptor.preset_name.empty()) {
		set_default_preset();
		return true;

	} else {
		if (const auto maybe_preset = GetOrLoadAndCacheShaderPreset(descriptor);
		    maybe_preset) {

			pass2.shader_preset = *maybe_preset;
			return true;

		} else {
			set_default_preset();
			curr_shader_descriptor.preset_name.clear();
			return false;
		}
	}
}

std::optional<ShaderPreset> OpenGlRenderer::GetOrLoadAndCacheShaderPreset(
        const ShaderDescriptor& descriptor)
{
	assert(!descriptor.shader_name.empty());

	assert(shader_cache.contains(descriptor.shader_name));
	const auto& default_preset = shader_cache[descriptor.shader_name].info.default_preset;

	const auto cache_key = descriptor.ToString();

	if (!shader_preset_cache.contains(cache_key)) {
		const auto maybe_preset = ShaderManager::GetInstance().LoadShaderPreset(
		        descriptor, default_preset);

		if (!maybe_preset) {
			LOG_ERR("RENDER: Error loading shader preset '%s'; using default preset",
			        descriptor.ToString().c_str());
			return {};
		}

		shader_preset_cache[cache_key] = *maybe_preset;

#ifdef DEBUG_OPENGL
		LOG_DEBUG("OPENGL: Loaded and cached shader preset '%s'",
		          descriptor.ToString().c_str());
#endif

	} else {
#ifdef DEBUG_OPENGL
		LOG_DEBUG("OPENGL: Using cached shader preset '%s'",
		          descriptor.ToString().c_str());
#endif
	}

	return shader_preset_cache[cache_key];
}

std::optional<OpenGlRenderer::Shader> OpenGlRenderer::LoadAndBuildShader(
        const std::string& shader_name)
{

	auto log_error = [&]() {
		LOG_ERR("RENDER: Error loading shader '%s'", shader_name.c_str());
	};

	const auto maybe_result = ShaderManager::GetInstance().LoadShader(
	        shader_name, GlslExtension);

	if (!maybe_result) {
		log_error();
		return {};
	}

	const auto& [shader_info, shader_source] = *maybe_result;
	assert(shader_info.name == shader_name);

	const auto maybe_shader_program = BuildShaderProgram(shader_source);
	if (!maybe_shader_program) {
		log_error();
		return {};
	}

	return Shader{shader_info, *maybe_shader_program};
}

std::optional<OpenGlRenderer::Shader> OpenGlRenderer::GetOrLoadAndCacheShader(
        const std::string& shader_name)
{
	if (!shader_cache.contains(shader_name)) {
		const auto maybe_shader = LoadAndBuildShader(shader_name);
		if (!maybe_shader) {
			return {};
		}
		const auto& shader = *maybe_shader;

		shader_cache[shader.info.name] = shader;

#ifdef DEBUG_OPENGL
		LOG_DEBUG("OPENGL: Built and cached shader '%s'",
		          shader_name.c_str());
#endif

	} else {
#ifdef DEBUG_OPENGL
		LOG_DEBUG("OPENGL: Using cached shader '%s'", shader_name.c_str());
#endif
	}

	return shader_cache[shader_name];
}

void OpenGlRenderer::SetVsync(const bool is_enabled)
{
	const auto swap_interval = is_enabled ? 1 : 0;

	if (SDL_GL_SetSwapInterval(swap_interval) != 0) {
		// The requested swap_interval is not supported
		LOG_ERR("OPENGL: Error %s vsync: %s",
		        (is_enabled ? "enabling" : "disabling"),
		        SDL_GetError());
	}
}

void OpenGlRenderer::SetColorSpace(const ColorSpace _color_space)
{
	color_space = _color_space;

	glUseProgram(pass1.shader.program_object);
	UpdatePass1Uniforms();
}

void OpenGlRenderer::EnableImageAdjustments(const bool enable)
{
	pass1.enable_image_adjustments = enable;

	glUseProgram(pass1.shader.program_object);
	UpdatePass1Uniforms();
}

void OpenGlRenderer::SetImageAdjustmentSettings(const ImageAdjustmentSettings& settings)
{
	pass1.image_adjustment_settings = settings;

	glUseProgram(pass1.shader.program_object);
	UpdatePass1Uniforms();
}

void OpenGlRenderer::GetPass1UniformLocations()
{
	auto& u       = pass1.uniforms;
	const auto po = pass1.shader.program_object;

	u.input_texture = glGetUniformLocation(po, "inputTexture");

	u.color_space = glGetUniformLocation(po, "COLOR_SPACE");

	u.enable_adjustments = glGetUniformLocation(po, "ENABLE_ADJUSTMENTS");

	u.color_profile     = glGetUniformLocation(po, "COLOR_PROFILE");
	u.brightness        = glGetUniformLocation(po, "BRIGHTNESS");
	u.contrast          = glGetUniformLocation(po, "CONTRAST");
	u.gamma             = glGetUniformLocation(po, "GAMMA");
	u.saturation        = glGetUniformLocation(po, "SATURATION");
	u.digital_contrast  = glGetUniformLocation(po, "DIGITAL_CONTRAST");
	u.black_level_color = glGetUniformLocation(po, "BLACK_LEVEL_COLOR");
	u.black_level       = glGetUniformLocation(po, "BLACK_LEVEL");

	u.color_temperature_kelvin = glGetUniformLocation(po, "COLOR_TEMPERATURE_KELVIN");

	u.color_temperature_luma_preserve =
	        glGetUniformLocation(po, "COLOR_TEMPERATURE_LUMA_PRESERVE");

	u.red_gain   = glGetUniformLocation(po, "RED_GAIN");
	u.green_gain = glGetUniformLocation(po, "GREEN_GAIN");
	u.blue_gain  = glGetUniformLocation(po, "BLUE_GAIN");
}

void OpenGlRenderer::UpdatePass1Uniforms()
{
	const auto& u = pass1.uniforms;
	const auto& s = pass1.image_adjustment_settings;

	glUniform1i(u.input_texture, 0);

	glUniform1i(u.color_space, enum_val(color_space));

	glUniform1i(u.enable_adjustments, pass1.enable_image_adjustments ? 1 : 0);
	glUniform1i(u.color_profile, enum_val(s.crt_color_profile));
	glUniform1f(u.brightness, s.brightness);
	glUniform1f(u.contrast, s.contrast);
	glUniform1f(u.gamma, s.gamma);
	glUniform1f(u.digital_contrast, s.digital_contrast);

	constexpr auto RgbMaxValue = 255.0f;

	glUniform3f(u.black_level_color,
	            s.black_level_color.red / RgbMaxValue,
	            s.black_level_color.green / RgbMaxValue,
	            s.black_level_color.blue / RgbMaxValue);

	glUniform1f(u.black_level, s.black_level);
	glUniform1f(u.saturation, s.saturation);

	glUniform1f(u.color_temperature_kelvin,
	            static_cast<float>(s.color_temperature_kelvin));

	glUniform1f(u.color_temperature_luma_preserve,
	            s.color_temperature_luma_preserve);

	glUniform1f(u.red_gain, s.red_gain);
	glUniform1f(u.green_gain, s.green_gain);
	glUniform1f(u.blue_gain, s.blue_gain);
}

void OpenGlRenderer::GetPass2UniformLocations(const ShaderParameters& params)
{
	auto& u       = pass2.uniforms;
	const auto po = pass2.shader.program_object;

	u.input_texture = glGetUniformLocation(po, "rubyTexture");

	u.texture_size = glGetUniformLocation(po, "rubyTextureSize");
	u.input_size   = glGetUniformLocation(po, "rubyInputSize");
	u.output_size  = glGetUniformLocation(po, "rubyOutputSize");
	u.frame_count  = glGetUniformLocation(po, "rubyFrameCount");

	for (const auto& [name, value] : params) {
		const auto location = glGetUniformLocation(po, name.c_str());

		if (location == -1) {
			LOG_ERR("OPENGL: Error retrieving location of uniform '%s'",
			        name.c_str());
		} else {
			u.params[name] = location;
		}
	}
}

void OpenGlRenderer::UpdatePass2Uniforms()
{
	const auto& u = pass2.uniforms;

	if (u.texture_size > -1) {
		glUniform2f(u.texture_size,
		            static_cast<GLfloat>(pass1.width),
		            static_cast<GLfloat>(pass1.height));
	}

	if (u.input_size > -1) {
		glUniform2f(u.input_size,
		            static_cast<GLfloat>(pass1.width),
		            static_cast<GLfloat>(pass1.height));
	}

	if (u.output_size > -1) {
		glUniform2f(u.output_size, viewport_rect_px.w, viewport_rect_px.h);
	}

	if (u.frame_count > -1) {
		glUniform1i(u.frame_count, frame_count);
	}

	if (u.input_texture > -1) {
		glUniform1i(u.input_texture, 0);
	}

	for (const auto& [uniform_name, value] : pass2.shader_preset.params) {
		if (auto it = u.params.find(uniform_name); it != u.params.end()) {
			const auto& [_, location] = *it;

			if (location > -1) {
				glUniform1f(location, value);
			}
		} else {
			LOG_ERR("OPENGL: Unknown uniform name: '%s'",
			        uniform_name.c_str());
		}
	}
}

ShaderInfo OpenGlRenderer::GetCurrentShaderInfo()
{
	return pass2.shader.info;
}

ShaderPreset OpenGlRenderer::GetCurrentShaderPreset()
{
	return pass2.shader_preset;
}

std::string OpenGlRenderer::GetCurrentSymbolicShaderDescriptor()
{
	return curr_symbolic_shader_descriptor;
}

RenderedImage OpenGlRenderer::ReadPixelsPostShader(const DosBox::Rect output_rect_px)
{
	// Create new image
	RenderedImage image = {};

	image.params.width              = iroundf(output_rect_px.w);
	image.params.height             = iroundf(output_rect_px.h);
	image.params.double_width       = false;
	image.params.double_height      = false;
	image.params.pixel_aspect_ratio = {1};
	image.params.pixel_format       = PixelFormat::BGR24_ByteArray;

	constexpr auto BitsInByte = 8;

	image.pitch = image.params.width *
	              (get_bits_per_pixel(image.params.pixel_format) / BitsInByte);

	const auto image_size_bytes = check_cast<uint32_t>(image.params.height *
	                                                   image.pitch);

	image.image_data = new uint8_t[image_size_bytes];

	image.is_flipped_vertically = true;

	// Read from backbuffer
	glReadBuffer(GL_BACK);

	// Alignment is 4 by default which works fine when using the
	// GL_BGRA pixel format with glReadPixels(). We need to set it 1
	// to be able to use the GL_BGR format in order to conserve
	// memory. This should not cause any slowdowns whatsoever.
	//
	// TODO measure if this is actually true with some 4k captures and
	// potentially revert to the default 4-byte alignment
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	glReadPixels(iroundf(output_rect_px.x),
	             iroundf(output_rect_px.y),
	             image.params.width,
	             image.params.height,
	             GL_BGR,
	             GL_UNSIGNED_BYTE,
	             image.image_data);

	// Restore default
	glPixelStorei(GL_PACK_ALIGNMENT, 4);

	return image;
}

uint32_t OpenGlRenderer::MakePixel(const uint8_t red, const uint8_t green,
                                   const uint8_t blue)
{
	return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
}

#endif // C_OPENGL
