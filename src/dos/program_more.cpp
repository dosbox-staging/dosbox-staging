/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
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

#include "program_more_output.h"

#include "../ints/int10.h"
#include "callback.h"
#include "checks.h"
#include "dos_inc.h"
#include "string_utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>

CHECK_NARROWING();

extern unsigned int result_errorcode;

void MORE::Run()
{
	// Handle command line
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_MORE_HELP_LONG"));
		output.Display();
		return;
	}

	MoreOutputFiles output(*this);
	if (!ParseCommandLine(output) || !FindInputFiles(output) ||
	    shutdown_requested) {
		return;
	}
	output.Display();
}

bool MORE::ParseCommandLine(MoreOutputFiles &output)
{
	// Check (and remove if found) all the simple arguments
	auto has_arg = [&](const char* arg) {
		constexpr bool remove_if_found = true;
		return cmd->FindExist(arg, remove_if_found);
	};
	output.SetOptionClear(has_arg("/c"));
	output.SetOptionExtendedMode(has_arg("/e"));
	output.SetOptionExpandFormFeed(has_arg("/p"));
	output.SetOptionSquish(has_arg("/s"));

	std::string tmp_str;

	// Check if specified tabulation size
	if (cmd->FindStringBegin("/t", tmp_str, true)) {
		const auto value = parse_int(tmp_str);
		if (!value || *value < 1 || *value > 9) {
			std::string full_switch = std::string("/t") + tmp_str;
			result_errorcode = DOSERR_FUNCTION_NUMBER_INVALID;
			WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), full_switch.c_str());
			return false;
		}

		output.SetOptionTabSize(static_cast<uint8_t>(*value));
	}

	// Check if specified start line
	if (cmd->FindStringBegin("+", tmp_str, true)) {
		const auto value = parse_int(tmp_str);
		if (!value || *value < 0) {
			std::string full_switch = std::string("+") + tmp_str;
			result_errorcode = DOSERR_FUNCTION_NUMBER_INVALID;
			WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), full_switch.c_str());
			return false;
		}

		output.SetOptionStartLine(static_cast<uint32_t>(*value));
	}

	// Make sure no other switches are supplied
	if (cmd->FindStringBegin("/", tmp_str)) {
		tmp_str = std::string("/") + tmp_str;
		result_errorcode = DOSERR_FUNCTION_NUMBER_INVALID;
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), tmp_str.c_str());
		return false;
	}

	return true;
}

bool MORE::FindInputFiles(MoreOutputFiles &output)
{
	const auto params = cmd->GetArguments();
	if (params.empty()) {
		return true;
	}

	FatAttributeFlags search_attr = {UINT8_MAX};
	search_attr.directory         = false;
	search_attr.volume            = false;

	const RealPt save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);

	bool found = false;
	for (const auto &param : params) {
		// Retrieve path to current file/pattern
		char path[DOS_PATHLENGTH];
		if (!DOS_Canonicalize(param.c_str(), path)) {
			continue;
		}
		char *const end = strrchr(path, '\\') + 1;
		assert(end);
		*end = 0;

		// Search for the first file from pattern
		if (!DOS_FindFirst(param.c_str(), search_attr)) {
			LOG_WARNING("DOS: MORE - no match for pattern '%s'",
			            param.c_str());
			continue;
		}

		found = true;
		while (!shutdown_requested) {
			CALLBACK_Idle();

			DOS_DTA::Result search_result = {};

			const DOS_DTA dta(dos.dta());
			dta.GetResult(search_result);

			const bool is_device = search_result.IsDevice();
			if (is_device) {
				output.AddFile(search_result.name, is_device);
			} else {
				output.AddFile(std::string(path) + search_result.name,
				               is_device);
			}

			if (!DOS_FindNext()) {
				break;
			}
		}
	}

	dos.dta(save_dta);

	if (!shutdown_requested && !found) {
		result_errorcode = DOSERR_FILE_NOT_FOUND;
		WriteOut(MSG_Get("PROGRAM_MORE_NO_FILE"));
		WriteOut("\n");
		return false;
	}

	return true;
}

void MORE::AddMessages()
{
	MSG_Add("PROGRAM_MORE_HELP_LONG",
	        "Display command output or text file one screen at a time.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-cyan]COMMAND[reset] | [color=light-green]more[reset] [/c] [/e] [/p] [[reset]/s] [/t[color=white]n[reset]] [+[color=white]nnn[reset]]\n"
	        "  [color=light-green]more[reset] [/c] [/e] [/p] [[reset]/s] [/t[color=white]n[reset]] [+[color=white]nnn[reset]] < [color=light-cyan]FILE[reset]\n"
	        "  [color=light-green]more[reset] [/c] [/e] [/p] [[reset]/s] [/t[color=white]n[reset]] [+[color=white]nnn[reset]] [color=light-cyan]PATTERN[reset] [[color=light-cyan]PATTERN[reset] ...]\n"
	        "\n"
	        "Where:\n"
	        "  [color=light-cyan]COMMAND[reset] is the command to display the output of.\n"
	        "  [color=light-cyan]FILE[reset]    is an exact name of the file to display, optionally with a path.\n"
	        "  [color=light-cyan]PATTERN[reset] is either a path to a single file or a path with wildcards,\n"
	        "          which are the asterisk (*) and the question mark (?).\n"
	        "  /c      clears the screen before each file.\n"
	        "  /e      extended mode, with more hotkeys available.\n"
	        "  /p      expands the new page / form feed character.\n"
	        "  /s      squishes multiple empty lines into one.\n"
	        "  /t[color=white]n[reset]     specifies the tab size, 1-9, default is 8.\n"
	        "  +[color=white]nnn[reset]    skips first [color=white]nnn[reset] lines of the first file.\n"
	        "\n"
	        "Notes:\n"
	        "  This command is only for viewing text files, not binary files.\n"
	        "  The following hotkeys are available:\n"
	        "  [color=yellow]Space[reset]          to show the next screen.\n"
	        "  [color=yellow]Enter[reset]          to show the next line.\n"
	        "  [color=yellow]N[reset] or [color=yellow]F[reset]         to skip to the next file.\n"
	        "  [color=yellow]Q[reset], [color=yellow]Esc[reset], [color=yellow]Ctrl+C[reset] to terminate the command.\n"
	        "  Also, the [color=yellow]Ctrl+C[reset] can be used to terminate the command reading data from the\n"
	        "  keyboard input, like when [color=light-green]more[reset] is executed without any arguments.\n"
	        "  The following extra hotkeys are available in extended mode only:\n"
	        "  [color=yellow]P[reset] [color=white]nnn[reset]          to display the next [color=white]nnn[reset] lines and prompt again.\n"
	        "  [color=yellow]S[reset] [color=white]nnn[reset]          to skip the next [color=white]nnn[reset] lines.\n"
	        "  [color=yellow]=[reset]              to display the current line number.\n"
	        "  Option /p disables certain incompatible hotkeys.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-cyan]dir /on[reset] | [color=light-green]more[reset]             ; displays sorted directory one screen at a time\n"
	        "  [color=light-green]more[reset] /t[color=white]4[reset] < [color=light-cyan]A:\\MANUAL.TXT[reset]   ; shows the file's content with tab size 4\n");

	MSG_Add("PROGRAM_MORE_NO_FILE", "No input file found.");
	MSG_Add("PROGRAM_MORE_END",
	        "[reset][color=brown]--- end of input ---[reset]");
	MSG_Add("PROGRAM_MORE_NEW_FILE",
	        "[reset][color=brown]--- file %s ---[reset]");
	MSG_Add("PROGRAM_MORE_NEW_DEVICE",
	        "[reset][color=brown]--- device %s ---[reset]");
	MSG_Add("PROGRAM_MORE_PROMPT_SINGLE",
	        "[reset][color=brown]--- press SPACE for next page, ENTER for next line, Q to quit ---[reset]");
	MSG_Add("PROGRAM_MORE_PROMPT_PERCENT",
	        "[reset][color=brown]--- (%d%%) press SPACE for next page, ENTER for next line, Q to quit ---[reset]");
	MSG_Add("PROGRAM_MORE_PROMPT_MULTI",
	        "[reset][color=brown]--- press SPACE or ENTER for more, N for next file, Q to quit ---[reset]");
	MSG_Add("PROGRAM_MORE_PROMPT_SHORT",
	        "[reset][color=brown]--- more ---[reset]");
	MSG_Add("PROGRAM_MORE_PROMPT_SHORT_PERCENT",
	        "[reset][color=brown]--- (%d%%) more ---[reset]");
	MSG_Add("PROGRAM_MORE_PROMPT_LINE",
	        "[reset][color=brown]--- line %u ---[reset]");
	MSG_Add("PROGRAM_MORE_OPEN_ERROR",
	        "[reset][color=light-red]--- could not open %s ---[reset]");
	MSG_Add("PROGRAM_MORE_TERMINATE",
	        "[reset][color=brown](terminated)[reset]");
	MSG_Add("PROGRAM_MORE_NEXT_FILE",
	        "[reset][color=brown](next file)[reset]");
	MSG_Add("PROGRAM_MORE_SKIPPED",
	        "[reset][color=brown](skipped content)[reset]");
	MSG_Add("PROGRAM_MORE_HOW_MANY_LINES",
	        "[reset][color=brown]how many lines?[reset]");
}
