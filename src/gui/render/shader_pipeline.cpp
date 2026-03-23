// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/shader_pipeline.h"

#include "gui/render/private/shader_manager.h"

#include "misc/support.h"
#include "utils/checks.h"
#include "utils/math_utils.h"

// Glad must be included before SDL
#include "glad/gl.h"

// must be included after dosbox_config.h
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

CHECK_NARROWING();

std::string ShaderPass::ToString() const
{
	return format_str(
	        "shader.info.name:        %s\n"
	        "shader.info.pass_name:   %s\n"
	        "shader.info.input_ids:   %s\n"
	        "shader.info.output_size: %s\n"
	        "shader.program_object:   %d\n\n"

	        "in_textures:             %s\n"
	        "out_size:                %s\n"
	        "out_fbo:                 %d\n"
	        "out_texture:             %d\n",

	        shader.info.name.c_str(),
	        shader.info.pass_name.c_str(),
	        join(shader.info.input_ids).c_str(),
	        to_string(shader.info.output_size),
	        shader.program_object,

	        to_string(in_textures).c_str(),
	        out_size.ToString().c_str(),
	        out_fbo,
	        out_texture);
}

ShaderPipeline::ShaderPipeline()
{
	CreateSamplers();

	// The pipeline will be created lazily once we've received all necessary
	// information via notifications. IsPipelineComplete() will return true
	// when we're ready to create the pipeline.
}

ShaderPipeline::~ShaderPipeline()
{
	DestroyPipeline();
	DestroySamplers();
}

void ShaderPipeline::CreateSamplers()
{
	glGenSamplers(1, &nearest_sampler);
	glSamplerParameteri(nearest_sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glSamplerParameteri(nearest_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glSamplerParameteri(nearest_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glSamplerParameteri(nearest_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	glGenSamplers(1, &linear_sampler);
	glSamplerParameteri(linear_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glSamplerParameteri(linear_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(linear_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glSamplerParameteri(linear_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
}

void ShaderPipeline::DestroySamplers()
{
	if (nearest_sampler) {
		glDeleteSamplers(1, &nearest_sampler);
		nearest_sampler = 0;
	}
	if (linear_sampler) {
		glDeleteSamplers(1, &linear_sampler);
		linear_sampler = 0;
	}
}

bool ShaderPipeline::IsPipelineComplete() const
{
	return (video_mode.width > 0 && video_mode.height > 0 &&
	        !input_texture.size.IsEmpty() && !viewport.IsEmpty() && main_shader);
}

void ShaderPipeline::NotifyViewportSizeChanged(const DosBox::Rect& new_viewport)
{
	if (viewport == new_viewport) {
		return;
	}

	viewport = new_viewport;

	if (IsPipelineComplete()) {
		DestroyPipeline();
		CreatePipeline();
	}
}

void ShaderPipeline::NotifyRenderSizeChanged(const int input_texture_width,
                                             const int input_texture_height,
                                             const GLuint new_input_texture)
{
	if ((ifloor(input_texture.size.w) == input_texture_width) &&
	    (ifloor(input_texture.size.h) == input_texture_height) &&
	    (input_texture.texture == new_input_texture)) {
		return;
	}

	input_texture.size    = {input_texture_width, input_texture_height};
	input_texture.texture = new_input_texture;

	if (IsPipelineComplete()) {
		DestroyPipeline();
		CreatePipeline();
	}
}

void ShaderPipeline::NotifyVideoModeChanged(const VideoMode& new_video_mode)
{
	if (video_mode == new_video_mode) {
		return;
	}

	video_mode = new_video_mode;

	if (IsPipelineComplete()) {
		DestroyPipeline();
		CreatePipeline();
	}
}

void ShaderPipeline::CreatePipeline()
{
	assert(IsPipelineComplete());

	LoadAndAddInternalPasses();

	SetPassOutputSizes();
	CreatePassOutputTextures();

	// Update uniforms
	UpdatePassTextureUniforms();
	UpdateMainShaderPassUniforms();
	UpdateImageAdjustmentsPassUniforms();

	if (config.dedithering_enabled) {
		UpdateDeditherPassUniforms();
	}
}

void ShaderPipeline::LoadAndAddInternalPasses()
{
	if (config.dedithering_enabled) {
		// Resize the input image (the rendered image) to the size of
		// the video mode. This can perform width and/or halving.
		LoadAndAddInternalPassOrExit("integer-downscale");

		// Checkerboard dedither
		LoadAndAddInternalPassOrExit("checkerboard-dedither-linearize");
		LoadAndAddInternalPassOrExit("checkerboard-dedither-pass1");
		LoadAndAddInternalPassOrExit("checkerboard-dedither-pass2");
		LoadAndAddInternalPassOrExit("checkerboard-dedither-pass3");
	}

	// The image adjustments pass is always the first; it cannot be disabled
	// as it performs the colour space transforms as well.
	LoadAndAddInternalPassOrExit("image-adjustments");

	if (config.dedithering_enabled) {
		// Resize the image to the size of the rendered imge. This can
		// perform width and/or doubling.
		LoadAndAddInternalPassOrExit("integer-upscale");
	}

	// Main shader pass (e.g., a CRT shader, the sharp shader, an upscaler,
	// etc.)
	ShaderPass main_pass1            = {};
	main_pass1.shader.info.pass_name = "Main_Pass1";

	assert(main_shader);
	main_pass1.shader = *main_shader;

	shader_passes.push_back(main_pass1);
}

void ShaderPipeline::SetPassOutputSizes()
{
	for (auto it = shader_passes.begin(); it != shader_passes.end(); ++it) {
		auto& pass = *it;

		auto [width, height] = [&]() -> std::pair<float, float> {
			using enum ShaderOutputSize;
			switch (pass.shader.info.output_size) {
			case Previous: {
				const auto [_, size] = GetPreviousPassOutputTexture(it);
				return {size.w, size.h};
			}

			case Rendered:
				return {input_texture.size.w, input_texture.size.h};

			case VideoMode:
				return {video_mode.width, video_mode.height};

			case Viewport: return {viewport.w, viewport.h};

			default:
				assertm(false, "Invalid ShaderOutputSize value");
				return {};
			}
		}();

		if (std::next(it) == shader_passes.end()) {
			// The last pass is rendered directly to the window's
			// framebuffer
			pass.out_size = viewport;
		} else {
			pass.out_size = {width, height};
		}
	}
}

void ShaderPipeline::CreatePassOutputTextures()
{
	for (auto it = shader_passes.begin(); it != shader_passes.end(); ++it) {
		auto& pass = *it;

		// The last pass is rendered directly to the window's
		// framebuffer, so we don't need to create an output texture for
		// it
		if (std::next(it) != shader_passes.end()) {

			// Create output texture
			const auto& preset = pass.shader.info.default_preset;

			pass.out_texture = CreateTexture(pass.out_size,
			                                 preset.settings.float_output_texture);

			// Set up off-screen framebuffer
			glGenFramebuffers(1, &pass.out_fbo);
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
		}
	}
}

void ShaderPipeline::LoadAndAddInternalPassOrExit(const std::string& shader_name)
{
	const auto path = std_fs::path("_internal") / shader_name;

	const auto maybe_shader = ShaderManager::GetInstance().LoadShader(
	        path.string());

	if (!maybe_shader) {
		E_Exit("OPENGL: Cannot load shader pass '%s' shader, exiting",
		       path.string().c_str());
	}

	ShaderPass pass = {};
	pass.shader     = *maybe_shader;

	shader_passes.push_back(pass);
}

void ShaderPipeline::DestroyPipeline()
{
	for (auto& pass : shader_passes) {
		if (pass.out_texture != 0) {
			glDeleteTextures(1, &pass.out_texture);
			pass.out_texture = 0;
		}
		if (pass.out_fbo != 0) {
			glDeleteFramebuffers(1, &pass.out_fbo);
		}
	}

	shader_passes.clear();
}

GLuint ShaderPipeline::CreateTexture(const DosBox::Rect& size,
                                     const bool float_texture) const
{
	GLuint texture = 0;

	glGenTextures(1, &texture);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	const auto internal_format = (float_texture ? GL_RGBA32F : GL_RGBA8);
	const auto pixel_data_type = (float_texture ? GL_FLOAT : GL_UNSIGNED_BYTE);

	glTexImage2D(GL_TEXTURE_2D,
	             0, // mimap level (0 = base image)
	             internal_format,
	             static_cast<GLsizei>(size.w), // width
	             static_cast<GLsizei>(size.h), // height
	             0,                            // border (must be always 0)
	             GL_BGRA,                      // pixel data format
	             pixel_data_type,
	             nullptr // pointer to image data
	);

	glBindTexture(GL_TEXTURE_2D, 0);

	return texture;
}

void ShaderPipeline::SetMainShader(const Shader& shader)
{
	main_shader        = shader;
	main_shader_preset = shader.info.default_preset;

	if (IsPipelineComplete()) {
		DestroyPipeline();
		CreatePipeline();
	}
}

void ShaderPipeline::SetMainShaderPreset(const ShaderPreset& preset)
{
	main_shader_preset = preset;

	if (IsPipelineComplete()) {
		UpdateMainShaderPassUniforms();
	}
}

void ShaderPipeline::SetColorSpace(const ColorSpace _color_space)
{
	color_space = _color_space;

	if (IsPipelineComplete()) {
		UpdateImageAdjustmentsPassUniforms();
	}
}

void ShaderPipeline::EnableImageAdjustments(const bool enable)
{
	enable_image_adjustments = enable;

	if (IsPipelineComplete()) {
		UpdateImageAdjustmentsPassUniforms();
	}
}

void ShaderPipeline::SetImageAdjustmentSettings(const ImageAdjustmentSettings& settings)
{
	image_adjustment_settings = settings;

	if (IsPipelineComplete()) {
		UpdateImageAdjustmentsPassUniforms();
	}
}

void ShaderPipeline::SetDeditheringStrength(const float strength)
{
	dedithering_strength = strength;

	const auto enable_dedithering = (strength > 0.0f);

	if (!IsPipelineComplete()) {
		config.dedithering_enabled = enable_dedithering;
		return;
	}

	if (config.dedithering_enabled != enable_dedithering) {
		config.dedithering_enabled = enable_dedithering;

		DestroyPipeline();
		CreatePipeline();

	} else if (config.dedithering_enabled) {
		UpdateDeditherPassUniforms();
	}
}

void ShaderPipeline::Render(const GLuint vertex_array_object) const
{
	assert(IsPipelineComplete());

	for (const auto& pass : shader_passes) {
		RenderPass(pass, vertex_array_object);
	}
}

ShaderPass& ShaderPipeline::GetShaderPass(const std::string& name)
{
	for (auto& pass : shader_passes) {
		if (pass.shader.info.pass_name == name) {
			return pass;
		}
	}
	assertm(false, "Invalid shader pass ID");

	// Just to make the compiler happy
	return *shader_passes.begin();
}

void ShaderPipeline::RenderPass(const ShaderPass& pass,
                                const GLuint vertex_array_object) const
{
	glUseProgram(pass.shader.program_object);

	glBindFramebuffer(GL_FRAMEBUFFER, pass.out_fbo);
	glClear(GL_COLOR_BUFFER_BIT);

	for (size_t i = 0; i < pass.in_textures.size(); ++i) {
		glActiveTexture(GL_TEXTURE0 + check_cast<GLenum>(i));
		glBindTexture(GL_TEXTURE_2D, pass.in_textures[i]);

		// Bind sampler with the requested filter mode to the texture unit
		const auto& preset = pass.shader.info.default_preset;

		const auto texture_unit = check_cast<GLuint>(i);

		switch (preset.settings.texture_filter_mode) {
		case TextureFilterMode::NearestNeighbour:
			glBindSampler(texture_unit, nearest_sampler);
			break;

		case TextureFilterMode::Bilinear:
			glBindSampler(texture_unit, linear_sampler);
			break;

		default: assertm(false, "Invalid TextureFilterMode");
		}
	}

	// Set up viewport
	glViewport(static_cast<GLsizei>(pass.out_size.x),
	           static_cast<GLsizei>(pass.out_size.y),
	           static_cast<GLsizei>(pass.out_size.w),
	           static_cast<GLsizei>(pass.out_size.h));

	// Apply shader by drawing an oversized triangle
	glBindVertexArray(vertex_array_object);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// Reset binds
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
}

void ShaderPipeline::UpdateTextureUniforms(const std::vector<ShaderPass>::iterator pass) const
{
	const auto shader = pass->shader;

	glUseProgram(shader.program_object);

	pass->in_textures.clear();

	for (size_t i = 0; i < shader.info.input_ids.size(); ++i) {
		const auto pass_id = shader.info.input_ids[i];

		GLuint in_texture            = 0;
		DosBox::Rect in_texture_size = {};

		if (pass_id == "Previous") {
			const auto [texture,
			            size] = GetPreviousPassOutputTexture(pass);

			in_texture      = texture;
			in_texture_size = size;

		} else {
			// Only specifying the outputs of previous passes is
			// valid as input for the current pass
			bool found = false;

			auto it = pass;
			while (it != shader_passes.begin()) {
				it = std::prev(it);

				const auto& p = *it;
				if (p.shader.info.pass_name == pass_id) {
					in_texture      = p.out_texture;
					in_texture_size = p.out_size;

					found = true;
					break;
				}
			}
			if (!found) {
				LOG_ERR("RENDER: Invalid shader pass ID for input texture %zu: %s",
				        i,
				        pass_id.c_str());
				return;
			}
		}

		const auto in_texture_name = format_str("INPUT_TEXTURE_%d", i);
		pass->shader.SetUniform1i(in_texture_name, check_cast<GLint>(i));

		const auto in_texture_size_name = format_str("INPUT_SIZE_%d",
		                                             i);

		pass->shader.SetUniform2f(in_texture_size_name,
		                          in_texture_size.w,
		                          in_texture_size.h);

		pass->in_textures.emplace_back(in_texture);
	}

	shader.SetUniform2f("OUTPUT_SIZE",
	                    pass->out_size.w,
	                    pass->out_size.h);
}

void ShaderPipeline::UpdatePassTextureUniforms()
{
	for (auto it = shader_passes.begin(); it != shader_passes.end(); ++it) {
		UpdateTextureUniforms(it);
	}
}

std::pair<GLuint, DosBox::Rect> ShaderPipeline::GetPreviousPassOutputTexture(
        const std::vector<ShaderPass>::iterator pass) const
{
	if (pass == shader_passes.begin()) {
		return {input_texture.texture, input_texture.size};

	} else {
		const auto prev_pass = std::prev(pass);

		return {prev_pass->out_texture, prev_pass->out_size};
	}
}

void ShaderPipeline::UpdateMainShaderPassUniforms()
{
	const auto& pass   = GetShaderPass("Main_Pass1");
	const auto& shader = pass.shader;

	glUseProgram(shader.program_object);

	for (const auto& [uniform_name, value] : main_shader_preset.params) {
		shader.SetUniform1f(uniform_name, value);
	}
}

void ShaderPipeline::UpdateImageAdjustmentsPassUniforms()
{
	const auto& pass   = GetShaderPass("ImageAdjustments");
	const auto& s      = image_adjustment_settings;
	const auto& shader = pass.shader;

	glUseProgram(shader.program_object);

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

	shader.SetUniform1f("COLOR_TEMPERATURE_KELVIN", s.color_temperature_kelvin);

	shader.SetUniform1f("COLOR_TEMPERATURE_LUMA_PRESERVE",
	                    s.color_temperature_luma_preserve);

	shader.SetUniform1f("RED_GAIN", s.red_gain);
	shader.SetUniform1f("GREEN_GAIN", s.green_gain);
	shader.SetUniform1f("BLUE_GAIN", s.blue_gain);
}

void ShaderPipeline::UpdateDeditherPassUniforms()
{
	static std::vector<std::string> names = {"CheckerboardDedither_Linearize",
	                                         "CheckerboardDedither_Pass1",
	                                         "CheckerboardDedither_Pass2",
	                                         "CheckerboardDedither_Pass3"};

	for (const auto& name : names) {
		const auto& pass   = GetShaderPass(name);
		const auto& shader = pass.shader;

		glUseProgram(shader.program_object);

		// Always on (we set the strength to zero, or remove the
		// dedithering passes from the pipeline to disable it)
		//
		shader.SetUniform1f("CD_BLEND_OPTION", 1.0f);

		// Enable gamma correction to ensure correct luminosity of the
		// dedithered areas (one way to test this is to A/B compare the
		// original and deithered images while squinting; the dithered
		// areas should have the same perceptual brightness in both
		// images).
		//
		shader.SetUniform1f("CD_USE_GAMMA", 1.0f);

		// We just use the blend factor to set the dedithering "strength".
		shader.SetUniform1f("CD_BLEND_LEVEL", dedithering_strength);

		// These options yield the best results for typical dither
		// patterns in DOS games. Enabling CD_MITIG_LINES is important
		// to ensure sharp 1px text and 45-degree diagonals. Raising
		// CD_MITIG_NEIGHBOURS above 1.0 only results in more stray
		// pixels in non-checkerboard dither patterns, so 1.0 is ideal.
		//
		shader.SetUniform1f("CD_MITIG_NEIGHBOURS", 1.0f);
		shader.SetUniform1f("CD_MITIG_LINES", 1.0f);

		// Always off (this is a diagnostic option to show the
		// only dedithering mask).
		shader.SetUniform1f("CD_ADJUST_VIEW", 0.0f);
	}
}
