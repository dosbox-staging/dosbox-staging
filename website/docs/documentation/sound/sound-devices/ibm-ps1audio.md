# IBM PS/1 Audio Card

Optional audio card released in 1990 for IBM PS/1 computers (model 2011 and
2121). Relatively few games support this option, and the FM sounds very much
like the PCjr or Tandy 1000, as they all use the same or similar TI sound
chips.

 So, by the time the PS/1 Audio card was released, IBM was now playing catch-up to Tandy. Not that it mattered, because the second generation Sound Blaster was now available, making both audio systems obsolete.


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
      [Custom filter settings](../../analog-output-filters/#custom-filter-settings)
      for details.


##### ps1audio_filter

:   Filter for the PS/1 Audio synth output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output.
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../../analog-output-filters/#custom-filter-settings)
      for details.
