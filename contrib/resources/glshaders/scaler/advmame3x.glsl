#version 120

// SPDX-FileCopyrightText:  2020-2024 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2004-2020 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma force_single_scan
#pragma force_no_pixel_doubling

varying vec2 v_texCoord;
uniform sampler2D rubyTexture;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

#if defined(VERTEX)
attribute vec4 a_position;

void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize * 3.0;
}

#elif defined(FRAGMENT)

vec3 getadvmame3xtexel(vec2 coord)
{
	vec2 base = floor(coord / vec2(3.0)) + vec2(0.5);
	coord = mod(coord, 3.0);
	bvec2 l = lessThan(coord, vec2(1.0));
	bvec2 h = greaterThanEqual(coord, vec2(2.0));
	bvec2 m = equal(l, h);

	vec2 left = vec2(h.x ? 1.0 : -1.0, 0.0);
	vec2 up = vec2(0.0, h.y ? 1.0 : -1.0);
	if (l == h)
		left.x = 0.0; // hack for center pixel, will ensure outer==false
	if (m.y) {
		// swap
		left -= up;
		up += left;
		left = up - left;
	}

	vec3 c0 = texture2D(rubyTexture, (base + up + left) / rubyTextureSize).xyz;
	vec3 c1 = texture2D(rubyTexture, (base + up) / rubyTextureSize).xyz;
	vec3 c2 = texture2D(rubyTexture, (base + up - left) / rubyTextureSize).xyz;
	vec3 c3 = texture2D(rubyTexture, (base + left) / rubyTextureSize).xyz;
	vec3 c4 = texture2D(rubyTexture, base / rubyTextureSize).xyz;
	vec3 c5 = texture2D(rubyTexture, (base - left) / rubyTextureSize).xyz;
	vec3 c7 = texture2D(rubyTexture, (base - up) / rubyTextureSize).xyz;

	bool outer = c1 != c7 && c3 != c5;
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
	gl_FragColor = vec4(mix(mix(c0, c1, coord.x), mix(c2, c3, coord.x), coord.y), 1.0);
}
#endif
