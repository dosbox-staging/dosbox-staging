# Mixer effects

## Crossfeed

Listening to audio on headphones where certain sounds are only present in
either the left or the right channel can be a rather distracting and
unpleasant experience. This is most noticeable on Dual OPL and CMS
(Game Blaster) game soundtracks where instruments are hard-panned 100% left
or 100% right in the stereo field.

Crossfeed remedies this by mixing a portion of the left channel's signal into
the right channel, and vice versa, creating a more natural listening
experience. The default crossfeed strength is 40%, which is a good general
setting.

The crossfeed presets only apply to the **OPL** and **CMS** mixer channels by
default, as these are the two devices that typically feature hard-panned
sounds. You can apply crossfeed to other channels as well with the
[DOSBox mixer](dosbox-mixer.md) `MIXER` command (e.g., `MIXER GUS x50`).

`off`
: No crossfeed (default).

`on`
: Enables the `normal` preset.

`light`
: Light crossfeed (strength 15).

`normal`
: Normal crossfeed (strength 40).

`strong`
: Strong crossfeed (strength 65).

You can fine-tune per-channel crossfeed levels via mixer commands.


## Reverb

Enable reverb globally to add a sense of space to the sound:

`off`
: No reverb (default).

`on`
: Enables the `medium` preset.

`tiny`
: Simulates the sound of a small integrated speaker in a room; specifically
  designed for small-speaker audio systems (PC Speaker, Tandy, PS/1 Audio, and
  LPT DAC devices).

`small`
: Adds a subtle sense of space; good for games that use a single synth
  channel (typically OPL) for both music and sound effects.

`medium`
: Medium room preset that works well with a wide variety of games.

`large`
: Large hall preset recommended for games that use separate
  channels for music and digital audio.

`huge`
: A stronger variant of the large hall preset; works really well
  in some games with more atmospheric soundtracks.

You can fine-tune per-channel reverb levels via mixer commands.


## Chorus

Enable chorus globally to add a sense of stereo movement to the sound:

`off`
: No chorus (default).

`on`
: Enable chorus (normal preset).

`light`
: A light chorus effect (especially suited for
  synth music that features lots of white noise.)

`normal`
: Normal chorus that works well with a wide variety of games.

`strong`
: An obvious and upfront chorus effect.

You can fine-tune per-channel chorus levels via mixer commands.


## Combining effects

Crossfeed, reverb, and chorus can be combined, and doing so is highly
recommended for achieving the best results. Together with the
[analog output filters](analog-output-filters.md), these effects can
give old DOS game soundtracks a new life.

Suggested audio configurations for many games are available
[on the wiki](https://github.com/dosbox-staging/dosbox-staging/wiki/Audio-configuration-recommendations).


## Configuration settings

Mixer effects settings are to be configured in the `[mixer]` section.


##### chorus

:   Chorus effect that adds a sense of stereo movement to the sound.

    Possible values:

    <div class="compact" markdown>

    - `off` *default*{ .default } -- No chorus.
    - `on` -- Enable chorus (normal preset).
    - `light` -- A light chorus effect (especially suited for synth music that
      features lots of white noise).
    - `normal` -- Normal chorus that works well with a wide variety of games.
    - `strong` -- An obvious and upfront chorus effect.

    </div>

    !!! note

        The presets apply the chorus effect to the synth channels only (except
        for synths with built-in chorus, e.g., the Roland MT-32). Use the
        `MIXER` command to fine-tune the chorus levels per channel.


##### crossfeed

:   Set crossfeed on the OPL and CMS (Gameblaster) mixer channels. Many
    games pan the instruments 100% left and 100% right in the stereo field on
    these audio devices which is unpleasant to listen to in headphones. With
    crossfeed enabled, a portion of the left channel signal is mixed into the
    right channel and vice versa, creating a more natural listening experience.

    Possible values:

    <div class="compact" markdown>

    - `off` *default*{ .default } -- No crossfeed.
    - `on` -- Enable crossfeed (normal preset).
    - `light` -- Light crossfeed (strength 15).
    - `normal` -- Normal crossfeed (strength 40).
    - `strong` -- Strong crossfeed (strength 65).

    </div>

    !!! note

        Use the `MIXER` command to apply crossfeed to other audio channels
        as well and to fine-tune the crossfeed strength per channel.


##### reverb

:   Reverb effect that adds a sense of space to the sound.

    Possible values:

    <div class="compact" markdown>

    - `off` *default*{ .default } -- No reverb.
    - `on` -- Enable reverb (medium preset).
    - `tiny` -- Simulates the sound of a small integrated speaker in a room;
      specifically designed for small-speaker audio systems (PC speaker,
      Tandy, PS/1 Audio, and LPT DAC devices).
    - `small` -- Adds a subtle sense of space; good for games that use a
      single synth channel (typically OPL) for both music and sound effects.
    - `medium` -- Medium room preset that works well with a wide variety of
      games.
    - `large` -- Large hall preset recommended for games that use separate
      channels for music and digital audio.
    - `huge` -- A stronger variant of the large hall preset; works really well
      in some games with more atmospheric soundtracks.

    </div>

    !!! note

        The presets apply a noticeable amount of reverb to the synth mixer
        channels (except for synths with built-in reverb, e.g., the Roland
        MT-32), and a subtle amount to the digital audio channels. Use the
        `MIXER` command to fine-tune the reverb levels per channel.
