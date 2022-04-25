PDCurses for SDL 1.x
====================

This is a port of PDCurses for version 1.x of SDL.


Building
--------

- On *nix (including Linux), run "make" in the sdl1 directory. There is
  no configure script (yet?) for this port. This assumes a working
  sdl-config, and GNU make. It builds the library pdcurses.a (dynamic
  lib not implemented).

- The makefile accepts the optional parameter "DEBUG=Y", and recognizes
  the optional PDCURSES_SRCDIR environment variable, as with the console
  ports. "WIDE=Y" builds a version that not only uses 16-bit Unicode
  characters, but depends on the SDL_ttf library, instead of using
  simple bitmap fonts. "UTF8=Y" makes PDCurses ignore the system locale,
  and treat all narrow-character strings as UTF-8; this option has no
  effect unless WIDE=Y is also set. Add the target "demos" to build the
  sample programs.


Usage
-----

There are no special requirements to use PDCurses for SDL -- all
PDCurses-compatible code should work fine. Nothing extra is needed
beyond the base SDL library. However, there are some optional special
features, described here.

The SDL ports operate in one of two ways, depending on whether or not
they were built with WIDE=Y:


### 8-bit mode

The font is a simple BMP, 32 characters wide by 8 characters tall,
preferably with a palette. (BMPs without palettes still work, but in
that case, no attributes will be available, nor will the cursor work.)
The first entry in the palette (usually black) is treated as the
background color; the last entry (usually white) is treated as the
foreground. These are changed or made transparent as appropriate; any
other colors in the palette are passed through unchanged. So -- although
a one-bit depth is sufficient for a normal font -- you could redraw some
characters as multi-colored tiles.

The font must be monospaced. The size of each character is derived by
dividing the width of the BMP by 32 and the height by 8. There is no
constraint on the dimensions.

As provided in the default font and expected by acs_map[], the font is
in Code Page 437 form. But you can of course use any layout if you're
not relying on correct values for the ACS_* macros.

The font can be set via the environment variable PDC_FONT. If it's not
set, PDCurses looks for a file named "pdcfont.bmp" in the current
directory at the time of initscr(). If neither is found, it uses the
built-in default font encoded in font437.h.


### 16-bit mode

Instead of a BMP, PDC_FONT points to a TrueType font. Only true
monospaced fonts work well. The font can be set at compile time via
PDC_FONT_PATH, and/or at runtime via pdc_ttffont. The environment
variable PDC_FONT_SIZE is also available to control the font size (also
as a compile-time define, and at runtime as pdc_font_size.) The
character mapping for chtypes is 16-bit Unicode (the Basic Multilingual
Plane).

The default font is: /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf


Backgrounds
-----------

PDCurses for SDL supports an optional background image BMP. This is used
whenever start_color() has not been called (see the ptest demo for an
example), or when use_default_colors() has been called after
start_color(), and the background color of a pair has been set to -1
(see ozdemo, worm, and rain for examples). The usage parallels that of
ncurses in an appropriate terminal (e.g., Gnome Terminal). The image is
tiled to cover the PDCurses window, and can be any size or depth.

As with the font, you can point to a location for the background via the
environment variable PDC_BACKGROUND; "pdcback.bmp" is the fallback.
(There is no default background.)


Icons
-----

The icon (used with SDL_WM_SetIcon() -- not used for the executable
file) can be set via the environment variable PDC_ICON, and falls back
to "pdcicon.bmp", and then to the built-in icon from iconbmp.h. The
built-in icon is the PDCurses logo, as seen in ../common/icon32.xpm.

If pdc_screen is preinitialized (see below), PDCurses does not attempt
to set the icon.


Screen size
-----------

The default screen size is 80x25 characters (whatever size they may be),
but you can override this via the environment variables PDC_COLS and/or
PDC_LINES. If pdc_screen is preinitialized (see below), these are
ignored.


Integration with SDL
--------------------

If you want to go further, you can mix PDCurses and SDL functions. (Of
course this is extremely non-portable!) To aid you, there are several
external variables and functions specific to the SDL ports; you could
include pdcsdl.h, or just add the declarations you need in your code:

    PDCEX SDL_Surface *pdc_screen, *pdc_font, *pdc_icon, *pdc_back;
    PDCEX int pdc_sheight, pdc_swidth, pdc_yoffset, pdc_xoffset;

    PDCEX void PDC_update_rects(void);
    PDCEX void PDC_retile(void);

pdc_screen is the main surface, created by SDL_SetVideoMode(), unless
it's preset before initscr(). (See sdltest.c for examples.) You can
perform normal SDL operations on this surface, but PDCurses won't
respect them when it updates. (For that, see PDC_retile().) As an
alternative, you can preinitialize this surface before calling
initscr(). In that case, you can use pdc_sheight, pdc_swidth,
pdc_yoffset and/or pdc_xoffset (q.v.) to confine PDCurses to only a
specific area of the surface, reserving the rest for other SDL
operations. If you preinitialize pdc_screen, you'll have to close it
yourself; PDCurses will ignore resize events, and won't try to set the
icon. Also note that if you preinitialize pdc_screen, it need not be the
display surface.

pdc_font (in 8-bit mode), pdc_icon, and pdc_back are the SDL_surfaces
for the font, icon, and background, respectively. You can set any or all
of them before initscr(), and thus override any of the other ways to set
them. But note that pdc_icon will be ignored if pdc_screen is preset.

pdc_sheight and pdc_swidth are the dimensions of the area of pdc_screen
to be used by PDCurses. You can preset them before initscr(); if either
is not set, it defaults to the full screen size minus the x or y offset,
as appropriate.

pdc_xoffset and pdc_yoffset are the x and y offset for the area of
pdc_screen to be used by PDCurses. See the sdltest demo for an example.

PDC_retile() makes a copy of pdc_screen, then tiles it with the
background image, if any. The resulting surface is used as the
background for transparent character cells. PDC_retile() is called from
initscr() and resize_term(). However, you can also use it at other
times, to take advantage of the way it copies pdc_screen: Draw some SDL
stuff; call PDC_retile(); do some curses stuff -- it will use whatever
was on pdc_screen as the background. Then you can erase the curses
screen, do some more SDL stuff, and call PDC_retile() again to make a
new background. (If you don't erase the curses screen, it will be
incorporated into the background when you call PDC_retile().) But this
only works if no background image is set.


Interaction with stdio
----------------------

As with X11, it's a bad idea to mix curses and stdio calls. (In fact,
that's true for PDCurses on any platform; but especially these two,
which don't run under terminals.) Depending on how SDL is built, stdout
and stderr may be redirected to files.


Acknowledgements
----------------

Original SDL port was provided by William McBrine  
TTF support based on contributions by Laura Michaels
