# Special features

Beyond authentically emulating real CRT monitors, DOSBox Staging offers two
visual enhancements that are impossible on real hardware: blending away dither
patterns in old EGA and CGA games, and deinterlacing FMV videos to remove the
distracting black lines found in many 90s games.

Both are opt-in and work best when enabled per-game rather than globally.


## FMV deinterlacer

Many 90s DOS games displayed full-motion video (FMV) using interlaced
rendering, showing only every second line of each frame. This halved storage
requirements and looked fine on small CRTs where the brain filled in the gaps,
but on modern flat screens the alternating black lines look distracting and
halve the apparent brightness.

The [`deinterlacing`](#deinterlacing) setting automatically detects interlaced
regions within each frame and reconstructs the missing lines. The detection is
intelligent; it only touches interlaced areas while leaving HUDs, UI frames,
subtitles, and mouse cursors untouched.

Both common interlacing patterns are supported: standard line interlacing
(alternating black horizontal lines, used by most games) and "dot interlacing"
--- a special pattern used by the CD-ROM versions of
[Dune](https://www.mobygames.com/game/380/dune/) and
[KGB](https://www.mobygames.com/game/2894/kgb/).

<!--- TODO image examples --->

!!! note

    Enabling vertical [`integer_scaling`](aspect-ratios-and-scaling.md#integer_scaling)
    is recommended on lower resolution displays to avoid interference artifacts
    when using lower deinterlacing strengths. Alternatively, use `full`
    strength to completely eliminate all potential interference patterns.

??? note "Games that benefit from deinterlacing"

    Here's a non-exclusive list of DOS games that benefit from the
    deinterlacing feature:

    <div class="compact" markdown>

    - [Angel Devoid (1996)](https://www.mobygames.com/game/3468/angel-devoid-face-of-the-enemy/)
    - [Crusader: No Regret (1996)](https://www.mobygames.com/game/852/crusader-no-regret/)
    - [Crusader: No Remorse (1995)](https://www.mobygames.com/game/851/crusader-no-remorse/)
    - [CyberMage (1995)](https://www.mobygames.com/game/791/cybermage-darklight-awakening/)
    - [Gabriel Knight 2 (1995)](https://www.mobygames.com/game/118/the-beast-within-a-gabriel-knight-mystery/)
    - [Heroes of Might and Magic II (1996)](https://www.mobygames.com/game/1513/heroes-of-might-and-magic-ii-the-succession-wars/)
    - [Phantasmagoria (1995)](https://www.mobygames.com/game/1164/roberta-williams-phantasmagoria/)
    - [Wing Commander IV (1996)](https://www.mobygames.com/game/343/wing-commander-iv-the-price-of-freedom/)

    </div>


## Checkerboard dedithering

Many DOS games used dithering --- alternating pixel patterns --- to simulate
more colours than their limited palette could display. This was especially
common in 16-colour EGA games, where checkerboard patterns created the
illusion of intermediate shades.

The [`dedithering`](#dedithering) setting detects these checkerboard patterns
and blends them into solid colours. It works with any graphics adapter (CGA,
EGA, VGA, Hercules) and any resolution, and can be combined with any shader.

<!--- TODO image examples --->

!!! note

    Dedithering is *not* a more authentic representation. On real PC CRT
    monitors, dither patterns were clearly visible. Blending them into solid
    colours was only a thing on consoles connected to consumer TV sets. For a
    more authentic look, use [`shader`](shaders.md#shader) `= crt-auto`
    instead.

??? note "Games that benefit from dedithering"

    Here's a non-exclusive list of DOS games that benefit from the
    dedithering feature:

    <div class="compact" markdown>

    - [Leisure Suit Larry 2 (1988)](https://www.mobygames.com/game/409/leisure-suit-larry-goes-looking-for-love-in-several-wrong-places/) & [3](https://www.mobygames.com/game/412/leisure-suit-larry-iii-passionate-patti-in-pursuit-of-the-pulsat/)
    - [Quest for Glory I (1989)](https://www.mobygames.com/game/168/heros-quest-so-you-want-to-be-a-hero/) & [II](https://www.mobygames.com/game/169/quest-for-glory-ii-trial-by-fire/)
    - [Space Quest III (1989)](https://www.mobygames.com/game/142/space-quest-iii-the-pirates-of-pestulon/)
    - [The Secret of Monkey Island (1990)](https://www.mobygames.com/game/616/the-secret-of-monkey-island/) (EGA version)
    - [Loom (1990)](https://www.mobygames.com/game/176/loom/)
    - [Spellcasting 101 (1990)](https://www.mobygames.com/game/1027/spellcasting-101-sorcerers-get-all-the-girls/)
    - [Timequest (1991)](https://www.mobygames.com/game/1026/timequest/)
    - [Gateway (1992)](https://www.mobygames.com/game/317/frederik-pohls-gateway/)

    </div>


## Configuration settings

You can set these in the `[render]` configuration section.

##### deinterlacing

:   Remove black lines from interlaced videos. Use with games that display
    video content with alternating black lines.

    Possible values:

    <div class="compact" markdown>

    - `off` *default*{ .default } -- Disable deinterlacing.

    - `on` -- Enable deinterlacing at `medium` strength.

    - `light` -- Black scanlines are softened to mimic the CRT look.

    - `medium` -- Best balance between removing black lines, increasing
      brightness, and keeping the higher resolution look.

    - `strong` -- Image brightness is almost completely restored at the
      expense of diminishing the higher resolution look.

    - `full` -- Completely removes black lines and maximises brightness, but
      the image will appear blockier.

    </div>

    !!! note

        Enabling vertical [`integer_scaling`](aspect-ratios-and-scaling.md#integer_scaling)
        is recommended on lower resolution displays to avoid interference
        artifacts when using lower deinterlacing strengths. Alternatively, use
        `full` strength to completely eliminate all potential interference
        patterns.


##### dedithering

:   Remove checkerboard dither patterns from the video output. Useful with
    CGA and EGA games that use dither patterns to create the illusion of more
    colours.

    Possible values:

    - `off` *default*{ .default } -- Disable dedithering.

    - `on` -- Enable dedithering at full strength.

    - `<number>` -- Set dedithering strength from 0 (off) to 100 (full
      strength). Lower values softly blend dither patterns; higher values
      blend between the original and the fully dedithered image.

    !!! note

        - Dedithering only works in OpenGL output mode.

        - Dedithering is applied to rendered screenshots, but not to raw and
          upscaled screenshots and video captures.
