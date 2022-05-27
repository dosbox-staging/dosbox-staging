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

#include "program_keyb.h"

#include "dos_keyboard_layout.h"
#include "string_utils.h"

void KEYB::Run(void) {
	if (cmd->FindCommand(1,temp_line)) {
		if (cmd->FindExist("/?", false) || cmd->FindExist("-?", false) ||
		    cmd->FindString("?", temp_line, false)) {
			WriteOut(MSG_Get("SHELL_CMD_KEYB_HELP_LONG"));
		} else {
			/* first parameter is layout ID */
			Bitu keyb_error=0;
			std::string cp_string;
			int32_t tried_cp = -1;
			if (cmd->FindCommand(2,cp_string)) {
				/* second parameter is codepage number */
				tried_cp=atoi(cp_string.c_str());
				char cp_file_name[256];
				if (cmd->FindCommand(3,cp_string)) {
					/* third parameter is codepage file */
					safe_strcpy(cp_file_name, cp_string.c_str());
				} else {
					/* no codepage file specified, use automatic selection */
					safe_strcpy(cp_file_name, "auto");
				}

				keyb_error=DOS_LoadKeyboardLayout(temp_line.c_str(), tried_cp, cp_file_name);
			} else {
				keyb_error=DOS_SwitchKeyboardLayout(temp_line.c_str(), tried_cp);
			}
			switch (keyb_error) {
				case KEYB_NOERROR:
					WriteOut(MSG_Get("PROGRAM_KEYB_NOERROR"),temp_line.c_str(),dos.loaded_codepage);
					break;
				case KEYB_FILENOTFOUND:
					WriteOut(MSG_Get("PROGRAM_KEYB_FILENOTFOUND"),temp_line.c_str());
					WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
					break;
				case KEYB_INVALIDFILE:
					WriteOut(MSG_Get("PROGRAM_KEYB_INVALIDFILE"),temp_line.c_str());
					break;
				case KEYB_LAYOUTNOTFOUND:
					WriteOut(MSG_Get("PROGRAM_KEYB_LAYOUTNOTFOUND"),temp_line.c_str(),tried_cp);
					break;
				case KEYB_INVALIDCPFILE:
					WriteOut(MSG_Get("PROGRAM_KEYB_INVCPFILE"),temp_line.c_str());
					WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
					break;
				default:
				        LOG(LOG_DOSMISC, LOG_ERROR)("KEYB:Invalid returncode %x",
				         static_cast<uint32_t>(keyb_error));
				        break;
			}
		}
	} else {
		/* no parameter in the command line, just output codepage info and possibly loaded layout ID */
		const char* layout_name = DOS_GetLoadedLayout();
		if (layout_name==NULL) {
			WriteOut(MSG_Get("PROGRAM_KEYB_INFO"),dos.loaded_codepage);
		} else {
			WriteOut(MSG_Get("PROGRAM_KEYB_INFO_LAYOUT"),dos.loaded_codepage,layout_name);
		}
	}
}

void KEYB::AddMessages() {
	MSG_Add("PROGRAM_KEYB_INFO","Codepage %i has been loaded\n");
	MSG_Add("PROGRAM_KEYB_INFO_LAYOUT","Codepage %i has been loaded for layout %s\n");
	MSG_Add("SHELL_CMD_KEYB_HELP_LONG",
	        "Configures a keyboard for a specific language.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]keyb[reset] [color=cyan][LANGCODE][reset]\n"
	        "  [color=green]keyb[reset] [color=cyan]LANGCODE[reset] [color=white]CODEPAGE[reset] [CODEPAGEFILE]\n"
	        "\n"
	        "Where:\n"
	        "  [color=cyan]LANGCODE[reset]     is a language code or keyboard layout ID.\n"
	        "  [color=white]CODEPAGE[reset]     is a code page number, such as [color=white]437[reset] and [color=white]850[reset].\n"
	        "  CODEPAGEFILE is a file containing information for a code page.\n"
	        "\n"
	        "Notes:\n"
	        "  Running [color=green]keyb[reset] without an argument shows the currently loaded keyboard layout\n"
	        "  and code page. It will change to [color=cyan]LANGCODE[reset] if provided, optionally with a\n"
	        "  [color=white]CODEPAGE[reset] and an additional CODEPAGEFILE to load the specified code page\n"
	        "  number and code page file if provided. This command is especially useful if\n"
	        "  you use a non-US keyboard, and [color=cyan]LANGCODE[reset] can also be set in the configuration\n"
	        "  file under the [dos] section using the \"keyboardlayout = [color=cyan]LANGCODE[reset]\" setting.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]KEYB[reset]\n"
	        "  [color=green]KEYB[reset] [color=cyan]uk[reset]\n"
	        "  [color=green]KEYB[reset] [color=cyan]sp[reset] [color=white]850[reset]\n"
	        "  [color=green]KEYB[reset] [color=cyan]de[reset] [color=white]858[reset] mycp.cpi\n");
	MSG_Add("PROGRAM_KEYB_NOERROR","Keyboard layout %s loaded for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_FILENOTFOUND","Keyboard file %s not found\n\n");
	MSG_Add("PROGRAM_KEYB_INVALIDFILE","Keyboard file %s invalid\n");
	MSG_Add("PROGRAM_KEYB_LAYOUTNOTFOUND","No layout in %s for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_INVCPFILE","None or invalid codepage file for layout %s\n\n");
}
