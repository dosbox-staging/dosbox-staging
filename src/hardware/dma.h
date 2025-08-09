// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DMA_H
#define DOSBOX_DMA_H

#include "dosbox.h"

#include <cassert>
#include <functional>

#include "hardware/inout.h"
#include "misc/support.h"

enum class DmaDirection {
	Read,
	Write,
};

enum class DmaEvent {
	ReachedTerminalCount,
	IsMasked,
	IsUnmasked,
};

class Section;
using DMA_ReservationCallback = std::function<void(Section*)>;

class DmaChannel;
using DMA_Callback = std::function<void(const DmaChannel* chan, DmaEvent event)>;

class DmaChannel {
public:
	// Defaults at the time of initialization
	uint32_t page_base  = 0;
	uint32_t curr_addr = 0;

	uint16_t base_addr  = 0;
	uint16_t base_count = 0;
	uint16_t curr_count = 0;

	const uint8_t chan_num = 0;
	uint8_t page_num = 0;
	const uint8_t is_16bit = 0;

	bool is_incremented             = true;
	bool is_autoiniting             = false;
	bool is_masked                  = true;
	bool has_reached_terminal_count = false;
	bool has_raised_request         = false;

	DMA_Callback callback = {};

	DmaChannel(uint8_t num, bool dma16);
	~DmaChannel();

	void DoCallback(DmaEvent event) const;
	void SetMask(bool _mask);
	void RegisterCallback(const DMA_Callback cb);
	void ReachedTerminalCount();
	void SetPage(uint8_t val);
	void RaiseRequest();
	void ClearRequest();
	size_t Read(size_t words, uint8_t* const dest_buffer);
	size_t Write(size_t words, uint8_t* const src_buffer);
	void LogDetails() const;

	// Reset the channel back to defaults, without callbacks or reservations.
	void Reset();

	// Reserves the channel for the owner. If a subsequent reservation is
	// made then the previously held reservation callback is run to
	// cleanup/remove that reserver (see the EvictReserver call below).
	void ReserveFor(const std::string& new_owner,
	                const DMA_ReservationCallback new_cb);

private:
	void EvictReserver();
	bool HasReservation() const;
	size_t ReadOrWrite(DmaDirection direction, size_t words,
	                   uint8_t* const buffer);

	DMA_ReservationCallback reservation_callback = {};
	std::string reservation_owner                = {};
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

DmaChannel* DMA_GetChannel(uint8_t chan);

void DMA_ShutdownSecondaryController();
void DMA_ResetChannel(const uint8_t channel_num);
void DMA_SetWrapping(const uint32_t wrap);

#endif
