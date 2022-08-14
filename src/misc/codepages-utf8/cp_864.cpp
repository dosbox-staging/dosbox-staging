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

#include "cp.h"
#include "checks.h"

CHECK_NARROWING();

// Based on information from:
// - https://unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP864.TXT

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
// are also added to table in 'cp_ascii.cpp'!


// Arabic
const CodePageImport cp_864 = {
    { 0x60, 0x0060 }, // GRAVE ACCENT
    { 0x61, 0x0061 }, // LATIN SMALL LETTER A
    { 0x62, 0x0062 }, // LATIN SMALL LETTER B
    { 0x63, 0x0063 }, // LATIN SMALL LETTER C
    { 0x64, 0x0064 }, // LATIN SMALL LETTER D
    { 0x65, 0x0065 }, // LATIN SMALL LETTER E
    { 0x66, 0x0066 }, // LATIN SMALL LETTER F
    { 0x67, 0x0067 }, // LATIN SMALL LETTER G
    { 0x68, 0x0068 }, // LATIN SMALL LETTER H
    { 0x69, 0x0069 }, // LATIN SMALL LETTER I
    { 0x6a, 0x006a }, // LATIN SMALL LETTER J
    { 0x6b, 0x006b }, // LATIN SMALL LETTER K
    { 0x6c, 0x006c }, // LATIN SMALL LETTER L
    { 0x6d, 0x006d }, // LATIN SMALL LETTER M
    { 0x6e, 0x006e }, // LATIN SMALL LETTER N
    { 0x6f, 0x006f }, // LATIN SMALL LETTER O
    { 0x70, 0x0070 }, // LATIN SMALL LETTER P
    { 0x71, 0x0071 }, // LATIN SMALL LETTER Q
    { 0x72, 0x0072 }, // LATIN SMALL LETTER R
    { 0x73, 0x0073 }, // LATIN SMALL LETTER S
    { 0x74, 0x0074 }, // LATIN SMALL LETTER T
    { 0x75, 0x0075 }, // LATIN SMALL LETTER U
    { 0x76, 0x0076 }, // LATIN SMALL LETTER V
    { 0x77, 0x0077 }, // LATIN SMALL LETTER W
    { 0x78, 0x0078 }, // LATIN SMALL LETTER X
    { 0x79, 0x0079 }, // LATIN SMALL LETTER Y
    { 0x7a, 0x007a }, // LATIN SMALL LETTER Z
    { 0x7b, 0x007b }, // LEFT CURLY BRACKET
    { 0x7c, 0x007c }, // VERTICAL LINE
    { 0x7d, 0x007d }, // RIGHT CURLY BRACKET
    { 0x7e, 0x007e }, // TILDE
    { 0x7f, 0x007f }, // DELETE
    { 0x80, 0x00b0 }, // DEGREE SIGN
    { 0x81, 0x00b7 }, // MIDDLE DOT
    { 0x82, 0x2219 }, // BULLET OPERATOR
    { 0x83, 0x221a }, // SQUARE ROOT
    { 0x84, 0x2592 }, // MEDIUM SHADE
    { 0x85, 0x2500 }, // FORMS LIGHT HORIZONTAL
    { 0x86, 0x2502 }, // FORMS LIGHT VERTICAL
    { 0x87, 0x253c }, // FORMS LIGHT VERTICAL AND HORIZONTAL
    { 0x88, 0x2524 }, // FORMS LIGHT VERTICAL AND LEFT
    { 0x89, 0x252c }, // FORMS LIGHT DOWN AND HORIZONTAL
    { 0x8a, 0x251c }, // FORMS LIGHT VERTICAL AND RIGHT
    { 0x8b, 0x2534 }, // FORMS LIGHT UP AND HORIZONTAL
    { 0x8c, 0x2510 }, // FORMS LIGHT DOWN AND LEFT
    { 0x8d, 0x250c }, // FORMS LIGHT DOWN AND RIGHT
    { 0x8e, 0x2514 }, // FORMS LIGHT UP AND RIGHT
    { 0x8f, 0x2518 }, // FORMS LIGHT UP AND LEFT
    { 0x90, 0x03b2 }, // GREEK SMALL BETA
    { 0x91, 0x221e }, // INFINITY
    { 0x92, 0x03c6 }, // GREEK SMALL PHI
    { 0x93, 0x00b1 }, // PLUS-OR-MINUS SIGN
    { 0x94, 0x00bd }, // FRACTION 1/2
    { 0x95, 0x00bc }, // FRACTION 1/4
    { 0x96, 0x2248 }, // ALMOST EQUAL TO
    { 0x97, 0x00ab }, // LEFT POINTING GUILLEMET
    { 0x98, 0x00bb }, // RIGHT POINTING GUILLEMET
    { 0x99, 0xfef7 }, // ARABIC LIGATURE LAM WITH ALEF WITH HAMZA ABOVE ISOLATED FORM
    { 0x9a, 0xfef8 }, // ARABIC LIGATURE LAM WITH ALEF WITH HAMZA ABOVE FINAL FORM
//  { 0x9b, 0x???? }, // UNDEFINED
//  { 0x9c, 0x???? }, // UNDEFINED
    { 0x9d, 0xfefb }, // ARABIC LIGATURE LAM WITH ALEF ISOLATED FORM
    { 0x9e, 0xfefc }, // ARABIC LIGATURE LAM WITH ALEF FINAL FORM
//  { 0x9f, 0x???? }, // UNDEFINED
    { 0xa0, 0x00a0 }, // NON-BREAKING SPACE
    { 0xa1, 0x00ad }, // SOFT HYPHEN
    { 0xa2, 0xfe82 }, // ARABIC LETTER ALEF WITH MADDA ABOVE FINAL FORM
    { 0xa3, 0x00a3 }, // POUND SIGN
    { 0xa4, 0x00a4 }, // CURRENCY SIGN
    { 0xa5, 0xfe84 }, // ARABIC LETTER ALEF WITH HAMZA ABOVE FINAL FORM
//  { 0xa6, 0x???? }, // UNDEFINED
//  { 0xa7, 0x???? }, // UNDEFINED
    { 0xa8, 0xfe8e }, // ARABIC LETTER ALEF FINAL FORM
    { 0xa9, 0xfe8f }, // ARABIC LETTER BEH ISOLATED FORM
    { 0xaa, 0xfe95 }, // ARABIC LETTER TEH ISOLATED FORM
    { 0xab, 0xfe99 }, // ARABIC LETTER THEH ISOLATED FORM
    { 0xac, 0x060c }, // ARABIC COMMA
    { 0xad, 0xfe9d }, // ARABIC LETTER JEEM ISOLATED FORM
    { 0xae, 0xfea1 }, // ARABIC LETTER HAH ISOLATED FORM
    { 0xaf, 0xfea5 }, // ARABIC LETTER KHAH ISOLATED FORM
    { 0xb0, 0x0660 }, // ARABIC-INDIC DIGIT ZERO
    { 0xb1, 0x0661 }, // ARABIC-INDIC DIGIT ONE
    { 0xb2, 0x0662 }, // ARABIC-INDIC DIGIT TWO
    { 0xb3, 0x0663 }, // ARABIC-INDIC DIGIT THREE
    { 0xb4, 0x0664 }, // ARABIC-INDIC DIGIT FOUR
    { 0xb5, 0x0665 }, // ARABIC-INDIC DIGIT FIVE
    { 0xb6, 0x0666 }, // ARABIC-INDIC DIGIT SIX
    { 0xb7, 0x0667 }, // ARABIC-INDIC DIGIT SEVEN
    { 0xb8, 0x0668 }, // ARABIC-INDIC DIGIT EIGHT
    { 0xb9, 0x0669 }, // ARABIC-INDIC DIGIT NINE
    { 0xba, 0xfed1 }, // ARABIC LETTER FEH ISOLATED FORM
    { 0xbb, 0x061b }, // ARABIC SEMICOLON
    { 0xbc, 0xfeb1 }, // ARABIC LETTER SEEN ISOLATED FORM
    { 0xbd, 0xfeb5 }, // ARABIC LETTER SHEEN ISOLATED FORM
    { 0xbe, 0xfeb9 }, // ARABIC LETTER SAD ISOLATED FORM
    { 0xbf, 0x061f }, // ARABIC QUESTION MARK
    { 0xc0, 0x00a2 }, // CENT SIGN
    { 0xc1, 0xfe80 }, // ARABIC LETTER HAMZA ISOLATED FORM
    { 0xc2, 0xfe81 }, // ARABIC LETTER ALEF WITH MADDA ABOVE ISOLATED FORM
    { 0xc3, 0xfe83 }, // ARABIC LETTER ALEF WITH HAMZA ABOVE ISOLATED FORM
    { 0xc4, 0xfe85 }, // ARABIC LETTER WAW WITH HAMZA ABOVE ISOLATED FORM
    { 0xc5, 0xfeca }, // ARABIC LETTER AIN FINAL FORM
    { 0xc6, 0xfe8b }, // ARABIC LETTER YEH WITH HAMZA ABOVE INITIAL FORM
    { 0xc7, 0xfe8d }, // ARABIC LETTER ALEF ISOLATED FORM
    { 0xc8, 0xfe91 }, // ARABIC LETTER BEH INITIAL FORM
    { 0xc9, 0xfe93 }, // ARABIC LETTER TEH MARBUTA ISOLATED FORM
    { 0xca, 0xfe97 }, // ARABIC LETTER TEH INITIAL FORM
    { 0xcb, 0xfe9b }, // ARABIC LETTER THEH INITIAL FORM
    { 0xcc, 0xfe9f }, // ARABIC LETTER JEEM INITIAL FORM
    { 0xcd, 0xfea3 }, // ARABIC LETTER HAH INITIAL FORM
    { 0xce, 0xfea7 }, // ARABIC LETTER KHAH INITIAL FORM
    { 0xcf, 0xfea9 }, // ARABIC LETTER DAL ISOLATED FORM
    { 0xd0, 0xfeab }, // ARABIC LETTER THAL ISOLATED FORM
    { 0xd1, 0xfead }, // ARABIC LETTER REH ISOLATED FORM
    { 0xd2, 0xfeaf }, // ARABIC LETTER ZAIN ISOLATED FORM
    { 0xd3, 0xfeb3 }, // ARABIC LETTER SEEN INITIAL FORM
    { 0xd4, 0xfeb7 }, // ARABIC LETTER SHEEN INITIAL FORM
    { 0xd5, 0xfebb }, // ARABIC LETTER SAD INITIAL FORM
    { 0xd6, 0xfebf }, // ARABIC LETTER DAD INITIAL FORM
    { 0xd7, 0xfec1 }, // ARABIC LETTER TAH ISOLATED FORM
    { 0xd8, 0xfec5 }, // ARABIC LETTER ZAH ISOLATED FORM
    { 0xd9, 0xfecb }, // ARABIC LETTER AIN INITIAL FORM
    { 0xda, 0xfecf }, // ARABIC LETTER GHAIN INITIAL FORM
    { 0xdb, 0x00a6 }, // BROKEN VERTICAL BAR
    { 0xdc, 0x00ac }, // NOT SIGN
    { 0xdd, 0x00f7 }, // DIVISION SIGN
    { 0xde, 0x00d7 }, // MULTIPLICATION SIGN
    { 0xdf, 0xfec9 }, // ARABIC LETTER AIN ISOLATED FORM
    { 0xe0, 0x0640 }, // ARABIC TATWEEL
    { 0xe1, 0xfed3 }, // ARABIC LETTER FEH INITIAL FORM
    { 0xe2, 0xfed7 }, // ARABIC LETTER QAF INITIAL FORM
    { 0xe3, 0xfedb }, // ARABIC LETTER KAF INITIAL FORM
    { 0xe4, 0xfedf }, // ARABIC LETTER LAM INITIAL FORM
    { 0xe5, 0xfee3 }, // ARABIC LETTER MEEM INITIAL FORM
    { 0xe6, 0xfee7 }, // ARABIC LETTER NOON INITIAL FORM
    { 0xe7, 0xfeeb }, // ARABIC LETTER HEH INITIAL FORM
    { 0xe8, 0xfeed }, // ARABIC LETTER WAW ISOLATED FORM
    { 0xe9, 0xfeef }, // ARABIC LETTER ALEF MAKSURA ISOLATED FORM
    { 0xea, 0xfef3 }, // ARABIC LETTER YEH INITIAL FORM
    { 0xeb, 0xfebd }, // ARABIC LETTER DAD ISOLATED FORM
    { 0xec, 0xfecc }, // ARABIC LETTER AIN MEDIAL FORM
    { 0xed, 0xfece }, // ARABIC LETTER GHAIN FINAL FORM
    { 0xee, 0xfecd }, // ARABIC LETTER GHAIN ISOLATED FORM
    { 0xef, 0xfee1 }, // ARABIC LETTER MEEM ISOLATED FORM
    { 0xf0, 0xfe7d }, // ARABIC SHADDA MEDIAL FORM
    { 0xf1, 0x0651 }, // ARABIC SHADDAH
    { 0xf2, 0xfee5 }, // ARABIC LETTER NOON ISOLATED FORM
    { 0xf3, 0xfee9 }, // ARABIC LETTER HEH ISOLATED FORM
    { 0xf4, 0xfeec }, // ARABIC LETTER HEH MEDIAL FORM
    { 0xf5, 0xfef0 }, // ARABIC LETTER ALEF MAKSURA FINAL FORM
    { 0xf6, 0xfef2 }, // ARABIC LETTER YEH FINAL FORM
    { 0xf7, 0xfed0 }, // ARABIC LETTER GHAIN MEDIAL FORM
    { 0xf8, 0xfed5 }, // ARABIC LETTER QAF ISOLATED FORM
    { 0xf9, 0xfef5 }, // ARABIC LIGATURE LAM WITH ALEF WITH MADDA ABOVE ISOLATED FORM
    { 0xfa, 0xfef6 }, // ARABIC LIGATURE LAM WITH ALEF WITH MADDA ABOVE FINAL FORM
    { 0xfb, 0xfedd }, // ARABIC LETTER LAM ISOLATED FORM
    { 0xfc, 0xfed9 }, // ARABIC LETTER KAF ISOLATED FORM
    { 0xfd, 0xfef1 }, // ARABIC LETTER YEH ISOLATED FORM
    { 0xfe, 0x25a0 }, // BLACK SQUARE
//  { 0xff, 0x???? }, // UNDEFINED
};
