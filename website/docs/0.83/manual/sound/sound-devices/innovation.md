# Innovation SSI-2001

The ultra-rare **Innovation SSI-2001** card was released in April 1989 by
Innovation Computer Corporation, utilizing the MOS 6581 (aka SID) chip, as
found in the Commodore 64. Microprose later released its own variant,
called "The Entertainer".

Only 15 games are known to support this truly unique sound option.

??? note "Games with Innovation SS-2001 support"

    <div class="compact" markdown>

    - [Airball (1988)](https://www.mobygames.com/game/44/airball/)
    - [Bad Blood (1990)](https://www.mobygames.com/game/2127/bad-blood/)
    - [Battle Chess II: Chinese Chess (1990)](https://www.mobygames.com/game/1301/battle-chess-ii-chinese-chess/)
    - [BattleTech: The Crescent Hawks' Revenge (1990)](https://www.mobygames.com/game/233/battletech-the-crescent-hawks-revenge/)
    - [F-19 Stealth Fighter (1988)](https://www.mobygames.com/game/512/f-19-stealth-fighter/)
    - [Falcon A.T. (1990)](https://www.mobygames.com/game/1628/falcon-at/)
    - [Gunship versions 429.04 and 429.05 (1986)](https://www.mobygames.com/game/335/gunship/)
    - [Harpoon (1989)](https://www.mobygames.com/game/8308/harpoon/)
    - [Joe Montana Football (1991)](https://www.mobygames.com/game/21183/joe-montana-football/)
    - [J.R.R. Tolkien's The Lord of the Rings, Vol. I (1990)](https://www.mobygames.com/game/3870/jrr-tolkiens-the-lord-of-the-rings-vol-i/)
    - [Pirates! versions 432.1, 432.2, and 432.3 (1987)](https://www.mobygames.com/game/214/sid-meiers-pirates/)
    - [Red Storm Rising (1988)](https://www.mobygames.com/game/1655/red-storm-rising/)
    - [Super Jeopardy! (1990)](https://www.mobygames.com/game/33449/super-jeopardy/)
    - [Ultima VI: The False Prophet (1990)](https://www.mobygames.com/game/104/ultima-vi-the-false-prophet/)
    - [Windwalker (1989)](https://www.mobygames.com/game/1636/windwalker/)

    </div>

## Mixer channel

The Innovation SSI-2001 outputs to the **INNOVATION** [mixer
channel](../mixer.md#list-of-mixer-channels).


## Configuration settings

You can set the parameters of the Innovation SSI-2001 card in the
`[innovation]` configuration section.


##### innovation

:   Enable emulation of the Innovation SSI-2001 and Microprose's "The
Entertainer" sound cards on base address 280. These use the iconic MOS 6581 SID
chip of the Commodore 64 personal computer from the 1980s. Only 15 games are
known to support these cards.


##### innovation_sid_filter

:   The SID's analog filtering meant that each chip was physically unique.

    Adjusts the 6581's filtering strength as a percent from 0 to 100 (defaults to 50).


##### innovation_filter

:   Filter for the Innovation audio output:

    - `off` *default*{ .default } -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../output-filters.md#custom-filter-settings)
      for details.


