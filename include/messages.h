// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MESSAGES_H
#define DOSBOX_MESSAGES_H

#include <cstdint>
#include <string>

/*
 * Load translated messages according to the configuration.
 */
void MSG_LoadMessages();

/*
 * Add English message to the translation system in UTF-8.
 * The message may contais ANSI markup tags (e.g., [white]CONFIG[reset]).
 */
void MSG_Add(const std::string& name, const std::string& message);

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
const char* MSG_Get(const std::string& message_key);

/*
 * Get the original English message in UTF-8.
 *
 * On success, this returns the exact same byte-sequence previously added with
 * `MSG_Add()` without any DOS code page or ANSI markup tags conversions
 * applied.
 *
 * Returns an error message if an error occured.
 */
const char* MSG_GetEnglishRaw(const std::string& message_key);

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
const char* MSG_GetTranslatedRaw(const std::string& message_key);

/*
 * Return true if a message with this key exists.
 */
bool MSG_Exists(const std::string& message_key);

bool MSG_WriteToFile(const std::string& file_name);

void MSG_NotifyNewCodePage();

#endif // DOSBOX_MESSAGES_H
