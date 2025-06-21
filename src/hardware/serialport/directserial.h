// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

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
	~CDirectSerial() override;

	void updatePortConfig(uint16_t divider, uint8_t lcr) override;
	void updateMSR() override;
	void transmitByte(uint8_t val, bool first) override;
	void setBreak(bool value) override;
	
	void setRTSDTR(bool rts, bool dtr) override;
	void setRTS(bool val) override;
	void setDTR(bool val) override;
	void handleUpperEvent(uint16_t type) override;

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
