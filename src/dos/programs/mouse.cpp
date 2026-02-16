// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mouse.h"

#include "cpu/callback.h"
#include "cpu/registers.h"
#include "dos/dos.h"
#include "dos/dos_windows.h"
#include "hardware/input/mouse.h"
#include "misc/notifications.h"
#include "more_output.h"
#include "utils/checks.h"

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

	// TODO: Find out what /Mn option of the Microsoft driver exactly does.

	// TODO: Implement missing mouse driver functions and the relevant
	// configuration switches: rotation angle (/Or, values 0-359), clickload
	// (/KC to enable, /K to disable), button mapping (/KPnSm, values 1-3),
	// cursor delay (/Nn, values 0-255), ballistic curve selection (/Pn,
	// values 1-4), double speed threshold setting (/Dn).

	RemoveUnsupportedOptions();

	// The quiet mode should not inhibit error messages - checked with
	// Microsoft Mouse Driver v9.01
	const bool has_option_quiet = cmd->FindExistRemoveAll("/q");

	// Microsoft option to load the driver, currently the only supported
	// action
	cmd->FindExistRemoveAll("on");

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

void MOUSE::RemoveUnsupportedOptions()
{
	// Due to the nature of DOSBox host mouse driver, these options are
	// probably never going to be supported

	// Mouse hardware port selection: probe (/f), PS/2 (/z), bus (/b),
	// inport (/i1, /i2), or serial (/c1 - /c4).
	// Not feasible to be implemented, we are a virtual (host) mouse driver.
	if (cmd->FindExistRemoveAll(
	            "/f", "/z", "/b", "/i1", "/i2", "/c1", "/c2", "/c3", "/c4")) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "DOS",
		                      "PROGRAM_MOUSE_PORT_SELECTION");
	}

	// Disable hardware mouse cursor on some cards, we are not emulating
	// anything like this.
	if (cmd->FindExistRemoveAll("/y")) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "DOS",
		                      "PROGRAM_MOUSE_HARDWARE_CURSOR");
	}

	// Language selection: German (/LD), Spanish (/LE), French (/LF),
	// Italian (/LI), Korean (/LK), Japanese (/LJ), Dutch (/LNL),
	// Portuguese (/LP), Swedish (/LS), Finnish (/LSF).
	// We offer a more flexible, system-wide translation support instead.
	if (cmd->FindExistRemoveAll(
	            "/LD", "/LE", "/LF", "/LI", "/LK", "/LJ",
	            "/LNL", "/LP", "/LS", "/LSF")) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "DOS",
		                      "PROGRAM_MOUSE_LANGUAGE");
	}

	// Switches below are not implemented, because our driver accepts wider
	// range of values; thus they are skipped, mainly to avoid confusion

	// Mouse sensitivity: vertical (/Vn), horizontal (/Hn), both (/Sn);
	// Microsoft mouse driver accepts values 5-100
	auto check_remove_numeric = [&](const std::string& begin) {

		std::string value = {};
		if (cmd->FindStringBegin(begin, value) &&
		    is_digits(value)) {

			// Found an argument with a numeric value
			constexpr bool Remove = true;
			cmd->FindStringBegin(begin, value, Remove);
			return true;
		}

		return false;
	};

	const bool found_vertical   = check_remove_numeric("/V");
	const bool found_horizontal = check_remove_numeric("/H");
	const bool found_both       = check_remove_numeric("/S");

	if (found_vertical || found_horizontal || found_both) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "DOS",
		                      "PROGRAM_MOUSE_SENSITIVITY");
	}

	// Mouse interrupt rate - /Rn when n is one of:
	// 1 (30Hz), 2 (50Hz), 3 (100Hz), or 4 (200Hz).
	if (cmd->FindExistRemoveAll("/R1", "/R2", "/R3", "/R4")) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "DOS",
		                      "PROGRAM_MOUSE_HINTERRUPT_RATE");
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
	        "Mouse port selection not supported, driver always uses the host mouse.");
	MSG_Add("PROGRAM_MOUSE_HARDWARE_CURSOR",
	        "Hardware mouse cursor not supported.");
	MSG_Add("PROGRAM_MOUSE_LANGUAGE",
	        "Mouse driver language selection not supported.\n"
	        "Use the '[color=light-green]config[reset]'"
	        " command to change the system language.");
	MSG_Add("PROGRAM_MOUSE_SENSITIVITY",
	        "Mouse sensitivity selection ignored.\n"
	        "Use the '[color=light-green]mousectl[reset]'"
	        " command to change the mouse sensitivity.");
	MSG_Add("PROGRAM_MOUSE_HINTERRUPT_RATE",
	        "Mouse interrupt rate selection ignored.\n"
	        "Use the '[color=light-green]mousectl[reset]'"
	        " command to change the interrupt rate.");
}
