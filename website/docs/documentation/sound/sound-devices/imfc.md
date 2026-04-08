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

TODO(CL) mobygames links

- Codename: Iceman
- Colonel's Bequest, The
- Conquests of Camelot
- Hoyle's Official Book of Games
- Jones in the Fast Lane
- King's Quest I (SCI remake)
- King's Quest IV
- King's Quest V
- Leisure Suit Larry 2
- Leisure Suit Larry 3
- Mixed Up Mother Goose
- Police Quest II
- Quest for Glory I (Hero's Quest)
- Quest for Glory 2
- Silpheed
- Sorcerian
- Space Quest III
- Thexder 2

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
