#version 330 core

// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2011-2015 Hyllian <sergiogdb@gmail.com>
// SPDX-License-Identifier: MIT

// Hyllian's xBR-lv3 Shader
//
// Note from Hyllian:
//   Incorporates some of the ideas from SABR shader. Thanks to Joshua Street.

/*

#pragma use_nearest_texture_filter
#pragma force_single_scan
#pragma force_no_pixel_doubling

#pragma parameter XBR_Y_WEIGHT "Y Weight" 48.0 0.0 100.0 1.0
#pragma parameter XBR_EQ_THRESHOLD "EQ Threshold" 10.0 0.0 50.0 1.0
#pragma parameter XBR_EQ_THRESHOLD2 "EQ Threshold 2" 2.0 0.0 4.0 1.0
#pragma parameter XBR_LV2_COEFFICIENT "Lv2 Coefficient" 2.0 1.0 3.0 1.0
#pragma parameter XBR_CORNER_TYPE "Corner Calculation" 3.0 1.0 3.0 1.0

*/

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

uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;
uniform vec2 rubyInputSize;

void main()
{
	gl_Position = vec4(a_position, 0.0, 1.0);

	v_texCoord = vec2(a_position.x + 1.0, a_position.y + 1.0) / 2.0 *
	             rubyInputSize / rubyTextureSize;

	vec2 ps = vec2(1.0) / rubyTextureSize.xy;

	float dx = ps.x;
	float dy = ps.y;

	//    A1 B1 C1
	// A0  A  B  C C4
	// D0  D  E  F F4
	// G0  G  H  I I4
	//    G5 H5 I5

	t1 = v_texCoord.xxxy + vec4( -dx, 0, dx,-2.0*dy); // A1 B1 C1
	t2 = v_texCoord.xxxy + vec4( -dx, 0, dx,    -dy); //  A  B  C
	t3 = v_texCoord.xxxy + vec4( -dx, 0, dx,      0); //  D  E  F
	t4 = v_texCoord.xxxy + vec4( -dx, 0, dx,     dy); //  G  H  I
	t5 = v_texCoord.xxxy + vec4( -dx, 0, dx, 2.0*dy); // G5 H5 I5
	t6 = v_texCoord.xyyy + vec4(-2.0*dx,-dy, 0,  dy); // A0 D0 G0
	t7 = v_texCoord.xyyy + vec4( 2.0*dx,-dy, 0,  dy); // C4 F4 I4
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

uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;
uniform vec2 rubyInputSize;
uniform sampler2D rubyTexture;

// compatibility #defines

#define Source rubyTexture
#define vTexCoord v_texCoord.xy

#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize) //either TextureSize or InputSize
#define OutputSize vec4(rubyOutputSize, 1.0 / rubyOutputSize)

uniform float XBR_Y_WEIGHT;
uniform float XBR_EQ_THRESHOLD;
uniform float XBR_EQ_THRESHOLD2;
uniform float XBR_LV2_COEFFICIENT;
uniform float XBR_CORNER_TYPE;

const mat3 yuv = mat3(0.299, 0.587, 0.114, -0.169, -0.331, 0.499, 0.499, -0.418, -0.0813);
const vec4 delta = vec4(0.4, 0.4, 0.4, 0.4);

vec4 df(vec4 A, vec4 B)
{
	return vec4(abs(A-B));
}

float c_df(vec3 c1, vec3 c2) {
	vec3 df = abs(c1 - c2);
	return df.r + df.g + df.b;
}

bvec4 eq(vec4 A, vec4 B)
{
	return lessThan(df(A, B), vec4(XBR_EQ_THRESHOLD));
}

bvec4 eq2(vec4 A, vec4 B)
{
	return lessThan(df(A, B), vec4(XBR_EQ_THRESHOLD2));
}

bvec4 and(bvec4 A, bvec4 B)
{
	return bvec4(A.x && B.x, A.y && B.y, A.z && B.z, A.w && B.w);
}

bvec4 or(bvec4 A, bvec4 B)
{
	return bvec4(A.x || B.x, A.y || B.y, A.z || B.z, A.w || B.w);
}

vec4 weighted_distance(vec4 a, vec4 b, vec4 c, vec4 d, vec4 e, vec4 f, vec4 g, vec4 h)
{
	return (df(a,b) + df(a,c) + df(d,e) + df(d,f) + 4.0*df(g,h));
}

void main()
{
	bvec4 edr, edr_left, edr_up, edr3_left, edr3_up, px; // px = pixel, edr = edge detection rule
	bvec4 interp_restriction_lv1, interp_restriction_lv2_left, interp_restriction_lv2_up;
	bvec4 interp_restriction_lv3_left, interp_restriction_lv3_up;
	bvec4 nc, nc30, nc60, nc45, nc15, nc75; // new_color
	vec4 fx, fx_left, fx_up, finalfx, fx3_left, fx3_up; // inequations of straight lines.
	vec3 res1, res2, pix1, pix2;
	float blend1, blend2;

	vec2 fp = fract(vTexCoord * SourceSize.xy);

	vec3 A1 = texture(rubyTexture, t1.xw).rgb;
	vec3 B1 = texture(rubyTexture, t1.yw).rgb;
	vec3 C1 = texture(rubyTexture, t1.zw).rgb;

	vec3 A = texture(rubyTexture, t2.xw).rgb;
	vec3 B = texture(rubyTexture, t2.yw).rgb;
	vec3 C = texture(rubyTexture, t2.zw).rgb;

	vec3 D = texture(rubyTexture, t3.xw).rgb;
	vec3 E = texture(rubyTexture, t3.yw).rgb;
	vec3 F = texture(rubyTexture, t3.zw).rgb;

	vec3 G = texture(rubyTexture, t4.xw).rgb;
	vec3 H = texture(rubyTexture, t4.yw).rgb;
	vec3 I = texture(rubyTexture, t4.zw).rgb;

	vec3 G5 = texture(rubyTexture, t5.xw).rgb;
	vec3 H5 = texture(rubyTexture, t5.yw).rgb;
	vec3 I5 = texture(rubyTexture, t5.zw).rgb;

	vec3 A0 = texture(rubyTexture, t6.xy).rgb;
	vec3 D0 = texture(rubyTexture, t6.xz).rgb;
	vec3 G0 = texture(rubyTexture, t6.xw).rgb;

	vec3 C4 = texture(rubyTexture, t7.xy).rgb;
	vec3 F4 = texture(rubyTexture, t7.xz).rgb;
	vec3 I4 = texture(rubyTexture, t7.xw).rgb;

	vec4 b = transpose(mat4x3(B, D, H, F)) * (XBR_Y_WEIGHT * yuv[0]);
	vec4 c = transpose(mat4x3(C, A, G, I)) * (XBR_Y_WEIGHT * yuv[0]);
	vec4 e = transpose(mat4x3(E, E, E, E)) * (XBR_Y_WEIGHT * yuv[0]);
	vec4 d = b.yzwx;
	vec4 f = b.wxyz;
	vec4 g = c.zwxy;
	vec4 h = b.zwxy;
	vec4 i = c.wxyz;

	vec4 i4 = transpose(mat4x3(I4, C1, A0, G5)) * (XBR_Y_WEIGHT*yuv[0]);
	vec4 i5 = transpose(mat4x3(I5, C4, A1, G0)) * (XBR_Y_WEIGHT*yuv[0]);
	vec4 h5 = transpose(mat4x3(H5, F4, B1, D0)) * (XBR_Y_WEIGHT*yuv[0]);
	vec4 f4 = h5.yzwx;

	vec4 c1 = i4.yzwx;
	vec4 g0 = i5.wxyz;
	vec4 b1 = h5.zwxy;
	vec4 d0 = h5.wxyz;

	vec4 Ao = vec4( 1.0, -1.0, -1.0, 1.0 );
	vec4 Bo = vec4( 1.0,  1.0, -1.0,-1.0 );
	vec4 Co = vec4( 1.5,  0.5, -0.5, 0.5 );
	vec4 Ax = vec4( 1.0, -1.0, -1.0, 1.0 );
	vec4 Bx = vec4( 0.5,  2.0, -0.5,-2.0 );
	vec4 Cx = vec4( 1.0,  1.0, -0.5, 0.0 );
	vec4 Ay = vec4( 1.0, -1.0, -1.0, 1.0 );
	vec4 By = vec4( 2.0,  0.5, -2.0,-0.5 );
	vec4 Cy = vec4( 2.0,  0.0, -1.0, 0.5 );

	vec4 Az = vec4( 6.0, -2.0, -6.0, 2.0 );
	vec4 Bz = vec4( 2.0, 6.0, -2.0, -6.0 );
	vec4 Cz = vec4( 5.0, 3.0, -3.0, -1.0 );
	vec4 Aw = vec4( 2.0, -6.0, -2.0, 6.0 );
	vec4 Bw = vec4( 6.0, 2.0, -6.0,-2.0 );
	vec4 Cw = vec4( 5.0, -1.0, -3.0, 3.0 );

	fx      = (Ao*fp.y+Bo*fp.x);
	fx_left = (Ax*fp.y+Bx*fp.x);
	fx_up   = (Ay*fp.y+By*fp.x);
	fx3_left= (Az*fp.y+Bz*fp.x);
	fx3_up  = (Aw*fp.y+Bw*fp.x);

	// It uses CORNER_C if none of the others are defined.
    if (XBR_CORNER_TYPE == 1.0) {
		interp_restriction_lv1 = and(notEqual(e, f), notEqual(e, h));

	} else if (XBR_CORNER_TYPE == 2.0) {
		interp_restriction_lv1 = and(and(notEqual(e,f) , notEqual(e,h))  ,
									( or(or(and(and(or(and(not(eq(f,b)) ,  not(eq(h,d))) ,
									eq(e,i)) , not(eq(f,i4))) , not(eq(h,i5))) , eq(e,g)) , eq(e,c)) ) );
	//commenting this one out because something got broken
	/*else if(XBR_CORNER_TYPE == 6.0)
		{interp_restriction_lv1 = and(and(and(notEqual(e,f) , notEqual(e,h))  ,
									( or(or(and(and(or(and(not(eq(f,b)) , not(eq(h,d))) ,
									eq(e,i)) , not(eq(f,i4))) , not(eq(h,i5))) , eq(e,g)) ,
									eq(e,c)) )) , (and(or(or(or(or(and(notEqual(f,f4) , notEqual(f,i)) ,
									and(notEqual(h,h5) , notEqual(h,i))) , notEqual(h,g)) , notEqual(f,c)) ,
									eq(b,c1)) , eq(d,g0))));} */
	} else {
		interp_restriction_lv1 = and(and(notEqual(e, f), notEqual(e, h)),
								 or(or(and(not(eq(f,b)), not(eq(f,c))),
									   and(not(eq(h,d)), not(eq(h,g)))),
									or(and(eq(e,i), or(and(not(eq(f,f4)), not(eq(f,i4))),
													   and(not(eq(h,h5)), not(eq(h,i5))))),
									   or(eq(e,g), eq(e,c)))));
	}

	interp_restriction_lv2_left = and(notEqual(e, g), notEqual(d, g));
	interp_restriction_lv2_up   = and(notEqual(e, c), notEqual(b, c));
	interp_restriction_lv3_left = and(eq2(g,g0), not(eq2(d0,g0)));
	interp_restriction_lv3_up   = and(eq2(c,c1), not(eq2(b1,c1)));

	vec4 fx45 = smoothstep(Co - delta, Co + delta, fx);
	vec4 fx30 = smoothstep(Cx - delta, Cx + delta, fx_left);
	vec4 fx60 = smoothstep(Cy - delta, Cy + delta, fx_up);
	vec4 fx15 = smoothstep(Cz - delta, Cz + delta, fx3_left);
	vec4 fx75 = smoothstep(Cw - delta, Cw + delta, fx3_up);

	edr = and(lessThan(weighted_distance( e, c, g, i, h5, f4, h, f), weighted_distance( h, d, i5, f, i4, b, e, i)), interp_restriction_lv1);
	edr_left = and(lessThanEqual((XBR_LV2_COEFFICIENT*df(f,g)), df(h,c)), interp_restriction_lv2_left);
	edr_up   = and(greaterThanEqual(df(f,g), (XBR_LV2_COEFFICIENT*df(h,c))), interp_restriction_lv2_up);
	edr3_left = interp_restriction_lv3_left;
	edr3_up = interp_restriction_lv3_up;

	nc45 = and(edr, bvec4(fx45));
	nc30 = and(edr, and(edr_left, bvec4(fx30)));
	nc60 = and(edr, and(edr_up, bvec4(fx60)));
	nc15 = and(and(edr, edr_left), and(edr3_left, bvec4(fx15)));
	nc75 = and(and(edr, edr_up), and(edr3_up, bvec4(fx75)));

	px = lessThanEqual(df(e, f), df(e, h));

	nc = bvec4(nc75.x || nc15.x || nc30.x || nc60.x || nc45.x, nc75.y || nc15.y || nc30.y || nc60.y || nc45.y, nc75.z || nc15.z || nc30.z || nc60.z || nc45.z, nc75.w || nc15.w || nc30.w || nc60.w || nc45.w);

	vec4 final45 = vec4(nc45) * fx45;
	vec4 final30 = vec4(nc30) * fx30;
	vec4 final60 = vec4(nc60) * fx60;
	vec4 final15 = vec4(nc15) * fx15;
	vec4 final75 = vec4(nc75) * fx75;

	vec4 maximo = max(max(max(final15, final75),max(final30, final60)), final45);

	// Just to get rid of the uninitialised warnings
	blend1 = 0;
	blend2 = 0;
	pix1 = vec3(0);
	pix2 = vec3(0);

		 if (nc.x) {pix1 = px.x ? F : H; blend1 = maximo.x;}
	else if (nc.y) {pix1 = px.y ? B : F; blend1 = maximo.y;}
	else if (nc.z) {pix1 = px.z ? D : B; blend1 = maximo.z;}
	else if (nc.w) {pix1 = px.w ? H : D; blend1 = maximo.w;}

		 if (nc.w) {pix2 = px.w ? H : D; blend2 = maximo.w;}
	else if (nc.z) {pix2 = px.z ? D : B; blend2 = maximo.z;}
	else if (nc.y) {pix2 = px.y ? B : F; blend2 = maximo.y;}
	else if (nc.x) {pix2 = px.x ? F : H; blend2 = maximo.x;}

	res1 = mix(E, pix1, blend1);
	res2 = mix(E, pix2, blend2);
	vec3 res = mix(res1, res2, step(c_df(E, res1), c_df(E, res2)));

	FragColor = vec4(res, 1.0);
}
#endif
