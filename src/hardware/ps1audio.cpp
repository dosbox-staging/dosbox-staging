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

// FIXME: MAME updates broke this code!
constexpr auto DAC_CLOCK = 1000000; // 950272?

constexpr auto FIFOSIZE = 2048; // powers of two
constexpr auto FIFOSIZE_MASK = FIFOSIZE - 1;
constexpr auto FIFO_NEARLY_EMPTY_VAL = 128;

constexpr auto FRAC_SHIFT = 12; // Fixed precision

// High when the interrupt can't do anything but wait (cleared by reading 0200?).
constexpr auto FIFO_READ_AVAILABLE = 0x10;

// High when we can't write any more.
constexpr auto FIFO_FULL = 0x08;

// High when we can write direct values???
constexpr auto FIFO_EMPTY = 0x04;

// High when we can write more to the dac.fifo (or, at least, there are 0x700
// bytes free).
constexpr auto FIFO_NEARLY_EMPTY = 0x02;

// High when IRQ was triggered by the DAC?
constexpr auto FIFO_IRQ = 0x01;

using mixer_channel_t = std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;

struct Ps1Dac {
	mixer_channel_t channel{nullptr, MIXER_DelChannel};
	uint8_t fifo[FIFOSIZE] = {};
	size_t adder = 0;
	size_t bytes_pending = 0;
	size_t last_write = 0;
	size_t read_index_high = 0;
	uint32_t rate = 0;
	uint16_t read_index = 0;
	uint16_t write_index = 0;
	bool is_enabled = false;
	bool is_playing = false;
	bool can_trigger_irq = false;
};

struct Ps1Synthesizer {
	mixer_channel_t channel{nullptr, MIXER_DelChannel};
#if 0
	struct SN76496 service;
#endif
	size_t last_write = 0;
	bool is_enabled = false;
};

struct Ps1Registers {
	uint8_t status = 0;  // 0202 RD
	uint8_t command = 0; // 0202 WR / 0200 RD
	uint8_t data = 0;    // 0200 WR
	uint8_t divisor = 0; // 0203 WR
	uint8_t unknown = 0; // 0204 WR (Reset?)
};

class Ps1Audio {
public:
	void Open();
	void Close();

private:
	uint8_t CalcStatus() const;
	uint8_t ReadFromPort(size_t port, MAYBE_UNUSED size_t iolen);
	void WriteToPort(size_t port, size_t data, MAYBE_UNUSED size_t iolen);

	void ResetDac(bool bTotal);
	void UpdateDac(size_t length);

	void UpdateSynth(size_t length);

	void ResetStates();

	Ps1Dac dac = {};
	Ps1Synthesizer synth = {};
	Ps1Registers regs = {};

	IO_ReadHandleObject read_handlers[3] = {};
	IO_WriteHandleObject write_handlers[2] = {};
	uint32_t sample_rate = 0;
	bool is_open = false;
};

uint8_t Ps1Audio::CalcStatus() const
{
	uint8_t status = regs.status & FIFO_IRQ;
	if (!dac.bytes_pending)
		status |= FIFO_EMPTY;

	if (dac.bytes_pending < (FIFO_NEARLY_EMPTY_VAL << FRAC_SHIFT) &&
	    (regs.command & 3) == 3)
		status |= FIFO_NEARLY_EMPTY;

	if (dac.bytes_pending > ((FIFOSIZE - 1) << FRAC_SHIFT))
		status |= FIFO_FULL;

	return status;
}

void Ps1Audio::ResetDac(bool bTotal)
{
	PIC_DeActivateIRQ( 7 );
	regs.data = 0x80;
	memset(dac.fifo, 0x80, FIFOSIZE);
	dac.read_index = 0;
	dac.write_index = 0;
	if (bTotal)
		dac.rate = 0xFFFFFFFF;
	dac.read_index_high = 0;
	if (bTotal)
		dac.adder = 0; // Be careful with this, 5 second timeout and
		               // Space Quest 4!
	dac.bytes_pending = 0;
	regs.status = CalcStatus();
	dac.is_playing = true;
	dac.can_trigger_irq = false;
}

void Ps1Audio::WriteToPort(size_t port, size_t data, MAYBE_UNUSED size_t iolen)
{
	if (port != 0x0205) {
		dac.last_write = PIC_Ticks;
		if (!dac.is_enabled) {
			dac.channel->Enable(true);
			dac.is_enabled = true;
		}
	} else {
		synth.last_write = PIC_Ticks;
		if (!synth.is_enabled) {
			synth.channel->Enable(true);
			synth.is_enabled = true;
		}
	}

#if C_DEBUG != 0
	if( ( port != 0x0205 ) && ( port != 0x0200 ) )
		LOG_MSG("PS1 WR %04X,%02X (%04X:%08X)",(int)port,(int)data,(int)SegValue(cs),(int)reg_eip);
#endif
	switch(port)
	{
		case 0x0200:
		        // regs.data - insert into dac.fifo.
		        regs.data = (uint8_t)data;
		        regs.status = CalcStatus();
		        if (!(regs.status & FIFO_FULL)) {
			        dac.fifo[dac.write_index++] = (uint8_t)data;
			        dac.write_index &= FIFOSIZE_MASK;
			        dac.bytes_pending += (1 << FRAC_SHIFT);
			        if (dac.bytes_pending > (FIFOSIZE << FRAC_SHIFT)) {
				        dac.bytes_pending = FIFOSIZE << FRAC_SHIFT;
			        }
		        }
		        break;
	        case 0x0202:
		        // regs.command.
		        regs.command = (uint8_t)data;
		        if (data & 3)
			        dac.can_trigger_irq = true;
		        break;
	        case 0x0203: {
		        // Clock divisor (maybe trigger first IRQ here).
		        regs.divisor = (uint8_t)data;
		        dac.rate = (uint32_t)(DAC_CLOCK / (data + 1));
		        dac.adder = (dac.rate << FRAC_SHIFT) /
		                    (unsigned int)sample_rate;
		        regs.status = CalcStatus();
		        if ((regs.status & FIFO_NEARLY_EMPTY) &&
		            (dac.can_trigger_irq)) {
			        // Generate request for stuff.
			        regs.status |= FIFO_IRQ;
			        dac.can_trigger_irq = false;
			        PIC_ActivateIRQ(7);
		        }
	        } break;
	        case 0x0204:
		        // Reset? (PS1MIC01 sets it to 08 for playback...)
		        regs.unknown = (uint8_t)data;
		        if (!data)
			        ResetDac(true);
		        break;
	        case 0x0205:
#if 0
			SN76496Write(&synth.service,port,data);
#endif
			break;
		default:break;
	}
}

uint8_t Ps1Audio::ReadFromPort(size_t port, MAYBE_UNUSED size_t iolen)
{
	dac.last_write = PIC_Ticks;
	if (!dac.is_enabled) {
		dac.channel->Enable(true);
		dac.is_enabled = true;
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

void Ps1Audio::UpdateDac(size_t length)
{
	if ((dac.last_write + 5000) < PIC_Ticks) {
		dac.is_enabled = false;
		dac.channel->Enable(false);
		// Excessive?
		ResetDac(false);
	}
	uint8_t * buffer=(uint8_t *)MixTemp;

	Bits pending = 0;
	Bitu add = 0;
	Bitu pos = dac.read_index_high;
	Bitu count=length;

	if (dac.is_playing) {
		regs.status = CalcStatus();
		pending = (Bits)dac.bytes_pending;
		add = dac.adder;
		if ((regs.status & FIFO_NEARLY_EMPTY) && (dac.can_trigger_irq)) {
			// More bytes needed.
			regs.status |= FIFO_IRQ;
			dac.can_trigger_irq = false;
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
			out = dac.fifo[pos >> FRAC_SHIFT];
			pos += add;
			pos &= ( ( FIFOSIZE << FRAC_SHIFT ) - 1 );
			pending -= (Bits)add;
		}

		*(buffer++) = out;
		count--;
	}
	// Update positions and see if we can clear the FIFO_FULL flag.
	dac.read_index_high = pos;
	dac.read_index = (uint16_t)(pos >> FRAC_SHIFT);
	if (pending < 0)
		pending = 0;
	dac.bytes_pending = (Bitu)pending;

	dac.channel->AddSamples_m8(length, MixTemp);
}

void Ps1Audio::UpdateSynth(size_t length)
{
	if ((synth.last_write + 5000) < PIC_Ticks) {
		synth.is_enabled = false;
		synth.channel->Enable(false);
	}
#if 0
	SN76496Update(&synth.service,buffer,length);
#endif
	synth.channel->AddSamples_m16(length, (int16_t *)MixTemp);
}

void Ps1Audio::ResetStates()
{
	// Initialize the PS/1 states
	dac.is_enabled = false;
	dac.last_write = 0;

	synth.is_enabled = false;
	synth.last_write = 0;
	ResetDac(true);

	// > Jmk wrote:
	// > Judging by what I've read in that technical document, it looks like
	// the sound chip is fed by a 4 Mhz clock instead of a ~3.5 Mhz clock.
	// >
	// > So, there's a line in ps1_sound.cpp that looks like this:
	// > SN76496Reset( &synth.service, 3579545, sample_rate );
	// >
	// > Instead, it should look like this:
	// > SN76496Reset( &synth.service, 4000000, sample_rate );
	// >
	// > That should fix it! Mind you, that was with the old code (it was
	// 0.72 I worked with) which may have been updated since, but the same
	// principle applies.
	//
	// NTS: I do not have anything to test this change! --J.C.
	//		SN76496Reset( &synth.service, 3579545, sample_rate );
#if 0
	SN76496Reset( &synth.service, 4000000, sample_rate );
#endif
}

void Ps1Audio::Close()
{
	if (!is_open)
		return;

	DEBUG_LOG_MSG("PS/1: Shutting down IBM PS/1 Audio card");
	// Stop the game from accessing the IO ports
	for (auto &handler : read_handlers)
		handler.Uninstall();
	for (auto &handler : write_handlers)
		handler.Uninstall();

	// Stop and remove the mixer callbacks
	if (dac.channel) {
		dac.channel->Enable(false);
		dac.channel.reset();
	}
	if (synth.channel) {
		synth.channel->Enable(false);
		synth.channel.reset();
	}
	ResetStates();
	is_open = false;
}

void Ps1Audio::Open()
{
	Close();
	// Setup the mixer callbacks
	using namespace std::placeholders;
	const auto dac_callback = std::bind(&Ps1Audio::UpdateDac, this, _1);
	const auto synth_callback = std::bind(&Ps1Audio::UpdateSynth, this, _1);

	dac.channel = mixer_channel_t(MIXER_AddChannel(dac_callback, 0, "PS1DAC"),
	                              MIXER_DelChannel);

	synth.channel = mixer_channel_t(MIXER_AddChannel(synth_callback, 0, "PS1"),
	                                MIXER_DelChannel);
	assert(dac.channel);
	assert(synth.channel);

	// Operate at native sampling rates
	sample_rate = dac.channel->GetSampleRate();

	// Register port handlers for 8-bit IO
	const auto read_from = std::bind(&Ps1Audio::ReadFromPort, this, _1, _2);
	read_handlers[0].Install(0x02F, read_from, IO_MB);
	read_handlers[1].Install(0x200, read_from, IO_MB);
	read_handlers[2].Install(0x202, read_from, IO_MB, 6);

	const auto write_to = std::bind(&Ps1Audio::WriteToPort, this, _1, _2, _3);
	write_handlers[0].Install(0x200, write_to, IO_MB);
	write_handlers[1].Install(0x202, write_to, IO_MB, 4);

	ResetStates();
	is_open = true;
	LOG_MSG("PS/1: Initialized IBM PS/1 Audio card");
}

static Ps1Audio ps1;

static void PS1AUDIO_ShutDown(MAYBE_UNUSED Section *sec)
{
	ps1.Close();
}

void PS1AUDIO_Init(Section *sec)
{
	assert(sec);
	Section_prop *section = static_cast<Section_prop *>(sec);
	if (!section->Get_bool("ps1audio"))
		return;

	ps1.Open();
	sec->AddDestroyFunction(&PS1AUDIO_ShutDown, true);
}
