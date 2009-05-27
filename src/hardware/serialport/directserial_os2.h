/*
 *  Copyright (C) 2002-2009  The DOSBox Team
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

/* $Id: directserial_os2.h,v 1.5 2009-05-27 09:15:41 qbix79 Exp $ */

// include guard
#ifndef DOSBOX_DIRECTSERIAL_OS2_H
#define DOSBOX_DIRECTSERIAL_OS2_H

#include "dosbox.h"

#if C_DIRECTSERIAL
#if defined(OS2)
#define DIRECTSERIAL_AVAILIBLE
#include "serialport.h"
#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSPROCESS
#include <os2.h>

class CDirectSerial : public CSerial {
public:
	HFILE hCom;
	BOOL fSuccess;

	CDirectSerial(Bitu id, CommandLine* cmd);
	~CDirectSerial();
	
	
	//Bitu lastChance;		// If there is no space for new
							// received data, it gets a little chance
	//Bit8u ChanceChar;

	//bool CanRecv(void);
	//bool CanSend(void);


	//void RXBufferEmpty();
	bool receiveblock;
	Bitu rx_retry;
	Bitu rx_retry_max;

	void CheckErrors();
	
	void updatePortConfig(Bit16u divider, Bit8u lcr);
	void updateMSR();
	void transmitByte(Bit8u val, bool first);
	void setBreak(bool value);

	void setRTSDTR(bool rts, bool dtr);
	void setRTS(bool val);
	void setDTR(bool val);
	void handleUpperEvent(Bit16u type);


	//void updateModemControlLines(/*Bit8u mcr*/);
	//void Timer2(void);
	
		
};
#endif	// IFDEF

#endif	// C_DIRECTSERIAL
#endif	// include guard

