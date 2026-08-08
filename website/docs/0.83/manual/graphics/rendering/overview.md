# Rendering overview

The `[render]` section controls how DOSBox Staging turns an emulated video
frame into the picture you see on your display. It covers the final stages of
video output: scaling the image to fit the viewport, correcting its aspect
ratio, applying shaders and filtering, adjusting its colours and brightness,
and correcting certain video artifacts.

These settings do **not** affect the [graphics hardware](../adapters.md) or video mode presented
to the game. They only change how the resulting image is displayed.

The rendering settings are organised into the following areas:

* [Shaders](shaders.md) --- Choose how the image is rendered, from adaptive CRT
  emulation to sharp, pixel-perfect output.

* [Aspect ratios & scaling](aspect-ratios-and-scaling.md) --- Control pixel
  aspect ratio correction, integer scaling, viewport size, borders, and
  stretching.

* [Colour & image adjustments](colour-and-image-adjustments.md) --- Adjust
  brightness, contrast, gamma, saturation, colour temperature, CRT colour
  profiles, monochrome palettes, and custom CGA colours.

* [Special features](special-features.md) --- Apply targeted corrections such as
  dedithering for EGA/CGA artwork and deinterlacing for FMV (Full Motion
  Video) games.

For most games, the defaults produce a good result. These settings are mainly
useful when you want to change the presentation, or when a particular game
looks stretched, incorrectly scaled, too soft or sharp, too dark, or visibly
interlaced.
