// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_HOST_LOCALE_H
#define DOSBOX_HOST_LOCALE_H

#include "../dos/dos_locale.h"

#include <cstdint>
#include <optional>
#include <vector>

// Convert language and territory (using 'ISO 3166-1 alpha-2' codes) to a DOS
// country code
std::optional<DosCountry> iso_to_dos_country(const std::string& language,
                                             const std::string& territory);

// Convert language and territory (using 'ISO 3166-1 alpha-2' codes, also
// supports POSIX dummy languages) to a language file name (without extension)
std::string iso_to_language_file(const std::string& language,
                                 const std::string& territory);

struct KeyboardLayoutMaybeCodepage {
	// Keyboard layout, as supported by the FreeDOS
	std::string keyboard_layout = {};

	// Code is normally determined from the keyboard layout - but if there
	// is a specific need to use a particular code page, set it here
	std::optional<uint16_t> code_page = {};

	// Set this if the host layout does not have a reasonable FreeDOS
	// counterpart and the mapping is very poor/imprecise; this will lower
	// the priority of this particular layout as much as possible
	bool is_mapping_fuzzy = false;

	bool operator==(const KeyboardLayoutMaybeCodepage& other) const
	{
		if (keyboard_layout != other.keyboard_layout) {
			return false;
		}
		if (is_mapping_fuzzy != other.is_mapping_fuzzy) {
			return false;
		}
		if (!code_page && !other.code_page) {
			return true;
		}
		if (code_page && other.code_page && *code_page == *other.code_page) {
			return true;
		}
		return false;
	}
};

struct StdLibLocale {
	StdLibLocale();

	std::optional<char> thousands_separator = {};
	std::optional<char> decimal_separator   = {};

	// Like 'USD', ASCII
	std::string currency_code = {};
	// Like '$', UTF-8
	std::string currency_utf8 = {};
	// Digits to display
	uint8_t currency_precision = {};

	std::optional<DosCurrencyFormat> currency_code_format = {};
	std::optional<DosCurrencyFormat> currency_utf8_format = {};

	std::optional<DosDateFormat> date_format = {};
	std::optional<DosTimeFormat> time_format = {};

	char date_separator = {};
	char time_separator = {};

private:
	void GetNumericFormat(const std::locale& locale);
	void GetDateFormat(const std::locale& locale);

	void DetectCurrencyFormat(const std::locale& locale);
	void DetectTimeDateFormat(const std::locale& locale);
};

struct HostLocaleElement {
	std::optional<DosCountry> country_code = {};

	// If detection was successful, always provide info for the log output,
	// telling which host OS property/value was used to determine the given
	// locale.
	std::string log_info = {};
};

struct HostLocale {
	// These are locale detected by the portable routines of C++ library.
	// Override them if the host-specific code can do a better job detecting
	// them. Feel free to clear them if they are known to be incorrect for
	// the given platform and no better way to detect the values is known.
	StdLibLocale native = {};

	// If the host OS support code cannot determine any of these values, it
	// should leave them as default

	// DOS country code
	HostLocaleElement country = {};

	// These are completely optional; leave them unset if you can't get the
	// concrete value from the host OS, do not blindly copy 'country' here!
	HostLocaleElement numeric   = {};
	HostLocaleElement time_date = {};
	HostLocaleElement currency  = {};
};

struct HostKeyboardLayouts {
	// Keyboard layouts, optionally with code pages
	std::vector<KeyboardLayoutMaybeCodepage> keyboard_layout_list = {};

	// If the keyboard layouts list retrieved from the host OS is already
	// sorted by user priority, set this to 'true'.
	bool is_layout_list_sorted = false;

	// If detection was successful, always provide info for the log output,
	// telling which host OS property/value was used to determine the
	// language.
	std::string log_info = {};
};

struct HostLanguages {
	// Put here the name of the language file corresponding to the host UI
	// language. Leave empty if it can't be determined.
	std::string language_file_gui = {};

	// If the OS allows to get the list of UI languages preferred by the
	// user, put it here.
	std::vector<std::string> language_files = {};

	// If detection was successful, always provide info for the log output,
	// telling which host OS properties/values were used to determine the
	// language.
	std::string log_info = {};
};

bool IsMonetaryUtf8(const std::locale& locale);

const HostLocale&          GetHostLocale();
const HostKeyboardLayouts& GetHostKeyboardLayouts();
const HostLanguages&       GetHostLanguages();

#endif
