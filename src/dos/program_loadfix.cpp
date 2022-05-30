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
#include "shell.h"
#include "string_utils.h"

void LOADFIX::Run(void)
{
	if (HelpRequested()) {
		WriteOut(MSG_Get("SHELL_CMD_LOADFIX_HELP_LONG"));
		return;
	}
	uint16_t commandNr = 1;
	uint16_t kb = 64;
	if (cmd->FindCommand(commandNr, temp_line)) {
		if (temp_line[0] == '-' || temp_line[0] == '/') {
			const auto ch = std::toupper(temp_line[1]);
			if ((ch == 'D') || (ch == 'F')) {
				// Deallocate all
				DOS_FreeProcessMemory(0x40);
				WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOCALL"),kb);
				return;
			} else {
				// Set mem amount to allocate
				kb = atoi(temp_line.c_str()+1);
				if (kb==0) kb=64;
				commandNr++;
			}
		}
	}
	// Allocate Memory
	uint16_t segment;
	uint16_t blocks = kb*1024/16;
	if (DOS_AllocateMemory(&segment,&blocks)) {
		DOS_MCB mcb((uint16_t)(segment-1));
		mcb.SetPSPSeg(0x40);			// use fake segment
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ALLOC"),kb);
		// Prepare commandline...
		if (cmd->FindCommand(commandNr++,temp_line)) {
			// get Filename
			char filename[128];
			safe_strcpy(filename, temp_line.c_str());
			// Setup commandline
			char args[256+1];
			args[0] = 0;
			bool found = cmd->FindCommand(commandNr++,temp_line);
			while (found) {
				if (safe_strlen(args)+temp_line.length()+1>256) break;
				safe_strcat(args, temp_line.c_str());
				found = cmd->FindCommand(commandNr++,temp_line);
				if (found)
					safe_strcat(args, " ");
			}
			// Use shell to start program
			DOS_Shell shell;
			shell.Execute(filename,args);
			DOS_FreeMemory(segment);
			WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOC"),kb);
		}
	} else {
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ERROR"),kb);
	}
}

void LOADFIX::AddMessages() {
	MSG_Add("SHELL_CMD_LOADFIX_HELP_LONG",
	        "Loads a program in the specific memory region and then runs it.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]loadfix[reset] [color=cyan]GAME[reset] [color=white][PARAMETERS][reset]\n"
	        "  [color=green]loadfix[reset] [/d] (or [/f])[reset]\n"
	        "\n"
	        "Where:\n"
	        "  [color=cyan]GAME[reset] is a game or program to be loaded, optionally with parameters.\n"
	        "\n"
	        "Notes:\n"
	        "  The most common use cases of this command are to fix DOS games or programs\n"
	        "  which show either the \"[color=white]Packed File Corrupt[reset]\" or \"[color=white]Not enough memory\"[reset] (e.g.,\n"
	        "  from some 1980's games such as California Games II) error message when run.\n"
	        "  Running [color=green]loadfix[reset] without an argument simply allocates memory for your game\n"
	        "  to run; you can free the memory with either /d or /f option when it finishes.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]loadfix[reset] [color=cyan]mygame[reset] [color=white]args[reset]\n"
	        "  [color=green]loadfix[reset] /d\n");
	MSG_Add("PROGRAM_LOADFIX_ALLOC", "%d kB allocated.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOC", "%d kB freed.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOCALL","Used memory freed.\n");
	MSG_Add("PROGRAM_LOADFIX_ERROR","Memory allocation error.\n");
}
