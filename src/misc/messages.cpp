// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include "config/config.h"
#include "config/setup.h"
#include "dos/dos_locale.h"
#include "hardware/input/mouse.h"
#include "host_locale.h"
#include "misc/ansi_code_markup.h"
#include "misc/cross.h"
#include "misc/std_filesystem.h"
#include "misc/support.h"
#include "misc/unicode.h"
#include "private/messages_adjust.h"
#include "private/messages_po_entry.h"
#include "utils/checks.h"
#include "utils/fs_utils.h"
#include "utils/string_utils.h"

#include <cctype>
#include <set>
#include <string>
#include <unordered_map>

CHECK_NARROWING();

static const std::string MsgNotFound = " MESSAGE NOT FOUND! ";
static const std::string MsgNotValid = " MESSAGE NOT VALID! ";

// ***************************************************************************
// Single message storage class
// ***************************************************************************

class Message {
public:
	// Note: any message needs to be verified before it can be safely used!

	Message(const std::string& message_english);
	Message(const std::string& message_english,
	        const std::string& message_translated);

	const std::string& Get();
	const std::string& GetRaw() const;

	bool IsFuzzy() const;
	bool IsValid() const;

	// Call to mark the message as requiring manual check, i.e. if outdated
	// translation is detected
	void MarkFuzzy();

	// Call to mark the message not suitable for usage, i.e. if C-style
	// format string mismatch is detected
	void MarkInvalid();

	// Use this one for English messages only
	void VerifyEnglish(const std::string& message_key);

	// Use this one for translated messages
	void VerifyTranslated(const std::string& message_key,
	                      const Message& message_english);

private:
	Message() = delete;

	std::string GetLogStart(const std::string& message_key) const;

	void AutoAdjustTranslation(const Message& message_english);

	void VerifyMessage(const std::string& message_key);
	void VerifyFormatString(const std::string& message_key);
	void VerifyNoLeftoverHelperLines(const std::string& message_key);

	void VerifyFormatStringAgainst(const std::string& message_key,
	                               const Message& message_english);

	void VerifyTranslationUpToDate(const std::string& message_key,
	                               const Message& message_english);

	static bool IsImportantHelp(const std::string& message_key);

	const bool is_english;

	bool is_fuzzy    = false;
	bool is_verified = false;
	bool is_ok       = true;

	// Original message, UTF-8, can contain DOSBox ANSI markups
	std::string message_raw = {};
	// Message in DOS encoding, markups converted to ANSI control codes
	std::string message_dos_ansi = {};

	// Previous English message, to detect outdated translations
	std::string message_previous_english = {};

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

Message::Message(const std::string& message_english)
        : is_english(true),
          message_raw(replace_eol(message_english, "\n"))
{}

Message::Message(const std::string& message_english,
                 const std::string& message_translated)
        : is_english(false),
          message_raw(replace_eol(message_translated, "\n")),
          message_previous_english(replace_eol(message_english, "\n"))
{}

bool Message::IsFuzzy() const
{
	return is_fuzzy;
}

bool Message::IsValid() const
{
	return is_verified && is_ok;
}

void Message::MarkFuzzy()
{
	is_fuzzy = true;
}

void Message::MarkInvalid()
{
	is_ok = false;
}

std::string Message::GetLogStart(const std::string& message_key) const
{
	std::string result = is_english ? "LOCALE: English message '"
	                                : "LOCALE: Translated message '";
	return result + message_key + "'";
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

const std::string& Message::GetRaw() const
{
	return message_raw;
}

void Message::AutoAdjustTranslation(const Message& message_english)
{
	// If the translation is known to be fuzzy, do not try to auto-adjust
	// it; translator attention was already requested here
	if (!is_ok || is_verified || is_fuzzy) {
		return;
	}

	// If the English string is unchanged, there is nothing in the
	// translated string that needs to be adjusted
	if (message_previous_english == message_english.message_raw) {
		return;
	}

	adjust_newlines(message_english.message_raw,
	                message_previous_english,
	                message_raw);
}

void Message::VerifyMessage(const std::string& message_key)
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
		LOG_WARNING("LOCALE: '%s' contains invalid character 0x%02x",
		            GetLogStart(message_key).c_str(),
		            item);
		MarkInvalid();
		break;
	}
}

void Message::VerifyFormatString(const std::string& message_key)
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
		LOG_WARNING("LOCALE: '%s' contains an incorrect format specifier: %s",
		            GetLogStart(message_key).c_str(),
		            error.c_str());
		MarkInvalid();
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
		format_specifiers.emplace_back();
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

void Message::VerifyNoLeftoverHelperLines(const std::string& message_key)
{
	if (!is_ok || is_verified) {
		return;
	}

	auto get_helper_line = [](const std::string& segment) {
		assert(!segment.empty());

		constexpr size_t LineLength = 80;

		std::string helper_line = {};
		helper_line.reserve(LineLength);

		while (helper_line.size() < LineLength) {
			helper_line += segment;
		}

		return helper_line.substr(0, LineLength);
	};

	static const std::vector<std::string> HelperLines = {
		get_helper_line("0123456789"),
		get_helper_line("1234567890"),
		get_helper_line("-")
	};

	for (const auto& helper_line : HelperLines) {

		if (message_raw.find(helper_line) == std::string::npos) {
			// No given leftover helper line in the translation
			continue;
		}

		if (message_previous_english.find(helper_line) != std::string::npos) {
			// Same string is present in the English message
			continue;
		}

		LOG_WARNING("LOCALE: '%s' contains a leftover helper line",
		            GetLogStart(message_key).c_str());
		MarkInvalid();

		break;
	}
}

void Message::VerifyFormatStringAgainst(const std::string& message_key,
                                        const Message& message_english)
{
	if (!is_ok || is_verified) {
		return;
	}

	// Check if the number of format specifiers match
	if (format_specifiers.size() != message_english.format_specifiers.size()) {
		LOG_WARNING(
		        "LOCALE '%s' has %d format specifier(s) "
		        "while English has %d specifier(s)",
		        GetLogStart(message_key).c_str(),
		        static_cast<int>(format_specifiers.size()),
		        static_cast<int>(message_english.format_specifiers.size()));

		MarkInvalid();
		return;
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
	                                  message_english.format_specifiers.size());

	for (size_t i = 0; i < index_limit; ++i) {
		const auto& specifier = format_specifiers[i];

		const auto& specifier_english = message_english.format_specifiers[i];

		if (!are_compatible(specifier.format, specifier_english.format) ||
		    (specifier.width == "*" && specifier_english.width != "*") ||
		    (specifier.width != "*" && specifier_english.width == "*") ||
		    (specifier.precision == "*" && specifier_english.precision != "*") ||
		    (specifier.precision != "*" && specifier_english.precision == "*")) {

			LOG_WARNING(
			        "LOCALE: '%s' has format specifier '%s' "
			        "incompatible with English counterpart '%s'",
			        GetLogStart(message_key).c_str(),
			        specifier.AsString().c_str(),
			        specifier_english.AsString().c_str());

			MarkInvalid();
			break;
		}
	}
}

bool Message::IsImportantHelp(const std::string& message_key)
{
	// First check for certain hardcoded messages/prefixes

	static const std::vector<std::string> ImportantMessagesList = {
	        "DOSBOX_HELP", "AUTOEXEC_CONFIGFILE_HELP", "CONFIGFILE_INTRO"};

	static const std::vector<std::string> ImportantMessagePrefixes = {
	        // Config file option descriptions
	        "CONFIG_",
	        "CONFIGITEM_",
	        // AUTOEXEC.BAT messages
	        "AUTOEXEC_BAT_",
	};

	if (contains(ImportantMessagesList, message_key)) {
		return true;
	}

	for (const auto& prefix : ImportantMessagePrefixes) {
		if (message_key.starts_with(prefix)) {
			return true;
		}
	}

	// Check for program/command help messages

	static const std::string ShellPrefix   = "SHELL_CMD_";
	static const std::string ProgramPrefix = "PROGRAM_";
	static const std::string HelpSuffix    = "_HELP";

	auto is_help_message = [&](const std::string& prefix) {
		if (!message_key.starts_with(prefix)) {
			return false;
		}

		const auto prefix_length   = prefix.length();
		const auto suffix_position = message_key.find(HelpSuffix);

		if (suffix_position == std::string::npos ||
		    suffix_position <= prefix_length) {
			return false;
		}

		const auto interior = std::string_view(message_key).substr(
			prefix_length, suffix_position - prefix_length);

		return std::ranges::count(interior, '_') == 0;
	};

	return is_help_message(ShellPrefix) || is_help_message(ProgramPrefix);
}

void Message::VerifyTranslationUpToDate(const std::string& message_key,
                                        const Message& message_english)
{
	assert(!is_english);

	if (message_previous_english.empty() ||
	    message_previous_english != message_english.GetRaw()) {

		MarkFuzzy();
	}

	if (IsFuzzy() && IsImportantHelp(message_key)) {

		// If the translation is not up to date (fuzzy), important help
		// messages shall use English strings
		MarkInvalid();
	}
}

void Message::VerifyEnglish(const std::string& message_key)
{
	assert(is_english);
	if (is_verified) {
		return;
	}

	VerifyFormatString(message_key);
	VerifyMessage(message_key);
	is_verified = true;
}

void Message::VerifyTranslated(const std::string& message_key,
                               const Message& message_english)
{
	assert(!is_english);
	if (is_verified) {
		return;
	}

	VerifyFormatString(message_key);
	VerifyNoLeftoverHelperLines(message_key);
	if (message_english.IsValid()) {
		AutoAdjustTranslation(message_english);
		VerifyTranslationUpToDate(message_key, message_english);
		VerifyFormatStringAgainst(message_key, message_english);
	}

	VerifyMessage(message_key);
	is_verified = message_english.IsValid();
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
// Single message location storage class
// ***************************************************************************

bool MessageLocation::operator==(const MessageLocation& other) const
{
	return file_name == other.file_name && line_number == other.line_number;
}

std::string MessageLocation::GetUnified() const
{
	// Convert the path to Unix format - we need it to be uniform between
	// all the platforms.  Strip the part before the 'src' directrory.

	const std::string RootSourceDirectory = "src";

	std::vector<std::string> path_elements = {};
	for (const auto& token : std_fs::path(file_name)) {
		if (token.string() == RootSourceDirectory) {
			path_elements.clear();
		}
		path_elements.emplace_back(token.string());
	}

	std::string unified_path = {};
	for (const auto& token : path_elements) {
		if (!unified_path.empty()) {
			unified_path.push_back('/');
		}
		unified_path += token;
	}

	return unified_path + ":" + std::to_string(line_number);
}

// ***************************************************************************
// Internal implementation
// ***************************************************************************

static std::vector<std::string> message_order = {};

static std::unordered_map<std::string, MessageLocation> message_location = {};

static std::unordered_map<std::string, Message> dictionary_english    = {};
static std::unordered_map<std::string, Message> dictionary_translated = {};

static std::string translation_language         = {};
static std::optional<Script> translation_script = {};
static bool is_translation_script_fuzzy         = false;

// Whether the translation is compatible with the current code page
static bool is_code_page_compatible = true;

static std::set<std::string> already_warned_not_found = {};

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

static bool check_message_exists(const std::string& message_key)
{
	if (!dictionary_english.contains(message_key)) {
		if (!already_warned_not_found.contains(message_key)) {
			LOG_WARNING("LOCALE: Message '%s' not found",
			            message_key.c_str());
			already_warned_not_found.insert(message_key);
		}
		return false;
	}
	return true;
}

static void clear_translated_messages()
{
	const bool notify_new_language = !translation_language.empty();

	dictionary_translated.clear();

	translation_language        = {};
	translation_script          = {};
	is_translation_script_fuzzy = false;

	if (notify_new_language) {
		MOUSE_NotifyLanguageChanged();
	}
}

// ***************************************************************************
// Messages loading/saving
// ***************************************************************************

static bool write_dosbox_metadata_script(PoWriter& writer, const std::string& language)
{
	std::string script_name = {};
	if (translation_script) {
		// Writing translated messages, script is specified
		assert(LocaleData::ScriptInfo.contains(*translation_script));
		script_name = LocaleData::ScriptInfo.at(*translation_script).script_name;

	} else if (dictionary_translated.empty() && language == "en") {
		// Writing English messages, script is always Latin
		assert(LocaleData::ScriptInfo.contains(Script::Latin));
		script_name = LocaleData::ScriptInfo.at(Script::Latin).script_name;
	}

	std::vector<std::string> help = {};
	help.emplace_back("Writing script used in this language, can be one of:");
	help.emplace_back();

	for (const auto& entry : LocaleData::ScriptInfo) {
		if (!help.back().empty()) {
			help.back() += ", ";
		}
		help.back() += entry.second.script_name;
	}

	const bool is_fuzzy = script_name.empty() || is_translation_script_fuzzy;

	return writer.WriteDosBoxMetadata(PoEntry::MetadataKeyScript,
	                                  script_name,
	                                  help,
	                                  is_fuzzy);
}

static bool write_messages(PoWriter& writer)
{
	for (const auto& message_key : message_order) {
		assert(message_location.contains(message_key));
		assert(dictionary_english.contains(message_key));

		writer.AddFlag(PoEntry::FlagCFormat);
		writer.AddFlag(PoEntry::FlagNoWrap);
		writer.SetContext(message_key);
		writer.SetLocation(message_location.at(message_key).GetUnified());
		writer.SetEnglish(dictionary_english.at(message_key).GetRaw());

		bool is_fuzzy = false;
		if (dictionary_translated.contains(message_key)) {
			const auto translated = dictionary_translated.at(message_key);
			// Translated message exists
			writer.SetTranslated(translated.GetRaw());
			if (translated.IsFuzzy() || !translated.IsValid()) {
				is_fuzzy = true;
			}
		} else {
			// Translation is missing
			is_fuzzy = true;
		}

		if (is_fuzzy) {
			writer.AddFlag(PoEntry::FlagFuzzy);
		}

		if (!writer.WriteEntry()) {
			return false;
		}
	}

	return true;
}

static void read_dosbox_metadata_script(const PoReader& reader)
{
	if (translation_script) {
		reader.LogWarning("writing script already known");
		is_translation_script_fuzzy = true;
		return;
	}

	const auto& value = reader.GetTranslated();

	for (const auto& entry : LocaleData::ScriptInfo) {
		if (iequals(value, entry.second.script_name)) {
			translation_script = entry.first;
			is_translation_script_fuzzy = reader.HasFlag(PoEntry::FlagFuzzy);
			break;
		}
	}
	if (!translation_script) {
		reader.LogWarning("unknown writing script");
		is_translation_script_fuzzy = true;
		return;
	}
}

static void read_dosbox_metadata(const PoReader& reader)
{
	// Only check the first line - remaining ones are a help message
	const auto tmp = reader.GetEnglish();
	const auto key = tmp.substr(0, tmp.find('\n'));

	if (key == PoEntry::MetadataKeyScript) {
		read_dosbox_metadata_script(reader);
	} else {
		reader.LogWarning("unknown DOSBox metadata");
	}
}

static void read_message(const PoReader& reader)
{
	const auto message_key = reader.GetContext();
	if (message_key.empty()) {
		reader.LogWarning("no message content");
		return;
	}

	const auto translated = reader.GetTranslated();
	if (translated.empty()) {
		// Message was not translated, skip reading
		return;
	}

	const auto english = reader.GetEnglish();

	dictionary_translated.try_emplace(message_key, Message(english, translated));

	auto& message = dictionary_translated.at(message_key);
	if (reader.HasFlag(PoEntry::FlagFuzzy)) {
		message.MarkFuzzy();
	}

	if (dictionary_english.contains(message_key)) {
		message.VerifyTranslated(message_key,
		                         dictionary_english.at(message_key));
	}
}

static bool load_messages_from_path(const std_fs::path& file_path)
{
	PoReader reader(file_path);
	if (!reader.IsFileOk()) {
		LOG_ERR("LOCALE: Error opening the translation file '%s'",
		        file_path.string().c_str());
		return false;
	}

	clear_translated_messages();

	bool found_message = false;
	while (reader.ReadEntry()) {
		// Check if gettext metadata indicates file suitable for reading
		if (reader.IsGettextMetadataEntry()) {
			if (!reader.ValidateGettextMetadata()) {
				break;
			}
			const auto language = reader.GetLanguageFromMetadata();
			if (!language.empty()) {
				translation_language = language;
			}
			continue;
		}

		if (reader.IsFirstEntry()) {
			reader.LogWarning(
			        "first entry should only contain "
			        "gettext metadata");
		}

		// Read metadata
		if (reader.IsDosBoxMetadataEntry()) {
			read_dosbox_metadata(reader);
			continue;
		}

		// Read message
		found_message = true;
		read_message(reader);
	}

	if (!reader.IsFileOk()) {
		LOG_ERR("LOCALE: I/O error reading the translation file");
		clear_translated_messages();
		return false;
	}

	if (!found_message) {
		reader.LogWarning("no messages found in the file");
	}

	// If no language was found in the metadata, use the file name instead
	if (translation_language.empty()) {
		translation_language = std_fs::path(file_path).stem().string();
	}

	// Check if current code page is suitable for this translation
	if (dos.loaded_codepage) {
		check_code_page();
	}

	MOUSE_NotifyLanguageChanged();
	return true;
}

static bool save_messages_to_path(const std_fs::path& file_path)
{
	if (file_path.empty()) {
		return false;
	}

	const auto language = std_fs::path(file_path).stem().string();

	PoWriter writer(file_path);

	if (!writer.WriteHeader(language) || !writer.WriteEmptyLine() ||
	    !write_dosbox_metadata_script(writer, language) ||
	    !writer.WriteEmptyLine() || !write_messages(writer)) {
		LOG_ERR("LOCALE: I/O error writing translation file");
		return false;
	}

	return true;
}

// ***************************************************************************
// External interface
// ***************************************************************************

void MSG_Add(const std::string& message_key, const std::string& message,
             const MessageLocation& location)
{
	if (message_location.contains(message_key)) {
		if (message_location.at(message_key) != location) {
			LOG_ERR("LOCALE: Text for '%s' defined in multiple locations",
			        message_key.c_str());
			return;
		}
	} else {
		message_location.try_emplace(message_key, location);
	}

	if (dictionary_english.contains(message_key)) {
		if (dictionary_english.at(message_key).GetRaw() != message) {
			dictionary_english.at(message_key).MarkInvalid();
			LOG_ERR("LOCALE: Duplicate text for '%s'",
			        message_key.c_str());
		}
		return;
	}

	message_order.push_back(message_key);
	dictionary_english.try_emplace(message_key, Message(message));

	auto& message_english = dictionary_english.at(message_key);
	message_english.VerifyEnglish(message_key);
	if (dictionary_translated.contains(message_key)) {
		dictionary_translated.at(message_key)
		        .VerifyTranslated(message_key, message_english);
	}
}

std::string MSG_Get(const std::string& message_key)
{
	if (!check_message_exists(message_key)) {
		return MsgNotFound;
	}

	// Try to return the translated message converted to the current DOS
	// code page and the ANSI tags converted to ANSI sequences
	if (is_code_page_compatible && dictionary_translated.contains(message_key) &&
	    dictionary_translated.at(message_key).IsValid()) {

		return dictionary_translated.at(message_key).Get();
	}

	// Fall back to English if any errors
	if (!dictionary_english.at(message_key).IsValid()) {
		return MsgNotValid;
	}
	return dictionary_english.at(message_key).Get();
}

std::string MSG_GetEnglishRaw(const std::string& message_key)
{
	if (!check_message_exists(message_key)) {
		return MsgNotFound;
	}

	// Return English original in UTF-8 with the ANSI tags intact
	if (!dictionary_english.at(message_key).IsValid()) {
		return MsgNotValid;
	}
	return dictionary_english.at(message_key).GetRaw();
}

std::string MSG_GetTranslatedRaw(const std::string& message_key)
{
	if (!check_message_exists(message_key)) {
		return MsgNotFound;
	}

	// Try to return the translated message in UTF-8 with the ANSI tags intact
	if (dictionary_translated.contains(message_key) &&
	    dictionary_translated.at(message_key).IsValid()) {

		return dictionary_translated.at(message_key).GetRaw();
	}

	// Fall back to the English message if any errors
	return MSG_GetEnglishRaw(message_key);
}

bool MSG_Exists(const std::string& message_key)
{
	return dictionary_english.contains(message_key);
}

std::string MSG_GetLanguage()
{
	if (!translation_language.empty()) {
		return translation_language;
	}

	return "en";
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
//
// 1. The provided language can be an exact filename and path to the lng
//    file, which is the traditionnal method to load a language file.
//
// 2. It also supports the more convenient syntax without needing to provide a
//    filename or path: `-lang ru`. In this case, it constructs a path into the
//    platform's config path/translations/<lang>.po.

static const std::string InternalLangauge = "en";
static const std::string Extension        = ".po";
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

	if (file_path.empty()) {
		LOG_WARNING("LOCALE: Translation file '%s' not found",
		            file_with_extension.c_str());
		return false;
	}

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

		language_file = static_cast<const SectionProp*>(section)->GetString(
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
	auto languages = host_languages.app_languages;
	languages.insert(languages.end(),
	                 host_languages.gui_languages.begin(),
	                 host_languages.gui_languages.end());

	std::vector<std::string> language_files = {};
	for (const auto& language : languages) {
		for (const auto& language_file : language.GetLanguageFiles()) {
			if (contains(language_files, language_file)) {
				continue;
			}
			language_files.push_back(language_file);
		}
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
		if (file_path.empty()) {
			continue;
		}

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
