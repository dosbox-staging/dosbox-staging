/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2021  The DOSBox Staging Team
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
	if (cmd->FindExist("/l", false) || cmd->FindExist("/list", false)) {
		for (int x = 0; x < SERIAL_MAX_PORTS; x++)
			showPort(x);
		return;
	}

	// Select COM port type.
	if (cmd->GetCount() >= 2) {
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
			WriteOut(MSG_Get("PROGRAM_SERIAL_BAD_MODE"));
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
		// Recreate the port with the new mode.
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
	WriteOut(MSG_Get("PROGRAM_SERIAL_HELP"), SERIAL_MAX_PORTS);
}

void SERIAL_ProgramStart(Program **make)
{
	*make = new SERIAL;
}
