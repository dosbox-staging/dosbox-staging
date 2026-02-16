// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "serial.h"

#include <map>

#include "hardware/serialport/nullmodem.h"
#include "hardware/serialport/serialdummy.h"
#include "hardware/serialport/serialmouse.h"
#include "hardware/serialport/softmodem.h"

#include "shell/command_line.h"
#include "more_output.h"

// Map the serial port type enums to printable names
static std::map<SERIAL_PORT_TYPE, const std::string> serial_type_names = {
        {  SERIAL_PORT_TYPE::DISABLED,  "disabled"},
        {     SERIAL_PORT_TYPE::DUMMY,     "dummy"},
        {     SERIAL_PORT_TYPE::MODEM,     "modem"},
        {SERIAL_PORT_TYPE::NULL_MODEM, "nullmodem"},
        {     SERIAL_PORT_TYPE::MOUSE,     "mouse"},
        {   SERIAL_PORT_TYPE::INVALID,   "invalid"},
};

void SERIAL::showPort(int port)
{
	if (serialports[port] != nullptr) {
		WriteOut(MSG_Get("PROGRAM_SERIAL_SHOW_PORT"), port + 1,
		         serial_type_names[serialports[port]->serialType].c_str(),
		         serialports[port]->commandLineString.c_str());
	} else {
		WriteOut(MSG_Get("PROGRAM_SERIAL_SHOW_PORT"), port + 1,
		         serial_type_names[SERIAL_PORT_TYPE::DISABLED].c_str(), "");
	}
}

void SERIAL::Run()
{
	// Show current serial configurations.
	if (!cmd->GetCount()) {
		for (int x = 0; x < SERIAL_MAX_PORTS; x++)
			showPort(x);
		return;
	}

	// Select COM port type.
	if (cmd->GetCount() >= 1 && !HelpRequested()) {
		// Which COM did they want to change?
		if (!cmd->FindCommand(1, temp_line)) {
			// Port number not provided or invalid type
			WriteOut(MSG_Get("PROGRAM_SERIAL_BAD_PORT"), SERIAL_MAX_PORTS);
			return;
		}
		// A port value was provided, can it be converted to an integer?
		int port = -1;
		try {
			port = stoi(temp_line);
		} catch (...) {
		}
		if (port < 1 || port > SERIAL_MAX_PORTS) {
			// Didn't understand the port number.
			WriteOut(MSG_Get("PROGRAM_SERIAL_BAD_PORT"), SERIAL_MAX_PORTS);
			return;
		}
		const auto port_index = port - 1;
		assert(port_index >= 0 && port_index < SERIAL_MAX_PORTS);
		if (cmd->GetCount() == 1) {
			showPort(port_index);
			return;
		}

		// Helper to print a nice list of the supported port types
		auto write_msg_for_invalid_port_type = [this]() {
			WriteOut(MSG_Get("PROGRAM_SERIAL_BAD_TYPE"));
			for (const auto& [port_type, port_type_str] : serial_type_names) {
				// Skip the invalid type; show only valid types
				if (port_type != SERIAL_PORT_TYPE::INVALID) {
					WriteOut(MSG_Get("PROGRAM_SERIAL_INDENTED_LIST"),
					         port_type_str.c_str());
				}
			}
		};

		// If we're here, then SERIAL.COM was given more than one
		// argument and the second argument must be the port type.
		constexpr auto port_type_arg_pos = 2; // (indexed starting at 1)
		assert(cmd->GetCount() >= port_type_arg_pos);

		// Which port type do they want?
		temp_line.clear();
		if (!cmd->FindCommand(port_type_arg_pos, temp_line)) {
			// Encountered a problem parsing the port type
			write_msg_for_invalid_port_type();
			return;
		}
		// They entered something, but do we have a matching type?
		auto desired_type = SERIAL_PORT_TYPE::INVALID;
		for (const auto& [type, name] : serial_type_names) {
			if (iequals(temp_line, name)) {
				desired_type = type;
				break;
			}
		}
		if (desired_type == SERIAL_PORT_TYPE::INVALID) {
			// They entered a port type; but it was invalid
			write_msg_for_invalid_port_type();
			return;
		}
		// Build command line, if any.
		int i = 3;
		std::string commandLineString = "";
		while (cmd->FindCommand(i++, temp_line)) {
			commandLineString.append(temp_line);
			commandLineString.append(" ");
		}
		auto commandLine = new CommandLine("SERIAL.COM", commandLineString);
		// Remove existing port.
		delete serialports[port_index];
		// Recreate the port with the new type.
		switch (desired_type) {
		case SERIAL_PORT_TYPE::INVALID:
		case SERIAL_PORT_TYPE::DISABLED:
			serialports[port_index] = nullptr;
			break;
		case SERIAL_PORT_TYPE::DUMMY:
			serialports[port_index] = new CSerialDummy(port_index,
			                                           commandLine);
			break;
		case SERIAL_PORT_TYPE::MODEM:
			serialports[port_index] = new CSerialModem(port_index,
			                                           commandLine);
			break;
		case SERIAL_PORT_TYPE::NULL_MODEM:
			serialports[port_index] = new CNullModem(port_index, commandLine);
			break;
		case SERIAL_PORT_TYPE::MOUSE:
			serialports[port_index] = new CSerialMouse(port_index,
			                                           commandLine);
			break;
		default:
			serialports[port_index] = nullptr;
			LOG_WARNING("SERIAL: Unknown serial port type %d", desired_type);
			break;
		}
		if (serialports[port_index] != nullptr) {
			serialports[port_index]->serialType = desired_type;
			serialports[port_index]->commandLineString = std::move(
			        commandLineString);
		}
		delete commandLine;
		showPort(port_index);
		return;
	}

	// Show help.
	MoreOutputStrings output(*this);
	output.AddString(MSG_Get("PROGRAM_SERIAL_HELP_LONG"));
	output.Display();
}

void SERIAL::AddMessages() {
	MSG_Add("PROGRAM_SERIAL_HELP_LONG",
	        "Manage the serial ports.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]serial[reset] [color=white][PORT#][reset]                   List all or specified ([color=white]1[reset], [color=white]2[reset], [color=white]3[reset], or [color=white]4[reset]) ports.\n"
	        "  [color=light-green]serial[reset] [color=white]PORT#[reset] [color=light-cyan]DEVICE[reset] [settings]   Attach specified device to the given port.\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]DEVICE[reset]  one of: [color=light-cyan]MODEM[reset], [color=light-cyan]NULLMODEM[reset], [color=light-cyan]MOUSE[reset], [color=light-cyan]DIRECT[reset], [color=light-cyan]DUMMY[reset], or [color=light-cyan]DISABLED[reset]\n"
	        "\n"
	        "  Optional settings for each [color=light-cyan]DEVICE[reset]:\n"
	        "  For [color=light-cyan]MODEM[reset]     : IRQ, LISTENPORT, SOCK\n"
	        "  For [color=light-cyan]NULLMODEM[reset] : IRQ, SERVER, RXDELAY, TXDELAY, TELNET, USEDTR, TRANSPARENT,\n"
	        "                  PORT, INHSOCKET, SOCK\n"
	        "  For [color=light-cyan]MOUSE[reset]     : IRQ, MODEL (2BUTTON, 3BUTTON, WHEEL, MSM, 2BUTTON+MSM,\n"
	        "                  3BUTTON+MSM, or WHEEL+MSM)\n"
	        "  For [color=light-cyan]DIRECT[reset]    : IRQ, REALPORT (required), RXDELAY\n"
	        "  For [color=light-cyan]DUMMY[reset]     : IRQ\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]SERIAL[reset] [color=white]1[reset] [color=light-cyan]NULLMODEM[reset] PORT:1250                 : Listen on TCP:1250 as server\n"
	        "  [color=light-green]SERIAL[reset] [color=white]2[reset] [color=light-cyan]NULLMODEM[reset] SERVER:10.0.0.6 PORT:1250 : Connect to TCP:1250 as client\n"
	        "  [color=light-green]SERIAL[reset] [color=white]3[reset] [color=light-cyan]MODEM[reset] LISTENPORT:5000 SOCK:1        : Listen on UDP:5000 as server\n"
	        "  [color=light-green]SERIAL[reset] [color=white]4[reset] [color=light-cyan]DIRECT[reset] REALPORT:ttyUSB0             : Use a physical port on Linux\n"
	        "  [color=light-green]SERIAL[reset] [color=white]1[reset] [color=light-cyan]MOUSE[reset] MODEL:MSM                     : Mouse Systems mouse\n");
	MSG_Add("PROGRAM_SERIAL_SHOW_PORT", "COM%d: %s %s\n");
	MSG_Add("PROGRAM_SERIAL_BAD_PORT",
	        "Must specify a numeric port value between 1 and %d, inclusive.\n");
	MSG_Add("PROGRAM_SERIAL_BAD_TYPE", "Type must be one of the following:\n");
	MSG_Add("PROGRAM_SERIAL_INDENTED_LIST", "  %s\n");
}
