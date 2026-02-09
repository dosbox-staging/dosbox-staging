// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/shader_pipeline.h"

#include "gui/render/private/shader_manager.h"
#include "utils/checks.h"

// Glad must be included before SDL
#include "glad/gl.h"

// must be included after dosbox_config.h
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_syswm.h>

CHECK_NARROWING();

ShaderPipeline::ShaderPipeline()
{
	// The image adjustments pass is always the first; it cannot be disabled
	// as it performs the colour space transforms as well.
	LoadInternalShaderPassOrExit("image-adjustments-pass",
	                             {.id = ShaderPassId::ImageAdjustments});

	// Main shader pass (e.g., a CRT shader, the sharp shader, an upscaler,
	// etc.)
	ShaderPass pass = {};

	pass.id               = ShaderPassId::Main;
	pass.shader           = {}; // will be set later
	pass.out_fbo          = 0;  // last pass; doesn't have an FBO
	pass.out_texture      = 0;  // last pass; doesn't have an output texture
	pass.out_texture_size = {}; // will be set later

	shader_passes.push_back(pass);
}

void ShaderPipeline::LoadInternalShaderPassOrExit(const std::string shader_name,
                                                  const ShaderPass pass_params)
{
	const auto path = std_fs::path("_internal") / shader_name;

	const auto maybe_shader = ShaderManager::GetInstance().LoadShader(
	        path.string());

	if (!maybe_shader) {
		E_Exit("OPENGL: Cannot load shader pass '%s' shader, exiting",
		       path.string().c_str());
	}

	ShaderPass pass = pass_params;

	pass.id               = ShaderPassId::ImageAdjustments;
	pass.out_texture      = 0;  // will be set later
	pass.out_texture_size = {}; // will be set later

	glGenFramebuffers(1, &pass.out_fbo);

	pass.shader = *maybe_shader;

	shader_passes.push_back(pass);
}

ShaderPipeline::~ShaderPipeline()
{
	for (auto& pass : shader_passes) {
		if (pass.out_texture) {
			glDeleteTextures(1, &pass.out_texture);
			pass.out_texture = 0;
		}
		glDeleteFramebuffers(1, &pass.out_fbo);
	}
}

void ShaderPipeline::NotifyViewportSizeChanged(const DosBox::Rect draw_rect_px)
{
	auto& pass            = GetShaderPass(ShaderPassId::Main);
	pass.out_texture_size = draw_rect_px;

	for (auto it = shader_passes.begin(); it != shader_passes.end(); ++it) {
		UpdateTextureUniforms(it);
	}

	UpdateMainShaderPassUniforms();
}

void ShaderPipeline::NotifyRenderSizeChanged(const int input_texture_width,
                                             const int input_texture_height,
                                             const GLuint _input_texture)
{
	input_texture.width   = input_texture_width;
	input_texture.height  = input_texture_height;
	input_texture.texture = _input_texture;

	for (auto it = shader_passes.begin(); it != shader_passes.end(); ++it) {
		auto& pass = *it;

		if (pass.out_fbo != 0) {
			if (pass.out_texture) {
				glDeleteTextures(1, &pass.out_texture);
			}

			const auto& preset = pass.shader.info.default_preset;

			pass.out_texture = CreateTexture(input_texture.width,
			                                 input_texture.height,
			                                 preset.settings.texture_filter_mode);

			pass.out_texture_size = {0,
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
		}

		UpdateTextureUniforms(it);
	}

	UpdateMainShaderPassUniforms();
}

GLuint ShaderPipeline::CreateTexture(const int width, const int height,
                                     const TextureFilterMode filter_mode) const
{
	GLuint texture = 0;

	glGenTextures(1, &texture);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	SetTextureFiltering(texture, filter_mode);

	glTexImage2D(GL_TEXTURE_2D,
	             0,         // mimap level (0 = base image)
	             GL_RGB32F, // internal format
	             width,     // width
	             height,    // height
	             0,         // border (must be always 0)
	             GL_BGRA,   // pixel data format
	             GL_FLOAT,  // pixel data type
	             nullptr);  // pointer to image data

	glBindTexture(GL_TEXTURE_2D, 0);

	return texture;
}

void ShaderPipeline::SetMainShader(const Shader& shader)
{
	auto& main_pass  = GetShaderPass(ShaderPassId::Main);
	main_pass.shader = shader;

	main_shader_preset = shader.info.default_preset;

	// The texture filtering mode might have changed
	const auto& pass = GetShaderPass(ShaderPassId::ImageAdjustments);

	SetTextureFiltering(pass.out_texture,
	                    main_shader_preset.settings.texture_filter_mode);

	UpdateMainShaderPassUniforms();
}

void ShaderPipeline::SetMainShaderPreset(const ShaderPreset& preset)
{
	main_shader_preset = preset;
}

void ShaderPipeline::SetColorSpace(const ColorSpace _color_space)
{
	color_space = _color_space;
	UpdateImageAdjustmentsPassUniforms();
}

void ShaderPipeline::EnableImageAdjustments(const bool enable)
{
	enable_image_adjustments = enable;
	UpdateImageAdjustmentsPassUniforms();
}

void ShaderPipeline::SetImageAdjustmentSettings(const ImageAdjustmentSettings& settings)
{
	image_adjustment_settings = settings;
	UpdateImageAdjustmentsPassUniforms();
}

void ShaderPipeline::SetTextureFiltering(const GLuint texture,
                                         const TextureFilterMode filter_mode) const
{
	glBindTexture(GL_TEXTURE_2D, texture);

	const int filter_param = [&] {
		switch (filter_mode) {
		case TextureFilterMode::NearestNeighbour: return GL_NEAREST;
		case TextureFilterMode::Bilinear: return GL_LINEAR;
		default: assertm(false, "Invalid TextureFilterMode"); return 0;
		}
	}();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_param);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_param);
}

void ShaderPipeline::Render(const GLuint vertex_array_object) const
{
	for (const auto& pass : shader_passes) {
		RenderPass(pass, vertex_array_object);
	}
}

ShaderPass& ShaderPipeline::GetShaderPass(const ShaderPassId id)
{
	for (auto& pass : shader_passes) {
		if (pass.id == id) {
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
	//	LOG_TRACE("\n%s", pass.ToString().c_str());
	glUseProgram(pass.shader.program_object);

	glBindFramebuffer(GL_FRAMEBUFFER, pass.out_fbo);
	glClear(GL_COLOR_BUFFER_BIT);

	for (size_t i = 0; i < pass.in_textures.size(); ++i) {
		glActiveTexture(GL_TEXTURE0 + check_cast<GLenum>(i));
		glBindTexture(GL_TEXTURE_2D, pass.in_textures[i]);
	}

	// Set up viewport
	glViewport(static_cast<GLsizei>(pass.out_texture_size.x),
	           static_cast<GLsizei>(pass.out_texture_size.y),
	           static_cast<GLsizei>(pass.out_texture_size.w),
	           static_cast<GLsizei>(pass.out_texture_size.h));

	// Apply shader by drawing an oversized triangle
	glBindVertexArray(vertex_array_object);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// Reset binds
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
}

void ShaderPipeline::UpdateTextureUniforms(const std::list<ShaderPass>::iterator pass) const
{
	glUseProgram(pass->shader.program_object);

	pass->in_textures.clear();

	for (size_t i = 0; i < pass->in_texture_ids.size(); ++i) {
		const auto pass_id = pass->in_texture_ids[i];

		GLfloat width     = 0;
		GLfloat height    = 0;
		GLuint in_texture = 0;

		if (pass_id == ShaderPassId::Previous) {
			if (pass == shader_passes.begin()) {
				width = static_cast<GLfloat>(input_texture.width);
				height = static_cast<GLfloat>(input_texture.height);

				in_texture = input_texture.texture;

			} else {
				const auto prev_pass = std::prev(pass);

				width = static_cast<GLfloat>(
				        prev_pass->out_texture_size.w);

				height = static_cast<GLfloat>(
				        prev_pass->out_texture_size.h);

				in_texture = prev_pass->out_texture;
			}

		} else {
			// Only specifying the outputs of previous passes is
			// valid as input for the current pass
			bool found = false;

			auto it = pass;
			while (it != shader_passes.begin()) {
				it = std::prev(it);

				const auto p = *it;
				if (p.id == pass_id) {
					width = static_cast<GLfloat>(
					        p.out_texture_size.w);

					height = static_cast<GLfloat>(
					        p.out_texture_size.h);

					in_texture = p.out_texture;

					found = true;
					break;
				}
			}
			if (!found) {
				assertm(false,
				        format_str("Invalid shader pass ID for input texture %d: %d",
				                   i,
				                   pass_id));
			}
		}

		const auto input_texture_name = format_str("INPUT_TEXTURE_%d", i);
		pass->shader.SetUniform1i(input_texture_name.c_str(),
		                          check_cast<GLint>(i));

		const auto input_texture_size_name = format_str("INPUT_TEXTURE_SIZE_%d",
		                                                i);
		pass->shader.SetUniform2f(input_texture_size_name.c_str(), width, height);

		pass->in_textures.emplace_back(in_texture);
	}

	pass->shader.SetUniform2f("OUTPUT_TEXTURE_SIZE",
	                          pass->out_texture_size.w,
	                          pass->out_texture_size.h);
}

void ShaderPipeline::UpdateMainShaderPassUniforms()
{
	auto& pass         = GetShaderPass(ShaderPassId::Main);
	const auto& shader = pass.shader;

	glUseProgram(shader.program_object);

	for (const auto& [uniform_name, value] : main_shader_preset.params) {
		shader.SetUniform1f(uniform_name, value);
	}
}

void ShaderPipeline::UpdateImageAdjustmentsPassUniforms()
{
	auto& pass         = GetShaderPass(ShaderPassId::ImageAdjustments);
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

	shader.SetUniform1f("COLOR_TEMPERATURE_KELVIN",
	                    static_cast<float>(s.color_temperature_kelvin));

	shader.SetUniform1f("COLOR_TEMPERATURE_LUMA_PRESERVE",
	                    s.color_temperature_luma_preserve);

	shader.SetUniform1f("RED_GAIN", s.red_gain);
	shader.SetUniform1f("GREEN_GAIN", s.green_gain);
	shader.SetUniform1f("BLUE_GAIN", s.blue_gain);
}
