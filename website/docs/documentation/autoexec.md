# Autoexec

The `[autoexec]` section is the last section in the configuration file. Each
line in this section is executed at startup as a DOS command.

Unlike other configuration sections, the autoexec section does not contain
individual settings. Instead, it's a freeform block of DOS commands that are
run sequentially when DOSBox starts up, similar to the `AUTOEXEC.BAT` file
in DOS.

!!! important

    The `[autoexec]` section must be the last section in the config file.

See the [autoexec_section](system/general.md#autoexec_section) setting for
how autoexec sections from multiple config files are handled.
