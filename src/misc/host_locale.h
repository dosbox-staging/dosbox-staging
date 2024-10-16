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

#ifndef DOSBOX_HOST_LOCALE_H
#define DOSBOX_HOST_LOCALE_H

#include "../dos/dos_locale.h"

#include <cstdint>
#include <optional>
#include <vector>

// Converts language and territory in the typical POSIX format (with underscore
// or hyphen - "en_US" or "en-US"), using 'ISO 3166-1 alpha-2' codes, to a
// DOS country code
std::optional<DosCountry> IsoToDosCountry(const std::string& language,
                                          const std::string& territory);

struct KeyboardLayoutMaybeCodepage {
	// Keyboard layout, as supported by the FreeDOS
	std::string keyboard_layout = {};
	// Code is normally determined from the keyboard layout - but if there
	// is a specific need to use a particular code page, set it here
	std::optional<uint16_t> code_page = {};

	bool operator==(const KeyboardLayoutMaybeCodepage& other) const
	{
		if (keyboard_layout != other.keyboard_layout) {
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

	std::string currency_code  = {}; // like 'USD', ASCII
	std::string currency_utf8  = {}; // like '$', UTF-8
	uint8_t currency_precision = {}; // digits to display

	std::optional<DosCurrencyFormat> currency_code_format = {};
	std::optional<DosCurrencyFormat> currency_utf8_format = {};

	std::optional<DosDateFormat> date_format = {};
	std::optional<DosTimeFormat> time_format = {};

	char date_separator = {};
	char time_separator = {};
};

struct HostLocale {
	// These are locale detected by the portable routines of C++ library.
	// Override them if the host-specific code can do a better job detecting
	// them. Feel free to clear them if they are known to be incorrect for
	// the given platform and no better way to detect the values is known.
	StdLibLocale native = {};

	// If the host OS support code cannot determine any of these values, it
	// should leave them as default

	std::vector<KeyboardLayoutMaybeCodepage> keyboard_layout_list = {};

	std::optional<DosCountry> country = {}; // DOS country code

	// These are completely optional; leave them unset if you can't get the
	// concrete value from the host OS, do not blindly copy 'country' here!
	std::optional<DosCountry> numeric   = {};
	std::optional<DosCountry> time_date = {};
	std::optional<DosCountry> currency  = {};

	// If detection was successful, always provide info for the log output,
	// telling which host OS property/value was used to determine the given
	// locale. Feel free to provide it even if detection failed.
	struct {
		std::string keyboard  = {};
		std::string country   = {};
		std::string numeric   = {};
		std::string time_date = {};
		std::string currency  = {};
	} log_info = {}; // XXX make sure it is always provided
};

struct HostLanguage {
	// If the host OS support code cannot determine the UI language, leave
	// it as default
	std::string language_file = {}; // translation (messages)
	// If detection was successful, always provide info for the log output,
	// telling which host OS property/value was used to determine the
	// language. Feel free to provide it even if detection failed.
	std::string log_info = {}; // XXX make sure it is always provided
};

const HostLocale& GetHostLocale();
const HostLanguage& GetHostLanguage();

#endif
