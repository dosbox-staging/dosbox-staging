/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2025-2025  The DOSBox Staging Team
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

#include "program_mouse.h"

#include "checks.h"
#include "dos_inc.h"
#include "mouse.h"
#include "program_more_output.h"

CHECK_NARROWING();


void MOUSE::Run()
{
	// Handle command line
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_MOUSE_HELP_LONG"));
		output.Display();
		return;
	}

	// XXX check for running under Windows

	if (MOUSEDOS_IsDriverStarted()) {
		// XXX
		return;
	}

	// XXX check for any other mouse driver

	if (!MOUSEDOS_StartDriver()) {
		// XXX error message
	}
}

void MOUSE::AddMessages()
{
	MSG_Add("PROGRAM_MOUSE_HELP_LONG",
	        "Load the built-in mouse driver.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]mouse[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  None\n"
	        "\n"
	        "Notes:\n"
	        "  The \"builtin_dos_mouse_driver\" configuration setting can be used to add the\n"
	        "  driver loading command to the [color=light-cyan]AUTOEXEC.BAT[reset] file.\n");

}
