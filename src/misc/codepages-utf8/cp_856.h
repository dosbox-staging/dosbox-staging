/*
 *  Copyright (C) 2022       The DOSBox Staging Team
 *  Copyright (C) 1998-1999  Unicode, Inc.
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
// - https://unicode.org/Public/MAPPINGS/VENDORS/MISC/CP856.TXT

// Original file contained the following disclaimer:

// This file is provided as-is by Unicode, Inc. (The Unicode Consortium).
// No claims are made as to fitness for any particular purpose.  No
// warranties of any kind are expressed or implied.  The recipient
// agrees to determine applicability of information provided.  If this
// file has been provided on optical media by Unicode, Inc., the sole
// remedy for any claim will be exchange of defective media within 90
// days of receipt.
//
// Unicode, Inc. hereby grants the right to freely use the information
// supplied in this file in the creation of products supporting the
// Unicode Standard, and to make copies of this file in any form for
// internal or external distribution as long as this notice remains
// attached.


// When adding/updating code page, make sure all the UTF-8 codes
// are also added to table in 'cp_ascii.h'!


// Hebrew-2
static const CodePageImport cp_856 = {
    { 0x80, 0x05D0 }, // HEBREW LETTER ALEF
    { 0x81, 0x05D1 }, // HEBREW LETTER BET
    { 0x82, 0x05D2 }, // HEBREW LETTER GIMEL
    { 0x83, 0x05D3 }, // HEBREW LETTER DALET
    { 0x84, 0x05D4 }, // HEBREW LETTER HE
    { 0x85, 0x05D5 }, // HEBREW LETTER VAV
    { 0x86, 0x05D6 }, // HEBREW LETTER ZAYIN
    { 0x87, 0x05D7 }, // HEBREW LETTER HET
    { 0x88, 0x05D8 }, // HEBREW LETTER TET
    { 0x89, 0x05D9 }, // HEBREW LETTER YOD
    { 0x8A, 0x05DA }, // HEBREW LETTER FINAL KAF
    { 0x8B, 0x05DB }, // HEBREW LETTER KAF
    { 0x8C, 0x05DC }, // HEBREW LETTER LAMED
    { 0x8D, 0x05DD }, // HEBREW LETTER FINAL MEM
    { 0x8E, 0x05DE }, // HEBREW LETTER MEM
    { 0x8F, 0x05DF }, // HEBREW LETTER FINAL NUN
    { 0x90, 0x05E0 }, // HEBREW LETTER NUN
    { 0x91, 0x05E1 }, // HEBREW LETTER SAMEKH
    { 0x92, 0x05E2 }, // HEBREW LETTER AYIN
    { 0x93, 0x05E3 }, // HEBREW LETTER FINAL PE
    { 0x94, 0x05E4 }, // HEBREW LETTER PE
    { 0x95, 0x05E5 }, // HEBREW LETTER FINAL TSADI
    { 0x96, 0x05E6 }, // HEBREW LETTER TSADI
    { 0x97, 0x05E7 }, // HEBREW LETTER QOF
    { 0x98, 0x05E8 }, // HEBREW LETTER RESH
    { 0x99, 0x05E9 }, // HEBREW LETTER SHIN
    { 0x9A, 0x05EA }, // HEBREW LETTER TAV
//  { 0x9B, 0x???? }, // UNDEFINED
    { 0x9C, 0x00A3 }, // POUND SIGN
//  { 0x9D, 0x???? }, // UNDEFINED
    { 0x9E, 0x00D7 }, // MULTIPLICATION SIGN
//  { 0x9F, 0x???? }, // UNDEFINED
//  { 0xA0, 0x???? }, // UNDEFINED
//  { 0xA1, 0x???? }, // UNDEFINED
//  { 0xA2, 0x???? }, // UNDEFINED
//  { 0xA3, 0x???? }, // UNDEFINED
//  { 0xA4, 0x???? }, // UNDEFINED
//  { 0xA5, 0x???? }, // UNDEFINED
//  { 0xA6, 0x???? }, // UNDEFINED
//  { 0xA7, 0x???? }, // UNDEFINED
//  { 0xA8, 0x???? }, // UNDEFINED
    { 0xA9, 0x00AE }, // REGISTERED SIGN
    { 0xAA, 0x00AC }, // NOT SIGN
    { 0xAB, 0x00BD }, // VULGAR FRACTION ONE HALF
    { 0xAC, 0x00BC }, // VULGAR FRACTION ONE QUARTER
//  { 0xAD, 0x???? }, // UNDEFINED
    { 0xAE, 0x00AB }, // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
    { 0xAF, 0x00BB }, // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
    { 0xB0, 0x2591 }, // LIGHT SHADE
    { 0xB1, 0x2592 }, // MEDIUM SHADE
    { 0xB2, 0x2593 }, // DARK SHADE
    { 0xB3, 0x2502 }, // BOX DRAWINGS LIGHT VERTICAL
    { 0xB4, 0x2524 }, // BOX DRAWINGS LIGHT VERTICAL AND LEFT
//  { 0xB5, 0x???? }, // UNDEFINED
//  { 0xB6, 0x???? }, // UNDEFINED
//  { 0xB7, 0x???? }, // UNDEFINED
//  { 0xB8, 0x00A9 }, // COPYRIGHT SIGN
    { 0xB9, 0x2563 }, // BOX DRAWINGS DOUBLE VERTICAL AND LEFT
    { 0xBA, 0x2551 }, // BOX DRAWINGS DOUBLE VERTICAL
    { 0xBB, 0x2557 }, // BOX DRAWINGS DOUBLE DOWN AND LEFT
    { 0xBC, 0x255D }, // BOX DRAWINGS DOUBLE UP AND LEFT
    { 0xBD, 0x00A2 }, // CENT SIGN
    { 0xBE, 0x00A5 }, // YEN SIGN
    { 0xBF, 0x2510 }, // BOX DRAWINGS LIGHT DOWN AND LEFT
    { 0xC0, 0x2514 }, // BOX DRAWINGS LIGHT UP AND RIGHT
    { 0xC1, 0x2534 }, // BOX DRAWINGS LIGHT UP AND HORIZONTAL
    { 0xC2, 0x252C }, // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
    { 0xC3, 0x251C }, // BOX DRAWINGS LIGHT VERTICAL AND RIGHT
    { 0xC4, 0x2500 }, // BOX DRAWINGS LIGHT HORIZONTAL
    { 0xC5, 0x253C }, // BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
//  { 0xC6, 0x???? }, // UNDEFINED
//  { 0xC7, 0x???? }, // UNDEFINED
    { 0xC8, 0x255A }, // BOX DRAWINGS DOUBLE UP AND RIGHT
    { 0xC9, 0x2554 }, // BOX DRAWINGS DOUBLE DOWN AND RIGHT
    { 0xCA, 0x2569 }, // BOX DRAWINGS DOUBLE UP AND HORIZONTAL
    { 0xCB, 0x2566 }, // BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
    { 0xCC, 0x2560 }, // BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
    { 0xCD, 0x2550 }, // BOX DRAWINGS DOUBLE HORIZONTAL
    { 0xCE, 0x256C }, // BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
    { 0xCF, 0x00A4 }, // CURRENCY SIGN
//  { 0xD0, 0x???? }, // UNDEFINED
//  { 0xD1, 0x???? }, // UNDEFINED
//  { 0xD2, 0x???? }, // UNDEFINED
//  { 0xD3, 0x???? }, // UNDEFINEDS
//  { 0xD4, 0x???? }, // UNDEFINED
//  { 0xD5, 0x???? }, // UNDEFINED
//  { 0xD6, 0x???? }, // UNDEFINEDE
//  { 0xD7, 0x???? }, // UNDEFINED
//  { 0xD8, 0x???? }, // UNDEFINED
    { 0xD9, 0x2518 }, // BOX DRAWINGS LIGHT UP AND LEFT
    { 0xDA, 0x250C }, // BOX DRAWINGS LIGHT DOWN AND RIGHT
    { 0xDB, 0x2588 }, // FULL BLOCK
    { 0xDC, 0x2584 }, // LOWER HALF BLOCK
    { 0xDD, 0x00A6 }, // BROKEN BAR
//  { 0xDE, 0x???? }, // UNDEFINED
    { 0xDF, 0x2580 }, // UPPER HALF BLOCK
//  { 0xE0, 0x???? }, // UNDEFINED
//  { 0xE1, 0x???? }, // UNDEFINED
//  { 0xE2, 0x???? }, // UNDEFINED
//  { 0xE3, 0x???? }, // UNDEFINED
//  { 0xE4, 0x???? }, // UNDEFINED
//  { 0xE5, 0x???? }, // UNDEFINED
    { 0xE6, 0x00B5 }, // MICRO SIGN
//  { 0xE7, 0x???? }, // UNDEFINED
//  { 0xE8, 0x???? }, // UNDEFINED
//  { 0xE9, 0x???? }, // UNDEFINED
//  { 0xEA, 0x???? }, // UNDEFINED
//  { 0xEB, 0x???? }, // UNDEFINED
//  { 0xEC, 0x???? }, // UNDEFINED
//  { 0xED, 0x???? }, // UNDEFINED
    { 0xEE, 0x00AF }, // MACRON
    { 0xEF, 0x00B4 }, // ACUTE ACCENT
    { 0xF0, 0x00AD }, // SOFT HYPHEN
    { 0xF1, 0x00B1 }, // PLUS-MINUS SIGN
    { 0xF2, 0x2017 }, // DOUBLE LOW LINE
    { 0xF3, 0x00BE }, // VULGAR FRACTION THREE QUARTERS
    { 0xF4, 0x00B6 }, // PILCROW SIGN
    { 0xF5, 0x00A7 }, // SECTION SIGN
    { 0xF6, 0x00F7 }, // DIVISION SIGN
    { 0xF7, 0x00B8 }, // CEDILLA
    { 0xF8, 0x00B0 }, // DEGREE SIGN
    { 0xF9, 0x00A8 }, // DIAERESIS
    { 0xFA, 0x00B7 }, // MIDDLE DOT
    { 0xFB, 0x00B9 }, // SUPERSCRIPT ONE
    { 0xFC, 0x00B3 }, // SUPERSCRIPT THREE
    { 0xFD, 0x00B2 }, // SUPERSCRIPT TWO
    { 0xFE, 0x25A0 }, // BLACK SQUARE
    { 0xFF, 0x00A0 }, // NO-BREAK SPACE
};
