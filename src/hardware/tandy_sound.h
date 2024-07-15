/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2019-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2018  The DOSBox Team
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

#ifndef TANDY_SOUND_H
#define TANDY_SOUND_H

#include "dma.h"
#include "mixer.h"
#include "rwqueue.h"

enum class ConfigProfile {
	TandySystem,
	PcjrSystem,
	SoundCardOnly,
	SoundCardRemoved,
};

class TandyDAC {
public:
	struct IoConfig {
		uint16_t base = 0;
		uint8_t irq   = 0;
		uint8_t dma   = 0;
	};

	struct Dma {
		std::array<uint8_t, 128> fifo = {};
		DmaChannel* channel           = nullptr;
		bool is_done                  = false;
	};

	struct Registers {
		uint16_t clock_divider = 0;
		uint8_t mode           = 0;
		uint8_t control        = 0;
		uint8_t amplitude      = 0;
		bool irq_activated     = false;
	};

    RWQueue<uint8_t> output_queue{1};
	MixerChannelPtr channel = nullptr;
	float frame_counter     = 0.0f;

	// There's only one Tandy sound's IO configuration, so make it permanent
	static constexpr IoConfig io = {0xc4, 7, 1};

	TandyDAC(const ConfigProfile config_profile, const std::string& filter_choice);
	~TandyDAC();

	void PicCallback(const int requested);

private:
	void ChangeMode();

	void DmaCallback(const DmaChannel* chan, DMAEvent event);
	uint8_t ReadFromPort(io_port_t port, io_width_t);
	void WriteToPort(io_port_t port, io_val_t value, io_width_t);

	TandyDAC() = delete;

	Dma dma = {};

	IO_ReadHandleObject read_handler       = {};
	IO_WriteHandleObject write_handlers[2] = {};

	// States
	Registers regs     = {};
	int sample_rate_hz = 0;
};

extern std::unique_ptr<TandyDAC> tandy_dac;

#endif
