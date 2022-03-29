FreeDOS official keyboard layouts v3.1 for KEYB v2.0
====================================================

Keyboard Layout Files to be used with KEYB v2.0 or later.
Copyright (C) 2002-2011 by Henrique Peron (hperon@terra.com.br)

This software is free software, and is distributed under the GNU-GPL
license version 2.0 or later.

You should have received a copy of this  license  with  the  package
(COPYING.TXT). Should you be missing that file, please  contact  the
Free Software Foundation at http://www.gnu.org .

********************************************************************

This package contains 103 KEY files. They must be compiled with  the
KEY file compiler program (KC) which generates a KL file named after
the original KEY file. However, the user might prefer another name.

All files can be recompiled with other name, according to the user's
wish. The spanish keyboard, for instance, is released as "SP" so  to
comply with MS-DOS v6.22(c) standards but it can be recompiled  with
the name of "ES".

KL files can be included into library (SYS) files, e.g. "MYKEYB.SYS"
through a program called KLIB, available in the  KEY  File  Compiler
(KC) package.

More details on recompiling and creating libraries: please refer  to
the documentation files found on the KC package.

Please browse the "LAYOUTS.TXT" file to know  the  default  2-letter
name for your keyboard, as well as the name of the library file that
contains that.

If you're interested in knowing all the possible names for a layout,
browse the file you want to analyze (e.g. "GR.KEY") and, in the last
section ([GENERAL]), you'll find the possible names for a  keyboard.
In the german case, you'll find "DE". You can add your own names and
they don't need to be limited to 2-letter codes.

It is important to notice that, once the keyboard is included into a
library (SYS) file, it can be accessed by all names described in the
[GENERAL] section. This means, for instance, that the  finnish  (SU)
layout can also be loaded, for instance, with the swedish (SV) label
since they're actually the same keyboard. Therefore, compiling a KEY
file in order to generate KL files with different names is necessary
only if you do not plan to build library files.



Details concerning particular layouts
-------------------------------------

There are KEY files named after no major industry standards.

Conflict: There are OSes which apply "SL" to the slovak keyboard and
"SI" to the slovenian keyboard while other OSes apply  "SK"  to  the
former and "SL" to the latter. In this case, the 2-letter's Internet
top-level domain was selected. So, "SK" goes to the slovak  keyboard
while "SI" applies to the slovenian keyboard. (Actually, "SI" is one
of the possible names of the former yugoslavian ("YU") keyboard.)

Different keyboards for a same language: there are several languages
which are assisted by two (or more) different keyboards. In order to
distinguish them, the major industry defined  2-  or  3-digit  codes
known as "identifiers". Therefore (and taking the polish case as  an
example), the "polish for programmers"  keyboard  (which seems to be
much more used and is in fact a regular US keyboard) is simply named
"PL" while the Microsoft version (which seems not to  be  much  used
and is a 102-key keyboard) is referred to as "PL214".  Please  refer
to the documentation found on the KEYB package  on  how  to  specify
identifiers when loading a layout.

Redundancy: there are some countries on the  list  above  for  which
there are no particular layouts; nonetheless,  they  are  listed  to
comply with MS-DOS standards. Countries like Chile  and  Mexico  use
the latin-american keyboard while others like South Africa  and  New
Zealand use the US keyboard.

It was defined that all keyboards should provide the Euro  sign; so,
even non-european keyboards (such as the brazilian   or  the  latin-
american keyboards) also provide that sign, which is generally found
under <AltGr> + <E> (Brazilian ABNT: <Shift> + <AltGr> + <E>) unless
labeled elsewhere on the keyboard. If not labeled and not  available
under <AltGr> + <E>, it will be available under <AltGr> + <5> or yet
under <AltGr> + <4>.



The Euro sign: codepage 858
---------------------------

In order to ensure that the Euro sign  is  available, the  codepages
containing such sign were made primary, unlike what seems to  happen
on OSes from the major industry. The norwegian  case, for  instance,
is defined on MS-DOS as using cp850 as the primary codepage while on
FreeDOS, that is defined as using cp858 as the primary codepage.

Latvia and Lithuania: it seems that there are no codepages for those
keyboards which provide the Euro sign.

Some codepages were redundantly included into more than one CPX file
in order to make it easy for the user to find all CPs related to the
keyboard on the same CPX file.



The Euro sign and "full french": codepage 859
---------------------------------------------

Three european languages were left partially unassisted on codepages
850/858. Full support for them (french, finnish  and  estonian)  was
finally provided under cp859. However, that codepage cannot be  made
default for finnish, estonian, canadian and swiss keyboards  because
there would be labeled characters which would not be available.

Users of those keyboards can still use that codepage; if you're  one
of them, please check the file "ENHANCED.TXT" on  the  documentation
to see what you will miss.

On the other hand, since french and belgian keyboards provide all of
their labeled characters when working with cp859, that codepage  has
been made default for them. Capital-Y with diaeresis  and  ligatures
ae/AE and oe/OE can now be typed. More details on "ENHANCED.TXT".



Keyboards dealing with bidi scripts
-----------------------------------

There are now keyboard layouts to handle bidi scripts, namely arabic
and hebrew. However, it is important to notice that they can only be
used on MINED, which is a text editor that handles bidi scripts  and
Unicode. More details on MINED documentation.



Final words
-----------

The information collected to allow the encoding of all the keyboards
available on this package was sometimes contradictory; therefore, it
is asked to the user which finds any discrepancy to provide feedback
to me (Henrique Peron, hperon@terra.com.br) as soon as possible.

Thank you.
