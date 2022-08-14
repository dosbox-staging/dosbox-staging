/*
 *  Copyright (C) 2022       The DOSBox Staging Team
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

// This file contains partial code page information; full code page mapping
// will be constructed by combining these data with other code pages

// When adding/updating code page, make sure all the UTF-8 codes
// are also added to table in 'cp_ascii.cpp'!


// ***************************************************************************
// Partial definitions for code page mappings
// ***************************************************************************

// Euro sign on 0xaa position, IBM standard
static const CodePageImport cp_euro_aa = {
    { 0xaa, 0x20ac }, // EURO SIGN
};

// Euro sign on 0xd5 position, mainly for CP 858
static const CodePageImport cp_euro_d5 = {
    { 0xd5, 0x20ac }, // EURO SIGN
};

// Euro sign on 0xee position
static const CodePageImport cp_euro_cf = {
    { 0xcf, 0x20ac }, // EURO SIGN
};

// Euro sign on 0xee position, common to many code pages
static const CodePageImport cp_euro_ee = {
    { 0xee, 0x20ac }, // EURO SIGN
};

// Polish - this is to effectively clear a position
// occupied by a 'PLN' currency symbol, which is not present
// in the Unicode standard
static const CodePageImport cp_pln_none = {
    { 0x9b, UINT16_MAX },
};

// Polish - old standard, also known as Mazovia
// see: https://en.wikipedia.org/wiki/Mazovia_encoding
static const CodePageImport cp_partial_667 = {
    { 0x86, 0x0105 }, // LATIN SMALL LETTER A WITH OGONEK
    { 0x8d, 0x0107 }, // LATIN SMALL LETTER C WITH ACUTE
    { 0x8f, 0x0104 }, // LATIN CAPITAL LETTER A WITH OGONEK
    { 0x90, 0x0118 }, // LATIN CAPITAL LETTER E WITH OGONEK
    { 0x91, 0x0119 }, // LATIN SMALL LETTER E WITH OGONEK
    { 0x92, 0x0142 }, // LATIN SMALL LETTER L WITH STROKE
    { 0x95, 0x0106 }, // LATIN CAPITAL LETTER C WITH ACUTE
    { 0x98, 0x015a }, // LATIN CAPITAL LETTER S WITH ACUTE
    { 0x9c, 0x0141 }, // LATIN CAPITAL LETTER L WITH STROKE
    { 0x9e, 0x015b }, // LATIN SMALL LETTER S WITH ACUTE
    { 0xa0, 0x0179 }, // LATIN CAPITAL LETTER Z WITH ACUTE
    { 0xa1, 0x017b }, // LATIN CAPITAL LETTER Z WITH DOT ABOVE
    { 0xa2, 0x00f3 }, // LATIN SMALL LETTER O WITH ACUTE
    { 0xa3, 0x00d3 }, // LATIN CAPITAL LETTER O WITH ACUTE
    { 0xa4, 0x0144 }, // LATIN SMALL LETTER N WITH ACUTE
    { 0xa5, 0x0143 }, // LATIN CAPITAL LETTER N WITH ACUTE
    { 0xa6, 0x017a }, // LATIN SMALL LETTER Z WITH ACUTE
    { 0xa7, 0x017c }, // LATIN SMALL LETTER Z WITH DOT ABOVE
};

// Polish - provides national characters on the same points as with CP 852,
// but is limited to Polish characters only, thus preserving more graphical
// characters
static const CodePageImport cp_partial_668 = {
    { 0x86, 0x0107 }, // LATIN SMALL LETTER C WITH ACUTE
    { 0x88, 0x0142 }, // LATIN SMALL LETTER L WITH STROKE
    { 0x8d, 0x0179 }, // LATIN CAPITAL LETTER Z WITH ACUTE
    { 0x8f, 0x0106 }, // LATIN CAPITAL LETTER C WITH ACUTE
    { 0x97, 0x015a }, // LATIN CAPITAL LETTER S WITH ACUTE
    { 0x98, 0x015b }, // LATIN SMALL LETTER S WITH ACUTE
    { 0x9d, 0x0141 }, // LATIN CAPITAL LETTER L WITH STROKE
    { 0xa2, 0x00f3 }, // LATIN SMALL LETTER O WITH ACUTE
    { 0xa4, 0x0104 }, // LATIN CAPITAL LETTER A WITH OGONEK
    { 0xa5, 0x0105 }, // LATIN SMALL LETTER A WITH OGONEK
    { 0xa8, 0x0118 }, // LATIN CAPITAL LETTER E WITH OGONEK
    { 0xa9, 0x0119 }, // LATIN SMALL LETTER E WITH OGONEK
    { 0xab, 0x017a }, // LATIN SMALL LETTER Z WITH ACUTE
    { 0xbd, 0x017b }, // LATIN CAPITAL LETTER Z WITH DOT ABOVE
    { 0xbe, 0x017c }, // LATIN SMALL LETTER Z WITH DOT ABOVE
    { 0xe0, 0x00d3 }, // LATIN CAPITAL LETTER O WITH ACUTE
    { 0xe3, 0x0143 }, // LATIN CAPITAL LETTER N WITH ACUTE
    { 0xe4, 0x0144 }, // LATIN SMALL LETTER N WITH ACUTE
};

// Baltic
// see: https://en.wikipedia.org/wiki/Code_page_770
static const CodePageImport cp_partial_770 = {
    { 0x80, 0x010c }, // LATIN CAPITAL LETTER C WITH CARON
    { 0x82, 0x0117 }, // LATIN SMALL LETTER E WITH DOT ABOVE
    { 0x83, 0x0101 }, // LATIN SMALL LETTER A WITH MACRON
    { 0x85, 0x0105 }, // LATIN SMALL LETTER A WITH OGONEK
    { 0x86, 0x013c }, // LATIN SMALL LETTER L WITH CEDILLA
    { 0x87, 0x010d }, // LATIN SMALL LETTER C WITH CARON
    { 0x88, 0x0113 }, // LATIN SMALL LETTER E WITH MACRON
    { 0x89, 0x0112 }, // LATIN CAPITAL LETTER E WITH MACRON
    { 0x8a, 0x0119 }, // LATIN SMALL LETTER E WITH OGONEK
    { 0x8b, 0x0118 }, // LATIN CAPITAL LETTER E WITH OGONEK
    { 0x8c, 0x012b }, // LATIN SMALL LETTER I WITH MACRON
    { 0x8d, 0x012f }, // LATIN SMALL LETTER I WITH OGONEK
    { 0x8e, 0x00c4 }, // LATIN CAPITAL LETTER A WITH DIAERESIS
    { 0x8f, 0x0104 }, // LATIN CAPITAL LETTER A WITH OGONEK
    { 0x90, 0x0116 }, // LATIN CAPITAL LETTER E WITH DOT ABOVE
    { 0x91, 0x017e }, // LATIN SMALL LETTER Z WITH CARON
    { 0x92, 0x017d }, // LATIN CAPITAL LETTER Z WITH CARON
    { 0x93, 0x00f5 }, // LATIN SMALL LETTER O WITH TILDE
    { 0x95, 0x00d5 }, // LATIN CAPITAL LETTER O WITH TILDE
    { 0x96, 0x016b }, // LATIN SMALL LETTER U WITH MACRON
    { 0x97, 0x0173 }, // LATIN SMALL LETTER U WITH OGONEK
    { 0x98, 0x0123 }, // LATIN SMALL LETTER G WITH CEDILLA
    { 0x9c, 0x013b }, // LATIN CAPITAL LETTER L WITH CEDILLA
    { 0x9d, 0x201e }, // DOUBLE LOW-9 QUOTATION MARK
    { 0x9e, 0x0161 }, // LATIN SMALL LETTER S WITH CARON
    { 0x9f, 0x0160 }, // LATIN CAPITAL LETTER S WITH CARON
    { 0xa0, 0x0100 }, // LATIN CAPITAL LETTER A WITH MACRON
    { 0xa1, 0x012a }, // LATIN CAPITAL LETTER I WITH MACRON
    { 0xa2, 0x0137 }, // LATIN SMALL LETTER K WITH CEDILLA
    { 0xa3, 0x0136 }, // LATIN CAPITAL LETTER K WITH CEDILLA
    { 0xa4, 0x0146 }, // LATIN SMALL LETTER N WITH CEDILLA
    { 0xa5, 0x0145 }, // LATIN CAPITAL LETTER N WITH CEDILLA
    { 0xa6, 0x016a }, // LATIN CAPITAL LETTER U WITH MACRON
    { 0xa7, 0x0172 }, // LATIN CAPITAL LETTER U WITH OGONEK
    { 0xa8, 0x0122 }, // LATIN CAPITAL LETTER G WITH CEDILLA
    { 0xad, 0x012e }, // LATIN CAPITAL LETTER I WITH OGONEK
};

// Czech and Slovak, Kamenicky encoding
// see: https://en.wikipedia.org/wiki/Kamenick%C3%BD_encoding
static const CodePageImport cp_partial_867 = {
    { 0x80, 0x010c }, // LATIN CAPITAL LETTER C WITH CARON
    { 0x83, 0x010f }, // LATIN SMALL LETTER D WITH CARON
    { 0x85, 0x010e }, // LATIN CAPITAL LETTER D WITH CARON
    { 0x86, 0x0164 }, // LATIN CAPITAL LETTER T WITH CARON
    { 0x87, 0x010d }, // LATIN SMALL LETTER C WITH CARON
    { 0x88, 0x011b }, // LATIN SMALL LETTER E WITH CARON
    { 0x89, 0x011a }, // LATIN CAPITAL LETTER E WITH CARON
    { 0x8a, 0x0139 }, // LATIN CAPITAL LETTER L WITH ACUTE
    { 0x8b, 0x00cd }, // LATIN CAPITAL LETTER I WITH ACUTE
    { 0x8c, 0x013e }, // LATIN SMALL LETTER L WITH CARON
    { 0x8d, 0x013a }, // LATIN SMALL LETTER L WITH ACUTE
    { 0x8f, 0x00c1 }, // LATIN CAPITAL LETTER A WITH ACUTE
    { 0x91, 0x017e }, // LATIN SMALL LETTER Z WITH CARON
    { 0x92, 0x017d }, // LATIN CAPITAL LETTER Z WITH CARON
    { 0x95, 0x00d3 }, // LATIN CAPITAL LETTER O WITH ACUTE
    { 0x96, 0x016f }, // LATIN SMALL LETTER U WITH RING ABOVE
    { 0x97, 0x00da }, // LATIN CAPITAL LETTER U WITH ACUTE
    { 0x98, 0x00fd }, // LATIN SMALL LETTER Y WITH ACUTE
    { 0x9b, 0x0160 }, // LATIN CAPITAL LETTER S WITH CARON
    { 0x9c, 0x013d }, // LATIN CAPITAL LETTER L WITH CARON
    { 0x9d, 0x00dd }, // LATIN CAPITAL LETTER Y WITH ACUTE
    { 0x9e, 0x0158 }, // LATIN CAPITAL LETTER R WITH CARON
    { 0x9f, 0x0165 }, // LATIN SMALL LETTER T WITH CARON
    { 0xa4, 0x0148 }, // LATIN SMALL LETTER N WITH CARON
    { 0xa5, 0x0147 }, // LATIN CAPITAL LETTER N WITH CARON
    { 0xa6, 0x016e }, // LATIN CAPITAL LETTER U WITH RING ABOVE
    { 0xa7, 0x00d4 }, // LATIN CAPITAL LETTER O WITH CIRCUMFLEX
    { 0xa8, 0x0161 }, // LATIN SMALL LETTER S WITH CARON
    { 0xa9, 0x0159 }, // LATIN SMALL LETTER R WITH CARON
    { 0xaa, 0x0155 }, // LATIN SMALL LETTER R WITH ACUTE
    { 0xab, 0x0154 }, // LATIN CAPITAL LETTER R WITH ACUTE
    { 0xad, 0x00a7 }, // SECTION SIGN
};

// Cyrillic MIK Bulgarian
// see: https://en.wikipedia.org/wiki/MIK_(character_set)
static const CodePageImport cp_partial_3021 = {
    { 0x80, 0x0410 }, // CYRILLIC CAPITAL LETTER A
    { 0x81, 0x0411 }, // CYRILLIC CAPITAL LETTER BE
    { 0x82, 0x0412 }, // CYRILLIC CAPITAL LETTER VE
    { 0x83, 0x0413 }, // CYRILLIC CAPITAL LETTER GHE
    { 0x84, 0x0414 }, // CYRILLIC CAPITAL LETTER DE
    { 0x85, 0x0415 }, // CYRILLIC CAPITAL LETTER IE
    { 0x86, 0x0416 }, // CYRILLIC CAPITAL LETTER ZHE
    { 0x87, 0x0417 }, // CYRILLIC CAPITAL LETTER ZE
    { 0x88, 0x0418 }, // CYRILLIC CAPITAL LETTER I
    { 0x89, 0x0419 }, // CYRILLIC CAPITAL LETTER SHORT I
    { 0x8a, 0x041a }, // CYRILLIC CAPITAL LETTER KA
    { 0x8b, 0x041b }, // CYRILLIC CAPITAL LETTER EL
    { 0x8c, 0x041c }, // CYRILLIC CAPITAL LETTER EM
    { 0x8d, 0x041d }, // CYRILLIC CAPITAL LETTER EN
    { 0x8e, 0x041e }, // CYRILLIC CAPITAL LETTER O
    { 0x8f, 0x041f }, // CYRILLIC CAPITAL LETTER PE
    { 0x90, 0x0420 }, // CYRILLIC CAPITAL LETTER ER
    { 0x91, 0x0421 }, // CYRILLIC CAPITAL LETTER ES
    { 0x92, 0x0422 }, // CYRILLIC CAPITAL LETTER TE
    { 0x93, 0x0423 }, // CYRILLIC CAPITAL LETTER U
    { 0x94, 0x0424 }, // CYRILLIC CAPITAL LETTER EF
    { 0x95, 0x0425 }, // CYRILLIC CAPITAL LETTER HA
    { 0x96, 0x0426 }, // CYRILLIC CAPITAL LETTER TSE
    { 0x97, 0x0427 }, // CYRILLIC CAPITAL LETTER CHE
    { 0x98, 0x0428 }, // CYRILLIC CAPITAL LETTER SHA
    { 0x99, 0x0429 }, // CYRILLIC CAPITAL LETTER SHCHA
    { 0x9a, 0x042a }, // CYRILLIC CAPITAL LETTER HARD SIGN
    { 0x9b, 0x042b }, // CYRILLIC CAPITAL LETTER YERU
    { 0x9c, 0x042c }, // CYRILLIC CAPITAL LETTER SOFT SIGN
    { 0x9d, 0x042d }, // CYRILLIC CAPITAL LETTER E
    { 0x9e, 0x042e }, // CYRILLIC CAPITAL LETTER YU
    { 0x9f, 0x042f }, // CYRILLIC CAPITAL LETTER YA
    { 0xa0, 0x0430 }, // CYRILLIC SMALL LETTER A
    { 0xa1, 0x0431 }, // CYRILLIC SMALL LETTER BE
    { 0xa2, 0x0432 }, // CYRILLIC SMALL LETTER VE
    { 0xa3, 0x0433 }, // CYRILLIC SMALL LETTER GHE
    { 0xa4, 0x0434 }, // CYRILLIC SMALL LETTER DE
    { 0xa5, 0x0435 }, // CYRILLIC SMALL LETTER IE
    { 0xa6, 0x0436 }, // CYRILLIC SMALL LETTER ZHE
    { 0xa7, 0x0437 }, // CYRILLIC SMALL LETTER ZE
    { 0xa8, 0x0438 }, // CYRILLIC SMALL LETTER I
    { 0xa9, 0x0439 }, // CYRILLIC SMALL LETTER SHORT I
    { 0xaa, 0x043a }, // CYRILLIC SMALL LETTER KA
    { 0xab, 0x043b }, // CYRILLIC SMALL LETTER EL
    { 0xac, 0x043c }, // CYRILLIC SMALL LETTER EM
    { 0xad, 0x043d }, // CYRILLIC SMALL LETTER EN
    { 0xae, 0x043e }, // CYRILLIC SMALL LETTER O
    { 0xaf, 0x043f }, // CYRILLIC SMALL LETTER PE
    { 0xb0, 0x0440 }, // CYRILLIC SMALL LETTER ER
    { 0xb1, 0x0441 }, // CYRILLIC SMALL LETTER ES
    { 0xb2, 0x0442 }, // CYRILLIC SMALL LETTER TE
    { 0xb3, 0x0443 }, // CYRILLIC SMALL LETTER U
    { 0xb4, 0x0444 }, // CYRILLIC SMALL LETTER EF
    { 0xb5, 0x0445 }, // CYRILLIC SMALL LETTER HA
    { 0xb6, 0x0446 }, // CYRILLIC SMALL LETTER TSE
    { 0xb7, 0x0447 }, // CYRILLIC SMALL LETTER CHE
    { 0xb8, 0x0448 }, // CYRILLIC SMALL LETTER SHA
    { 0xb9, 0x0449 }, // CYRILLIC SMALL LETTER SHCHA
    { 0xba, 0x044a }, // CYRILLIC SMALL LETTER HARD SIGN
    { 0xbb, 0x044b }, // CYRILLIC SMALL LETTER YERU
    { 0xbc, 0x044c }, // CYRILLIC SMALL LETTER SOFT SIGN
    { 0xbd, 0x044d }, // CYRILLIC SMALL LETTER E
    { 0xbe, 0x044e }, // CYRILLIC SMALL LETTER YU
    { 0xbf, 0x044f }, // CYRILLIC SMALL LETTER YA
    { 0xc6, 0x2563 }, // BOX DRAWINGS DOUBLE VERTICAL AND LEFT
    { 0xc7, 0x2551 }, // BOX DRAWINGS DOUBLE VERTICAL
    { 0xcf, 0x2510 }, // BOX DRAWINGS LIGHT DOWN AND LEFT
    { 0xd0, 0x2591 }, // LIGHT SHADE
    { 0xd1, 0x2592 }, // MEDIUM SHADE
    { 0xd2, 0x2593 }, // DARK SHADE
    { 0xd3, 0x2502 }, // BOX DRAWINGS LIGHT VERTICAL
    { 0xd4, 0x2524 }, // BOX DRAWINGS LIGHT VERTICAL AND LEFT
    { 0xd5, 0x2116 }, // NUMERO SIGN
    { 0xd6, 0x00d7 }, // MULTIPLICATION SIGN
    { 0xd7, 0x2557 }, // BOX DRAWINGS DOUBLE DOWN AND LEFT
    { 0xd8, 0x255d }, // BOX DRAWINGS DOUBLE UP AND LEFT
};

// Hungarian, CWI-2 encoding
// see: https://en.wikipedia.org/wiki/CWI-2
static const CodePageImport cp_partial_3845 = {
    { 0x8d, 0x00cd }, // LATIN CAPITAL LETTER I WITH ACUTE
    { 0x8f, 0x00c1 }, // LATIN CAPITAL LETTER A WITH ACUTE
    { 0x93, 0x0151 }, // LATIN SMALL LETTER O WITH DOUBLE ACUTE
    { 0x95, 0x00d3 }, // LATIN CAPITAL LETTER O WITH ACUTE
    { 0x96, 0x0171 }, // LATIN SMALL LETTER U WITH DOUBLE ACUTE
    { 0x97, 0x00da }, // LATIN CAPITAL LETTER U WITH ACUTE
    { 0x98, 0x0170 }, // LATIN CAPITAL LETTER U WITH DOUBLE ACUTE
    { 0xa7, 0x0150 }, // LATIN CAPITAL LETTER O WITH DOUBLE ACUTE
};

// Polish - additional characters for Kashubian dialect
// see: https://en.wikipedia.org/wiki/Kashubian_alphabet
static const CodePageImport cp_partial_58335 = {
    { 0x81, 0x00f9 }, // LATIN SMALL LETTER U WITH GRAVE
    { 0x82, 0x00e9 }, // LATIN SMALL LETTER E WITH ACUTE
    { 0x83, 0x00cb }, // LATIN CAPITAL LETTER E WITH DIAERESIS
    { 0x84, 0x00e3 }, // LATIN SMALL LETTER A WITH TILDE
    { 0x89, 0x00eb }, // LATIN SMALL LETTER E WITH DIAERESIS
    { 0x8a, 0x00c9 }, // LATIN CAPITAL LETTER E WITH ACUTE
    { 0x8e, 0x00c3 }, // LATIN CAPITAL LETTER A WITH TILDE
    { 0x93, 0x00f4 }, // LATIN SMALL LETTER O WITH CIRCUMFLEX
    { 0x94, 0x00f2 }, // LATIN SMALL LETTER O WITH GRAVE
    { 0x96, 0x00d4 }, // LATIN CAPITAL LETTER O WITH CIRCUMFLEX
    { 0x99, 0x00d2 }, // LATIN CAPITAL LETTER O WITH GRAVE
    { 0x9a, 0x00d9 }, // LATIN CAPITAL LETTER U WITH GRAVE
};


// ***************************************************************************
// Maping patches to accomodate characters missing in code pages
// ***************************************************************************

// Provide pairs of code points here. Here is how the 'patches' work:
// - if (from code page definition) we know how to map Unicode code point,
//   the patch information is ignored for the given code page
// - if we do not, than we try mapping from the other code point from the
//   same row of the table entry
// - the pairs below are bi-directional; you can reverse the order within
//   pair, and it will have no effect

const CodePagePatches cp_patches = {

    // Greek alphabet characters are the same or very similar as many
    // (usually mathematical) symbols:
    // - U+00DF - LATIN SMALL LETTER SHARP S
    // - U+2211 - N-ARY SUMMATION
    // - U+00B5 - MICRO SIGN
    // - U+2126 - OHM SIGN
    // - U+2208 - ELEMENT OF
    // can be substituted by:
    // - U+03B2 - GREEK SMALL LETTER BETA
    // - U+03A3 - GREEK CAPITAL LETTER SIGMA
    // - U+03BC - GREEK SMALL LETTER MU
    // - U+03A9 - GREEK CAPITAL LETTER OMEGA
    // - U+03B5 - GREEK SMALL LETTER EPSILON

    { 0x00df, 0x03b2 },
    { 0x2211, 0x03a3 },
    { 0x00b5, 0x03bc },
    { 0x2126, 0x03a9 },
    { 0x2208, 0x03b5 },

    // Polish language has 2 characters, which are frequently omitted for
    // code pages, including CP 852:
    // - U+01B5 - LATIN CAPITAL LETTER Z WITH STROKE 
    // - U+01B6 - LATIN SMALL LETTER Z WITH STROKE 
    // They serve exactly the same purpose (tehy are just different graphical
    // representations, rarely used) as the following:
    // - U+017B - LATIN CAPITAL LETTER Z WITH DOT ABOVE
    // - U+017C - LATIN SMALL LETTER Z WITH DOT ABOVE

    { 0x01b5, 0x017b },
    { 0x01b6, 0x017c },

    // Romanian language has 4 characters, which are frequently omitted from
    // code pages, including CP 852:
    // - U+0x0218 - LATIN CAPITAL LETTER S WITH COMMA BELOW
    // - U+0x0219 - LATIN SMALL LETTER S WITH COMMA BELOW
    // - U+0x021A - LATIN CAPITAL LETTER T WITH COMMA BELOW
    // - U+0x021B - LATIN SMALL LETTER T WITH COMMA BELOW
    // Instead, letters with cedilla are being used:
    // - U+0x015E - LATIN CAPITAL LETTER S WITH CEDILLA
    // - U+0x015F - LATIN SMALL LETTER S WITH CEDILLA
    // - U+0x0162 - LATIN CAPITAL LETTER T WITH CEDILLA
    // - U+0x0163 - LATIN SMALL LETTER T WITH CEDILLA
    // For more information see:
    // - https://pl.wikipedia.org/wiki/CP852
    // - https://www.quora.com/What-is-the-difference-between-%C8%98-and-%C5%9E-in-Romanian

    { 0x0218, 0x015e },
    { 0x0219, 0x015f },
    { 0x021a, 0x0162 },
    { 0x021b, 0x0163 },

    // TODO: Consider some appearance patches for Greek alphabet:
    // - letter WITH TONOS <-> letter WITH ACCUTE
    // - Greek alphabet <-> Cyrillic alphabet (some characters matches)
};


// ***************************************************************************
// List of duplicated code pages
// ***************************************************************************

// FreeDOS CPX files contain several duplicated code pages. List them here in
// pairs; whenever the first code page is encountered, it the engine will use
// the second one from the pair

const CodePageDuplicates cp_duplicates = {
    {  790,  667 }, // Polish, Mazovia encoding, with Euro
    {  895,  867 }, // Czech / Slovak, Kamenicky encoding
    { 1118,  774 }, // Lithuanian
    { 1119,  772 }, // Cyrillic Russian and Lithuanian
};


// ***************************************************************************
// Receipts how to construct code page mappings
// ***************************************************************************

// For each code page, provide the list of partial mappings, starting from
// the highest priority one - if more than one mapping provides the Unicode
// code point for the given code page symbol, only the first will be used.

// Note, that FreeDOS code pages sometimes differ slightly from code pages
// of the era (definitions available in 'cp_XXX.h' files), they often add
// Euro sign. Please check (for example, using Dos Navigator built in tool,
// Utilities -> ASCII table) this for each code page you add, this is not
// always properly docummented.

const CodePageMappingReceipt cp_receipts = {

    // United States
    { 437, { &cp_437 } },

    // Polish, Mazovia encoding, with Euro
    { 667, { &cp_euro_ee, &cp_partial_667, &cp_437 } },

    // Polish, mostly 852 compatible, with Euro
    { 668, { &cp_euro_ee, &cp_partial_668, &cp_437 } },

    // Greek-2, with Euro
    { 737, { &cp_euro_ee, &cp_737 } },

    // Baltic, with Euro
    { 770, { &cp_euro_ee, &cp_partial_770, &cp_437 } },

    // Latin-7 (Baltic Rim)
    { 775, { &cp_775 } },

    // Latin-1 (Western European)
    { 850, { &cp_850 } },

    // Latin-2 (Central European), with Euro
    { 852, { &cp_euro_aa, &cp_852 } },

    // Cyrillic South Slavic
    { 855, { &cp_855 } },

    // Hebrew-2
    { 856, { &cp_856 } },

    // Latin-5, with Euro
    { 857, { &cp_euro_d5, &cp_857 } },

    // Latin-1 (Western European), with Euro
    { 858, { &cp_euro_d5, &cp_850 } },

    // Portuguese, with Euro
    { 860, { &cp_euro_ee, &cp_860 } },

    // Icelandic, with Euro
    { 861, { &cp_euro_ee, &cp_861 } },

    // Hebrew
    { 862, { &cp_862 } },

    // Canandian French
    { 863, { &cp_euro_ee, &cp_863 } },

    // Arabic
    { 864, { &cp_864 } },

    // Nordic, with Euro
    { 865, { &cp_euro_ee, &cp_865 } },

    // Cyrillic Russian
    { 866, { &cp_866 } },

    // Czech and Slovak, Kamenicky encoding, with Euro 
    { 867, { &cp_euro_ee, &cp_partial_867, &cp_437 } },

    // Greek
    { 869, { &cp_869 } },

    // Cyrillic South Slavic, with Euro
    { 872, { &cp_euro_cf, &cp_855 } },

    // Cyrillic South Slavic, with Euro
    { 874, { &cp_874 } },

    // Polish, Mazovia encoding, with Euro and PLN
    { 991, { &cp_pln_none, &cp_euro_ee, &cp_partial_667, &cp_437 } },

    // Bulgarian, Cyrillic, MIK encoding, with Euro
    { 3021, { &cp_euro_ee, &cp_partial_3021, &cp_437 } },   

    // Hungarian, CWI-2 encoding, with Euro
    { 3845, { &cp_euro_ee, &cp_partial_3845, &cp_437 } },

    // Polish, for Kashubian dialect, with Euro and PLN, Mazovia based
    { 58335, { &cp_pln_none, &cp_euro_ee, &cp_partial_58335, &cp_partial_667, &cp_437 } },
};


// TODO: Add definitions for the following FreeDOS code pages:
//
// 113   - Yugoslavian Latin
// 771   - Cyrillic Russian and Lithuanian (KBL)
// 772   - Cyrillic Russian and Lithuanian
// 773   - Latin-7 (old standard)
// 774   - Lithuanian
// 777   - Accented Lithuanian (old)
// 778   - Accented Lithuanian
// 808   - Cyrillic Russian with Euro
// 848   - Cyrillic Ukrainian with Euro
// 849   - Cyrillic Belarusian with Euro
// 851   - Greek (old codepage)
// 853   - Latin-3 (Southern European)
// 859   - Latin-9
// 866   - Cyrillic Russian
// 899   - Armenian
// 1116  - Estonian
// 1117  - Latvian
// 1125  - Cyrillic Ukrainian
// 1131  - Cyrillic Belarusian
// 3012  - Cyrillic Russian and Latvian ("RusLat")
// 3846  - Turkish
// 3848  - Brazilian ABICOMP
// 30000 - Saami
// 30001 - Celtic
// 30002 - Cyrillic Tajik
// 30003 - Latin American
// 30004 - Greenlandic
// 30005 - Nigerian
// 30006 - Vietnamese
// 30007 - Latin
// 30008 - Cyrillic Abkhaz and Ossetian
// 30009 - Romani
// 30010 - Cyrillic Gagauz and Moldovan
// 30011 - Cyrillic Russian Southern District
// 30012 - Cyrillic Russian Siberian and Far Eastern Districts
// 30013 - Cyrillic Volga District - Turkic languages
// 30014 - Cyrillic Volga District - Finno-ugric languages
// 30015 - Cyrillic Khanty
// 30016 - Cyrillic Mansi
// 30017 - Cyrillic Northwestern District
// 30018 - Cyrillic Russian and Latin Tatar
// 30019 - Cyrillic Russian and Latin Chechen
// 30020 - Low saxon and frisian
// 30021 - Oceania
// 30022 - Canadian First Nations
// 30023 - Southern Africa
// 30024 - Northern and Eastern Africa
// 30025 - Western Africa
// 30026 - Central Africa
// 30027 - Beninese
// 30028 - Nigerien
// 30029 - Mexican
// 30030 - Mexican II
// 30031 - Latin-4 (Northern European)
// 30032 - Latin-6
// 30034 - Cherokee
// 30033 - Crimean Tatar with Hryvnia
// 30039 - Cyrillic Ukrainian with Hryvnia
// 30040 - Cyrillic Russian with Hryvnia
// 58152 - Cyrillic Kazakh with Euro
// 58210 - Cyrillic Russian and Azeri
// 59829 - Georgian
// 59234 - Cyrillic Tatar
// 60258 - Cyrillic Russian and Latin Azeri
// 60853 - Georgian with capital letters
// 62306 - Cyrillic Uzbek
