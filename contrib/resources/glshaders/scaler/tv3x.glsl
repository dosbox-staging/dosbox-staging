/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2006-2020  The DOSBox Team
 *
 * Contributors:
 *   - 2006, Sjoerd van der Berg <harekiet@users.sourceforge.net>: authored
 *           https://svn.code.sf.net/p/dosbox/code-0/dosbox/trunk@2444
 *
 *   - 2020, jmarsh <jmarsh@vogons.org>: converted to OpenGL fragment shader
 *           https://svn.code.sf.net/p/dosbox/code-0/dosbox/trunk@4319
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

varying vec2 v_texCoord;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

#if defined(VERTEX)
attribute vec4 a_position;
void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize;
}

#elif defined(FRAGMENT)
uniform sampler2D rubyTexture;

void main()
{
	vec2 prescale = vec2(3.0);
	vec2 texel = v_texCoord;
	vec2 texel_floored = floor(texel);
	vec2 s = fract(texel);
	vec2 region_range = vec2(0.5) - vec2(0.5) / prescale;

	vec2 center_dist = s - 0.5;
	vec2 f = (center_dist - clamp(center_dist, -region_range, region_range)) * prescale + vec2(0.5);

	vec2 mod_texel = min(texel_floored + f, rubyInputSize - 0.5);
	vec4 p = texture2D(rubyTexture, mod_texel / rubyTextureSize);
	float ss = abs(s.y * 2.0 - 1.0);
	p -= p * ss * 11.0 / 16.0;

	gl_FragColor = vec4(p.rgb, 1.0);
}
#endif
