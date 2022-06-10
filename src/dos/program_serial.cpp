/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2022  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "program_serial.h"

#include <map>

#include "../hardware/serialport/directserial.h"
#include "../hardware/serialport/serialdummy.h"
#include "../hardware/serialport/softmodem.h"
#include "../hardware/serialport/nullmodem.h"
#include "../hardware/serialport/serialmouse.h"

// Map the serial port type enums to printable names
static std::map<SERIAL_PORT_TYPE, const std::string> serial_type_names = {
        {SERIAL_PORT_TYPE::DISABLED, "disabled"},
        {SERIAL_PORT_TYPE::DUMMY, "dummy"},
#ifdef C_DIRECTSERIAL
        {SERIAL_PORT_TYPE::DIRECT_SERIAL, "directserial"},
#endif
#if C_MODEM
        {SERIAL_PORT_TYPE::MODEM, "modem"},
        {SERIAL_PORT_TYPE::NULL_MODEM, "nullmodem"},
#endif
        {SERIAL_PORT_TYPE::MOUSE,   "serialmouse"},
        {SERIAL_PORT_TYPE::INVALID, "invalid"},
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
		int port = -1;
		cmd->FindCommand(1, temp_line);
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
		// Which type of device do they want?
		SERIAL_PORT_TYPE desired_type = SERIAL_PORT_TYPE::INVALID;
		cmd->FindCommand(2, temp_line);
		for (const auto &[type, name] : serial_type_names) {
			if (temp_line == name) {
				desired_type = type;
				break;
			}
		}
		if (desired_type == SERIAL_PORT_TYPE::INVALID) {
			// No idea what they asked for.
			WriteOut(MSG_Get("PROGRAM_SERIAL_BAD_TYPE"));
			for (const auto &type_name : serial_type_names) {
				if (type_name.first == SERIAL_PORT_TYPE::DISABLED)
					continue; // Don't show the invalid placeholder.
				WriteOut(MSG_Get("PROGRAM_SERIAL_INDENTED_LIST"),
				         type_name.second.c_str());
			}
			return;
		}
		// Build command line, if any.
		int i = 3;
		std::string commandLineString = "";
		while (cmd->FindCommand(i++, temp_line)) {
			commandLineString.append(temp_line);
			commandLineString.append(" ");
		}
		CommandLine *commandLine = new CommandLine("SERIAL.COM",
		                                           commandLineString.c_str());
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
#ifdef C_DIRECTSERIAL
		case SERIAL_PORT_TYPE::DIRECT_SERIAL:
			serialports[port_index] = new CDirectSerial(port_index,
			                                          commandLine);
			break;
#endif
#if C_MODEM
		case SERIAL_PORT_TYPE::MODEM:
			serialports[port_index] = new CSerialModem(port_index,
			                                         commandLine);
			break;
		case SERIAL_PORT_TYPE::NULL_MODEM:
			serialports[port_index] = new CNullModem(port_index, commandLine);
			break;
#endif
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
			serialports[port_index]->commandLineString = commandLineString;
		}
		delete commandLine;
		showPort(port_index);
		return;
	}

	// Show help.
	WriteOut(MSG_Get("SHELL_CMD_SERIAL_HELP_LONG"), SERIAL_MAX_PORTS);
}

void SERIAL::AddMessages() {
	MSG_Add("SHELL_CMD_SERIAL_HELP_LONG",
	        "Manages the serial ports.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]serial[reset] [color=white][PORT#][reset]                List all or specified serial ports.\n"
	        "  [color=green]serial[reset] [color=white]PORT#[reset] [color=cyan]TYPE[reset] [settings]  Set the specified port to the given type.\n"
	        "\n"
	        "Where:\n"
	        "  [color=white]PORT#[reset] The port number: [color=white]1[reset], [color=white]2[reset], [color=white]3[reset], or [color=white]4[reset]\n"
	        "  [color=cyan]TYPE[reset]  The port type: [color=cyan]MODEM[reset], [color=cyan]NULLMODEM[reset], [color=cyan]DIRECTSERIAL[reset], [color=cyan]DUMMY[reset], or [color=cyan]DISABLED[reset]\n"
	        "\n"
	        "Notes:\n"
	        "  Optional settings for each [color=cyan]TYPE[reset]:\n"
	        "  For [color=cyan]MODEM[reset]        : IRQ, LISTENPORT, SOCK\n"
	        "  For [color=cyan]NULLMODEM[reset]    : IRQ, SERVER, RXDELAY, TXDELAY, TELNET,\n"
	        "                     USEDTR, TRANSPARENT, PORT, INHSOCKET, SOCK\n"
	        "  For [color=cyan]DIRECTSERIAL[reset] : IRQ, REALPORT (required), RXDELAY\n"
	        "  For [color=cyan]DUMMY[reset]        : IRQ\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]SERIAL[reset]                                       : List the current serial ports\n"
	        "  [color=green]SERIAL[reset] [color=white]1[reset] [color=cyan]NULLMODEM[reset] PORT:1250                 : Listen on TCP:1250 as server\n"
	        "  [color=green]SERIAL[reset] [color=white]2[reset] [color=cyan]NULLMODEM[reset] SERVER:10.0.0.6 PORT:1250 : Connect to TCP:1250 as client\n"
	        "  [color=green]SERIAL[reset] [color=white]3[reset] [color=cyan]MODEM[reset] LISTENPORT:5000 SOCK:1        : Listen on UDP:5000 as server\n"
	        "  [color=green]SERIAL[reset] [color=white]4[reset] [color=cyan]DIRECTSERIAL[reset] REALPORT:ttyUSB0       : Use a physical port on Linux\n");
	MSG_Add("PROGRAM_SERIAL_SHOW_PORT", "COM%d: %s %s\n");
	MSG_Add("PROGRAM_SERIAL_BAD_PORT",
	        "Must specify a numeric port value between 1 and %d, inclusive.\n");
	MSG_Add("PROGRAM_SERIAL_BAD_TYPE", "Type must be one of the following:\n");
	MSG_Add("PROGRAM_SERIAL_INDENTED_LIST", "  %s\n");
}
