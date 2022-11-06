/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
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

#include "program_more.h"

#include "../ints/int10.h"
#include "callback.h"
#include "checks.h"
#include "dos_inc.h"
#include "string_utils.h"

#include <algorithm>
#include <array>
#include <cctype>

CHECK_NARROWING();

// ASCII control characters
constexpr char code_ctrl_c = 0x03; // end of text
constexpr char code_lf     = 0x0a; // line feed
constexpr char code_cr     = 0x0d; // carriage return

void MORE::Run()
{
	// Handle command line
	if (HelpRequested()) {
		WriteOut(MSG_Get("PROGRAM_MORE_HELP_LONG"));
		return;
	}
	if (!ParseCommandLine() || shutdown_requested)
		return;

	// Retrieve screen size, prepare limits
	constexpr uint16_t min_lines   = 10;
	constexpr uint16_t min_columns = 40;
	max_lines   = std::max(min_lines, INT10_GetTextRows());
	max_columns = std::max(min_columns, INT10_GetTextColumns());
	// The prompt at the bottom will cause scrolling,
	// so reduce the maximum number of lines accordingly
	max_lines = static_cast<uint16_t>(max_lines - 1);

	line_counter = 0;

	// Show STDIN or input file(s) content
	if (input_files.empty()) {
		DisplayInputStream();
	} else {
		DisplayInputFiles();

		// End message and command prompt is going to appear; ensure the
		// scrolling won't make top lines disappear before user reads them
		const int free_rows_threshold = 2;
		if (max_lines - line_counter < free_rows_threshold)
			PromptUser();

		WriteOut(MSG_Get("PROGRAM_MORE_END"));
		WriteOut("\n");
	}

	WriteOut("\n");
}

bool MORE::ParseCommandLine()
{
	// Put all the parameters into vector
	std::vector<std::string> params;
	cmd->FillVector(params);

	// Check if specified tabulation size
	if (!params.empty()) {
		const auto &param = params[0];
		if ((starts_with("/t", param) || starts_with("/T", param)) &&
		    (param.length() == 3) && (param.back() >= '1') &&
		    (param.back() <= '9')) {
			// FreeDOS extension - custom TAB size
			tab_size = static_cast<uint8_t>(param.back() - '0');
			params.erase(params.begin());
		}
	}

	// Make sure no other switches are supplied
	for (const auto &param : params)
		if (starts_with("/", param)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), param.c_str());
			return false;
		}

	// Create list of input files
	return FindInputFiles(params);
}

bool MORE::FindInputFiles(const std::vector<std::string> &params)
{
	input_files.clear();
	if (params.empty())
		return true;

	constexpr auto search_attr = UINT16_MAX & ~DOS_ATTR_DIRECTORY &
	                             ~DOS_ATTR_VOLUME;

	RealPt save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);

	for (const auto &param : params) {
		// Retrieve path to current file/pattern
		char path[DOS_PATHLENGTH];
		if (!DOS_Canonicalize(param.c_str(), path))
			continue;
		char *const end = strrchr(path, '\\') + 1;
		assert(end);
		*end = 0;

		// Search for the first file from pattern
		if (!DOS_FindFirst(param.c_str(),
		                   static_cast<uint16_t>(search_attr))) {
			LOG_WARNING("DOS: MORE.COM - no match for pattern '%s'",
			            param.c_str());
			continue;
		}

		while (!shutdown_requested) {
			CALLBACK_Idle();

			char name[DOS_NAMELENGTH_ASCII];
			uint32_t size = 0;
			uint16_t time = 0;
			uint16_t date = 0;
			uint8_t attr  = 0;

			DOS_DTA dta(dos.dta());
			dta.GetResult(name, size, date, time, attr);
			assert(name);

			input_files.emplace_back();
			auto &entry = input_files.back();

			if (attr & DOS_ATTR_DEVICE) {
				entry.is_device = true;
				entry.name      = std::string(name);
			} else {
				entry.is_device = false;
				entry.name = std::string(path) + std::string(name);
			}

			if (!DOS_FindNext()) {
				break;
			}
		}
	}

	dos.dta(save_dta);

	if (!shutdown_requested && input_files.empty()) {
		WriteOut(MSG_Get("PROGRAM_MORE_NO_FILE"));
		WriteOut("\n");
		return false;
	}

	return true;
}

std::string MORE::GetShortName(const std::string &file_name, const char *msg_id)
{
	assert(msg_id);

	// The shortest name we should be able to display is:
	// - 3 dots
	// - 1 path separator
	// - 8 characters of name
	// - 1 dot
	// - 3 characters of extension
	// This gives 16 characters.
	// We need to keep the last column free (reduces max length by 1).
	// Format string contains '%s' (increases max length by 2).
	constexpr size_t min = 16;
	const auto max_len   = std::max(min, max_columns - std::strlen(MSG_Get(msg_id)) + 1);

	// Nothing to do if file name maches the constraint
	if (file_name.length() <= max_len)
		return file_name;

	// We need to shorten the name - try to strip part of the path
	auto shortened = file_name;
	while (shortened.length() > max_len &&
	       std::count(shortened.begin(), shortened.end(), '\\') > 1) {
		// Strip one level of path at a time
		const auto pos = shortened.find('\\', shortened.find('\\') + 1);
		shortened      = std::string("...") + shortened.substr(pos);
	}

	// If still too long, just cut away the beginning
	const auto len = shortened.length();
	if (len > max_len)
		shortened = std::string("...") + shortened.substr(len - max_len + 3);

	return shortened;
}

void MORE::DisplayInputFiles()
{
	WriteOut("\n");

	bool first = true;
	for (const auto &input_file : input_files) {
		if (!first && UserDecision::Cancel == PromptUser())
			break;
		first = false;

		if (!DOS_OpenFile(input_file.name.c_str(), 0, &input_handle)) {
			LOG_WARNING("DOS: MORE.COM - could not open '%s'",
			            input_file.name.c_str());
			const auto short_name = GetShortName(input_file.name,
			                                     "PROGRAM_MORE_OPEN_ERROR");
			WriteOut(MSG_Get("PROGRAM_MORE_OPEN_ERROR"), short_name.c_str());
			WriteOut("\n");
			++line_counter;
			continue;
		}

		if (input_file.is_device) {
			const auto short_name = GetShortName(input_file.name,
			                                     "PROGRAM_MORE_NEW_DEVICE");
			WriteOut(MSG_Get("PROGRAM_MORE_NEW_DEVICE"), short_name.c_str());
		} else {
			const auto short_name = GetShortName(input_file.name,
			                                     "PROGRAM_MORE_NEW_FILE");
			WriteOut(MSG_Get("PROGRAM_MORE_NEW_FILE"), short_name.c_str());
		}
		WriteOut("\n");
		++line_counter;

		// If input from a device, CTRL+C shall quit
		ctrl_c_enable = input_file.is_device;

		const auto decision = DisplaySingleStream();
		DOS_CloseFile(input_handle);
		if (decision == UserDecision::Cancel) {
			break;
		}
	}
}

void MORE::DisplayInputStream()
{
	// We need to be able to read STDIN for key presses, but it is most
	// likely redirected - so clone the handle, and reconstruct real STDIN
	// from STDERR (idea from FreeDOS implementation,
	// https://github.com/FDOS/more/blob/master/src/more.c)
	if (!DOS_DuplicateEntry(STDIN, &input_handle) ||
	    !DOS_ForceDuplicateEntry(STDERR, STDIN)) {
		LOG_ERR("DOS: Unable to prepare handles in MORE.COM");
		return;
	}

	WriteOut("\n");

	// Since this CAN be STDIN input (there is no way to check),
	// CTRL+C shall quit
	ctrl_c_enable = true;
	DisplaySingleStream();
}

UserDecision MORE::DisplaySingleStream()
{
	auto previous_column = GetCurrentColumn();

	tabs_remaining = 0;
	skip_next_cr   = false;
	skip_next_lf   = false;

	auto decision = UserDecision::Next;
	while (true) {
		if (shutdown_requested) {
			decision = UserDecision::Cancel;
			break;
		}

		// Read character
		char code = 0;
		if (!GetCharacter(code)) {
			decision = UserDecision::Next; // end of current file
			break;
		}

		// A trick to make it more resistant to ANSI cursor movements
		const auto current_row = GetCurrentRow();
		if (line_counter > current_row)
			line_counter = current_row;

		// Handle new line characters
		bool new_line = false;
		if (code == code_cr) {
			skip_next_lf = true;
			new_line     = true;
		} else if (code == code_lf) {
			skip_next_cr = true;
			new_line     = true;
		} else {
			skip_next_cr = false;
			skip_next_lf = false;
		}

		// Duplicate character on the output
		if (new_line)
			code = '\n';
		WriteOut("%c", code);

		// Detect 'new line' due to character passing the last column
		const auto current_column = GetCurrentColumn();
		if (!current_column && previous_column) {
			new_line = true;
		}
		previous_column = current_column;

		// Update new line counter, decide if pause needed
		if (new_line && current_row) {
			++line_counter;
		}
		if (line_counter < max_lines) {
			continue;
		}

		// New line occured just enough times for a pause
		decision = PromptUser();
		if (decision == UserDecision::Cancel ||
		    decision == UserDecision::Next) {
			break;
		}
	}

	if (GetCurrentColumn()) {
		++line_counter;
		WriteOut("\n");
	}

	return decision;
}

UserDecision MORE::PromptUser()
{
	line_counter = 0;
	const bool multiple_files = input_files.size() > 1;

	if (GetCurrentColumn())
		WriteOut("\n");

	if (multiple_files)
		WriteOut(MSG_Get("PROGRAM_MORE_PROMPT_MULTI"));
	else
		WriteOut(MSG_Get("PROGRAM_MORE_PROMPT_SINGLE"));

	auto decision = UserDecision::Cancel;

	if (multiple_files)
		decision = DOS_WaitForCancelContinueNext();
	else
		decision = DOS_WaitForCancelContinue();

	if (decision == UserDecision::Cancel || decision == UserDecision::Next) {
		WriteOut(" ");
		WriteOut(MSG_Get("PROGRAM_MORE_TERMINATE"));
		WriteOut("\n");
		++line_counter;
	} else {
		// We are going to continue - erase the prompt
		WriteOut("\033[M"); // clear line
		auto counter = GetCurrentColumn();
		while (counter--)
			WriteOut("\033[D"); // cursor one position back
	}

	return decision;
}

bool MORE::GetCharacter(char &code)
{
	if (!tabs_remaining) {
		while (true) {
			// Retrieve character from input stream
			uint16_t count = 1;
			DOS_ReadFile(input_handle,
			             reinterpret_cast<uint8_t *>(&code),
			             &count);

			if (!count) {
				return false; // end of stream
			}

			if (ctrl_c_enable && code == code_ctrl_c) {
				if (input_files.empty()) {
					WriteOut("^C");
				}
				return false; // quit by CTRL+C
			}

			// Skip CR/LF characters if requested
			if (skip_next_cr && code == code_cr) {
				skip_next_cr = false;
			} else if (skip_next_lf && code == code_lf) {
				skip_next_lf = false;
			} else {
				break;
			}
		}

		// If TAB found, replace it with given number of spaces
		if (code == '\t') {
			tabs_remaining = tab_size;
		}
	}

	if (tabs_remaining) {
		--tabs_remaining;
		code = ' ';
	}

	return true;
}

uint8_t MORE::GetCurrentColumn()
{
	const auto page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
	return CURSOR_POS_COL(page);
}

uint8_t MORE::GetCurrentRow()
{
	const auto page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
	return CURSOR_POS_ROW(page);
}

void MORE::AddMessages()
{
	MSG_Add("PROGRAM_MORE_HELP_LONG",
	        "Display command output or text file one screen at a time.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=cyan]COMMAND[reset] | [color=green]more[reset] [/t[color=white]n[reset]]\n"
	        "  [color=green]more[reset] [/t[color=white]n[reset]] < [color=cyan]FILE[reset]\n"
	        "  [color=green]more[reset] [/t[color=white]n[reset]] [color=cyan]PATTERN[reset] [[color=cyan]PATTERN[reset] ...]\n"
	        "\n"
	        "Where:\n"
	        "  [color=cyan]COMMAND[reset] is the command to display the output of.\n"
	        "  [color=cyan]FILE[reset]    is an exact name of the file to display, optionally with a path.\n"
	        "  [color=cyan]PATTERN[reset] is either a path to a single file or a path with wildcards,\n"
	        "          which are the asterisk (*) and the question mark (?).\n"
	        "  [color=white]n[reset]       is the tab size, 1-9, default is 8.\n"
	        "\n"
	        "Notes:\n"
	        "  This command is only for viewing text files, not binary files.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=cyan]dir /on[reset] | [color=green]more[reset]             ; displays sorted directory one screen at a time\n"
	        "  [color=green]more[reset] /t[color=white]4[reset] < [color=cyan]A:\\MANUAL.TXT[reset]   ; shows the file's content with tab size 4\n");

	MSG_Add("PROGRAM_MORE_NO_FILE",       "No input file found.");
	MSG_Add("PROGRAM_MORE_END",           "[reset][color=light-yellow]--- end of input ---[reset]");
	MSG_Add("PROGRAM_MORE_NEW_FILE",      "[reset][color=light-yellow]--- file %s ---[reset]");
	MSG_Add("PROGRAM_MORE_NEW_DEVICE",    "[reset][color=light-yellow]--- device %s ---[reset]");
	MSG_Add("PROGRAM_MORE_PROMPT_SINGLE", "[reset][color=light-yellow]--- press SPACE for more ---[reset]");
	MSG_Add("PROGRAM_MORE_PROMPT_MULTI",  "[reset][color=light-yellow]--- press SPACE for more, N for next file ---[reset]");
	MSG_Add("PROGRAM_MORE_OPEN_ERROR",    "[reset][color=red]--- could not open %s ---[reset]");
	MSG_Add("PROGRAM_MORE_TERMINATE",     "[reset][color=light-yellow](terminated)[reset]");
}
