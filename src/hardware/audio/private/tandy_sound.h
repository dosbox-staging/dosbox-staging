// SPDX-FileCopyrightText:  2019-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2018 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TANDY_SOUND_H
#define TANDY_SOUND_H

#include "audio/mixer.h"
#include "hardware/dma.h"
#include "utils/rwqueue.h"

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

	void DmaCallback(const DmaChannel* chan, DmaEvent event);
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

#endif
