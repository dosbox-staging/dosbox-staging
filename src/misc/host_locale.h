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

struct KeyboardLayoutMaybeCodepage {
	// Keyboard layout, as supported by FreeDOS
	std::string keyboard_layout = {};
	// Code is normally determined from the keyboard layout - but if there
	// is a specific need to use a particular code page, set it here
	std::optional<uint16_t> code_page = {};
};

struct HostLocale {
	// If the host OS support code cannot determine any of these values, it
	// should leave them as default - but we should really try to determine
	// them
	
	std::vector<KeyboardLayoutMaybeCodepage> keyboard_layout_list = {};

	std::optional<DosCountry> country = {}; // DOS country code

	// These are completely optional; leave them unset if you can't get the
	// concrete value from the host OS, do not blindly copy 'country' here!
	std::optional<DosCountry> numeric   = {};
	std::optional<DosCountry> time_date = {};
	std::optional<DosCountry> currency  = {};

	// Always provide some info for the log output, telling which host OS
	// property/value was used to determine the given locale
	struct {
		std::string keyboard  = {};
		std::string country   = {};
		std::string numeric   = {};
		std::string time_date = {};
		std::string currency  = {};
	} log_info = {};
};

struct HostLanguage {
	// If the host OS support code cannot determine the UI language, leave
	// it as default
	std::string language_file = {}; // translation (messages)
	// Always provide some info for the log output, telling which host OS
	// property/value was used to determine the language
	std::string log_info = {};
};

HostLocale DetectHostLocale();
HostLanguage DetectHostLanguage();


#endif
