// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// NOTE: a lot of this code assumes that the callback is called every emulated
// millisecond

#include "private/pcspeaker_discrete.h"
#include "private/pcspeaker_impulse.h"

#include "utils/math_utils.h"

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
	const auto prop = static_cast<SectionProp *>(section);

	// Get the user's PC speaker model choice
	const std::string model_choice = prop->GetString("pcspeaker");

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
	const std::string filter_choice = prop->GetString("pcspeaker_filter");

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
	section->AddDestroyFunction(PCSPEAKER_ShutDown, changeable_at_runtime);

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

void PCSPEAKER_NotifyLockMixer()
{
	if (pc_speaker) {
		pc_speaker->output_queue.Stop();
	}
}
void PCSPEAKER_NotifyUnlockMixer()
{
	if (pc_speaker) {
		pc_speaker->output_queue.Start();
	}
}
