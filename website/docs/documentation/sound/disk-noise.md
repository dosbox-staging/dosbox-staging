# Disk noise

## Overview

For those who miss the gentle whirring and clicking of old hard drives and
floppies, DOSBox Staging can play emulated disk noises. It's a small touch,
but it adds a surprising amount of atmosphere — there's something oddly
satisfying about hearing the floppy drive chatter while a game loads.

For the full "waiting for the game to load" experience, pair disk noises
with throttled disk speeds (see the
[floppy_disk_speed](../system/general.md#floppy_disk_speed) and
[hard_disk_speed](../system/general.md#hard_disk_speed) settings). The
volume is adjustable via the `DISKNOISE` mixer channel.


## Configuration settings

You can set the disk noise parameters in the `[disknoise]` configuration
section.


##### floppy_disk_noise

:   Enable emulated floppy disk noises. Plays spinning disk and seek noise
    sounds when enabled. It's recommended to set
    [floppy_disk_speed](../system/general.md#floppy_disk_speed) to lower than
    `maximum` for an authentic experience.

    Possible values:

    <div class="compact" markdown>

    - `off` *default*{ .default } -- No floppy disk noises.
    - `seek-only` -- Play floppy disk seek noises only, no spin noises.
    - `on` -- Play both floppy disk seek and spin noises.

    </div>

    !!! note

        You can customise the disk noise volume by changing the volume of the
        `DISKNOISE` mixer channel.


##### hard_disk_noise

:   Enable emulated hard disk noises. Plays spinning disk and seek noise
    sounds when enabled. It's recommended to set
    [hard_disk_speed](../system/general.md#hard_disk_speed) to lower than
    `maximum` for an authentic experience.

    Possible values:

    <div class="compact" markdown>

    - `off` *default*{ .default } -- No hard disk noises.
    - `seek-only` -- Play hard disk seek noises only, no spin noises.
    - `on` -- Play both hard disk seek and spin noises.

    </div>

    !!! note

        You can customise the disk noise volume by changing the volume of the
        `DISKNOISE` mixer channel.
