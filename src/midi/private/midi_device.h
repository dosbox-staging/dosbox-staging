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

// Idle-render batch size for internal MIDI synth render threads.
//
// When the work FIFO is empty, the render loop calls
// `RenderAudioFramesToFifo(IdleRenderBatchFrames)` to refill
// `audio_frame_fifo`. Choosing a small batch (originally 1) means
// renderFloat() and the FIFO mutex are touched once per audio
// frame -- at typical sample rates (~44 kHz) that is tens of
// thousands of unnecessary per-call cycles per second.
//
// 64 frames is a balance: large enough to amortise that overhead
// roughly 64x, small enough that the worst-case delay between a
// MIDI event arriving and taking effect in the rendered audio is
// well below human perception (~1.5 ms at 44.1 kHz). MIDI message
// timing is unchanged because the per-message catch-up render in
// ProcessWorkFromFifo continues to use the exact pending frame
// count.
//
constexpr int IdleRenderBatchFrames = 64;

class MidiDevice {
public:
	enum class Type { Internal, External };

	virtual ~MidiDevice() = default;

	virtual std::string GetName() const = 0;
	virtual Type GetType() const        = 0;

	virtual void SendMidiMessage(const MidiMessage& msg)      = 0;
	virtual void SendSysExMessage(uint8_t* sysex, size_t len) = 0;

	// Pause/resume hooks for internal synth render threads.
	// Default no-op; only Internal devices override.
	virtual void Pause() {}
	virtual void Resume() {}
};

void MIDI_Reset(MidiDevice* device);
MidiDevice* MIDI_GetCurrentDevice();

#endif // DOSBOX_MIDI_DEVICE_H
