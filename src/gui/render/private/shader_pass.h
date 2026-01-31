// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SHADER_PASS_H
#define DOSBOX_SHADER_PASS_H

#include <optional>
#include <string>

#include "shader.h"
#include "utils/rect.h"

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
};

#endif // DOSBOX_SHADER_PASS_H
