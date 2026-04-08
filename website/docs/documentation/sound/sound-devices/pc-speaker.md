# PC speaker

The **PC speaker** was the built-in speaker in every IBM PC and compatible
from day one. It was really only meant for simple beeps and diagnostic codes,
but it ended up as the lowest common denominator for PC game audio --- every
PC had one, so every game could count on it being there.

Technically, it's a single-channel square-wave generator. Not exactly hi-fi.
But with clever programming tricks, developers managed to coax surprisingly
decent sound out of it. Games like [Alley
Cat](https://www.mobygames.com/game/190/alley-cat/) and
[Sopwith](https://www.mobygames.com/game/1380/sopwith/) made the most of its
musical abilities, while [Castle
Wolfenstein](https://www.mobygames.com/game/3115/castle-wolfenstein/) even
managed digitised speech --- impressive given the hardware's severe
limitations.

Sound quality varied quite a bit depending on the actual speaker hardware and
the PC case it was mounted in. Early PCs used a proper dynamic speaker that
had some bass response; later models switched to a tiny piezo buzzer that
sounded thinner and quieter.

PC speaker emulation is enabled by default. The `impulse` model provides the
most faithful reproduction of the actual speaker hardware; it generally
improves the accuracy of square-wave effects in games like [Commander
Keen](https://www.mobygames.com/game/216/commander-keen-1-marooned-on-mars/)
and [Duke Nukem](https://www.mobygames.com/game/559/duke-nukem/) (2D), and can
even produce sounds where the `discrete` model produces none (e.g.,
[Wizball](https://www.mobygames.com/game/526/wizball/)). Fall back to
`discrete` only if a specific game has problems with the `impulse` model.


### Mixer channel

The PC speaker outputs to the **PCSPEAKER** mixer channel.


## Configuration settings

PC speaker settings are to be configured in the `[speaker]` section.


##### pcspeaker

:   PC speaker emulation model.

    Possible values:

    - `impulse` *default*{ .default } -- A very faithful emulation of the PC
      speaker's output. Works with most games, but may result in garbled sound
      or silence in a small number of programs.
    - `discrete` -- Legacy simplified PC speaker emulation; only use this on
      specific titles that give you problems with the `impulse` model.
    - `none`/`off` -- Don't emulate the PC speaker.


##### pcspeaker_filter

:   Filter for the PC speaker output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output. This emulates a small speaker in an acoustic environment using a built-in multi-stage filter.
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.
