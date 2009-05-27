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

/* $Id: directserial_os2.cpp,v 1.5 2009-05-27 09:15:41 qbix79 Exp $ */

#include "dosbox.h"

#if C_DIRECTSERIAL


#if defined(OS2)
#include "serialport.h"
#include "directserial_os2.h"
#include "misc_util.h"
#include "pic.h"

// OS/2 related headers
#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSPROCESS
#include <os2.h>

/* This is a serial passthrough class.  Its amazingly simple to */
/* write now that the serial ports themselves were abstracted out */

CDirectSerial::CDirectSerial (Bitu id, CommandLine *cmd)
	: CSerial(id, cmd) {
	InstallationSuccessful = false;


	rx_retry = 0;
	rx_retry_max = 0;

	std::string tmpstring;

	if (!cmd->FindStringBegin("realport:", tmpstring, false))
	{
		return;
	}
#if SERIAL_DEBUG
	if (dbg_modemcontrol)
	{
		fprintf(debugfp, "%12.3f Port type directserial realport %s\r\n", PIC_FullIndex(), tmpstring.c_str());
	}
#endif


	// rxdelay: How many milliseconds to wait before causing an
	// overflow when the application is unresponsive.
	if(getBituSubstring("rxdelay:", &rx_retry_max, cmd)) {
		if(!(rx_retry_max<=10000)) {
			rx_retry_max=0;
		}
	}

	const char* tmpchar=tmpstring.c_str();

	LOG_MSG ("Serial%d: Opening %s", COMNUMBER, tmpstring.c_str());

	ULONG ulAction = 0;
	APIRET rc = DosOpen((unsigned char*)tmpstring.c_str(), &hCom, &ulAction, 0L, FILE_NORMAL, FILE_OPEN,
			OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYNONE | OPEN_FLAGS_SEQUENTIAL, 0L);
	if (rc != NO_ERROR)
	{
		LOG_MSG ("Serial%d: Serial port \"%s\" could not be opened.", COMNUMBER, tmpstring.c_str());
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
		LOG_MSG("GetCommState failed with error %d.\n", rc);
		DosClose(hCom);
		hCom = 0;
		return;
	}

	dcb.usWriteTimeout = 0;
	dcb.usReadTimeout = 0; //65535;
	dcb.fbCtlHndShake = dcb.fbFlowReplace = 0;
	dcb.fbTimeout = 6;
	rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_SETDCBINFO, &dcb, ulParmLen, &ulParmLen, 0, 0, 0);
	if ( rc != NO_ERROR)
	{
		LOG_MSG("SetDCBInfo failed with error %d.\n", rc);
		DosClose(hCom);
		hCom = 0;
		return;
	}


	struct {
		ULONG baud;
		BYTE fraction;
	} setbaud;
	setbaud.baud = 9600;
	setbaud.fraction = 0;
	ulParmLen = sizeof(setbaud);
	rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_EXTSETBAUDRATE, &setbaud, ulParmLen, &ulParmLen, 0, 0, 0);
	if (rc != NO_ERROR)
	{
		LOG_MSG("ExtSetBaudrate failed with error %d.\n", rc);
		DosClose (hCom);
		hCom = 0;
		return;
}

	struct {
		UCHAR data;
		UCHAR parity;
		UCHAR stop;
	} paramline;

	// byte length
	paramline.data = 8;
	paramline.parity = 0;
	paramline.stop = 0;
	ulParmLen = sizeof(paramline);
	rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_SETLINECTRL, &paramline, ulParmLen, &ulParmLen, 0, 0, 0);
	if ( rc != NO_ERROR)
	{
		LOG_MSG ("SetLineCtrl failed with error %d.\n", rc);
		}

	CSerial::Init_Registers();
	InstallationSuccessful = true;
	receiveblock = false;

	// Clears comm errors
	USHORT errors = 0;
	ulParmLen = sizeof(errors);
	DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETCOMMERROR, 0, 0, 0, &errors, ulParmLen, &ulParmLen);
	setEvent(SERIAL_POLLING_EVENT, 1);
	}

CDirectSerial::~CDirectSerial () {
	if (hCom != 0)
		DosClose (hCom);
}



/*****************************************************************************/
/* updatePortConfig is called when emulated app changes the serial port     **/
/* parameters baudrate, stopbits, number of databits, parity.               **/
/*****************************************************************************/
void CDirectSerial::updatePortConfig (Bit16u divider, Bit8u lcr) {
	Bit8u parity = 0;
	Bit8u bytelength = 0;
	struct {
		ULONG baud;
		BYTE fraction;
	} setbaud;

	// baud
	if (divider <= 0x1)
		setbaud.baud = 115200;
	else if (divider <= 0x2)
		setbaud.baud = 57600;
	else if (divider <= 0x3)
		setbaud.baud = 38400;
	else if (divider <= 0x6)
		setbaud.baud = 19200;
	else if (divider <= 0xc)
		setbaud.baud = 9600;
	else if (divider <= 0x18)
		setbaud.baud = 4800;
	else if (divider <= 0x30)
		setbaud.baud = 2400;
	else if (divider <= 0x60)
		setbaud.baud = 1200;
	else if (divider <= 0xc0)
		setbaud.baud = 600;
	else if (divider <= 0x180)
		setbaud.baud = 300;
	else if (divider <= 0x417)
		setbaud.baud = 110;

	// I read that windows can handle nonstandard baudrates:
	else
		setbaud.baud = 115200 / divider;


	setbaud.fraction = 0;
	ULONG ulParmLen = sizeof(setbaud);
	APIRET rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_EXTSETBAUDRATE, &setbaud, ulParmLen, &ulParmLen, 0, 0, 0);
	if (rc != NO_ERROR)
	{
		LOG_MSG("Serial%d: Desired serial mode not supported (Baud: %d, %d, Error: %d)",
			COMNUMBER, setbaud.baud, divider, rc);
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


#ifdef SERIAL_DEBUG
	LOG_MSG("_____________________________________________________");
	LOG_MSG("Serial%d, new baud rate: %d", COMNUMBER, setbaud.baud);
	LOG_MSG("Serial%d: new bytelen: %d", COMNUMBER, paramline.data);
	LOG_MSG("Serial%d: new parity: %d", COMNUMBER, paramline.parity);
	LOG_MSG("Serial%d: new stopbits: %d", COMNUMBER, paramline.stop);
#endif

	ulParmLen = sizeof(paramline);
	rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_SETLINECTRL, &paramline, ulParmLen, &ulParmLen, 0, 0, 0);
	if ( rc != NO_ERROR)
	{
#ifdef SERIAL_DEBUG
		if (dbg_modemcontrol)
		{
			fprintf(debugfp, "%12.3f serial mode not supported: rate=%d, LCR=%x.\r\n", PIC_FullIndex(), setbaud.baud, lcr);
	}
#endif
		LOG_MSG("Serial%d: Desired serial mode not supported (%d,%d,%d,%d)",
			COMNUMBER, setbaud.baud, paramline.data, paramline.parity, lcr);
	}


}

void CDirectSerial::updateMSR () {
	Bit8u newmsr = 0;
	UCHAR dptr = 0;
	ULONG ulParmLen = sizeof(dptr);

	APIRET rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETMODEMINPUT, 0, 0, 0, &dptr, ulParmLen, &ulParmLen);
	if (rc != NO_ERROR) {
	     LOG_MSG ("Serial port at %x: GetModemInput failed with %d !", idnumber, dptr);
	}
	setCTS( (dptr & 16) != 0);
	setDSR( (dptr & 32) != 0);
	setRI( (dptr & 64) != 0);
	setCD( (dptr & 128) != 0);
}

void CDirectSerial::transmitByte (Bit8u val, bool first) {
	ULONG bytesWritten = 0;
	APIRET rc = DosWrite (hCom, &val, 1, &bytesWritten);
	if (rc == NO_ERROR && bytesWritten > 0) {
		//LOG_MSG("UART 0x%x: TX 0x%x", base,val);
	} else {
		LOG_MSG ("Serial%d: NO BYTE WRITTEN!", idnumber);
	}
	if (first)
	{
		setEvent(SERIAL_THR_EVENT, bytetime / 8);
	} else {
		setEvent(SERIAL_TX_EVENT, bytetime);
	}
}

/*****************************************************************************/
/* setBreak(val) switches break on or off                                   **/
/*****************************************************************************/

void CDirectSerial::setBreak (bool value) {
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
void CDirectSerial::setRTSDTR(bool rts, bool dtr)
{
	bool change = false;
	DCBINFO dcb;
	ULONG ulParmLen = sizeof(dcb);

	DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETDCBINFO, 0, 0, 0, &dcb, ulParmLen, &ulParmLen);

		/*** DTR ***/
	if (dtr) {			// DTR on
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
	if (rts) {			// RTS on
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

void CDirectSerial::setRTS(bool val)
{
	bool change = false;
	DCBINFO dcb;
	ULONG ulParmLen = sizeof(dcb);

	DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETDCBINFO, 0, 0, 0, &dcb, ulParmLen, &ulParmLen);

		/*** RTS ***/
	if (val) {			// RTS on
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

void CDirectSerial::setDTR(bool val)
{
	bool change = false;
	DCBINFO dcb;
	ULONG ulParmLen = sizeof(dcb);

	DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETDCBINFO, 0, 0, 0, &dcb, ulParmLen, &ulParmLen);

		/*** DTR ***/
	if (val) {			// DTR on
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
	if (change)
		DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_SETDCBINFO, &dcb, ulParmLen, &ulParmLen, 0, 0, 0);
	}


void CDirectSerial::handleUpperEvent(Bit16u type)
	{
		switch(type) {
		case SERIAL_POLLING_EVENT: {
			ULONG dwRead = 0;
			ULONG errors = 0;
			Bit8u chRead = 0;

			setEvent(SERIAL_POLLING_EVENT, 1);
			if(!receiveblock) {
				if(((!(LSR&LSR_RX_DATA_READY_MASK)) || rx_retry>=rx_retry_max ))
				{
					  rx_retry=0;
					  if (DosRead (hCom, &chRead, 1, &dwRead) == NO_ERROR) {
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
			ULONG dwRead = 0;
			Bit8u chRead = 0;
			receiveblock=false;
			// check if there is something to receive
			if(((!(LSR&LSR_RX_DATA_READY_MASK)) || rx_retry>=rx_retry_max ))
	{
				rx_retry=0;
				if (DosRead (hCom, &chRead, 1, &dwRead) == NO_ERROR) {
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
			ULONG dwRead = 0;
			Bit8u chRead = 0;
			if(!receiveblock) {
				if(((!(LSR&LSR_RX_DATA_READY_MASK)) || rx_retry>=rx_retry_max ))
	{
					  rx_retry=0;
					  if (DosRead (hCom, &chRead, 1, &dwRead) == NO_ERROR) {
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

	USHORT errors = 0, event = 0;
	ULONG ulParmLen = sizeof(errors);
	DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETCOMMEVENT, 0, 0, 0, &event, ulParmLen, &ulParmLen);
	if (event & (64 + 128) ) { // Break (Bit 6) or Frame or Parity (Bit 7) error
		Bit8u errreg = 0;
		if (event & 64) errreg |= LSR_RX_BREAK_MASK;
		if (event & 128) {
			DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETCOMMERROR, 0, 0, 0, &errors, ulParmLen, &ulParmLen);
			if (errors & 8) errreg |= LSR_FRAMING_ERROR_MASK;
			if (errors & 4) errreg |= LSR_PARITY_ERROR_MASK;
		}
		receiveError (errreg);
	}
}

#endif
#endif
