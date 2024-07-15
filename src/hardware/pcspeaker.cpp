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

#include "pcspeaker_discrete.h"
#include "pcspeaker_impulse.h"
#include "math_utils.h"
#include "timer.h"

// The PC speaker managed pointer
std::unique_ptr<PcSpeaker> pc_speaker = {};

static void PCSPEAKER_PicCallback()
{
	if (!pc_speaker || !pc_speaker->channel->is_enabled) {
		return;
	}
	pc_speaker->frame_counter += pc_speaker->channel->GetFramesPerTick();
	const int requested_frames = ifloor(pc_speaker->frame_counter);
	pc_speaker->frame_counter -= static_cast<float>(requested_frames);
	pc_speaker->PicCallback(requested_frames);
}

void PCSPEAKER_ShutDown([[maybe_unused]] Section *sec)
{
	MIXER_LockMixerThread();
	TIMER_DelTickHandler(PCSPEAKER_PicCallback);
	pc_speaker.reset();
	MIXER_UnlockMixerThread();
}

void PCSPEAKER_Init(Section *section)
{
	// Always reset the speaker on changes
	PCSPEAKER_ShutDown(nullptr);

	assert(section);
	const auto prop = static_cast<Section_prop *>(section);

	// Get the user's PC speaker model choice
	const std::string model_choice = prop->Get_string("pcspeaker");

	const auto model_choice_has_bool = parse_bool_setting(model_choice);

	if (model_choice_has_bool && *model_choice_has_bool == false) {
		return;

	} else if (model_choice == "discrete") {
		MIXER_LockMixerThread();
		pc_speaker = std::make_unique<PcSpeakerDiscrete>();

	} else if (model_choice == "impulse") {
		MIXER_LockMixerThread();
		pc_speaker = std::make_unique<PcSpeakerImpulse>();

	} else {
		LOG_ERR("PCSPEAKER: Invalid PC speaker model: %s",
		        model_choice.c_str());
		return;
	}
	assert(pc_speaker);

	// Get the user's filering choice
	const std::string filter_choice = prop->Get_string("pcspeaker_filter");

	if (!pc_speaker->TryParseAndSetCustomFilter(filter_choice)) {
		if (const auto maybe_bool = parse_bool_setting(filter_choice)) {
			pc_speaker->SetFilterState(*maybe_bool ? FilterState::On
			                                       : FilterState::Off);
		} else {
			LOG_WARNING(
			        "PCSPEAKER: Invalid 'pcspeaker_filter' setting: '%s', "
			        "using 'on'",
			        filter_choice.c_str());

			pc_speaker->SetFilterState(FilterState::On);
			set_section_property_value("speaker", "pcspeaker_filter", "on");
		}
	}

	constexpr auto changeable_at_runtime = true;
	section->AddDestroyFunction(&PCSPEAKER_ShutDown, changeable_at_runtime);

	// Size to 2x blocksize. The mixer callback will request 1x blocksize.
	// This provides a good size to avoid over-runs and stalls.
	pc_speaker->output_queue.Resize(iceil(pc_speaker->channel->GetFramesPerBlock() * 2.0f));
	TIMER_AddTickHandler(PCSPEAKER_PicCallback);

	MIXER_UnlockMixerThread();
}

// PC speaker external API, used by the PIT timer and keyboard
void PCSPEAKER_SetCounter(const int counter, const PitMode pit_mode)
{
	if (pc_speaker)
		pc_speaker->SetCounter(counter, pit_mode);
}

void PCSPEAKER_SetPITControl(const PitMode pit_mode)
{
	if (pc_speaker)
		pc_speaker->SetPITControl(pit_mode);
}

void PCSPEAKER_SetType(const PpiPortB &port_b)
{
	if (pc_speaker)
		pc_speaker->SetType(port_b);
}
