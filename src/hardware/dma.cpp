/*
 *  Copyright (C) 2002  The DOSBox Team
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

/*
	Still need to implement reads from dma ports :)
	Perhaps sometime also implement dma writes.
	DMA transfer that get setup with a size 0 are also done wrong should be 0x10000 probably.
	
*/

#include <string.h>
#include "dosbox.h"
#include "mem.h"
#include "inout.h"
#include "dma.h"

#if DEBUG_DMA
#define DMA_DEBUG LOG_DEBUG
#else
#define DMA_DEBUG
#endif

#define DMA_MODE_DEMAND  0
#define DMA_MODE_SINGLE  1
#define DMA_MODE_BLOCK   2
#define DMA_MODE_CASCADE 3

struct DMA_CHANNEL {
	struct {
		Bit8u mode_type;
		Bit8u address_decrement;
		Bit8u autoinit_enable;
		Bit8u transfer_type;
	} mode;
	Bit16u base_address;
	Bit16u base_count;
	Bit16u current_address;
	Bit32u current_count;
	Bit8u page;
	bool masked;
	PhysPt address;
	bool addr_changed;
};


struct DMA_CONTROLLER {
	bool flipflop;
	Bit8u status_reg;
	Bit8u command_reg;
	DMA_CHANNEL chan[4];
};

static DMA_CONTROLLER dma[2];
static Bit8u read_dma(Bit32u port) {
	/* only use first dma for now */
	DMA_CONTROLLER * cont=&dma[0];
	DMA_CHANNEL * chan=&cont->chan[port>>1];
	Bit8u ret;
	switch (port) {
	case 0x00:case 0x02:case 0x04:case 0x06:
		if (cont->flipflop) {
			ret=chan->current_address & 0xff;
		} else {
			ret=(chan->current_address>>8)&0xff;
		}
		cont->flipflop=!cont->flipflop;
	case 0x01:case 0x03:case 0x05:case 0x07:
		if (cont->flipflop) {
			ret=(chan->current_count-1) & 0xff;
		} else {
			ret=((chan->current_count-1)>>8)&0xff;
		}
		cont->flipflop=!cont->flipflop;
		break;
	default:
		LOG_WARN("DMA:Unhandled read from %d",port);
	}
	return ret;
}


static void write_dma(Bit32u port,Bit8u val) {
	/* only use first dma for now */
	DMA_CONTROLLER * cont=&dma[0];
	DMA_CHANNEL * chan=&cont->chan[port>>1];
	switch (port) {
	case 0x00:case 0x02:case 0x04:case 0x06:
		if (cont->flipflop) {
			chan->base_address=val;
		} else {
			chan->base_address|=(val<<8);
		}
		cont->flipflop=!cont->flipflop;
		chan->addr_changed=true;
		break;
	case 0x01:case 0x03:case 0x05:case 0x07:
		if (cont->flipflop) {
			chan->base_count=val;
		} else {
			chan->base_count|=(val<<8);
		}
		cont->flipflop=!cont->flipflop;
		chan->addr_changed=true;
		break;
	case 0x08:	/* Command Register */
		if (val != 4) LOG_WARN("DMA1:Illegal command %2X",val);
		cont->command_reg=val;
		break;
	case 0x09:	/* Request Register */
		if (val&4) {
			/* Set Request bit */
		} else {
			Bitu channel = val & 0x03;
			cont->status_reg &= ~(1 << (channel+4));
		}
		break;
	case 0x0a:	/* single mask bit register */
		chan->masked=(val & 4)>0;
		break;
	case 0x0b:	/* mode register */
		chan->mode.mode_type = (val >> 6) & 0x03;
		chan->mode.address_decrement = (val >> 5) & 0x01;
 		chan->mode.autoinit_enable = (val >> 4) & 0x01;
		chan->mode.transfer_type = (val >> 2) & 0x03;
		if (chan->mode.address_decrement) {
			LOG_WARN("DMA:Address Decrease not supported yet");
		}

		break;
	case 0x0c:	/* Clear Flip/Flip */
		cont->flipflop=true;
		break;
	};
};


static Bit8u channelindex[7] = {2, 3, 1, 0, 0, 0, 0};
void write_dma_page(Bit32u port,Bit8u val) {

	switch (port) {
    case 0x81: /* dma0 page register, channel 2 */
    case 0x82: /* dma0 page register, channel 3 */
    case 0x83: /* dma0 page register, channel 1 */
    case 0x87: /* dma0 page register, channel 0 */
		Bitu channel = channelindex[port - 0x81];
		dma[0].chan[channel].page=val;
		dma[0].chan[channel].addr_changed=true;
		break;
	}
}

Bit16u DMA_8_Read(Bit32u dmachan,Bit8u * buffer,Bit16u count) {
	DMA_CHANNEL * chan=&dma[0].chan[dmachan];

	if (chan->masked) return 0;
	if (!count) return 0;	
/* DMA always does autoinit should work under normal situations */
	if (chan->addr_changed) {
		/* Calculate the new starting position for dma read*/
		chan->addr_changed=false;
		chan->address=(chan->page << 16)+chan->base_address;

		chan->current_count=chan->base_count+1;
		chan->current_address=chan->base_address;
		DMA_DEBUG("DMA:Transfer from %d size %d",(chan->page << 16)+chan->base_address,chan->current_count);
	}
	if (chan->current_count>=count) {
		MEM_BlockRead(chan->address,buffer,count);
		chan->address+=count;
		chan->current_address+=count;
		chan->current_count-=count;
		return count;
	} else {
		/* Copy remaining piece of first buffer */
		MEM_BlockRead(chan->address,buffer,chan->current_count);
		buffer+=chan->current_count;
		count-=(Bit16u)chan->current_count;
		/* Autoinit reset the dma channel */
		chan->address=(chan->page << 16)+chan->base_address;
		chan->current_count=chan->base_count+1;
		chan->current_address=chan->base_address;
		/* Copy the rest of the buffer */
		MEM_BlockRead(chan->address,buffer,count);
		chan->address+=count;
		chan->current_address+=count;
		chan->current_count-=count;
		return count;
	}
};

Bit16u  DMA_8_Write(Bit32u dmachan,Bit8u * buffer,Bit16u count) {

	return 0;
};




Bit16u DMA_16_Read(Bit32u dmachan,Bit8u * buffer,Bit16u count) {

	return 0;
}

Bit16u DMA_16_Write(Bit32u dmachan,Bit8u * buffer,Bit16u count) {


	return 0;
}




void DMA_Init(Section* sec) {
	for (Bit32u i=0;i<0x10;i++) {
		IO_RegisterWriteHandler(i,write_dma,"DMA1");
		IO_RegisterReadHandler(i,read_dma,"DMA1");
	}
	IO_RegisterWriteHandler(0x81,write_dma_page,"DMA Pages");
	IO_RegisterWriteHandler(0x82,write_dma_page,"DMA Pages");
	IO_RegisterWriteHandler(0x83,write_dma_page,"DMA Pages");
	IO_RegisterWriteHandler(0x87,write_dma_page,"DMA Pages");
}
