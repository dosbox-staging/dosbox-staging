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
// - https://unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP855.TXT

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


// Cyrillic South Slavic
static const CodePageImport cp_855 = {
    { 0x80, 0x0452 }, // CYRILLIC SMALL LETTER DJE
    { 0x81, 0x0402 }, // CYRILLIC CAPITAL LETTER DJE
    { 0x82, 0x0453 }, // CYRILLIC SMALL LETTER GJE
    { 0x83, 0x0403 }, // CYRILLIC CAPITAL LETTER GJE
    { 0x84, 0x0451 }, // CYRILLIC SMALL LETTER IO
    { 0x85, 0x0401 }, // CYRILLIC CAPITAL LETTER IO
    { 0x86, 0x0454 }, // CYRILLIC SMALL LETTER UKRAINIAN IE
    { 0x87, 0x0404 }, // CYRILLIC CAPITAL LETTER UKRAINIAN IE
    { 0x88, 0x0455 }, // CYRILLIC SMALL LETTER DZE
    { 0x89, 0x0405 }, // CYRILLIC CAPITAL LETTER DZE
    { 0x8a, 0x0456 }, // CYRILLIC SMALL LETTER BYELORUSSIAN-UKRAINIAN I
    { 0x8b, 0x0406 }, // CYRILLIC CAPITAL LETTER BYELORUSSIAN-UKRAINIAN I
    { 0x8c, 0x0457 }, // CYRILLIC SMALL LETTER YI
    { 0x8d, 0x0407 }, // CYRILLIC CAPITAL LETTER YI
    { 0x8e, 0x0458 }, // CYRILLIC SMALL LETTER JE
    { 0x8f, 0x0408 }, // CYRILLIC CAPITAL LETTER JE
    { 0x90, 0x0459 }, // CYRILLIC SMALL LETTER LJE
    { 0x91, 0x0409 }, // CYRILLIC CAPITAL LETTER LJE
    { 0x92, 0x045a }, // CYRILLIC SMALL LETTER NJE
    { 0x93, 0x040a }, // CYRILLIC CAPITAL LETTER NJE
    { 0x94, 0x045b }, // CYRILLIC SMALL LETTER TSHE
    { 0x95, 0x040b }, // CYRILLIC CAPITAL LETTER TSHE
    { 0x96, 0x045c }, // CYRILLIC SMALL LETTER KJE
    { 0x97, 0x040c }, // CYRILLIC CAPITAL LETTER KJE
    { 0x98, 0x045e }, // CYRILLIC SMALL LETTER SHORT U
    { 0x99, 0x040e }, // CYRILLIC CAPITAL LETTER SHORT U
    { 0x9a, 0x045f }, // CYRILLIC SMALL LETTER DZHE
    { 0x9b, 0x040f }, // CYRILLIC CAPITAL LETTER DZHE
    { 0x9c, 0x044e }, // CYRILLIC SMALL LETTER YU
    { 0x9d, 0x042e }, // CYRILLIC CAPITAL LETTER YU
    { 0x9e, 0x044a }, // CYRILLIC SMALL LETTER HARD SIGN
    { 0x9f, 0x042a }, // CYRILLIC CAPITAL LETTER HARD SIGN
    { 0xa0, 0x0430 }, // CYRILLIC SMALL LETTER A
    { 0xa1, 0x0410 }, // CYRILLIC CAPITAL LETTER A
    { 0xa2, 0x0431 }, // CYRILLIC SMALL LETTER BE
    { 0xa3, 0x0411 }, // CYRILLIC CAPITAL LETTER BE
    { 0xa4, 0x0446 }, // CYRILLIC SMALL LETTER TSE
    { 0xa5, 0x0426 }, // CYRILLIC CAPITAL LETTER TSE
    { 0xa6, 0x0434 }, // CYRILLIC SMALL LETTER DE
    { 0xa7, 0x0414 }, // CYRILLIC CAPITAL LETTER DE
    { 0xa8, 0x0435 }, // CYRILLIC SMALL LETTER IE
    { 0xa9, 0x0415 }, // CYRILLIC CAPITAL LETTER IE
    { 0xaa, 0x0444 }, // CYRILLIC SMALL LETTER EF
    { 0xab, 0x0424 }, // CYRILLIC CAPITAL LETTER EF
    { 0xac, 0x0433 }, // CYRILLIC SMALL LETTER GHE
    { 0xad, 0x0413 }, // CYRILLIC CAPITAL LETTER GHE
    { 0xae, 0x00ab }, // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
    { 0xaf, 0x00bb }, // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
    { 0xb0, 0x2591 }, // LIGHT SHADE
    { 0xb1, 0x2592 }, // MEDIUM SHADE
    { 0xb2, 0x2593 }, // DARK SHADE
    { 0xb3, 0x2502 }, // BOX DRAWINGS LIGHT VERTICAL
    { 0xb4, 0x2524 }, // BOX DRAWINGS LIGHT VERTICAL AND LEFT
    { 0xb5, 0x0445 }, // CYRILLIC SMALL LETTER HA
    { 0xb6, 0x0425 }, // CYRILLIC CAPITAL LETTER HA
    { 0xb7, 0x0438 }, // CYRILLIC SMALL LETTER I
    { 0xb8, 0x0418 }, // CYRILLIC CAPITAL LETTER I
    { 0xb9, 0x2563 }, // BOX DRAWINGS DOUBLE VERTICAL AND LEFT
    { 0xba, 0x2551 }, // BOX DRAWINGS DOUBLE VERTICAL
    { 0xbb, 0x2557 }, // BOX DRAWINGS DOUBLE DOWN AND LEFT
    { 0xbc, 0x255d }, // BOX DRAWINGS DOUBLE UP AND LEFT
    { 0xbd, 0x0439 }, // CYRILLIC SMALL LETTER SHORT I
    { 0xbe, 0x0419 }, // CYRILLIC CAPITAL LETTER SHORT I
    { 0xbf, 0x2510 }, // BOX DRAWINGS LIGHT DOWN AND LEFT
    { 0xc0, 0x2514 }, // BOX DRAWINGS LIGHT UP AND RIGHT
    { 0xc1, 0x2534 }, // BOX DRAWINGS LIGHT UP AND HORIZONTAL
    { 0xc2, 0x252c }, // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
    { 0xc3, 0x251c }, // BOX DRAWINGS LIGHT VERTICAL AND RIGHT
    { 0xc4, 0x2500 }, // BOX DRAWINGS LIGHT HORIZONTAL
    { 0xc5, 0x253c }, // BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
    { 0xc6, 0x043a }, // CYRILLIC SMALL LETTER KA
    { 0xc7, 0x041a }, // CYRILLIC CAPITAL LETTER KA
    { 0xc8, 0x255a }, // BOX DRAWINGS DOUBLE UP AND RIGHT
    { 0xc9, 0x2554 }, // BOX DRAWINGS DOUBLE DOWN AND RIGHT
    { 0xca, 0x2569 }, // BOX DRAWINGS DOUBLE UP AND HORIZONTAL
    { 0xcb, 0x2566 }, // BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
    { 0xcc, 0x2560 }, // BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
    { 0xcd, 0x2550 }, // BOX DRAWINGS DOUBLE HORIZONTAL
    { 0xce, 0x256c }, // BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
    { 0xcf, 0x00a4 }, // CURRENCY SIGN
    { 0xd0, 0x043b }, // CYRILLIC SMALL LETTER EL
    { 0xd1, 0x041b }, // CYRILLIC CAPITAL LETTER EL
    { 0xd2, 0x043c }, // CYRILLIC SMALL LETTER EM
    { 0xd3, 0x041c }, // CYRILLIC CAPITAL LETTER EM
    { 0xd4, 0x043d }, // CYRILLIC SMALL LETTER EN
    { 0xd5, 0x041d }, // CYRILLIC CAPITAL LETTER EN
    { 0xd6, 0x043e }, // CYRILLIC SMALL LETTER O
    { 0xd7, 0x041e }, // CYRILLIC CAPITAL LETTER O
    { 0xd8, 0x043f }, // CYRILLIC SMALL LETTER PE
    { 0xd9, 0x2518 }, // BOX DRAWINGS LIGHT UP AND LEFT
    { 0xda, 0x250c }, // BOX DRAWINGS LIGHT DOWN AND RIGHT
    { 0xdb, 0x2588 }, // FULL BLOCK
    { 0xdc, 0x2584 }, // LOWER HALF BLOCK
    { 0xdd, 0x041f }, // CYRILLIC CAPITAL LETTER PE
    { 0xde, 0x044f }, // CYRILLIC SMALL LETTER YA
    { 0xdf, 0x2580 }, // UPPER HALF BLOCK
    { 0xe0, 0x042f }, // CYRILLIC CAPITAL LETTER YA
    { 0xe1, 0x0440 }, // CYRILLIC SMALL LETTER ER
    { 0xe2, 0x0420 }, // CYRILLIC CAPITAL LETTER ER
    { 0xe3, 0x0441 }, // CYRILLIC SMALL LETTER ES
    { 0xe4, 0x0421 }, // CYRILLIC CAPITAL LETTER ES
    { 0xe5, 0x0442 }, // CYRILLIC SMALL LETTER TE
    { 0xe6, 0x0422 }, // CYRILLIC CAPITAL LETTER TE
    { 0xe7, 0x0443 }, // CYRILLIC SMALL LETTER U
    { 0xe8, 0x0423 }, // CYRILLIC CAPITAL LETTER U
    { 0xe9, 0x0436 }, // CYRILLIC SMALL LETTER ZHE
    { 0xea, 0x0416 }, // CYRILLIC CAPITAL LETTER ZHE
    { 0xeb, 0x0432 }, // CYRILLIC SMALL LETTER VE
    { 0xec, 0x0412 }, // CYRILLIC CAPITAL LETTER VE
    { 0xed, 0x044c }, // CYRILLIC SMALL LETTER SOFT SIGN
    { 0xee, 0x042c }, // CYRILLIC CAPITAL LETTER SOFT SIGN
    { 0xef, 0x2116 }, // NUMERO SIGN
    { 0xf0, 0x00ad }, // SOFT HYPHEN
    { 0xf1, 0x044b }, // CYRILLIC SMALL LETTER YERU
    { 0xf2, 0x042b }, // CYRILLIC CAPITAL LETTER YERU
    { 0xf3, 0x0437 }, // CYRILLIC SMALL LETTER ZE
    { 0xf4, 0x0417 }, // CYRILLIC CAPITAL LETTER ZE
    { 0xf5, 0x0448 }, // CYRILLIC SMALL LETTER SHA
    { 0xf6, 0x0428 }, // CYRILLIC CAPITAL LETTER SHA
    { 0xf7, 0x044d }, // CYRILLIC SMALL LETTER E
    { 0xf8, 0x042d }, // CYRILLIC CAPITAL LETTER E
    { 0xf9, 0x0449 }, // CYRILLIC SMALL LETTER SHCHA
    { 0xfa, 0x0429 }, // CYRILLIC CAPITAL LETTER SHCHA
    { 0xfb, 0x0447 }, // CYRILLIC SMALL LETTER CHE
    { 0xfc, 0x0427 }, // CYRILLIC CAPITAL LETTER CHE
    { 0xfd, 0x00a7 }, // SECTION SIGN
    { 0xfe, 0x25a0 }, // BLACK SQUARE
    { 0xff, 0x00a0 }, // NO-BREAK SPACE
};
