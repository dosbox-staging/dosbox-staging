/*
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


// include guard
#ifndef DOSBOX_NULLMODEM_WIN32_H
#define DOSBOX_NULLMODEM_WIN32_H

#include "dosbox.h"

#if C_MODEM

#include "misc_util.h"
#include "serialport.h"

#define SERIAL_SERVER_POLLING_EVENT	SERIAL_BASE_EVENT_COUNT+1
#define SERIAL_TX_REDUCTION		SERIAL_BASE_EVENT_COUNT+2
#define SERIAL_NULLMODEM_DTR_EVENT	SERIAL_BASE_EVENT_COUNT+3
#define SERIAL_NULLMODEM_EVENT_COUNT	SERIAL_BASE_EVENT_COUNT+3

class CNullModem final : public CSerial {
public:
	CNullModem(const CNullModem &) = delete;            // prevent copying
	CNullModem &operator=(const CNullModem &) = delete; // prevent assignment

	CNullModem(const uint8_t port_idx, CommandLine *cmd);
	~CNullModem();

	void updatePortConfig(uint16_t divider, uint8_t lcr);
	void updateMSR();
	void transmitByte(uint8_t val, bool first);
	void setBreak(bool value);
	
	void setRTSDTR(bool rts, bool dtr);
	void setRTS(bool val);
	void setDTR(bool val);
	void handleUpperEvent(uint16_t type);

private:
	TCPServerSocket *serversocket = nullptr;
	TCPClientSocket *clientsocket = nullptr;

	uint16_t serverport = 0; // we are a server if this is nonzero
	uint16_t clientport = 0;

	uint8_t hostnamebuffer[128] = {0}; // the name passed to us by the user

	uint32_t rx_state = 0;
#define N_RX_IDLE		0
#define N_RX_WAIT		1
#define N_RX_BLOCKED	2
#define N_RX_FASTWAIT	3
#define N_RX_DISC		4

	bool doReceive();
	bool ClientConnect(TCPClientSocket* newsocket);
	bool ServerListen();
	bool ServerConnect();
    void Disconnect();
    SocketState readChar(uint8_t &val);
    void WriteChar(uint8_t data);

	bool DTR_delta = false; // with dtrrespect, we try to establish a
	                        // connection whenever DTR switches to 1. This
	                        // variable is used to remember the old state.

	bool tx_block = false; // true while the SERIAL_TX_REDUCTION event
	                       // is pending

	uint32_t rx_retry = 0; // counter of retries

	uint32_t rx_retry_max = 0; // how many POLL_EVENTS to wait before
	                           // causing a overrun error.

	uint32_t tx_gather = 0; // how long to gather tx data before
	                        // sending all of them [milliseconds]

	bool dtrrespect = false; // dtr behavior - only send data to the serial
	                         // port when DTR is on

	bool transparent = false; // if true, don't send 0xff 0xXX to toggle
	                          // DSR/CTS.

	bool telnet = false; // Do Telnet parsing.

    // Telnet's brain
#define TEL_CLIENT 0
#define TEL_SERVER 1

	SocketState TelnetEmulation(const uint8_t data);

	// Telnet's memory
	struct {
		bool binary[2] = {false};
		bool echo[2] = {false};
		bool supressGA[2] = {false};
		bool timingMark[2] = {false};

		bool inIAC = false;
		bool recCommand = false;
		uint8_t command = 0;
	} telClient;
};

#endif	// C_MODEM
#endif	// include guard
