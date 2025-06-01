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
#include "dos_windows.h"
#include "mouse.h"
#include "program_more_output.h"

CHECK_NARROWING();


void MOUSE::Run()
{
	// Parse command line
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_MOUSE_HELP_LONG"));
		output.Display();
		return;
	}

	// TODO: Add option to control cursor visibility in Windows 3.1x
	// windowed MS-DOS prompt (to override what Windows tells us to do).

	// TODO: Add support for mouse driver unloading: 'off' (as in Microsoft
	// driver) or /U switch (drivers from DR-DOS, Mouse Systems, CtMouse).

	// TODO: Implement options to change mouse sensitivity: /Vn (vertical),
	// /Hn (horizontal), /Sn (both), Microsoft accepts values 5-100.

	// TODO: Implement option /Rn to change interrupt rate; n can be
	// 1 (30Hz), 2 (50Hz), 3 (100Hz), or 4 (200Hz).

	// TODO: Find out what /Mn option of the Microsoft driver exactly does.

	// TODO: Implement missing mouse driver functions and the relevant
	// configuration switches: rotation angle (/Or, values 0-359), clickload
	// (/KC to enable, /K to disable), button mapping (/KPnSm, values 1-3),
	// cursor delay (/Nn, values 0-255), ballistic curve selection (/Pn,
	// values 1-4), double speed threshold setting (/Dn).

	// TODO: Implementing original options to change the driver language
	// value: /LD (German), /LE (Spanish), /LF (French), /LI (Italian),
	// /LK (Korean), /LJ (Japanese), /LNL (Dutch), /LP (Portuguese),
	// /LS (Swedish), /LSF (Finnish)

	// XXX add cmd->FindExistsRemoveAll()
	auto check_option = [&](const std::string& option) {
		constexpr bool RemoveIfFound = true;

		bool option_exists = false;
		while (cmd->FindExist(option, RemoveIfFound)) {
			option_exists = true;
		}
		return option_exists;
	};

	// XXX should quiet output also inhibit error messages?
	const bool has_option_quiet = check_option("/q");

	// Microsoft driver option to load the driver, currently the only
	// supported action
	check_option("on");

	// Check if user requested for a specific mouse - probe ports (/f),
	// PS/2 (/z), bus (/b), inport (/i1, /i2), serial (/c1 - /c4)
	if (check_option("/f") | check_option("/z") | check_option("/b") |
	    check_option("/i1") | check_option("/i2") |
	    check_option("/c1") | check_option("/c2") |
	    check_option("/c3") | check_option("/c4")) {
	    	WriteOut(MSG_Get("PROGRAM_MOUSE_PORT_SELECTION"));
	}

	// This option disables hardware mouse cursor on some cards
	if (check_option("/y")) {
	    	WriteOut(MSG_Get("PROGRAM_MOUSE_HARDWARE_CURSOR"));		
	}

	const bool has_option_low_memory = check_option("/e");

	// XXX check for illegal/unsupported options

	if (WINDOWS_IsStarted()) {
		WriteOut(MSG_Get("SHELL_CANT_RUN_UNDER_WINDOWS"));
		return;
	}

	// XXX also check if int33 API is responding
	if (MOUSEDOS_IsDriverStarted()) {
		WriteOut(MSG_Get("PROGRAM_MOUSE_ALREADY_INSTALLED"));
		return;
	}

	if (!MOUSEDOS_StartDriver(has_option_low_memory)) {
		WriteOut(MSG_Get("PROGRAM_MOUSE_COULD_NOT_INSTALL"));
		return;
	}

	if (!has_option_quiet) {
		WriteOut(MSG_Get("PROGRAM_MOUSE_INSTALLED"));
	}
}

void MOUSE::AddMessages()
{
	MSG_Add("PROGRAM_MOUSE_HELP_LONG",
	        "Load the built-in mouse driver.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]mouse[reset] [on] [/e] [/q]\n"
	        "\n"
	        "Parameters:\n"
	        "  on    load the driver (default action)\n"
	        "  /e    load driver into low (conventional) memory\n"
	        "  /q    quiet, skip confirmation messages\n"
	        "\n"
	        "Notes:\n"
	        "  The \"builtin_dos_mouse_driver\" configuration setting can be used to add the\n"
	        "  driver loading command to the [color=light-cyan]AUTOEXEC.BAT[reset] file.\n");

	MSG_Add("PROGRAM_MOUSE_INSTALLED", "Mouse driver installed.\n");

	MSG_Add("PROGRAM_MOUSE_ALREADY_INSTALLED",
		"Mouse driver is already installed.\n");
	MSG_Add("PROGRAM_MOUSE_COULD_NOT_INSTALL",
	        "Could not install the mouse driver.\n");

	MSG_Add("PROGRAM_MOUSE_PORT_SELECTION",
	        "Port selection not supported, driver always uses the host mouse.\n");
	MSG_Add("PROGRAM_MOUSE_HARDWARE_CURSOR",
	        "Hardware cursor not supported.\n");
}
