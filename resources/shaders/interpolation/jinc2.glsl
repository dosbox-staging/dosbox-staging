#version 330 core

// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2011-2026 Hyllian <sergiogdb@gmail.com>
// SPDX-License-Identifier: MIT

// Hyllian's jinc windowed-jinc 2-lobe sharper with anti-ringing Shader
//
// Ported from Libretro's GLSL shader jinc2.glsl to DOSBox-compatible
// format by Tyrells.

/*

#pragma force_single_scan
#pragma force_no_pixel_doubling

*/

#define JINC2_WINDOW_SINC 0.44
#define JINC2_SINC 0.82
#define JINC2_AR_STRENGTH 0.8

const float pi = 3.1415926535897932384626433832795;
const float wa = JINC2_WINDOW_SINC * pi;
const float wb = JINC2_SINC * pi;

// Calculates the distance between two points
float d(vec2 pt1, vec2 pt2)
{
    vec2 v = pt2 - pt1;
    return sqrt(dot(v, v));
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
   res = (x == vec4(0.0, 0.0, 0.0, 0.0)) ?
        vec4(wa * wb) :
        sin(x * wa) * sin(x * wb) / (x * x);
   return res;
}

#if defined(VERTEX)

uniform vec2 rubyTextureSize;
uniform vec2 rubyInputSize;

layout (location = 0) in vec2 a_position;

out vec2 v_texCoord;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texCoord  = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 *
                 rubyInputSize / rubyTextureSize;
}

#elif defined(FRAGMENT)

in vec2 v_texCoord;

out vec4 FragColor;

uniform vec2 rubyTextureSize;
uniform sampler2D rubyTexture;

void main()
{
    vec3 color;
    vec4 weights[4];

    vec2 dx = vec2(1.0, 0.0);
    vec2 dy = vec2(0.0, 1.0);
    vec2 pc = v_texCoord * rubyTextureSize;
    vec2 tc = floor(pc - vec2(0.5, 0.5)) + vec2(0.5, 0.5);
     
    weights[0] = resampler(vec4(
        d(pc, tc    -dx    -dy),
        d(pc, tc           -dy),
        d(pc, tc    +dx    -dy),
        d(pc, tc+2.0*dx    -dy)
    ));
    weights[1] = resampler(vec4(
        d(pc, tc    -dx       ),
        d(pc, tc              ),
        d(pc, tc    +dx       ),
        d(pc, tc+2.0*dx       )
   ));
    weights[2] = resampler(vec4(
        d(pc, tc    -dx    +dy),
        d(pc, tc           +dy),
        d(pc, tc    +dx    +dy),
        d(pc, tc+2.0*dx    +dy)
   ));
    weights[3] = resampler(vec4(
        d(pc, tc    -dx+2.0*dy),
        d(pc, tc       +2.0*dy),
        d(pc, tc    +dx+2.0*dy),
        d(pc, tc+2.0*dx+2.0*dy)
    ));

    dx /= rubyTextureSize;
    dy /= rubyTextureSize;
    tc /= rubyTextureSize;

    vec3 c00 = texture(rubyTexture, tc    -dx    -dy).xyz;
    vec3 c10 = texture(rubyTexture, tc           -dy).xyz;
    vec3 c20 = texture(rubyTexture, tc    +dx    -dy).xyz;
    vec3 c30 = texture(rubyTexture, tc+2.0*dx    -dy).xyz;
    vec3 c01 = texture(rubyTexture, tc    -dx       ).xyz;
    vec3 c11 = texture(rubyTexture, tc              ).xyz;
    vec3 c21 = texture(rubyTexture, tc    +dx       ).xyz;
    vec3 c31 = texture(rubyTexture, tc+2.0*dx       ).xyz;
    vec3 c02 = texture(rubyTexture, tc    -dx    +dy).xyz;
    vec3 c12 = texture(rubyTexture, tc           +dy).xyz;
    vec3 c22 = texture(rubyTexture, tc    +dx    +dy).xyz;
    vec3 c32 = texture(rubyTexture, tc+2.0*dx    +dy).xyz;
    vec3 c03 = texture(rubyTexture, tc    -dx+2.0*dy).xyz;
    vec3 c13 = texture(rubyTexture, tc       +2.0*dy).xyz;
    vec3 c23 = texture(rubyTexture, tc    +dx+2.0*dy).xyz;
    vec3 c33 = texture(rubyTexture, tc+2.0*dx+2.0*dy).xyz;

    color = texture(rubyTexture, v_texCoord).xyz;

    //  Get min/max samples
    vec3 min_sample = min4(c11, c21, c12, c22);
    vec3 max_sample = max4(c11, c21, c12, c22);

    color = vec3(
        dot(weights[0], vec4(c00.x, c10.x, c20.x, c30.x)),
        dot(weights[0], vec4(c00.y, c10.y, c20.y, c30.y)),
        dot(weights[0], vec4(c00.z, c10.z, c20.z, c30.z))
    );
    color += vec3(
        dot(weights[1], vec4(c01.x, c11.x, c21.x, c31.x)),
        dot(weights[1], vec4(c01.y, c11.y, c21.y, c31.y)),
        dot(weights[1], vec4(c01.z, c11.z, c21.z, c31.z))
    );
    color += vec3(
        dot(weights[2], vec4(c02.x, c12.x, c22.x, c32.x)),
        dot(weights[2], vec4(c02.y, c12.y, c22.y, c32.y)),
        dot(weights[2], vec4(c02.z, c12.z, c22.z, c32.z))
    );
    color += vec3(
        dot(weights[3], vec4(c03.x, c13.x, c23.x, c33.x)),
        dot(weights[3], vec4(c03.y, c13.y, c23.y, c33.y)),
        dot(weights[3], vec4(c03.z, c13.z, c23.z, c33.z))
    );
    color /= (
        dot(weights[0], vec4(1, 1, 1, 1)) +
        dot(weights[1], vec4(1, 1, 1, 1)) +
        dot(weights[2], vec4(1, 1, 1, 1)) +
        dot(weights[3], vec4(1, 1, 1, 1))
    );

    // Anti-ringing
    vec3 aux = color;
    color = clamp(color, min_sample, max_sample);
    color = mix(aux, color, JINC2_AR_STRENGTH);

    // final sum and weight normalization
    FragColor.xyz = color;
}

#endif
