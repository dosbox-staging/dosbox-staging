#version 330 core

// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2011-2022 Hyllian <sergiogdb@gmail.com>
// SPDX-License-Identifier: MIT

// Hyllian's checkerboard dedither shader - linearize pass
//
// Converted to GLSL from the libretro Slang sources
//

/*

#pragma name        CheckerboardDedither_Linearize
#pragma output_size VideoMode

#pragma use_nearest_texture_filter

#pragma parameter CD_USE_GAMMA "Gamma Slider" 1.0  0.0 1.0  0.1

*/

//////////////////////////////////////////////////////////////////////////////

#define InputGamma       (CD_USE_GAMMA + 1.0)
#define GAMMA_IN(color)  pow(color, vec3(InputGamma, InputGamma, InputGamma))

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

uniform float CD_USE_GAMMA;

void main()
{
	FragColor = vec4(GAMMA_IN(texture(INPUT_TEXTURE_0, v_texCoord).rgb), 1.0);
}

#endif
