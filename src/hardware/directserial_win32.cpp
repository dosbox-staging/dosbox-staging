/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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

/* $Id: directserial_win32.cpp,v 1.3 2004-08-04 09:12:55 qbix79 Exp $ */

#include "dosbox.h"

#if C_DIRECTSERIAL

/* Windows version */
#ifdef __WIN32__

#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "setup.h"
#include "serialport.h"

// Win32 related headers
#include <windows.h>

/* This is a serial passthrough class.  Its amazingly simple to */
/* write now that the serial ports themselves were abstracted out */


class CDirectSerial : public CSerial {
public:
	HANDLE hCom;
	DCB dcb;
	BOOL fSuccess;

	CDirectSerial(char * realPort, Bit16u baseAddr, Bit8u initIrq, Bit32u initBps, Bit16u bytesize, char *parity, Bit16u stopbits ) : CSerial(baseAddr, initIrq, initBps) {
		LOG_MSG("Opening Windows serial port");
		hCom = CreateFile(realPort, GENERIC_READ | GENERIC_WRITE,
                    0,    // must be opened with exclusive-access
                    NULL, // no security attributes
                    OPEN_EXISTING, // must use OPEN_EXISTING
                    0,    // non overlapped I/O
                    NULL  // hTemplate must be NULL for comm devices
                    );

		if (hCom == INVALID_HANDLE_VALUE) 
		{
			LOG_MSG("CreateFile failed with error %d.\n", GetLastError());
			hCom = 0;
			return;
		}

		fSuccess = GetCommState(hCom, &dcb);

		if (!fSuccess) 
		{
			// Handle the error.
			LOG_MSG("GetCommState failed with error %d.\n", GetLastError());
			return;
		}


		dcb.BaudRate = initBps;     // set the baud rate
		dcb.ByteSize = bytesize;             // data size, xmit, and rcv
		if(parity[0] == 'N') 
			dcb.Parity = NOPARITY;        // no parity bit
		if(parity[1] == 'E')
			dcb.Parity = EVENPARITY;        // even parity bit
		if(parity[2] == 'O')
			dcb.Parity = ODDPARITY;        // odd parity bit


		if(stopbits == 1) 
			dcb.StopBits = ONESTOPBIT;    // one stop bit
		if(stopbits == 2)
			dcb.StopBits = TWOSTOPBITS;   // two stop bits

		fSuccess = SetCommState(hCom, &dcb);

		// Configure timeouts to effectively use polling
		COMMTIMEOUTS ct;
		ct.ReadIntervalTimeout = MAXDWORD;
		ct.ReadTotalTimeoutConstant = 0;
		ct.ReadTotalTimeoutMultiplier = 0;
		ct.WriteTotalTimeoutConstant = 0;
		ct.WriteTotalTimeoutMultiplier = 0;
		SetCommTimeouts(hCom, &ct);

	}

	~CDirectSerial() {
		if(hCom != INVALID_HANDLE_VALUE) CloseHandle(hCom);
	}

	bool CanRecv(void) { return true; }
	bool CanSend(void) { return true; }

	void Send(Bit8u val) { tqueue->addb(val); }

	Bit8u Recv(Bit8u val) { return rqueue->getb(); }

	void updatestatus(void) {
		Bit8u ms=0;
		DWORD stat = 0;
		GetCommModemStatus(hCom, &stat);

		//Check for data carrier
		if(stat & MS_RLSD_ON) ms|=MS_DCD;
		if (stat & MS_RING_ON) ms|=MS_RI;
		if (stat & MS_DSR_ON) ms|=MS_DSR;
		if (stat & MS_CTS_ON) ms|=MS_CTS;
		SetModemStatus(ms);
	}
	
	void Timer(void) {
		DWORD dwRead;
		Bit8u chRead;

		if (ReadFile(hCom, &chRead, 1, &dwRead, NULL)) {
			if(dwRead != 0) {
				if(!rqueue->isFull()) rqueue->addb(chRead);
			}
		}

		updatestatus();

		Bit8u txval;

		Bitu tx_size=tqueue->inuse();
		while (tx_size--) {
			txval = tqueue->getb();
			DWORD bytesWritten;
			BOOL result;
			result = WriteFile(hCom, &txval, 1, &bytesWritten, NULL);
			if (!result) 
			{
				// Handle the error.
				LOG_MSG("WriteFile failed with error %d.\n", GetLastError());
				return;
			}

		}
	}

};

CDirectSerial *cds;

void DIRECTSERIAL_Init(Section* sec) {

	unsigned long args = 1;
	Section_prop * section=static_cast<Section_prop *>(sec);


	if(!section->Get_bool("directserial")) return;

	Bit16u comport = section->Get_int("comport");
	Bit32u bps = section->Get_int("defaultbps");
	switch (comport) {
		case 1:
			cds = new CDirectSerial((char *)section->Get_string("realport"), 0x3f0, 4, bps, section->Get_int("bytesize"), (char *)section->Get_string("parity"), section->Get_int("stopbit"));
			break;
		case 2:
			cds = new CDirectSerial((char *)section->Get_string("realport"), 0x2f0, 3, bps, section->Get_int("bytesize"), (char *)section->Get_string("parity"), section->Get_int("stopbit"));
			break;
		case 3:
			cds = new CDirectSerial((char *)section->Get_string("realport"), 0x3e0, 4, bps, section->Get_int("bytesize"), (char *)section->Get_string("parity"), section->Get_int("stopbit"));
			break;
		case 4:
			cds = new CDirectSerial((char *)section->Get_string("realport"), 0x2e0, 3, bps, section->Get_int("bytesize"), (char *)section->Get_string("parity"), section->Get_int("stopbit"));
			break;
		default:
			cds = new CDirectSerial((char *)section->Get_string("realport"), 0x3f0, 4, bps, section->Get_int("bytesize"), (char *)section->Get_string("parity"), section->Get_int("stopbit"));
			break;

	}



	seriallist.push_back(cds);
}
#else /*linux and others oneday maybe */

#endif 
#endif

