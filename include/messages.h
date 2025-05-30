// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MESSAGES_H
#define DOSBOX_MESSAGES_H

#include <cstdint>
#include <string>

// Load translated messages according to the configuration
void MSG_LoadMessages();

// Add message (in UTF-8 encoding) to the translation system
void MSG_Add(const std::string& name, const std::string& message);

// Get message text, adapted to the current code page,
// with markup tags converted to ANSI escape codes
const char* MSG_Get(const std::string& message_key);

/*
 * Get the original English message in UTF-8.
 *
 * On success, this returns the exact same byte-sequence previously added with
 * `MSG_Add()` without any DOS code page or ANSI markup tags conversions
 * applied.
 *
 * Returns " MESSAGE NOT FOUND! " or " MESSAGE NOT VALID! " if there was an
 * error.
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
 * Returns " MESSAGE NOT FOUND! " or " MESSAGE NOT VALID! " if there was an
 * error.
 */
const char* MSG_GetTranslatedRaw(const std::string& message_key);

bool MSG_Exists(const std::string& message_key);

bool MSG_WriteToFile(const std::string& file_name);

void MSG_NotifyNewCodePage();

#endif // DOSBOX_MESSAGES_H
