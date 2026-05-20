# Display & window

DOSBox Staging opens in a resizable window by default --- press ++alt+enter++
to toggle fullscreen at any time. For most games, the out-of-the-box display
settings will serve you well and need no adjustment. This chapter covers the
cases where you might want to dig deeper.

The most important decision for most users is how to handle [frame
presentation](#frame-presentation) and [`vsync`](#vsync). If you have a variable
refresh rate monitor (AMD FreeSync, Nvidia G-Sync, or HDMI 2.1 VRR), the
defaults give you perfect frame pacing with no tearing and no further
configuration needed, regardless of any custom refresh rates a program might
use. On a fixed refresh rate monitor, enabling vsync eliminates tearing in
fast-paced games, but it can't fully solve the judder that comes from the
mismatch between DOS refresh rates and your display's fixed rate. Most VGA
games run at 70 Hz, and that difference is perceptible in scrolling and
animation. A VRR monitor is the only complete solution.

In practice, tearing is only noticeable in a relatively small number of games
--- typically fast-paced 3D games from the mid- to late-90s and
smooth-scrolling 2D games such as platformers and pinball simulators. If you
mostly play RPGs, adventures, or strategy games, you can ignore most of this
chapter and use the defaults.

For image output settings such as shaders, CRT emulation, aspect ratio
correction, dedithering, deinterlacing, and image adjustments, see
[Rendering](rendering/shaders.md).


## Frame presentation

DOSBox Staging offers two frame presentation strategies, controlled by the
[`presentation_mode`](#presentation_mode) setting:

- **`dos-rate`** presents frames at the refresh rate of the emulated DOS video
  mode (e.g., 70 Hz for standard VGA). This is the ideal choice for variable
  refresh rate (VRR) monitors (AMD FreeSync, Nvidia G-Sync, or HDMI 2.1 VRR)
  --- the display synchronises directly to the emulated refresh rate, giving
  you perfect frame pacing with no tearing and low input lag.

- **`host-rate`** presents the most recent frame at the host display's refresh
  rate. This is intended for fixed refresh rate monitors (typically 60 Hz)
  with [`vsync`](#vsync) enabled to eliminate screen tearing in fast-paced
  games.

The default **`auto`** mode selects `dos-rate` when vsync is off and
`host-rate` when vsync is on, which is the right choice for most setups. For a
practical example of configuring vsync and presentation modes for a specific
game, see the [Star Wars: Dark Forces](../../getting-started/star-wars-dark-forces.md#display-refresh-rate)
chapter of the getting started guide.

In practice, most users fall into one of three categories:

- **VRR monitor**: Use the defaults --- you'll get perfect
  frame pacing and no tearing in all games regardless of refresh rate. With
  the defaults, `presentation_mode = auto` resolves to `dos-rate` because
  `vsync` defaults to off.

- **Fixed 60 Hz monitor, fast-paced games** (typically mid- to late-90s 3D
  games and smooth-scrolling 2D titles like platformers and pinball games):
  Set `vsync = on` to eliminate tearing in fullscreen. The `auto` presentation
  mode will switch to `host-rate` accordingly. On Windows, macOS, and more
  modern Linux desktops, the OS compositor already provides vsync "for free"
  in windowed mode, so the default `vsync = off` is fine if you only play
  windowed.

- **Fixed 60 Hz monitor, slower games** (RPGs, adventures, strategy): The
  defaults work fine --- tearing is rarely noticeable in these genres.

Note that the game itself might tear on real hardware, in which case DOSBox
Staging will faithfully reproduce that tearing — see [Vsync](#vsync) for
details.


### Refresh rates of DOS graphics standards

Each DOS graphics standard has its own native refresh rate. The mismatch
between these rates and a typical 60 Hz fixed refresh rate display is the
main source of judder in DOS games.

| Graphics standard     | Refresh rate
| ---                   | ---
| SVGA and VESA         | 70 Hz or higher --- 640&times;480 or higher extended and VESA modes
| VGA                   | 60 Hz --- 640&times;480 standard mode only<br>70 Hz --- all other standard modes
| CGA, PCjr, Tandy, EGA | 60 Hz
| Hercules              | 50 Hz

Most VGA games run at 70 Hz. On a fixed 60 Hz display, the 60/70 Hz mismatch
causes perceptible judder on scrolling and animation. A VRR monitor eliminates
this entirely by syncing to the emulated refresh rate (the frame presentation
of the emulator dictates the effective refresh rate of the monitor). On
fixed-rate monitors, enabling vsync prevents tearing but cannot eliminate the
judder inherent in the refresh rate mismatch.

!!! note

    For perfectly smooth scrolling in 2D games (e.g.,
    [Pinball Dreams](https://www.mobygames.com/game/703/pinball-dreams/),
    [Epic Pinball](https://www.mobygames.com/game/263/epic-pinball/)), you'll
    need a VRR monitor running in VRR mode with vsync disabled. Fullscreen
    mode is recommended; in windowed mode you may see some slight residual
    judder even on a VRR monitor. The scrolling in 70 Hz VGA games will always
    appear juddery on 60 Hz fixed refresh rate monitors, even with vsync
    enabled.


### Vsync

Because we're running a game inside an emulator, tearing can be introduced
at _two levels_:

- The game itself might not care about vsync, so it would tear on real
  hardware too (DOSBox Staging accurately emulates this).

- DOSBox Staging might not present the emulated frames to the host operating
  system vsync'ed, which could result in a _second layer_ of tearing on
  fixed refresh monitors, solely introduced by the emulator.

To get zero tearing on **fixed refresh rate** monitors, both conditions must
be met: the game must vsync its own video output (i.e., no tearing on real
hardware), and [`vsync`](#vsync) must be enabled at the DOSBox Staging level.

On **VRR monitors**, the [`vsync`](#vsync) setting must be left at its default
`off` setting --- the monitor will sync automatically to the emulator's frame
presentation rate, so you'll get no additional tearing.

The most important thing to understand is that you have no control over the
game's own vsync behaviour. Many DOS games hardcode it (e.g.,
[Doom](https://www.mobygames.com/game/1068/doom/) runs at a fixed 35 FPS with
vsync). With games that ignore vsync and thus on real hardware too, the best
you can do is not to introduce _additional_ tearing at the emulator level.

The below table summarises all possible combinations between game and DOSBox
level vsync on **fixed refresh monitors**:

<div class="compact vsync" markdown>

| Game uses vsync? | DOSBox vsync enabled? | Result
| ---              | ---                   | ---
| yes              | yes                   | No tearing
| no               | yes                   | Some tearing (also present on real hardware)
| yes              | no                    | Some tearing (introduced by DOSBox Staging)
| no               | no                    | Very bad double tearing (in fast-paced games)

</div>

This table shows the possible outcomes with vsync on and off on **variable
refresh rate (VRR) monitors**:

<div class="compact vsync" markdown>

| Game uses vsync? | Result
| ---              | ---
| yes              | No tearing
| no               | Some tearing (also present on real hardware)

</div>

!!! danger

    Do **not** force vsync globally at the GPU driver level (e.g., in Nvidia
    Control Panel or similar). Leave the global vsync settings at their
    defaults ("let the application decide") and only configure vsync via the
    DOSBox Staging [`vsync`](#vsync) setting. Forcing global vsync causes
    problems with virtually all emulators, not just DOSBox Staging.


### Variable refresh rate (VRR) tips

**AMD FreeSync**, **Nvidia G-Sync**, and **HDMI 2.1 VRR** are all supported.
Just enable variable refresh in your driver settings, set your desktop to the
maximum refresh rate of your monitor, and choose the "let the application
decide" vsync option. DOSBox Staging will automatically take advantage of VRR
with the default settings.

!!! warning "Apple ProMotion notes"

    **Apple ProMotion** is not "true VRR" like FreeSync and G-Sync. It was not
    designed for the same low-latency, dynamic VRR experience, but primarily
    for power efficiency on portable devices. However, if you connect your Mac
    to a true VRR external monitor and select "Variable" refresh rate in
    System Settings, you can take full advantage of VRR in DOSBox Staging.

!!! danger

    Do **not** use any frame limiters; those are only useful for modern games,
    not emulators. Frame limiters **will cause** DOSBox Staging to
    malfunction! Similarly, do **not** force vsync globally or for DOSBox
    Staging specifically.


### Fixed refresh monitor tips

The simplest thing to do on fixed refresh monitors is to leave
[`vsync`](#vsync) disabled. The tearing will only be noticeable in fast-paced
3D games and smooth-scrolling 2D games. In games with mostly static screens
(RPGs, adventures, and strategy games) the tearing is virtually unnoticeable.

If really want to use vsync, you can create custom screen modes that match the
DOS refresh rate exactly. Use the Nvidia Control Panel or [Custom Resolution
Utility
(CRU)](https://www.monitortests.com/forum/Thread-Custom-Resolution-Utility-CRU)
on Windows. Enable *CVT reduced blanking* (CVT-RB or CVT-RBv2) to reach 70 Hz.
For the best results, use the exact fractional **59.713 Hz** and **70.086 Hz**
refresh rates for the nominal "60 Hz" and "70 Hz" DOS rates, respectively. The
drawback is that you need to set the appropriate refresh rate before starting
DOSBox Staging, and games that switch between different refresh rates cannot
be fully accommodated this way.

You can also experiment with using `dos_rate = host` to force the emulated DOS
video mode to present at your host display's refresh rate (e.g., forcing 60 Hz
in 70 Hz VGA games on a fixed 60 Hz monitor). This eliminates the 60/70 Hz
judder without needing a custom screen mode, but it's a hack --- some games
rely on the original refresh rate for timing and may not work correctly. Try
it on a per-game basis.


## Fullscreen and display output

DOSBox Staging starts in a window by default. Set [`fullscreen`](#fullscreen)
to `on` to launch directly into fullscreen, or press ++alt+enter++ to toggle
between windowed and fullscreen at any time. On Windows, if Alt+Tabbing out of
fullscreen is slow or disruptive (typically caused by the graphics driver
opting out of fullscreen optimisations, resulting in exclusive fullscreen), set
[`fullscreen_mode`](#fullscreen_mode) to `forced-borderless` to force
borderless fullscreen instead. This option is only available on Windows.

The [`output`](#output) setting selects the rendering backend. The default
`opengl` backend is the only one with shader support, and most users should
never need to change it. The `texture` and `texturenb` backends exist as
fallbacks for systems that lack OpenGL 3.3 Core Profile support.
[`texture_renderer`](#texture_renderer) is only relevant when using one of the
texture backends; the default `auto` value selects the best available driver.


## Window settings

The [`window_size`](#window_size) and [`window_position`](#window_position)
settings control where and how large the DOSBox Staging window appears on
startup. You can still resize the window freely after launch --- these only set
the initial state. The named sizes (`small`, `medium`, `large`) are relative to
your desktop, while the `WxH` format lets you request an exact size in logical
units.

On multi-monitor setups, use [`display`](#display) to select which screen DOSBox
opens on, and [`window_position`](#window_position) to fine-tune placement.

[`window_decorations`](#window_decorations) controls whether the operating
system's title bar and window borders are shown.
[`window_transparency`](#window_transparency) sets the window transparency level
(0--90%).

!!! note

    Both `window_size` and `window_position` use logical units that are
    multiplied by your OS-level DPI scaling. To use raw pixel coordinates
    instead, set the `SDL_WINDOWS_DPI_SCALING` environment variable to `0`.


## Titlebar customisation

The [`window_titlebar`](#window_titlebar) setting controls what information
appears in the window's title bar. It accepts a space-separated list of
parameters, each in `key=value` format.

The default configuration is:

``` ini
[sdl]
window_titlebar = program=name dosbox=auto cycles=on mouse=full
```

This shows the running program's name, the CPU cycles setting, and mouse
capture hints. "DOSBox Staging" only appears when no program is running.

Here are a few useful configurations:

**Minimal** --- show only "DOSBox Staging", nothing else:

``` ini
window_titlebar = program=none dosbox=always cycles=off mouse=off
```

**Custom game title** --- replace the program name with your own text:

``` ini
window_titlebar = program="Ultima VII" version=simple
```

**Full detail** --- show the full program path and DOSBox version:

``` ini
window_titlebar = program=path version=detailed cycles=on mouse=full
```

When audio or video recording is active, a `[REC]` indicator is prepended to
the title bar (animated by default --- the recording dot blinks). `[PAUSED]`
and `[MUTED]` indicators appear when applicable. Set `animation=off` if the
blinking circle symbol doesn't render correctly with your system font.


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

The [`dos_rate`](#dos_rate) setting below is in the `[dosbox]` section; the
rest are in `[sdl]`.

##### presentation_mode

:   Set the frame presentation mode.

    Possible values:

    - `auto` *default*{ .default } -- Use `host-rate` if
      [`vsync`](#vsync) is enabled, otherwise use `dos-rate`.

    - `dos-rate` -- Present frames at the refresh rate of the emulated DOS
      video mode. This is the best option on variable refresh rate (VRR)
      monitors. [`vsync`](#vsync) is not available with `dos-rate`
      presentation.

    - `host-rate` -- Present frames at the refresh rate of the host display.
      Use this with [`vsync`](#vsync) enabled on fixed refresh rate monitors
      for fast-paced games where tearing is a problem. `host-rate` combined
      with [`vsync`](#vsync) disabled can be a good workaround on systems that
      always enforce blocking vsync at the OS level (e.g., forced 60 Hz
      vsync could cause problems with VGA games presenting frames at 70 Hz).


##### dos_rate

:   Override the emulated DOS video mode's refresh rate with a custom rate.

    Possible values:

    - `default` *default*{ .default } -- Don't override; use the emulated
      DOS video mode's refresh rate.
    - `host` -- Override the refresh rate of all DOS video modes with the
      refresh rate of your monitor. This might allow you to play some 70 Hz
      VGA games with perfect [`vsync`](#vsync) on a 60 Hz fixed refresh rate
      monitor.
    - `<number>` -- Override the refresh rate of all DOS video modes with a
      fixed rate specified in Hz (valid range is from 24.000 to 1000.000).
      This is a niche option for a select few fast-paced mid to late 1990s 3D
      games for high refresh rate gaming.

    !!! important

        Many games will misbehave when overriding the DOS video mode's
        refresh rate with non-standard values. This can manifest in glitchy
        video, sped-up or slowed-down audio, jerky mouse movement, mouse
        button presses not being registered, and even gameplay bugs.
        Overriding the DOS refresh rate is a hack that only works acceptably
        with a small subset of all DOS games (typically mid to late 1990s
        games).


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
      [`presentation_mode`](#presentation_mode)).

    - `fullscreen-only` -- Enable vsync in fullscreen mode only. This might
      be useful if your operating system enforces vsync in windowed mode and
      the `on` setting causes audio glitches or other issues in windowed mode
      only. Vsync is only available with `host-rate` presentation (see
      [`presentation_mode`](#presentation_mode)).

    !!! note

        - For perfectly smooth scrolling in 2D games (e.g., in
          [Pinball Dreams](https://www.mobygames.com/game/703/pinball-dreams/)
          and [Epic Pinball](https://www.mobygames.com/game/263/epic-pinball/)),
          you'll need a VRR monitor running in VRR mode and vsync disabled.
          The scrolling in 70 Hz VGA games will always appear juddery on 60 Hz
          fixed refresh rate monitors even with vsync enabled.

        - Usually, you'll only get perfectly smooth 2D scrolling in fullscreen
          mode, even on a VRR monitor.

        - For the best results, disable all frame cappers and global vsync
          overrides in your video driver settings.

