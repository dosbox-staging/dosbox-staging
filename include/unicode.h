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

#ifndef DOSBOX_UNICODE_H
#define DOSBOX_UNICODE_H

#include <string>

// Get recommended DOS code page to render the UTF-8 strings to. This
// might not be the code page set using KEYB command, for example due
// to emulated hardware limitations, or duplicated code page numbers
uint16_t get_utf8_code_page();

// Specifies what to do if the DOS code page does not contain character
// representing given Unicode grapheme
enum class UnicodeFallback {

	// If any grapheme can't be converted without using a fallback
	// mechanism, return an empty string
	EmptyString,

	// Try to provide reasonable fallback using all the characters available
	// in target DOS code page; use for features like clipboard content
	// exchange with host system
	Simple,

	// Do not use certain DOS code page characters in order to draw boxes
	// (tables) which are consistent; for example, if code page contains
	// character '╠', but not '╣', both will be replaced with a fallback
	// character ('║' for example)
	Box
};

// Specifies how to interpret characters 0x00-0x1f and 0x7f in DOS strings
enum class DosStringConvertMode {

	// String contains control codes (new line, tabulation, delete, etc.)
	WithControlCodes,

	// String does not have any codes characters, consider all characters
	// as screen codes
	ScreenCodesOnly,

	// String should not contain characters mentioned above
	NoSpecialCharacters
};

// Convert the UTF-8 string to the format intended for display inside emulated
// environment, or vice-versa. Code page '0' means a pure 7-bit ASCII. Functions
// without 'code_page' parameters use current DOS code page.

std::string utf8_to_dos(const std::string& str,
                        const DosStringConvertMode convert_mode,
                        const UnicodeFallback fallback);

std::string utf8_to_dos(const std::string& str,
                        const DosStringConvertMode convert_mode,
                        const UnicodeFallback fallback,
                        const uint16_t code_page);

std::string dos_to_utf8(const std::string& str,
                        const DosStringConvertMode convert_mode);

std::string dos_to_utf8(const std::string& str,
                        const DosStringConvertMode convert_mode,
                        const uint16_t code_page);

// Convert DOS code page string to lower/upper case; converters are aware of all
// the national characters. Functions without 'code_page' parameter use current
// DOS code page.

std::string lowercase_dos(const std::string& str);
std::string lowercase_dos(const std::string& str, const uint16_t code_page);

std::string uppercase_dos(const std::string& str);
std::string uppercase_dos(const std::string& str, const uint16_t code_page);

// Check if two code pages are the same
bool is_code_page_equal(const uint16_t code_page1, const uint16_t code_page2);

#endif
