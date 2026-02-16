# Rendering code


## Shader system

DOSBox Staging uses a semi-modular multi-pass OpenGL shader system. This
strikes a balance between a fixed system that allows no custom shaders, and a
fully modular one, like the RetroArch shader system.

Our approach is to have a fixed pipeline with stages that can be optionally
toggled (e.g., when checkerboard dedithering is disabled, those passes are
removed from the pipeline). The "main shader" can be set to any of our bundled
shaders (e.g., `sharp`, `interpolation/nearest`, etc.) or one of the adaptive
CRT "meta-shaders" (e.g., `crt-auto`).

From the user experience and usability perspective, this approach has enough
flexibility for the 99% use case and allows for a much tigher integration
(e.g., we can implement emulated monitor controls in the OSD as the image
adjustment shader is guaranteed to be always present). Limited control and
freedom over the full pipeline is a benefit for most users: we've done the
hard part of researching and picking the best shaders to achieve authentic
results, and have exposed some simple, intuitive controls to the tweak the
output.

From the code perspective, our implementation is much simpler compared to a
fully flexible system and thus is easier to maintain.


### Current pipeline

This is an overview of the entire shader pipeline:

```
  IntegerDownscale               (optional; if dedithering is on)

  ImageAdjustments               (always present)

  CheckerboardDedither_Linearize (optional; can be toggled by the user)
  CheckerboardDedither_Pass1     (optional)
  CheckerboardDedither_Pass2     (optional)
  CheckerboardDedither_Pass3     (optional)

  IntegerUpscale                 (optional; if dedithering is on)

  <MAIN_SHADER>                  (always present; set by the user)
```

### Shader presets

Shader presets are regular INI files containing two sections.

The `[settings]` section is optional; it can contain the following shader
settings (see the [Custom shader pragmas]() section for more details):

- `force_single_scan` (boolean)
- `force_no_pixel_doubling` (boolean)
- `use_nearest_texture_filter` (boolean)

The `[parameters]` section in required; it is used to assign values to the
shader's uniform parameters. It's not mandatory to assign values to all
uniforms, but at least a single key-value pair must be present. Unspecified
uniforms will use their default values (defined in the `.glsl` file).

Example preset file:


```ini
[settings]
force_single_scan = yes
force_no_pixel_doubling = yes

[parameters]
BEAM_PROFILE        = 0.00
HFILTER_PROFILE     = 1.00
...
```

### Shader descriptors

Shaders and shader & preset pairs are identified by their _shader descriptor_
strings. The `shader` config setting expects such shader descriptors.

Shader descriptors are in the `SHADER_NAME[:SHADER_PRESET]` format.

- `SHADER_NAME` refers to either the filename of an actual shader on the
    filesystem, a symbolic alias, or a "meta-shader"
- `SHADER_PRESET` (after a colon) is optional; the default preset is used if
    it's not provided.

Examples:

- `sharp`
- `crt-auto`
- `crt/crt-hyllian`
- `crt/crt-hyllian:vga-4k`
- `interpolation/catmull-rom.glsl`
- `D:\Emulators\DOSBox\shaders\custom-shader.glsl`
- `../my-shaders/custom-shader`

See the comment of `ShaderDescriptor` for more details:

- src/gui/render/private/shader_common.h


### Adaptive CRT shaders

Staging features a uniqe adaptive CRT shader switching system that
automatically selects the appropriate CRT shader and/or preset based on the
current video mode, the viewport resolution, the configured machine type, and
various other runtime criteria.

See the comments in the following files for more details:

- src/gui/render/private/shader_common.h
- src/gui/render/auto_shader_switcher.cpp
- src/gui/render/private/auto_shader_switcher.h


### Custom shader pragmas

The `.glsl` shaders files contain metadata via custom shader pragmas.

> [!WARNING]
> As per the GLSL specification, shader compilers should ignore unknown
> pragmas. However, some older non-compliant Intel drivers try to process them
> and raise errors for some of our custom pragmas. The solution is to enclose
> all custom pragmas in a multiline comment block (`/* ... */`). Staging will
> parse them even when commented out.

#### name

_(mandatory)_

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

_(optional)_

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

_(optional)_

The size of the output image in pixels. This controls the output texture size
of intermediate shader passes. For the last shader, this must be set to
`Viewport`. If not specified, `Rendered` is used.

Valid values:

- `Rendered` — Size of the image as rendered by the video card emulation.
    Starting from 640x350, this is usually the same as the video mode's
    dimensions, but below that the rendered size might be different based on
    the double scanning and pixel doubling settings (e.g., the 13h VGA mode
    can be rendered as 320x200, 320x400 or 640x400).

- `VideoMode` — The resolution of the emulated DOS video mode. This is always
  the "nominal resolution" of the mode, as "seen" by DOS programs (e.g., the
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

_(optional)_

Specify shader parameters in the RetroArch format of `PARAM_NAME DEFAULT_VAL
MIN_VAL MAX_VAL STEP`. Only the name and default values are used currently,
the rest are only informational.

Shader presets can set these values to override the defaults (currently, only
for the main shader; preset are not supported for internal shaders). The
parameters are sent to the shaders via `float` uniforms, so you must ensure
you have a float uniform declared for each parameter.

Example:

```
#pragma parameter PHOSPHOR_LAYOUT "PHOSPHOR LAYOUT" 2.0 0.0 19.0 1.0
```


#### force_no_pixel_doubling

_(optional)_

Forces non-pixel-doubled rendering of video modes that are rendered
pixel-doubled on the EGA and VGA machine types to increase horizontal
sharpness (e.g., all 200-line modes).

To disable the setting, simply delete the pragma.

Shader presets can override the value of this setting.

Example:

```
#pragma force_no_pixel_doubling
```


#### force_single_scan

_(optional)_

Forces single-scanned rendering of video modes that are double-scanned on VGA
(e.g., all 200-line modes).

To disable the setting, simply delete the pragma.

Shader presets can override the value of this setting.

Example:

```
#pragma force_single_scan
```


### use_nearest_texture_filter

_(optional)_

If specified, nearest neighbour filtering is set for the output texture of the
shader (this only affects the shader(s) that uses this texture as input).
Bilinear filtering is used if the pragma is not specified.

To disable the setting, simply delete the pragma.

Shader presets can override the value of this setting.

Example:

```
#pragma use_nearest_texture_filter
```

