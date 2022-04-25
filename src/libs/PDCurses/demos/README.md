PDCurses Demos
==============

This directory contains demonstration programs to show and test the
capabilities of curses libraries. Some of them predate PDCurses,
PCcurses or even pcurses/ncurses. Although some PDCurses-specific code
has been added, all programs remain portable to other implementations
(at a minimum, to ncurses).


Building
--------

The demos are built by the platform-specific makefiles, in the platform
directories. Alternatively, you can build them manually, individually,
and link with any curses library; e.g., "cc -orain rain.c -lcurses".
There are no dependencies besides curses and the standard C library, and
no configuration is needed.


Distribution Status
-------------------

Public domain, except for rain.c and worm.c, which are under the ncurses
license (MIT-like), and UTF-8-demo.txt, which is under Creative Commons
Attribution ([CC BY]).

[CC BY]: https://creativecommons.org/licenses/by/4.0/
