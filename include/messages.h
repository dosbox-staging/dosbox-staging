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

// Get message text in UTF-8, without ANSI markup tags conversions
const char* MSG_GetRaw(const std::string& message_key);

// Like 'MSG_GetRaw', but does not check if code page is compatible with current
// translation.
// Use this one if the string is going to be logged or sent to the host OS.
const char* MSG_GetForHost(const std::string& message_key);

bool MSG_Exists(const std::string& message_key);

bool MSG_WriteToFile(const std::string& file_name);

void MSG_NotifyNewCodePage();

#endif // DOSBOX_MESSAGES_H
