/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020  The DOSBox Team
 *
 * Contributors:
 *   - 2020, jmarsh <jmarsh@vogons.org>: authored.
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

#pragma use_srgb_texture
#pragma use_srgb_framebuffer

varying vec2 v_texCoord;
#if defined(VERTEX)
uniform vec2 rubyTextureSize;
uniform vec2 rubyInputSize;
attribute vec4 a_position;
void main() {
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize/rubyTextureSize;
}
#elif defined(FRAGMENT)
uniform sampler2D rubyTexture;
void main() {
	gl_FragColor = texture2D(rubyTexture, v_texCoord);
}
#endif
