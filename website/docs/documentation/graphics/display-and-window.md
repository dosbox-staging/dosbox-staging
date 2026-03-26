# Display & window

## Overview

DOSBox Staging opens in a resizable window by default. Press
++alt+enter++ to toggle fullscreen mode.

The `opengl` [output mode](#output) is the recommended default because it's required
for CRT shaders and other post-processing effects (see
[Rendering](rendering.md)). If your system doesn't
support OpenGL, `texture` is a solid fallback that still looks good.

For monitors with variable refresh rates (G-Sync, FreeSync, or VRR), you'll
want to disable [`vsync`](#vsync) for the smoothest frame pacing. On
fixed-rate monitors, enable it to prevent screen tearing.

Window size and position are remembered between sessions, so you only need
to set things up once. Most users won't need to change the display defaults
at all — the out-of-the-box settings work well for the vast majority of
games.


## Presentation modes

TODO(CL) write section based on release notes from here https://github.com/dosbox-staging/dosbox-staging/pull/4438 . just text, no images.

## Dedithering

TODO(CL) write section based on release notes from here https://github.com/dosbox-staging/dosbox-staging/pull/4777 . just text, no images.

## Deinterlacing

TODO(CL) write section based on release notes from here https://github.com/dosbox-staging/dosbox-staging/pull/4689 . just text, no images.



## Configuration settings

You can set the display and window parameters in the `[sdl]` configuration
section.


### Window

##### fullscreen

:   Number of display to use; values depend on OS and user settings (`0` by
    default).


##### fullscreen

:   Start in fullscreen mode.

    Possible values: `on`, `off` *default*{ .default }


##### fullscreen_mode

:   Set fullscreen mode.

    Possible values:

    - `standard` *default*{ .default } -- Use the standard fullscreen mode
      of your operating system.


##### window_decorations

:   Enable window decorations in windowed mode.

    Possible values: `on` *default*{ .default }, `off`


##### window_position

:   Set initial window position for windowed mode.

    Possible values:

    - `auto` *default*{ .default } -- Let the window manager decide the
      position.
    - `X,Y` -- Set window position in X,Y format in logical units (e.g.,
      `250,100`). `0,0` is the top-left corner of the screen. The values
      will be multiplied by the OS-level DPI scaling to get the window
      position in pixels.

    !!! note

        If you want to use pixel coordinates instead and ignore DPI scaling,
        set the `SDL_WINDOWS_DPI_SCALING` environment variable to `0`.


##### window_size

:   Set initial window size for windowed mode. You can still resize the
    window after startup.

    Possible values:

    - `default` *default*{ .default } -- Select the best option based on
      your environment and other factors (such as whether aspect ratio
      correction is enabled).
    - `small`, `medium`, `large` (`s`, `m`, `l`) -- Size the window
      relative to the desktop.
    - `WxH` -- Specify window size in WxH format in logical units (e.g.,
      `1024x768`). The values will be multiplied by the OS-level DPI scaling
      to get the window size in pixels.

    !!! note

        If you want to use pixel coordinates instead and ignore DPI scaling,
        set the `SDL_WINDOWS_DPI_SCALING` environment variable to `0`.


##### window_titlebar

:   Space-separated list of information to be displayed in the window's
    titlebar. If a parameter is not specified, its default value is used
    (`program=name dosbox=auto cycles=on mouse=full` by default).

    Possible parameters:

    - `animation=<value>` -- If set to `on` (default), animate the
      audio/video recording mark. Set to `off` to disable animation; this
      is useful if your screen font produces weird results.

    - `program=<value>` -- Display the name of the running program. Values:
      `none`/`off`, `name` (default), `path`, `segment`, or a custom title
      in quotes (e.g., `'My Title'`).

    - `dosbox=<value>` -- Display "DOSBox Staging" in the title bar. Values:
      `always`, `auto` (default; only shows if no program is running or
      `program=none` is set).

    - `version=<value>` -- Display DOSBox version information. Values:
      `none`/`off` (default), `simple`, `detailed`.

    - `cycles=<value>` -- If set to `on` (default), show CPU cycles
      setting. Set to `off` to disable.

    - `mouse=<value>` -- Mouse capturing hint verbosity level:
      `none`/`off`, `short`, `full` (default).


##### window_transparency

:   Set the transparency of the DOSBox Staging window (`0` by default).
    Valid range from 0 (no transparency) to 90 (high transparency).


### Display

##### display

:   Number of display to use; values depend on OS and user settings (`0` by
    default).


##### output

:   Rendering backend to use for graphics output. Only the `opengl` backend
    has shader support and is thus the preferred option. The `texture` backend
    is only provided as a last resort fallback if OpenGL is not available or
    the OpenGL driver is not Core Profile 3.3 compliant.

    Possible values:

    <div class="compact" markdown>

    - `opengl` *default*{ .default } -- OpenGL backend with shader support.
    - `texture` -- SDL's texture backend with bilinear interpolation.
    - `texturenb` -- SDL's texture backend with nearest-neighbour
      interpolation (no bilinear).

    </div>


##### texture_renderer

:   Render driver to use in `texture` output mode. Use
    `texture_renderer = auto` for an automatic choice (`auto` by default).


### Presentation

##### presentation_mode

:   Set the frame presentation mode.

    Possible values:

    - `auto` *default*{ .default } -- Use `host-rate` if
      [vsync](#vsync) is enabled, otherwise use `dos-rate`.
    - `dos-rate` -- Present frames at the refresh rate of the emulated DOS
      video mode. This is the best option on variable refresh rate (VRR)
      monitors. [vsync](#vsync) is not available with `dos-rate`
      presentation.
    - `host-rate` -- Present frames at the refresh rate of the host display.
      Use this with [vsync](#vsync) enabled on fixed refresh rate monitors
      for fast-paced games where tearing is a problem. `host-rate` combined
      with [vsync](#vsync) disabled can be a good workaround on systems that
      always enforce blocking vsync at the OS level (e.g., forced 60 Hz
      vsync could cause problems with VGA games presenting frames at 70 Hz).


##### vsync

:   Set the host video driver's vertical synchronization (vsync) mode.

    Possible values:

    - `off` *default*{ .default } -- Disable vsync in both windowed and
      fullscreen mode. This is the best option on variable refresh rate (VRR)
      monitors running in VRR mode to get perfect frame pacing, no tearing,
      and low input lag. On fixed refresh rate monitors (or VRR monitors in
      fixed refresh mode), disabling vsync might cause visible tearing in
      fast-paced games.

    - `on` -- Enable vsync in both windowed and fullscreen mode. This can
      prevent tearing in fast-paced games but will increase input lag. Vsync
      is only available with `host-rate` presentation (see
      [presentation_mode](#presentation_mode)).

    - `fullscreen-only` -- Enable vsync in fullscreen mode only. This might
      be useful if your operating system enforces vsync in windowed mode and
      the `on` setting causes audio glitches or other issues in windowed mode
      only. Vsync is only available with `host-rate` presentation (see
      [presentation_mode](#presentation_mode)).

    !!! note

        - For perfectly smooth scrolling in 2D games (e.g., in Pinball Dreams
          and Epic Pinball), you'll need a VRR monitor running in VRR mode
          and vsync disabled. The scrolling in 70 Hz VGA games will always
          appear juddery on 60 Hz fixed refresh rate monitors even with vsync
          enabled.

        - Usually, you'll only get perfectly smooth 2D scrolling in
          fullscreen mode, even on a VRR monitor.

        - For the best results, disable all frame cappers and global vsync
          overrides in your video driver settings.


### Behaviour

##### mute_when_inactive

:   Mute the sound when the window is inactive.

    Possible values: `on`, `off` *default*{ .default }


##### pause_when_inactive

:   Pause emulation when the window is inactive.

    Possible values: `on`, `off` *default*{ .default }


##### screensaver

:   Use `allow` or `block` to override the `SDL_VIDEO_ALLOW_SCREENSAVER`
    environment variable which usually blocks the OS screensaver while the
    emulator is running.

    Possible values: `auto` *default*{ .default }, `allow`, `block`


### Other

##### mapperfile

:   Path to the mapper file (`mapper-sdl2-XYZ.map` by default, where XYZ is
    the current version). Pre-configured maps are bundled in
    `resources/mapperfiles`. These can be loaded by name, e.g., with
    `mapperfile = xbox/xenon2.map`.

    !!! note

        The [`--erasemapper`](../command-line.md#-erasemapper) command line
        option only deletes the default mapper file.
