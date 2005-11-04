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

/* $Id: softmodem.h,v 1.3 2005-11-04 08:53:07 qbix79 Exp $ */

#ifndef DOSBOX_SERIALMODEM_H
#define DOSBOX_SERIALMODEM_H

#include "dosbox.h"
#if C_MODEM
#include "SDL_net.h"
#include "serialport.h"

#define MODEMSPD 57600
#define SREGS 100

//If it's too high you overflow terminal clients buffers i think
#define MODEM_BUFFER_QUEUE_SIZE 1024

#define MODEM_DEFAULT_PORT 23

enum ResTypes {
	ResNONE,
	ResOK,ResERROR,
	ResCONNECT,ResRING,
	ResBUSY,ResNODIALTONE,ResNOCARRIER,
};

#define TEL_CLIENT 0
#define TEL_SERVER 1

class CFifo {
public:
	CFifo(Bitu _size) {
		size=_size;
		pos=used=0;
		data=new Bit8u[size];
	}
	~CFifo() {
		delete[] data;
	}
	INLINE Bitu left(void) {
		return size-used;
	}
	INLINE Bitu inuse(void) {
		return used;
	}
	void clear(void) {
		used=pos=0;
	}

	void addb(Bit8u _val) {
		if(used>=size) {
			LOG_MSG("FIFO Overflow!");
			return;
		}
		//assert(used<size);
		Bitu where=pos+used;
		if (where>=size) where-=size;
		data[where]=_val;
		//LOG_MSG("+%x",_val);
		used++;
	}
	void adds(Bit8u * _str,Bitu _len) {
		if((used+_len)>size) {
			LOG_MSG("FIFO Overflow!");
			return;
		}
		
		//assert((used+_len)<=size);
		Bitu where=pos+used;
		used+=_len;
		while (_len--) {
			if (where>=size) where-=size;
			//LOG_MSG("+%x",*_str);
			data[where++]=*_str++;
		}
	}
	Bit8u getb(void) {
		if (!used) {
			LOG_MSG("MODEM: FIFO UNDERFLOW!");
			return data[pos];
		}
			Bitu where=pos;
		if (++pos>=size) pos-=size;
		used--;
		//LOG_MSG("-%x",data[where]);
		return data[where];
	}
	void gets(Bit8u * _str,Bitu _len) {
		if (!used) {
			LOG_MSG("MODEM: FIFO UNDERFLOW!");
			return;
		}
			//assert(used>=_len);
		used-=_len;
		while (_len--) {
			//LOG_MSG("-%x",data[pos]);
			*_str++=data[pos];
			if (++pos>=size) pos-=size;
		}
	}
private:
	Bit8u * data;
	Bitu size,pos,used;
	//Bit8u tmpbuf[MODEM_BUFFER_QUEUE_SIZE];
	
	
};
#define MREG_AUTOANSWER_COUNT 0
#define MREG_RING_COUNT 1
#define MREG_ESCAPE_CHAR 2
#define MREG_CR_CHAR 3
#define MREG_LF_CHAR 4
#define MREG_BACKSPACE_CHAR 5


class CSerialModem : public CSerial {
public:

	CFifo *rqueue;
	CFifo *tqueue;

	CSerialModem(
		IO_ReadHandler* rh,
		IO_WriteHandler* wh,
		TIMER_TickHandler th,
		Bit16u baseAddr,
		Bit8u initIrq,
		Bit32u initBps,
		Bit8u bytesize,
		const char* parity,
		Bit8u stopbits,

		const char *remotestr = NULL,
		Bit16u lport = 23);

	~CSerialModem();

	void Reset();

	void SendLine(const char *line);
	void SendRes(ResTypes response);
	void SendNumber(Bitu val);

	void EnterIdleState();
	void EnterConnectedState();

	void openConnection(void);
	bool Dial(char * host);
	void AcceptIncomingCall(void);
	Bitu ScanNumber(char * & scan);

	void DoCommand();
	
	void MC_Changed(Bitu new_mc);

	void TelnetEmulation(Bit8u * data, Bitu size);

	void Timer2(void);

	void RXBufferEmpty();

	void transmitByte(Bit8u val);
	void updatePortConfig(Bit8u dll, Bit8u dlm, Bit8u lcr);
	void updateMSR();

	void setBreak(bool);

	void updateModemControlLines(/*Bit8u mcr*/);

protected:
	char cmdbuf[255];
	bool commandmode;		// true: interpret input as commands
	bool echo;				// local echo on or off

	bool oldDTRstate;
	bool ringing;
	//bool response;
	bool numericresponse;	// true: send control response as number.
							// false: send text (i.e. NO DIALTONE)
	bool telnetmode;		// true: process IAC commands.
	
	bool connected;
	bool txbufferfull;
	Bitu doresponse;

	

	Bitu cmdpause;
	Bits ringtimer;
	Bits ringcount;
	Bitu plusinc;
	Bitu cmdpos;


	//Bit8u mctrl;
	Bit8u tmpbuf[MODEM_BUFFER_QUEUE_SIZE];

	Bitu listenport;
	Bit8u reg[SREGS];
	IPaddress openip;
	TCPsocket incomingsocket;
	TCPsocket socket;
	
	TCPsocket listensocket;
	SDLNet_SocketSet socketset;
	SDLNet_SocketSet listensocketset;

	struct {
		bool binary[2];
		bool echo[2];
		bool supressGA[2];
		bool timingMark[2];
					
		bool inIAC;
		bool recCommand;
		Bit8u command;
	} telClient;
	struct {
		bool active;
		double f1, f2;
		Bitu len,pos;
		char str[256];
	} dial;
};
#endif
#endif 
