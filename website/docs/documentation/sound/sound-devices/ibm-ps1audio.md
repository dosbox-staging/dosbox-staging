# IBM PS/1 Audio Card

## Overview

Optional audio card released in 1990 for IBM PS/1 computers (models 2011 and
2121). Relatively few games support this option, and the FM sounds very much
like the PCjr or Tandy 1000, as they all use the same or similar TI sound
chips. By the time the PS/1 Audio card was released, the Sound Blaster was
already available, making it largely obsolete.


## Compatible games

PS/1 Audio support appears primarily in Sierra SCI1-era titles. Many of these
games detect PS/1 Audio automatically.

<div class="compact" markdown>

TODO(CL) mobygames links

- BattleTech: The Crescent Hawks' Revenge
- Castle of Dr. Brain
- Conquests of the Longbow
- Dagger of Amon Ra, The
- Freddy Pharkas: Frontier Pharmacist
- Hoyle Classic Card Games
- King's Quest V
- King's Quest VI
- Leather Goddesses of Phobos! 2
- Leisure Suit Larry 1 (VGA remake)
- Leisure Suit Larry 5
- Leisure Suit Larry 6
- Lost Secret of the Rainforest
- Mixed Up Fairy Tales
- Pepper's Adventures in Time
- Police Quest 3
- Police Quest: In Pursuit of the Death Angel (VGA remake)
- Prince of Persia 1
- Prince of Persia 2
- Quest for Glory I (VGA remake)
- Quest for Glory 2
- Shanghai II: Dragon's Eye
- Silpheed
- Space Quest I (VGA remake)
- Space Quest IV
- Treehouse, The

</div>


## Configuration settings

IBM PS/1 Audio settings are to be configured in the `[speaker]` section.


##### ps1audio

:   Enable IBM PS/1 Audio emulation.

    Possible values: `on`, `off` *default*{ .default }


##### ps1audio_dac_filter

:   Filter for the PS/1 Audio DAC output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output.
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.


##### ps1audio_filter

:   Filter for the PS/1 Audio synth output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output.
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.
