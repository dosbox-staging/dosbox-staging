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

#ifndef DOSBOX_DIRECTSERIAL_WIN32_H
#define DOSBOX_DIRECTSERIAL_WIN32_H

#include "dosbox.h"

#if C_DIRECTSERIAL

#define DIRECTSERIAL_AVAILIBLE
#include "serialport.h"

#include "libserial.h"

class CDirectSerial final : public CSerial {
public:
	CDirectSerial(const CDirectSerial &) = delete; // prevent copying
	CDirectSerial &operator=(const CDirectSerial &) = delete; // prevent
	                                                          // assignment

	CDirectSerial(const uint8_t port_idx, CommandLine *cmd);
	~CDirectSerial();

	void updatePortConfig(uint16_t divider, uint8_t lcr);
	void updateMSR();
	void transmitByte(uint8_t val, bool first);
	void setBreak(bool value);
	
	void setRTSDTR(bool rts, bool dtr);
	void setRTS(bool val);
	void setDTR(bool val);
	void handleUpperEvent(uint16_t type);

private:
	COMPORT comport = nullptr;

	uint32_t rx_state = 0;
#define D_RX_IDLE		0
#define D_RX_WAIT		1
#define D_RX_BLOCKED	2
#define D_RX_FASTWAIT	3

	uint32_t rx_retry = 0;     // counter of retries (every millisecond)
	uint32_t rx_retry_max = 0; // how many POLL_EVENTS to wait before
	                           // causing an overrun error.
	bool doReceive();

#if SERIAL_DEBUG
	bool dbgmsg_poll_block = false;
	bool dbgmsg_rx_block = false;
#endif
};

#endif	// C_DIRECTSERIAL

#endif
