# Display & window

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
at all --- the out-of-the-box settings work well for the vast majority of
games.

For image output settings such as shaders, CRT emulation, aspect ratio
correction, dedithering, deinterlacing, and image adjustments, see
[Rendering](rendering.md).


## Frame presentation

DOSBox Staging offers two frame presentation strategies, controlled by the
[presentation_mode](#presentation_mode) setting:

- **`dos-rate`** presents frames at the refresh rate of the emulated DOS video
mode (e.g., 70 Hz for standard VGA). This is the ideal choice for variable
refresh rate (VRR) monitors (G-Sync, FreeSync, or VRR) --- the display
synchronises directly to the emulated refresh rate, giving you perfect frame
pacing with no tearing and low input lag.

- **`host-rate`** presents the most recent frame at the host display's refresh
rate. This is intended for fixed refresh rate monitors (typically 60 Hz) with
[vsync](#vsync) enabled to eliminate screen tearing in fast-paced games.

The default **`auto`** mode selects `dos-rate` when vsync is off and `host-rate`
when vsync is on, which is the right choice for most setups. For a practical
example of configuring vsync and presentation modes for a specific game, see
the [Star Wars: Dark Forces](../../getting-started/star-wars-dark-forces.md#display-refresh-rate)
chapter of the getting started guide.

In practice, most users fall into one of three categories:

- **VRR monitor**: Use the defaults (`dos-rate` and `vsync` will be off).
  Perfect pacing, no tearing.

- **Fixed 60 Hz monitor, fast-paced games**: Set `vsync = on` to eliminate
  tearing in fullscreen. The `auto` presentation mode will switch to
  `host-rate` accordingly.

- **Fixed 60 Hz monitor, slower games** (RPGs, adventures, strategy): The
  defaults work fine --- tearing is rarely noticeable in these genres.

### Refresh rates of DOS graphics standards

Each DOS graphics standard has its own native refresh rate. The mismatch
between these rates and a typical 60 Hz fixed refresh rate display is the
main source of judder in DOS games.

| Graphics standard      | Refresh rate                                                                     |
|------------------------|----------------------------------------------------------------------------------|
| SVGA and VESA          | 70 Hz or higher --- 640&times;480 or higher extended and VESA modes              |
| VGA                    | 60 Hz --- 640&times;480 standard mode only<br>70 Hz --- all other standard modes |
| CGA, PCjr, Tandy, EGA  | 60 Hz                                                                            |
| Hercules               | 50 Hz                                                                            |

Most VGA games run at 70 Hz. On a fixed 60 Hz display, the 60/70 Hz mismatch
causes perceptible judder on scrolling and animation. A VRR monitor eliminates
this entirely by syncing to the emulated refresh rate (the frame presentation
of the emulator dictates the effective refresh rate of the monitor). On
fixed-rate monitors, enabling vsync prevents tearing but cannot eliminate the
judder inherent in the refresh rate mismatch.

!!! note

    For perfectly smooth scrolling in 2D games (e.g., [Pinball Dreams](https://www.mobygames.com/game/703/pinball-dreams/), [Epic
    Pinball](https://www.mobygames.com/game/263/epic-pinball/)), you'll need a VRR monitor running in VRR mode with vsync
    disabled. The scrolling in 70 Hz VGA games will always appear juddery on
    60 Hz fixed refresh rate monitors, even with vsync enabled.

!!! tip "Creating custom 60 and 70 Hz screen modes"

    On fixed-rate monitors, you can create custom screen modes that match
    the DOS refresh rate exactly. Use the Nvidia Control Panel or [Custom
    Resolution Utility
    (CRU)](https://www.monitortests.com/forum/Thread-Custom-Resolution-Utility-CRU)
    on Windows. Enable *CVT reduced blanking* (CVT-RB or CVT-RBv2) to
    reach 70 Hz. For the best results, use the exact fractional
    **59.713 Hz** and **70.086 Hz** refresh rates for the nominal "60 Hz"
    and "70 Hz" DOS rates, respectively. The drawback is that you need to
    set the appropriate refresh rate before starting DOSBox Staging, and
    games that switch between different refresh rates cannot be fully
    accommodated this way.

### Vsync

Because we're running a game inside an emulator, tearing can be introduced
at _two levels_:

- The game itself might not care about vsync, so it would tear on real
  hardware too (DOSBox faithfully emulates this).

- DOSBox might not present the emulated frames to the host operating
  system vsync'ed, which could result in a _second layer_ of tearing on fixed
  refresh monitors, solely introduced by the emulator.

To get zero tearing on **fixed refresh rate** monitors, both conditions must
be met: the game must vsync its own video output (i.e., no tearing on real
hardware), and [vsync](#vsync) must be enabled at the DOSBox Staging level.

On **VRR monitors**, the [vsync](#vsync) setting must be left at its default `off` setting
--- the monitor will sync automatically to the emulator's frame presentation
rate, so you'll get no additional tearing.

The most important thing to understand is that you have no control over the
game's own vsync behaviour. Many DOS games hardcode it (e.g.,
[Doom](https://www.mobygames.com/game/1068/doom/) runs at a fixed 35 FPS with
vsync). With games that ignore vsync and thus on real hardware too, the best
you can do is not to introduce _additional_ tearing at the emulator level.

The below table summarise all possible combinations between game and DOSBox
level vsync on **fixed refresh monitors**:

<div class="compact vsync" markdown>

| Game uses vsync? | DOSBox vsync enabled? | Result
| ---              | ---                   | ---
| yes              | yes                   | No tearing
| no               | yes                   | Some tearing (also present on real hardware)
| yes              | no                    | Bad tearing (in fast-paced games)
| no               | no                    | Very bad double tearing (in fast-paced games)

</div>

The below table summarise all possible combinations between game and DOSBox
level vsync on **fixed refresh monitors**:

<div class="compact vsync" markdown>

| Game uses vsync? | Result
| ---              | ---
| yes              | No tearing
| no               | Some tearing (also present on real hardware)

</div>

!!! warning

    Do **not** force vsync globally at the GPU driver level (e.g., in Nvidia
    Control Panel or similar). Leave the global vsync settings at their
    defaults ("let the application decide") and only configure vsync via the
    DOSBox Staging [vsync](#vsync) setting. Forcing global vsync causes
    problems with virtually all emulators, not just DOSBox Staging.


## Configuration settings

You can set the display and window parameters in the `[sdl]` configuration
section.


### Window

##### fullscreen

:   Start in fullscreen mode.

    Possible values: `on`, `off` *default*{ .default }


##### fullscreen_mode

:   Set fullscreen mode.

    Possible values:

    - `standard` *default*{ .default } -- Use the standard fullscreen mode
      of your operating system.
    - `forced-borderless` -- Force borderless fullscreen operation on Windows.
      Use this if your graphics card driver opts out of fullscreen
      optimisations, resulting in exclusive fullscreen that makes Alt+Tabbing
      cumbersome. This mode is only available on Windows.


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

        - For perfectly smooth scrolling in 2D games (e.g., in [Pinball Dreams](https://www.mobygames.com/game/703/pinball-dreams/)
          and [Epic Pinball](https://www.mobygames.com/game/263/epic-pinball/)), you'll need a VRR monitor running in VRR mode
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
