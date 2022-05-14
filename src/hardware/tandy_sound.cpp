/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2019-2022  The DOSBox Staging Team
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

if (dma.last_sample != 128) {
        for (Bitu ct=0; ct < length; ct++) {
                channel->AddSamples_m8(1,&dma.last_sample);
                if (dma.last_sample != 128)
                        dma.last_sample =
(uint8_t)(((((int)dma.last_sample - 128) * 63) / 64) + 128);
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
#include <array>
#include <queue>
#include <string_view>

#include "dma.h"
#include "hardware.h"
#include "inout.h"
#include "mem.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"

#include "mame/emu.h"
#include "mame/sn76496.h"
#include "../libs/residfp/resample/TwoPassSincResampler.h"

using namespace std::placeholders;

// Constants
constexpr auto ms_per_s = 1000;
constexpr auto tandy_psg_clock_hz = 14318180 / 4;
constexpr auto render_divisor = 16;
constexpr auto render_rate_hz = ceil_sdivide(tandy_psg_clock_hz, render_divisor);
constexpr uint16_t card_base_offset = 288;

enum class ConfigProfile {
	TandySystem,
	PCjrSystem,
	SoundCardOnly,
	SoundCardRemoved,
};

class TandyDAC {
	struct IOConfig {
		uint16_t base = 0;
		uint8_t irq = 0;
		uint8_t dma = 0;
	};
	struct DMA {
		std::array<uint8_t, 128> fifo = {};
		DmaChannel *channel = nullptr;
		bool is_done = false;
	};
	struct Registers {
		uint16_t frequency = 0;
		uint8_t mode = 0;
		uint8_t control = 0;
		uint8_t amplitude = 0;
		bool irq_activated = false;
	};

public:
	TandyDAC(const ConfigProfile config_profile);
	bool IsEnabled() const { return is_enabled; }
	const IOConfig &GetIOConfig() const { return io; }

private:
	void ChangeMode();
	void DmaCallback(DmaChannel *chan, DMAEvent event);
	uint8_t ReadFromPort(io_port_t port, io_width_t);
	void WriteToPort(io_port_t port, io_val_t value, io_width_t);
	void AudioCallback(uint16_t requested);
	TandyDAC() = delete;

	DMA dma = {};
	const IOConfig io = {0xc4, 7, 1};

	// Managed objects
	mixer_channel_t channel = nullptr;
	IO_ReadHandleObject read_handler = {};
	IO_WriteHandleObject write_handlers[2] = {};

	// States
	Registers regs = {};
	int sample_rate = 0;
	bool is_enabled = false;
};

class TandyPSG {
public:
	TandyPSG(const ConfigProfile config_profile, const bool is_dac_enabled);

private:
	TandyPSG() = delete;
	TandyPSG(const TandyPSG &) = delete;
	TandyPSG &operator=(const TandyPSG &) = delete;

	bool RenderOnce();
	void RenderForMs(const double duration_ms);
	int16_t GetSample();
	double ConvertFramesToMs(const int frames);
	void WriteToPort(io_port_t, io_val_t value, io_width_t);
	void AudioCallback(uint16_t requested_frames);

	static constexpr int16_t idle_after_ms = 200;
	int16_t idle_after_silent_samples = 0;

	// Managed objects
	mixer_channel_t channel = nullptr;
	IO_WriteHandleObject write_handlers[2] = {};
	std::unique_ptr<sn76496_base_device> device = {};
	std::unique_ptr<reSIDfp::TwoPassSincResampler> resampler = {};
	std::queue<int16_t> fifo = {};
	device_sound_interface *dsi = nullptr;

	// States
	double render_to_play_ratio = 0.0;
	double last_render_time = 0.0;
	int16_t unwritten_for_ms = 0;
	int16_t silent_samples = 0;
	bool is_enabled = false;
};

TandyDAC::TandyDAC(const ConfigProfile config_profile)
{
	assert(config_profile != ConfigProfile::SoundCardRemoved);

	// Run the audio channel at the mixer's native rate
	const auto callback = std::bind(&TandyDAC::AudioCallback, this, _1);
	channel = MIXER_AddChannel(callback, 0, "TANDYDAC");
	sample_rate = channel->GetSampleRate();

	// Register DAC per-port read handlers
	const auto reader = std::bind(&TandyDAC::ReadFromPort, this, _1, _2);
	read_handler.Install(io.base, reader, io_width_t::byte, 4);

	// Register DAC per-port write handlers
	const auto writer = std::bind(&TandyDAC::WriteToPort, this, _1, _2, _3);
	write_handlers[0].Install(io.base, writer, io_width_t::byte, 4);

	if (config_profile == ConfigProfile::SoundCardOnly)
		write_handlers[1].Install(io.base + card_base_offset, writer,
		                          io_width_t::byte, 4);

	is_enabled = true;
}

void TandyDAC::DmaCallback([[maybe_unused]] DmaChannel *, DMAEvent event)
{
	// LOG_MSG("TandyDAC: DMA event %d", event);
	if (event != DMA_REACHED_TC)
		return;
	dma.is_done = true;
	PIC_ActivateIRQ(io.irq);
}

void TandyDAC::ChangeMode()
{
	// Avoid under or overruning the mixer with invalid frequencies
	// Typically frequencies are in the 8 to 22 Khz range
	constexpr auto dac_min_freq_hz = 4900;
	constexpr auto dac_max_freq_hz = 49000;

	// LOG_MSG("TandyDAC: Mode changed to %d", regs.mode);
	switch (regs.mode & 3) {
	case 0: // joystick mode
	case 1:
	case 2: // recording
		break;
	case 3: // playback
		if (!regs.frequency)
			return;
		if (const auto freq = tandy_psg_clock_hz / regs.frequency;
		    freq > dac_min_freq_hz && freq < dac_max_freq_hz) {
			assert(channel);
			channel->FillUp(); // using the prior frequency
			channel->SetFreq(freq);
			const auto vol = static_cast<float>(regs.amplitude) / 7.0f;
			channel->SetVolume(vol, vol);
			if ((regs.mode & 0x0c) == 0x0c) {
				dma.is_done = false;
				dma.channel = GetDMAChannel(io.dma);
				if (dma.channel) {
					const auto callback =
					        std::bind(&TandyDAC::DmaCallback,
					                  this, _1, _2);
					dma.channel->Register_Callback(callback);
					channel->Enable(true);
					// LOG_MSG("Tandy DAC: playback started
					// with freqency %f, volume %f", freq,
					// vol);
				}
			}
		}
		break;
	}
}

uint8_t TandyDAC::ReadFromPort(io_port_t port, io_width_t)
{
	// LOG_MSG("TandyDAC: Read from port %04x", port);
	switch (port) {
	case 0xc4:
		return (regs.mode & 0x77) | (regs.irq_activated ? 0x08 : 0x00);
	case 0xc6: return static_cast<uint8_t>(regs.frequency & 0xff);
	case 0xc7:
		return static_cast<uint8_t>(((regs.frequency >> 8) & 0xf) |
		                            (regs.amplitude << 5));
	}
	LOG_MSG("Tandy DAC: Read from unknown %x", port);
	return 0xff;
}

void TandyDAC::WriteToPort(io_port_t port, io_val_t value, io_width_t)
{
	auto data = check_cast<uint8_t>(value);
	const auto previous_mode = regs.mode;

	switch (port) {
	case 0xc4:
		regs.mode = data;
		if ((data & 3) != (previous_mode & 3)) {
			ChangeMode();
		}
		if ((data & 0x0c) == 0x0c && (previous_mode & 0x0c) != 0x0c) {
			ChangeMode(); // DAC DMA is enabled
		} else if ((data & 0x0c) != 0x0c && (previous_mode & 0x0c) == 0x0c) {
			// DAC DMA is disabled
		}
		break;
	case 0xc5:
		switch (regs.mode & 3) {
		case 1: regs.control = data; break;
		case 0: // joystick mode
		case 2:
		case 3: // direct output
			break;
		}
		break;
	case 0xc6:
		regs.frequency = (regs.frequency & 0xf00) | data;
		switch (regs.mode & 3) {
		case 0: // joystick mode
			break;
		case 1:
		case 2:
		case 3: ChangeMode(); break;
		}
		break;
	case 0xc7:
		regs.frequency = static_cast<uint16_t>((regs.frequency & 0x00ff) |
		                                       ((data & 0xf) << 8));
		regs.amplitude = data >> 5;
		switch (regs.mode & 3) {
		case 0:
			// joystick mode
			break;
		case 1:
		case 2:
		case 3: ChangeMode(); break;
		}
		break;
	}
	// LOG_MSG("TandyDAC: Write %02x to port %04x", data, port);
	// LOG_MSG("TandyDAC: Mode %02x, Control %02x, Frequency %04x, Amplitude
	// %02x",
	//        regs.mode, regs.control, regs.frequency, regs.amplitude);
}

void TandyDAC::AudioCallback(uint16_t requested)
{
	if (!channel || !dma.channel) {
		DEBUG_LOG_MSG(
		        "TANDY: Skipping update until the DAC is initialized");
		return;
	}
	const bool should_read = is_enabled && (regs.mode & 0x0c) == 0x0c &&
	                         !dma.is_done;

	const auto buf = dma.fifo.data();
	const auto buf_size = check_cast<uint16_t>(dma.fifo.size());
	while (requested) {
		const auto bytes_to_read = std::min(requested, buf_size);

		auto actual = should_read ? dma.channel->Read(bytes_to_read, buf)
		                          : 0u;

		// If we came up short, move back one to terminate the tail in silence
		if (actual && actual < bytes_to_read)
			actual--;
		memset(buf + actual, 128u, bytes_to_read - actual);

		// Always write the requested quantity regardless of read status
		channel->AddSamples_m8(bytes_to_read, buf);
		requested -= bytes_to_read;
	}
}

TandyPSG::TandyPSG(const ConfigProfile config_profile, const bool is_dac_enabled)
{
	assert(config_profile != ConfigProfile::SoundCardRemoved);

	// Instantiate the MAME PSG device
	constexpr auto rounded_psg_clock = render_rate_hz * render_divisor;
	if (config_profile == ConfigProfile::PCjrSystem)
		device = std::make_unique<sn76496_device>(machine_config(),
		                                          "SN76489", nullptr,
		                                          rounded_psg_clock);
	else
		device = std::make_unique<ncr8496_device>(machine_config(),
		                                          "NCR 8496", nullptr,
		                                          rounded_psg_clock);
	// Register the write ports
	constexpr io_port_t base_addr = 0xc0;
	const auto writer = std::bind(&TandyPSG::WriteToPort, this, _1, _2, _3);

	write_handlers[0].Install(base_addr, writer, io_width_t::byte, 2);

	if (config_profile == ConfigProfile::SoundCardOnly && is_dac_enabled)
		write_handlers[1].Install(base_addr + card_base_offset, writer,
		                          io_width_t::byte, 2);

	// Run the audio channel at the mixer's native rate
	const auto callback = std::bind(&TandyPSG::AudioCallback, this, _1);
	channel = MIXER_AddChannel(callback, 0, "TANDY");

	// Setup the resampler
	const auto sample_rate = channel->GetSampleRate();
	const auto max_freq = std::max(sample_rate * 0.9 / 2, 8000.0);
	resampler.reset(reSIDfp::TwoPassSincResampler::create(render_rate_hz,
	                                                      sample_rate, max_freq));
	render_to_play_ratio = static_cast<double>(render_rate_hz) / sample_rate;

	// Compute how many silent samples before idling the PSG
	idle_after_silent_samples = check_cast<int16_t>(
	        sample_rate * idle_after_ms / ms_per_s);

	// Configure and start the MAME device
	dsi = static_cast<device_sound_interface *>(device.get());
	const auto base_device = static_cast<device_t *>(device.get());
	base_device->device_start();
	device->convert_samplerate(render_rate_hz);

	LOG_MSG("TANDY: Initialized audio card with a TI %s PSG %s",
	        base_device->shortName,
	        is_dac_enabled ? "and 8-bit DAC"
	                       : "but no DAC, because a Sound Blaster is present");
}

bool TandyPSG::RenderOnce()
{
	assert(dsi);
	assert(resampler);
	static int16_t sample;
	static int16_t *frame[] = {&sample, nullptr};
	static device_sound_interface::sound_stream ss;
	dsi->sound_stream_update(ss, nullptr, frame, 1);
	return resampler->input(sample);
}

int16_t TandyPSG::GetSample()
{
	const auto sample = resampler->output();
	if (!sample) {
		++silent_samples;
		return 0;
	}
	silent_samples = 0;
	return static_cast<int16_t>(clamp(sample, MIN_AUDIO, MAX_AUDIO));
}

void TandyPSG::RenderForMs(const double duration_ms)
{
	auto render_count = iround(duration_ms * render_rate_hz / ms_per_s);
	while (render_count-- > 0)
		if (RenderOnce())
			fifo.push(GetSample());
}

void TandyPSG::WriteToPort(io_port_t, io_val_t value, io_width_t)
{
	const auto now = PIC_FullIndex();
	if (!is_enabled) {
		assert(channel);
		channel->Enable(true);
		is_enabled = true;
	} else {
		RenderForMs(now - last_render_time);
	}
	last_render_time = now;
	const auto data = check_cast<uint8_t>(value);
	device->write(data);
	unwritten_for_ms = 0;
}

double TandyPSG::ConvertFramesToMs(const int frames)
{
	return (frames * render_to_play_ratio * ms_per_s) / render_rate_hz;
}

void TandyPSG::AudioCallback(uint16_t requested_frames)
{
	if (!channel)
		return;

	while (requested_frames && fifo.size()) {
		channel->AddSamples_m16(1, &fifo.front());
		fifo.pop();
		--requested_frames;
	}
	if (requested_frames) {
		last_render_time += ConvertFramesToMs(requested_frames);
		while (requested_frames--) {
			while (!RenderOnce())
				; // loop until we get a frame
			const auto frame = GetSample();
			channel->AddSamples_m16(1, &frame);
		}
	}

	if (unwritten_for_ms++ > idle_after_ms &&
	    silent_samples > idle_after_silent_samples) {
		channel->Enable(false);
		is_enabled = false;
	}
}

// The Tandy DAC and PSG (programmable sound generator) managed pointers
std::unique_ptr<TandyDAC> tandy_dac = {};
std::unique_ptr<TandyPSG> tandy_psg = {};

bool TS_Get_Address(Bitu &tsaddr, Bitu &tsirq, Bitu &tsdma)
{
	if (!tandy_dac || !tandy_dac->IsEnabled()) {
		tsaddr = 0;
		tsirq = 0;
		tsdma = 0;
		return false;
	}

	assert(tandy_dac && tandy_dac->IsEnabled());
	const auto io = tandy_dac->GetIOConfig();
	tsaddr = io.base;
	tsirq = io.irq;
	tsdma = io.dma;
	return true;
}

static bool is_sound_blaster_absent()
{
	uint16_t sbport;
	uint8_t sbirq;
	uint8_t sbdma;
	return (SB_Get_Address(sbport, sbirq, sbdma) == false);
}

static void set_tandy_sound_flag_in_bios(const bool is_enabled)
{
	real_writeb(0x40, 0xd4, is_enabled ? 0xff : 0x00);
}

static void TANDYSOUND_ShutDown([[maybe_unused]] Section *section)
{
	LOG_MSG("TANDY: Shutting down Tandy sound card");
	set_tandy_sound_flag_in_bios(false);
	tandy_dac.reset();
	tandy_psg.reset();
}

void TANDYSOUND_Init(Section *section)
{
	assert(section);
	const auto prop = static_cast<Section_prop *>(section);
	const auto pref = std::string_view(prop->Get_string("tandy"));
	const auto wants_tandy_sound = pref == "true" || pref == "on" ||
	                               (IS_TANDY_ARCH && pref == "auto");
	if (!wants_tandy_sound) {
		set_tandy_sound_flag_in_bios(false);
		return;
	}

	ConfigProfile cfg;
	switch (machine) {
	case MCH_PCJR: cfg = ConfigProfile::PCjrSystem; break;
	case MCH_TANDY: cfg = ConfigProfile::TandySystem; break;
	default: cfg = ConfigProfile::SoundCardOnly; break;
	}

	// the second DMA controller conflicts with the tandy sound's ports 0xc0
	CloseSecondDMAController();

	const auto can_use_tandy_dac = is_sound_blaster_absent();
	if (can_use_tandy_dac)
		tandy_dac = std::make_unique<TandyDAC>(cfg);
	tandy_psg = std::make_unique<TandyPSG>(cfg, can_use_tandy_dac);

	set_tandy_sound_flag_in_bios(true);

	section->AddDestroyFunction(&TANDYSOUND_ShutDown, true);
}
