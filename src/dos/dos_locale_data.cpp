/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#include "checks.h"
#include "string_utils.h"

CHECK_NARROWING();

using namespace LocaleData;

// NOTE: Locale settings below were selected based on our knowledge and various
// public sources. Since we are only a small group of volunteers and not
// linguists, and there are about 200 recognized countries in the world,
// mistakes and unfortunate choices could happen.
// Sorry for that, we do not mean to offend or discriminate anyone!

// ***************************************************************************
// Data structure methods
// ***************************************************************************

std::string CountryInfoEntry::GetMsgName() const
{
	const std::string base_str = "COUNTRY_NAME_";
	return base_str + country_code;
}

// ***************************************************************************
// Country info - time/date format, currency, etc.
// ***************************************************************************

// clang-format off
const std::map<uint16_t, DosCountry> LocaleData::CodeToCountryCorrectionMap = {
        // Duplicates listed here are mentioned in Ralf Brown's Interrupt List
        // and confirmed by us using different COUNTRY.SYS versions:

        { 35,  DosCountry::Bulgaria },
        { 88,  DosCountry::Taiwan   }, // also Paragon PTS DOS standard code
        { 112, DosCountry::Belarus  }, // from Ralph Brown Interrupt List
        { 384, DosCountry::Croatia  }, // most likely a mistake in MS-DOS 6.22
};
// clang-format on

// clang-format off
const std::map<DosCountry, CountryInfoEntry> LocaleData::CountryInfo = {
	{ DosCountry::International, { "International (English)", "XXA", { // stateless
		{ LocalePeriod::Modern, {
			// C
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 61
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Albania, { "Albania", "ALB", {
		{ LocalePeriod::Modern, {
			// sq_AL
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "L" }, "ALL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 355
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Lek" }, "ALL", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Algeria, { "Algeria", "DZA", {
		{ LocalePeriod::Modern, {
			// fr_DZ
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺟ.", "DA" }, "DZD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 213
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺟ.", "DA" }, "DZD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Arabic, { "Arabic (Middle East)", "XME", { // custom country code
		{ LocalePeriod::Modern, {
			// (common/representative values for Arabic languages)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "¤", "$" }, "USD", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 785
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ", "¤", "$" }, "USD", 3,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Argentina, { "Argentina", "ARG", {
		{ LocalePeriod::Modern, {
			// es_AR
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "ARS", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "ARS", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Armenia, { "Armenia", "ARM", {
		{ LocalePeriod::Modern, {
			// hy_AM
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "֏" }, "AMD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::AsiaEnglish, { "Asia (English)", "XAE", { // custom country code
		{ LocalePeriod::Modern, {
			// en_HK, en_MO, en_IN, en_PK
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "¤", "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 99
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Australia, { "Australia", "AUS", {
		{ LocalePeriod::Modern, {
			// en_AU
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "AU$", "$" }, "AUD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 61
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "AUD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Austria, { "Austria", "AUT", {
		{ LocalePeriod::Modern, {
			// de_AT
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 43
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "öS", "S" }, "ATS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Azerbaijan, { "Azerbaijan", "AZE", {
		{ LocalePeriod::Modern, {
			// az_AZ
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₼" }, "AZN", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Bahrain, { "Bahrain", "BHR", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺑ.", "BD" }, "BHD", 3,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 973
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺑ.", "BD" }, "BHD", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Belarus, { "Belarus", "BLR", {
		{ LocalePeriod::Modern, {
			// be_BY
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Руб", "Br" }, "BYN", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// Windows ME; country 375
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// Start currency from uppercase letter,
			// to match typical MS-DOS style.
			{ "р.", "p." }, "BYB", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Belgium, { "Belgium", "BEL", {
		{ LocalePeriod::Modern, {
			// fr_BE
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 32
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "BF" }, "BEF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Benin, { "Benin", "BEN", {
		{ LocalePeriod::Modern, {
			// fr_BJ
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "FCFA" }, "XOF", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Bolivia, { "Bolivia", "BOL", {
		{ LocalePeriod::Modern, {
			// es_BO
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Bs" }, "BOB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 591
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Bs" }, "BOB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::BosniaLatin, { "Bosnia and Herzegovina (Latin)", "BIH_LAT", {
		{ LocalePeriod::Modern, {
			// bs_BA
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "KM" }, "BAM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 387
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Din" }, "BAD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::BosniaCyrillic, { "Bosnia and Herzegovina (Cyrillic)", "BIH_CYR", {
		{ LocalePeriod::Modern, {
			// bs_BA
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "КМ", "KM" }, "BAM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// PC-DOS 2000; country 388
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Дин", "Din" }, "BAD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma            // list separator
		} }
	} } },
	{ DosCountry::Brazil, { "Brazil", "BRA", {
		{ LocalePeriod::Modern, {
			// pt_BR
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "R$", "$" }, "BRL", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 55
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Cr$" }, "BRR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Bulgaria, { "Bulgaria", "BGR", {
		{ LocalePeriod::Modern, {
			// bg_BG
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// TODO: Bulgaria is expected to switch currency to EUR
                        // soon - adapt this when it happens
			{ "лв.", "lv." }, "BGN", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 359
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "лв.", "lv." }, "BGL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::CanadaEnglish, { "Canada (English)", "CAN_EN", {
		{ LocalePeriod::Modern, {
			// en_CA
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "CAD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 4
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "CAD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::CanadaFrench, { "Canada (French)", "CAN_FR", {
		{ LocalePeriod::Modern, {
			// fr_CA
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "CAD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 2
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "CAD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Chile, { "Chile", "CHL", {
		{ LocalePeriod::Modern, {
			// es_CL
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "CLP", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 56
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "CLP", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::China, { "China", "CHN", {
		{ LocalePeriod::Modern, {
			// zh_CN
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "¥" }, "CNY", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 86
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "¥" }, "CNY", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Colombia, { "Colombia", "COL", {
		{ LocalePeriod::Modern, {
			// es_CO
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Col$", "$" }, "COP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 57
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "C$", "$" }, "COP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::CostaRica, { "Costa Rica", "CRI", {
		{ LocalePeriod::Modern, {
			// es_CR
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₡", "C" }, "CRC", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 506
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "₡", "C" }, "CRC", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Croatia, { "Croatia", "HRV", {
		{ LocalePeriod::Modern, {
			// hr_HR
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 384
			// (most likely MS-DOS used wrong code instead of 385)
			DosDateFormat::DayMonthYear, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Din" }, "HRD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Czechia, { "Czechia", "CZE", {
		{ LocalePeriod::Modern, {
			// cs_CZ
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Kč", "Kc" }, "CZK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 42
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "Kč", "Kc" }, "CZK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Denmark, { "Denmark", "DNK", {
		{ LocalePeriod::Modern, {
			// da_DK
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "DKK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 45
			DosDateFormat::DayMonthYear, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "DKK", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Ecuador, { "Ecuador", "ECU", {
		{ LocalePeriod::Modern, {
			// es_EC
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 593
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Egypt, { "Egypt", "EGY", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺟ.ﻣ.", "£E", "LE" }, "EGP", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 20
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺟ.ﻣ.", "£E", "LE" }, "EGP", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::ElSalvador, { "El Salvador", "SLV", {
		{ LocalePeriod::Modern, {
			// es_SV
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 503
			DosDateFormat::MonthDayYear, LocaleSeparator::Dash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "₡", "C" }, "SVC", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Semicolon          // list separator
		} }
	} } },
	{ DosCountry::Emirates, { "United Arab Emirates", "ARE", {
		{ LocalePeriod::Modern, {
			// en_AE
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.", "DH" }, "AED", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 971
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.", "DH" }, "AED", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Estonia, { "Estonia", "EST", {
		{ LocalePeriod::Modern, {
			// et_EE
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// Windows ME; country 372
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// Contrary to Windows ME results, 'KR' is a typical
			// Estonian kroon currency sign, not '$.'
			{ "KR" }, "EEK", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::FaroeIslands, { "Faroe Islands", "FRO", {
		{ LocalePeriod::Modern, {
			// fo_FO
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "DKK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Finland, { "Finland", "FIN", {
		{ LocalePeriod::Modern, {
			// fi_FI
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 358
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "mk" }, "FIM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::France, { "France", "FRA", {
		{ LocalePeriod::Modern, {
			// fr_FR
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 33
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "F" }, "FRF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Georgia, { "Georgia", "GEO", {
		{ LocalePeriod::Modern, {
			// ka_GE
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₾" }, "GEL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Germany, { "Germany", "DEU", {
		{ LocalePeriod::Modern, {
			// de_DE
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 49
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "DM" }, "DEM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Greece, { "Greece", "GRC", {
		{ LocalePeriod::Modern, {
			// el_GR
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 30
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "₯", "Δρχ", "Dp" }, "GRD", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Guatemala, { "Guatemala", "GTM", {
		{ LocalePeriod::Modern, {
			// es_GT
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Q" }, "GTQ", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 502
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Q" }, "GTQ", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Honduras, { "Honduras", "HND", {
		{ LocalePeriod::Modern, {
			// es_HN
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "L" }, "HNL", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 504
			DosDateFormat::MonthDayYear, LocaleSeparator::Dash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "L" }, "HNL", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::HongKong, { "Hong Kong", "HKG", {
		{ LocalePeriod::Modern, {
			// en_HK, zh_HK
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "HK$", "$" }, "HKD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 852
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "HK$", "$" }, "HKD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Hungary, { "Hungary", "HUN", {
		{ LocalePeriod::Modern, {
			// hu_HU
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Ft" }, "HUF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 36
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "Ft" }, "HUF", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Iceland, { "Iceland", "ISL", {
		{ LocalePeriod::Modern, {
			// is_IS
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "ISK", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 354
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "ISK", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::India, { "India", "IND", {
		{ LocalePeriod::Modern, {
			// hi_IN
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "₹", "Rs" }, "INR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 91
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Rs" }, "INR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Indonesia, { "Indonesia", "IDN", {
		{ LocalePeriod::Modern, {
			// id_ID
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Rp" }, "IDR", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 62
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Rp" }, "IDR", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Ireland, { "Ireland", "IRL", {
		{ LocalePeriod::Modern, {
			// en_IE
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 353
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "IR£" }, "IEP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Israel, { "Israel", "ISR", {
		{ LocalePeriod::Modern, {
			// he_IL
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₪" }, "NIS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 972
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₪", "NIS" }, "NIS", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Italy, { "Italy", "ITA", {
		{ LocalePeriod::Modern, {
			// it_IT
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 39
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "L." }, "ITL", 0,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Japan, { "Japan", "JPN", {
		{ LocalePeriod::Modern, {
			// ja_JP
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "¥" }, "JPY", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 81
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "¥" }, "JPY", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Jordan, { "Jordan", "JOR", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺍ.", "JD" }, "JOD", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 962
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺍ.", "JD" }, "JOD", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Kazakhstan, { "Kazakhstan", "KAZ", {
		{ LocalePeriod::Modern, {
			// kk_KZ
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₸" }, "KZT", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Kuwait, { "Kuwait", "KWT", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﻛ.", "KD" }, "KWD", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 965
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﻛ.", "KD" }, "KWD", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Kyrgyzstan, { "Kyrgyzstan", "KGZ", {
		{ LocalePeriod::Modern, {
			// ky_KG
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "⃀", "сом" }, "KGS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::LatinAmerica, { "Latin America", "XLA", { // custom country code
		{ LocalePeriod::Modern, {
			// es_419
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			//  MS-DOS 6.22; country 3
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Latvia, { "Latvia", "LVA", {
		{ LocalePeriod::Modern, {
			// lv_LV
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 371
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Ls" }, "LVL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Lebanon, { "Lebanon", "LBN", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﻛ.", "LL" }, "LBP", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 961
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﻛ.", "LL" }, "LBP", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Lithuania, { "Lithuania", "LTU", {
		{ LocalePeriod::Modern, {
			// lt_LT
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 370
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Lt" }, "LTL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,       // thousands separator
			LocaleSeparator::Comma,        // decimal separator
			LocaleSeparator::Semicolon     // list separator
		} }
	} } },
	{ DosCountry::Luxembourg, { "Luxembourg", "LUX", {
		{ LocalePeriod::Modern, {
			// lb_LU
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 352
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "F" }, "LUF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Malaysia, { "Malaysia", "MYS", {
		{ LocalePeriod::Modern, {
			// ms_MY
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "RM" }, "MYR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 60
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$", "M$" }, "MYR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Malta, { "Malta", "MLT", {
		{ LocalePeriod::Modern, {
			// mt_MT
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Mexico, { "Mexico", "MEX", {
		{ LocalePeriod::Modern, {
			// es_MX
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Mex$", "$" }, "MXN", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 52
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "N$" }, "MXN", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma            // list separator
		} }
	} } },
	{ DosCountry::Mongolia, { "Mongolia", "MNG", {
		{ LocalePeriod::Modern, {
			// mn_MN
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₮" }, "MNT", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Montenegro, { "Montenegro", "MNE", {
		{ LocalePeriod::Modern, {
			// sr_ME
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 381, but with DM currency
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "DM" }, "DEM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Morocco, { "Morocco", "MAR", {
		{ LocalePeriod::Modern, {
			// fr_MA
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "ﺩ.ﻣ.", "DH" }, "MAD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 212
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "ﺩ.ﻣ.", "DH" }, "MAD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Netherlands, { "Netherlands", "NLD", {
		{ LocalePeriod::Modern, {
			DosDateFormat::DayMonthYear, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 31
			DosDateFormat::DayMonthYear, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "ƒ", "f" }, "NLG", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::NewZealand, { "New Zealand", "NZL", {
		{ LocalePeriod::Modern, {
			// en_NZ
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$" }, "NZD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 64
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "NZD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Nicaragua, { "Nicaragua", "NIC", {
		{ LocalePeriod::Modern, {
			// es_NI
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "C$" }, "NIO", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 505
			DosDateFormat::MonthDayYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$C" }, "NIO", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Niger, { "Niger", "NER", {
		{ LocalePeriod::Modern, {
			// fr_NE
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "FCFA" }, "XOF", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Nigeria, { "Nigeria", "NGA", {
		{ LocalePeriod::Modern, {
			// en_NG
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₦" }, "NGN", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::NorthMacedonia, { "North Macedonia", "MKD", {
		{ LocalePeriod::Modern, {
			// mk_MK
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "ден.", "ден", "den.", "den" }, "MKD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 389
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Ден", "Den" }, "MKD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Norway, { "Norway", "NOR", {
		{ LocalePeriod::Modern, {
			// nn_NO
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "NOK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 47
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "NOK", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Oman, { "Oman", "OMN", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﻋ.", "R.O" }, "OMR", 3,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 968
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﻋ.", "R.O" }, "OMR", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Pakistan, { "Pakistan", "PAK", {
		{ LocalePeriod::Modern, {
			// en_PK
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Rs" }, "PKR", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 92
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Rs" }, "PKR", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Panama, { "Panama", "PAN", {
		{ LocalePeriod::Modern, {
			// es_PA
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "B/." }, "PAB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 507
			DosDateFormat::MonthDayYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "B" }, "PAB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Paraguay, { "Paraguay", "PRY", {
		{ LocalePeriod::Modern, {
			// es_PY
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₲", "Gs." }, "PYG", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 595
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "₲", "G" }, "PYG", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Philippines, { "Philippines", "PHL", {
		{ LocalePeriod::Modern, {
			// fil_PH
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "₱" }, "PHP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Poland, { "Poland", "POL", {
		{ LocalePeriod::Modern, {
			// pl_PL
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// TODO: Support 'zł' symbol from code pages 991 and 58335
			{ "zł", "zl" }, "PLN", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 48
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Zł", "Zl" }, "PLZ", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Portugal, { "Portugal", "PRT", {
		{ LocalePeriod::Modern, {
			// pt_PT
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 351
			DosDateFormat::DayMonthYear, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Esc." }, "PTE", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Qatar, { "Qatar", "QAT", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﻗ.", "QR" }, "QAR", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 974
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﻗ.", "QR" }, "QAR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Romania, { "Romania", "ROU", {
		{ LocalePeriod::Modern, {
			// ro_RO
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// TODO: Romania is expected to switch currency to EUR
                        // soon - adapt this when it happens
			{ "Lei" }, "RON", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 40
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Lei" }, "ROL", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Russia, { "Russia", "RUS", {
		{ LocalePeriod::Modern, {
			// ru_RU
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₽", "руб", "р.", "Rub" }, "RUB", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 7
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₽", "р.", "р.", }, "RUB", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::SaudiArabia, { "Saudi Arabia", "SAU", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﺳ.", "SR" }, "SAR", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 966
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﺳ.", "SR" }, "SAR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Serbia, { "Serbia", "SRB", {
		{ LocalePeriod::Modern, {
			// sr_RS
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "дин", "DIN" }, "RSD", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 381
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Дин", "Din" }, "RSD", 0,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Singapore, { "Singapore", "SGP", {
		{ LocalePeriod::Modern, {
			// ms_SG
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "S$", "$" }, "SGD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 65
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$" }, "SGD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Slovakia, { "Slovakia", "SVK", {
		{ LocalePeriod::Modern, {
			// sk_SK
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 421
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "Sk" }, "SKK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Slovenia, { "Slovenia", "SVN", {
		{ LocalePeriod::Modern, {
			// sl_SI
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 386
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// MS-DOS 6.22 seems to be wrong here, Slovenia used
			// tolars, not dinars; used definition from PC-DOS 2000
			{ }, "SIT", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::SouthAfrica, { "South Africa", "ZAF", {
		{ LocalePeriod::Modern, {
			// af_ZA
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "R" }, "ZAR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 27
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "R" }, "ZAR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::SouthKorea, { "South Korea", "KOR", {
		{ LocalePeriod::Modern, {
			// ko_KR
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₩", "W" }, "KRW", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separatorr
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 82
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// MS-DOS states precision is 2, but Windows ME states
			// it is 0. Given that even back then 1 USD was worth
			// hundreds South Korean wons, 0 is more sane.
			{ "₩", "W" }, "KRW", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Spain, { "Spain", "ESP", {
		{ LocalePeriod::Modern, {
			// es_ES
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 34
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₧", "Pts" }, "ESP ", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Sweden, { "Sweden", "SWE", {
		{ LocalePeriod::Modern, {
			// sv_SE
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "SEK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 46
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "SEK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Switzerland, { "Switzerland", "CHE", {
		{ LocalePeriod::Modern, {
			// de_CH
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Fr." }, "CHF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Apostrophe,    // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 41
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "SFr." }, "CHF", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Apostrophe,    // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Syria, { "Syria", "SYR", {
		{ LocalePeriod::Modern, {
			// fr_SY
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﻟ.ﺳ.", "LS" }, "SYP", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 963
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﻟ.ﺳ.", "LS" }, "SYP", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Tajikistan, { "Tajikistan", "TJK", {
		{ LocalePeriod::Modern, {
			// tg_TJ
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "сом", "SM" }, "TJS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Taiwan, { "Taiwan", "TWN", {
		{ LocalePeriod::Modern, {
			// zh_TW
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "NT$", "NT" }, "TWD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 886
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "NT$", }, "TWD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Thailand, { "Thailand", "THA", {
		{ LocalePeriod::Modern, {
			// th_TH
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "฿", "B" }, "THB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// Windows ME; country 66
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// Windows ME uses dollar symbol for currency, this 
			// looks wrong, or perhaps it is a workaround for some
			// OS limitation
			{ "฿", "B" }, "THB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Tunisia, { "Tunisia", "TUN", {
		{ LocalePeriod::Modern, {
			// fr_TN
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺗ.", "DT" }, "TND", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 216
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺗ.", "DT" }, "TND", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Slash          // list separator
		} }
	} } },
	{ DosCountry::Turkey, { "Turkey", "TUR", {
		{ LocalePeriod::Modern, {
			// tr_TR
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₺", "TL" }, "TRY", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 90
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₺", "TL" }, "TRL", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Turkmenistan, { "Turkmenistan", "TKM", {
		{ LocalePeriod::Modern, {
			// tk_TM
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "m" }, "TMT", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Ukraine, { "Ukraine", "UKR", {
		{ LocalePeriod::Modern, {
			// uk_UA
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₴", "грн", "hrn" }, "UAH", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// FreeDOS 1.3, Windows ME; country 380
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// Start currency from uppercase letter,
			// to match typical MS-DOS style.
			// Windows ME has a strange currency symbol,
			// so the whole format was taken from FreeDOS.
			{ "₴", "Грн", "Hrn" }, "UAH", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::UnitedKingdom, { "United Kingdom", "GBR", {
		{ LocalePeriod::Modern, {
			// en_GB
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "£" }, "GBP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 44
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "£" }, "GBP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::UnitedStates, { "United States", "USA", {
		{ LocalePeriod::Modern, {
			// en_US
			DosDateFormat::MonthDayYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 1
			DosDateFormat::MonthDayYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Uruguay, { "Uruguay", "URY", {
		{ LocalePeriod::Modern, {
			// es_UY
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$U", "$" }, "UYU", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 598
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "NU$", "$" }, "UYU", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Uzbekistan, { "Uzbekistan", "UZB", {
		{ LocalePeriod::Modern, {
			// uz_UZ
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "сўм", "soʻm", "so'm", "som" }, "UZS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Venezuela, { "Venezuela", "VEN", {
		{ LocalePeriod::Modern, {
			// es_VE
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Bs.F" }, "VEF", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 58
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Bs." }, "VEB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Vietnam, { "Vietnam", "VNM", {
		{ LocalePeriod::Modern, {
			// vi_VN
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₫", "đ" }, "VND", 0,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Yemen, { "Yemen", "YEM", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﻱ.", "YRI" }, "YER", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 967
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﻱ.", "YRI" }, "YER", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Yugoslavia, { "Yugoslavia", "YUG", { // obsolete country code
		{ LocalePeriod::Modern, {
			// sr_RS, sr_ME, hr_HR, sk_SK, bs_BA, mk_MK
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "дин.", "дин", "din.", "din" }, "YUM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 38
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Din" }, "YUM", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } }
};
// clang-format on
