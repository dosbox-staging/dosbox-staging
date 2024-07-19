#version 120

/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
 *
 *  Based on parts of the Caligari shader plus bits and pieces from Hyllian,
 *  Easymode, and probably a few others.
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

#pragma use_npot_texture
#pragma force_single_scan

#define PHOSPHOR_LAYOUT       2.00
#define SCANLINE_STRENGTH_MIN 0.80
#define SCANLINE_STRENGTH_MAX 0.85
#define COLOR_BOOST_EVEN      4.80
#define COLOR_BOOST_ODD       1.40
#define MASK_STRENGTH         0.10
#define GAMMA_INPUT           2.40
#define GAMMA_OUTPUT          2.62

/////////////////////////////////////////////////////////////////////////////

uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

varying vec2 v_texCoord_lores;
varying vec2 prescale;

#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize)


#if defined(VERTEX)

attribute vec4 a_position;

void main() {
    gl_Position = a_position;

    v_texCoord_lores = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize;

    prescale = ceil(rubyOutputSize / rubyInputSize);
}


#elif defined(FRAGMENT)

uniform sampler2D rubyTexture;

#define GAMMA_IN(color)   pow(color, vec4(GAMMA_INPUT))

// Macro for weights computing
#define WEIGHT(w) \
    if (w > 1.0) w = 1.0; \
  w = 1.0 - w * w; \
  w = w * w;


vec3 mask_weights(vec2 coord, float mask_intensity, int phosphor_layout){
   vec3 weights = vec3(1.,1.,1.);
   float on = 1.;
   float off = 1.-mask_intensity;
   vec3 green   = vec3(off, on,  off);
   vec3 magenta = vec3(on,  off, on );

   // This pattern is used by a few layouts, so we'll define it here
   vec3 aperture_weights = mix(magenta, green, floor(mod(coord.x, 2.0)));

   if (phosphor_layout == 0) {
       return weights;

   } else if (phosphor_layout == 1) {
      // classic aperture for RGB panels; good for 1080p, too small for 4K+
      // aka aperture_1_2_bgr
      weights  = aperture_weights;
      return weights;

   } else if (phosphor_layout == 2) {
      // 2x2 shadow mask for RGB panels; good for 1080p, too small for 4K+
      // aka delta_1_2x1_bgr
      vec3 inverse_aperture = mix(green, magenta, floor(mod(coord.x, 2.0)));
      weights               = mix(aperture_weights, inverse_aperture, floor(mod(coord.y, 2.0)));
      return weights;

   } else {
       return weights;
   }
}

vec4 add_vga_overlay(vec4 color, float scanlineStrengthMin, float scanlineStrengthMax, float color_boost_even, float color_boost_odd, float mask_strength) {
  // scanlines
  vec2 mask_coords = gl_FragCoord.xy;

  vec3 lum_factors = vec3(0.2126, 0.7152, 0.0722);
  float luminance = dot(lum_factors, color.rgb);

  float even_odd = floor(mod(mask_coords.y, 2.0));
  float dim_factor = mix(1.0 - scanlineStrengthMax, 1.0 - scanlineStrengthMin, luminance);
  float scanline_dim = clamp(even_odd + dim_factor, 0.0, 1.0);

  color.rgb *= vec3(scanline_dim);

  // color boost
  color.rgb *= mix(vec3(color_boost_even), vec3(color_boost_odd), even_odd);

  float saturation = mix(1.2, 1.03, even_odd);
  float l = length(color);
  color.r = pow(color.r + 1e-7, saturation);
  color.g = pow(color.g + 1e-7, saturation);
  color.b = pow(color.b + 1e-7, saturation);
  color = normalize(color)*l;

  // mask
  color.rgb *= mask_weights(mask_coords, mask_strength, int(PHOSPHOR_LAYOUT));
  return color;
}

vec4 tex2D_linear(in sampler2D sampler, in vec2 uv) {

    // subtract 0.5 here and add it again after the floor to centre the texel
    vec2 texCoord = uv * rubyTextureSize - vec2(0.5);
    vec2 s0t0 = floor(texCoord) + vec2(0.5);
    vec2 s0t1 = s0t0 + vec2(0.0, 1.0);
    vec2 s1t0 = s0t0 + vec2(1.0, 0.0);
    vec2 s1t1 = s0t0 + vec2(1.0);

    vec2 invTexSize = 1.0 / rubyTextureSize;
    vec4 c_s0t0 = GAMMA_IN(texture2D(sampler, s0t0 * invTexSize));
    vec4 c_s0t1 = GAMMA_IN(texture2D(sampler, s0t1 * invTexSize));
    vec4 c_s1t0 = GAMMA_IN(texture2D(sampler, s1t0 * invTexSize));
    vec4 c_s1t1 = GAMMA_IN(texture2D(sampler, s1t1 * invTexSize));

    vec2 weight = fract(texCoord);

    vec4 c0 = c_s0t0 + (c_s1t0 - c_s0t0) * weight.x;
    vec4 c1 = c_s0t1 + (c_s1t1 - c_s0t1) * weight.x;

    return (c0 + (c1 - c0) * weight.y);
}

void main()
{
  const vec2 halfp = vec2(0.5);
  vec2 texel_floored = floor(v_texCoord_lores);
  vec2 s = fract(v_texCoord_lores);
  vec2 region_range = halfp - halfp / prescale;

  vec2 center_dist = s - halfp;
  vec2 f = (center_dist - clamp(center_dist, -region_range, region_range)) * prescale + halfp;

  vec2 mod_texel = min(texel_floored + f, rubyInputSize-halfp);
  vec4 color = tex2D_linear(rubyTexture, mod_texel / rubyTextureSize);

  color = add_vga_overlay(
    color,
    SCANLINE_STRENGTH_MIN, SCANLINE_STRENGTH_MAX,
    COLOR_BOOST_EVEN, COLOR_BOOST_ODD,
    MASK_STRENGTH
  );

  color = pow(color, vec4(1.0 / GAMMA_OUTPUT));
  gl_FragColor = clamp(color, 0.0, 1.0);
}

#endif
