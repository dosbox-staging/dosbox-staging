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

#include "program_rescan.h"

#include "string_utils.h"

void RESCAN::Run(void)
{
	bool all = false;

	uint8_t drive = DOS_GetDefaultDrive();

	if (HelpRequested()) {
		WriteOut(MSG_Get("SHELL_CMD_RESCAN_HELP_LONG"));
		return;
	}

	if (cmd->FindCommand(1,temp_line)) {
		//-A -All /A /All
		if (temp_line.size() >= 2 &&
		    (temp_line[0] == '-' || temp_line[0] == '/') &&
		    (temp_line[1] == 'a' || temp_line[1] == 'A')) {
			all = true;
		} else if (temp_line.size() == 2 && temp_line[1] == ':') {
			lowcase(temp_line);
			drive  = temp_line[0] - 'a';
		}
	}
	// Get current drive
	if (all) {
		for (Bitu i =0; i<DOS_DRIVES; i++) {
			if (Drives[i]) Drives[i]->EmptyCache();
		}
		WriteOut(MSG_Get("PROGRAM_RESCAN_SUCCESS"));
	} else {
		if (drive < DOS_DRIVES && Drives[drive]) {
			Drives[drive]->EmptyCache();
			WriteOut(MSG_Get("PROGRAM_RESCAN_SUCCESS"));
		}
	}
}

void RESCAN::AddMessages() {
	MSG_Add("SHELL_CMD_RESCAN_HELP_LONG",
	        "Scans for changes on mounted DOS drives.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]rescan[reset] [color=cyan]DRIVE[reset]\n"
	        "  [color=green]rescan[reset] [/a]\n"
	        "\n"
	        "Where:\n"
	        "  [color=cyan]DRIVE[reset] is the drive to scan for changes.\n"
	        "\n"
	        "Notes:\n"
	        "  - Running [color=green]rescan[reset] without an argument scans for changes of the current drive.\n"
	        "  - Changes to this drive made on the host will then be reflected inside DOS.\n"
	        "  - You can also scan for changes on all mounted drives with the /a option.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]rescan[reset] [color=cyan]c:[reset]\n"
	        "  [color=green]rescan[reset] /a\n");
	MSG_Add("PROGRAM_RESCAN_SUCCESS","Drive re-scanned.\n");
}
