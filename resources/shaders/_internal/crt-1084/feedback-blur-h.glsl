#version 330 core

// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2026-2026 MagnusPerotinus
// SPDX-FileCopyrightText:  2016-2026 Mattias Gustavsson (crtemu), Amiberry team (1084 variant)
// SPDX-License-Identifier: MIT
//
// Horizontal blur of the previous frame's phosphor persistence buffer.
//
// Pre-pass of the 'crt/crt-1084' shader; ported from the CRTEMU_TYPE_1084
// pipeline of crtemu.h in Amiberry (MIT / public domain dual license).

/*
#pragma name Crt1084_FeedbackBlurH
#pragma input0 Crt1084_Accumulate
*/

#if defined(VERTEX)

layout (location = 0) in vec2 a_position;

out vec2 v_texCoord;

void main()
{
	gl_Position = vec4(a_position, 0.0, 1.0);
	v_texCoord  = vec2(a_position.x + 1.0, a_position.y + 1.0) / 2.0;
}

#elif defined(FRAGMENT)

in vec2 v_texCoord;

out vec4 FragColor;

uniform sampler2D INPUT_TEXTURE_0;
uniform vec2 INPUT_SIZE_0;

// 9-tap Gaussian blur, one texel apart (the crtemu blur shader with radius 1)
void main()
{
	vec2 blur = vec2(1.0, 0.0) / INPUT_SIZE_0;

	vec4 sum = texture(INPUT_TEXTURE_0, v_texCoord) * 0.2270270270;

	sum += texture(INPUT_TEXTURE_0, v_texCoord - 4.0 * blur) * 0.0162162162;
	sum += texture(INPUT_TEXTURE_0, v_texCoord - 3.0 * blur) * 0.0540540541;
	sum += texture(INPUT_TEXTURE_0, v_texCoord - 2.0 * blur) * 0.1216216216;
	sum += texture(INPUT_TEXTURE_0, v_texCoord - 1.0 * blur) * 0.1945945946;
	sum += texture(INPUT_TEXTURE_0, v_texCoord + 1.0 * blur) * 0.1945945946;
	sum += texture(INPUT_TEXTURE_0, v_texCoord + 2.0 * blur) * 0.1216216216;
	sum += texture(INPUT_TEXTURE_0, v_texCoord + 3.0 * blur) * 0.0540540541;
	sum += texture(INPUT_TEXTURE_0, v_texCoord + 4.0 * blur) * 0.0162162162;

	FragColor = sum;
}

#endif
