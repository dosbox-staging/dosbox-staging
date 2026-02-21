// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SHADER_H
#define DOSBOX_SHADER_H

#include <optional>
#include <string>
#include <unordered_map>

#include "shader_common.h"

#include "glad/gl.h"

struct Shader {
	ShaderInfo info       = {};
	GLuint program_object = 0;

	/*
	 * Build an OpenGL shader program.
	 *
	 * Input GLSL source must contain both vertex and fragment stages inside
	 * their respective preprocessor definitions.
	 *
	 * Returns a ready to use OpenGL shader program on success.
	 */
	bool BuildShaderProgram(const std::string& source);

	void SetUniform1i(const std::string& name, const int val) const;

	void SetUniform1f(const std::string& name, const float val) const;

	void SetUniform2f(const std::string& name, const float val1,
	                  const float val2) const;

	void SetUniform3f(const std::string& name, const float val1,
	                  const float val2, const float val3) const;

private:
	std::optional<GLuint> BuildShader(const GLenum type,
	                                  const std::string& source) const;

	GLint GetUniformLocation(const std::string& name) const;
};

#endif // DOSBOX_SHADER_H
