/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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

/* $Id: dma.h,v 1.8 2003-11-26 08:38:23 qbix79 Exp $ */

#ifndef __DMA_H
#define __DMA_H

#include "mem.h"

#define DMA_MODE_DEMAND  0
#define DMA_MODE_SINGLE  1
#define DMA_MODE_BLOCK   2
#define DMA_MODE_CASCADE 3

#define DMA_BASEADDR	0
#define DMA_TRANSCOUNT	1
#define DMA_PAGEREG		2

#define DMA_CMDREG		0
#define DMA_MODEREG		1
#define DMA_CLEARREG	2
#define DMA_DMACREG		3
#define DMA_CLRMASKREG	4
#define DMA_SINGLEREG	5
#define DMA_WRITEALLREG	6

static Bit8u ChannelPorts [3][8] = { 0x00, 0x02, 0x04, 0x06, 0xff, 0xc4, 0xc8, 0xcc,
									 0x01, 0x03, 0x05, 0x07, 0xff, 0xc6, 0xca, 0xce,
									 0x87, 0x83, 0x81, 0x82, 0xff, 0x8b, 0x89, 0x8a };

static Bit8u ControllerPorts [2][7] = { 0x08, 0x0b, 0x0c, 0x0d, 0x0e, 0x0a, 0xf,
										0xd0, 0xd6, 0xd8, 0xda, 0xdc, 0xd4, 0xde };


typedef void (* DMA_EnableCallBack)(bool enable);

typedef void (* DMA_NewCallBack)(void *useChannel, bool tc);

void DMA_SetEnableCallBack(Bitu channel,DMA_EnableCallBack callback);

void DMA_CheckEnabled(void * usechan);

Bitu DMA_8_Read(Bitu channel,Bit8u * buffer,Bitu count);
Bitu DMA_8_Write(Bitu dmachan,Bit8u * buffer,Bitu count);

Bitu DMA_16_Read(Bitu channel,Bit8u * buffer,Bitu count);
Bitu DMA_16_Write(Bitu dmachan,Bit8u * buffer,Bitu count);

extern Bit8u read_dmaB(Bit32u port);
extern Bit16u read_dmaW(Bit32u port);

extern void write_dmaB(Bit32u port,Bit8u val);
extern void write_dmaW(Bit32u port,Bit16u val);

class DmaController {
public:
	bool flipflop;
	Bit8u ctrlnum;
public:

	DmaController(Bit8u num) {
		int i;
		for(i=0;i<7;i++) {
			IO_RegisterReadBHandler(ControllerPorts[num][i],read_dmaB);
			IO_RegisterReadWHandler(ControllerPorts[num][i],read_dmaW);

			IO_RegisterWriteBHandler(ControllerPorts[num][i],write_dmaB);
			IO_RegisterWriteWHandler(ControllerPorts[num][i],write_dmaW);
		}
		flipflop = true;
		ctrlnum = num;
	}

	Bit16u portRead(Bit32u port, bool eightbit);
	void portWrite(Bit32u port, Bit16u val, bool eightbit);

};



class DmaChannel {
public:
	
	Bit8u channum;
	Bit16u baseaddr;
	Bit16u current_addr;
	Bit16u pageaddr;
	PhysPt physaddr;
	PhysPt curraddr;
	Bit32s transcnt;
	Bit32s currcnt;
	DmaController *myController;
	bool DMA16;
	bool addr_changed;
public:
	Bit8u dmamode;
	bool dir;
	bool autoinit;
	Bit8u trantype;
	bool masked;
	bool enabled;
	DMA_EnableCallBack enable_callback;
	DMA_NewCallBack newcallback;

	DmaChannel(Bit8u num, DmaController *useController, bool sb) {
		int i;
		masked = true;
		enabled = false;
		enable_callback = NULL;
		newcallback = NULL;
		if(num == 4) return;
		addr_changed=false;	

		for(i=0;i<3;i++) {
			IO_RegisterReadBHandler(ChannelPorts[i][num],read_dmaB);
			IO_RegisterReadWHandler(ChannelPorts[i][num],read_dmaW);

			IO_RegisterWriteBHandler(ChannelPorts[i][num],write_dmaB);
			IO_RegisterWriteWHandler(ChannelPorts[i][num],write_dmaW);
		}
		myController = useController;
		channum = num;
		DMA16 = sb;
		baseaddr = 0;
		pageaddr = 0;
		physaddr = 0;
		curraddr = 0;
		transcnt = 0;
		currcnt = 0;
		dir = false;
		autoinit = false;
	}

	void RegisterCallback(DMA_NewCallBack useCallBack) { newcallback = useCallBack; }

	void reset(void) {
		addr_changed=false;
		curraddr = physaddr;
		currcnt = transcnt+1;
		current_addr = baseaddr;
		//LOG(LOG_DMA,LOG_NORMAL)("Setup at address %X:%X count %X",pageaddr,baseaddr,currcnt);
	}

	void MakeCallback(bool tc) {
		if (newcallback != NULL) {
			if(tc) {
				(*newcallback)(this, true);
			} else {
				if ((enabled) && (!masked) && (transcnt!=0)) {
					(*newcallback)(this, false);
				}
			}

		}
	}

	Bit32u Read(Bit32s requestsize, Bit8u * buffer);

	Bit32u Write(Bit32s requestsize, Bit8u * buffer);

	void calcPhys(void);

	Bit16u portRead(Bit32u port, bool eightbit);

	void portWrite(Bit32u port, Bit16u val, bool eightbit);

	// Notify channel when mask changes
	void Notify(void);

};


extern DmaChannel *DmaChannels[8];
extern DmaController *DmaControllers[2];



#endif

