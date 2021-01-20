/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_RENDER_GLSL_H
#define DOSBOX_RENDER_GLSL_H

#if C_OPENGL

static const char advinterp2x_glsl[] = R"GLSL(
varying vec2 v_texCoord;
uniform sampler2D rubyTexture;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

#if defined(VERTEX)
attribute vec4 a_position;

void main() {
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)*rubyInputSize;
}

#elif defined(FRAGMENT)

vec3 getadvinterp2xtexel(vec2 coord) {
	vec2 base = floor(coord/vec2(2.0))+vec2(0.5);
	vec3 c4 = texture2D(rubyTexture, base/rubyTextureSize).xyz;
	vec3 c1 = texture2D(rubyTexture, (base-vec2(0.0,1.0))/rubyTextureSize).xyz;
	vec3 c7 = texture2D(rubyTexture, (base+vec2(0.0,1.0))/rubyTextureSize).xyz;
	vec3 c3 = texture2D(rubyTexture, (base-vec2(1.0,0.0))/rubyTextureSize).xyz;
	vec3 c5 = texture2D(rubyTexture, (base+vec2(1.0,0.0))/rubyTextureSize).xyz;

	bool outer = c1 != c7 && c3 != c5;
	bool c3c1 = outer && c3==c1;
	bool c1c5 = outer && c1==c5;
	bool c3c7 = outer && c3==c7;
	bool c7c5 = outer && c7==c5;

	vec3 l00 = mix(c3,c4,c3c1?3.0/8.0:1.0);
	vec3 l01 = mix(c5,c4,c1c5?3.0/8.0:1.0);
	vec3 l10 = mix(c3,c4,c3c7?3.0/8.0:1.0);
	vec3 l11 = mix(c5,c4,c7c5?3.0/8.0:1.0);

	coord = max(floor(mod(coord, 2.0)), 0.0);
	/* 2x2 output:
	 *    |x=0|x=1
	 * y=0|l00|l01
	 * y=1|l10|l11
	 */

	return mix(mix(l00,l01,coord.x), mix(l10,l11,coord.x), coord.y);
}

void main()
{
	vec2 coord = v_texCoord;
#if defined(OPENGLNB)
	gl_FragColor = getadvinterp2xtexel(coord);
#else
	coord -= 0.5;
	vec3 c0 = getadvinterp2xtexel(coord);
	vec3 c1 = getadvinterp2xtexel(coord+vec2(1.0,0.0));
	vec3 c2 = getadvinterp2xtexel(coord+vec2(0.0,1.0));
	vec3 c3 = getadvinterp2xtexel(coord+vec2(1.0));

	coord = fract(max(coord,0.0));
	gl_FragColor = vec4(mix(mix(c0,c1,coord.x), mix(c2,c3,coord.x), coord.y), 1.0);
#endif
}
#endif
)GLSL";

static const char advinterp3x_glsl[] = R"GLSL(
varying vec2 v_texCoord;
uniform sampler2D rubyTexture;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

#if defined(VERTEX)
attribute vec4 a_position;

void main() {
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize*3.0;
}

#elif defined(FRAGMENT)

vec3 getadvinterp3xtexel(vec2 coord) {
	vec2 base = floor(coord/vec2(3.0))+vec2(0.5);
	vec3 c0 = texture2D(rubyTexture, (base-vec2(1.0,1.0))/rubyTextureSize).xyz;
	vec3 c1 = texture2D(rubyTexture, (base-vec2(0.0,1.0))/rubyTextureSize).xyz;
	vec3 c2 = texture2D(rubyTexture, (base-vec2(-1.0,1.0))/rubyTextureSize).xyz;
	vec3 c3 = texture2D(rubyTexture, (base-vec2(1.0,0.0))/rubyTextureSize).xyz;
	vec3 c4 = texture2D(rubyTexture, base/rubyTextureSize).xyz;
	vec3 c5 = texture2D(rubyTexture, (base+vec2(1.0,0.0))/rubyTextureSize).xyz;
	vec3 c6 = texture2D(rubyTexture, (base+vec2(-1.0,1.0))/rubyTextureSize).xyz;
	vec3 c7 = texture2D(rubyTexture, (base+vec2(0.0,1.0))/rubyTextureSize).xyz;
	vec3 c8 = texture2D(rubyTexture, (base+vec2(1.0,1.0))/rubyTextureSize).xyz;

	bool outer = c1 != c7 && c3 != c5;

	vec3 l00 = mix(c3,c4,(outer && c3==c1) ? 3.0/8.0:1.0);
	vec3 l01 = (outer && ((c3==c1&&c4!=c2)||(c5==c1&&c4!=c0))) ? c1 : c4;
	vec3 l02 = mix(c5,c4,(outer && c5==c1) ? 3.0/8.0:1.0);
	vec3 l10 = (outer && ((c3==c1&&c4!=c6)||(c3==c7&&c4!=c0))) ? c3 : c4;
	vec3 l11 = c4;
	vec3 l12 = (outer && ((c5==c1&&c4!=c8)||(c5==c7&&c4!=c2))) ? c5 : c4;
	vec3 l20 = mix(c3,c4,(outer && c3==c7) ? 3.0/8.0:1.0);
	vec3 l21 = (outer && ((c3==c7&&c4!=c8)||(c5==c7&&c4!=c6))) ? c7 : c4;
	vec3 l22 = mix(c5,c4,(outer && c5==c7) ? 3.0/8.0:1.0);

	coord = mod(coord, 3.0);
	bvec2 l = lessThan(coord, vec2(1.0));
	bvec2 h = greaterThanEqual(coord, vec2(2.0));

	if (h.x) {
		l01 = l02;
		l11 = l12;
		l21 = l22;
	}
	if (h.y) {
		l10 = l20;
		l11 = l21;
	}
	if (l.x) {
		l01 = l00;
		l11 = l10;
	}
	return l.y ? l01 : l11;
}

void main()
{
	vec2 coord = v_texCoord;
#if defined(OPENGLNB)
	gl_FragColor = getadvinterp3xtexel(coord);
#else
	coord -= 0.5;
	vec3 c0 = getadvinterp3xtexel(coord);
	vec3 c1 = getadvinterp3xtexel(coord+vec2(1.0,0.0));
	vec3 c2 = getadvinterp3xtexel(coord+vec2(0.0,1.0));
	vec3 c3 = getadvinterp3xtexel(coord+vec2(1.0));

	coord = fract(max(coord,0.0));
	gl_FragColor = vec4(mix(mix(c0,c1,coord.x), mix(c2,c3,coord.x), coord.y), 1.0);
#endif
}
#endif
)GLSL";

static const char advmame2x_glsl[] = R"GLSL(
varying vec2 v_texCoord;
uniform sampler2D rubyTexture;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

#if defined(VERTEX)
attribute vec4 a_position;

void main() {
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)*rubyInputSize;
}

#elif defined(FRAGMENT)

vec3 getadvmame2xtexel(vec2 coord) {
	vec2 base = floor(coord/vec2(2.0))+vec2(0.5);
	vec3 c4 = texture2D(rubyTexture, base/rubyTextureSize).xyz;
	vec3 c1 = texture2D(rubyTexture, (base-vec2(0.0,1.0))/rubyTextureSize).xyz;
	vec3 c7 = texture2D(rubyTexture, (base+vec2(0.0,1.0))/rubyTextureSize).xyz;
	vec3 c3 = texture2D(rubyTexture, (base-vec2(1.0,0.0))/rubyTextureSize).xyz;
	vec3 c5 = texture2D(rubyTexture, (base+vec2(1.0,0.0))/rubyTextureSize).xyz;

	bool outer = c1 != c7 && c3 != c5;
	bool c3c1 = outer && c3==c1;
	bool c1c5 = outer && c1==c5;
	bool c3c7 = outer && c3==c7;
	bool c7c5 = outer && c7==c5;

	vec3 l00 = mix(c4,c3,c3c1?1.0:0.0);
	vec3 l01 = mix(c4,c5,c1c5?1.0:0.0);
	vec3 l10 = mix(c4,c3,c3c7?1.0:0.0);
	vec3 l11 = mix(c4,c5,c7c5?1.0:0.0);

	coord = max(floor(mod(coord, 2.0)), 0.0);
	/* 2x2 output:
	 *    |x=0|x=1
	 * y=0|l00|l01
	 * y=1|l10|l11
	 */

	return mix(mix(l00,l01,coord.x), mix(l10,l11,coord.x), coord.y);
}

void main()
{
	vec2 coord = v_texCoord;
#if defined(OPENGLNB)
	gl_FragColor = getadvmame2xtexel(coord);
#else
	coord -= 0.5;
	vec3 c0 = getadvmame2xtexel(coord);
	vec3 c1 = getadvmame2xtexel(coord+vec2(1.0,0.0));
	vec3 c2 = getadvmame2xtexel(coord+vec2(0.0,1.0));
	vec3 c3 = getadvmame2xtexel(coord+vec2(1.0));

	coord = fract(max(coord,0.0));
	gl_FragColor = vec4(mix(mix(c0,c1,coord.x), mix(c2,c3,coord.x), coord.y), 1.0);
#endif
}
#endif
)GLSL";

static const char advmame3x_glsl[] = R"GLSL(
varying vec2 v_texCoord;
uniform sampler2D rubyTexture;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

#if defined(VERTEX)
attribute vec4 a_position;

void main() {
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize*3.0;
}

#elif defined(FRAGMENT)

vec3 getadvmame3xtexel(vec2 coord) {
	vec2 base = floor(coord/vec2(3.0))+vec2(0.5);
	coord = mod(coord, 3.0);
	bvec2 l = lessThan(coord, vec2(1.0));
	bvec2 h = greaterThanEqual(coord, vec2(2.0));
	bvec2 m = equal(l,h);

	vec2 left = vec2(h.x?1.0:-1.0, 0.0);
	vec2 up = vec2(0.0, h.y?1.0:-1.0);
	if (l==h) left.x = 0.0; // hack for center pixel, will ensure outer==false
	if (m.y) {
		// swap
		left -= up;
		up += left;
		left = up - left;
	}

	vec3 c0 = texture2D(rubyTexture, (base+up+left)/rubyTextureSize).xyz;
	vec3 c1 = texture2D(rubyTexture, (base+up)/rubyTextureSize).xyz;
	vec3 c2 = texture2D(rubyTexture, (base+up-left)/rubyTextureSize).xyz;
	vec3 c3 = texture2D(rubyTexture, (base+left)/rubyTextureSize).xyz;
	vec3 c4 = texture2D(rubyTexture, base/rubyTextureSize).xyz;
	vec3 c5 = texture2D(rubyTexture, (base-left)/rubyTextureSize).xyz;
	vec3 c7 = texture2D(rubyTexture, (base-up)/rubyTextureSize).xyz;

	bool outer = c1 != c7 && c3 != c5;
	bool check1 = c3==c1 && (!any(m) || c4!=c2);
	bool check2 = any(m) && c5==c1 && c4!=c0;

	return (outer && (check1 || check2)) ? c1 : c4;
}

void main()
{
	vec2 coord = v_texCoord;
#if defined(OPENGLNB)
	gl_FragColor = getadvmame3xtexel(coord);
#else
	coord -= 0.5;
	vec3 c0 = getadvmame3xtexel(coord);
	vec3 c1 = getadvmame3xtexel(coord+vec2(1.0,0.0));
	vec3 c2 = getadvmame3xtexel(coord+vec2(0.0,1.0));
	vec3 c3 = getadvmame3xtexel(coord+vec2(1.0));

	coord = fract(max(coord,0.0));
	gl_FragColor = vec4(mix(mix(c0,c1,coord.x), mix(c2,c3,coord.x), coord.y), 1.0);
#endif
}
#endif
)GLSL";

static const char rgb2x_glsl[] = R"GLSL(
varying vec2 v_texCoord;
uniform sampler2D rubyTexture;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

#if defined(VERTEX)
attribute vec4 a_position;

void main() {
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize * 2.0;
}

#elif defined(FRAGMENT)

vec4 getRGB2xtexel(vec2 coord) {
	vec2 mid = vec2(0.5);
	vec4 s = texture2D(rubyTexture, (floor(coord/vec2(2.0))+mid)/rubyTextureSize);

	coord = max(floor(mod(coord, 2.0)), 0.0);
	/* 2x2 output:
	 *    |x=0|x=1
	 * y=0| r | g
	 * y=1| b |rgb
	 */

	s.r *= 1.0 - abs(coord.x - coord.y);
	s.g *= coord.x;
	s.b *= coord.y;
	return s;
}

void main()
{
	vec2 coord = v_texCoord;
#if defined(OPENGLNB)
	gl_FragColor = getRGB2xtexel(coord);
#else
	coord -= 0.5;
	vec4 c0 = getRGB2xtexel(coord);
	vec4 c1 = getRGB2xtexel(coord+vec2(1.0,0.0));
	vec4 c2 = getRGB2xtexel(coord+vec2(0.0,1.0));
	vec4 c3 = getRGB2xtexel(coord+vec2(1.0));

	coord = fract(max(coord,0.0));
	gl_FragColor = mix(mix(c0,c1,coord.x), mix(c2,c3,coord.x), coord.y);
#endif
}
#endif
)GLSL";

static const char rgb3x_glsl[] = R"GLSL(
varying vec2 v_texCoord;
uniform sampler2D rubyTexture;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

#if defined(VERTEX)
attribute vec4 a_position;

void main() {
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize * 3.0;
}

#elif defined(FRAGMENT)

vec4 getRGB3xtexel(vec2 coord) {
	vec2 mid = vec2(0.5);
	vec4 s = texture2D(rubyTexture, (floor(coord/vec2(3.0))+mid)/rubyTextureSize);

	coord = max(floor(mod(coord, 3.0)), 0.0);
	/* 3x3 output:
	 *  | l | m | h
	 * l|rgb| g | b
	 * m| g | r |rgb
	 * h|rgb| b | r
	 */
	vec2 l = step(0.0, -coord);
	vec2 m = vec2(1.0) - abs(coord-1.0);
	vec2 h = step(2.0, coord);

	s.r *= l.x + m.y - 2.0*l.x*m.y + h.x*h.y;
	s.g *= l.x + l.y*m.x + h.x*m.y;
	s.b *= l.x*l.y + h.x + h.y - 2.0*h.x*h.y;
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
	vec4 c1 = getRGB3xtexel(coord+vec2(1.0,0.0));
	vec4 c2 = getRGB3xtexel(coord+vec2(0.0,1.0));
	vec4 c3 = getRGB3xtexel(coord+vec2(1.0));

	coord = fract(max(coord,0.0));
	gl_FragColor = mix(mix(c0,c1,coord.x), mix(c2,c3,coord.x), coord.y);
#endif
}
#endif
)GLSL";

static const char scan2x_glsl[] = R"GLSL(
varying vec2 v_texCoord;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

#if defined(VERTEX)
attribute vec4 a_position;
void main() {
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize;
}

#elif defined(FRAGMENT)
uniform sampler2D rubyTexture;

void main() {
	vec2 prescale = vec2(2.0);
	vec2 texel = v_texCoord;
	vec2 texel_floored = floor(texel);
	vec2 s = fract(texel);
	vec2 region_range = vec2(0.5) - vec2(0.5) / prescale;

	vec2 center_dist = s - vec2(0.5);
	vec2 f = (center_dist - clamp(center_dist, -region_range, region_range)) * prescale + vec2(0.5);

	vec2 mod_texel = min(texel_floored + f, rubyInputSize-0.5);
	vec4 p = texture2D(rubyTexture, mod_texel/rubyTextureSize);
	float ss = abs(s.y*2.0-1.0);
	p -= p*ss;

	gl_FragColor = p;
}
#endif
)GLSL";

static const char scan3x_glsl[] = R"GLSL(
varying vec2 v_texCoord;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

#if defined(VERTEX)
attribute vec4 a_position;
void main() {
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize;
}

#elif defined(FRAGMENT)
uniform sampler2D rubyTexture;

void main() {
	vec2 prescale = vec2(3.0);
	vec2 texel = v_texCoord;
	vec2 texel_floored = floor(texel);
	vec2 s = fract(texel);
	vec2 region_range = vec2(0.5) - vec2(0.5) / prescale;

	vec2 center_dist = s - 0.5;
	vec2 f = (center_dist - clamp(center_dist, -region_range, region_range)) * prescale + vec2(0.5);

	vec2 mod_texel = min(texel_floored + f, rubyInputSize-0.5);
	vec4 p = texture2D(rubyTexture, mod_texel/rubyTextureSize);
	float m = s.y*6.0;
	m -= clamp(m, 2.0, 4.0);
	m = abs(m/2.0);
	gl_FragColor = p - p*m;
}
#endif
)GLSL";

static const char tv2x_glsl[] = R"GLSL(
varying vec2 v_texCoord;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

#if defined(VERTEX)
attribute vec4 a_position;
void main() {
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize;
}

#elif defined(FRAGMENT)
uniform sampler2D rubyTexture;

void main() {
	vec2 prescale = vec2(2.0);
	vec2 texel = v_texCoord;
	vec2 texel_floored = floor(texel);
	vec2 s = fract(texel);
	vec2 region_range = vec2(0.5) - vec2(0.5) / prescale;

	vec2 center_dist = s - 0.5;
	vec2 f = (center_dist - clamp(center_dist, -region_range, region_range)) * prescale + vec2(0.5);

	vec2 mod_texel = min(texel_floored + f, rubyInputSize-0.5);
	vec4 p = texture2D(rubyTexture, mod_texel/rubyTextureSize);
	float ss = abs(s.y*2.0-1.0);
	p -= p*ss*3.0/8.0;

	gl_FragColor = p;
}
#endif
)GLSL";

static const char tv3x_glsl[] = R"GLSL(
varying vec2 v_texCoord;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

#if defined(VERTEX)
attribute vec4 a_position;
void main() {
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize;
}

#elif defined(FRAGMENT)
uniform sampler2D rubyTexture;

void main() {
	vec2 prescale = vec2(3.0);
	vec2 texel = v_texCoord;
	vec2 texel_floored = floor(texel);
	vec2 s = fract(texel);
	vec2 region_range = vec2(0.5) - vec2(0.5) / prescale;

	vec2 center_dist = s - 0.5;
	vec2 f = (center_dist - clamp(center_dist, -region_range, region_range)) * prescale + vec2(0.5);

	vec2 mod_texel = min(texel_floored + f, rubyInputSize-0.5);
	vec4 p = texture2D(rubyTexture, mod_texel/rubyTextureSize);
	float ss = abs(s.y*2.0-1.0);
	p -= p*ss*11.0/16.0;

	gl_FragColor = p;
}
#endif
)GLSL";

static const char sharp_glsl[] = R"GLSL(
varying vec2 v_texCoord;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;
varying vec2 prescale; // const set by vertex shader

#if defined(VERTEX)
attribute vec4 a_position;
void main() {
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize;
	prescale = ceil(rubyOutputSize / rubyInputSize);
}

#elif defined(FRAGMENT)
uniform sampler2D rubyTexture;

void main() {
	const vec2 halfp = vec2(0.5);
	vec2 texel_floored = floor(v_texCoord);
	vec2 s = fract(v_texCoord);
	vec2 region_range = halfp - halfp / prescale;

	vec2 center_dist = s - halfp;
	vec2 f = (center_dist - clamp(center_dist, -region_range, region_range)) * prescale + halfp;

	vec2 mod_texel = min(texel_floored + f, rubyInputSize-halfp);
	gl_FragColor = texture2D(rubyTexture, mod_texel / rubyTextureSize);
}
#endif
)GLSL";

#endif // C_OPENGL

#endif
