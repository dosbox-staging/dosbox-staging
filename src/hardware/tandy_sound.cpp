/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2019-2023  The DOSBox Staging Team
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
#include "math_utils.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"

#include "mame/emu.h"
#include "mame/sn76496.h"

#include "residfp/resample/TwoPassSincResampler.h"

using namespace std::placeholders;

// Constants used by the DAC and PSG
constexpr uint16_t card_base_offset = 288;
constexpr auto tandy_psg_clock_hz   = 14318180 / 4;

enum class ConfigProfile {
	TandySystem,
	PCjrSystem,
	SoundCardOnly,
	SoundCardRemoved,
};

static void shutdown_dac(Section*);

class TandyDAC {
public:
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

	// There's only one Tandy sound's IO configuration, so make it permanent
	static constexpr IOConfig io = {0xc4, 7, 1};

	TandyDAC(const ConfigProfile config_profile,
	         const std::string_view filter_choice);
	~TandyDAC();

	bool IsEnabled() const
	{
		return is_enabled;
	}

private:
	void ChangeMode();
	void DmaCallback(const DmaChannel* chan, DMAEvent event);
	uint8_t ReadFromPort(io_port_t port, io_width_t);
	void WriteToPort(io_port_t port, io_val_t value, io_width_t);
	void AudioCallback(uint16_t requested);
	TandyDAC() = delete;

	DMA dma = {};

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
	TandyPSG(const ConfigProfile config_profile, const bool is_dac_enabled,
	         const std::string_view filter_choice);
	~TandyPSG();

private:
	TandyPSG() = delete;
	TandyPSG(const TandyPSG &) = delete;
	TandyPSG &operator=(const TandyPSG &) = delete;

	void AudioCallback(uint16_t requested_frames);
	bool MaybeRenderFrame(float &frame);
	void RenderUpToNow();
	void WriteToPort(io_port_t, io_val_t value, io_width_t);

	// Managed objects
	mixer_channel_t channel                                  = nullptr;
	IO_WriteHandleObject write_handlers[2]                   = {};
	std::unique_ptr<sn76496_base_device> device              = {};
	std::unique_ptr<reSIDfp::TwoPassSincResampler> resampler = {};
	std::queue<float> fifo                                   = {};

	// Static rate-related configuration
	static constexpr auto render_divisor = 16;
	static constexpr auto render_rate_hz = ceil_sdivide(tandy_psg_clock_hz,
	                                                    render_divisor);
	static constexpr auto ms_per_render  = millis_in_second / render_rate_hz;

	// Runtime states
	device_sound_interface *dsi       = nullptr;
	double last_rendered_ms           = 0.0;
};

static void setup_filters(mixer_channel_t &channel) {
	// The filters are meant to emulate the bandwidth limited sound of the
	// small integrated speaker of the Tandy. This more accurately
	// reflects people's actual experience of the Tandy sound than the raw
	// unfiltered output, and it's a lot more pleasant to listen to,
	// especially in headphones.
	constexpr auto hp_order       = 3;
	constexpr auto hp_cutoff_freq = 120;
	channel->ConfigureHighPassFilter(hp_order, hp_cutoff_freq);
	channel->SetHighPassFilter(FilterState::On);

	constexpr auto lp_order       = 2;
	constexpr auto lp_cutoff_freq = 4800;
	channel->ConfigureLowPassFilter(lp_order, lp_cutoff_freq);
	channel->SetLowPassFilter(FilterState::On);
}

TandyDAC::TandyDAC(const ConfigProfile config_profile,
                   const std::string_view filter_choice)
{
	assert(config_profile != ConfigProfile::SoundCardRemoved);

	// Run the audio channel at the mixer's native rate
	const auto callback = std::bind(&TandyDAC::AudioCallback, this, _1);

	channel = MIXER_AddChannel(callback,
	                           use_mixer_rate,
	                           "TANDYDAC",
	                           {ChannelFeature::Sleep,
	                            ChannelFeature::ChorusSend,
	                            ChannelFeature::ReverbSend,
	                            ChannelFeature::DigitalAudio});

	const auto mixer_rate_hz = channel->GetSampleRate();
	sample_rate = mixer_rate_hz;

	// Setup zero-order-hold resampler to emulate the "crunchiness" of early
	// DACs
	channel->SetZeroOrderHoldUpsamplerTargetFreq(check_cast<uint16_t>(sample_rate));
	channel->SetResampleMethod(ResampleMethod::ZeroOrderHoldAndResample);

	// Setup filters
	const auto filter_choice_has_bool = parse_bool_setting(filter_choice);

	if (filter_choice_has_bool && *filter_choice_has_bool == true) {
		setup_filters(channel);

	} else if (!channel->TryParseAndSetCustomFilter(filter_choice)) {
		if (!filter_choice_has_bool) {
			LOG_WARNING("TANDYDAC: Invalid 'tandy_dac_filter' value: '%s', using 'off'",
			            filter_choice.data());
		}

		channel->SetHighPassFilter(FilterState::Off);
		channel->SetLowPassFilter(FilterState::Off);
	}

	// Register DAC per-port read handlers
	const auto reader = std::bind(&TandyDAC::ReadFromPort, this, _1, _2);
	read_handler.Install(io.base, reader, io_width_t::byte, 4);

	// Register DAC per-port write handlers
	const auto writer = std::bind(&TandyDAC::WriteToPort, this, _1, _2, _3);
	write_handlers[0].Install(io.base, writer, io_width_t::byte, 4);

	if (config_profile == ConfigProfile::SoundCardOnly)
		write_handlers[1].Install(io.base + card_base_offset, writer,
		                          io_width_t::byte, 4);

	// Reserve the DMA channel
	if (dma.channel = DMA_GetChannel(io.dma); dma.channel) {
		dma.channel->ReserveFor("Tandy DAC", shutdown_dac);
	}

	is_enabled = true;
}

TandyDAC::~TandyDAC()
{
	if (!is_enabled) {
		return;
	}
	// Stop playback
	if (channel) {
		channel->Enable(false);
	}

	// Stop the game from accessing the IO ports
	read_handler.Uninstall();
	for (auto& handler : write_handlers) {
		handler.Uninstall();
	}

	// Deregister the mixer channel, after which it's cleaned up
	assert(channel);
	MIXER_DeregisterChannel(channel);

	// Reset the DMA channel as the mixer is no longer reading samples
	if (dma.channel) {
		dma.channel->Reset();
	}
}

void TandyDAC::DmaCallback([[maybe_unused]] const DmaChannel*, DMAEvent event)
{
	// LOG_MSG("TANDYDAC: DMA event %d", event);
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

	// LOG_MSG("TANDYDAC: Mode changed to %d", regs.mode);
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
			channel->SetSampleRate(freq);
			const auto vol = static_cast<float>(regs.amplitude) / 7.0f;
			channel->SetAppVolume(vol, vol);
			if ((regs.mode & 0x0c) == 0x0c) {
				dma.is_done = false;
				dma.channel = DMA_GetChannel(io.dma);
				if (dma.channel) {
					const auto callback =
					        std::bind(&TandyDAC::DmaCallback,
					                  this, _1, _2);
					dma.channel->RegisterCallback(callback);
					channel->Enable(true);
					// LOG_MSG("TANDYDAC: playback started with freqency %f, volume %f", freq, vol);
				}
			}
		}
		break;
	}
}

uint8_t TandyDAC::ReadFromPort(io_port_t port, io_width_t)
{
	// LOG_MSG("TANDYDAC: Read from port %04x", port);
	switch (port) {
	case 0xc4:
		return (regs.mode & 0x77) | (regs.irq_activated ? 0x08 : 0x00);
	case 0xc6: return static_cast<uint8_t>(regs.frequency & 0xff);
	case 0xc7:
		return static_cast<uint8_t>(((regs.frequency >> 8) & 0xf) |
		                            (regs.amplitude << 5));
	}
	LOG_MSG("TANDYDAC: Read from unknown %x", port);
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
	// LOG_MSG("TANDYDAC: Write %02x to port %04x", data, port);
	// LOG_MSG("TANDYDAC: Mode %02x, Control %02x, Frequency %04x, Amplitude %02x",
	//        regs.mode, regs.control, regs.frequency, regs.amplitude);
}

void TandyDAC::AudioCallback(uint16_t requested)
{
	if (!channel || !dma.channel) {
		LOG_DEBUG("TANDY: Skipping update until the DAC is initialized");
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

TandyPSG::TandyPSG(const ConfigProfile config_profile, const bool is_dac_enabled,
                   const std::string_view filter_choice)
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

	channel = MIXER_AddChannel(callback,
	                           use_mixer_rate,
	                           "TANDY",
	                           {ChannelFeature::Sleep,
	                            ChannelFeature::ReverbSend,
	                            ChannelFeature::ChorusSend,
	                            ChannelFeature::Synthesizer});

	// Setup filters
	const auto filter_choice_has_bool = parse_bool_setting(filter_choice);

	if (filter_choice_has_bool && *filter_choice_has_bool == true) {
		setup_filters(channel);

	} else if (!channel->TryParseAndSetCustomFilter(filter_choice)) {
		if (!filter_choice_has_bool) {
			LOG_WARNING("TANDY: Invalid 'tandy_filter' value: '%s', using 'off'",
			            filter_choice.data());
		}

		channel->SetHighPassFilter(FilterState::Off);
		channel->SetLowPassFilter(FilterState::Off);
	}

	// Setup the resampler
	const auto sample_rate = channel->GetSampleRate();
	const auto max_freq    = std::max(sample_rate * 0.9 / 2, 8000.0);
	resampler.reset(reSIDfp::TwoPassSincResampler::create(render_rate_hz,
	                                                      sample_rate,
	                                                      max_freq));

	// Configure and start the MAME device
	dsi = static_cast<device_sound_interface *>(device.get());
	const auto base_device = static_cast<device_t *>(device.get());
	base_device->device_start();
	device->convert_samplerate(render_rate_hz);

	LOG_MSG("TANDY: Initialised audio card with a TI %s PSG %s",
	        base_device->shortName,
	        is_dac_enabled ? "and 8-bit DAC" : "but without DAC");
}

TandyPSG::~TandyPSG()
{
	// Stop playback
	if (channel) {
		channel->Enable(false);
	}

	// Stop the game from accessing the IO ports
	for (auto& handler : write_handlers) {
		handler.Uninstall();
	}

	// Deregister the mixer channel, after which it's cleaned up
	assert(channel);
	MIXER_DeregisterChannel(channel);
}

bool TandyPSG::MaybeRenderFrame(float& frame)
{
	assert(dsi);
	assert(resampler);

	// Request a frame from the audio device
	static int16_t sample;
	static int16_t *buf[] = {&sample, nullptr};
	static device_sound_interface::sound_stream ss;
	dsi->sound_stream_update(ss, nullptr, buf, 1);

	const auto frame_is_ready = resampler->input(sample);

	// Get the frame
	if (frame_is_ready)
		frame = static_cast<float>(resampler->output());

	return frame_is_ready;
}

void TandyPSG::RenderUpToNow()
{
	const auto now = PIC_FullIndex();

	// Wake up the channel and update the last rendered time datum.
	assert(channel);
	if (channel->WakeUp()) {
		last_rendered_ms = now;
		return;
	}
	// Keep rendering until we're current
	while (last_rendered_ms < now) {
		last_rendered_ms += ms_per_render;
		if (float frame = 0.0f; MaybeRenderFrame(frame))
			fifo.emplace(frame);
	}
}

void TandyPSG::WriteToPort(io_port_t, io_val_t value, io_width_t)
{
	RenderUpToNow();

	const auto data = check_cast<uint8_t>(value);
	device->write(data);
}

void TandyPSG::AudioCallback(const uint16_t requested_frames)
{
	assert(channel);

	//if (fifo.size())
	//	LOG_MSG("TANDY: Queued %2lu cycle-accurate frames", fifo.size());

	auto frames_remaining = requested_frames;

	// First, send any frames we've queued since the last callback
	while (frames_remaining && fifo.size()) {
		channel->AddSamples_mfloat(1, &fifo.front());
		fifo.pop();
		--frames_remaining;
	}
	// If the queue's run dry, render the remainder and sync-up our time datum
	while (frames_remaining) {
		if (float frame = 0.0f; MaybeRenderFrame(frame)) {
			channel->AddSamples_mfloat(1, &frame);
		}
		--frames_remaining;
	}
	last_rendered_ms = PIC_FullIndex();
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
	tsaddr = TandyDAC::io.base;
	tsirq  = TandyDAC::io.irq;
	tsdma  = TandyDAC::io.dma;
	return true;
}

static void set_tandy_sound_flag_in_bios(const bool is_enabled)
{
	real_writeb(0x40, 0xd4, is_enabled ? 0xff : 0x00);
}

static void shutdown_dac(Section*)
{
	if (tandy_dac) {
		LOG_MSG("TANDY: Shutting down DAC");
		tandy_dac.reset();
	}
}

void TANDYSOUND_ShutDown(Section*)
{
	if (tandy_psg || tandy_dac) {
		LOG_MSG("TANDY: Shutting down");
		set_tandy_sound_flag_in_bios(false);
		tandy_dac.reset();
		tandy_psg.reset();
	}
}

void DMA_ConfigureSecondaryController(const ModuleLifecycle lifecycle);

void TANDYSOUND_Init(Section* section)
{
	assert(section);
	const auto prop = static_cast<Section_prop*>(section);
	const auto pref = std::string_view(prop->Get_string("tandy"));
	if (has_false(pref) || (!IS_TANDY_ARCH && pref == "auto")) {
		set_tandy_sound_flag_in_bios(false);
		return;
	}

	ConfigProfile cfg;
	switch (machine) {
	case MCH_PCJR: cfg = ConfigProfile::PCjrSystem; break;
	case MCH_TANDY: cfg = ConfigProfile::TandySystem; break;
	default: cfg = ConfigProfile::SoundCardOnly; break;
	}

	// The second DMA controller conflicts with the tandy sound's base IO
	// ports 0xc0. Closing the controller itself means that all the high DMA
	// ports (4 through 7) get automatically shutdown as well.
	//
	DMA_ConfigureSecondaryController(ModuleLifecycle::Destroy);

	const auto wants_dac = has_true(pref) || (IS_TANDY_ARCH && pref == "auto");
	if (wants_dac) {
		tandy_dac = std::make_unique<TandyDAC>(
		        cfg, prop->Get_string("tandy_dac_filter"));
	}
	tandy_psg = std::make_unique<TandyPSG>(cfg,
	                                       wants_dac,
	                                       prop->Get_string("tandy_filter"));

	set_tandy_sound_flag_in_bios(true);

	constexpr auto changeable_at_runtime = true;
	section->AddDestroyFunction(&TANDYSOUND_ShutDown, changeable_at_runtime);
}
