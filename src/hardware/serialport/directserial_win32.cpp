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

/* $Id: directserial_win32.cpp,v 1.5 2007-01-13 08:35:49 qbix79 Exp $ */

#include "dosbox.h"

#if C_DIRECTSERIAL

/* Windows version */
#if defined (WIN32)

#include "serialport.h"
#include "directserial_win32.h"
#include "misc_util.h"
#include "pic.h"

// Win32 related headers
#include <windows.h>

/* This is a serial passthrough class.  Its amazingly simple to */
/* write now that the serial ports themselves were abstracted out */

CDirectSerial::CDirectSerial (Bitu id, CommandLine* cmd)
					:CSerial (id, cmd) {
	InstallationSuccessful = false;

	rx_retry = 0;
    rx_retry_max = 0;

	// open the port in NT object space (recommended by Microsoft)
	// allows the user to open COM10+ and custom port names.
	std::string prefix="\\\\.\\";
	std::string tmpstring;
	if(!cmd->FindStringBegin("realport:",tmpstring,false)) return;

#if SERIAL_DEBUG
		if(dbg_modemcontrol)
			fprintf(debugfp,"%12.3f Port type directserial realport %s\r\n",
				PIC_FullIndex(),tmpstring.c_str());
#endif

	prefix.append(tmpstring);

	// rxdelay: How many milliseconds to wait before causing an
	// overflow when the application is unresponsive.
	if(getBituSubstring("rxdelay:", &rx_retry_max, cmd)) {
		if(!(rx_retry_max<=10000)) {
			rx_retry_max=0;
		}
	}

	const char* tmpchar=prefix.c_str();

	LOG_MSG ("Serial%d: Opening %s", COMNUMBER, tmpstring.c_str());
	hCom = CreateFile (tmpchar,
					   GENERIC_READ | GENERIC_WRITE, 0,
									  // must be opened with exclusive-access
	                   NULL,          // no security attributes
	                   OPEN_EXISTING, // must use OPEN_EXISTING
	                   0,             // non overlapped I/O
	                   NULL           // hTemplate must be NULL for comm devices
	                  );

	if (hCom == INVALID_HANDLE_VALUE) {
		int error = GetLastError ();
		LOG_MSG ("Serial%d: Serial Port \"%s\" could not be opened.",
			COMNUMBER, tmpstring.c_str());
		if (error == 2) {
			LOG_MSG ("The specified port does not exist.");
		} else if (error == 5) {
			LOG_MSG ("The specified port is already in use.");
		} else {
			LOG_MSG ("Windows error %d occurred.", error);
		}
		return;
	}

	dcb.DCBlength=sizeof(dcb);
	fSuccess = GetCommState (hCom, &dcb);

	if (!fSuccess) {
		// Handle the error.
		LOG_MSG ("GetCommState failed with error %d.\n", GetLastError ());
		hCom = INVALID_HANDLE_VALUE;
		return;
	}

	// initialize the port
	dcb.BaudRate=CBR_9600;
	dcb.fBinary=true;
	dcb.fParity=true;
	dcb.fOutxCtsFlow=false;
	dcb.fOutxDsrFlow=false;
	dcb.fDtrControl=DTR_CONTROL_DISABLE;
	dcb.fDsrSensitivity=false;
	
	dcb.fOutX=false;
	dcb.fInX=false;
	dcb.fErrorChar=0;
	dcb.fNull=false;
	dcb.fRtsControl=RTS_CONTROL_DISABLE;
	dcb.fAbortOnError=false;

	dcb.ByteSize=8;
	dcb.Parity=NOPARITY;
	dcb.StopBits=ONESTOPBIT;

	fSuccess = SetCommState (hCom, &dcb);

	if (!fSuccess) {
		// Handle the error.
		LOG_MSG ("SetCommState failed with error %d.\n", GetLastError ());
		hCom = INVALID_HANDLE_VALUE;
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

	CSerial::Init_Registers();
	InstallationSuccessful = true;
	receiveblock=false;

	ClearCommBreak (hCom);
	setEvent(SERIAL_POLLING_EVENT, 1); // millisecond tick
}

CDirectSerial::~CDirectSerial () {
	if (hCom != INVALID_HANDLE_VALUE) CloseHandle (hCom);
	// We do not use own events so we don't have to clear them.
}

void CDirectSerial::handleUpperEvent(Bit16u type) {
	
	switch(type) {
		case SERIAL_POLLING_EVENT: {
			DWORD dwRead = 0;
			DWORD errors = 0;
			Bit8u chRead = 0;
			
			setEvent(SERIAL_POLLING_EVENT, 1);
			if(!receiveblock) {
				if(((!(LSR&LSR_RX_DATA_READY_MASK)) || rx_retry>=rx_retry_max ))
				{
					rx_retry=0;
					if (ReadFile (hCom, &chRead, 1, &dwRead, NULL)) {
						if (dwRead) {
							receiveByte (chRead);
							setEvent(40, bytetime-0.03f); // receive timing
							receiveblock=true;
						}
					}
				} else rx_retry++;
			}
			// check for errors
			CheckErrors();
			// update Modem input line states
			updateMSR ();
			break;
		}
		case 40: {
		// receive time is up
			DWORD dwRead = 0;
			Bit8u chRead = 0;
			receiveblock=false;
			// check if there is something to receive
			if(((!(LSR&LSR_RX_DATA_READY_MASK)) || rx_retry>=rx_retry_max ))
			{
				rx_retry=0;
				if (ReadFile (hCom, &chRead, 1, &dwRead, NULL)) {
					if (dwRead) {
						receiveByte (chRead);
						setEvent(40, bytetime-0.03f); // receive timing
						receiveblock=true;
					}
				}
			} else rx_retry++;
			break;
		}
		case SERIAL_TX_EVENT: {
			DWORD dwRead = 0;
			Bit8u chRead = 0;
			if(!receiveblock) {
				if(((!(LSR&LSR_RX_DATA_READY_MASK)) || rx_retry>=rx_retry_max ))
				{
					rx_retry=0;
					if (ReadFile (hCom, &chRead, 1, &dwRead, NULL)) {
						if (dwRead) {
							receiveByte (chRead);
							setEvent(40, bytetime-0.03f); // receive timing
							receiveblock=true;
						}
					}
				} else rx_retry++;
			}
			ByteTransmitted();
			break;
		}
		case SERIAL_THR_EVENT: {
			ByteTransmitting();
			setEvent(SERIAL_TX_EVENT,bytetime+0.03f);
			break;				   
		}
	}
}

void CDirectSerial::CheckErrors() {
	
	DWORD errors=0;
	// check for errors
	if (ClearCommError (hCom, &errors, NULL))
		if (errors & (CE_BREAK | CE_FRAME | CE_RXPARITY)) {
			Bit8u errreg = 0;
			if (errors & CE_BREAK) errreg |= LSR_RX_BREAK_MASK;
			if (errors & CE_FRAME) errreg |= LSR_FRAMING_ERROR_MASK;
			if (errors & CE_RXPARITY) errreg |= LSR_PARITY_ERROR_MASK;
			receiveError (errreg);
		}
}

/*****************************************************************************/
/* updatePortConfig is called when emulated app changes the serial port     **/
/* parameters baudrate, stopbits, number of databits, parity.               **/
/*****************************************************************************/
void CDirectSerial::updatePortConfig (Bit16u divider, Bit8u lcr) {
	Bit8u parity = 0;
	Bit8u bytelength = 0;

	// baud
	if (divider == 0x1)
		dcb.BaudRate = CBR_115200;
	else if (divider == 0x2)
		dcb.BaudRate = CBR_57600;
	else if (divider == 0x3)
		dcb.BaudRate = CBR_38400;
	else if (divider == 0x6)
		dcb.BaudRate = CBR_19200;
	else if (divider == 0xc)
		dcb.BaudRate = CBR_9600;
	else if (divider == 0x18)
		dcb.BaudRate = CBR_4800;
	else if (divider == 0x30)
		dcb.BaudRate = CBR_2400;
	else if (divider == 0x60)
		dcb.BaudRate = CBR_1200;
	else if (divider == 0xc0)
		dcb.BaudRate = CBR_600;
	else if (divider == 0x180)
		dcb.BaudRate = CBR_300;
	else if (divider == 0x417)
		dcb.BaudRate = CBR_110;

	// I read that windows can handle nonstandard baudrates:
	else
		dcb.BaudRate = 115200 / divider;

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

#ifdef SERIALPORT_DEBUGMSG
	LOG_MSG ("__________________________");
	LOG_MSG ("Serial%d: new baud rate: %d", COMNUMBER, dcb.BaudRate);
	LOG_MSG ("Serial%d: new bytelen: %d", COMNUMBER, dcb.ByteSize);
	LOG_MSG ("Serial%d: new parity: %d", COMNUMBER, dcb.Parity);
	LOG_MSG ("Serial%d: new stopbits: %d", COMNUMBER, dcb.StopBits);
#endif

	if (!SetCommState (hCom, &dcb)) {

#if SERIAL_DEBUG
		if(dbg_modemcontrol)
			fprintf(debugfp,"%12.3f serial mode not supported: rate=%d,LCR=%x.\r\n",
			PIC_FullIndex(),dcb.BaudRate,lcr);
#endif

		LOG_MSG ("Serial%d: Desired serial mode not supported (%d,%d,%d,%d",
			dcb.BaudRate,dcb.ByteSize,dcb.Parity,dcb.StopBits, COMNUMBER);
	}
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
	setCTS((dptr & MS_CTS_ON)!=0);
	setDSR((dptr & MS_DSR_ON)!=0);
	setRI ((dptr & MS_RING_ON)!=0);
	setCD((dptr & MS_RLSD_ON)!=0);
}

void CDirectSerial::transmitByte (Bit8u val, bool first) {
	// mean bug: with break = 1, WriteFile will never return.
	if((LCR&LCR_BREAK_MASK) == 0) {
		DWORD bytesWritten = 0;
		WriteFile (hCom, &val, 1, &bytesWritten, NULL);
		if (bytesWritten != 1)
			LOG_MSG ("Serial%d: COM port error: write failed!", idnumber);
	}
	if(first) setEvent(SERIAL_THR_EVENT, bytetime/8);
	else setEvent(SERIAL_TX_EVENT, bytetime);
}

/*****************************************************************************/
/* setBreak(val) switches break on or off                                   **/
/*****************************************************************************/
void CDirectSerial::setBreak (bool value) {
	if (value) SetCommBreak (hCom);
	else ClearCommBreak (hCom);
}

/*****************************************************************************/
/* updateModemControlLines(mcr) sets DTR and RTS.                           **/
/*****************************************************************************/
void CDirectSerial::setRTSDTR(bool rts, bool dtr) {
	if(rts) dcb.fRtsControl = RTS_CONTROL_ENABLE;
	else dcb.fRtsControl = RTS_CONTROL_DISABLE;
	if(dtr) dcb.fDtrControl = DTR_CONTROL_ENABLE;
	else dcb.fDtrControl = DTR_CONTROL_DISABLE;
	SetCommState (hCom, &dcb);

}
void CDirectSerial::setRTS(bool val) {
	if(val) dcb.fRtsControl = RTS_CONTROL_ENABLE;
	else dcb.fRtsControl = RTS_CONTROL_DISABLE;
	SetCommState (hCom, &dcb);
}
void CDirectSerial::setDTR(bool val) {
	if(val) dcb.fDtrControl = DTR_CONTROL_ENABLE;
	else dcb.fDtrControl = DTR_CONTROL_DISABLE;
	SetCommState (hCom, &dcb);
}

#endif
#endif
