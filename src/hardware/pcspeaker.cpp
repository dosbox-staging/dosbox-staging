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

// The PC Speaker managed pointer
std::unique_ptr<PcSpeaker> pc_speaker = {};

void PCSPEAKER_ShutDown([[maybe_unused]] Section *sec)
{
	pc_speaker.reset();
}

void PCSPEAKER_Init(Section *section)
{
	// Always reset the speaker on changes
	PCSPEAKER_ShutDown(nullptr);

	assert(section);
	const auto prop = static_cast<Section_prop *>(section);

	// Get the user's PC Speaker model choice
	const std::string model_choice = prop->Get_string("pcspeaker");

	const auto model_choice_has_bool = parse_bool_setting(model_choice);

	if (model_choice_has_bool && *model_choice_has_bool == false) {
		return;
	} else if (model_choice == "discrete") {
		pc_speaker = std::make_unique<PcSpeakerDiscrete>();
	} else if (model_choice == "impulse") {
		pc_speaker = std::make_unique<PcSpeakerImpulse>();
	} else {
		LOG_ERR("PCSPEAKER: Invalid PC Speaker model: %s",
		        model_choice.data());
		return;
	}

	// Get the user's filering choice
	const std::string filter_choice = prop->Get_string("pcspeaker_filter");

	assert(pc_speaker);

	if (!pc_speaker->TryParseAndSetCustomFilter(filter_choice)) {
		const auto filter_choice_has_bool = parse_bool_setting(filter_choice);
		if (filter_choice_has_bool) {
			if (*filter_choice_has_bool) {
				pc_speaker->SetFilterState(FilterState::On);
			}
		} else {
			LOG_WARNING("PCSPEAKER: Invalid 'pcspeaker_filter' setting: '%s', using 'off'",
			            filter_choice.data());
			pc_speaker->SetFilterState(FilterState::Off);
		}
	}

	constexpr auto changeable_at_runtime = true;
	section->AddDestroyFunction(&PCSPEAKER_ShutDown, changeable_at_runtime);
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
