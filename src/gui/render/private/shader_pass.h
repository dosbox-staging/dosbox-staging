// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SHADER_PASS_H
#define DOSBOX_SHADER_PASS_H

#include <optional>
#include <string>

#include "gui/private/shader_manager.h"

#include "utils/rect.h"

// Glad must be included before SDL
#include "glad/gl.h"

struct Shader {
	ShaderInfo info       = {};
	GLuint program_object = 0;
};

enum class ShaderPassId { ImageAdjustments, Main };

struct ShaderPass {
	ShaderPassId id = {};

	Shader shader = {};

	GLuint in_texture  = 0;
	GLuint out_fbo     = 0;
	GLuint out_texture = 0;

	DosBox::Rect viewport = {};

	/*
	 * Build an OpenGL shader program.
	 *
	 * Input GLSL source must contain both vertex and fragment stages inside
	 * their respective preprocessor definitions.
	 *
	 * Returns a ready to use OpenGL shader program on success.
	 */
	bool BuildShaderProgram(const std::string& source);

	void SetUniform1i(const std::string& name, const int val);

	void SetUniform1f(const std::string& name, const float val);

	void SetUniform2f(const std::string& name, const float val1,
	                  const float val2);

	void SetUniform3f(const std::string& name, const float val1,
	                  const float val2, const float val3);

private:
	std::optional<GLuint> BuildShader(const GLenum type,
	                                  const std::string& source);

	GLint GetUniformLocation(const std::string& name);
};

#endif // DOSBOX_SHADER_PASS_H
