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

/*
	Based the port handling from the bochs dma code.
*/

#include <string.h>
#include "dosbox.h"
#include "mem.h"
#include "inout.h"
#include "dma.h"
#include "pic.h"

DmaChannel *DmaChannels[8];
DmaController *DmaControllers[2];


Bit16u DmaController::portRead(Bit32u port, bool eightbit) {
	LOG_MSG("Reading DMA controller at %x", port);
	return 0xffff;
}

void DmaController::portWrite(Bit32u port, Bit16u val, bool eightbit) {
	bool found;
	found = false;
	if(port == ControllerPorts[ctrlnum][DMA_CLRMASKREG]) {
		found = true;
		flipflop = true;
		// Disable DMA requests
		// Clear command and status registers
	}
	if(port == ControllerPorts[ctrlnum][DMA_SINGLEREG]) {
		found = true;
		int dmachan;
		dmachan = (ctrlnum * 2) + (val & 0x3);
		DmaChannels[dmachan]->masked = ((val & 0x4) == 0x4);
		DmaChannels[dmachan]->Notify();
	}
	if(port == ControllerPorts[ctrlnum][DMA_WRITEALLREG]) {
		found = true;
		int dmachan,i,r;
		dmachan = (ctrlnum * 2);
		r = 0;
		for(i=dmachan;i<dmachan+4;i++) {
			DmaChannels[i]->masked = (((val >> r) & 0x1) == 0x1);
			DmaChannels[i]->Notify();
			r++;
		}

	}
	if(port == ControllerPorts[ctrlnum][DMA_CLEARREG]) {
		found = true;
		flipflop = true;
	}
	if(port == ControllerPorts[ctrlnum][DMA_MODEREG]) {
		found = true;
		int dmachan;
		dmachan = (ctrlnum * 2) + (val & 0x3);
		DmaChannels[dmachan]->trantype = (val >> 2) & 0x3;
		DmaChannels[dmachan]->autoinit = ((val & 0x10) == 0x10);
		DmaChannels[dmachan]->dir = ((val & 0x20) == 0x20);
		DmaChannels[dmachan]->dmamode = (val >> 6) & 0x3;
		DmaChannels[dmachan]->Notify();
	}
	if(!found) LOG_MSG("Write to DMA port %x with %x", port, val);

}


Bit32u DmaChannel::Read(Bit32s requestsize, Bit8u * buffer) {
	Bit32s bytesread;
	bytesread = 0;
	if(autoinit) {
		while(requestsize>0) {
			if(currcnt>=requestsize) {
				MEM_BlockRead(curraddr,buffer,requestsize);
				curraddr+=requestsize;
				buffer+=requestsize;
				currcnt-=requestsize;
				bytesread+=requestsize;
				requestsize=0;
				break;
			} else {
				MEM_BlockRead(curraddr,buffer,currcnt);
				bytesread+=currcnt;
				buffer+=currcnt;
				requestsize-=currcnt;
				reset();
				MakeCallback(true);
			}
		}
		if(currcnt==0) {
			reset();
			MakeCallback(true);
		}
		return bytesread;

	} else {
		if(currcnt>=requestsize) {
			MEM_BlockRead(curraddr,buffer,requestsize);
			curraddr+=requestsize;
			buffer+=requestsize;
			currcnt-=requestsize;
			bytesread+=requestsize;
		} else {
			MEM_BlockRead(curraddr,buffer,currcnt);
			buffer+=currcnt;
			requestsize-=currcnt;
			bytesread+=currcnt;
			currcnt=0;
		}
	}
	if(currcnt==0) MakeCallback(true);
	return bytesread;
}

Bit32u DmaChannel::Write(Bit32s requestsize, Bit8u * buffer) {
	Bit32s byteswrite;
	byteswrite = 0;
	if(autoinit) {
		while(requestsize>0) {
			if(currcnt>=requestsize) {
				MEM_BlockWrite(curraddr,buffer,requestsize);
				curraddr+=requestsize;
				buffer+=requestsize;
				currcnt-=requestsize;
				byteswrite+=requestsize;
				requestsize=0;
				break;
			} else {
				MEM_BlockWrite(curraddr,buffer,currcnt);
				byteswrite+=currcnt;
				buffer+=currcnt;
				requestsize-=currcnt;
				reset();
				MakeCallback(true);
			}
		}
		if(currcnt==0) {
			reset();
			MakeCallback(true);
		}
		return byteswrite;

	} else {
		if(currcnt>=requestsize) {
			MEM_BlockWrite(curraddr,buffer,requestsize);
			curraddr+=requestsize;
			buffer+=requestsize;
			currcnt-=requestsize;
			byteswrite+=requestsize;
		} else {
			MEM_BlockWrite(curraddr,buffer,currcnt);
			buffer+=currcnt;
			requestsize-=currcnt;
			byteswrite+=currcnt;
			currcnt=0;
		}
	}
	if(currcnt==0) MakeCallback(true);
	return byteswrite;
}


void DmaChannel::calcPhys(void) {
	if (DMA16) {
		physaddr = (baseaddr << 1) | ((pageaddr >> 1) << 17);
	} else {
		physaddr = (baseaddr) | (pageaddr << 16);
	}
	curraddr = physaddr;
	current_addr = baseaddr;
}

#define ff myController->flipflop

Bit16u DmaChannel::portRead(Bit32u port, bool eightbit) {
	if (port == ChannelPorts[DMA_BASEADDR][channum]) {
		if(eightbit) {
			if(ff) {
				ff = !ff;
				return current_addr & 0xff;
			} else {
				ff = !ff;
				return current_addr >> 8;
			}
		} else {
			return current_addr;
		}
	}
	if (port == ChannelPorts[DMA_TRANSCOUNT][channum]) {
		if(eightbit) {
			if(ff) {
				ff = !ff;
				return (Bit8u)(currcnt-1);
			} else {
				ff = !ff;
				return (Bit8u)((currcnt-1) >> 8);
			}
		} else {
			return (Bit16u)currcnt;
		}
	}
	if (port == ChannelPorts[DMA_PAGEREG][channum]) return pageaddr;
	return 0xffff;
}

void DmaChannel::portWrite(Bit32u port, Bit16u val, bool eightbit) {
	if (port == ChannelPorts[DMA_BASEADDR][channum]) {
		if(eightbit) {
			if(ff) {
				baseaddr = (baseaddr & 0xff00) | (Bit8u)val;
			} else {
				baseaddr = (baseaddr & 0xff) | (val << 8);
			}
			ff = !ff;
		} else {
			baseaddr = val;
		}
		calcPhys();
		addr_changed = true;
	}
	if (port == ChannelPorts[DMA_TRANSCOUNT][channum]) {
		if(eightbit) {
			if(ff) {
				transcnt = (transcnt & 0xff00) | (Bit8u)val;
			} else {
				transcnt = (transcnt & 0xff) | (val << 8);
			}
			ff = !ff;
		} else {
			transcnt = val;
		}
		currcnt = transcnt+1;
		addr_changed = true;
		DMA_CheckEnabled(this);
		MakeCallback(false);
		
	}
	if (port == ChannelPorts[DMA_PAGEREG][channum]) {
		pageaddr = val;
		calcPhys();
		reset();
	}

}

#undef ff

// Notify channel when mask changes
void DmaChannel::Notify(void) {
	if(!masked) {
		DMA_CheckEnabled(this);
		MakeCallback(false);
	}
}


static Bit16u readDMAPorts(Bit32u port, bool eightbit) {
	int i,j;

	// Check for controller access
	for(i=0;i<2;i++) {
		for(j=0;j<7;j++) {
			if(ControllerPorts[i][j] == port) {
				return DmaControllers[i]->portRead(port, eightbit);
			}
		}
	}

	// Check for DMA access
	for(i=0;i<8;i++) {
		for(j=0;j<3;j++) {
			if(ChannelPorts[j][i] == port) {
				return DmaChannels[i]->portRead(port, eightbit);
			}
		}
	}

	LOG_MSG("Unmatched read port %x", port);

	return 0xffff;

}

static void writeDMAPorts(Bit32u port, Bit16u val, bool eightbit) {
	int i,j;

	// Check for controller access
	for(i=0;i<2;i++) {
		for(j=0;j<7;j++) {
			if(ControllerPorts[i][j] == port) {
				DmaControllers[i]->portWrite(port,val,eightbit);
				return;
			}
		}
	}

	// Check for DMA access
	for(i=0;i<8;i++) {
		for(j=0;j<3;j++) {
			if(ChannelPorts[j][i] == port) {
				DmaChannels[i]->portWrite(port,val,eightbit);
				return;
			}
		}
	}

	LOG_MSG("Unmatched write port %x - val %x", port, val);

}

Bit8u read_dmaB(Bit32u port) { return (Bit8u)readDMAPorts(port,true); }

Bit16u read_dmaW(Bit32u port) { 	return readDMAPorts(port,false); }

void write_dmaB(Bit32u port,Bit8u val) { writeDMAPorts(port,val,true); }

void write_dmaW(Bit32u port,Bit16u val) { writeDMAPorts(port,val,false); }


// Deprecated DMA read/write routines -- Keep compatibility with Sound Blaster
Bitu DMA_8_Read(Bitu dmachan,Bit8u * buffer,Bitu count) {
	DmaChannel *chan=DmaChannels[dmachan];
	
	if (chan->masked) return 0;
	if (!count) return 0;
	if (chan->addr_changed) chan->reset();

	if (chan->currcnt>(Bits)count) {
		MEM_BlockRead(chan->curraddr,buffer,count);
		chan->curraddr+=count;
		chan->current_addr+=count;
		chan->currcnt-=count;
		return count;
	} else {
		// Copy remaining piece of first buffer 
		MEM_BlockRead(chan->curraddr,buffer,chan->currcnt);
		if (!chan->autoinit) {
			// Set the end of counter bit 
			//dma[0].status_reg|=(1 << dmachan);
			count=chan->currcnt;
			chan->curraddr+=count;
			chan->current_addr+=count;
			chan->currcnt=0;
			chan->enabled=false;
			LOG(LOG_DMA,LOG_NORMAL)("8-bit Channel %d reached terminal count",chan->channum);
			return count;
		} else {
			buffer+=chan->currcnt;
			Bitu left=count-(Bit16u)chan->currcnt;
			// Autoinit reset the dma channel 
			chan->reset();
			// Copy the rest of the buffer
			MEM_BlockRead(chan->curraddr,buffer,left);
			chan->curraddr+=left;
			chan->current_addr+=left;
			chan->currcnt-=left;
			return count;
		} 
	}
}

Bitu DMA_8_Write(Bitu dmachan,Bit8u * buffer,Bitu count) {
	DmaChannel *chan=DmaChannels[dmachan];

	if (chan->masked) return 0;
	if (!count) return 0;
	if (chan->currcnt>(Bits)count) {
		MEM_BlockWrite(chan->curraddr,buffer,count);
		chan->curraddr+=count;
		chan->currcnt-=count;
		return count;
	} else {
		// Copy remaining piece of first buffer
		MEM_BlockWrite(chan->curraddr,buffer,chan->currcnt);
		if (!chan->autoinit) {
			// Set the end of counter bit 
			//dma[0].status_reg|=(1 << dmachan);
			count=chan->currcnt;
			chan->curraddr+=count;;
			chan->currcnt=0;
			return count;
		} else {
			buffer+=chan->currcnt;
			Bitu left=count-(Bit16u)chan->currcnt;
			// Autoinit reset the dma channel
			chan->reset();
			// Copy the rest of the buffer
			MEM_BlockWrite(chan->curraddr,buffer,left);
			chan->curraddr+=left;
			chan->currcnt-=left;
			return count;
		} 
	}
}


Bitu DMA_16_Read(Bitu dmachan,Bit8u * buffer,Bitu count) {

	return 0;
}

Bitu DMA_16_Write(Bitu dmachan,Bit8u * buffer,Bitu count) {


	return 0;
}

void DMA_SetEnabled(void * usechan,bool enabled) {
	DmaChannel * chan;
	chan = (DmaChannel *)usechan;

	if (chan->enabled == enabled) return;
	chan->enabled=enabled;
	if (chan->enable_callback) (*chan->enable_callback)(enabled);
}

void DMA_CheckEnabled(void * usechan) {
	DmaChannel * chan;
	chan = (DmaChannel *)usechan;

	bool enabled;
	if (chan->masked) enabled=false;
	else {
		if (chan->autoinit) enabled=true;
		else if (chan->currcnt) enabled=true;
		else enabled=false;
	}
	DMA_SetEnabled(chan,enabled);
}


void DMA_SetEnableCallBack(Bitu channel,DMA_EnableCallBack callback) {
	DmaChannel * chan;
	chan = DmaChannels[channel];
	chan->enabled=false;
	chan->enable_callback=callback;
	DMA_CheckEnabled(chan);
}


void DMA_Init(Section* sec) {
	
	Bitu i;

	DmaControllers[0] = new DmaController(0);
	DmaControllers[1] = new DmaController(1);

	for(i=0;i<4;i++) {
		DmaChannels[i] = new DmaChannel(i,DmaControllers[0],false);
	}
	for(i=4;i<8;i++) {
		DmaChannels[i] = new DmaChannel(i,DmaControllers[1],true);
	}

}


