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

/* $Id: directserial_win32.h,v 1.2 2006-02-09 11:47:54 qbix79 Exp $ */

// include guard
#ifndef DOSBOX_DIRECTSERIAL_WIN32_H
#define DOSBOX_DIRECTSERIAL_WIN32_H

#include "dosbox.h"

#if C_DIRECTSERIAL
#ifdef WIN32



#define DIRECTSERIAL_AVAILIBLE
#include "serialport.h"
#include <windows.h>

class CDirectSerial : public CSerial {
public:
	HANDLE hCom;
	DCB dcb;
	BOOL fSuccess;

	CDirectSerial(
			IO_ReadHandler* rh,
			IO_WriteHandler* wh,
			TIMER_TickHandler th,
			Bit16u baseAddr,
			Bit8u initIrq,
			Bit32u initBps,
			Bit8u bytesize,
			const char *parity,
			Bit8u stopbits,
			const char * realPort
		);
	

	~CDirectSerial();
	
	
	Bitu lastChance;		// If there is no space for new
							// received data, it gets a little chance
	Bit8u ChanceChar;

	bool CanRecv(void);
	bool CanSend(void);

	bool InstallationSuccessful;	// check after constructing. If
									// something was wrong, delete it right away.

	void RXBufferEmpty();

	
	void updatePortConfig(Bit8u dll, Bit8u dlm, Bit8u lcr);
	void updateMSR();
	void transmitByte(Bit8u val);
	void setBreak(bool value);
	void updateModemControlLines(/*Bit8u mcr*/);
	void Timer2(void);
	
		
};

#endif	// WIN32
#endif	// C_DIRECTSERIAL
#endif	// include guard
