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
// - https://unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP874.TXT

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


// Thai
static const CodePageImport cp_874 = {
    { 0x80, 0x20AC }, // EURO SIGN
//  { 0x81, 0x???? }, // UNDEFINED
//  { 0x82, 0x???? }, // UNDEFINED
//  { 0x83, 0x???? }, // UNDEFINED
//  { 0x84, 0x???? }, // UNDEFINED
    { 0x85, 0x2026 }, // HORIZONTAL ELLIPSIS
//  { 0x86, 0x???? }, // UNDEFINED
//  { 0x87, 0x???? }, // UNDEFINED
//  { 0x88, 0x???? }, // UNDEFINED
//  { 0x89, 0x???? }, // UNDEFINED
//  { 0x8A, 0x???? }, // UNDEFINED
//  { 0x8B, 0x???? }, // UNDEFINED
//  { 0x8C, 0x???? }, // UNDEFINED
//  { 0x8D, 0x???? }, // UNDEFINED
//  { 0x8E, 0x???? }, // UNDEFINED
//  { 0x8F, 0x???? }, // UNDEFINED
//  { 0x90, 0x???? }, // UNDEFINED
    { 0x91, 0x2018 }, // LEFT SINGLE QUOTATION MARK
    { 0x92, 0x2019 }, // RIGHT SINGLE QUOTATION MARK
    { 0x93, 0x201C }, // LEFT DOUBLE QUOTATION MARK
    { 0x94, 0x201D }, // RIGHT DOUBLE QUOTATION MARK
    { 0x95, 0x2022 }, // BULLET
    { 0x96, 0x2013 }, // EN DASH
    { 0x97, 0x2014 }, // EM DASH
//  { 0x98, 0x???? }, // UNDEFINED
//  { 0x99, 0x???? }, // UNDEFINED
//  { 0x9A, 0x???? }, // UNDEFINED
//  { 0x9B, 0x???? }, // UNDEFINED
//  { 0x9C, 0x???? }, // UNDEFINED
//  { 0x9D, 0x???? }, // UNDEFINED
//  { 0x9E, 0x???? }, // UNDEFINED
//  { 0x9F, 0x???? }, // UNDEFINED
    { 0xA0, 0x00A0 }, // NO-BREAK SPACE
    { 0xA1, 0x0E01 }, // THAI CHARACTER KO KAI
    { 0xA2, 0x0E02 }, // THAI CHARACTER KHO KHAI
    { 0xA3, 0x0E03 }, // THAI CHARACTER KHO KHUAT
    { 0xA4, 0x0E04 }, // THAI CHARACTER KHO KHWAI
    { 0xA5, 0x0E05 }, // THAI CHARACTER KHO KHON
    { 0xA6, 0x0E06 }, // THAI CHARACTER KHO RAKHANG
    { 0xA7, 0x0E07 }, // THAI CHARACTER NGO NGU
    { 0xA8, 0x0E08 }, // THAI CHARACTER CHO CHAN
    { 0xA9, 0x0E09 }, // THAI CHARACTER CHO CHING
    { 0xAA, 0x0E0A }, // THAI CHARACTER CHO CHANG
    { 0xAB, 0x0E0B }, // THAI CHARACTER SO SO
    { 0xAC, 0x0E0C }, // THAI CHARACTER CHO CHOE
    { 0xAD, 0x0E0D }, // THAI CHARACTER YO YING
    { 0xAE, 0x0E0E }, // THAI CHARACTER DO CHADA
    { 0xAF, 0x0E0F }, // THAI CHARACTER TO PATAK
    { 0xB0, 0x0E10 }, // THAI CHARACTER THO THAN
    { 0xB1, 0x0E11 }, // THAI CHARACTER THO NANGMONTHO
    { 0xB2, 0x0E12 }, // THAI CHARACTER THO PHUTHAO
    { 0xB3, 0x0E13 }, // THAI CHARACTER NO NEN
    { 0xB4, 0x0E14 }, // THAI CHARACTER DO DEK
    { 0xB5, 0x0E15 }, // THAI CHARACTER TO TAO
    { 0xB6, 0x0E16 }, // THAI CHARACTER THO THUNG
    { 0xB7, 0x0E17 }, // THAI CHARACTER THO THAHAN
    { 0xB8, 0x0E18 }, // THAI CHARACTER THO THONG
    { 0xB9, 0x0E19 }, // THAI CHARACTER NO NU
    { 0xBA, 0x0E1A }, // THAI CHARACTER BO BAIMAI
    { 0xBB, 0x0E1B }, // THAI CHARACTER PO PLA
    { 0xBC, 0x0E1C }, // THAI CHARACTER PHO PHUNG
    { 0xBD, 0x0E1D }, // THAI CHARACTER FO FA
    { 0xBE, 0x0E1E }, // THAI CHARACTER PHO PHAN
    { 0xBF, 0x0E1F }, // THAI CHARACTER FO FAN
    { 0xC0, 0x0E20 }, // THAI CHARACTER PHO SAMPHAO
    { 0xC1, 0x0E21 }, // THAI CHARACTER MO MA
    { 0xC2, 0x0E22 }, // THAI CHARACTER YO YAK
    { 0xC3, 0x0E23 }, // THAI CHARACTER RO RUA
    { 0xC4, 0x0E24 }, // THAI CHARACTER RU
    { 0xC5, 0x0E25 }, // THAI CHARACTER LO LING
    { 0xC6, 0x0E26 }, // THAI CHARACTER LU
    { 0xC7, 0x0E27 }, // THAI CHARACTER WO WAEN
    { 0xC8, 0x0E28 }, // THAI CHARACTER SO SALA
    { 0xC9, 0x0E29 }, // THAI CHARACTER SO RUSI
    { 0xCA, 0x0E2A }, // THAI CHARACTER SO SUA
    { 0xCB, 0x0E2B }, // THAI CHARACTER HO HIP
    { 0xCC, 0x0E2C }, // THAI CHARACTER LO CHULA
    { 0xCD, 0x0E2D }, // THAI CHARACTER O ANG
    { 0xCE, 0x0E2E }, // THAI CHARACTER HO NOKHUK
    { 0xCF, 0x0E2F }, // THAI CHARACTER PAIYANNOI
    { 0xD0, 0x0E30 }, // THAI CHARACTER SARA A
    { 0xD1, 0x0E31 }, // THAI CHARACTER MAI HAN-AKAT
    { 0xD2, 0x0E32 }, // THAI CHARACTER SARA AA
    { 0xD3, 0x0E33 }, // THAI CHARACTER SARA AM
    { 0xD4, 0x0E34 }, // THAI CHARACTER SARA I
    { 0xD5, 0x0E35 }, // THAI CHARACTER SARA II
    { 0xD6, 0x0E36 }, // THAI CHARACTER SARA UE
    { 0xD7, 0x0E37 }, // THAI CHARACTER SARA UEE
    { 0xD8, 0x0E38 }, // THAI CHARACTER SARA U
    { 0xD9, 0x0E39 }, // THAI CHARACTER SARA UU
    { 0xDA, 0x0E3A }, // THAI CHARACTER PHINTHU
//  { 0xDB, 0x???? }, // UNDEFINED
//  { 0xDC, 0x???? }, // UNDEFINED
//  { 0xDD, 0x???? }, // UNDEFINED
//  { 0xDE, 0x???? }, // UNDEFINED
    { 0xDF, 0x0E3F }, // THAI CURRENCY SYMBOL BAHT
    { 0xE0, 0x0E40 }, // THAI CHARACTER SARA E
    { 0xE1, 0x0E41 }, // THAI CHARACTER SARA AE
    { 0xE2, 0x0E42 }, // THAI CHARACTER SARA O
    { 0xE3, 0x0E43 }, // THAI CHARACTER SARA AI MAIMUAN
    { 0xE4, 0x0E44 }, // THAI CHARACTER SARA AI MAIMALAI
    { 0xE5, 0x0E45 }, // THAI CHARACTER LAKKHANGYAO
    { 0xE6, 0x0E46 }, // THAI CHARACTER MAIYAMOK
    { 0xE7, 0x0E47 }, // THAI CHARACTER MAITAIKHU
    { 0xE8, 0x0E48 }, // THAI CHARACTER MAI EK
    { 0xE9, 0x0E49 }, // THAI CHARACTER MAI THO
    { 0xEA, 0x0E4A }, // THAI CHARACTER MAI TRI
    { 0xEB, 0x0E4B }, // THAI CHARACTER MAI CHATTAWA
    { 0xEC, 0x0E4C }, // THAI CHARACTER THANTHAKHAT
    { 0xED, 0x0E4D }, // THAI CHARACTER NIKHAHIT
    { 0xEE, 0x0E4E }, // THAI CHARACTER YAMAKKAN
    { 0xEF, 0x0E4F }, // THAI CHARACTER FONGMAN
    { 0xF0, 0x0E50 }, // THAI DIGIT ZERO
    { 0xF1, 0x0E51 }, // THAI DIGIT ONE
    { 0xF2, 0x0E52 }, // THAI DIGIT TWO
    { 0xF3, 0x0E53 }, // THAI DIGIT THREE
    { 0xF4, 0x0E54 }, // THAI DIGIT FOUR
    { 0xF5, 0x0E55 }, // THAI DIGIT FIVE
    { 0xF6, 0x0E56 }, // THAI DIGIT SIX
    { 0xF7, 0x0E57 }, // THAI DIGIT SEVEN
    { 0xF8, 0x0E58 }, // THAI DIGIT EIGHT
    { 0xF9, 0x0E59 }, // THAI DIGIT NINE
    { 0xFA, 0x0E5A }, // THAI CHARACTER ANGKHANKHU
    { 0xFB, 0x0E5B }, // THAI CHARACTER KHOMUT
//  { 0xFC, 0x???? }, // UNDEFINED
//  { 0xFD, 0x???? }, // UNDEFINED
//  { 0xFE, 0x???? }, // UNDEFINED
//  { 0xFF, 0x???? }, // UNDEFINED
};
