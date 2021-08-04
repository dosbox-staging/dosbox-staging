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

/* 
	Based of sn76496.c of the M.A.M.E. project
*/

/*
A note about accurately emulating the Tandy's digital to analog (DAC) behavior
------------------------------------------------------------------------------
The Tandy's DAC is responsible for converting digital audio samples into their
analog equivalents in the form of voltage levels output to the line-out or
speaker.

After playing a sequence of samples, such as a sound effect, the last value fed
into the DAC will correspond to the final voltage set in the line-out.

Well behaved audio sequences end with zero amplitude and thus leave the speaker
in the neutral position (without a positive or negative voltage); and this is
what we see in practice.

However, Price of Persia uniquely terminates most of its sound effects with a
non-zero amplitude, leaving the DAC holding a non-zero voltage, which is also
called a DC-offset.

The Tandy controller mitigates DC-offset by incrementally stepping the DAC's
output voltage back toward the neutral centerline. This DAC ramp-down behavior
is audible as an artifact post-fixed onto every Prince of Persia sound effect,
which sounds like a soft, short-lived shoe squeak.

This hardware behavior can be emulated by tracking the last sample played,
checking if it's at the centerline or not (centerline being 128, in the range of
unisigned 8-bit values).  If it's not at the centerline, then steading generate
new samples than trend this last-played value toward the centerline until it's
reached.

The DOSBox-X project has faithfully [*] replicated this hardware artifact using
the following code-snippet:

if (tandy.dac.dma.last_sample != 128) {
        for (Bitu ct=0; ct < length; ct++) {
                tandy.dac.chan->AddSamples_m8(1,&tandy.dac.dma.last_sample);
                if (tandy.dac.dma.last_sample != 128)
                        tandy.dac.dma.last_sample =
(Bit8u)(((((int)tandy.dac.dma.last_sample - 128) * 63) / 64) + 128);
        }
}

[*] As the author Jonathan Campbell explains, "I used a VGA capture card and an
MCE2VGA to capture the output of a real Tandy 1000, and then tried to adjust the
Tandy DAC code to match the artifact after each step."

The implementation below prioritizes the game author's intended audio score
ahead of deleterious artifacts caused by hardware defects or limitations.
Because the sound caused by the Tandy's DAC is not part of the game's audio
score, we deliberately omit this behavior and terminate sequences at the
centerline.
*/

#include "dosbox.h"

#include <algorithm>
#include <cstring>
#include <math.h>

#include "dma.h"
#include "hardware.h"
#include "inout.h"
#include "mem.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"

#include "mame/emu.h"
#include "mame/sn76496.h"

constexpr int SOUND_CLOCK = 14318180 / 4;
constexpr uint16_t TDAC_DMA_BUFSIZE = 1024;

static struct {
	MixerChannel *chan = nullptr;
	bool enabled = false;
	Bitu last_write = 0u;
	struct {
		MixerChannel *chan = nullptr;
		bool enabled = false;
		struct {
			Bitu base = 0u;
			Bit8u irq = 0u;
			Bit8u dma = 0u;
		} hw;
		struct {
			Bitu rate = 0u;
			Bit8u buf[TDAC_DMA_BUFSIZE] = {};
			DmaChannel *chan = nullptr;
			bool transfer_done = false;
		} dma;
		Bit8u mode = 0u;
		Bit8u control = 0u;
		Bit16u frequency = 0u;
		Bit8u amplitude = 0u;
		bool irq_activated = false;
	} dac = {};
} tandy = {};

static sn76496_device device_sn76496(machine_config(), 0, 0, SOUND_CLOCK );
static ncr8496_device device_ncr8496(machine_config(), 0, 0, SOUND_CLOCK);

static sn76496_base_device* activeDevice = &device_ncr8496;
#define device (*activeDevice)

static void SN76496Write(io_port_t, uint8_t data, io_width_t)
{
	tandy.last_write = PIC_Ticks;
	if (!tandy.enabled && tandy.chan) {
		tandy.chan->Enable(true);
		tandy.enabled=true;
	}
	device.write(data);

	//	LOG_MSG("3voice write %#" PRIxPTR " at time
	//%7.3f",data,PIC_FullIndex());
}

static void SN76496Update(uint16_t length)
{
	if (!tandy.chan)
		return;

	// Disable the channel if it's been quiet for a while
	if ((tandy.last_write + 5000) < PIC_Ticks) {
		tandy.enabled=false;
		tandy.chan->Enable(false);
		return;
	}
	const Bitu MAX_SAMPLES = 2048;
	if (length > MAX_SAMPLES)
		return;
	Bit16s buffer[MAX_SAMPLES];
	Bit16s* outputs = buffer;

	device_sound_interface::sound_stream stream;
	static_cast<device_sound_interface&>(device).sound_stream_update(stream, 0, &outputs, length);
	tandy.chan->AddSamples_m16(length, buffer);
}

bool TS_Get_Address(Bitu& tsaddr, Bitu& tsirq, Bitu& tsdma) {
	tsaddr=0;
	tsirq =0;
	tsdma =0;
	if (tandy.dac.enabled) {
		tsaddr=tandy.dac.hw.base;
		tsirq =tandy.dac.hw.irq;
		tsdma =tandy.dac.hw.dma;
		return true;
	}
	return false;
}


static void TandyDAC_DMA_CallBack(DmaChannel * /*chan*/, DMAEvent event) {
	if (event == DMA_REACHED_TC) {
		tandy.dac.dma.transfer_done=true;
		PIC_ActivateIRQ(tandy.dac.hw.irq);
	}
}

static void TandyDACModeChanged()
{
	if (!tandy.dac.chan) {
		DEBUG_LOG_MSG("TANDY: Skipping mode change until the DAC is "
		              "initialized");
		return;
	}

	switch (tandy.dac.mode & 3) {
	case 0:
		// joystick mode
		break;
	case 1:
		break;
	case 2:
		// recording
		break;
	case 3:
		// playback
		tandy.dac.chan->FillUp();
		if (tandy.dac.frequency!=0) {
			float freq=3579545.0f/((float)tandy.dac.frequency);
			tandy.dac.chan->SetFreq((Bitu)freq);
			float vol=((float)tandy.dac.amplitude)/7.0f;
			tandy.dac.chan->SetVolume(vol,vol);
			if ((tandy.dac.mode&0x0c)==0x0c) {
				tandy.dac.dma.transfer_done=false;
				tandy.dac.dma.chan=GetDMAChannel(tandy.dac.hw.dma);
				if (tandy.dac.dma.chan) {
					tandy.dac.dma.chan->Register_Callback(TandyDAC_DMA_CallBack);
					tandy.dac.chan->Enable(true);
//					LOG_MSG("Tandy DAC: playback started with freqency %f, volume %f",freq,vol);
				}
			}
		}
		break;
	}
}

static void TandyDACDMAEnabled()
{
	TandyDACModeChanged();
}

static void TandyDACDMADisabled()
{}

static void TandyDACWrite(io_port_t port, uint8_t data, io_width_t)
{
	switch (port) {
	case 0xc4: {
		Bitu oldmode = tandy.dac.mode;
		tandy.dac.mode = data;
		if ((data&3)!=(oldmode&3)) {
			TandyDACModeChanged();
		}
		if (((data&0x0c)==0x0c) && ((oldmode&0x0c)!=0x0c)) {
			TandyDACDMAEnabled();
		} else if (((data&0x0c)!=0x0c) && ((oldmode&0x0c)==0x0c)) {
			TandyDACDMADisabled();
		}
		}
		break;
	case 0xc5:
		switch (tandy.dac.mode&3) {
		case 0:
			// joystick mode
			break;
		case 1: tandy.dac.control = data; break;
		case 2:
			break;
		case 3:
			// direct output
			break;
		}
		break;
	case 0xc6:
		tandy.dac.frequency = (tandy.dac.frequency & 0xf00) | data;
		switch (tandy.dac.mode&3) {
		case 0:
			// joystick mode
			break;
		case 1:
		case 2:
		case 3:
			TandyDACModeChanged();
			break;
		}
		break;
	case 0xc7:
		tandy.dac.frequency = static_cast<uint16_t>((tandy.dac.frequency & 0x00ff) | ((data & 0xf) << 8));
		tandy.dac.amplitude = data >> 5;
		switch (tandy.dac.mode & 3) {
		case 0:
			// joystick mode
			break;
		case 1:
		case 2:
		case 3:
			TandyDACModeChanged();
			break;
		}
		break;
	}
}

static uint8_t TandyDACRead(io_port_t port, io_width_t)
{
	switch (port) {
	case 0xc4:
		return (tandy.dac.mode&0x77) | (tandy.dac.irq_activated ? 0x08 : 0x00);
	case 0xc6: return static_cast<uint8_t>(tandy.dac.frequency & 0xff);
	case 0xc7:
		return static_cast<uint8_t>(((tandy.dac.frequency >> 8) & 0xf) | (tandy.dac.amplitude << 5));
	}
	LOG_MSG("Tandy DAC: Read from unknown %x", port);
	return 0xff;
}

static void TandyDACUpdate(uint16_t requested)
{
	if (!tandy.dac.chan || !tandy.dac.dma.chan) {
		DEBUG_LOG_MSG(
		        "TANDY: Skipping update until the DAC is initialized");
		return;
	}

	const bool should_read = tandy.dac.enabled &&
	                         (tandy.dac.mode & 0x0c) == 0x0c &&
	                         !tandy.dac.dma.transfer_done;

	auto buf = tandy.dac.dma.buf;
	while (requested) {
		const auto bytes_to_read = std::min(requested, TDAC_DMA_BUFSIZE);

		auto actual = should_read ? tandy.dac.dma.chan->Read(bytes_to_read, buf) : 0u;

		// If we came up short, move back one to terminate the tail in silence
		if (actual && actual < bytes_to_read)
			actual--;
		memset(buf + actual, 128u, bytes_to_read - actual);

		// Always write the requested quantity regardless of read status
		tandy.dac.chan->AddSamples_m8(bytes_to_read, buf);
		requested -= bytes_to_read;
	}
}

class TANDYSOUND final : public Module_base {
private:
	IO_WriteHandleObject WriteHandler[4];
	IO_ReadHandleObject ReadHandler[4];
	MixerObject MixerChan;
	MixerObject MixerChanDAC;
public:
	TANDYSOUND(Section *configuration)
	        : Module_base(configuration),
	          MixerChan(),
	          MixerChanDAC()
	{
		Section_prop *section = static_cast<Section_prop *>(configuration);

		bool enable_hw_tandy_dac=true;
		uint16_t sbport;
		uint8_t sbirq;
		uint8_t sbdma;
		if (SB_Get_Address(sbport, sbirq, sbdma)) {
			enable_hw_tandy_dac=false;
		}

		//Select the correct tandy chip implementation
		if (machine == MCH_PCJR) activeDevice = &device_sn76496;
		else activeDevice = &device_ncr8496;

		real_writeb(0x40,0xd4,0x00);
		if (IS_TANDY_ARCH) {
			/* enable tandy sound if tandy=true/auto */
			if ((strcmp(section->Get_string("tandy"),"true")!=0) &&
				(strcmp(section->Get_string("tandy"),"on")!=0) &&
				(strcmp(section->Get_string("tandy"),"auto")!=0)) return;
		} else {
			/* only enable tandy sound if tandy=true */
			if ((strcmp(section->Get_string("tandy"),"true")!=0) &&
				(strcmp(section->Get_string("tandy"),"on")!=0)) return;

			/* ports from second DMA controller conflict with tandy ports */
			CloseSecondDMAController();

			if (enable_hw_tandy_dac) {
				WriteHandler[2].Install(0x1e0, SN76496Write, io_width_t::byte, 2);
				WriteHandler[3].Install(0x1e4, TandyDACWrite, io_width_t::byte, 4);
				//				ReadHandler[3].Install(0x1e4,TandyDACRead,io_width_t::byte,4);
			}
		}

		const auto sample_rate = static_cast<uint32_t>(section->Get_int("tandyrate"));
		tandy.chan=MixerChan.Install(&SN76496Update,sample_rate,"TANDY");

		WriteHandler[0].Install(0xc0, SN76496Write, io_width_t::byte, 2);

		if (enable_hw_tandy_dac) {
			// enable low-level Tandy DAC emulation
			WriteHandler[1].Install(0xc4, TandyDACWrite, io_width_t::byte, 4);
			ReadHandler[1].Install(0xc4, TandyDACRead, io_width_t::byte, 4);

			tandy.dac.enabled=true;
			tandy.dac.chan=MixerChanDAC.Install(&TandyDACUpdate,sample_rate,"TANDYDAC");

			tandy.dac.hw.base=0xc4;
			tandy.dac.hw.irq =7;
			tandy.dac.hw.dma =1;
		} else {
			tandy.dac.enabled=false;
			tandy.dac.hw.base=0;
			tandy.dac.hw.irq =0;
			tandy.dac.hw.dma =0;
		}

		tandy.dac.control=0;
		tandy.dac.mode   =0;
		tandy.dac.irq_activated=false;
		tandy.dac.frequency=0;
		tandy.dac.amplitude = 0;

		tandy.enabled=false;
		real_writeb(0x40,0xd4,0xff);	/* BIOS Tandy DAC initialization value */

		((device_t&)device).device_start();
		device.convert_samplerate(static_cast<int>(sample_rate));
	}
	~TANDYSOUND(){ }
};

static TANDYSOUND* test;

void TANDYSOUND_ShutDown(Section* /*sec*/) {
	delete test;	
}

void TANDYSOUND_Init(Section* sec) {
	test = new TANDYSOUND(sec);
	sec->AddDestroyFunction(&TANDYSOUND_ShutDown,true);
}
