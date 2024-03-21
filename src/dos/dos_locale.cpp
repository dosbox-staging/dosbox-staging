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
#include "dos_keyboard_layout.h"
#include "dos_locale.h"
#include "logging.h"
#include "string_utils.h"
#include "../misc/host_locale.h"

CHECK_NARROWING();

using namespace LocaleData;

// ***************************************************************************
// Handling DOS country info structure
// ***************************************************************************

static struct {
	// If the config file settings were read
	bool is_config_loaded = false;

	// These variables store settings exactly as retrieved from config file
	std::string language_config_str = {};
	std::string country_config_str  = {};
	std::string keyboard_config_str = {};

	// XXX



	LocalePeriod locale_period = LocalePeriod::Modern;

	DosCountry country        = DosCountry::UnitedStates;
	uint16_t country_dos_code = enum_val(country);
	// If the locale has been generated to DOS table
	bool is_locale_generated = false;

	// If country in the configuration was set to 'auto'
	bool auto_detect_country = false;
} config;

static HostLocale host_locale = {};

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

static DosCountry deduplicate_country(const DosCountry country)
{
	// Correct country code to handle duplicates in DOS country numbers
	if (CodeToCountryCorrectionMap.contains(enum_val(country))) {
		return CodeToCountryCorrectionMap.at(enum_val(country));
	} else {
		return country;
	}
}

static std::string deduplicate_layout(const std::string &layout)
{
	for (const auto &layout_info : KeyboardLayoutInfo)
	{
		for (const auto &entry : layout_info.layout_codes) {
			if (layout == entry) {
				return layout_info.layout_codes[0];
			}
		}
	}

	return layout;
}

static std::string get_country_name_for_log(const DosCountry country)
{
	const auto country_deduplicated = deduplicate_country(country);

	if (CountryData.contains(country_deduplicated)) {
		return CountryData.at(country_deduplicated).country_name;
	}

	return "<unknown country>";
}

// XXX implement get_layout_name_for_log

static void set_country_dos_code()
{
	if (config.country == DosCountry::International) {
		// MS-DOS uses the same country code for International English
		// and Australia - we don't, as we have different settings for
		// these. Let's imitate MS-DOS behavior.
		config.country_dos_code = enum_val(DosCountry::Australia);
	} else {
		config.country_dos_code = enum_val(config.country);
	}
}

static void maybe_log_changed_country(const std::string &country_name,
                                      const LocalePeriod actual_period)
{
	static bool already_logged = false;

	static DosCountry logged_country         = {};
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

static void refresh_time_date_format(const LocaleInfoEntry &source) // XXX rename to 'populate'
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

void DOS_RefreshCountryInfo(const bool keyboard_layout_changed) // XXX needs a rework
{
	if (!config.is_config_loaded) {
		return;
	}

	if (config.auto_detect_country && !config.is_locale_generated &&
	    !keyboard_layout_changed) {
		return;
	}

	set_country_dos_code();
	const auto country_deduplicated = deduplicate_country(config.country);

	assert(CountryData.contains(country_deduplicated));
	const auto& country_info = CountryData.at(country_deduplicated);

	// Select locale period

	LocalePeriod locale_period = LocalePeriod::Modern;
	if (config.locale_period == LocalePeriod::Historic &&
	    country_info.locale_info.contains(config.locale_period)) {
		locale_period = config.locale_period;
	}

	assert(country_info.locale_info.contains(locale_period));
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

static bool set_country(const DosCountry country, const bool no_fallback = false)
{
	if (!dos.tables.country) {
		assert(false);
		return false;
	}

	// Validate country ID
	const auto country_deduplicated = deduplicate_country(country);
	if (!CountryData.contains(country_deduplicated)) {
		if (no_fallback) {
			return false;
		}

		const auto default_country = static_cast<DosCountry>(
		        DOS_GetDefaultCountry());
		LOG_WARNING("DOS: No locale info for country %d, using default %d",
		            enum_val(country),
		            enum_val(default_country));

		if (!CountryData.contains(default_country)) {
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
	return set_country(static_cast<DosCountry>(country_id), no_fallback);
}

uint16_t DOS_GetCountry()
{
	return config.country_dos_code;
}

uint16_t DOS_GetDefaultCountry()
{
	if (config.locale_period == LocalePeriod::Historic) {
		return enum_val(DosCountry::UnitedStates);
	} else {
		return enum_val(DosCountry::International);
	}
}


// ***************************************************************************
// Helper functions for 'dosbox --list-*' commands
// ***************************************************************************

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

std::string DOS_GenerateListKeyboardLayoutsMessage()
{
	std::string message = "\n";
	message += MSG_GetRaw("DOSBOX_HELP_LIST_KEYBOARD_LAYOUTS_1");
	message += "\n\n";

	std::vector<std::pair<std::string, std::string>> table = {};

	size_t column_1_width = 0;
	for (const auto &entry : KeyboardLayoutInfo) {
		// Column 1 - keyboard codes
		std::string column_1 = {};
		for (const auto &layout_code : entry.layout_codes) {
			if (!column_1.empty()) {
				column_1 += ", ";
			}
			column_1 += layout_code;
		}
		column_1_width = std::max(column_1_width, column_1.length());

		// Column 2 - localized keyboard name/description
		const auto column_2 = MSG_GetRaw(entry.GetMsgName().c_str());

		table.push_back({ column_1, column_2});
	}

	for (auto &table_row : table) {
		table_row.first.resize(column_1_width, ' ');
		message += format_str("  %s - %s\n",
		                      table_row.first.c_str(),
		                      table_row.second.c_str());
	}

	message += "\n";
	message += MSG_GetRaw("DOSBOX_HELP_LIST_KEYBOARD_LAYOUTS_2");
	message += "\n";

	return message;
}

// ***************************************************************************
// Helper functions for KEYB.COM command
// ***************************************************************************

std::string DOS_GetKeyboardLayoutName(const std::string &layout)
{
	const auto layout_deduplicated = deduplicate_layout(layout);

	for (const auto &entry : KeyboardLayoutInfo) {
		assert(!entry.layout_codes.empty());
		if (entry.layout_codes[0] == layout_deduplicated) {
			return MSG_Get(entry.GetMsgName().c_str());
		}
	}

	return {};
}

std::string DOS_GetKeyboardScriptName(const KeyboardScript script)
{
	switch (script)
	{
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
	case KeyboardScript::Arabic:
		return MSG_Get("SCRIPT_ARABIC");
	case KeyboardScript::Armenian:
		return MSG_Get("SCRIPT_ARMENIAN");
	case KeyboardScript::Cherokee:
		return MSG_Get("SCRIPT_CHEROKEE");
	case KeyboardScript::Cyrillic:
		return MSG_Get("SCRIPT_CYRILLIC");
	case KeyboardScript::CyrillicPhonetic:
		return std::string(MSG_Get("SCRIPT_CYRILLIC")) + " (" +
		       MSG_Get("SCRIPT_PROPERTY_PHONETIC") + ")";
	case KeyboardScript::Georgian:
		return MSG_Get("SCRIPT_GEORGIAN");
	case KeyboardScript::Greek:
		return MSG_Get("SCRIPT_GREEK");
	case KeyboardScript::Hebrew:
		return MSG_Get("SCRIPT_HEBREW");
	case KeyboardScript::Thai:
		return MSG_Get("SCRIPT_THAI");
	default:
		assert(false);
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

std::optional<KeyboardScript> DOS_GetKeyboardLayoutScript1(const std::string &layout)
{
	const auto layout_deduplicated = deduplicate_layout(layout);

	for (const auto &entry : KeyboardLayoutInfo) {
		assert(!entry.layout_codes.empty());
		if (entry.layout_codes[0] == layout_deduplicated) {
			return entry.primary_script;
		}
	}

	return {};
}

std::optional<KeyboardScript> DOS_GetKeyboardLayoutScript2(const std::string &layout,
                                                           const uint16_t code_page)
{
	const auto layout_deduplicated = deduplicate_layout(layout);

	for (const auto &entry : KeyboardLayoutInfo) {
		assert(!entry.layout_codes.empty());
		if (entry.layout_codes[0] == layout_deduplicated) {
			const auto &list = entry.secondary_scripts;
			if (list.contains(code_page)) {
				return list.at(code_page);
			}
		}
	}

	return {};
}

std::optional<KeyboardScript> DOS_GetKeyboardLayoutScript3(const std::string &layout,
                                                           const uint16_t code_page)
{
	// XXX deduplicate with function above!

	const auto layout_deduplicated = deduplicate_layout(layout);

	for (const auto &entry : KeyboardLayoutInfo) {
		assert(!entry.layout_codes.empty());
		if (entry.layout_codes[0] == layout_deduplicated) {
			const auto &list = entry.tertiary_scripts;
			if (list.contains(code_page)) {
				return list.at(code_page);
			}
		}
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
// Lifecycle
// ***************************************************************************

static void load_keyboard_layout(const std::string& keyboard_config_str)
{
	// XXX
}

static void load_country(const std::string& country_config_str,
                         const LocalePeriod locale_period)
{
	config.locale_period = locale_period;

	if (country_config_str == "auto") {
		// XXX
	}

	// XXX
}

static void load_locale_period(const LocalePeriod locale_period)
{
	// XXX
}

static void load_language(const std::string& language_config_str)
{
	// XXX
}

class DOS_Locale final : public Module_base {
public:
	DOS_Locale(Section* configuration);
	~DOS_Locale() = default;
};

DOS_Locale::DOS_Locale(Section* configuration) : Module_base(configuration)
{
	auto section = static_cast<Section_prop*>(configuration);
	assert(section);

	// Retrieve locale configuration and host settings

	if (!config.is_config_loaded) {
		// Must be the first time - gather host locale information
		host_locale = DetectHostLocale();
	}

	const auto keyboard_config_str = section->Get_string("keyboardlayout");
	const auto country_config_str  = section->Get_string("country");
	const auto period_config_str   = section->Get_string("locale_period");
	// XXX forward autodetected country to messages subsystems

	LocalePeriod locale_period = {};
	if (period_config_str == "modern") {
		locale_period = LocalePeriod::Modern;
	} else if (period_config_str == "historic") {
		locale_period = LocalePeriod::Historic;
	} else {
		assert(false);
	}

	const bool is_keyboard_changed = !config.is_config_loaded ||
	    keyboard_config_str != config.keyboard_config_str;
	const bool is_country_changed = !config.is_config_loaded ||
	    country_config_str != config.country_config_str;
	const bool is_period_changed = !config.is_config_loaded ||
	    locale_period != config.locale_period;

	config.is_config_loaded = true;

	// Apply keyboard layout and code page

	if (is_keyboard_changed) {
		load_keyboard_layout(keyboard_config_str);
	}

	// Apply country and locale period

	if (is_country_changed) {
		load_country(country_config_str, locale_period);
	} else if (is_period_changed) {
		load_locale_period(locale_period);
	}

	// XXX just for testing
	/*
	LOG_ERR("XXX Language '%s', autodetected from host '%s' setting",
	        host_locale.language.c_str(),
	        host_locale.log_info.language.c_str());
	LOG_ERR("XXX Keyboard '%s', autodetected from host '%s' setting",
	        host_locale.keyboard.c_str(),
	        host_locale.log_info.keyboard.c_str());
	LOG_ERR("XXX Country #%d, autodetected from host '%s' setting", // XXX we need a better logging function, using numeric, time_date, currency
	        enum_val(host_locale.country),
	        host_locale.log_info.country.c_str());
	*/

	// XXX add warning about code page 60258 and dotted I'









	config.locale_period = locale_period;

	if (country_config_str == "auto") {
		config.auto_detect_country = true;
		// Autoselection is going to be performed once
		// keyboard layout is loaded
	} else {
		config.auto_detect_country = false;
		const auto property_value  = parse_int(country_config_str);
		DosCountry country            = {};
		if (!property_value || (*property_value < 0) ||
		    (*property_value > UINT16_MAX)) {
			LOG_WARNING("DOS: '%s' is not a valid country code, using default",
			            country_config_str.c_str());
			// NOTE: Real MS-DOS 6.22 uses modified locale
			// in this case, it uses country 1 with date
			// separator '-' instead of '/'.
			// This is not simulated, I believe it is due to
			// hardcoded locale being slightly different
			// than the one from COUNTRY.SYS (MS-DOS bug)
			country = static_cast<DosCountry>(DOS_GetDefaultCountry());
		} else {
			country = static_cast<DosCountry>(*property_value);
		}
		set_country(country);
	}
}




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
	        "The above codes can be used in the 'country' config setting.");

	MSG_Add("DOSBOX_HELP_LIST_KEYBOARD_LAYOUTS_1",
	        "List of keyboard layout codes\n"
	        "-----------------------------");
	MSG_Add("DOSBOX_HELP_LIST_KEYBOARD_LAYOUTS_2",
	        "The above codes can be used in the 'keyboardlayout' config setting.");

	// Add strings with country names
	for (auto it = CountryData.begin(); it != CountryData.end(); ++it) {
		MSG_Add(it->second.GetMsgName().c_str(),
		        it->second.country_name.c_str());
	}

	// Add strings with keybaord layout names
	for (const auto &entry : KeyboardLayoutInfo) {
		MSG_Add(entry.GetMsgName().c_str(), entry.layout_name.c_str());
	}

	MSG_Add("KEYBOARD_SHORTCUT_SCRIPT_1", "Alt+LeftShift");
	MSG_Add("KEYBOARD_SHORTCUT_SCRIPT_2", "Alt+RightShift");
	MSG_Add("KEYBOARD_SHORTCUT_SCRIPT_3", "Alt+LeftCtrl");

	MSG_Add("SCRIPT_LATIN",    "Latin");	
	MSG_Add("SCRIPT_ARABIC",   "Arabic");
	MSG_Add("SCRIPT_ARMENIAN", "Armenian");
	MSG_Add("SCRIPT_CHEROKEE", "Cherokee");
	MSG_Add("SCRIPT_CYRILLIC", "Cyrillic");
	MSG_Add("SCRIPT_GEORGIAN", "Georgian");
	MSG_Add("SCRIPT_GREEK",    "Greek");
	MSG_Add("SCRIPT_HEBREW",   "Hebrew");
	MSG_Add("SCRIPT_THAI",     "Thai");

	MSG_Add("SCRIPT_PROPERTY_PHONETIC",     "phonetic");
	MSG_Add("SCRIPT_PROPERTY_NON_STANDARD", "non-standard");


}

/* XXX 'in keyboardlayout ='', add information about dual-script and --list-layouts:
[ALT]+[LEFT_SHIFT]
[ALT]+[RIGHT_SHIFT]
[ALT]+[LEFT_CTRL]
*/
