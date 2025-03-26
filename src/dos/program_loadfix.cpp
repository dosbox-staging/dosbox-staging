/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "program_loadfix.h"

#include "dosbox.h"
#include "program_more_output.h"
#include "shell.h"
#include "string_utils.h"

void LOADFIX::Run(void)
{
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_LOADFIX_HELP_LONG"));
		output.Display();
		return;
	}
	uint16_t commandNr = 1;
	uint16_t kb        = 64;
	if (cmd->FindCommand(commandNr, temp_line)) {
		if (temp_line[0] == '-' || temp_line[0] == '/') {
			const auto ch = std::toupper(temp_line[1]);
			if ((ch == 'D') || (ch == 'F')) {
				// Deallocate all
				DOS_FreeProcessMemory(0x40);
				WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOCALL"), kb);
				return;
			} else {
				// Set mem amount to allocate
				kb = atoi(temp_line.c_str() + 1);
				if (kb == 0) {
					kb = 64;
				}
				commandNr++;
			}
		}
	}
	// Allocate Memory
	uint16_t segment;
	uint16_t blocks = kb * 1024 / 16;
	if (DOS_AllocateMemory(&segment, &blocks)) {
		DOS_MCB mcb((uint16_t)(segment - 1));
		mcb.SetPSPSeg(0x40); // use fake segment
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ALLOC"), kb);
		// Prepare commandline...
		if (cmd->FindCommand(commandNr++, temp_line)) {
			// get Filename
			char filename[128];
			safe_strcpy(filename, temp_line.c_str());
			// Setup commandline
			char args[256 + 1];
			args[0]    = 0;
			bool found = cmd->FindCommand(commandNr++, temp_line);
			while (found) {
				if (safe_strlen(args) + temp_line.length() + 1 > 256) {
					break;
				}
				safe_strcat(args, temp_line.c_str());
				found = cmd->FindCommand(commandNr++, temp_line);
				if (found) {
					safe_strcat(args, " ");
				}
			}
			// Use shell to start program
			DOS_Shell shell;
			// If it's a batch file, this call places it into an
			// internal data structure.
			shell.ExecuteProgram(filename, args);
			// Actually run the batch file. This is a no-op if it's
			// an executable.
			shell.RunBatchFile();
			DOS_FreeMemory(segment);
			WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOC"), kb);
		}
	} else {
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ERROR"), kb);
	}
}

void LOADFIX::AddMessages()
{
	MSG_Add("PROGRAM_LOADFIX_HELP_LONG",
	        "Load a program in the specific memory region and then run it.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]loadfix[reset] [color=white][-SIZE][reset] [color=light-cyan]GAME[reset] [color=white][PARAMETERS][reset]\n"
	        "  [color=light-green]loadfix[reset] [/d] (or [/f])[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]GAME[reset]        game or program to load, optionally with parameters\n"
	        "  [color=white]-SIZE[reset]       SIZE indicates the number of kilobytes to be allocated\n"
	        "  /d (or /f)  Frees the previously allocated memory.\n"
	        "\n"
	        "Notes:\n"
	        "  - The most common use case of this command is to fix games such as\n"
	        "    California Games II and Wing Commander 2 that show [color=white]\"Packed File Corrupt\"[reset]\n"
	        "    or [color=white]\"Not enough memory\"[reset] error messages.\n"
	        "  - Running [color=light-green]loadfix[reset] without an argument simply allocates memory for your game\n"
	        "    to run; you can free the memory with either /d or /f option when it\n"
	        "    finishes.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]loadfix[reset] [color=light-cyan]wc2[reset]\n"
	        "  [color=light-green]loadfix[reset] [color=white]-32[reset] [color=light-cyan]wc2[reset]\n"
	        "  [color=light-green]loadfix[reset] [color=white]-128[reset]\n"
	        "  [color=light-green]loadfix[reset] /d\n"
	        "\n");

	MSG_Add("PROGRAM_LOADFIX_ALLOC", "%d kB allocated.\n\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOC", "%d kB freed.\n\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOCALL", "Used memory freed.\n\n");
	MSG_Add("PROGRAM_LOADFIX_ERROR", "Memory allocation error.\n\n");
}
