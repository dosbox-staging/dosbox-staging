// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CLAP_SYNTH_H
#define DOSBOX_CLAP_SYNTH_H

#include <array>
#include <memory>

#include "audio/audio_frame.h"
#include "audio/clap/event_list.h"
#include "audio/clap/plugin.h"
#include "midi/midi.h"
#include "utils/rwqueue.h"

// Shared CLAP backend for MIDI devices that inherit directly from MidiSynth.
class ClapSynth {
public:
	void Initialize(std::unique_ptr<Clap::Plugin> plugin,
	                const int sample_rate_hz);

	void ProcessWorkItem(const MidiWork& work);
	void StopProcessing();
	void RenderAudioFramesToFifo(const int num_frames,
	                             RWQueue<AudioFrame>& audio_frame_fifo);

private:
	std::unique_ptr<Clap::Plugin> plugin = nullptr;
	Clap::EventList event_list           = {};

	// Each synth owns its render buffers; processing never resizes them.
	std::array<float, Clap::Plugin::MaxFrameCount> left  = {};
	std::array<float, Clap::Plugin::MaxFrameCount> right = {};
};

#endif // DOSBOX_CLAP_SYNTH_H
