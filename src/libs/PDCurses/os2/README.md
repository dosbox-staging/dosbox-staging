PDCurses for OS/2
=================

This directory contains PDCurses source code files specific to OS/2.


Building
--------

- Choose the appropriate makefile for your compiler:

        Makefile     - GCC/EMX
        Makefile.bcc - Borland C++
        Makefile.wcc - Watcom

- Optionally, you can build in a different directory than the platform
  directory by setting PDCURSES_SRCDIR to point to the directory where
  you unpacked PDCurses, and changing to your target directory:

        set PDCURSES_SRCDIR=c:\pdcurses

- Build it:

        make -f makefilename

  (For Watcom, use "wmake" instead of "make".) You'll get the library
  (pdcurses.lib or .a, depending on your compiler) and a lot of object
  files. Add the target "demos" to build the sample programs.
