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

#if !defined(WIN32)
#if !C_COREFOUNDATION

#include "host_locale.h"

#include "checks.h"
#include "dosbox.h"
#include "string_utils.h"

#include <cstdio>
#include <set>
#include <string>

CHECK_NARROWING();

// ***************************************************************************
// Detection data
// ***************************************************************************

// Names of various environment variables used
static const std::string LcAll             = "LC_ALL";
static const std::string LcMessages        = "LC_MESSAGES";
static const std::string VariableLang      = "LANG";
static const std::string VariableLanguage  = "LANGUAGE";

// ***************************************************************************
// Generic helper routines
// ***************************************************************************

static std::string get_env_variable(const std::string& variable)
{
	const auto result = getenv(variable.c_str());
	return result ? result : "";
}

static std::pair<std::string, std::string> get_env_variable_from_list(
        const std::vector<std::string>& list)
{
	for (const auto& variable : list) {
		const auto value = get_env_variable(variable);
		if (!value.empty()) {
			return {variable, value};
		}
	}

	return {};
}

// ***************************************************************************
// Detection routines
// ***************************************************************************

// Split locale string into language and territory, drop the rest
static std::pair<std::string, std::string> split_posix_locale(const std::string& value)
{
	std::string tmp = value; // language[_TERRITORY][.codeset][@modifier]

	tmp = tmp.substr(0, tmp.rfind('@')); // strip the modifier
	tmp = tmp.substr(0, tmp.rfind('.')); // strip the codeset

	std::pair<std::string, std::string> result = {};

	result.first = tmp.substr(0, tmp.find('_')); // get the language
	lowcase(result.first);

	const auto position = tmp.rfind('_');
	if (position != std::string::npos) {
		result.second = tmp.substr(position + 1); // get the territory
		upcase(result.second);
	}

	return result;
}

static std::string get_language_file(std::string& log_info)
{
	const std::vector<std::string> Variables = {
	        VariableLanguage,
	        LcAll,
	        LcMessages,
	        VariableLang,
	};

	const auto [variable, value] = get_env_variable_from_list(Variables);
	if (value.empty()) {
		return {};
	}
	log_info = variable + "=" + value;

	const auto [language, teritory] = split_posix_locale(value);

	if (language == "pt" && teritory == "BR") {
		// We have a dedicated Brazilian translation
		return "br";
	}
	if (language == "c" || language == "posix") {
		return "en";
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
#endif
