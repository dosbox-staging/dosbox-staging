// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "opengl_renderer.h"

#if C_OPENGL

#include "gui/private/common.h"
#include "private/auto_shader_switcher.h"

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
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data.data(), GL_STATIC_DRAW);

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

	// Set up shader passes
	// --------------------
	// Pass 1
	ShaderPass pass1;

	pass1.id          = ShaderPassId::ImageAdjustments;
	pass1.in_texture  = 0;  // will be set later
	pass1.out_texture = 0;  // will be set later
	pass1.viewport    = {}; // will be set later

	glGenFramebuffers(1, &pass1.out_fbo);

	const auto maybe_shader = ShaderManager::GetInstance().LoadShader(
	        ImageAdjustmentsShaderName);

	if (!maybe_shader) {
		E_Exit("OPENGL: Cannot load '%s' shader, exiting",
		       ImageAdjustmentsShaderName);
	}
	pass1.shader = *maybe_shader;

	shader_passes.push_back(pass1);

	// Pass 2
	ShaderPass pass2;
	pass2.id          = ShaderPassId::Main;
	pass2.shader      = {}; // will be set later
	pass2.in_texture  = 0;  // will be set later
	pass2.out_fbo     = 0;  // last pass; doesn't have an FBO
	pass2.out_texture = 0;  // last pass; doesn't have an output texture
	pass2.viewport    = {}; // will be set later

	shader_passes.push_back(pass2);

	return true;
}

OpenGlRenderer::~OpenGlRenderer()
{
	SDL_GL_ResetAttributes();

	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);

	if (input_texture.texture) {
		glDeleteTextures(1, &input_texture.texture);
		input_texture.texture = 0;
	}

	for (auto& pass : shader_passes) {
		if (pass.out_texture) {
			glDeleteTextures(1, &pass.out_texture);
			pass.out_texture = 0;
		}
		glDeleteFramebuffers(1, &pass.out_fbo);
	}

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
	auto& pass    = GetShaderPass(ShaderPassId::Main);
	pass.viewport = draw_rect_px;

	UpdateMainShaderPassUniforms();

	// If the viewport size has changed, the canvas size might have
	// changed too.
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

	auto& shader_switcher = AutoShaderSwitcher::GetInstance();
	shader_switcher.NotifyRenderParametersChanged(canvas_size_px, video_mode);

	const auto new_descriptor = shader_switcher.GetCurrentShaderDescriptor();

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

ShaderPass& OpenGlRenderer::GetShaderPass(const ShaderPassId id)
{
	for (auto& pass : shader_passes) {
		if (pass.id == id) {
			return pass;
		}
	}
	assertm(false, "Invalid shader pass ID");

	// Just to make the compiler happy
	return shader_passes[0];
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
	if (new_render_width_px == input_texture.width &&
	    new_render_height_px == input_texture.height) {
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

	input_texture.width  = new_render_width_px;
	input_texture.height = new_render_height_px;

	RecreateInputTexture();

	GLuint prev_pass_out_texture = 0;

	for (auto& pass : shader_passes) {
		if (pass.id == ShaderPassId::ImageAdjustments) {
			pass.in_texture = input_texture.texture;
		} else {
			pass.in_texture = prev_pass_out_texture;
		}

		if (pass.out_fbo != 0) {
			if (pass.out_texture) {
				glDeleteTextures(1, &pass.out_texture);
			}
			pass.out_texture = CreateTexture();

			pass.viewport = {0,
			                 0,
			                 input_texture.width,
			                 input_texture.height};

			// Set up off-screen framebuffer
			glBindFramebuffer(GL_FRAMEBUFFER, pass.out_fbo);

			glFramebufferTexture2D(GL_FRAMEBUFFER,
			                       GL_COLOR_ATTACHMENT0,
			                       GL_TEXTURE_2D,
			                       pass.out_texture,
			                       0);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
			    GL_FRAMEBUFFER_COMPLETE) {
				LOG_ERR("OPENGL: Framebuffer is not complete");
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			prev_pass_out_texture = pass.out_texture;
		}
	}

	UpdateMainShaderPassUniforms();
}

void OpenGlRenderer::RecreateInputTexture()
{
	if (input_texture.texture) {
		glDeleteTextures(1, &input_texture.texture);
	}

	glGenTextures(1, &input_texture.texture);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, input_texture.texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Just create the texture; we'll copy the image data later with
	// `glTexSubImage2D()`
	//
	glTexImage2D(GL_TEXTURE_2D,
	             0,                    // mimap level (0 = base image)
	             GL_RGB8,              // internal format
	             input_texture.width,  // width
	             input_texture.height, // height
	             0,                    // border (must be always 0)
	             GL_BGRA,              // pixel data format
	             GL_UNSIGNED_BYTE,     // pixel data type
	             nullptr               // pointer to image data
	);

	glBindTexture(GL_TEXTURE_2D, 0);

	// Allocate host memory buffers for the texture data. The video card
	// emulation will write to these buffers, then we'll copy the data to
	// the texture in GPU memory with `glTexSubImage2D()` before presenting
	// the frame.
	const auto pitch_pixels = input_texture.width;
	const auto num_pixels   = static_cast<size_t>(pitch_pixels) *
	                        input_texture.height;

	curr_framebuf.resize(num_pixels);
	last_framebuf.resize(num_pixels);

	constexpr auto BytesPerPixel = sizeof(uint32_t);
	const auto pitch_bytes       = pitch_pixels * BytesPerPixel;

	input_texture.pitch = check_cast<int>(pitch_bytes);
}

GLuint OpenGlRenderer::CreateTexture()
{
	GLuint texture = 0;

	glGenTextures(1, &texture);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	SetTextureFiltering(texture);

	glTexImage2D(GL_TEXTURE_2D,
	             0,                    // mimap level (0 = base image)
	             GL_RGB32F,            // internal format
	             input_texture.width,  // width
	             input_texture.height, // height
	             0,                    // border (must be always 0)
	             GL_BGRA,              // pixel data format
	             GL_FLOAT,             // pixel data type
	             nullptr);             // pointer to image data

	glBindTexture(GL_TEXTURE_2D, 0);

	return texture;
}

void OpenGlRenderer::SetTextureFiltering(const GLuint texture)
{
	glBindTexture(GL_TEXTURE_2D, texture);

	const auto& shader_settings = main_shader_preset.settings;

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
	pitch_out = input_texture.pitch;
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
		glBindTexture(GL_TEXTURE_2D, input_texture.texture);

		glTexSubImage2D(GL_TEXTURE_2D,
		                0, // mimap level (0 = base image)
		                0, // x offset
		                0, // y offset
		                input_texture.width,  // width
		                input_texture.height, // height
		                GL_BGRA,              // pixel data format
		                GL_UNSIGNED_INT_8_8_8_8_REV, // pixel data type
		                last_framebuf.data() // pointer to image data
		);

		glBindTexture(GL_TEXTURE_2D, 0);

		last_framebuf_dirty = false;
	}
}

void OpenGlRenderer::RenderPass(const ShaderPass& pass)
{
	// Render into an off-screen buffer
	glBindFramebuffer(GL_FRAMEBUFFER, pass.out_fbo);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(pass.shader.program_object);

	// Bind input texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, pass.in_texture);

	// Set up viewport
	glViewport(static_cast<GLsizei>(pass.viewport.x),
	           static_cast<GLsizei>(pass.viewport.y),
	           static_cast<GLsizei>(pass.viewport.w),
	           static_cast<GLsizei>(pass.viewport.h));

	// Apply shader by drawing an oversized triangle
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// Reset binds
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
}

void OpenGlRenderer::PresentFrame()
{
	for (const auto& pass : shader_passes) {
		RenderPass(pass);
	}

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

OpenGlRenderer::SetShaderResult OpenGlRenderer::SetShader(const std::string& symbolic_shader_descriptor)
{
	return SetShaderInternal(symbolic_shader_descriptor);
}

OpenGlRenderer::SetShaderResult OpenGlRenderer::SetShaderInternal(
        const std::string& new_symbolic_shader_descriptor, const bool force_reload)
{
	using enum OpenGlRenderer::SetShaderResult;

	auto& shader_switcher = AutoShaderSwitcher::GetInstance();

	const auto curr_descriptor = force_reload ? ShaderDescriptor{"", ""}
	                                          : curr_shader_descriptor;

	shader_switcher.NotifyShaderChanged(new_symbolic_shader_descriptor);

	const auto new_descriptor = shader_switcher.GetCurrentShaderDescriptor();

	const auto result = MaybeSetShaderAndPreset(curr_descriptor, new_descriptor);

	switch (result) {
	case Ok:
		curr_shader_descriptor = new_descriptor;
		curr_symbolic_shader_descriptor = new_symbolic_shader_descriptor;
		break;

	case PresetError: {
		curr_shader_descriptor = {new_descriptor.shader_name, ""};

		auto descriptor = ShaderDescriptor::FromString(new_symbolic_shader_descriptor,
		                                               GlslExtension);
		descriptor.preset_name.clear();

		curr_symbolic_shader_descriptor = descriptor.ToString();
		break;
	}
	case ShaderError: break;

	default:
		assertm(false, "Invalid SetShaderResult value");
		return ShaderError;
	}

	if (result != ShaderError) {
		// The texture filtering mode might have changed
		const auto& pass = GetShaderPass(ShaderPassId::ImageAdjustments);
		SetTextureFiltering(pass.out_texture);

		UpdateMainShaderPassUniforms();
	}

	return result;
}

void OpenGlRenderer::ForceReloadCurrentShader()
{
	LOG_MSG("RENDER: Reloading shader '%s'",
	        curr_shader_descriptor.ToString().c_str());

	// Force reload by clearing the shader manager's cache
	ShaderManager::GetInstance().ForceReloadShader(
	        curr_shader_descriptor.shader_name);

	constexpr auto ForceReload = true;
	SetShaderInternal(curr_symbolic_shader_descriptor, ForceReload);
}

void OpenGlRenderer::NotifyVideoModeChanged(const VideoMode& video_mode)
{
	const auto canvas_size_px = GetCanvasSizeInPixels();

	// We always expect a valid canvas and DOS video mode
	assert(!canvas_size_px.IsEmpty());
	assert(video_mode.width > 0 && video_mode.height > 0);

	// Handle shader auto-switching
	auto& shader_switcher = AutoShaderSwitcher::GetInstance();
	shader_switcher.NotifyRenderParametersChanged(canvas_size_px, video_mode);

	const auto new_descriptor = shader_switcher.GetCurrentShaderDescriptor();

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

	MaybeUpdateRenderSize(input_texture.width, input_texture.height);

	if (SwitchShaderPresetOrSetDefault(new_descriptor)) {
		return Ok;
	} else {
		return PresetError;
	}
}

bool OpenGlRenderer::SwitchShader(const std::string& shader_name)
{
	auto& pass = GetShaderPass(ShaderPassId::Main);

	const auto maybe_shader = ShaderManager::GetInstance().LoadShader(shader_name);

	if (!maybe_shader) {
		return false;
	}

	pass.shader = *maybe_shader;
	return true;
}

bool OpenGlRenderer::SwitchShaderPresetOrSetDefault(const ShaderDescriptor& descriptor)
{
	assert(!descriptor.shader_name.empty());

	const auto maybe_preset = ShaderManager::GetInstance().LoadShaderPresetOrDefault(
	        descriptor);

	if (maybe_preset) {
		main_shader_preset = *maybe_preset;
		curr_shader_descriptor.preset_name = main_shader_preset.name;
		return true;
	}

	// Loading failed - use whatever default preset we got
	main_shader_preset = ShaderManager::GetInstance().GetDefaultPreset(
	        descriptor.shader_name);
	curr_shader_descriptor.preset_name.clear();
	return false;
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

	UpdateImageAdjustmentsPassUniforms();
}

void OpenGlRenderer::EnableImageAdjustments(const bool enable)
{
	enable_image_adjustments = enable;

	UpdateImageAdjustmentsPassUniforms();
}

void OpenGlRenderer::SetImageAdjustmentSettings(const ImageAdjustmentSettings& settings)
{
	image_adjustment_settings = settings;

	UpdateImageAdjustmentsPassUniforms();
}

void OpenGlRenderer::UpdateImageAdjustmentsPassUniforms()
{
	auto& pass         = GetShaderPass(ShaderPassId::ImageAdjustments);
	const auto& s      = image_adjustment_settings;
	const auto& shader = pass.shader;

	glUseProgram(shader.program_object);

	shader.SetUniform1i("INPUT_TEXTURE", 0);

	shader.SetUniform1i("COLOR_SPACE", enum_val(color_space));
	shader.SetUniform1i("ENABLE_ADJUSTMENTS", enable_image_adjustments ? 1 : 0);
	shader.SetUniform1i("COLOR_PROFILE", enum_val(s.crt_color_profile));
	shader.SetUniform1f("BRIGHTNESS", s.brightness);
	shader.SetUniform1f("CONTRAST", s.contrast);
	shader.SetUniform1f("GAMMA", s.gamma);
	shader.SetUniform1f("DIGITAL_CONTRAST", s.digital_contrast);

	constexpr auto RgbMaxValue = 255.0f;

	shader.SetUniform3f("BLACK_LEVEL_COLOR",
	                    s.black_level_color.red / RgbMaxValue,
	                    s.black_level_color.green / RgbMaxValue,
	                    s.black_level_color.blue / RgbMaxValue);

	shader.SetUniform1f("BLACK_LEVEL", s.black_level);
	shader.SetUniform1f("SATURATION", s.saturation);

	shader.SetUniform1f("COLOR_TEMPERATURE_KELVIN",
	                    static_cast<float>(s.color_temperature_kelvin));

	shader.SetUniform1f("COLOR_TEMPERATURE_LUMA_PRESERVE",
	                    s.color_temperature_luma_preserve);

	shader.SetUniform1f("RED_GAIN", s.red_gain);
	shader.SetUniform1f("GREEN_GAIN", s.green_gain);
	shader.SetUniform1f("BLUE_GAIN", s.blue_gain);
}

void OpenGlRenderer::UpdateMainShaderPassUniforms()
{
	auto& pass         = GetShaderPass(ShaderPassId::Main);
	const auto& shader = pass.shader;

	glUseProgram(shader.program_object);

	shader.SetUniform1i("INPUT_TEXTURE", 0);
	shader.SetUniform2f("INPUT_TEXTURE_SIZE",
	                    static_cast<GLfloat>(input_texture.width),
	                    static_cast<GLfloat>(input_texture.height));

	shader.SetUniform2f("OUTPUT_TEXTURE_SIZE",
	                    pass.viewport.w,
	                    pass.viewport.h);

	for (const auto& [uniform_name, value] : main_shader_preset.params) {
		shader.SetUniform1f(uniform_name, value);
	}
}

ShaderInfo OpenGlRenderer::GetCurrentShaderInfo()
{
	const auto& pass = GetShaderPass(ShaderPassId::Main);
	return pass.shader.info;
}

ShaderPreset OpenGlRenderer::GetCurrentShaderPreset()
{
	return main_shader_preset;
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
