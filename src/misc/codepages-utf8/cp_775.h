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
// - https://unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP775.TXT

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


// Latin-7 (Baltic Rim)
static const CodePageImport cp_775 = {
    { 0x80, 0x0106 }, // LATIN CAPITAL LETTER C WITH ACUTE
    { 0x81, 0x00fc }, // LATIN SMALL LETTER U WITH DIAERESIS
    { 0x82, 0x00e9 }, // LATIN SMALL LETTER E WITH ACUTE
    { 0x83, 0x0101 }, // LATIN SMALL LETTER A WITH MACRON
    { 0x84, 0x00e4 }, // LATIN SMALL LETTER A WITH DIAERESIS
    { 0x85, 0x0123 }, // LATIN SMALL LETTER G WITH CEDILLA
    { 0x86, 0x00e5 }, // LATIN SMALL LETTER A WITH RING ABOVE
    { 0x87, 0x0107 }, // LATIN SMALL LETTER C WITH ACUTE
    { 0x88, 0x0142 }, // LATIN SMALL LETTER L WITH STROKE
    { 0x89, 0x0113 }, // LATIN SMALL LETTER E WITH MACRON
    { 0x8a, 0x0156 }, // LATIN CAPITAL LETTER R WITH CEDILLA
    { 0x8b, 0x0157 }, // LATIN SMALL LETTER R WITH CEDILLA
    { 0x8c, 0x012b }, // LATIN SMALL LETTER I WITH MACRON
    { 0x8d, 0x0179 }, // LATIN CAPITAL LETTER Z WITH ACUTE
    { 0x8e, 0x00c4 }, // LATIN CAPITAL LETTER A WITH DIAERESIS
    { 0x8f, 0x00c5 }, // LATIN CAPITAL LETTER A WITH RING ABOVE
    { 0x90, 0x00c9 }, // LATIN CAPITAL LETTER E WITH ACUTE
    { 0x91, 0x00e6 }, // LATIN SMALL LIGATURE AE
    { 0x92, 0x00c6 }, // LATIN CAPITAL LIGATURE AE
    { 0x93, 0x014d }, // LATIN SMALL LETTER O WITH MACRON
    { 0x94, 0x00f6 }, // LATIN SMALL LETTER O WITH DIAERESIS
    { 0x95, 0x0122 }, // LATIN CAPITAL LETTER G WITH CEDILLA
    { 0x96, 0x00a2 }, // CENT SIGN
    { 0x97, 0x015a }, // LATIN CAPITAL LETTER S WITH ACUTE
    { 0x98, 0x015b }, // LATIN SMALL LETTER S WITH ACUTE
    { 0x99, 0x00d6 }, // LATIN CAPITAL LETTER O WITH DIAERESIS
    { 0x9a, 0x00dc }, // LATIN CAPITAL LETTER U WITH DIAERESIS
    { 0x9b, 0x00f8 }, // LATIN SMALL LETTER O WITH STROKE
    { 0x9c, 0x00a3 }, // POUND SIGN
    { 0x9d, 0x00d8 }, // LATIN CAPITAL LETTER O WITH STROKE
    { 0x9e, 0x00d7 }, // MULTIPLICATION SIGN
    { 0x9f, 0x00a4 }, // CURRENCY SIGN
    { 0xa0, 0x0100 }, // LATIN CAPITAL LETTER A WITH MACRON
    { 0xa1, 0x012a }, // LATIN CAPITAL LETTER I WITH MACRON
    { 0xa2, 0x00f3 }, // LATIN SMALL LETTER O WITH ACUTE
    { 0xa3, 0x017b }, // LATIN CAPITAL LETTER Z WITH DOT ABOVE
    { 0xa4, 0x017c }, // LATIN SMALL LETTER Z WITH DOT ABOVE
    { 0xa5, 0x017a }, // LATIN SMALL LETTER Z WITH ACUTE
    { 0xa6, 0x201d }, // RIGHT DOUBLE QUOTATION MARK
    { 0xa7, 0x00a6 }, // BROKEN BAR
    { 0xa8, 0x00a9 }, // COPYRIGHT SIGN
    { 0xa9, 0x00ae }, // REGISTERED SIGN
    { 0xaa, 0x00ac }, // NOT SIGN
    { 0xab, 0x00bd }, // VULGAR FRACTION ONE HALF
    { 0xac, 0x00bc }, // VULGAR FRACTION ONE QUARTER
    { 0xad, 0x0141 }, // LATIN CAPITAL LETTER L WITH STROKE
    { 0xae, 0x00ab }, // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
    { 0xaf, 0x00bb }, // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
    { 0xb0, 0x2591 }, // LIGHT SHADE
    { 0xb1, 0x2592 }, // MEDIUM SHADE
    { 0xb2, 0x2593 }, // DARK SHADE
    { 0xb3, 0x2502 }, // BOX DRAWINGS LIGHT VERTICAL
    { 0xb4, 0x2524 }, // BOX DRAWINGS LIGHT VERTICAL AND LEFT
    { 0xb5, 0x0104 }, // LATIN CAPITAL LETTER A WITH OGONEK
    { 0xb6, 0x010c }, // LATIN CAPITAL LETTER C WITH CARON
    { 0xb7, 0x0118 }, // LATIN CAPITAL LETTER E WITH OGONEK
    { 0xb8, 0x0116 }, // LATIN CAPITAL LETTER E WITH DOT ABOVE
    { 0xb9, 0x2563 }, // BOX DRAWINGS DOUBLE VERTICAL AND LEFT
    { 0xba, 0x2551 }, // BOX DRAWINGS DOUBLE VERTICAL
    { 0xbb, 0x2557 }, // BOX DRAWINGS DOUBLE DOWN AND LEFT
    { 0xbc, 0x255d }, // BOX DRAWINGS DOUBLE UP AND LEFT
    { 0xbd, 0x012e }, // LATIN CAPITAL LETTER I WITH OGONEK
    { 0xbe, 0x0160 }, // LATIN CAPITAL LETTER S WITH CARON
    { 0xbf, 0x2510 }, // BOX DRAWINGS LIGHT DOWN AND LEFT
    { 0xc0, 0x2514 }, // BOX DRAWINGS LIGHT UP AND RIGHT
    { 0xc1, 0x2534 }, // BOX DRAWINGS LIGHT UP AND HORIZONTAL
    { 0xc2, 0x252c }, // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
    { 0xc3, 0x251c }, // BOX DRAWINGS LIGHT VERTICAL AND RIGHT
    { 0xc4, 0x2500 }, // BOX DRAWINGS LIGHT HORIZONTAL
    { 0xc5, 0x253c }, // BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
    { 0xc6, 0x0172 }, // LATIN CAPITAL LETTER U WITH OGONEK
    { 0xc7, 0x016a }, // LATIN CAPITAL LETTER U WITH MACRON
    { 0xc8, 0x255a }, // BOX DRAWINGS DOUBLE UP AND RIGHT
    { 0xc9, 0x2554 }, // BOX DRAWINGS DOUBLE DOWN AND RIGHT
    { 0xca, 0x2569 }, // BOX DRAWINGS DOUBLE UP AND HORIZONTAL
    { 0xcb, 0x2566 }, // BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
    { 0xcc, 0x2560 }, // BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
    { 0xcd, 0x2550 }, // BOX DRAWINGS DOUBLE HORIZONTAL
    { 0xce, 0x256c }, // BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
    { 0xcf, 0x017d }, // LATIN CAPITAL LETTER Z WITH CARON
    { 0xd0, 0x0105 }, // LATIN SMALL LETTER A WITH OGONEK
    { 0xd1, 0x010d }, // LATIN SMALL LETTER C WITH CARON
    { 0xd2, 0x0119 }, // LATIN SMALL LETTER E WITH OGONEK
    { 0xd3, 0x0117 }, // LATIN SMALL LETTER E WITH DOT ABOVE
    { 0xd4, 0x012f }, // LATIN SMALL LETTER I WITH OGONEK
    { 0xd5, 0x0161 }, // LATIN SMALL LETTER S WITH CARON
    { 0xd6, 0x0173 }, // LATIN SMALL LETTER U WITH OGONEK
    { 0xd7, 0x016b }, // LATIN SMALL LETTER U WITH MACRON
    { 0xd8, 0x017e }, // LATIN SMALL LETTER Z WITH CARON
    { 0xd9, 0x2518 }, // BOX DRAWINGS LIGHT UP AND LEFT
    { 0xda, 0x250c }, // BOX DRAWINGS LIGHT DOWN AND RIGHT
    { 0xdb, 0x2588 }, // FULL BLOCK
    { 0xdc, 0x2584 }, // LOWER HALF BLOCK
    { 0xdd, 0x258c }, // LEFT HALF BLOCK
    { 0xde, 0x2590 }, // RIGHT HALF BLOCK
    { 0xdf, 0x2580 }, // UPPER HALF BLOCK
    { 0xe0, 0x00d3 }, // LATIN CAPITAL LETTER O WITH ACUTE
    { 0xe1, 0x00df }, // LATIN SMALL LETTER SHARP S (GERMAN)
    { 0xe2, 0x014c }, // LATIN CAPITAL LETTER O WITH MACRON
    { 0xe3, 0x0143 }, // LATIN CAPITAL LETTER N WITH ACUTE
    { 0xe4, 0x00f5 }, // LATIN SMALL LETTER O WITH TILDE
    { 0xe5, 0x00d5 }, // LATIN CAPITAL LETTER O WITH TILDE
    { 0xe6, 0x00b5 }, // MICRO SIGN
    { 0xe7, 0x0144 }, // LATIN SMALL LETTER N WITH ACUTE
    { 0xe8, 0x0136 }, // LATIN CAPITAL LETTER K WITH CEDILLA
    { 0xe9, 0x0137 }, // LATIN SMALL LETTER K WITH CEDILLA
    { 0xea, 0x013b }, // LATIN CAPITAL LETTER L WITH CEDILLA
    { 0xeb, 0x013c }, // LATIN SMALL LETTER L WITH CEDILLA
    { 0xec, 0x0146 }, // LATIN SMALL LETTER N WITH CEDILLA
    { 0xed, 0x0112 }, // LATIN CAPITAL LETTER E WITH MACRON
    { 0xee, 0x0145 }, // LATIN CAPITAL LETTER N WITH CEDILLA
    { 0xef, 0x2019 }, // RIGHT SINGLE QUOTATION MARK
    { 0xf0, 0x00ad }, // SOFT HYPHEN
    { 0xf1, 0x00b1 }, // PLUS-MINUS SIGN
    { 0xf2, 0x201c }, // LEFT DOUBLE QUOTATION MARK
    { 0xf3, 0x00be }, // VULGAR FRACTION THREE QUARTERS
    { 0xf4, 0x00b6 }, // PILCROW SIGN
    { 0xf5, 0x00a7 }, // SECTION SIGN
    { 0xf6, 0x00f7 }, // DIVISION SIGN
    { 0xf7, 0x201e }, // DOUBLE LOW-9 QUOTATION MARK
    { 0xf8, 0x00b0 }, // DEGREE SIGN
    { 0xf9, 0x2219 }, // BULLET OPERATOR
    { 0xfa, 0x00b7 }, // MIDDLE DOT
    { 0xfb, 0x00b9 }, // SUPERSCRIPT ONE
    { 0xfc, 0x00b3 }, // SUPERSCRIPT THREE
    { 0xfd, 0x00b2 }, // SUPERSCRIPT TWO
    { 0xfe, 0x25a0 }, // BLACK SQUARE
    { 0xff, 0x00a0 }, // NO-BREAK SPACE
};
