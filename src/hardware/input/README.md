# Mouse code

Few comments on mouse emulation code organization:


**mouse.cpp**

Entry point for all the notifications and requests from external subsystems -
see `include/mouse.h` for API definition. Interacts mainly with GFX subsystem
(SDL-based at the moment of writing this code), but contains a configuration
API (`MouseControlAPI` class) for interaction with command-line configuration
tool and yet-to-be-written GUI.

**`../dos/program_mousectl.cpp`**

Implementation of the command line tool, allowing to change settings (like
sensitivity or emulated sampling rate) during the runtime. The tool can also
be used to map a single physical mouse to the given emulated interface, thus
allowing for dual-mice gaming.

**`mouse_common.cpp`**

Various helper methods, like implementation of mouse ballistics acceleration
model).

**`mouse_config.cpp`**

Handles `[mouse]` section of the DOSBox configuration file.

**`mouse_interfaces.cpp`**

Common object-oriented API, abstracting various emulated mouse interfaces for
`mouse.cpp` and `mouse_manymouse.cpp`.

**`mouse_manymouse.cpp`**

Object-oriented wrapper around a 3rd party _ManyMouse_ library, used for
dual-mice gaming. Used as a source of mouse events - but only when a physical
mouse is mapped to the emulated interface, otherwise all mouse events come
from SDL-based GFX subsystem.

**`mouse_queue.cpp`**

A 'queue' - delays and aggregates mouse events from host OS to simulate
the desired mouse scanning rate.

**`mouseif_*.cpp`**

Implementations (non-object oriented) of various mouse interfaces:

- a simulated DOS driver, compatible with _MOUSE.COM_, with some extensions
(like _CtMouse_ wheel API or direct support for _INT33_ Windows driver from
_javispedro_) 
- PS/2 (currently only via BIOS calls, physical mouse port is not emulated
yet)
- a _VMWare_ mouse protocol (actually a PS/2 mouse extension), allowing for
seamless integration with host mouse cursor if a proper driver is running

Future (not yet implemented) planned interfaces include:

- _VirtualBox_ mouse protocol extension; very similar to the _VMWare_ one,
but requires a PCI device
(see <https://wiki.osdev.org/VirtualBox_Guest_Additions> for description)
- InPort / Bus mouse - I know no description, but Bochs contains an emulation
code, under GPL2-or-above license

**`../serialport/serialmouse.cpp`**

Serial mice emulation - idea is similar to `mouseif_*.cpp` files, but
implemented in object-oriented way, as a serial port device; multiple
instances can exist, for each serial port.
