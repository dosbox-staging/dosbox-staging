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
#include "program_more_output.h"
#include "string_utils.h"

void KEYB::Run(void) {
	auto log_keyboard_code = [&](const KeyboardErrorCode rcode, const std::string &layout, const int codepage) {
		switch (rcode) {
		case KEYB_NOERROR:
			WriteOut(MSG_Get("PROGRAM_KEYB_NOERROR"), layout.c_str(), dos.loaded_codepage);
			break;
		case KEYB_FILENOTFOUND:
			WriteOut(MSG_Get("PROGRAM_KEYB_FILENOTFOUND"), layout.c_str());
			break;
		case KEYB_INVALIDFILE: WriteOut(MSG_Get("PROGRAM_KEYB_INVALIDFILE"), layout.c_str()); break;
		case KEYB_LAYOUTNOTFOUND:
			WriteOut(MSG_Get("PROGRAM_KEYB_LAYOUTNOTFOUND"), layout.c_str(), codepage);
			break;
		case KEYB_INVALIDCPFILE:
			WriteOut(MSG_Get("PROGRAM_KEYB_INVCPFILE"), layout.c_str());
			break;
		default:
			LOG(LOG_DOSMISC, LOG_ERROR)
			("KEYB:Invalid returncode %x", static_cast<uint8_t>(rcode));
			break;
		}
	};

	// No arguments: print codepage info and possibly loaded layout ID
	if (!cmd->FindCommand(1, temp_line)) {
		const char *layout_name = DOS_GetLoadedLayout();
		if (!layout_name) {
			WriteOut(MSG_Get("PROGRAM_KEYB_INFO"),dos.loaded_codepage);
		} else {
			WriteOut(MSG_Get("PROGRAM_KEYB_INFO_LAYOUT"),dos.loaded_codepage,layout_name);
		}
		return;
	}

	// One argument: asked for help
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_KEYB_HELP_LONG"));
		output.Display();
		return;
	}

	// One argument: The language/country. We'll infer the codepage
	int32_t tried_cp        = -1; // default to auto
	KeyboardErrorCode rcode = KEYB_LAYOUTNOTFOUND;
	if (cmd->GetCount() == 1) {
		rcode = DOS_LoadKeyboardLayoutFromLanguage(temp_line.c_str());
	}

	// Two or more arguments: language/country and a specific codepage
	else if (std::string cp_string; cmd->FindCommand(2, cp_string)) {
		// second parameter is codepage number
		tried_cp = atoi(cp_string.c_str());

		// Possibly a third parameter, the codepage file
		std::string cp_filename = "auto";
		(void)cmd->FindCommand(3, cp_filename); // fallback to auto

		rcode = DOS_LoadKeyboardLayout(temp_line.c_str(),
		                               tried_cp,
		                               cp_filename.c_str());
	}

	// Switch if loading the layout succeeded
	if (rcode == KEYB_NOERROR) {
		rcode = DOS_SwitchKeyboardLayout(temp_line.c_str(), tried_cp);
	}

	log_keyboard_code(rcode, temp_line, tried_cp);
}

void KEYB::AddMessages() {
	MSG_Add("PROGRAM_KEYB_INFO","Codepage %i has been loaded\n");
	MSG_Add("PROGRAM_KEYB_INFO_LAYOUT","Codepage %i has been loaded for layout %s\n");
	MSG_Add("PROGRAM_KEYB_HELP_LONG",
	        "Configures a keyboard for a specific language.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]keyb[reset] [color=light-cyan][LANGCODE][reset]\n"
	        "  [color=light-green]keyb[reset] [color=light-cyan]LANGCODE[reset] [color=white]CODEPAGE[reset] [CODEPAGEFILE]\n"
	        "\n"
	        "Where:\n"
	        "  [color=light-cyan]LANGCODE[reset]     is a language code or keyboard layout ID.\n"
	        "  [color=white]CODEPAGE[reset]     is a code page number, such as [color=white]437[reset] and [color=white]850[reset].\n"
	        "  CODEPAGEFILE is a file containing information for a code page.\n"
	        "\n"
	        "Notes:\n"
	        "  Running [color=light-green]keyb[reset] without an argument shows the currently loaded keyboard layout\n"
	        "  and code page. It will change to [color=light-cyan]LANGCODE[reset] if provided, optionally with a\n"
	        "  [color=white]CODEPAGE[reset] and an additional CODEPAGEFILE to load the specified code page\n"
	        "  number and code page file if provided. This command is especially useful if\n"
	        "  you use a non-US keyboard, and [color=light-cyan]LANGCODE[reset] can also be set in the configuration\n"
	        "  file under the [dos] section using the \"keyboardlayout = [color=light-cyan]LANGCODE[reset]\" setting.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]KEYB[reset]\n"
	        "  [color=light-green]KEYB[reset] [color=light-cyan]uk[reset]\n"
	        "  [color=light-green]KEYB[reset] [color=light-cyan]sp[reset] [color=white]850[reset]\n"
	        "  [color=light-green]KEYB[reset] [color=light-cyan]de[reset] [color=white]858[reset] mycp.cpi\n");
	MSG_Add("PROGRAM_KEYB_NOERROR","Keyboard layout %s loaded for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_FILENOTFOUND","Keyboard file %s not found\n");
	MSG_Add("PROGRAM_KEYB_INVALIDFILE","Keyboard file %s invalid\n");
	MSG_Add("PROGRAM_KEYB_LAYOUTNOTFOUND","No layout in %s for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_INVCPFILE","None or invalid codepage file for layout %s\n");
}
