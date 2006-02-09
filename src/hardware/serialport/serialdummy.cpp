/*
 *  Copyright (C) 2002-2006  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: serialdummy.cpp,v 1.2 2006-02-09 11:47:54 qbix79 Exp $ */

#include "dosbox.h"

#include "setup.h"
#include "serialdummy.h"
#include "serialport.h"

	
CSerialDummy::CSerialDummy(
					IO_ReadHandler* rh,
					IO_WriteHandler* wh,
					TIMER_TickHandler th, 
					Bit16u baseAddr,
					Bit8u initIrq,
					Bit32u initBps,
					Bit8u bytesize,
					const char* parity,
					Bit8u stopbits
		) : CSerial(
				rh, wh, th,
				baseAddr,initIrq,initBps,bytesize,parity,stopbits)
	{
		CSerial::Init_Registers(initBps,bytesize,parity,stopbits);
	}

CSerialDummy::~CSerialDummy() {
}

void CSerialDummy::RXBufferEmpty() {
// no external buffer, not used here
}

/*****************************************************************************/
/* updatePortConfig is called when emulated app changes the serial port     **/
/* parameters baudrate, stopbits, number of databits, parity.               **/
/*****************************************************************************/
void CSerialDummy::updatePortConfig(Bit8u dll, Bit8u dlm, Bit8u lcr) {
	//LOG_MSG("Serial port at 0x%x: Port params changed: %d Baud", base,dcb.BaudRate);
}

void CSerialDummy::updateMSR() {
	changeMSR(0);
}

void CSerialDummy::transmitByte(Bit8u val) {
	ByteTransmitted();
	//LOG_MSG("UART 0x%x: TX 0x%x", base,val);
}

/*****************************************************************************/
/* setBreak(val) switches break on or off                                   **/
/*****************************************************************************/

void CSerialDummy::setBreak(bool value) {
	//LOG_MSG("UART 0x%x: Break toggeled: %d", base, value);
}

/*****************************************************************************/
/* updateModemControlLines(mcr) sets DTR and RTS.                           **/
/*****************************************************************************/
void CSerialDummy::updateModemControlLines(/*Bit8u mcr*/) {
}
	
void CSerialDummy::Timer2(void) {	
}	

