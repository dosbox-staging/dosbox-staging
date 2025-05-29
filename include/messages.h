/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2025  The DOSBox Staging Team
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
const char* MSG_Get(const std::string& name);

// Get message text in UTF-8, without ANSI markup tags conversions
const char* MSG_GetRaw(const std::string& name);

// Like 'MSG_GetRaw', but does not check if code page is compatible with current
// translation.
// Use this one if the string is going to be logged or sent to the host OS.
const char* MSG_GetForHost(const std::string& name);

bool MSG_Exists(const std::string& name);

bool MSG_WriteToFile(const std::string& file_name);

void MSG_NotifyNewCodePage();

#endif // DOSBOX_MESSAGES_H
