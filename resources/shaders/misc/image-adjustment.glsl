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

uniform float BLACK_LEVEL;

uniform float COLORSPACE 1.0
uniform float COLOR_PRESET 1.0

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

vec3 apply_color_profile(vec3 color, int profile)
{
	const mat3x3 profile1 = mat3x3(0.412391, 0.357584, 0.180481,
	                               0.212639, 0.715169, 0.072192,
	                               0.019331, 0.119195, 0.950532);

	const mat3x3 profile2 = mat3x3(0.430554, 0.341550, 0.178352,
	                               0.222004, 0.706655, 0.071341,
	                               0.020182, 0.129553, 0.939322);

	const mat3x3 profile3 = mat3x3(0.396686, 0.372504, 0.181266,
	                               0.210299, 0.713766, 0.075936,
	                               0.006131, 0.115356, 0.967571);

	const mat3x3 profile4 = mat3x3(0.393521, 0.365258, 0.191677,
	                               0.212376, 0.701060, 0.086564,
	                               0.018739, 0.111934, 0.958385);

	const mat3x3 profile5 = mat3x3(0.392258, 0.351135, 0.166603,
	                               0.209410, 0.725680, 0.064910,
	                               0.016061, 0.093636, 0.850324);

	const mat3x3 profile6 = mat3x3(0.377923, 0.317366, 0.207738,
	                               0.195679, 0.722319, 0.082002,
	                               0.010514, 0.097826, 1.076960);

	if      (profile == 1) color *= profile1
	else if (profile == 2) color *= profile2
	else if (profile == 3) color *= profile3
	else if (profile == 4) color *= profile4
	else if (profile == 5) color *= profile5
	else if (profile == 6) color *= profile7
	else { // no transform }

	return color;
}

vec3 linear_to_colorspace(vec3 color, int colorspace)
{
	// gamma 2.2
	const mat3x3 to_srgb = mat3x3(
	 3.240970, -1.537383, -0.498611,
	-0.969244,  1.875968,  0.041555,
	 0.055630, -0.203977,  1.056972
	);

	// gamma 2.6
	const mat3x3 to_dci_p3 = mat3x3(
	 2.725394,  -1.018003,  -0.440163,
	-0.795168,   1.689732,   0.022647,
	 0.041242,  -0.087639,   1.100929
	);

	// gamma 2.2
	const mat3x3 to_adobe_2020 = mat3x3(
	 2.041588, -0.565007, -0.344731,
	-0.969244,  1.875968,  0.041555,
	 0.013444, -0.118362,  1.015175
	);

	// gamma 2.2
	const mat3x3 to_rec_2020 = mat3x3(
	 1.716651, -0.355671, -0.253366,
	-0.666684,  1.616481,  0.015769,
	 0.017640, -0.042771,  0.942103
	);

	if      (colorspace == 1) color *= to_srgb;
	else if (colorspace == 2) color *= to_dci_p3;
	else if (colorspace == 3) color *= to_adobe_2020;
	else if (colorspace == 4) color *= to_rec_2020;
	else { // no transform }

	return color;
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
