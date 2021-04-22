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

struct Ps1Registers {
	uint8_t status = 0;  // 0202 RD
	uint8_t command = 0; // 0202 WR / 0200 RD
	uint8_t data = 0;    // 0200 WR
	uint8_t divisor = 0; // 0203 WR
	uint8_t unknown = 0; // 0204 WR (Reset?)
};

using mixer_channel_t = std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;
using namespace std::placeholders;

class Ps1Dac {
public:
	void Open();
	void Close();

private:
	uint8_t CalcStatus() const;
	void Reset(bool bTotal);
	void Update(size_t length);
	uint8_t ReadFromPort(size_t port, MAYBE_UNUSED size_t iolen);
	void WriteTo0200_0204(size_t port, size_t data, MAYBE_UNUSED size_t iolen);

	// Constants
	static constexpr auto clock_rate = 1000000; // 950272?
	static constexpr auto FIFOSIZE = 2048;      // powers of two
	static constexpr auto FIFOSIZE_MASK = FIFOSIZE - 1;
	static constexpr auto FIFO_NEARLY_EMPTY_VAL = 128;
	static constexpr auto FRAC_SHIFT = 12; // Fixed precision
	// High when the interrupt can't do anything but wait (cleared by
	// reading 0200?).
	static constexpr auto FIFO_READ_AVAILABLE = 0x10;
	// High when we can't write any more.
	static constexpr auto FIFO_FULL = 0x08;
	// High when we can write direct values???
	static constexpr auto FIFO_EMPTY = 0x04;
	// High when we can write more to the fifo (or, at least, there are
	// 0x700 bytes free).
	static constexpr auto FIFO_NEARLY_EMPTY = 0x02;
	// High when IRQ was triggered by the DAC?
	static constexpr auto FIFO_IRQ = 0x01;

	// Managed objects
	mixer_channel_t channel{nullptr, MIXER_DelChannel};
	IO_ReadHandleObject read_handlers[3] = {};
	IO_WriteHandleObject write_handlers[2] = {};
	Ps1Registers regs = {};
	uint8_t fifo[FIFOSIZE] = {};

	// Counters
	size_t adder = 0;
	size_t bytes_pending = 0;
	size_t last_write = 0;
	size_t read_index_high = 0;
	uint32_t rate = 0;
	uint32_t sample_rate = 0;
	uint16_t read_index = 0;
	uint16_t write_index = 0;

	// States
	bool is_enabled = false;
	bool is_playing = false;
	bool can_trigger_irq = false;
	bool is_open = false;
};

class Ps1Synth {
public:
	void Open();
	void Close();

private:
	void Update(size_t length);
	void WriteTo0205(size_t port, size_t data, MAYBE_UNUSED size_t iolen);

	mixer_channel_t channel{nullptr, MIXER_DelChannel};
	IO_WriteHandleObject write_handler = {};

	static constexpr auto synth_clock = 4000000;

#if 0
	struct SN76496 service;
#endif
	size_t last_write = 0;
	bool is_enabled = false;
	bool is_open = false;
};

uint8_t Ps1Dac::CalcStatus() const
{
	uint8_t status = regs.status & FIFO_IRQ;
	if (!bytes_pending)
		status |= FIFO_EMPTY;

	if (bytes_pending < (FIFO_NEARLY_EMPTY_VAL << FRAC_SHIFT) &&
	    (regs.command & 3) == 3)
		status |= FIFO_NEARLY_EMPTY;

	if (bytes_pending > ((FIFOSIZE - 1) << FRAC_SHIFT))
		status |= FIFO_FULL;

	return status;
}

void Ps1Dac::Reset(bool bTotal)
{
	PIC_DeActivateIRQ( 7 );
	regs.data = 0x80;
	memset(fifo, 0x80, FIFOSIZE);
	read_index = 0;
	write_index = 0;
	if (bTotal)
		rate = 0xFFFFFFFF;
	read_index_high = 0;
	if (bTotal)
		adder = 0; // Be careful with this, 5 second timeout and
		           // Space Quest 4!
	bytes_pending = 0;
	regs.status = CalcStatus();
	is_playing = true;
	can_trigger_irq = false;
}

void Ps1Synth::WriteTo0205(size_t port, size_t data, MAYBE_UNUSED size_t iolen)
{
	last_write = PIC_Ticks;
	if (!is_enabled) {
		channel->Enable(true);
		is_enabled = true;
	}
#if 0
	SN76496Write(&synth.service,port,data);
#endif
}

void Ps1Dac::WriteTo0200_0204(size_t port, size_t data, MAYBE_UNUSED size_t iolen)
{
	last_write = PIC_Ticks;
	if (!is_enabled) {
		channel->Enable(true);
		is_enabled = true;
	}

#if C_DEBUG != 0
	if( ( port != 0x0205 ) && ( port != 0x0200 ) )
		LOG_MSG("PS1 WR %04X,%02X (%04X:%08X)",(int)port,(int)data,(int)SegValue(cs),(int)reg_eip);
#endif
	switch(port)
	{
	case 0x0200:
		// regs.data - insert into fifo.
		regs.data = (uint8_t)data;
		regs.status = CalcStatus();
		if (!(regs.status & FIFO_FULL)) {
			fifo[write_index++] = (uint8_t)data;
			write_index &= FIFOSIZE_MASK;
			bytes_pending += (1 << FRAC_SHIFT);
			if (bytes_pending > (FIFOSIZE << FRAC_SHIFT)) {
				bytes_pending = FIFOSIZE << FRAC_SHIFT;
			}
		}
		break;
	case 0x0202:
		// regs.command.
		regs.command = (uint8_t)data;
		if (data & 3)
			can_trigger_irq = true;
		break;
	case 0x0203: {
		// Clock divisor (maybe trigger first IRQ here).
		regs.divisor = (uint8_t)data;
		rate = (uint32_t)(clock_rate / (data + 1));
		adder = (rate << FRAC_SHIFT) / (unsigned int)sample_rate;
		regs.status = CalcStatus();
		if ((regs.status & FIFO_NEARLY_EMPTY) && (can_trigger_irq)) {
			// Generate request for stuff.
			regs.status |= FIFO_IRQ;
			can_trigger_irq = false;
			PIC_ActivateIRQ(7);
		}
	} break;
	case 0x0204:
		// Reset? (PS1MIC01 sets it to 08 for playback...)
		regs.unknown = (uint8_t)data;
		if (!data)
			Reset(true);
		break;
	default: break;
	}
}

uint8_t Ps1Dac::ReadFromPort(size_t port, MAYBE_UNUSED size_t iolen)
{
	last_write = PIC_Ticks;
	if (!is_enabled) {
		channel->Enable(true);
		is_enabled = true;
	}
#if C_DEBUG != 0
	LOG_MSG("PS1 RD %04X (%04X:%08X)",(int)port,(int)SegValue(cs),(int)reg_eip);
#endif
	switch(port)
	{
	case 0x02F: // CMOS Card is present check
		return 0xff;
	case 0x0200:
		// Read last command.
		regs.status &= ~FIFO_READ_AVAILABLE;
		return regs.command;
	case 0x0202: {
		// Read status / clear IRQ?.
		uint8_t status = regs.status = CalcStatus();
		// Don't do this until we have some better way of
		// detecting the triggering and ending of an IRQ.
		// ---> regs.status &= ~FIFO_IRQ;
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

void Ps1Dac::Update(size_t length)
{
	if ((last_write + 5000) < PIC_Ticks) {
		is_enabled = false;
		channel->Enable(false);
		// Excessive?
		Reset(false);
	}
	uint8_t * buffer=(uint8_t *)MixTemp;

	Bits pending = 0;
	Bitu add = 0;
	Bitu pos = read_index_high;
	Bitu count=length;

	if (is_playing) {
		regs.status = CalcStatus();
		pending = (Bits)bytes_pending;
		add = adder;
		if ((regs.status & FIFO_NEARLY_EMPTY) && (can_trigger_irq)) {
			// More bytes needed.
			regs.status |= FIFO_IRQ;
			can_trigger_irq = false;
			PIC_ActivateIRQ(7);
		}
	}

	while (count)
	{
		unsigned int out;

		if( pending <= 0 ) {
			pending = 0;
			while( count-- ) *(buffer++) = 0x80;	// Silence.
			break;
		} else {
			out = fifo[pos >> FRAC_SHIFT];
			pos += add;
			pos &= ( ( FIFOSIZE << FRAC_SHIFT ) - 1 );
			pending -= (Bits)add;
		}

		*(buffer++) = out;
		count--;
	}
	// Update positions and see if we can clear the FIFO_FULL flag.
	read_index_high = pos;
	read_index = (uint16_t)(pos >> FRAC_SHIFT);
	if (pending < 0)
		pending = 0;
	bytes_pending = (Bitu)pending;

	channel->AddSamples_m8(length, MixTemp);
}

void Ps1Synth::Update(size_t length)
{
	if ((last_write + 5000) < PIC_Ticks) {
		is_enabled = false;
		channel->Enable(false);
	}
#if 0
	SN76496Update(&service,buffer,length);
#endif
	channel->AddSamples_m16(length, (int16_t *)MixTemp);
}

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

void Ps1Dac::Close()
{
	if (!is_open)
		return;
	// Stop the game from accessing the IO ports
	for (auto &handler : read_handlers)
		handler.Uninstall();
	for (auto &handler : write_handlers)
		handler.Uninstall();

	// Stop and remove the mixer callbacks
	if (channel) {
		channel->Enable(false);
		channel.reset();
	}
	is_open = false;
}

void Ps1Synth::Open()
{
	Close();
	const auto callback = std::bind(&Ps1Synth::Update, this, _1);
	channel = mixer_channel_t(MIXER_AddChannel(callback, 0, "PS1"),
	                          MIXER_DelChannel);
	assert(channel);

	const auto write_to = std::bind(&Ps1Synth::WriteTo0205, this, _1, _2, _3);
	write_handler.Install(0x205, write_to, IO_MB);

	is_enabled = false;
	last_write = 0;
#if 0
	SN76496Reset( &service, synth_clock, channel->GetSampleRate());
#endif
	is_open = true;
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
	DEBUG_LOG_MSG("PS/1: Shutting down IBM PS/1 Audio card");
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
