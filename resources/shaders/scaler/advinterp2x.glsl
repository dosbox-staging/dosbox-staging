#version 330 core

// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2004-2020 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

/*

#pragma force_single_scan
#pragma force_no_pixel_doubling

*/

#if defined(VERTEX)

layout (location = 0) in vec2 a_position;

out vec2 v_texCoord;

uniform vec2 rubyInputSize;

void main()
{
	gl_Position = vec4(a_position, 0.0, 1.0);
	v_texCoord = vec2(a_position.x + 1.0, a_position.y + 1.0) * rubyInputSize;
}

#elif defined(FRAGMENT)

in vec2 v_texCoord;

out vec4 FragColor;

uniform vec2 rubyTextureSize;
uniform sampler2D rubyTexture;

vec3 getadvinterp2xtexel(vec2 coord)
{
	vec2 base = floor(coord / vec2(2.0)) + vec2(0.5);
	vec3 c4   = texture(rubyTexture, base / rubyTextureSize).xyz;

	vec3 c1 = texture(rubyTexture, (base - vec2(0.0, 1.0)) / rubyTextureSize).xyz;
	vec3 c7 = texture(rubyTexture, (base + vec2(0.0, 1.0)) / rubyTextureSize).xyz;
	vec3 c3 = texture(rubyTexture, (base - vec2(1.0, 0.0)) / rubyTextureSize).xyz;
	vec3 c5 = texture(rubyTexture, (base + vec2(1.0, 0.0)) / rubyTextureSize).xyz;

	bool outer = c1 != c7 && c3 != c5;
	bool c3c1  = outer && c3 == c1;
	bool c1c5  = outer && c1 == c5;
	bool c3c7  = outer && c3 == c7;
	bool c7c5  = outer && c7 == c5;

	vec3 l00 = mix(c3, c4, c3c1 ? 3.0 / 8.0 : 1.0);
	vec3 l01 = mix(c5, c4, c1c5 ? 3.0 / 8.0 : 1.0);
	vec3 l10 = mix(c3, c4, c3c7 ? 3.0 / 8.0 : 1.0);
	vec3 l11 = mix(c5, c4, c7c5 ? 3.0 / 8.0 : 1.0);

	coord = max(floor(mod(coord, 2.0)), 0.0);
	/* 2x2 output:
	 *    |x=0|x=1
	 * y=0|l00|l01
	 * y=1|l10|l11
	 */

	return mix(mix(l00, l01, coord.x), mix(l10, l11, coord.x), coord.y);
}

void main()
{
	vec2 coord = v_texCoord;
	coord -= 0.5;

	vec3 c0 = getadvinterp2xtexel(coord);
	vec3 c1 = getadvinterp2xtexel(coord + vec2(1.0, 0.0));
	vec3 c2 = getadvinterp2xtexel(coord + vec2(0.0, 1.0));
	vec3 c3 = getadvinterp2xtexel(coord + vec2(1.0));

	coord = fract(max(coord, 0.0));

	FragColor = vec4(mix(mix(c0, c1, coord.x), mix(c2, c3, coord.x), coord.y), 1.0);
}

#endif
