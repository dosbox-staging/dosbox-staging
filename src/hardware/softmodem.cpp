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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dosbox.h"

#if C_MODEM

#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "SDL_net.h"

#include "inout.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"
#include "programs.h"
#include "debug.h"
#include "timer.h"
#include "callback.h"
#include "math.h"
#include "regs.h"
#include "serialport.h"

#define MODEMSPD 57600
#define SREGS 100


static Bit8u tmpbuf[QUEUE_SIZE];

struct ModemHd {
	char cmdbuf[QUEUE_SIZE];
	bool commandmode;
	bool answermode;
	bool echo,response,numericresponse;
	bool telnetmode;
	Bitu cmdpause;
	Bits ringtimer;
	Bits ringcount;
	Bitu plusinc;
	Bitu cmdpos;

	Bit8u reg[SREGS];
	TCPsocket incomingsocket;
	TCPsocket socket; 
	TCPsocket listensocket;
	SDLNet_SocketSet socketset;

	IPaddress openip;

	Bitu comport;
	Bitu listenport;

	char remotestr[4096];

	bool dialing;
	double f1, f2;
	Bitu diallen;
	Bitu dialpos;
	char dialstr[256];
	// TODO: Re-enable dialtons
	//MIXER_Channel * chan;
};

enum ResTypes {
	ResNONE,
	ResOK,ResERROR,
	ResCONNECT,ResRING,
	ResBUSY,ResNODIALTONE,ResNOCARRIER,
};

#define TEL_CLIENT 0
#define TEL_SERVER 1

struct telnetClient {
	bool binary[2];
	bool echo[2];
	bool supressGA[2];
	bool timingMark[2];

	bool inIAC;
	bool recCommand;
	Bit8u command;
};


#if 1

static void toUpcase(char *buffer) {
	Bitu i=0;
	while (buffer[i] != 0) {
		buffer[i] = toupper(buffer[i]);
		i++;
	}
}



class CSerialModem : public CSerial {
public:
	ModemHd mhd;

	CSerialModem(Bit16u baseAddr, Bit8u initIrq, Bit32u initBps, const char *remotestr = NULL, Bit16u lport = 27)
		: CSerial(baseAddr, initIrq, initBps)
	{
		
		
		mhd.cmdpos = 0;
		mhd.commandmode = true;
		mhd.plusinc = 0;
		mhd.incomingsocket = 0;
		mhd.answermode = false;
		memset(&mhd.reg,0,sizeof(mhd.reg));
		mhd.cmdpause = 0;
		mhd.echo = true;
		mhd.response = true;
		mhd.numericresponse = false;

		/* Default to direct null modem connection.  Telnet mode interprets IAC codes */
		mhd.telnetmode = false;

		/* Bind the modem to the correct serial port */
		//strcpy(mhd.remotestr, remotestr);

		/* Initialize the sockets and setup the listening port */
		mhd.socketset = SDLNet_AllocSocketSet(1);
		if (!mhd.socketset) {
			LOG_MSG("MODEM:Can't open socketset:%s",SDLNet_GetError());
	//TODO Should probably just exit
			return;
		}
		mhd.socket=0;
		mhd.listenport=lport;
		if (mhd.listenport) {
			IPaddress listen_ip;
			SDLNet_ResolveHost(&listen_ip, NULL, mhd.listenport);
			mhd.listensocket=SDLNet_TCP_Open(&listen_ip);
			if (!mhd.listensocket) LOG_MSG("MODEM:Can't open listen port:%s",SDLNet_GetError());
		} else mhd.listensocket=0;

		// TODO: Fix dialtones if requested
		//mhd.chan=MIXER_AddChannel((MIXER_MixHandler)this->MODEM_CallBack,8000,"MODEM");
		//MIXER_Enable(mhd.chan,false);
		//MIXER_SetMode(mhd.chan,MIXER_16MONO);

	}



	void SendLine(const char *line) {
		rqueue->addb(0xd);
		rqueue->addb(0xa);
		rqueue->adds((Bit8u *)line,strlen(line));
		rqueue->addb(0xd);
		rqueue->addb(0xa);

		}

	void SendRes(ResTypes response) {
		char * string;char * code;
		switch (response) {
		case ResNONE:
			return;
		case ResOK:string="OK";code="0";break;
		case ResERROR:string="ERROR";code="4";break;
		case ResRING:string="RING";code="2";
		case ResNODIALTONE:string="NO DIALTONE";code="6";break;
		case ResNOCARRIER:string="NO CARRIER";code="3";break;
		case ResCONNECT:string="CONNECT 57600";code="1";break;
	}

		rqueue->addb(0xd);rqueue->addb(0xa);
		rqueue->adds((Bit8u *)string,strlen(string));
		rqueue->addb(0xd);rqueue->addb(0xa);
	}

	void Send(Bit8u val) {
		tqueue->addb(val);
	}

	Bit8u Recv(Bit8u val) {
		return rqueue->getb();

	}

	void openConnection(void) {
	if (mhd.socket) {
		LOG_MSG("Huh? already connected");
		SDLNet_TCP_DelSocket(mhd.socketset,mhd.socket);
		SDLNet_TCP_Close(mhd.socket);
	}
		mhd.socket = SDLNet_TCP_Open(&openip);
	if (mhd.socket) {
		SDLNet_TCP_AddSocket(mhd.socketset,mhd.socket);
			SendRes(ResCONNECT);
		mhd.commandmode = false;
			memset(&telClient, 0, sizeof(telClient));
			updatemstatus();
	} else {
			SendRes(ResNODIALTONE);
		}
	}

	void updatemstatus(void) {
		Bit8u ms=0;
		//Check for data carrier, a connection that is
		if (mhd.incomingsocket) ms|=MS_RI;
		if (mhd.socket) ms|=MS_DCD;
		if (!mhd.commandmode) ms|=MS_DSR;
		//Check for DTR reply with DSR
	//	if (cport->mctrl & MC_DTR) ms|=MS_DSR;
		//Check for RTS reply with CTS
		if (mctrl & MC_RTS) ms|=MS_CTS;
		SetModemStatus(ms);
	}
	
	bool Dial(char * host) {
	/* Scan host for port */
	Bit16u port;
	char * hasport=strrchr(host,':');
	if (hasport) {
		*hasport++=0;
		port=(Bit16u)atoi(hasport);
	} else port=23;
	/* Resolve host we're gonna dial */
	LOG_MSG("host %s port %x",host,port);
		if (!SDLNet_ResolveHost(&openip,host,port)) {
			openConnection();
		return true;
	} else {
		LOG_MSG("Failed to resolve host %s:%s",host,SDLNet_GetError());
			SendRes(ResNOCARRIER);
		return false;
	}
		}

	void AcceptIncomingCall(void) {
		assert(!mhd.socket);
		mhd.socket=mhd.incomingsocket;
		SDLNet_TCP_AddSocket(mhd.socketset,mhd.socket);
		SendRes(ResCONNECT);
		LOG_MSG("Connected!\n");

		mhd.incomingsocket = 0;
		mhd.commandmode = false;
		}

	Bitu ScanNumber(char * & scan) {
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


	void HangUp(void) {
		SendRes(ResNOCARRIER);
				SDLNet_TCP_DelSocket(mhd.socketset,mhd.socket);
				SDLNet_TCP_Close(mhd.socket);
				mhd.socket=0;
				mhd.commandmode = true;
		updatemstatus();
		}

	void DoCommand() {
		mhd.cmdbuf[mhd.cmdpos] = 0;
		mhd.cmdpos = 0;			//Reset for next command
		toUpcase(mhd.cmdbuf);
		LOG_MSG("Modem Sent Command: %s\n", mhd.cmdbuf);
		/* Check for empty line, stops dialing and autoanswer */
		if (!mhd.cmdbuf[0]) {
			if(!mhd.dialing) {
				mhd.answermode = false;
				goto ret_none;
			} else {
				//MIXER_Enable(mhd.chan,false);
				mhd.dialing = false;
				SendRes(ResNOCARRIER);
				goto ret_none;
			}
		}
		/* AT command set interpretation */
		if ((mhd.cmdbuf[0] != 'A') || (mhd.cmdbuf[1] != 'T')) goto ret_error;
		/* Check for dial command */
		if(strncmp(mhd.cmdbuf,"ATD3",3)==0) {
			char * foundstr=&mhd.cmdbuf[3];
			if (*foundstr=='T' || *foundstr=='P') foundstr++;
			/* Small protection against empty line */
			if (!foundstr[0]) goto ret_error;
			if (strlen(foundstr) >= 12){
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
			goto ret_none;
			}
		char * scanbuf;
		scanbuf=&mhd.cmdbuf[2];char chr;Bitu num;
		while (chr=*scanbuf++) {
			switch (chr) {
			case 'I':	//Some strings about firmware
				switch (num=ScanNumber(scanbuf)) {
				case 3:SendLine("DosBox Emulated Modem Firmware V1.00");break;
				case 4:SendLine("Modem compiled for DosBox version " VERSION);break;
				};break;
			case 'E':	//Echo on/off
				switch (num=ScanNumber(scanbuf)) {
				case 0:mhd.echo = false;break;
				case 1:mhd.echo = true;break;
				};break;
			case 'V':
				switch (num=ScanNumber(scanbuf)) {
				case 0:mhd.numericresponse = true;break;
				case 1:mhd.numericresponse = false;break;
				};break;
			case 'H':	//Hang up
				switch (num=ScanNumber(scanbuf)) {
				case 0:
					if (mhd.socket) {
						HangUp();
						goto ret_none;
		}
					//Else return ok
				};break;
			case 'O':	//Return to data mode
				switch (num=ScanNumber(scanbuf)) {
				case 0:
					if (mhd.socket) {
				mhd.commandmode = false;
						goto ret_none;
			} else {
						goto ret_error;
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
				if (mhd.incomingsocket) {
					AcceptIncomingCall();
				} else {
					mhd.answermode = true;
				}
				goto ret_none;
			case 'Z':	//Reset and load profiles
				num=ScanNumber(scanbuf);
				break;
			case ' ':	//Space just skip
				break;
			case 'S':	//Registers	
			{
				Bitu index=ScanNumber(scanbuf);
				bool hasequal=(*scanbuf == '=');
				if (hasequal) scanbuf++;			
				Bitu val=ScanNumber(scanbuf);
				if (index>=SREGS) goto ret_error;
				if (hasequal) mhd.reg[index]=val;
				else LOG_MSG("print reg %d with %d",index,mhd.reg[index]);
			};break;
			default:
				LOG_MSG("Unhandled cmd %c%d",chr,ScanNumber(scanbuf));
				}
			}
	#if 0
			if (strstr(mhd.cmdbuf,"S0=1")) {
				mhd.autoanswer = true;
			}
			if (strstr(mhd.cmdbuf,"S0=0")) {
				mhd.autoanswer = false;
		}

			if (strstr(mhd.cmdbuf,"NET0")) {
				mhd.telnetmode = false;
		}
			if (strstr(mhd.cmdbuf,"NET1")) {
				mhd.telnetmode = true;
	}
	#endif
		LOG_MSG("Sending OK");
		SendRes(ResOK);
		return;
	ret_error:
		LOG_MSG("Sending ERROR");
		SendRes(ResERROR);
	ret_none:
		return;

	}
	
	void MC_Changed(Bitu new_mc) {
		LOG_MSG("New MC %x",new_mc );
		if (!(new_mc & 1) && mhd.socket) HangUp();
		updatemstatus();
	}

	void TelnetEmulation(Bit8u * data, Bitu size) {
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

	void Timer(void) {
	int result =0;
	unsigned long args = 1;
	bool sendbyte = true;
	Bitu usesize;
	Bit8u txval;

	/* Check for eventual break command */
	if (!mhd.commandmode) mhd.cmdpause++;
	/* Handle incoming data from serial port, read as much as available */
		Bitu tx_size=tqueue->inuse();
	while (tx_size--) {
			txval = tqueue->getb();
		if (mhd.commandmode) {
				if (mhd.echo) rqueue->addb(txval);
				if (txval==0xa) continue;		//Real modem doesn't seem to skip this?
				else if (txval==0x8 && (mhd.cmdpos > 0)) --mhd.cmdpos;
				else if (txval==0xd) DoCommand();
				else if (txval != '+') {
					if(mhd.cmdpos<QUEUE_SIZE) {
							mhd.cmdbuf[mhd.cmdpos] = txval;
							mhd.cmdpos++;
						}
					}
		} else {
			/* 1000 ticks have passed, can check for pause command */
			if (mhd.cmdpause > 1000) {
				if(txval == '+') {
					mhd.plusinc++;
					if(mhd.plusinc>=3) {
						LOG_MSG("Entering command mode");
						mhd.commandmode = true;
							SendRes(ResOK);
						mhd.plusinc = 0;
					}
					sendbyte=false;
				} else {
					mhd.plusinc=0;
				}
	//If not a special pause command, should go for bigger blocks to send 
			}
			
			tmpbuf[0] = txval;
			tmpbuf[1] = 0x0;

			if (mhd.socket && sendbyte) {
				SDLNet_TCP_Send(mhd.socket, tmpbuf,1);
				//TODO error testing
			}
		}
	} 

	SDLNet_CheckSockets(mhd.socketset,0);
		/* Handle incoming to the serial port */
		if(!mhd.commandmode && mhd.socket) {
			if(rqueue->left() && SDLNet_SocketReady(mhd.socket) && (mctrl & MC_RTS)) {
			usesize = rqueue->left();
			if (usesize>16) usesize=16;
		result = SDLNet_TCP_Recv(mhd.socket, tmpbuf, usesize);
		if (result>0) {
			if(mhd.telnetmode) {
				/* Filter telnet commands */
				TelnetEmulation(tmpbuf, result);
			} else {
					rqueue->adds(tmpbuf,result);
			}
			mhd.cmdpause = 0;
			} else HangUp();
		}
	}
	/* Check for incoming calls */
		if (!mhd.socket && !mhd.incomingsocket && mhd.listensocket) {
			mhd.incomingsocket = SDLNet_TCP_Accept(mhd.listensocket);
			if (mhd.incomingsocket) {
			mhd.diallen = 12000;
			mhd.dialpos = 0;
				SendRes(ResRING);
				//MIXER_Enable(mhd.chan,true);
				mhd.ringtimer = 3000;
				mhd.reg[1] = 0;		//Reset ring counter reg
		}
	}
		if (mhd.incomingsocket) {
			if (mhd.ringtimer <= 0) {
				mhd.reg[1]++;
				if (mhd.answermode || (mhd.reg[0]==mhd.reg[1])) {
					AcceptIncomingCall();
				return;
			}
				SendRes(ResRING);
			mhd.diallen = 12000;
			mhd.dialpos = 0;
				//MIXER_Enable(mhd.chan,true);
				mhd.ringtimer = 3000;
		}
			--mhd.ringtimer;
		}
		updatemstatus();
	}

	bool CanSend(void) {
		return true;
		}

	bool CanRecv(void) {
		return true;
	}


protected:
	char cmdbuf[QUEUE_SIZE];
	bool commandmode;
	bool answermode;
	bool echo;
	bool telnetmode;
	Bitu cmdpause;
	Bits ringtimer;
	Bits ringcount;
	Bitu plusinc;
	Bitu cmdpos;
		
	Bit8u reg[SREGS];
	IPaddress openip;
	TCPsocket incomingsocket;
	TCPsocket socket;
	TCPsocket listensocket;
	SDLNet_SocketSet socketset;

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
		MIXER_Channel * chan;
	} dial;
};

#endif 



CSerialModem *csm;


void MODEM_Init(Section* sec) {

	unsigned long args = 1;
	Section_prop * section=static_cast<Section_prop *>(sec);

	if(!section->Get_bool("modem")) return;

	if(SDLNet_Init()==-1) {
		LOG_MSG("SDLNet_Init failed: %s\n", SDLNet_GetError());
		return;
	}

	if(!SDLNetInited) {
		if(SDLNet_Init()==-1) {
			LOG_MSG("SDLNet_Init failed: %s\n", SDLNet_GetError());
			return;
		}
		SDLNetInited = true;
	}

	Bit16u comport = section->Get_int("comport");

	csm = NULL;

	switch (comport) {
		case 1:
			csm = new CSerialModem(0x3f0, 4, 57600, section->Get_string("remote"), section->Get_int("listenport"));
			break;
		case 2:
			csm = new CSerialModem(0x2f0, 3, 57600, section->Get_string("remote"), section->Get_int("listenport"));
			break;
		case 3:
			csm = new CSerialModem(0x3e0, 4, 57600, section->Get_string("remote"), section->Get_int("listenport"));
			break;
		case 4:
			csm = new CSerialModem(0x2e0, 3, 57600, section->Get_string("remote"), section->Get_int("listenport"));
			break;
		default:
			// Default to COM2
			csm = new CSerialModem(0x2f0, 3, 57600, section->Get_string("remote"), section->Get_int("listenport"));
			break;

	}

	if(csm != NULL) seriallist.push_back(csm);
}


#endif

