// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "program_clip.h"

#include "checks.h"
#include "clipboard.h"
#include "dos_inc.h"
#include "math_utils.h"
#include "program_more_output.h"

CHECK_NARROWING();

// Maximum allowed size of data to copy to host clipboard
constexpr uint32_t MaxFileSize = 16 * 1024 * 1024;

// NOTE: Although the command did not exist in the original MS-DOS, it was
// available on the official 'Windows 98 Resource Kit' CD and now is a part of
// modern Microsoft Windows.

void CLIP::Run()
{
	// Handle command line
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_CLIP_HELP_LONG"));
		output.Display();
		return;
	}

	if (!cmd->GetArguments().empty()) {
		WriteOut(MSG_Get("SHELL_TOO_MANY_PARAMETERS"));
		return;
	}

	// Prepare handle for standard input
	uint16_t input_handle = 0;
	DOS_DuplicateEntry(STDIN, &input_handle);

	// Check if we have any input
	uint32_t input_size = 0;
	DOS_SeekFile(input_handle, &input_size, DOS_SEEK_END);
	if (input_size == 0) {
		// We don't - print out the clipboard content and exit
		WriteOut(CLIPBOARD_PasteText().c_str());
		return;
	}

	// Check if input is suitable
	if (input_size > MaxFileSize) {
		WriteOut(MSG_Get("PROGRAM_CLIP_INPUT_TOO_LARGE"));
		return;
	}

	// Go back to the first byte of the input
	uint32_t begin = 0;
	DOS_SeekFile(input_handle, &begin, DOS_SEEK_SET);

	// Prepare buffer for input data
	std::string input_data = {};
	input_data.reserve(input_size);

	// Read the input
	uint32_t remaining = input_size;
	while (remaining != 0) {
		uint16_t amount = (remaining < UINT16_MAX)
		                        ? clamp_to_uint16(remaining)
		                        : UINT16_MAX;

		std::string buffer = {};
		buffer.resize(amount);
		const auto position = reinterpret_cast<uint8_t*>(buffer.data());
		if (!DOS_ReadFile(input_handle, position, &amount) || amount == 0) {
			WriteOut(MSG_Get("PROGRAM_CLIP_READ_ERROR"));
			return;
		}

		input_data += buffer;
		remaining -= amount;
	}

	// Copy text to the clipboard
	CLIPBOARD_CopyText(input_data);
}

void CLIP::AddMessages()
{
	MSG_Add("PROGRAM_CLIP_HELP_LONG",
	        "Copy text to the clipboard or retrieve the clipboard's content.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-cyan]COMMAND[reset] | [color=light-green]clip[reset]\n"
	        "  [color=light-green]clip[reset] < [color=light-cyan]FILE[reset]\n"
	        "  [color=light-green]clip[reset]\n"
	        "\n"
	        "Notes:\n"
	        "  - If no input is provided, the command prints out the clipboard's content.\n"
	        "  - This command is only for handling text data, not binary data.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-cyan]dir[reset] | [color=light-green]clip[reset]               ; copies the list of files to the clipboard\n"
	        "  [color=light-green]clip[reset] < [color=light-cyan]Z:\\AUTOEXEC.BAT[reset]   ; copies the file to the clipboard\n"
	        "  [color=light-green]clip[reset]                     ; displays the clipboard's content\n");

	MSG_Add("PROGRAM_CLIP_INPUT_TOO_LARGE", "Input stream too large.\n");
	MSG_Add("PROGRAM_CLIP_READ_ERROR", "Error reading input stream.\n");
}
