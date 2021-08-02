/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_DMA_H
#define DOSBOX_DMA_H

#include "dosbox.h"

#include <cassert>
#include <functional>

#include "inout.h"
#include "support.h"

enum DMAEvent {
	DMA_REACHED_TC,
	DMA_MASKED,
	DMA_UNMASKED,
};

class DmaChannel;
using DMA_CallBack = std::function<void(DmaChannel *chan, DMAEvent event)>;

class DmaChannel {
public:
	Bit32u pagebase;
	Bit16u baseaddr;
	Bit32u curraddr;
	Bit16u basecnt;
	Bit16u currcnt;
	Bit8u channum;
	Bit8u pagenum;
	Bit8u DMA16;
	bool increment;
	bool autoinit;
//	Bit8u trantype; //Not used at the moment
	bool masked;
	bool tcount;
	bool request;
	DMA_CallBack callback;

	DmaChannel(uint8_t num, bool dma16);

	void DoCallBack(DMAEvent event) {
		if (callback)
			callback(this, event);
	}
	void SetMask(bool _mask) {
		masked=_mask;
		DoCallBack(masked ? DMA_MASKED : DMA_UNMASKED);
	}
	void Register_Callback(DMA_CallBack _cb) { 
		callback = _cb; 
		SetMask(masked);
		if (callback) Raise_Request();
		else Clear_Request();
	}
	void ReachedTC(void) {
		tcount=true;
		DoCallBack(DMA_REACHED_TC);
	}
	void SetPage(Bit8u val) {
		pagenum=val;
		pagebase=(pagenum >> DMA16) << (16+DMA16);
	}
	void Raise_Request(void) {
		request=true;
	}
	void Clear_Request(void) {
		request=false;
	}
	Bitu Read(Bitu size, Bit8u * buffer);
	Bitu Write(Bitu size, Bit8u * buffer);
};

class DmaController {
private:
	bool flipflop;
	DmaChannel *dma_channels[4];

public:
	IO_ReadHandleObject DMA_ReadHandler[0x12];
	IO_WriteHandleObject DMA_WriteHandler[0x12];

	DmaController(uint8_t ctrl) : flipflop(false)
	{
		assert(ctrl == 0 || ctrl == 1); // first or second DMA controller
		constexpr auto n = ARRAY_LEN(dma_channels);
		for (uint8_t i = 0; i < n; ++i)
			dma_channels[i] = new DmaChannel(i + ctrl * n, ctrl == 1);
	}

	DmaController(const DmaController &) = delete; // prevent copy
	DmaController &operator=(const DmaController &) = delete; // prevent assignment

	~DmaController()
	{
		for (auto *channel : dma_channels)
			delete channel;
	}

	DmaChannel *GetChannel(uint8_t chan) const
	{
		constexpr auto n = ARRAY_LEN(dma_channels);
		if (chan < n)
			return dma_channels[chan];
		else
			return nullptr;
	}

	void WriteControllerReg(io_port_t reg, uint16_t val, io_width_t width);
	uint16_t ReadControllerReg(io_port_t reg, io_width_t width);
};

DmaChannel * GetDMAChannel(Bit8u chan);

void CloseSecondDMAController(void);
bool SecondDMAControllerAvailable(void);

void DMA_SetWrapping(Bitu wrap);

#endif
