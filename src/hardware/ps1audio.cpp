/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2023  The DOSBox Staging Team
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

#include <algorithm>
#include <cassert>
#include <memory>
#include <queue>
#include <string.h>

#include "control.h"
#include "dma.h"
#include "inout.h"
#include "math_utils.h"
#include "mem.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"

#include "mame/emu.h"
#include "mame/sn76496.h"

#include "residfp/resample/TwoPassSincResampler.h"

using namespace std::placeholders;

struct Ps1Registers {
	uint8_t status = 0;     // Read via port 0x202 control status
	uint8_t command = 0;    // Written via port 0x202 for control, read via 0x200 for DAC
	uint8_t divisor = 0;    // Read via port 0x203 for FIFO timing
	uint8_t fifo_level = 0; // Written via port 0x204 when FIFO is almost empty
};

class Ps1Dac {
public:
	Ps1Dac(const std::string_view filter_choice);
	~Ps1Dac();

private:
	uint8_t CalcStatus() const;
	void Reset(bool should_clear_adder);
	void Update(uint16_t samples);
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
	static constexpr auto clock_rate_hz = 1000000;
	static constexpr auto fifo_size = 2048;
	static constexpr auto fifo_size_mask = fifo_size - 1;
	static constexpr auto fifo_nearly_empty_val = 128;
	static constexpr auto frac_shift = 12;            // Fixed precision
	static constexpr auto fifo_status_ready_flag = 0x10;
	static constexpr auto fifo_full_flag = 0x08;
	static constexpr auto fifo_empty_flag = 0x04;
	static constexpr auto fifo_nearly_empty_flag = 0x02; // >= 1792 bytes free
	static constexpr auto fifo_irq_flag = 0x01; // IRQ triggered by DAC
	static constexpr auto fifo_midline = ceil_udivide(static_cast<uint8_t>(UINT8_MAX), 2u);
	static constexpr auto irq_number = 7;
	static constexpr auto bytes_pending_limit =  fifo_size << frac_shift;

	// Managed objects
	mixer_channel_t channel = nullptr;
	IO_ReadHandleObject read_handlers[5] = {};
	IO_WriteHandleObject write_handlers[4] = {};
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
	int8_t signal_bias = 0;

	// States
	bool is_new_transfer = true;
	bool is_playing = false;
	bool can_trigger_irq = false;
};

static void setup_filter(mixer_channel_t &channel)
{
	constexpr auto hp_order       = 3;
	constexpr auto hp_cutoff_freq = 160;
	channel->ConfigureHighPassFilter(hp_order, hp_cutoff_freq);
	channel->SetHighPassFilter(FilterState::On);

	constexpr auto lp_order       = 1;
	constexpr auto lp_cutoff_freq = 2100;
	channel->ConfigureLowPassFilter(lp_order, lp_cutoff_freq);
	channel->SetLowPassFilter(FilterState::On);
}

Ps1Dac::Ps1Dac(const std::string_view filter_choice)
{
	const auto callback = std::bind(&Ps1Dac::Update, this, _1);

	channel = MIXER_AddChannel(callback,
	                           use_mixer_rate,
	                           "PS1DAC",
	                           {ChannelFeature::Sleep,
	                            ChannelFeature::ReverbSend,
	                            ChannelFeature::ChorusSend,
	                            ChannelFeature::DigitalAudio});

	// Setup filters
	const auto filter_choice_has_bool = parse_bool_setting(filter_choice);

	if (filter_choice_has_bool && *filter_choice_has_bool == true) {
		// Using the same filter settings for the DAC as for the PSG
		// synth. It's unclear whether this is accurate, but in any
		// case, the filters do a good approximation of how a small
		// integrated speaker would sound.
		setup_filter(channel);

	} else if (!channel->TryParseAndSetCustomFilter(filter_choice)) {
		if (!filter_choice_has_bool) {
			LOG_WARNING("PS1DAC: Invalid 'ps1audio_dac_filter' value: '%s', using 'off'",
			            filter_choice.data());
		}

		channel->SetHighPassFilter(FilterState::Off);
		channel->SetLowPassFilter(FilterState::Off);
	}

	// Register DAC per-port read handlers
	read_handlers[0].Install(0x02F, std::bind(&Ps1Dac::ReadPresencePort02F, this, _1, _2), io_width_t::byte);
	read_handlers[1].Install(0x200, std::bind(&Ps1Dac::ReadCmdResultPort200, this, _1, _2), io_width_t::byte);
	read_handlers[2].Install(0x202, std::bind(&Ps1Dac::ReadStatusPort202, this, _1, _2), io_width_t::byte);
	read_handlers[3].Install(0x203, std::bind(&Ps1Dac::ReadTimingPort203, this, _1, _2), io_width_t::byte);
	read_handlers[4].Install(0x204, // to 0x207
	                         std::bind(&Ps1Dac::ReadJoystickPorts204To207, this, _1, _2), io_width_t::byte, 3);

	// Register DAC per-port write handlers
	write_handlers[0].Install(0x200, std::bind(&Ps1Dac::WriteDataPort200, this, _1, _2, _3), io_width_t::byte);
	write_handlers[1].Install(0x202, std::bind(&Ps1Dac::WriteControlPort202, this, _1, _2, _3), io_width_t::byte);
	write_handlers[2].Install(0x203, std::bind(&Ps1Dac::WriteTimingPort203, this, _1, _2, _3), io_width_t::byte);
	write_handlers[3].Install(0x204, std::bind(&Ps1Dac::WriteFifoLevelPort204, this, _1, _2, _3),
	                          io_width_t::byte);

	// Operate at native sampling rates
	sample_rate = check_cast<uint32_t>(channel->GetSampleRate());
	last_write = 0;
	Reset(true);
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
	memset(fifo, fifo_midline, fifo_size);
	read_index = 0;
	write_index = 0;
	read_index_high = 0;

	 // Be careful with this, 5 second timeout and Space Quest 4
	if (should_clear_adder)
		adder = 0;

	bytes_pending = 0;
	regs.status = CalcStatus();
	can_trigger_irq = false;
	is_playing = true;
	is_new_transfer = true;
}

void Ps1Dac::WriteDataPort200(io_port_t, io_val_t value, io_width_t)
{
	channel->WakeUp();

	const auto data = check_cast<uint8_t>(value);
	if (is_new_transfer) {
		is_new_transfer = false;
		if (data) {
			signal_bias = static_cast<int8_t>(data - fifo_midline);
		}
	}
	regs.status = CalcStatus();
	if (!(regs.status & fifo_full_flag)) {
		const auto corrected_data = data - signal_bias;
		fifo[write_index++] = static_cast<uint8_t>(corrected_data);
		write_index &= fifo_size_mask;
		bytes_pending += (1 << frac_shift);

		if (bytes_pending > bytes_pending_limit) {
			bytes_pending = bytes_pending_limit;
		}
	}
}

void Ps1Dac::WriteControlPort202(io_port_t, io_val_t value, io_width_t)
{
	channel->WakeUp();

	const auto data = check_cast<uint8_t>(value);
	regs.command = data;
	if (data & 3)
		can_trigger_irq = true;
}

void Ps1Dac::WriteTimingPort203(io_port_t, io_val_t value, io_width_t)
{
	channel->WakeUp();

	auto data = check_cast<uint8_t>(value);
	// Clock divisor (maybe trigger first IRQ here).
	regs.divisor = data;

	if (data < 45) // common in Infocom games
		data = 125; // fallback to a default 8 KHz data rate
	const auto data_rate_hz = static_cast<uint32_t>(clock_rate_hz / data);
	adder = (data_rate_hz << frac_shift) / sample_rate;

	regs.status = CalcStatus();
	if ((regs.status & fifo_nearly_empty_flag) && (can_trigger_irq)) {
		// Generate request for stuff.
		regs.status |= fifo_irq_flag;
		can_trigger_irq = false;
		PIC_ActivateIRQ(irq_number);
	}
}

void Ps1Dac::WriteFifoLevelPort204(io_port_t, io_val_t value, io_width_t)
{
	channel->WakeUp();

	const auto data = check_cast<uint8_t>(value);
	regs.fifo_level = data;
	if (!data)
		Reset(true);
	// When the Microphone is used (PS1MIC01), it writes 0x08 to this during
	// playback presumably beacuse the card is constantly filling the
	// analog-to-digital buffer.
}

uint8_t Ps1Dac::ReadPresencePort02F(io_port_t, io_width_t)
{
	return 0xff;
}

uint8_t Ps1Dac::ReadCmdResultPort200(io_port_t, io_width_t)
{
	regs.status &= ~fifo_status_ready_flag;
	return regs.command;
}

uint8_t Ps1Dac::ReadStatusPort202(io_port_t, io_width_t)
{
	regs.status = CalcStatus();
	return regs.status;
}

// Used by Stunt Island and Roger Rabbit 2 during setup.
uint8_t Ps1Dac::ReadTimingPort203(io_port_t, io_width_t)
{
	return regs.divisor;
}

// Used by Bush Buck as an alternate detection method.
uint8_t Ps1Dac::ReadJoystickPorts204To207(io_port_t, io_width_t)
{
	return 0;
}

void Ps1Dac::Update(uint16_t samples)
{
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
		uint8_t out = 0;
		if (pending <= 0) {
			pending = 0;
			while (count--) {
				*(buffer++) = fifo_midline;
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

Ps1Dac::~Ps1Dac()
{
	// Stop playback
	if (channel) {
		channel->Enable(false);
	}

	// Stop the game from accessing the IO ports
	for (auto &handler : read_handlers)
		handler.Uninstall();
	for (auto &handler : write_handlers)
		handler.Uninstall();

	// Deregister the mixer channel, after which it's cleaned up
	assert(channel);
	MIXER_DeregisterChannel(channel);
}

class Ps1Synth {
public:
	Ps1Synth(const std::string_view filter_choice);
	~Ps1Synth();

private:
	// Block alternate construction routes
	Ps1Synth()                            = delete;
	Ps1Synth(const Ps1Synth &)            = delete;
	Ps1Synth &operator=(const Ps1Synth &) = delete;

	void AudioCallback(uint16_t requested_frames);
	bool MaybeRenderFrame(float &frame);
	void RenderUpToNow();

	void WriteSoundGeneratorPort205(io_port_t port, io_val_t, io_width_t);

	// Managed objects
	mixer_channel_t channel = nullptr;
	IO_WriteHandleObject write_handler = {};
	sn76496_device device;
	std::unique_ptr<reSIDfp::TwoPassSincResampler> resampler = {};
	std::queue<float> fifo                                   = {};

	// Static rate-related configuration
	static constexpr auto ps1_psg_clock_hz = 4000000;
	static constexpr auto render_divisor   = 16;
	static constexpr auto render_rate_hz   = ceil_sdivide(ps1_psg_clock_hz,
                                                            render_divisor);
	static constexpr auto ms_per_render    = millis_in_second / render_rate_hz;

	// Runtime states
	device_sound_interface *dsi = static_cast<sn76496_base_device *>(&device);
	double last_rendered_ms     = 0.0;
};

Ps1Synth::Ps1Synth(const std::string_view filter_choice)
        : device(machine_config(), nullptr, nullptr, ps1_psg_clock_hz)
{
	const auto callback = std::bind(&Ps1Synth::AudioCallback, this, _1);

	channel = MIXER_AddChannel(callback,
	                           use_mixer_rate,
	                           "PS1",
	                           {ChannelFeature::Sleep,
	                            ChannelFeature::ReverbSend,
	                            ChannelFeature::ChorusSend,
	                            ChannelFeature::Synthesizer});

	// Setup filters
	const auto filter_choice_has_bool = parse_bool_setting(filter_choice);

	if (filter_choice_has_bool && *filter_choice_has_bool == true) {
		// The filter parameters have been tweaked by analysing real
		// hardware recordings. The results are virtually
		// indistinguishable from the real thing by ear only.
		setup_filter(channel);

	} else if (!channel->TryParseAndSetCustomFilter(filter_choice)) {
		if (!filter_choice_has_bool) {
			LOG_WARNING("PS1: Invalid 'ps1audio_filter' setting: '%s', using 'off'",
			            filter_choice.data());
		}

		channel->SetHighPassFilter(FilterState::Off);
		channel->SetLowPassFilter(FilterState::Off);
	}

	// Setup the resampler
	const auto channel_rate_hz = channel->GetSampleRate();
	const auto max_freq = std::max(channel_rate_hz * 0.9 / 2, 8000.0);
	resampler.reset(reSIDfp::TwoPassSincResampler::create(render_rate_hz,
	                                                      channel_rate_hz,
	                                                      max_freq));

	const auto generate_sound =
	        std::bind(&Ps1Synth::WriteSoundGeneratorPort205, this, _1, _2, _3);
	write_handler.Install(0x205, generate_sound, io_width_t::byte);
	static_cast<device_t &>(device).device_start();
	device.convert_samplerate(render_rate_hz);
}

bool Ps1Synth::MaybeRenderFrame(float &frame)
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

void Ps1Synth::RenderUpToNow()
{
	const auto now = PIC_FullIndex();

	// Wake up the channel and update the last rendered time datum.
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

void Ps1Synth::WriteSoundGeneratorPort205(io_port_t, io_val_t value, io_width_t)
{
	RenderUpToNow();

	const auto data = check_cast<uint8_t>(value);
	device.write(data);
}

void Ps1Synth::AudioCallback(const uint16_t requested_frames)
{
	assert(channel);

	// if (fifo.size())
	//	LOG_MSG("PS1: Queued %2lu cycle-accurate frames", fifo.size());

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

Ps1Synth::~Ps1Synth()
{
	// Stop playback
	if (channel) {
		channel->Enable(false);
	}

	// Stop the game from accessing the IO ports
	write_handler.Uninstall();

	// Deregister the mixer channel, after which it's cleaned up
	assert(channel);
	MIXER_DeregisterChannel(channel);
}

bool PS1AUDIO_IsEnabled()
{
	assert(control);
	const auto section = control->GetSection("speaker");
	const auto properties = static_cast<Section_prop *>(section);

	assert(properties);
	return properties->Get_bool("ps1audio");
}

void PS1AUDIO_Configure(const ModuleLifecycle lifecycle, Section* section)
{
	static std::unique_ptr<Ps1Dac> ps1_dac_instance     = {};
	static std::unique_ptr<Ps1Synth> ps1_synth_instance = {};

	// Always reset on section changes
	auto reset_dac_and_synth = [&]() {
		if (ps1_dac_instance || ps1_synth_instance) {
			LOG_MSG("PS1: Shutting down IBM PS/1 Audio card");
			ps1_dac_instance.reset();
			ps1_synth_instance.reset();
		}
	};

	switch (lifecycle) {
	case ModuleLifecycle::Reconfigure:
		reset_dac_and_synth();
		[[fallthrough]];

	case ModuleLifecycle::Create:
		if (PS1AUDIO_IsEnabled()) {
			// Do we need to create it?
			if (!ps1_dac_instance || !ps1_synth_instance) {
				const auto properties = static_cast<Section_prop*>(section);
				assert(properties);

				ps1_dac_instance = std::make_unique<Ps1Dac>(
				        properties->Get_string("ps1audio_dac_filter"));

				ps1_synth_instance = std::make_unique<Ps1Synth>(
				        properties->Get_string("ps1audio_filter"));

				LOG_MSG("PS1: Initialised IBM PS/1 Audio card");
			}
		} else { // User doesn't want PS/1 audio
			reset_dac_and_synth();
		}
		break;

	case ModuleLifecycle::Destroy:
		reset_dac_and_synth();
		break;
	}
}

void PS1AUDIO_Destroy(Section* section) {
	PS1AUDIO_Configure(ModuleLifecycle::Destroy, section);
}

void PS1AUDIO_Init(Section * section) {
	PS1AUDIO_Configure(ModuleLifecycle::Create, section);

	constexpr auto changeable_at_runtime = true;
	section->AddDestroyFunction(&PS1AUDIO_Destroy, changeable_at_runtime);
}
