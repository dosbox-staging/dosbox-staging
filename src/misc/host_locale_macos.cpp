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

#include "dosbox.h"

#if C_COREFOUNDATION

#include "host_locale.h"

#include "checks.h"

#include <CoreFoundation/CoreFoundation.h>

CHECK_NARROWING();

static std::string get_locale(const CFLocaleKey key)
{
	std::string result = {};

	constexpr auto Encoding = kCFStringEncodingASCII;

	CFLocaleRef locale = CFLocaleCopyCurrent();
	const auto value = static_cast<CFStringRef>(CFLocaleGetValue(locale, key));

	const char* buffer = CFStringGetCStringPtr(value, Encoding);
	if (buffer != nullptr) {
		result = buffer;
	} else {
		const auto max_length = CFStringGetMaximumSizeForEncoding(
		        CFStringGetLength(value), Encoding);
		result.resize(max_length + 1); // add space for terminating '0'
		if (!CFStringGetCString(value, result.data(), max_length + 1, Encoding)) {
			return {}; // conversion failed
		}

		result = result.substr(0, result.find('\0'));
	}
	CFRelease(locale);

	return result;
}

static std::string get_language_file(std::string& log_info)
{
	const auto language = get_locale(kCFLocaleLanguageCode);
	const auto country  = get_locale(kCFLocaleCountryCode);

	log_info = language + "_" + country;

	if (language == "pt" && country == "BR") {
		// We have a dedicated Brazilian translation
		return "br";
	}

	return language;
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
