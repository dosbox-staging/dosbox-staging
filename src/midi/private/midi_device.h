// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MIDI_DEVICE_H
#define DOSBOX_MIDI_DEVICE_H

#include "midi/midi.h"

#include <cstdint>

namespace MidiDeviceName {
// Internal synths
constexpr auto FluidSynth  = "fluidsynth";
constexpr auto SoundCanvas = "soundcanvas";
constexpr auto Mt32        = "mt32";

// External devices
constexpr auto Alsa      = "alsa";
constexpr auto CoreAudio = "coreaudio";
constexpr auto CoreMidi  = "coremidi";
constexpr auto Win32     = "win32";
} // namespace MidiDeviceName

class MidiDevice {
public:
	enum class Type { Internal, External };

	virtual ~MidiDevice() = default;

	virtual std::string GetName() const = 0;
	virtual Type GetType() const        = 0;

	virtual void SendMidiMessage([[maybe_unused]] const MidiMessage& msg) = 0;

	virtual void SendSysExMessage([[maybe_unused]] uint8_t* sysex,
	                              [[maybe_unused]] size_t len) = 0;
};

void MIDI_Reset(MidiDevice* device);
MidiDevice* MIDI_GetCurrentDevice();

#endif // DOSBOX_MIDI_DEVICE_H
