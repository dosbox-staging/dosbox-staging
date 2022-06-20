/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
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
	assert(section);
	const auto prop = static_cast<Section_prop *>(section);

	if (!prop->Get_bool("pcspeaker"))
		return;

	// Create the PC Speaker
	if (!pc_speaker)
		pc_speaker = std::make_unique<PcSpeakerDiscrete>();
	//		pc_speaker = std::make_unique<PcSpeakerImpulse>();

	// Get the user's filering preference
	const auto filter_pref  = std::string_view(prop->Get_string("pcspeaker_filter"));
	const auto filter_state = filter_pref == "on" ? FilterState::On : FilterState::Off;

	// Apply the user's filtering preference
	assert(pc_speaker);
	pc_speaker->SetFilterState(filter_state);

	section->AddDestroyFunction(&PCSPEAKER_ShutDown, true);
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

void PCSPEAKER_SetType(const PpiPortB &port_b_state)
{
	if (pc_speaker)
		pc_speaker->SetType(port_b_state);
}
