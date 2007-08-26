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

/* $Id: directserial_posix.cpp,v 1.2 2007-08-26 17:19:46 qbix79 Exp $ */

#include "dosbox.h"

#if C_DIRECTSERIAL

// Posix version
#if defined (LINUX) || defined (MACOSX)

#include "serialport.h"
#include "directserial_posix.h"
#include "pic.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>

/* This is a serial passthrough class.  Its amazingly simple to */
/* write now that the serial ports themselves were abstracted out */

CDirectSerial::CDirectSerial (Bitu id, CommandLine* cmd)
					:CSerial (id, cmd) {
	InstallationSuccessful = false;

	rx_retry = 0;
	rx_retry_max = 0;

	std::string prefix="/dev/";
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
		if(!(rx_retry_max<=10000)) rx_retry_max=0;
	}

	const char* tmpchar=prefix.c_str();

	LOG_MSG ("Serial%d: Opening %s", COMNUMBER, tmpchar);
	
	fileHandle = open (tmpchar, O_RDWR | O_NOCTTY | O_NONBLOCK);
	
	if (fileHandle < 0) {
		LOG_MSG ("Serial%d: Serial Port \"%s\" could not be opened.",
			COMNUMBER, tmpchar);
		if (errno == 2) {
			LOG_MSG ("The specified port does not exist.");
		} else if (errno == EBUSY) {
			LOG_MSG ("The specified port is already in use.");
		} else {
			LOG_MSG ("Errno %d occurred.", errno);
		}
		return;
	}

	int result = tcgetattr(fileHandle, &termInfo);

	
	if (result==-1) {
		// Handle the error.
		LOG_MSG ("tcgetattr failed with error %d.\n", errno);
		return;
	}

	// save it here to restore in destructor
	tcgetattr(fileHandle,&backup);

	// initialize the port
	termInfo.c_cflag = CS8 | CREAD | CLOCAL; // noparity, 1 stopbit
	termInfo.c_iflag = PARMRK | INPCK;
	termInfo.c_oflag = 0;
	termInfo.c_lflag = 0;

	cfsetospeed (&termInfo, B9600);
	cfsetispeed (&termInfo, B9600);

	termInfo.c_cc[VMIN] = 0;
	termInfo.c_cc[VTIME] = 0;

	tcflush (fileHandle, TCIFLUSH);
	tcsetattr (fileHandle, TCSANOW, &termInfo);

	//initialise base class
	CSerial::Init_Registers();
	InstallationSuccessful = true;
	receiveblock=false;

	setEvent(SERIAL_POLLING_EVENT, 1); // millisecond tick
}

CDirectSerial::~CDirectSerial () {
	if (fileHandle >= 0) 
	{
		tcsetattr(fileHandle, TCSANOW, &backup);
		close(fileHandle);
	}
	// We do not use own events so we don't have to clear them.
}

void CDirectSerial::handleUpperEvent(Bit16u type) {
	
	switch(type) {
		case SERIAL_POLLING_EVENT: {
			setEvent(SERIAL_POLLING_EVENT, 1);
			if(!receiveblock) {
				if(((!(LSR&LSR_RX_DATA_READY_MASK)) || rx_retry>=rx_retry_max ))
				{
					ReadCharacter();
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
			receiveblock=false;
			// check if there is something to receive
			if(((!(LSR&LSR_RX_DATA_READY_MASK)) || rx_retry>=rx_retry_max ))
			{
				ReadCharacter();
			} else rx_retry++;
			break;
		}
		case SERIAL_TX_EVENT: {
			if(!receiveblock) {
				if(((!(LSR&LSR_RX_DATA_READY_MASK)) || rx_retry>=rx_retry_max ))
				{
					ReadCharacter();
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

void CDirectSerial::ReadCharacter()
{
	Bit8u chRead = 0;
	int dwRead = 0;
	rx_retry=0;

	dwRead=read(fileHandle,&chRead,1);
	if (dwRead==1) {
		if(chRead==0xff) // error escape
		{
			dwRead=read(fileHandle,&chRead,1);
			if(chRead==0x00) // an error 
			{
				dwRead=read(fileHandle,&chRead,1);
				if(chRead==0x0)receiveError(LSR_RX_BREAK_MASK);
				else receiveError(LSR_PARITY_ERROR_MASK);
			}
		}
		receiveByte (chRead);
		setEvent(40, bytetime-0.03f); // receive timing
		receiveblock=true;
	}
}

void CDirectSerial::CheckErrors() {

}

/*****************************************************************************/
/* updatePortConfig is called when emulated app changes the serial port     **/
/* parameters baudrate, stopbits, number of databits, parity.               **/
/*****************************************************************************/
void CDirectSerial::updatePortConfig (Bit16u divider, Bit8u lcr) {
	Bit8u parity = 0;
	Bit8u bytelength = 0;
	int baudrate=0;
	
	// baud
	termInfo.c_cflag = CREAD | CLOCAL;

	if (divider == 0x1)	baudrate = B115200;
	else if (divider == 0x2) baudrate = B57600;
	else if (divider == 0x3) baudrate = B38400;
	else if (divider == 0x6) baudrate = B19200;
	else if (divider == 0xc) baudrate = B9600;
	else if (divider == 0x18) baudrate = B4800;
	else if (divider == 0x30) baudrate = B2400;
	else if (divider == 0x60) baudrate = B1200;
	else if (divider == 0xc0) baudrate = B600;
	else if (divider == 0x180) baudrate = B300;
	else if (divider == 0x417) baudrate = B110;

	// Don't think termios supports nonstandard baudrates
	else baudrate = B9600;

	// byte length
	bytelength = lcr & 0x3;
	bytelength += 5;

	switch (bytelength) {
	case 5:
		termInfo.c_cflag |= CS5;
		break;

	case 6:
		termInfo.c_cflag |= CS6;
		break;

	case 7:
		termInfo.c_cflag |= CS7;
		break;

	case 8:
	default:
		termInfo.c_cflag |= CS8;
		break;
	}

	// parity
	parity = lcr & 0x38;
	parity >>= 3;
	switch (parity) {
	case 0x1:
		termInfo.c_cflag |= PARODD;
		termInfo.c_cflag |= PARENB;
		break;
	case 0x3:
		termInfo.c_cflag |= PARENB;
		break;
	case 0x5:

// "works on many systems"
#define CMSPAR 010000000000

		termInfo.c_cflag |= PARODD;
		termInfo.c_cflag |= PARENB;
		termInfo.c_cflag |= CMSPAR;	
		//LOG_MSG("Serial%d: Mark parity not supported.", COMNUMBER);	
		break;
	case 0x7:
		termInfo.c_cflag |= PARENB;
		termInfo.c_cflag |= CMSPAR;
		//LOG_MSG("Serial%d: Space parity not supported.", COMNUMBER);	
		break;
	default: // no parity
		break;
	}

	// stopbits
	if (lcr & 0x4) termInfo.c_cflag |= CSTOPB;
	
	cfsetospeed (&termInfo, baudrate);
	cfsetispeed (&termInfo, baudrate);

	int retval = tcsetattr(fileHandle, TCSANOW, &termInfo);

	if(retval==-1)
		LOG_MSG ("Serial%d: Desired serial mode not supported", COMNUMBER);

}

void CDirectSerial::updateMSR () {
	long flags = 0;
	ioctl (fileHandle, TIOCMGET, &flags);

	if (flags & TIOCM_CTS) setCTS(true);
	else setCTS(false);

	if (flags & TIOCM_DSR) setDSR(true);
	else setDSR(false);

	if (flags & TIOCM_RI) setRI(true);
	else setRI(false);

	if (flags & TIOCM_CD) setCD(true);
	else setCD(false);
}

void CDirectSerial::transmitByte (Bit8u val, bool first) {
	if((LCR&LCR_BREAK_MASK) == 0) {
		
		int bytesWritten = write(fileHandle, &val, 1);
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
	if (value) ioctl (fileHandle, TIOCSBRK);
	else ioctl (fileHandle, TIOCCBRK);
}

/*****************************************************************************/
/* updateModemControlLines(mcr) sets DTR and RTS.                           **/
/*****************************************************************************/
void CDirectSerial::setRTSDTR(bool rts, bool dtr) {
	
	long setflags = 0;
	long clearflags = 0;
	
	if(rts) setflags |= TIOCM_RTS;
	else clearflags |= TIOCM_RTS;
	
	if(dtr) setflags |= TIOCM_DTR;
	else clearflags |= TIOCM_DTR;
	
	if(setflags) ioctl (fileHandle, TIOCMBIS, &setflags);
	if(clearflags) ioctl (fileHandle, TIOCMBIC, &clearflags);
}
void CDirectSerial::setRTS(bool val) {
	long flag = TIOCM_RTS;
	if(val) ioctl(fileHandle, TIOCMBIS, &flag);
	else ioctl(fileHandle, TIOCMBIC, &flag);
}
void CDirectSerial::setDTR(bool val) {
	long flag = TIOCM_DTR;
	if(val) ioctl(fileHandle, TIOCMBIS, &flag);
	else ioctl(fileHandle, TIOCMBIC, &flag);
}

#endif
#endif
