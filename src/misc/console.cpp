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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "misc/console.h"
#include "util/checks.h"
#include "dos_inc.h"

CHECK_NARROWING();

static uint8_t last_written_character = '\n';

void CONSOLE_Write(std::string_view output)
{
	dos.internal_output = true;

	for (const auto& chr : output) {
		uint8_t out;
		uint16_t bytes_to_write = 1;

		if (chr == '\n' && last_written_character != '\r') {
			out = '\r';
			DOS_WriteFile(STDOUT, &out, &bytes_to_write);
		}

		out = static_cast<uint8_t>(chr);
		DOS_WriteFile(STDOUT, &out, &bytes_to_write);

		last_written_character = out;
	}

	dos.internal_output = false;
}

void CONSOLE_ResetLastWrittenChar(char c)
{
	last_written_character = c;
}

void CONSOLE_InjectMissingNewline()
{
	if (last_written_character == '\n') {
		return;
	}

	uint16_t num_bytes_to_write = 2;

	uint8_t dos_newline[] = "\r\n";

	dos.internal_output = true;
	DOS_WriteFile(STDOUT, dos_newline, &num_bytes_to_write);
	last_written_character = '\n';
}

