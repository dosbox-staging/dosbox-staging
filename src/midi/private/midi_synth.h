// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MIDI_SYNTH_H
#define DOSBOX_MIDI_SYNTH_H

#include "midi_device.h"

#include <cstdint>

#include "audio/mixer.h"
#include "midi/midi.h"
#include "utils/rwqueue.h"

// Base class encapsulating the commonalities of all MIDI synthesizers
// (SoundCanvas, MT-32, and FluidSynth). Implements the Template Method design
// patterns. Actual MIDI implementations only need to implement the
// `ProcessWorkItem()` and `RenderAudioFramesToFifo()` hooks.
class MidiSynth : public MidiDevice {
public:
	void MixerCallback(const int requested_audio_frames);

protected:
	void Render();
	void Shutdown();

	MixerChannelPtr mixer_channel        = nullptr;
	RWQueue<AudioFrame> audio_frame_fifo = {1};
	RWQueue<MidiWork> work_fifo          = {1};

	double ms_per_audio_frame = 0.0;
	bool had_underruns        = false;

	// MIDI synths run in a separate render thread
	std::thread renderer = {};

private:
	void SendMidiMessage(const MidiMessage& msg) override;
	void SendSysExMessage(uint8_t* sysex, size_t len) override;

	void RenderBacklogged();
	void ProcessWorkFromFifo();
	void ProcessWorkFromFifoBacklogged();
	int GetNumPendingAudioFrames();

	virtual void ProcessWorkItem(const MidiWork& work)         = 0;
	virtual void RenderAudioFramesToFifo(const int num_frames) = 0;

	virtual void CloseSynth() = 0;

	// Used to track the balance of time between the last mixer
	// callback versus the current MIDI SysEx or Msg event.
	double last_rendered_ms = 0.0;

	bool is_work_fifo_backlogged = false;
};

#endif // DOSBOX_MIDI_SYNTH_H
