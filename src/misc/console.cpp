// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "misc/console.h"
#include "dos/dos.h"
#include "utils/checks.h"

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

