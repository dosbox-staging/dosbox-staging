#version 330 core

// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2026-2026 MagnusPerotinus
// SPDX-FileCopyrightText:  2016-2026 Mattias Gustavsson (crtemu), Amiberry team (1084 variant)
// SPDX-License-Identifier: MIT
//
// Phosphor persistence: the new frame or the decaying, blurred afterglow of the
// previous frames, whichever is brighter. Read back in the next frame.
//
// Pre-pass of the 'crt/crt-1084' shader; ported from the CRTEMU_TYPE_1084
// pipeline of crtemu.h in Amiberry (MIT / public domain dual license).

/*
#pragma name Crt1084_Accumulate
#pragma input0 Original
#pragma input1 Previous
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
uniform sampler2D INPUT_TEXTURE_1;

uniform float PHOSPHOR_PERSISTENCE;

void main()
{
	vec4 current  = texture(INPUT_TEXTURE_0, v_texCoord);
	vec4 previous = texture(INPUT_TEXTURE_1, v_texCoord);

	FragColor = max(current, previous * PHOSPHOR_PERSISTENCE);
}

#endif
