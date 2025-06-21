#version 120

// SPDX-FileCopyrightText:  2020-2024 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2020-2020 Hyllian <sergiogdb@gmail.com>
// SPDX-FileCopyrightText:  2020-2020 jmarsh <jmarsh@vogons.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma use_npot_texture
#pragma use_srgb_texture
#pragma use_srgb_framebuffer
#pragma force_single_scan
#pragma force_no_pixel_doubling

varying vec2 v_texCoord;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;
varying vec2 prescale; // const set by vertex shader

#if defined(VERTEX)

attribute vec4 a_position;

void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize;
	prescale = ceil(rubyOutputSize / rubyInputSize);
}

#elif defined(FRAGMENT)

uniform sampler2D rubyTexture;

void main()
{
	const vec2 halfp = vec2(0.5);
	vec2 texel_floored = floor(v_texCoord);
	vec2 s = fract(v_texCoord);
	vec2 region_range = halfp - halfp / prescale;

	vec2 center_dist = s - halfp;
	vec2 f = (center_dist - clamp(center_dist, -region_range, region_range)) * prescale + halfp;

	vec2 mod_texel = min(texel_floored + f, rubyInputSize - halfp);
	gl_FragColor = texture2D(rubyTexture, mod_texel / rubyTextureSize);
}

#endif
