/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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


#ifndef DOSBOX_SERIALMODEM_H
#define DOSBOX_SERIALMODEM_H

#include <vector>
#include <memory>

#include "dosbox.h"
#if C_MODEM
#include "serialport.h"
#include "misc_util.h"

#define MODEMSPD 57600
#define SREGS 100

//If it's too high you overflow terminal clients buffers i think
#define MODEM_BUFFER_QUEUE_SIZE 1024

#define MODEM_DEFAULT_PORT 23

#define MODEM_TX_EVENT SERIAL_BASE_EVENT_COUNT + 1
#define MODEM_RX_POLLING SERIAL_BASE_EVENT_COUNT + 2
#define MODEM_RING_EVENT SERIAL_BASE_EVENT_COUNT + 3
#define SERIAL_MODEM_EVENT_COUNT SERIAL_BASE_EVENT_COUNT+3


enum ResTypes {
	ResNONE,
	ResOK,
	ResERROR,
	ResCONNECT,
	ResRING,
	ResBUSY,
	ResNODIALTONE,
	ResNOCARRIER,
	ResNOANSWER
};

#define TEL_CLIENT 0
#define TEL_SERVER 1

bool MODEM_ReadPhonebook(const std::string &filename);

class CFifo {
public:
	CFifo(const size_t n) : data(n), size(n) {}

	uint32_t left() const { return size - used; }
	uint32_t inuse() const { return used; }
	void clear()
	{
		used = 0;
		pos = 0;
	}

	void addb(uint8_t val)
	{
		if (used >= size) {
			static uint16_t lcount = 0;
			if (lcount < 1000) {
				lcount++;
				LOG_MSG("MODEM: FIFO Overflow! (addb)");
			}
			return;
		}
		//assert(used<size);
		size_t where = pos + used;
		if (where >= size)
			where -= size;
		data[where] = val;
		//LOG_MSG("+%x", val);
		used++;
	}

	void adds(uint8_t *str, size_t len)
	{
		if ((used + len) > size) {
			static uint16_t lcount = 0;
			if (lcount < 1000) {
				lcount++;
				LOG_MSG("MODEM: FIFO Overflow! (adds len %u)",
				        static_cast<unsigned>(len));
			}
			return;
		}

		//assert((used + len) <= size);
		size_t where = pos + used;
		used += len;
		while (len--) {
			if (where >= size)
				where -= size;
			//LOG_MSG("+'%x'", *str);
			data[where++] = *str++;
		}
	}

	uint8_t getb()
	{
		if (!used) {
			static uint16_t lcount = 0;
			if (lcount < 1000) {
				lcount++;
				LOG_MSG("MODEM: FIFO UNDERFLOW! (getb)");
			}
			return data[pos];
		}
		const size_t where = pos;
		if (++pos >= size)
			pos -= size;
		used--;
		//LOG_MSG("-%x",data[where]);
		return data[where];
	}

	void gets(uint8_t *str, size_t len)
	{
		if (!used) {
			static uint16_t lcount = 0;
			if (lcount < 1000) {
				lcount++;
				LOG_MSG("MODEM: FIFO UNDERFLOW! (gets len %u)",
				        static_cast<unsigned>(len));
			}
			return;
		}
		// assert(used >= len);
		used -= len;
		while (len--) {
			//LOG_MSG("-%x", data[pos]);
			*str++ = data[pos];
			if (++pos >= size)
				pos -= size;
		}
	}

private:
	std::vector<uint8_t> data;
	size_t size = 0;
	size_t pos = 0;
	size_t used = 0;
};
#define MREG_AUTOANSWER_COUNT 0
#define MREG_RING_COUNT 1
#define MREG_ESCAPE_CHAR 2
#define MREG_CR_CHAR 3
#define MREG_LF_CHAR 4
#define MREG_BACKSPACE_CHAR 5
#define MREG_GUARD_TIME 12
#define MREG_DTR_DELAY 25


class CSerialModem : public CSerial {
public:
	CSerialModem(const uint8_t port_idx, CommandLine *cmd);
	~CSerialModem();
	void Reset();

	void SendLine(const char *line);
	void SendRes(const ResTypes response);
	void SendNumber(uint32_t val);

	void EnterIdleState();
	void EnterConnectedState();
	bool Dial(const char *host);
	void AcceptIncomingCall();
	uint32_t ScanNumber(char *&scan) const;
	char GetChar(char * & scan) const;

	void DoCommand();

	// void MC_Changed(uint32_t new_mc);

	void TelnetEmulation(uint8_t *data, uint32_t size);

	//TODO
	void Timer2();
	void handleUpperEvent(uint16_t type);

	void RXBufferEmpty();

	void transmitByte(uint8_t val, bool first);
	void updatePortConfig(uint16_t divider, uint8_t lcr);
	void updateMSR();

	void setBreak(bool);

	void setRTSDTR(bool rts, bool dtr);
	void setRTS(bool val);
	void setDTR(bool val);

	std::unique_ptr<CFifo> rqueue;
	std::unique_ptr<CFifo> tqueue;

protected:
	char cmdbuf[255] = {0};
	bool commandmode = false; // true: interpret input as commands
	bool echo = false;        // local echo on or off
	bool oldDTRstate = false;
	bool ringing = false;
	bool numericresponse = false; // true: send control response as number.
	                              // false: send text (i.e. NO DIALTONE)
	bool telnetmode = false; // Default to direct null modem connection;
	                         // Telnet mode interprets IAC
	bool connected = false;
	uint32_t doresponse = 0;
	uint8_t waiting_tx_character = 0;
	uint32_t cmdpause = 0;
	int32_t ringtimer = 0;
	int32_t ringcount = 0;
	uint32_t plusinc = 0;
	uint32_t cmdpos = 0;
	uint32_t flowcontrol = 0;
	uint32_t dtrmode = 0;
	int32_t dtrofftimer = 0;
	uint8_t tmpbuf[MODEM_BUFFER_QUEUE_SIZE] = {0};
	uint16_t listenport = 23; // 23 is the default telnet TCP/IP port
	uint8_t reg[SREGS] = {0};
	std::unique_ptr<TCPServerSocket> serversocket = nullptr;
	std::unique_ptr<TCPClientSocket> clientsocket = nullptr;
	std::unique_ptr<TCPClientSocket> waitingclientsocket = nullptr;

	struct {
		bool binary[2] = {false};
		bool echo[2] = {false};
		bool supressGA[2] = {false};
		bool timingMark[2] = {false};
		bool inIAC = false;
		bool recCommand = false;
		uint8_t command = 0;
	} telClient;

	struct {
		bool active = false;
		double f1 = 0.0f;
		double f2 = 0.0f;
		uint32_t len = 0;
		uint32_t pos = 0;
		char str[256] = {0};
	} dial;
};
#endif
#endif
