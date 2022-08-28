/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2022  The DOSBox Staging Team
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

#include "innovation.h"

#include "checks.h"
#include "control.h"
#include "pic.h"
#include "support.h"

CHECK_NARROWING();

void Innovation::Open(const std::string &model_choice,
                      const std::string &clock_choice, const int filter_strength_6581,
                      const int filter_strength_8580, const int port_choice,
                      const std::string &channel_filter_choice)
{
	Close();

	// Sentinel
	if (model_choice == "none")
		return;

	std::string model_name;
	int filter_strength = 0;
	auto sid_service    = std::make_unique<reSIDfp::SID>();

	// Setup the model and filter
	if (model_choice == "8580") {
		model_name = "8580";
		sid_service->setChipModel(reSIDfp::MOS8580);
		filter_strength = filter_strength_8580;
		if (filter_strength > 0) {
			sid_service->enableFilter(true);
			sid_service->setFilter8580Curve(filter_strength / 100.0);
		}
	} else {
		model_name = "6581";
		sid_service->setChipModel(reSIDfp::MOS6581);
		filter_strength = filter_strength_6581;
		if (filter_strength > 0) {
			sid_service->enableFilter(true);
			sid_service->setFilter6581Curve(filter_strength / 100.0);
		}
	}

	// Determine chip clock frequency
	if (clock_choice == "default")
		chip_clock = 894886.25;
	else if (clock_choice == "c64ntsc")
		chip_clock = 1022727.14;
	else if (clock_choice == "c64pal")
		chip_clock = 985250.0;
	else if (clock_choice == "hardsid")
		chip_clock = 1000000.0;
	assert(chip_clock);
	ms_per_clock = millis_in_second / chip_clock;

	// Setup the mixer and get it's sampling rate
	using namespace std::placeholders;
	const auto mixer_callback = std::bind(&Innovation::AudioCallback, this, _1);

	auto mixer_channel = MIXER_AddChannel(mixer_callback,
	                                      use_mixer_rate,
	                                      "INNOVATION",
	                                      {ChannelFeature::Sleep,
	                                       ChannelFeature::ReverbSend,
	                                       ChannelFeature::ChorusSend,
	                                       ChannelFeature::Synthesizer});

	if (!mixer_channel->TryParseAndSetCustomFilter(channel_filter_choice)) {
		if (channel_filter_choice != "off")
			LOG_WARNING("INNOVATION: Invalid 'innovation_filter' value: '%s', using 'off'",
			            channel_filter_choice.c_str());

		mixer_channel->SetHighPassFilter(FilterState::Off);
		mixer_channel->SetLowPassFilter(FilterState::Off);
	}

	const auto frame_rate_hz = mixer_channel->GetSampleRate();

	// Determine the passband frequency, which is capped at 90% of Nyquist.
	const double passband = 0.9 * frame_rate_hz / 2;

	// Assign the sampling parameters
	sid_service->setSamplingParameters(chip_clock,
	                                   reSIDfp::RESAMPLE,
	                                   frame_rate_hz,
	                                   passband);

	// Setup and assign the port address
	const auto read_from = std::bind(&Innovation::ReadFromPort, this, _1, _2);
	const auto write_to = std::bind(&Innovation::WriteToPort, this, _1, _2, _3);
	base_port           = check_cast<io_port_t>(port_choice);
	read_handler.Install(base_port, read_from, io_width_t::byte, 0x20);
	write_handler.Install(base_port, write_to, io_width_t::byte, 0x20);

	// Move the locals into members
	service = std::move(sid_service);
	channel = std::move(mixer_channel);

	// Ready state-values for rendering
	last_rendered_ms = 0.0;

	constexpr auto us_per_s = 1'000'000.0;
	if (filter_strength == 0)
		LOG_MSG("INNOVATION: Running on port %xh with a SID %s at %0.3f MHz",
		        base_port,
		        model_name.c_str(),
		        chip_clock / us_per_s);
	else
		LOG_MSG("INNOVATION: Running on port %xh with a SID %s at %0.3f MHz filtering at %d%%",
		        base_port,
		        model_name.c_str(),
		        chip_clock / us_per_s,
		        filter_strength);

	is_open = true;
}

void Innovation::Close()
{
	if (!is_open)
		return;

	DEBUG_LOG_MSG("INNOVATION: Shutting down the SSI-2001 on port %xh", base_port);

	// Stop playback
	if (channel)
		channel->Enable(false);

	// Remove the IO handlers before removing the SID device
	read_handler.Uninstall();
	write_handler.Uninstall();

	// Reset the members
	channel.reset();
	service.reset();
	is_open = false;
}

uint8_t Innovation::ReadFromPort(io_port_t port, io_width_t)
{
	const auto sid_port = static_cast<io_port_t>(port - base_port);
	return service->read(sid_port);
}

void Innovation::WriteToPort(io_port_t port, io_val_t value, io_width_t)
{
	RenderUpToNow();

	const auto data = check_cast<uint8_t>(value);
	const auto sid_port = static_cast<io_port_t>(port - base_port);
	service->write(sid_port, data);
}

void Innovation::RenderUpToNow()
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
		last_rendered_ms += ms_per_clock;
		if (float frame = 0.0f; MaybeRenderFrame(frame))
			fifo.emplace(frame);
	}
}

bool Innovation::MaybeRenderFrame(float &frame)
{
	assert(service);

	int16_t sample = {0};

	const auto frame_is_ready = service->clock(1, &sample);

	// Get the frame
	if (frame_is_ready)
		frame = static_cast<float>(sample * 2);

	return frame_is_ready;
}

void Innovation::AudioCallback(const uint16_t requested_frames)
{
	assert(channel);

	//if (fifo.size())
	//	LOG_MSG("INNOVATION: Queued %2lu cycle-accurate frames", fifo.size());

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

Innovation innovation;
static void innovation_destroy([[maybe_unused]] Section *sec)
{
	innovation.Close();
}

static void innovation_init(Section *sec)
{
	assert(sec);
	Section_prop *conf = static_cast<Section_prop *>(sec);

	const auto model_choice          = conf->Get_string("sidmodel");
	const auto clock_choice          = conf->Get_string("sidclock");
	const auto port_choice           = conf->Get_hex("sidport");
	const auto filter_strength_6581  = conf->Get_int("6581filter");
	const auto filter_strength_8580  = conf->Get_int("8580filter");
	const auto channel_filter_choice = conf->Get_string("innovation_filter");

	innovation.Open(model_choice,
	                clock_choice,
	                filter_strength_6581,
	                filter_strength_8580,
	                port_choice,
	                channel_filter_choice);

	sec->AddDestroyFunction(&innovation_destroy, true);
}

static void init_innovation_dosbox_settings(Section_prop &sec_prop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	// Chip type
	auto *str_prop = sec_prop.Add_string("sidmodel", when_idle, "none");
	const char *sid_models[] = {"auto", "6581", "8580", "none", 0};
	str_prop->Set_values(sid_models);
	str_prop->Set_help(
	        "Model of chip to emulate in the Innovation SSI-2001 card:\n"
	        " - auto:  Selects the 6581 chip.\n"
	        " - 6581:  The original chip, known for its bassy and rich character.\n"
	        " - 8580:  A later revision that more closely matched the SID specification.\n"
	        "          It fixed the 6581's DC bias and is less prone to distortion.\n"
	        "          The 8580 is an option on reproduction cards, like the DuoSID.\n"
	        " - none:  Disables the card.");

	// Chip clock frequency
	str_prop = sec_prop.Add_string("sidclock", when_idle, "default");
	const char *sid_clocks[] = {"default", "c64ntsc", "c64pal", "hardsid", 0};
	str_prop->Set_values(sid_clocks);
	str_prop->Set_help(
	        "The SID chip's clock frequency, which is jumperable on reproduction cards.\n"
	        " - default: uses 0.895 MHz, per the original SSI-2001 card.\n"
	        " - c64ntsc: uses 1.023 MHz, per NTSC Commodore PCs and the DuoSID.\n"
	        " - c64pal:  uses 0.985 MHz, per PAL Commodore PCs and the DuoSID.\n"
	        " - hardsid: uses 1.000 MHz, available on the DuoSID.");

	// IO Address
	auto *hex_prop = sec_prop.Add_hex("sidport", when_idle, 0x280);
	const char *sid_ports[] = {"240", "260", "280", "2a0", "2c0", 0};
	hex_prop->Set_values(sid_ports);
	hex_prop->Set_help("The IO port address of the Innovation SSI-2001.");

	// Filter strengths
	auto *int_prop = sec_prop.Add_int("6581filter", when_idle, 50);
	int_prop->SetMinMax(0, 100);
	int_prop->Set_help(
	        "The SID's analog filtering meant that each chip was physically unique.\n"
	        "Adjusts the 6581's filtering strength as a percent from 0 to 100.");

	int_prop = sec_prop.Add_int("8580filter", when_idle, 50);
	int_prop->SetMinMax(0, 100);
	int_prop->Set_help(
	        "Adjusts the 8580's filtering strength as a percent from 0 to 100.");

	str_prop = sec_prop.Add_string("innovation_filter", when_idle, "off");
	assert(str_prop);
	str_prop->Set_help(
	        "Filter for the Innovation audio output:\n"
	        "  off:       Don't filter the output (default).\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

void INNOVATION_AddConfigSection(const config_ptr_t &conf)
{
	assert(conf);
	Section_prop *sec = conf->AddSection_prop("innovation",
	                                          &innovation_init, true);
	assert(sec);
	init_innovation_dosbox_settings(*sec);
}
