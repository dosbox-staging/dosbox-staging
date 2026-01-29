// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/shader_pass.h"

#if C_OPENGL

bool ShaderPass::BuildShaderProgram(const std::string& shader_source)
{
	if (shader_source.empty()) {
		LOG_ERR("OPENGL: No shader source present");
		return false;
	}

	const auto maybe_vertex_shader = BuildShader(GL_VERTEX_SHADER, shader_source);
	if (!maybe_vertex_shader) {
		LOG_ERR("OPENGL: Error compiling vertex shader");
		return false;
	}
	const auto vertex_shader = *maybe_vertex_shader;

	const auto maybe_fragment_shader = BuildShader(GL_FRAGMENT_SHADER,
	                                               shader_source);
	if (!maybe_fragment_shader) {
		LOG_ERR("OPENGL: Error compiling fragment shader");
		glDeleteShader(vertex_shader);
		return false;
	}
	const auto fragment_shader = *maybe_fragment_shader;

	const GLuint shader_program = glCreateProgram();

	if (!shader_program) {
		LOG_ERR("OPENGL: Error creating shader program");
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);
		return false;
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
		return false;
	}

	shader.program_object = shader_program;
	return true;
}

std::optional<GLuint> ShaderPass::BuildShader(const GLenum type,
                                              const std::string& source)
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

GLint ShaderPass::GetUniformLocation(const std::string& name)
{
	const auto location = glGetUniformLocation(shader.program_object, name.c_str());
#ifdef DEBUG_OPENGL
	if (location == -1) {
		LOG_DEBUG("OPENGL: Error retrieving location of '%s' uniform",
		          name.c_str());
	}
#endif
	return location;
}

void ShaderPass::SetUniform1i(const std::string& name, const int val)
{
	const auto location = GetUniformLocation(name);
	if (location != -1) {
		glUniform1i(location, val);
	}
}

void ShaderPass::SetUniform1f(const std::string& name, const float val)
{
	const auto location = GetUniformLocation(name);
	if (location != -1) {
		glUniform1f(location, val);
	}
}

void ShaderPass::SetUniform2f(const std::string& name, const float val1,
                              const float val2)
{
	const auto location = GetUniformLocation(name);
	if (location != -1) {
		glUniform2f(location, val1, val2);
	}
}

void ShaderPass::SetUniform3f(const std::string& name, const float val1,
                              const float val2, const float val3)
{
	const auto location = GetUniformLocation(name);
	if (location != -1) {
		glUniform3f(location, val1, val2, val3);
	}
}

#endif
