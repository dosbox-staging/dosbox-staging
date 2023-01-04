#version 120

/*
    zfast_crt - A very simple CRT shader.

    Copyright (C) 2017 Greg Hogan (SoltanGris42)
	edited by metallic 77.

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This file ported from Libretro's GLSL shader zfast-composite.glslp
    to DOSBox-compatible format by Tyrells.

*/

/* 

#pragma parameter blurx "Convergence X-Axis" 0.45 -1.0 2.0 0.05
#pragma parameter blury "Convergence Y-Axis" -0.25 -1.0 1.0 0.05
#pragma parameter HIGHSCANAMOUNT1 "Scanline Amount (Low)" 0.3 0.0 1.0 0.05
#pragma parameter HIGHSCANAMOUNT2 "Scanline Amount (High)" 0.2 0.0 1.0 0.05
#pragma parameter MASK_DARK "Mask Effect Amount" 0.25 0.0 1.0 0.05
#pragma parameter MASK_FADE "Mask/Scanline Fade" 0.8 0.0 1.0 0.05
#pragma parameter sat "Saturation" 1.1 0.0 3.0 0.05

*/

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
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec2 v_texCoord;
COMPAT_VARYING float maskFade;

uniform COMPAT_PRECISION vec2 rubyOutputSize;
uniform COMPAT_PRECISION vec2 rubyTextureSize;
uniform COMPAT_PRECISION vec2 rubyInputSize;

// compatibility #defines
#define vTexCoord v_texCoord.xy
#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize) //either rubyTextureSize or rubyInputSize
#define OutSize vec4(rubyOutputSize, 1.0 / rubyOutputSize)

#ifdef PARAMETER_UNIFORM
// All parameter floats need to have COMPAT_PRECISION in front of them
uniform COMPAT_PRECISION float MASK_FADE;
#else
#define MASK_FADE 0.8
#endif

void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize / rubyTextureSize;
	v_texCoord.xy *= 1.0001;

	maskFade = 0.3333 * MASK_FADE;
}

#elif defined(FRAGMENT)

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

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

uniform COMPAT_PRECISION vec2 rubyOutputSize;
uniform COMPAT_PRECISION vec2 rubyTextureSize;
uniform COMPAT_PRECISION vec2 rubyInputSize;
uniform sampler2D rubyTexture;
COMPAT_VARYING vec2 v_texCoord;
COMPAT_VARYING float maskFade;

// compatibility #defines
#define Source rubyTexture
#define vTexCoord v_texCoord.xy
#define texture(c, d) COMPAT_TEXTURE(c, d)
#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize) //either rubyTextureSize or rubyInputSize
#define OutSize vec4(rubyOutputSize, 1.0 / rubyOutputSize)

#ifdef PARAMETER_UNIFORM
// All parameter floats need to have COMPAT_PRECISION in front of them
uniform COMPAT_PRECISION float blurx;
uniform COMPAT_PRECISION float blury;
uniform COMPAT_PRECISION float HIGHSCANAMOUNT1;
uniform COMPAT_PRECISION float HIGHSCANAMOUNT2;
uniform COMPAT_PRECISION float MASK_DARK;
uniform COMPAT_PRECISION float sat;
#else
#define blurx 0.35
#define blury -0.15
#define HIGHSCANAMOUNT1  0.30
#define HIGHSCANAMOUNT2  0.20
#define MASK_DARK 0.25
#define sat 1.0
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

	These custom implemenations rely on the `rubyTextureSize` uniform variable.
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

#define PI 3.14159

void main()
{
	COMPAT_PRECISION vec2 pos = vTexCoord;

	COMPAT_PRECISION vec3 sample1 = COMPAT_TEXTURE(Source, vec2(pos.x + blurx/1000.0, pos.y - blury/1000.0)).rgb;
	COMPAT_PRECISION vec3 sample2 = COMPAT_TEXTURE(Source, pos).rgb;
	COMPAT_PRECISION vec3 sample3 = COMPAT_TEXTURE(Source, vec2(pos.x - blurx/1000.0, pos.y + blury/1000.0)).rgb;

	COMPAT_PRECISION vec3 colour = vec3(sample1.r*0.5 + sample2.r*0.5, sample1.g*0.25 + sample2.g*0.5 + sample3.g*0.25, sample2.b*0.5 + sample3.b*0.5);
    COMPAT_PRECISION float lum = colour.r*0.4 + colour.g*0.4 + colour.b*0.2;

    COMPAT_PRECISION vec3 lumweight = vec3(0.3, 0.6, 0.1);
    COMPAT_PRECISION float gray = dot(colour, lumweight);
    COMPAT_PRECISION vec3 graycolour = vec3(gray);

	//Gamma-like
	colour *= mix(0.4, 1.0, lum);

	COMPAT_PRECISION float SCANAMOUNT = mix(HIGHSCANAMOUNT1, HIGHSCANAMOUNT2, lum);
	COMPAT_PRECISION float scanLine = SCANAMOUNT * sin(2.0*PI*pos.y * rubyTextureSize.y);

	COMPAT_PRECISION float whichmask = fract(gl_FragCoord.x*-0.4999);
	COMPAT_PRECISION float mask = 1.0 + float(whichmask < 0.5) * -MASK_DARK;

	//Gamma-like
	colour *= mix(2.0, 1.0, lum);

	colour = vec3(mix(graycolour, colour.rgb, sat));

	colour.rgb *= mix(mask*(1.0-scanLine), 1.0-scanLine, dot(colour.rgb,vec3(maskFade)));
	FragColor.rgba = vec4(colour.rgb, 1.0);
}
#endif
