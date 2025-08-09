// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "program_mouse.h"

#include "callback.h"
#include "util/checks.h"
#include "dos_inc.h"
#include "dos_windows.h"
#include "mouse.h"
#include "program_more_output.h"
#include "regs.h"

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

	// The quiet mode should not inhibit error messages - checked with
	// Microsoft Mouse Driver v9.01
	const bool has_option_quiet = cmd->FindExistRemoveAll("/q");

	// Microsoft option to load the driver, currently the only supported
	// action
	cmd->FindExistRemoveAll("on");

	// Check if user requested to proble for mouse port
	bool warn_port_selection = cmd->FindExistRemoveAll("/f");
	// Check if user requested PS/2 (/z), bus (/b), inport (/i1, /i2), or
	// serial (/c1 - /c4) mouse port
	for (const auto option : { "/z", "/b", "/i1", "/i2",
	                           "/c1", "/c2", "/c3", "/c4" }) {
		if (cmd->FindExistRemoveAll(option)) {
			warn_port_selection = true;
		}		
	}
	if (warn_port_selection) {
		WriteOut(MSG_Get("PROGRAM_MOUSE_PORT_SELECTION"));	
	}

	// This option disables hardware mouse cursor on some cards
	if (cmd->FindExistRemoveAll("/y")) {
		WriteOut(MSG_Get("PROGRAM_MOUSE_HARDWARE_CURSOR"));
	}

	const bool has_option_low_memory = cmd->FindExistRemoveAll("/e");

	// Check for unsupported/errorneous arguments
	const auto params = cmd->GetArguments();
	if (!params.empty()) {
		WriteOut(MSG_Get("SHELL_SYNTAX_ERROR"));
		return;
	}

	// Check if Windows is running
	if (WINDOWS_IsStarted()) {
		WriteOut(MSG_Get("SHELL_CANT_RUN_UNDER_WINDOWS"));
		return;
	}

	// Check if our simulated mouse driver is started
	if (MOUSEDOS_IsDriverStarted()) {
		WriteOut(MSG_Get("PROGRAM_MOUSE_ALREADY_INSTALLED"));
		return;
	}

	// Check if 3rd party mouse driver is started
	reg_ax = 0x00;
	CALLBACK_RunRealInt(0x33);
	if (reg_ax == 0xFFFF) {
		WriteOut(MSG_Get("PROGRAM_MOUSE_ALREADY_INSTALLED"));
		return;
	}

	// Try to start the driver
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
	        "  on    load driver (default action)\n"
	        "  /e    load driver into low (conventional) memory\n"
	        "  /q    quiet mode (skip confirmation messages)\n"
	        "\n"
	        "Notes:\n"
	        "  The built-in driver bypasses the PS/2 and serial (COM) ports and communicates\n"
	        "  with the mouse directly. This results in lower input lag, smoother movement,\n"
	        "  and increased mouse responsiveness.\n");

	MSG_Add("PROGRAM_MOUSE_INSTALLED", "Mouse driver installed.\n");

	MSG_Add("PROGRAM_MOUSE_ALREADY_INSTALLED",
	        "Mouse driver is already installed.\n");
	MSG_Add("PROGRAM_MOUSE_COULD_NOT_INSTALL",
	        "Could not install the mouse driver.\n");

	MSG_Add("PROGRAM_MOUSE_PORT_SELECTION",
	        "Port selection not supported, driver always uses the host mouse.\n");
	MSG_Add("PROGRAM_MOUSE_HARDWARE_CURSOR",
	        "Hardware mouse cursor not supported.\n");
}
