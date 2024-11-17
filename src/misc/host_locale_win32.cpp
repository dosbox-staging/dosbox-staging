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

#if defined(WIN32)

#include "host_locale.h"

#include "checks.h"
#include "string_utils.h"

#include <stringapiset.h>
#include <windows.h>
#include <winnls.h>

CHECK_NARROWING();

static std::string to_string(const wchar_t* input, const size_t input_length)
{
	if (input_length == 0) {
		return {};
	}

	const auto buffer_size = WideCharToMultiByte(DefaultCodePage, 0,
                                                     input, input_length,
                                                     nullptr, 0, nullptr, nullptr);
	if (buffer_size == 0) {
		return {};
	}

	std::vector<char> buffer(buffer_size);
	WideCharToMultiByte(DefaultCodePage,  0, input, input_length,
	                    buffer.data(), buffer_size, nullptr, nullptr);
	return std::string(buffer.begin(), buffer.end());
}

static std::string get_language_file(std::string& log_info)
{
	wchar_t buffer[LOCALE_NAME_MAX_LENGTH];

	// Get the main language name
	const auto ui_language = GetUserDefaultUILanguage();
	if (LCIDToLocaleName(ui_language, buffer, LOCALE_NAME_MAX_LENGTH, 0) == 0) {
		LOG_WARNING("LOCALE: Could not get locale name for language 0x%04x, error code %lu",
		            ui_language,
		            GetLastError());
		return {};
	}

	const auto language_territory = to_string(&buffer[0], LOCALE_NAME_MAX_LENGTH);

	log_info = language_territory;

	if (language_territory == "pt-BR") {
		// We have a dedicated Brazilian translation
		return "br";
	}

	return language_territory.substr(0, language_territory.find('-'));
}

const HostLanguage& GetHostLanguage()
{
	static std::optional<HostLanguage> locale = {};

	if (!locale) {
		locale = HostLanguage();

		locale->language_file = get_language_file(locale->log_info);
	}

	return *locale;
}

#endif
