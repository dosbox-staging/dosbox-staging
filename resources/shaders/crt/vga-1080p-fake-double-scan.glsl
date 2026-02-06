#version 330 core

// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: MIT

/*

#pragma force_single_scan

#pragma parameter PHOSPHOR_LAYOUT "Phosphor Layout" 2.0 0.0 19.0 1.0
#pragma parameter SCANLINE_STRENGTH_MIN "Scanline Strength Min" 0.80 0.0 1.0 0.80
#pragma parameter SCANLINE_STRENGTH_MAX "Scanline Strength Max" 0.85 0.0 1.0 0.85
#pragma parameter COLOR_BOOST_EVEN "Color Boost Even" 4.80 1.0 2.0 0.05
#pragma parameter COLOR_BOOST_ODD "Color Boost Odd" 1.40 1.0 2.0 0.05
#pragma parameter MASK_STRENGTH "Mask Strength" 0.10 0.0 1.0 0.1
#pragma parameter GAMMA_INPUT "Gamma Input" 2.4 0.0 5.0 0.1
#pragma parameter GAMMA_OUTPUT "Gamma Output" 2.62 0.0 5.0 0.1

*/

/////////////////////////////////////////////////////////////////////////////

#if defined(VERTEX)

layout (location = 0) in vec2 a_position;

out vec2 v_texCoord;
out vec2 prescale;

uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

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

uniform float PHOSPHOR_LAYOUT;
uniform float SCANLINE_STRENGTH_MIN;
uniform float SCANLINE_STRENGTH_MAX;
uniform float COLOR_BOOST_EVEN;
uniform float COLOR_BOOST_ODD;
uniform float MASK_STRENGTH;
uniform float GAMMA_INPUT;
uniform float GAMMA_OUTPUT;

#define GAMMA_IN(color) pow(color, vec4(GAMMA_INPUT))

// Macro for weights computing
#define WEIGHT(w) \
	if (w > 1.0) \
		w = 1.0; \
	w = 1.0 - w * w; \
	w = w * w;

vec3 mask_weights(vec2 coord, float mask_intensity, int phosphor_layout)
{
	vec3 weights = vec3(1., 1., 1.);
	float on     = 1.;
	float off    = 1. - mask_intensity;
	vec3 green   = vec3(off, on, off);
	vec3 magenta = vec3(on, off, on);

	// This pattern is used by a few layouts, so we'll define it here
	vec3 aperture_weights = mix(magenta, green, floor(mod(coord.x, 2.0)));

	if (phosphor_layout == 0) {
		return weights;

	} else if (phosphor_layout == 1) {
		// classic aperture for RGB panels; good for 1080p, too small
		// for 4K+ aka aperture_1_2_bgr
		weights = aperture_weights;
		return weights;

	} else if (phosphor_layout == 2) {
		// 2x2 shadow mask for RGB panels; good for 1080p, too small for
		// 4K+ aka delta_1_2x1_bgr
		vec3 inverse_aperture = mix(green, magenta, floor(mod(coord.x, 2.0)));
		weights = mix(aperture_weights,
		              inverse_aperture,
		              floor(mod(coord.y, 2.0)));
		return weights;

	} else {
		return weights;
	}
}

vec4 add_vga_overlay(vec4 color, float scanlineStrengthMin,
                     float scanlineStrengthMax, float color_boost_even,
                     float color_boost_odd, float mask_strength)
{
	// scanlines
	vec2 mask_coords = gl_FragCoord.xy;

	vec3 lum_factors = vec3(0.2126, 0.7152, 0.0722);
	float luminance  = dot(lum_factors, color.rgb);

	float even_odd = floor(mod(mask_coords.y, 2.0));

	float dim_factor = mix(1.0 - scanlineStrengthMax,
	                       1.0 - scanlineStrengthMin,
	                       luminance);

	float scanline_dim = clamp(even_odd + dim_factor, 0.0, 1.0);

	color.rgb *= vec3(scanline_dim);

	// color boost
	color.rgb *= mix(vec3(color_boost_even), vec3(color_boost_odd), even_odd);

	float saturation = mix(1.2, 1.03, even_odd);

	float l = length(color);
	color.r = pow(color.r + 1e-7, saturation);
	color.g = pow(color.g + 1e-7, saturation);
	color.b = pow(color.b + 1e-7, saturation);
	color   = normalize(color) * l;

	// mask
	color.rgb *= mask_weights(mask_coords, mask_strength, int(PHOSPHOR_LAYOUT));
	return color;
}

vec4 tex2D_linear(in sampler2D sampler, in vec2 uv)
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

void main()
{
	const vec2 halfp   = vec2(0.5);
	vec2 texel_floored = floor(v_texCoord);
	vec2 s             = fract(v_texCoord);
	vec2 region_range  = halfp - halfp / prescale;

	vec2 center_dist = s - halfp;

	vec2 f = (center_dist - clamp(center_dist, -region_range, region_range)) *
	                 prescale +
	         halfp;

	vec2 mod_texel = min(texel_floored + f, rubyInputSize - halfp);
	vec4 color     = tex2D_linear(rubyTexture, mod_texel / rubyTextureSize);

	color = add_vga_overlay(color,
	                        SCANLINE_STRENGTH_MIN,
	                        SCANLINE_STRENGTH_MAX,
	                        COLOR_BOOST_EVEN,
	                        COLOR_BOOST_ODD,
	                        MASK_STRENGTH);

	color = pow(color, vec4(1.0 / GAMMA_OUTPUT));

	FragColor = clamp(color, 0.0, 1.0);
}

#endif
