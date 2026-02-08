#version 330 core

// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2011-2022 Hyllian <sergiogdb@gmail.com>
// SPDX-License-Identifier: MIT

// Hyllian's checkerboard dedither shader - pass2
//
// Converted to GLSL from the libretro Slang sources
//

/*

#pragma name        CheckerboardDedither_Pass2
#pragma input0      Previous
#pragma input1      CheckerboardDedither_Linearize
#pragma output_size VideoMode

#pragma use_nearest_texture_filter

#pragma parameter CD_BLEND_LEVEL "Blend Level" 1.0  0.0 1.0  0.1

*/

//////////////////////////////////////////////////////////////////////////////

#define PATTERN(A) step(Delta, A)

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

uniform float CD_BLEND_LEVEL;

void main()
{
	vec2 ps = vec2(1.0) / INPUT_TEXTURE_SIZE_0.xy;

	vec2 dx = vec2(1.0, 0.0) * ps.xy;
	vec2 dy = vec2(0.0, 1.0) * ps.xy;

	// Reading the texels
    vec4 C =  texture(INPUT_TEXTURE_0, v_texCoord        );
    vec4 L =  texture(INPUT_TEXTURE_0, v_texCoord -dx    );
    vec4 R =  texture(INPUT_TEXTURE_0, v_texCoord +dx    );
    vec4 U =  texture(INPUT_TEXTURE_0, v_texCoord     -dy);
    vec4 D =  texture(INPUT_TEXTURE_0, v_texCoord     +dy);
    vec4 UL = texture(INPUT_TEXTURE_0, v_texCoord -dx -dy);
    vec4 UR = texture(INPUT_TEXTURE_0, v_texCoord +dx -dy);
    vec4 DL = texture(INPUT_TEXTURE_0, v_texCoord -dx +dy);
    vec4 DR = texture(INPUT_TEXTURE_0, v_texCoord +dx +dy);

    vec4 L2 =  texture(INPUT_TEXTURE_0, v_texCoord -2.0*dx    );
    vec4 R2 =  texture(INPUT_TEXTURE_0, v_texCoord +2.0*dx    );
    vec4 U2 =  texture(INPUT_TEXTURE_0, v_texCoord     -2.0*dy);
    vec4 D2 =  texture(INPUT_TEXTURE_0, v_texCoord     +2.0*dy);

    vec4 UL2 =  texture(INPUT_TEXTURE_0, v_texCoord -2.0*dx -dy);
    vec4 UR2 =  texture(INPUT_TEXTURE_0, v_texCoord +2.0*dx -dy);
    vec4 DL2 =  texture(INPUT_TEXTURE_0, v_texCoord -2.0*dx +dy);
    vec4 DR2 =  texture(INPUT_TEXTURE_0, v_texCoord +2.0*dx +dy);

    vec4 LU2 =  texture(INPUT_TEXTURE_0, v_texCoord -dx -2.0*dy);
    vec4 RU2 =  texture(INPUT_TEXTURE_0, v_texCoord +dx -2.0*dy);
    vec4 LD2 =  texture(INPUT_TEXTURE_0, v_texCoord -dx +2.0*dy);
    vec4 RD2 =  texture(INPUT_TEXTURE_0, v_texCoord +dx -2.0*dy);

    vec3 color =  C.rgb;

    vec3 oriC = texture(INPUT_TEXTURE_1, v_texCoord        ).rgb;
    vec3 oriL = texture(INPUT_TEXTURE_1, v_texCoord -dx    ).rgb;
    vec3 oriR = texture(INPUT_TEXTURE_1, v_texCoord +dx    ).rgb;
    vec3 oriU = texture(INPUT_TEXTURE_1, v_texCoord     -dy).rgb;
    vec3 oriD = texture(INPUT_TEXTURE_1, v_texCoord     +dy).rgb;

	float count1 = 0.0;
	float count2 = 0.0;

	float diff = (1.0 - CD_BLEND_LEVEL);

	//      LU2  U2  RU2
	//  UL2  UL   U  UR  UR2
	//   L2   L   C   R   R2
	//  DL2  DL   D  DR  DR2
	//      LD2  D2  RD2

    count1 += PATTERN(L.a*D.a*R.a);
    count1 += PATTERN(L.a*U.a*R.a);
    count1 += PATTERN(L.a*UL.a*R.a*UR.a + L.a*DL.a*R.a*DR.a);

    count1 += PATTERN(U.a*D2.a*(UL.a*LD2.a + UR.a*RD2.a) + U2.a*D.a*(LU2.a*DL.a + RU2.a*DR.a));

    count2 += PATTERN(U.a*L.a*D.a);
    count2 += PATTERN(U.a*R.a*D.a);
    count2 += PATTERN(U.a*UR.a*D.a*DR.a + D.a*DL.a*U.a*UL.a);

    count2 += PATTERN(L.a*R2.a*(DL.a*DR2.a + UL.a*UR2.a) + L2.a*R.a*(DL2.a*DR.a + UL2.a*UR.a));

	if ((count1 * count2) > 0.0 && count1 == count2) {
		color = 0.5 * (1.0 + diff) * oriC + 0.125 * (1.0 - diff) * (oriL + oriR + oriU + oriD);

	} else if (count1 > 0.0 && count1 > count2) {
		color = 0.5 * (1.0 + diff) * oriC + 0.25 * (1.0 - diff) * (oriL + oriR);

	} else if (count2 > 0.0 && count2 > count1) {
		color = 0.5 * (1.0 + diff) * oriC + 0.25 * (1.0 - diff) * (oriU + oriD);
	}

	float luma_diff = abs(dot(oriC, Y) - dot(color, Y));

	FragColor = vec4(color, luma_diff);
}

#endif
