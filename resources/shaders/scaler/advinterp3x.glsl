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

	v_texCoord = vec2(a_position.x + 1.0, a_position.y + 1.0) / 2.0 *
	             rubyInputSize * 3.0;
}

#elif defined(FRAGMENT)

in vec2 v_texCoord;

out vec4 FragColor;

uniform vec2 rubyTextureSize;
uniform sampler2D rubyTexture;

vec3 getadvinterp3xtexel(vec2 coord)
{
	vec2 base = floor(coord / vec2(3.0)) + vec2(0.5);
	vec3 c0 = texture(rubyTexture, (base - vec2(1.0, 1.0)) / rubyTextureSize).xyz;
	vec3 c1 = texture(rubyTexture, (base - vec2(0.0, 1.0)) / rubyTextureSize).xyz;
	vec3 c2 = texture(rubyTexture, (base - vec2(-1.0, 1.0)) / rubyTextureSize).xyz;
	vec3 c3 = texture(rubyTexture, (base - vec2(1.0, 0.0)) / rubyTextureSize).xyz;
	vec3 c4 = texture(rubyTexture, base / rubyTextureSize).xyz;
	vec3 c5 = texture(rubyTexture, (base + vec2(1.0, 0.0)) / rubyTextureSize).xyz;
	vec3 c6 = texture(rubyTexture, (base + vec2(-1.0, 1.0)) / rubyTextureSize).xyz;
	vec3 c7 = texture(rubyTexture, (base + vec2(0.0, 1.0)) / rubyTextureSize).xyz;
	vec3 c8 = texture(rubyTexture, (base + vec2(1.0, 1.0)) / rubyTextureSize).xyz;

	bool outer = c1 != c7 && c3 != c5;

	vec3 l00 = mix(c3, c4, (outer && c3 == c1) ? 3.0 / 8.0 : 1.0);
	vec3 l01 = (outer && ((c3 == c1 && c4 != c2) || (c5 == c1 && c4 != c0))) ? c1 : c4;
	vec3 l02 = mix(c5, c4, (outer && c5 == c1) ? 3.0 / 8.0 : 1.0);
	vec3 l10 = (outer && ((c3 == c1 && c4 != c6) || (c3 == c7 && c4 != c0))) ? c3 : c4;
	vec3 l11 = c4;
	vec3 l12 = (outer && ((c5 == c1 && c4 != c8) || (c5 == c7 && c4 != c2))) ? c5 : c4;
	vec3 l20 = mix(c3, c4, (outer && c3 == c7) ? 3.0 / 8.0 : 1.0);
	vec3 l21 = (outer && ((c3 == c7 && c4 != c8) || (c5 == c7 && c4 != c6))) ? c7 : c4;
	vec3 l22 = mix(c5, c4, (outer && c5 == c7) ? 3.0 / 8.0 : 1.0);

	coord = mod(coord, 3.0);
	bvec2 l = lessThan(coord, vec2(1.0));
	bvec2 h = greaterThanEqual(coord, vec2(2.0));

	if (h.x) {
		l01 = l02;
		l11 = l12;
		l21 = l22;
	}
	if (h.y) {
		l10 = l20;
		l11 = l21;
	}
	if (l.x) {
		l01 = l00;
		l11 = l10;
	}
	return l.y ? l01 : l11;
}

void main()
{
	vec2 coord = v_texCoord;
	coord -= 0.5;

	vec3 c0 = getadvinterp3xtexel(coord);
	vec3 c1 = getadvinterp3xtexel(coord + vec2(1.0, 0.0));
	vec3 c2 = getadvinterp3xtexel(coord + vec2(0.0, 1.0));
	vec3 c3 = getadvinterp3xtexel(coord + vec2(1.0));

	coord = fract(max(coord, 0.0));

	FragColor = vec4(mix(mix(c0, c1, coord.x), mix(c2, c3, coord.x), coord.y), 1.0);
}

#endif
