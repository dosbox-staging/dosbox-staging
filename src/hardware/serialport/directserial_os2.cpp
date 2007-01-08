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

/* $Id: directserial_os2.cpp,v 1.3 2007-01-08 19:45:40 qbix79 Exp $ */

#include "dosbox.h"

#if C_DIRECTSERIAL


#if defined(OS2)
#include "serialport.h"
#include "directserial_os2.h"

// OS/2 related headers
#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSPROCESS
#include <os2.h>

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
	LOG_MSG ("OS/2 Serial port at %x: Opening %s", base, realPort);
        LOG_MSG("Opening OS2 serial port");

	ULONG ulAction = 0;
	APIRET rc = DosOpen((unsigned char*)realPort, &hCom, &ulAction, 0L, FILE_NORMAL, FILE_OPEN,
			OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYNONE | OPEN_FLAGS_SEQUENTIAL, 0L);
	if (rc != NO_ERROR)
	{
		LOG_MSG ("Serial port \"%s\" could not be opened.", realPort);
		if (rc == 2) {
			LOG_MSG ("The specified port does not exist.");
		} else if (rc == 99) {
			LOG_MSG ("The specified port is already in use.");
		} else {
			LOG_MSG ("OS/2 error %d occurred.", rc);
		}

		hCom = 0;
		return;
	}

	DCBINFO dcb;
	ULONG ulParmLen = sizeof(DCBINFO);
	rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETDCBINFO, 0, 0, 0, &dcb, ulParmLen, &ulParmLen);
	if ( rc != NO_ERROR)
	{
		DosClose(hCom);
		hCom = 0;
		return;
	}
	dcb.usWriteTimeout = 0;
	dcb.usReadTimeout = 0; //65535;
	dcb.fbTimeout |= 6;
	rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_SETDCBINFO, &dcb, ulParmLen, &ulParmLen, 0, 0, 0);
	if ( rc != NO_ERROR)
	{
		DosClose(hCom);
		hCom = 0;
		return;
	}

	CSerial::Init_Registers (initBps, bytesize, parity, stopbits);
	InstallationSuccessful = true;
	//LOG_MSG("InstSuccess");
}

CDirectSerial::~CDirectSerial () {
	if (hCom != 0)
		DosClose (hCom);
}

Bitu lastChance;

void CDirectSerial::RXBufferEmpty () {
	ULONG dwRead;
	Bit8u chRead;
	USHORT errors = 0;
	ULONG ulParmLen = sizeof(errors);

	if (lastChance > 0) {
		receiveByte (ChanceChar);
		lastChance = 0;
	} else {
		// update RX
		if (DosRead (hCom, &chRead, 1, &dwRead) != NO_ERROR) {
			if (dwRead != 0) {
				//LOG_MSG("UART 0x%x: RX 0x%x", base,chRead);
				receiveByte (chRead);
			}
		}
	}
	// check for errors
	Bit8u errreg = 0;
	APIRET rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETCOMMERROR, 0, 0, 0, &errors, ulParmLen, &ulParmLen);
	if (rc != NO_ERROR && errors)
                                     {
		if (errors & 8) {
			LOG_MSG ("Serial port at 0x%x: line error: framing error.", base);
			errreg |= LSR_FRAMING_ERROR_MASK;
		}
		if (errors & 4) {
			LOG_MSG ("Serial port at 0x%x: line error: parity error.", base);
			errreg |= LSR_PARITY_ERROR_MASK;
		}
	}
	errors = 0;
	rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETCOMMEVENT, 0, 0, 0, &errors, ulParmLen, &ulParmLen);
	if (rc != NO_ERROR && errors)
	{
		if (errors & 6) {
			LOG_MSG ("Serial port at 0x%x: line error: break received.", base);
			errreg |= LSR_RX_BREAK_MASK;
		}
	}
	if (errreg != 0)
	{
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
	Bit16u baudrate = 0, baud = 0;

	// baud
	baudrate = dlm;
	baudrate = baudrate << 8;
	baudrate |= dll;
	if (baudrate <= 0x1)
		baud = 115200;
	else if (baudrate <= 0x2)
		baud = 57600;
	else if (baudrate <= 0x3)
		baud = 38400;
	else if (baudrate <= 0x6)
		baud = 19200;
	else if (baudrate <= 0xc)
		baud = 9600;
	else if (baudrate <= 0x18)
		baud = 4800;
	else if (baudrate <= 0x30)
		baud = 2400;
	else if (baudrate <= 0x60)
		baud = 1200;
	else if (baudrate <= 0xc0)
		baud = 600;
	else if (baudrate <= 0x180)
		baud = 300;
	else if (baudrate <= 0x417)
		baud = 110;

	// I read that windows can handle nonstandard baudrates:
	else
		baud = 115200 / baudrate;

#ifdef SERIALPORT_DEBUGMSG
	LOG_MSG ("Serial port at %x: new baud rate: %d", base, dcb.BaudRate);
#endif

	struct {
		ULONG baud;
		BYTE fraction;
	} setbaud;
	setbaud.baud = baud;
	setbaud.fraction = 0;
	ULONG ulParmLen = sizeof(setbaud);
	APIRET rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_EXTSETBAUDRATE, &setbaud, ulParmLen, &ulParmLen, 0, 0, 0);
	if (rc != NO_ERROR)
	{
	}


	struct {
		UCHAR data;
		UCHAR parity;
		UCHAR stop;
	} paramline;

	// byte length
	bytelength = lcr & 0x3;
	bytelength += 5;
	paramline.data = bytelength;

	// parity
	parity = lcr & 0x38;
	parity = parity >> 3;
	switch (parity) {
	case 0x1:
		paramline.parity = 1;
		break;
	case 0x3:
		paramline.parity = 2;
		break;
	case 0x5:
		paramline.parity = 3;
		break;
	case 0x7:
		paramline.parity = 4;
		break;
	default:
		paramline.parity = 0;
		break;
	}

	// stopbits
	if (lcr & 0x4) {
		if (bytelength == 5)
			paramline.stop = 1;
		else
			paramline.stop = 2;
	} else {
		paramline.stop = 0;
	}


	ulParmLen = sizeof(paramline);
	rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_SETLINECTRL, &paramline, ulParmLen, &ulParmLen, 0, 0, 0);
	if ( rc != NO_ERROR)
	{
		LOG_MSG ("Serial port at 0x%x: API did not like the new values.", base);
	}

}

void CDirectSerial::updateMSR () {
	Bit8u newmsr = 0;
	UCHAR dptr = 0;
	ULONG ulParmLen = sizeof(dptr);

	APIRET rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETMODEMINPUT, &dptr, ulParmLen, &ulParmLen, 0, 0, 0);
	if (rc != NO_ERROR) {
#ifdef SERIALPORT_DEBUGMSG
//		LOG_MSG ("Serial port at %x: GetCommModemStatus failed!", base);
#endif
		//return;
	}
	if (dptr & 16)
		newmsr |= MSR_CTS_MASK;
	if (dptr & 32)
		newmsr |= MSR_DSR_MASK;
	if (dptr & 64)
		newmsr |= MSR_RI_MASK;
	if (dptr & 128)
		newmsr |= MSR_CD_MASK;
	changeMSR (newmsr);
}

void CDirectSerial::transmitByte (Bit8u val) {
	// mean bug: with break = 1, WriteFile will never return.
	ULONG bytesWritten = 0;
	APIRET rc = DosWrite (hCom, &val, 1, &bytesWritten);
	if (rc == NO_ERROR && bytesWritten > 0) {
		ByteTransmitted ();
		//LOG_MSG("UART 0x%x: TX 0x%x", base,val);
	} else {
		LOG_MSG ("UART 0x%x: NO BYTE WRITTEN!", base);
	}
}

/*****************************************************************************/
/* setBreak(val) switches break on or off                                   **/
/*****************************************************************************/

void CDirectSerial::setBreak (bool value) {
	//#ifdef SERIALPORT_DEBUGMSG
	//LOG_MSG("UART 0x%x: Break toggeled: %d", base, value);
	//#endif
	USHORT error;
	ULONG ulParmLen = sizeof(error);
	if (value)
		DosDevIOCtl (hCom, IOCTL_ASYNC, ASYNC_SETBREAKON, 0,0,0, &error, ulParmLen, &ulParmLen);
	else
		DosDevIOCtl (hCom, IOCTL_ASYNC, ASYNC_SETBREAKOFF, 0,0,0, &error, ulParmLen, &ulParmLen);
}

/*****************************************************************************/
/* updateModemControlLines(mcr) sets DTR and RTS.                           **/
/*****************************************************************************/
void CDirectSerial::updateModemControlLines ( /*Bit8u mcr */ ) {
	bool change = false;
	DCBINFO dcb;
	ULONG ulParmLen = sizeof(dcb);

	DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETDCBINFO, 0, 0, 0, &dcb, ulParmLen, &ulParmLen);

		/*** DTR ***/
	if (CSerial::getDTR ()) {			// DTR on
		if (dcb.fbCtlHndShake && 3 == 0) { // DTR disabled
			dcb.fbCtlHndShake |= 1;
			change = true;
		}
	} else {
		if (dcb.fbCtlHndShake && 3 == 1) { // DTR enabled
			dcb.fbCtlHndShake &= ~3;
			change = true;
		}
	}
		/*** RTS ***/
	if (CSerial::getRTS ()) {			// RTS on
		if (dcb.fbFlowReplace && 192 == 0) { //RTS disabled
			dcb.fbFlowReplace |= 64;
			change = true;
		}
	} else {
		if (dcb.fbFlowReplace && 192 == 1) { // RTS enabled
			dcb.fbFlowReplace &= ~192;
			change = true;
		}
	}
	if (change)
		DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_SETDCBINFO, &dcb, ulParmLen, &ulParmLen, 0, 0, 0);
}

void CDirectSerial::Timer2(void) {
	ULONG dwRead = 0;
	USHORT errors = 0;
	Bit8u chRead = 0;
	ULONG ulParmLen = sizeof(errors);


	if (lastChance == 0) {		// lastChance = 0
		if (CanReceiveByte ()) {
			if (DosRead (hCom, &chRead, 1, &dwRead)) {
				if (dwRead)
					receiveByte (chRead);
			}
		} else {
			if (DosRead (hCom, &chRead, 1, &dwRead)) {
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
			DosRead (hCom, &chRead, 1, &dwRead);
		}
	} else {			// lastChance>0    // already one waiting
		if (CanReceiveByte ()) {		// chance used
			receiveByte (ChanceChar);
			lastChance = 0;
		} else
			lastChance++;
	}

	// check for errors
	Bit8u errreg = 0;
	APIRET rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETCOMMERROR, 0, 0, 0, &errors, ulParmLen, &ulParmLen);
	if (rc != NO_ERROR && errors)
	{
		if (errors & 8) {
			LOG_MSG ("Serial port at 0x%x: line error: framing error.", base);
			errreg |= LSR_FRAMING_ERROR_MASK;
		}
		if (errors & 4) {
			LOG_MSG ("Serial port at 0x%x: line error: parity error.", base);
			errreg |= LSR_PARITY_ERROR_MASK;
		}
	}
	errors = 0;
	rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETCOMMEVENT, 0, 0, 0, &errors, ulParmLen, &ulParmLen);
	if (rc != NO_ERROR && errors)
	{
		if (errors & 6) {
			LOG_MSG ("Serial port at 0x%x: line error: break received.", base);
			errreg |= LSR_RX_BREAK_MASK;
		}
	}
	if (errreg != 0)
	{
		receiveError (errreg);
	}
	// update Modem input line states
	updateMSR ();
}

#endif
#endif
