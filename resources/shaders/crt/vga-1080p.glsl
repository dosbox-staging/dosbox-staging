#version 120

// SPDX-FileCopyrightText:  2020-2024 The DOSBox Staging Team
// SPDX-License-Identifier: MIT

#define SPOT_WIDTH             0.85
#define SPOT_HEIGHT            0.80
#define PHOSPHOR_LAYOUT        2.00
#define SCANLINE_STRENGTH_MIN  0.72
#define SCANLINE_STRENGTH_MAX  0.83
#define COLOR_BOOST_EVEN       4.45
#define COLOR_BOOST_ODD        1.65
#define MASK_STRENGTH          0.10
#define GAMMA_INPUT            2.15
#define GAMMA_OUTPUT           2.45

/////////////////////////////////////////////////////////////////////////////

uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

varying vec2 v_texCoord;
varying vec2 onex;
varying vec2 oney;

#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize)


#if defined(VERTEX)

attribute vec4 a_position;

void main() {
  gl_Position = a_position;

  v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize;
  v_texCoord /= rubyTextureSize;

  onex = vec2(SourceSize.z, 0.0);
  oney = vec2(0.0, SourceSize.w);
}


#elif defined(FRAGMENT)

uniform sampler2D rubyTexture;

#define GAMMA_IN(color)     pow(color, vec4(GAMMA_INPUT))
#define TEX2D(coords)       GAMMA_IN(texture2D(rubyTexture, coords))

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

void main()
{
  vec2 coords = v_texCoord.xy * SourceSize.xy;
  vec2 pixel_center = floor(coords) + vec2(0.5, 0.5);
  vec2 texture_coords = pixel_center * SourceSize.zw;

  vec4 color = TEX2D(texture_coords);

  float dx = coords.x - pixel_center.x;

  float h_weight_00 = dx / SPOT_WIDTH;
  WEIGHT(h_weight_00);

  color *= vec4(h_weight_00, h_weight_00, h_weight_00, h_weight_00);

  // get closest horizontal neighbour to blend
  vec2 coords01;
  if (dx > 0.0) {
    coords01 = onex;
    dx = 1.0 - dx;
  } else {
    coords01 = -onex;
    dx = 1.0 + dx;
  }
  vec4 colorNB = TEX2D(texture_coords + coords01);

  float h_weight_01 = dx / SPOT_WIDTH;
  WEIGHT(h_weight_01);

  color = color + colorNB * vec4(h_weight_01);

  //////////////////////////////////////////////////////
  // Vertical Blending
  float dy = coords.y - pixel_center.y;
  float v_weight_00 = dy / SPOT_HEIGHT;
  WEIGHT(v_weight_00);
  color *= vec4(v_weight_00);

  // get closest vertical neighbour to blend
  vec2 coords10;
  if (dy > 0.0) {
    coords10 = oney;
    dy = 1.0 - dy;
  } else {
    coords10 = -oney;
    dy = 1.0 + dy;
  }
  colorNB = TEX2D(texture_coords + coords10);

  float v_weight_10 = dy / SPOT_HEIGHT;
  WEIGHT(v_weight_10);

  color = color + colorNB * vec4(v_weight_10 * h_weight_00, v_weight_10 * h_weight_00, v_weight_10 * h_weight_00, v_weight_10 * h_weight_00);

  colorNB = TEX2D(texture_coords + coords01 + coords10);

  color = color + colorNB * vec4(v_weight_10 * h_weight_01, v_weight_10 * h_weight_01, v_weight_10 * h_weight_01, v_weight_10 * h_weight_01);

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
