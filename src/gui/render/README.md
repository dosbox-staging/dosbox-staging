# Rendering code


## Shader system

DOSBox Staging uses a semi-modular multi-pass OpenGL shader system. This
strikes a balance between a fully modular system (like the one in RetroArch)
and a fully fixed one that allows no custom shaders.

The current pipeline looks like this:

- Pass 1 - IntegerDownscale
- Pass 2 - ImageAdjustments
- Pass 3 - CheckerboardDedither_Linearize (optional)
- Pass 4 - CheckerboardDedither_Pass1     (optional)
- Pass 5 - CheckerboardDedither_Pass2     (optional)
- Pass 6 - CheckerboardDedither_Pass3     (optional)
- Pass 7 - IntegerUpscale
- Pass 8 - <MAIN_SHADER>

Our approach is to have a fixed pipeline with stages that can be optionally
toggled (e.g., when the checkerboard dedithering is disabled via a config
setting, those passes are removed from the pipeline). The `<MAIN_SHADER>` pass
can be set to any of our bundled shaders with the `shader` setting, or is
auto-switched by our award-winning, patent-pending Zero-Config Adaptive CRT
Shader System(C)(R)(TM) at runtime (e.g., when selecting `shader =
crt-auto`).

This approach allows for a much tigher integration (e.g., we can implement
emulated monitor controls in the OSD that simply just update uniforms in the
ImageAdjustments shader) and just enough flexibility for the 99% use case. It
also keeps the implementation much simpler compared to a fully generic
solution. While a fully flexible solution is great for experienced shader
_authors_, it does not help regular users at all who simply just want the
output to look good or authentic with minimum fuss.


### Custom shader pragmas

#### name

Name of the shader pass. Other shaders can feed the output of this shader as
their input by referencing this name with the `inputN` pragma.

The name must not contain spaces. Names are globally scoped, so it is advised
to use the `ShaderName_PassName` convention for multipass shaders.

The following reserved names cannot be used:

- `Previous`

Example:

```
#pragma name CheckerboardDedither_Linearize
```


#### inputN

Specifies the input textures of this shader. The first input is `input0`, the
second `input1`, and so on. The inputs must reference the names of earlier
shaders in the pipeline; the output of the referenced shader is used as the
input texture. The reserved name `Previous` can be also used to refer to the
output of the previous shader.

The input texture sizes in pixels are passed to the shader as `vec2` uniforms
named `INPUT_TEXTURE_SIZE_0`, `INPUT_TEXTURE_SIZE_1`, etc. The textures are
passed as `sampler2D` objects named `INPUT_TEXTURE_0`, `INPUT_TEXTURE_1`, etc.

Every shader must have at least one input, so if no inputs are specified, we
default to `input0 Previous`. Consequently, the `INPUT_TEXTURE_SIZE_0` and
`INPUT_TEXTURE_0` uniforms always exist.

Example:

```
#pragma input0 Previous
#pragma input1 CheckerboardDedither_Linearize
```


#### output_size

The size of the output image in pixels. This controls the output texture size
of intermediate shader passes. For the last shader, this must be set to
`Viewport`.

Valid values:

- `Rendered` — Size of the image as rendered by the video card emulation.
    Starting from 640x350, this is usually the same as the video mode's
    dimensions, but below that the rendered size might be different based on
    the double scanning and pixel doubling settings (e.g., the 13h VGA mode
    can be rendered as 320x200, 320x400 or 640x400).

- `VideoMode` — The resolution of the emulated DOS video mode. This is always the
    "nominal resolution" of the mode, as "seen" by DOS programs (e.g., the
    pixel data of the 320x200 13h VGA mode is always seen as 320 * 200 = 64000
    contiguous bytes in the video card's memory, regardless of any downstream
    pixel and line doubling performed by the VGA hardware or our equivalent
    emulated functionality).

- `Viewport` — Size of the viewport on the host OS (depends on the window size
    or the screen dimensions in fullscreen, and the various viewport
    restriction settings like integer scaling, etc.)

The actual output size in pixels is passed to shader as `vec2` a uniform
named `OUTPUT_TEXTURE_SIZE`.

Example:

```
#pragma output_size Viewport
```


#### parameter

Specify shader parameters in the RetroArch format of `PARAM_NAME DEFAULT_VAL
MIN_VAL MAX_VAL STEP`. Only the name and default values are used currently,
the rest are only informational.

Shader presets can set these values to override the default (currently, only
for the main shader; preset are not supported for internal shaders). The
parameters are sent to the shaders via `float` uniforms, so you must ensure you
have a float uniform declared for each parameter.

Example:

```
#pragma parameter PHOSPHOR_LAYOUT "PHOSPHOR LAYOUT" 2.0 0.0 19.0 1.0
```


#### force_single_scan

Forces single-scanned rendering of video modes that are double-scanned on VGA
(e.g., all 200-line modes).

Example:

```
#pragma force_single_scan
```


#### force_no_pixel_doubling

Forces non-pixel-doubled rendering of video modes that are rendered
pixel-doubled on the EGA and VGA machine types to increase horizontal
sharpness (e.g., all 200-line modes).

Example:

```
#pragma force_no_pixel_doubling
```


### use_nearest_texture_filter

If specified, nearest neighbour filtering is set for the output texture of the
shader (this only affects the shader(s) that uses this texture as input).
Bilinear filtering is used if the pragma is not specified.

Example:

```
#pragma use_nearest_texture_filter
```

