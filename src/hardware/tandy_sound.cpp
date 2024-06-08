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

/*
	Based of sn76496.c of the M.A.M.E. project
*/

// Interaction between the Tandy DAC and the Sound Blaster:
//
// Because the Tandy DAC operates on IRQ 7 and DMA 1, it often conflicts with
// the Sound Blaster. Later models of Sound Blaster included an IRQ sharing
// feature to avoid crashes, so such Tandy + SB machines were possible to run
// without issues.

// How does this work in DOSBox? DOSBox Staging always shuts down conflicting
// DMA devices (and the Tandy DAC vs. SB is no exception), however the Tandy DAC
// is unique in that the machine's BIOS (yes, on real hardware, too) is
// programmed with a callback that points to the DAC device.  In the case of
// DOSBox, that BIOS routine either points to the Sound Blaster's DAC or Tandy
// DAC, whichever is running.
//
// So using this BIOS callback, DOSBox (and Staging) is able to support a
// Tandy+SB combo configuration as well. Note that the Tandy DAC BIOS routine
// only exists if the Tandy Card is enabled (either 'tandy=on' or 'tandy=psg').

#include "dosbox.h"

#include <algorithm>
#include <array>
#include <queue>
#include <string_view>

#include "bios.h"
#include "channel_names.h"
#include "dma.h"
#include "hardware.h"
#include "inout.h"
#include "math_utils.h"
#include "mem.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"

#include "mame/emu.h"
#include "mame/sn76496.h"

#include "residfp/resample/TwoPassSincResampler.h"

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
		uint16_t clock_divider = 0;
		uint8_t mode = 0;
		uint8_t control = 0;
		uint8_t amplitude = 0;
		bool irq_activated = false;
	};

	// There's only one Tandy sound's IO configuration, so make it permanent
	static constexpr IOConfig io = {0xc4, 7, 1};

	TandyDAC(const ConfigProfile config_profile,
	         const std::string& filter_choice);
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
	MixerChannelPtr channel = nullptr;
	IO_ReadHandleObject read_handler = {};
	IO_WriteHandleObject write_handlers[2] = {};

	// States
	Registers regs     = {};
	int sample_rate_hz = 0;
	bool is_enabled    = false;
};

class TandyPSG {
public:
	TandyPSG(const ConfigProfile config_profile, const bool is_dac_enabled,
	         const std::string& fadeout_choice,
	         const std::string& filter_choice);
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
	MixerChannelPtr channel                                  = nullptr;
	IO_WriteHandleObject write_handlers[2]                   = {};
	std::unique_ptr<sn76496_base_device> device              = {};
	std::unique_ptr<reSIDfp::TwoPassSincResampler> resampler = {};
	std::queue<float> fifo                                   = {};

	// Static rate-related configuration
	static constexpr auto render_divisor = 16;
	static constexpr auto render_rate_hz = ceil_sdivide(tandy_psg_clock_hz,
	                                                    render_divisor);
	static constexpr auto ms_per_render  = MillisInSecond / render_rate_hz;

	// Runtime states
	device_sound_interface *dsi       = nullptr;
	double last_rendered_ms           = 0.0;
};

static void setup_filter(MixerChannelPtr& channel, const bool filter_enabled)
{
	// The filters are meant to emulate the bandwidth limited sound of the
	// small integrated speaker of the Tandy. This more accurately
	// reflects people's actual experience of the Tandy sound than the raw
	// unfiltered output, and it's a lot more pleasant to listen to,
	// especially in headphones.
	if (filter_enabled) {
		constexpr auto hp_order          = 3;
		constexpr auto hp_cutoff_freq_hz = 120;
		channel->ConfigureHighPassFilter(hp_order, hp_cutoff_freq_hz);
		channel->SetHighPassFilter(FilterState::On);

		constexpr auto lp_order          = 2;
		constexpr auto lp_cutoff_freq_hz = 4800;
		channel->ConfigureLowPassFilter(lp_order, lp_cutoff_freq_hz);
		channel->SetLowPassFilter(FilterState::On);
	} else {
		channel->SetHighPassFilter(FilterState::Off);
		channel->SetLowPassFilter(FilterState::Off);
	}
}

TandyDAC::TandyDAC(const ConfigProfile config_profile,
                   const std::string& filter_choice)
{
	using namespace std::placeholders;

	assert(config_profile != ConfigProfile::SoundCardRemoved);

	// Run the audio channel at the mixer's native rate
	const auto callback = std::bind(&TandyDAC::AudioCallback, this, _1);

	channel = MIXER_AddChannel(callback,
	                           UseMixerRate,
	                           ChannelName::TandyDac,
	                           {ChannelFeature::Sleep,
	                            ChannelFeature::ChorusSend,
	                            ChannelFeature::ReverbSend,
	                            ChannelFeature::DigitalAudio});

	const auto mixer_rate_hz = channel->GetSampleRate();

	sample_rate_hz = mixer_rate_hz;

	// Set up zero-order-hold resampler to emulate the "crunchiness" of
	// early DACs
	channel->SetZeroOrderHoldUpsamplerTargetRate(
	        check_cast<uint16_t>(sample_rate_hz));

	channel->SetResampleMethod(ResampleMethod::ZeroOrderHoldAndResample);

	// Set up DAC filters
	if (const auto maybe_bool = parse_bool_setting(filter_choice)) {
		const auto filter_enabled = *maybe_bool;
		setup_filter(channel, filter_enabled);

	} else if (!channel->TryParseAndSetCustomFilter(filter_choice)) {
		LOG_WARNING(
		        "TANDYDAC: Invalid 'tandy_dac_filter' setting: '%s', "
		        "using 'on'",
		        filter_choice.c_str());

		const auto filter_enabled = true;
		setup_filter(channel, filter_enabled);
		set_section_property_value("speaker", "tandy_dac_filter", "on");
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
	using namespace std::placeholders;

	// Typical sample rates are 1.7. 5.5, 11, and rarely 22 KHz. Although
	// several games (one being OutRun) set instantaneous rates above
	// 100,000 Hz, we throw these out as they can cause garbage high
	// frequency harmonics and also cause problems for the Speex Resampler.
	// For example, a clock divider value of 8 (which is valid) produces a
	// 450 KHz sampling rate, which is way beyond what Speex can handle.
	//
	constexpr auto DacMaxSampleRateHz = 49000;

	// LOG_MSG("TANDYDAC: Mode changed to %d", regs.mode);
	switch (regs.mode & 3) {
	case 0: // joystick mode
	case 1:
	case 2: // recording
		break;
	case 3: // playback
		// Prevent divide-by-zero
		if (regs.clock_divider == 0) {
			return;
		}

		if (const auto new_sample_rate_hz = tandy_psg_clock_hz / regs.clock_divider;
		    new_sample_rate_hz < DacMaxSampleRateHz) {
			assert(channel);

			// Fill using the prior sample rate
			channel->FillUp();

			channel->SetSampleRate(
			        check_cast<uint16_t>(new_sample_rate_hz));

			const auto vol = static_cast<float>(regs.amplitude) / 7.0f;

			channel->SetAppVolume({vol, vol});

			if ((regs.mode & 0x0c) == 0x0c) {
				dma.is_done = false;
				dma.channel = DMA_GetChannel(io.dma);

				if (dma.channel) {
					const auto callback = std::bind(
					        &TandyDAC::DmaCallback, this, _1, _2);
					dma.channel->RegisterCallback(callback);

					channel->Enable(true);
#if 0
					LOG_MSG("TANDYDAC: playback started with freqency %i, volume %f",
					        sample_rate_hz,
					        vol);
#endif
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
	case 0xc6: return static_cast<uint8_t>(regs.clock_divider & 0xff);
	case 0xc7:
		return static_cast<uint8_t>(((regs.clock_divider >> 8) & 0xf) |
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
		regs.clock_divider = (regs.clock_divider & 0xf00) | data;
		switch (regs.mode & 3) {
		case 0: // joystick mode
			break;
		case 1:
		case 2:
		case 3: ChangeMode(); break;
		}
		break;
	case 0xc7:
		regs.clock_divider = static_cast<uint16_t>(
		        (regs.clock_divider & 0x00ff) | ((data & 0xf) << 8));
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
	// LOG_MSG("TANDYDAC: Mode %02x, Control %02x, clock divider %04x, Amplitude %02x",
	//         regs.mode, regs.control, regs.clock_divider, regs.amplitude);
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
                   const std::string& fadeout_choice,
                   const std::string& filter_choice)
{
	using namespace std::placeholders;

	assert(config_profile != ConfigProfile::SoundCardRemoved);

	// Instantiate the MAME PSG device
	constexpr auto rounded_psg_clock = render_rate_hz * render_divisor;
	if (config_profile == ConfigProfile::PCjrSystem)
		device = std::make_unique<sn76496_device>("SN76489", nullptr,
		                                          rounded_psg_clock);
	else
		device = std::make_unique<ncr8496_device>("NCR 8496", nullptr,
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
	                           UseMixerRate,
	                           ChannelName::TandyPsg,
	                           {ChannelFeature::Sleep,
	                            ChannelFeature::FadeOut,
	                            ChannelFeature::ReverbSend,
	                            ChannelFeature::ChorusSend,
	                            ChannelFeature::Synthesizer});

	// Setup fadeout
	if (!channel->ConfigureFadeOut(fadeout_choice)) {
		set_section_property_value("speaker", "tandy_fadeout", "off");
	}

	// Set up PSG filters
	if (const auto maybe_bool = parse_bool_setting(filter_choice)) {
		const auto filter_enabled = *maybe_bool;
		setup_filter(channel, filter_enabled);

	} else if (!channel->TryParseAndSetCustomFilter(filter_choice)) {
		LOG_WARNING(
		        "TANDY: Invalid 'tandy_filter' value: '%s', "
		        "using 'on'",
		        filter_choice.c_str());

		const auto filter_enabled = true;
		setup_filter(channel, filter_enabled);
		set_section_property_value("speaker", "tandy_filter", "on");
	}

	// Set up the resampler
	const auto sample_rate_hz = channel->GetSampleRate();

	const auto max_rate_hz = std::max(sample_rate_hz * 0.9 / 2, 8000.0);

	resampler.reset(reSIDfp::TwoPassSincResampler::create(render_rate_hz,
	                                                      sample_rate_hz,
	                                                      max_rate_hz));

	// Configure and start the MAME device
	dsi = static_cast<device_sound_interface *>(device.get());
	const auto base_device = static_cast<device_t *>(device.get());
	base_device->device_start();
	device->convert_samplerate(render_rate_hz);

	LOG_MSG("TANDY: Initialised audio card with a TI %s PSG",
	        base_device->shortName);
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

bool TANDYSOUND_GetAddress(Bitu &tsaddr, Bitu &tsirq, Bitu &tsdma)
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
		BIOS_ConfigureTandyDacCallbacks(false);
		LOG_MSG("TANDY: Shutting down");
		tandy_dac.reset();
		tandy_psg.reset();
	}
}

void TANDYSOUND_Init(Section *section)
{
	assert(section);
	const auto prop = static_cast<Section_prop*>(section);
	const auto pref = prop->Get_string("tandy");
	if (has_false(pref) || (!IS_TANDY_ARCH && pref == "auto")) {
		BIOS_ConfigureTandyDacCallbacks(false);
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
	DMA_ShutdownSecondaryController();

	const auto wants_dac = has_true(pref) || (IS_TANDY_ARCH && pref == "auto");
	if (wants_dac) {
		tandy_dac = std::make_unique<TandyDAC>(
		        cfg, prop->Get_string("tandy_dac_filter"));
	}

	// Always request the DAC even if the card doesn't have one because the
	// BIOS can be routed to the Sound Blaster's DAC if one exists.
	BIOS_ConfigureTandyDacCallbacks(true);

	tandy_psg = std::make_unique<TandyPSG>(cfg,
	                                       wants_dac,
	                                       prop->Get_string("tandy_fadeout"),
	                                       prop->Get_string("tandy_filter"));

	constexpr auto changeable_at_runtime = true;
	section->AddDestroyFunction(&TANDYSOUND_ShutDown, changeable_at_runtime);
}
