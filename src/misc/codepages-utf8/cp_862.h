/*
 *  Copyright (C) 2022       The DOSBox Staging Team
 *  Copyright (C) 1991-2022  Unicode, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
 
// Based on information from:
// - https://unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP862.TXT

// License of the data can be found here:
// - https://www.unicode.org/license.txt
// At the moment of importing the information it stated:

// UNICODE, INC. LICENSE AGREEMENT - DATA FILES AND SOFTWARE
//
// See Terms of Use <https://www.unicode.org/copyright.html>
// for definitions of Unicode Inc.’s Data Files and Software.
//
// NOTICE TO USER: Carefully read the following legal agreement.
// BY DOWNLOADING, INSTALLING, COPYING OR OTHERWISE USING UNICODE INC.'S
// DATA FILES ("DATA FILES"), AND/OR SOFTWARE ("SOFTWARE"),
// YOU UNEQUIVOCALLY ACCEPT, AND AGREE TO BE BOUND BY, ALL OF THE
// TERMS AND CONDITIONS OF THIS AGREEMENT.
// IF YOU DO NOT AGREE, DO NOT DOWNLOAD, INSTALL, COPY, DISTRIBUTE OR USE
// THE DATA FILES OR SOFTWARE.
//
// COPYRIGHT AND PERMISSION NOTICE
//
// Copyright © 1991-2022 Unicode, Inc. All rights reserved.
// Distributed under the Terms of Use in https://www.unicode.org/copyright.html.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of the Unicode data files and any associated documentation
// (the "Data Files") or Unicode software and any associated documentation
// (the "Software") to deal in the Data Files or Software
// without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, and/or sell copies of
// the Data Files or Software, and to permit persons to whom the Data Files
// or Software are furnished to do so, provided that either
// (a) this copyright and permission notice appear with all copies
// of the Data Files or Software, or
// (b) this copyright and permission notice appear in associated
// Documentation.
//
// THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF
// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT OF THIRD PARTY RIGHTS.
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS
// NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL
// DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
// DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
// TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THE DATA FILES OR SOFTWARE.
//
// Except as contained in this notice, the name of a copyright holder
// shall not be used in advertising or otherwise to promote the sale,
// use or other dealings in these Data Files or Software without prior
// written authorization of the copyright holder.


// When adding/updating code page, make sure all the UTF-8 codes
// are also added to table in 'cp_ascii.h'!


// Hebrew
static const CodePageImport cp_862 = {
    { 0x80, 0x05d0 }, // HEBREW LETTER ALEF
    { 0x81, 0x05d1 }, // HEBREW LETTER BET
    { 0x82, 0x05d2 }, // HEBREW LETTER GIMEL
    { 0x83, 0x05d3 }, // HEBREW LETTER DALET
    { 0x84, 0x05d4 }, // HEBREW LETTER HE
    { 0x85, 0x05d5 }, // HEBREW LETTER VAV
    { 0x86, 0x05d6 }, // HEBREW LETTER ZAYIN
    { 0x87, 0x05d7 }, // HEBREW LETTER HET
    { 0x88, 0x05d8 }, // HEBREW LETTER TET
    { 0x89, 0x05d9 }, // HEBREW LETTER YOD
    { 0x8a, 0x05da }, // HEBREW LETTER FINAL KAF
    { 0x8b, 0x05db }, // HEBREW LETTER KAF
    { 0x8c, 0x05dc }, // HEBREW LETTER LAMED
    { 0x8d, 0x05dd }, // HEBREW LETTER FINAL MEM
    { 0x8e, 0x05de }, // HEBREW LETTER MEM
    { 0x8f, 0x05df }, // HEBREW LETTER FINAL NUN
    { 0x90, 0x05e0 }, // HEBREW LETTER NUN
    { 0x91, 0x05e1 }, // HEBREW LETTER SAMEKH
    { 0x92, 0x05e2 }, // HEBREW LETTER AYIN
    { 0x93, 0x05e3 }, // HEBREW LETTER FINAL PE
    { 0x94, 0x05e4 }, // HEBREW LETTER PE
    { 0x95, 0x05e5 }, // HEBREW LETTER FINAL TSADI
    { 0x96, 0x05e6 }, // HEBREW LETTER TSADI
    { 0x97, 0x05e7 }, // HEBREW LETTER QOF
    { 0x98, 0x05e8 }, // HEBREW LETTER RESH
    { 0x99, 0x05e9 }, // HEBREW LETTER SHIN
    { 0x9a, 0x05ea }, // HEBREW LETTER TAV
    { 0x9b, 0x00a2 }, // CENT SIGN
    { 0x9c, 0x00a3 }, // POUND SIGN
    { 0x9d, 0x00a5 }, // YEN SIGN
    { 0x9e, 0x20a7 }, // PESETA SIGN
    { 0x9f, 0x0192 }, // LATIN SMALL LETTER F WITH HOOK
    { 0xa0, 0x00e1 }, // LATIN SMALL LETTER A WITH ACUTE
    { 0xa1, 0x00ed }, // LATIN SMALL LETTER I WITH ACUTE
    { 0xa2, 0x00f3 }, // LATIN SMALL LETTER O WITH ACUTE
    { 0xa3, 0x00fa }, // LATIN SMALL LETTER U WITH ACUTE
    { 0xa4, 0x00f1 }, // LATIN SMALL LETTER N WITH TILDE
    { 0xa5, 0x00d1 }, // LATIN CAPITAL LETTER N WITH TILDE
    { 0xa6, 0x00aa }, // FEMININE ORDINAL INDICATOR
    { 0xa7, 0x00ba }, // MASCULINE ORDINAL INDICATOR
    { 0xa8, 0x00bf }, // INVERTED QUESTION MARK
    { 0xa9, 0x2310 }, // REVERSED NOT SIGN
    { 0xaa, 0x00ac }, // NOT SIGN
    { 0xab, 0x00bd }, // VULGAR FRACTION ONE HALF
    { 0xac, 0x00bc }, // VULGAR FRACTION ONE QUARTER
    { 0xad, 0x00a1 }, // INVERTED EXCLAMATION MARK
    { 0xae, 0x00ab }, // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
    { 0xaf, 0x00bb }, // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
    { 0xb0, 0x2591 }, // LIGHT SHADE
    { 0xb1, 0x2592 }, // MEDIUM SHADE
    { 0xb2, 0x2593 }, // DARK SHADE
    { 0xb3, 0x2502 }, // BOX DRAWINGS LIGHT VERTICAL
    { 0xb4, 0x2524 }, // BOX DRAWINGS LIGHT VERTICAL AND LEFT
    { 0xb5, 0x2561 }, // BOX DRAWINGS VERTICAL SINGLE AND LEFT DOUBLE
    { 0xb6, 0x2562 }, // BOX DRAWINGS VERTICAL DOUBLE AND LEFT SINGLE
    { 0xb7, 0x2556 }, // BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE
    { 0xb8, 0x2555 }, // BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE
    { 0xb9, 0x2563 }, // BOX DRAWINGS DOUBLE VERTICAL AND LEFT
    { 0xba, 0x2551 }, // BOX DRAWINGS DOUBLE VERTICAL
    { 0xbb, 0x2557 }, // BOX DRAWINGS DOUBLE DOWN AND LEFT
    { 0xbc, 0x255d }, // BOX DRAWINGS DOUBLE UP AND LEFT
    { 0xbd, 0x255c }, // BOX DRAWINGS UP DOUBLE AND LEFT SINGLE
    { 0xbe, 0x255b }, // BOX DRAWINGS UP SINGLE AND LEFT DOUBLE
    { 0xbf, 0x2510 }, // BOX DRAWINGS LIGHT DOWN AND LEFT
    { 0xc0, 0x2514 }, // BOX DRAWINGS LIGHT UP AND RIGHT
    { 0xc1, 0x2534 }, // BOX DRAWINGS LIGHT UP AND HORIZONTAL
    { 0xc2, 0x252c }, // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
    { 0xc3, 0x251c }, // BOX DRAWINGS LIGHT VERTICAL AND RIGHT
    { 0xc4, 0x2500 }, // BOX DRAWINGS LIGHT HORIZONTAL
    { 0xc5, 0x253c }, // BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
    { 0xc6, 0x255e }, // BOX DRAWINGS VERTICAL SINGLE AND RIGHT DOUBLE
    { 0xc7, 0x255f }, // BOX DRAWINGS VERTICAL DOUBLE AND RIGHT SINGLE
    { 0xc8, 0x255a }, // BOX DRAWINGS DOUBLE UP AND RIGHT
    { 0xc9, 0x2554 }, // BOX DRAWINGS DOUBLE DOWN AND RIGHT
    { 0xca, 0x2569 }, // BOX DRAWINGS DOUBLE UP AND HORIZONTAL
    { 0xcb, 0x2566 }, // BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
    { 0xcc, 0x2560 }, // BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
    { 0xcd, 0x2550 }, // BOX DRAWINGS DOUBLE HORIZONTAL
    { 0xce, 0x256c }, // BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
    { 0xcf, 0x2567 }, // BOX DRAWINGS UP SINGLE AND HORIZONTAL DOUBLE
    { 0xd0, 0x2568 }, // BOX DRAWINGS UP DOUBLE AND HORIZONTAL SINGLE
    { 0xd1, 0x2564 }, // BOX DRAWINGS DOWN SINGLE AND HORIZONTAL DOUBLE
    { 0xd2, 0x2565 }, // BOX DRAWINGS DOWN DOUBLE AND HORIZONTAL SINGLE
    { 0xd3, 0x2559 }, // BOX DRAWINGS UP DOUBLE AND RIGHT SINGLE
    { 0xd4, 0x2558 }, // BOX DRAWINGS UP SINGLE AND RIGHT DOUBLE
    { 0xd5, 0x2552 }, // BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE
    { 0xd6, 0x2553 }, // BOX DRAWINGS DOWN DOUBLE AND RIGHT SINGLE
    { 0xd7, 0x256b }, // BOX DRAWINGS VERTICAL DOUBLE AND HORIZONTAL SINGLE
    { 0xd8, 0x256a }, // BOX DRAWINGS VERTICAL SINGLE AND HORIZONTAL DOUBLE
    { 0xd9, 0x2518 }, // BOX DRAWINGS LIGHT UP AND LEFT
    { 0xda, 0x250c }, // BOX DRAWINGS LIGHT DOWN AND RIGHT
    { 0xdb, 0x2588 }, // FULL BLOCK
    { 0xdc, 0x2584 }, // LOWER HALF BLOCK
    { 0xdd, 0x258c }, // LEFT HALF BLOCK
    { 0xde, 0x2590 }, // RIGHT HALF BLOCK
    { 0xdf, 0x2580 }, // UPPER HALF BLOCK
    { 0xe0, 0x03b1 }, // GREEK SMALL LETTER ALPHA
    { 0xe1, 0x00df }, // LATIN SMALL LETTER SHARP S (GERMAN)
    { 0xe2, 0x0393 }, // GREEK CAPITAL LETTER GAMMA
    { 0xe3, 0x03c0 }, // GREEK SMALL LETTER PI
    { 0xe4, 0x03a3 }, // GREEK CAPITAL LETTER SIGMA
    { 0xe5, 0x03c3 }, // GREEK SMALL LETTER SIGMA
    { 0xe6, 0x00b5 }, // MICRO SIGN
    { 0xe7, 0x03c4 }, // GREEK SMALL LETTER TAU
    { 0xe8, 0x03a6 }, // GREEK CAPITAL LETTER PHI
    { 0xe9, 0x0398 }, // GREEK CAPITAL LETTER THETA
    { 0xea, 0x03a9 }, // GREEK CAPITAL LETTER OMEGA
    { 0xeb, 0x03b4 }, // GREEK SMALL LETTER DELTA
    { 0xec, 0x221e }, // INFINITY
    { 0xed, 0x03c6 }, // GREEK SMALL LETTER PHI
    { 0xee, 0x03b5 }, // GREEK SMALL LETTER EPSILON
    { 0xef, 0x2229 }, // INTERSECTION
    { 0xf0, 0x2261 }, // IDENTICAL TO
    { 0xf1, 0x00b1 }, // PLUS-MINUS SIGN
    { 0xf2, 0x2265 }, // GREATER-THAN OR EQUAL TO
    { 0xf3, 0x2264 }, // LESS-THAN OR EQUAL TO
    { 0xf4, 0x2320 }, // TOP HALF INTEGRAL
    { 0xf5, 0x2321 }, // BOTTOM HALF INTEGRAL
    { 0xf6, 0x00f7 }, // DIVISION SIGN
    { 0xf7, 0x2248 }, // ALMOST EQUAL TO
    { 0xf8, 0x00b0 }, // DEGREE SIGN
    { 0xf9, 0x2219 }, // BULLET OPERATOR
    { 0xfa, 0x00b7 }, // MIDDLE DOT
    { 0xfb, 0x221a }, // SQUARE ROOT
    { 0xfc, 0x207f }, // SUPERSCRIPT LATIN SMALL LETTER N
    { 0xfd, 0x00b2 }, // SUPERSCRIPT TWO
    { 0xfe, 0x25a0 }, // BLACK SQUARE
    { 0xff, 0x00a0 }, // NO-BREAK SPACE
};
