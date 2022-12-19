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
uniform sampler2D rubyTexture;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

#if defined(VERTEX)
attribute vec4 a_position;

void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize * 3.0;
}

#elif defined(FRAGMENT)

vec4 getRGB3xtexel(vec2 coord)
{
	vec2 mid = vec2(0.5);
	vec4 s = texture2D(rubyTexture, (floor(coord / vec2(3.0)) + mid) / rubyTextureSize);

	coord = max(floor(mod(coord, 3.0)), 0.0);
	/* 3x3 output:
	 *  | l | m | h
	 * l|rgb| g | b
	 * m| g | r |rgb
	 * h|rgb| b | r
	 */
	vec2 l = step(0.0, -coord);
	vec2 m = vec2(1.0) - abs(coord - 1.0);
	vec2 h = step(2.0, coord);

	s.r *= l.x + m.y - 2.0 * l.x * m.y + h.x * h.y;
	s.g *= l.x + l.y * m.x + h.x * m.y;
	s.b *= l.x * l.y + h.x + h.y - 2.0 * h.x * h.y;
	return s;
}

void main()
{
	vec2 coord = v_texCoord;
#if defined(OPENGLNB)
	gl_FragColor = getRGB3xtexel(coord);
#else
	coord -= 0.5;
	vec4 c0 = getRGB3xtexel(coord);
	vec4 c1 = getRGB3xtexel(coord + vec2(1.0, 0.0));
	vec4 c2 = getRGB3xtexel(coord + vec2(0.0, 1.0));
	vec4 c3 = getRGB3xtexel(coord + vec2(1.0));

	coord = fract(max(coord, 0.0));
	gl_FragColor = mix(mix(c0, c1, coord.x), mix(c2, c3, coord.x), coord.y);
#endif
}
#endif
