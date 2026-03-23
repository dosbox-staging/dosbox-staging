# Audio capture

DOSBox Staging can capture screenshots, record audio, and record video
directly from the emulator. Screenshots can be saved in multiple formats:
upscaled (clean, aspect-corrected), rendered (post-shader, exactly what you
see on screen), or raw (the framebuffer contents with square pixels).

The most common [shortcuts](../shortcuts.md): ++ctrl+f5++ / ++cmd+f5++ for
screenshots, ++ctrl+f6++ / ++cmd+f6++ to start and stop audio recording, and
++ctrl+f7++ / ++cmd+f7++ for video recording. Raw MIDI and OPL output can
also be captured for those who want to tinker with game music outside the
emulator.

All captures are saved to the `capture/` directory in the current working
directory by default. You can change this with the `capture_dir` setting.


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
