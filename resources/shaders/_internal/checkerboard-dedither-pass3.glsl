#version 330 core

// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2011-2022 Hyllian <sergiogdb@gmail.com>
// SPDX-License-Identifier: MIT

// Hyllian's checkerboard dedither shader - pass3
//
// Converted to GLSL from the libretro Slang sources
//

/*

#pragma name        CheckerboardDedither_Pass3
#pragma input0      Previous
#pragma input1      CheckerboardDedither_Linearize
#pragma output_size VideoMode

#pragma use_nearest_texture_filter

#pragma parameter CD_MITIG_NEIGHBOURS "Mitigate Errors (neighbors)" 1.0  0.0 4.0  1.0
#pragma parameter CD_MITIG_LINES      "Mitigate Errors (regions)"   1.0  0.0 1.0  1.0
#pragma parameter CD_ADJUST_VIEW      "Adjust View"                 0.0  0.0 1.0  1.0
#pragma parameter CD_USE_GAMMA        "Gamma Slider"                1.0  0.0 1.0  0.1

*/

//////////////////////////////////////////////////////////////////////////////

#define OuputGamma        (CD_USE_GAMMA + 1.0)
#define GAMMA_OUT(color)  pow(color, vec3(1.0 / OuputGamma, 1.0 / OuputGamma, 1.0 / OuputGamma))
#define PATTERN(A)        step(Delta,A)

const float Delta = 0.000000001;
const vec3 Y      = vec3(0.299, 0.587, 0.114);

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

uniform vec2 INPUT_TEXTURE_SIZE_0;
uniform sampler2D INPUT_TEXTURE_0;
uniform sampler2D INPUT_TEXTURE_1;

uniform float CD_MITIG_NEIGHBOURS;
uniform float CD_MITIG_LINES;
uniform float CD_ADJUST_VIEW;
uniform float CD_USE_GAMMA;

void main()
{
	vec2 ps = vec2(1.0) / INPUT_TEXTURE_SIZE_0.xy;

	vec2 dx = vec2(1.0, 0.0) * ps.xy;
	vec2 dy = vec2(0.0, 1.0) * ps.xy;

    // Reading the texels
    vec4 C  = texture(INPUT_TEXTURE_0, v_texCoord        );
    vec4 L  = texture(INPUT_TEXTURE_0, v_texCoord -dx    );
    vec4 R  = texture(INPUT_TEXTURE_0, v_texCoord +dx    );
    vec4 U  = texture(INPUT_TEXTURE_0, v_texCoord     -dy);
    vec4 D  = texture(INPUT_TEXTURE_0, v_texCoord     +dy);
    vec4 UL = texture(INPUT_TEXTURE_0, v_texCoord -dx -dy);
    vec4 UR = texture(INPUT_TEXTURE_0, v_texCoord +dx -dy);
    vec4 DL = texture(INPUT_TEXTURE_0, v_texCoord -dx +dy);
    vec4 DR = texture(INPUT_TEXTURE_0, v_texCoord +dx +dy);

    vec4 L2 = texture(INPUT_TEXTURE_0, v_texCoord -2.0*dx    );
    vec4 R2 = texture(INPUT_TEXTURE_0, v_texCoord +2.0*dx    );
    vec4 U2 = texture(INPUT_TEXTURE_0, v_texCoord     -2.0*dy);
    vec4 D2 = texture(INPUT_TEXTURE_0, v_texCoord     +2.0*dy);

    vec4 L3 = texture(INPUT_TEXTURE_0, v_texCoord -3.0*dx    );
    vec4 R3 = texture(INPUT_TEXTURE_0, v_texCoord +3.0*dx    );
    vec4 U3 = texture(INPUT_TEXTURE_0, v_texCoord     -3.0*dy);
    vec4 D3 = texture(INPUT_TEXTURE_0, v_texCoord     +3.0*dy);

    vec3 color = C.rgb;
    vec3 oriC  = texture(INPUT_TEXTURE_1, v_texCoord).rgb;

    float count  = 0.0;
    float count2 = 0.0;
    float count3 = 0.0;

    //              U3
    //              U2
    //          UL   U  UR
    //  L3  L2   L   C   R   R2  R3
    //          DL   D  DR
    //              D2
    //              D3

    count += PATTERN(L.a);
    count += PATTERN(R.a);
    count += PATTERN(U.a);
    count += PATTERN(D.a);
    count += PATTERN(UL.a*UR.a*DL.a*DR.a);

    count2 += PATTERN(L.a*UL.a*U.a + U.a*UR.a*R.a + R.a*DR.a*D.a + D.a*DL.a*L.a);

    count3 += PATTERN(L3.a*L2.a*L.a);
    count3 += PATTERN(L2.a*L.a*R.a);
    count3 += PATTERN(L.a*R.a*R2.a);
    count3 += PATTERN(R.a*R2.a*R3.a);

    count3 += PATTERN(U3.a*U2.a*U.a);
    count3 += PATTERN(U2.a*U.a*D.a);
    count3 += PATTERN(U.a*D.a*D2.a);
    count3 += PATTERN(D.a*D2.a*D3.a);

    if ((count < CD_MITIG_NEIGHBOURS) && (count2 < 1.0)) {
	    color = oriC;
    } else if ((CD_MITIG_LINES == 1.0) && (count3 < 1.0)) {
	    color = oriC;
    }

    float luma_diff = abs(dot(oriC, Y) - dot(color, Y));

    color = mix(color, vec3(luma_diff), CD_ADJUST_VIEW);

    FragColor = vec4(GAMMA_OUT(color), 1.0);
}

#endif
