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

/* $Id: directserial_posix.h,v 1.3 2009-01-26 20:15:58 qbix79 Exp $ */

// include guard
#ifndef DOSBOX_DIRECTSERIAL_POSIX_H
#define DOSBOX_DIRECTSERIAL_POSIX_H

#include "dosbox.h"

#if C_DIRECTSERIAL
#if defined (LINUX) || defined (MACOSX) || defined (BSD)



#define DIRECTSERIAL_AVAILIBLE
#include "serialport.h"
#include <termios.h>
#include <unistd.h>

class CDirectSerial : public CSerial {
public:
	termios termInfo;
	termios backup;
	int fileHandle;

	CDirectSerial(Bitu id, CommandLine* cmd);
	~CDirectSerial();
	bool receiveblock;		// It's not a block of data it rather blocks

	Bitu rx_retry;		// counter of retries

	Bitu rx_retry_max;	// how many POLL_EVENTS to wait before causing
						// a overrun error.

	void ReadCharacter();
	void CheckErrors();
	
	void updatePortConfig(Bit16u divider, Bit8u lcr);
	void updateMSR();
	void transmitByte(Bit8u val, bool first);
	void setBreak(bool value);
	
	void setRTSDTR(bool rts, bool dtr);
	void setRTS(bool val);
	void setDTR(bool val);
	void handleUpperEvent(Bit16u type);
		
};

#endif	// WIN32
#endif	// C_DIRECTSERIAL
#endif	// include guard
