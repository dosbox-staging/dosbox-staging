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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: dma.h,v 1.12 2004-08-04 09:12:50 qbix79 Exp $ */

#ifndef __DMA_H
#define __DMA_H

enum DMAEvent {
	DMA_REACHED_TC,
	DMA_MASKED,
	DMA_UNMASKED,
	DMA_TRANSFEREND
};

class DmaChannel;
typedef void (* DMA_CallBack)(DmaChannel * chan,DMAEvent event);

class DmaController {
public:
	bool flipflop;
	Bit8u ctrlnum;
	Bit8u chanbase;
public:
	DmaController(Bit8u num) {
		flipflop = false;
		ctrlnum = num;
		chanbase = num * 4;
	}
};

class DmaChannel {
public:
	Bit32u pagebase;
	Bit16u baseaddr;
	Bit16u curraddr;
	Bit16u basecnt;
	Bit16u currcnt;
	Bit8u channum;
	Bit8u pagenum;
	Bit8u DMA16;
	bool increment;
	bool autoinit;
	Bit8u trantype;
	bool masked;
	bool tcount;
	DMA_CallBack callback;

	DmaChannel(Bit8u num, bool dma16);
	void DoCallBack(DMAEvent event) {
		if (callback)	(*callback)(this,event);
	}
	void SetMask(bool _mask) {
		masked=_mask;
		DoCallBack(masked ? DMA_MASKED : DMA_UNMASKED);
	}
	void Register_Callback(DMA_CallBack _cb) { 
		callback = _cb; 
		SetMask(masked);
	}
	void ReachedTC(void) {
		tcount=true;
		DoCallBack(DMA_REACHED_TC);
	}
	void SetPage(Bit8u val) {
		pagenum=val;
		pagebase=(pagenum >> DMA16) << (16+DMA16);
	}
	Bitu Read(Bitu size, Bit8u * buffer);
	Bitu Write(Bitu size, Bit8u * buffer);
};

extern DmaChannel *DmaChannels[8];
extern DmaController *DmaControllers[2];

#endif


