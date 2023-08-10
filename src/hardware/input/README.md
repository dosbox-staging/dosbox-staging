# Input code

Few comments on input device emulation code organization:

**intel8042.cpp**

Intel 8042 micro-controller emulation, responsible for keyboard and mouse ports.
Handles only the micro-controller part, communicates with `keyboard.cpp` and
`mouseif_ps2_bios.cpp`, similarly as a real-life micro-controller communicates
with keyboard and mouse.

**intel8255.cpp**

Partial emulation of Intel 8255 microcontroller.

**keyboard.cpp**

Emulates a microcontroller typically found in a keyboard.

**keyboard_scancodes.cpp**

Convert internal keyboard codes to scancodes relevant to key press/release,
for a given scancode set. Just a simple (but quite large) converter.

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

**`mouseif_*.cpp`**

Implementations (non-object oriented) of various mouse interfaces:

- a simulated DOS driver, compatible with _MOUSE.COM_, with some extensions
(like _CtMouse_ wheel API or direct support for _INT33_ Windows driver from
_javispedro_) 
- PS/2 (both register-level access abd via BIOS calls)
- a _VMware_ mouse protocol (actually a PS/2 mouse extension), allowing for
seamless integration with host mouse cursor if a proper driver is running
- a _VirtualBox_, similar to  _VMWare_ one, currently disabled due to problems
with Windows 3.1x driver

Future (not yet implemented) planned interfaces include:

- InPort / Bus mouse - I know no description, but Bochs contains an emulation
code, under GPL2-or-above license

**`../serialport/serialmouse.cpp`**

Serial mice emulation - idea is similar to `mouseif_*.cpp` files, but
implemented in object-oriented way, as a serial port device; multiple
instances can exist, for each serial port.
