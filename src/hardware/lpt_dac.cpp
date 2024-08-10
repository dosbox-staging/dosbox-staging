/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2024  The DOSBox Staging Team
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

#include "math_utils.h"
#include "pic.h"
#include "setup.h"
#include "support.h"
#include "timer.h"

#include "covox.h"
#include "disney.h"
#include "ston1_dac.h"

LptDac::LptDac(const std::string_view name, const int channel_rate_hz,
               std::set<ChannelFeature> extra_features)
        : dac_name(name)
{
	using namespace std::placeholders;

	assert(!dac_name.empty());

	constexpr bool Stereo = true;
	constexpr bool SignedData = true;
	constexpr bool NativeOrder = true;
	const auto audio_callback = std::bind(MIXER_PullFromQueueCallback<LptDac, AudioFrame, Stereo, SignedData, NativeOrder>,
	                                      std::placeholders::_1,
	                                      this);

	std::set<ChannelFeature> features = {ChannelFeature::Sleep,
	                                     ChannelFeature::ReverbSend,
	                                     ChannelFeature::ChorusSend,
	                                     ChannelFeature::DigitalAudio};

	features.insert(extra_features.begin(), extra_features.end());

	// Setup the mixer callback
	channel = MIXER_AddChannel(audio_callback,
	                           channel_rate_hz,
	                           dac_name.c_str(),
	                           features);

	ms_per_frame = MillisInSecond / channel->GetSampleRate();

	// Update our status to indicate we're ready
	status_reg.error = false;
	status_reg.busy  = false;
}

bool LptDac::TryParseAndSetCustomFilter(const std::string& filter_choice)
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
		++frames_rendered_this_tick;
		output_queue.NonblockingEnqueue(Render());
	}
}

void LptDac::PicCallback(const int requested_frames)
{
	assert(channel);

	const auto frames_remaining = requested_frames - frames_rendered_this_tick;

	// If the queue's run dry, render the remainder and sync-up our time datum
	for (int i = 0; i < frames_remaining; ++i) {
		output_queue.NonblockingEnqueue(Render());
	}
	last_rendered_ms = PIC_FullIndex();
	frames_rendered_this_tick = 0;
}

LptDac::~LptDac()
{
	LOG_MSG("%s: Shutting down DAC", dac_name.c_str());

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
}

std::unique_ptr<LptDac> lpt_dac = {};

static void LPT_DAC_PicCallback()
{
	if (!lpt_dac || !lpt_dac->channel->is_enabled) {
		return;
	}
	lpt_dac->frame_counter += lpt_dac->channel->GetFramesPerTick();
	const auto requested_frames = ifloor(lpt_dac->frame_counter);
	lpt_dac->frame_counter -= static_cast<float>(requested_frames);
	lpt_dac->PicCallback(requested_frames);
}

void LPT_DAC_ShutDown([[maybe_unused]] Section *sec)
{
	MIXER_LockMixerThread();
	TIMER_DelTickHandler(LPT_DAC_PicCallback);
	lpt_dac.reset();
	MIXER_UnlockMixerThread();
}

void LPT_DAC_Init(Section* section)
{
	assert(section);

	// Always reset on changes
	LPT_DAC_ShutDown(nullptr);

	// Get the user's LPT DAC choices
	assert(section);
	const auto prop = static_cast<Section_prop*>(section);

	const std::string dac_choice = prop->Get_string("lpt_dac");

	if (dac_choice == "disney") {
		MIXER_LockMixerThread();
		lpt_dac = std::make_unique<Disney>();
	} else if (dac_choice == "covox") {
		MIXER_LockMixerThread();
		lpt_dac = std::make_unique<Covox>();
	} else if (dac_choice == "ston1") {
		MIXER_LockMixerThread();
		lpt_dac = std::make_unique<StereoOn1>();
	} else {
		// The remaining setting is to turn the LPT DAC off
		const auto dac_choice_has_bool = parse_bool_setting(dac_choice);
		if (!dac_choice_has_bool || *dac_choice_has_bool != false) {
			LOG_WARNING("LPT_DAC: Invalid 'lpt_dac' setting: '%s', using 'none'",
			            dac_choice.c_str());
		}
		return;
	}

	// Let the DAC apply its own filter type
	const std::string filter_choice = prop->Get_string("lpt_dac_filter");
	assert(lpt_dac);

	if (!lpt_dac->TryParseAndSetCustomFilter(filter_choice)) {
		if (const auto maybe_bool = parse_bool_setting(filter_choice)) {
			lpt_dac->ConfigureFilters(*maybe_bool ? FilterState::On
			                                      : FilterState::Off);
		} else {
			LOG_WARNING(
			        "LPT_DAC: Invalid 'lpt_dac_filter' setting: '%s', "
			        "using 'on'",
			        filter_choice.c_str());

			set_section_property_value("speaker", "lpt_dac_filter", "on");
			lpt_dac->ConfigureFilters(FilterState::On);
		}
	}

	lpt_dac->BindToPort(Lpt1Port);

	constexpr auto changeable_at_runtime = true;
	section->AddDestroyFunction(&LPT_DAC_ShutDown, changeable_at_runtime);

	// Size to 2x blocksize. The mixer callback will request 1x blocksize.
	// This provides a good size to avoid over-runs and stalls.
	lpt_dac->output_queue.Resize(iceil(lpt_dac->channel->GetFramesPerBlock() * 2.0f));
	TIMER_AddTickHandler(LPT_DAC_PicCallback);

	MIXER_UnlockMixerThread();
}
