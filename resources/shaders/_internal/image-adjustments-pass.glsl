#version 330 core

// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: MIT
//
// Most of the code was taken from various sources with minor adjustments:
//
//  - White point mapping: Dogway, Neil Bartlett, Tanner Helland
//  - Colur space transforms & the rest: guest(r), Dr. Venom
//

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

uniform int COLOR_SPACE;

uniform int ENABLE_ADJUSTMENTS;

uniform int   COLOR_PROFILE;
uniform float BRIGHTNESS;
uniform float CONTRAST;
uniform float GAMMA;
uniform float DIGITAL_CONTRAST;
uniform vec3  BLACK_LEVEL_COLOR;
uniform float BLACK_LEVEL;
uniform float SATURATION;

uniform float COLOR_TEMPERATURE_KELVIN;
uniform float COLOR_TEMPERATURE_LUMA_PRESERVE;

uniform float RED_GAIN;
uniform float GREEN_GAIN;
uniform float BLUE_GAIN;


// Adapted from 'WinUaeColor.fx'
// https://github.com/guestrr/WinUAE-Shaders/
//
// Copyright (C) 2020 guest(r), Dr. Venom - guest.r@gmail.com

// Color profile transforms (sRGB to XYZ)

const mat3 sRGB_to_XYZ_sRGB = mat3(
     0.412391,  0.212639,  0.019331,
     0.357584,  0.715169,  0.119195,
     0.180481,  0.072192,  0.950532
);

const mat3 sRGB_to_XYZ_Profile1 = mat3(
     0.430554,  0.222004,  0.020182,
     0.341550,  0.706655,  0.129553,
     0.178352,  0.071341,  0.939322
);

const mat3 sRGB_to_XYZ_Profile2 = mat3(
     0.396686,  0.210299,  0.006131,
     0.372504,  0.713766,  0.115356,
     0.181266,  0.075936,  0.967571
);

const mat3 sRGB_to_XYZ_Profile3 = mat3(
     0.393521,  0.212376,  0.018739,
     0.365258,  0.701060,  0.111934,
     0.191677,  0.086564,  0.958385
);

const mat3 sRGB_to_XYZ_Profile4 = mat3(
     0.392258,  0.209410,  0.016061,
     0.351135,  0.725680,  0.093636,
     0.166603,  0.064910,  0.850324
);

const mat3 sRGB_to_XYZ_Profile5 = mat3(
     0.377923,  0.195679,  0.010514,
     0.317366,  0.722319,  0.097826,
     0.207738,  0.082002,  1.076960
);

// Colour space transforms (XYZ to linear RGB in target colour space)

const mat3 XYZ_to_sRGB = mat3(
     3.240970, -0.969244,  0.055630,
    -1.537383,  1.875968, -0.203977,
    -0.498611,  0.041555,  1.056972
);

const mat3 XYZ_to_DCI_P3 = mat3(
     2.725394, -0.795168,  0.041242,
    -1.018003,  1.689732, -0.087639,
    -0.440163,  0.022647,  1.100929
);

const mat3 XYZ_to_DisplayP3 = mat3(
     2.493497, -0.829489,  0.035846,
    -0.931384,  1.762664, -0.076172,
    -0.402711,  0.023625,  0.956885
);

const mat3 XYZ_to_ModernP3 = mat3(
     2.791723, -0.894766,  0.041678,
    -1.173165,  1.815586, -0.130886,
    -0.440973,  0.032000,  1.002034
);

const mat3 XYZ_to_AdobeRGB = mat3(
     2.041588, -0.969244,  0.013444,
    -0.565007,  1.875968, -0.118360,
    -0.344731,  0.041555,  1.015175
);

const mat3 XYZ_to_Rec2020 = mat3(
     1.716651, -0.666684,  0.017640,
    -0.355671,  1.616481, -0.042771,
    -0.253366,  0.015769,  0.942103
);

vec3 XYZ_to_Yxy(vec3 XYZ)
{
	float XYZrgb = XYZ.r + XYZ.g + XYZ.b;
	float Yxyr   = XYZ.g;
	float Yxyg   = (XYZrgb <= 0.0) ? 0.3805 : XYZ.r / XYZrgb;
	float Yxyb   = (XYZrgb <= 0.0) ? 0.3769 : XYZ.g / XYZrgb;

	return vec3(Yxyr, Yxyg, Yxyb);
}

vec3 Yxy_to_XYZ(vec3 Yxy)
{
	float Xs  = Yxy.r * (Yxy.g / Yxy.b);
	float Xsz = (Yxy.r <= 0.0) ? 0 : 1;
	vec3 XYZ  = vec3(Xsz) * vec3(Xs, Yxy.r, (Xs / Yxy.g) - Xs - Yxy.r);

	return XYZ;
}

// Expects gamma-encoded color
float luminance(vec3 x)
{
	return dot(x, vec3(0.212656, 0.715158, 0.072186));
}

// Expects gamma-encoded color
vec3 saturation(vec3 color, float s)
{
	return clamp(mix(vec3(luminance(color)), color, s + 1.0), 0.0, 1.0);
}

// Adaptred from Guest's 'pre-shaders-afterglow.slang'
// Copyright (C) 2019-2025 guest(r) and Dr. Venom
//
// Source: https://github.com/libretro/slang-shaders/blob/cf5c768ffda2520d4938df68d33fd63fff276c0c/crt/shaders/guest/advanced/pre-shaders-afterglow.slang
//
vec3 plant(vec3 tar, float r)
{
	float t = max(max(tar.r, tar.g), tar.b) + 0.00001;
	return tar * r / t;
}

// Expects gamma-encoded color
// Amount range: -2.0 to 2.0
vec3 sigmoid_contrast(vec3 color, float amount)
{
	float x = max(max(color.r, color.g), color.b);
	float c = max(mix(x, smoothstep(0.0, 1.0, x), amount), 0.0);
	return plant(color, c);
}

// Adapted from `PR80_00_Base_Effects.fxh` by prod80 (Bas Veth)
// https://github.com/prod80/prod80-ReShade-Repository/
//
// MIT License, Copyright (c) 2020 prod80
//

// Expects gamma-encoded color
vec3 rgb_to_hcv(vec3 color)
{
	// Based on work by Sam Hocevar and Emil Persson
	vec4 p = (color.g < color.b) ? vec4(color.bg, -1.0, 2.0 / 3.0)
	                             : vec4(color.gb, 0.0, -1.0 / 3.0);

	vec4 q1 = (color.r < p.x) ? vec4(p.xyw, color.r) : vec4(color.r, p.yzx);
	float c = q1.x - min(q1.w, q1.y);
	float h = abs((q1.w - q1.y) / (6.0 * c + 0.000001) + q1.z);
	return vec3(h, c, q1.x);
}

// Expects gamma-encoded color
vec3 rgb_to_hsl(vec3 color)
{
	color    = max(color, 0.000001);
	vec3 hcv = rgb_to_hcv(color);
	float l  = hcv.z - hcv.y * 0.5;
	float s  = hcv.y / (1.0 - abs(l * 2.0 - 1.0) + 0.000001);
	return vec3(hcv.x, s, l);
}

// Expects gamma-encoded color
vec3 hue_to_rgb(float hue)
{
	return clamp(vec3(abs(hue * 6.0 - 3.0) - 1.0,
	                  2.0 - abs(hue * 6.0 - 2.0),
	                  2.0 - abs(hue * 6.0 - 4.0)),
	             0.0,
	             1.0);
}

// Expects gamma-encoded color
vec3 hsl_to_rgb(in vec3 hsl)
{
	vec3 color = hue_to_rgb(hsl.x);
	float c    = (1.0 - abs(2.0 * hsl.z - 1.0)) * hsl.y;
	return (color - 0.5f) * c + hsl.z;
}

// Expects gamma-encoded color
vec3 kelvin_to_rgb(float k)
{
	vec3 ret;
	float kelvin = clamp(k, 1000.0f, 40000.0f) / 100.0f;

	if (kelvin <= 66.0f) {
		ret.r = 1.0f;
		ret.g = clamp(0.39008157876901960784f * log(kelvin) -
		                      0.63184144378862745098f,
		              0.0,
		              1.0);

	} else {
		float t = max(kelvin - 60.0f, 0.0f);

		ret.r = clamp(1.29293618606274509804f * pow(t, -0.1332047592f),
		              0.0,
		              1.0);

		ret.g = clamp(1.12989086089529411765f * pow(t, -0.0755148492f),
		              0.0,
		              1.0);
	}

	if (kelvin >= 66.0f) {
		ret.b = 1.0f;
	} else if (kelvin < 19.0f) {
		ret.b = 0.0f;
	} else {
		ret.b = clamp(0.54320678911019607843f * log(kelvin - 10.0f) -
		                      1.19625408914f,
		              0.0,
		              1.0);
	}
	return ret;
}

// Expects gamma-encoded color
vec3 color_temperature(vec3 color, float kelvin, float luma_preserve)
{
	float orig_luma = rgb_to_hsl(color).z;
	color *= kelvin_to_rgb(kelvin);
	vec3 color2 = hsl_to_rgb(vec3(rgb_to_hsl(color).xy, orig_luma));

	return mix(color, color2, luma_preserve);
}

// Adapted from Dogway's 'pre-shaders-afterglow-grade.slang'
// Copyright (C) 2020-2023 Dogway (Jose Linares)
//
// Source: https://github.com/libretro/slang-shaders/blob/cf5c768ffda2520d4938df68d33fd63fff276c0c/crt/shaders/guest/advanced/grade/pre-shaders-afterglow-grade.slang
//
float eotf_1886a(float color, float bl, float brightness, float contrast)
{
	const float wl = 100.0;

	float b = pow(bl, 1.0 / 2.4);
	float a = pow(wl, 1.0 / 2.4) - b;

	// Convert range to -0.20 to +0.20
	b = (brightness - 50.0) / 250. + b / a;

	// Convert range to 0.50 to +2.00
	a = contrast != 50.0 ? pow(2.0, (contrast - 50.0) / 50.0) : 1.0;

	const float Vc = 0.35;           // offset
	float Lw       = wl / 100.0 * a; // white level
	float Lb       = min(b * a, Vc); // black level
	const float a1 = 2.6;            // shoulder gamma
	const float a2 = 3.0;            // knee gamma

	float k = Lw / pow(1.0 + Lb, a1);

	// Slope for knee gamma
	float sl = k * pow(Vc + Lb, a1 - a2);

	color = color >= Vc ? k * pow(color + Lb, a1) : sl * pow(color + Lb, a2);

	// Black lift compensation
	float bc = 0.00446395 * pow(bl, 1.23486);

	// Undo black lift
	color = min(max(color - bc, 0.0) * (1.0 / (1.0 - bc)), 1.0);

	// Restore gamma from 'undo black lift'
	color = pow(color, 1.0 - 0.00843283 * pow(bl, 1.22744));

	return color;
}

// Expects gamma-encoded color
vec3 eotf_1886a(vec3 color, float black_level, float brightness, float contrast)
{
	color.r = eotf_1886a(color.r,
						 black_level,
	                     brightness,
	                     contrast);

	color.g = eotf_1886a(color.g,
	                     black_level,
	                     brightness,
	                     contrast);

	color.b = eotf_1886a(color.b,
	                     black_level,
	                     brightness,
	                     contrast);

	return color.rgb;
}

const float BlackLiftApproxGamma = 2.5;

const float CrtBlackLevel =
        -(100000.0 *
          log((72981.0 - 500000.0 / (3.0 * max(2.3, BlackLiftApproxGamma))) / 9058.0)) /
        945461.0;


void main()
{
	vec3 color = texture(inputTexture, v_texCoord).rgb;
	vec3 orig_color = color;

	color = sigmoid_contrast(color, DIGITAL_CONTRAST);
	color = max(color, BLACK_LEVEL_COLOR * BLACK_LEVEL);
	color = saturation(color, SATURATION);

	color = color_temperature(color,
	                          COLOR_TEMPERATURE_KELVIN,
	                          COLOR_TEMPERATURE_LUMA_PRESERVE);

	if (ENABLE_ADJUSTMENTS == 0) {
		color = orig_color;
	}

	// Color space & color profile transforms
	float out_gamma = 0.0;
	mat3 XYZ_to_linear_RGB = mat3(0.0);

	if      (COLOR_SPACE == 1) { out_gamma = 2.6; XYZ_to_linear_RGB = XYZ_to_DCI_P3;    } // DCI-P3, DCI whitepoint
	else if (COLOR_SPACE == 2) { out_gamma = 2.6; XYZ_to_linear_RGB = XYZ_to_DisplayP3; } // DCI-P3, D65 whitepoint
	else if (COLOR_SPACE == 3) { out_gamma = 2.2; XYZ_to_linear_RGB = XYZ_to_DisplayP3; } // Display P3, D65 whitepoint
	else if (COLOR_SPACE == 4) { out_gamma = 2.2; XYZ_to_linear_RGB = XYZ_to_ModernP3;  }
	else if (COLOR_SPACE == 5) { out_gamma = 2.2; XYZ_to_linear_RGB = XYZ_to_AdobeRGB;  }
	else if (COLOR_SPACE == 6) { out_gamma = 2.4; XYZ_to_linear_RGB = XYZ_to_Rec2020;   }
	else   /*COLOR_SPACE == 0*/{ out_gamma = 2.2; XYZ_to_linear_RGB = XYZ_to_sRGB;      }

	mat3 sRGB_to_XYZ = sRGB_to_XYZ_sRGB;

	if      (COLOR_PROFILE == 1) { sRGB_to_XYZ = sRGB_to_XYZ_Profile1; }
	else if (COLOR_PROFILE == 2) { sRGB_to_XYZ = sRGB_to_XYZ_Profile2; }
	else if (COLOR_PROFILE == 3) { sRGB_to_XYZ = sRGB_to_XYZ_Profile3; }
	else if (COLOR_PROFILE == 4) { sRGB_to_XYZ = sRGB_to_XYZ_Profile4; }
	else if (COLOR_PROFILE == 5) { sRGB_to_XYZ = sRGB_to_XYZ_Profile5; }
	else   /*COLOR_PROFILE == 0)*/ sRGB_to_XYZ = sRGB_to_XYZ_sRGB;

	if (ENABLE_ADJUSTMENTS == 0) {
		sRGB_to_XYZ = sRGB_to_XYZ_sRGB;
	}

	// sRGB => linear RGB
	// The colour profiles are correct when using 2.2 gamma
	color = pow(color, vec3(2.2));

	// linear RGB => XYZ (with optional color profile transform)
	color = sRGB_to_XYZ * color;

	// XYZ => linear RGB
	color = XYZ_to_linear_RGB * color;

	// Apply RGB channel gain adjustments
	orig_color = color;

	// Use the square of the input params to achieve roughly perceptual linear
	// taper (that's close enough to ~2.2 gamma)
	color *= vec3(RED_GAIN * RED_GAIN,
	              GREEN_GAIN * GREEN_GAIN,
	              BLUE_GAIN * BLUE_GAIN);

	if (ENABLE_ADJUSTMENTS == 0) {
		color = orig_color;
	}

	// linear RGB => gamma-encoded output space
	color = pow(color, vec3(1.0 / 2.2));
	orig_color = color;

	// gamma-encoded output space => linear RGB via CRT EOTF
	// (Electro-Optical Transform Function)
	//
	// This transform to display referred linear and undoes developer-baked
	// CRT gamma (from 2.40 at default 0.1 CRT black level, to 2.60 at 0.0 CRT
	// black level).
	color = eotf_1886a(color, CrtBlackLevel, BRIGHTNESS, CONTRAST);

	// linear RGB => gamma-encoded output space
	//
	// We needed this extra gamma encode/decode roundtrip because applying the
	// CRT EOTF before the colour profile transforms would result in too
	// saturated colours.
	color = pow(color, vec3(1.0 / (out_gamma + GAMMA)));

	if (ENABLE_ADJUSTMENTS == 0) {
		color = orig_color;
	}
	color = clamp(color, vec3(0.0), vec3(1.0));

	FragColor = vec4(color, 1.0);
}

#endif
