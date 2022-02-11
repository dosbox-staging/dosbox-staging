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

#include "dosbox.h"

#include <algorithm>
#include <string.h>
#include <memory>

#include "dma.h"
#include "mem.h"
#include "inout.h"
#include "pic.h"
#include "paging.h"
#include "setup.h"

DmaController *DmaControllers[2];

#define EMM_PAGEFRAME4K ((0xE000 * 16) / MEM_PAGESIZE)
Bit32u ems_board_mapping[LINK_START];

constexpr uint16_t NULL_PAGE = 0xffff;
static uint32_t dma_wrapping = NULL_PAGE; // initial value

static void UpdateEMSMapping(void) {
	/* if EMS is not present, this will result in a 1:1 mapping */
	Bitu i;
	for (i=0;i<0x10;i++) {
		ems_board_mapping[EMM_PAGEFRAME4K+i]=paging.firstmb[EMM_PAGEFRAME4K+i];
	}
}

// Generic function to read or write a block of data to or from memory.
// Don't use this directly; call two helpers: DMA_BlockRead or DMA_BlockWrite
static void perform_dma_io(const DMA_DIRECTION direction,
                           const PhysPt spage,
                           PhysPt mem_address,
                           void *data_start,
                           const size_t num_words,
                           const uint8_t is_dma16)
{
	assert(is_dma16 == 0 || is_dma16 == 1);

	const auto highpart_addr_page = spage >> 12;

	// Maybe move the mem_address into the 16-bit range
	mem_address <<= is_dma16;

	// The data pointer will be incremented per transfer
	auto data_pt = reinterpret_cast<uint8_t *>(data_start);

	// Convert from DMA 'words' to actual bytes, no greater than 64 KiB
	auto remaining_bytes = check_cast<uint16_t>(num_words << is_dma16);
	do {
		// Find the right EMS page that contains the current address
		auto page = highpart_addr_page + (mem_address >> 12);
		if (page < EMM_PAGEFRAME4K)
			page = paging.firstmb[page];
		else if (page < EMM_PAGEFRAME4K + 0x10)
			page = ems_board_mapping[page];
		else if (page < LINK_START)
			page = paging.firstmb[page];

		// Calculate the offset within the page
		const auto pos_in_page = mem_address & (MEM_PAGESIZE - 1);
		const auto bytes_to_page_end = check_cast<uint16_t>(MEM_PAGESIZE - pos_in_page);
		const auto chunk_start = check_cast<PhysPt>(page * MEM_PAGESIZE + pos_in_page);

		// Determine how many bytes to transfer within this page
		const auto chunk_bytes = std::min(remaining_bytes, bytes_to_page_end);

		// Copy the data from the page address into the data pointer
		if (direction == DMA_DIRECTION::READ)
			for (auto i = 0; i < chunk_bytes; ++i)
				data_pt[i] = phys_readb(chunk_start + i);

		// Copy the data from the data pointer into the page address
		else if (direction == DMA_DIRECTION::WRITE)
			for (auto i = 0; i < chunk_bytes; ++i)
				phys_writeb(chunk_start + i, data_pt[i]);

		mem_address += chunk_bytes;
		data_pt += chunk_bytes;
		remaining_bytes -= chunk_bytes;
	} while (remaining_bytes);
}

DmaChannel * GetDMAChannel(Bit8u chan) {
	if (chan<4) {
		/* channel on first DMA controller */
		if (DmaControllers[0]) return DmaControllers[0]->GetChannel(chan);
	} else if (chan<8) {
		/* channel on second DMA controller */
		if (DmaControllers[1]) return DmaControllers[1]->GetChannel(chan-4);
	}
	return NULL;
}

/* remove the second DMA controller (ports are removed automatically) */
void CloseSecondDMAController(void) {
	if (DmaControllers[1]) {
		delete DmaControllers[1];
		DmaControllers[1]=NULL;
	}
}

/* check availability of second DMA controller, needed for SB16 */
bool SecondDMAControllerAvailable(void) {
	if (DmaControllers[1]) return true;
	else return false;
}

static DmaChannel *GetChannelFromPort(const io_port_t port)
{
	uint8_t num = UINT8_MAX;
	switch (port) {
	/* read DMA page register */
	case 0x81: num = 2; break;
	case 0x82: num = 3; break;
	case 0x83: num = 1; break;
	case 0x87: num = 0; break;
	case 0x89: num = 6; break;
	case 0x8a: num = 7; break;
	case 0x8b: num = 5; break;
	case 0x8f: num = 4; break;
	default:
		LOG_WARNING("DMA: Attempted to lookup DMA channel from invalid port %04x",
		            port);
	}
	return GetDMAChannel(num);
}

static void DMA_Write_Port(io_port_t port, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint16_t>(value);
	// LOG(LOG_DMACONTROL,LOG_ERROR)("Write %" sBitfs(X) " %" sBitfs(X),port,val);
	if (port < 0x10) {
		/* write to the first DMA controller (channels 0-3) */
		DmaControllers[0]->WriteControllerReg(port, val, io_width_t::byte);
	} else if (port >= 0xc0 && port <= 0xdf) {
		/* write to the second DMA controller (channels 4-7) */
		DmaControllers[1]->WriteControllerReg((port - 0xc0) >> 1, val, io_width_t::byte);
	} else {
		UpdateEMSMapping();
		auto channel = GetChannelFromPort(port);
		if (channel)
			channel->SetPage(check_cast<uint8_t>(val));
	}
}

static uint16_t DMA_Read_Port(io_port_t port, io_width_t width)
{
	// LOG(LOG_DMACONTROL,LOG_ERROR)("Read %" sBitfs(X),port);
	if (port < 0x10) {
		/* read from the first DMA controller (channels 0-3) */
		return DmaControllers[0]->ReadControllerReg(port, width);
	} else if (port >= 0xc0 && port <= 0xdf) {
		/* read from the second DMA controller (channels 4-7) */
		return DmaControllers[1]->ReadControllerReg((port - 0xc0) >> 1, width);
	} else {
		const auto channel = GetChannelFromPort(port);
		if (channel)
			return channel->pagenum;
	}
	return 0;
}

void DmaController::WriteControllerReg(io_port_t reg, io_val_t value, io_width_t)
{
	auto val = check_cast<uint16_t>(value);
	DmaChannel *chan = nullptr;
	switch (reg) {
	/* set base address of DMA transfer (1st byte low part, 2nd byte high part) */
	case 0x0:case 0x2:case 0x4:case 0x6:
		UpdateEMSMapping();
		chan=GetChannel((Bit8u)(reg >> 1));
		flipflop=!flipflop;
		if (flipflop) {
			chan->baseaddr=(chan->baseaddr&0xff00)|val;
			chan->curraddr=(chan->curraddr&0xff00)|val;
		} else {
			chan->baseaddr=(chan->baseaddr&0x00ff)|(val << 8);
			chan->curraddr=(chan->curraddr&0x00ff)|(val << 8);
		}
		break;
	/* set DMA transfer count (1st byte low part, 2nd byte high part) */
	case 0x1:case 0x3:case 0x5:case 0x7:
		UpdateEMSMapping();
		chan=GetChannel((Bit8u)(reg >> 1));
		flipflop=!flipflop;
		if (flipflop) {
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
		if ((val & 0x4)==0) UpdateEMSMapping();
		chan=GetChannel(val & 3);
		chan->SetMask((val & 0x4)>0);
		break;
	case 0xb:		/* Mode Register */
		UpdateEMSMapping();
		chan=GetChannel(val & 3);
		chan->autoinit=(val & 0x10) > 0;
		chan->increment=(val & 0x20) > 0;
		//TODO Maybe other bits?
		break;
	case 0xc:		/* Clear Flip/Flip */
		flipflop=false;
		break;
	case 0xd: /* Clear/Reset all channels */
		for (Bit8u ct=0;ct<4;ct++) {
			chan=GetChannel(ct);
			chan->SetMask(true);
			chan->tcount=false;
		}
		flipflop=false;
		break;
	case 0xe:		/* Clear Mask register */		
		UpdateEMSMapping();
		for (Bit8u ct=0;ct<4;ct++) {
			chan=GetChannel(ct);
			chan->SetMask(false);
		}
		break;
	case 0xf:		/* Multiple Mask register */
		UpdateEMSMapping();
		for (Bit8u ct=0;ct<4;ct++) {
			chan=GetChannel(ct);
			chan->SetMask(val & 1);
			val>>=1;
		}
		break;
	}
}

uint16_t DmaController::ReadControllerReg(io_port_t reg, io_width_t)
{
	DmaChannel *chan = nullptr;
	uint16_t ret = 0;
	switch (reg) {
	/* read base address of DMA transfer (1st byte low part, 2nd byte high part) */
	case 0x0:
	case 0x2:
	case 0x4:
	case 0x6:
		chan = GetChannel((Bit8u)(reg >> 1));
		flipflop = !flipflop;
		if (flipflop) {
			return chan->curraddr & 0xff;
		} else {
			return (chan->curraddr >> 8) & 0xff;
		}
	/* read DMA transfer count (1st byte low part, 2nd byte high part) */
	case 0x1:
	case 0x3:
	case 0x5:
	case 0x7:
		chan = GetChannel((Bit8u)(reg >> 1));
		flipflop = !flipflop;
		if (flipflop) {
			return chan->currcnt & 0xff;
		} else {
			return (chan->currcnt >> 8) & 0xff;
		}
	case 0x8: /* Status Register */
		ret = 0;
		for (uint8_t ct = 0; ct < 4; ct++) {
			chan = GetChannel(ct);
			if (chan->tcount)
				ret |= 1 << ct;
			chan->tcount = false;
			if (chan->request)
				ret |= 1 << (4 + ct);
		}
		return ret;
	default:
		LOG(LOG_DMACONTROL, LOG_NORMAL)("Trying to read undefined DMA port %x", reg);
		break;
	}
	return 0xffff;
}

DmaChannel::DmaChannel(uint8_t num, bool dma16)
        : pagebase(0),
          baseaddr(0),
          curraddr(0),
          basecnt(0),
          currcnt(0),
          channum(0),
          pagenum(0),
          DMA16(0x0),
          increment(false),
          autoinit(false),
          masked(true),
          tcount(false),
          request(false),
          callback(nullptr)
{
	if (num == 4)
		return;
	channum = num;
	DMA16 = dma16 ? 0x1 : 0x0;
	increment = true;
}

size_t DmaChannel::ReadOrWrite(DMA_DIRECTION direction, size_t words, uint8_t *buffer)
{
	auto want = check_cast<uint16_t>(words);
	uint16_t done = 0;
	curraddr &= dma_wrapping;
again:
	Bitu left = (currcnt + 1);
	if (want < left) {
		perform_dma_io(direction, pagebase, curraddr, buffer, want, DMA16);
		done += want;
		curraddr += want;
		currcnt -= want;
	} else {
		perform_dma_io(direction, pagebase, curraddr, buffer, left, DMA16);
		buffer += left << DMA16;
		want -= left;
		done += left;
		ReachedTC();
		if (autoinit) {
			currcnt = basecnt;
			curraddr = baseaddr;
			if (want)
				goto again;
			UpdateEMSMapping();
		} else {
			curraddr += left;
			currcnt = 0xffff;
			masked = true;
			UpdateEMSMapping();
			DoCallBack(DMA_MASKED);
		}
	}
	return done;
}

class DMA final : public Module_base {
public:
	DMA(Section *configuration) : Module_base(configuration)
	{
		DmaControllers[0] = new DmaController(0);
		if (IS_EGAVGA_ARCH)
			DmaControllers[1] = new DmaController(1);
		else
			DmaControllers[1] = nullptr;

		for (io_port_t i = 0; i < 0x10; ++i) {
			const io_width_t width = (i < 8) ? io_width_t::word : io_width_t::byte;
			/* install handler for first DMA controller ports */
			DmaControllers[0]->DMA_WriteHandler[i].Install(i, DMA_Write_Port, width);
			DmaControllers[0]->DMA_ReadHandler[i].Install(i, DMA_Read_Port, width);
			if (IS_EGAVGA_ARCH) {
				/* install handler for second DMA controller ports */
				const auto dma_port = static_cast<io_port_t>(0xc0 + i * 2);
				DmaControllers[1]->DMA_WriteHandler[i].Install(dma_port,
				                                               DMA_Write_Port, width);
				DmaControllers[1]->DMA_ReadHandler[i].Install(dma_port,
				                                              DMA_Read_Port, width);
			}
		}
		/* install handlers for ports 0x81-0x83,0x87 (on the first DMA controller) */
		DmaControllers[0]->DMA_WriteHandler[0x10].Install(0x81, DMA_Write_Port, io_width_t::byte, 3);
		DmaControllers[0]->DMA_ReadHandler[0x10].Install(0x81, DMA_Read_Port, io_width_t::byte, 3);
		DmaControllers[0]->DMA_WriteHandler[0x11].Install(0x87, DMA_Write_Port, io_width_t::byte, 1);
		DmaControllers[0]->DMA_ReadHandler[0x11].Install(0x87, DMA_Read_Port, io_width_t::byte, 1);

		if (IS_EGAVGA_ARCH) {
			/* install handlers for ports 0x89-0x8b,0x8f (on the second DMA controller) */
			DmaControllers[1]->DMA_WriteHandler[0x10].Install(0x89, DMA_Write_Port, io_width_t::byte, 3);
			DmaControllers[1]->DMA_ReadHandler[0x10].Install(0x89, DMA_Read_Port, io_width_t::byte, 3);
			DmaControllers[1]->DMA_WriteHandler[0x11].Install(0x8f, DMA_Write_Port, io_width_t::byte, 1);
			DmaControllers[1]->DMA_ReadHandler[0x11].Install(0x8f, DMA_Read_Port, io_width_t::byte, 1);
		}
	}
	~DMA(){
		if (DmaControllers[0]) {
			delete DmaControllers[0];
			DmaControllers[0]=NULL;
		}
		if (DmaControllers[1]) {
			delete DmaControllers[1];
			DmaControllers[1]=NULL;
		}
	}
};

void DMA_SetWrapping(const uint32_t wrap) {
	dma_wrapping = wrap;
}

static DMA* test;

void DMA_Destroy(Section* /*sec*/){
	delete test;
}
void DMA_Init(Section* sec) {
	DMA_SetWrapping(0xffff);
	test = new DMA(sec);
	sec->AddDestroyFunction(&DMA_Destroy);
	Bitu i;
	for (i=0;i<LINK_START;i++) {
		ems_board_mapping[i]=i;
	}
}
