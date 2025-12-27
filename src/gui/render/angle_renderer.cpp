// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "angle_renderer.h"

#if C_ANGLE

#include "gui/private/common.h"
#include "gui/private/shader_manager.h"

#include "capture/capture.h"
#include "dosbox_config.h"
#include "misc/support.h"
#include "misc/video.h"
#include "utils/checks.h"
#include "utils/math_utils.h"

#include <glad/gles2.h>
#include <EGL/egl.h>

// must be included after dosbox_config.h
#include <SDL.h>
#include <SDL_syswm.h>
#if defined(SDL_VIDEO_DRIVER_COCOA)
#include <SDL_metal.h>
#endif

CHECK_NARROWING();

// #define DEBUG_OPENGL

constexpr auto GlslExtension = ".es.glsl";

AngleRenderer::AngleRenderer(const int x, const int y, const int width,
                               const int height, uint32_t sdl_window_flags)
{
	window = CreateSdlWindow(x, y, width, height, sdl_window_flags);
	if (!window) {
		const auto msg = format_str("ANGLE: Error creating window with flags %u (%s)",
		                            sdl_window_flags,
		                            SDL_GetError());
		LOG_ERR("%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	if (!InitRenderer()) {
		SDL_DestroyWindow(window);

		const auto msg = format_str("ANGLE: Error creating renderer");
		LOG_ERR("%s", msg.c_str());
		throw std::runtime_error(msg);
	}
}

SDL_Window* AngleRenderer::CreateSdlWindow(int x, int y,
                                           int width, int height,
                                           uint32_t flags)
{
#if defined(SDL_VIDEO_DRIVER_COCOA)
	flags |= SDL_WINDOW_METAL;
#endif
	
	return SDL_CreateWindow(
        DOSBOX_NAME,
        x, y,
        width, height,
        flags | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
}

bool AngleRenderer::InitRenderer()
{
#if defined(SDL_VIDEO_DRIVER_COCOA)
    metal_view = SDL_Metal_CreateView(window);
    if (!metal_view) {
        LOG_ERR("SDL_Metal_CreateView failed");
        return false;
    }
	auto raw_metal_layer = SDL_Metal_GetLayer(metal_view);
	if (!raw_metal_layer) {
		LOG_ERR("SDL_Metal_GetLayer failed");
		return false;
	}

	EGLNativeWindowType native_window =
    	(EGLNativeWindowType)raw_metal_layer;
#elif defined(SDL_VIDEO_DRIVER_WINDOWS)
    // Get native window
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);

    EGLNativeWindowType native_window =
        (EGLNativeWindowType)wmInfo.info.win.window;
#else
#error Unsupported platform
#endif

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);
    eglBindAPI(EGL_OPENGL_ES_API);

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_NONE
    };

    EGLint numConfigs;
    eglChooseConfig(display, configAttribs, &config, 1, &numConfigs);

    const EGLint ctxAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttribs);
	if (context == EGL_NO_CONTEXT) {
		EGLint err = eglGetError();
		LOG_ERR("ANGLE: eglCreateContext failed: 0x%x", err);
		return false;
    }

    surface = eglCreateWindowSurface(display, config, native_window, nullptr);	
	if (surface == EGL_NO_SURFACE) {
		EGLint err = eglGetError();
		LOG_ERR("ANGLE: eglCreateWindowSurface failed: 0x%x", err);
		return false;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
		EGLint err = eglGetError();
		LOG_ERR("ANGLE: eglMakeCurrent failed: 0x%x", err);
		return false;
	}

	if (!gladLoadGLES2((GLADloadfunc)eglGetProcAddress)) {
		LOG_ERR("ANGLE: failed to load GLES2");
		return false;
	}

	GLint size = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
	assert(size > 0);
	
	max_texture_size_px = size;

    LOG_INFO("ANGLE: %s", glGetString(GL_VERSION));

	// Vertex data (1 triangle)
	// ------------------------
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
	                      static_cast<GLvoid*>(0));

	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	return true;
}

AngleRenderer::~AngleRenderer()
{
    if (texture) glDeleteTextures(1, &texture);

    for (auto& [_, shader] : shader_cache)
        glDeleteProgram(shader.program_object);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);

#if defined(SDL_VIDEO_DRIVER_COCOA)	
	SDL_Metal_DestroyView(metal_view);
#endif

    if (display) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
    }

    SDL_DestroyWindow(window);
}

SDL_Window* AngleRenderer::GetWindow()
{
	return window;
}

DosBox::Rect AngleRenderer::GetCanvasSizeInPixels()
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

void AngleRenderer::NotifyViewportSizeChanged(const DosBox::Rect new_draw_rect_px)
{
	draw_rect_px = new_draw_rect_px;

	glViewport(static_cast<GLsizei>(draw_rect_px.x),
	           static_cast<GLsizei>(draw_rect_px.y),
	           static_cast<GLsizei>(draw_rect_px.w),
	           static_cast<GLsizei>(draw_rect_px.h));

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

	auto& shader_manager = ShaderManager::GetInstance();
	const auto curr_descriptor = shader_manager.GetCurrentShaderDescriptor();

	shader_manager.NotifyRenderParametersChanged(canvas_size_px, video_mode);

	const auto new_descriptor = shader_manager.GetCurrentShaderDescriptor();

	MaybeSwitchShaderAndPreset(curr_descriptor, new_descriptor);
}

void AngleRenderer::NotifyRenderSizeChanged(const int new_render_width_px,
                                             const int new_render_height_px)
{
	MaybeUpdateRenderSize(new_render_width_px, new_render_height_px);
}

void AngleRenderer::MaybeUpdateRenderSize(const int new_render_width_px,
                                           const int new_render_height_px)
{
	if (new_render_width_px > max_texture_size_px ||
	    new_render_height_px > max_texture_size_px) {

		LOG_ERR("ANGLE: No support for texture size of %dx%d pixels",
		        new_render_width_px,
		        new_render_height_px);
		return;
	}

	render_width_px  = new_render_width_px;
	render_height_px = new_render_height_px;

	GLuint new_texture;
	glGenTextures(1, &new_texture);

	if (!new_texture) {
		LOG_ERR("ANGLE: Error generating texture");
		return;
	}

	glBindTexture(GL_TEXTURE_2D, new_texture);

	// No borders
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	const auto& shader_settings = current_shader_preset.settings;

	const int filter_param = [&] {
		switch (shader_settings.texture_filter_mode) {
		case TextureFilterMode::NearestNeighbour: return GL_NEAREST;
		case TextureFilterMode::Bilinear: return GL_LINEAR;
		default: assertm(false, "Invalid TextureFilterMode"); return 0;
		}
	}();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_param);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_param);

	// Just create the texture; we'll copy the image data later with
	// `glTexSubImage2D()`
	//
	glTexImage2D(GL_TEXTURE_2D,
	             0,                	// mimap level (0 = base image)
	             GL_RGBA8,         	// internal format
	             render_width_px,  	// width
	             render_height_px, 	// height
	             0,                	// border (must be always 0)
	             GL_RGBA,     		// pixel data format
	             GL_UNSIGNED_BYTE, 	// pixel data type
	             nullptr           	// pointer to image data
	);

	// Fix channel order in hardware
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);

	constexpr auto BytesPerPixel = 4;

	// Create the texture
	const auto framebuf_bytes = static_cast<size_t>(render_width_px) *
	                            render_height_px * BytesPerPixel;

	curr_framebuf.resize(framebuf_bytes);
	last_framebuf.resize(framebuf_bytes);

	pitch = render_width_px * BytesPerPixel;

	if (texture) {
		glDeleteTextures(1, &texture);
	}

	texture = new_texture;
}

void AngleRenderer::StartFrame(uint8_t*& pixels_out, int& pitch_out)
{
	assert(!curr_framebuf.empty());

	pixels_out = curr_framebuf.data();

	if (pixels_out == nullptr) {
		return;
	}

	pitch_out = pitch;
}

void AngleRenderer::EndFrame()
{
	assert(!curr_framebuf.empty());
	assert(!last_framebuf.empty());

	// We need to copy the buffers. We can't just swap them because the VGA
	// emulation only writes the changed pixels to the framebuffer in each
	// frame.

	last_framebuf       = curr_framebuf;
	last_framebuf_dirty = true;
}

void AngleRenderer::PrepareFrame()
{
	assert(!last_framebuf.empty());

	if (last_framebuf_dirty) {
		glTexSubImage2D(GL_TEXTURE_2D,
		                0,                // mimap level (0 = base image)
		                0,                // x offset
		                0,                // y offset
		                render_width_px,  // width
		                render_height_px, // height
		                GL_RGBA,     // pixel data format
		                GL_UNSIGNED_BYTE, // pixel data type
		                last_framebuf.data() // pointer to image data
		);

		++frame_count;

		last_framebuf_dirty = false;
	}
}

void AngleRenderer::PresentFrame()
{
	glClear(GL_COLOR_BUFFER_BIT);

	UpdateUniforms();

	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	if (CAPTURE_IsCapturingPostRenderImage()) {
		// glReadPixels() implicitly blocks until all pipelined
		// rendering commands have finished, so we're guaranteed to
		// read the contents of the up-to-date backbuffer here right
		// before the buffer swap.
		//
		GFX_CaptureRenderedImage();
	}

	if (!eglSwapBuffers(display, surface)) {
		EGLint err = eglGetError();
		LOG_ERR("ANGLE:eglSwapBuffers failed: 0x%x", err);
	}
}

void AngleRenderer::GetUniformLocations(const ShaderParameters& params)
{
	// Get uniform locations
	uniforms.texture_size = glGetUniformLocation(current_shader.program_object,
	                                             "rubyTextureSize");

	uniforms.input_size = glGetUniformLocation(current_shader.program_object,
	                                           "rubyInputSize");

	uniforms.output_size = glGetUniformLocation(current_shader.program_object,
	                                            "rubyOutputSize");

	uniforms.frame_count = glGetUniformLocation(current_shader.program_object,
	                                            "rubyFrameCount");

	uniforms.input_texture = glGetUniformLocation(current_shader.program_object,
	                                              "rubyTexture");

	for (const auto& [name, value] : params) {
		const auto location = glGetUniformLocation(current_shader.program_object,
		                                           name.c_str());

		if (location == -1) {
			LOG_WARNING("ANGLE: Error retrieving location of uniform '%s'",
			            name.c_str());
		} else {
			uniforms.params[name] = location;
		}
	}
}

void AngleRenderer::UpdateUniforms()
{
	if (uniforms.texture_size > -1) {
		glUniform2f(uniforms.texture_size,
		            static_cast<GLfloat>(render_width_px),
		            static_cast<GLfloat>(render_height_px));
	}

	if (uniforms.input_size > -1) {
		glUniform2f(uniforms.input_size,
		            static_cast<GLfloat>(render_width_px),
		            static_cast<GLfloat>(render_height_px));
	}

	if (uniforms.output_size > -1) {
		glUniform2f(uniforms.output_size,
		            static_cast<GLfloat>(draw_rect_px.w),
		            static_cast<GLfloat>(draw_rect_px.h));
	}

	if (uniforms.frame_count > -1) {
		glUniform1i(uniforms.frame_count, frame_count);
	}

	if (uniforms.input_texture > -1) {
		glUniform1i(uniforms.input_texture, 0);
	}

	for (const auto& [uniform_name, value] : current_shader_preset.params) {
		if (auto it = uniforms.params.find(uniform_name);
		    it != uniforms.params.end()) {

			const auto& [_, location] = *it;

			if (location > -1) {
				glUniform1f(location, value);
			}
		} else {
			LOG_WARNING("ANGLE: Unknown uniform name: '%s'",
			            uniform_name.c_str());
		}
	}
}

std::optional<GLuint> AngleRenderer::BuildShader(const GLenum type,
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
			LOG_WARNING("ANGLE: Shader info log: %s", info_log.data());
		} else {
			LOG_ERR("ANGLE: Error compiling shader: %s",
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
std::optional<GLuint> AngleRenderer::BuildShaderProgram(const std::string& shader_source)
{
	if (shader_source.empty()) {
		LOG_ERR("ANGLE: No shader source present");
		return {};
	}

	const auto maybe_vertex_shader = BuildShader(GL_VERTEX_SHADER, shader_source);
	if (!maybe_vertex_shader) {
		LOG_ERR("ANGLE: Error compiling vertex shader");
		return {};
	}
	const auto vertex_shader = *maybe_vertex_shader;

	const auto maybe_fragment_shader = BuildShader(GL_FRAGMENT_SHADER,
	                                               shader_source);
	if (!maybe_fragment_shader) {
		LOG_ERR("ANGLE: Error compiling fragment shader");
		glDeleteShader(vertex_shader);
		return {};
	}
	const auto fragment_shader = *maybe_fragment_shader;

	const GLuint shader_program = glCreateProgram();

	if (!shader_program) {
		LOG_ERR("ANGLE: Error creating shader program");
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
			LOG_WARNING("ANGLE: Program info log:\n %s",
			            info_log.data());
		} else {
			LOG_ERR("ANGLE: Error linking shader program:\n %s",
			        info_log.data());
		}
	}

	if (!is_program_linked) {
		glDeleteProgram(shader_program);
		return {};
	}

	return shader_program;
}

AngleRenderer::SetShaderResult AngleRenderer::SetShader(const std::string& shader_descriptor)
{
	return SetShaderInternal(shader_descriptor);
}

AngleRenderer::SetShaderResult AngleRenderer::SetShaderInternal(
        const std::string& shader_descriptor, const bool force_reload)
{
	using enum AngleRenderer::SetShaderResult;

	auto& shader_manager = ShaderManager::GetInstance();

	const auto curr_descriptor = force_reload
	                                   ? ShaderDescriptor{"", ""}
	                                   : shader_manager.GetCurrentShaderDescriptor();

	shader_manager.NotifyShaderChanged(shader_descriptor, GlslExtension);

	const auto new_descriptor = shader_manager.GetCurrentShaderDescriptor();

	if (!MaybeSwitchShaderAndPreset(curr_descriptor, new_descriptor)) {
		return ShaderError;
	}

	if (!new_descriptor.preset_name.empty() &&
	    current_shader_descriptor.preset_name.empty()) {

		current_shader_descriptor_string = current_shader_descriptor.shader_name;

		// We could set the shader but not the preset.
		return PresetError;
	}

	current_shader_descriptor_string = shader_descriptor;
	return Ok;
}

bool AngleRenderer::ForceReloadCurrentShader()
{
	assert(shader_cache.contains(current_shader.info.name));

	const auto& shader = shader_cache[current_shader.info.name];
	glDeleteProgram(shader.program_object);

	shader_cache.erase(current_shader.info.name);

	const auto descriptor = ShaderManager::GetInstance().GetCurrentShaderDescriptor();
	shader_preset_cache.erase(descriptor.ToString());

	constexpr auto ForceReload = true;
	return (SetShaderInternal(current_shader_descriptor_string, ForceReload) ==
	        AngleRenderer::SetShaderResult::Ok);
}

void AngleRenderer::NotifyVideoModeChanged(const VideoMode& video_mode)
{
	const auto canvas_size_px = GetCanvasSizeInPixels();

	// We always expect a valid canvas and DOS video mode
	assert(!canvas_size_px.IsEmpty());
	assert(video_mode.width > 0 && video_mode.height > 0);

	auto& shader_manager = ShaderManager::GetInstance();
	const auto curr_descriptor = shader_manager.GetCurrentShaderDescriptor();

	shader_manager.NotifyRenderParametersChanged(canvas_size_px, video_mode);

	const auto new_descriptor = shader_manager.GetCurrentShaderDescriptor();

	MaybeSwitchShaderAndPreset(curr_descriptor, new_descriptor);
}

bool AngleRenderer::MaybeSwitchShaderAndPreset(const ShaderDescriptor& curr_descriptor,
                                                const ShaderDescriptor& new_descriptor)
{
	const auto changed_shader = (curr_descriptor.shader_name !=
	                             new_descriptor.shader_name);

	const auto changed_preset = (curr_descriptor.preset_name !=
	                             new_descriptor.preset_name);

	if (!changed_shader && !changed_preset) {
		// No change; report success
		return true;
	}

	if (changed_shader) {
		if (!SwitchShader(new_descriptor.shader_name)) {
			// Loading shader failed; report error
			return false;
		}
	}

	SwitchShaderPresetOrSetDefault(new_descriptor);
	MaybeUpdateRenderSize(render_width_px, render_height_px);

	return true;
}

bool AngleRenderer::SwitchShader(const std::string& shader_name)
{
	const auto maybe_shader = GetOrLoadAndCacheShader(shader_name);
	if (!maybe_shader) {
		return false;
	}

	current_shader = *maybe_shader;

	glUseProgram(current_shader.program_object);
	GetUniformLocations(current_shader.info.default_preset.params);

	return true;
}

void AngleRenderer::SwitchShaderPresetOrSetDefault(const ShaderDescriptor& descriptor)
{
	assert(!descriptor.shader_name.empty());

	auto set_default_preset = [&]() {
#ifdef DEBUG_OPENGL
		LOG_DEBUG("ANGLE: Using default shader preset",
		          descriptor.ToString().c_str());
#endif
		assert(shader_cache.contains(descriptor.shader_name));
		auto& default_preset = shader_cache[descriptor.shader_name].info.default_preset;

		current_shader_preset = default_preset;
	};

	current_shader_descriptor = descriptor;

	if (descriptor.preset_name.empty()) {
		set_default_preset();

	} else {
		if (const auto maybe_preset = GetOrLoadAndCacheShaderPreset(descriptor);
		    maybe_preset) {

			current_shader_preset = *maybe_preset;

		} else {
			set_default_preset();
			current_shader_descriptor.preset_name.clear();
		}
	}
}

std::optional<ShaderPreset> AngleRenderer::GetOrLoadAndCacheShaderPreset(
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
			// Error loading preset; report error
			return {};
		}

		shader_preset_cache[cache_key] = *maybe_preset;

#ifdef DEBUG_OPENGL
		LOG_DEBUG("ANGLE: Loaded and cached shader preset '%s'",
		          descriptor.ToString().c_str());
#endif

	} else {
#ifdef DEBUG_OPENGL
		LOG_DEBUG("ANGLE: Using cached shader preset '%s'",
		          descriptor.ToString().c_str());
#endif
	}

	return shader_preset_cache[cache_key];
}

std::optional<AngleRenderer::Shader> AngleRenderer::GetOrLoadAndCacheShader(
        const std::string& shader_name)
{
	if (!shader_cache.contains(shader_name)) {
		const auto maybe_result = ShaderManager::GetInstance().LoadShader(
		        shader_name, GlslExtension);

		if (!maybe_result) {
			return {};
		}

		const auto& [shader_info, shader_source] = *maybe_result;
		assert(shader_info.name == shader_name);

		const auto maybe_shader_program = BuildShaderProgram(shader_source);
		if (!maybe_shader_program) {
			return {};
		}

		shader_cache[shader_info.name] = {shader_info, *maybe_shader_program};

#ifdef DEBUG_OPENGL
		LOG_DEBUG("ANGLE: Built and cached shader '%s'",
		          shader_name.c_str());
#endif

	} else {
#ifdef DEBUG_OPENGL
		LOG_DEBUG("ANGLE: Using cached shader '%s'", shader_name.c_str());
#endif
	}

	return shader_cache[shader_name];
}

void AngleRenderer::SetVsync(const bool is_enabled)
{
	const auto swap_interval = is_enabled ? 1 : 0;

	if (eglSwapInterval(display, swap_interval) != EGL_TRUE) {
		// The requested swap_interval is not supported
		LOG_WARNING("ANGLE: Error %s vsync: %s",
		            (is_enabled ? "enabling" : "disabling"),
		            SDL_GetError());
	}
}

ShaderInfo AngleRenderer::GetCurrentShaderInfo()
{
	return current_shader.info;
}

ShaderPreset AngleRenderer::GetCurrentShaderPreset()
{
	return current_shader_preset;
}

std::string AngleRenderer::GetCurrentShaderDescriptorString()
{
	return current_shader_descriptor_string;
}

RenderedImage AngleRenderer::ReadPixelsPostShader(const DosBox::Rect output_rect_px)
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

	image.image_data   = new uint8_t[image_size_bytes];
	image.palette_data = nullptr;

	image.is_flipped_vertically = true;

	// Read from backbuffer
	glReadBuffer(GL_BACK);

	// Alignment is 4 by default which works fine when using the
	// GL_BGRA8_EXT pixel format with glReadPixels(). We need to set it 1
	// to be able to use the GL_BGR format in order to conserve
	// memory. This should not cause any slowdowns whatsoever.
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	glReadPixels(iroundf(output_rect_px.x),
	             iroundf(output_rect_px.y),
	             image.params.width,
	             image.params.height,
	             GL_RGBA,
	             GL_UNSIGNED_BYTE,
	             image.image_data);

	return image;
}

uint32_t AngleRenderer::MakePixel(const uint8_t red, const uint8_t green,
                                   const uint8_t blue)
{
	return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
}

#endif // C_ANGLE
