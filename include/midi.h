/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_MIDI_H
#define DOSBOX_MIDI_H

#include "dosbox.h"

#include "setup.h"

class Program;

extern uint8_t MIDI_evt_len[256];

constexpr auto MIDI_SYSEX_SIZE = 8192;

bool MIDI_Available();
void MIDI_Disengage();
void MIDI_Engage();
void MIDI_HaltSequence();
void MIDI_Init(Section *sec);
void MIDI_ListAll(Program *output_handler);
void MIDI_RawOutByte(uint8_t data);
void MIDI_ResumeSequence();

enum class MessageType : uint8_t {
	Channel,
	SysEx,
};

struct MidiWork {
	std::vector<uint8_t> message      = {};
	uint16_t num_pending_audio_frames = 0;
	MessageType message_type          = {};

	// Default value constructor
	MidiWork()                      = default;
	MidiWork(MidiWork&&)            = default;
	MidiWork& operator=(MidiWork&&) = default;

	// Construct from movable values
	MidiWork(std::vector<uint8_t>&& _message,
	         const uint16_t _num_audio_frames_pending,
	         const MessageType _message_type)
	        : message(std::move(_message)),
	          num_pending_audio_frames(_num_audio_frames_pending),
	          message_type(_message_type)
	{
		// leave the source in a valid state
		_message.clear();
	}

	// Prevent copy construction
	MidiWork(const MidiWork&)            = default;
	MidiWork& operator=(const MidiWork&) = delete;
};

#if C_FLUIDSYNTH
void FLUID_AddConfigSection(const config_ptr_t &conf);
#endif

#if C_MT32EMU
void MT32_AddConfigSection(const config_ptr_t &conf);
#endif

#endif
