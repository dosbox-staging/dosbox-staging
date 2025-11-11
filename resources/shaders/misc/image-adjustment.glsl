#version 330 core

// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: MIT
//
// Most of the code was taken from various sources with minor adjustments:
//
//  - White point mapping: Dogway, Neil Bartlett, Tanner Helland
//  - Colur space transforms & the rest: guest(r), Dr. Venom
//

/*
   TODO remove
#pragma parameter WP_ADJUST "White Point Adjustments [ OFF | ON ]" 0.0 0.0 1.0 1.0
#pragma parameter WP_TEMPERATURE "White Point" 9300.0 1000.0 12000.0 72.0
#pragma parameter WP_RED "Red Shift" 0.0 -1.0 1.0 0.01
#pragma parameter WP_GREEN "Green Shift" 0.0 -1.0 1.0 0.01
#pragma parameter WP_BLUE "Blue Shift" 0.0 -1.0 1.0 0.01

#pragma parameter BRIGHTNESS "Brightness" 1.0 0.0 2.0 0.01
#pragma parameter CONTRAST "Contrast" 0.0 -2.0 2.0 0.05
#pragma parameter SATURATION "Saturation" 1.0 0.0 2.0 0.05
#pragma parameter BLACK_LEVEL "Black Level" 0.0 -100.0 25.0 1.0
*/

#if defined(VERTEX)

layout (location = 0) in vec2 a_position;

out vec2 v_texCoord;

uniform vec2 inputSize;

void main()
{
	gl_Position = vec4(a_position, 0.0, 1.0);
	v_texCoord  = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0;
}

#elif defined(FRAGMENT)

in vec2 v_texCoord;

out vec4 FragColor;

uniform sampler2D inputTexture;

uniform float WP_ADJUST;
uniform float WP_TEMPERATURE;
uniform float WP_RED_OFFSET;
uniform float WP_GREEN_OFFSET;
uniform float WP_BLUE_OFFSET;

uniform float BRIGHTNESS;
uniform float CONTRAST;
uniform float SATURATION;
uniform float BLACK_LEVEL;

#define WP_LUMA_PRESERVE 1.0

// White Point Mapping
//          ported by Dogway
//
// From the first comment post (sRGB primaries and linear light compensated)
//      http://www.zombieprototypes.com/?p=210#comment-4695029660

// Based on the Neil Bartlett's blog update
//      http://www.zombieprototypes.com/?p=210

// Inspired itself by Tanner Helland's work
//      http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/
//
vec3 wp_adjust(vec3 color){
    float temp = WP_TEMPERATURE / 100.;
    float k = WP_TEMPERATURE / 10000.;
    float lk = log(k);

    vec3 wp = vec3(1.);

    // Calculate red component
    wp.r = (temp <= 65.)
	         ? 1.
	         : 0.32068362618584273 +
	                   (0.19668730877673762 *
	                    pow(k - 0.21298613432655075, -1.5139012907556737)) +
	                   (-0.013883432789258415 * lk);

    // Calculate green component
    float mg = 1.226916242502167 +
	       (-1.3109482654223614 * pow(k - 0.44267061967913873, 3.) *
	        exp(-5.089297600846147 * (k - 0.44267061967913873))) +
	       (0.6453936305542096 * lk);

    float pg = 0.4860175851734596 +
	       (0.1802139719519286 * pow(k - 0.14573069517701578, -1.397716496795082)) +
	       (-0.00803698899233844 * lk);

    wp.g = (temp <= 65.5) ? ((temp <= 8.) ? 0. : mg) : pg;

    // Calculate blue component
    wp.b = (temp <= 19.) ? 0.
	 : (temp >= 66.)
	         ? 1.
	         : 1.677499032830161 +
	                   (-0.02313594016938082 * pow(k - 1.1367244820333684, 3.) *
	                    exp(-4.221279555918655 * (k - 1.1367244820333684))) +
	                   (1.6550275798913296 * lk);

    // Clamp
    wp.rgb = clamp(wp.rgb, vec3(0.), vec3(1.));

    // Manual white point adjustment via RGB offsets
    wp.rgb += vec3(WP_RED_OFFSET, WP_GREEN_OFFSET, WP_BLUE_OFFSET);

    return color * wp;
}

vec3 sRGB_to_XYZ(vec3 RGB)
{
	const mat3x3 m = mat3x3(0.4124564, 0.3575761, 0.1804375,
	                        0.2126729, 0.7151522, 0.0721750,
	                        0.0193339, 0.1191920, 0.9503041);
	return RGB * m;
}

vec3 XYZtoYxy(vec3 XYZ)
{
	float XYZrgb = XYZ.r + XYZ.g + XYZ.b;
	float Yxyr   = XYZ.g;
	float Yxyg   = (XYZrgb <= 0.0) ? 0.3805 : XYZ.r / XYZrgb;
	float Yxyb   = (XYZrgb <= 0.0) ? 0.3769 : XYZ.g / XYZrgb;
	return vec3(Yxyr, Yxyg, Yxyb);
}

vec3 XYZ_to_sRGB(vec3 XYZ)
{
	const mat3x3 m = mat3x3( 3.2404542, -1.5371385, -0.4985314,
	                        -0.9692660,  1.8760108,  0.0415560,
	                         0.0556434, -0.2040259,  1.0572252);
	return XYZ * m;
}

vec3 YxytoXYZ(vec3 Yxy) {
    float Xs = Yxy.r * (Yxy.g/Yxy.b);
    float Xsz = (Yxy.r <= 0.0) ? 0 : 1;
    vec3 XYZ = vec3(Xsz,Xsz,Xsz) * vec3(Xs, Yxy.r, (Xs/Yxy.g)-Xs-Yxy.r);
    return XYZ;
}

// From guest-advanced
vec3 plant(vec3 tar, float r)
{
	float t = max(max(tar.r, tar.g), tar.b) + 0.00001;
	return tar * r / t;
}

// From guest-advanced
float contrast(float x)
{
	return max(mix(x, smoothstep(0.0, 1.0, x), CONTRAST), 0.0);
}

void main()
{
	vec3 color = texture(inputTexture, v_texCoord).rgb;

/*	// Colour temperature
	if (WP_ADJUST == 1.0) {
		vec3 wp_adjusted   = wp_adjust(color);
		vec3 base_luma     = XYZtoYxy(sRGB_to_XYZ(color));
		vec3 adjusted_luma = XYZtoYxy(sRGB_to_XYZ(wp_adjusted));

		wp_adjusted = (WP_LUMA_PRESERVE == 1.0)
		                    ? adjusted_luma + (vec3(base_luma.r, 0., 0.) -
		                                       vec3(adjusted_luma.r, 0., 0.))
		                    : adjusted_luma;

		color = XYZ_to_sRGB(YxytoXYZ(wp_adjusted));
	}

	// Saturation (from guest-advanced)
	vec3 scolor1 = plant(pow(color, vec3(SATURATION)),
	                     max(max(color.r, color.g), color.b));

	float luma   = dot(color, vec3(0.299, 0.587, 0.114));
	vec3 scolor2 = mix(vec3(luma), color, SATURATION);
	color        = (SATURATION > 1.0) ? scolor1 : scolor2;

	// Contrast
	color = plant(color, contrast(max(max(color.r, color.g), color.b)));

	// Black point
	if (BLACK_LEVEL > -0.5) {
		color = color + BLACK_LEVEL;
	} else {
		color = max(color + BLACK_LEVEL / 255.0, 0.0) /
		        (1.0 + BLACK_LEVEL / 255.0 *
		                       step(-BLACK_LEVEL / 255.0,
		                            max(max(color.r, color.g), color.b)));
	}

	color *= BRIGHTNESS; */

	FragColor = vec4(color.rgb / 1.5, 1.0);
}

#endif
