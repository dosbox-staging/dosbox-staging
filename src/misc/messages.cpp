/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2025  The DOSBox Staging Team
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

#include "dosbox.h"

#include "../dos/dos_locale.h"
#include "ansi_code_markup.h"
#include "checks.h"
#include "control.h"
#include "cross.h"
#include "fs_utils.h"
#include "host_locale.h"
#include "setup.h"
#include "std_filesystem.h"
#include "string_utils.h"
#include "support.h"
#include "unicode.h"

#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <string>

CHECK_NARROWING();

static const std::string MsgNotFound = " MESSAGE NOT FOUND! ";
static const std::string MsgNotValid = " MESSAGE NOT VALID! ";

// ***************************************************************************
// Single message storage class
// ***************************************************************************

class Message {
public:
	// Note: any message needs to be verified before it can be safely used!

	// Constructor for original, English strings
	Message(const std::string& message, const bool can_contain_format_string);
	// Constructor for translated strings from external file
	Message(const std::string& message);

	const std::string& Get();
	const std::string& GetRaw();

	bool IsValid() const
	{
		return is_verified && is_ok;
	}
	void MarkInvalid()
	{
		is_ok = false;
	}

	// Use this one for English messages only
	void VerifyEnglish(const std::string& name);
	// Use this one for translated messages
	void VerifyTranslated(const std::string& name, const Message& english);

private:
	Message() = delete;

	std::string GetLogStart(const std::string& name) const;

	void VerifyMessage(const std::string& name);
	void VerifyFormatString(const std::string& name);
	void VerifyFormatStringAgainst(const std::string& name,
	                               const Message& english);

	const bool is_english;
	const bool can_contain_format_string;

	bool is_verified = false;
	bool is_ok       = true;

	// Original message, UTF-8, can contain DOSBox ANSI markups
	std::string message_raw = {};
	// Message in DOS encoding, markups converted to ANSI control codes
	std::string message_dos_ansi = {};

	uint16_t code_page = 0;

	struct FormatSpecifier {
		std::string flags     = {};
		std::string width     = {};
		std::string precision = {};
		std::string length    = {};

		char format = '\0';

		std::string AsString() const;
	};

	std::vector<FormatSpecifier> format_specifiers = {};
};

Message::Message(const std::string& message, const bool can_contain_format_string)
        : is_english(true),
          can_contain_format_string(can_contain_format_string),
          message_raw(message)
{}

Message::Message(const std::string& message)
	: is_english(false),
	  can_contain_format_string(false),
	  message_raw(message)
{}

std::string Message::GetLogStart(const std::string& name) const
{
	std::string result = is_english ? "LOCALE: English message '"
	                                : "LOCALE: Translated message '";
	return result + name + "'";
}

const std::string& Message::Get()
{
	if (message_raw.empty()) {
		return message_raw;
	}

	const auto current_code_page = get_utf8_code_page();
	if (message_dos_ansi.empty() || code_page != current_code_page) {
		code_page = current_code_page;
		message_dos_ansi = utf8_to_dos(convert_ansi_markup(message_raw),
                                               DosStringConvertMode::WithControlCodes,
                                               UnicodeFallback::Box,
                                               code_page);
	}

	return message_dos_ansi;
}

const std::string& Message::GetRaw()
{
	return message_raw;
}

void Message::VerifyMessage(const std::string& name)
{
	if (!is_ok || is_verified) {
		return;
	}

	for (const auto item : message_raw) {
		if (item == '\n' || is_extended_printable_ascii(item)) {
			continue;
		}

		// No special characters allowed, except a newline character;
		// please use DOSBox tags instead of ANSI escape sequences
		LOG_WARNING("%s contains invalid character 0x%02x",
		            GetLogStart(name).c_str(),
		            item);
		is_ok = false;
		break;
	}
}

void Message::VerifyFormatString(const std::string& name)
{
	if (!is_ok || is_verified) {
		return;
	}

	// clang-format off
	const std::set<char> Flags   = {
		'-', '+', ' ', '#', '0'
	};
	const std::set<char> Lengths = {
		'h', 'l', 'j', 'z', 't', 'L'
	};
	const std::set<char> Formats = {
		'd', 'i', 'u', 'o', 'x', 'X', 'f', 'F', 'e', 'E', 'g', 'G',
		'a', 'A', 'c', 'C', 's', 'p', 'n'
	};
	// clang-format on

	auto log_problem = [&](const std::string& error) {
		if (!is_ok) {
			// At least one error already reported
			return;
		}

		// NOTE: If you want to skip format string checks for the given
		//       message, put a 'MsgFlagNoFormatString' flag into
		//       the relevant 'MSG_Add' call
		LOG_WARNING("%s contains an incorrect format specifier: %s",
		            GetLogStart(name).c_str(),
		            error.c_str());
		is_ok = false;
	};

	// Look for format specifier
	for (auto it = message_raw.begin(); it < message_raw.end(); ++it) {
		if (*it != '%') {
			// Not a format specifier
			continue;
		} else if (*(++it) == '%') {
			// Percent sign, not a format specifier
			continue;
		}

		// Found a new specifier - parse it according to:
		// - https://cplusplus.com/reference/cstdio/printf/
		format_specifiers.push_back({});
		auto& specifier = format_specifiers.back();

		// First check for POSIX format string extensions
		auto it_tmp = it;
		if (*it_tmp == '*') {
			++it_tmp;
		}
		if (*it_tmp != '0' && std::isdigit(*it_tmp)) {
			// Skip all the digits
			while (std::isdigit(*it_tmp)) {
				++it_tmp;
			}
			if (*it_tmp == '$') {
				log_problem(
				        "POSIX extension used, "
				        "this won't work on Windows");
				// We do not support parsing this
				format_specifiers.clear();
				return;
			}
		}

		// Extract the 'flags'
		std::set<char> flags = {};
		bool already_warned  = false;
		while (Flags.contains(*it)) {
			if (flags.contains(*it) && !already_warned) {
				log_problem("duplicated flag");
				already_warned = true;
			} else {
				flags.insert(*it);
			}
			specifier.flags.push_back(*(it++));
		}

		// Extract the 'width'
		if (*it == '*') {
			specifier.width = "*";
		} else {
			while (std::isdigit(*it)) {
				specifier.width.push_back(*(it++));
			}
		}

		// Extract the 'precision'
		if (*it == '.') {
			if (*(++it) == '*') {
				specifier.precision = "*";
			}
			while (std::isdigit(*it)) {
				specifier.precision.push_back(*(it++));
			}
			if (specifier.precision.empty()) {
				log_problem("precision not specified");
			}
		}

		// Extract the 'length'
		if ((*it == 'h' && *(it + 1) == 'h') ||
		    (*it == 'l' && *(it + 1) == 'l')) {
			specifier.length.push_back(*(it++));
			specifier.length.push_back(*(it++));
		} else if (Lengths.contains(*it)) {
			specifier.length.push_back(*(it++));
		}

		// Extract the 'format'
		if (Formats.contains(*it)) {
			specifier.format = *it;
		} else {
			log_problem("data format not specified");
		}
	}
}

void Message::VerifyFormatStringAgainst(const std::string& name, const Message& english)
{
	if (!is_ok || is_verified) {
		return;
	}

	// Check if the number of format specifiers match
	if (format_specifiers.size() != english.format_specifiers.size()) {
		LOG_WARNING(
		        "%s has %d format specifier(s) "
		        "while English has %d specifier(s)",
		        GetLogStart(name).c_str(),
		        static_cast<int>(format_specifiers.size()),
		        static_cast<int>(english.format_specifiers.size()));
		if (format_specifiers.size() < english.format_specifiers.size()) {
			is_ok = false;
			return;
		}
	}

	// Check if format specifiers are compatible to each other
	auto are_compatible = [&](const char format_1, const char format_2) {
		if (format_1 == format_2) {
			return true;
		}

		const std::set<std::pair<char, char>> CompatiblePairs = {
		        // Fully interchangeable formats
		        {'d', 'i'}, // signed decimal integer
		        // Different case pairs
		        {'x', 'X'}, // octal
		        {'f', 'F'}, // decimal floating point
		        {'e', 'E'}, // scientific notation
		        {'g', 'G'}, // floating or scientific - shorter one
		        {'a', 'A'}, // decimal floating point
		        {'c', 'C'}, // character
		};

		return CompatiblePairs.contains({format_1, format_2}) ||
		       CompatiblePairs.contains({format_2, format_1});
	};

	const auto index_limit = std::min(format_specifiers.size(),
	                                  english.format_specifiers.size());

	for (size_t i = 0; i < index_limit; ++i) {
		const auto& specifier         = format_specifiers[i];
		const auto& specifier_english = english.format_specifiers[i];

		if (!are_compatible(specifier.format, specifier_english.format) ||
		    (specifier.width == "*" && specifier_english.width != "*") ||
		    (specifier.width != "*" && specifier_english.width == "*") ||
		    (specifier.precision == "*" && specifier_english.precision != "*") ||
		    (specifier.precision != "*" && specifier_english.precision == "*")) {
			LOG_WARNING(
			        "%s has format specifier '%s' "
			        "incompatible with English counterpart '%s'",
			        GetLogStart(name).c_str(),
			        specifier.AsString().c_str(),
			        specifier_english.AsString().c_str());
			is_ok = false;
			break;
		}
	}
}

void Message::VerifyEnglish(const std::string& name)
{
	assert(is_english);
	if (is_verified) {
		return;
	}

	if (can_contain_format_string) {
		VerifyFormatString(name);
	}

	VerifyMessage(name);
	is_verified = true;
}

void Message::VerifyTranslated(const std::string& name, const Message& english)
{
	assert(!is_english);
	if (is_verified) {
		return;
	}

	const auto is_english_valid = english.IsValid();

	if (english.can_contain_format_string) {
		VerifyFormatString(name);
		if (is_english_valid) {
			VerifyFormatStringAgainst(name, english);
		}
	}

	VerifyMessage(name);
	is_verified = is_english_valid;
}

std::string Message::FormatSpecifier::AsString() const
{
	std::string result = "%";

	result += flags + width;
	if (!precision.empty()) {
		result += ".";
		result += precision;
	}
	result += length;
	if (format != '\0') {
		result += format;
	}

	return result;
}

// ***************************************************************************
// Internal implementation
// ***************************************************************************

static std::vector<std::string> message_order = {};

static std::map<std::string, Message> dictionary_english    = {};
static std::map<std::string, Message> dictionary_translated = {};
static std::optional<Script> translation_script             = {};

// Whether the translation is compatible with the current code page
static bool is_code_page_compatible = true;

static std::set<std::string> already_warned_not_found = {};

// Metadata keys - for now only one is available, writing script type
static const std::string KeyScript = "#SCRIPT ";

// Check if currently set code page is compatible with the translation
static void check_code_page()
{
	// Every DOS code page is suitable for displaying Latin script
	if (!translation_script || *translation_script == Script::Latin) {
		is_code_page_compatible = true;
		return;
	}

	// For known code pages check their compatibility
	bool is_code_page_known = false;
	for (const auto& pack : LocaleData::CodePageInfo) {
		if (!pack.contains(dos.loaded_codepage)) {
			continue;
		}

		is_code_page_known = true;

		const auto code_page_script = pack.at(dos.loaded_codepage).script;
		if (*translation_script == code_page_script) {
			is_code_page_compatible = true;
			return;
		}

		break;
	}

	// Code page is unknown or not compatible
	is_code_page_compatible = false;

	// Log a warning before exit
	const auto& script_name = LocaleData::ScriptInfo.at(*translation_script).script_name;
	LOG_WARNING(
	        "LOCALE: Code page %d %s the '%s' script; "
	        "using internal English language messages as a fallback",
	        dos.loaded_codepage,
	        is_code_page_known ? "does not support"
	                           : "is unknown and can't be used with",
	        script_name.c_str());
	return;
}

static void add_message(const std::string& name, const std::string& message,
                        const bool can_contain_format_string)
{
	if (dictionary_english.contains(name)) {
		if (dictionary_english.at(name).GetRaw() != message) {
			dictionary_english.at(name).MarkInvalid();
			LOG_ERR("LOCALE: Duplicate text for '%s'", name.c_str());
		}
		return;
	}

	message_order.push_back(name);
	dictionary_english.emplace(name, Message(message, can_contain_format_string));

	auto& english = dictionary_english.at(name);
	english.VerifyEnglish(name);
	if (dictionary_translated.contains(name)) {
		dictionary_translated.at(name).VerifyTranslated(name, english);
	}
}

static const char* get_message(const std::string& name,
                               const bool raw_requested        = false,
                               const bool skip_code_page_check = false)
{
	// Check if message exists in English dictionary
	if (!dictionary_english.contains(name)) {
		if (!already_warned_not_found.contains(name)) {
			LOG_WARNING("LOCALE: Message '%s' not found", name.c_str());
			already_warned_not_found.insert(name);
		}
		return MsgNotFound.c_str();
	}

	// Try to return translated string, if possible
	if ((is_code_page_compatible || skip_code_page_check) &&
	    dictionary_translated.contains(name) &&
	    dictionary_translated.at(name).IsValid()) {
		if (raw_requested) {
			return dictionary_translated.at(name).GetRaw().c_str();
		} else {
			return dictionary_translated.at(name).Get().c_str();
		}
	}

	// Try to return the original, English string
	if (!dictionary_english.at(name).IsValid()) {
		return MsgNotValid.c_str();
	}

	if (raw_requested) {
		return dictionary_english.at(name).GetRaw().c_str();
	} else {
		return dictionary_english.at(name).Get().c_str();
	}
}

static void clear_translated_messages()
{
	dictionary_translated.clear();
	translation_script = {};
}

static bool load_messages_from_path(const std_fs::path& file_path)
{
	if (file_path.empty()) {
		return false;
	}

	if (!path_exists(file_path) || !is_readable(file_path)) {
		return false;
	}

	clear_translated_messages();

	std::ifstream in_file(file_path);

	std::string line = {};
	int line_number  = -1;

	auto problem_generic = [&](const std::string& error) {
		LOG_ERR("LOCALE: Translation file '%s' error in line %d: %s",
		        file_path.string().c_str(),
		        static_cast<int>(line_number),
		        error.c_str());
		clear_translated_messages();
	};

	auto problem_with_message = [&](const std::string& name,
	                                const std::string& error) {
		LOG_ERR("LOCALE: Translation file '%s' error in line %d, "
		        "message '%s': %s",
		        file_path.string().c_str(),
		        static_cast<int>(line_number),
		        name.c_str(),
		        error.c_str());
		clear_translated_messages();
	};

	bool reading_metadata = true;
	while (std::getline(in_file, line)) {
		++line_number;
		if (line.empty() || line.starts_with("//")) {
			continue;
		}

		auto trimmed_line = line;
		trim(trimmed_line);
		if (trimmed_line.starts_with(KeyScript)) {
			if (!reading_metadata) {
				problem_generic("metadata not at the start of file");
				return false;
			}
			if (translation_script) {
				problem_generic("script already specified");
				return false;
			}
			std::string value = trimmed_line.substr(KeyScript.length());
			trim(value);
			for (const auto& entry : LocaleData::ScriptInfo) {
				if (iequals(value, entry.second.script_name)) {
					translation_script = entry.first;
					break;
				}
			}
			if (!translation_script) {
				problem_generic("unknown script");
				return false;
			}
			continue;
		}

		reading_metadata = false;
		if (!line.starts_with(':')) {
			problem_generic("wrong syntax");
			return false;
		}

		const std::string name = line.substr(1);

		if (name.empty()) {
			problem_generic("message name is empty");
			return false;
		}

		std::string text = {};

		bool is_text_terminated = false;
		bool is_first_text_line = true;
		while (std::getline(in_file, line)) {
			++line_number;
			if (line == ".") {
				is_text_terminated = true;
				break;
			}

			if (is_first_text_line) {
				is_first_text_line = false;
			} else {
				text += "\n";
			}

			text += line;
		}

		if (!is_text_terminated) {
			problem_with_message(name, "message text not terminated");
			return false;
		}

		if (text.empty()) {
			problem_with_message(name, "message text is empty");
			return false;
		}

		if (dictionary_translated.contains(name)) {
			problem_with_message(name, "duplicated message name");
			return false;
		}

		dictionary_translated.emplace(name, Message(text));

		auto& translated = dictionary_translated.at(name);
		if (dictionary_english.contains(name)) {
			translated.VerifyTranslated(name, dictionary_english.at(name));
		}
	}

	++line_number;
	if (in_file.bad()) {
		problem_generic("I/O error");
		return false;
	} else if (dictionary_translated.empty()) {
		problem_generic("file has no content");
		return false;
	}

	if (!translation_script) {
		LOG_WARNING("LOCALE: Translation file did not specify the language script");
	}

	if (dos.loaded_codepage) {
		check_code_page();
	}

	return true;
}

static bool save_messages_to_path(const std_fs::path& file_path)
{
	if (file_path.empty()) {
		return false;
	}

	std::ofstream out_file(file_path);

	// Output help line
	out_file << "// Writing script used in this translation, can be one of:\n";
	out_file << "// ";
	bool is_first_script = true;
	for (const auto& entry : LocaleData::ScriptInfo) {
		if (is_first_script) {
			is_first_script = false;
		} else {
			out_file << ", ";
		}
		out_file << entry.second.script_name;
	}
	out_file << "\n";

	// Output script definition
	std::string script_name = {};
	if (translation_script) {
		// Saving translated messages, script is specified
		assert(LocaleData::ScriptInfo.contains(*translation_script));
		script_name = LocaleData::ScriptInfo.at(*translation_script).script_name;
	} else {
		// Saving English messages, script is always Latin
		assert(LocaleData::ScriptInfo.contains(Script::Latin));
		script_name = LocaleData::ScriptInfo.at(Script::Latin).script_name;
	}
	if (translation_script || dictionary_translated.empty()) {
		out_file << KeyScript << script_name;
	} else {
		// Script was not specified in the input file
		out_file << "// " << KeyScript << script_name;
	}
	out_file << "\n\n";

	// Save all the messages, in the original order
	for (const auto& name : message_order) {
		if (out_file.fail()) {
			break;
		}

		std::string text = {};
		if (dictionary_translated.contains(name)) {
			text = dictionary_translated.at(name).GetRaw();
		} else {
			assert(dictionary_english.contains(name));
			text = dictionary_english.at(name).GetRaw();
		}

		out_file << ":" << name << "\n" << text << "\n.\n";
	}

	if (out_file.fail()) {
		LOG_ERR("LOCALE: I/O error writing translation file");
		return false;
	}

	return true;
}

// ***************************************************************************
// External interface
// ***************************************************************************

void MSG_Add(const std::string& name, const std::string& message)
{
	constexpr bool CanContainFormatString = true;
	add_message(name, message, CanContainFormatString);
}

void MSG_AddNoFormatString(const std::string& name, const std::string& message)
{
	constexpr bool CanContainFormatString = false;
	add_message(name, message, CanContainFormatString);
}

const char* MSG_Get(const std::string& name)
{
	return get_message(name);
}

const char* MSG_GetRaw(const std::string& name)
{
	constexpr bool RawRequested = true;
	return get_message(name, RawRequested);
}

const char* MSG_GetForHost(const std::string& name)
{
	constexpr bool RawRequested      = true;
	constexpr bool SkipCodePageCheck = true;
	return get_message(name, RawRequested, SkipCodePageCheck);
}

bool MSG_Exists(const std::string& name)
{
	return dictionary_english.contains(name);
}

bool MSG_WriteToFile(const std::string& file_name)
{
	return save_messages_to_path(file_name);
}

void MSG_NotifyNewCodePage()
{
	check_code_page();
}

// MSG_LoadMessages loads the requested language provided on the command line or
// from the language = conf setting.

// 1. The provided language can be an exact filename and path to the lng
//    file, which is the traditionnal method to load a language file.

// 2. It also supports the more convenient syntax without needing to provide a
//    filename or path: `-lang ru`. In this case, it constructs a path into the
//    platform's config path/translations/<lang>.lng.

static const std::string InternalLangauge = "en";
static const std::string Extension        = ".lng";
static const std_fs::path Subdirectory    = "translations";

static std::string get_file_name_with_extension(const std::string& file_name)
{
	if (file_name.ends_with(Extension)) {
		return file_name;
	} else {
		return file_name + Extension;
	}
}

static bool load_messages_by_name(const std::string& language_file)
{
	if (language_file == InternalLangauge ||
	    language_file == InternalLangauge + Extension) {
		LOG_MSG("LOCALE: Using internal English language messages");
		return true;
	}

	const auto file_with_extension = get_file_name_with_extension(language_file);
	const auto file_path = get_resource_path(Subdirectory, file_with_extension);

	const auto result = load_messages_from_path(file_path);
	if (!result) {
		LOG_MSG("LOCALE: Could not load language file '%s', "
		        "using internal English language messages",
		        file_with_extension.c_str());
	} else {
		LOG_MSG("LOCALE: Loaded language file '%s'",
		        file_path.string().c_str());
	}

	return result;
}

static std::optional<std::string> get_new_language_file()
{
	static std::optional<std::string> old_language_file = {};

	// Get the language file from command line
	assert(control);
	auto language_file = control->GetArgumentLanguage();

	// If not available, get it from the config file
	if (language_file.empty()) {
		const auto section = control->GetSection("dosbox");
		assert(section);
		language_file = static_cast<const Section_prop*>(section)->Get_string(
		        "language");
	}

	// Check if requested language has changed
	if (old_language_file && *old_language_file == language_file) {
		// Config not changed, nothing to do
		return {};
	}

	old_language_file = language_file;
	return language_file;
}

void MSG_LoadMessages()
{
	// Ensure autodetection happens the same time, regardless of the
	// configuration
	const auto& host_languages = GetHostLanguages();

	// Check if the language configuration has changed
	const auto new_language_file = get_new_language_file();
	if (!new_language_file) {
		// Config not changed, nothing to do
		return;
	}

	const auto& language_file = *new_language_file;
	clear_translated_messages();

	// If concrete language file is provided, load it
	if (!language_file.empty() && language_file != "auto") {
		load_messages_by_name(language_file);
		return;
	}

	// Get the list of autodetected languages
	auto language_files = host_languages.language_files;
	if (!host_languages.language_file_gui.empty()) {
		language_files.push_back(host_languages.language_file_gui);
	}

	// If autodetection failed, use internal English messages
	if (language_files.empty()) {
		if (host_languages.log_info.empty()) {
			LOG_MSG("LOCALE: Could not detect host langauge, "
			        "using internal English language messages");
		} else {
			LOG_MSG("LOCALE: Could not detected language file from "
			        "host value '%s', using internal English "
			        "language messages",
			        host_languages.log_info.c_str());
		}
		return;
	}

	// Use the first detected language for which we have a translation
	for (const auto& detected_file : language_files) {
		// If detected_file language is English, use internal messages
		if (detected_file == InternalLangauge) {
			LOG_MSG("LOCALE: Using internal English language "
			        "messages (detected from '%s')",
			        host_languages.log_info.c_str());
			return;
		}

		const auto file_with_extension = get_file_name_with_extension(
		        detected_file);
		const auto file_path = get_resource_path(Subdirectory,
		                                         file_with_extension);

		if (load_messages_from_path(file_path)) {
			LOG_MSG("LOCALE: Loaded language file '%s' "
			        "(detected from '%s')",
			        file_with_extension.c_str(),
			        host_languages.log_info.c_str());
			return;
		}
	}

	LOG_MSG("LOCALE: Could not find a valid language file corresponding to "
	        "'%s', using internal English language messages",
	        host_languages.log_info.c_str());
}
