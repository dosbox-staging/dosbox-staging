# Rendering overview

The `[render]` section controls DOSBox Staging's final video presentation.
After the emulated graphics adapter has produced a frame, these settings decide
how that frame is scaled to your window or screen, which shader or filtering
pass is applied, how its colours are mapped, and whether a few common video
artifacts should be corrected. This section is about the image you see, not
which graphics adapter or DOS video mode the game uses.

[Shaders](shaders.md) choose the overall presentation style, from adaptive CRT
monitor emulation to clean sharp-pixel output. [Aspect ratios &
scaling](aspect-ratios-and-scaling.md) controls pixel aspect ratio correction,
integer scaling, and viewport size so DOS-era modes keep their intended shape
on modern displays. [Colour & image
adjustments](colour-and-image-adjustments.md) covers monitor response, CRT
colour profiles, monochrome palettes, and manual picture tuning. [Special
features](special-features.md) contains targeted per-game cleanup tools such
as dedithering for patterned EGA/CGA artwork and deinterlacing for FMV or
other interlaced video content.

Most games look right with the defaults. Use these pages when a game looks
stretched, too large, too small, too sharp, too soft, too dark, visibly
interlaced, or when you want to choose a different balance between authentic
CRT presentation and crisp modern output.

- [Shaders](shaders.md) - Choose adaptive CRT shaders, sharp-pixel output, and
  shader presets for different graphics standards and viewport sizes.
- [Aspect ratios & scaling](aspect-ratios-and-scaling.md) - Control pixel
  aspect ratio correction, integer scaling, viewport sizing, black borders,
  and custom stretch.
- [Colour & image adjustments](colour-and-image-adjustments.md) - Tune
  brightness, contrast, gamma, saturation, colour temperature, CRT colour
  profiles, monochrome palettes, and CGA colour overrides.
- [Special features](special-features.md) - Enable dedithering and
  deinterlacing for games that benefit from those specific cleanup passes.
