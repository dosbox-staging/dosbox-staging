/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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

/* $Id: directserial_win32.cpp,v 1.2 2005-11-04 08:53:07 qbix79 Exp $ */

#include "dosbox.h"

#if C_DIRECTSERIAL

/* Windows version */
#if defined (WIN32)

#include "serialport.h"
#include "directserial_win32.h"

// Win32 related headers
#include <windows.h>

/* This is a serial passthrough class.  Its amazingly simple to */
/* write now that the serial ports themselves were abstracted out */

CDirectSerial::CDirectSerial (IO_ReadHandler * rh, IO_WriteHandler * wh,
                              TIMER_TickHandler th, Bit16u baseAddr, Bit8u initIrq,
                              Bit32u initBps, Bit8u bytesize, const char *parity,
                              Bit8u stopbits,const char *realPort)
                              :CSerial (rh, wh, th,baseAddr,initIrq, initBps,
                              bytesize, parity,stopbits) {
	InstallationSuccessful = false;
	InstallTimerHandler(th);
	lastChance = 0;
	LOG_MSG ("Serial port at %x: Opening %s", base, realPort);
	hCom = CreateFile (realPort, GENERIC_READ | GENERIC_WRITE, 0,	// must be opened with exclusive-access
	                   NULL,          // no security attributes
	                   OPEN_EXISTING, // must use OPEN_EXISTING
	                   0,             // non overlapped I/O
	                   NULL           // hTemplate must be NULL for comm devices
	                  );

	if (hCom == INVALID_HANDLE_VALUE) {
		int error = GetLastError ();
		LOG_MSG ("Serial port \"%s\" could not be opened.", realPort);
		if (error == 2) {
			LOG_MSG ("The specified port does not exist.");
		} else if (error == 5) {
			LOG_MSG ("The specified port is already in use.");
		} else {
			LOG_MSG ("Windows error %d occurred.", error);
		}

		hCom = 0;
		return;
	}

	fSuccess = GetCommState (hCom, &dcb);

	if (!fSuccess) {
		// Handle the error.
		LOG_MSG ("GetCommState failed with error %d.\n", GetLastError ());
		hCom = 0;
		return;
	}
	// Configure timeouts to effectively use polling
	COMMTIMEOUTS ct;
	ct.ReadIntervalTimeout = MAXDWORD;
	ct.ReadTotalTimeoutConstant = 0;
	ct.ReadTotalTimeoutMultiplier = 0;
	ct.WriteTotalTimeoutConstant = 0;
	ct.WriteTotalTimeoutMultiplier = 0;
	SetCommTimeouts (hCom, &ct);

	CSerial::Init_Registers (initBps, bytesize, parity, stopbits);
	InstallationSuccessful = true;
	//LOG_MSG("InstSuccess");
}

CDirectSerial::~CDirectSerial () {
	if (hCom != INVALID_HANDLE_VALUE)
		CloseHandle (hCom);
}

Bitu lastChance;

void CDirectSerial::RXBufferEmpty () {
	DWORD dwRead;
	DWORD errors;
	Bit8u chRead;
	if (lastChance > 0) {
		receiveByte (ChanceChar);
		lastChance = 0;
	} else {
		// update RX
		if (ReadFile (hCom, &chRead, 1, &dwRead, NULL)) {
			if (dwRead != 0) {
				//LOG_MSG("UART 0x%x: RX 0x%x", base,chRead);
				receiveByte (chRead);
			}
		}
	}
	// check for errors
	if (ClearCommError (hCom, &errors, NULL))
		if (errors & (CE_BREAK | CE_FRAME | CE_RXPARITY)) {
			Bit8u errreg = 0;
			if (errors & CE_BREAK) {
				LOG_MSG ("Serial port at 0x%x: line error: break received.", base);
				errreg |= LSR_RX_BREAK_MASK;
			}
			if (errors & CE_FRAME) {
				LOG_MSG ("Serial port at 0x%x: line error: framing error.", base);
				errreg |= LSR_FRAMING_ERROR_MASK;
			}
			if (errors & CE_RXPARITY) {
				LOG_MSG ("Serial port at 0x%x: line error: parity error.", base);
				errreg |= LSR_PARITY_ERROR_MASK;
			}
			receiveError (errreg);
		}
}

/*****************************************************************************/
/* updatePortConfig is called when emulated app changes the serial port     **/
/* parameters baudrate, stopbits, number of databits, parity.               **/
/*****************************************************************************/
void CDirectSerial::updatePortConfig (Bit8u dll, Bit8u dlm, Bit8u lcr) {
	Bit8u parity = 0;
	Bit8u bytelength = 0;
	Bit16u baudrate = 0;

	// baud
	baudrate = dlm;
	baudrate = baudrate << 8;
	baudrate |= dll;
	if (baudrate <= 0x1)
		dcb.BaudRate = CBR_115200;
	else if (baudrate <= 0x2)
		dcb.BaudRate = CBR_57600;
	else if (baudrate <= 0x3)
		dcb.BaudRate = CBR_38400;
	else if (baudrate <= 0x6)
		dcb.BaudRate = CBR_19200;
	else if (baudrate <= 0xc)
		dcb.BaudRate = CBR_9600;
	else if (baudrate <= 0x18)
		dcb.BaudRate = CBR_4800;
	else if (baudrate <= 0x30)
		dcb.BaudRate = CBR_2400;
	else if (baudrate <= 0x60)
		dcb.BaudRate = CBR_1200;
	else if (baudrate <= 0xc0)
		dcb.BaudRate = CBR_600;
	else if (baudrate <= 0x180)
		dcb.BaudRate = CBR_300;
	else if (baudrate <= 0x417)
		dcb.BaudRate = CBR_110;

	// I read that windows can handle nonstandard baudrates:
	else
		dcb.BaudRate = 115200 / baudrate;

#ifdef SERIALPORT_DEBUGMSG
	LOG_MSG ("Serial port at %x: new baud rate: %d", base, dcb.BaudRate);
#endif

	// byte length
	bytelength = lcr & 0x3;
	bytelength += 5;
	dcb.ByteSize = bytelength;

	// parity
	parity = lcr & 0x38;
	parity = parity >> 3;
	switch (parity) {
	case 0x1:
			dcb.Parity = ODDPARITY;
			break;
	case 0x3:
			dcb.Parity = EVENPARITY;
			break;
	case 0x5:
			dcb.Parity = MARKPARITY;
			break;
	case 0x7:
			dcb.Parity = SPACEPARITY;
			break;
	default:
			dcb.Parity = NOPARITY;
			break;
	}

	// stopbits
	if (lcr & 0x4) {
		if (bytelength == 5)
			dcb.StopBits = ONE5STOPBITS;
		else
			dcb.StopBits = TWOSTOPBITS;
	} else {
		dcb.StopBits = ONESTOPBIT;
	}

	if (!SetCommState (hCom, &dcb))
		LOG_MSG ("Serial port at 0x%x: API did not like the new values.", base);
	//LOG_MSG("Serial port at 0x%x: Port params changed: %d Baud", base,dcb.BaudRate);
}

void CDirectSerial::updateMSR () {
	Bit8u newmsr = 0;
	DWORD dptr = 0;

	if (!GetCommModemStatus (hCom, &dptr)) {
#ifdef SERIALPORT_DEBUGMSG
//		LOG_MSG ("Serial port at %x: GetCommModemStatus failed!", base);
#endif
		//return;
	}
	if (dptr & MS_CTS_ON)
		newmsr |= MSR_CTS_MASK;
	if (dptr & MS_DSR_ON)
		newmsr |= MSR_DSR_MASK;
	if (dptr & MS_RING_ON)
		newmsr |= MSR_RI_MASK;
	if (dptr & MS_RLSD_ON)
		newmsr |= MSR_CD_MASK;
	changeMSR (newmsr);
}

void CDirectSerial::transmitByte (Bit8u val) {
	// mean bug: with break = 1, WriteFile will never return.
	if((LCR&LCR_BREAK_MASK) == 0) {
    
		DWORD bytesWritten = 0;
		WriteFile (hCom, &val, 1, &bytesWritten, NULL);
		if (bytesWritten > 0) {
			ByteTransmitted ();
			//LOG_MSG("UART 0x%x: TX 0x%x", base,val);
		} else {
			LOG_MSG ("UART 0x%x: NO BYTE WRITTEN! PORT HANGS NOW!", base);
		}
	} else {
		// have a delay here, it's the only sense of sending
		// data with break=1
		Bitu ticks;
		Bitu elapsed = 0;
		ticks = GetTicks();

		while(elapsed < 10) {
			elapsed = GetTicks() - ticks;
		}
		ByteTransmitted();
	}
}

/*****************************************************************************/
/* setBreak(val) switches break on or off                                   **/
/*****************************************************************************/

void CDirectSerial::setBreak (bool value) {
	//#ifdef SERIALPORT_DEBUGMSG
	//LOG_MSG("UART 0x%x: Break toggeled: %d", base, value);
	//#endif
	if (value)
		SetCommBreak (hCom);
	else
		ClearCommBreak (hCom);
}

/*****************************************************************************/
/* updateModemControlLines(mcr) sets DTR and RTS.                           **/
/*****************************************************************************/
void CDirectSerial::updateModemControlLines ( /*Bit8u mcr */ ) {
	bool change = false;

		/*** DTR ***/
	if (CSerial::getDTR ()) {			// DTR on
		if (dcb.fDtrControl == DTR_CONTROL_DISABLE) {
			dcb.fDtrControl = DTR_CONTROL_ENABLE;
			change = true;
		}
	} else {
		if (dcb.fDtrControl == DTR_CONTROL_ENABLE) {
			dcb.fDtrControl = DTR_CONTROL_DISABLE;
			change = true;
		}
	}
		/*** RTS ***/
	if (CSerial::getRTS ()) {			// RTS on
		if (dcb.fRtsControl == RTS_CONTROL_DISABLE) {
			dcb.fRtsControl = RTS_CONTROL_ENABLE;
			change = true;
		}
	} else {
		if (dcb.fRtsControl == RTS_CONTROL_ENABLE) {
			dcb.fRtsControl = RTS_CONTROL_DISABLE;
			change = true;
		}
	}
	if (change)
		SetCommState (hCom, &dcb);
}

void CDirectSerial::Timer2(void) {
	DWORD dwRead = 0;
	DWORD errors = 0;
	Bit8u chRead = 0;


	if (lastChance == 0) {		// lastChance = 0
		if (CanReceiveByte ()) {
			if (ReadFile (hCom, &chRead, 1, &dwRead, NULL)) {
				if (dwRead)
					receiveByte (chRead);
			}
		} else {
			if (ReadFile (hCom, &chRead, 1, &dwRead, NULL)) {
				if (dwRead) {
					ChanceChar = chRead;
					lastChance++;
				}
			}
		}
	} else if (lastChance > 10) {
		receiveByte (0);						// this causes RX Overrun now
		lastChance = 0;
		// empty serial buffer
		dwRead = 1;
		while (dwRead > 0) {				// throw away bytes in buffer
			ReadFile (hCom, &chRead, 1, &dwRead, NULL);
		}
	} else {			// lastChance>0    // already one waiting
		if (CanReceiveByte ()) {		// chance used
			receiveByte (ChanceChar);
			lastChance = 0;
		} else
			lastChance++;
	}

	// check for errors
	if (ClearCommError (hCom, &errors, NULL))
		if (errors & (CE_BREAK | CE_FRAME | CE_RXPARITY)) {
			Bit8u errreg = 0;

			if (errors & CE_BREAK) {
				LOG_MSG ("Serial port at 0x%x: line error: break received.", base);
				errreg |= LSR_RX_BREAK_MASK;
			}
			if (errors & CE_FRAME) {
				LOG_MSG ("Serial port at 0x%x: line error: framing error.", base);
				errreg |= LSR_FRAMING_ERROR_MASK;
			}
			if (errors & CE_RXPARITY) {
				LOG_MSG ("Serial port at 0x%x: line error: parity error.", base);
				errreg |= LSR_PARITY_ERROR_MASK;
			}

			receiveError (errreg);
		}
	// update Modem input line states
	updateMSR ();
}


#else	/*linux and others oneday maybe */

#endif
#endif
