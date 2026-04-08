# IBM PS/1 Audio Card

Optional audio card released in 1990 for IBM PS/1 computers (models 2011 and
2121). Relatively few games support this option, and the FM sounds very much
like the PCjr or Tandy 1000, as they all use the same or similar TI sound
chips. By the time the PS/1 Audio card was released, the Sound Blaster was
already available, making it largely obsolete.


## Compatible games

PS/1 Audio support appears primarily in Sierra SCI1-era titles. Many of these
games detect PS/1 Audio automatically.

<div class="compact" markdown>

- [BattleTech: The Crescent Hawks' Revenge](https://www.mobygames.com/game/233/battletech-the-crescent-hawks-revenge/)
- [Castle of Dr. Brain](https://www.mobygames.com/game/1523/castle-of-dr-brain/)
- [Conquests of the Longbow](https://www.mobygames.com/game/1967/conquests-of-the-longbow-the-legend-of-robin-hood/)
- [The Dagger of Amon Ra](https://www.mobygames.com/game/462/the-dagger-of-amon-ra/)
- [Freddy Pharkas: Frontier Pharmacist](https://www.mobygames.com/game/1785/freddy-pharkas-frontier-pharmacist/)
- [Hoyle Classic Card Games](https://www.mobygames.com/game/30264/hoyle-classic-card-games/)
- [King's Quest V](https://www.mobygames.com/game/130/kings-quest-v-absence-makes-the-heart-go-yonder/)
- [King's Quest VI](https://www.mobygames.com/game/131/kings-quest-vi-heir-today-gone-tomorrow/)
- [Leather Goddesses of Phobos! 2](https://www.mobygames.com/game/480/leather-goddesses-of-phobos-2-gas-pump-girls-meet-the-pulsating-/)
- [Leisure Suit Larry 1 (VGA remake)](https://www.mobygames.com/game/413/leisure-suit-larry-1-in-the-land-of-the-lounge-lizards/)
- [Leisure Suit Larry 5](https://www.mobygames.com/game/408/leisure-suit-larry-5-passionate-patti-does-a-little-undercover-w/)
- [Leisure Suit Larry 6](https://www.mobygames.com/game/407/leisure-suit-larry-6-shape-up-or-slip-out/)
- [Lost Secret of the Rainforest](https://www.mobygames.com/game/619/lost-secret-of-the-rainforest/)
- [Mixed Up Fairy Tales](https://www.mobygames.com/game/6880/mixed-up-fairy-tales/)
- [Pepper's Adventures in Time](https://www.mobygames.com/game/6312/peppers-adventures-in-time/)
- [Police Quest 3](https://www.mobygames.com/game/148/police-quest-3-the-kindred/)
- [Police Quest: In Pursuit of the Death Angel (VGA remake)](https://www.mobygames.com/game/2031/police-quest-in-pursuit-of-the-death-angel/)
- [Prince of Persia](https://www.mobygames.com/game/196/prince-of-persia/)
- [Prince of Persia 2](https://www.mobygames.com/game/78/prince-of-persia-2-the-shadow-the-flame/)
- [Quest for Glory I (VGA remake)](https://www.mobygames.com/game/16075/quest-for-glory-i-so-you-want-to-be-a-hero/)
- [Quest for Glory II](https://www.mobygames.com/game/169/quest-for-glory-ii-trial-by-fire/)
- [Shanghai II: Dragon's Eye](https://www.mobygames.com/game/2055/shanghai-ii-dragons-eye/)
- [Silpheed](https://www.mobygames.com/game/167/silpheed/)
- [Space Quest I (VGA remake)](https://www.mobygames.com/game/187/space-quest-i-roger-wilco-in-the-sarien-encounter/)
- [Space Quest IV](https://www.mobygames.com/game/143/space-quest-iv-roger-wilco-and-the-time-rippers/)
- [The Treehouse](https://www.mobygames.com/game/11866/the-treehouse/)

</div>


### Mixer channels

The PS/1 Audio synthesiser outputs to the **PS1** mixer channel, and the DAC
to the **PS1DAC** channel.


## Configuration settings

IBM PS/1 Audio settings are to be configured in the `[speaker]` section.


##### ps1audio

:   Enable IBM PS/1 Audio emulation.

    Possible values: `on`, `off` *default*{ .default }


##### ps1audio_dac_filter

:   Filter for the PS/1 Audio DAC output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output (3rd order high-pass at 160 Hz + 1st order low-pass at 2.1 kHz).
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.


##### ps1audio_filter

:   Filter for the PS/1 Audio synth output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output (3rd order high-pass at 160 Hz + 1st order low-pass at 2.1 kHz).
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.
