# Innovation SSI-2001

Released in April 1989 by Innovation Computer Corporation, utilizing the MOS
6581 (aka SID) chip, as found in the Commodore 64.

Very few games support this sound option.


## Compatible games

<div class="compact" markdown>

- [Airball](https://www.mobygames.com/game/44/airball/)
- [Bad Blood](https://www.mobygames.com/game/2127/bad-blood/)
- [Battle Chess II: Chinese Chess](https://www.mobygames.com/game/1301/battle-chess-ii-chinese-chess/)
- [BattleTech: The Crescent Hawks' Revenge](https://www.mobygames.com/game/233/battletech-the-crescent-hawks-revenge/)
- [F-19 Stealth Fighter](https://www.mobygames.com/game/512/f-19-stealth-fighter/)
- [Falcon A.T.](https://www.mobygames.com/game/1628/falcon-at/)
- [Harpoon](https://www.mobygames.com/game/8308/harpoon/)
- [Joe Montana Football](https://www.mobygames.com/game/21183/joe-montana-football/)
- [J.R.R. Tolkien's The Lord of the Rings, Vol. I](https://www.mobygames.com/game/3870/jrr-tolkiens-the-lord-of-the-rings-vol-i/)
- [Red Storm Rising](https://www.mobygames.com/game/1655/red-storm-rising/)
- [Super Jeopardy!](https://www.mobygames.com/game/33449/super-jeopardy/)
- [Ultima VI: The False Prophet](https://www.mobygames.com/game/104/ultima-vi-the-false-prophet/)
- [Windwalker](https://www.mobygames.com/game/1636/windwalker/)

</div>


## Configuration parameters

You can set the parameters of the Innovation SSI-2001 card in the
`[innovation]` configuration section.

##### 6581filter

:   The SID's analog filtering meant that each chip was physically unique.

    Adjusts the 6581's filtering strength as a percent from 0 to 100 (defaults to 50).


##### 8580filter

:   Adjusts the 8580's filtering strength as a percent from 0 to 100 (defaults to 50).


##### innovation_filter

:   Filter for the Innovation audio output:

    - `off` *default*{ .default } -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.


##### sidclock

:   The SID chip's clock frequency, which is jumperable on reproduction cards:

    - `default` *default*{ .default } -- Uses 0.895 MHz, per the original SSI-2001 card.
    - `c64ntsc` -- Uses 1.023 MHz, per NTSC Commodore PCs and the DuoSID.
    - `c64pal` --  Uses 0.985 MHz, per PAL Commodore PCs and the DuoSID.
    - `hardsid` -- Uses 1.000 MHz, available on the DuoSID.


##### sidmodel

:   Model of chip to emulate in the Innovation SSI-2001 card:

    - `none` *default*{ .default } --  Disables the card.
    - `auto` --  Selects the 6581 chip.
    - `6581` --  The original chip, known for its bassy and rich character.
    - `8580` --  A later revision that more closely matched the SID specification.
               It fixed the 6581's DC bias and is less prone to distortion.
               The 8580 is an option on reproduction cards, like the DuoSID.


##### sidport

:   The IO port address of the Innovation SSI-2001.

    Possible values: `240`, `260`, `280` *default*{ .default }, `2a0`, `2c0`.

