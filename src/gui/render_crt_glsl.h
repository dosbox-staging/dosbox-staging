/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_RENDER_CRT_GLSL_H
#define DOSBOX_RENDER_CRT_GLSL_H

#if C_OPENGL

/*
	CRT Shader by EasyMode
	License: GPL

	A flat CRT shader ideally for 1080p or higher displays.

	Recommended Settings:

	Video
	- Aspect Ratio:  4:3
	- Integer Scale: Off

	Shader
	- Filter: Nearest
	- Scale:  Don't Care

	Example RGB Mask Parameter Settings:

	Aperture Grille (Default)
	- Dot Width:  1
	- Dot Height: 1
	- Stagger:    0

	Lottes' Shadow Mask
	- Dot Width:  2
	- Dot Height: 1
	- Stagger:    3
*/

static const char crt_easymode_tweaked_glsl[] = R"GLSL(
#version 120

// Parameter lines go here:
#pragma parameter SHARPNESS_H "Sharpness Horizontal" 0.5 0.0 1.0 0.05
#pragma parameter SHARPNESS_V "Sharpness Vertical" 1.0 0.0 1.0 0.05
#pragma parameter MASK_STRENGTH "Mask Strength" 0.3 0.0 1.0 0.01
#pragma parameter MASK_DOT_WIDTH "Mask Dot Width" 1.0 1.0 100.0 1.0
#pragma parameter MASK_DOT_HEIGHT "Mask Dot Height" 1.0 1.0 100.0 1.0
#pragma parameter MASK_STAGGER "Mask Stagger" 0.0 0.0 100.0 1.0
#pragma parameter MASK_SIZE "Mask Size" 1.0 1.0 100.0 1.0
#pragma parameter SCANLINE_STRENGTH "Scanline Strength" 1.0 0.0 1.0 0.05
#pragma parameter SCANLINE_BEAM_WIDTH_MIN "Scanline Beam Width Min." 1.5 0.5 5.0 0.5
#pragma parameter SCANLINE_BEAM_WIDTH_MAX "Scanline Beam Width Max." 1.5 0.5 5.0 0.5
#pragma parameter SCANLINE_BRIGHT_MIN "Scanline Brightness Min." 0.35 0.0 1.0 0.05
#pragma parameter SCANLINE_BRIGHT_MAX "Scanline Brightness Max." 0.65 0.0 1.0 0.05
#pragma parameter SCANLINE_CUTOFF "Scanline Cutoff" 400.0 1.0 1000.0 1.0
#pragma parameter GAMMA_INPUT "Gamma Input" 2.0 0.1 5.0 0.1
#pragma parameter GAMMA_OUTPUT "Gamma Output" 1.8 0.1 5.0 0.1
#pragma parameter BRIGHT_BOOST "Brightness Boost" 1.2 1.0 2.0 0.01
#pragma parameter DILATION "Dilation" 1.0 0.0 1.0 1.0

#if defined(VERTEX)

#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying
#define COMPAT_ATTRIBUTE attribute
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

COMPAT_ATTRIBUTE vec4 a_position;
COMPAT_ATTRIBUTE vec4 COLOR;
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec4 COL0;
COMPAT_VARYING vec2 v_texCoord;

uniform COMPAT_PRECISION vec2 rubyOutputSize;
uniform COMPAT_PRECISION vec2 rubyTextureSize;
uniform COMPAT_PRECISION vec2 rubyInputSize;

void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize / rubyTextureSize;
}

#elif defined(FRAGMENT)

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
precision mediump int;
#endif
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

uniform COMPAT_PRECISION vec2 rubyOutputSize;
uniform COMPAT_PRECISION vec2 rubyTextureSize;
uniform COMPAT_PRECISION vec2 rubyInputSize;
uniform sampler2D rubyTexture;
COMPAT_VARYING vec2 v_texCoord;

#define FIX(c) max(abs(c), 1e-5)
#define PI 3.141592653589

#define TEX2D(c) dilate(COMPAT_TEXTURE(rubyTexture, c))

// compatibility #defines
#define Source rubyTexture
#define vTexCoord v_texCoord.xy

#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize) //either rubyTextureSize or rubyInputSize
#define outsize vec4(rubyOutputSize, 1.0 / rubyOutputSize)

#ifdef PARAMETER_UNIFORM
// All parameter floats need to have COMPAT_PRECISION in front of them
uniform COMPAT_PRECISION float SHARPNESS_H;
uniform COMPAT_PRECISION float SHARPNESS_V;
uniform COMPAT_PRECISION float MASK_STRENGTH;
uniform COMPAT_PRECISION float MASK_DOT_WIDTH;
uniform COMPAT_PRECISION float MASK_DOT_HEIGHT;
uniform COMPAT_PRECISION float MASK_STAGGER;
uniform COMPAT_PRECISION float MASK_SIZE;
uniform COMPAT_PRECISION float SCANLINE_STRENGTH;
uniform COMPAT_PRECISION float SCANLINE_BEAM_WIDTH_MIN;
uniform COMPAT_PRECISION float SCANLINE_BEAM_WIDTH_MAX;
uniform COMPAT_PRECISION float SCANLINE_BRIGHT_MIN;
uniform COMPAT_PRECISION float SCANLINE_BRIGHT_MAX;
uniform COMPAT_PRECISION float SCANLINE_CUTOFF;
uniform COMPAT_PRECISION float GAMMA_INPUT;
uniform COMPAT_PRECISION float GAMMA_OUTPUT;
uniform COMPAT_PRECISION float BRIGHT_BOOST;
uniform COMPAT_PRECISION float DILATION;
#else
#define SHARPNESS_H 0.55                //tweaked
#define SHARPNESS_V 0.55                //tweaked
#define MASK_STRENGTH 0.45              //tweaked
#define MASK_DOT_WIDTH 1.0
#define MASK_DOT_HEIGHT 1.0
#define MASK_STAGGER 0.0
#define MASK_SIZE 1.0
#define SCANLINE_STRENGTH 1.0
#define SCANLINE_BEAM_WIDTH_MIN 1.5
#define SCANLINE_BEAM_WIDTH_MAX 1.5
#define SCANLINE_BRIGHT_MIN 0.35
#define SCANLINE_BRIGHT_MAX 0.65
#define SCANLINE_CUTOFF 400.0
#define GAMMA_INPUT 2.0
#define GAMMA_OUTPUT 1.8
#define BRIGHT_BOOST 1.225              //tweaked
#define DILATION 1.0
#endif

// Set to 0 to use linear filter and gain speed
#define ENABLE_LANCZOS 1

vec4 dilate(vec4 col)
{
	vec4 x = mix(vec4(1.0), col, DILATION);

	return col * x;
}

float curve_distance(float x, float sharp)
{

/*
	apply half-circle s-curve to distance for sharper (more pixelated) interpolation
	single line formula for Graph Toy:
	0.5 - sqrt(0.25 - (x - step(0.5, x)) * (x - step(0.5, x))) * sign(0.5 - x)
*/

	float x_step = step(0.5, x);
	float curve = 0.5 - sqrt(0.25 - (x - x_step) * (x - x_step)) * sign(0.5 - x);

	return mix(x, curve, sharp);
}

mat4 get_color_matrix(vec2 co, vec2 dx)
{
	return mat4(TEX2D(co - dx), TEX2D(co), TEX2D(co + dx), TEX2D(co + 2.0 * dx));
}

vec3 filter_lanczos(vec4 coeffs, mat4 color_matrix)
{
	vec4 col        = color_matrix * coeffs;
	vec4 sample_min = min(color_matrix[1], color_matrix[2]);
	vec4 sample_max = max(color_matrix[1], color_matrix[2]);

	col = clamp(col, sample_min, sample_max);

	return col.rgb;
}

void main()
{
	vec2 dx     = vec2(SourceSize.z, 0.0);
	vec2 dy     = vec2(0.0, SourceSize.w);
	vec2 pix_co = vTexCoord * SourceSize.xy - vec2(0.5, 0.5);
	vec2 tex_co = (floor(pix_co) + vec2(0.5, 0.5)) * SourceSize.zw;
	vec2 dist   = fract(pix_co);
	float curve_x;
	vec3 col, col2;

#if ENABLE_LANCZOS
	curve_x = curve_distance(dist.x, SHARPNESS_H * SHARPNESS_H);

	vec4 coeffs = PI * vec4(1.0 + curve_x, curve_x, 1.0 - curve_x, 2.0 - curve_x);

	coeffs = FIX(coeffs);
	coeffs = 2.0 * sin(coeffs) * sin(coeffs * 0.5) / (coeffs * coeffs);
	coeffs /= dot(coeffs, vec4(1.0));

	col  = filter_lanczos(coeffs, get_color_matrix(tex_co, dx));
	col2 = filter_lanczos(coeffs, get_color_matrix(tex_co + dy, dx));
#else
	curve_x = curve_distance(dist.x, SHARPNESS_H);

	col  = mix(TEX2D(tex_co).rgb,      TEX2D(tex_co + dx).rgb,      curve_x);
	col2 = mix(TEX2D(tex_co + dy).rgb, TEX2D(tex_co + dx + dy).rgb, curve_x);
#endif

	col = mix(col, col2, curve_distance(dist.y, SHARPNESS_V));
	col = pow(col, vec3(GAMMA_INPUT / (DILATION + 1.0)));

	float luma        = dot(vec3(0.2126, 0.7152, 0.0722), col);
	float bright      = (max(col.r, max(col.g, col.b)) + luma) * 0.5;
	float scan_bright = clamp(bright, SCANLINE_BRIGHT_MIN, SCANLINE_BRIGHT_MAX);
	float scan_beam   = clamp(bright * SCANLINE_BEAM_WIDTH_MAX, SCANLINE_BEAM_WIDTH_MIN, SCANLINE_BEAM_WIDTH_MAX);
	float scan_weight = 1.0 - pow(cos(vTexCoord.y * 2.0 * PI * SourceSize.y) * 0.5 + 0.5, scan_beam) * SCANLINE_STRENGTH;

	float mask   = 1.0 - MASK_STRENGTH;
	vec2 mod_fac = floor(vTexCoord * outsize.xy * SourceSize.xy / (rubyInputSize.xy * vec2(MASK_SIZE, MASK_DOT_HEIGHT * MASK_SIZE)));
	int dot_no   = int(mod((mod_fac.x + mod(mod_fac.y, 2.0) * MASK_STAGGER) / MASK_DOT_WIDTH, 3.0));
	vec3 mask_weight;

	if      (dot_no == 0) mask_weight = vec3(1.0,  mask, mask);
	else if (dot_no == 1) mask_weight = vec3(mask, 1.0,  mask);
	else                  mask_weight = vec3(mask, mask, 1.0);

	if (rubyInputSize.y >= SCANLINE_CUTOFF)
		scan_weight = 1.0;

	col2 = col.rgb;
	col *= vec3(scan_weight);
	col  = mix(col, col2, scan_bright);
	col *= mask_weight;
	col  = pow(col, vec3(1.0 / GAMMA_OUTPUT));

	FragColor = vec4(col * BRIGHT_BOOST, 1.0);
}
#endif
)GLSL";

// Simple scanlines with curvature and mask effects lifted from crt-lottes
// by hunterk

static const char crt_fakelottes_tweaked_glsl[] = R"GLSL(
#version 120

////////////////////////////////////////////////////////////////////
////////////////////////////  SETTINGS  ////////////////////////////
/////  comment these lines to disable effects and gain speed  //////
////////////////////////////////////////////////////////////////////

#define MASK // fancy, expensive phosphor mask effect
//#define CURVATURE // applies barrel distortion to the screen
#define SCANLINES // applies horizontal scanline effect
//#define ROTATE_SCANLINES // for TATE games; also disables the mask effects, which look bad with it
#define EXTRA_MASKS // disable these if you need extra registers freed up

////////////////////////////////////////////////////////////////////
//////////////////////////  END SETTINGS  //////////////////////////
////////////////////////////////////////////////////////////////////

///////////////////////  Runtime Parameters  ///////////////////////
#pragma parameter shadowMask "shadowMask" 1.0 0.0 4.0 1.0
#pragma parameter SCANLINE_SINE_COMP_B "Scanline Intensity" 0.40 0.0 1.0 0.05
#pragma parameter warpX "warpX" 0.031 0.0 0.125 0.01
#pragma parameter warpY "warpY" 0.041 0.0 0.125 0.01
#pragma parameter maskDark "maskDark" 0.5 0.0 2.0 0.1
#pragma parameter maskLight "maskLight" 1.5 0.0 2.0 0.1
#pragma parameter crt_gamma "CRT Gamma" 2.5 1.0 4.0 0.05
#pragma parameter monitor_gamma "Monitor Gamma" 2.2 1.0 4.0 0.05
#pragma parameter SCANLINE_SINE_COMP_A "Scanline Sine Comp A" 0.0 0.0 0.10 0.01
#pragma parameter SCANLINE_BASE_BRIGHTNESS "Scanline Base Brightness" 0.95 0.0 1.0 0.01

// prevent stupid behavior
#if defined ROTATE_SCANLINES && !defined SCANLINES
	#define SCANLINES
#endif

#if defined(VERTEX)

#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying
#define COMPAT_ATTRIBUTE attribute
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

COMPAT_ATTRIBUTE vec4 a_position;
COMPAT_ATTRIBUTE vec4 COLOR;
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec4 COL0;
COMPAT_VARYING vec2 v_texCoord;

vec4 _oPosition1;
uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int rubyFrameCount;
uniform COMPAT_PRECISION vec2 rubyOutputSize;
uniform COMPAT_PRECISION vec2 rubyTextureSize;
uniform COMPAT_PRECISION vec2 rubyInputSize;

// compatibility #defines
#define vTexCoord v_texCoord.xy
#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize) //either rubyTextureSize or rubyInputSize
#define OutSize vec4(rubyOutputSize, 1.0 / rubyOutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float WHATEVER;
#else
#define WHATEVER 0.0
#endif

void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize / rubyTextureSize;
}

#elif defined(FRAGMENT)

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int rubyFrameCount;
uniform COMPAT_PRECISION vec2 rubyOutputSize;
uniform COMPAT_PRECISION vec2 rubyTextureSize;
uniform COMPAT_PRECISION vec2 rubyInputSize;
uniform sampler2D rubyTexture;
COMPAT_VARYING vec2 v_texCoord;

// compatibility #defines
#define Source rubyTexture
#define vTexCoord v_texCoord.xy

#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize) //either rubyTextureSize or rubyInputSize
#define OutSize vec4(rubyOutputSize, 1.0 / rubyOutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float SCANLINE_BASE_BRIGHTNESS;
uniform COMPAT_PRECISION float SCANLINE_SINE_COMP_A;
uniform COMPAT_PRECISION float SCANLINE_SINE_COMP_B;
uniform COMPAT_PRECISION float warpX;
uniform COMPAT_PRECISION float warpY;
uniform COMPAT_PRECISION float maskDark;
uniform COMPAT_PRECISION float maskLight;
uniform COMPAT_PRECISION float shadowMask;
uniform COMPAT_PRECISION float crt_gamma;
uniform COMPAT_PRECISION float monitor_gamma;
#else
#define SCANLINE_BASE_BRIGHTNESS 0.95
#define SCANLINE_SINE_COMP_A 0.0
#define SCANLINE_SINE_COMP_B 0.40
#define warpX 0.031
#define warpY 0.041
#define maskDark 0.5
#define maskLight 1.5
#define shadowMask 1.0
#define crt_gamma 2.5
#define monitor_gamma 2.2
#endif

/*
	The following code allows the shader to override any texture filtering
	configured in DOSBox. if 'output' is set to 'opengl', bilinear filtering
	will be enabled and OPENGLNB will not be defined, if 'output' is set to
	'openglnb', nearest neighbour filtering will be enabled and OPENGLNB will
	be defined.

	If you wish to use the default filtering method that is currently enabled
	in DOSBox, use COMPAT_TEXTURE to lookup a texel from the input texture.

	If you wish to force nearest-neighbor interpolation use NN_TEXTURE.

	If you wish to force bilinear interpolation use BL_TEXTURE.

	If DOSBox is configured to use the filtering method that is being forced,
	the default	hardware implementation will be used, otherwise the custom
	implementations below will be used instead.

	These custom implementations rely on the `rubyTextureSize` uniform variable.
	The code could calculate the texture size from the sampler using the
	textureSize() GLSL function, but this would require a minimum of GLSL
	version 130, which may prevent the shader from working on older systems.
*/

#if defined(OPENGLNB)
#define NN_TEXTURE COMPAT_TEXTURE
#define BL_TEXTURE blTexture
vec4 blTexture(in sampler2D sampler, in vec2 uv)
{
	// subtract 0.5 here and add it again after the floor to centre the texel
	vec2 texCoord = uv * rubyTextureSize - vec2(0.5);
	vec2 s0t0 = floor(texCoord) + vec2(0.5);
	vec2 s0t1 = s0t0 + vec2(0.0, 1.0);
	vec2 s1t0 = s0t0 + vec2(1.0, 0.0);
	vec2 s1t1 = s0t0 + vec2(1.0);

	vec2 invTexSize = 1.0 / rubyTextureSize;
	vec4 c_s0t0 = COMPAT_TEXTURE(sampler, s0t0 * invTexSize);
	vec4 c_s0t1 = COMPAT_TEXTURE(sampler, s0t1 * invTexSize);
	vec4 c_s1t0 = COMPAT_TEXTURE(sampler, s1t0 * invTexSize);
	vec4 c_s1t1 = COMPAT_TEXTURE(sampler, s1t1 * invTexSize);

	vec2 weight = fract(texCoord);

	vec4 c0 = c_s0t0 + (c_s1t0 - c_s0t0) * weight.x;
	vec4 c1 = c_s0t1 + (c_s1t1 - c_s0t1) * weight.x;

	return (c0 + (c1 - c0) * weight.y);
}
#else
#define BL_TEXTURE COMPAT_TEXTURE
#define NN_TEXTURE nnTexture
vec4 nnTexture(in sampler2D sampler, in vec2 uv)
{
	vec2 texCoord = floor(uv * rubyTextureSize) + vec2(0.5);
	vec2 invTexSize = 1.0 / rubyTextureSize;
	return COMPAT_TEXTURE(sampler, texCoord * invTexSize);
}
#endif

vec4 scanline(vec2 coord, vec4 frame)
{
#if defined SCANLINES
	vec2 omega = vec2(3.1415 * rubyOutputSize.x, 2.0 * 3.1415 * rubyTextureSize.y);
	vec2 sine_comp = vec2(SCANLINE_SINE_COMP_A, SCANLINE_SINE_COMP_B);
	vec3 res = frame.xyz;
	#ifdef ROTATE_SCANLINES
		sine_comp = sine_comp.yx;
		omega = omega.yx;
	#endif
	vec3 scanline = res * (SCANLINE_BASE_BRIGHTNESS + dot(sine_comp * sin(coord * omega), vec2(1.0, 1.0)));

	return vec4(scanline.x, scanline.y, scanline.z, 1.0);
#else
	return frame;
#endif
}

#ifdef CURVATURE
// Distortion of scanlines, and end of screen alpha.
vec2 Warp(vec2 pos)
{
	pos  = pos*2.0-1.0;
	pos *= vec2(1.0 + (pos.y*pos.y)*warpX, 1.0 + (pos.x*pos.x)*warpY);

	return pos*0.5 + 0.5;
}
#endif

#if defined MASK && !defined ROTATE_SCANLINES
	// Shadow mask.
	vec4 Mask(vec2 pos)
	{
		vec3 mask = vec3(maskDark, maskDark, maskDark);

		// Very compressed TV style shadow mask.
		if (shadowMask == 1.0)
		{
			float line = maskLight;
			float odd = 0.0;

			if (fract(pos.x*0.166666666) < 0.5) odd = 1.0;
			if (fract((pos.y + odd) * 0.5) < 0.5) line = maskDark;

			pos.x = fract(pos.x*0.333333333);

			if      (pos.x < 0.333) mask.r = maskLight;
			else if (pos.x < 0.666) mask.g = maskLight;
			else                    mask.b = maskLight;
			mask*=line;
		}

		// Aperture-grille.
		else if (shadowMask == 2.0)
		{
			pos.x = fract(pos.x*0.333333333);

			if      (pos.x < 0.333) mask.r = maskLight;
			else if (pos.x < 0.666) mask.g = maskLight;
			else                    mask.b = maskLight;
		}
	#ifdef EXTRA_MASKS
		// These can cause moire with curvature and scanlines
		// so they're an easy target for freeing up registers

		// Stretched VGA style shadow mask (same as prior shaders).
		else if (shadowMask == 3.0)
		{
			pos.x += pos.y*3.0;
			pos.x  = fract(pos.x*0.166666666);

			if      (pos.x < 0.333) mask.r = maskLight;
			else if (pos.x < 0.666) mask.g = maskLight;
			else                    mask.b = maskLight;
		}

		// VGA style shadow mask.
		else if (shadowMask == 4.0)
		{
			pos.xy  = floor(pos.xy*vec2(1.0, 0.5));
			pos.x  += pos.y*3.0;
			pos.x   = fract(pos.x*0.166666666);

			if      (pos.x < 0.333) mask.r = maskLight;
			else if (pos.x < 0.666) mask.g = maskLight;
			else                    mask.b = maskLight;
		}
	#endif

		else mask = vec3(1.,1.,1.);

		return vec4(mask, 1.0);
	}
#endif

void main()
{
#ifdef CURVATURE
	vec2 pos = Warp(v_texCoord.xy*(rubyTextureSize.xy/rubyInputSize.xy))*(rubyInputSize.xy/rubyTextureSize.xy);
#else
	vec2 pos = v_texCoord.xy;
#endif

#if defined MASK && !defined ROTATE_SCANLINES
	// mask effects look bad unless applied in linear gamma space
	vec4 in_gamma = vec4(monitor_gamma, monitor_gamma, monitor_gamma, 1.0);
	vec4 out_gamma = vec4(1.0 / crt_gamma, 1.0 / crt_gamma, 1.0 / crt_gamma, 1.0);
	vec4 res = pow(BL_TEXTURE(Source, pos), in_gamma);
#else
	vec4 res = BL_TEXTURE(Source, pos);
#endif

#if defined MASK && !defined ROTATE_SCANLINES
	// apply the mask; looks bad with vert scanlines so make them mutually exclusive
	res *= Mask(gl_FragCoord.xy * 1.0001);
#endif

#if defined CURVATURE && defined GL_ES
	// hacky clamp fix for GLES
	vec2 bordertest = (pos);
	if ( bordertest.x > 0.0001 && bordertest.x < 0.9999 && bordertest.y > 0.0001 && bordertest.y < 0.9999)
		res = res;
	else
		res = vec4(0.,0.,0.,0.);
#endif

#if defined MASK && !defined ROTATE_SCANLINES
	// re-apply the gamma curve for the mask path
	FragColor = pow(scanline(pos, res), out_gamma);
#else
	FragColor = scanline(pos, res);
#endif
}
#endif
)GLSL";

#endif // C_OPENGL

#endif
