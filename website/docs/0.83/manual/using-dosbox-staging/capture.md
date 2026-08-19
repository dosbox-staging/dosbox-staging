# Audio & video captures

DOSBox Staging can capture screenshots, record audio, and record video
directly from the emulator. By default, all captures are saved to the
`capture` folder in the current [working
directory](starting.md#the-working-directory). You can change this with the
[`capture_dir`](#capture_dir) setting.


## Screenshots

Screenshots can be saved in multiple formats: **upscaled** (sharp pixels,
aspect-corrected), **rendered** (post-shader, exactly what you see on
screen), or **raw** (the framebuffer's contents with 1:1 pixel aspect
ratio).

Use ++ctrl+f5++ / ++cmd+f5++ to take a screenshot using the default
format(s) configured by the
[`default_image_capture_formats`](#default_image_capture_formats) setting.
Keybindings for taking single screenshots in a specific format are also
available.

Screenshots are captured in the background on separate threads, so taking
one doesn't pause emulation or cause audio glitches, even if you spam the
screenshot hotkey or capture at 4K in `rendered` mode.


### Aspect ratio correction

By default, screenshots are captured in the same aspect ratio you see on
screen (the `upscaled` format), not the raw, non-corrected image. This
matters because most 320&times;200 and similar low-resolution DOS games need
aspect ratio correction to look right; only a small fraction of games
actually want square pixels. DOSBox Staging performs aspect ratio correction
out of the box, and aspect-ratio-correct screenshots follow the same logic,
rather than reproducing the square-pixel captures common on emulators (and,
as a result, across many DOS game videos and screenshots online).

In `upscaled` mode, the image is integer or bilinear-sharp upscaled to
around 1200 pixels of vertical resolution. For example:

- **320&times;200** content is upscaled to 1600&times;1200
- **640&times;480** content is upscaled to 1920&times;1440
- **640&times;350** content is upscaled to 1400&times;1050 (3&times; vertical,
  2.1875&times; horizontal scaling)

Upscaled captures are only about 30% larger than raw screenshots on
average.

`raw` captures always contain the actual emulated DOS framebuffer image at its
native resolution (e.g., 320&times;200 modes are captured as 320&times;200 PNG
files), regardless of other settings. The one exception is
[composite](../graphics/composite-video.md) CGA, PCjr, and Tandy modes, which
are internally rendered width-doubled to leave enough horizontal resolution
for composite artifacts; raw captures of these modes preserve that
width-doubling (e.g., 640&times;200 composite CGA is captured as a
1280&times;200 PNG).


## Video capture

Press ++ctrl+f7++ / ++cmd+f7++ to start and stop video recording. Video is
captured as an AVI file using the lossless ZMBV codec. Like `raw`
screenshots, video capture records the raw emulated framebuffer contents
without any CRT shader applied, though deinterlacing is applied. Video
capture uses the optimized zlib-ng library, which is several times faster
than plain zlib, so you can capture at higher resolutions without dropped
frames or glitches, even on older CPUs.

!!! warning

    If your host CPU can't keep up with recording in realtime, MIDI output may
    need to be timestretched a little, and other audio may need to be
    stretched too, to keep it in sync with the video. This can introduce
    audible artifacts. Enabling [CPU
    throttling](../system/cpu.md#cpu-throttling) is one way to reduce this,
    but if it keeps happening, the real fix is a faster machine.


## Audio capture

Press ++ctrl+f6++ / ++cmd+f6++ to start and stop audio recording. Raw MIDI
and OPL output can also be captured for those who want to tinker with game
music outside the emulator.


## Shortcuts

| Windows / Linux | macOS          | Action                           |
| ---             | ---            | ---                              |
| ++ctrl+f5++     | ++cmd+f5++     | Take screenshot (default format) |
| ++alt+f5++      | ++alt+f5++     | Take rendered screenshot         |
| ++ctrl+f6++     | ++cmd+f6++     | Start/stop audio recording       |
| ++ctrl+alt+f6++ | ++cmd+alt+f6++ | Start/stop MIDI recording        |
| ++ctrl+f7++     | ++cmd+f7++     | Start/stop video recording       |

See the [Keyboard shortcuts](../appendices/shortcuts.md) appendix for the full
list of shortcuts.


## Configuration settings

Capture settings are to be configured in the `[capture]` section.

##### capture_dir

:   Directory where the various captures are saved, such as audio, video,
    MIDI, and screenshot captures (`capture` in the current [working
    directory](starting.md#the-working-directory) by default).

##### default_image_capture_formats

:   Set the capture format of the default screenshot action.

    If multiple formats are specified separated by spaces, the default
    screenshot action will save multiple images in the specified formats.
    Keybindings for taking single screenshots in specific formats are also
    available.

    Possible values:

    - `upscaled` *default*{ .default } -- The image is captured in the same
      aspect ratio you see on screen. It's integer or bilinear-sharp
      upscaled to around 1200 pixels of vertical resolution, and always uses
      sharp pixels regardless of the shader in use.

    - `rendered` -- The post-rendered, post-shader image is captured -- an
      exact pixel-by-pixel replica of what you see on screen. Filenames end
      with `-rendered`.

    - `raw` -- The contents of the raw framebuffer are captured (always
      results in square pixels). Filenames end with `-raw`.

    Example, capturing both raw and upscaled images on every screenshot:

    ```ini
    [capture]
    default_image_capture_formats = upscaled raw
    ```
