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
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 8);
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
		LOG_WARNING("OPENGL: Could not initialise debug context");
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
	glEnable(GL_BLEND);

	shader_pipeline = std::make_unique<ShaderPipeline>();

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
	const auto curr_descriptor = shader_switcher.GetCurrentShaderDescriptor();

	shader_switcher.NotifyRenderParametersChanged(canvas_size_px, video_mode);

	const auto new_descriptor = shader_switcher.GetCurrentShaderDescriptor();

	MaybeSwitchShaderAndPreset(curr_descriptor, new_descriptor);

	shader_pipeline->NotifyViewportSizeChanged(draw_rect_px);
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
	// `MaybeSwitchShaderAndPreset()` calling `UpdateRenderSize()` with zero
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

	shader_pipeline->NotifyRenderSizeChanged(input_texture.width,
	                                         input_texture.height,
	                                         input_texture.texture);
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
	             GL_RGBA8,             // internal format
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

void OpenGlRenderer::PresentFrame()
{
	shader_pipeline->Render(vao);

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

OpenGlRenderer::SetShaderResult OpenGlRenderer::SetShader(const std::string& shader_descriptor)
{
	return SetShaderInternal(shader_descriptor);
}

OpenGlRenderer::SetShaderResult OpenGlRenderer::SetShaderInternal(
        const std::string& shader_descriptor, const bool force_reload)
{
	using enum OpenGlRenderer::SetShaderResult;

	auto& shader_switcher = AutoShaderSwitcher::GetInstance();

	const auto curr_descriptor = force_reload
	                                   ? ShaderDescriptor{"", ""}
	                                   : shader_switcher.GetCurrentShaderDescriptor();

	shader_switcher.NotifyShaderChanged(shader_descriptor);

	const auto new_descriptor = shader_switcher.GetCurrentShaderDescriptor();

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
	const auto& current_shader_info = GetCurrentShaderInfo();
	const auto descriptor =
	        AutoShaderSwitcher::GetInstance().GetCurrentShaderDescriptor();

	constexpr auto ForceReload = true;
	return (SetShaderInternal(current_shader_descriptor_string, ForceReload) ==
	        OpenGlRenderer::SetShaderResult::Ok);
}

void OpenGlRenderer::NotifyVideoModeChanged(const VideoMode& video_mode)
{
	const auto canvas_size_px = GetCanvasSizeInPixels();

	// We always expect a valid canvas and DOS video mode
	assert(!canvas_size_px.IsEmpty());
	assert(video_mode.width > 0 && video_mode.height > 0);

	// Handle shader auto-switching
	auto& shader_switcher = AutoShaderSwitcher::GetInstance();
	const auto curr_descriptor = shader_switcher.GetCurrentShaderDescriptor();

	shader_switcher.NotifyRenderParametersChanged(canvas_size_px, video_mode);

	const auto new_descriptor = shader_switcher.GetCurrentShaderDescriptor();

	MaybeSwitchShaderAndPreset(curr_descriptor, new_descriptor);

	shader_pipeline->NotifyVideoModeChanged(video_mode);
}

bool OpenGlRenderer::MaybeSwitchShaderAndPreset(const ShaderDescriptor& curr_descriptor,
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

	main_shader_preset = ShaderManager::GetInstance().LoadShaderPresetOrDefault(
	        new_descriptor);

	shader_pipeline->SetMainShaderPreset(main_shader_preset);

	current_shader_descriptor.preset_name = main_shader_preset.name;

	MaybeUpdateRenderSize(input_texture.width, input_texture.height);
	return true;
}

bool OpenGlRenderer::SwitchShader(const std::string& shader_name)
{
	const auto maybe_shader = ShaderManager::GetInstance().LoadShader(shader_name);
	if (!maybe_shader) {
		return false;
	}

	const auto shader = *maybe_shader;
	main_shader_info  = shader.info;

	shader_pipeline->SetMainShader(shader);
	return true;
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

void OpenGlRenderer::SetColorSpace(const ColorSpace color_space)
{
	shader_pipeline->SetColorSpace(color_space);
}

void OpenGlRenderer::EnableImageAdjustments(const bool enable)
{
	shader_pipeline->EnableImageAdjustments(enable);
}

void OpenGlRenderer::SetImageAdjustmentSettings(const ImageAdjustmentSettings& settings)
{
	shader_pipeline->SetImageAdjustmentSettings(settings);
}

void OpenGlRenderer::SetDeditheringStrength(const float strength)
{
	shader_pipeline->SetDeditheringStrength(strength);
}

ShaderInfo OpenGlRenderer::GetCurrentShaderInfo()
{
	return main_shader_info;
}

ShaderPreset OpenGlRenderer::GetCurrentShaderPreset()
{
	return main_shader_preset;
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
