// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos_locale.h"

#include <cstring>
#include <map>
#include <vector>

#include "dos_keyboard_layout.h"
#include "dos_locale.h"
#include "gui/mapper.h"
#include "misc/ansi_code_markup.h"
#include "misc/host_locale.h"
#include "misc/logging.h"
#include "misc/unicode.h"
#include "utils/bitops.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

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
	bool is_country_international = false;
	bool is_country_overriden     = false;
	bool is_using_fallback_period = false;
	bool is_using_native_numeric  = false;
	bool is_using_native_time     = false;
	bool is_using_native_date     = false;
	bool is_using_native_currency = false;

        // POSIX systems can be configured to use different country locales
        // for different elements - like US time/date format, but UK currency
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

// Only to be called from within 'populate_all_country_info'!
static void populate_country_code()
{
	auto country = current_country;

	if (current_country == DosCountry::International) {
		// MS-DOS uses the same country code for International English
		// and Australia - we don't, as we have different settings for
		// these. Let's imitate the MS-DOS behavior.
		country = DosCountry::Australia;
		populated.is_country_international = true;
	} else {
		populated.is_country_international = false;
	}
	dos.country_code = enum_val(country);

	if (guest_country_override && *guest_country_override != country) {
		dos.country_code = enum_val(*guest_country_override);
		populated.is_country_overriden = true;
		populated.is_country_international = false;
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
	destination[offset + 0] = enum_val(source.list_separator);

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

	// Yes, the separators are suitable for using in DOS - populate
	const auto& destination = dos.tables.country;

	destination[InfoOffsetThousandsSeparator] = thousands_separator;
	destination[InfoOffsetDecimalSeparator]   = decimal_separator;
	destination[InfoOffsetListSeparator]      = enum_val(LocaleSeparator::Semicolon);
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
	const auto& [period, info] = populated.is_country_international
	                                   ? get_info(DosCountry::International)
	                                   : get_info(static_cast<DosCountry>(
	                                              dos.country_code));
	if (!config.country && !populated.is_country_overriden &&
	    host_locale.numeric.country_code) {
		const auto country_code = *host_locale.numeric.country_code;
		populated.separate_numeric = country_code;
		const auto& [period_specific, info_specific] = get_info(country_code);
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
	    host_locale.time_date.country_code) {
		const auto country_code = *host_locale.time_date.country_code;
		populated.separate_time_date = country_code;
		const auto& [period_specific, info_specific] = get_info(country_code);
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
	    host_locale.currency.country_code) {
		const auto country_code = *host_locale.currency.country_code;
		populated.separate_currency = country_code;
		const auto& [period_specific, info_specific] = get_info(country_code);
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

	LOG_MSG("LOCALE: Switched country to %d '%s'",
	        country_id,
	        get_country_name_for_log(country).c_str());

	return true;
}

uint16_t DOS_GetCountry()
{
	return dos.country_code;
}

// ***************************************************************************
// Helper functions for commands and logging output
// ***************************************************************************

static std::string get_output_header(const char* header_msg_id,
                                     const bool for_keyb_command = false)
{
	if (for_keyb_command) {
		return Ansi::HighlightHeader + MSG_Get(header_msg_id) + ":" +
		       Ansi::Reset + "\n\n";
	} else {
		std::string header_str = MSG_GetTranslatedRaw(header_msg_id);
		return std::string("\n") + header_str.c_str() + "\n" +
		       std::string(length_utf8(header_str), '-') + "\n\n";
	}
}

std::string DOS_GenerateListCountriesMessage()
{
	std::string message = get_output_header("DOSBOX_HELP_LIST_COUNTRIES_1");

	for (auto it = LocaleData::CountryInfo.begin();
	     it != LocaleData::CountryInfo.end();
	     ++it) {
		message += format_str(
		        "  %-5d - %s\n",
		        enum_val(it->first),
		        MSG_GetTranslatedRaw(it->second.GetMsgName()).c_str());
	}

	message += "\n";
	message += MSG_GetTranslatedRaw("DOSBOX_HELP_LIST_COUNTRIES_2");
	message += "\n";

	return message;
}

std::string DOS_GenerateListKeyboardLayoutsMessage(const bool for_keyb_command)
{
	std::string message = get_output_header("DOSBOX_HELP_LIST_KEYBOARD_LAYOUTS_1",
	                                        for_keyb_command);

	std::string highlight_code = {};
	if (for_keyb_command) {
		highlight_code = DOS_GetLoadedLayout();
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
				column_1_ansi += Ansi::HighlightSelection;
				column_1_ansi += layout_code;
				column_1_ansi += Ansi::Reset;
				highlight = true;
			} else {
				column_1_ansi += layout_code;
			}
		}
		column_1_width = std::max(column_1_width, column_1_raw.length());

		// Column 2 - localized keyboard name/description
		std::string column_2;
		if (for_keyb_command) {
			column_2 = MSG_Get(entry.GetMsgName());
		} else {
			column_2 = MSG_GetTranslatedRaw(entry.GetMsgName());
		}

		table.emplace_back(Row{column_1_ansi, column_2, highlight});
	}

	const size_t column_1_highlighted_width = Ansi::HighlightSelection.size() +
	                                          Ansi::Reset.size() + column_1_width;
	for (auto& table_row : table) {
		if (table_row.highlight) {
			table_row.column1.resize(column_1_highlighted_width, ' ');
			message += format_str("%s*%s %s %s- %s%s\n",
			                      Ansi::HighlightSelection.c_str(),
                                              Ansi::Reset.c_str(),
                                              table_row.column1.c_str(),
			                      Ansi::HighlightSelection.c_str(),
			                      table_row.column2.c_str(),
			                      Ansi::Reset.c_str());
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
		message += MSG_GetTranslatedRaw("DOSBOX_HELP_LIST_KEYBOARD_LAYOUTS_2");
		message += "\n";

		return message;
	}
}

std::string DOS_GenerateListCodePagesMessage()
{
	std::string message = get_output_header("DOSBOX_HELP_LIST_CODE_PAGES_1");

	struct Row {
		std::string column1 = {};
		std::string column2 = {};
		std::string column3 = {};
	};
	std::vector<Row> table = {};

	size_t max_column2_length = 0;

	for (const auto& pack : LocaleData::CodePageInfo) {
		for (const auto& entry : pack) {
			assert(LocaleData::ScriptInfo.contains(entry.second.script));
			const auto script_msg_name =
			        LocaleData::ScriptInfo.at(entry.second.script).GetMsgName();
			const auto page_msg_name = CodePageInfoEntry::GetMsgName(entry.first);

			Row row = {};

			row.column1 = format_str("% 7d - ", entry.first);
			row.column2 = MSG_GetTranslatedRaw(page_msg_name);
			row.column3 = MSG_GetTranslatedRaw(script_msg_name);

			max_column2_length = std::max(max_column2_length,
			                              length_utf8(row.column2));

			table.emplace_back(row);
		}
	}

	for (auto& row : table) {
		std::string align = {};
		align.resize(max_column2_length - length_utf8(row.column2), ' ');
		row.column2 += align;
	}

	for (const auto& row : table) {
		message += row.column1 + row.column2 + "  (" + row.column3 + ")\n";
	}

	message += "\n";
	message += MSG_GetTranslatedRaw("DOSBOX_HELP_LIST_CODE_PAGES_2");
	message += "\n";

	return message;
}

// ***************************************************************************
// Helper functions for KEYB.COM command
// ***************************************************************************

static std::string get_keyboard_layout_name(const std::string& layout,
                                            const bool translated)
{
	const auto layout_deduplicated = deduplicate_layout(layout);

	for (const auto& entry : LocaleData::KeyboardLayoutInfo) {
		assert(!entry.layout_codes.empty());
		if (entry.layout_codes[0] == layout_deduplicated) {
			return translated ? MSG_Get(entry.GetMsgName())
			                  : entry.layout_name;
		}
	}

	return {};
}

std::string DOS_GetKeyboardLayoutName(const std::string& layout)
{
	constexpr bool Translated = true;
	return get_keyboard_layout_name(layout, Translated);
}

std::string DOS_GetEnglishKeyboardLayoutName(const std::string& layout)
{
	constexpr bool Translated = false;
	return get_keyboard_layout_name(layout, Translated);
}

static Script to_script(const KeyboardScript keyboard_script)
{
	switch (keyboard_script) {
	case KeyboardScript::LatinQwerty:
	case KeyboardScript::LatinQwertz:
	case KeyboardScript::LatinAzerty:
	case KeyboardScript::LatinAsertt:
	case KeyboardScript::LatinJcuken:
	case KeyboardScript::LatinUgjrmv:
	case KeyboardScript::LatinColemak:
	case KeyboardScript::LatinDvorak:
	case KeyboardScript::LatinNonStandard:
		return Script::Latin;

	case KeyboardScript::Cyrillic: 
	case KeyboardScript::CyrillicPhonetic:
		return Script::Cyrillic;

	case KeyboardScript::Arabic:   return Script::Arabic;
	case KeyboardScript::Armenian: return Script::Armenian;
	case KeyboardScript::Cherokee: return Script::Cherokee;
	case KeyboardScript::Georgian: return Script::Georgian;
	case KeyboardScript::Greek:    return Script::Greek;
	case KeyboardScript::Hebrew:   return Script::Hebrew;

	default:
		assert(false);
		return Script::Latin;
	}	
}

std::string DOS_GetKeyboardScriptName(const KeyboardScript keyboard_script)
{
	const auto script = to_script(keyboard_script);
	assert(LocaleData::ScriptInfo.contains(script));
	const auto msg_id = LocaleData::ScriptInfo.at(script).GetMsgName();

	std::string result = MSG_Get(msg_id);

	switch (keyboard_script) {
	case KeyboardScript::LatinQwerty:
		return result + " (QWERTY)";
	case KeyboardScript::LatinQwertz:
		return result + " (QWERTZ)";
	case KeyboardScript::LatinAzerty:
		return result + " (AZERTY)";
	case KeyboardScript::LatinAsertt:
		return result + " (ASERTT)";
	case KeyboardScript::LatinJcuken:
		return result + " (JCUKEN)";
	case KeyboardScript::LatinUgjrmv:
		return result + " (UGJRMV)";
	case KeyboardScript::LatinColemak:
		return result + " (Colemak)";
	case KeyboardScript::LatinDvorak:
		return result + " (Dvorak)";

	case KeyboardScript::LatinNonStandard:
		return result + " (" +
		       MSG_Get("SCRIPT_PROPERTY_NON_STANDARD") + ")";
	case KeyboardScript::CyrillicPhonetic:
		return result + " (" +
		       MSG_Get("SCRIPT_PROPERTY_PHONETIC") + ")";

	case KeyboardScript::Arabic:   return result;
	case KeyboardScript::Armenian: return result;
	case KeyboardScript::Cherokee: return result;
	case KeyboardScript::Cyrillic: return result;
	case KeyboardScript::Georgian: return result;
	case KeyboardScript::Greek:    return result;
	case KeyboardScript::Hebrew:   return result;

	default:
		assert(false);
		return "<unknown keyboard script>";
	}
}

std::string DOS_GetShortcutKeyboardScript1()
{
	const std::string Left = MSG_Get("KEYBOARD_MOD_ADJECTIVE_LEFT");

	return Left + MMOD2_NAME + "+" + Left + "Shift";
}

std::string DOS_GetShortcutKeyboardScript2()
{
	const std::string Left  = MSG_Get("KEYBOARD_MOD_ADJECTIVE_LEFT");
	const std::string Right = MSG_Get("KEYBOARD_MOD_ADJECTIVE_RIGHT");

	return Left + MMOD2_NAME + "+" + Right + "Shift";;
}

std::string DOS_GetShortcutKeyboardScript3()
{
	const std::string Left = MSG_Get("KEYBOARD_MOD_ADJECTIVE_LEFT");

	return Left + MMOD2_NAME + "+" + Left + PRIMARY_MOD_NAME;
}

static std::optional<KeyboardLayoutInfoEntry> get_keyboard_layout_info(
        const std::string& layout)
{
	const auto layout_deduplicated = deduplicate_layout(layout);

	for (const auto& entry : LocaleData::KeyboardLayoutInfo) {
		assert(!entry.layout_codes.empty());
		if (entry.layout_codes[0] == layout_deduplicated) {
			return entry;
		}
	}

	return {};
}

std::optional<KeyboardScript> DOS_GetKeyboardLayoutScript1(const std::string& layout)
{
	const auto layout_info = get_keyboard_layout_info(layout);
	if (layout_info) {
		return layout_info->primary_script;
	}

	return {};
}

std::optional<KeyboardScript> DOS_GetKeyboardLayoutScript2(const std::string& layout,
                                                           const uint16_t code_page)
{
	const auto layout_info = get_keyboard_layout_info(layout);
	if (layout_info) {
		auto& list = layout_info->secondary_scripts;
		if (list.contains(code_page)) {
			return list.at(code_page);
		}
	}

	return {};
}

std::optional<KeyboardScript> DOS_GetKeyboardLayoutScript3(const std::string& layout,
                                                           const uint16_t code_page)
{
	const auto layout_info = get_keyboard_layout_info(layout);
	if (layout_info) {
		auto& list = layout_info->tertiary_scripts;
		if (list.contains(code_page)) {
			return list.at(code_page);
		}
	}

	return {};
}

static std::optional<std::pair<uint16_t, CodePageInfoEntry>> get_code_page_info_entry(
        const uint16_t code_page)
{
	for (const auto& pack : LocaleData::CodePageInfo) {
		for (const auto& entry : pack) {
			if (!is_code_page_equal(code_page, entry.first)) {
				continue;
			}
			return entry;
		}
	}

	return {};
}

std::string DOS_GetCodePageDescription(const uint16_t code_page)
{
	const auto entry = get_code_page_info_entry(code_page);
	if (entry) {
		return MSG_Get(CodePageInfoEntry::GetMsgName(entry->first));
	}

	return {};
}

std::string DOS_GetEnglishCodePageDescription(const uint16_t code_page)
{
	const auto entry = get_code_page_info_entry(code_page);
	if (entry) {
		return entry->second.description;
	}

	return {};
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
	// First search for exact matches
	for (const auto& entry : LocaleData::BundledCpiContent) {
		for (const auto& known_code_page : entry.second) {
			if (known_code_page == code_page) {
				return entry.first;
			}
		}
	}

	// Exact match not found, look for known duplicates
	for (const auto& entry : LocaleData::BundledCpiContent) {
		for (const auto& known_code_page : entry.second) {
			if (is_code_page_equal(known_code_page, code_page)) {
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

	LOG_WARNING("LOCALE: No default code page for keyboard layout '%s'",
	            keyboard_layout.c_str());
	return assert_code_page(DefaultCodePage);
}

// ***************************************************************************
// Locale loading
// ***************************************************************************

static void log_country_locale(const bool only_changed_period = false)
{
	const auto& host_locale = GetHostLocale();

	if (!config.country && !only_changed_period) {
		LOG_MSG("LOCALE: Country detected from '%s'",
		        host_locale.country.log_info.c_str());
	}

	std::string country_name = {};
	if (populated.is_country_overriden) {
		assert(guest_country_override);
		country_name = get_country_name_for_log(*guest_country_override);
	} else {
		country_name = get_country_name_for_log(current_country);
	}
	LOG_MSG("LOCALE: Loading %s locale for country %d - '%s'",
	        get_locale_period_for_log(config.locale_period).c_str(),
	        dos.country_code,
	        country_name.c_str());

	if (populated.separate_numeric &&
	    config.locale_period != LocalePeriod::Native) {
		const auto extra_country = *populated.separate_numeric;
		LOG_MSG("LOCALE: Using numeric format for country %d - '%s', detected from '%s'",
		        enum_val(extra_country),
		        get_country_name_for_log(extra_country).c_str(),
		        host_locale.numeric.log_info.c_str());
	}

	if (populated.separate_time_date &&
	    config.locale_period != LocalePeriod::Native) {
		const auto extra_country = *populated.separate_time_date;
		LOG_MSG("LOCALE: Using time/date format for country %d - '%s', detected from '%s'",
		        enum_val(extra_country),
		        get_country_name_for_log(extra_country).c_str(),
		        host_locale.time_date.log_info.c_str());
	}

	if (populated.separate_currency &&
	    config.locale_period != LocalePeriod::Native) {
		const auto extra_country = *populated.separate_currency;
		LOG_MSG("LOCALE: Using currency format for country %d - '%s', detected from '%s'",
		        enum_val(extra_country),
		        get_country_name_for_log(extra_country).c_str(),
		        host_locale.currency.log_info.c_str());
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

		LOG_MSG("LOCALE: Using %s format detected from host OS",
		        message.c_str());
	}

	if (populated.is_using_fallback_period &&
	    config.locale_period != LocalePeriod::Native) {
		LOG_WARNING("LOCALE: No period correct country data found");
	}
}

static void load_country()
{
	assert(dos.tables.country);
	config.country = {};

	// Parse the config file value
	if (!config.country_str.empty() && config.country_str != "auto") {
		const auto value = parse_int(config.country_str);
		if (value && *value >= 0 && *value <= UINT16_MAX &&
		    is_country_supported(static_cast<DosCountry>(*value))) {
			config.country = static_cast<DosCountry>(*value);
		} else {
			// NOTE: In such case MS-DOS 6.22 uses modified locale,
			// it uses country 1 with date separator '-' instead of
			// '/'. This behavior is not emulated - I believe it is
			// due to the hardcoded locale being slightly different
			// than the one from COUNTRY.SYS (likely MS-DOS bug).
			LOG_WARNING("LOCALE: '%s' is not a valid country code, using autodetected",
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
		if (host_locale.country.country_code) {
			current_country = *host_locale.country.country_code;
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

static void preprocess_arabic_keyboard_layouts(
        std::vector<KeyboardLayoutMaybeCodepage>& keyboard_layouts)
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
			processed.emplace_back(
			        KeyboardLayoutMaybeCodepage{Ar470, source.code_page});
		}
		if (source.keyboard_layout == Ar470 && votes_ar462 > votes_ar470) {
			processed.emplace_back(
			        KeyboardLayoutMaybeCodepage{Ar462, source.code_page});
		}
		processed.emplace_back(source);
	}

	keyboard_layouts = processed;
}

static void sort_detected_keyboard_layouts(
        std::vector<KeyboardLayoutMaybeCodepage>& keyboard_layouts,
        const bool is_layout_list_sorted)
{
	// Get the relevant keyboard layout information into a local std::map
	std::map<std::string, KeyboardLayoutInfoEntry> info_map = {};
	for (const auto& detected : keyboard_layouts) {
		const auto detected_deduplicated = deduplicate_layout(
		        detected.keyboard_layout);
		for (const auto& entry : LocaleData::KeyboardLayoutInfo) {
			for (const auto& layout_code : entry.layout_codes) {
				if (layout_code == detected_deduplicated) {
					info_map[layout_code] = entry;
					break;
				}
			}
		}
		assert(info_map.contains(detected_deduplicated));
	}

	// Fetch the keyboard layouts matching the GUI language
	std::vector<std::set<std::string>> layouts_matching_gui = {};
	auto maybe_add_matching_language = [&](const LanguageTerritory& language) {
		constexpr size_t MaxMatchingSets = 30;

		if (layouts_matching_gui.size() >= MaxMatchingSets) {
			return;
		}

		// Skip this criteria if the host GUI language is either generic
		// or English; tech-savvy folks often prefer the original GUI
		// than a translated one
		if (language.IsEmpty() || language.IsEnglish() || language.IsGeneric()) {
			return;
		}

		layouts_matching_gui.emplace_back();
		for (const auto &layout : language.GetMatchingKeyboardLayouts()) {
			layouts_matching_gui.back().emplace(deduplicate_layout(layout));
		}
	};

	// Always try to match the loaded translation language
	maybe_add_matching_language(MSG_GetLanguage());

	// If the host OS does not allow the user to prioritize the keyboard
	// layouts, try to also match the GUI language(s)
	if (!is_layout_list_sorted) {
		for (const auto& language : GetHostLanguages().gui_languages) {
			maybe_add_matching_language(language);
		}
	}

	// The highest value returned, the better the keyboard layout matches
	// the GUI language settings
	auto get_gui_match_score = [&](const KeyboardLayoutMaybeCodepage& entry) {
		uint32_t score = 0;

		const auto& info_entry = info_map.at(deduplicate_layout(entry.keyboard_layout));
		assert(!info_entry.layout_codes.empty());
		const auto layout_code = deduplicate_layout(info_entry.layout_codes[0]);

		for (const auto& layouts : layouts_matching_gui) {
			score = score * 2;
			if (layouts.contains(layout_code)) {
				score++;
			}
		}

		return score;
	};

	auto get_layout_priority = [&](const KeyboardLayoutMaybeCodepage& entry) {
		const auto& info_entry = info_map.at(
		        deduplicate_layout(entry.keyboard_layout));
		return info_entry.priority;
	};

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

	auto has_code_page = [&](const KeyboardLayoutMaybeCodepage& entry) {
		return entry.code_page && (*entry.code_page != DefaultCodePage);
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

			// Do not use fuzzy mappings if others are available
			if (!l.is_mapping_fuzzy && r.is_mapping_fuzzy) {
				return true;
			} else if (l.is_mapping_fuzzy && !r.is_mapping_fuzzy) {
				return false;
			}

			// Prefer keyboard layouts matching the GUI language
			const auto l_gui_match_score = get_gui_match_score(l);
			const auto r_gui_match_score = get_gui_match_score(r);
			if (l_gui_match_score != r_gui_match_score) {
				return (l_gui_match_score > r_gui_match_score);
			}

			// Skip remaining criteria if the host OS provided us
			// with the list already sorted by user preference
			if (is_layout_list_sorted) {
				return false;
			}

			// Prefer safer keyboard layout choices
			const auto l_priority = get_layout_priority(l);
			const auto r_priority = get_layout_priority(r);
			if (l_priority != r_priority) {
				return (l_priority > r_priority);
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
			const bool l_has_code_page = has_code_page(l);
			const bool r_has_code_page = has_code_page(r);
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
	const auto& host_locale = GetHostKeyboardLayouts();

	auto keyboard_layouts = host_locale.keyboard_layout_list;
	if (keyboard_layouts.empty()) {
		LOG_MSG("LOCALE: Could not detect host keyboard layouts");
		return {};
	}

	assert(!host_locale.log_info.empty());
	LOG_MSG("LOCALE: Keyboard layout and code page detected from '%s'",
	        host_locale.log_info.c_str());

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

	sort_detected_keyboard_layouts(keyboard_layouts,
	                               host_locale.is_layout_list_sorted);
	preprocess_arabic_keyboard_layouts(keyboard_layouts);

	return keyboard_layouts;
}

static void load_keyboard_layout()
{
	std::vector<KeyboardLayoutMaybeCodepage> keyboard_layouts = {};

	bool using_detected     = false; // if layout list is autodetected
	bool code_page_supplied = false; // if code page given in the parameter

	const auto tokens = split(config.keyboard_str);
	if (tokens.size() > 2) {
		LOG_WARNING("LOCALE: Invalid 'keyboard_layout' setting: '%s', using 'auto'",
		            config.keyboard_str.c_str());
		set_section_property_value("dos", "keyboard_layout", "auto");
		config.keyboard_str = "auto";
	}

	if (tokens.empty() || config.keyboard_str == "auto") {
		keyboard_layouts = get_detected_keyboard_layouts();
		using_detected   = true;
	} else {
		keyboard_layouts.emplace_back(KeyboardLayoutMaybeCodepage{tokens[0]});
		if (tokens.size() == 2) {
			const auto result = parse_int(tokens[1]);
			if (!result || *result < 1 || *result > UINT16_MAX) {
				LOG_WARNING("LOCALE: Invalid 'keyboard_layout' code page: '%s', ignoring",
				            tokens[1].c_str());
			} else {
				keyboard_layouts[0].code_page =
				        static_cast<uint16_t>(*result);
				code_page_supplied = true;
			}
		}
	}

	// Apply the code page
	KeyboardLayoutResult result = KeyboardLayoutResult::LayoutNotKnown;
	const bool prefer_rom_font  = using_detected || !code_page_supplied;
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
			LOG_WARNING("LOCALE: Unable to use 'keyboard_layout' code page %d, ignoring",
			            *keyboard_layouts[0].code_page);
			uint16_t tried_code_page = 0;
			result = DOS_LoadKeyboardLayout(keyboard_layouts[0].keyboard_layout,
			                                tried_code_page,
			                                {},
			                                prefer_rom_font);
			if (result != KeyboardLayoutResult::OK) {
				LOG_WARNING("LOCALE: Unable to use specified 'keyboard_layout' setting: '%s', using 'us'",
				            config.keyboard_str.c_str());
			}
		} else if (result == KeyboardLayoutResult::IncompatibleMachine) {
			LOG_WARNING("LOCALE: Invalid 'keyboard_layout' setting: '%s' for this display adapter, using 'us'",
			            config.keyboard_str.c_str());
		} else {
			LOG_WARNING("LOCALE: Unable to use specified 'keyboard_layout' setting: '%s', using 'us'",
			            config.keyboard_str.c_str());
		}
	}

	// Make sure some keyboard layout is actually set
	if (DOS_GetLoadedLayout().empty()) {
		constexpr bool PreferRomFont = true;
		uint16_t tried_code_page = 0;
		DOS_LoadKeyboardLayout("us", tried_code_page, {}, PreferRomFont);
	}
}

// ***************************************************************************
// Lifecycle
// ***************************************************************************

class DOS_Locale {
public:
	DOS_Locale(SectionProp& section);
	~DOS_Locale() = default;
};

DOS_Locale::DOS_Locale(SectionProp& section)
{
	if (!config.is_config_loaded) {
		dos.loaded_codepage    = DefaultCodePage;
		// Make sure the locale detection is done in a predictable place
		GetHostLocale();
	}

	LocalePeriod locale_period = {};

	const auto period_str = section.GetString("locale_period");
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
	const auto country_str = section.GetString("country");
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
		config.keyboard_str = section.GetString("keyboard_layout");
		load_keyboard_layout();
	}

	config.is_config_loaded = true;
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

	MSG_Add("DOSBOX_HELP_LIST_CODE_PAGES_1",
	        "List of available code pages");
	MSG_Add("DOSBOX_HELP_LIST_CODE_PAGES_2",
	        "The above code pages can be used in the 'keyboard_layout' config setting.");

	// Add strings with country names
	for (auto it = LocaleData::CountryInfo.begin();
	     it != LocaleData::CountryInfo.end();
	     ++it) {
		MSG_Add(it->second.GetMsgName(), it->second.country_name);
	}

	// Add strings with code page descriptions
	for (const auto& pack : LocaleData::CodePageInfo) {
		for (const auto& entry : pack) {
			MSG_Add(CodePageInfoEntry::GetMsgName(entry.first),
			        entry.second.description);
		}
	}

	// Add strings with script names
	for (const auto& entry : LocaleData::ScriptInfo) {
		MSG_Add(entry.second.GetMsgName(), entry.second.script_name);
	}

	MSG_Add("SCRIPT_PROPERTY_PHONETIC", "phonetic");
	MSG_Add("SCRIPT_PROPERTY_NON_STANDARD", "non-standard");

	// Add strings with keyboard layout names
	for (const auto& entry : LocaleData::KeyboardLayoutInfo) {
		MSG_Add(entry.GetMsgName(), entry.layout_name);
	}

	MSG_Add("KEYBOARD_MOD_ADJECTIVE_LEFT",  "Left");
	MSG_Add("KEYBOARD_MOD_ADJECTIVE_RIGHT", "Right");
}

static std::unique_ptr<DOS_Locale> dos_locale = {};

void DOS_Locale_Init(SectionProp& section)
{
	dos_locale = std::make_unique<DOS_Locale>(section);
}

void DOS_Locale_Destroy()
{
	dos_locale = {};
}

