/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
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

#include <array>
#include <cassert>

#include "control.h"
#include "setup.h"

class Program;

// Lookup to figure out the total length of a MIDI message (including the
// first status byte) based on the status byte.
// Using data bytes will result in a dummy zero lookup.
extern uint8_t MIDI_message_len_by_status[256];

constexpr auto MaxMidiSysExSize = 8192;

constexpr uint8_t MaxMidiMessageLen = 3;

constexpr uint8_t NumMidiChannels  = 16;
constexpr uint8_t FirstMidiChannel = 0;
constexpr uint8_t LastMidiChannel  = NumMidiChannels - 1;

constexpr uint8_t NumMidiNotes  = 128;
constexpr uint8_t FirstMidiNote = 0;
constexpr uint8_t LastMidiNote  = NumMidiNotes - 1;


enum class MessageType : uint8_t { Channel, SysEx };

struct MidiMessage {
	std::array<uint8_t, MaxMidiMessageLen> data = {};

	constexpr MidiMessage() = default;

	constexpr MidiMessage(const uint8_t status, const uint8_t data1)
	        : data{status, data1, 0}
	{}

	constexpr MidiMessage(const uint8_t status, const uint8_t data1,
	                      const uint8_t data2)
	        : data{status, data1, data2}
	{}

	constexpr uint8_t& operator[](const size_t i) noexcept
	{
		assert(i < MaxMidiMessageLen);
		return data[i];
	}
	constexpr const uint8_t& operator[](const size_t i) const noexcept
	{
		assert(i < MaxMidiMessageLen);
		return data[i];
	}

	constexpr uint8_t& status() noexcept
	{
		return data[0];
	}
	constexpr const uint8_t& status() const noexcept
	{
		return data[0];
	}

	constexpr uint8_t& data1() noexcept
	{
		return data[1];
	}
	constexpr const uint8_t& data1() const noexcept
	{
		return data[1];
	}

	constexpr uint8_t& data2() noexcept
	{
		return data[2];
	}
	constexpr const uint8_t& data2() const noexcept
	{
		return data[2];
	}
};

// From "The Complete MIDI 1.0 Detailed Specification",
// document version 96.1, third edition (1996, MIDI Manufacturers Association)
//
// https://archive.org/details/Complete_MIDI_1.0_Detailed_Specification_96-1-3/

namespace MidiStatus {
// Channel Voice Messages -- lower 4-bits (nibble) specify one of the 16 MIDI
// Channels (channel 1 = 0x0, channel 16 = 0xf)
constexpr uint8_t NoteOff         = 0x80;
constexpr uint8_t NoteOn          = 0x90;
constexpr uint8_t PolyKeyPressure = 0xa0;
constexpr uint8_t ControlChange   = 0xb0;
constexpr uint8_t ProgramChange   = 0xc0;
constexpr uint8_t ChannelPressure = 0xd0;
constexpr uint8_t PitchBend       = 0xe0;

// System Messages
constexpr uint8_t SystemMessage = 0xf0;

// System Common Messages
constexpr uint8_t MidiTimeCodeQuarterFrame = 0xf1;
constexpr uint8_t SongPositionPointer      = 0xf2;
constexpr uint8_t SongSelect               = 0xf3;
constexpr uint8_t TuneRequest              = 0xf6;
constexpr uint8_t EndOfExclusive           = 0xf7;

// System Real-Time Messages
constexpr uint8_t TimingClock   = 0xf8;
constexpr uint8_t Start         = 0xfa;
constexpr uint8_t Continue      = 0xfb;
constexpr uint8_t Stop          = 0xfc;
constexpr uint8_t ActiveSensing = 0xfe;
constexpr uint8_t SystemReset   = 0xff;

// System Exclusive Messages
constexpr uint8_t SystemExclusive = 0xf0;
} // namespace MidiStatus

// Channel Mode Messages are Control Change Messages that use the reserved
// 120-127 controller number range to set the Channel Mode.
namespace MidiChannelMode {

constexpr uint8_t AllSoundOff         = 120;
constexpr uint8_t ResetAllControllers = 121;
constexpr uint8_t LocalControl        = 122;
constexpr uint8_t AllNotesOff         = 123;
constexpr uint8_t OmniOff             = 124;
constexpr uint8_t OmniOn              = 125;
constexpr uint8_t MonoOn              = 126;
constexpr uint8_t PolyOn              = 127;
} // namespace MidiChannelMode

// Only controllers implemented by the Roland Sound Canvas SC-8850 released in
// 1999 are included, which is a reasonable superset of all General Midi/GS/XG
// implementations from the 1990s. Most of these are not used in the code but
// are included here anyway for reference and troubleshooting purposes.
// Names were taken from the Owner's Manual of the SC-8850.
namespace MidiController {
constexpr uint8_t Modulation        = 1;
constexpr uint8_t PortamentoTime    = 5;
constexpr uint8_t DataEntryMsb      = 6;
constexpr uint8_t Volume            = 7;
constexpr uint8_t Pan               = 8;
constexpr uint8_t Expression        = 11;
constexpr uint8_t DataEntryLsb      = 38;
constexpr uint8_t Hold1             = 64;
constexpr uint8_t Portamento        = 65;
constexpr uint8_t Sostenuto         = 66;
constexpr uint8_t Soft              = 67;
constexpr uint8_t FilterResonance   = 71;
constexpr uint8_t ReleaseTime       = 72;
constexpr uint8_t AttackTime        = 73;
constexpr uint8_t Cutoff            = 74;
constexpr uint8_t DecayTime         = 75;
constexpr uint8_t VibrateRate       = 76;
constexpr uint8_t VibrateDepth      = 77;
constexpr uint8_t VibrateDelay      = 78;
constexpr uint8_t PortamentoControl = 84;
constexpr uint8_t ReverbSendLevel   = 91;
constexpr uint8_t ChorusSendLevel   = 93;
constexpr uint8_t DelaySendLevel    = 94;
constexpr uint8_t NrpnMsb           = 98;
constexpr uint8_t NrpnLsb           = 99;
constexpr uint8_t RpnMsb            = 100;
constexpr uint8_t RpnLsb            = 101;
} // namespace MidiController

bool is_midi_data_byte(const uint8_t byte);
bool is_midi_status_byte(const uint8_t byte);

MessageType get_midi_message_type(const uint8_t status_byte);

uint8_t get_midi_status(const uint8_t status_byte);
uint8_t get_midi_channel(const uint8_t channel_status);

void MIDI_Init();

bool MIDI_IsAvailable();
void MIDI_Reset();
void MIDI_ListDevices(Program* output_handler);
void MIDI_RawOutByte(const uint8_t data);

void MIDI_Mute();
void MIDI_Unmute();

struct MidiWork {
	std::vector<uint8_t> message      = {};
	int num_pending_audio_frames      = 0;
	MessageType message_type          = {};

	// Default value constructor
	MidiWork()                      = default;
	MidiWork(MidiWork&&)            = default;
	MidiWork& operator=(MidiWork&&) = default;

	// Construct from movable values
	MidiWork(std::vector<uint8_t>&& _message,
	         const int _num_audio_frames_pending,
	         const MessageType _message_type)
	        : message(std::move(_message)),
	          num_pending_audio_frames(_num_audio_frames_pending),
	          message_type(_message_type)
	{
		// leave the source in a valid state
		_message.clear();
	}

	// Prevent copy construction
	MidiWork(const MidiWork&)            = delete;
	MidiWork& operator=(const MidiWork&) = delete;
};

#if C_FLUIDSYNTH
void FSYNTH_AddConfigSection(const ConfigPtr& conf);
#endif

#if C_MT32EMU
void MT32_AddConfigSection(const ConfigPtr& conf);
#endif

void MIDI_AddConfigSection(const ConfigPtr& conf);

#endif
