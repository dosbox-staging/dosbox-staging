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

#include <string.h>
#include <ctype.h>
#include "SDL_net.h"

#include "inout.h"
#include "mixer.h"
#include "dma.h"
#include "pic.h"
#include "hardware.h"
#include "setup.h"
#include "programs.h"
#include "debug.h"
#include "timer.h"
#include "callback.h"
#include "math.h"
#include "regs.h"
#include "serialport.h"

#define MODEMSPD 57600
#define CONNECTED (M_CTS | M_DSR | M_DCD )
#define DISCONNECTED (M_CTS | M_DSR )

/* DTMF tone generator */
float col[] = { 1209.0, 1336.0, 1477.0, 1633.0 };
float row[] = { 697.0, 770.0, 852.0, 941.0 };
char positions[] = "123A456B789C*0#D";
#define duration 1000
#define pause 400

static Bit8u tmpbuf[FIFO_SIZE+1];

struct ModemHd {
	char cmdbuf[FIFO_SIZE];
	bool commandmode;
	bool cantrans;
	bool incomingcall;
	bool autoanswer;
	bool echo;
	Bitu cmdpause;
	Bits ringcounter;
	Bit16u plusinc;
	Bit16u cmdpos;


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
	MIXER_Channel * chan;
};

static CSerial * mdm;
static ModemHd mhd;

static void sendStr(const char *usestr) {
	if (!mhd.echo) return;
	Bitu i=0;
	while (*usestr != 0) {
		if (*usestr == 10) {
			mdm->rx_addb(0xd);
			mdm->rx_addb(0xa);
		} else {
			mdm->rx_addb((Bit8u)*usestr);
		}
		usestr++;
	}
}

static void sendOK() {
	sendStr("\nOK\n");
}

static void sendError() {
	sendStr("\nERROR\n");
}

static void toUpcase(char *buffer) {
	Bitu i=0;
	while (buffer[i] != 0) {
		buffer[i] = toupper(buffer[i]);
		i++;
	}
}

static void openConnection() {
	if (mhd.socket) {
		LOG_MSG("Huh? already connected");
		SDLNet_TCP_DelSocket(mhd.socketset,mhd.socket);
		SDLNet_TCP_Close(mhd.socket);
	}
	mhd.socket = SDLNet_TCP_Open(&mhd.openip);
	if (mhd.socket) {
		SDLNet_TCP_AddSocket(mhd.socketset,mhd.socket);
		sendStr("\nCONNECT 57600\n");
		mhd.commandmode = false;
		mdm->setmodemstatus(CONNECTED);
	} else {
		sendStr("\nNO DIALTONE\n");
	}
}

static bool Dial(char * host) {
	
	/* Scan host for port */
	Bit16u port;
	char * hasport=strrchr(host,':');
	if (hasport) {
		*hasport++=0;
		port=(Bit16u)atoi(hasport);
	} else port=23;
	/* Resolve host we're gonna dial */
	LOG_MSG("host %s port %x",host,port);
	if (!SDLNet_ResolveHost(&mhd.openip,host,port)) {
		/* Prepare string for dial sound generator */
		int c;
		char *addrptr=host;
		mhd.dialstr[0] = 'd';
		mhd.dialstr[1] = 'd';
		mhd.dialstr[2] = 'd';
		mhd.dialstr[3] = 'd';
		mhd.dialstr[4] = 'd';
		mhd.dialstr[5] = 'p';
		c=6;
		while(*addrptr != 0x00) {
			if (strchr(positions, *addrptr)) {
				mhd.dialstr[c] = *addrptr;
				c++;
			}
			addrptr++;
		}
		mhd.dialstr[c] = 0x00;

		mhd.diallen = strlen(mhd.dialstr) * (Bit32u)(duration + pause);
		mhd.dialpos = 0;
		mhd.f1 = 0;
		mhd.f2 = 0;
		mhd.dialing = true;
		MIXER_Enable(mhd.chan,true);
		return true;
	} else {
		LOG_MSG("Failed to resolve host %s:%s",host,SDLNet_GetError());
		sendStr("\nNO CARRIER\n");
		return false;
	}
}


static void DoCommand() {
	bool found = false;
	bool foundat = false;
	char *foundstr;
	bool connResult = false;
	char msgbuf[4096];

	Bitu result;
	mhd.cmdbuf[mhd.cmdpos] = 0;
	toUpcase(mhd.cmdbuf);
	LOG_MSG("Modem Sent Command: %s\n", mhd.cmdbuf);
	mhd.cmdpos = 0;

	result = 0;
	

	/* Just for kicks */
	if ((mhd.cmdbuf[0] == 'A') && (mhd.cmdbuf[1] == 'T')) foundat = true;
	if (foundat) {
		if (strstr(mhd.cmdbuf,"I3")) {
			sendStr("\nDosBox Emulated Modem Firmware V1.00\n");
			result = 1;
		} 
		if (strstr(mhd.cmdbuf,"I4")) {
			sprintf(msgbuf, "\nModem compiled for DosBox version %s\n", VERSION);
			sendStr(msgbuf);
			result = 1;
		}
		if (strstr(mhd.cmdbuf,"S0=1")) {
			mhd.autoanswer = true;
		}
		if (strstr(mhd.cmdbuf,"S0=0")) {
			mhd.autoanswer = false;
		}

		if (strstr(mhd.cmdbuf,"E0")) {
			mhd.echo = false;
		}
		if (strstr(mhd.cmdbuf,"E1")) {
			mhd.echo = true;
		}

		if (strstr(mhd.cmdbuf,"ATH")) {
			/* Check if we're actually connected */
			if (mhd.socket) {
				sendStr("\nNO CARRIER\n");
				SDLNet_TCP_DelSocket(mhd.socketset,mhd.socket);
				SDLNet_TCP_Close(mhd.socket);
				mhd.socket=0;
				mdm->setmodemstatus(DISCONNECTED);
				mhd.commandmode = true;
				result = 3;
			} else result = 2;
		}
		if(strstr(mhd.cmdbuf,"ATO")) {
			/* Check for connection before switching to data mode */
			if (mhd.socket) {
				mhd.commandmode = false;
				result=3;
			} else {
				result=2;
			}
		}
		if(strstr(mhd.cmdbuf,"ATDT")) {
			foundstr = strstr(mhd.cmdbuf,"ATDT");
			foundstr+=4;
			/* Small protection against empty line */
			if (!foundstr[0]) {
				result=2;
			} else {
				connResult = Dial(foundstr);
				result=3;
			}
		}
		if(strstr(mhd.cmdbuf,"ATA")) {
 			if (mhd.incomingcall) {
				sendStr("\nCONNECT 57600\n");
				LOG_MSG("Connected!\n");
				MIXER_Enable(mhd.chan,false);
				mdm->setmodemstatus(CONNECTED);
				mhd.incomingcall = false;
				mhd.commandmode = false;
				SDLNet_TCP_AddSocket(mhd.socketset,mhd.socket);
				result = 3;
			} else {
				mhd.autoanswer = true;
				result = 3;
			}
		}
		if (result==0) result = 1;
	} else result=2;

	if (strlen(mhd.cmdbuf)<2) {
		if(!mhd.dialing) {
			result = 0;
			mhd.autoanswer = false;
		} else {
			MIXER_Enable(mhd.chan,false);
			mhd.dialing = false;
			sendStr("\nNO CARRIER\n");
			result = 0;
		}
	}

	switch (result) {
	case 1:
		sendOK();
		break;
	case 2:
		sendError();
		break;
	}
	
}


static void MC_Changed(Bitu new_mc) {
	LOG_MSG("DTR %d RTS %d",new_mc & 1,new_mc & 2);
	if (!(new_mc & 1) && mhd.socket) {
		sendStr("\nNO CARRIER\n");
		SDLNet_TCP_DelSocket(mhd.socketset,mhd.socket);
		SDLNet_TCP_Close(mhd.socket);
		mhd.socket=0;
		mhd.commandmode = true;
	}
	mdm->setmodemstatus(
		((new_mc & 1 ) ? M_DSR : 0) |
		((new_mc & 2) ? M_CTS : 0) |
		(mhd.socket ? M_DCD : 0)

		);	
}

static void MODEM_Hardware(void) {
	int result =0;
	unsigned long args = 1;
	bool sendbyte = true;
	Bitu usesize;
	Bit8u txval;

	/* Check for eventual break command */
	if (!mhd.commandmode) mhd.cmdpause++;
	/* Handle incoming data from serial port, read as much as available */
	Bitu tx_size=mdm->tx_size();
	while (tx_size--) {
		txval = mdm->tx_readb();
		if (mhd.commandmode) {
			if(txval != 0xd) {
				if(txval == 0x8) {
					if (mhd.cmdpos > 0) {
						--mhd.cmdpos;
					}
				} else {
					if (txval != '+') {
						if(mhd.cmdpos<FIFO_SIZE) {
							mhd.cmdbuf[mhd.cmdpos] = txval;
							mhd.cmdpos++;
						}
					}
				}
				if(txval != 0x10) {
					if (mhd.echo) mdm->rx_addb(txval);
				} else if (mhd.echo) {
					mdm->rx_addb(10);
					mdm->rx_addb(13);
				}
			} else {
				DoCommand();
			}
		} else {
			/* 1000 ticks have passed, can check for pause command */
			if (mhd.cmdpause > 1000) {
				if(txval == '+') {
					mhd.plusinc++;
					if(mhd.plusinc>=3) {
						LOG_MSG("Entering command mode");
						mhd.commandmode = true;
						sendStr("\nOK\n");
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
	/* Handle outgoing to the serial port */
	if(!mhd.commandmode && mhd.socket && mdm->rx_free() && SDLNet_SocketReady(mhd.socket)) {
		usesize = mdm->rx_free();
		result = SDLNet_TCP_Recv(mhd.socket, tmpbuf, usesize);
		if (result>0) {
			mdm->rx_adds(tmpbuf,result);
			mhd.cmdpause = 0;
		} else {
			/* Error close the socket and disconnect */
			mdm->setmodemstatus(DISCONNECTED);
			mhd.commandmode = true;
			sendStr("\nNO CARRIER\n");
			SDLNet_TCP_DelSocket(mhd.socketset,mhd.socket);
			SDLNet_TCP_Close(mhd.socket);
			mhd.socket=0;
		}
	}

	/* Check for incoming calls */
	if (!mhd.socket && !mhd.incomingcall && mhd.listensocket) {
		mhd.socket = SDLNet_TCP_Accept(mhd.listensocket);
		if (mhd.socket) {
			mhd.dialpos = 0;
			mhd.incomingcall = true;
			mhd.diallen = 12000;
			mhd.dialpos = 0;
//TODO Set ring in Modemstatus?
			sendStr("\nRING\n");
			MIXER_Enable(mhd.chan,true);
			mhd.ringcounter = 24000;
		}
	}

	if (mhd.incomingcall) {
		if (mhd.ringcounter <= 0) {
			if (mhd.autoanswer) {
				mhd.incomingcall = false;
				sendStr("\nCONNECT 57600\n");
				MIXER_Enable(mhd.chan,false);
				mdm->setmodemstatus(CONNECTED);
				mhd.incomingcall = false;
				mhd.commandmode = false;
				SDLNet_TCP_AddSocket(mhd.socketset,mhd.socket);
				return;
			}
			sendStr("\nRING\n");
			mhd.diallen = 12000;
			mhd.dialpos = 0;

			MIXER_Enable(mhd.chan,true);
			
			mhd.ringcounter = 3000; /* Ring every three seconds for accurate emulation */
			
		}
		if (mhd.ringcounter > 0) --mhd.ringcounter;

	}

}

static void MODEM_CallBack(Bit8u * stream,Bit32u len) {
	char *cp;
	float ci,ri;
	int innum, splitnum, quad, eighth, sixth, amp;
	Bit8u curchar;
	Bit32s buflen = (Bit32s)len;
	if(mhd.incomingcall) {
		
		if(mhd.dialpos>=mhd.diallen) {
			MIXER_Enable(mhd.chan,false);
			return;
		} else {
			quad = (mhd.diallen/14);
			eighth = quad / 2;
			sixth = eighth /2;

			while ((buflen>0) && (mhd.dialpos<mhd.diallen)) {
				innum = mhd.dialpos % quad;
				splitnum = innum % eighth;
				ci = 650;
				ri = 950;

				while (splitnum < eighth) {
					amp = (sixth - abs(sixth - splitnum)) * (0x4000/sixth); 
					

					*(Bit16s*)(stream) = (Bit16s)(sin(mhd.f1)*amp + (sin(mhd.f2)*amp));
					mhd.f1 += 6.28/8000.0*ri; 
					mhd.f2 += 6.28/8000.0*ci;
					--buflen;
					innum++;
					splitnum++;
					mhd.dialpos++;
					stream+=2;
					if(buflen<=0) return;
				}
				while (splitnum < quad) {
					*(Bit16s*)(stream) = 0;
					mhd.f1 = 0;
					mhd.f2 = 0;
					--buflen;
					innum++;
					splitnum++;
					mhd.dialpos++;
					stream+=2;
					if(buflen<=0) return;
				}
			}
		}
	}


	if(mhd.dialing) {
		if(mhd.dialpos>=mhd.diallen) {
			while(len-->0) {
				*(Bit16s*)(stream) = 0;
				stream+=2;
			}
			MIXER_Enable(mhd.chan,false);
			mhd.dialing = false;
			openConnection();
			return;
		} else {

			while ((buflen>0) && (mhd.dialpos<mhd.diallen)) {
				curchar = (Bit8u)(mhd.dialpos / (duration + pause));
				innum = mhd.dialpos % (duration + pause);

				switch(mhd.dialstr[curchar]) {
				case 'p':
					while(innum<pause) {
						*(Bit16s*)(stream) = 0;
						mhd.f1 = 0;
						mhd.f2 = 0;
						--buflen;
						innum++;
						mhd.dialpos++;
						stream+=2;
						if(buflen<=0) return;
					}
					mhd.dialpos+=duration;
					break;

				case 'd':
					/* Dialtone */
					ci = 350;
					ri = 440;
					while(innum<(pause+duration)) {
						*(Bit16s*)(stream) = (Bit16s)((sin(mhd.f1)*0x1fff) + (sin(mhd.f2)*0x1fff));
						mhd.f1 += 6.28/8000.0*ri; 
						mhd.f2 += 6.28/8000.0*ci;
						--buflen;
						innum++;
						mhd.dialpos++;
						stream+=2;
						if(buflen<=0) return;
					}
					break;

				default:
					cp = strchr(positions, mhd.dialstr[curchar]);
					
					ci = col[(cp - positions) % 4];
					ri = row[(cp - positions) / 4];
					while(innum<duration) {
						*(Bit16s*)(stream) = (Bit16s)((sin(mhd.f1)*0x3fff) + (sin(mhd.f2)*0x3fff));
						mhd.f1 += 6.28/8000.0*ri; 
						mhd.f2 += 6.28/8000.0*ci;
						--buflen;
						innum++;
						mhd.dialpos++;
						stream+=2;
						if(buflen<=0) return;
					}
					while(innum<(pause+duration)) {
						mhd.f1 = 0;
						mhd.f2 = 0;
						*(Bit16s*)(stream) = 0;
						--buflen;
						innum++;
						mhd.dialpos++;
						stream+=2;
						if(buflen<=0) return;

					}
					break;
				} 

			}
		}
	}
}

void MODEM_Init(Section* sec) {

	unsigned long args = 1;
	Section_prop * section=static_cast<Section_prop *>(sec);

	if(!section->Get_bool("modem")) return;

	if(SDLNet_Init()==-1) {
		LOG_MSG("SDLNet_Init failed: %s\n", SDLNet_GetError());
		return;
	}

	mhd.cmdpos = 0;
	mhd.commandmode = true;
	mhd.plusinc = 0;
	mhd.cantrans = false;
	mhd.incomingcall = false;
	mhd.autoanswer = false;
	mhd.cmdpause = 0;
	mhd.echo = true;

	/* Bind the modem to the correct serial port */
	mhd.comport=section->Get_int("comport");
	strcpy(mhd.remotestr, section->Get_string("remote"));
	mdm = getComport(mhd.comport);
	mdm->setmodemstatus(DISCONNECTED);
	mdm->SetMCHandler(&MC_Changed);

	TIMER_AddTickHandler(&MODEM_Hardware);

	/* Initialize the sockets and setup the listening port */
	mhd.socketset = SDLNet_AllocSocketSet(1);
	if (!mhd.socketset) {
		LOG_MSG("MODEM:Can't open socketset:%s",SDLNet_GetError());
//TODO Should probably just exit
		return;
	}
	mhd.socket=0;
	mhd.listenport=section->Get_int("listenport");
	if (mhd.listenport) {
		IPaddress listen_ip;
		SDLNet_ResolveHost(&listen_ip, NULL, mhd.listenport);
		mhd.listensocket=SDLNet_TCP_Open(&listen_ip);
		if (!mhd.listensocket) LOG_MSG("MODEM:Can't open listen port:%s",SDLNet_GetError());
	} else mhd.listensocket=0;

	mhd.chan=MIXER_AddChannel(&MODEM_CallBack,8000,"MODEM");
	MIXER_Enable(mhd.chan,false);
	MIXER_SetMode(mhd.chan,MIXER_16MONO);
}

#endif

