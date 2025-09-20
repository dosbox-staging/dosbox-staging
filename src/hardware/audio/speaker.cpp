// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/audio/speaker.h"

#include "private/pcspeaker.h"
#include "private/ps1audio.h"

#include "lpt_dac.h"
#include "ps1audio.h"
#include "pcspeaker.h"
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

void SPEAKER_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("speaker", nullptr);

	LPT_DAC_AddConfigSection(section);
	PCSPEAKER_AddConfigSection(section);
	PS1AUDIO_AddConfigSection(section);
	TANDYSOUND_AddConfigSection(section);

	init_speaker_settings(*section);
}
