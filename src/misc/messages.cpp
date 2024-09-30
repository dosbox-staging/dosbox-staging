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

#include "dosbox.h"

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
        Message(const std::string &text);
        Message(const MsgFlags flags, const std::string &text);

        const std::string &Get(const bool raw_requested);

        bool IsValid() const { return is_verified && is_ok; }
        void MarkInvalid() { is_ok = false; }

        // Use this one for English messages only
        void Verify(const std::string &key);
        // Use this one for translated messages
        void Verify(const std::string &key, const Message &english);

private:
	Message() = delete;

        void VerifyMessage(const std::string &key, const bool is_translated);
        void VerifyFormatString(const std::string &key, const bool is_translated);
        void VerifyFormatStringAgainst(const std::string &key, const Message &english);

        bool is_verified = false;
        bool is_ok       = false;

        MsgFlags flags = {};

        std::string text_raw = {}; // original text, UTF-8, DOSBox markup
        std::string text_dos = {}; // DOS encoding, ANSI control codes

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

Message::Message(const std::string &text) :
        is_verified(false),
        is_ok(true),
        text_raw(text),
        text_dos(),
        code_page(),
        format_specifiers()
{
}

Message::Message(const MsgFlags _flags, const std::string &text) :
        Message(text)
{
        flags = _flags;
}

const std::string &Message::Get(const bool raw_requested)
{
        if (raw_requested || text_raw.empty()) {
                return text_raw;
        }

        const auto current_code_page = get_utf8_code_page();
        if (text_dos.empty() || code_page != current_code_page) {
                code_page = current_code_page;
                text_dos  = utf8_to_dos(convert_ansi_markup(text_raw),
                                        DosStringConvertMode::WithControlCodes,
		                        UnicodeFallback::Box,
		                        code_page);
        }

        return text_dos;
}

void Message::VerifyMessage(const std::string &key, const bool is_translated)
{
        if (!is_ok || is_verified) {
                return;
        }

        if (text_raw.ends_with(' ') || text_raw.find(" \n") != std::string::npos) {
                // Don't do this - such strings are a nuisance when translating,
                // please write a code to determine desired padding length
                LOG_WARNING("LANG: %s message '%s' " \
                            "has whitespace at the end of the line",
                            is_translated ? "Translated" : "English",
                            key.c_str());
                // XXX reaction: is_ok = false;
        }

        for (const auto item : text_raw) {
                if (item == '\n' || is_extended_printable_ascii(item)) {
                        continue;
                }

                // No special characters allowed, except a newline character;
                // please use DOSBox tags instead of ANSI escape sequences
                LOG_WARNING("LANG: %s message '%s' " \
                            "contains invalid character 0x%02x",
                            is_translated ? "Translated" : "English",
                            key.c_str(),
                            item);
                // XXX reaction: is_ok = false;
                break;
        }
}

void Message::VerifyFormatString(const std::string &key, const bool is_translated)
{
        if (!is_ok || is_verified) {
                return;
        }

        const std::set<char> Flags   = { '-', '+', ' ', '#', '0' };
        const std::set<char> Lengths = { 'h', 'l', 'j', 'z', 't', 'L' };
        const std::set<char> Formats = { 'd', 'i', 'u', 'o', 'x', 'X', 'f', 'F',
                                         'e', 'E', 'g', 'G', 'a', 'A', 'c', 's',
                                         'p', 'n' };

        auto log_problem = [&](const std::string &error) {
                if (!is_ok) {
                        return; // At least one error already reported
                }

                // NOTE: If you want to skip format string checks for the given
                //       message, put a 'MsgFlagNoFormatString' flag into
                //       the relevant 'MSG_Add' call

                LOG_WARNING("LANG: %s message '%s' " \
                            "has incorrect format specifier: %s",
                            is_translated ? "Translated" : "English",
                            key.c_str(),
                            error.c_str());
                is_ok = false;
        };

        // Look for format specifier
        for (auto it = text_raw.begin(); it < text_raw.end(); ++it) {
                if (*it != '%') {
                        continue; // not a format specifier
                } else if (*(++it) == '%') {
                        continue; // percent sign, not a format specifier
                }

                // Found a new specifier - parse it according to:
                // - https://cplusplus.com/reference/cstdio/printf/
                format_specifiers.push_back({});
                auto &specifier = format_specifiers.back();

                // First check for POSIX extensions
                auto it_tmp = it;
                if (*it_tmp == '*') {
                        ++it_tmp;
                }
                if (*it_tmp != '0' && is_digit(*it_tmp)) {
                        while (is_digit(*it_tmp)) {
                                ++it_tmp; // skip all the digits
                        }
                        if (*it_tmp == '$') {
                                log_problem("POSIX extension used, " \
                                            "won't work on Windows");
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

                // Extract 'width'
                if (*it == '*') {
                        specifier.width = "*";
                } else while (is_digit(*it)) {
                        specifier.width.push_back(*(it++));
                }

                // Extract 'precision'
                if (*it == '.') {
                        if (*(++it) == '*') {
                                specifier.precision = "*";
                        } while (is_digit(*it)) {
                                specifier.precision.push_back(*(it++));
                        }
                        if (specifier.precision.empty()) {
                                log_problem("precision not specified");
                        }
                }

                // Extract 'length'
                if ((*it == 'h' && *(it + 1) == 'h') ||
                    (*it == 'l' && *(it + 1) == 'l')) {
                        specifier.length.push_back(*(it++));
                        specifier.length.push_back(*(it++));
                } else if (Lengths.contains(*it)) {
                        specifier.length.push_back(*(it++));
                }

                // Extract 'format'
                if (Formats.contains(*it)) {
                        specifier.format = *it;
                } else {
                        log_problem("data format not specified");
                }
        }
}

void Message::VerifyFormatStringAgainst(const std::string &key, const Message &english)
{
        if (!is_ok || is_verified) {
                return;
        }

        // Check if the number of format specifiers match
        if (format_specifiers.size() != english.format_specifiers.size()) {
                LOG_WARNING("LANG: Translated message '%s' " \
                            "has %d format specifier(s) " \
                            "while English has %d specifier(s)",
                            key.c_str(),
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
                        // fully interchangeable formats
                        { 'd', 'i' }, // signed decimal integer
                        // different case pairs
                        { 'x', 'X' }, // octal
                        { 'f', 'F' }, // decimal floating point
                        { 'e', 'E' }, // scientific notation
                        { 'g', 'G' }, // floating or scientific - shorter one
                        { 'a', 'A' }, // decimal floating point
                };

                return CompatiblePairs.contains({ format_1, format_2 }) ||
                       CompatiblePairs.contains({ format_2, format_1 });
        };

        const auto index_limit = std::min(format_specifiers.size(),
                                          english.format_specifiers.size());

        for (size_t i = 0; i < index_limit; ++i) {
                const auto &specifier         = format_specifiers[i];
                const auto &specifier_english = english.format_specifiers[i];

                if (!are_compatible(specifier.format, specifier_english.format) ||
                    (specifier.width == "*" && specifier_english.width != "*") ||
                    (specifier.width != "*" && specifier_english.width == "*") ||
                    (specifier.precision == "*" && specifier_english.precision != "*") ||
                    (specifier.precision != "*" && specifier_english.precision == "*")) {
                        LOG_WARNING("LANG: Translated message '%s' " \
                                    "has format specifier '%s' incompatible " \
                                    "with English counterpart '%s'",
                                    key.c_str(),
                                    specifier.AsString().c_str(),
                                    specifier_english.AsString().c_str());
                        is_ok = false;
                        break;
                }
        }
}

void Message::Verify(const std::string &key)
{
        // XXX introduce 'is_english' member variable for API call correctness check

        constexpr bool IsTranslated = false;
        if (!(flags & MsgFlagNoFormatString)) {
                VerifyFormatString(key, IsTranslated);
        }

        VerifyMessage(key, IsTranslated);
        is_verified = true;
}

void Message::Verify(const std::string &key, const Message &english)
{
        flags = english.flags;

        const auto is_english_valid = english.IsValid();

        constexpr bool IsTranslated = true;
        if (!(flags & MsgFlagNoFormatString)) {
                VerifyFormatString(key, IsTranslated);
                if (is_english_valid) {
                        VerifyFormatStringAgainst(key, english);
                }
        }

        VerifyMessage(key, IsTranslated);
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

static std::set<std::string> already_warned_not_found = {};

static const char* get_message(const std::string &key, const bool raw_requested)
{
        // Check if message exists in English dictionary
        if (!dictionary_english.contains(key)) {
                if (!already_warned_not_found.contains(key)) {
                        LOG_WARNING("LANG: Message '%s' not found", key.c_str());
                        already_warned_not_found.insert(key);
                }
                return MsgNotFound.c_str();
        }

        // Try to return translated string, if possible
        if (dictionary_translated.contains(key) &&
            dictionary_translated.at(key).IsValid()) {
                return dictionary_translated.at(key).Get(raw_requested).c_str();
        }

        // Try to return the original, English string
        if (!dictionary_english.at(key).IsValid()) {
                return MsgNotValid.c_str();
        }

        return dictionary_english.at(key).Get(raw_requested).c_str();
}

static void clear_translated_messages()
{
        dictionary_translated.clear();
}

static bool load_messages(const std_fs::path &file_name)
{
	if (file_name.empty()) {
		return false;
        }

	if (!path_exists(file_name) || !is_readable(file_name)) {
		return false;
	}

        std::ifstream in_file(file_name);

        std::string line = {};
        int line_number  = -1;

        auto log_problem = [&](const std::string &error) {
                LOG_ERR("LANG: Translation file error in line %d: %s",
                        static_cast<int>(line_number),
                        error.c_str());
                clear_translated_messages();
        };

        auto log_problem_key = [&](const std::string &key,
                                   const std::string &error) {
                LOG_ERR("LANG: Translation file error in line %d, key '%s': %s",
                        static_cast<int>(line_number),
                        key.c_str(),
                        error.c_str());
                clear_translated_messages();
        };

        while (std::getline(in_file, line))
        {
                ++line_number;

                trim_rear(line);
                if (line.empty()) {
                        continue;
                }

                if (!line.starts_with(':')) {
                        log_problem("wrong syntax");
                        return false;
                }

                const std::string key = line.substr(1);

                if (key.empty()) {
                        log_problem("message key is empty");
                        return false;
                }

                std::string text = {};

                bool is_text_terminated = false;
                bool is_first_text_line = true;
                while (std::getline(in_file, line))
                {
                        ++line_number;

                        trim_rear(line);
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
                        log_problem_key(key, "message text not terminated");
                        return false;
                }

                if (text.empty()) {
                        log_problem_key(key, "message text is empty");
                        return false;
                }

                if (dictionary_translated.contains(key)) {
                        log_problem_key(key, "duplicated key");
                        return false;
                }

                dictionary_translated.emplace(key, Message(text));

                auto &translated = dictionary_translated.at(key);
                if (dictionary_english.contains(key)) {
                        translated.Verify(key, dictionary_english.at(key));
                }
        }

        ++line_number;
        if (in_file.bad()) {
                log_problem("I/O error");
                return false;
        } else if (dictionary_translated.empty()) {
                log_problem("file has no content");
                return false;
        }

        return true;
}

static bool save_messages(const std_fs::path &file_name)
{
	if (file_name.empty()) {
		return false;
        }

        constexpr bool RawRequested = true;

        std::ofstream out_file(file_name);
        for (const auto &key : message_order) {
                std::string text = {};
                if (dictionary_translated.contains(key)) {
                        text = dictionary_translated.at(key).Get(RawRequested);
                } else {
                        assert(dictionary_english.contains(key));
                        text = dictionary_english.at(key).Get(RawRequested);
                }

                out_file << ":" << key << "\n" << text << "\n.\n";

                if (out_file.fail()) {
                        LOG_ERR("LANG: I/O error writing translation file");
                        return false;
                }
        }

        return true;
}

// ***************************************************************************
// External interface
// ***************************************************************************

void MSG_Add(const std::string &key, const std::string &text)
{
        MSG_Add(key, 0, text);
}

void MSG_Add(const std::string &key, MsgFlags flags, const std::string &text)
{
        if (dictionary_english.contains(key)) {
                constexpr bool RawRequested = true;

                if (dictionary_english.at(key).Get(RawRequested) != text) {
                        dictionary_english.at(key).MarkInvalid();
                        LOG_ERR("LANG: Duplicate text for '%s'", key.c_str());
                }
                return;
        }

        message_order.push_back(key);
        dictionary_english.emplace(key, Message(flags, text));

        auto &english = dictionary_english.at(key);
        english.Verify(key);
        if (dictionary_translated.contains(key)) {
                dictionary_translated.at(key).Verify(key, english);
        }
}

const char* MSG_Get(const std::string &key)
{
        constexpr bool RawRequested = false;
        return get_message(key, RawRequested);
}

const char* MSG_GetRaw(const std::string &key)
{
        constexpr bool RawRequested = true;
        return get_message(key, RawRequested);
}

bool MSG_Exists(const std::string &key)
{
	return dictionary_english.contains(key);
}

bool MSG_WriteToFile(const std::string &file_name)
{
	return save_messages(file_name);
}

// MSG_Init loads the requested language provided on the command line or
// from the language = conf setting.

// 1. The provided language can be an exact filename and path to the lng
//    file, which is the traditionnal method to load a language file.

// 2. It also supports the more convenient syntax without needing to provide a
//    filename or path: `-lang ru`. In this case, it constructs a path into the
//    platform's config path/translations/<lang>.lng.

void MSG_Init([[maybe_unused]] Section_prop *section)
{
	static const std_fs::path subdirectory = "translations";
	static const std::string extension     = ".lng";

	const auto& host_language = GetHostLanguage();

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

	// If the language is English, use the internal messages
	if (language_file == "en" || language_file == "en.lng") {
		LOG_MSG("LANG: Using internal English language messages");
		return;
	}

	std_fs::path file_path = {};

	// If concrete language file is provided, try to load it
	if (!language_file.empty() && language_file != "auto") {
		if (language_file.ends_with(extension)) {
			file_path = GetResourcePath(subdirectory, language_file);
		} else {
			// If a short-hand name was provided,
                        // add the file extension
			file_path = GetResourcePath(subdirectory,
			                            language_file + extension);
		}

		const auto result = load_messages(file_path);
		if (!result) {
			LOG_MSG("LANG: Could not load language file '%s', "
			        "using internal English language messages",
			        language_file.c_str());
		} else {
			LOG_MSG("LANG: Loaded language file '%s'",
			        file_path.string().c_str());
		}

		return;
	}

	// If autodetection failed, use internal English messages
	if (host_language.language_file.empty()) {
		if (host_language.log_info.empty()) {
			LOG_MSG("LANG: Could not detect host value, "
			        "using internal English language messages");
		} else {
			LOG_MSG("LANG: Could not detected language file from host value '%s', "
			        "using internal English language messages",
			        host_language.log_info.c_str());
		}
		return;
	}

	// If language was detected, some extra information for log is expected
	assert(!host_language.log_info.empty());

	// If autodetected messages file is English, use internal messages
	if (host_language.language_file == "en") {
		LOG_MSG("LANG: Using internal English language messages "
		        "(detected from '%s')",
		        host_language.log_info.c_str());
		return;
	}

	// Try to load autodetected language
	const auto file_extension = host_language.language_file + extension;
	file_path = GetResourcePath(subdirectory, file_extension);
	const auto result = load_messages(file_path);

	if (result) {
		LOG_MSG("LANG: Loaded language file '%s' "
		        "(detected from '%s')",
		        file_extension.c_str(),
		        host_language.log_info.c_str());
	} else {
		LOG_MSG("LANG: Could not load language file '%s' "
		        "(detected from '%s'), "
		        "using internal English language messages",
		        file_extension.c_str(),
		        host_language.log_info.c_str());
	}
}
