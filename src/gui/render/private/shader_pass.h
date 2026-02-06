// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SHADER_PASS_H
#define DOSBOX_SHADER_PASS_H

#include <optional>
#include <string>

#include "shader.h"
#include "utils/rect.h"
#include "utils/string_utils.h"

// Glad must be included before SDL
#include "glad/gl.h"

enum class ShaderPassId { ImageAdjustments, Main };

struct ShaderPass {
	ShaderPassId id = {};

	Shader shader = {};

	GLuint in_texture  = 0;
	GLuint out_fbo     = 0;
	GLuint out_texture = 0;

	DosBox::Rect viewport = {};

	std::string ToString() const
	{
		return format_str(
		        "id:             %d\n"
		        "shader.name:    %s\n"
		        "shader.program: %d\n"
		        "in_texture:     %d\n"
		        "out_fbo:        %d\n"
		        "out_texture:    %d\n"
		        "viewport:       %s",
		        id,
		        shader.info.name.c_str(),
		        shader.program_object,
		        in_texture,
		        out_fbo,
		        out_texture,
		        viewport.ToString().c_str());
	}
};

#endif // DOSBOX_SHADER_PASS_H
