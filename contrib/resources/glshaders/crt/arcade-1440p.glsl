#version 120

/*
   Hyllian's CRT Shader

   Copyright (C) 2020-2024  The DOSBox Staging Team

   Copyright (C) 2011-2020 Hyllian - sergiogdb@gmail.com

   Copyright (C) 2020, this file ported from Libretro's GLSL
   shader crt-hyllian.glslp to DOSBox-compatible format by Tyrells.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.

*/

#pragma use_npot_texture
#pragma force_single_scan
#pragma force_no_pixel_doubling

/*

#pragma parameter BEAM_PROFILE "BEAM PROFILE (BP)" 0.0 0.0 2.0 1.0
#pragma parameter HFILTER_PROFILE "HORIZONTAL FILTER PROFILE [ HERMITE | CATMULL-ROM ]" 0.0 0.0 1.0 1.0
#pragma parameter BEAM_MIN_WIDTH "Custom [If BP=0.00] MIN BEAM WIDTH" 1.0 0.0 1.0 0.01
#pragma parameter BEAM_MAX_WIDTH "Custom [If BP=0.00] MAX BEAM WIDTH" 1.0 0.0 1.0 0.01
#pragma parameter SCANLINES_STRENGTH "Custom [If BP=0.00] SCANLINES STRENGTH" 0.58 0.0 1.0 0.01
#pragma parameter COLOR_BOOST "Custom [If BP=0.00] COLOR BOOST" 1.30 1.0 2.0 0.05
#pragma parameter SHARPNESS_HACK "SHARPNESS_HACK" 1.0 1.0 4.0 1.0
#pragma parameter PHOSPHOR_LAYOUT "PHOSPHOR LAYOUT" 4.0 0.0 19.0 1.0
#pragma parameter MASK_INTENSITY "MASK INTENSITY" 0.7 0.0 1.0 0.1
#pragma parameter CRT_ANTI_RINGING "ANTI RINGING" 1.0 0.0 1.0 0.2
#pragma parameter INPUT_GAMMA "INPUT GAMMA" 2.4 0.0 5.0 0.1
#pragma parameter OUTPUT_GAMMA "OUTPUT GAMMA" 2.2 0.0 5.0 0.1
#pragma parameter VSCANLINES "VERTICAL SCANLINES [ OFF | ON ]" 0.0 0.0 1.0 1.0
#pragma parameter BLACK_LEVEL "BLACK LEVEL" 0.0 0.0 0.005 0.0001

#pragma parameter WP_ADJUST "White Point Adjustments [ OFF | ON ]" 0.0 0.0 1.0 1.0
#pragma parameter TEMPERATURE "White Point" 9311.0 1031.0 12047.0 72.0
#pragma parameter LUMA_PRESERVE "Preserve Luminance [ OFF | ON ]" 1.0 0.0 1.0 1.0
#pragma parameter WP_RED "Red Shift" 0.0 -1.0 1.0 0.01
#pragma parameter WP_GREEN "Green Shift" 0.0 -1.0 1.0 0.01
#pragma parameter WP_BLUE "Blue Shift" 0.0 -1.0 1.0 0.01

*/

#define GAMMA_IN(color)   pow(color, vec4(INPUT_GAMMA, INPUT_GAMMA, INPUT_GAMMA, INPUT_GAMMA))
#define GAMMA_OUT(color)  pow(color, vec4(1.0 / OUTPUT_GAMMA, 1.0 / OUTPUT_GAMMA, 1.0 / OUTPUT_GAMMA, 1.0 / OUTPUT_GAMMA))


#define texCoord v_texCoord

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


IN  vec4 a_position;
IN  vec4 Color;
IN  vec2 TexCoord;
OUT vec4 color;
OUT vec2 texCoord;

uniform PRECISION vec2 rubyOutputSize;
uniform PRECISION vec2 rubyTextureSize;
uniform PRECISION vec2 rubyInputSize;

void main()
{
    gl_Position = a_position;
    v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize / rubyTextureSize;
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

uniform PRECISION vec2 rubyOutputSize;
uniform PRECISION vec2 rubyTextureSize;
uniform PRECISION vec2 rubyInputSize;
uniform sampler2D s_p;
IN vec2 texCoord;

#ifdef PARAMETER_UNIFORM
uniform PRECISION float BEAM_PROFILE;
uniform PRECISION float HFILTER_PROFILE;
uniform PRECISION float BEAM_MIN_WIDTH;
uniform PRECISION float BEAM_MAX_WIDTH;
uniform PRECISION float SCANLINES_STRENGTH;
uniform PRECISION float COLOR_BOOST;
uniform PRECISION float SHARPNESS_HACK;
uniform PRECISION float PHOSPHOR_LAYOUT;
uniform PRECISION float MASK_INTENSITY;
uniform PRECISION float CRT_ANTI_RINGING;
uniform PRECISION float INPUT_GAMMA;
uniform PRECISION float OUTPUT_GAMMA;
uniform PRECISION float VSCANLINES;

uniform PRECISION float WP_ADJUST;
uniform PRECISION float TEMPERATURE;
uniform PRECISION float LUMA_PRESERVE;
uniform PRECISION float RED_SHIFT;
uniform PRECISION float GREEN_SHIFT;
uniform PRECISION float BLUE_SHIFT;

#else
#define BEAM_PROFILE        0.00
#define HFILTER_PROFILE     0.00
#define BEAM_MIN_WIDTH      1.00
#define BEAM_MAX_WIDTH      1.20
#define SCANLINES_STRENGTH  0.75
#define COLOR_BOOST         2.00
#define SHARPNESS_HACK      1.00
#define PHOSPHOR_LAYOUT    14.00
#define MASK_INTENSITY      0.30
#define CRT_ANTI_RINGING    1.00
#define INPUT_GAMMA         2.40
#define OUTPUT_GAMMA        2.25
#define VSCANLINES          0.00
#define BLACK_LEVEL         0.000

#define WP_ADJUST           0.00
#define TEMPERATURE      9300.00
#define LUMA_PRESERVE       1.00
#define RED_SHIFT           0.00
#define GREEN_SHIFT         0.00
#define BLUE_SHIFT          0.00
#endif
// END PARAMETERS //


// White Point Mapping
//          ported by Dogway
//
// From the first comment post (sRGB primaries and linear light compensated)
//      http://www.zombieprototypes.com/?p=210#comment-4695029660
// Based on the Neil Bartlett's blog update
//      http://www.zombieprototypes.com/?p=210
// Inspired itself by Tanner Helland's work
//      http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/

vec3 wp_adjust(vec3 color){
    float temp = TEMPERATURE / 100.;
    float k = TEMPERATURE / 10000.;
    float lk = log(k);

    vec3 wp = vec3(1.);

    // calculate RED
    wp.r = (temp <= 65.) ? 1. : 0.32068362618584273 + (0.19668730877673762 * pow(k - 0.21298613432655075, - 1.5139012907556737)) + (- 0.013883432789258415 * lk);

    // calculate GREEN
    float mg = 1.226916242502167 + (- 1.3109482654223614 * pow(k - 0.44267061967913873, 3.) * exp(- 5.089297600846147 * (k - 0.44267061967913873))) + (0.6453936305542096 * lk);
    float pg = 0.4860175851734596 + (0.1802139719519286 * pow(k - 0.14573069517701578, - 1.397716496795082)) + (- 0.00803698899233844 * lk);
    wp.g = (temp <= 65.5) ? ((temp <= 8.) ? 0. : mg) : pg;

    // calculate BLUE
    wp.b = (temp <= 19.) ? 0. : (temp >= 66.) ? 1. : 1.677499032830161 + (- 0.02313594016938082 * pow(k - 1.1367244820333684, 3.) * exp(- 4.221279555918655 * (k - 1.1367244820333684))) + (1.6550275798913296 * lk);

    // clamp
    wp.rgb = clamp(wp.rgb, vec3(0.), vec3(1.));

    // R/G/B independent manual White Point adjustment
    wp.rgb += vec3(RED_SHIFT, GREEN_SHIFT, BLUE_SHIFT);

    // Linear color input
    return color * wp;
}

vec3 sRGB_to_XYZ(vec3 RGB){
    const mat3x3 m = mat3x3(
    0.4124564,  0.3575761,  0.1804375,
    0.2126729,  0.7151522,  0.0721750,
    0.0193339,  0.1191920,  0.9503041);
    return RGB * m;
}


vec3 XYZtoYxy(vec3 XYZ){
    float XYZrgb = XYZ.r+XYZ.g+XYZ.b;
    float Yxyr = XYZ.g;
    float Yxyg = (XYZrgb <= 0.0) ? 0.3805 : XYZ.r / XYZrgb;
    float Yxyb = (XYZrgb <= 0.0) ? 0.3769 : XYZ.g / XYZrgb;
    return vec3(Yxyr,Yxyg,Yxyb);
}

vec3 XYZ_to_sRGB(vec3 XYZ){
    const mat3x3 m = mat3x3(
    3.2404542, -1.5371385, -0.4985314,
   -0.9692660,  1.8760108,  0.0415560,
    0.0556434, -0.2040259,  1.0572252);
    return XYZ * m;
}


vec3 YxytoXYZ(vec3 Yxy) {
    float Xs = Yxy.r * (Yxy.g/Yxy.b);
    float Xsz = (Yxy.r <= 0.0) ? 0 : 1;
    vec3 XYZ = vec3(Xsz,Xsz,Xsz) * vec3(Xs, Yxy.r, (Xs/Yxy.g)-Xs-Yxy.r);
    return XYZ;
}

/*
    A collection of CRT mask effects that work with LCD subpixel structures for
    small details

    author: hunterk
    license: public domain

    How to use it:

    Multiply your image by the vec3 output:
    FragColor.rgb *= mask_weights(gl_FragCoord.xy, 1.0, 1);

    The function needs to be tiled across the screen using the physical pixels, e.g.
    gl_FragCoord (the "vec2 coord" input). In the case of slang shaders, we use
    (vTexCoord.st * OutputSize.xy).

    The "mask_intensity" (float value between 0.0 and 1.0) is how strong the mask
    effect should be. Full-strength red, green and blue subpixels on a white pixel
    are the ideal, and are achieved with an intensity of 1.0, though this darkens
    the image significantly and may not always be desirable.

    The "phosphor_layout" (int value between 0 and 19) determines which phophor
    layout to apply. 0 is no mask/passthru.

    Many of these mask arrays are adapted from cgwg's crt-geom-deluxe LUTs, and
    those have their filenames included for easy identification

    NOTE: Having too many branches result in a black screen (but no
    compilation errors) on some older GPUs such as the Intel HD 4000 iGPU. So
    we're commenting out the branches that are not used by the adaptive CRT
    shaders.
*/
vec3 mask_weights(vec2 coord, float mask_intensity, int phosphor_layout) {
    vec3 weights = vec3(1.,1.,1.);

    float on = 1.;
    float off = 1.-mask_intensity;

    vec3 red     = vec3(on,  off, off);
    vec3 green   = vec3(off, on,  off);
    vec3 blue    = vec3(off, off, on );
    vec3 magenta = vec3(on,  off, on );
    vec3 yellow  = vec3(on,  on,  off);
    vec3 cyan    = vec3(off, on,  on );
    vec3 black   = vec3(off, off, off);
    vec3 white   = vec3(on,  on,  on );

    int w, z = 0;

    // This pattern is used by a few layouts, so we'll define it here
    vec3 aperture_weights = mix(magenta, green, floor(mod(coord.x, 2.0)));

    if (phosphor_layout == 0) {
        return weights;
    }

    else if (phosphor_layout == 1) {
        // classic aperture for RGB panels; good for 1080p, too small for 4K+
        // aka aperture_1_2_bgr
        weights  = aperture_weights;
        return weights;
    }

    else if (phosphor_layout == 2) {
        // 2x2 shadow mask for RGB panels; good for 1080p, too small for 4K+
        // aka delta_1_2x1_bgr
        vec3 inverse_aperture = mix(green, magenta, floor(mod(coord.x, 2.0)));
        weights               = mix(aperture_weights, inverse_aperture, floor(mod(coord.y, 2.0)));
        return weights;
    }

/*
    else if (phosphor_layout == 3) {
        // slot mask for RGB panels; looks okay at 1080p, looks better at 4K
        // {magenta, green, black,   black},
        // {magenta, green, magenta, green},
        // {black,   black, magenta, green}

        // GLSL can't do 2D arrays until version 430, so do this stupid thing instead for compatibility's sake:
        // First lay out the horizontal pixels in arrays
        vec3 slotmask_x1[4] = vec3[](magenta, green, black,   black);
        vec3 slotmask_x2[4] = vec3[](magenta, green, magenta, green);
        vec3 slotmask_x3[4] = vec3[](black,   black, magenta, green);

        // find the vertical index
        w = int(floor(mod(coord.y, 3.0)));

        // find the horizontal index
        z = int(floor(mod(coord.x, 4.0)));

        // do a big, dumb comparison in place of a 2D array
        weights = (w == 1) ? slotmask_x1[z] : (w == 2) ? slotmask_x2[z] :  slotmask_x3[z];
        return weights;
    }
*/
    else if (phosphor_layout == 4) {
        // classic aperture for RBG panels; good for 1080p, too small for 4K+
        weights  = mix(yellow, blue, floor(mod(coord.x, 2.0)));
        return weights;
    }
/*
    else if (phosphor_layout == 5) {
        // 2x2 shadow mask for RBG panels; good for 1080p, too small for 4K+
        vec3 inverse_aperture = mix(blue, yellow, floor(mod(coord.x, 2.0)));

        weights = mix(
            mix(yellow, blue, floor(mod(coord.x, 2.0))),
            inverse_aperture,
            floor(mod(coord.y, 2.0))
        );
        return weights;
    }
*/
/*
    else if (phosphor_layout == 6) {
        // aperture_1_4_rgb; good for simulating lower
        vec3 ap4[4] = vec3[](red, green, blue, black);

        z = int(floor(mod(coord.x, 4.0)));

        weights = ap4[z];
        return weights;
    }
*/
/*
    else if (phosphor_layout == 7) {
        // aperture_2_5_bgr
        vec3 ap3[5] = vec3[](red, magenta, blue, green, green);

        z = int(floor(mod(coord.x, 5.0)));

        weights = ap3[z];
        return weights;
*/
/*
    else if (phosphor_layout == 8) {
        // aperture_3_6_rgb
        vec3 big_ap[7] = vec3[](red, red, yellow, green, cyan, blue, blue);

        w = int(floor(mod(coord.x, 7.)));

        weights = big_ap[w];
        return weights;
    }
*/
    else if (phosphor_layout == 9) {
        // reduced TVL aperture for RGB panels
        // aperture_2_4_rgb
        vec3 big_ap_rgb[4] = vec3[](red, yellow, cyan, blue);

        w = int(floor(mod(coord.x, 4.)));

        weights = big_ap_rgb[w];
        return weights;
    }
/*
    else if (phosphor_layout == 10) {
        // reduced TVL aperture for RBG panels
        vec3 big_ap_rbg[4] = vec3[](red, magenta, cyan, green);

        w = int(floor(mod(coord.x, 4.)));

        weights = big_ap_rbg[w];
        return weights;
    }
*/
    else if(phosphor_layout == 11) {
        // delta_1_4x1_rgb; dunno why this is called 4x1 when it's obviously 4x2 /shrug
        vec3 delta_1_1[4] = vec3[](red, green, blue, black);
        vec3 delta_1_2[4] = vec3[](blue, black, red, green);

        w = int(floor(mod(coord.y, 2.0)));
        z = int(floor(mod(coord.x, 4.0)));

        weights = (w == 1) ? delta_1_1[z] : delta_1_2[z];
        return weights;
    }
/*
    else if (phosphor_layout == 12) {
        // delta_2_4x1_rgb
        vec3 delta_2_1[4] = vec3[](red, yellow, cyan, blue);
        vec3 delta_2_2[4] = vec3[](cyan, blue, red, yellow);

        z = int(floor(mod(coord.x, 4.0)));

        weights = (w == 1) ? delta_2_1[z] : delta_2_2[z];
        return weights;
    }
*/
/*
    else if (phosphor_layout == 13) {
        // delta_2_4x2_rgb
        vec3 delta_1[4] = vec3[](red, yellow, cyan, blue);
        vec3 delta_2[4] = vec3[](red, yellow, cyan, blue);
        vec3 delta_3[4] = vec3[](cyan, blue, red, yellow);
        vec3 delta_4[4] = vec3[](cyan, blue, red, yellow);

        w = int(floor(mod(coord.y, 4.0)));
        z = int(floor(mod(coord.x, 4.0)));

        weights = (w == 1) ? delta_1[z] : (w == 2) ? delta_2[z] : (w == 3) ? delta_3[z] : delta_4[z];
        return weights;
    }
*/
    else if (phosphor_layout == 14) {
        // slot mask for RGB panels; too low-pitch for 1080p, looks okay at 4K, but wants 8K+
        // {magenta, green, black, black,   black, black},
        // {magenta, green, black, magenta, green, black},
        // {black,   black, black, magenta, green, black}
        vec3 slot2_1[6] = vec3[](magenta, green, black, black,   black, black);
        vec3 slot2_2[6] = vec3[](magenta, green, black, magenta, green, black);
        vec3 slot2_3[6] = vec3[](black,   black, black, magenta, green, black);

        w = int(floor(mod(coord.y, 3.0)));
        z = int(floor(mod(coord.x, 6.0)));

        weights = (w == 1) ? slot2_1[z] : (w == 2) ? slot2_2[z] : slot2_3[z];
        return weights;
    }
/*
    else if(phosphor_layout == 15) {
        // slot_2_4x4_rgb
        // {red,   yellow, cyan,  blue,  red,   yellow, cyan,  blue },
        // {red,   yellow, cyan,  blue,  black, black,  black, black},
        // {red,   yellow, cyan,  blue,  red,   yellow, cyan,  blue },
        // {black, black,  black, black, red,   yellow, cyan,  blue }
        vec3 slotmask_RBG_x1[8] = vec3[](red,   yellow, cyan,  blue,  red,   yellow, cyan,  blue );
        vec3 slotmask_RBG_x2[8] = vec3[](red,   yellow, cyan,  blue,  black, black,  black, black);
        vec3 slotmask_RBG_x3[8] = vec3[](red,   yellow, cyan,  blue,  red,   yellow, cyan,  blue );
        vec3 slotmask_RBG_x4[8] = vec3[](black, black,  black, black, red,   yellow, cyan,  blue );

        // find the vertical index
        w = int(floor(mod(coord.y, 4.0)));

        // find the horizontal index
        z = int(floor(mod(coord.x, 8.0)));

        weights = (w == 1) ? slotmask_RBG_x1[z] : (w == 2) ? slotmask_RBG_x2[z] : (w == 3) ? slotmask_RBG_x3[z] : slotmask_RBG_x4[z];
        return weights;
    }
*/
/*
    else if(phosphor_layout == 16) {
        // slot mask for RBG panels; too low-pitch for 1080p, looks okay at 4K, but wants 8K+
        // {yellow, blue,  black,  black},
        // {yellow, blue,  yellow, blue},
        // {black,  black, yellow, blue}
        vec3 slot2_1[4] = vec3[](yellow, blue,  black,  black);
        vec3 slot2_2[4] = vec3[](yellow, blue,  yellow, blue);
        vec3 slot2_3[4] = vec3[](black,  black, yellow, blue);

        w = int(floor(mod(coord.y, 3.0)));
        z = int(floor(mod(coord.x, 4.0)));

        weights = (w == 1) ? slot2_1[z] : (w == 2) ? slot2_2[z] : slot2_3[z];
        return weights;
    }
*/
    else if (phosphor_layout == 17) {
        // slot_2_5x4_bgr
        // {red,   magenta, blue,  green, green, red,   magenta, blue,  green, green},
        // {black, blue,    blue,  green, green, red,   red,     black, black, black},
        // {red,   magenta, blue,  green, green, red,   magenta, blue,  green, green},
        // {red,   red,     black, black, black, black, blue,    blue,  green, green}
        vec3 slot_1[10] = vec3[](red,   magenta, blue,  green, green, red,   magenta, blue,  green, green);
        vec3 slot_2[10] = vec3[](black, blue,    blue,  green, green, red,   red,     black, black, black);
        vec3 slot_3[10] = vec3[](red,   magenta, blue,  green, green, red,   magenta, blue,  green, green);
        vec3 slot_4[10] = vec3[](red,   red,     black, black, black, black, blue,    blue,  green, green);

        w = int(floor(mod(coord.y, 4.0)));
        z = int(floor(mod(coord.x, 10.0)));

        weights = (w == 1) ? slot_1[z] : (w == 2) ? slot_2[z] : (w == 3) ? slot_3[z] : slot_4[z];
        return weights;
    }
/*
    else if (phosphor_layout == 18) {
        // same as above but for RBG panels
        // {red,   yellow, green, blue,  blue,  red,   yellow, green, blue,  blue },
        // {black, green,  green, blue,  blue,  red,   red,    black, black, black},
        // {red,   yellow, green, blue,  blue,  red,   yellow, green, blue,  blue },
        // {red,   red,    black, black, black, black, green,  green, blue,  blue }
        vec3 slot_1[10] = vec3[](red,   yellow, green, blue,  blue,  red,   yellow, green, blue,  blue );
        vec3 slot_2[10] = vec3[](black, green,  green, blue,  blue,  red,   red,    black, black, black);
        vec3 slot_3[10] = vec3[](red,   yellow, green, blue,  blue,  red,   yellow, green, blue,  blue );
        vec3 slot_4[10] = vec3[](red,   red,    black, black, black, black, green,  green, blue,  blue );

        w = int(floor(mod(coord.y, 4.0)));
        z = int(floor(mod(coord.x, 10.0)));

        weights = (w == 1) ? slot_1[z] : (w == 2) ? slot_2[z] : (w == 3) ? slot_3[z] : slot_4[z];
        return weights;
    }
*/
/*
    else if(phosphor_layout == 19) {
        // slot_3_7x6_rgb
        // {red,   red,   yellow, green, cyan,  blue,  blue,  red,   red,   yellow, green,  cyan,  blue,  blue},
        // {red,   red,   yellow, green, cyan,  blue,  blue,  red,   red,   yellow, green,  cyan,  blue,  blue},
        // {red,   red,   yellow, green, cyan,  blue,  blue,  black, black, black,  black,  black, black, black},
        // {red,   red,   yellow, green, cyan,  blue,  blue,  red,   red,   yellow, green,  cyan,  blue,  blue},
        // {red,   red,   yellow, green, cyan,  blue,  blue,  red,   red,   yellow, green,  cyan,  blue,  blue},
        // {black, black, black,  black, black, black, black, black, red,   red,    yellow, green, cyan,  blue}

        vec3 slot_1[14] = vec3[](red,   red,   yellow, green, cyan,  blue,  blue,  red,   red,   yellow, green,  cyan,  blue,  blue);
        vec3 slot_2[14] = vec3[](red,   red,   yellow, green, cyan,  blue,  blue,  red,   red,   yellow, green,  cyan,  blue,  blue);
        vec3 slot_3[14] = vec3[](red,   red,   yellow, green, cyan,  blue,  blue,  black, black, black,  black,  black, black, black);
        vec3 slot_4[14] = vec3[](red,   red,   yellow, green, cyan,  blue,  blue,  red,   red,   yellow, green,  cyan,  blue,  blue);
        vec3 slot_5[14] = vec3[](red,   red,   yellow, green, cyan,  blue,  blue,  red,   red,   yellow, green,  cyan,  blue,  blue);
        vec3 slot_6[14] = vec3[](black, black, black,  black, black, black, black, black, red,   red,    yellow, green, cyan,  blue);

        w = int(floor(mod(coord.y, 6.0)));
        z = int(floor(mod(coord.x, 14.0)));

        weights = (w == 1) ? slot_1[z] : (w == 2) ? slot_2[z] : (w == 3) ? slot_3[z] : (w == 4) ? slot_4[z] : (w == 5) ? slot_5[z] : slot_6[z];
        return weights;
     }
*/
    else {
        return weights;
    }
}

// Horizontal cubic filter.
//
// Some known filters use these values:
//
//   B = 0.0, C = 0.0  =>  Hermite cubic filter.
//   B = 1.0, C = 0.0  =>  Cubic B-Spline filter.
//   B = 0.0, C = 0.5  =>  Catmull-Rom Spline filter. This is the default used in this shader.
//   B = C = 1.0/3.0   =>  Mitchell-Netravali cubic filter.
//   B = 0.3782, C = 0.3109  =>  Robidoux filter.
//   B = 0.2620, C = 0.3690  =>  Robidoux Sharp filter.

// Using only Hermite and Catmull-Rom, as the others aren't useful for crt shader.
// For more info, see: http://www.imagemagick.org/Usage/img_diagrams/cubic_survey.gif
mat4x4 get_hfilter_profile()
{
    float bf = 0.0;
    float cf = 0.0;

    if (HFILTER_PROFILE == 1) {
        bf = 0.0;
        cf = 0.5;
    }

    return mat4(
                   (-bf - 6.0*cf)/6.0,          (3.0*bf + 12.0*cf)/6.0, (-3.0*bf - 6.0*cf)/6.0,             bf/6.0,
         (12.0 - 9.0*bf - 6.0*cf)/6.0, (-18.0 + 12.0*bf  + 6.0*cf)/6.0,                    0.0, (6.0 - 2.0*bf)/6.0,
        -(12.0 - 9.0*bf - 6.0*cf)/6.0,  (18.0 - 15.0*bf - 12.0*cf)/6.0,  (3.0*bf + 6.0*cf)/6.0,             bf/6.0,
                    (bf + 6.0*cf)/6.0,                        -cf,                         0.0,                0.0
    );
}

#define scanlines_strength (4.0 * profile.x)
#define beam_min_width     profile.y
#define beam_max_width     profile.z
#define color_boost        profile.w

vec4 get_beam_profile()
{
    vec4 bp = vec4(SCANLINES_STRENGTH, BEAM_MIN_WIDTH, BEAM_MAX_WIDTH, COLOR_BOOST);

    if (BEAM_PROFILE == 1.0)  bp = vec4(0.62, 1.00, 1.00, 1.40); // Catmull-rom
    if (BEAM_PROFILE == 2.0)  bp = vec4(0.72, 1.00, 1.00, 1.20); // Catmull-rom

    return bp;
}

void main()
{
    vec4 profile = get_beam_profile();

    vec2 TextureSize = mix(
        vec2(rubyTextureSize.x * SHARPNESS_HACK, rubyTextureSize.y),
        vec2(rubyTextureSize.x, rubyTextureSize.y * SHARPNESS_HACK), VSCANLINES
    );

    vec2 dx = mix(vec2(1.0/TextureSize.x, 0.0), vec2(0.0, 1.0/TextureSize.y), VSCANLINES);
    vec2 dy = mix(vec2(0.0, 1.0/TextureSize.y), vec2(1.0/TextureSize.x, 0.0), VSCANLINES);

    vec2 pix_coord = texCoord.xy * TextureSize + vec2(-0.5, 0.5);

    vec2 tc = mix(
        (floor(pix_coord) + vec2(0.5,  0.5)) / TextureSize,
        (floor(pix_coord) + vec2(1.0, -0.5)) / TextureSize,
        VSCANLINES
    );

    vec2 fp = mix(fract(pix_coord), fract(pix_coord.yx), VSCANLINES);

    vec4 c00 = GAMMA_IN(tex2D(s_p, tc     - dx - dy).xyzw) + BLACK_LEVEL;
    vec4 c01 = GAMMA_IN(tex2D(s_p, tc          - dy).xyzw) + BLACK_LEVEL;
    vec4 c02 = GAMMA_IN(tex2D(s_p, tc     + dx - dy).xyzw) + BLACK_LEVEL;
    vec4 c03 = GAMMA_IN(tex2D(s_p, tc + 2.0*dx - dy).xyzw) + BLACK_LEVEL;

    vec4 c10 = GAMMA_IN(tex2D(s_p, tc     - dx).xyzw) + BLACK_LEVEL;
    vec4 c11 = GAMMA_IN(tex2D(s_p, tc         ).xyzw) + BLACK_LEVEL;
    vec4 c12 = GAMMA_IN(tex2D(s_p, tc     + dx).xyzw) + BLACK_LEVEL;
    vec4 c13 = GAMMA_IN(tex2D(s_p, tc + 2.0*dx).xyzw) + BLACK_LEVEL;

    mat4 invX = get_hfilter_profile();

    mat4 color_matrix0 = mat4(c00, c01, c02, c03);
    mat4 color_matrix1 = mat4(c10, c11, c12, c13);

    vec4 invX_Px = vec4(fp.x*fp.x*fp.x, fp.x*fp.x, fp.x, 1.0) * invX;
    vec4 color0  = color_matrix0 * invX_Px;
    vec4 color1  = color_matrix1 * invX_Px;

    //  Get min/max samples
    vec4 min_sample0 = min(c01, c02);
    vec4 max_sample0 = max(c01, c02);
    vec4 min_sample1 = min(c11, c12);
    vec4 max_sample1 = max(c11, c12);

    // Anti-ringing
    vec4 aux = color0;
    color0 = clamp(color0, min_sample0, max_sample0);
    color0 = mix(aux, color0, CRT_ANTI_RINGING * step(0.0, (c00-c01)*(c02-c03)));
    aux = color1;
    color1 = clamp(color1, min_sample1, max_sample1);
    color1 = mix(aux, color1, CRT_ANTI_RINGING * step(0.0, (c10-c11)*(c12-c13)));

    float pos0 = fp.y;
    float pos1 = 1.0 - fp.y;

    vec4 lum0 = mix(vec4(beam_min_width), vec4(beam_max_width), color0);
    vec4 lum1 = mix(vec4(beam_min_width), vec4(beam_max_width), color1);

    vec4 d0 = scanlines_strength * pos0 / (lum0 + 0.0000001);
    vec4 d1 = scanlines_strength * pos1 / (lum1 + 0.0000001);

    d0 = exp(-d0*d0);
    d1 = exp(-d1*d1);

    vec4 color = color_boost * (color0*d0 + color1*d1);

    // Mask
    vec2 mask_coords = gl_FragCoord.xy; //texCoord.xy * OutputSize.xy;
    mask_coords = mix(mask_coords.xy, mask_coords.yx, VSCANLINES);
    color.rgb *= mask_weights(mask_coords, MASK_INTENSITY, int(PHOSPHOR_LAYOUT));

    // Colour temperature
    if (WP_ADJUST == 1.0) {
        vec3 wp_adjusted = wp_adjust(color.rgb);
        vec3 base_luma = XYZtoYxy(sRGB_to_XYZ(color.rgb));
        vec3 adjusted_luma = XYZtoYxy(sRGB_to_XYZ(wp_adjusted));
        wp_adjusted = (LUMA_PRESERVE == 1.0) ? adjusted_luma + (vec3(base_luma.r,0.,0.) - vec3(adjusted_luma.r,0.,0.)) : adjusted_luma;
        color = vec4(XYZ_to_sRGB(YxytoXYZ(wp_adjusted)), 1.0);
    }

    // Output gamma
    color = clamp(GAMMA_OUT(color), 0.0, 1.0);

    FragColor = vec4(color.rgb, 1.0);
}
#endif
