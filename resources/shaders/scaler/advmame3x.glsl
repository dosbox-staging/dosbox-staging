#version 330 core

// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2004-2020 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

/*

#pragma name        Main_Pass1
#pragma output_size Viewport

#pragma force_single_scan
#pragma force_no_pixel_doubling

*/

#if defined(VERTEX)

layout(location = 0) in vec2 a_position;

out vec2 v_texCoord;

uniform vec2 INPUT_TEXTURE_SIZE_0;

void main()
{
	gl_Position = vec4(a_position, 0.0, 1.0);

	v_texCoord = vec2(a_position.x + 1.0, a_position.y + 1.0) / 2.0 *
	             INPUT_TEXTURE_SIZE_0 * 3.0;
}

#elif defined(FRAGMENT)

in vec2 v_texCoord;

out vec4 FragColor;

uniform vec2 INPUT_TEXTURE_SIZE_0;
uniform sampler2D INPUT_TEXTURE_0;

vec3 getadvmame3xtexel(vec2 coord)
{
	vec2 base = floor(coord / vec2(3.0)) + vec2(0.5);
	coord     = mod(coord, 3.0);
	bvec2 l   = lessThan(coord, vec2(1.0));
	bvec2 h   = greaterThanEqual(coord, vec2(2.0));
	bvec2 m   = equal(l, h);

	vec2 left = vec2(h.x ? 1.0 : -1.0, 0.0);
	vec2 up   = vec2(0.0, h.y ? 1.0 : -1.0);
	if (l == h) {
		left.x = 0.0; // hack for center pixel, will ensure outer==false
	}
	if (m.y) {
		// swap
		left -= up;
		up += left;
		left = up - left;
	}

	vec3 c0 = texture(INPUT_TEXTURE_0, (base + up + left) / INPUT_TEXTURE_SIZE_0).xyz;
	vec3 c1 = texture(INPUT_TEXTURE_0, (base + up) / INPUT_TEXTURE_SIZE_0).xyz;
	vec3 c2 = texture(INPUT_TEXTURE_0, (base + up - left) / INPUT_TEXTURE_SIZE_0).xyz;
	vec3 c3 = texture(INPUT_TEXTURE_0, (base + left) / INPUT_TEXTURE_SIZE_0).xyz;
	vec3 c4 = texture(INPUT_TEXTURE_0, base / INPUT_TEXTURE_SIZE_0).xyz;
	vec3 c5 = texture(INPUT_TEXTURE_0, (base - left) / INPUT_TEXTURE_SIZE_0).xyz;
	vec3 c7 = texture(INPUT_TEXTURE_0, (base - up) / INPUT_TEXTURE_SIZE_0).xyz;

	bool outer  = c1 != c7 && c3 != c5;
	bool check1 = c3 == c1 && (!any(m) || c4 != c2);
	bool check2 = any(m) && c5 == c1 && c4 != c0;

	return (outer && (check1 || check2)) ? c1 : c4;
}

void main()
{
	vec2 coord = v_texCoord;
	coord -= 0.5;

	vec3 c0 = getadvmame3xtexel(coord);
	vec3 c1 = getadvmame3xtexel(coord + vec2(1.0, 0.0));
	vec3 c2 = getadvmame3xtexel(coord + vec2(0.0, 1.0));
	vec3 c3 = getadvmame3xtexel(coord + vec2(1.0));

	coord = fract(max(coord, 0.0));

	FragColor = vec4(mix(mix(c0, c1, coord.x), mix(c2, c3, coord.x), coord.y), 1.0);
}

#endif
