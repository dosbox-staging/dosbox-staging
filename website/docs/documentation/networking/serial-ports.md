# Serial ports


## Configuration settings

You can set the serial port parameters in the `[serial]` configuration
section.


##### phonebookfile

:   File used to map fake phone numbers to addresses (`phonebook.txt` by
    default).


##### serial1

:   Set type of device connected to the COM1 port.

    Possible values:

    <div class="compact" markdown>

    - `dummy` *default*{ .default } -- Emulate the port without a device
      attached to it.
    - `disabled` -- Disable the port.
    - `mouse` -- Emulate a serial mouse attached to the port.
    - `modem` -- Emulate a modem attached to the port.
    - `nullmodem` -- Emulate a null modem attached to the port.
    - `direct` -- Emulate a direct serial link.

    </div>

    Additional parameters must be on the same line in the form of
    `PARAMETER:VALUE`. The optional `irq` parameter is common for all types.
    Available parameters:

    <div class="compact" markdown>

    - For `mouse`: `model` (optional; overrides the `com_mouse_model`
      setting).
    - For `direct`: `realport` (required), `rxdelay` (optional). E.g.,
      `realport:COM1`, `realport:ttyS0`.
    - For `modem`: `listenport`, `sock`, `bps` (all optional).
    - For `nullmodem`: `server`, `rxdelay`, `txdelay`, `telnet`, `usedtr`,
      `transparent`, `port`, `inhsocket`, `sock` (all optional).

    </div>

    The `sock` parameter specifies the protocol to use at both sides of the
    connection. Valid values are `0` for TCP, and `1` for ENet reliable UDP.
    Example: `serial1 = modem listenport:5000 sock:1`


##### serial2

:   See [serial1](#serial1) (`dummy` by default).


##### serial3

:   See [serial1](#serial1) (`disabled` by default).


##### serial4

:   See [serial1](#serial1) (`disabled` by default).
