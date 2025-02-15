
# Input code

## Existing code layout

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

## Not implemented keyboard scancodes

Multimedia keyboards are not emulated yet - this chapter contains a list of
known scancodes (taken from various sources, many confirmed using a real
keyboard) which can be added.
Note that our BIOS might be unable to handle these scancodes  correctly - this
needs to be tested before the support is added.

| Key              | Scancode set 1 | Scancode set 2 | Remarks |
|------------------|----------------|----------------|---------|
| SysReq           | `0x54`         | `0x84`         |         |
| Menu             | `0xe0 0x5d`    | `0xe0 0x2f`    | (1)     |
| Log Off          | `0xe0 0x16`    | `0xe0 0x3c`    | (1)     |
| ACPI Sleep       | `0xe0 0x5f`    | `0xe0 0x3f`    | (1)     |
| ACPI Wake        | `0xe0 0x63`    | `0xe0 0x5e`    |         |
| ACPI Power       | `0xe0 0x5e`    | `0xe0 0x37`    |         |
| F13              | `0x5b`         | `0x1f`         |         |
| F14              | `0x5c`         | `0x27`         |         |
| F15              | `0x5d`         | `0x2f`         |         |
| F16              | `0x63`         | `0x5e`         |         |
| F17              | `0x64`         | `0x08`         |         |
| F18              | `0x65`         | `0x10`         |         |
| F19              | `0x66`         | `0x18`         |         |
| F20              | `0x67`         | `0x20`         |         |
| F21              | `0x68`         | `0x28`         |         |
| F22              | `0x69`         | `0x30`         |         |
| F23              | `0x6a`         | `0x38`         |         |
| F24              | `0x6b`         | `0x40`         |         |
| Intl 1           | `0x59`         | (unknown)      |         |
| Intl 2           |                |                | (A)     |
| Intl 4           | `0x7d`         | `0x6a`         |         |
| Intl 5           | `0x7e`         | `0x6d`         |         |
| Katakana         | `0x70`         | `0x13`         |         |
| Fugirana         | `0x77`         | `0x62`         |         |
| Kanji            | `0x79`         | `0x64`         |         |
| Hiragana         | `0x7b`         | `0x67`         |         |
| Undo             | `0xe0 0x08`    | `0xe0 0x3d`    | (1)     |
| Redo             | `0xe0 0x07`    | `0xe0 0x36`    | (1)     |
| Help             | `0xe0 0x3b`    | `0xe0 0x05`    | (1)     |
| Volume Mute      | `0xe0 0x20`    | `0xe0 0x23`    | (1)     |
| Volume+          | `0xe0 0x30`    | `0xe0 0x32`    | (1)     |
| Volume-          | `0xe0 0x2e`    | `0xe0 0x21`    | (1)     |
| Media Play/Pause | `0xe0 0x22`    | `0xe0 0x34`    | (1)     |
| Media Stop       | `0xe0 0x24`    | `0xe0 0x3b`    | (1)     |
| Media Previous   | `0xe0 0x10`    | `0xe0 0x15`    | (1)     |
| Media Next       | `0xe0 0x19`    | `0xe0 0x4d`    | (1)     |
| Media Music      | `0xe0 0x3c`    | `0xe0 0x06`    | (1)     |
| Media Pictures   | `0xe0 0x64`    | `0xe0 0x08`    | (1)     |
| Media Select     | `0xe0 0x6d`    | `0xe0 0x50`    | (B)     |
| Media Eject      | `0xe0 0x2c`    | `0xe0 0x1a`    |         |
| Zoom+            | `0xe0 0x0b`    | `0xe0 0x45`    | (1)     |
| Zoom-            | `0xe0 0x11`    | `0xe0 0x1d`    | (1)     |
| Calculator       | `0xe0 0x21`    | `0xe0 0x2b`    | (1)     |
| E-Mail           | `0xe0 0x6c`    | `0xe0 0x48`    | (1)     |
| Messenger        | `0xe0 0x05`    | `0xe0 0x25`    | (1)     |
| WWW Home         | `0xe0 0x32`    | `0xe0 0x3a`    | (1)     |
| WWW Forward      | `0xe0 0x69`    | `0xe0 0x30`    |         |
| WWW Back         | `0xe0 0x6a`    | `0xe0 0x38`    |         |
| WWW Stop         | `0xe0 0x68`    | `0xe0 0x28`    |         |
| WWW Refresh      | `0xe0 0x67`    | `0xe0 0x20`    |         |
| WWW Search       | `0xe0 0x65`    | `0xe0 0x10`    |         |
| WWW Favorites    | `0xe0 0x66`    | `0xe0 0x18`    |         |
| My Computer      | `0xe0 0x6b`    | `0xe0 0x40`    |         |
| My Documents     | `0xe0 0x4c`    | `0xe0 0x73`    | (1)     |
| Cut              | `0xe0 0x17`    | `0xe0 0x43`    |         |
| Copy             | `0xe0 0x18`    | `0xe0 0x44`    |         |
| Paste            | `0xe0 0x1a`    | `0xe0 0x46`    |         |
| New              | `0xe0 0x3e`    | `0xe0 0x0c`    | (1)     |
| Open             | `0xe0 0x3f`    | `0xe0 0x03`    | (1)     |
| Close            | `0xe0 0x40`    | `0xe0 0x0b`    | (1)     |
| Save             | `0xe0 0x57`    | `0xe0 0x78`    | (1)     |
| Print            | `0xe0 0x58`    | `0xe0 0x07`    | (1)     |
| Spell            | `0xe0 0x23`    | `0xe0 0x33`    | (1)     |
| Reply            | `0xe0 0x41`    | `0xe0 0x83`    | (1)     |
| Forward          | `0xe0 0x42`    | `0xe0 0x0a`    | (1)     |
| Send             | `0xe0 0x43`    | `0xe0 0x01`    | (1)     |
| My Favorites     | `0xe0 0x78`    | `0xe0 0x63`    | (1)     |
| My Favorite 1    | `0xe0 0x73`    | `0xe0 0x51`    | (1)     |
| My Favorite 2    | `0xe0 0x74`    | `0xe0 0x53`    | (1)     |
| My Favorite 3    | `0xe0 0x75`    | `0xe0 0x5c`    | (1)     |
| My Favorite 4    | `0xe0 0x76`    | `0xe0 0x5f`    | (1)     |
| My Favorite 5    | `0xe0 0x77`    | `0xe0 0x62`    | (1)     |

| Key              | Scancode set 3 | Remarks |
|------------------|----------------|---------|
| Menu             | `0x8d`         |         |
| F13              | `0x08`         |         |
| Intl 2           | `0x53`         |         |
| Intl 4           | `0x5d`         |         |
| Intl 5           | `0x7b`         |         |
| Hiragana         | `0x85`         |         |
| Kanji            | `0x86`         |         |
| katagana         | `0x87`         |         |

(1) Scancodes confirmed using the Microsoft 'Digital Media Pro Keyboard':
- model 1031 (as printed on the box)
- model KC-0405 (as printed on the bottom side sticker)
- part number X809745-002 (as printed on the bottom side sticker)
- if querried, the keyboard returns ID 0x41

(A) Intl 2 and backslash have same scancodes 1 and 2
(B) Media Video according to some sources
