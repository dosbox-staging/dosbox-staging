# Capture

## Overview

DOSBox Staging can capture screenshots, record audio, and record video
directly from the emulator. By default, all captures are saved to the
`capture` directory in the current working directory. You can change this with
the `capture_dir` setting.


## Screenshots

Screenshots can be saved in multiple formats: **upscaled** (sharp pixels,
aspect-corrected), **rendered** (post-shader, exactly what you see on screen), or
**raw** (the framebuffer's contents with 1:1 pixel aspect ratio).

Use ++ctrl+f5++ / ++cmd+f5++ to take a screenshot using the default format(s)
configured by the [default_image_capture_formats](#default_image_capture_formats)
setting.


## Video capture

Press ++ctrl+f7++ / ++cmd+f7++ to start and stop video recording. The video
is captured as a lossless AVI file.


## Audio capture

Press ++ctrl+f6++ / ++cmd+f6++ to start and stop audio recording. Raw MIDI
and OPL output can also be captured for those who want to tinker with game
music outside the emulator.


## Configuration settings

Capture settings are to be configured in the `[capture]` section.


##### capture_dir

:   Directory where the various captures are saved, such as audio, video,
    MIDI, and screenshot captures (`capture` in the current working directory
    by default).


##### default_image_capture_formats

:   Set the capture format of the default screenshot action.

    If multiple formats are specified separated by spaces, the default
    screenshot action will save multiple images in the specified formats.
    Keybindings for taking single screenshots in specific formats are also
    available.

    Possible values:

    - `upscaled` *default*{ .default } -- The image is bilinear-sharp
      upscaled and the correct aspect ratio is maintained, depending on the
      `aspect` setting. The vertical scaling factor is always an integer.
    - `rendered` -- The post-rendered, post-shader image shown on the screen
      is captured. Filenames end with `-rendered`.
    - `raw` -- The contents of the raw framebuffer is captured (always results
      in square pixels). Filenames end with `-raw`.
