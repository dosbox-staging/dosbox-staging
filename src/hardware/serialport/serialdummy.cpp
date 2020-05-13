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


#include "dosbox.h"

#include "setup.h"
#include "serialdummy.h"
#include "serialport.h"

CSerialDummy::CSerialDummy(const uint8_t port_idx, CommandLine *cmd)
        : CSerial(port_idx, cmd)
{
	CSerial::Init_Registers();
	setRI(false);
	setDSR(false);
	setCD(false);
	setCTS(false);
	InstallationSuccessful=true;
}

CSerialDummy::~CSerialDummy() {
	// clear events
	removeEvent(SERIAL_TX_EVENT);
}

void CSerialDummy::handleUpperEvent(uint16_t type)
{
	if (type == SERIAL_TX_EVENT) {
		// LOG_MSG("SERIAL: Port %" PRIu8 " TX_EVENT", GetPortNumber());
#ifdef CHECKIT_TESTPLUG
		receiveByte(loopbackdata);
#endif
		ByteTransmitted(); // tx timeout
	} else if (type == SERIAL_THR_EVENT) {
		//LOG_MSG("SERIAL: Port %" PRIu8 " THR_EVENT", GetPortNumber());
		ByteTransmitting();
		setEvent(SERIAL_TX_EVENT,bytetime);
	}
}

/*****************************************************************************/
/* updatePortConfig is called when emulated app changes the serial port     **/
/* parameters baudrate, stopbits, number of databits, parity.               **/
/*****************************************************************************/
void CSerialDummy::updatePortConfig(uint16_t divider, uint8_t lcr)
{
	(void)divider; // unused
	(void)lcr;     // unused

	// LOG_MSG("SERIAL: Port %" PRIu8 " at UART 0x%x params changed: %"
	//         PRIu16 " baud", GetPortNumber(), base, dcb.BaudRate);
}

void CSerialDummy::updateMSR() {
}

void CSerialDummy::transmitByte(uint8_t val, bool first)
{
	(void)val; // unused
	if (first)
		setEvent(SERIAL_THR_EVENT, bytetime / 10);
	else setEvent(SERIAL_TX_EVENT, bytetime);

#ifdef CHECKIT_TESTPLUG
	loopbackdata=val;
#endif
}

/*****************************************************************************/
/* setBreak(val) switches break on or off                                   **/
/*****************************************************************************/

void CSerialDummy::setBreak(bool value) {
	(void)value; // unused
	// LOG_MSG("SERIAL: Port %" PRIu8 " at UART 0x%x break "
	//         "toggled: %d.", GetPortNumber(), base, value);
}

/*****************************************************************************/
/* setRTSDTR sets the modem control lines                                   **/
/*****************************************************************************/
void CSerialDummy::setRTSDTR(bool rts_state, bool dtr_state)
{
	setRTS(rts_state);
	setDTR(dtr_state);
}
void CSerialDummy::setRTS(bool val) {
#ifdef CHECKIT_TESTPLUG
	setCTS(val);
#else
	(void)val; // unused
#endif
}
void CSerialDummy::setDTR(bool val) {
#ifdef CHECKIT_TESTPLUG
	setDSR(val);
	setRI(val);
	setCD(val);
#else
	(void)val; // unused
#endif
}
