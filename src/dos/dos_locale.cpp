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
#include "misc/logging.h"
#include "misc/unicode.h"
#include "utils/bitops.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

// ***************************************************************************
// Handling DOS country info structure
// ***************************************************************************

// TODO Probably not needed anymore now that we have the per config setting
// change notification mechanism to avoid full module reinits. This workaround
// could probably be simplified.
//
static struct {
	// If the config file settings were read
	bool is_config_loaded = false;

	// Settings exactly as retrieved from config file
	std::string country_str  = {};
	std::string keyboard_str = {};

	// Config file settings interpretation
	LocalePeriod locale_period = LocalePeriod::Modern;
} config;

// Status set when populating DOS data
static struct Populated {
	bool is_country_international = false;
	bool is_country_overriden     = false;
	bool is_using_fallback_period = false;
} populated;

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
constexpr size_t ReservedAreaSize        = 10;

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
	case LocalePeriod::Modern: return "modern";
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

		populated.is_country_overriden     = true;
		populated.is_country_international = false;
	}
}

// Only to be called from within 'populate_all_country_info'!
static void populate_numeric_format(const LocaleInfoEntry& source)
{
	const auto& destination = dos.tables.country;

	size_t offset           = InfoOffsetThousandsSeparator;
	destination[offset + 0] = enum_val(source.thousands_separator);
	destination[offset + 1] = 0;

	offset                  = InfoOffsetDecimalSeparator;
	destination[offset + 0] = enum_val(source.decimal_separator);
	destination[offset + 1] = 0;

	offset                  = InfoOffsetListSeparator;
	destination[offset + 0] = enum_val(source.list_separator);

	destination[offset + 1] = 0;
}

// Only to be called from within 'populate_all_country_info'!
static void populate_time_date_format(const LocaleInfoEntry& source)
{
	const auto& destination = dos.tables.country;

	size_t offset       = InfoOffsetTimeFormat;
	destination[offset] = enum_val(source.time_format);

	offset                  = InfoOffsetTimeSeparator;
	destination[offset + 0] = enum_val(source.time_separator);
	destination[offset + 1] = 0;

	offset                  = InfoOffsetDateFormat;
	destination[offset + 0] = enum_val(source.date_format);
	destination[offset + 1] = 0;

	offset                  = InfoOffsetDateSeparator;
	destination[offset + 0] = enum_val(source.date_separator);
	destination[offset + 1] = 0;
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

	size_t offset       = InfoOffsetCurrencyFormat;
	destination[offset] = enum_val(source.currency_format);

	if (!found) {
		// Fallback - use the currency code instead
		memcpy(&destination[InfoOffsetCurrencySymbol],
		       source.currency_code.c_str(),
		       source.currency_code.length());

		// Force separation between symbol and amount
		bit::set(destination[offset], bit::literals::b0);
	}

	offset              = InfoOffsetCurrencyPrecision;
	destination[offset] = source.currency_precision;
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

		if (country_info.locale_info.contains(locale_period)) {
			return {locale_period,
			        country_info.locale_info.at(locale_period)};

		} else {
			const auto iter = country_info.locale_info.begin();
			return {iter->first, iter->second};
		}
	};

	populated = Populated();
	populate_country_code();

	// Populate numeric format
	const auto& [period, info] = populated.is_country_international
	                                   ? get_info(DosCountry::International)
	                                   : get_info(static_cast<DosCountry>(
	                                             dos.country_code));

	populate_numeric_format(info);

	if (period != config.locale_period) {
		populated.is_using_fallback_period = true;
	}

	// Populate time/date format
	populate_time_date_format(info);

	if (period != config.locale_period) {
		populated.is_using_fallback_period = true;
	}

	// Populate currency format
	populate_currency_format(info);

	if (period != config.locale_period) {
		populated.is_using_fallback_period = true;
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
		// not a valid value for DOS int 21h call
		return false;
	}

	const auto country = static_cast<DosCountry>(country_id);
	if (!is_country_supported(country)) { // requested country not supported
		return false;
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

			const auto page_msg_name = CodePageInfoEntry::GetMsgName(
			        entry.first);

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
	case KeyboardScript::LatinNonStandard: return Script::Latin;

	case KeyboardScript::Cyrillic:
	case KeyboardScript::CyrillicPhonetic: return Script::Cyrillic;

	case KeyboardScript::Arabic: return Script::Arabic;
	case KeyboardScript::Armenian: return Script::Armenian;
	case KeyboardScript::Cherokee: return Script::Cherokee;
	case KeyboardScript::Georgian: return Script::Georgian;
	case KeyboardScript::Greek: return Script::Greek;
	case KeyboardScript::Hebrew: return Script::Hebrew;

	default: assert(false); return Script::Latin;
	}
}

std::string DOS_GetKeyboardScriptName(const KeyboardScript keyboard_script)
{
	const auto script = to_script(keyboard_script);
	assert(LocaleData::ScriptInfo.contains(script));
	const auto msg_id = LocaleData::ScriptInfo.at(script).GetMsgName();

	std::string result = MSG_Get(msg_id);

	switch (keyboard_script) {
	case KeyboardScript::LatinQwerty: return result + " (QWERTY)";

	case KeyboardScript::LatinQwertz: return result + " (QWERTZ)";

	case KeyboardScript::LatinAzerty: return result + " (AZERTY)";

	case KeyboardScript::LatinAsertt: return result + " (ASERTT)";

	case KeyboardScript::LatinJcuken: return result + " (JCUKEN)";

	case KeyboardScript::LatinUgjrmv: return result + " (UGJRMV)";

	case KeyboardScript::LatinColemak: return result + " (Colemak)";

	case KeyboardScript::LatinDvorak: return result + " (Dvorak)";

	case KeyboardScript::LatinNonStandard:
		return result + " (" + MSG_Get("SCRIPT_PROPERTY_NON_STANDARD") + ")";

	case KeyboardScript::CyrillicPhonetic:
		return result + " (" + MSG_Get("SCRIPT_PROPERTY_PHONETIC") + ")";

	case KeyboardScript::Arabic: return result;
	case KeyboardScript::Armenian: return result;
	case KeyboardScript::Cherokee: return result;
	case KeyboardScript::Cyrillic: return result;
	case KeyboardScript::Georgian: return result;
	case KeyboardScript::Greek: return result;
	case KeyboardScript::Hebrew: return result;

	default: assert(false); return "<unknown keyboard script>";
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

	return Left + MMOD2_NAME + "+" + Right + "Shift";
	;
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

static void log_country_locale()
{
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

	if (populated.is_using_fallback_period &&
	    config.locale_period != LocalePeriod::Native) {
		LOG_WARNING("LOCALE: No period correct country data found");
	}
}

static void load_country()
{
	assert(dos.tables.country);

	// Parse the config file value
	if (!config.country_str.empty()) {
		const auto value = parse_int(config.country_str);

		if (value && *value >= 0 && *value <= UINT16_MAX &&
		    is_country_supported(static_cast<DosCountry>(*value))) {

			current_country = static_cast<DosCountry>(*value);

		} else {
			// NOTE: In such case MS-DOS 6.22 uses modified locale,
			// it uses country 1 with date separator '-' instead of
			// '/'. This behavior is not emulated - I believe it is
			// due to the hardcoded locale being slightly different
			// than the one from COUNTRY.SYS (likely MS-DOS bug).
			LOG_WARNING("LOCALE: '%s' is not a valid country code, using 'us'",
			            config.country_str.c_str());

			current_country = DosCountry::UnitedStates;
			set_section_property_value("dos", "country", "1");
		}
	}

	guest_country_override = {};
	populate_all_country_info();
	log_country_locale();
}

static void change_locale_period()
{
	populate_all_country_info();
	log_country_locale();
}

static void load_keyboard_layout()
{
	std::string keyboard_layout       = {};
	std::optional<uint16_t> code_page = {};

	const auto tokens = split(config.keyboard_str);
	if (tokens.size() > 2) {
		LOG_WARNING("LOCALE: Invalid 'keyboard_layout' setting: '%s', using 'us'",
		            config.keyboard_str.c_str());

		set_section_property_value("dos", "keyboard_layout", "us");
		config.keyboard_str = "us";
	}

	keyboard_layout = tokens[0];

	if (tokens.size() == 2) {
		const auto result = parse_int(tokens[1]);

		if (!result || *result < 1 || *result > UINT16_MAX) {
			LOG_WARNING("LOCALE: Invalid 'keyboard_layout' code page: '%s', ignoring",
			            tokens[1].c_str());
		} else {
			code_page = static_cast<uint16_t>(*result);
		}
	}

	// Apply the code page
	auto result = KeyboardLayoutResult::LayoutNotKnown;

	const bool prefer_rom_font = !code_page;
	uint16_t tried_code_page   = 0;

	if (code_page) {
		tried_code_page = *code_page;
	}
	result = DOS_LoadKeyboardLayout(keyboard_layout, tried_code_page, {}, prefer_rom_font);

	// If failed to set user provided settings, print out warning
	if (result != KeyboardLayoutResult::OK) {

		// We have tried to set the user-requested keyboard layout, but
		// something went wrong
		if (code_page &&
		    (result == KeyboardLayoutResult::NoBundledCpiFileForCodePage ||
		     result == KeyboardLayoutResult::LayoutNotKnown)) {

			// Retry without code page
			LOG_WARNING("LOCALE: Unable to use 'keyboard_layout' code page %d, ignoring",
			            *code_page);

			uint16_t tried_code_page = 0;

			result = DOS_LoadKeyboardLayout(keyboard_layout,
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
		uint16_t tried_code_page     = 0;
		DOS_LoadKeyboardLayout("us", tried_code_page, {}, PreferRomFont);

		set_section_property_value("dos", "keyboard_layout", "us");
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
		dos.loaded_codepage = DefaultCodePage;
	}

	LocalePeriod locale_period = {};

	const auto period_str = section.GetString("locale_period");

	if (period_str == "modern") {
		locale_period = LocalePeriod::Modern;

	} else if (period_str == "historic") {
		locale_period = LocalePeriod::Historic;

	} else {
		assertm(false, "Invalid locale_period setting");
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

	MSG_Add("DOSBOX_HELP_LIST_CODE_PAGES_1", "List of available code pages");

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

	MSG_Add("KEYBOARD_MOD_ADJECTIVE_LEFT", "Left");
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

