// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/audio/speaker.h"

#include "private/pcspeaker.h"
#include "private/ps1audio.h"

#include "lpt_dac.h"
#include "pcspeaker.h"
#include "ps1audio.h"
#include "tandy_sound.h"

#include "config/config.h"
#include "config/setup.h"

static void init_speaker_settings(SectionProp& section)
{
	using enum Property::Changeable::Value;

	auto pstring = section.AddString("zero_offset", Deprecated, "");
	pstring->SetHelp(
	        "DC-offset is now eliminated globally from the master mixer output.");
}

// TODO The LPT DAC, PS/1 Audio, and Tandy sound emulations will be moved out
// of the [speaker] section into their own respective sections. Until then,
// the lifecycle of these devices are managed here at the top level.

void SPEAKER_Init()
{
	const auto section = get_section("speaker");

	LPTDAC_Init(section);
	PCSPEAKER_Init(section);
	PS1AUDIO_Init(section);
	TANDYSOUND_Init(section);
}

void SPEAKER_Destroy()
{
	const auto section = get_section("speaker");

	TANDYSOUND_Destroy(section);
	PS1AUDIO_Destroy(section);
	PCSPEAKER_Destroy(section);
	LPTDAC_Destroy(section);
}

void notify_speaker_setting_updated(SectionProp* section,
                                    const std::string& prop_name)
{
	LPTDAC_NotifySettingUpdated(section, prop_name);
	PCSPEAKER_NotifySettingUpdated(section, prop_name);
	PS1AUDIO_NotifySettingUpdated(section, prop_name);
	TANDYSOUND_NotifySettingUpdated(section, prop_name);
}

void SPEAKER_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("speaker");
	section->AddUpdateHandler(notify_speaker_setting_updated);

	LPTDAC_AddConfigSection(section);
	PCSPEAKER_AddConfigSection(section);
	PS1AUDIO_AddConfigSection(section);
	TANDYSOUND_AddConfigSection(section);

	init_speaker_settings(*section);
}
