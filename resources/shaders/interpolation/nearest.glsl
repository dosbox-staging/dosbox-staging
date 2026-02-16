#version 330 core

// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2020-2020 Hyllian <sergiogdb@gmail.com>
// SPDX-FileCopyrightText:  2020-2020 jmarsh <jmarsh@vogons.org>
// SPDX-License-Identifier: GPL-2.0-or-later

/*

#pragma use_nearest_texture_filter
#pragma force_single_scan
#pragma force_no_pixel_doubling

*/

#if defined(VERTEX)

uniform vec2 rubyTextureSize;
uniform vec2 rubyInputSize;

layout (location = 0) in vec2 a_position;

out vec2 v_texCoord;

void main()
{
	gl_Position = vec4(a_position, 0.0, 1.0);

	v_texCoord = vec2(a_position.x + 1.0, a_position.y + 1.0) / 2.0 *
	             rubyInputSize / rubyTextureSize;
}

#elif defined(FRAGMENT)

in vec2 v_texCoord;

out vec4 FragColor;

uniform sampler2D rubyTexture;

void main()
{
	FragColor = texture(rubyTexture, v_texCoord);
}

#endif
