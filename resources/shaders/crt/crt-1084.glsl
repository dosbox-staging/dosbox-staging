#version 330 core

// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2026-2026 MagnusPerotinus
// SPDX-FileCopyrightText:  2016-2026 Mattias Gustavsson (crtemu), Amiberry team (1084 variant)
// SPDX-License-Identifier: MIT
//
// Commodore 1084S monitor emulation, ported from the built-in '1084' shader of
// the Amiberry Amiga emulator (CRTEMU_TYPE_1084 in Amiberry's crtemu.h; MIT /
// public domain dual license).
//
// This is a multi-pass shader. The pre-passes run first, in this order:
//
//   1. Blur the previous frame's persistence buffer horizontally...
//   2. ...and vertically
//   3. Accumulate: the brighter of the new frame and the blurred afterglow
//      times PHOSPHOR_PERSISTENCE (this buffer is read back next frame)
//   4. Blend: the brighter of the new frame and the persistence buffer times
//      GHOSTING
//   5. Blur the blended image horizontally...
//   6. ...and vertically (input of the halation)
//
// This final pass then applies the nearly flat screen curvature, RGB
// misconvergence, halation, warm colour temperature, contrast curve,
// vignette, brightness-dependent scanlines, aperture grille, filmic tone
// mapping, analog noise, and flicker.
//
// The default parameter values reproduce the Amiberry shader, except that the
// screen curvature doesn't crop the edges of the image (see curve()). With
// SCANLINE_LOCK = 0 (Amiberry's look) the scanlines have a fixed pitch of
// about 4 screen pixels and drift slowly, independent of the emulated video
// mode; with SCANLINE_LOCK = 1 there is one static scanline per emulated
// line.

/*
#pragma name        Main_Pass1
#pragma output_size Viewport

#pragma input0 Crt1084_Blend
#pragma input1 Crt1084_GhostBlurV

#pragma wrap_mode0 ClampToBorder
#pragma wrap_mode1 ClampToBorder

#pragma pre_pass _internal/crt-1084/feedback-blur-h
#pragma pre_pass _internal/crt-1084/feedback-blur-v
#pragma pre_pass _internal/crt-1084/accumulate
#pragma pre_pass _internal/crt-1084/blend
#pragma pre_pass _internal/crt-1084/ghost-blur-h
#pragma pre_pass _internal/crt-1084/ghost-blur-v

#pragma parameter PHOSPHOR_PERSISTENCE "Phosphor persistence"         0.96  0.0  1.0  0.01
#pragma parameter GHOSTING             "Ghosting"                     0.24  0.0  1.0  0.01
#pragma parameter CURVATURE            "Curvature"                    0.20  0.0  1.0  0.05
#pragma parameter CONVERGENCE          "RGB misconvergence"           1.00  0.0  4.0  0.10
#pragma parameter HALATION             "Halation"                     0.12  0.0  1.0  0.01
#pragma parameter SCANLINE_STRENGTH    "Scanline strength"            1.00  0.0  1.0  0.05
#pragma parameter SCANLINE_LOCK        "Lock scanlines to video mode" 0.0   0.0  1.0  1.0
#pragma parameter MASK_STRENGTH        "Aperture grille strength"     0.35  0.0  1.0  0.05
#pragma parameter NOISE                "Analog noise"                 0.008 0.0  0.1  0.001
#pragma parameter FLICKER              "Flicker"                      0.001 0.0  0.1  0.001
*/

#if defined(VERTEX)

layout (location = 0) in vec2 a_position;

out vec2 v_texCoord;

void main()
{
	gl_Position = vec4(a_position, 0.0, 1.0);
	v_texCoord  = vec2(a_position.x + 1.0, a_position.y + 1.0) / 2.0;
}

#elif defined(FRAGMENT)

in vec2 v_texCoord;

out vec4 FragColor;

uniform sampler2D INPUT_TEXTURE_0;
uniform sampler2D INPUT_TEXTURE_1;
uniform vec2 INPUT_SIZE_0;
uniform vec2 OUTPUT_SIZE;
uniform float TIME_SECONDS;

uniform float PHOSPHOR_PERSISTENCE;
uniform float GHOSTING;
uniform float CURVATURE;
uniform float CONVERGENCE;
uniform float HALATION;
uniform float SCANLINE_STRENGTH;
uniform float SCANLINE_LOCK;
uniform float MASK_STRENGTH;
uniform float NOISE;
uniform float FLICKER;

const float Pi = 3.14159265;

vec3 tsample(sampler2D samp, vec2 tc)
{
	vec3 s = pow(abs(texture(samp, tc).rgb), vec3(2.2));
	return s * vec3(1.25);
}

vec3 filmic(vec3 linear_color)
{
	vec3 x = max(vec3(0.0), linear_color - vec3(0.004));
	return (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
}

// Barrel distortion without crtemu's overscan zoom (uv * 1.04, then
// uv * 0.92 + 0.04): that hides the edges of the image like a real monitor,
// which is fine for Amiga screens with their borders, but DOS programs draw
// right up to the edges (e.g., the first column of text). The midpoints of
// the screen edges map exactly to the image edges, so the whole image stays
// visible; only the corners round off into black.
vec2 curve(vec2 uv)
{
	uv = (uv - 0.5) * 2.0;
	uv.x *= 1.0 + pow((abs(uv.y) / 8.0), 2.0);
	uv.y *= 1.0 + pow((abs(uv.x) / 7.0), 2.0);
	return (uv / 2.0) + 0.5;
}

float rand(vec2 co)
{
	return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
	// crtemu runs its time 1.5 times faster than the real time
	float time = 1.5 * TIME_SECONDS;

	// crtemu's 'resolution' and 'size' uniforms are both the size of the
	// image on the screen here
	vec2 resolution = OUTPUT_SIZE;
	vec2 size       = OUTPUT_SIZE;

	vec2 uv = v_texCoord;

	// Barrel distortion - 1084S nearly flat screen
	vec2 curved_uv = mix(curve(uv), uv, 1.0 - CURVATURE);
	vec2 scuv      = curved_uv;

	// Edge-dependent RGB convergence
	vec2 center_dist  = curved_uv - 0.5;
	float edge_factor = dot(center_dist, center_dist) * 4.0;

	// Horizontal instability (much less than TV - stable monitor sync)
	float x = sin(0.1 * time + curved_uv.y * 13.0) *
	          sin(0.23 * time + curved_uv.y * 19.0) *
	          sin(0.3 + 0.11 * time + curved_uv.y * 23.0) * 0.0012;

	float o = sin(gl_FragCoord.y * 1.5) / resolution.x;
	x       = x * 0.06 + o * 0.06;

	// Sample RGB with edge-dependent convergence
	vec3 col;
	float cs = 0.0004 * CONVERGENCE;

	col.r = tsample(INPUT_TEXTURE_0,
	                vec2(x + scuv.x + cs * (1.0 + 2.0 * edge_factor),
	                     scuv.y + cs * (0.7 + 1.2 * edge_factor))).x + 0.02;

	col.g = tsample(INPUT_TEXTURE_0, vec2(x + scuv.x, scuv.y)).y + 0.02;

	col.b = tsample(INPUT_TEXTURE_0,
	                vec2(x + scuv.x - cs * (1.2 + 1.5 * edge_factor),
	                     scuv.y + cs * (0.5 + 1.0 * edge_factor))).z + 0.02;

	// Halation: bright areas glow through CRT glass
	vec3 blur_sample    = tsample(INPUT_TEXTURE_1, scuv);
	float blur_luma     = dot(blur_sample, vec3(0.299, 0.587, 0.114));
	vec3 halation_color = blur_sample * vec3(1.1, 1.0, 0.85);

	col += halation_color * HALATION * smoothstep(0.15, 0.8, blur_luma);
	col += blur_sample * 0.04;

	// 1084S warm color temperature
	col *= vec3(1.08, 1.02, 0.88);

	// Level adjustment (contrast curve)
	col = clamp(col * 1.2 + 0.65 * col * col + 1.1 * col * col * col * col * col,
	            vec3(0.0),
	            vec3(10.0));

	// Vignette - mild for monitor
	float vig = (0.3 + 1.0 * 16.0 * curved_uv.x * curved_uv.y *
	                           (1.0 - curved_uv.x) * (1.0 - curved_uv.y));
	vig = 1.2 * pow(vig, 0.4);
	col *= vig;

	// Brightness-dependent scanlines
	float luma = dot(col, vec3(0.299, 0.587, 0.114));

	float free_phase   = 1.5 * time + curved_uv.y * size.y * 1.5;
	float locked_phase = 2.0 * Pi * curved_uv.y * INPUT_SIZE_0.y - 0.5 * Pi;
	float phase        = mix(free_phase, locked_phase, step(0.5, SCANLINE_LOCK));

	float scans = clamp(0.35 + 0.35 * sin(phase), 0.0, 1.0);
	float s     = pow(scans, 0.9);

	float scanline_reduction = smoothstep(0.0, 0.7, luma);
	s = mix(s, 1.0, scanline_reduction * 0.65);
	s = mix(1.0, s, SCANLINE_STRENGTH);
	col *= s;

	// Aperture grille: vertical RGB phosphor stripes (Trinitron-style)
	float stripe  = mod(gl_FragCoord.x, 3.0);
	float dark    = 1.0 - MASK_STRENGTH;
	vec3 grille_mask;
	if (stripe < 1.0) {
		grille_mask = vec3(1.0, dark, dark);
	} else if (stripe < 2.0) {
		grille_mask = vec3(dark, 1.0, dark);
	} else {
		grille_mask = vec3(dark, dark, 1.0);
	}
	col *= grille_mask;

	// Filmic tone mapping
	col = filmic(col);

	// Subtle analog noise
	vec2 seed = curved_uv * resolution.xy;
	col -= NOISE * pow(vec3(rand(seed + time),
	                        rand(seed + time * 2.0),
	                        rand(seed + time * 3.0)),
	                   vec3(1.5));

	// Minimal flicker
	col *= (1.0 - FLICKER * (sin(50.0 * time + curved_uv.y * 2.0) * 0.5 + 0.5));

	// Boundary clamp
	if (curved_uv.x < 0.0 || curved_uv.x > 1.0) {
		col *= 0.0;
	}
	if (curved_uv.y < 0.0 || curved_uv.y > 1.0) {
		col *= 0.0;
	}

	FragColor = vec4(col, 1.0);
}

#endif
