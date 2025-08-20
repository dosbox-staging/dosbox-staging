// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MESSAGES_H
#define DOSBOX_MESSAGES_H

#include <cstdint>
#include <string>

class MessageLocation {
public:
	MessageLocation(const std::string& file_name, const size_t line_number)
	        : file_name(file_name),
	          line_number(line_number)
	{}

	bool operator==(const MessageLocation& other) const;

	std::string GetUnified() const;

private:
	MessageLocation() = delete;

	const std::string file_name = {};
	const size_t line_number    = 0;
};

/*
 * Load translated messages according to the configuration.
 */
void MSG_LoadMessages();

/*
 * Add English message to the translation system in UTF-8.
 * The message may contais ANSI markup tags (e.g., [white]CONFIG[reset]).
 */
void MSG_Add(const std::string& message_key, const std::string& message,
             const MessageLocation& location = {__builtin_FILE(), __builtin_LINE()});

// TODO: Use 'std::source_location::current()' once all the supported compilers
// implement this C++20 feature; it should be possible to move 'MsgLocation'
// declaration to 'messages.cpp' then.

/*
 * Get translated message preprocessed for output to the DOS console.
 *
 * On success, this returns the translated message, or the English message if
 * no translation exists for the current language, adapted to the currently
 * active DOS code page with markup tags converted to ANSI escape codes.
 *
 * Returns an error message if an error occured.
 *
 * Use `CONSOLE_Write()` to output the message to the DOS console.
 */
std::string MSG_Get(const std::string& message_key);

/*
 * Get the original English message in UTF-8.
 *
 * On success, this returns the exact same byte-sequence previously added with
 * `MSG_Add()` without any DOS code page or ANSI markup tags conversions
 * applied.
 *
 * Returns an error message if an error occured.
 */
std::string MSG_GetEnglishRaw(const std::string& message_key);

/*
 * Get the translated message in UTF-8.
 *
 * On success, this returns the translated message, or English message if no
 * translation exists for the current language, without any DOS code page or
 * ANSI markup tags conversions applied. It effectively returns the translated
 * message as-is from the UTF-8 language file.
 *
 * Returns an error message if an error occured.
 */
std::string MSG_GetTranslatedRaw(const std::string& message_key);

/*
 * Return true if a message with this key exists.
 */
bool MSG_Exists(const std::string& message_key);

bool MSG_WriteToFile(const std::string& file_name);

void MSG_NotifyNewCodePage();

#endif // DOSBOX_MESSAGES_H
