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

#include "setup.h"
#include "../hardware/serialport/directserial.h"
#include "../hardware/serialport/serialdummy.h"
#include "../hardware/serialport/softmodem.h"
#include "../hardware/serialport/nullmodem.h"
#include "program_serial.h"

static const char *serialTypes[SERIAL_TYPE_COUNT] = {"disabled", "dummy",
                                                     "directserial", "modem",
                                                     "nullmodem"};

void SERIAL::showPort(int port)
{
	if (serialports[port] != nullptr) {
		WriteOut(MSG_Get("PROGRAM_SERIAL_SHOW_PORT"), port + 1,
		         serialTypes[serialports[port]->serialType],
		         serialports[port]->commandLineString.c_str());
	} else {
		WriteOut(MSG_Get("PROGRAM_SERIAL_SHOW_PORT"), port + 1,
		         serialTypes[SERIAL_TYPE_DISABLED], "");
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

	// Select COM mode.
	if (cmd->GetCount() >= 2) {
		// Which COM did they want to change?
		int port = -1;
		cmd->FindCommand(1, temp_line);
		try {
			port = stoi(temp_line);
		} catch (...) {
		}
		if (port < 1 || port >= SERIAL_MAX_PORTS) {
			// Didn't understand the port number.
			WriteOut(MSG_Get("PROGRAM_SERIAL_BAD_PORT"), SERIAL_MAX_PORTS);
			return;
		}
		// Which mode do they want?
		int mode = -1;
		cmd->FindCommand(2, temp_line);
		for (int x = 0; x < SERIAL_TYPE_COUNT; x++) {
			if (!strcasecmp(temp_line.c_str(), serialTypes[x])) {
				mode = x;
				break;
			}
		}
		if (mode < 0) {
			// No idea what they asked for.
			WriteOut(MSG_Get("PROGRAM_SERIAL_BAD_MODE"));
			for (int x = 0; x < SERIAL_TYPE_COUNT; x++) {
				WriteOut(MSG_Get("PROGRAM_SERIAL_INDENTED_LIST"),
				         serialTypes[x]);
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
		delete serialports[port - 1];
		// Recreate the port with the new mode.
		switch (mode) {
		case SERIAL_TYPE_DISABLED:
			serialports[port - 1] = nullptr;
			break;
		case SERIAL_TYPE_DUMMY:
			serialports[port - 1] = new CSerialDummy(port - 1,
			                                         commandLine);
			break;
		case SERIAL_TYPE_DIRECT_SERIAL:
			serialports[port - 1] = new CDirectSerial(port - 1,
			                                          commandLine);
			break;
		case SERIAL_TYPE_MODEM:
			serialports[port - 1] = new CSerialModem(port - 1,
			                                         commandLine);
			break;
		case SERIAL_TYPE_NULL_MODEM:
			serialports[port - 1] = new CNullModem(port - 1, commandLine);
			break;
		}
		if (serialports[port - 1] != nullptr) {
			serialports[port - 1]->serialType = (SerialTypesE)mode;
			serialports[port - 1]->commandLineString = commandLineString;
		}
		delete commandLine;
		showPort(port - 1);
		return;
	}

	// Show help.
	WriteOut(MSG_Get("PROGRAM_SERIAL_HELP"));

	/*
	//bool  found              = false;
	//SocketTypesE socketType = SOCKET_TYPE_COUNT;  // Just need an invalid
	value here.

	// Select COM mode.
	if (cmd->GetCount() == 2) {
	        // Which COM did they want to change?
	        cmd->FindCommand(1, temp_line);
	        try {
	                port = stoi(temp_line);
	        } catch (...) {
	        }
	        if (port > 0 && port < SERIAL_MAX_PORTS) {
	                // Is this a null-modem port?
	                if (serialports[port-1] != nullptr &&
	serialports[port-1]->serialType == SERIAL_TYPE_NULL_MODEM) { CNullModem
	*nullModem = static_cast<CNullModem*>(serialports[port-1]);
	                        // What mode?
	                        cmd->FindCommand(2, temp_line);
	                        if (!strcasecmp("E", temp_line.c_str()) ||
	!strcasecmp("ENET", temp_line.c_str())) { socketType = SOCKET_TYPE_ENET;
	                        }
	                        if (!strcasecmp("T", temp_line.c_str()) ||
	!strcasecmp("TCP", temp_line.c_str())) { socketType = SOCKET_TYPE_TCP;
	                        }
	                        if (socketType != SOCKET_TYPE_COUNT) {  // Our
	invalid value.
	                                // Is this a different mode from what
	we're in now? if (nullModem->socketType != socketType) {
	                                        // Get a copy of the original
	command line. CommandLine *newCmd = new CommandLine("NULMODEM.COM",
	nullModem->commandLineString.c_str());
	                                        // Remove any enet setting it
	included. newCmd->FindExist("enet:1", true); newCmd->FindExist("enet:0",
	true);
	                                        // Create new command line
	string with the updated enet setting. std::string newCmdString;
	                                        newCmd->GetStringRemain(newCmdString);
	                                        newCmdString.append(" enet:");
	                                        newCmdString.append((socketType
	== SOCKET_TYPE_ENET) ? "1" : "0");
	                                        // Make a new CommandLine
	object. delete newCmd; newCmd = new CommandLine("NULMODEM.COM",
	newCmdString.c_str());
	                                        // Recreate the port with the
	new mode. delete serialports[port-1]; serialports[port-1] = new
	CNullModem (port-1, newCmd); serialports[port-1]->serialType =
	SERIAL_TYPE_NULL_MODEM; serialports[port-1]->commandLineString =
	newCmdString; WriteOut(MSG_Get("PROGRAM_NULMODEM_CHANGED"), port,
	(socketType == SOCKET_TYPE_ENET) ? "ENET" : "TCP"); delete newCmd; }
	else {
	                                        // Same mode - no change.
	                                        WriteOut(MSG_Get("PROGRAM_NULMODEM_UNCHANGED"),
	port, (socketType == SOCKET_TYPE_ENET) ? "ENET" : "TCP");
	                                }
	                        } else {
	                                // No idea what mode they asked for.
	                                WriteOut(MSG_Get("PROGRAM_NULMODEM_BAD_MODE"));
	                        }
	                } else {
	                        // Not a null-modem.
	                        WriteOut(MSG_Get("PROGRAM_NULMODEM_NOT_NULL_MODEM"),
	port);
	                }
	        } else {
	                // Didn't understand the port number.
	                WriteOut(MSG_Get("PROGRAM_NULMODEM_BAD_PORT"),
	SERIAL_MAX_PORTS);
	        }
	        showHelp = false;
	}
	*/
}

void SERIAL_ProgramStart(Program **make)
{
	*make = new SERIAL;
}
