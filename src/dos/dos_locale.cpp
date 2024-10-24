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

#include "../misc/host_locale.h"
#include "ansi_code_markup.h"
#include "bitops.h"
#include "checks.h"
#include "dos_keyboard_layout.h"
#include "dos_locale.h"
#include "logging.h"
#include "string_utils.h"
#include "unicode.h"

CHECK_NARROWING();

// ***************************************************************************
// Handling DOS country info structure
// ***************************************************************************

static struct {
	// If the config file settings were read
	bool is_config_loaded = false;

	// Settings exactly as retrieved from config file
	std::string language_str = {};
	std::string country_str  = {};
	std::string keyboard_str = {};

	// Config file settings interpretation
	LocalePeriod locale_period = LocalePeriod::Modern;

	// If no country is available, use auto-detected
	std::optional<DosCountry> country = {};
} config;

// Status set when populating DOS data
static struct Populated {
	bool is_country_overriden     = false;
	bool is_using_euro_currency   = false;
	bool is_using_fallback_period = false;
	bool is_using_native_numeric  = false;
	bool is_using_native_time     = false;
	bool is_using_native_date     = false;
	bool is_using_native_currency = false;

	std::optional<DosCountry> separate_numeric   = {};
	std::optional<DosCountry> separate_time_date = {};
	std::optional<DosCountry> separate_currency  = {};

} populated;

// Either set from the configuration, or autodetected
static DosCountry current_country = DosCountry::UnitedStates;
// Country set by guest software via the DOS API
static std::optional<DosCountry> guest_country_override = {};

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
constexpr size_t InfoOffsetListSeparator      = 0x16;
constexpr size_t InfoOffsetReserved           = 0x18;
// TODO: add support for 'constexpr size_t InfoOffsetCasemap = 0x12'

constexpr size_t MaxCurrencySymbolLength = 4;
constexpr size_t MaxCurrencyPrecision    = 4;
constexpr size_t ReservedAreaSize        = 10;

const std::string EuroCurrencyCode = "EUR";

static DosCountry get_default_country()
{
	if (config.locale_period == LocalePeriod::Historic) {
		return DosCountry::UnitedStates;
	} else {
		return DosCountry::International;
	}
}

static DosCountry deduplicate_country(const DosCountry country)
{
	// Correct country code to handle duplicates in DOS country numbers
	if (LocaleData::CodeToCountryCorrectionMap.contains(enum_val(country))) {
		return LocaleData::CodeToCountryCorrectionMap.at(enum_val(country));
	} else {
		return country;
	}
}

static std::string deduplicate_layout(const std::string& layout)
{
	for (const auto& layout_info : LocaleData::KeyboardLayoutInfo) {
		for (const auto& entry : layout_info.layout_codes) {
			if (layout == entry) {
				return layout_info.layout_codes[0];
			}
		}
	}

	return layout;
}

static bool is_country_supported(const DosCountry country)
{
	return LocaleData::CountryInfo.contains(deduplicate_country(country));
}

static std::string get_country_name_for_log(const DosCountry country)
{
	const auto country_deduplicated = deduplicate_country(country);

	if (LocaleData::CountryInfo.contains(country_deduplicated)) {
		return LocaleData::CountryInfo.at(country_deduplicated).country_name;
	}

	return "<unknown country>";
}

static std::string get_locale_period_for_log(const LocalePeriod period)
{
	switch (period) {
	case LocalePeriod::Native:   return "native";
	case LocalePeriod::Modern:   return "modern";
	case LocalePeriod::Historic: return "historic";
	default: assert(false); return {};
	}
}

static LocaleSeparator get_default_list_separator(const LocaleSeparator thousands_separator,
                                                  const LocaleSeparator decimal_separator)
{
	// Current locale systems (like Unicode CLDR) do not seem to specify
        // list separators anymore.
	// On 'https://answers.microsoft.com' one can find a question:
	// "Why does Excel seem to use ; and , differently per localization?"
	// And the top answer is:
	// "In countries such as the USA and UK, the comma is used as list
        // separator. (...) In countries that use comma as decimal separator,
        // such as many continental European countries, using a comma both as
	// decimal separator and as list separator would be very
	// confusing: does 3,5 mean the numbers 3 and 5, or does it mean
	// 3 and 5/10? So, in such countries, the semi-colon ; is use
	// as a list separator".
	// - https://answers.microsoft.com/en-us/msoffice/forum/all/why-does-excel-seem-to-use-and-differently-per/6467032f-43a0-4343-bae7-af8853fec754
	//
	// Let's use this algorithm to determine default list separator.

	if (thousands_separator != LocaleSeparator::Comma &&
	    decimal_separator != LocaleSeparator::Comma) {
		return LocaleSeparator::Comma;
	} else {
		return LocaleSeparator::Semicolon;
	}
}

// Only to be called from within 'populate_all_country_info'!
static void populate_country_code()
{
	if (current_country == DosCountry::International) {
		// MS-DOS uses the same country code for International English
		// and Australia - we don't, as we have different settings for
		// these. Let's imitate MS-DOS behavior.
		dos.country_code = enum_val(DosCountry::Australia);
	} else {
		dos.country_code = enum_val(current_country);
	}

	if (guest_country_override && *guest_country_override != current_country) {
		dos.country_code = enum_val(*guest_country_override);
		populated.is_country_overriden = true;
	}
}

// Only to be called from within 'populate_all_country_info'!
static void populate_numeric_format(const LocaleInfoEntry& source)
{
	const auto& destination = dos.tables.country;

	size_t offset = InfoOffsetThousandsSeparator;
	destination[offset + 0] = enum_val(source.thousands_separator);
	destination[offset + 1] = 0;

	offset = InfoOffsetDecimalSeparator;
	destination[offset + 0] = enum_val(source.decimal_separator);
	destination[offset + 1] = 0;

	offset = InfoOffsetListSeparator;
	if (source.list_separator) {
		destination[offset + 0] = enum_val(*source.list_separator);
	} else {
		const auto list_separator = get_default_list_separator(
		        source.thousands_separator, source.decimal_separator);
		destination[offset + 0] = enum_val(list_separator);
	}

	destination[offset + 1] = 0;
}

// Only to be called from within 'populate_all_country_info'!
static bool try_override_numeric_format(const StdLibLocale& locale)
{
	// Check if standard library + host-specific detection provided us with
	// something suitable for populating into DOS
	if (!locale.thousands_separator || !locale.decimal_separator) {
		return false;
	}

	const auto thousands_separator = *locale.thousands_separator;
	const auto decimal_separator   = *locale.decimal_separator;

	if (thousands_separator == decimal_separator) {
		return false;
	}

	if (thousands_separator != enum_val(LocaleSeparator::Space) &&
	    thousands_separator != enum_val(LocaleSeparator::Apostrophe) &&
	    thousands_separator != enum_val(LocaleSeparator::Comma) &&
	    thousands_separator != enum_val(LocaleSeparator::Period)) {
		return false;
	}

	if (decimal_separator != enum_val(LocaleSeparator::Comma) &&
	    decimal_separator != enum_val(LocaleSeparator::Period)) {
		return false;
	}

	// Yes, this is suitable for using - populate
	const auto& destination = dos.tables.country;

	destination[InfoOffsetThousandsSeparator] = thousands_separator;
	destination[InfoOffsetDecimalSeparator]   = decimal_separator;

	const auto thousands = static_cast<LocaleSeparator>(thousands_separator);
	const auto decimal = static_cast<LocaleSeparator>(decimal_separator);

	const auto list_separator = get_default_list_separator(thousands, decimal);

	destination[InfoOffsetListSeparator] = enum_val(list_separator);
	return true;
}

// Only to be called from within 'populate_all_country_info'!
static void populate_time_date_format(const LocaleInfoEntry& source)
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
}

// Only to be called from within 'populate_all_country_info'!
static bool try_override_time_format(const StdLibLocale& locale)
{
	// Check if standard library + host-specific detection provided us with
	// something suitable for populating into DOS
	if (!locale.time_format) {
		return false;
	}

	const auto time_separator = locale.time_separator;

	if (time_separator != enum_val(LocaleSeparator::Colon) &&
	    time_separator != enum_val(LocaleSeparator::Period)) {
		return false;
	}

	// Yes, this is suitable for using - populate
	const auto& destination = dos.tables.country;

	destination[InfoOffsetTimeFormat]    = enum_val(*locale.time_format);
	destination[InfoOffsetTimeSeparator] = time_separator;
	return true;
}

// Only to be called from within 'populate_all_country_info'!
static bool try_override_date_format(const StdLibLocale& locale)
{
	// Check if standard library + host-specific detection provided us with
	// something suitable for populating into DOS
	if (!locale.date_format) {
		return false;
	}

	const auto date_separator = locale.date_separator;

	if (date_separator != enum_val(LocaleSeparator::Dash) &&
	    date_separator != enum_val(LocaleSeparator::Period) &&
	    date_separator != enum_val(LocaleSeparator::Slash)) {
		return false;
	}

	// Yes, this is suitable for using - populate
	const auto& destination = dos.tables.country;

	destination[InfoOffsetDateFormat]    = enum_val(*locale.date_format);
	destination[InfoOffsetDateSeparator] = date_separator;
	return true;
}

// Only to be called from within 'populate_all_country_info'!
static void populate_currency_format(const LocaleInfoEntry& source)
{
	const auto& destination = dos.tables.country;
	populated.is_using_euro_currency = (source.currency_code == EuroCurrencyCode);

	assert(source.currency_code.size() < MaxCurrencySymbolLength);
	memset(&destination[InfoOffsetCurrencySymbol], 0, MaxCurrencySymbolLength + 1);

	bool found = false;
	for (const auto& candidate_utf8 : source.currency_symbols_utf8) {

		// Check if the currency can be converted to current code page
		const auto candidate = utf8_to_dos(candidate_utf8,
		                                   DosStringConvertMode::NoSpecialCharacters,
		                                   UnicodeFallback::EmptyString);

		if (candidate.empty() || candidate.length() > MaxCurrencySymbolLength) {
			continue;
		}

		found = true;
		memcpy(&destination[InfoOffsetCurrencySymbol],
		       candidate.c_str(),
		       candidate.length());
		break;
	}

	size_t offset = InfoOffsetCurrencyFormat;
	destination[offset] = enum_val(source.currency_format);

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

// Only to be called from within 'populate_all_country_info'!
static bool try_override_currency_format(const StdLibLocale& locale)
{
	constexpr uint8_t DefaultCurrencyPrecision = 2;

	// Check if standard library + host-specific detection provided us with
	// something suitable for populating into DOS

	auto currency = locale.currency_code;

	if (currency.empty() || currency.length() > MaxCurrencySymbolLength) {
		return false;
	}
	for (const auto character : currency) {
		// Accept only printable, code-page independent ASCII
		if (is_upper_ascii(character) || !is_printable_ascii(character)) {
			return false;
		}
	}

	// Yes, this is suitable for using - populate
	const auto& destination = dos.tables.country;
	populated.is_using_euro_currency = (currency == EuroCurrencyCode);

	memset(&destination[InfoOffsetCurrencySymbol], 0, MaxCurrencySymbolLength + 1);

	// Try to use the UTF-8 currency variant first; we have only checked
	// the ASCII currency code so far, because we want the decision whether
	// to use the host currency or not to be code page agnostic.
	const auto candidate = utf8_to_dos(locale.currency_utf8,
	                                   DosStringConvertMode::NoSpecialCharacters,
	                                   UnicodeFallback::EmptyString);

	std::optional<DosCurrencyFormat> format = locale.currency_utf8_format;
	if (!candidate.empty() && candidate.length() <= MaxCurrencySymbolLength) {
		currency = candidate;
		format   = locale.currency_code_format;
	}

	memcpy(&destination[InfoOffsetCurrencySymbol], currency.c_str(), currency.length());
	if (format) {
		destination[InfoOffsetCurrencyFormat] = enum_val(*format);
	}

	auto currency_precision = locale.currency_precision;
	if (currency_precision == 0 || currency_precision > MaxCurrencyPrecision) {
		destination[InfoOffsetCurrencyPrecision] = currency_precision;
	} else {
		destination[InfoOffsetCurrencyPrecision] = DefaultCurrencyPrecision;
	}

	return true;
}

static void populate_all_country_info()
{
	// Set reserved/undocumented values to 0's
	memset(&dos.tables.country[InfoOffsetReserved], 0, ReservedAreaSize);

	auto get_info = [&](const DosCountry country)
	        -> std::pair<LocalePeriod, LocaleInfoEntry> {
		const auto deduplicated = deduplicate_country(country);

		assert(LocaleData::CountryInfo.contains(deduplicated));
		const auto& country_info = LocaleData::CountryInfo.at(deduplicated);

		assert(!country_info.locale_info.empty());

		// Select locale period
		auto locale_period = config.locale_period;
		if (locale_period == LocalePeriod::Native) {
			locale_period = LocalePeriod::Modern;
		}
		if (country_info.locale_info.contains(locale_period)) {
			return {locale_period, country_info.locale_info.at(locale_period)};
		} else {
			const auto iter = country_info.locale_info.begin();
			return {iter->first, iter->second};
		}
	};

	populated = Populated();
	populate_country_code();

	const auto& host_locale = GetHostLocale();

	// Populate numeric format
	const auto& [period, info] = get_info(static_cast<DosCountry>(dos.country_code));
	if (!config.country && !populated.is_country_overriden &&
	    host_locale.numeric) {
		populated.separate_numeric  = *host_locale.numeric;
		const auto& [period_specific,
		             info_specific] = get_info(*host_locale.numeric);
		populate_numeric_format(info_specific);
		if (period_specific != config.locale_period) {
			populated.is_using_fallback_period = true;
		}
	} else {
		populate_numeric_format(info);
		if (period != config.locale_period) {
			populated.is_using_fallback_period = true;
		}
	}
	if (config.locale_period == LocalePeriod::Native) {
		populated.is_using_native_numeric = try_override_numeric_format(
		        host_locale.native);
	}

	// Populate time/date format
	if (!config.country && !populated.is_country_overriden &&
	    host_locale.time_date) {
		populated.separate_time_date = *host_locale.time_date;
		const auto& [period_specific,
		             info_specific]  = get_info(*host_locale.time_date);
		populate_time_date_format(info_specific);
		if (period_specific != config.locale_period) {
			populated.is_using_fallback_period = true;
		}
	} else {
		populate_time_date_format(info);
		if (period != config.locale_period) {
			populated.is_using_fallback_period = true;
		}
	}
	if (config.locale_period == LocalePeriod::Native) {
		populated.is_using_native_time = try_override_time_format(
		        host_locale.native);
		populated.is_using_native_date = try_override_date_format(
		        host_locale.native);
	}

	// Populate currency format
	if (!config.country && !populated.is_country_overriden &&
	    host_locale.currency) {
		populated.separate_currency = *host_locale.currency;
		const auto& [period_specific,
		             info_specific] = get_info(*host_locale.currency);
		populate_currency_format(info_specific);
		if (period_specific != config.locale_period) {
			populated.is_using_fallback_period = true;
		}
	} else {
		populate_currency_format(info);
		if (period != config.locale_period) {
			populated.is_using_fallback_period = true;
		}
	}
	if (config.locale_period == LocalePeriod::Native) {
		populated.is_using_native_currency = try_override_currency_format(
		        host_locale.native);
	}
}

void DOS_RepopulateCountryInfo()
{
	populate_all_country_info();
}

// ***************************************************************************
// DOS API support
// ***************************************************************************

bool DOS_SetCountry(const uint16_t country_id)
{
	if (country_id == 0) {
		return false; // not a valid value for DOS int 21h call
	}

	const auto country = static_cast<DosCountry>(country_id);
	if (!is_country_supported(country)) {\
		return false; // requested country not supported
	}

	guest_country_override = country;
	populate_all_country_info();

	LOG_MSG("DOS: Switched country to %d '%s'",
	        country_id,
	        get_country_name_for_log(country).c_str());

	return true;
}

uint16_t DOS_GetCountry()
{
	return dos.country_code;
}

// ***************************************************************************
// Helper functions for 'dosbox --list-*' commands
// ***************************************************************************

// ANSI codes for colors
static const std::string AnsiReset     = "[reset]";
static const std::string AnsiWhite     = "[color=white]";
static const std::string AnsiHighlight = "[color=light-green]";

std::string DOS_GenerateListCountriesMessage()
{
	std::string message = {};

	const std::string header = MSG_GetRaw("DOSBOX_HELP_LIST_COUNTRIES_1");

	message += std::string("\n") + header + "\n";
	message += std::string(header.size(), '-') + "\n\n";

	for (auto it = LocaleData::CountryInfo.begin();
	     it != LocaleData::CountryInfo.end();
	     ++it) {
		message += format_str("  %-5d - %s\n",
		                      enum_val(it->first),
		                      MSG_GetRaw(it->second.GetMsgName().c_str()));
	}

	message += "\n";
	message += MSG_GetRaw("DOSBOX_HELP_LIST_COUNTRIES_2");
	message += "\n";

	return message;
}

std::string DOS_GenerateListKeyboardLayoutsMessage(const bool for_keyb_command)
{
	std::string message        = {};
	std::string highlight_code = {};
	std::string header = MSG_GetRaw("DOSBOX_HELP_LIST_KEYBOARD_LAYOUTS_1");

	if (for_keyb_command) {
                header  += ":";
		message += AnsiWhite + header + AnsiReset + "\n\n";
		highlight_code = DOS_GetLoadedLayout();
	} else {
		message += std::string("\n") + header + "\n";
		message += std::string(header.size(), '-') + "\n\n";
	}

	struct Row {
		std::string column1 = {};
		std::string column2 = {};
		bool highlight      = false;
	};

	std::vector<Row> table = {};

	size_t column_1_width = 0;
	for (const auto& entry : LocaleData::KeyboardLayoutInfo) {
		bool highlight = false;

		// Column 1 - keyboard codes
		std::string column_1_raw  = {};
		std::string column_1_ansi = {};
		for (const auto& layout_code : entry.layout_codes) {
			if (!column_1_raw.empty()) {
				column_1_raw += ", ";
				column_1_ansi += ", ";
			}
			column_1_raw += layout_code;
			if (highlight_code == layout_code) {
				column_1_ansi += AnsiHighlight;
				column_1_ansi += layout_code;
				column_1_ansi += AnsiReset;
				highlight = true;
			} else {
				column_1_ansi += layout_code;
			}
		}
		column_1_width = std::max(column_1_width, column_1_raw.length());

		// Column 2 - localized keyboard name/description
		const auto column_2 = MSG_GetRaw(entry.GetMsgName().c_str());
		table.push_back({column_1_ansi, column_2, highlight});
	}

	const size_t column_1_highlighted_width = AnsiHighlight.size() +
	                                          AnsiReset.size() + column_1_width;
	for (auto& table_row : table) {
		if (table_row.highlight) {
			table_row.column1.resize(column_1_highlighted_width, ' ');
			message += format_str("%s*%s %s %s- %s%s\n",
			                      AnsiHighlight.c_str(),
                                              AnsiReset.c_str(),
                                              table_row.column1.c_str(),
			                      AnsiHighlight.c_str(),
			                      table_row.column2.c_str(),
			                      AnsiReset.c_str());
		} else {
			table_row.column1.resize(column_1_width, ' ');
			message += format_str("  %s - %s\n",
			                      table_row.column1.c_str(),
			                      table_row.column2.c_str());
		}
	}

	if (for_keyb_command) {
		return convert_ansi_markup(message);
	} else {
		message += "\n";
		message += MSG_GetRaw("DOSBOX_HELP_LIST_KEYBOARD_LAYOUTS_2");
		message += "\n";

		return message;
	}
}

// ***************************************************************************
// Helper functions for KEYB.COM command
// ***************************************************************************

std::string DOS_GetKeyboardLayoutName(const std::string& layout)
{
	const auto layout_deduplicated = deduplicate_layout(layout);

	for (const auto& entry : LocaleData::KeyboardLayoutInfo) {
		assert(!entry.layout_codes.empty());
		if (entry.layout_codes[0] == layout_deduplicated) {
			return MSG_Get(entry.GetMsgName().c_str());
		}
	}

	return {};
}

std::string DOS_GetKeyboardScriptName(const KeyboardScript script)
{
	switch (script) {
	case KeyboardScript::LatinQwerty:
		return std::string(MSG_Get("SCRIPT_LATIN")) + " (QWERTY)";
	case KeyboardScript::LatinQwertz:
		return std::string(MSG_Get("SCRIPT_LATIN")) + " (QWERTZ)";
	case KeyboardScript::LatinAzerty:
		return std::string(MSG_Get("SCRIPT_LATIN")) + " (AZERTY)";
	case KeyboardScript::LatinAsertt:
		return std::string(MSG_Get("SCRIPT_LATIN")) + " (ASERTT)";
	case KeyboardScript::LatinJcuken:
		return std::string(MSG_Get("SCRIPT_LATIN")) + " (JCUKEN)";
	case KeyboardScript::LatinUgjrmv:
		return std::string(MSG_Get("SCRIPT_LATIN")) + " (UGJRMV)";
	case KeyboardScript::LatinColemak:
		return std::string(MSG_Get("SCRIPT_LATIN")) + " (Colemak)";
	case KeyboardScript::LatinDvorak:
		return std::string(MSG_Get("SCRIPT_LATIN")) + " (Dvorak)";
	case KeyboardScript::LatinNonStandard:
		return std::string(MSG_Get("SCRIPT_LATIN")) + " (" +
		       MSG_Get("SCRIPT_PROPERTY_NON_STANDARD") + ")";
	case KeyboardScript::Arabic: return MSG_Get("SCRIPT_ARABIC");
	case KeyboardScript::Armenian: return MSG_Get("SCRIPT_ARMENIAN");
	case KeyboardScript::Cherokee: return MSG_Get("SCRIPT_CHEROKEE");
	case KeyboardScript::Cyrillic: return MSG_Get("SCRIPT_CYRILLIC");
	case KeyboardScript::CyrillicPhonetic:
		return std::string(MSG_Get("SCRIPT_CYRILLIC")) + " (" +
		       MSG_Get("SCRIPT_PROPERTY_PHONETIC") + ")";
	case KeyboardScript::Georgian: return MSG_Get("SCRIPT_GEORGIAN");
	case KeyboardScript::Greek: return MSG_Get("SCRIPT_GREEK");
	case KeyboardScript::Hebrew: return MSG_Get("SCRIPT_HEBREW");
	case KeyboardScript::Thai: return MSG_Get("SCRIPT_THAI");
	default: assert(false);
	}

	return "<unknown keyboard script>";
}

std::string DOS_GetShortcutKeyboardScript1()
{
	return MSG_Get("KEYBOARD_SHORTCUT_SCRIPT_1");
}

std::string DOS_GetShortcutKeyboardScript2()
{
	return MSG_Get("KEYBOARD_SHORTCUT_SCRIPT_2");
}

std::string DOS_GetShortcutKeyboardScript3()
{
	return MSG_Get("KEYBOARD_SHORTCUT_SCRIPT_3");
}

std::optional<KeyboardScript> DOS_GetKeyboardLayoutScript1(const std::string& layout)
{
	const auto layout_deduplicated = deduplicate_layout(layout);

	for (const auto& entry : LocaleData::KeyboardLayoutInfo) {
		assert(!entry.layout_codes.empty());
		if (entry.layout_codes[0] == layout_deduplicated) {
			return entry.primary_script;
		}
	}

	return {};
}

std::optional<KeyboardScript> DOS_GetKeyboardLayoutScript2(const std::string& layout,
                                                           const uint16_t code_page)
{
	const auto layout_deduplicated = deduplicate_layout(layout);

	for (const auto& entry : LocaleData::KeyboardLayoutInfo) {
		assert(!entry.layout_codes.empty());
		if (entry.layout_codes[0] == layout_deduplicated) {
			const auto& list = entry.secondary_scripts;
			if (list.contains(code_page)) {
				return list.at(code_page);
			}
		}
	}

	return {};
}

std::optional<KeyboardScript> DOS_GetKeyboardLayoutScript3(const std::string& layout,
                                                           const uint16_t code_page)
{
	const auto layout_deduplicated = deduplicate_layout(layout);

	for (const auto& entry : LocaleData::KeyboardLayoutInfo) {
		assert(!entry.layout_codes.empty());
		if (entry.layout_codes[0] == layout_deduplicated) {
			const auto& list = entry.tertiary_scripts;
			if (list.contains(code_page)) {
				return list.at(code_page);
			}
		}
	}

	return {};
}

std::optional<CodePageWarning> DOS_GetCodePageWarning(const uint16_t code_page)
{
	if (!LocaleData::CodePageWarnings.contains(code_page)) {
		return {};
	}

	return LocaleData::CodePageWarnings.at(code_page);
}

// ***************************************************************************
// Locale retrieval functions
// ***************************************************************************

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
// Misc functions
// ***************************************************************************

std::string DOS_GetBundledCpiFileName(const uint16_t code_page)
{
	for (const auto& entry : LocaleData::BundledCpiContent) {
		for (const auto& known_code_page : entry.second) {
			if (known_code_page == code_page) {
				return entry.first;
			}
		}
	}

	return {};
}

uint16_t DOS_GetBundledCodePage(const std::string& keyboard_layout)
{
	const auto deduplicated_layout = deduplicate_layout(keyboard_layout);

	auto assert_code_page = [](const uint16_t code_page) {
		assert(!DOS_GetBundledCpiFileName(code_page).empty());
		return code_page;
	};

	for (const auto& entry : LocaleData::KeyboardLayoutInfo) {
		for (const auto& entry_keyboard_layout : entry.layout_codes) {
			if (deduplicated_layout == entry_keyboard_layout) {
				return assert_code_page(entry.default_code_page);
			}
		}
	}

	LOG_WARNING("DOS: No default code page for keyboard layout '%s'",
	            keyboard_layout.c_str());
	return assert_code_page(DefaultCodePage);
}

std::vector<uint16_t> DOS_SuggestBetterCodePages(const uint16_t code_page,
                                                 const std::string& keyboard_layout)
{
        // We are only suggesting code pages which did not exist in the original
        // MS-DOS, don't do it if the user requested historic locale settings
	if (config.locale_period == LocalePeriod::Historic) {
                return {};
        }

	std::vector<uint16_t> suggestions = {};

        // According to FreeDOS keyboard layouts package documentation, for the
        // French and Belgian keyboard layouts there is now a better code page,
        // which provides more national characters than the original MS-DOS one
        if ((code_page == 850 || code_page == 858) &&
            (keyboard_layout == "be" || keyboard_layout == "fr")) {
                suggestions.push_back(859);
        }

        // Suggest replacing certain code pages with EUR currency variants
        if (populated.is_using_euro_currency) {
                switch (code_page) {
                case 850: suggestions.push_back(858); break;
                case 855: suggestions.push_back(872); break;
                case 866: suggestions.push_back(808); break;
                case 1125: suggestions.push_back(848); break;
                default: break;
                }
        }

	return suggestions;
}

// ***************************************************************************
// Locale loading
// ***************************************************************************

static void log_country_locale(const bool only_changed_period = false)
{
	const auto& host_locale = GetHostLocale();

	if (!config.country && !only_changed_period) {
		LOG_MSG("DOS: Country detected from '%s'",
		        host_locale.log_info.country.c_str());
	}

	std::string country_name = {};
	if (populated.is_country_overriden) {
		assert(guest_country_override);
		country_name = get_country_name_for_log(*guest_country_override);
	} else {
		country_name = get_country_name_for_log(current_country);
	}
	LOG_MSG("DOS: Loading %s locale for country %d - '%s'",
	        get_locale_period_for_log(config.locale_period).c_str(),
	        dos.country_code,
	        country_name.c_str());

	if (populated.separate_numeric &&
	    config.locale_period != LocalePeriod::Native) {
		const auto extra_country = *populated.separate_numeric;
		LOG_MSG("DOS: Using numeric format for country %d - '%s', detected from '%s'",
		        enum_val(extra_country),
		        get_country_name_for_log(extra_country).c_str(),
		        host_locale.log_info.numeric.c_str());
	}

	if (populated.separate_time_date &&
	    config.locale_period != LocalePeriod::Native) {
		const auto extra_country = *populated.separate_time_date;
		LOG_MSG("DOS: Using time/date format for country %d - '%s', detected from '%s'",
		        enum_val(extra_country),
		        get_country_name_for_log(extra_country).c_str(),
		        host_locale.log_info.time_date.c_str());
	}

	if (populated.separate_currency &&
	    config.locale_period != LocalePeriod::Native) {
		const auto extra_country = *populated.separate_currency;
		LOG_MSG("DOS: Using currency format for country %d - '%s', detected from '%s'",
		        enum_val(extra_country),
		        get_country_name_for_log(extra_country).c_str(),
		        host_locale.log_info.currency.c_str());
	}

	if (populated.is_using_native_numeric || populated.is_using_native_time ||
	    populated.is_using_native_date || populated.is_using_native_currency) {
		std::string message = {};
		auto maybe_add = [&](const bool should_add, const std::string& name) {
			if (!should_add) {
				return;
			}
			if (!message.empty()) {
				message += "/";
			}
			message += name;
		};

		maybe_add(populated.is_using_native_numeric, "numeric");
		maybe_add(populated.is_using_native_time, "time");
		maybe_add(populated.is_using_native_date, "date");
		maybe_add(populated.is_using_native_currency, "currency");

		LOG_MSG("DOS: Using %s format detected from host OS",
		        message.c_str());
	}

	if (populated.is_using_fallback_period &&
	    config.locale_period != LocalePeriod::Native) {
		LOG_WARNING("DOS: No period correct country data found");
	}
}

static void load_country()
{
	assert(dos.tables.country);
	config.country = {};

	// Parse the config file value
	if (!config.country_str.empty() && config.country_str != "auto") {
		const auto value = parse_int(config.country_str);
		if (value && *value > 0 && *value <= UINT16_MAX &&
		    is_country_supported(static_cast<DosCountry>(*value))) {
			config.country = static_cast<DosCountry>(*value);
		} else {
			// NOTE: In such case MS-DOS 6.22 uses modified locale,
			// it uses country 1 with date separator '-' instead of
			// '/'. This behavior is not emulated - I believe it is
			// due to the hardcoded locale being slightly different
			// than the one from COUNTRY.SYS (likely MS-DOS bug).
			LOG_WARNING("DOS: '%s' is not a valid country code, using autodetected",
			            config.country_str.c_str());
		}
	}

	// Determine country ID
	if (config.country && *config.country == DosCountry::International) {
		current_country = get_default_country();
	} else if (config.country) {
		current_country = *config.country;
	} else {
		const auto& host_locale = GetHostLocale();
		if (host_locale.country) {
			current_country = *host_locale.country;
			if (current_country == DosCountry::International) {
				current_country = get_default_country();
			}
		} else {
			current_country = get_default_country();
		}
	}

	guest_country_override = {};
	populate_all_country_info();
	log_country_locale();
}

static void change_locale_period()
{
	populate_all_country_info();

	constexpr bool only_changed_period = true;
	log_country_locale(only_changed_period);
}

static void preprocess_layouts_arabic(std::vector<KeyboardLayoutMaybeCodepage>& keyboard_layouts)
{
	// Arabic keyboard layouts 'ar462' and 'ar470' differs in one important
	// aspect: 'ar462' provides Latin layout AZERTY, 'ar470' QWERTY.
	// It might be very hard for the OS-specific detection code to tell
	// which of them is better for the current user - therefore let's look
	// at other keyboard layouts (Arabic users are likely to have English or
	// French configured in their host OS, for typing Latin characters) and
	// see whether the user prefers QWERTY-like or AZERTY-like layouts.

	const std::string Ar462 = "ar462";
	const std::string Ar470 = "ar470";

	size_t votes_ar462     = 0; // AZERTY
	size_t votes_ar470     = 0; // QWERTY
	bool has_arabic_layout = false;

	auto check_azerty_asertt = [](const std::string& keyboard_layout) {
		const auto script = DOS_GetKeyboardLayoutScript1(keyboard_layout);
		if (!script) {
			return false;
		}
		return *script == KeyboardScript::LatinAzerty ||
                       *script == KeyboardScript::LatinAsertt;
	};

	auto check_qwerty_qwertz = [](const std::string& keyboard_layout) {
		const auto script = DOS_GetKeyboardLayoutScript1(keyboard_layout);
		if (!script) {
			return false;
		}
		return *script == KeyboardScript::LatinQwerty ||
                       *script == KeyboardScript::LatinQwertz;
	};

	for (const auto& entry : keyboard_layouts) {
		if (entry.keyboard_layout == Ar462 || entry.keyboard_layout == Ar470) {
			has_arabic_layout = true;
			continue;
		}

		if (check_azerty_asertt(entry.keyboard_layout)) {
			++votes_ar462;
		}
		if (check_qwerty_qwertz(entry.keyboard_layout)) {
			++votes_ar470;
		}
	}

	if (!has_arabic_layout || votes_ar462 == votes_ar470) {
		return; // nothing to do
	}

	// Propose layout swapping
	std::vector<KeyboardLayoutMaybeCodepage> processed = {};
	for (const auto& source : keyboard_layouts) {
		if (source.keyboard_layout == Ar462 && votes_ar470 > votes_ar462) {
			processed.push_back({Ar470, source.code_page});
		}
		if (source.keyboard_layout == Ar470 && votes_ar462 > votes_ar470) {
			processed.push_back({Ar462, source.code_page});
		}
		processed.push_back(source);
	}

	keyboard_layouts = processed;
}

static void preprocess_layouts_better_code_pages(
        std::vector<KeyboardLayoutMaybeCodepage>& keyboard_layouts)
{
	// Sometimes (depending on various conditions, like other user settings)
	// we can propose a better code page than default one. There is
	// no guarantee it will be compatible with the given keyboard layout,
	// but let's add them to testing/
	// For code pages left for autodetection, better code page proposals
	// will be evaluated later/

	std::vector<KeyboardLayoutMaybeCodepage> processed = {};
	for (const auto& source : keyboard_layouts) {
		if (source.code_page) {
			for (const auto code_page :
			     DOS_SuggestBetterCodePages(*source.code_page,
			                                source.keyboard_layout)) {
				processed.push_back(
				        {source.keyboard_layout, code_page});
			}
		}
		processed.push_back(source);
	}

	keyboard_layouts = processed;
}

static void preprocess_detected_keyboard_layouts(
        std::vector<KeyboardLayoutMaybeCodepage>& keyboard_layouts)
{
	preprocess_layouts_arabic(keyboard_layouts);
	preprocess_layouts_better_code_pages(keyboard_layouts);
}

static void sort_detected_keyboard_layouts(
        std::vector<KeyboardLayoutMaybeCodepage>& keyboard_layouts)
{
	// Get the relevant keyboard layout information into a local std::map
	std::map<std::string, KeyboardLayoutInfoEntry> info_map = {};
	for (const auto& detected : keyboard_layouts) {
		const auto detected_deduplicated = deduplicate_layout(
		        detected.keyboard_layout);
		for (const auto& entry : LocaleData::KeyboardLayoutInfo) {
			for (const auto& layout_code : entry.layout_codes) {
				if (layout_code == detected_deduplicated) {
					assert(!info_map.contains(layout_code));
					info_map[layout_code] = entry;
					break;
				}
			}
		}
		assert(info_map.contains(detected_deduplicated));
	}

	auto has_script_above = [&](const KeyboardLayoutMaybeCodepage& entry,
	                            const KeyboardScript script) {
		const auto& info_entry = info_map.at(
		        deduplicate_layout(entry.keyboard_layout));
		if (info_entry.primary_script > script) {
			return true;
		}

		const auto code_page = entry.code_page
		                             ? *entry.code_page
		                             : info_entry.default_code_page;

		const auto& secondary = info_entry.secondary_scripts;
		if (secondary.contains(code_page) && secondary.at(code_page) > script) {
			return true;
		}
		const auto& tertiary = info_entry.secondary_scripts;
		if (tertiary.contains(code_page) && tertiary.at(code_page) > script) {
			return true;
		}

		return false;
	};

	auto has_non_latin_script = [&](const KeyboardLayoutMaybeCodepage& entry) {
		return has_script_above(entry, KeyboardScript::LastLatinScript);
	};

	auto has_non_qwerty_like_script = [&](const KeyboardLayoutMaybeCodepage& entry) {
		return has_script_above(entry, KeyboardScript::LastQwertyLikeScript);
	};

	static_assert(enum_val(KeyboardScript::LatinQwerty) == 0);
	auto has_non_qwerty_script = [&](const KeyboardLayoutMaybeCodepage& entry) {
		return has_script_above(entry, KeyboardScript::LatinQwerty);
	};

	auto count_supported_scripts = [&](const KeyboardLayoutMaybeCodepage& entry) {
		uint8_t result         = 1;
		const auto& info_entry = info_map.at(
		        deduplicate_layout(entry.keyboard_layout));
		const auto code_page = entry.code_page
		                             ? *entry.code_page
		                             : info_entry.default_code_page;

		const auto& secondary = info_entry.secondary_scripts;
		if (secondary.contains(code_page)) {
			++result;
		}
		const auto& tertiary = info_entry.secondary_scripts;
		if (tertiary.contains(code_page)) {
			++result;
		}

		return result;
	};

	// Sort the detected keyboard layouts from the most to least probable
	// to be the main national layout
	std::stable_sort(
	        keyboard_layouts.begin(),
	        keyboard_layouts.end(),
	        [&](const auto& l, const auto& r) {
		        // 'l' more preferable than 'r'    => return true
		        // 'l' less preferable than 'r'    => return false
		        // both layouts equally preferable => return false
		        if (l == r) {
			        return false;
		        }

		        // Prefer keyboard layouts containing a non-Latin script
		        const bool l_has_non_latin = has_non_latin_script(l);
		        const bool r_has_non_latin = has_non_latin_script(r);
		        if (l_has_non_latin && !r_has_non_latin) {
			        return true;
		        } else if (!l_has_non_latin && r_has_non_latin) {
			        return false;
		        }

		        // Prefer keyboard layouts containing a non-QWERTY-like
		        // script
		        const bool l_has_non_qwerty_like = has_non_qwerty_like_script(l);
		        const bool r_has_non_qwerty_like = has_non_qwerty_like_script(r);
		        if (l_has_non_qwerty_like && !r_has_non_qwerty_like) {
			        return true;
		        } else if (!l_has_non_qwerty_like && r_has_non_qwerty_like) {
			        return false;
		        }

		        // Prefer keyboard layouts containing a non-QWERTY script
		        const bool l_has_non_qwerty = has_non_qwerty_script(l);
		        const bool r_has_non_qwerty = has_non_qwerty_script(r);
		        if (l_has_non_qwerty && !r_has_non_qwerty) {
			        return true;
		        } else if (!l_has_non_qwerty && r_has_non_qwerty) {
			        return false;
		        }

		        // Prefer keyboard layouts supporting more scripts
		        const auto num_scripts_l = count_supported_scripts(l);
		        const auto num_scripts_r = count_supported_scripts(r);
		        if (num_scripts_l > num_scripts_r) {
			        return true;
		        } else if (num_scripts_l < num_scripts_r) {
			        return false;
		        }

		        // Prefer layouts with non-default and non-standard code
		        // page
		        const bool l_has_code_page = l.code_page &&
		                                     (*l.code_page != DefaultCodePage);
		        const bool r_has_code_page = r.code_page &&
		                                     (*r.code_page != DefaultCodePage);
		        if (l_has_code_page && !r_has_code_page) {
			        return true;
		        } else if (!l_has_code_page && r_has_code_page) {
			        return false;
		        }

		        // Prefer layouts which are not plain US ones
		        const bool l_is_not_us = l.keyboard_layout != "us";
		        const bool r_is_not_us = r.keyboard_layout != "us";
		        if (l_is_not_us && !r_is_not_us) {
			        return true;
		        } else if (!l_is_not_us && r_is_not_us) {
			        return false;
		        }

		        // Prefer layouts which are not plain UK ones
		        const bool l_is_not_uk = l.keyboard_layout != "uk";
		        const bool r_is_not_uk = r.keyboard_layout != "uk";
		        if (l_is_not_uk && !r_is_not_uk) {
			        return true;
		        } else if (!l_is_not_uk && r_is_not_uk) {
			        return false;
		        }

		        // For now I have no sane idea for more criteria...
		        return false;
	        });
}

static std::vector<KeyboardLayoutMaybeCodepage> get_detected_keyboard_layouts()
{
	auto keyboard_layouts = GetHostLocale().keyboard_layout_list;

	// Keyboard layouts in modern OSes support just one script (like only
	// Latin, only Greek, only Cyrillic, etc.) and in countries using
	// non-Latin alphabets users often enable more than one layout - like
	// Greek + US English, or Arabic + French AZERTY - and switch between
	// them using a keyboard shortcut or a system tray applet.
	// This was different in DOS days, where you could configure only a
	// single keyboard layout, and Greek/Cyrillic/etc. layouts also support
	// Latin scripts; DOS keyboard layouts can be 2-in-1, or even 3-in-1.
	// The host OS support code should have already mapped host keyboard
	// layouts to DOS ones, our task is now to guess which one is the most
	// likely to be the main national one (Greek, Hebrew, etc.).

	sort_detected_keyboard_layouts(keyboard_layouts);
	preprocess_detected_keyboard_layouts(keyboard_layouts);

	return keyboard_layouts;
}

static void load_keyboard_layout()
{
	std::vector<KeyboardLayoutMaybeCodepage> keyboard_layouts = {};

	bool using_detected     = false; // if layout list is autodetected
	bool code_page_supplied = false; // if code page given in the parameter

	if (config.keyboard_str.empty() || config.keyboard_str == "auto") {
		keyboard_layouts = get_detected_keyboard_layouts();
		using_detected   = true;
	} else {
		const auto tokens = split(config.keyboard_str);
		if (tokens.size() != 1 && tokens.size() != 2) {
			keyboard_layouts = get_detected_keyboard_layouts();
			using_detected   = true;
			LOG_WARNING("DOS: Invalid 'keyboard_layout' setting '%s', using 'auto'",
			            config.keyboard_str.c_str());
		} else {
			keyboard_layouts.push_back({tokens[0]});
			if (tokens.size() >= 2) {
				const auto result = parse_int(tokens[1]);
				if (!result || *result < 1 || *result > UINT16_MAX) {
					LOG_WARNING("DOS: Invalid 'keyboard_layout' code page '%s', ignoring",
					            tokens[1].c_str());
				} else {
					keyboard_layouts[0].code_page =
					        static_cast<uint16_t>(*result);
					code_page_supplied = true;
				}
			}
		}
	}

	const auto& host_locale = GetHostLocale();
	if (using_detected && !host_locale.log_info.keyboard.empty()) {
		LOG_MSG("DOS: Keyboard layout and code page detected from '%s'",
		        host_locale.log_info.keyboard.c_str());
	}

	// Apply the code page
	KeyboardLayoutResult result = KeyboardLayoutResult::LayoutNotKnown;
	const bool prefer_rom_font  = !using_detected && !code_page_supplied;
	for (const auto& keyboard_layout : keyboard_layouts) {
		uint16_t tried_code_page = 0;
		if (keyboard_layout.code_page) {
			tried_code_page = *keyboard_layout.code_page;
		}

		result = DOS_LoadKeyboardLayout(keyboard_layout.keyboard_layout,
		                                tried_code_page,
		                                {},
		                                prefer_rom_font);
		if (result == KeyboardLayoutResult::OK) {
			break; // success
		}
	}

	// If failed to set user provided settings, print out warning
	if (!using_detected && result != KeyboardLayoutResult::OK) {
		// We have tried to set user-requested keyboard layout, but
		// something went wrong
		if (code_page_supplied &&
		    (result == KeyboardLayoutResult::NoBundledCpiFileForCodePage ||
		     result == KeyboardLayoutResult::LayoutNotKnown)) {
			// Retry without code page
			LOG_WARNING("DOS: Unable to use 'keyboard_layout' code page %d, ignoring",
			            *keyboard_layouts[0].code_page);
			uint16_t tried_code_page = 0;
			result = DOS_LoadKeyboardLayout(keyboard_layouts[0].keyboard_layout,
			                                tried_code_page,
			                                {},
			                                prefer_rom_font);
			if (result != KeyboardLayoutResult::OK) {
				LOG_WARNING("DOS: Unable to use specified 'keyboard_layout' setting '%s', using 'us'",
				            config.keyboard_str.c_str());
			}
		} else if (result == KeyboardLayoutResult::IncompatibleMachine) {
			LOG_WARNING("DOS: Invalid 'keyboard_layout' setting '%s' for this display adapter, using 'us'",
			            config.keyboard_str.c_str());
		} else {
			LOG_WARNING("DOS: Unable to use specified 'keyboard_layout' setting '%s', using 'us'",
			            config.keyboard_str.c_str());
		}
	}

	// Make sure some keyboard layout is actually set
	if (DOS_GetLoadedLayout().empty()) {
		uint16_t tried_code_page = 0;
		DOS_LoadKeyboardLayout("us", tried_code_page);
	}
}

// ***************************************************************************
// Lifecycle
// ***************************************************************************

class DOS_Locale final : public Module_base {
public:
	DOS_Locale(Section* configuration);
	~DOS_Locale();
};

DOS_Locale::DOS_Locale(Section* configuration) : Module_base(configuration)
{
	auto section = static_cast<Section_prop*>(configuration);
	assert(section);

	if (!config.is_config_loaded) {
		dos.loaded_codepage    = DefaultCodePage;
		// Make sure the locale detection is done in a predictable place
		GetHostLocale();
	}

	const auto keyboard_str = section->Get_string("keyboard_layout");
	const auto country_str  = section->Get_string("country");
	const auto period_str   = section->Get_string("locale_period");

	LocalePeriod locale_period = {};
	if (period_str == "native") {
		locale_period = LocalePeriod::Native;
	} else if (period_str == "modern") {
		locale_period = LocalePeriod::Modern;
	} else if (period_str == "historic") {
		locale_period = LocalePeriod::Historic;
	} else {
		assert(false);
	}

	// Apply country and locale period
	if (!config.is_config_loaded || country_str != config.country_str) {
		config.country_str   = country_str;
		config.locale_period = locale_period;
		load_country();
	} else if (locale_period != config.locale_period) {
		config.locale_period = locale_period;
		change_locale_period();
	}

	// Apply keyboard layout and code page
	if (!config.is_config_loaded) {
		// Has to be done after the country is detected and set up,
		// as the usage of EUR currency might affect code page
		// selection
		config.keyboard_str = keyboard_str;
		load_keyboard_layout();
	}

	config.is_config_loaded = true;
}

DOS_Locale::~DOS_Locale() {}

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
	        "List of available country codes (mostly same as telephone call codes)");
	MSG_Add("DOSBOX_HELP_LIST_COUNTRIES_2",
	        "The above codes can be used in the 'country' config setting.");

	MSG_Add("DOSBOX_HELP_LIST_KEYBOARD_LAYOUTS_1",
	        "List of available keyboard layout codes");
	MSG_Add("DOSBOX_HELP_LIST_KEYBOARD_LAYOUTS_2",
	        "The above codes can be used in the 'keyboard_layout' config setting.");

	// Add strings with country names
	for (auto it = LocaleData::CountryInfo.begin();
	     it != LocaleData::CountryInfo.end();
	     ++it) {
		MSG_Add(it->second.GetMsgName().c_str(),
		        it->second.country_name.c_str());
	}

	// Add strings with keyboard layout names
	for (const auto& entry : LocaleData::KeyboardLayoutInfo) {
		MSG_Add(entry.GetMsgName().c_str(), entry.layout_name.c_str());
	}

	MSG_Add("SCRIPT_LATIN",    "Latin");
	MSG_Add("SCRIPT_ARABIC",   "Arabic");
	MSG_Add("SCRIPT_ARMENIAN", "Armenian");
	MSG_Add("SCRIPT_CHEROKEE", "Cherokee");
	MSG_Add("SCRIPT_CYRILLIC", "Cyrillic");
	MSG_Add("SCRIPT_GEORGIAN", "Georgian");
	MSG_Add("SCRIPT_GREEK",    "Greek");
	MSG_Add("SCRIPT_HEBREW",   "Hebrew");
	MSG_Add("SCRIPT_THAI",     "Thai");

	MSG_Add("KEYBOARD_SHORTCUT_SCRIPT_1", "LeftAlt+LeftShift");
	MSG_Add("KEYBOARD_SHORTCUT_SCRIPT_2", "LeftAlt+RightShift");
	MSG_Add("KEYBOARD_SHORTCUT_SCRIPT_3", "LeftAlt+LeftCtrl");

	MSG_Add("SCRIPT_PROPERTY_PHONETIC",     "phonetic");
	MSG_Add("SCRIPT_PROPERTY_NON_STANDARD", "non-standard");
}
