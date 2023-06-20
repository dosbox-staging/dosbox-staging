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

enum class DMA_DIRECTION { READ, WRITE };

enum DMAEvent {
	DMA_REACHED_TC,
	DMA_MASKED,
	DMA_UNMASKED,
};

class Section;
using DMA_ReservationCallback = std::function<void(Section*)>;

class DmaChannel;
using DMA_Callback = std::function<void(DmaChannel *chan, DMAEvent event)>;

class DmaChannel {
public:
	// Defaults at the time of initialization
	uint32_t pagebase = 0;
	uint32_t curraddr = 0;

	uint16_t baseaddr = 0;
	uint16_t basecnt  = 0;
	uint16_t currcnt  = 0;

	const uint8_t channum = 0;
	uint8_t pagenum = 0;
	const uint8_t DMA16 = 0;

	bool increment = true;
	bool autoinit  = false;
	bool masked    = true;
	bool tcount    = false;
	bool request   = false;

	DMA_Callback callback = {};

	DmaChannel(uint8_t num, bool dma16);
	~DmaChannel();

	void DoCallback(DMAEvent event);
	void SetMask(bool _mask);
	void RegisterCallback(const DMA_Callback cb);
	void ReachedTerminalCount();
	void SetPage(uint8_t val);
	void RaiseRequest();
	void ClearRequest();
	size_t Read(size_t words, uint8_t* const dest_buffer);
	size_t Write(size_t words, uint8_t* const src_buffer);

	// Reset the channel back to defaults, without callbacks or reservations.
	void Reset();

	// Reserves the channel for the owner. If a subsequent reservation is
	// made then the previously held reservation callback is run to
	// cleanup/remove that reserver (see the EvictReserver call below).
	void ReserveFor(const std::string_view new_owner,
	                const DMA_ReservationCallback new_cb);

private:
	void EvictReserver();
	bool HasReservation() const;
	size_t ReadOrWrite(DMA_DIRECTION direction, size_t words, uint8_t* const buffer);

	DMA_ReservationCallback reservation_callback = {};
	std::string_view reservation_owner = {};
};

class DmaController {
private:
	bool flipflop = false;

	std::unique_ptr<DmaChannel> dma_channels[4] = {};

	IO_ReadHandleObject io_read_handlers[0x12]   = {};
	IO_WriteHandleObject io_write_handlers[0x12] = {};

	const uint8_t index = 0;

public:
	DmaController(const uint8_t controller_index);
	~DmaController();

	// prevent copy
	DmaController(const DmaController&) = delete;

	// prevent assignment
	DmaController& operator=(const DmaController&) = delete;

	DmaChannel* GetChannel(const uint8_t channel_num) const;

	void WriteControllerReg(io_port_t reg, io_val_t value, io_width_t width);
	uint16_t ReadControllerReg(io_port_t reg, io_width_t width);
	void ResetChannel(const uint8_t channel_num) const;
};

DmaChannel * GetDMAChannel(uint8_t chan);

void CloseSecondDMAController();
void DMA_ResetChannel(const uint8_t channel_num);
void DMA_SetWrapping(const uint32_t wrap);

#endif
