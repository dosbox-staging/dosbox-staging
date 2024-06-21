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

#include "dos_locale.h"

#include <cstring>
#include <map>
#include <vector>

#if defined(WIN32)
#include <windows.h>
#endif

#include "bitops.h"
#include "checks.h"
#include "dos_inc.h"
#include "dos_keyboard_layout.h"
#include "logging.h"
#include "string_utils.h"

CHECK_NARROWING();

// NOTE: Locale settings below were selected based on our knowledge and various
// public sources. Since we are only a small group of volunteers, not even
// the linguists, and there are about 200 recognized countries in the world,
// mistakes and unfortunate choices could happen.
// Sorry for that, we do not mean to offend or discriminate anyone!

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

// TODO: 'Ralph Brown Interrupt List' also mentions countries listed below.
//       Moreover, OS/2 Warp 4.52 contains definitions which can be used for
//       historic settings for several countries.
// 51  = Peru                          53  = Cuba
// 93  = Afghanistan                   94  = Sri Lanka
// 98  = Iran                          112 = Belarus
// 218 = Libya                         220 = Gambia
// 221 = Senegal                       222 = Maruitania
// 223 = Mali                          224 = African Guinea
// 225 = Ivory Coast                   226 = Burkina Faso
// 228 = Togo                          230 = Mauritius
// 231 = Liberia                       232 = Sierra Leone
// 233 = Ghana                         235 = Chad
// 236 = Centra African Republic       237 = Cameroon
// 238 = Cape Verde Islands            239 = Sao Tome and Principe
// 240 = Equatorial Guinea             241 = Gabon
// 242 = Congo                         243 = Zaire
// 244 = Angola                        245 = Guinea-Bissau
// 246 = Diego Garcia                  247 = Ascension Isle
// 248 = Seychelles                    249 = Sudan
// 250 = Rwhanda                       251 = Ethiopia
// 252 = Somalia                       253 = Djibouti
// 254 = Kenya                         255 = Tanzania
// 256 = Uganda                        257 = Burundi
// 259 = Mozambique                    260 = Zambia
// 261 = Madagascar                    262 = Reunion Island
// 263 = Zimbabwe                      264 = Namibia
// 265 = Malawi                        266 = Lesotho
// 267 = Botswana                      268 = Swaziland
// 269 = Comoros                       270 = Mayotte
// 290 = St. Helena                    297 = Aruba
// 299 = Greenland                     350 = Gibraltar
// 357 = Cyprus                        373 = Moldova
// 500 = Falkland Islands              501 = Belize
// 508 = St. Pierre and Miquelon       509 = Haiti
// 590 = Guadeloupe                    592 = Guyana
// 594 = French Guiana                 596 = Martinique / French Antilles
// 597 = Suriname                      599 = Netherland Antilles
// 670 = Saipan / N. Mariana Island    671 = Guam
// 672 = Norfolk Island (Australia) / Christmas Island/Cocos Islands / Antarctica
// 673 = Brunei Darussalam             674 = Nauru
// 675 = Papua New Guinea              676 = Tonga Islands
// 677 = Solomon Islands               678 = Vanuatu
// 679 = Fiji                          680 = Palau
// 681 = Wallis & Futuna               682 = Cook Islands
// 683 = Niue                          684 = American Samoa
// 685 = Western Samoa                 686 = Kiribati
// 687 = New Caledonia                 688 = Tuvalu
// 689 = French Polynesia              690 = Tokealu
// 691 = Micronesia                    692 = Marshall Islands
// 809 = Antigua and Barbuda / Anguilla / Bahamas / Barbados / Bermuda
//       British Virgin Islands / Cayman Islands / Dominica
//       Dominican Republic / Grenada / Jamaica / Montserra
//       St. Kitts and Nevis / St. Lucia / St. Vincent and Grenadines
//       Trinidad and Tobago / Turks and Caicos
// 850 = North Korea                   853 = Macao
// 855 = Cambodia                      856 = Laos
// 880 = Bangladesh                    960 = Maldives
// 964 = Iraq                          969 = Yemen
// 971 = United Arab Emirates          975 = Bhutan
// 977 = Nepal                         995 = Myanmar (Burma)

enum class Country : uint16_t {
	International   = 0,   // internal, not used by any DOS
	UnitedStates    = 1,   // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2 (*)
	CanadaFrench    = 2,   // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	LatinAmerica    = 3,   // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	CanadaEnglish   = 4,   // MS-DOS,                                   OS/2 (*)
	Russia          = 7,   // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Egypt           = 20,  //                                           OS/2
	SouthAfrica     = 27,  // MS-DOS,                                   OS/2
	Greece          = 30,  // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Netherlands     = 31,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Belgium         = 32,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	France          = 33,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Spain           = 34,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2 (*)
	// 35 - unofficial duplicate of Bulgaria
	Hungary         = 36,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS,          OS/2
	Yugoslavia      = 38,  // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Italy           = 39,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Romania         = 40,  // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Switzerland     = 41,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Czechia         = 42,  // MS-DOS, PC-DOS,                           OS/2 (*)
	Austria         = 43,  // MS-DOS                  FreeDOS,          OS/2
	UnitedKingdom   = 44,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Denmark         = 45,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Sweden          = 46,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Norway          = 47,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Poland          = 48,  // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Germany         = 49,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Mexico          = 52,  // MS-DOS,                                   OS/2
	Argentina       = 54,  // MS-DOS,                 FreeDOS,          OS/2
	Brazil          = 55,  // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Chile           = 56,  // MS-DOS
	Colombia        = 57,  // MS-DOS,                                   OS/2
	Venezuela       = 58,  // MS-DOS,                                   OS/2
	Malaysia        = 60,  // MS-DOS,                 FreeDOS
	Australia       = 61,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS,          OS/2 (*)
	Indonesia       = 62,  //                                           OS/2
	Philippines     = 63,
	NewZealand      = 64,  // MS-DOS,                                   OS/2
	Singapore       = 65,  // MS-DOS,                 FreeDOS,          OS/2
	Thailand        = 66,  // WIN-ME,                                   OS/2
	Kazakhstan      = 77,
	Japan           = 81,  // MS-DOS, PC-DOS,         FreeDOS, Paragon, OS/2
	SouthKorea      = 82,  // MS-DOS,                 FreeDOS, Paragon, OS/2
	Vietnam         = 84,
	China           = 86,  // MS-DOS,                 FreeDOS, Paragon, OS/2
	// 88 - duplicate of Taiwan
	Turkey          = 90,  // MS-DOS, PC-DOS, DR-DOS, FreeDOS,          OS/2
	India           = 91,  // MS-DOS,                 FreeDOS,          OS/2
	Pakistan        = 92,  //                                           OS/2
	AsiaEnglish     = 99,  //                                           OS/2
	Morocco         = 212, //                                           OS/2
	Algeria         = 213, //                                           OS/2
	Tunisia         = 216, //                                           OS/2
	Niger           = 227,
	Benin           = 229,
	Nigeria         = 234,
	FaroeIslands    = 298,
	Portugal        = 351, // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Luxembourg      = 352, //                                           OS/2
	Ireland         = 353, // MS-DOS,                                   OS/2
	Iceland         = 354, // MS-DOS, PC-DOS,                           OS/2
	Albania         = 355, // MS-DOS, PC-DOS,                           OS/2
	Malta           = 356,
	Finland         = 358, // MS-DOS, PC-DOS, DR-DOS, FreeDOS, Paragon, OS/2
	Bulgaria        = 359, // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Lithuania       = 370, //                                           OS/2
	Latvia          = 371, //                                           OS/2
	Estonia         = 372, // WIN-ME,                                   OS/2
	Armenia         = 374,
	Belarus         = 375, // WIN-ME                  FreeDOS,          OS/2
	Ukraine         = 380, // WIN-ME                  FreeDOS,          OS/2
	Serbia          = 381, // MS-DOS, PC-DOS,         FreeDOS,          OS/2 (*)
	Montenegro      = 382,
	// 384 - duplicate of Croatia
	Croatia         = 385, // MS-DOS, PC-DOS,         FreeDOS,          OS/2 (*)
	Slovenia        = 386, // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	BosniaLatin     = 387, // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	BosniaCyrillic  = 388, //         PC-DOS,
	NorthMacedonia  = 389, // MS-DOS, PC-DOS,         FreeDOS,          OS/2
	Slovakia        = 421, // MS-DOS, PC-DOS,                           OS/2 (*)
	// 422 - used for Slovakia by CP-DOS and OS/2
        Guatemala       = 502, //                                           OS/2
        ElSalvador      = 503, //                                           OS/2
        Honduras        = 504, //                                           OS/2
        Nicaragua       = 505, //                                           OS/2
        CostaRica       = 506, //                                           OS/2
        Panama          = 507, //                                           OS/2
        Bolivia         = 591, //                                           OS/2
	Ecuador         = 593, // MS-DOS,                                   OS/2
        Paraguay        = 595, //                                           OS/2
        Uruguay         = 598, //                                           OS/2
	Arabic          = 785, // MS-DOS,                 FreeDOS, Paragon, OS/2
	HongKong        = 852, // MS-DOS
	Taiwan          = 886, // MS-DOS,                                   OS/2 (*)
	Lebanon         = 961, //                                           OS/2
	Jordan          = 962, //                                           OS/2
	Syria           = 963, //                                           OS/2
	Kuwait          = 965, //                                           OS/2
	SaudiArabia     = 966, //                                           OS/2
	Yemen           = 967, //                                           OS/2
	Oman            = 968, //                                           OS/2
	Emirates        = 971, //                                           OS/2
	Israel          = 972, // MS-DOS,                 FreeDOS, Paragon, OS/2	
	Bahrain         = 973, //                                           OS/2
	Qatar           = 974, //                                           OS/2
	Mongolia        = 976,
	Tajikistan      = 992,
	Turkmenistan    = 993,
	Azerbaijan      = 994,
	Georgia         = 995,
	Kyrgyzstan      = 996,
	Uzbekistan      = 998,

	// (*) Remarks:
	// - MS-DOS, PC-DOS, and OS/2 use country code 381 for both Serbia and Montenegro
	// - MS-DOS and PC-DOS use country code 61 also for International English
	// - PC-DOS uses country code 381 also for Yugoslavia Cyrillic
	// - MS-DOS (contrary to PC-DOS or Windows ME) uses code 384 (not 385)
	//   for Croatia, FreeDOS follows MS-DOS standard - this was most likely
	//   a bug in the OS, Croatia calling code is 385
	// - PC-DOS and OS/2 use country code 421 for Czechia and 422 for Slovakia
	// - FreeDOS uses country code 042 for Czechoslovakia
	// - FreeDOS calls country code 785 Middle-East, MS-DOS calls it Arabic South
	// - Paragon PTS DOS uses country code 61 only for Australia
	// - Paragon PTS DOS and OS/2 use country code 88 for Taiwan, DOS 6.22 uses
	//   this code as a duplicate of 886, probably for compatibility
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

static const std::map<uint16_t, Country> CodeToCountryCorrectionMap = {
        // Duplicates listed here are mentioned in Ralf Brown's Interrupt List
        // and confirmed by us using different COUNTRY.SYS versions:

	// clang-format off
        { 35,  Country::Bulgaria },
        { 88,  Country::Taiwan   }, // also Paragon PTS DOS standard code
        { 384, Country::Croatia  }, // most likely a mistake in MS-DOS 6.22
	// clang-format on
};

// TODO: at least MS-DOS 6.22 and Windows ME unofficially support
//       country code 711 - not sure what it represents:
//       - date format  / separator             : YearMonthDay / '-'
//       - time format / separator              : 24H / ':'
//       - currency symbol / precision / format : 'EA$' / 2 / SymbolAmount
//       - thousands /decimal / list separator  : '.' / ',' / ','

// ***************************************************************************
// Country info - time/date format, currency, etc.
// ***************************************************************************

enum class LocalePeriod : uint8_t {
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

enum class Separator : uint8_t {
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
	DosDateFormat date_format;
	Separator     date_separator;
	DosTimeFormat time_format;
	Separator     time_separator;

	// Several variants of the currency symbol can be provided, ideally
	// start from "best, but few code pages can display it", to "worst,
	// but many code pages can handle it".
	// Add the rarely available symbols even for Historic locales; old code
	// pages won't display them nevertheless and the engine will select
	// the more common one.
	// If the currency symbol is written in a non-Latin script, provide also
	// a Latin version (if there is any); we do not want to display 'ºδ⌠'
	// where the currency was 'Δρχ' (drachma), we really prefer "Dp"
	std::vector<std::string> currency_symbols_utf8;

	// Currency codes are selected based on ISO 4217 standard,
	// they are used as a last resort solution if no UTF-8 encoded symbols
	// can be losslessly converted to current code page.
	std::string currency_code;

	uint8_t currency_precision; // digits after decimal point
	DosCurrencyFormat currenct_format;

	Separator thousands_separator;
	Separator decimal_separator;

	// Current locale systems (like Unicode CLDR) do not seem to specify
	// list separators anymore. On 'https://answers.microsoft.com' one can
	// find a question:
	// "Why does Excel seem to use ; and , differently per localization?""
	// And the top answer is:
	// "In countries such as the USA and UK, the comma is used as list
	// separator. (...)
	// In countries that use comma as decimal separator, such as many
	// continental European countries, using a comma both as decimal
	// separator and as list separator would be very confusing: does 3,5
	// mean the numbers 3 and 5, or does it mean 3 and 5/10?
	// So in such countries, the semi-colon ; is used as list separator".
	//
	// https://answers.microsoft.com/en-us/msoffice/forum/all/why-does-excel-seem-to-use-and-differently-per/6467032f-43a0-4343-bae7-af8853fec754
	//
	// Let's use this algorithm to determine default list separator.

	Separator list_separator = (thousands_separator != Separator::Comma &&
	                            decimal_separator != Separator::Comma)
	                                 ? Separator::Comma
	                                 : Separator::Semicolon;
};

struct CountryInfo {
	std::string country_name;
	// Country codes are selected based on ISO 3166-1 alpha-3 standard,
	// - if necessary, add a language, i.e.
	//   CAD_FR  = Canada (French)
	// - if necessary, add an alphabet i.e.
	//   BIH_LAT = Bosnia and Herzegovina (Latin)
	std::string country_code;

	std::map<LocalePeriod, LocaleInfoEntry> locale_info;

	std::string GetMsgName() const
	{
		const std::string base_str = "COUNTRY_NAME_";
		return base_str + country_code;
	}
};

static const std::map<Country, CountryInfo> CountryData = {
	// clang-format off
	{ Country::International, { "International (English)", "XXA", { // stateless
		{ LocalePeriod::Modern, {
			// C
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Space,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 61
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Albania, { "Albania", "ALB", {
		{ LocalePeriod::Modern, {
			// sq_AL
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "L" }, "ALL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 355
			DosDateFormat::YearMonthDay, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Lek" }, "ALL", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Algeria, { "Algeria", "DZA", {
		{ LocalePeriod::Modern, {
			// fr_DZ
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ.ﺟ.", "DA" }, "DZD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 213
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ.ﺟ.", "DA" }, "DZD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Arabic, { "Arabic (Middle East)", "XME", { // custom country code
		{ LocalePeriod::Modern, {
			// (common/representative values for Arabic languages)
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "¤", "$" }, "USD", 2,
			DosCurrencyFormat::AmountSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 785
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ", "¤", "$" }, "USD", 3,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Argentina, { "Argentina", "ARG", {
		{ LocalePeriod::Modern, {
			// es_AR
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "ARS", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "ARS", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Armenia, { "Armenia", "ARM", {
		{ LocalePeriod::Modern, {
			// hy_AM
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "֏" }, "AMD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }
	} } },
	{ Country::AsiaEnglish, { "Asia (English)", "XAE", { // custom country code
		{ LocalePeriod::Modern, {
			// en_HK, en_MO, en_IN, en_PK
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "¤", "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 99
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Australia, { "Australia", "AUS", {
		{ LocalePeriod::Modern, {
			// en_AU
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "AU$", "$" }, "AUD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 61
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "AUD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Austria, { "Austria", "AUT", {
		{ LocalePeriod::Modern, {
			// de_AT
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 43
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "öS", "S" }, "ATS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Azerbaijan, { "Azerbaijan", "AZE", {
		{ LocalePeriod::Modern, {
			// az_AZ
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₼" }, "AZN", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }
	} } },
	{ Country::Bahrain, { "Bahrain", "BHR", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ.ﺑ.", "BD" }, "BHD", 3,
			DosCurrencyFormat::AmountSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 973
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ.ﺑ.", "BD" }, "BHD", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Belarus, { "Belarus", "BLR", {
		{ LocalePeriod::Modern, {
			// be_BY
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Руб", "Br" }, "BYN", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// Windows ME; country 375
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			// Start currency from uppercase letter,
			// to match typical MS-DOS style.
			{ "р.", "p." }, "BYB", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Belgium, { "Belgium", "BEL", {
		{ LocalePeriod::Modern, {
			// fr_BE
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 32
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "BF" }, "BEF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Benin, { "Benin", "BEN", {
		{ LocalePeriod::Modern, {
			// fr_BJ
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "FCFA" }, "XOF", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }
	} } },
	{ Country::Bolivia, { "Bolivia", "BOL", {
		{ LocalePeriod::Modern, {
			// es_BO
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Bs" }, "BOB", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 591
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "Bs" }, "BOB", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::BosniaLatin, { "Bosnia and Herzegovina (Latin)", "BIH_LAT", {
		{ LocalePeriod::Modern, {
			// bs_BA
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "KM" }, "BAM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 387
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Din" }, "BAD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::BosniaCyrillic, { "Bosnia and Herzegovina (Cyrillic)", "BIH_CYR", {
		{ LocalePeriod::Modern, {
			// bs_BA
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "КМ", "KM" }, "BAM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// PC-DOS 2000; country 388
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Дин", "Din" }, "BAD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma            // list separator
		} }
	} } },
	{ Country::Brazil, { "Brazil", "BRA", {
		{ LocalePeriod::Modern, {
			// pt_BR
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "R$", "$" }, "BRL", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 55
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Cr$" }, "BRR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Bulgaria, { "Bulgaria", "BGR", {
		{ LocalePeriod::Modern, {
			// bg_BG
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			// TODO: Bulgaria is expected to switch currency to EUR
			// on 1 January 2025; adapt this for late 2024 release
			{ "лв.", "lv." }, "BGN", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 359
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Period,
			{ "лв.", "lv." }, "BGL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::CanadaEnglish, { "Canada (English)", "CAN_EN", {
		{ LocalePeriod::Modern, {
			// en_CA
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "CAD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 4
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "CAD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::CanadaFrench, { "Canada (French)", "CAN_FR", {
		{ LocalePeriod::Modern, {
			// fr_CA
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "CAD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 2
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "CAD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Chile, { "Chile", "CHL", {
		{ LocalePeriod::Modern, {
			// es_CL
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "CLP", 0,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 56
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "CLP", 0,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::China, { "China", "CHN", {
		{ LocalePeriod::Modern, {
			// zh_CN
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "¥" }, "CNY", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 86
			DosDateFormat::YearMonthDay, Separator::Period,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "¥" }, "CNY", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Colombia, { "Colombia", "COL", {
		{ LocalePeriod::Modern, {
			// es_CO
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "Col$", "$" }, "COP", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 57
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "C$", "$" }, "COP", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::CostaRica, { "Costa Rica", "CRI", {
		{ LocalePeriod::Modern, {
			// es_CR
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₡", "C" }, "CRC", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 506
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "₡", "C" }, "CRC", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Croatia, { "Croatia", "HRV", {
		{ LocalePeriod::Modern, {
			// hr_HR
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 384
			// (most likely MS-DOS used wrong code instead of 385)
			DosDateFormat::DayMonthYear, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Din" }, "HRD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Czechia, { "Czechia", "CZE", {
		{ LocalePeriod::Modern, {
			// cs_CZ
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Kč", "Kc" }, "CZK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 42
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Period,
			{ "Kč", "Kc" }, "CZK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Denmark, { "Denmark", "DNK", {
		{ LocalePeriod::Modern, {
			// da_DK
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "kr" }, "DKK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 45
			DosDateFormat::DayMonthYear, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "kr" }, "DKK", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Ecuador, { "Ecuador", "ECU", {
		{ LocalePeriod::Modern, {
			// es_EC
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 593
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Egypt, { "Egypt", "EGY", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺟ.ﻣ.", "£E", "LE" }, "EGP", 2,
			DosCurrencyFormat::AmountSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 20
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺟ.ﻣ.", "£E", "LE" }, "EGP", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::ElSalvador, { "El Salvador", "SLV", {
		{ LocalePeriod::Modern, {
			// es_SV
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 503
			DosDateFormat::MonthDayYear, Separator::Dash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "₡", "C" }, "SVC", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Semicolon          // list separator
		} }
	} } },
	{ Country::Emirates, { "United Arab Emirates", "ARE", {
		{ LocalePeriod::Modern, {
			// en_AE
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ.", "DH" }, "AED", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 971
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ.", "DH" }, "AED", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Estonia, { "Estonia", "EST", {
		{ LocalePeriod::Modern, {
			// et_EE
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// Windows ME; country 372
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			// Contrary to Windows ME results, 'KR' is a typical
			// Estonian kroon currency sign, not '$.'
			{ "KR" }, "EEK", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::FaroeIslands, { "Faroe Islands", "FRO", {
		{ LocalePeriod::Modern, {
			// fo_FO
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "kr" }, "DKK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }
	} } },
	{ Country::Finland, { "Finland", "FIN", {
		{ LocalePeriod::Modern, {
			// fi_FI
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 358
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Period,
			{ "mk" }, "FIM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::France, { "France", "FRA", {
		{ LocalePeriod::Modern, {
			// fr_FR
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 33
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "F" }, "FRF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Georgia, { "Georgia", "GEO", {
		{ LocalePeriod::Modern, {
			// ka_GE
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₾" }, "GEL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }
	} } },
	{ Country::Germany, { "Germany", "DEU", {
		{ LocalePeriod::Modern, {
			// de_DE
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 49
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "DM" }, "DEM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Greece, { "Greece", "GRC", {
		{ LocalePeriod::Modern, {
			// el_GR
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 30
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "₯", "Δρχ", "Dp" }, "GRD", 2,
			DosCurrencyFormat::AmountSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Guatemala, { "Guatemala", "GTM", {
		{ LocalePeriod::Modern, {
			// es_GT
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Q" }, "GTQ", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 502
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "Q" }, "GTQ", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Honduras, { "Honduras", "HND", {
		{ LocalePeriod::Modern, {
			// es_HN
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "L" }, "HNL", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 504
			DosDateFormat::MonthDayYear, Separator::Dash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "L" }, "HNL", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::HongKong, { "Hong Kong", "HKG", {
		{ LocalePeriod::Modern, {
			// en_HK, zh_HK
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "HK$", "$" }, "HKD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 852
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "HK$", "$" }, "HKD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Hungary, { "Hungary", "HUN", {
		{ LocalePeriod::Modern, {
			// hu_HU
			DosDateFormat::YearMonthDay, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Ft" }, "HUF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 36
			DosDateFormat::YearMonthDay, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Period,
			{ "Ft" }, "HUF", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Iceland, { "Iceland", "ISL", {
		{ LocalePeriod::Modern, {
			// is_IS
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "kr" }, "ISK", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 354
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "kr" }, "ISK", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::India, { "India", "IND", {
		{ LocalePeriod::Modern, {
			// hi_IN
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "₹", "Rs" }, "INR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 91
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "Rs" }, "INR", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Indonesia, { "Indonesia", "IDN", {
		{ LocalePeriod::Modern, {
			// id_ID
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Rp" }, "IDR", 0,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 62
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Rp" }, "IDR", 0,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Ireland, { "Ireland", "IRL", {
		{ LocalePeriod::Modern, {
			// en_IE
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 353
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "IR£" }, "IEP", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Israel, { "Israel", "ISR", {
		{ LocalePeriod::Modern, {
			// he_IL
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₪" }, "NIS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 972
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₪", "NIS" }, "NIS", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Italy, { "Italy", "ITA", {
		{ LocalePeriod::Modern, {
			// it_IT
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 39
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Period,
			{ "L." }, "ITL", 0,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Japan, { "Japan", "JPN", {
		{ LocalePeriod::Modern, {
			// ja_JP
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "¥" }, "JPY", 0,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 81
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "¥" }, "JPY", 0,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Jordan, { "Jordan", "JOR", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ.ﺍ.", "JD" }, "JOD", 2,
			DosCurrencyFormat::AmountSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 962
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ.ﺍ.", "JD" }, "JOD", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Kazakhstan, { "Kazakhstan", "KAZ", {
		{ LocalePeriod::Modern, {
			// kk_KZ
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₸" }, "KZT", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }
	} } },
	{ Country::Kuwait, { "Kuwait", "KWT", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ.ﻛ.", "KD" }, "KWD", 2,
			DosCurrencyFormat::AmountSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 965
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ.ﻛ.", "KD" }, "KWD", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Kyrgyzstan, { "Kyrgyzstan", "KGZ", {
		{ LocalePeriod::Modern, {
			// ky_KG
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "⃀", "сом" }, "KGS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }
	} } },
	{ Country::LatinAmerica, { "Latin America", "XLA", { // custom country code
		{ LocalePeriod::Modern, {
			// es_419
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			//  MS-DOS 6.22; country 3
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Latvia, { "Latvia", "LVA", {
		{ LocalePeriod::Modern, {
			// lv_LV
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 371
			DosDateFormat::YearMonthDay, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Ls" }, "LVL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Lebanon, { "Lebanon", "LBN", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ.ﻛ.", "LL" }, "LBP", 2,
			DosCurrencyFormat::AmountSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 961
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ.ﻛ.", "LL" }, "LBP", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Lithuania, { "Lithuania", "LTU", {
		{ LocalePeriod::Modern, {
			// lt_LT
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 370
			DosDateFormat::YearMonthDay, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Lt" }, "LTL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,       // thousands separator
			Separator::Comma,        // decimal separator
			Separator::Semicolon     // list separator
		} }
	} } },
	{ Country::Luxembourg, { "Luxembourg", "LUX", {
		{ LocalePeriod::Modern, {
			// lb_LU
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 352
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "F" }, "LUF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Malaysia, { "Malaysia", "MYS", {
		{ LocalePeriod::Modern, {
			// ms_MY
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "RM" }, "MYR", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 60
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "$", "M$" }, "MYR", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Malta, { "Malta", "MLT", {
		{ LocalePeriod::Modern, {
			// mt_MT
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }
	} } },
	{ Country::Mexico, { "Mexico", "MEX", {
		{ LocalePeriod::Modern, {
			// es_MX
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Mex$", "$" }, "MXN", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 52
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "N$" }, "MXN", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma            // list separator
		} }
	} } },
	{ Country::Mongolia, { "Mongolia", "MNG", {
		{ LocalePeriod::Modern, {
			// mn_MN
			DosDateFormat::YearMonthDay, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₮" }, "MNT", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }
	} } },
	{ Country::Montenegro, { "Montenegro", "MNE", {
		{ LocalePeriod::Modern, {
			// sr_ME
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 381, but with DM currency
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "DM" }, "DEM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Morocco, { "Morocco", "MAR", {
		{ LocalePeriod::Modern, {
			// fr_MA
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "ﺩ.ﻣ.", "DH" }, "MAD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 212
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "ﺩ.ﻣ.", "DH" }, "MAD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Netherlands, { "Netherlands", "NLD", {
		{ LocalePeriod::Modern, {
			DosDateFormat::DayMonthYear, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 31
			DosDateFormat::DayMonthYear, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "ƒ", "f" }, "NLG", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::NewZealand, { "New Zealand", "NZL", {
		{ LocalePeriod::Modern, {
			// en_NZ
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "$" }, "NZD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 64
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$" }, "NZD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Nicaragua, { "Nicaragua", "NIC", {
		{ LocalePeriod::Modern, {
			// es_NI
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "C$" }, "NIO", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 505
			DosDateFormat::MonthDayYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "$C" }, "NIO", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Niger, { "Niger", "NER", {
		{ LocalePeriod::Modern, {
			// fr_NE
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "FCFA" }, "XOF", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }
	} } },
	{ Country::Nigeria, { "Nigeria", "NGA", {
		{ LocalePeriod::Modern, {
			// en_NG
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₦" }, "NGN", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }
	} } },
	{ Country::NorthMacedonia, { "North Macedonia", "MKD", {
		{ LocalePeriod::Modern, {
			// mk_MK
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "ден.", "ден", "den.", "den" }, "MKD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 389
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Ден", "Den" }, "MKD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Norway, { "Norway", "NOR", {
		{ LocalePeriod::Modern, {
			// nn_NO
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "kr" }, "NOK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 47
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "kr" }, "NOK", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Oman, { "Oman", "OMN", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺭ.ﻋ.", "R.O" }, "OMR", 3,
			DosCurrencyFormat::AmountSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 968
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺭ.ﻋ.", "R.O" }, "OMR", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Pakistan, { "Pakistan", "PAK", {
		{ LocalePeriod::Modern, {
			// en_PK
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "Rs" }, "PKR", 0,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 92
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Rs" }, "PKR", 0,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Panama, { "Panama", "PAN", {
		{ LocalePeriod::Modern, {
			// es_PA
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "B/." }, "PAB", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 507
			DosDateFormat::MonthDayYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "B" }, "PAB", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Paraguay, { "Paraguay", "PRY", {
		{ LocalePeriod::Modern, {
			// es_PY
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₲", "Gs." }, "PYG", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 595
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "₲", "G" }, "PYG", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Philippines, { "Philippines", "PHL", {
		{ LocalePeriod::Modern, {
			// fil_PH
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "₱" }, "PHP", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }
	} } },
	{ Country::Poland, { "Poland", "POL", {
		{ LocalePeriod::Modern, {
			// pl_PL
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			// TODO: Support 'zł' symbol from code pages 991 and 58335
			{ "zł", "zl" }, "PLN", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 48
			DosDateFormat::YearMonthDay, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Zł", "Zl" }, "PLZ", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Portugal, { "Portugal", "PRT", {
		{ LocalePeriod::Modern, {
			// pt_PT
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 351
			DosDateFormat::DayMonthYear, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Esc." }, "PTE", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Qatar, { "Qatar", "QAT", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺭ.ﻗ.", "QR" }, "QAR", 2,
			DosCurrencyFormat::AmountSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 974
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺭ.ﻗ.", "QR" }, "QAR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Romania, { "Romania", "ROU", {
		{ LocalePeriod::Modern, {
			// ro_RO
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			// TODO: Romania is expected to switch currency to EUR
			// on 1 January 2026; adapt this for late 2025 release
			{ "Lei" }, "RON", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 40
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Lei" }, "ROL", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Russia, { "Russia", "RUS", {
		{ LocalePeriod::Modern, {
			// ru_RU
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₽", "руб", "р.", "Rub" }, "RUB", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 7
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₽", "р.", "р.", }, "RUB", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::SaudiArabia, { "Saudi Arabia", "SAU", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺭ.ﺳ.", "SR" }, "SAR", 2,
			DosCurrencyFormat::AmountSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 966
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺭ.ﺳ.", "SR" }, "SAR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Serbia, { "Serbia", "SRB", {
		{ LocalePeriod::Modern, {
			// sr_RS
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "дин", "DIN" }, "RSD", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 381
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Дин", "Din" }, "RSD", 0,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Singapore, { "Singapore", "SGP", {
		{ LocalePeriod::Modern, {
			// ms_SG
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "S$", "$" }, "SGD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 65
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "$" }, "SGD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Slovakia, { "Slovakia", "SVK", {
		{ LocalePeriod::Modern, {
			// sk_SK
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 421
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Period,
			{ "Sk" }, "SKK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Slovenia, { "Slovenia", "SVN", {
		{ LocalePeriod::Modern, {
			// sl_SI
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 386
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			// MS-DOS 6.22 seems to be wrong here, Slovenia used
			// tolars, not dinars; used definition from PC-DOS 2000
			{ }, "SIT", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::SouthAfrica, { "South Africa", "ZAF", {
		{ LocalePeriod::Modern, {
			// af_ZA
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "R" }, "ZAR", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 27
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "R" }, "ZAR", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::SouthKorea, { "South Korea", "KOR", {
		{ LocalePeriod::Modern, {
			// ko_KR
			DosDateFormat::YearMonthDay, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₩", "W" }, "KRW", 0,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separatorr
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 82
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			// MS-DOS states precision is 2, but Windows ME states
			// it is 0. Given that even back then 1 USD was worth
			// hundreds South Korean wons, 0 is more sane.
			{ "₩", "W" }, "KRW", 0,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Spain, { "Spain", "ESP", {
		{ LocalePeriod::Modern, {
			// es_ES
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 34
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₧", "Pts" }, "ESP ", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Sweden, { "Sweden", "SWE", {
		{ LocalePeriod::Modern, {
			// sv_SE
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "kr" }, "SEK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 46
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "kr" }, "SEK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Switzerland, { "Switzerland", "CHE", {
		{ LocalePeriod::Modern, {
			// de_CH
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Fr." }, "CHF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Apostrophe,    // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 41
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "SFr." }, "CHF", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Apostrophe,    // thousands separator
			Separator::Period,        // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Syria, { "Syria", "SYR", {
		{ LocalePeriod::Modern, {
			// fr_SY
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﻟ.ﺳ.", "LS" }, "SYP", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 963
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﻟ.ﺳ.", "LS" }, "SYP", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Tajikistan, { "Tajikistan", "TJK", {
		{ LocalePeriod::Modern, {
			// tg_TJ
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "сом", "SM" }, "TJS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }
	} } },
	{ Country::Taiwan, { "Taiwan", "TWN", {
		{ LocalePeriod::Modern, {
			// zh_TW
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "NT$", "NT" }, "TWD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 886
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "NT$", }, "TWD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Thailand, { "Thailand", "THA", {
		{ LocalePeriod::Modern, {
			// th_TH
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "฿", "B" }, "THB", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// Windows ME; country 66
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			// Windows ME uses dollar symbol for currency, this 
			// looks wrong, or perhaps it is a workaround for some
			// OS limitation
			{ "฿", "B" }, "THB", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Tunisia, { "Tunisia", "TUN", {
		{ LocalePeriod::Modern, {
			// fr_TN
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺩ.ﺗ.", "DT" }, "TND", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 216
			DosDateFormat::YearMonthDay, Separator::Dash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "ﺩ.ﺗ.", "DT" }, "TND", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Slash          // list separator
		} }
	} } },
	{ Country::Turkey, { "Turkey", "TUR", {
		{ LocalePeriod::Modern, {
			// tr_TR
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₺", "TL" }, "TRY", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 90
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₺", "TL" }, "TRL", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Turkmenistan, { "Turkmenistan", "TKM", {
		{ LocalePeriod::Modern, {
			// tk_TM
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "m" }, "TMT", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }
	} } },
	{ Country::Ukraine, { "Ukraine", "UKR", {
		{ LocalePeriod::Modern, {
			// uk_UA
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₴", "грн", "hrn" }, "UAH", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// FreeDOS 1.3, Windows ME; country 380
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			// Start currency from uppercase letter,
			// to match typical MS-DOS style.
			// Windows ME has a strange currency symbol,
			// so the whole format was taken from FreeDOS.
			{ "₴", "Грн", "Hrn" }, "UAH", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::UnitedKingdom, { "United Kingdom", "GBR", {
		{ LocalePeriod::Modern, {
			// en_GB
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "£" }, "GBP", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 44
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "£" }, "GBP", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::UnitedStates, { "United States", "USA", {
		{ LocalePeriod::Modern, {
			// en_US
			DosDateFormat::MonthDayYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 1
			DosDateFormat::MonthDayYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Comma,         // thousands separator
			Separator::Period,        // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Uruguay, { "Uruguay", "URY", {
		{ LocalePeriod::Modern, {
			// es_UY
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "$U", "$" }, "UYU", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 598
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "NU$", "$" }, "UYU", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Uzbekistan, { "Uzbekistan", "UZB", {
		{ LocalePeriod::Modern, {
			// uz_UZ
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "сўм", "soʻm", "so'm", "som" }, "UZS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Space,         // thousands separator
			Separator::Comma,         // decimal separator
		} }
	} } },
	{ Country::Venezuela, { "Venezuela", "VEN", {
		{ LocalePeriod::Modern, {
			// es_VE
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "Bs.F" }, "VEF", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 58
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "Bs." }, "VEB", 2,
			DosCurrencyFormat::SymbolAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } },
	{ Country::Vietnam, { "Vietnam", "VNM", {
		{ LocalePeriod::Modern, {
			// vi_VN
			DosDateFormat::DayMonthYear, Separator::Slash,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "₫", "đ" }, "VND", 0,
			DosCurrencyFormat::AmountSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }
	} } },
	{ Country::Yemen, { "Yemen", "YEM", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺭ.ﻱ.", "YRI" }, "YER", 2,
			DosCurrencyFormat::AmountSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 967
			DosDateFormat::YearMonthDay, Separator::Slash,
			DosTimeFormat::Time12H,      Separator::Colon,
			{ "ﺭ.ﻱ.", "YRI" }, "YER", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Semicolon      // list separator
		} }
	} } },
	{ Country::Yugoslavia, { "Yugoslavia", "YUG", { // obsolete country code
		{ LocalePeriod::Modern, {
			// sr_RS, sr_ME, hr_HR, sk_SK, bs_BA, mk_MK
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "дин.", "дин", "din.", "din" }, "YUM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 38
			DosDateFormat::DayMonthYear, Separator::Period,
			DosTimeFormat::Time24H,      Separator::Colon,
			{ "Din" }, "YUM", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			Separator::Period,        // thousands separator
			Separator::Comma,         // decimal separator
			Separator::Comma          // list separator
		} }
	} } }
	// clang-format on
};

// ***************************************************************************
// Handling DOS country info structure
// ***************************************************************************

static struct {
	LocalePeriod locale_period = LocalePeriod::Modern;

	Country country           = Country::UnitedStates;
	uint16_t country_dos_code = enum_val(country);

	// If the config file settings were read
	bool is_config_loaded = false;

	// If the locale has been generated to DOS table
	bool is_locale_generated = false;

	// If country in the configuration was set to 'auto'
	bool auto_detect_country = false;
} config;

// Offsets to data in DOS country info structure
constexpr size_t InfoOffsetDateFormat         = 0x00;
constexpr size_t InfoOffsetCurrencySymbol     = 0x02;
constexpr size_t InfoOffsetThousandsSeparator = 0x07;
constexpr size_t InfoOffsetDecimalSeparator   = 0x09;
constexpr size_t InfoOffsetDateSeparator      = 0x0b;
constexpr size_t InfoOffsetTimeSeparator      = 0x0d;
constexpr size_t InfoOffsetCurrencyFormat     = 0x0f;
constexpr size_t InfoOffsetCurrencyPrecision  = 0x10;
constexpr size_t InfoOffsetTimeFormat         = 0x11;
// TODO: support constexpr size_t InfoOffsetCasemap = 0x12;
//       move the implementation here from dos_tables.cpp
constexpr size_t InfoOffsetListSeparator = 0x16;
constexpr size_t InfoOffsetReserved      = 0x18;

constexpr size_t MaxCurrencySymbolLength = 4;
constexpr size_t ReservedAreaSize        = 10;

static Country correct_country(const Country country)
{
	// Correct country code to handle duplicates in DOS country numbers
	if (CodeToCountryCorrectionMap.count(enum_val(country))) {
		return CodeToCountryCorrectionMap.at(enum_val(country));
	} else {
		return country;
	}
}

static std::string get_country_name(const Country country)
{
	const auto country_corrected = correct_country(country);

	if (CountryData.count(country_corrected)) {
		return CountryData.at(country_corrected).country_name;
	}

	return "<unknown country id>";
}

std::string DOS_GenerateListCountriesMessage()
{
	std::string message = "\n";
	message += MSG_GetRaw("DOSBOX_HELP_LIST_COUNTRIES_1");
	message += "\n\n";

	for (auto it = CountryData.begin(); it != CountryData.end(); ++it) {
		message += format_str("  %5d - %s\n",
		                         enum_val(it->first),
		                         MSG_GetRaw(it->second.GetMsgName().c_str()));
	}

	message += "\n";
	message += MSG_GetRaw("DOSBOX_HELP_LIST_COUNTRIES_2");
	message += "\n";

	return message;
}

static void set_country_dos_code()
{
	if (config.country == Country::International) {
		// MS-DOS uses the same country code for International English
		// and Australia - we don't, as we have different settings for
		// these. Let's imitate MS-DOS behavior.
		config.country_dos_code = enum_val(Country::Australia);
	} else {
		config.country_dos_code = enum_val(config.country);
	}
}

static void auto_detect_country()
{
	const auto layout = DOS_GetLoadedLayout();
	if (!layout) {
		return;
	}

	uint16_t country_id = {};
	if (!DOS_GetCountryFromLayout(layout, country_id)) {
		LOG_WARNING("DOS: The keyboard layout's country code '%s' "
		            "does not have a corresponding country",
		            layout);
		return;
	}

	config.country = static_cast<Country>(country_id);
}

static void maybe_log_changed_country(const std::string &country_name,
                                      const LocalePeriod actual_period)
{
	static bool already_logged = false;

	static Country logged_country            = {};
	static LocalePeriod logged_real_period   = {};
	static LocalePeriod logged_config_period = {};

	if ((logged_country == config.country) && already_logged &&
	    (logged_real_period == actual_period) &&
	    (logged_config_period == config.locale_period)) {
		return;
	}

	auto period_to_string = [](const LocalePeriod period) {
		if (period == LocalePeriod::Modern) {
			return "modern";
		} else {
			return "historic";
		}
	};

	std::string additional_comment = {};
	if (actual_period != config.locale_period) {
		additional_comment = " (";
		additional_comment += period_to_string(config.locale_period);
		additional_comment += " locale not known)";
	}

	LOG_MSG("DOS: Loaded %s locale for country %d, '%s'%s",
	        period_to_string(actual_period),
	        enum_val(config.country),
	        country_name.c_str(),
	        additional_comment.c_str());

	already_logged       = true;
	logged_country       = config.country;
	logged_real_period   = actual_period;
	logged_config_period = config.locale_period;
}

static void refresh_time_date_format(const LocaleInfoEntry &source)
{
	const auto& destination = dos.tables.country;

	size_t offset = InfoOffsetTimeFormat;
	destination[offset] = enum_val(source.time_format);

	offset = InfoOffsetTimeSeparator;
	destination[offset + 0] = enum_val(source.time_separator);
	destination[offset + 1] = 0;

	offset = InfoOffsetDateFormat;
	destination[offset + 0] = enum_val(source.date_format);
	destination[offset + 1] = 0;

	offset = InfoOffsetDateSeparator;
	destination[offset + 0] = enum_val(source.date_separator);
	destination[offset + 1] = 0;

	offset = InfoOffsetThousandsSeparator;
	destination[offset + 0] = enum_val(source.thousands_separator);
	destination[offset + 1] = 0;

	offset = InfoOffsetDecimalSeparator;
	destination[offset + 0] = enum_val(source.decimal_separator);
	destination[offset + 1] = 0;

	offset = InfoOffsetListSeparator;
	destination[offset + 0] = enum_val(source.list_separator);
	destination[offset + 1] = 0;
}

static void refresh_currency_format(const LocaleInfoEntry &source)
{
	const auto& destination = dos.tables.country;

	assert(source.currency_code.size() < MaxCurrencySymbolLength);
	memset(&destination[InfoOffsetCurrencySymbol], 0, MaxCurrencySymbolLength + 1);

	bool found = false;
	for (const auto& candidate_utf8 : source.currency_symbols_utf8) {
		std::string candidate = {};

		// Check if the currency can be converted to current code page

		if (!utf8_to_dos(candidate_utf8,
		                 candidate,
		                 DosStringConvertMode::NoSpecialCharacters,
		                 UnicodeFallback::Null)) {
			continue;
		}
		if (candidate.length() > MaxCurrencySymbolLength) {
			continue;
		}

		found = true;
		for (const auto character : candidate) {
			if (character == 0) {
				found = false;
				break;
			}
		}

		if (found) {
			memcpy(&destination[InfoOffsetCurrencySymbol],
			       candidate.c_str(),
			       candidate.length());
			break;
		}
	}

	size_t offset = InfoOffsetCurrencyFormat;
	destination[offset] = enum_val(source.currenct_format);

	if (!found) {
		// Fallback - use the currency code instead
		memcpy(&destination[InfoOffsetCurrencySymbol],
		       source.currency_code.c_str(),
		       source.currency_code.length());

		// Force separation between symbol and amount
		bit::set(destination[offset], bit::literals::b0);
	}

	offset = InfoOffsetCurrencyPrecision;
	destination[offset] = source.currency_precision;
}

void DOS_RefreshCountryInfo(const bool keyboard_layout_changed)
{
	if (!config.is_config_loaded) {
		return;
	}

	if (config.auto_detect_country && !config.is_locale_generated &&
	    !keyboard_layout_changed) {
		return;
	}

	if (config.auto_detect_country) {
		auto_detect_country();
	}

	set_country_dos_code();
	const auto country_corrected = correct_country(config.country);

	assert(CountryData.count(country_corrected));
	const auto& country_info = CountryData.at(country_corrected);

	// Select locale period

	LocalePeriod locale_period = LocalePeriod::Modern;
	if (config.locale_period == LocalePeriod::Historic &&
	    country_info.locale_info.count(config.locale_period)) {
		locale_period = config.locale_period;
	}

	assert(country_info.locale_info.count(locale_period));
	const auto& source      = country_info.locale_info.at(locale_period);
	const auto& destination = dos.tables.country;

	// Set reserved/undocumented values to 0's
	memset(&destination[InfoOffsetReserved], 0, ReservedAreaSize);

	// Set time/date/number/list/currency formats
	refresh_time_date_format(source);
	refresh_currency_format(source);

	// Mark locale as generated
	config.is_locale_generated = true;

	// If locale changed, log it
	maybe_log_changed_country(country_info.country_name, locale_period);
}

static bool set_country(const Country country, const bool no_fallback = false)
{
	if (!dos.tables.country) {
		assert(false);
		return false;
	}

	// Validate country ID
	const auto country_corrected = correct_country(country);
	if (!CountryData.count(country_corrected)) {
		if (no_fallback) {
			return false;
		}

		const auto default_country = static_cast<Country>(
		        DOS_GetDefaultCountry());
		LOG_WARNING("DOS: No locale info for country %d, using default %d",
		            enum_val(country),
		            enum_val(default_country));

		if (!CountryData.count(default_country)) {
			assert(false);
			return false;
		}
		config.country = default_country;
	} else {
		config.country = country;
	}

	// Generate country information
	DOS_RefreshCountryInfo();
	return true;
}

bool DOS_SetCountry(const uint16_t country_id)
{
	if (country_id == 0) {
		return false; // for DOS int 21h call this is not valid
	}

	constexpr bool no_fallback = true;
	return set_country(static_cast<Country>(country_id), no_fallback);
}

uint16_t DOS_GetCountry()
{
	return config.country_dos_code;
}

uint16_t DOS_GetDefaultCountry()
{
	if (config.locale_period == LocalePeriod::Historic) {
		return enum_val(Country::UnitedStates);
	} else {
		return enum_val(Country::International);
	}
}

// ***************************************************************************
// Autodetection code
// ***************************************************************************

static const std::map<std::string, Country> LayoutToCountryMap = {
        // clang-format off
	// reference: https://gitlab.com/FreeDOS/base/keyb_lay/-/blob/master/DOC/KEYB/LAYOUTS/LAYOUTS.TXT
	{ "ar462",  Country::Arabic             },
	{ "ar470",  Country::Arabic             },
	{ "az",     Country::Azerbaijan         },
	{ "ba",     Country::BosniaLatin        },
	{ "be",     Country::Belgium            },
	{ "bg",     Country::Bulgaria           }, // 101-key
	{ "bg103",  Country::Bulgaria           }, // 101-key, Phonetic
	{ "bg241",  Country::Bulgaria           }, // 102-key
	{ "bl",     Country::Belarus            },
	{ "bn",     Country::Benin              },
	{ "br",     Country::Brazil             }, // ABNT layout
	{ "br274",  Country::Brazil             }, // US layout
	{ "bx",     Country::Belgium            }, // International
	{ "by",     Country::Belarus            },
	{ "ca",     Country::CanadaEnglish      }, // Standard
	{ "ce",     Country::Russia             }, // Chechnya Standard
	{ "ce443",  Country::Russia             }, // Chechnya Typewriter
	{ "cg",     Country::Montenegro         },
	{ "cf",     Country::CanadaFrench       }, // Standard
	{ "cf445",  Country::CanadaFrench       }, // Dual-layer
	{ "co",     Country::UnitedStates       }, // Colemak
	{ "cz",     Country::Czechia            }, // QWERTY
	{ "cz243",  Country::Czechia            }, // Standard
	{ "cz489",  Country::Czechia            }, // Programmers
	{ "de",     Country::Germany            }, // Standard
	{ "dk",     Country::Denmark            },
	{ "dv",     Country::UnitedStates       }, // Dvorak
	{ "ee",     Country::Estonia            },
	{ "el",     Country::Greece             }, // 319
	{ "es",     Country::Spain              },
	{ "et",     Country::Estonia            },
	{ "fi",     Country::Finland            },
	{ "fo",     Country::FaroeIslands       },
	{ "fr",     Country::France             }, // Standard
	{ "fx",     Country::France             }, // International
	{ "gk",     Country::Greece             }, // 319
	{ "gk220",  Country::Greece             }, // 220
	{ "gk459",  Country::Greece             }, // 101-key
	{ "gr",     Country::Germany            }, // Standard
	{ "gr453",  Country::Germany            }, // Dual-layer
	{ "hr",     Country::Croatia            },
	{ "hu",     Country::Hungary            }, // 101-key
	{ "hu208",  Country::Hungary            }, // 102-key
	{ "hy",     Country::Armenia            },
	{ "il",     Country::Israel             },
	{ "is",     Country::Iceland            }, // 101-key
	{ "is161",  Country::Iceland            }, // 102-key
	{ "it",     Country::Italy              }, // Standard
	{ "it142",  Country::Italy              }, // Comma on Numeric Pad
	{ "ix",     Country::Italy              }, // International
	{ "jp",     Country::Japan              },
	{ "ka",     Country::Georgia            },
	{ "kk",     Country::Kazakhstan         },
	{ "kk476",  Country::Kazakhstan         },
	{ "kx",     Country::UnitedKingdom      }, // International
	{ "ky",     Country::Kyrgyzstan         },
	{ "la",     Country::LatinAmerica       },
	{ "lh",     Country::UnitedStates       }, // Left-Hand Dvorak
	{ "lt",     Country::Lithuania          }, // Baltic
	{ "lt210",  Country::Lithuania          }, // 101-key, Programmers
	{ "lt211",  Country::Lithuania          }, // AZERTY
	{ "lt221",  Country::Lithuania          }, // Standard
	{ "lt456",  Country::Lithuania          }, // Dual-layout
	{ "lv",     Country::Latvia             }, // Standard
	{ "lv455",  Country::Latvia             }, // Dual-layout
	{ "ml",     Country::Malta              }, // UK-based
	{ "mk",     Country::NorthMacedonia     },
	{ "mn",     Country::Mongolia           },
	{ "mo",     Country::Mongolia           },
	{ "mt",     Country::Malta              }, // UK-based
	{ "mt103",  Country::Malta              }, // US-based
	{ "ne",     Country::Niger              },
	{ "ng",     Country::Nigeria            },
	{ "nl",     Country::Netherlands        }, // 102-key
	{ "no",     Country::Norway             },
	{ "ph",     Country::Philippines        },
	{ "pl",     Country::Poland             }, // 101-key, Programmers
	{ "pl214",  Country::Poland             }, // 102-key
	{ "po",     Country::Portugal           },
	{ "px",     Country::Portugal           }, // International
	{ "ro",     Country::Romania            }, // Standard
	{ "ro446",  Country::Romania            }, // QWERTY
	{ "rh",     Country::UnitedStates       }, // Right-Hand Dvorak
	{ "ru",     Country::Russia             }, // Standard
	{ "ru443",  Country::Russia             }, // Typewriter
	{ "rx",     Country::Russia             }, // Extended Standard
	{ "rx443",  Country::Russia             }, // Extended Typewriter
	{ "sd",     Country::Switzerland        }, // German
	{ "sf",     Country::Switzerland        }, // French
	{ "sg",     Country::Switzerland        }, // German
	{ "si",     Country::Slovenia           },
	{ "sk",     Country::Slovakia           },
	{ "sp",     Country::Spain              },
	{ "sq",     Country::Albania            }, // No-deadkeys
	{ "sq448",  Country::Albania            }, // Deadkeys
	{ "sr",     Country::Serbia             }, // Deadkey
	{ "su",     Country::Finland            },
	{ "sv",     Country::Sweden             },
	{ "sx",     Country::Spain              }, // International
	{ "tj",     Country::Tajikistan         },
	{ "tm",     Country::Turkmenistan       },
	{ "tr",     Country::Turkey             }, // QWERTY
	{ "tr440",  Country::Turkey             }, // Non-standard
	{ "tt",     Country::Russia             }, // Tatarstan Standard
	{ "tt443",  Country::Russia             }, // Tatarstan Typewriter
	{ "ua",     Country::Ukraine            }, // 101-key
	{ "uk",     Country::UnitedKingdom      }, // Standard
	{ "uk168",  Country::UnitedKingdom      }, // Alternate
	{ "ur",     Country::Ukraine            }, // 101-key
	{ "ur465",  Country::Ukraine            }, // 101-key
	{ "ur1996", Country::Ukraine            }, // 101-key
	{ "ur2001", Country::Ukraine            }, // 102-key
	{ "ur2007", Country::Ukraine            }, // 102-key
	{ "us",     Country::UnitedStates       }, // Standard
	{ "ux",     Country::UnitedStates       }, // International
	{ "uz",     Country::Uzbekistan         },
	{ "vi",     Country::Vietnam            },
	{ "yc",     Country::Serbia             }, // Deadkey
	{ "yc450",  Country::Serbia             }, // No-deadkey
	{ "yu",     Country::Yugoslavia         },
        // clang-format on
};

static const std::map<std::string, std::string> LanguageToLayoutExceptionMap = {
        {"nl", "us"},
};

std::string DOS_CheckLanguageToLayoutException(const std::string& language_code)
{
	if (!language_code.empty()) {
		const auto it = LanguageToLayoutExceptionMap.find(language_code);
		if (it != LanguageToLayoutExceptionMap.end()) {
			return it->second;
		}
	}
	return language_code;
}

bool DOS_GetCountryFromLayout(const std::string& layout, uint16_t& country)
{
	const auto it = LayoutToCountryMap.find(layout);
	if (it != LayoutToCountryMap.end()) {
		country = enum_val(it->second);
		return true;
	}

	return false;
}

uint16_t DOS_GetCodePageFromCountry(const uint16_t country)
{
	auto assert_code_page = [](const uint16_t code_page) {
		assert(!DOS_GetBundledCodePageFileName(code_page).empty());
		return code_page;
	};

	const auto country_id = static_cast<Country>(country);

	// Countries grouped in ascending order by code_page value
	switch (country_id) {
	case Country::AsiaEnglish:
	case Country::Australia:
	case Country::China:
	case Country::HongKong:
	case Country::India:
	case Country::Indonesia:
	case Country::International:
	case Country::Ireland:
	case Country::Japan:
	case Country::SouthKorea:
	case Country::Malaysia:
	case Country::NewZealand:
	case Country::Singapore:
	case Country::SouthAfrica:
	case Country::Taiwan:
	case Country::Thailand: // only because me are missing Thai CPI files!
	case Country::UnitedKingdom:
	case Country::UnitedStates: return assert_code_page(437);

	// Stripped down 852 variant, just for Polish language, preserves more
	// table drawing characters than 852 [confirmed by a native speaker]
	case Country::Poland: return assert_code_page(668);

	// Note: there seems to be no 774 variant with EUR currency
	case Country::Lithuania: return assert_code_page(774);

	case Country::Argentina:
	case Country::Bolivia:
	case Country::CanadaEnglish:
	case Country::Chile:
	case Country::Colombia:
	case Country::CostaRica:
	case Country::Ecuador:
	case Country::ElSalvador:
	case Country::Guatemala:
	case Country::Honduras:
	case Country::LatinAmerica:
	case Country::Mexico:
	case Country::Nicaragua:
	case Country::Panama:
	case Country::Paraguay:
	case Country::Philippines:
	case Country::Sweden:
	case Country::Switzerland:
	case Country::Uruguay:
	case Country::Venezuela: return assert_code_page(850);

	case Country::Austria:
	case Country::Belgium:
	case Country::Finland:
	case Country::France:
	case Country::Germany:
	case Country::Italy:
	case Country::Luxembourg:
	case Country::Netherlands:
	case Country::Spain:
		if (config.locale_period == LocalePeriod::Modern) {
			return assert_code_page(858); // supports EUR
		} else {
			return assert_code_page(850);
		}

	case Country::Albania:
	case Country::Croatia:
	case Country::Montenegro:
	case Country::Romania:
	case Country::Slovenia:
	case Country::Turkmenistan: return assert_code_page(852);

	case Country::Malta: return assert_code_page(853);

	case Country::BosniaCyrillic:
	case Country::BosniaLatin:
	case Country::NorthMacedonia:
	case Country::Serbia:
	case Country::Yugoslavia: return assert_code_page(855);

	case Country::Turkey: return assert_code_page(857);

	case Country::Brazil:
	// Note: there seems to be no 860 variant with EUR currency
	case Country::Portugal: return assert_code_page(860);

	case Country::FaroeIslands:
	case Country::Iceland: return assert_code_page(861);

	case Country::Israel: return assert_code_page(862);

	case Country::CanadaFrench: return assert_code_page(863);

	case Country::Algeria:
	case Country::Arabic:
	case Country::Bahrain:
	case Country::Egypt:
	case Country::Emirates:
	case Country::Jordan:
	case Country::Kuwait:
	case Country::Lebanon:
	case Country::Morocco:
	case Country::Oman:
	case Country::SaudiArabia:
	case Country::Syria:
	case Country::Tunisia:
	case Country::Pakistan:
	case Country::Qatar:
	case Country::Yemen: return assert_code_page(864);

	case Country::Denmark:
	case Country::Norway: return assert_code_page(865);

	case Country::Russia: return assert_code_page(866);

	// Kamenický encoding
	// Note: there seems to be no 774 variant with EUR currency
	case Country::Czechia:
	case Country::Slovakia: return assert_code_page(867);

	case Country::Greece: return assert_code_page(869); // supports EUR

	case Country::Armenia: return assert_code_page(899);

	// Note: there seems to be no 1116 variant with EUR currency
	case Country::Estonia: return assert_code_page(1116);

	// Note: there seems to be no 1117 variant with EUR currency
	case Country::Latvia: return assert_code_page(1117);

	case Country::Ukraine: return assert_code_page(1125);

	case Country::Belarus: return assert_code_page(1131);

	// MIK encoding [confirmed by a native speaker]
	case Country::Bulgaria: return assert_code_page(3021);

	// CWI-2 encoding
	case Country::Hungary: return assert_code_page(3845);

	case Country::Tajikistan: return assert_code_page(30002);

	case Country::Nigeria: return assert_code_page(30005);

	case Country::Vietnam: return assert_code_page(30006);

	case Country::Benin: return assert_code_page(30027);

	case Country::Niger: return assert_code_page(30028);

	case Country::Kazakhstan:
	case Country::Kyrgyzstan:
	case Country::Mongolia: return assert_code_page(58152);

	case Country::Azerbaijan: return assert_code_page(58210);

	case Country::Georgia: return assert_code_page(59829);

	case Country::Uzbekistan: return assert_code_page(62306);

	default:
		LOG_WARNING("DOS: No default code page for country %d, '%s'",
		            enum_val(country_id),
		            get_country_name(country_id).c_str());
		return assert_code_page(DefaultCodePage437);
	}
}

std::string DOS_GetLayoutFromHost()
{
#if defined(WIN32)
	// Use Windows-specific calls to extract the keyboard layout

	WORD current_kb_layout = LOWORD(GetKeyboardLayout(0));
	WORD current_kb_sub_id = 0;
	char layout_id_string[KL_NAMELENGTH];

	auto parse_hex_string = [](const char* s) {
		uint32_t value = 0;
		sscanf(s, "%x", &value);
		return static_cast<int>(value);
	};

	if (GetKeyboardLayoutName(layout_id_string) &&
	    (safe_strlen(layout_id_string) == 8)) {
		const int current_kb_layout_by_name = parse_hex_string(
		        (char*)&layout_id_string[4]);
		layout_id_string[4] = 0;

		const int sub_id = parse_hex_string((char*)&layout_id_string[0]);

		if ((current_kb_layout_by_name > 0) &&
		    (current_kb_layout_by_name <= UINT16_MAX)) {
			// use layout _id extracted from the layout string
			current_kb_layout = static_cast<WORD>(current_kb_layout_by_name);
		}
		if ((sub_id >= 0) && (sub_id < 100)) {
			// use sublanguage ID extracted from the layout
			// string
			current_kb_sub_id = static_cast<WORD>(sub_id);
		}
	}

	// Try to match emulated keyboard layout with host-keyboardlayout
	switch (current_kb_layout) {
	case 1025:  // Saudi Arabia
	case 1119:  // Tamazight
	case 1120:  // Kashmiri
	case 2049:  // Iraq
	case 3073:  // Egypt
	case 4097:  // Libya
	case 5121:  // Algeria
	case 6145:  // Morocco
	case 7169:  // Tunisia
	case 8193:  // Oman
	case 9217:  // Yemen
	case 10241: // Syria
	case 11265: // Jordan
	case 12289: // Lebanon
	case 13313: // Kuwait
	case 14337: // U.A.E
	case 15361: // Bahrain
	case 16385: // Qatar
		return "ar462";

	case 1026: return "bg";    // Bulgarian
	case 1029: return "cz243"; // Czech
	case 1030: return "dk";    // Danish

	case 2055: // German - Switzerland
	case 3079: // German - Austria
	case 4103: // German - Luxembourg
	case 5127: // German - Liechtenstein
	case 1031: // German - Germany
		return "gr";

	case 1032: return "gk"; // Greek
	case 1034: return "sp"; // Spanish - Spain (Traditional Sort)
	case 1035: return "su"; // Finnish

	case 1036:  // French - France
	case 2060:  // French - Belgium
	case 4108:  // French - Switzerland
	case 5132:  // French - Luxembourg
	case 6156:  // French - Monaco
	case 7180:  // French - West Indies
	case 8204:  // French - Reunion
	case 9228:  // French - Democratic Rep. of Congo
	case 10252: // French - Senegal
	case 11276: // French - Cameroon
	case 12300: // French - Cote d'Ivoire
	case 13324: // French - Mali
	case 14348: // French - Morocco
	case 15372: // French - Haiti
	case 58380: // French - North Africa
		return "fr";

	case 1037: return "il"; // Hebrew
	case 1038: return current_kb_sub_id ? "hu" : "hu208";
	case 1039: return "is161"; // Icelandic

	case 2064: // Italian - Switzerland
	case 1040: // Italian - Italy
		return "it";

	case 3084: return "ca"; // French - Canada
	case 1041: return "jp"; // Japanese

	case 2067: // Dutch - Belgium
	case 1043: // Dutch - Netherlands
		return "nl";

	case 1044: return "no"; // Norwegian (Bokmål)
	case 1045: return "pl"; // Polish
	case 1046: return "br"; // Portuguese - Brazil

	case 2073: // Russian - Moldava
	case 1049: // Russian
		return "ru";

	case 4122: // Croatian (Bosnia/Herzegovina)
	case 1050: // Croatian
		return "hr";

	case 1051: return "sk"; // Slovak
	case 1052: return "sq"; // Albanian - Albania

	case 2077: // Swedish - Finland
	case 1053: // Swedish
		return "sv";

	case 1055: return "tr"; // Turkish
	case 1058: return "ur"; // Ukrainian
	case 1059: return "bl"; // Belarusian
	case 1060: return "si"; // Slovenian
	case 1061: return "et"; // Estonian
	case 1062: return "lv"; // Latvian
	case 1063: return "lt"; // Lithuanian
	case 1064: return "tj"; // Tajik
	case 1066: return "vi"; // Vietnamese
	case 1067: return "hy"; // Armenian - Armenia
	case 1071: return "mk"; // F.Y.R.O. Macedonian
	case 1079: return "ka"; // Georgian
	case 2070: return "po"; // Portuguese - Portugal
	case 2072: return "ro"; // Romanian - Moldava
	case 5146: return "ba"; // Bosnian (Bosnia/Herzegovina)

	case 2058:  // Spanish - Mexico
	case 3082:  // Spanish - Spain (Modern Sort)
	case 4106:  // Spanish - Guatemala
	case 5130:  // Spanish - Costa Rica
	case 6154:  // Spanish - Panama
	case 7178:  // Spanish - Dominican Republic
	case 8202:  // Spanish - Venezuela
	case 9226:  // Spanish - Colombia
	case 10250: // Spanish - Peru
	case 11274: // Spanish - Argentina
	case 12298: // Spanish - Ecuador
	case 13322: // Spanish - Chile
	case 14346: // Spanish - Uruguay
	case 15370: // Spanish - Paraguay
	case 16394: // Spanish - Bolivia
	case 17418: // Spanish - El Salvador
	case 18442: // Spanish - Honduras
	case 19466: // Spanish - Nicaragua
	case 20490: // Spanish - Puerto Rico
	case 21514: // Spanish - United States
	case 58378: // Spanish - Latin America
		return "la";
	}
#endif
	// TODO: On POSIX systems try to check environment variables to get
	//       the language, in order: LANGUAGE, LC_ALL (and others), LANG.
	//       Check LC_MESSAGES, LC_NUMERIC, LC_TIME, LC_MONETARY, even
	//       LC_COLLATE - maybe we should create a 'mixed' locale?
	//       Also check 'get_language_from_os()' from 'cross.cpp'.

	return "";
}

// ***************************************************************************
// Locale retrieval functions
// ***************************************************************************

std::string DOS_GetBundledCodePageFileName(const uint16_t code_page)
{
	// reference:
	// https://gitlab.com/FreeDOS/base/cpidos/-/blob/master/DOC/CPIDOS/CODEPAGE.TXT
	switch (code_page) {
	case 437:
	case 850:
	case 852:
	case 853:
	case 857:
	case 858: return "EGA.CPI";
	case 775:
	case 859:
	case 1116:
	case 1117:
	case 1118:
	case 1119: return "EGA2.CPI";
	case 771:
	case 772:
	case 808:
	case 855:
	case 866:
	case 872: return "EGA3.CPI";
	case 848:
	case 849:
	case 1125:
	case 1131:
	case 3012:
	case 30010: return "EGA4.CPI";
	case 113:
	case 737:
	case 851:
	case 869: return "EGA5.CPI";
	case 899:
	case 30008:
	case 58210:
	case 59829:
	case 60258:
	case 60853: return "EGA6.CPI";
	case 30011:
	case 30013:
	case 30014:
	case 30017:
	case 30018:
	case 30019: return "EGA7.CPI";
	case 770:
	case 773:
	case 774:
	case 777:
	case 778: return "EGA8.CPI";
	case 860:
	case 861:
	case 863:
	case 865:
	case 867: return "EGA9.CPI";
	case 667:
	case 668:
	case 790:
	case 991:
	case 3845: return "EGA10.CPI";
	case 30000:
	case 30001:
	case 30004:
	case 30007:
	case 30009: return "EGA11.CPI";
	case 30003:
	case 30029:
	case 30030:
	case 58335: return "EGA12.CPI";
	case 895:
	case 30002:
	case 58152:
	case 59234:
	case 62306: return "EGA13.CPI";
	case 30006:
	case 30012:
	case 30015:
	case 30016:
	case 30020:
	case 30021: return "EGA14.CPI";
	case 30023:
	case 30024:
	case 30025:
	case 30026:
	case 30027:
	case 30028: return "EGA15.CPI";
	case 3021:
	case 30005:
	case 30022:
	case 30031:
	case 30032: return "EGA16.CPI";
	case 862:
	case 864:
	case 30034:
	case 30033:
	case 30039:
	case 30040: return "EGA17.CPI";
	case 856:
	case 3846:
	case 3848: return "EGA18.CPI";
	default: return {}; // none
	}
}

DosDateFormat DOS_GetLocaleDateFormat()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetDateFormat;
	return static_cast<DosDateFormat>(dos.tables.country[offset]);
}

DosTimeFormat DOS_GetLocaleTimeFormat()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetTimeFormat;
	return static_cast<DosTimeFormat>(dos.tables.country[offset]);
}

char DOS_GetLocaleDateSeparator()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetDateSeparator;
	return static_cast<char>(dos.tables.country[offset]);
}

char DOS_GetLocaleTimeSeparator()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetTimeSeparator;
	return static_cast<char>(dos.tables.country[offset]);
}

char DOS_GetLocaleThousandsSeparator()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetThousandsSeparator;
	return static_cast<char>(dos.tables.country[offset]);
}

char DOS_GetLocaleDecimalSeparator()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetDecimalSeparator;
	return static_cast<char>(dos.tables.country[offset]);
}

char DOS_GetLocaleListSeparator()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetListSeparator;
	return static_cast<char>(dos.tables.country[offset]);
}

// ***************************************************************************
// Lifecycle
// ***************************************************************************

class DOS_Locale final : public Module_base {
public:
	DOS_Locale(Section* configuration) : Module_base(configuration)
	{
		auto section = static_cast<Section_prop*>(configuration);
		assert(section);

		const auto locale_period = section->Get_string("locale_period");
		const auto country_code  = section->Get_string("country");

		if (locale_period == "modern") {
			config.locale_period = LocalePeriod::Modern;
		} else if (locale_period == "historic") {
			config.locale_period = LocalePeriod::Historic;
		} else {
			assert(false);
		}

		config.is_config_loaded = true;

		if (country_code == "auto") {
			config.auto_detect_country = true;
			// Autoselection is going to be performed once
			// keyboard layout is loaded
		} else {
			config.auto_detect_country = false;
			const auto property_value  = parse_int(country_code);
			Country country            = {};
			if (!property_value || (*property_value < 0) ||
			    (*property_value > UINT16_MAX)) {
				LOG_WARNING("DOS: '%s' is not a valid country code, using default",
				            country_code.c_str());
				// NOTE: Real MS-DOS 6.22 uses modified locale
				// in this case, it uses country 1 with date
				// separator '-' instead of '/'.
				// This is not simulated, I believe it is due to
				// hardcoded locale being slightly different
				// than the one from COUNTRY.SYS (MS-DOS bug)
				country = static_cast<Country>(DOS_GetDefaultCountry());
			} else {
				country = static_cast<Country>(*property_value);
			}
			set_country(country);
		}
	}

	~DOS_Locale() {}
};

static std::unique_ptr<DOS_Locale> Locale = {};

void DOS_Locale_ShutDown(Section*)
{
	Locale.reset();
}

void DOS_Locale_Init(Section* sec)
{
	assert(sec);
	Locale = std::make_unique<DOS_Locale>(sec);

	constexpr auto changeable_at_runtime = true;
	sec->AddDestroyFunction(&DOS_Locale_ShutDown, changeable_at_runtime);
}

void DOS_Locale_AddMessages()
{
	MSG_Add("DOSBOX_HELP_LIST_COUNTRIES_1",
	        "List of country codes (mostly same as telephone call codes)\n"
	        "-----------------------------------------------------------");
	MSG_Add("DOSBOX_HELP_LIST_COUNTRIES_2",
	        "The above numeric country codes can be used exactly as listed\n"
	        "in the 'country' config setting.");

	// Add strings with country names
	for (auto it = CountryData.begin(); it != CountryData.end(); ++it) {
		MSG_Add(it->second.GetMsgName().c_str(),
		        it->second.country_name.c_str());
	}
}
