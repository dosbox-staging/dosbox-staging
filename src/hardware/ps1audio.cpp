/*
 *  Copyright (C) 2021-2021  The DOSBox Staging Team
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

#include <cassert>
#include <memory>
#include <string.h>

#include "control.h"
#include "dma.h"
#include "inout.h"
#include "mem.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"

#include "mame/emu.h"
#include "mame/sn76496.h"

using namespace std::placeholders;
using mixer_channel_t = std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;

struct Ps1Registers {
	uint8_t status = 0;  // 0202 RD
	uint8_t command = 0; // 0202 WR / 0200 RD
	uint8_t data = 0;    // 0200 WR
	uint8_t divisor = 0; // 0203 WR
	uint8_t unknown = 0; // 0204 WR (Reset?)
};

class Ps1Dac {
public:
	void Open();
	void Close();

private:
	uint8_t CalcStatus() const;
	void Reset(bool should_clear_adder);
	void Update(uint16_t samples);
	uint8_t ReadFromPort(uint16_t port, MAYBE_UNUSED size_t iolen);
	void WriteTo0200_0204(uint16_t port, uint8_t data, MAYBE_UNUSED size_t iolen);

	// Constants
	static constexpr auto clock_rate = 1000000; // 950272?
	static constexpr auto fifo_size = 2048;
	static constexpr auto fifo_size_mask = fifo_size - 1;
	static constexpr auto fifo_nearly_empty_val = 128;
	static constexpr auto frac_shift = 12;            // Fixed precision
	static constexpr auto fifo_status_ready_flag = 0x10;
	static constexpr auto fifo_full_flag = 0x08;
	static constexpr auto fifo_empty_flag = 0x04;
	static constexpr auto fifo_nearly_empty_flag = 0x02; // >= 1792 bytes free
	static constexpr auto fifo_irq_flag = 0x01; // IRQ triggered by DAC
	static constexpr auto irq_number = 7;

	// Managed objects
	mixer_channel_t channel{nullptr, MIXER_DelChannel};
	IO_ReadHandleObject read_handlers[3] = {};
	IO_WriteHandleObject write_handlers[2] = {};
	Ps1Registers regs = {};
	uint8_t fifo[fifo_size] = {};

	// Counters
	size_t last_write = 0;
	uint32_t adder = 0;
	uint32_t bytes_pending = 0;
	uint32_t read_index_high = 0;
	uint32_t sample_rate = 0;
	uint16_t read_index = 0;
	uint16_t write_index = 0;

	// States
	bool is_enabled = false;
	bool is_playing = false;
	bool can_trigger_irq = false;
	bool is_open = false;
};

void Ps1Dac::Open()
{
	Close();

	const auto callback = std::bind(&Ps1Dac::Update, this, _1);
	channel = mixer_channel_t(MIXER_AddChannel(callback, 0, "PS1DAC"),
	                          MIXER_DelChannel);
	assert(channel);

	// Register port handlers for 8-bit IO
	const auto read_from = std::bind(&Ps1Dac::ReadFromPort, this, _1, _2);
	read_handlers[0].Install(0x02F, read_from, IO_MB);
	read_handlers[1].Install(0x200, read_from, IO_MB);
	read_handlers[2].Install(0x202, read_from, IO_MB, 4);

	const auto write_to = std::bind(&Ps1Dac::WriteTo0200_0204, this, _1, _2, _3);
	write_handlers[0].Install(0x200, write_to, IO_MB);
	write_handlers[1].Install(0x202, write_to, IO_MB, 3);

	// Operate at native sampling rates
	sample_rate = channel->GetSampleRate();
	is_enabled = false;
	last_write = 0;
	Reset(true);
	is_open = true;
}

uint8_t Ps1Dac::CalcStatus() const
{
	uint8_t status = regs.status & fifo_irq_flag;
	if (!bytes_pending)
		status |= fifo_empty_flag;

	if (bytes_pending < (fifo_nearly_empty_val << frac_shift) &&
	    (regs.command & 3) == 3)
		status |= fifo_nearly_empty_flag;

	if (bytes_pending > ((fifo_size - 1) << frac_shift))
		status |= fifo_full_flag;

	return status;
}

void Ps1Dac::Reset(bool should_clear_adder)
{
	PIC_DeActivateIRQ(irq_number);
	regs.data = 0x80;
	memset(fifo, 0x80, fifo_size);
	read_index = 0;
	write_index = 0;
	read_index_high = 0;

	 // Be careful with this, 5 second timeout and Space Quest 4
	if (should_clear_adder)
		adder = 0;

	bytes_pending = 0;
	regs.status = CalcStatus();
	is_playing = true;
	can_trigger_irq = false;
}

void Ps1Dac::WriteTo0200_0204(uint16_t port, uint8_t data, MAYBE_UNUSED size_t iolen)
{
	last_write = PIC_Ticks;
	if (!is_enabled) {
		channel->Enable(true);
		is_enabled = true;
	}
	switch (port) {
	case 0x0200:
		// regs.data - insert into fifo.
		regs.data = (uint8_t)data;
		regs.status = CalcStatus();
		if (!(regs.status & fifo_full_flag)) {
			fifo[write_index++] = (uint8_t)data;
			write_index &= fifo_size_mask;
			bytes_pending += (1 << frac_shift);
			if (bytes_pending > (fifo_size << frac_shift)) {
				bytes_pending = fifo_size << frac_shift;
			}
		}
		break;
	case 0x0202:
		// regs.command.
		regs.command = data;
		if (data & 3)
			can_trigger_irq = true;
		break;
	case 0x0203: {
		// Clock divisor (maybe trigger first IRQ here).
		regs.divisor = data;
		const auto rate = static_cast<uint32_t>(clock_rate / (data + 1));
		adder = (rate << frac_shift) / sample_rate;
		regs.status = CalcStatus();
		if ((regs.status & fifo_nearly_empty_flag) && (can_trigger_irq)) {
			// Generate request for stuff.
			regs.status |= fifo_irq_flag;
			can_trigger_irq = false;
			PIC_ActivateIRQ(irq_number);
		}
	} break;
	case 0x0204:
		// Reset? (PS1MIC01 sets it to 08 for playback...)
		regs.unknown = data;
		if (!data)
			Reset(true);
		break;
	default: break;
	}
}

uint8_t Ps1Dac::ReadFromPort(uint16_t port, MAYBE_UNUSED size_t iolen)
{
	last_write = PIC_Ticks;
	if (!is_enabled) {
		channel->Enable(true);
		is_enabled = true;
	}
	switch (port) {
	case 0x02F: // CMOS Card is present check
		return 0xff;
	case 0x0200:
		// Read last command.
		regs.status &= ~fifo_status_ready_flag;
		return regs.command;
	case 0x0202: {
		// Read status / clear IRQ?.
		uint8_t status = regs.status = CalcStatus();
		// Don't do this until we have some better way of
		// detecting the triggering and ending of an IRQ.
		// ---> regs.status &= ~fifo_irq_flag;
		return status;
	}
	case 0x0203:
		// Stunt Island / Roger Rabbit 2 setup.
		return regs.divisor;
	case 0x0205:
	case 0x0206:
		// Bush Buck detection.
		return 0;
	default: break;
	}
	return 0xFF;
}

void Ps1Dac::Update(uint16_t samples)
{
	if ((last_write + 5000) < PIC_Ticks) {
		is_enabled = false;
		channel->Enable(false);
		// Excessive?
		Reset(false);
	}
	uint8_t *buffer = MixTemp;

	int32_t pending = 0;
	uint32_t add = 0;
	uint32_t pos = read_index_high;
	uint16_t count = samples;

	if (is_playing) {
		regs.status = CalcStatus();
		pending = static_cast<int32_t>(bytes_pending);
		add = adder;
		if ((regs.status & fifo_nearly_empty_flag) && (can_trigger_irq)) {
			// More bytes needed.
			regs.status |= fifo_irq_flag;
			can_trigger_irq = false;
			PIC_ActivateIRQ(irq_number);
		}
	}

	while (count) {
		unsigned int out;

		if (pending <= 0) {
			pending = 0;
			while (count--) {
				*(buffer++) = 0x80; // Silence
			}
			break;
		} else {
			out = fifo[pos >> frac_shift];
			pos += add;
			pos &= (fifo_size << frac_shift) - 1;
			pending -= static_cast<int32_t>(add);
		}

		*(buffer++) = out;
		count--;
	}
	// Update positions and see if we can clear the fifo_full_flag
	read_index_high = pos;
	read_index = static_cast<uint16_t>(pos >> frac_shift);
	if (pending < 0)
		pending = 0;
	bytes_pending = static_cast<uint32_t>(pending);

	channel->AddSamples_m8(samples, MixTemp);
}

void Ps1Dac::Close()
{
	if (!is_open)
		return;

	// Stop the game from accessing the IO ports
	for (auto &handler : read_handlers)
		handler.Uninstall();
	for (auto &handler : write_handlers)
		handler.Uninstall();

	// Stop and remove the mixer callback
	if (channel) {
		channel->Enable(false);
		channel.reset();
	}
	is_open = false;
}

class Ps1Synth {
public:
	Ps1Synth() : device(machine_config(), 0, 0, clock_rate) {}
	void Open();
	void Close();

private:
	void Update(uint16_t samples);
	void WriteTo0205(uint16_t port, uint8_t data, MAYBE_UNUSED size_t iolen);

	mixer_channel_t channel{nullptr, MIXER_DelChannel};
	IO_WriteHandleObject write_handler = {};
	static constexpr auto clock_rate = 4000000;
	sn76496_device device;
	static constexpr auto max_samples_expected = 64;
	int16_t buffer[max_samples_expected];
	size_t last_write = 0;
	bool is_enabled = false;
	bool is_open = false;
};

void Ps1Synth::Open()
{
	Close();
	const auto callback = std::bind(&Ps1Synth::Update, this, _1);
	channel = mixer_channel_t(MIXER_AddChannel(callback, 0, "PS1"),
	                          MIXER_DelChannel);
	assert(channel);

	const auto write_to = std::bind(&Ps1Synth::WriteTo0205, this, _1, _2, _3);
	write_handler.Install(0x205, write_to, IO_MB);
	static_cast<device_t &>(device).device_start();
	device.convert_samplerate(channel->GetSampleRate());

	is_enabled = false;
	last_write = 0;
	is_open = true;
}

void Ps1Synth::WriteTo0205(MAYBE_UNUSED uint16_t port,
                           uint8_t data,
                           MAYBE_UNUSED size_t iolen)
{
	last_write = PIC_Ticks;
	if (!is_enabled) {
		channel->Enable(true);
		is_enabled = true;
	}
	device.write(data);
}

void Ps1Synth::Update(uint16_t samples)
{
	assert(samples <= max_samples_expected);
	if ((last_write + 5000) < PIC_Ticks) {
		is_enabled = false;
		channel->Enable(false);
		return;
	}
	int16_t *pbuf = buffer;
	device_sound_interface::sound_stream ss;
	static_cast<device_sound_interface &>(device).sound_stream_update(ss, 0, &pbuf, samples);
	channel->AddSamples_m16(samples, buffer);
}

void Ps1Synth::Close()
{
	if (!is_open)
		return;

	// Stop the game from accessing the IO ports
	write_handler.Uninstall();

	if (channel) {
		channel->Enable(false);
		channel.reset();
	}
	is_open = false;
}

static Ps1Dac ps1_dac;
static Ps1Synth ps1_synth;

static void PS1AUDIO_ShutDown(MAYBE_UNUSED Section *sec)
{
	LOG_MSG("PS/1: Shutting down IBM PS/1 Audio card");
	ps1_dac.Close();
	ps1_synth.Close();
}

void PS1AUDIO_Init(Section *sec)
{
	assert(sec);
	Section_prop *section = static_cast<Section_prop *>(sec);
	if (!section->Get_bool("ps1audio"))
		return;

	ps1_dac.Open();
	ps1_synth.Open();

	LOG_MSG("PS/1: Initialized IBM PS/1 Audio card");
	sec->AddDestroyFunction(&PS1AUDIO_ShutDown, true);
}
