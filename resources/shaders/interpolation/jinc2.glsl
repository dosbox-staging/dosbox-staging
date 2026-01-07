//#version 130

// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2011-2026 Hyllian <sergiogdb@gmail.com>
// SPDX-License-Identifier: MIT

// Hyllian's jinc windowed-jinc 2-lobe sharper with anti-ringing Shader
//
// Ported from Libretro's GLSL shader jinc2.glsl to DOSBox-compatible
// format by Tyrells.

/*
//#pragma parameter JINC2_WINDOW_SINC "Window Sinc Param" 0.44 0.0 1.0 0.01
//#pragma parameter JINC2_SINC "Sinc Param" 0.82 0.0 1.0 0.01
//#pragma parameter JINC2_AR_STRENGTH "Anti-ringing Strength" 0.8 0.0 1.0 0.1
//#ifdef PARAMETER_UNIFORM
//uniform float JINC2_WINDOW_SINC;
//uniform float JINC2_SINC;
//uniform float JINC2_AR_STRENGTH;
//#else
*/
#define JINC2_WINDOW_SINC 0.44
#define JINC2_SINC 0.82
#define JINC2_AR_STRENGTH 0.8
//#endif
// END PARAMETERS //

/* COMPATIBILITY
   - HLSL compilers
   - Cg   compilers
*/

const   float halfpi            = 1.5707963267948966192313216916398;
const   float pi                = 3.1415926535897932384626433832795;
const   float wa                = JINC2_WINDOW_SINC*pi;
const   float wb                = JINC2_SINC*pi;

// Calculates the distance between two points
float d(vec2 pt1, vec2 pt2)
{
  vec2 v = pt2 - pt1;
  return sqrt(dot(v,v));
}

vec3 min4(vec3 a, vec3 b, vec3 c, vec3 d)
{
    return min(a, min(b, min(c, d)));
}

vec3 max4(vec3 a, vec3 b, vec3 c, vec3 d)
{
    return max(a, max(b, max(c, d)));
}

vec4 resampler(vec4 x)
{
   vec4 res;

   res = (x==vec4(0.0, 0.0, 0.0, 0.0)) ?  vec4(wa*wb)  :  sin(x*wa)*sin(x*wb)/(x*x);

   return res;
}


#define texCoord TEX0

#if defined(VERTEX)

#if __VERSION__ >= 130
#define OUT out
#define IN  in
#define tex2D texture
#else
#define OUT varying
#define IN attribute
#define tex2D texture2D
#endif

#ifdef GL_ES
#define PRECISION mediump
#else
#define PRECISION
#endif


IN  vec4 VertexCoord;
IN  vec4 Color;
IN  vec2 TexCoord;
OUT vec4 color;
OUT vec2 texCoord;

uniform mat4 MVPMatrix;
uniform int  FrameDirection;
uniform int  FrameCount;
uniform PRECISION vec2 OutputSize;
uniform PRECISION vec2 TextureSize;
uniform PRECISION vec2 InputSize;

void main()
{
    gl_Position = MVPMatrix * VertexCoord;
    color = Color;
    texCoord = TexCoord;
}


#elif defined(FRAGMENT)

#if __VERSION__ >= 130
#define IN in
#define tex2D texture
out vec4 FragColor;
#else
#define IN varying
#define FragColor gl_FragColor
#define tex2D texture2D
#endif

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#define PRECISION mediump
#else
#define PRECISION
#endif

uniform int FrameDirection;
uniform int FrameCount;
uniform PRECISION vec2 OutputSize;
uniform PRECISION vec2 TextureSize;
uniform PRECISION vec2 InputSize;
uniform sampler2D s_p;
IN vec2 texCoord;


void main()
{

    vec3 color;
    vec4 weights[4];

    vec2 dx = vec2(1.0, 0.0);
    vec2 dy = vec2(0.0, 1.0);

    vec2 pc = texCoord*TextureSize;

    vec2 tc = (floor(pc-vec2(0.5,0.5))+vec2(0.5,0.5));
     
    weights[0] = resampler(vec4(d(pc, tc    -dx    -dy), d(pc, tc           -dy), d(pc, tc    +dx    -dy), d(pc, tc+2.0*dx    -dy)));
    weights[1] = resampler(vec4(d(pc, tc    -dx       ), d(pc, tc              ), d(pc, tc    +dx       ), d(pc, tc+2.0*dx       )));
    weights[2] = resampler(vec4(d(pc, tc    -dx    +dy), d(pc, tc           +dy), d(pc, tc    +dx    +dy), d(pc, tc+2.0*dx    +dy)));
    weights[3] = resampler(vec4(d(pc, tc    -dx+2.0*dy), d(pc, tc       +2.0*dy), d(pc, tc    +dx+2.0*dy), d(pc, tc+2.0*dx+2.0*dy)));

    dx = dx/TextureSize;
    dy = dy/TextureSize;
    tc = tc/TextureSize;

    vec3 c00 = tex2D(s_p, tc    -dx    -dy).xyz;
    vec3 c10 = tex2D(s_p, tc           -dy).xyz;
    vec3 c20 = tex2D(s_p, tc    +dx    -dy).xyz;
    vec3 c30 = tex2D(s_p, tc+2.0*dx    -dy).xyz;
    vec3 c01 = tex2D(s_p, tc    -dx       ).xyz;
    vec3 c11 = tex2D(s_p, tc              ).xyz;
    vec3 c21 = tex2D(s_p, tc    +dx       ).xyz;
    vec3 c31 = tex2D(s_p, tc+2.0*dx       ).xyz;
    vec3 c02 = tex2D(s_p, tc    -dx    +dy).xyz;
    vec3 c12 = tex2D(s_p, tc           +dy).xyz;
    vec3 c22 = tex2D(s_p, tc    +dx    +dy).xyz;
    vec3 c32 = tex2D(s_p, tc+2.0*dx    +dy).xyz;
    vec3 c03 = tex2D(s_p, tc    -dx+2.0*dy).xyz;
    vec3 c13 = tex2D(s_p, tc       +2.0*dy).xyz;
    vec3 c23 = tex2D(s_p, tc    +dx+2.0*dy).xyz;
    vec3 c33 = tex2D(s_p, tc+2.0*dx+2.0*dy).xyz;

    color = tex2D(s_p, texCoord).xyz;

    //  Get min/max samples
    vec3 min_sample = min4(c11, c21, c12, c22);
    vec3 max_sample = max4(c11, c21, c12, c22);
/*
      color = mat4x3(c00, c10, c20, c30) * weights[0];
      color+= mat4x3(c01, c11, c21, c31) * weights[1];
      color+= mat4x3(c02, c12, c22, c32) * weights[2];
      color+= mat4x3(c03, c13, c23, c33) * weights[3];
      mat4 wgts = mat4(weights[0], weights[1], weights[2], weights[3]);
      vec4 wsum = wgts * vec4(1.0,1.0,1.0,1.0);
      color = color/(dot(wsum, vec4(1.0,1.0,1.0,1.0)));
*/


    color = vec3(dot(weights[0], vec4(c00.x, c10.x, c20.x, c30.x)), dot(weights[0], vec4(c00.y, c10.y, c20.y, c30.y)), dot(weights[0], vec4(c00.z, c10.z, c20.z, c30.z)));
    color+= vec3(dot(weights[1], vec4(c01.x, c11.x, c21.x, c31.x)), dot(weights[1], vec4(c01.y, c11.y, c21.y, c31.y)), dot(weights[1], vec4(c01.z, c11.z, c21.z, c31.z)));
    color+= vec3(dot(weights[2], vec4(c02.x, c12.x, c22.x, c32.x)), dot(weights[2], vec4(c02.y, c12.y, c22.y, c32.y)), dot(weights[2], vec4(c02.z, c12.z, c22.z, c32.z)));
    color+= vec3(dot(weights[3], vec4(c03.x, c13.x, c23.x, c33.x)), dot(weights[3], vec4(c03.y, c13.y, c23.y, c33.y)), dot(weights[3], vec4(c03.z, c13.z, c23.z, c33.z)));
    color = color/(dot(weights[0], vec4(1,1,1,1)) + dot(weights[1], vec4(1,1,1,1)) + dot(weights[2], vec4(1,1,1,1)) + dot(weights[3], vec4(1,1,1,1)));

    // Anti-ringing
    vec3 aux = color;
    color = clamp(color, min_sample, max_sample);
    color = mix(aux, color, JINC2_AR_STRENGTH);

    // final sum and weight normalization
    FragColor.xyz = color;
}
#endif