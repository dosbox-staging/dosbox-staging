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

#ifndef DOSBOX_CONSOLE_H
#define DOSBOX_CONSOLE_H

#include <string>

#include "string_utils.h"

void CONSOLE_RawWrite(const std::string_view output);

template <typename... Args>
void CONSOLE_Write(const char* format, const Args&... args)
{
	const auto str = format_str(format, args...);
	CONSOLE_RawWrite(str);
}

template <typename... Args>
void CONSOLE_Write(const std::string& format, const Args&... args)
{
	CONSOLE_Write(format.c_str(), args...);
}

// TODO this seems like a hack; try to get rid of it later
uint8_t CONSOLE_GetLastWrittenCharacter();

#endif // DOSBOX_CONSOLE_H
