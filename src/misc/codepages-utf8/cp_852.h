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
// - https://unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP852.TXT

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


// Latin-2 (Central European)
static const CodePageImport cp_852 = {
    { 0x80, 0x00c7 }, // LATIN CAPITAL LETTER C WITH CEDILLA
    { 0x81, 0x00fc }, // LATIN SMALL LETTER U WITH DIAERESIS
    { 0x82, 0x00e9 }, // LATIN SMALL LETTER E WITH ACUTE
    { 0x83, 0x00e2 }, // LATIN SMALL LETTER A WITH CIRCUMFLEX
    { 0x84, 0x00e4 }, // LATIN SMALL LETTER A WITH DIAERESIS
    { 0x85, 0x016f }, // LATIN SMALL LETTER U WITH RING ABOVE
    { 0x86, 0x0107 }, // LATIN SMALL LETTER C WITH ACUTE
    { 0x87, 0x00e7 }, // LATIN SMALL LETTER C WITH CEDILLA
    { 0x88, 0x0142 }, // LATIN SMALL LETTER L WITH STROKE
    { 0x89, 0x00eb }, // LATIN SMALL LETTER E WITH DIAERESIS
    { 0x8a, 0x0150 }, // LATIN CAPITAL LETTER O WITH DOUBLE ACUTE
    { 0x8b, 0x0151 }, // LATIN SMALL LETTER O WITH DOUBLE ACUTE
    { 0x8c, 0x00ee }, // LATIN SMALL LETTER I WITH CIRCUMFLEX
    { 0x8d, 0x0179 }, // LATIN CAPITAL LETTER Z WITH ACUTE
    { 0x8e, 0x00c4 }, // LATIN CAPITAL LETTER A WITH DIAERESIS
    { 0x8f, 0x0106 }, // LATIN CAPITAL LETTER C WITH ACUTE
    { 0x90, 0x00c9 }, // LATIN CAPITAL LETTER E WITH ACUTE
    { 0x91, 0x0139 }, // LATIN CAPITAL LETTER L WITH ACUTE
    { 0x92, 0x013a }, // LATIN SMALL LETTER L WITH ACUTE
    { 0x93, 0x00f4 }, // LATIN SMALL LETTER O WITH CIRCUMFLEX
    { 0x94, 0x00f6 }, // LATIN SMALL LETTER O WITH DIAERESIS
    { 0x95, 0x013d }, // LATIN CAPITAL LETTER L WITH CARON
    { 0x96, 0x013e }, // LATIN SMALL LETTER L WITH CARON
    { 0x97, 0x015a }, // LATIN CAPITAL LETTER S WITH ACUTE
    { 0x98, 0x015b }, // LATIN SMALL LETTER S WITH ACUTE
    { 0x99, 0x00d6 }, // LATIN CAPITAL LETTER O WITH DIAERESIS
    { 0x9a, 0x00dc }, // LATIN CAPITAL LETTER U WITH DIAERESIS
    { 0x9b, 0x0164 }, // LATIN CAPITAL LETTER T WITH CARON
    { 0x9c, 0x0165 }, // LATIN SMALL LETTER T WITH CARON
    { 0x9d, 0x0141 }, // LATIN CAPITAL LETTER L WITH STROKE
    { 0x9e, 0x00d7 }, // MULTIPLICATION SIGN
    { 0x9f, 0x010d }, // LATIN SMALL LETTER C WITH CARON
    { 0xa0, 0x00e1 }, // LATIN SMALL LETTER A WITH ACUTE
    { 0xa1, 0x00ed }, // LATIN SMALL LETTER I WITH ACUTE
    { 0xa2, 0x00f3 }, // LATIN SMALL LETTER O WITH ACUTE
    { 0xa3, 0x00fa }, // LATIN SMALL LETTER U WITH ACUTE
    { 0xa4, 0x0104 }, // LATIN CAPITAL LETTER A WITH OGONEK
    { 0xa5, 0x0105 }, // LATIN SMALL LETTER A WITH OGONEK
    { 0xa6, 0x017d }, // LATIN CAPITAL LETTER Z WITH CARON
    { 0xa7, 0x017e }, // LATIN SMALL LETTER Z WITH CARON
    { 0xa8, 0x0118 }, // LATIN CAPITAL LETTER E WITH OGONEK
    { 0xa9, 0x0119 }, // LATIN SMALL LETTER E WITH OGONEK
    { 0xaa, 0x00ac }, // NOT SIGN
    { 0xab, 0x017a }, // LATIN SMALL LETTER Z WITH ACUTE
    { 0xac, 0x010c }, // LATIN CAPITAL LETTER C WITH CARON
    { 0xad, 0x015f }, // LATIN SMALL LETTER S WITH CEDILLA
    { 0xae, 0x00ab }, // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
    { 0xaf, 0x00bb }, // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
    { 0xb0, 0x2591 }, // LIGHT SHADE
    { 0xb1, 0x2592 }, // MEDIUM SHADE
    { 0xb2, 0x2593 }, // DARK SHADE
    { 0xb3, 0x2502 }, // BOX DRAWINGS LIGHT VERTICAL
    { 0xb4, 0x2524 }, // BOX DRAWINGS LIGHT VERTICAL AND LEFT
    { 0xb5, 0x00c1 }, // LATIN CAPITAL LETTER A WITH ACUTE
    { 0xb6, 0x00c2 }, // LATIN CAPITAL LETTER A WITH CIRCUMFLEX
    { 0xb7, 0x011a }, // LATIN CAPITAL LETTER E WITH CARON
    { 0xb8, 0x015e }, // LATIN CAPITAL LETTER S WITH CEDILLA
    { 0xb9, 0x2563 }, // BOX DRAWINGS DOUBLE VERTICAL AND LEFT
    { 0xba, 0x2551 }, // BOX DRAWINGS DOUBLE VERTICAL
    { 0xbb, 0x2557 }, // BOX DRAWINGS DOUBLE DOWN AND LEFT
    { 0xbc, 0x255d }, // BOX DRAWINGS DOUBLE UP AND LEFT
    { 0xbd, 0x017b }, // LATIN CAPITAL LETTER Z WITH DOT ABOVE
    { 0xbe, 0x017c }, // LATIN SMALL LETTER Z WITH DOT ABOVE
    { 0xbf, 0x2510 }, // BOX DRAWINGS LIGHT DOWN AND LEFT
    { 0xc0, 0x2514 }, // BOX DRAWINGS LIGHT UP AND RIGHT
    { 0xc1, 0x2534 }, // BOX DRAWINGS LIGHT UP AND HORIZONTAL
    { 0xc2, 0x252c }, // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
    { 0xc3, 0x251c }, // BOX DRAWINGS LIGHT VERTICAL AND RIGHT
    { 0xc4, 0x2500 }, // BOX DRAWINGS LIGHT HORIZONTAL
    { 0xc5, 0x253c }, // BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
    { 0xc6, 0x0102 }, // LATIN CAPITAL LETTER A WITH BREVE
    { 0xc7, 0x0103 }, // LATIN SMALL LETTER A WITH BREVE
    { 0xc8, 0x255a }, // BOX DRAWINGS DOUBLE UP AND RIGHT
    { 0xc9, 0x2554 }, // BOX DRAWINGS DOUBLE DOWN AND RIGHT
    { 0xca, 0x2569 }, // BOX DRAWINGS DOUBLE UP AND HORIZONTAL
    { 0xcb, 0x2566 }, // BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
    { 0xcc, 0x2560 }, // BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
    { 0xcd, 0x2550 }, // BOX DRAWINGS DOUBLE HORIZONTAL
    { 0xce, 0x256c }, // BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
    { 0xcf, 0x00a4 }, // CURRENCY SIGN
    { 0xd0, 0x0111 }, // LATIN SMALL LETTER D WITH STROKE
    { 0xd1, 0x0110 }, // LATIN CAPITAL LETTER D WITH STROKE
    { 0xd2, 0x010e }, // LATIN CAPITAL LETTER D WITH CARON
    { 0xd3, 0x00cb }, // LATIN CAPITAL LETTER E WITH DIAERESIS
    { 0xd4, 0x010f }, // LATIN SMALL LETTER D WITH CARON
    { 0xd5, 0x0147 }, // LATIN CAPITAL LETTER N WITH CARON
    { 0xd6, 0x00cd }, // LATIN CAPITAL LETTER I WITH ACUTE
    { 0xd7, 0x00ce }, // LATIN CAPITAL LETTER I WITH CIRCUMFLEX
    { 0xd8, 0x011b }, // LATIN SMALL LETTER E WITH CARON
    { 0xd9, 0x2518 }, // BOX DRAWINGS LIGHT UP AND LEFT
    { 0xda, 0x250c }, // BOX DRAWINGS LIGHT DOWN AND RIGHT
    { 0xdb, 0x2588 }, // FULL BLOCK
    { 0xdc, 0x2584 }, // LOWER HALF BLOCK
    { 0xdd, 0x0162 }, // LATIN CAPITAL LETTER T WITH CEDILLA
    { 0xde, 0x016e }, // LATIN CAPITAL LETTER U WITH RING ABOVE
    { 0xdf, 0x2580 }, // UPPER HALF BLOCK
    { 0xe0, 0x00d3 }, // LATIN CAPITAL LETTER O WITH ACUTE
    { 0xe1, 0x00df }, // LATIN SMALL LETTER SHARP S
    { 0xe2, 0x00d4 }, // LATIN CAPITAL LETTER O WITH CIRCUMFLEX
    { 0xe3, 0x0143 }, // LATIN CAPITAL LETTER N WITH ACUTE
    { 0xe4, 0x0144 }, // LATIN SMALL LETTER N WITH ACUTE
    { 0xe5, 0x0148 }, // LATIN SMALL LETTER N WITH CARON
    { 0xe6, 0x0160 }, // LATIN CAPITAL LETTER S WITH CARON
    { 0xe7, 0x0161 }, // LATIN SMALL LETTER S WITH CARON
    { 0xe8, 0x0154 }, // LATIN CAPITAL LETTER R WITH ACUTE
    { 0xe9, 0x00da }, // LATIN CAPITAL LETTER U WITH ACUTE
    { 0xea, 0x0155 }, // LATIN SMALL LETTER R WITH ACUTE
    { 0xeb, 0x0170 }, // LATIN CAPITAL LETTER U WITH DOUBLE ACUTE
    { 0xec, 0x00fd }, // LATIN SMALL LETTER Y WITH ACUTE
    { 0xed, 0x00dd }, // LATIN CAPITAL LETTER Y WITH ACUTE
    { 0xee, 0x0163 }, // LATIN SMALL LETTER T WITH CEDILLA
    { 0xef, 0x00b4 }, // ACUTE ACCENT
    { 0xf0, 0x00ad }, // SOFT HYPHEN
    { 0xf1, 0x02dd }, // DOUBLE ACUTE ACCENT
    { 0xf2, 0x02db }, // OGONEK
    { 0xf3, 0x02c7 }, // CARON
    { 0xf4, 0x02d8 }, // BREVE
    { 0xf5, 0x00a7 }, // SECTION SIGN
    { 0xf6, 0x00f7 }, // DIVISION SIGN
    { 0xf7, 0x00b8 }, // CEDILLA
    { 0xf8, 0x00b0 }, // DEGREE SIGN
    { 0xf9, 0x00a8 }, // DIAERESIS
    { 0xfa, 0x02d9 }, // DOT ABOVE
    { 0xfb, 0x0171 }, // LATIN SMALL LETTER U WITH DOUBLE ACUTE
    { 0xfc, 0x0158 }, // LATIN CAPITAL LETTER R WITH CARON
    { 0xfd, 0x0159 }, // LATIN SMALL LETTER R WITH CARON
    { 0xfe, 0x25a0 }, // BLACK SQUARE
    { 0xff, 0x00a0 }, // NO-BREAK SPACE
};
