# IBM Music Feature Card

## Overview

The IBM Music Feature Card (IMFC) was released by IBM in 1987 as a
MIDI-capable sound card for the IBM PC and compatible machines. It featured a
Yamaha FB-01 FM synthesiser module and an MPU-401 compatible MIDI interface.

Very few games support the IBM Music Feature Card. It was primarily aimed at
musicians and MIDI enthusiasts rather than gamers. Sierra was essentially the
only game company to support it, along with a handful of Game Arts/Falcom
ports.


## Compatible games

<div class="compact" markdown>

- [Codename: Iceman](https://www.mobygames.com/game/436/code-name-iceman/)
- [The Colonel's Bequest](https://www.mobygames.com/game/461/the-colonels-bequest/)
- [Conquests of Camelot](https://www.mobygames.com/game/1408/conquests-of-camelot-the-search-for-the-grail/)
- [Hoyle's Official Book of Games](https://www.mobygames.com/game/759/hoyle-official-book-of-games-volume-1/)
- [Jones in the Fast Lane](https://www.mobygames.com/game/370/jones-in-the-fast-lane/)
- [King's Quest I (SCI remake)](https://www.mobygames.com/game/439/roberta-williams-kings-quest-i-quest-for-the-crown/)
- [King's Quest IV](https://www.mobygames.com/game/129/kings-quest-iv-the-perils-of-rosella/)
- [King's Quest V](https://www.mobygames.com/game/130/kings-quest-v-absence-makes-the-heart-go-yonder/)
- [Leisure Suit Larry 2](https://www.mobygames.com/game/409/leisure-suit-larry-goes-looking-for-love-in-several-wrong-places/)
- [Leisure Suit Larry 3](https://www.mobygames.com/game/412/leisure-suit-larry-iii-passionate-patti-in-pursuit-of-the-pulsat/)
- [Mixed Up Mother Goose](https://www.mobygames.com/game/22295/mixed-up-mother-goose/)
- [Police Quest II](https://www.mobygames.com/game/147/police-quest-2-the-vengeance/)
- [Quest for Glory I (Hero's Quest)](https://www.mobygames.com/game/168/heros-quest-so-you-want-to-be-a-hero/)
- [Quest for Glory II](https://www.mobygames.com/game/169/quest-for-glory-ii-trial-by-fire/)
- [Silpheed](https://www.mobygames.com/game/167/silpheed/)
- [Sorcerian](https://www.mobygames.com/game/1010/sorcerian/)
- [Space Quest III](https://www.mobygames.com/game/142/space-quest-iii-the-pirates-of-pestulon/)
- [Thexder 2](https://www.mobygames.com/game/64/fire-hawk-thexder-the-second-contact/)

</div>


## Configuration settings

IBM Music Feature Card settings are to be configured in the `[imfc]` section.


##### imfc

:   Enable the IBM Music Feature Card.

    Possible values: `on`, `off` *default*{ .default }


##### imfc_base

:   The IO base address of the IBM Music Feature Card.

    Possible values: `2A20` *default*{ .default }, `2A30`.


##### imfc_filter

:   Filter for the IBM Music Feature Card output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output.
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.


##### imfc_irq

:   The IRQ number of the IBM Music Feature Card.

    Possible values: `2`, `3` *default*{ .default }, `4`, `5`, `6`, `7`.
