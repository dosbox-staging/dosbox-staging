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

// PC speaker external API, used by the PIT timer and keyboard
void PCSPEAKER_SetCounter(const int counter, const PitMode pit_mode)
{
	if (pc_speaker) {
		pc_speaker->SetCounter(counter, pit_mode);
	}
}

void PCSPEAKER_SetPITControl(const PitMode pit_mode)
{
	if (pc_speaker) {
		pc_speaker->SetPITControl(pit_mode);
	}
}

void PCSPEAKER_SetType(const PpiPortB& port_b)
{
	if (pc_speaker) {
		pc_speaker->SetType(port_b);
	}
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

static void init_pcspeaker_settings(SectionProp& section)
{
	using enum Property::Changeable::Value;

	auto pstring = section.AddString("pcspeaker", WhenIdle, "impulse");
	pstring->SetHelp(
	        "PC speaker emulation model ('impulse' by default). Possible values:\n"
	        "\n"
	        "  impulse:    A very faithful emulation of the PC speaker's output (default).\n"
	        "              Works with most games, but may result in garbled sound or silence\n"
	        "              in a small number of programs.\n"
	        "\n"
	        "  discrete:   Legacy simplified PC speaker emulation; only use this on specific\n"
	        "              titles that give you problems with the 'impulse' model.\n"
	        "\n"
	        "  none, off:  Don't emulate the PC speaker.");
	pstring->SetValues({"impulse", "discrete", "none", "off"});

	pstring = section.AddString("pcspeaker_filter", WhenIdle, "on");
	pstring->SetHelp(
	        "Filter for the PC speaker output ('on' by default). Possible values:\n"
	        "\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

static void set_filter(SectionProp& section)
{
	assert(pc_speaker);

	const std::string filter_pref = section.GetString("pcspeaker_filter");

	if (!pc_speaker->TryParseAndSetCustomFilter(filter_pref)) {
		if (const auto maybe_bool = parse_bool_setting(filter_pref)) {
			pc_speaker->SetFilterState(*maybe_bool ? FilterState::On
			                                       : FilterState::Off);
		} else {
			LOG_WARNING(
			        "PCSPEAKER: Invalid 'pcspeaker_filter' setting: '%s', "
			        "using 'on'",
			        filter_pref.c_str());

			pc_speaker->SetFilterState(FilterState::On);
			set_section_property_value("speaker", "pcspeaker_filter", "on");
		}
	}
}

void PCSPEAKER_Init(SectionProp& section)
{
	const std::string pcspeaker_pref = section.GetString("pcspeaker");

	if (has_false(pcspeaker_pref)) {
		return;

	} else if (pcspeaker_pref == "discrete") {
		MIXER_LockMixerThread();
		pc_speaker = std::make_unique<PcSpeakerDiscrete>();

	} else if (pcspeaker_pref == "impulse") {
		MIXER_LockMixerThread();
		pc_speaker = std::make_unique<PcSpeakerImpulse>();
	}

	set_filter(section);

	// Size to 2x blocksize. The mixer callback will request 1x blocksize.
	// This provides a good size to avoid over-runs and stalls.
	pc_speaker->output_queue.Resize(
	        iceil(pc_speaker->channel->GetFramesPerBlock() * 2.0f));

	TIMER_AddTickHandler(PCSPEAKER_PicCallback);

	MIXER_UnlockMixerThread();
}

void PCSPEAKER_Destroy()
{
	if (pc_speaker) {
		MIXER_LockMixerThread();

		TIMER_DelTickHandler(PCSPEAKER_PicCallback);
		pc_speaker = {};

		MIXER_UnlockMixerThread();
	}
}

void PCSPEAKER_NotifySettingUpdated(SectionProp& section, const std::string& prop_name)
{
	// The [speaker] section controls multiple audio devices, so we want to
	// make sure to only restart the device affected by the setting.
	//
	if (prop_name == "pcspeaker") {
		PCSPEAKER_Destroy();
		PCSPEAKER_Init(section);

	} else if (prop_name == "pcspeaker_filter") {
		if (pc_speaker) {
			set_filter(section);
		}
	}
}

void PCSPEAKER_AddConfigSection(Section* sec)
{
	assert(sec);

	const auto section = static_cast<SectionProp*>(sec);

	init_pcspeaker_settings(*section);
}
