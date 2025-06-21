#version 120

// SPDX-FileCopyrightText:  2020-2024 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 *	Ported from:
 *	https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
 *
 *	Based on Tyrell Sassen's template from
 *	https://github.com/tyrells/dosbox-svn-shaders
 *
 *	The following code is licensed under the MIT license:
 *	https://gist.github.com/TheRealMJP/bc503b0b87b643d3505d41eab8b332ae
 *
 *	Samples a texture with Catmull-Rom filtering, using 9 texture fetches
 *	instead of 16. See http://vec3.ca/bicubic-filtering-in-fewer-taps/ for
 *	more details
 */

#pragma use_npot_texture
#pragma use_srgb_texture
#pragma use_srgb_framebuffer
#pragma force_single_scan
#pragma force_no_pixel_doubling

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

uniform vec2 rubyTextureSize;
uniform vec2 rubyInputSize;

COMPAT_ATTRIBUTE vec4 a_position;
COMPAT_VARYING vec2 vTexCoord;

void main()
{
	gl_Position = a_position;
	vTexCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize / rubyTextureSize;
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
#define COMPAT_PRECISION highp
#else
#define COMPAT_PRECISION
#endif

uniform vec2 rubyTextureSize;
uniform sampler2D rubyTexture;

COMPAT_VARYING vec2 vTexCoord;

void main()
{
	// We're going to sample a a 4x4 grid of texels surrounding the target
	// UV coordinate. We'll do this by rounding down the sample location to
	// get the exact center of our "starting" texel. The starting texel will
	// be at location [1, 1] in the grid, where [0, 0] is the top left corner.
	vec2 samplePos = vTexCoord * rubyTextureSize;
	vec2 texCoord1 = floor(samplePos - 0.5) + 0.5;

	// Compute the fractional offset from our starting texel to our original
	// sample location, which we'll feed into the Catmull-Rom spline
	// function to get our filter weights.
	vec2 f = samplePos - texCoord1;

	// Compute the Catmull-Rom weights using the fractional offset that we
	// calculated earlier. These equations are pre-expanded based on our
	// knowledge of where the texels will be located, which lets us avoid
	// having to evaluate a piece-wise function.
	vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
	vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
	vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
	vec2 w3 = f * f * (-0.5 + 0.5 * f);

	// Work out weighting factors and sampling offsets that will let us use
	// bilinear filtering to simultaneously evaluate the middle 2 samples
	// from the 4x4 grid.
	vec2 w12 = w1 + w2;
	vec2 offset12 = w2 / (w1 + w2);

	// Compute the final UV coordinates we'll use for sampling the texture
	vec2 texCoord0 = texCoord1 - 1.0;
	vec2 texCoord3 = texCoord1 + 2.0;
	vec2 texCoord12 = texCoord1 + offset12;

	texCoord0 /= rubyTextureSize;
	texCoord3 /= rubyTextureSize;
	texCoord12 /= rubyTextureSize;

	FragColor = COMPAT_TEXTURE(rubyTexture, vec2(texCoord0.x, texCoord0.y)) * w0.x * w0.y
		+ COMPAT_TEXTURE(rubyTexture, vec2(texCoord12.x, texCoord0.y)) * w12.x * w0.y
		+ COMPAT_TEXTURE(rubyTexture, vec2(texCoord3.x, texCoord0.y)) * w3.x * w0.y

		+ COMPAT_TEXTURE(rubyTexture, vec2(texCoord0.x, texCoord12.y)) * w0.x * w12.y
		+ COMPAT_TEXTURE(rubyTexture, vec2(texCoord12.x, texCoord12.y)) * w12.x * w12.y
		+ COMPAT_TEXTURE(rubyTexture, vec2(texCoord3.x, texCoord12.y)) * w3.x * w12.y

		+ COMPAT_TEXTURE(rubyTexture, vec2(texCoord0.x, texCoord3.y)) * w0.x * w3.y
		+ COMPAT_TEXTURE(rubyTexture, vec2(texCoord12.x, texCoord3.y)) * w12.x * w3.y
		+ COMPAT_TEXTURE(rubyTexture, vec2(texCoord3.x, texCoord3.y)) * w3.x * w3.y;
}

#endif
