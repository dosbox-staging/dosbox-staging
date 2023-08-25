/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
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

// NOTE: a lot of this code assumes that the callback is called every emulated
// millisecond

#include "dosbox.h"

#include "pic.h"
#include "setup.h"
#include "support.h"

#include "covox.h"
#include "disney.h"
#include "ston1_dac.h"

LptDac::LptDac(const std::string_view name, const uint16_t channel_rate_hz,
               channel_features_t extra_features)
        : dac_name(name)
{
	assert(!dac_name.empty());
	using namespace std::placeholders;
	const auto audio_callback = std::bind(&LptDac::AudioCallback, this, _1);

	auto features = channel_features_t{ChannelFeature::Sleep,
	                                   ChannelFeature::ReverbSend,
	                                   ChannelFeature::ChorusSend,
	                                   ChannelFeature::DigitalAudio};

	features.insert(extra_features.begin(), extra_features.end());

	// Setup the mixer callback
	channel = MIXER_AddChannel(audio_callback,
	                           channel_rate_hz,
	                           dac_name.data(),
	                           features);

	ms_per_frame = millis_in_second / channel->GetSampleRate();

	// Update our status to indicate we're ready
	status_reg.error = false;
	status_reg.busy  = false;
}

bool LptDac::TryParseAndSetCustomFilter(const std::string_view filter_choice)
{
	assert(channel);
	return channel->TryParseAndSetCustomFilter(filter_choice);
}

void LptDac::BindHandlers(const io_port_t lpt_port, const io_write_f write_data,
                          const io_read_f read_status, const io_write_f write_control)
{
	// Register port handlers for 8-bit IO
	data_write_handler.Install(lpt_port, write_data, io_width_t::byte);

	const auto status_port = static_cast<io_port_t>(lpt_port + 1u);
	status_read_handler.Install(status_port, read_status, io_width_t::byte);

	const auto control_port = static_cast<io_port_t>(lpt_port + 2u);
	control_write_handler.Install(control_port, write_control, io_width_t::byte);
}

void LptDac::RenderUpToNow()
{
	const auto now = PIC_FullIndex();

	// Wake up the channel and update the last rendered time datum.
	assert(channel);
	if (channel->WakeUp()) {
		last_rendered_ms = now;
		return;
	}
	// Keep rendering until we're current
	assert(ms_per_frame > 0.0);
	while (last_rendered_ms < now) {
		last_rendered_ms += ms_per_frame;
		render_queue.emplace(Render());
	}
}

void LptDac::AudioCallback(const uint16_t requested_frames)
{
	assert(channel);

	auto frames_remaining = requested_frames;

	// First, add any frames we've queued since the last callback
	while (frames_remaining && render_queue.size()) {
		channel->AddSamples_sfloat(1, &render_queue.front()[0]);
		render_queue.pop();
		--frames_remaining;
	}
	// If the queue's run dry, render the remainder and sync-up our time datum
	while (frames_remaining) {
		const auto frame = Render();
		channel->AddSamples_sfloat(1, &frame[0]);
		--frames_remaining;
	}
	last_rendered_ms = PIC_FullIndex();
}

LptDac::~LptDac()
{
	LOG_MSG("%s: Shutting down DAC", dac_name.data());

	// Update our status to indicate we're no longer ready
	status_reg.error = true;
	status_reg.busy  = true;

	// Stop the game from accessing the IO ports
	status_read_handler.Uninstall();
	data_write_handler.Uninstall();
	control_write_handler.Uninstall();

	// Deregister the mixer channel, after which it's cleaned up
	assert(channel);
	MIXER_DeregisterChannel(channel);

	render_queue = {};
}

void LPT_DAC_Configure(const ModuleLifecycle lifecycle, Section* sec)
{
	static std::unique_ptr<LptDac> lpt_dac_instance = {};

	const auto prop = static_cast<const Section_prop*>(sec);

	auto get_lpt_dac_model =
	        [](const std::string_view choice) -> std::unique_ptr<LptDac> {
		if (choice == "disney") {
			return std::make_unique<Disney>();
		}
		if (choice == "covox") {
			return std::make_unique<Covox>();
		}
		if (choice == "ston1") {
			return std::make_unique<StereoOn1>();
		}
		if (!has_false(choice)) {
			LOG_WARNING("LPT_DAC: Invalid 'lpt_dac' choice: '%s', using 'none'",
			            choice.data());
		}
		return {};
	};

	// Configure the model
	// ~~~~~~~~~~~~~~~~~~~
	const auto model_choice = prop->Get_string("lpt_dac");
	switch (lifecycle) {
	case ModuleLifecycle::Reconfigure:
	case ModuleLifecycle::Create:
		assert(model_choice);
		lpt_dac_instance = get_lpt_dac_model(model_choice);
		if (lpt_dac_instance) {
			lpt_dac_instance->BindToPort(Lpt1Port);
		}
		break;

	case ModuleLifecycle::Destroy:
		lpt_dac_instance.reset();
		break;
	}

	// Only configure the filter if we have an LPT DAC
	if (!lpt_dac_instance) {
		return;
	}
	// Configure the filter
	// ~~~~~~~~~~~~~~~~~~~~
	assert(prop);
	const auto filter_choice = prop->Get_string("lpt_dac_filter");
	assert(filter_choice);

	if (!lpt_dac_instance->TryParseAndSetCustomFilter(filter_choice)) {
		auto filter_state = FilterState::Off;
		const auto filter_choice_has_bool = parse_bool_setting(filter_choice);

		if (filter_choice_has_bool) {
			filter_state = *filter_choice_has_bool ? FilterState::On
			                                       : FilterState::Off;
		} else {
			LOG_WARNING("LPT_DAC: Invalid 'lpt_dac_filter' value: '%s', using 'off'",
			            filter_choice);
			assert(filter_state == FilterState::Off);
		}
		lpt_dac_instance->ConfigureFilters(filter_state);
	}
}

void LPT_DAC_Destroy(Section* section) {
	LPT_DAC_Configure(ModuleLifecycle::Destroy, section);
}

void LPT_DAC_Init(Section * section) {
	LPT_DAC_Configure(ModuleLifecycle::Create, section);

	constexpr auto changeable_at_runtime = true;
	section->AddDestroyFunction(&LPT_DAC_Destroy, changeable_at_runtime);
}
