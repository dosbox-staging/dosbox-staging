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
constexpr auto MAX_OUTPUT = 0x7fff;
constexpr auto STEP = 0x10000;

constexpr auto FIFOSIZE = 2048; // powers of two
constexpr auto FIFOSIZE_MASK = FIFOSIZE - 1;

constexpr auto FIFO_NEARLY_EMPTY_VAL = 128;
constexpr auto FIFO_NEARLY_FULL_VAL = FIFOSIZE - 128;

constexpr auto FRAC_SHIFT = 12; // Fixed precision

// Nearly full and half full flags (somewhere) on the SN74V2x5/IDT72V2x5 datasheet (just guessing on the hardware).
constexpr auto FIFO_HALF_FULL = 0x00;
constexpr auto FIFO_NEARLY_FULL = 0x00;

// High when the interrupt can't do anything but wait (cleared by reading 0200?).
constexpr auto FIFO_READ_AVAILABLE = 0x10;
constexpr auto FIFO_FULL = 0x08;  // High when we can't write any more.
constexpr auto FIFO_EMPTY = 0x04; // High when we can write direct values???

// High when we can write more to the dac_fifo (or, at least, there are 0x700 bytes
// free).
constexpr auto FIFO_NEARLY_EMPTY = 0x02;
constexpr auto FIFO_IRQ = 0x01; // High when IRQ was triggered by the DAC?

using mixer_channel_t = std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;

struct PS1AUDIO {
	// DOSBox interface objects
	IO_ReadHandleObject read_handlers[3] = {};
	IO_WriteHandleObject write_handlers[2] = {};
	uint32_t sample_rate = 0;

	// Three-voice synthesizer
#if 0
	struct SN76496 sn;
#endif
	bool is_synth_enabled = false;
	size_t synth_last_write = 0;
	mixer_channel_t synth_channel{nullptr, MIXER_DelChannel};

	// DAC
	mixer_channel_t dac_channel{nullptr, MIXER_DelChannel};
	bool is_dac_enabled = false;
	size_t dac_last_write = 0;
	uint8_t dac_fifo[FIFOSIZE] = {};
	uint16_t dac_read_index = 0;
	uint16_t dac_write_index = 0;
	bool is_dac_playing = false;
	bool can_trigger_irq = false;
	uint32_t dac_rate = 0;
	size_t dac_read_index_high = 0;	// dac_read_index << FRAC_SHIFT
	size_t dac_adder = 0; // Step << FRAC_SHIFT
	size_t dac_pending = 0; // Bytes to go << FRAC_SHIFT

	// Registers
	uint8_t reg_status = 0; // 0202 RD
	uint8_t reg_command = 0; // 0202 WR / 0200 RD
	uint8_t reg_data = 0; // 0200 WR
	uint8_t reg_divisor = 0; // 0203 WR
	uint8_t reg_unknown = 0; // 0204 WR (Reset?)
};

static struct PS1AUDIO ps1;

static uint8_t PS1SOUND_CalcStatus(void)
{
	uint8_t reg_status = ps1.reg_status & FIFO_IRQ;
	if( !ps1.dac_pending ) {
		reg_status |= FIFO_EMPTY;
	}
	if( ( ps1.dac_pending < ( FIFO_NEARLY_EMPTY_VAL << FRAC_SHIFT ) ) && ( ( ps1.reg_command & 3 ) == 3 ) ) {
		reg_status |= FIFO_NEARLY_EMPTY;
	}
	if( ps1.dac_pending > ( ( FIFOSIZE - 1 ) << FRAC_SHIFT ) ) {
//	if( ps1.dac_pending >= ( ( FIFOSIZE - 1 ) << FRAC_SHIFT ) ) { // OK
		// Should never be bigger than FIFOSIZE << FRAC_SHIFT...?
		reg_status |= FIFO_FULL;
	}
	if( ps1.dac_pending > ( FIFO_NEARLY_FULL_VAL << FRAC_SHIFT ) ) {
		reg_status |= FIFO_NEARLY_FULL;
	}
	if( ps1.dac_pending >= ( ( FIFOSIZE >> 1 ) << FRAC_SHIFT ) ) {
		reg_status |= FIFO_HALF_FULL;
	}
	return reg_status;
}

static void PS1DAC_Reset(bool bTotal)
{
	PIC_DeActivateIRQ( 7 );
	ps1.reg_data = 0x80;
	memset( ps1.dac_fifo, 0x80, FIFOSIZE );
	ps1.dac_read_index = 0;
	ps1.dac_write_index = 0;
	if( bTotal ) ps1.dac_rate = 0xFFFFFFFF;
	ps1.dac_read_index_high = 0;
	if( bTotal ) ps1.dac_adder = 0;	// Be careful with this, 5 second timeout and Space Quest 4!
	ps1.dac_pending = 0;
	ps1.reg_status = PS1SOUND_CalcStatus();
	ps1.is_dac_playing = true;
	ps1.can_trigger_irq = false;
}


#include "regs.h"
static void PS1SOUNDWrite(Bitu port,Bitu data,Bitu iolen) {
    (void)iolen;//UNUSED
	if( port != 0x0205 ) {
		ps1.dac_last_write=PIC_Ticks;
		if (!ps1.is_dac_enabled) {
			ps1.dac_channel->Enable(true);
			ps1.is_dac_enabled=true;
		}
	}
	else
	{
		ps1.synth_last_write=PIC_Ticks;
		if (!ps1.is_synth_enabled) {
			ps1.synth_channel->Enable(true);
			ps1.is_synth_enabled=true;
		}
	}

#if C_DEBUG != 0
	if( ( port != 0x0205 ) && ( port != 0x0200 ) )
		LOG_MSG("PS1 WR %04X,%02X (%04X:%08X)",(int)port,(int)data,(int)SegValue(cs),(int)reg_eip);
#endif
	switch(port)
	{
		case 0x0200:
			// reg_data - insert into dac_fifo.
			ps1.reg_data = (uint8_t)data;
			ps1.reg_status = PS1SOUND_CalcStatus();
			if( !( ps1.reg_status & FIFO_FULL ) )
			{
				ps1.dac_fifo[ ps1.dac_write_index++ ]=(uint8_t)data;
				ps1.dac_write_index &= FIFOSIZE_MASK;
				ps1.dac_pending += ( 1 << FRAC_SHIFT );
				if( ps1.dac_pending > ( FIFOSIZE << FRAC_SHIFT ) ) {
					ps1.dac_pending = FIFOSIZE << FRAC_SHIFT;
				}
			}
			break;
		case 0x0202:
			// reg_command.
			ps1.reg_command = (uint8_t)data;
			if( data & 3 ) ps1.can_trigger_irq = true;
//			switch( data & 3 )
//			{
//				case 0: // Stop?
//					ps1.dac_adder = 0;
//					break;
//			}
			break;
		case 0x0203:
			{
				// Clock divisor (maybe trigger first IRQ here).
				ps1.reg_divisor = (uint8_t)data;
				ps1.dac_rate = (uint32_t)( DAC_CLOCK / ( data + 1 ) );
				// 22050 << FRAC_SHIFT / 22050 = 1 << FRAC_SHIFT
				ps1.dac_adder = ( ps1.dac_rate << FRAC_SHIFT ) / (unsigned int)ps1.sample_rate;
				if( ps1.dac_rate > 22050 )
				{
//					if( ( ps1.reg_command & 3 ) == 3 ) {
//						LOG_MSG("Attempt to set DAC rate too high (%dhz).",ps1.dac_rate);
//					}
					//ps1.reg_divisor = 0x2C;
					//ps1.dac_rate = 22050;
					//ps1.dac_adder = 0;	// Not valid.
				}
				ps1.reg_status = PS1SOUND_CalcStatus();
				if( ( ps1.reg_status & FIFO_NEARLY_EMPTY ) && ( ps1.can_trigger_irq ) )
				{
					// Generate request for stuff.
					ps1.reg_status |= FIFO_IRQ;
					ps1.can_trigger_irq = false;
					PIC_ActivateIRQ( 7 );
				}
			}
			break;
		case 0x0204:
			// Reset? (PS1MIC01 sets it to 08 for playback...)
			ps1.reg_unknown = (uint8_t)data;
			if( !data )
				PS1DAC_Reset(true);
			break;
		case 0x0205:
#if 0
			SN76496Write(&ps1.sn,port,data);
#endif
			break;
		default:break;
	}
}

static Bitu PS1SOUNDRead(Bitu port,Bitu iolen) {
    (void)iolen;//UNUSED
	ps1.dac_last_write=PIC_Ticks;
	if (!ps1.is_dac_enabled) {
		ps1.dac_channel->Enable(true);
		ps1.is_dac_enabled=true;
	}
#if C_DEBUG != 0
	LOG_MSG("PS1 RD %04X (%04X:%08X)",(int)port,(int)SegValue(cs),(int)reg_eip);
#endif
	switch(port)
	{
		case 0x0200:
			// Read last command.
			ps1.reg_status &= ~FIFO_READ_AVAILABLE;
			return ps1.reg_command;
		case 0x0202:
			{
//				LOG_MSG("PS1 RD %04X (%04X:%08X)",port,SegValue(cs),reg_eip);

				// Read status / clear IRQ?.
				uint8_t reg_status = ps1.reg_status = PS1SOUND_CalcStatus();
// Don't do this until we have some better way of detecting the triggering and ending of an IRQ.
//				ps1.reg_status &= ~FIFO_IRQ;
				return reg_status;
			}
		case 0x0203:
			// Stunt Island / Roger Rabbit 2 setup.
			return ps1.reg_divisor;
		case 0x0205:
		case 0x0206:
			// Bush Buck detection.
			return 0;
		default:break;
	}
	return 0xFF;
}

static void PS1SOUNDUpdate(Bitu length)
{
	if ((ps1.dac_last_write+5000)<PIC_Ticks) {
		ps1.is_dac_enabled=false;
		ps1.dac_channel->Enable(false);
		// Excessive?
		PS1DAC_Reset(false);
	}
	uint8_t * buffer=(uint8_t *)MixTemp;

	Bits pending = 0;
	Bitu add = 0;
	Bitu pos=ps1.dac_read_index_high;
	Bitu count=length;

	if( ps1.is_dac_playing )
	{
		ps1.reg_status = PS1SOUND_CalcStatus();
		pending = (Bits)ps1.dac_pending;
		add = ps1.dac_adder;
		if( ( ps1.reg_status & FIFO_NEARLY_EMPTY ) && ( ps1.can_trigger_irq ) )
		{
			// More bytes needed.

			//PIC_AddEvent( ??, ??, ?? );
			ps1.reg_status |= FIFO_IRQ;
			ps1.can_trigger_irq = false;
			PIC_ActivateIRQ( 7 );
		}
	}

	while (count)
	{
		unsigned int out;

		if( pending <= 0 ) {
			pending = 0;
			while( count-- ) *(buffer++) = 0x80;	// Silence.
			break;
			//pos = ( ( ps1.dac_read_index - 1 ) & FIFOSIZE_MASK ) << FRAC_SHIFT;	// Stay on last byte.
		}
		else
		{
			out = ps1.dac_fifo[ pos >> FRAC_SHIFT ];
			pos += add;
			pos &= ( ( FIFOSIZE << FRAC_SHIFT ) - 1 );
			pending -= (Bits)add;
		}

		*(buffer++) = out;
		count--;
	}
	// Update positions and see if we can clear the FIFO_FULL flag.
	ps1.dac_read_index_high = pos;
//	if( ps1.dac_read_index != ( pos >> FRAC_SHIFT ) ) ps1.reg_status &= ~FIFO_FULL;
	ps1.dac_read_index = (uint16_t)(pos >> FRAC_SHIFT);
	if( pending < 0 ) pending = 0;
	ps1.dac_pending = (Bitu)pending;

	ps1.dac_channel->AddSamples_m8(length,MixTemp);
}

static void PS1SN76496Update(Bitu length)
{
	if ((ps1.synth_last_write+5000)<PIC_Ticks) {
		ps1.is_synth_enabled=false;
		ps1.synth_channel->Enable(false);
	}

	//int16_t * buffer=(int16_t *)MixTemp;
#if 0
	SN76496Update(&ps1.sn,buffer,length);
#endif
	ps1.synth_channel->AddSamples_m16(length,(int16_t *)MixTemp);
}

#include "regs.h"
//static void PS1SOUND_Write(Bitu port,Bitu data,Bitu iolen) {
//	LOG_MSG("Write PS1 dac %X val %X (%04X:%08X)",port,data,SegValue(cs),reg_eip);
//}

static uint8_t ps1_audio_present(MAYBE_UNUSED uint16_t port, MAYBE_UNUSED uint16_t iolen) {
	return 0xff;
}

static void reset_states()
{
	// Initialize the PS/1 states
	ps1.is_dac_enabled = false;
	ps1.is_synth_enabled = false;
	ps1.dac_last_write = 0;
	ps1.synth_last_write = 0;
	PS1DAC_Reset(true);

	// > Jmk wrote:
	// > Judging by what I've read in that technical document, it looks like
	// the sound chip is fed by a 4 Mhz clock instead of a ~3.5 Mhz clock.
	// >
	// > So, there's a line in ps1_sound.cpp that looks like this:
	// > SN76496Reset( &ps1.sn, 3579545, sample_rate );
	// >
	// > Instead, it should look like this:
	// > SN76496Reset( &ps1.sn, 4000000, sample_rate );
	// >
	// > That should fix it! Mind you, that was with the old code (it was
	// 0.72 I worked with) which may have been updated since, but the same
	// principle applies.
	//
	// NTS: I do not have anything to test this change! --J.C.
	//		SN76496Reset( &ps1.sn, 3579545, sample_rate );
#if 0
	SN76496Reset( &ps1.sn, 4000000, sample_rate );
#endif
}

static void PS1SOUND_ShutDown(MAYBE_UNUSED Section *sec)
{
	DEBUG_LOG_MSG("PS/1: Shutting down IBM PS/1 Audio card");

	// Stop the game from accessing the IO ports
	for (auto &handler : ps1.read_handlers)
		handler.Uninstall();
	for (auto &handler : ps1.write_handlers)
		handler.Uninstall();

	// Stop and remove the mixer callbacks
	if (ps1.dac_channel) {
		ps1.dac_channel->Enable(false);
		ps1.dac_channel.reset();
	}
	if (ps1.synth_channel) {
		ps1.synth_channel->Enable(false);
		ps1.synth_channel.reset();
	}
	reset_states();
}

void PS1SOUND_Init(Section *sec)
{
	Section_prop *section = static_cast<Section_prop *>(sec);
	assert(section);
	if (!section->Get_bool("ps1audio"))
		return;

	// Setup the mixer callbacks
	ps1.dac_channel = mixer_channel_t(MIXER_AddChannel(PS1SOUNDUpdate, 0, "PS1DAC"),
	                              MIXER_DelChannel);
	ps1.synth_channel = mixer_channel_t(MIXER_AddChannel(PS1SN76496Update, 0, "PS1"),
	                             MIXER_DelChannel);
	assert(ps1.dac_channel);
	assert(ps1.synth_channel);

	// Operate at native sampling rates
	ps1.sample_rate = ps1.dac_channel->GetSampleRate();

	// Register port handlers for 8-bit IO
	ps1.read_handlers[0].Install(0x200, &PS1SOUNDRead, IO_MB);
	ps1.read_handlers[1].Install(0x202, &PS1SOUNDRead, IO_MB, 6); // 5); //3);
	ps1.read_handlers[2].Install(0x02F, &ps1_audio_present, IO_MB);

	ps1.write_handlers[0].Install(0x200, PS1SOUNDWrite, IO_MB);
	ps1.write_handlers[1].Install(0x202, PS1SOUNDWrite, IO_MB, 4);

	reset_states();

	sec->AddDestroyFunction(&PS1SOUND_ShutDown, true);

	LOG_MSG("PS/1: Initialized IBM PS/1 Audio card");
}
