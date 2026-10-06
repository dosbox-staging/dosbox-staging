// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/clap_synth.h"

#include <algorithm>
#include <cassert>
#include <utility>

void ClapSynth::Initialize(std::unique_ptr<Clap::Plugin> new_plugin,
                           const int sample_rate_hz)
{
	assert(new_plugin);

	plugin = std::move(new_plugin);
	plugin->Activate(sample_rate_hz);
}

void ClapSynth::ProcessWorkItem(const MidiWork& work)
{
	constexpr auto SampleOffset = 0;

	if (work.message_type == MessageType::Channel) {
		assert(work.message.size() >= MaxMidiMessageLen);

		event_list.AddMidiEvent(work.message, SampleOffset);
	} else {
		assert(work.message_type == MessageType::SysEx);

		event_list.AddMidiSysExEvent(work.message, SampleOffset);
	}
}

void ClapSynth::RenderAudioFramesToFifo(const int num_frames,
                                        RWQueue<AudioFrame>& audio_frame_fifo)
{
	assert(plugin);
	assert(num_frames > 0);

	float* audio_out[] = {left.data(), right.data()};

	for (auto remaining = num_frames; remaining > 0;) {
		const auto block_size = std::min(remaining,
		                                 Clap::Plugin::MaxFrameCount);

		plugin->Process(audio_out, block_size, event_list);

		// Pending events apply only to the first block of this render.
		event_list.Clear();

		for (auto frame_index = 0; frame_index < block_size; ++frame_index) {
			if (!audio_frame_fifo.Enqueue(
			            {left[frame_index], right[frame_index]})) {
				return;
			}
		}

		remaining -= block_size;
	}
}
