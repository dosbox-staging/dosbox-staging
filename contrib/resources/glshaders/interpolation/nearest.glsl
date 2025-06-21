#version 120

// SPDX-FileCopyrightText:  2020-2024 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2020-2020 Hyllian <sergiogdb@gmail.com>
// SPDX-FileCopyrightText:  2020-2020 jmarsh <jmarsh@vogons.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma use_npot_texture
#pragma use_nearest_texture_filter
#pragma force_single_scan
#pragma force_no_pixel_doubling

varying vec2 v_texCoord;

#if defined(VERTEX)

uniform vec2 rubyTextureSize;
uniform vec2 rubyInputSize;
attribute vec4 a_position;

void main()
{
	gl_Position = a_position;
	v_texCoord  = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 *
	              rubyInputSize / rubyTextureSize;
}

#elif defined(FRAGMENT)

uniform sampler2D rubyTexture;
void main()
{
	gl_FragColor = texture2D(rubyTexture, v_texCoord);
}

#endif
