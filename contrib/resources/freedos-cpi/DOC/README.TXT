Code Page Information (CPX) files - DOS codepages, v3.0
=======================================================

Copyright (C) 2002-2011 by Henrique Peron (hperon@terra.com.br)

Code Page Information files provide DOS-based codepages to  be  used
with DISPLAY version 0.10b or later.

This software is free software, and is distributed under the GNU-GPL
license version 2.0 or later.

You should have received a copy of this  license  with  the  package
(COPYING.TXT). Should you be missing that file, please  contact  the
Free Software Foundation at http://www.gnu.org .


   1. About the files
   2. How to install the codepages
   3. Known issues
   4. Redesigned accented characters
   5. Codepages with 5-digit numbers (on the 50000, 60000 range)
   6. Codepages with 5-digit numbers (on the 30000 range)
   7. Feedback



1. ABOUT THE FILES
==================

This package contains 18 files.

CPX files contain codepages (also referred to as "cp"s).

The list of codepages they contain is available on CODEPAGE.TXT.

These CPX files do NOT contain PRINTER fonts.



2. HOW TO INSTALL THE CODEPAGES
===============================

In order to use codepages, the following programs should be used:

- Aitor Santamaria Merino's DISPLAY
- Eric Auer's MODE

Both programs follow MS-DOS(c) syntax, therefore:

AUTOEXEC.BAT
------------

DISPLAY CON=(EGA,<cp>,<n>)  (where 1<=n<=6)
MODE CON CP PREP=((<cp>[,<cp>,<cp>...]) C:\FDOS\BIN\<EGAFILE.CPX>)
MODE CON CP SEL=<cp>

Unlike MS-DOS(c) DISPLAY, FreeDOS DISPLAY is ONLY executable through
command line and can be inserted into AUTOEXEC.BAT as in the example
above.

Please read documentation available on the DISPLAY and MODE packages
for more details.



3. KNOWN ISSUES
===============

None.



4. REDESIGNED CHARACTERS
========================

After analyzing the characters  available  on  codepages of DOS-like
operating systems of the major industry, many of them were found  to
be misaligned according to their non-accented  counterparts; on what
concerns their shapes on the fonts  used  in  the 50-line text mode,
most of them were found barely understandable and, in many cases, it
was hard to distinguish between their lower- and uppercase  accented
variants.

With that in mind, I have redrawn and/or realigned all characters of
all fonts of all codepages in order to improve readability. However,
it is important to bear in mind that, on 50-line mode, specially  on
what concerns letters carrying two or even three  different  accents
simultaneously, easy reading might have been inevitably compromised.
That affects, for instance, accented  lithuanian, vietnamese, yoruba
and many of the indigenous languages spoken in all Americas.



5. CODEPAGES WITH 5-DIGIT NUMBERS (ON THE 50000, 60000 RANGE)
=============================================================

There are several character sets ("charsets") which seem not to have
official codepage ("CP") numbers despite the fact that they are used
worldwide. Therefore, 5-digit CP numbers have been provided for them.

These numbers are considered temporary; in case they change  or  the
major industry provides official numbers, they will be adopted here.

Furthermore, if the major industry provides new  official  charsets,
they will be provided here as well.

Should any keyboard be affected by changes on CP  numbers, they will
be listed in the HISTORY.TXT file available on  this  documentation;
further details would be available on the documentation available on
the keyboard packs for KEYB.



6. CODEPAGES WITH 5-DIGIT NUMBERS (ON THE 30000 RANGE)
======================================================

Those codepages were created to provide character sets for languages
which weren't assisted previously either by the major industry or by
third-party solutions. Saami and celtic languages and others fall in
this category.



7. FEEDBACK
===========

Please e-mail the contact below if you have comments, suggestions or
problems regarding...

  CODEPAGES:
  Henrique PERON (hperon@terra.com.br)          (English/Portuguese)

  DISPLAY:
  Aitor SANTAMARIA Merino (aitorsm@inicia.es)   (English/Spanish)

  MODE:
  Eric AUER (eric@coli.uni-sb.de)               (English/German)

If you are not sure where the problem lies, please e-mail all of us.
Thank you.