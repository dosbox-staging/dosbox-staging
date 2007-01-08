/*
 *  Copyright (C) 2002-2007  The DOSBox Team
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

/* $Id: serialdummy.h,v 1.3 2007-01-08 19:45:41 qbix79 Exp $ */

#ifndef INCLUDEGUARD_SERIALDUMMY_H
#define INCLUDEGUARD_SERIALDUMMY_H

#include "serialport.h"

class CSerialDummy : public CSerial {
public:
	
	CSerialDummy(
		IO_ReadHandler* rh,
		IO_WriteHandler* wh,
		TIMER_TickHandler th,
		Bit16u baseAddr,
		Bit8u initIrq,
		Bit32u initBps,
		Bit8u bytesize,
		const char* parity,
		Bit8u stopbits
		);


	~CSerialDummy();
	bool CanRecv(void);
	bool CanSend(void);
	void RXBufferEmpty();
	void updatePortConfig(Bit8u dll, Bit8u dlm, Bit8u lcr);
	void updateMSR();
	void transmitByte(Bit8u val);
	void setBreak(bool value);
	void updateModemControlLines(/*Bit8u mcr*/);
	void Timer2(void);
};

#endif // INCLUDEGUARD
