# Rendering

## Overview

See the comment at the top of
[src/hardware/video/vga_draw.cpp](src/hardware/video/vga_draw.cpp) for a
high-level overview of the VGA rendering.


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
flexibility for the 99% use case, and allows for a much tighter integration
(e.g., we can implement emulated monitor controls in the OSD as the image
adjustments shader is guaranteed to be always present). Limited control and
freedom over the full pipeline is a benefit to most users: we've done the hard
part of researching and picking the best shaders to achieve authentic results,
and have exposed some simple, intuitive controls to tweak the output.

From the code perspective, our implementation is much simpler compared to a
fully flexible system and thus is easier to maintain.


### Current pipeline

This is an overview of the entire shader pipeline:

```
  IntegerDownscale               (optional; if dedithering is on)

  CheckerboardDedither_Linearize (optional; can be toggled by the user)
  CheckerboardDedither_Pass1     (optional)
  CheckerboardDedither_Pass2     (optional)
  CheckerboardDedither_Pass3     (optional)

  IntegerUpscale                 (optional; if dedithering is on)

  ImageAdjustments               (always present)

  <MAIN_SHADER>                  (always present; set by the user)
```


### Shader presets

Shader presets are regular INI files containing two sections.

The `[settings]` section is optional; it can contain the following shader
settings (see the [Custom shader pragmas](#custom-shader-pragmas) section for
more details):

- `float_output` (boolean)
- `force_no_pixel_doubling` (boolean)
- `force_single_scan` (boolean)
- `linear_filtering` (boolean)

The `[parameters]` section is required; it is used to assign values to the
shader's uniform parameters. It's not mandatory to assign values to all
uniforms, but at least a single key-value pair must be present. Unspecified
uniforms will use their default values defined in the `.glsl` file.

Example preset file:


```ini
[settings]
force_single_scan = yes
force_no_pixel_doubling = yes

[parameters]
BEAM_PROFILE    = 0.00
HFILTER_PROFILE = 1.00
...
```

### Shader uniforms

The following uniform are passed to 

- `INPUT_TEXTURE_N` — `sampler2D`; sampler object of the Nth input texture (N
  starts from 0). Every shader has at least one input.

- `INPUT_TEXTURE_SIZE_N` — `vec2`; size of the Nth input texture (N starts from 0) 
- `OUTPUT_TEXTURE_SIZE` — `vec2`; size of the output texture, or the viewport for the last pass (always present)


### Custom shader pragmas

This section lists all available shader pragmas in alphabetical order. They
are used to pass extra metadata about the shaders to Staging (e.g., attaching
a name to the shader, setting the size of the output texture, specifying the
user-changeable shader parameters and their defaults, etc.).

> [!IMPORTANT]
> 
> Please enclose all custom pragmas in multi-line comment blocks (`/* ...
> */`). This will make them "invisible" to the graphics driver, but DOSBox
> Staging always processes these custom pragmas, even when they're commented
> out.
>
> The reason for this is that GLSL compiler implementations should ignore
> unknown pragmas, but certain non-compliant drivers (e.g., old Intel drivers)
> tend to fail the shader compilation with an error if they encounter unknown
> pragmas.


#### float_output

When enabled, the output of the shader is rendered as a 32-bit float RGBA
texture. When disabled, the output is 8-bit RGBA. The setting has no effect
on the last shader pass of the pipeline.

Valid values are `on` and `off`. The setting is disabled by default.

Example:

```
#pragma float_output off
```


#### force_single_scan

When enabled, it forces single-scanned rendering of video modes that are
double-scanned on VGA, e.g., all 200-line modes.

Valid values are `on` and `off`. The setting is disabled by default.

Example:

```
#pragma force_single_scan on
```


#### force_no_pixel_doubling

When enabled, it forces non-pixel-doubled rendering of video modes that are
rendered pixel-doubled on the EGA and VGA machine types to increase horizontal
sharpness, e.g., all 200-line modes (disabled by default).

Valid values are `on` and `off`. The setting is disabled by default.

Example:

```
#pragma force_no_pixel_doubling on
```


#### inputN

Specifies the input textures of this shader. The first input is `input0`, the
second `input1`, and so on. The inputs must reference the names of earlier
shaders in the pipeline; the output of the referenced shader is used as the
input texture. The reserved name `Previous` can also be used to refer to the
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


#### linear_filtering

When enabled, bilinear filtering is used when sampling the input textures.
When disabled, nearest neighbour filtering is used.

Valid values are `on` and `off`. The setting is enabled by default.

Example:

```
#pragma linear_filtering off
```


##### name

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


#### output_size

The size of the output image in pixels. This controls the output texture size
of intermediate shader passes. For the last shader, this must be set to
`Viewport`. If unset, the settings defaults to `Previous`.

Valid values:

- `Previous` — Size of the output of the previous shader pass (default).

- `Rendered` — Size of the image as rendered by the video card emulation.
    Starting from 640x350, this is usually the same as the video mode's
    dimensions, but below that, the rendered size might be different based on
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

Specifies shader parameters in the RetroArch format of `PARAM_NAME DEFAULT_VAL
MIN_VAL MAX_VAL STEP`. Only the name and default values are currently used;
the rest are informational.

Shader presets can set these values to override the defaults (currently, only
for the main shader; presets are not supported for internal shaders). All
parameters are sent to the shaders via `float` uniforms, so you must ensure
you have a float uniform declared for each parameter in the shader source.

Example:

```
#pragma parameter PHOSPHOR_LAYOUT "PHOSPHOR LAYOUT" 2.0 0.0 19.0 1.0
```
