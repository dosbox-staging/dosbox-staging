#version 330 core

// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2011-2022 Hyllian <sergiogdb@gmail.com>
// SPDX-License-Identifier: MIT

// Hyllian's checkerboard dedither shader - pass1
//
// Converted to GLSL from the libretro Slang sources
//

/*

#pragma name        CheckerboardDedither_Pass1
#pragma output_size VideoMode

#pragma use_nearest_texture_filter

#pragma parameter CD_BLEND_OPTION "Checkerboard Dedither [ OFF | ON ]" 1.0  0.0 1.0  1.0
#pragma parameter CD_BLEND_LEVEL  "Blend Level"                        1.0  0.0 1.0  0.1

*/

//////////////////////////////////////////////////////////////////////////////

const vec3 Y = vec3(0.299, 0.587, 0.114);

vec3 min_s(vec3 central, vec3 adj1, vec3 adj2)
{
	return min(central, max(adj1, adj2));
}

vec3 max_s(vec3 central, vec3 adj1, vec3 adj2)
{
	return max(central, min(adj1, adj2));
}

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

uniform vec2 INPUT_TEXTURE_SIZE_0;
uniform sampler2D INPUT_TEXTURE_0;

uniform float CD_BLEND_OPTION;
uniform float CD_BLEND_LEVEL;

void main()
{
	vec2 ps = vec2(1.0) / INPUT_TEXTURE_SIZE_0.xy;

	vec2 dx = vec2(1.0, 0.0) * ps.xy;
	vec2 dy = vec2(0.0, 1.0) * ps.xy;

	// Reading the texels
	vec3 C  = texture(INPUT_TEXTURE_0, v_texCoord).xyz;
	vec3 L  = texture(INPUT_TEXTURE_0, v_texCoord - dx).xyz;
	vec3 R  = texture(INPUT_TEXTURE_0, v_texCoord + dx).xyz;
	vec3 U  = texture(INPUT_TEXTURE_0, v_texCoord - dy).xyz;
	vec3 D  = texture(INPUT_TEXTURE_0, v_texCoord + dy).xyz;

	vec3 color = C;

	if (CD_BLEND_OPTION == 1) {
		float diff = dot(max(C, max(L, R)) - min(C, min(L, R)), Y);

		diff *= (1.0 - CD_BLEND_LEVEL);

		vec3 min_sample = max(min_s(C, L, R), min_s(C, U, D));
		vec3 max_sample = min(max_s(C, L, R), max_s(C, U, D));

		color = 0.5 * (1.0 + diff) * C + 0.125 * (1.0 - diff) * (L + R + U + D);
		color = clamp(color, min_sample, max_sample);
	}

	float luma_diff = abs(dot(C, Y) - dot(color, Y));

	FragColor = vec4(color, luma_diff);
}

#endif
