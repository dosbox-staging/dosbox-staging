// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MIDI_SYNTH_RENDER_PAUSER_H
#define DOSBOX_MIDI_SYNTH_RENDER_PAUSER_H

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "audio/audio_frame.h"
#include "midi/midi.h"
#include "utils/rwqueue.h"

// Parks an internal MIDI synth's renderer thread (FluidSynth, MT-32,
// SoundCanvas) at a DOSBox pause edge so the synth's internal clock stops
// advancing while paused.
//
// While parked it stops the outbound `audio_frame_fifo` so a mixer
// `BulkDequeue()` that races the halt gets a short read instead of blocking on
// the empty-but-running queue -- that block (while the mixer held
// `mixer.mutex`) was the pause/resume deadlock the old `MIDI_Pause`/
// `MIDI_Resume` lock + ordering used to guard against. Buffered pre-pause
// frames survive the stop and drain on resume.
//
// Shared by the three internal synths via composition; each holds one and
// delegates its `MidiDevice::Pause()`/`Resume()` to it and calls
// `ParkIfPaused()` at the top of its render loop.
class SynthRenderPauser {
public:
	// Request the renderer to park at its next loop iteration.
	void Pause()
	{
		is_paused.store(true, std::memory_order_release);
	}

	// Wake a parked renderer. Also called during device teardown so a
	// paused synth doesn't deadlock on shutdown.
	void Resume()
	{
		{
			const std::lock_guard lock(mutex);
			is_paused.store(false, std::memory_order_release);
		}
		cv.notify_all();
	}

	// Called at the top of the render loop. If a pause is in effect, block
	// until resumed or until `work_fifo` stops (shutdown), stopping
	// `audio_frame_fifo` for the duration. Returns true if it parked, in
	// which case the caller should re-check its loop condition (`continue`).
	bool ParkIfPaused(RWQueue<AudioFrame>& audio_frame_fifo,
	                  RWQueue<MidiWork>& work_fifo)
	{
		if (!is_paused.load(std::memory_order_acquire)) {
			return false;
		}

		std::unique_lock lock(mutex);

		audio_frame_fifo.Stop();

		cv.wait(lock, [this, &work_fifo] {
			return !is_paused.load(std::memory_order_acquire) ||
			       !work_fifo.IsRunning();
		});

		audio_frame_fifo.Start();

		return true;
	}

private:
	std::atomic<bool> is_paused = false;
	std::mutex mutex            = {};
	std::condition_variable cv  = {};
};

#endif // DOSBOX_MIDI_SYNTH_RENDER_PAUSER_H
