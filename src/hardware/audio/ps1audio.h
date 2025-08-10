// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PS1AUDIO_H
#define PS1AUDIO_H

#include "audio/mixer.h"
#include "util/math_utils.h"
#include "rwqueue.h"

struct Ps1Registers {
	// Read via port 0x202 control status
	uint8_t status = 0;

	// Written via port 0x202 for control, read via 0x200 for DAC
	uint8_t command = 0;

	// Read via port 0x203 for FIFO timing
	uint8_t divisor = 0;

	// Written via port 0x204 when FIFO is almost empty
	uint8_t fifo_level = 0;
};

class Ps1Dac {
public:
	Ps1Dac(const std::string& filter_choice);
	~Ps1Dac();
	void PicCallback(const int frames_requested);

	RWQueue<uint8_t> output_queue{1};
	MixerChannelPtr channel = nullptr;
	float frame_counter     = 0.0f;

private:
	uint8_t CalcStatus() const;
	void Reset(bool should_clear_adder);

	uint8_t ReadPresencePort02F(io_port_t port, io_width_t);
	uint8_t ReadCmdResultPort200(io_port_t port, io_width_t);
	uint8_t ReadStatusPort202(io_port_t port, io_width_t);
	uint8_t ReadTimingPort203(io_port_t port, io_width_t);
	uint8_t ReadJoystickPorts204To207(io_port_t port, io_width_t);

	void WriteDataPort200(io_port_t port, io_val_t value, io_width_t);
	void WriteControlPort202(io_port_t port, io_val_t value, io_width_t);
	void WriteTimingPort203(io_port_t port, io_val_t value, io_width_t);
	void WriteFifoLevelPort204(io_port_t port, io_val_t value, io_width_t);

	// Constants
	static constexpr auto ClockRateHz        = 1'000'000;
	static constexpr auto FifoSize           = 2048;
	static constexpr auto FifoMaskSize       = FifoSize - 1;
	static constexpr auto FifoNearlyEmptyVal = 128;

	// Fixed precision
	static constexpr auto FracShift = 12;

	static constexpr auto FifoStatusReadyFlag = 0x10;
	static constexpr auto FifoFullFlag        = 0x08;

	static constexpr auto FifoEmptyFlag = 0x04;
	// >= 1792 bytes free
	static constexpr auto FifoNearlyEmptyFlag = 0x02;

	// IRQ triggered by DAC
	static constexpr auto FifoIrqFlag = 0x01;

	static constexpr auto FifoMidline = ceil_udivide(static_cast<uint8_t>(UINT8_MAX),
	                                                 2u);

	static constexpr auto IrqNumber = 7;

	static constexpr auto BytesPendingLimit = FifoSize << FracShift;

	IO_ReadHandleObject read_handlers[5]   = {};
	IO_WriteHandleObject write_handlers[4] = {};

	Ps1Registers regs = {};

	uint8_t fifo[FifoSize] = {};

	// Counters
	size_t last_write        = 0;
	uint32_t adder           = 0;
	uint32_t bytes_pending   = 0;
	uint32_t read_index_high = 0;
	uint32_t sample_rate_hz  = 0;
	uint16_t read_index      = 0;
	uint16_t write_index     = 0;
	int8_t signal_bias       = 0;

	// States
	bool is_new_transfer = true;
	bool is_playing      = false;
	bool can_trigger_irq = false;
};

#endif
