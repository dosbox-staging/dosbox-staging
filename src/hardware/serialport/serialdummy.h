// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef INCLUDEGUARD_SERIALDUMMY_H
#define INCLUDEGUARD_SERIALDUMMY_H

#include "serialport.h"

//#define CHECKIT_TESTPLUG

class CSerialDummy final : public CSerial {
public:
	CSerialDummy(const uint8_t port_idx, CommandLine *cmd);
	~CSerialDummy() override;

	void setRTSDTR(bool rts, bool dtr) override;
	void setRTS(bool val) override;
	void setDTR(bool val) override;

	void updatePortConfig(uint16_t, uint8_t lcr) override;
	void updateMSR() override;
	void transmitByte(uint8_t val, bool first) override;
	void setBreak(bool value) override;
	void handleUpperEvent(uint16_t type) override;

#ifdef CHECKIT_TESTPLUG
	uint8_t loopbackdata = 0;
#endif

};

#endif // INCLUDEGUARD
