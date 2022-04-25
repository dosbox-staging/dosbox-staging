PDCurses for X11
================

This is a port of PDCurses for X11, aka XCurses.  It is designed to
allow existing curses programs to be re-compiled with PDCurses,
resulting in native X11 programs.


Building
--------

- Run "./configure". To build the narrow-character version of the
  library, specify "--disable-widec" as a parameter.

  If your system is lacking in UTF-8 support, you can force the use of
  UTF-8 instead of the system locale via "--enable-force-utf8".

  If configure can't find your X include files or X libraries, you can
  specify the paths with the arguments "--x-includes=inc_path" and/or
  "--x-libraries=lib_path".

  By default, the library and demo programs are built with the optimizer
  switch -O2. You can turn this off, and turn on debugging (-g), by
  adding "--enable-debug" to the configure command.

- Run "make". This should build libXCurses. Add the target "demos" to
  build the sample programs.

- Optionally, run "make install". To avoid conflicts with any existing
  curses installations, copies of curses.h and panel.h are installed in
  (by default) /usr/local/include/xcurses.


Usage
-----

When compiling your application, you need to include the \<curses.h\>
that comes with PDCurses. You also need to link your code with
libXCurses. You may need to link with the following libraries:

    Xaw Xmu Xt X11 SM ICE Xext Xpm

You can run "xcurses-config --libs" to show the link parameters for your
system. If using dynamic linking, on some systems, "-lXCurses" suffices.

By calling Xinitscr() rather than initscr(), you can pass your program
name and resource overrides to PDCurses. The program name is used as the
title of the X window, and for defining X resources specific to your
program. You can also set the width and height via PDC_COLS and
PDC_LINES (command-line options and resources will take precedence), and
as always, you can set the title via PDC_set_title().


Interaction with stdio
----------------------

Be aware that curses programs that expect to have a normal tty
underneath them will be very disappointed! Output directed to stdout
will go to the xterm that invoked the PDCurses application, or to the
console if not invoked directly from an xterm. Similarly, stdin will
expect its input from the same place as stdout.


X Resources
-----------

PDCurses for X11 recognizes the following resources:

### lines, cols

Specify the number of lines and columns the "screen" will have. Directly
equate to LINES and COLS. There is no theoretical maximum. The minimum
values must each be 2. Defaults: 24, 80

### normalFont, italicFont, boldFont

Names of fonts to use. These should be fixed-width. italicFont and
boldFont are used for characters with the A_ITALIC and A_BOLD
atttributes, respectively (if PDC_set_bold(), in the case of boldFont),
and must have the same cell size as normalFont.

Defaults -- wide:
- -misc-fixed-medium-r-normal--20-200-75-75-c-100-iso10646-1
- -misc-fixed-medium-o-normal--20-200-75-75-c-100-iso10646-1
- -misc-fixed-bold-r-normal--20-200-75-75-c-100-iso10646-1

Narrow:
- -misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1
- -misc-fixed-medium-o-normal--13-120-75-75-c-70-iso8859-1
- -misc-fixed-bold-r-normal--13-120-75-75-c-70-iso8859-1

### pointer

The name of a valid pointer cursor. Default: xterm

### pointerForeColor, pointerBackColor

Foreground and background colors for the pointer. Defaults: black, white

### textCursor

The alignment of the text cursor; horizontal or vertical. Default:
horizontal

### colorBlack through colorWhite

The initial values for the first eight colors, as represented by the
curses macros COLOR_BLACK through COLOR_WHITE. Note that these can all
be changed via init_color().

The defaults are Black, red3, green3, yellow3, blue3, magenta3, cyan3
and Grey.

### colorBoldBlack through colorBoldWhite

The initial values for the next eight colors (8 through 15). When COLORS
was limited to 8, these colors could only be accessed by combining the
appropriate COLOR_# with the A_BOLD attribute, in the case of foreground
colors, or A_BLINK for background colors. They can now be used directly
by their numbers (or COLOR_# + 8). (Note that some terminals still use
COLORS = 8.) See also PDC_set_bold() and PDC_set_blink().

The defaults are grey40, red1, green1, yellow1, blue1, magenta1, cyan1
and White.

### bitmap

The name of a valid bitmap file of depth 1 (black and white) used for
the application's icon. The file is an X bitmap. Default: none

### pixmap

The name of a valid pixmap file of any depth supported by the window
manager (color) for the application's icon, The file is an X11 pixmap.
This resource overrides the "bitmap" resource. Default: a 32x32 or 64x64
pixmap depending on the window manager

### clickPeriod

The period (in milliseconds) between a button press and a button release
that determines if a click of a button has occurred. Default: 100

### doubleClickPeriod

The period (in milliseconds) between two button press events that
determines if a double click of a button has occurred. Default: 200


Using Resources
---------------

All applications have a top-level class name of "XCurses". If Xinitscr()
is used, it sets an application's top-level widget name. (Otherwise the
name defaults to "PDCurses".)

Examples for app-defaults or .Xdefaults:

    !
    ! resources for XCurses class of programs
    !
    XCurses*lines:  30
    XCurses*cols:   80
    XCurses*normalFont:     9x13
    XCurses*bitmap: /tmp/xcurses.xbm
    XCurses*pointer: top_left_arrow
    !
    ! resources for testcurs - XCurses
    !
    testcurs.colorRed:      orange
    testcurs.colorBlack:    midnightblue
    testcurs.lines: 25
    *testcurs.Translations: #override \n \
      <Key>F12:  string(0x1b) string("[11~") \n
    !
    ! resources for THE - XCurses
    !
    ! resources with the * wildcard can be overridden by a parameter passed
    ! to initscr()
    !
    the*normalFont: 9x15
    the*lines:      40
    the*cols:       86
    the*pointer:    xterm
    the*pointerForeColor: white
    the*pointerBackColor: black
    !
    ! resources with the . format can not be overridden by a parameter passed
    ! to Xinitscr()
    !
    the.bitmap:     /home/mark/the/the64.xbm
    the.pixmap:     /home/mark/the/the64.xpm

Resources may also be passed as parameters to the Xinitscr() function.
Parameters are strings in the form of switches; e.g., to set the color
"red" to "indianred", and the number of lines to 30, the string passed
to Xinitscr would be: "-colorRed indianred -lines 30"


Deprecated
----------

XCursesProgramName is no longer used. To set the program name, you must
use Xinitscr(), or PDC_set_title() to set just the window title.

The XCursesExit() function is now called automatically via atexit().
(Multiple calls to it are OK, so you don't need to remove it if you've
already added it for previous versions of PDCurses.)

XCURSES is no longer defined automatically, but need not be defined,
unless you want the X11-specific prototypes. (Normal curses programs
won't need it.)


Distribution Status
-------------------

As of April 13, 2006, the files in this directory are released to the
public domain, except for ScrollBox*, which are under essentially the
MIT X License; config.guess and config.sub, which are under the GPL; and
configure, which is under a free license described within it.


Acknowledgements
----------------

X11 port was provided by Mark Hessling <mark@rexx.org>  
Single-process modifications by William McBrine
