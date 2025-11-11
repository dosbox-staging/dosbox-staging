#version 330 core

// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
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
uniform int COLOR_PROFILE;

uniform float BRIGHTNESS;
uniform float CONTRAST;
uniform float GAMMA;
uniform float BLACK_LEVEL;
uniform vec3  BLACK_LEVEL_TINT;
uniform float SATURATION;

uniform float WHITE_POINT_KELVIN;

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

// From `PR80_00_Base_Effects.fxh` by prod80 (Bas Veth)
// https://github.com/prod80/prod80-ReShade-Repository/
//
// MIT License, Copyright (c) 2020 prod80
//

float softlight(float c, float b)
{
	return (b < 0.5) ? (2.0 * c * b + c * c * (1.0 - 2.0 * b))
	                 : (sqrt(c) * (2.0 * b - 1.0) + 2.0 * c * (1.0 - b));
}

vec3 softlight(vec3 c, vec3 b)
{
	return vec3(softlight(c.r, b.r), softlight(c.g, b.g), softlight(c.b, b.b));
}

float luminance(vec3 x)
{
	return dot(x, vec3(0.212656, 0.715158, 0.072186));
}

vec3 contrast(vec3 res, float x)
{
	vec3 c = softlight(res, res);
	x = (x < 0.0) ? x * 0.5 : x;

	return clamp(mix(res, c, x), vec3(0.0), vec3(1.0));
}

vec3 brightness(vec3 res, float x)
{
	vec3 c = vec3(1.0) - (vec3(1.0) - res) * (vec3(1.0) - res);
	x = (x < 0.0) ? x * 0.5 : x;

	return clamp(mix(res, c, x), vec3(0.0), vec3(1.0));
}

vec3 saturation(vec3 res, float x)
{
	return clamp(mix(vec3(luminance(res)), res, x + 1.0), 0.0, 1.0);
}

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

vec3 whitepoint(vec3 color, float kelvin)
{
	return color * kelvin_to_rgb(kelvin);
}

void main()
{
	vec3 color = texture(inputTexture, v_texCoord).rgb;

	float gamma = 0.0;
	mat3 XYZ_to_linear_RGB = mat3(0.0);

	if      (COLOR_SPACE == 1) { gamma = 2.6; XYZ_to_linear_RGB = XYZ_to_DCI_P3;   }
	else if (COLOR_SPACE == 2) { gamma = 2.6; XYZ_to_linear_RGB = XYZ_to_DisplayP3; }
	else if (COLOR_SPACE == 3) { gamma = 2.2; XYZ_to_linear_RGB = XYZ_to_DisplayP3; }
	else if (COLOR_SPACE == 4) { gamma = 2.2; XYZ_to_linear_RGB = XYZ_to_ModernP3; }
	else if (COLOR_SPACE == 5) { gamma = 2.2; XYZ_to_linear_RGB = XYZ_to_AdobeRGB; }
	else if (COLOR_SPACE == 6) { gamma = 2.4; XYZ_to_linear_RGB = XYZ_to_Rec2020;  }
	else   /*COLOR_SPACE == 0*/{ gamma = 2.2; XYZ_to_linear_RGB = XYZ_to_sRGB;     }

	// sRGB => linear RGB
	color = pow(color, vec3(2.2));

	mat3 sRGB_to_XYZ = sRGB_to_XYZ_sRGB;

	if      (COLOR_PROFILE == 1) { sRGB_to_XYZ = sRGB_to_XYZ_Profile1; }
	else if (COLOR_PROFILE == 2) { sRGB_to_XYZ = sRGB_to_XYZ_Profile2; }
	else if (COLOR_PROFILE == 3) { sRGB_to_XYZ = sRGB_to_XYZ_Profile3; }
	else if (COLOR_PROFILE == 4) { sRGB_to_XYZ = sRGB_to_XYZ_Profile4; }
	else if (COLOR_PROFILE == 5) { sRGB_to_XYZ = sRGB_to_XYZ_Profile5; }
	else   /*COLOR_PROFILE == 0)*/ sRGB_to_XYZ = sRGB_to_XYZ_sRGB;

	// linear RGB => XYZ (with optional color profile transform)
	color = sRGB_to_XYZ * color;

	// XYZ => linear RGB
	color = XYZ_to_linear_RGB * color;

	// linear RGB => sRGB
	color = pow(color, vec3(1.0 / gamma));

	color = clamp(pow(color, vec3(1.0 + GAMMA)), vec3(0.0), vec3(1.0));

	// Image adjustments
	color = contrast(color, CONTRAST);
	color = brightness(color, BRIGHTNESS);
	color = saturation(color, SATURATION);

	color += (BLACK_LEVEL_TINT * 0.02) * (BLACK_LEVEL * 100);

	color = whitepoint(color, WHITE_POINT_KELVIN);

	FragColor = vec4(color.rgb, 1.0);
}

#endif
