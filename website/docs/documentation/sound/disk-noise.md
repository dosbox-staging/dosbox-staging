# Disk noise


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
