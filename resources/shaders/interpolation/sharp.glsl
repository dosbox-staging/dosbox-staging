#version 330 core

// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2020-2020 Hyllian <sergiogdb@gmail.com>
// SPDX-FileCopyrightText:  2020-2020 jmarsh <jmarsh@vogons.org>
// SPDX-License-Identifier: GPL-2.0-or-later

/*

#pragma force_single_scan
#pragma force_no_pixel_doubling

*/

#if defined(VERTEX)

layout(location = 0) in vec2 a_position;

out vec2 v_texCoord;
out vec2 prescale;

uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;

void main()
{
	gl_Position = vec4(a_position, 0.0, 1.0);
	v_texCoord = vec2(a_position.x + 1.0, a_position.y + 1.0) / 2.0 * rubyInputSize;

	prescale = ceil(rubyOutputSize / rubyInputSize);
}

#elif defined(FRAGMENT)

in vec2 v_texCoord;
in vec2 prescale;

out vec4 FragColor;

uniform vec2 rubyInputSize;
uniform vec2 rubyTextureSize;
uniform sampler2D rubyTexture;

#define GAMMA             2.2
#define GAMMA_IN(color)   pow(color, vec4(GAMMA))
#define GAMMA_OUT(color)  pow(color, vec4(1.0 / GAMMA))

vec4 texture_linear(in sampler2D sampler, in vec2 uv)
{
	// subtract 0.5 here and add it again after the floor to centre the texel
	vec2 texCoord = uv * rubyTextureSize - vec2(0.5);

	vec2 s0t0 = floor(texCoord) + vec2(0.5);
	vec2 s0t1 = s0t0 + vec2(0.0, 1.0);
	vec2 s1t0 = s0t0 + vec2(1.0, 0.0);
	vec2 s1t1 = s0t0 + vec2(1.0);

	vec2 invTexSize = 1.0 / rubyTextureSize;

	vec4 c_s0t0 = GAMMA_IN(texture(sampler, s0t0 * invTexSize));
	vec4 c_s0t1 = GAMMA_IN(texture(sampler, s0t1 * invTexSize));
	vec4 c_s1t0 = GAMMA_IN(texture(sampler, s1t0 * invTexSize));
	vec4 c_s1t1 = GAMMA_IN(texture(sampler, s1t1 * invTexSize));

	vec2 weight = fract(texCoord);

	vec4 c0 = c_s0t0 + (c_s1t0 - c_s0t0) * weight.x;
	vec4 c1 = c_s0t1 + (c_s1t1 - c_s0t1) * weight.x;

	return (c0 + (c1 - c0) * weight.y);
}

void main() {
	const vec2 halfp   = vec2(0.5);
	vec2 texel_floored = floor(v_texCoord);
	vec2 s             = fract(v_texCoord);
	vec2 region_range  = halfp - halfp / prescale;

	vec2 center_dist = s - halfp;
	vec2 f = (center_dist - clamp(center_dist, -region_range, region_range)) *
	                 prescale + halfp;

	vec2 mod_texel = min(texel_floored + f, rubyInputSize - halfp);

	FragColor = GAMMA_OUT(texture_linear(rubyTexture, mod_texel / rubyTextureSize));
}

#endif
