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

/* $Id: softmodem.cpp,v 1.3 2006-02-09 11:47:55 qbix79 Exp $ */

#include "dosbox.h"

#if C_MODEM

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "SDL_net.h"

#include "support.h"
#include "timer.h"
#include "serialport.h"
#include "softmodem.h"

//#include "mixer.h"


CSerialModem::CSerialModem(
		IO_ReadHandler* rh,
		IO_WriteHandler* wh,
		TIMER_TickHandler th,		   
		Bit16u baseAddr,
		Bit8u initIrq,
		Bit32u initBps,
		Bit8u bytesize,
		const char* parity,
		Bit8u stopbits,
		const char *remotestr,
		Bit16u lport)
		: CSerial(rh, wh, th, 
		baseAddr, initIrq, initBps, bytesize, parity, stopbits) {
	socket=0;
	incomingsocket=0;
	InstallTimerHandler(th);

	if(!SDLNetInited) {
        if(SDLNet_Init()==-1) {
			LOG_MSG("SDLNet_Init failed: %s\n", SDLNet_GetError());
			return;
		}
		SDLNetInited = true;
	}
	rqueue=new CFifo(MODEM_BUFFER_QUEUE_SIZE);
	tqueue=new CFifo(MODEM_BUFFER_QUEUE_SIZE);
	
	//cmdpos = 0;
	
	//plusinc = 0;
	//incomingsocket = 0;
//	answermode = false;
	//memset(&reg,0,sizeof(reg));
	//cmdpause = 0;
	//echo = true;
	//doresponse = true;
	//numericresponse = false;

	/* Default to direct null modem connection.  Telnet mode interprets IAC codes */
	telnetmode = false;

	/* Initialize the sockets and setup the listening port */
	socketset = SDLNet_AllocSocketSet(1);
	listensocketset = SDLNet_AllocSocketSet(1);
	if (!socketset || !listensocketset) {
		LOG_MSG("MODEM:Can't open socketset:%s",SDLNet_GetError());
		//TODO Should probably just exit
		return;
	}
		socket=0;
		listenport=lport;
		if (listenport) 	{
			IPaddress listen_ip;
			SDLNet_ResolveHost(&listen_ip, NULL, listenport);
			listensocket=SDLNet_TCP_Open(&listen_ip);
			if (!listensocket) LOG_MSG("MODEM:Can't open listen port: %s",SDLNet_GetError());
			
			else LOG_MSG("MODEM: Port listener installed at port %d",listenport);
			
		}
		else listensocket=0;

		// TODO: Fix dialtones if requested
		//mhd.chan=MIXER_AddChannel((MIXER_MixHandler)this->MODEM_CallBack,8000,"MODEM");
		//MIXER_Enable(mhd.chan,false);
		//MIXER_SetMode(mhd.chan,MIXER_16MONO);
		
		Reset();
		//EnterIdleState();
		CSerial::Init_Registers(initBps,bytesize,parity,stopbits);
	}

	CSerialModem::~CSerialModem() {
		if(socket) {
			SDLNet_TCP_DelSocket(socketset,socket);
			SDLNet_TCP_Close(socket);
		}

		if(listensocket) SDLNet_TCP_Close(listensocket);
		if(socketset) SDLNet_FreeSocketSet(socketset);

		delete rqueue;
		delete tqueue;
	}

void CSerialModem::SendLine(const char *line) {
	rqueue->addb(0xd);
	rqueue->addb(0xa);
	rqueue->adds((Bit8u *)line,strlen(line));
	rqueue->addb(0xd);
	rqueue->addb(0xa);
}

// only for numbers < 1000...
void CSerialModem::SendNumber(Bitu val) {
	rqueue->addb(0xd);
	rqueue->addb(0xa);
	
	rqueue->addb(val/100+'0');
	val = val%100;
	rqueue->addb(val/10+'0');
	val = val%10;
	rqueue->addb(val+'0');

	rqueue->addb(0xd);
	rqueue->addb(0xa);
}

void CSerialModem::SendRes(ResTypes response) {
	char * string;Bitu code;
	switch (response)
	{
		case ResNONE:		return;
		case ResOK:			string="OK"; code=0; break;
		case ResERROR:		string="ERROR"; code=4; break;
		case ResRING:		string="RING"; code=2; break;
		case ResNODIALTONE: string="NO DIALTONE"; code=6; break;
		case ResNOCARRIER:	string="NO CARRIER" ;code=3; break;
		case ResCONNECT:	string="CONNECT 57600"; code=1; break;
	}
	
	if(doresponse!=1) {
		if(doresponse==2 && (response==ResRING || 
			response == ResCONNECT || response==ResNOCARRIER)) return;
		if(numericresponse) SendNumber(code);
		else SendLine(string);

		//if(CSerial::CanReceiveByte())	// very fast response
		//	if(rqueue->inuse() && CSerial::getRTS())
		//	{ Bit8u rbyte =rqueue->getb();
		//		CSerial::receiveByte(rbyte);
		//	LOG_MSG("Modem: sending byte %2x back to UART2",rbyte);
		//	}

		LOG_MSG("Modem response: %s", string);
	}
}

void CSerialModem::openConnection(void) {
	if (socket) {
		LOG_MSG("Huh? already connected");
		SDLNet_TCP_DelSocket(socketset,socket);
		SDLNet_TCP_Close(socket);
	}
	socket = SDLNet_TCP_Open(&openip);
}

bool CSerialModem::Dial(char * host) {

	/* Scan host for port */
	Bit16u port;
	char * hasport=strrchr(host,':');
	if (hasport) {
		*hasport++=0;
		port=(Bit16u)atoi(hasport);
	}
	else port=MODEM_DEFAULT_PORT;
	/* Resolve host we're gonna dial */
	LOG_MSG("Connecting to host %s port %d",host,port);
	if (!SDLNet_ResolveHost(&openip,host,port)) {
		openConnection();
		EnterConnectedState();
		return true;
	} else {
		LOG_MSG("Failed to resolve host %s: %s",host,SDLNet_GetError());
		SendRes(ResNODIALTONE);
		EnterIdleState();
		return false;
	}
}

void CSerialModem::AcceptIncomingCall(void) {
//	assert(!socket);
	socket=incomingsocket;
	SDLNet_TCP_AddSocket(socketset,socket);
	incomingsocket = 0;
	EnterConnectedState();
}

Bitu CSerialModem::ScanNumber(char * & scan) {
	Bitu ret=0;
	while (char c=*scan) {
		if (c>='0' && c<='9') {
			ret*=10;
			ret+=c-'0';
			scan++;
		} else break;
	}
	return ret;
}

void CSerialModem::Reset(){
	EnterIdleState();
	cmdpos = 0;
	cmdbuf[0]=0;
	oldDTRstate = getDTR();

	plusinc = 0;
	incomingsocket = 0;
	memset(&reg,0,sizeof(reg));
	reg[MREG_AUTOANSWER_COUNT]=0;	// no autoanswer
	reg[MREG_RING_COUNT] = 1;
	reg[MREG_ESCAPE_CHAR]='+';
	reg[MREG_CR_CHAR]='\r';
	reg[MREG_LF_CHAR]='\n';
	reg[MREG_BACKSPACE_CHAR]='\b';

	cmdpause = 0;	
	echo = true;
	doresponse = 0;
	numericresponse = false;

	/* Default to direct null modem connection.  Telnet mode interprets IAC codes */
	telnetmode = false;
}

void CSerialModem::EnterIdleState(void){
	connected=false;
	ringing=false;
	txbufferfull=false;

	if(socket) {	// clear current socket
		SDLNet_TCP_DelSocket(socketset,socket);
		SDLNet_TCP_Close(socket);
		socket=0;
	}
	if(incomingsocket) {	// clear current incoming socket
		SDLNet_TCP_DelSocket(socketset,incomingsocket);
		SDLNet_TCP_Close(incomingsocket);
	}
	// get rid of everything
	while(incomingsocket=SDLNet_TCP_Accept(listensocket)) {
		SDLNet_TCP_DelSocket(socketset,incomingsocket);
		SDLNet_TCP_Close(incomingsocket);
	}
	incomingsocket=0;
	
	commandmode = true;
	CSerial::setCD(false);
	CSerial::setRI(false);
	tqueue->clear();
}

void CSerialModem::EnterConnectedState(void) {
	if(socket) {
		SDLNet_TCP_AddSocket(socketset,socket);
		SendRes(ResCONNECT);
		commandmode = false;
		memset(&telClient, 0, sizeof(telClient));
		connected = true;
		ringing = false;
		CSerial::setCD(true);
		CSerial::setRI(false);
	} else {
		SendRes(ResNOCARRIER);
		EnterIdleState();
	}
}

void CSerialModem::DoCommand() {
	cmdbuf[cmdpos] = 0;
	cmdpos = 0;			//Reset for next command
	upcase(cmdbuf);
	LOG_MSG("Command sent to modem: ->%s<-\n", cmdbuf);
		/* Check for empty line, stops dialing and autoanswer */
		if (!cmdbuf[0]) {
			reg[0]=0;	// autoanswer off
			return;
		//	} 
		//else {
				//MIXER_Enable(mhd.chan,false);
		//		dialing = false;
		//		SendRes(ResNOCARRIER);
		//		goto ret_none;
		//	}
		}
		/* AT command set interpretation */

		if ((cmdbuf[0] != 'A') || (cmdbuf[1] != 'T')) {
			SendRes(ResERROR);
			return;//goto ret_error;
		}
		
		if (strstr(cmdbuf,"NET0")) {
			telnetmode = false;
			SendRes(ResOK);
			return;
		}
		else if (strstr(cmdbuf,"NET1")) {
			telnetmode = true;
			SendRes(ResOK);
			return;
		}
		
		/* Check for dial command */
		if(strncmp(cmdbuf,"ATD3",3)==0) {
			char * foundstr=&cmdbuf[3];
			if (*foundstr=='T' || *foundstr=='P') foundstr++;
			/* Small protection against empty line */
			if (!foundstr[0]) {
				SendRes(ResERROR);//goto ret_error;
				return;
			}
			char* helper;
			// scan for and remove spaces; weird bug: with leading spaces in the string,
			// SDLNet_ResolveHost will return no error but not work anyway (win)
			while(foundstr[0]==' ') foundstr++;
			helper=foundstr;
			helper+=strlen(foundstr);
			while(helper[0]==' ') {
				helper[0]=0;
				helper--;
			}

			if (strlen(foundstr) >= 12) {
					// Check if supplied parameter only consists of digits
					bool isNum = true;
				for (Bitu i=0; i<strlen(foundstr); i++)
						if (foundstr[i] < '0' || foundstr[i] > '9')
							isNum = false;
				if (isNum) {
					// Parameter is a number with at least 12 digits => this cannot be a valid IP/name
					// Transform by adding dots
					char buffer[128];
					Bitu j = 0;
					for (Bitu i=0; i<strlen(foundstr); i++) {
							buffer[j++] = foundstr[i];
							// Add a dot after the third, sixth and ninth number
							if (i == 2 || i == 5 || i == 8)
								buffer[j++] = '.';
							// If the string is longer than 12 digits, interpret the rest as port
							if (i == 11 && strlen(foundstr)>12)
								buffer[j++] = ':';
						}
						buffer[j] = 0;
						foundstr = buffer;
					}
				}
			Dial(foundstr);
			return;
		}
		char * scanbuf;
		scanbuf=&cmdbuf[2];
		char chr;
		Bitu num;
		while (chr=*scanbuf++) {
			switch (chr) {
			case 'I':	//Some strings about firmware
				switch (num=ScanNumber(scanbuf)) {
				case 3:SendLine("DosBox Emulated Modem Firmware V1.00");break;
				case 4:SendLine("Modem compiled for DosBox version " VERSION);break;
				};break;
			case 'E':	//Echo on/off
				switch (num=ScanNumber(scanbuf)) {
				case 0:echo = false;break;
				case 1:echo = true;break;
				};break;
			case 'V':
				switch (num=ScanNumber(scanbuf)) {
				case 0:numericresponse = true;break;
				case 1:numericresponse = false;break;
				};break;
			case 'H':	//Hang up
				switch (num=ScanNumber(scanbuf)) {
				case 0:
					if (connected) {
						SendRes(ResNOCARRIER);
						EnterIdleState();
						return;//goto ret_none;
		}
					//Else return ok
				};break;
			case 'O':	//Return to data mode
				switch (num=ScanNumber(scanbuf))
				{
				case 0:
					if (socket) {
						commandmode = false;
						return;//goto ret_none;
					} else {
						SendRes(ResERROR);
						return;//goto ret_none;
					}
				};break;
			case 'T':	//Tone Dial
			case 'P':	//Pulse Dial
				break;
			case 'M':	//Monitor
			case 'L':	//Volume
				ScanNumber(scanbuf);
				break;
			case 'A':	//Answer call
				if (incomingsocket) {
					AcceptIncomingCall();
				} else {
					SendRes(ResERROR);
					return;//goto ret_none;
				}
				return;//goto ret_none;
			case 'Z':	//Reset and load profiles
			{
				// scan the number away, if any
				ScanNumber(scanbuf);
				if (socket) SendRes(ResNOCARRIER);
				Reset();
				break;
			}
			case ' ':	//Space just skip
				break;
			case 'Q':	// Response options
				{			// 0 = all on, 1 = all off,
							// 2 = no ring and no connect/carrier in answermode
					Bitu val = ScanNumber(scanbuf);	
					if(!(val>2)) {
						doresponse=val;
						break;
					} else {
						SendRes(ResERROR);
						return;
					}
				}
			case 'S':	//Registers	
			{
				Bitu index=ScanNumber(scanbuf);
				if(index>=SREGS) {
					SendRes(ResERROR);
					return; //goto ret_none;
				}
				
				while(scanbuf[0]==' ') scanbuf++;	// skip spaces
				
				if(scanbuf[0]=='=') {	// set register
					scanbuf++;
					while(scanbuf[0]==' ') scanbuf++;	// skip spaces
					Bitu val = ScanNumber(scanbuf);
					reg[index]=val;
					break;
				}
				else if(scanbuf[0]=='?') {	// get register
					SendNumber(reg[index]);
					scanbuf++;
					break;
				}
				//else LOG_MSG("print reg %d with %d",index,reg[index]);
			}
			break;
			case '&':
			{
				if(scanbuf[0]!=0) {
					char ch = scanbuf[0];
					//switch(scanbuf[0])		Maybe You want to implement it?
					scanbuf++;
					LOG_MSG("Modem: Unhandled command: &%c%d",ch,ScanNumber(scanbuf));
				} else {
					SendRes(ResERROR);
					return;
				}
			}
			break;

			default:
				LOG_MSG("Modem: Unhandled command: %c%d",chr,ScanNumber(scanbuf));
				}
			}
		
/*
		}

			if (strstr(mhd.cmdbuf,"NET0"))
			{
				telnetmode = false;
			}
			if (strstr(mhd.cmdbuf,"NET1"))
			{
				telnetmode = true;
			}
	#endif*/
	//ret_ok:
		SendRes(ResOK);
		return;
	//ret_error:
		//SendRes(ResERROR);
	//ret_none:
	//	return;

	}

void CSerialModem::TelnetEmulation(Bit8u * data, Bitu size) {
	Bitu i;
	Bit8u c;
	for(i=0;i<size;i++) {
		c = data[i];
		if(telClient.inIAC) {
			if(telClient.recCommand) {
				if((c != 0) && (c != 1) && (c != 3)) {
					LOG_MSG("MODEM: Unrecognized option %d", c);
					if(telClient.command>250) {
						/* Reject anything we don't recognize */
							tqueue->addb(0xff);
							tqueue->addb(252);
							tqueue->addb(c); /* We won't do crap! */
					}
				}
				switch(telClient.command) {
					case 251: /* Will */
						if(c == 0) telClient.binary[TEL_SERVER] = true;
						if(c == 1) telClient.echo[TEL_SERVER] = true;
						if(c == 3) telClient.supressGA[TEL_SERVER] = true;
						break;
					case 252: /* Won't */
						if(c == 0) telClient.binary[TEL_SERVER] = false;
						if(c == 1) telClient.echo[TEL_SERVER] = false;
						if(c == 3) telClient.supressGA[TEL_SERVER] = false;
						break;
					case 253: /* Do */
						if(c == 0) {
							telClient.binary[TEL_CLIENT] = true;
								tqueue->addb(0xff);
								tqueue->addb(251);
								tqueue->addb(0); /* Will do binary transfer */
						}
						if(c == 1) {
							telClient.echo[TEL_CLIENT] = false;
								tqueue->addb(0xff);
								tqueue->addb(252);
								tqueue->addb(1); /* Won't echo (too lazy) */
						}
						if(c == 3) {
							telClient.supressGA[TEL_CLIENT] = true;
								tqueue->addb(0xff);
								tqueue->addb(251);
								tqueue->addb(3); /* Will Suppress GA */
						}
						break;
					case 254: /* Don't */
						if(c == 0) {
							telClient.binary[TEL_CLIENT] = false;
								tqueue->addb(0xff);
								tqueue->addb(252);
								tqueue->addb(0); /* Won't do binary transfer */
						}
						if(c == 1) {
							telClient.echo[TEL_CLIENT] = false;
								tqueue->addb(0xff);
								tqueue->addb(252);
								tqueue->addb(1); /* Won't echo (fine by me) */
						}
						if(c == 3) {
							telClient.supressGA[TEL_CLIENT] = true;
								tqueue->addb(0xff);
								tqueue->addb(251);
								tqueue->addb(3); /* Will Suppress GA (too lazy) */
						}
						break;
					default:
						LOG_MSG("MODEM: Telnet client sent IAC %d", telClient.command);
						break;
				}

				telClient.inIAC = false;
				telClient.recCommand = false;
				continue;

			} else {
				if(c==249) {
					/* Go Ahead received */
					telClient.inIAC = false;
					continue;
				}
				telClient.command = c;
				telClient.recCommand = true;
				
				if((telClient.binary[TEL_SERVER]) && (c == 0xff)) {
					/* Binary data with value of 255 */
					telClient.inIAC = false;
					telClient.recCommand = false;
						rqueue->addb(0xff);
					continue;
				}

			}
		} else {
			if(c == 0xff) {
				telClient.inIAC = true;
				continue;
			}
				rqueue->addb(c);
			}
		}
	}

void CSerialModem::Timer2(void) {
	int result =0;
	unsigned long args = 1;
	bool sendbyte = true;
	Bitu usesize;
	Bit8u txval;
	Bitu txbuffersize =0;
	Bitu testres = 0;

	// check for bytes to be sent to port
	if(CSerial::CanReceiveByte())
		if(rqueue->inuse() && CSerial::getRTS()) {
			Bit8u rbyte = rqueue->getb();
			//LOG_MSG("Modem: sending byte %2x back to UART3",rbyte);
			CSerial::receiveByte(rbyte);
		}
	/* Check for eventual break command */
	if (!commandmode) cmdpause++;
	/* Handle incoming data from serial port, read as much as available */
	//Bitu tx_size=tqueue->inuse();
	//Bitu tx_first = tx_size;	// TODO:comment out
	CSerial::setCTS(true);	// buffer will get 'emptier', new data can be received 
	while (/*tx_size--*/tqueue->inuse()) {
		txval = tqueue->getb();
		if (commandmode) {
			if (echo) {
				rqueue->addb(txval);
				//LOG_MSG("Echo back to queue: %x",txval);
			}
			if (txval==0xa) continue;		//Real modem doesn't seem to skip this?
			else if (txval==0x8 && (cmdpos > 0)) --cmdpos;	// backspace
			else if (txval==0xd) DoCommand();				// return
			else if (txval != '+') {
				if(cmdpos<99) {
					cmdbuf[cmdpos] = txval;
					cmdpos++;
				}
			}
		}
		else {// + character
			/* 1000 ticks have passed, can check for pause command */
			if (cmdpause > 1000) {
				if(txval ==/* '+')*/reg[MREG_ESCAPE_CHAR])
				{
					plusinc++;
					if(plusinc>=3) {
						LOG_MSG("Modem: Entering command mode(escape sequence)");
						commandmode = true;
						SendRes(ResOK);
						plusinc = 0;
					}
					sendbyte=false;
				} else {
					plusinc=0;
				}
	//If not a special pause command, should go for bigger blocks to send 
			}
			tmpbuf[txbuffersize] = txval;
			txbuffersize++;
		}
	} // while loop
	
	if (socket && sendbyte && txbuffersize) {
		// down here it saves a lot of network traffic
		SDLNet_TCP_Send(socket, tmpbuf,txbuffersize);
		//TODO error testing
	}
	SDLNet_CheckSockets(socketset,0);
	/* Handle incoming to the serial port */
	if(!commandmode && socket) {
		if(rqueue->left() && SDLNet_SocketReady(socket) /*&& CSerial::getRTS()*/)
		{
			usesize = rqueue->left();
			if (usesize>16) usesize=16;
			result = SDLNet_TCP_Recv(socket, tmpbuf, usesize);
			if (result>0) {
				if(telnetmode) {
					/* Filter telnet commands */
					TelnetEmulation(tmpbuf, result);
				} else {
					rqueue->adds(tmpbuf,result);
				}
				cmdpause = 0;
			} else {
				SendRes(ResNOCARRIER);
				EnterIdleState();
			}
		}
	}
	/* Check for incoming calls */
	if (!connected && !incomingsocket && listensocket) {
			incomingsocket = SDLNet_TCP_Accept(listensocket);
		if (incomingsocket) {
			SDLNet_TCP_AddSocket(listensocketset, incomingsocket);
				
			if(!CSerial::getDTR()) {
				// accept no calls with DTR off; TODO: AT &Dn
				EnterIdleState();
			} else {
				ringing=true;
				SendRes(ResRING);
				CSerial::setRI(!CSerial::getRI());
				//MIXER_Enable(mhd.chan,true);
				ringtimer = 3000;
				reg[1] = 0;		//Reset ring counter reg
			}
		}
	}
	if (ringing) {
		if (ringtimer <= 0) {
			reg[1]++;
			if ((reg[0]>0) && (reg[0]>=reg[1])) {
				AcceptIncomingCall();
				return;
			}
			SendRes(ResRING);
			CSerial::setRI(!CSerial::getRI());

			//MIXER_Enable(mhd.chan,true);
			ringtimer = 3000;
		}
		--ringtimer;
	}
}


//TODO
void CSerialModem::RXBufferEmpty() {
	// see if rqueue has some more bytes
	if(rqueue->inuse() && CSerial::getRTS()){
		Bit8u rbyte = rqueue->getb();
		//LOG_MSG("Modem: sending byte %2x back to UART1",rbyte);
		CSerial::receiveByte(rbyte);
		
		//CSerial::receiveByte(rqueue->getb());
	}
}

void CSerialModem::transmitByte(Bit8u val) {
	//LOG_MSG("MODEM: Byte %x to be transmitted",val);
	if(tqueue->left()) {
		tqueue->addb(val);
		if(!tqueue->left()) {
			CSerial::setCTS(false);
			txbufferfull=true;
		}
	} else LOG_MSG("MODEM: TX Buffer overflow!");
	CSerial::ByteTransmitted();	
}

void CSerialModem::updatePortConfig(Bit8u dll, Bit8u dlm, Bit8u lcr) { 
// nothing to do here right?
}

void CSerialModem::updateMSR() {
	// think it is not needed
}

void CSerialModem::setBreak(bool) {
	// TODO: handle this
}

void CSerialModem::updateModemControlLines(/*Bit8u mcr*/) {
	//if(!txbufferfull)
	//{
	//	if(CSerial::getRTS()) CSerial::setCTS(true);
	//	else CSerial::setCTS(false);
	//}
	
	// If DTR goes low, hang up.
	if(connected)
		if(oldDTRstate)
			if(!getDTR()) {
				SendRes(ResNOCARRIER);
				EnterIdleState();
				LOG_MSG("Modem: Hang up due to dropped DTR.");
			}

	oldDTRstate = getDTR();
}


#endif

