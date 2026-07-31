// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/midi_synth.h"

#include "hardware/pic.h"
#include "utils/math_utils.h"
#include "utils/string_utils.h"

// #define MIDI_SYNTH_DEBUG

// Keep the FIFO populated with freshly rendered buffers
void MidiSynth::Render()
{
	while (work_fifo.IsRunning()) {
		if (is_work_fifo_backlogged) {
			RenderBacklogged();
		} else {
			constexpr auto OneFrame = 1;
			work_fifo.IsEmpty() ? RenderAudioFramesToFifo(OneFrame)
			                    : ProcessWorkFromFifo();
		}
	}
}

// The next MIDI work task is processed, which includes rendering audio frames
// prior to applying channel and sysex messages to the service
void MidiSynth::ProcessWorkFromFifo()
{
	const auto work = work_fifo.Dequeue();
	if (!work) {
		return;
	}

	// Detect if the work FIFO is heavily backlogged and enter the special
	// backlogged rendering mode. This happens in fast-forward mode if the
	// MIDI synth can't keep up with the sped-up CPU emulation.
	const auto delta_from_now    = PIC_AtomicIndex() - work->timestamp;
	constexpr auto OneSecondInMs = 1000.0;

	if (delta_from_now > OneSecondInMs) {
		is_work_fifo_backlogged = true;
	}

#ifdef MIDI_SYNTH_DEBUG
	LOG_TRACE(
	        "%s: %2u audio frames prior to %s message, followed by "
	        "%2lu more messages. Have %4lu audio frames queued",
	        upcase(GetName()).c_str(),
	        work->num_pending_audio_frames,
	        work->message_type == MessageType::Channel ? "channel" : "SysEx",
	        work_fifo.Size(),
	        audio_frame_fifo.Size());
#endif

	if (work->num_pending_audio_frames > 0) {
		RenderAudioFramesToFifo(work->num_pending_audio_frames);
	}

	ProcessWorkItem(*work);
}

void MidiSynth::RenderBacklogged()
{
	// This will only keep the MIDI events we must process (e.g. program
	// change and SysEx messages).
	ProcessWorkFromFifoBacklogged();

	// We must drip-feed these essential MIDI events to the MIDI synth
	// while in fast-forward mode and render a nominal sample now and then
	// to keep the emulation ticking along. Batching them up in groups of 10
	// does the job fine.
	//
	// If we'd let them pile up and send in one big batch after exiting
	// fast-forward mode, we'd overload the MIDI synth's input buffers so
	// not all messages would be processed. This has been proven to not be a
	// viable approach as it resulted in wrong-sounding instruments in many
	// cases.
	//
	if (work_fifo.Size() > 10) {
		constexpr auto OneFrame = 1;
		RenderAudioFramesToFifo(OneFrame);
	}

	if (!MIXER_FastForwardModeEnabled()) {
		is_work_fifo_backlogged = false;

		// Send "All Notes Off" message to all MIDI channels when
		// exiting from fast-forward mode. This is the best we can do as
		// we've skipped processing any "Note On" or "Note Off" messages
		// while in fast-forward mode. There would be a lot of hanging
		// notes if we don't do this.
		//
		for (uint8_t ch = 0; ch < NumMidiChannels; ++ch) {
			const uint8_t status = MidiStatus::ControlChange | ch;

			MidiMessage msg = {};
			msg[0]          = status;
			msg[1]          = MidiChannelMode::AllNotesOff;

			SendMidiMessage(msg);
		}
	}
}

void MidiSynth::ProcessWorkFromFifoBacklogged()
{
	const auto work = work_fifo.Dequeue();
	if (!work) {
		return;
	}

	// If we're in backlogged mode when fast-forward is activated, it means
	// the MIDI synth can't keep up with the sped-up CPU emulation.
	// Therefore, we need to minimise the work to catch up.
	//
	// We can't just *not* process any MIDI events at all; we need to keep
	// processing program change, control change, etc. events, otherwise
	// there's a chance the instrument sounds will be wrong when we resume
	// normal playback. But we can drop all MIDI notes and bypass the actual
	// audio rendering; we'll just render a few samples from time to time to
	// keep the MIDI synth ticking along. This way, we can catch up and stay
	// in sync with the CPU emulation.
	//
	if (const auto status = get_midi_status(work->message[0]);
	    get_midi_message_type(status) == MessageType::Channel) {

		if (status == MidiStatus::NoteOn || status == MidiStatus::NoteOff) {
			// Drop all MIDI note messages as we won't render any audio
			return;
		}
	}

	ProcessWorkItem(*work);
}

// The request to play the channel message is placed in the MIDI work FIFO
void MidiSynth::SendMidiMessage(const MidiMessage& msg)
{
	std::vector<uint8_t> message(msg.data.begin(), msg.data.end());

	MidiWork work{std::move(message),
	              GetNumPendingAudioFrames(),
	              MessageType::Channel,
	              PIC_AtomicIndex()};

	work_fifo.Enqueue(std::move(work));
}

// The request to play the sysex message is placed in the MIDI work FIFO
void MidiSynth::SendSysExMessage(uint8_t* sysex, size_t len)
{
	std::vector<uint8_t> message(sysex, sysex + len);

	MidiWork work{std::move(message),
	              GetNumPendingAudioFrames(),
	              MessageType::SysEx,
	              PIC_AtomicIndex()};

	work_fifo.Enqueue(std::move(work));
}

int MidiSynth::GetNumPendingAudioFrames()
{
	const auto now_ms = PIC_AtomicIndex();

	// Wake up the channel and update the last rendered time datum.
	assert(mixer_channel);
	if (mixer_channel->WakeUp()) {
		last_rendered_ms = now_ms;
		return 0;
	}
	if (last_rendered_ms >= now_ms) {
		return 0;
	}

	// Return the number of audio frames needed to get current again
	assert(ms_per_audio_frame > 0.0);

	const auto elapsed_ms = now_ms - last_rendered_ms;
	const auto num_audio_frames = iround(ceil(elapsed_ms / ms_per_audio_frame));
	last_rendered_ms += (num_audio_frames * ms_per_audio_frame);

	return num_audio_frames;
}

// The callback operates at the audio frame-level, steadily adding
// samples to the mixer until the requested numbers of audio frames is
// met.
void MidiSynth::MixerCallback(const int requested_audio_frames)
{
	assert(mixer_channel);

	// Report buffer underruns
	constexpr auto WarningPercent = 5.0f;

	if (const auto percent_full = audio_frame_fifo.GetPercentFull();
	    percent_full < WarningPercent) {
		static auto iteration = 0;
		if (iteration++ % 100 == 0) {
			LOG_WARNING("FSYNTH: Audio buffer underrun");
		}
		had_underruns = true;
	}

	static std::vector<AudioFrame> audio_frames = {};

	const auto has_dequeued = audio_frame_fifo.BulkDequeue(audio_frames,
	                                                       requested_audio_frames);

	if (has_dequeued) {
		assert(check_cast<int>(audio_frames.size()) == requested_audio_frames);
		mixer_channel->AddSamples_sfloat(requested_audio_frames,
		                                 &audio_frames[0][0]);

		last_rendered_ms = PIC_AtomicIndex();
	} else {
		assert(!audio_frame_fifo.IsRunning());
		mixer_channel->AddSilence();
	}
}
