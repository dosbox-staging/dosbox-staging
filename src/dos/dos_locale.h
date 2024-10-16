/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_DOS_LOCALE_H
#define DOSBOX_DOS_LOCALE_H

#include "dos_inc.h"
#include "setup.h"

#include <cstdint>
#include <optional>
#include <string>

constexpr uint16_t DefaultCodePage = 437;

// ***************************************************************************
// List of countries
// ***************************************************************************

// Sources of the country numbers:
// - MS-DOS 6.22, COUNTRY.TXT file
// - PC-DOS 2000, HELP COUNTRY command, information table
// - DR-DOS 7.03, HELP, Table 9-2: Country Codes and Code Pages
// - FreeDOS 1.3, country.asm (source code)
// - Paragon PTS DOS 2000 Pro manual
// - OS/2 Warp 4.52, keyboard.pdf file
// - https://en.wikipedia.org/wiki/List_of_country_calling_codes
//   (used for remaining countries, especially where we have keyboard layout)

// clang-format off
//
// TODO: 'Ralph Brown Interrupt List' also mentions countries listed below:
//
// 51  = Peru                          ISO 3166-1: PE
// 53  = Cuba                          ISO 3166-1: CU
// 93  = Afghanistan                   ISO 3166-1: AF
// 94  = Sri Lanka                     ISO 3166-1: LK
// 98  = Iran                          ISO 3166-1: IR
// 218 = Libya                         ISO 3166-1: LY
// 220 = Gambia                        ISO 3166-1: GM
// 221 = Senegal                       ISO 3166-1: SN
// 222 = Maruitania                    ISO 3166-1: MR
// 223 = Mali                          ISO 3166-1: ML
// 224 = African Guinea                ISO 3166-1: GN
// 225 = Ivory Coast                   ISO 3166-1: CI
// 226 = Burkina Faso                  ISO 3166-1: BF, HV (Upper Volta)
// 228 = Togo                          ISO 3166-1: TG
// 230 = Mauritius                     ISO 3166-1: MU
// 231 = Liberia                       ISO 3166-1: LR
// 232 = Sierra Leone                  ISO 3166-1: SL
// 233 = Ghana                         ISO 3166-1: GH
// 235 = Chad                          ISO 3166-1: TD
// 236 = Central African Republic      ISO 3166-1: CF
// 237 = Cameroon                      ISO 3166-1: CM
// 238 = Cabo Verde                    ISO 3166-1: CV
// 239 = Sao Tome and Principe         ISO 3166-1: ST
// 240 = Equatorial Guinea             ISO 3166-1: GQ
// 241 = Gabon                         ISO 3166-1: GA
// 242 = Congo                         ISO 3166-1: CG
// 243 = Zaire                         ISO 3166-1: CD, ZR
// 244 = Angola                        ISO 3166-1: AO
// 245 = Guinea-Bissau                 ISO 3166-1: GW
// 246 = Diego Garcia                  ISO 3166-1: DG
// 247 = Ascension Island              ISO 3166-1: AC
// 248 = Seychelles                    ISO 3166-1: SC
// 249 = Sudan                         ISO 3166-1: SD
// 250 = Rwhanda                       ISO 3166-1: RW
// 251 = Ethiopia                      ISO 3166-1: ET
// 252 = Somalia                       ISO 3166-1: SO
// 253 = Djibouti                      ISO 3166-1: DJ
// 254 = Kenya                         ISO 3166-1: KE
// 255 = Tanzania                      ISO 3166-1: TZ
// 256 = Uganda                        ISO 3166-1: UG
// 257 = Burundi                       ISO 3166-1: BI
// 259 = Mozambique                    ISO 3166-1: MZ
// 260 = Zambia                        ISO 3166-1: ZM
// 261 = Madagascar                    ISO 3166-1: MG
// 262 = Reunion Island                ISO 3166-1: RE
// 263 = Zimbabwe                      ISO 3166-1: ZW, RH (Southern Rhodesia)
// 264 = Namibia                       ISO 3166-1: NA
// 265 = Malawi                        ISO 3166-1: MW
// 266 = Lesotho                       ISO 3166-1: LS
// 267 = Botswana                      ISO 3166-1: BW
// 268 = Eswatini                      ISO 3166-1: SZ (old name: Swaziland)
// 269 = Comoros                       ISO 3166-1: KM
// 270 = Mayotte                       ISO 3166-1: YT
// 290 = St. Helena                    ISO 3166-1: SH, TA (Tristan da Cunha)
// 297 = Aruba                         ISO 3166-1: AW
// 299 = Greenland                     ISO 3166-1: GL
// 350 = Gibraltar                     ISO 3166-1: GI
// 357 = Cyprus                        ISO 3166-1: CY
// 373 = Moldova                       ISO 3166-1: MD
// 500 = Falkland Islands              ISO 3166-1: FK
// 501 = Belize                        ISO 3166-1: BZ
// 508 = St. Pierre and Miquelon       ISO 3166-1: PM
// 509 = Haiti                         ISO 3166-1: HT
// 590 = Guadeloupe                    ISO 3166-1: GP
// 592 = Guyana                        ISO 3166-1: GY
// 594 = French Guiana                 ISO 3166-1: GF
// 596 = Martinique / French Antilles  ISO 3166-1: MQ
// 597 = Suriname                      ISO 3166-1: SR
// 599 = Netherland Antilles           ISO 3166-1: AN
// 670 = Saipan / N. Mariana Island    ISO 3166-1: MP
// 671 = Guam                          ISO 3166-1: GU
// 672 = Norfolk Island (Australia) / Christmas Island/Cocos Islands / Antarctica
//       ISO 3166-1: NF / CX / AQ
// 673 = Brunei                        ISO 3166-1: BN
// 674 = Nauru                         ISO 3166-1: NR
// 675 = Papua New Guinea              ISO 3166-1: PG
// 676 = Tonga Islands                 ISO 3166-1: TO
// 677 = Solomon Islands               ISO 3166-1: SB
// 678 = Vanuatu                       ISO 3166-1: VU, NH (New Hebrides)
// 679 = Fiji                          ISO 3166-1: FJ
// 680 = Palau                         ISO 3166-1: PW
// 681 = Wallis & Futuna               ISO 3166-1: WF
// 682 = Cook Islands                  ISO 3166-1: CK
// 683 = Niue                          ISO 3166-1: NU
// 684 = American Samoa                ISO 3166-1: AS
// 685 = Western Samoa                 ISO 3166-1: WS
// 686 = Kiribati                      ISO 3166-1: KI, CT (Canton and Enderbury Islands)
// 687 = New Caledonia                 ISO 3166-1: NC
// 688 = Tuvalu                        ISO 3166-1: TV
// 689 = French Polynesia              ISO 3166-1: PF
// 690 = Tokealu                       ISO 3166-1: TK
// 691 = Micronesia                    ISO 3166-1: FM
// 692 = Marshall Islands              ISO 3166-1: MH
// 809 = Antigua and Barbuda / Anguilla / Bahamas / Barbados / Bermuda
//       British Virgin Islands / Cayman Islands / Dominica
//       Dominican Republic / Grenada / Jamaica / Montserrat
//       St. Kitts and Nevis / St. Lucia / St. Vincent and Grenadines
//       Trinidad and Tobago / Turks and Caicos
//       ISO 3166-1: AG / AI / BS / BB / BM / VG / KY / DM / DO /
//                   GD / JM / MS / KN / LC / VC / TT / TC
// 850 = North Korea                   ISO 3166-1: KP
// 853 = Macao                         ISO 3166-1: MO
// 855 = Cambodia                      ISO 3166-1: KH
// 856 = Laos                          ISO 3166-1: LA
// 880 = Bangladesh                    ISO 3166-1: BD
// 960 = Maldives                      ISO 3166-1: MV
// 964 = Iraq                          ISO 3166-1: IQ
// 969 = South Yemen                   ISO 3166-1: YD
// 975 = Bhutan                        ISO 3166-1: BT
// 977 = Nepal                         ISO 3166-1: NP
// 995 = Myanmar                       ISO 3166-1: MM, BU (Burma)
//
// TODO: In addition, the following ountries can be identified using host
// interfaces, we should consider adding them, too:
//
// 150 = Europe (English)              ISO 3166-1: UE, EZ, en_??
// 211 = South Sudan                   ISO 3166-1: SS
// 284 = Virgin Islands (British)      ISO 3166-1: VG
// 291 = Eritrea                       ISO 3166-1: ER
// 299 = Greenland                     ISO 3166-1: GL
// 376 = Andorra                       ISO 3166-1: AD
// 377 = Monaco                        ISO 3166-1: MC
// 379 = Vatican                       ISO 3166-1: VA
// 383 = Kosovo                        ISO 3166-1: XK
// 423 = Liechtenstein                 ISO 3166-1: LI
// 599 = Curaçao / Sint Maarten / Bonaire
//       ISO 3166-1: CW / SX / BQ
// 670 = Timor-Leste                   ISO 3166-1: TL, TP (East Timor)
// 787/939 = Puerto Rico               ISO 3166-1: PR
// 970 = Palestine                     ISO 3166-1: PS
//
// TODO: at least MS-DOS 6.22 and Windows ME unofficially support country
// code 711 - not sure what it represents (most likely this is some prototype
// generic support for Euro Area), but it contains the following:
// - date format  / separator             : YearMonthDay / '-'
// - time format / separator              : 24H / ':'
// - currency symbol / precision / format : 'EA$' / 2 / SymbolAmount
// - thousands /decimal / list separator  : '.' / ',' / ','
//
// clang-format on

// clang-format off
enum class DosCountry : uint16_t {
	International  = 0,   // internal, not used by any DOS
	UnitedStates   = 1,   // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2 (*)
	CanadaFrench   = 2,   // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	LatinAmerica   = 3,   // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	CanadaEnglish  = 4,   // MS-DOS,                                   OS/2 (*)
	Russia         = 7,   // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Egypt          = 20,  //                                           OS/2
	SouthAfrica    = 27,  // MS-DOS,                                   OS/2
	Greece         = 30,  // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Netherlands    = 31,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Belgium        = 32,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	France         = 33,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Spain          = 34,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2 (*)
	// 35 - unofficial duplicate of Bulgaria
	Hungary        = 36,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS,          OS/2
	Yugoslavia     = 38,  // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Italy          = 39,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Romania        = 40,  // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Switzerland    = 41,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Czechia        = 42,  // MS-DOS, PC-DOS,                           OS/2 (*)
	Austria        = 43,  // MS-DOS                  FreeDOS,          OS/2
	UnitedKingdom  = 44,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Denmark        = 45,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Sweden         = 46,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Norway         = 47,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Poland         = 48,  // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Germany        = 49,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Mexico         = 52,  // MS-DOS,                                   OS/2
	Argentina      = 54,  // MS-DOS,                 FreeDOS,          OS/2
	Brazil         = 55,  // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Chile          = 56,  // MS-DOS
	Colombia       = 57,  // MS-DOS,                                   OS/2
	Venezuela      = 58,  // MS-DOS,                                   OS/2
	Malaysia       = 60,  // MS-DOS,                 FreeDOS
	Australia      = 61,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS,          OS/2 (*)
	Indonesia      = 62,  //                                           OS/2
	Philippines    = 63,
	NewZealand     = 64,  // MS-DOS,                                   OS/2
	Singapore      = 65,  // MS-DOS,                 FreeDOS,          OS/2
	Thailand       = 66,  // WIN-ME,                                   OS/2
	Kazakhstan     = 77,
	Japan          = 81,  // MS-DOS, PC-DOS,         FreeDOS, Paragon, OS/2
	SouthKorea     = 82,  // MS-DOS,                 FreeDOS, Paragon, OS/2
	Vietnam        = 84,
	China          = 86,  // MS-DOS,                 FreeDOS, Paragon, OS/2
	// 88 - duplicate of Taiwan
	Turkey         = 90,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS,          OS/2
	India          = 91,  // MS-DOS,                 FreeDOS,          OS/2
	Pakistan       = 92,  //                                           OS/2
	AsiaEnglish    = 99,  //                                           OS/2
	Morocco        = 212, //                                           OS/2
	Algeria        = 213, //                                           OS/2
	Tunisia        = 216, //                                           OS/2
	Niger          = 227,
	Benin          = 229,
	Nigeria        = 234,
	FaroeIslands   = 298,
	Portugal       = 351, // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Luxembourg     = 352, //                                           OS/2
	Ireland        = 353, // MS-DOS,                                   OS/2
	Iceland        = 354, // MS-DOS, PC-DOS,                           OS/2
	Albania        = 355, // MS-DOS, PC-DOS,                           OS/2
	Malta          = 356,
	Finland        = 358, // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Bulgaria       = 359, // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Lithuania      = 370, //                                           OS/2
	Latvia         = 371, //                                           OS/2
	Estonia        = 372, // WIN-ME,                                   OS/2
	Armenia        = 374,
	Belarus        = 375, // WIN-ME                  FreeDOS,          OS/2
	Ukraine        = 380, // WIN-ME                  FreeDOS,          OS/2
	Serbia         = 381, // MS-DOS, PC-DOS,         FreeDOS,          OS/2 (*)
	Montenegro     = 382,
	// 384 - duplicate of Croatia
	Croatia        = 385, // MS-DOS, PC-DOS,         FreeDOS,          OS/2 (*)
	Slovenia       = 386, // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	BosniaLatin    = 387, // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	BosniaCyrillic = 388, //         PC-DOS,
	NorthMacedonia = 389, // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Slovakia       = 421, // MS-DOS, PC-DOS,                           OS/2 (*)
        // 422 - used for Slovakia by CP-DOS and OS/2
	Guatemala      = 502, //                                           OS/2
	ElSalvador     = 503, //                                           OS/2
	Honduras       = 504, //                                           OS/2
	Nicaragua      = 505, //                                           OS/2
	CostaRica      = 506, //                                           OS/2
	Panama         = 507, //                                           OS/2
	Bolivia        = 591, //                                           OS/2
	Ecuador        = 593, // MS-DOS,                                   OS/2
	Paraguay       = 595, //                                           OS/2
	Uruguay        = 598, //                                           OS/2
	Arabic         = 785, // MS-DOS,                 FreeDOS, Paragon, OS/2
	HongKong       = 852, // MS-DOS
	Taiwan         = 886, // MS-DOS,                                   OS/2 (*)
	Lebanon        = 961, //                                           OS/2
	Jordan         = 962, //                                           OS/2
	Syria          = 963, //                                           OS/2
	Kuwait         = 965, //                                           OS/2
	SaudiArabia    = 966, //                                           OS/2
	Yemen          = 967, //                                           OS/2
	Oman           = 968, //                                           OS/2
	Emirates       = 971, //                                           OS/2
	Israel         = 972, // MS-DOS,                 FreeDOS, Paragon, OS/2
	Bahrain        = 973, //                                           OS/2
	Qatar          = 974, //                                           OS/2
	Mongolia       = 976,
	Tajikistan     = 992,
	Turkmenistan   = 993,
	Azerbaijan     = 994,
	Georgia        = 995,
	Kyrgyzstan     = 996,
	Uzbekistan     = 998,

	// (*) Remarks:
	// - MS-DOS, PC-DOS, and OS/2 use country code 381 for both Serbia and
	//   Montenegro
	// - MS-DOS and PC-DOS use country code 61 also for International English
	// - PC-DOS uses country code 381 also for Yugoslavia Cyrillic
	// - MS-DOS (contrary to PC-DOS or Windows ME) uses code 384 (not 385)
	//   for Croatia, FreeDOS follows MS-DOS standard - this was most likely
	//   a bug in the OS, Croatia calling code is 385
	// - PC-DOS and OS/2 use country code 421 for Czechia and 422 for Slovakia
	// - FreeDOS uses country code 042 for Czechoslovakia
	// - FreeDOS calls country code 785 Middle-East,
        //   MS-DOS calls it Arabic South
	// - Paragon PTS DOS uses country code 61 only for Australia
	// - Paragon PTS DOS and OS/2 use country code 88 for Taiwan, DOS 6.22
	//   uses this code as a duplicate of 886, probably for compatibility
	// - OS/2 uses country code 1 also for Canada, English, it does not use 4
	// - OS/2 uses country 34 also for Catalunya

	// FreeDOS also supports the following, not yet handled here:
	// - Belgium/Dutch        40032
	// - Belgium/French       41032
	// - Belgium/German       42032
	// - Spain/Spanish        40034
	// - Spain/Catalan        41034
	// - Spain/Gallegan       42034
	// - Spain/Basque         43034
	// - Switzerland/German   40041
	// - Switzerland/French   41041
	// - Switzerland/Italian  42041
};
// clang-format on

enum class LocalePeriod : uint8_t {
	// Tries to follow the host OS locale settings if only possible, gaps
	// are filled-in using the 'Modern' settings.
	Native,

	// Values for Modern period reflect the KDE/Linux system settings and
	// currency information from Wikipedia - they won't produce 100% same
	// result as old MS-DOS systems, but should at least provide reasonably
	// consistent user experience with certain host operating systems.
	// Please provide them for each supported country!
	Modern,

	// Values for Historic period mimic the locale present in (in order of
	// priority): MS-DOS 6.22, Windows ME, PC-DOS 2000, DR-DOS 7.03,
	// FreeDOS. Possibly utilize other software, preferrably from before
	// 2000, or even 1995.
	// Do not omit it if the definitions are the same as in 'Modern', as
	// 'Modern' might easily change in the future.
	// NOTE: If only possible, keep the Euro currency away from Historic!
	Historic
};

enum class LocaleSeparator : uint8_t {
	Space      = 0x20, // ( )
	Apostrophe = 0x27, // (')
	Comma      = 0x2c, // (,)
	Dash       = 0x2d, // (-)
	Period     = 0x2e, // (.)
	Slash      = 0x2f, // (/)
	Colon      = 0x3a, // (:)
	Semicolon  = 0x3b, // (;)
};

struct LocaleInfoEntry {

	DosDateFormat date_format      = {};
	LocaleSeparator date_separator = {};
	DosTimeFormat time_format      = {};
	LocaleSeparator time_separator = {};

	// Several variants of the currency symbol can be provided, ideally
	// start from "best, but few code pages can display it", to "worst,
	// but many code pages can handle it".
	// Add the rarely available symbols even for Historic locales; old code
	// pages won't display them nevertheless and the engine will select
	// the more common one.
	// If the currency symbol is written in a non-Latin script, provide also
	// a Latin version (if there is any); we do not want to display 'ºδ⌠'
	// where the currency was 'Δρχ' (drachma), we really prefer "Dp"
	std::vector<std::string> currency_symbols_utf8 = {};

	// Currency codes are selected based on ISO 4217 standard,
	// they are used as a last resort solution if no UTF-8 encoded symbols
	// can be losslessly converted to current code page.
	std::string currency_code = {};

	uint8_t currency_precision        = {}; // digits after decimal point
	DosCurrencyFormat currency_format = {};

	LocaleSeparator thousands_separator = {};
	LocaleSeparator decimal_separator   = {};

	std::optional<LocaleSeparator> list_separator = {};
};

struct CountryInfoEntry {
	std::string country_name;
	// Country codes are selected based on ISO 3166-1 alpha-3 standard,
	// - if necessary, add a language, i.e.
	//   CAD_FR  = Canada (French)
	// - if necessary, add an alphabet i.e.
	//   BIH_LAT = Bosnia and Herzegovina (Latin)
	std::string country_code;

	std::map<LocalePeriod, LocaleInfoEntry> locale_info;

	std::string GetMsgName() const;
};

enum class KeyboardScript {
	// Latin scripts
	LatinQwerty, /// always keep it the 1st one!
	LatinQwertz,
	LatinAzerty,
	LatinAsertt,
	LatinJcuken,
	LatinUgjrmv,
	LatinColemak,
	LatinDvorak,
	LatinNonStandard,
	// Other scripts
	Arabic,
	Armenian,
	Cherokee,
	Cyrillic,
	CyrillicPhonetic,
	Georgian,
	Greek,
	Hebrew,
	Thai,
	// Markers for comparing
	LastQwertyLikeScript = LatinAsertt,
	LastLatinScript      = LatinNonStandard,
};

struct KeyboardLayoutInfoEntry {
	std::vector<std::string> layout_codes = {};

	std::string layout_name    = {}; // will be shown to the user
	uint16_t default_code_page = {};

	KeyboardScript primary_script                        = {};
	std::map<uint16_t, KeyboardScript> secondary_scripts = {};
	std::map<uint16_t, KeyboardScript> tertiary_scripts  = {};

	std::string GetMsgName() const;
};

enum class CodePageWarning {
	DottedI, // code page puts 'dotted I' in place of the regular ASCII 'I'
};

namespace LocaleData {

extern const std::map<std::string, std::vector<uint16_t>> BundledCpiContent;
extern const std::map<uint16_t, CodePageWarning>          CodePageWarnings;
extern const std::vector<KeyboardLayoutInfoEntry>         KeyboardLayoutInfo;
extern const std::map<uint16_t, DosCountry>               CodeToCountryCorrectionMap;
extern const std::map<DosCountry, CountryInfoEntry>       CountryInfo;

} // namespace LocaleData

// Functions to generate command line help messages

std::string DOS_GenerateListCountriesMessage();
std::string DOS_GenerateListKeyboardLayoutsMessage(const bool for_keyb_command = false);

// Functions for the KEYB command - help output

std::string DOS_GetKeyboardLayoutName(const std::string& layout);
std::string DOS_GetKeyboardScriptName(const KeyboardScript script);
std::string DOS_GetShortcutKeyboardScript1();
std::string DOS_GetShortcutKeyboardScript2();
std::string DOS_GetShortcutKeyboardScript3();
std::optional<KeyboardScript> DOS_GetKeyboardLayoutScript1(const std::string& layout);
std::optional<KeyboardScript> DOS_GetKeyboardLayoutScript2(const std::string& layout,
                                                           const uint16_t code_page);
std::optional<KeyboardScript> DOS_GetKeyboardLayoutScript3(const std::string& layout,
                                                           const uint16_t code_page);

std::optional<CodePageWarning> DOS_GetCodePageWarning(const uint16_t code_page);

// DOS API support

bool DOS_SetCountry(const uint16_t country_id);
uint16_t DOS_GetCountry();

// Misc functions

std::string DOS_GetBundledCpiFileName(const uint16_t code_page);
uint16_t DOS_GetBundledCodePage(const std::string& keyboard_layout);

// Tries to find better code page than the given one. Currently only checks if
// the country uses euro currency - if so, proposes euro variant of the original
// code page, for example 858 instead of 850.
std::vector<uint16_t> DOS_SuggestBetterCodePages(const uint16_t code_page,
                                                 const std::string& keyboard_layout);

// Puts the country-related locale information into DOS memory;
// should be called when code page changes
void DOS_RepopulateCountryInfo();

// Lifecycle

void DOS_Locale_Init(Section* sec);

// Separate function to support '--list-countries' and '--list-layouts' command
// line switches (and possibly others in the future) - they needs translated
// strings, but do not initialize DOSBox fully.
void DOS_Locale_AddMessages();

#endif
