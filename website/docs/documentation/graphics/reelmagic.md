# ReelMagic

## Overview

The ReelMagic (marketed as REALmagic) was an MPEG-1 hardware decoder card
from Sigma Designs, released in 1993. At a time when PCs could barely
handle software video playback, this card enabled smooth full-motion video
in a handful of DOS games — *Return to Zork*, *Lord of the Rings*,
and *The Horde* among them.

Only a small number of titles ever supported it, so this is a niche feature.
If you're not specifically trying to run one of those games, you can safely
ignore this section entirely.

The following DOS games are known to use the ReelMagic card:

<div class="compact" markdown>

- Crime Patrol
- Crime Patrol 2: Drug Wars
- Dragon's Lair
- Entity
- Flash Traffic: City of Angels
- Lord of the Rings (Interplay, 1993)
- Man Enough
- Return to Zork
- Space Ace
- The Horde

</div>


## Configuration settings

You can set the ReelMagic parameters in the `[reelmagic]` configuration
section.


##### reelmagic

:   ReelMagic (aka REALmagic) MPEG playback support.

    Possible values:

    <div class="compact" markdown>

    - `off` *default*{ .default } -- Disable support.
    - `cardonly` -- Initialise the card without loading the `FMPDRV.EXE`
      driver.
    - `on` -- Initialise the card and load the `FMPDRV.EXE` on startup.

    </div>


##### reelmagic_fcode

:   Override the frame rate code used during video playback.

    Possible values:

    - `0` *default*{ .default } -- No override: attempt automatic rate
      discovery.
    - `1` to `7` -- Override the frame rate to one of the following: 1=23.976,
      2=24, 3=25, 4=29.97, 5=30, 6=50, or 7=59.94 FPS.


##### reelmagic_key

:   Set the 32-bit magic key used to decode the game's videos.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- Use the built-in routines to determine
      the key.
    - `common` -- Use the most commonly found key, which is `0x40044041`.
    - `thehorde` -- Use The Horde's key, which is `0xC39D7088`.
    - `<custom>` -- Set a custom key in hex format (e.g., `0x12345678`).

    </div>
