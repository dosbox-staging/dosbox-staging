#version 330 core

// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2011-2016 Hyllian <sergiogdb@gmail.com>
// SPDX-License-Identifier: MIT

// Hyllian's xBR-lv2-noblend Shader

/*

#pragma use_nearest_texture_filter
#pragma force_single_scan
#pragma force_no_pixel_doubling

#pragma parameter XBR_EQ_THRESHOLD "Eq Threshold" 0.6 0.0 2.0 0.1
#pragma parameter XBR_LV2_COEFFICIENT "Lv2 Coefficient" 2.0 1.0 3.0 0.1
#pragma parameter XBR_CORNER_TYPE "Corner Calculation" 1.0 1.0 4.0 1.0

*/

#define mul(a,b) (b*a)

#if defined(VERTEX)

layout (location = 0) in vec2 a_position;

out vec2 v_texCoord;
out vec4 t1;
out vec4 t2;
out vec4 t3;
out vec4 t4;
out vec4 t5;
out vec4 t6;
out vec4 t7;

uniform vec2 rubyTextureSize;
uniform vec2 rubyInputSize;

void main()
{
	gl_Position = vec4(a_position, 0.0, 1.0);

	float dx = (1.0/rubyTextureSize.x);
	float dy = (1.0/rubyTextureSize.y);

	vec2 TexCoord = vec2(a_position.x + 1.0, a_position.y + 1.0) / 2.0 *
	                rubyInputSize / rubyTextureSize;

	v_texCoord     = TexCoord;
	v_texCoord.x *= 1.00000001;

	t1 = TexCoord.xxxy + vec4( -dx, 0, dx,-2.0*dy); // A1 B1 C1
	t2 = TexCoord.xxxy + vec4( -dx, 0, dx,    -dy); //  A  B  C
	t3 = TexCoord.xxxy + vec4( -dx, 0, dx,      0); //  D  E  F
	t4 = TexCoord.xxxy + vec4( -dx, 0, dx,     dy); //  G  H  I
	t5 = TexCoord.xxxy + vec4( -dx, 0, dx, 2.0*dy); // G5 H5 I5
	t6 = TexCoord.xyyy + vec4(-2.0*dx,-dy, 0,  dy); // A0 D0 G0
	t7 = TexCoord.xyyy + vec4( 2.0*dx,-dy, 0,  dy); // C4 F4 I4
}


#elif defined(FRAGMENT)

in vec2 v_texCoord;
in vec4 t1;
in vec4 t2;
in vec4 t3;
in vec4 t4;
in vec4 t5;
in vec4 t6;
in vec4 t7;

out vec4 FragColor;

uniform vec2 rubyTextureSize;
uniform sampler2D rubyTexture;

uniform float XBR_EQ_THRESHOLD;
uniform float XBR_LV2_COEFFICIENT;
uniform float XBR_CORNER_TYPE;

const vec3 Y = vec3(0.2126, 0.7152, 0.0722);

// Difference between vector components.
vec4 df(vec4 A, vec4 B)
{
	return vec4(abs(A-B));
}

// Compare two vectors and return their components are different.
vec4 diff(vec4 A, vec4 B)
{
	return vec4(notEqual(A, B));
}

// Determine if two vector components are equal based on a threshold.
vec4 eq(vec4 A, vec4 B)
{
	return (step(df(A, B), vec4(XBR_EQ_THRESHOLD)));
}

// Determine if two vector components are NOT equal based on a threshold.
vec4 neq(vec4 A, vec4 B)
{
	return (vec4(1.0, 1.0, 1.0, 1.0) - eq(A, B));
}

// Weighted distance.
vec4 wd(vec4 a, vec4 b, vec4 c, vec4 d, vec4 e, vec4 f, vec4 g, vec4 h)
{
	return (df(a,b) + df(a,c) + df(d,e) + df(d,f) + 4.0*df(g,h));
}

vec4 weighted_distance(vec4 a, vec4 b, vec4 c, vec4 d, vec4 e, vec4 f, vec4 g, vec4 h, vec4 i, vec4 j, vec4 k, vec4 l)
{
	return (df(a,b) + df(a,c) + df(d,e) + df(d,f) + df(i,j) + df(k,l) + 2.0*df(g,h));
}

float c_df(vec3 c1, vec3 c2)
{
	vec3 df = abs(c1 - c2);
	return df.r + df.g + df.b;
}

void main()
{
	vec4 edri, edr, edr_l, edr_u; // px = pixel, edr = edge detection rule
	vec4 irlv1, irlv2l, irlv2u, block_3d;
	bvec4 nc, px;
	vec4 fx, fx_l, fx_u; // inequations of straight lines.

	vec2 fp  = fract(v_texCoord*rubyTextureSize);

	vec3 A1 = texture(rubyTexture, t1.xw ).xyz;
	vec3 B1 = texture(rubyTexture, t1.yw ).xyz;
	vec3 C1 = texture(rubyTexture, t1.zw ).xyz;
	vec3 A  = texture(rubyTexture, t2.xw ).xyz;
	vec3 B  = texture(rubyTexture, t2.yw ).xyz;
	vec3 C  = texture(rubyTexture, t2.zw ).xyz;
	vec3 D  = texture(rubyTexture, t3.xw ).xyz;
	vec3 E  = texture(rubyTexture, t3.yw ).xyz;
	vec3 F  = texture(rubyTexture, t3.zw ).xyz;
	vec3 G  = texture(rubyTexture, t4.xw ).xyz;
	vec3 H  = texture(rubyTexture, t4.yw ).xyz;
	vec3 I  = texture(rubyTexture, t4.zw ).xyz;
	vec3 G5 = texture(rubyTexture, t5.xw ).xyz;
	vec3 H5 = texture(rubyTexture, t5.yw ).xyz;
	vec3 I5 = texture(rubyTexture, t5.zw ).xyz;
	vec3 A0 = texture(rubyTexture, t6.xy ).xyz;
	vec3 D0 = texture(rubyTexture, t6.xz ).xyz;
	vec3 G0 = texture(rubyTexture, t6.xw ).xyz;
	vec3 C4 = texture(rubyTexture, t7.xy ).xyz;
	vec3 F4 = texture(rubyTexture, t7.xz ).xyz;
	vec3 I4 = texture(rubyTexture, t7.xw ).xyz;

	vec4 b = mul( mat4x3(B, D, H, F), Y );
	vec4 c = mul( mat4x3(C, A, G, I), Y );
	vec4 e = mul( mat4x3(E, E, E, E), Y );
	vec4 d  = b.yzwx;
	vec4 f  = b.wxyz;
	vec4 g  = c.zwxy;
	vec4 h  = b.zwxy;
	vec4 i  = c.wxyz;

	vec4 i4 = mul( mat4x3(I4, C1, A0, G5), Y );
	vec4 i5 = mul( mat4x3(I5, C4, A1, G0), Y );
	vec4 h5 = mul( mat4x3(H5, F4, B1, D0), Y );
	vec4 f4 = h5.yzwx;

	vec4 Ao = vec4( 1.0, -1.0, -1.0, 1.0 );
	vec4 Bo = vec4( 1.0,  1.0, -1.0,-1.0 );
	vec4 Co = vec4( 1.5,  0.5, -0.5, 0.5 );
	vec4 Ax = vec4( 1.0, -1.0, -1.0, 1.0 );
	vec4 Bx = vec4( 0.5,  2.0, -0.5,-2.0 );
	vec4 Cx = vec4( 1.0,  1.0, -0.5, 0.0 );
	vec4 Ay = vec4( 1.0, -1.0, -1.0, 1.0 );
	vec4 By = vec4( 2.0,  0.5, -2.0,-0.5 );
	vec4 Cy = vec4( 2.0,  0.0, -1.0, 0.5 );

	// These inequations define the line below which interpolation occurs.
	fx   = vec4(greaterThan(Ao * fp.y + Bo * fp.x, Co));
	fx_l = vec4(greaterThan(Ax * fp.y + Bx * fp.x, Cx));
	fx_u = vec4(greaterThan(Ay * fp.y + By * fp.x, Cy));

	if (XBR_CORNER_TYPE == 1.0) {
		irlv1 = diff(e,f) * diff(e,h);

	} else if (XBR_CORNER_TYPE == 2.0) {
		irlv1 = (neq(f,b) * neq(h,d) + eq(e,i) * neq(f,i4) * neq(h,i5) + eq(e,g) + eq(e,c));

	} else if (XBR_CORNER_TYPE == 3.0) {
		vec4 c1 = i4.yzwx;
		vec4 g0 = i5.wxyz;
		irlv1 = (neq(f,b) * neq(h,d) + eq(e,i) * neq(f,i4) * neq(h,i5) + eq(e,g) + eq(e,c) ) * (diff(f,f4) * diff(f,i) + diff(h,h5) * diff(h,i) + diff(h,g) + diff(f,c) + eq(b,c1) * eq(d,g0));

	} else { // XBR_CORNER_TYPE == 4.0
		irlv1 = (neq(f,b) * neq(f,c) + neq(h,d) * neq(h,g) + eq(e,i) * (neq(f,f4) * neq(f,i4) + neq(h,h5) * neq(h,i5)) + eq(e,g) + eq(e,c));
	}

	irlv2l = diff(e,g) * diff(d,g);
	irlv2u = diff(e,c) * diff(b,c);

	vec4 wd1 = wd( e, c,  g, i, h5, f4, h, f);
	vec4 wd2 = wd( h, d, i5, f, i4,  b, e, i);

	edri  = step(wd1, wd2) * irlv1;
	edr   = step(wd1 + vec4(0.1, 0.1, 0.1, 0.1), wd2) * step(vec4(0.5, 0.5, 0.5, 0.5), irlv1);
	edr_l = step( XBR_LV2_COEFFICIENT *df(f,g), df(h,c) ) * irlv2l * edr;
	edr_u = step( XBR_LV2_COEFFICIENT *df(h,c), df(f,g) ) * irlv2u * edr;

	nc = bvec4( edr * ( fx + edr_l * (fx_l)) + edr_u * fx_u);

	px = lessThanEqual(df(e, f), df(e, h));

	vec3 res1 = nc.x ? px.x ? F : H : nc.y ? px.y ? B : F : nc.z ? px.z ? D : B : E;
	vec3 res2 = nc.w ? px.w ? H : D : nc.z ? px.z ? D : B : nc.y ? px.y ? B : F : E;

	vec2 df12 = abs( mul( mat2x3(res1, res2), Y ) - e.xy);

	vec3 res = mix(res1, res2, step(df12.x, df12.y));

	FragColor.xyz = res;
}

#endif
