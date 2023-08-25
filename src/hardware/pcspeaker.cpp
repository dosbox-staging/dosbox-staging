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

#include "pcspeaker_discrete.h"
#include "pcspeaker_impulse.h"

// The PC Speaker pointer used in public access functions below
PcSpeaker* pc_speaker = {};

void PCSPEAKER_Configure(const ModuleLifecycle lifecycle, Section* sec)
{
	static std::unique_ptr<PcSpeaker> pc_speaker_instance = {};

	assert(sec);
	const auto prop = static_cast<const Section_prop*>(sec);

	auto get_pc_speaker_model =
	        [](const std::string_view choice) -> std::unique_ptr<PcSpeaker> {
		if (choice == "discrete") {
			return std::make_unique<PcSpeakerDiscrete>();
		}
		if (choice == "impulse") {
			return std::make_unique<PcSpeakerImpulse>();
		}
		if (!has_false(choice)) {
			LOG_ERR("PCSPEAKER: Invalid 'pcspeaker' choice: '%s', using 'none'",
			        choice.data());
		}
		return {};
	};

	// Configure the model
	// ~~~~~~~~~~~~~~~~~~~
	const auto model_choice = prop->Get_string("pcspeaker");
	assert(model_choice);

	switch (lifecycle) {
	case ModuleLifecycle::Reconfigure:
		pc_speaker = nullptr;
		pc_speaker_instance.reset();
		[[fallthrough]];

	case ModuleLifecycle::Create:
		if (!pc_speaker_instance) {
			assert(!pc_speaker);
			pc_speaker_instance = get_pc_speaker_model(model_choice);
			pc_speaker = pc_speaker_instance.get();
		}
		break;

	case ModuleLifecycle::Destroy:
		pc_speaker = nullptr;
		pc_speaker_instance.reset();
		break;
	}

	// Only configure the filter if we have a PC Speaker
	if (!pc_speaker) {
		return;
	}
	// Configure the filter
	// ~~~~~~~~~~~~~~~~~~~~
	// Get the user's filering choice
	const auto filter_choice = prop->Get_string("pcspeaker_filter");
	assert(filter_choice);

	if (!pc_speaker->TryParseAndSetCustomFilter(filter_choice)) {
		const auto filter_choice_has_bool = parse_bool_setting(filter_choice);
		if (filter_choice_has_bool) {
			if (*filter_choice_has_bool) {
				pc_speaker->SetFilterState(FilterState::On);
			}
		} else {
			LOG_WARNING("PCSPEAKER: Invalid 'pcspeaker_filter' value: '%s', using 'off'",
			            filter_choice);
			pc_speaker->SetFilterState(FilterState::Off);
		}
	}
}

void PCSPEAKER_Destroy(Section* section) {
	PCSPEAKER_Configure(ModuleLifecycle::Destroy, section);
}

void PCSPEAKER_Init(Section * section) {
	PCSPEAKER_Configure(ModuleLifecycle::Create, section);

	constexpr auto changeable_at_runtime = true;
	section->AddDestroyFunction(&PCSPEAKER_Destroy, changeable_at_runtime);
}

// PC Speaker external API, used by the PIT timer and keyboard
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
