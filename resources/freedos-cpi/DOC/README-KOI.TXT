Code Page Information (CPI) files - KOI codepages, v1.2
=======================================================

  Henrique Peron (hperon@terra.com.br)

Code Page Information files to be used with  DISPLAY  version  0.10b
or later.

This software is free software, and is distributed under the GNU-GPL
license version 2.0 or later.

You should have received a copy of this  license  with  the  package
(COPYING.TXT). Should you be missing that file, please  contact  the
Free Software Foundation at http://www.gnu.org .


--------
   1.- About the files
   2.- How to install the codepages
   3.- Known issues
   4.- Redesigned accented characters
   5.- Codepages with 5-digit numbers
   6.- Feedback



1.- ABOUT THE CPI FILES
=======================

The package contains 2 files, both of them compressed  with  the
UPX program. The new extension is "CPX".

CPI files contain codepages.
The list of codepages they contain is available on CODEPAGE.TXT.

These CPI files do NOT contain PRINTER fonts.



2.- HOW TO INSTALL THE CODEPAGES
================================

In order to use codepages, the following programs should be used:

- Aitor Santamaria Merino's DISPLAY
- Eric Auer's MODE

Both programs follow MS-DOS(c) syntax, therefore:

AUTOEXEC.BAT
------------

DISPLAY CON=(EGA,,1)
MODE CON CP PREP=((878) C:\<directory>\EGAKOI.CPX)
MODE CON CP SEL=878

Unlike MS-DOS(c) DISPLAY, FreeDOS DISPLAY is ONLY executable through
command line and can be inserted into AUTOEXEC.BAT as in the example
above.

Please read documentation available on the DISPLAY and MODE packages
for more details.



3.- KNOWN ISSUES
================

None.



4.- REDESIGNED ACCENTED CHARACTERS
==================================

All characters available on the codepages have been redesigned in an
attempt to improve readability; most of the accented characters were
misaligned according to their non-accented counterparts as it can be
noticed on the original CPI files of the major industry. Furthermore,
when the 50-line mode was selected, it was not easy to tell the caps
from the small versions of many accented characters.



5.- CODEPAGES WITH 5-DIGIT NUMBERS
==================================

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



6.- FEEDBACK
============

Please e-mail the contact below if you have comments, suggestions or
problems regarding...

  CODEPAGES:
  Henrique PERON (hperon@terra.com.br)          (English/Portuguese)

  DISPLAY:
  Aitor SANTAMARIA Merino (aitorsm@inicia.es)   (English/Spanish)

  MODE:
  Eric AUER (eric@coli.uni-sb.de)               (English/German)

If you are not sure where the problem lies, please e-mail all of us.
