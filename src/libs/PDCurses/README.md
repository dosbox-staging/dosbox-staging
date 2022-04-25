PDCurses
========

Stable: [v3.9]  
Beta: [See git repository][git]

PDCurses is a public domain curses library for [DOS], [OS/2], [Windows]
console, [X11] and [SDL], implementing most of the functions available
in [X/Open][xopen] and System V R4 curses. It supports many compilers
for these platforms. The X11 port lets you recompile existing text-mode
curses programs to produce native X11 applications.

PDCurses is distributed mainly as source code, but some pre-compiled
libraries may be available.

The latest version can be found at:

   <https://pdcurses.org/>

For changes, see the [History] file. The main documentation is now in
the [docs] directory.


Mailing lists
-------------

There's a low-traffic mailing list for announcements and discussion. To
subscribe, email the [list server], with the first line of the body of
the message containing:

`subscribe pdcurses-l`

or you can read the mailing list [archive].


Other links
-----------

* [Docs][docs]
* [GitHub Page][git]
* [SourceForge Page]
* [X/Open curses][xopen]


Legal Stuff
-----------

The core package and most ports are in the public domain, but a few
files in the [demos] and [X11][xstatus] directories are subject to
copyright under licenses described there.

This software is provided AS IS with NO WARRANTY whatsoever.


Maintainer
----------

[William McBrine]


[v3.9]: https://github.com/wmcbrine/PDCurses/releases/tag/3.9
[git]: https://github.com/wmcbrine/PDCurses

[History]: docs/HISTORY.md
[docs]: docs/README.md

[list server]: mailto:majordomo@lightlink.com
[archive]: https://www.mail-archive.com/pdcurses-l@lightlink.com/

[SourceForge Page]: https://sourceforge.net/projects/pdcurses
[xopen]: https://pubs.opengroup.org/onlinepubs/007908799/cursesix.html

[DOS]: dos/README.md
[OS/2]: os2/README.md
[SDL]: sdl2/README.md
[Windows]: wincon/README.md
[X11]: x11/README.md

[demos]: demos/README.md#distribution-status
[xstatus]: x11/README.md#distribution-status

[William McBrine]: https://wmcbrine.com/
