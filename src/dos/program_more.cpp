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

#include "program_more_output.h"

#include "../ints/int10.h"
#include "callback.h"
#include "checks.h"
#include "dos_inc.h"
#include "string_utils.h"

#include <algorithm>
#include <array>
#include <cctype>

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
	if (!ParseCommandLine(output) || shutdown_requested) {
		return;
	}
	output.Display();
}

bool MORE::ParseCommandLine(MoreOutputFiles &output)
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
			output.SetTabSize(static_cast<uint8_t>(param.back() - '0'));
			params.erase(params.begin());
		}
	}

	// Make sure no other switches are supplied
	for (const auto &param : params) {
		if (starts_with("/", param)) {
			result_errorcode = DOSERR_FUNCTION_NUMBER_INVALID;
			WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), param.c_str());
			return false;
		}
	}

	// Create list of input files
	return FindInputFiles(output, params);
}

bool MORE::FindInputFiles(MoreOutputFiles &output,
                          const std::vector<std::string> &params)
{
	if (params.empty())
		return true;

	constexpr auto search_attr = UINT16_MAX & ~DOS_ATTR_DIRECTORY & ~DOS_ATTR_VOLUME;

	RealPt save_dta = dos.dta();
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
		if (!DOS_FindFirst(param.c_str(),
		                   static_cast<uint16_t>(search_attr))) {
			LOG_WARNING("DOS: MORE - no match for pattern '%s'",
			            param.c_str());
			continue;
		}

		found = true;
		while (!shutdown_requested) {
			CALLBACK_Idle();

			char name[DOS_NAMELENGTH_ASCII];
			uint32_t size = 0;
			uint16_t time = 0;
			uint16_t date = 0;
			uint8_t  attr = 0;

			DOS_DTA dta(dos.dta());
			dta.GetResult(name, size, date, time, attr);
			assert(name);

			const bool is_device = attr & DOS_ATTR_DEVICE;
			if (is_device) {
				output.AddFile(std::string(name), is_device);
			} else {
				output.AddFile(std::string(path) + std::string(name), is_device);
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
	MSG_Add("PROGRAM_MORE_NEXT_FILE",     "[reset][color=light-yellow](next file)[reset]");
}
