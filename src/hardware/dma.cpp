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

#include <string.h>
#include "dosbox.h"
#include "mem.h"
#include "inout.h"
#include "dma.h"
#include "pic.h"
#include "paging.h"

DmaChannel *DmaChannels[8];
DmaController *DmaControllers[2];

static void DMA_BlockRead(PhysPt pt,void * data,Bitu size) {
	Bit8u * write=(Bit8u *) data;
	for ( ; size ; size--, pt++) {
		Bitu page = pt >> 12;
		if (page < LINK_START)
			page = paging.firstmb[page];
		*write++=phys_readb(page*4096 + (pt & 4095));
	}
}

static void DMA_BlockWrite(PhysPt pt,void * data,Bitu size) {
	Bit8u * read=(Bit8u *) data;
	for ( ; size ; size--, pt++) {
		Bitu page = pt >> 12;
		if (page < LINK_START)
			page = paging.firstmb[page];
		phys_writeb(page*4096 + (pt & 4095), *read++);
	}
}

static void DMA_WriteControllerReg(DmaController * cont,Bitu reg,Bitu val,Bitu len) {
	DmaChannel * chan;Bitu i;
	Bitu base=cont->chanbase;
	switch (reg) {
	case 0x0:case 0x2:case 0x4:case 0x6:
		chan=DmaChannels[base+(reg >> 1)];
		cont->flipflop=!cont->flipflop;
		if (cont->flipflop) {
			chan->baseaddr=(chan->baseaddr&0xff00)|val;
			chan->curraddr=(chan->curraddr&0xff00)|val;
		} else {
			chan->baseaddr=(chan->baseaddr&0x00ff)|(val << 8);
			chan->curraddr=(chan->curraddr&0x00ff)|(val << 8);
		}
		break;
	case 0x1:case 0x3:case 0x5:case 0x7:
		chan=DmaChannels[base+(reg >> 1)];
		cont->flipflop=!cont->flipflop;
		if (cont->flipflop) {
			chan->basecnt=(chan->basecnt&0xff00)|val;
			chan->currcnt=(chan->currcnt&0xff00)|val;
		} else {
			chan->basecnt=(chan->basecnt&0x00ff)|(val << 8);
			chan->currcnt=(chan->currcnt&0x00ff)|(val << 8);
		}
		break;
	case 0x8:		/* Comand reg not used */
		break;
	case 0x9:		/* Request registers, memory to memory */
		//TODO Warning?
		break;
	case 0xa:		/* Mask Register */
		chan=DmaChannels[base+(val & 3 )];
		chan->SetMask((val & 0x4)>0);
		break;
	case 0xb:		/* Mode Register */
		chan=DmaChannels[base+(val & 3 )];
		chan->autoinit=(val & 0x10) > 0;
		chan->increment=(val & 0x20) > 0;
		//TODO Maybe other bits?
		break;
	case 0xc:		/* Clear Flip/Flip */
		cont->flipflop=false;
		break;
	case 0xd:		/* Master Clear/Reset */
		for (i=0;i<4;i++) {
			DmaChannels[base+i]->SetMask(true);
			DmaChannels[base+i]->tcount=false;
		}
		cont->flipflop=false;
		break;
	case 0xe:		/* Clear Mask register */		
		for (i=0;i<4;i++) {
			DmaChannels[base+i]->SetMask(false);
		}
		break;
	case 0xf:		/* Multiple Mask register */
		for (i=0;i<4;i++) {
			DmaChannels[base+i]->SetMask(val & 1);
			val>>=1;
		}
		break;
	}
}

static Bitu DMA_ReadControllerReg(DmaController * cont,Bitu reg,Bitu len) {
	DmaChannel * chan;Bitu i,ret;
	Bitu base=cont->chanbase;
	switch (reg) {
	case 0x0:case 0x2:case 0x4:case 0x6:
		chan=DmaChannels[base+(reg >> 1)];
		cont->flipflop=!cont->flipflop;
		if (cont->flipflop) {
			return chan->curraddr & 0xff;
		} else {
			return (chan->curraddr >> 8) & 0xff;
		}
	case 0x1:case 0x3:case 0x5:case 0x7:
		chan=DmaChannels[base+(reg >> 1)];
		cont->flipflop=!cont->flipflop;
		if (cont->flipflop) {
			return chan->currcnt & 0xff;
		} else {
			return (chan->currcnt >> 8) & 0xff;
		}
	case 0x8:
		ret=0;
		for (i=0;i<4;i++) {
			chan=DmaChannels[base+i];
			if (chan->tcount) ret|=1 << i;
			chan->tcount=false;
			if (chan->callback) ret|=1 << (i+4);
		}
		return ret;
	default:
		LOG_MSG("Trying to read undefined DMA port %x",reg);
		break;
	}
	return 0xffffffff;
}


static void DMA_Write_Port(Bitu port,Bitu val,Bitu iolen) {
	if (port<0x10) {
		DMA_WriteControllerReg(DmaControllers[0],port,val,1);
	} else if (port>=0xc0 && port <=0xdf) {
		DMA_WriteControllerReg(DmaControllers[1],(port-0xc0) >> 1,val,1);
	} else switch (port) {
		case 0x81:DmaChannels[2]->SetPage(val);break;
		case 0x82:DmaChannels[3]->SetPage(val);break;		
		case 0x83:DmaChannels[1]->SetPage(val);break;
		case 0x89:DmaChannels[6]->SetPage(val);break;
		case 0x8a:DmaChannels[7]->SetPage(val);break;		
		case 0x8b:DmaChannels[5]->SetPage(val);break;
	}
}

static Bitu DMA_Read_Port(Bitu port,Bitu iolen) {
	if (port<0x10) {
		return DMA_ReadControllerReg(DmaControllers[0],port,iolen);
	} else if (port>=0xc0 && port <=0xdf) {
		return DMA_ReadControllerReg(DmaControllers[1],(port-0xc0) >> 1,iolen);
	} else switch (port) {
		case 0x81:return DmaChannels[2]->pagenum;
		case 0x82:return DmaChannels[3]->pagenum;
		case 0x83:return DmaChannels[1]->pagenum;
		case 0x89:return DmaChannels[6]->pagenum;
		case 0x8a:return DmaChannels[7]->pagenum;
		case 0x8b:return DmaChannels[5]->pagenum;
	}
	return 0;
}

DmaChannel::DmaChannel(Bit8u num, bool dma16) {
		masked = true;
		callback = NULL;
		if(num == 4) return;
		channum = num;
		DMA16 = dma16 ? 0x1 : 0x0;
		pagenum = 0;
		pagebase = 0;
		baseaddr = 0;
		curraddr = 0;
		basecnt = 0;
		currcnt = 0;
		increment = true;
		autoinit = false;
		tcount = false;
}

Bitu DmaChannel::Read(Bitu want, Bit8u * buffer) {
	Bitu done=0;
again:
	Bitu left=(currcnt+1);
	if (want<left) {
		DMA_BlockRead(pagebase+(curraddr << DMA16),buffer,want << DMA16);
		done+=want;
		curraddr+=want;
		currcnt-=want;
	} else {
		DMA_BlockRead(pagebase+(curraddr << DMA16),buffer,left << DMA16);
		buffer+=left << DMA16;
		want-=left;
		done+=left;
		ReachedTC();
		if (autoinit) {
			currcnt=basecnt;
			curraddr=baseaddr;
			if (want) goto again;
		} else {
			curraddr+=left;
			currcnt=0xffff;
			masked=true;
			DoCallBack(DMA_TRANSFEREND);
		}
	}
	return done;
}
Bitu DmaChannel::Write(Bitu want, Bit8u * buffer) {
	Bitu done=0;
again:
	Bitu left=(currcnt+1);
	if (want<left) {
		DMA_BlockWrite(pagebase+(curraddr << DMA16),buffer,want << DMA16);
		done+=want;
		curraddr+=want;
		currcnt-=want;
	} else {
		DMA_BlockWrite(pagebase+(curraddr << DMA16),buffer,left << DMA16);
		buffer+=left << DMA16;
		want-=left;
		done+=left;
		ReachedTC();
		if (autoinit) {
			currcnt=basecnt;
			curraddr=baseaddr;
			if (want) goto again;
		} else {
			curraddr+=left;
			currcnt=0xffff;
			masked=true;
			DoCallBack(DMA_TRANSFEREND);
		}
	}
	return done;
}



void DMA_Init(Section* sec) {
	Bitu i;
	DmaControllers[0] = new DmaController(0);
	DmaControllers[1] = new DmaController(1);

	for(i=0;i<8;i++) {
		DmaChannels[i] = new DmaChannel(i,i>=4);
	}
	
	for (i=0;i<0x10;i++) {
		Bitu mask=IO_MB;
		if (i<8) mask|=IO_MW;
		IO_RegisterWriteHandler(i,DMA_Write_Port,mask);
		IO_RegisterReadHandler(i,DMA_Read_Port,mask);
		if (machine==MCH_VGA) {
			IO_RegisterWriteHandler(0xc0+i*2,DMA_Write_Port,mask);
			IO_RegisterReadHandler(0xc0+i*2,DMA_Read_Port,mask);
		}
	}
	IO_RegisterWriteHandler(0x81,DMA_Write_Port,IO_MB,3);
	IO_RegisterWriteHandler(0x89,DMA_Write_Port,IO_MB,3);

	IO_RegisterReadHandler(0x81,DMA_Read_Port,IO_MB,3);
	IO_RegisterReadHandler(0x89,DMA_Read_Port,IO_MB,3);
}


