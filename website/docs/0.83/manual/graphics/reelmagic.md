# ReelMagic

The **ReelMagic** (also known as **REALmagic**) was an MPEG-1 hardware decoder
card from Sigma Designs, released in 1993. At a time when PCs could barely
handle software video playback, this card enabled smooth full-motion video in
a handful of DOS games, [Return to
Zork](https://www.mobygames.com/game/1219/return-to-zork/), [Lord of the
Rings](https://www.mobygames.com/game/3870/jrr-tolkiens-the-lord-of-the-rings-vol-i/),
and [The Horde](https://www.mobygames.com/game/6142/the-horde/) among them.

Only a small number of titles ever supported it, so this is a niche feature.
If you're not specifically trying to run one of those games, you can safely
ignore this section entirely.

The following DOS games are known to use the ReelMagic card:

<div class="compact" markdown>

- [Crime Patrol](https://www.mobygames.com/game/459/crime-patrol/)
- [Crime Patrol 2: Drug Wars](https://www.mobygames.com/game/768/drug-wars/)
- [Dragon's Lair](https://www.mobygames.com/game/1504/dragons-lair/)
- [Entity](https://www.mobygames.com/game/6072/entity/)
- [Flash Traffic: City of Angels](https://www.mobygames.com/game/14651/flash-traffic-city-of-angels/)
- [Lord of the Rings](https://www.mobygames.com/game/3870/jrr-tolkiens-the-lord-of-the-rings-vol-i/) (Interplay, 1993)
- [Man Enough](https://www.mobygames.com/game/29122/man-enough/)
- [Return to Zork](https://www.mobygames.com/game/1219/return-to-zork/)
- [Space Ace](https://www.mobygames.com/game/6009/space-ace/)
- [The Horde](https://www.mobygames.com/game/6142/the-horde/)

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


##### reelmagic_fcode

:   Override the frame rate code used during video playback.

    Possible values:

    - `0` *default*{ .default } -- No override: attempt automatic rate
      discovery.
    - `1` to `7` -- Override the frame rate to one of the following: 1=23.976,
      2=24, 3=25, 4=29.97, 5=30, 6=50, or 7=59.94 FPS.


