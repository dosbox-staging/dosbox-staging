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
	TODO
	Implement 16-bit dma
*/

#include <string.h>
#include "dosbox.h"
#include "mem.h"
#include "inout.h"
#include "dma.h"
#include "pic.h"

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
		bool address_decrement;
		bool autoinit_enable;
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
	DMA_EnableCallBack enable_callback;
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
		break;
	case 0x01:case 0x03:case 0x05:case 0x07:
		if (cont->flipflop) {
			ret=(Bit8u)((chan->current_count-1) & 0xff);
		} else {
			ret=(Bit8u)(((chan->current_count-1)>>8)&0xff);
		}
		cont->flipflop=!cont->flipflop;
		break;
	case 0x08:	/* Read Status */
		ret=cont->status_reg;
		cont->status_reg&=0xf;	/* Clear lower 4 bits on read */
		break;

	default:
		LOG_WARN("DMA:Unhandled read from %d",port);
	}
	return ret;
}


static void write_dma(Bit32u port,Bit8u val) {
	/* only use first dma for now */
	DMA_CONTROLLER * cont=&dma[0];
	DMA_CHANNEL * chan;
	switch (port) {
	case 0x00:case 0x02:case 0x04:case 0x06:
		chan=&cont->chan[port>>1];
		if (cont->flipflop) {
			chan->base_address=(chan->base_address & 0xff00) | val;
		} else {
			chan->base_address=(chan->base_address & 0x00ff) | (val<<8);
		}
		cont->flipflop=!cont->flipflop;
		chan->addr_changed=true;
		break;
	case 0x01:case 0x03:case 0x05:case 0x07:
		chan=&cont->chan[port>>1];
		if (cont->flipflop) {
			chan->base_count=(chan->base_count & 0xff00) | val;
		} else {
			chan->base_count=(chan->base_count & 0x00ff) | (val<<8);
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
			Bitu channel = val & 0x03;
			cont->status_reg |= (1 << (channel+4));
		} else {
			Bitu channel = val & 0x03;
			cont->status_reg &= ~(1 << (channel+4));
		}
		break;
	case 0x0a:	/* single mask bit register */
		chan=&cont->chan[val & 0x3];
		chan->masked=(val & 4)>0;
		if (chan->enable_callback) chan->enable_callback(!chan->masked);
		break;
	case 0x0b:	/* mode register */
		chan=&cont->chan[val & 0x3];
		chan->mode.mode_type = (val >> 6) & 0x03;
		chan->mode.address_decrement = (val & 0x20) > 0;
 		chan->mode.autoinit_enable = (val & 0x10) > 0;
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


void write_dma_page(Bit32u port,Bit8u val) {
	Bitu channel;
	switch (port) {
    case 0x81: /* dma0 page register, channel 2 */
		channel=2;break;
    case 0x82: /* dma0 page register, channel 3 */
		channel=3;break;
    case 0x83: /* dma0 page register, channel 1 */
		channel=1;break;
    case 0x87: /* dma0 page register, channel 0 */
		channel=0;break;
	}
	dma[0].chan[channel].page=val;
	dma[0].chan[channel].addr_changed=true;
}


INLINE void ResetDMA8(DMA_CHANNEL * chan) {
	chan->addr_changed=false;
	chan->address=(chan->page << 16)+chan->base_address;
	chan->current_count=chan->base_count+1;
	chan->current_address=chan->base_address;
}



Bitu DMA_8_Read(Bitu dmachan,Bit8u * buffer,Bitu count) {
	DMA_CHANNEL * chan=&dma[0].chan[dmachan];
	if (chan->masked || !count) return 0;
	if (chan->addr_changed) ResetDMA8(chan);
	if (chan->current_count>count) {
		MEM_BlockRead(chan->address,buffer,count);
		chan->address+=count;
		chan->current_address+=count;
		chan->current_count-=count;
		return count;
	} else {
		/* Copy remaining piece of first buffer */
		MEM_BlockRead(chan->address,buffer,chan->current_count);
		if (!chan->mode.autoinit_enable) {
			/* Set the end of counter bit */
			dma[0].status_reg|=(1 << dmachan);
			count=chan->current_count;
			chan->current_address+=count;;
			chan->current_count=0;
			return count;
		} else {
			buffer+=chan->current_count;
			Bitu left=count-(Bit16u)chan->current_count;
			/* Autoinit reset the dma channel */
			ResetDMA8(chan);
			/* Copy the rest of the buffer */
			MEM_BlockRead(chan->address,buffer,left);
			chan->address+=left;
			chan->current_address+=left;
			chan->current_count-=left;
			return count;
		} 
	}
}

Bitu DMA_8_Write(Bitu dmachan,Bit8u * buffer,Bitu count) {
	DMA_CHANNEL * chan=&dma[0].chan[dmachan];
	if (chan->masked || !count) return 0;
	if (chan->addr_changed) ResetDMA8(chan);
	if (chan->current_count>count) {
		MEM_BlockWrite(chan->address,buffer,count);
		chan->address+=count;
		chan->current_address+=count;
		chan->current_count-=count;
		return count;
	} else {
		/* Copy remaining piece of first buffer */
		MEM_BlockWrite(chan->address,buffer,chan->current_count);
		if (!chan->mode.autoinit_enable) {
			/* Set the end of counter bit */
			dma[0].status_reg|=(1 << dmachan);
			count=chan->current_count;
			chan->current_address+=count;;
			chan->current_count=0;
			return count;
		} else {
			buffer+=chan->current_count;
			Bitu left=count-(Bit16u)chan->current_count;
			/* Autoinit reset the dma channel */
			ResetDMA8(chan);
			/* Copy the rest of the buffer */
			MEM_BlockWrite(chan->address,buffer,left);
			chan->address+=left;
			chan->current_address+=left;
			chan->current_count-=left;
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




void DMA_SetEnableCallBack(Bitu channel,DMA_EnableCallBack callback) {
	if (channel<4) {
		dma[0].chan[channel].enable_callback=callback;
		if (callback) callback(!dma[0].chan[channel].masked);
		return;
	}
	if (channel<8) {
		dma[1].chan[channel-4].enable_callback=callback;
		if (callback) callback(!dma[1].chan[channel-4].masked);
	}
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
