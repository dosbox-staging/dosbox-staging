#version 120

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
varying vec2 onex;
varying vec2 oney;
varying vec2 prescale;

#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize)


#if defined(VERTEX)

attribute vec4 a_position;

void main() {
	gl_Position = a_position;

    v_texCoord_lores = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize;

    prescale = ceil(rubyOutputSize / rubyInputSize);

	onex = vec2(SourceSize.z, 0.0);
	oney = vec2(0.0, SourceSize.w);
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

   if(phosphor_layout == 0) return weights;

   else if(phosphor_layout == 1){
      // classic aperture for RGB panels; good for 1080p, too small for 4K+
      // aka aperture_1_2_bgr
      weights  = aperture_weights;
      return weights;
   }

   else if(phosphor_layout == 2){
      // 2x2 shadow mask for RGB panels; good for 1080p, too small for 4K+
      // aka delta_1_2x1_bgr
      vec3 inverse_aperture = mix(green, magenta, floor(mod(coord.x, 2.0)));
      weights               = mix(aperture_weights, inverse_aperture, floor(mod(coord.y, 2.0)));
      return weights;
   }

   else if(phosphor_layout == 3){
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
    }

   if(phosphor_layout == 4){
      // classic aperture for RBG panels; good for 1080p, too small for 4K+
      weights  = mix(yellow, blue, floor(mod(coord.x, 2.0)));
      return weights;
   }

   else if(phosphor_layout == 5){
      // 2x2 shadow mask for RBG panels; good for 1080p, too small for 4K+
      vec3 inverse_aperture = mix(blue, yellow, floor(mod(coord.x, 2.0)));
      weights               = mix(mix(yellow, blue, floor(mod(coord.x, 2.0))), inverse_aperture, floor(mod(coord.y, 2.0)));
      return weights;
   }

   else if(phosphor_layout == 6){
      // aperture_1_4_rgb; good for simulating lower
      vec3 ap4[4] = vec3[](red, green, blue, black);

      z = int(floor(mod(coord.x, 4.0)));

      weights = ap4[z];
      return weights;
   }

   else if(phosphor_layout == 7){
      // aperture_2_5_bgr
      vec3 ap3[5] = vec3[](red, magenta, blue, green, green);

      z = int(floor(mod(coord.x, 5.0)));

      weights = ap3[z];
      return weights;
   }

   else if(phosphor_layout == 8){
      // aperture_3_6_rgb

      vec3 big_ap[7] = vec3[](red, red, yellow, green, cyan, blue, blue);

      w = int(floor(mod(coord.x, 7.)));

      weights = big_ap[w];
      return weights;
   }

   else if(phosphor_layout == 9){
      // reduced TVL aperture for RGB panels
      // aperture_2_4_rgb

      vec3 big_ap_rgb[4] = vec3[](red, yellow, cyan, blue);

      w = int(floor(mod(coord.x, 4.)));

      weights = big_ap_rgb[w];
      return weights;
   }

   else if(phosphor_layout == 10){
      // reduced TVL aperture for RBG panels

      vec3 big_ap_rbg[4] = vec3[](red, magenta, cyan, green);

      w = int(floor(mod(coord.x, 4.)));

      weights = big_ap_rbg[w];
      return weights;
   }

   else if(phosphor_layout == 11){
      // delta_1_4x1_rgb; dunno why this is called 4x1 when it's obviously 4x2 /shrug
      vec3 delta_1_1[4] = vec3[](red, green, blue, black);
      vec3 delta_1_2[4] = vec3[](blue, black, red, green);

      w = int(floor(mod(coord.y, 2.0)));
      z = int(floor(mod(coord.x, 4.0)));

      weights = (w == 1) ? delta_1_1[z] : delta_1_2[z];
      return weights;
   }

   else if(phosphor_layout == 12){
      // delta_2_4x1_rgb
      vec3 delta_2_1[4] = vec3[](red, yellow, cyan, blue);
      vec3 delta_2_2[4] = vec3[](cyan, blue, red, yellow);

      z = int(floor(mod(coord.x, 4.0)));

      weights = (w == 1) ? delta_2_1[z] : delta_2_2[z];
      return weights;
   }

   else if(phosphor_layout == 13){
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

   else if(phosphor_layout == 14){
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

   else if(phosphor_layout == 15){
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

   else if(phosphor_layout == 16){
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

   else if(phosphor_layout == 17){
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

   else if(phosphor_layout == 18){
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

   else if(phosphor_layout == 19){
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

   else return weights;
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
  color.r = pow(color.r + 1e-5, saturation);
  color.g = pow(color.g + 1e-5, saturation);
  color.b = pow(color.b + 1e-5, saturation);
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
