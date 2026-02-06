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

enum class ShaderPassId {
	// Special ID that refers to the previous pass. Used to feed the output
	// texture of the previous pass as the input texture of the current pass
	// via `in_textures`.
	//
	// For the first pass, `Previous` means the rendered DOS framebuffer.
	Previous = -1,

	ImageAdjustments = 0,

	// User-selectable main shader pass. This is the what the user can set
	// with the `shader` setting (e.g., `sharp`, `crt-auto`, etc).
	Main = 1
};

struct ShaderPass {
	ShaderPassId id = {};

	Shader shader = {};

	std::vector<ShaderPassId> in_texture_ids = {ShaderPassId::Previous};
	std::vector<GLuint> in_textures          = {};

	GLuint out_fbo     = 0;
	GLuint out_texture = 0;

	DosBox::Rect viewport = {};

	std::string ToString() const
	{
		return format_str(
		        "id:             %d\n"
		        "shader.name:    %s\n"
		        "shader.program: %d\n"
		        "in_texture_ids: %s\n"
		        "in_textures:    %s\n"
		        "out_fbo:        %d\n"
		        "out_texture:    %d\n"
		        "viewport:       %s",
		        id,
		        shader.info.name.c_str(),
		        shader.program_object,
		        to_string(in_texture_ids).c_str(),
		        to_string(in_textures).c_str(),
		        out_fbo,
		        out_texture,
		        viewport.ToString().c_str());
	}
};

#endif // DOSBOX_SHADER_PASS_H
