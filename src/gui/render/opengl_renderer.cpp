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

CHECK_NARROWING();

// #define DEBUG_OPENGL

// Define to report OpenGL errors
// #define DEBUG_OPENGL_ERROR

#ifdef DEBUG_OPENGL_ERROR
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
[[maybe_unused]] static void maybe_log_opengl_error(const char*)
{
	return;
}
#endif

constexpr auto GlslExtension = ".glsl";

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
	// TODO Ideally, all of these calls should be checked for failure.

	// Request 24-bits sRGB framebuffer, don't care about depth buffer
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	// SDL_GL_SetAttribute() is not a reliable error point.
	// SDL mostly only does sanity checks to ensure we pass valid values.
	// Success does not mean we will actually get an sRGB framebuffer.
	// We won't know if it actually worked until after we create the window.
	if (SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1)) {
		LOG_ERR("OPENGL: Error requesting an sRGB framebuffer: %s",
		        SDL_GetError());
	}

	// Explicitly request an OpenGL 2.1 compatibility context
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
	                    SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

	// Request an OpenGL-ready window
	auto flags = sdl_window_flags;
	flags |= SDL_WINDOW_OPENGL;

	SDL_Window* window = SDL_CreateWindow(DOSBOX_NAME, x, y, width, height, flags);
	if (!window) {
		// Try again without sRGB. This has been a problem with KMSDRM.
		SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 0);
		window = SDL_CreateWindow(DOSBOX_NAME, x, y, width, height, flags);
		if (window) {
			LOG_ERR("OPENGL: Failed to create an sRGB framebuffer. Falling back to non-sRGB.");
		}
	}

	return window;
}

bool OpenGlRenderer::InitRenderer()
{
	auto new_context = SDL_GL_CreateContext(window);
	if (!new_context) {
		LOG_ERR("SDL: Error creating OpenGL context: %s", SDL_GetError());
		return false;
	}

	context = new_context;

	const auto version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);

	is_framebuffer_srgb_capable = [&] {
		int gl_framebuffer_srgb_capable = 0;

		if (SDL_GL_GetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,
		                        &gl_framebuffer_srgb_capable)) {

			LOG_WARNING("OPENGL: Error getting the framebuffer's sRGB status: %s",
			            SDL_GetError());
		}

		// TODO GL_ARB_framebuffer_sRGB & GL_EXT_framebuffer_sRGB are
		// OpenGL 3.0 Core Profile features. There will be no need for
		// these checks once we're on core profile 3.3
		//
		return (GLAD_VERSION_MAJOR(version) >= 3 ||
		        // TODO use glad
		        SDL_GL_ExtensionSupported("GL_ARB_framebuffer_sRGB") ||
		        // TODO use glad
		        SDL_GL_ExtensionSupported("GL_EXT_framebuffer_sRGB")) &&
		       (gl_framebuffer_srgb_capable > 0);
	}();

	GLint size = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);

	max_texture_size_px = size;

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	LOG_INFO("OPENGL: Version: %d.%d, GLSL version: %s, vendor: %s",
	         GLAD_VERSION_MAJOR(version),
	         GLAD_VERSION_MINOR(version),
	         safe_gl_get_string(GL_SHADING_LANGUAGE_VERSION, "unknown"),
	         safe_gl_get_string(GL_VENDOR, "unknown"));

	return true;
}

OpenGlRenderer::~OpenGlRenderer()
{
	SDL_GL_ResetAttributes();

	if (texture) {
		glDeleteTextures(1, &texture);
		texture = 0;
	}

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

void OpenGlRenderer::UpdateViewport(const DosBox::Rect _draw_rect_px)
{
	draw_rect_px = _draw_rect_px;

	glViewport(static_cast<GLsizei>(draw_rect_px.x),
	           static_cast<GLsizei>(draw_rect_px.y),
	           static_cast<GLsizei>(draw_rect_px.w),
	           static_cast<GLsizei>(draw_rect_px.h));
}

bool OpenGlRenderer::UpdateRenderSize(const int new_render_width_px,
                                      const int new_render_height_px)
{
	if (new_render_width_px > max_texture_size_px ||
	    new_render_height_px > max_texture_size_px) {

		LOG_ERR("OPENGL: No support for texture size of %dx%d pixels",
		        new_render_width_px,
		        new_render_height_px);
		return false;
	}

	render_width_px  = new_render_width_px;
	render_height_px = new_render_height_px;

	GLuint new_texture;
	glGenTextures(1, &new_texture);

	if (!new_texture) {
		LOG_ERR("OPENGL: Error generating texture");
		return false;
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

	if ((shader_settings.use_srgb_framebuffer || shader_settings.use_srgb_texture) &&
	    !is_framebuffer_srgb_capable) {

		LOG_WARNING("OPENGL: sRGB framebuffer not supported");
	}

	// Using GL_SRGB8_ALPHA8 because GL_SRGB8 doesn't work properly
	// with Mesa drivers on certain integrated Intel GPUs
	const auto texture_format = shader_settings.use_srgb_texture &&
	                                            is_framebuffer_srgb_capable
	                                  ? GL_SRGB8_ALPHA8
	                                  : GL_RGB8;

	glTexImage2D(GL_TEXTURE_2D,
	             0,
	             texture_format,
	             render_width_px,
	             render_height_px,
	             0,
	             GL_BGRA,
	             GL_UNSIGNED_BYTE,
	             nullptr);

	if (shader_settings.use_srgb_framebuffer && is_framebuffer_srgb_capable) {
		glEnable(GL_FRAMEBUFFER_SRGB);
#if 0
			LOG_MSG("OPENGL: Using sRGB framebuffer");
#endif
	} else {
		glDisable(GL_FRAMEBUFFER_SRGB);
	}

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

	return true;
}

void OpenGlRenderer::StartFrame(uint8_t*& pixels_out, int& pitch_out)
{
	assert(!curr_framebuf.empty());

	pixels_out = curr_framebuf.data();

	if (pixels_out == nullptr) {
		return;
	}

	pitch_out = pitch;
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
		glTexSubImage2D(GL_TEXTURE_2D,
		                0,
		                0,
		                0,
		                render_width_px,
		                render_height_px,
		                GL_BGRA,
		                GL_UNSIGNED_INT_8_8_8_8_REV,
		                last_framebuf.data());

		++frame_count;

		last_framebuf_dirty = false;
	}
}

void OpenGlRenderer::PresentFrame()
{
	glClear(GL_COLOR_BUFFER_BIT);

	UpdateUniforms();

	glDrawArrays(GL_TRIANGLES, 0, 3);

	if (CAPTURE_IsCapturingPostRenderImage()) {
		// glReadPixels() implicitly blocks until all pipelined
		// rendering commands have finished, so we're guaranteed to
		// read the contents of the up-to-date backbuffer here right
		// before the buffer swap.
		//
		GFX_CaptureRenderedImage();
	}

	SDL_GL_SwapWindow(window);
}

void OpenGlRenderer::GetUniformLocations(const ShaderParameters& params)
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
			LOG_WARNING("OPENGL: Error retrieving location of uniform '%s'",
			            name.c_str());
		} else {
			uniforms.params[name] = location;
		}
	}
}

void OpenGlRenderer::UpdateUniforms()
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
			LOG_WARNING("OPENGL: Unknown uniform name: '%s'",
			            uniform_name.c_str());
		}
	}
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

	// Set vertex data. Vertices are in counter-clockwise order.
	const GLint vertex_attrib_location = glGetAttribLocation(shader_program,
	                                                         "a_position");

	if (vertex_attrib_location == -1) {
		LOG_ERR("OPENGL: Error retrieving vertex position attribute location");
		glDeleteProgram(shader_program);
		return {};
	}

	// Lower left
	vertex_data[0] = -1.0f;
	vertex_data[1] = -1.0f;

	// Lower right
	vertex_data[2] = 3.0f;
	vertex_data[3] = -1.0f;

	// Upper left
	vertex_data[4] = -1.0f;
	vertex_data[5] = 3.0f;

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
	                      vertex_data);

	glEnableVertexAttribArray(static_cast<GLuint>(vertex_attrib_location));

	return shader_program;
}

OpenGlRenderer::SetShaderResult OpenGlRenderer::SetShader(const std::string& shader_descriptor)
{
	return SetShaderInternal(shader_descriptor);
}

OpenGlRenderer::SetShaderResult OpenGlRenderer::SetShaderInternal(
        const std::string& shader_descriptor, const bool force_reload)
{
	using enum OpenGlRenderer::SetShaderResult;

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

bool OpenGlRenderer::ForceReloadCurrentShader()
{
	assert(shader_cache.contains(current_shader.info.name));

	const auto& shader = shader_cache[current_shader.info.name];
	glDeleteProgram(shader.program_object);

	shader_cache.erase(current_shader.info.name);

	const auto descriptor = ShaderManager::GetInstance().GetCurrentShaderDescriptor();
	shader_preset_cache.erase(descriptor.ToString());

	constexpr auto ForceReload = true;
	return (SetShaderInternal(current_shader_descriptor_string, ForceReload) ==
	        OpenGlRenderer::SetShaderResult::Ok);
}

bool OpenGlRenderer::MaybeAutoSwitchShader(const DosBox::Rect canvas_size_px,
                                           const VideoMode& video_mode)
{
	// We always expect a valid canvas and DOS video mode
	assert(!canvas_size_px.IsEmpty());
	assert(video_mode.width > 0 && video_mode.height > 0);

	auto& shader_manager = ShaderManager::GetInstance();
	const auto curr_descriptor = shader_manager.GetCurrentShaderDescriptor();

	shader_manager.NotifyRenderParametersChanged(canvas_size_px, video_mode);

	const auto new_descriptor = shader_manager.GetCurrentShaderDescriptor();

	return MaybeSwitchShaderAndPreset(curr_descriptor, new_descriptor);
}

bool OpenGlRenderer::MaybeSwitchShaderAndPreset(const ShaderDescriptor& curr_descriptor,
                                                const ShaderDescriptor& new_descriptor)
{
	const auto changed_shader = (curr_descriptor.shader_name !=
	                             new_descriptor.shader_name);
	if (changed_shader) {
		if (!SwitchShader(new_descriptor.shader_name)) {
			return false;
		}
	}

	if (changed_shader ||
	    (curr_descriptor.preset_name != new_descriptor.preset_name)) {

		SwitchShaderPresetOrSetDefault(new_descriptor);
	}

	return UpdateRenderSize(render_width_px, render_height_px);
}

bool OpenGlRenderer::SwitchShader(const std::string& shader_name)
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

void OpenGlRenderer::SwitchShaderPresetOrSetDefault(const ShaderDescriptor& descriptor)
{
	assert(!descriptor.shader_name.empty());

	auto set_default_preset = [&]() {
#ifdef DEBUG_OPENGL
		LOG_DEBUG("OPENGL: Using default shader preset",
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

std::optional<OpenGlRenderer::Shader> OpenGlRenderer::GetOrLoadAndCacheShader(
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
		LOG_WARNING("OPENGL: Error %s vsync: %s",
		            (is_enabled ? "enabling" : "disabling"),
		            SDL_GetError());
	}
}

ShaderInfo OpenGlRenderer::GetCurrentShaderInfo()
{
	return current_shader.info;
}

ShaderPreset OpenGlRenderer::GetCurrentShaderPreset()
{
	return current_shader_preset;
}

std::string OpenGlRenderer::GetCurrentShaderDescriptorString()
{
	return current_shader_descriptor_string;
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
	// GL_BGRA pixel format with glReadPixels(). We need to set it 1
	// to be able to use the GL_BGR format in order to conserve
	// memory. This should not cause any slowdowns whatsoever.
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	glReadPixels(iroundf(output_rect_px.x),
	             iroundf(output_rect_px.y),
	             image.params.width,
	             image.params.height,
	             GL_BGR,
	             GL_UNSIGNED_BYTE,
	             image.image_data);

	return image;
}

uint32_t OpenGlRenderer::MakePixel(const uint8_t red, const uint8_t green,
                                   const uint8_t blue)
{
	return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
}

#endif // C_OPENGL
