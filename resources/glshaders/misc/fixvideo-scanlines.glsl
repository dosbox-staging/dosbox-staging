#version 120

// SPDX-FileCopyrightText:  2020-2024 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2015-2015 Ove Kaaven
// SPDX-License-Identifier: GPL-2.0-or-later

// Adaptation of the old fixvideo.fx Daum shader originally written by Ove
// Kavven with some improvements and an intelligent scanline-doubling method
// added by John Novak.

// Deinterlace method
// ------------------
//   0.0 = Mode 0: vertical average + brightness-boost
//   1.0 = Mode 1: intelligent scanline-doubling
#define DEINTERLACE_METHOD 1.0

// Mode 0 params
// -------------
//   Brightness boost (0.00 to 1.00)
#define BRIGHT_BOOST 0.50

// Mode 1 params
// -------------
//   4 sample points to check if the scanline above the current is black;
//   might need to tweak it a bit per game
//   0.00 = leftmost pixel; 0.50 = middle pixel; 1.0 = rightmost pixel
#define SAMPLE_POINT_1 0.00
#define SAMPLE_POINT_2 0.25
#define SAMPLE_POINT_3 0.75
#define SAMPLE_POINT_4 1.00

//   Intensity of the deinterlaced odd & even scanlines.
//   Use values above 1.0 for extra brightness boost.
//   1.00 = full intensity, 0.0 = zero intensity (black)
#define ODD_LINE_INTENSITY  1.10
#define EVEN_LINE_INTENSITY 0.90


#define texCoord v_texCoord

#if defined(VERTEX)

#if __VERSION__ >= 130
#define OUT out
#define IN  in
#define tex2D texture
#else
#define OUT varying
#define IN attribute
#define tex2D texture2D
#endif

#ifdef GL_ES
#define PRECISION mediump
#else
#define PRECISION
#endif

IN  vec4 a_position;
IN  vec4 Color;
IN  vec2 TexCoord;
OUT vec4 color;
OUT vec2 texCoord;

uniform PRECISION vec2 rubyInputSize;
uniform PRECISION vec2 rubyOutputSize;
uniform PRECISION vec2 rubyTextureSize;

void main() {
    gl_Position = a_position;
    texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize / rubyTextureSize;
}


#elif defined(FRAGMENT)

#if __VERSION__ >= 130
#define IN in
#define tex2D texture
out vec4 FragColor;
#else
#define IN varying
#define FragColor gl_FragColor
#define tex2D texture2D
#endif

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#define PRECISION mediump
#else
#define PRECISION
#endif

IN vec2 texCoord;

uniform PRECISION vec2 rubyInputSize;
uniform PRECISION vec2 rubyOutputSize;
uniform PRECISION vec2 rubyTextureSize;
uniform sampler2D s_p;

void main() {
    vec2 dx = vec2(1.0/rubyTextureSize.x, 0.0);
    vec2 dy = vec2(0.0, 1.0/rubyTextureSize.y);

    vec2 pix_coord = texCoord.xy * rubyTextureSize + vec2(-0.5, 0.5);
    vec2 tex_center = (floor(pix_coord) + vec2(0.5, 0.5)) / rubyTextureSize;

    vec4 c00 = tex2D(s_p, tex_center - dy);
    vec4 c01 = tex2D(s_p, tex_center);
    vec4 c02 = tex2D(s_p, tex_center + dy);
    vec4 color;

    if (DEINTERLACE_METHOD == 0.0) {
        color = (c00 + c01*2.0 + c02) / (3.0 - BRIGHT_BOOST);

    } else {
        vec4 c11 = tex2D(s_p, vec2(SAMPLE_POINT_1, tex_center.y));
        vec4 c21 = tex2D(s_p, vec2(SAMPLE_POINT_2, tex_center.y));
        vec4 c31 = tex2D(s_p, vec2(SAMPLE_POINT_3, tex_center.y));
        vec4 c41 = tex2D(s_p, vec2(SAMPLE_POINT_4, tex_center.y));

        vec4 sum1 = c01 + c11 + c21 + c31 + c41;

        vec4 c12 = tex2D(s_p, vec2(SAMPLE_POINT_1, (tex_center + dy).y));
        vec4 c22 = tex2D(s_p, vec2(SAMPLE_POINT_2, (tex_center + dy).y));
        vec4 c32 = tex2D(s_p, vec2(SAMPLE_POINT_3, (tex_center + dy).y));
        vec4 c42 = tex2D(s_p, vec2(SAMPLE_POINT_4, (tex_center + dy).y));

        vec4 sum2 = c02 + c12 + c22 + c32 + c42;

        vec4 odd_color  = c01 * ODD_LINE_INTENSITY;
        vec4 even_color = c00 * EVEN_LINE_INTENSITY;

        color = mix(
            mix(
                c01,
                even_color,
                vec4(step(1.0, 1.0 - (sum1.r + sum1.g + sum1.b)))
            ),
            odd_color,
            vec4(step(1.0, 1.0 - (sum2.r + sum2.g + sum2.b)))
        );

    }

    FragColor = vec4(color.rgb, 1.0);
}

#endif

