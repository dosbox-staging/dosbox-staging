// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/clap_base.h"

#include <cmath>

#include "hardware/pic.h"
#include "misc/support.h"
#include "utils/checks.h"

CHECK_NARROWING();

void MidiDeviceClapBase::Initialize(std::unique_ptr<Clap::Plugin> loaded_plugin,
                                    const Config& config)
{
	assert(loaded_plugin);
	assert(!mixer_channel);
	assertm(config.sample_rate_hz >= 8000.0f,
	        "Sample rate must be at least 8 kHz");

	plugin                      = std::move(loaded_plugin);
	log_prefix                  = config.log_prefix;
	enable_backlog_mode         = config.enable_backlog_mode;
	backlogged_event_batch_size = config.backlogged_event_batch_size;
	ms_per_audio_frame          = MillisInSecond / config.sample_rate_hz;

	MIXER_LockMixerThread();
	try {
		const auto mixer_callback = std::bind(&MidiDeviceClapBase::MixerCallback,
		                                      this,
		                                      std::placeholders::_1);

		mixer_channel = MIXER_AddChannel(mixer_callback,
		                                 iroundf(config.sample_rate_hz),
		                                 config.mixer_channel_name,
		                                 config.channel_features);

		mixer_channel->SetResampleMethod(ResampleMethod::Resample);

		// CLAP plugins output float samples between -1.0f and +1.0f.
		mixer_channel->Set0dbScalar(Max16BitSampleValue);

		if (config.configure_channel) {
			config.configure_channel(mixer_channel);
		}

		// MIDI input is bursty, so render twice the baseline PCM
		// prebuffer.
		const auto render_ahead_ms     = MIXER_GetPreBufferMs() * 2;
		const auto audio_frames_per_ms = iround(config.sample_rate_hz /
		                                        MillisInSecond);
		audio_frame_fifo.Resize(
		        check_cast<size_t>(render_ahead_ms * audio_frames_per_ms));
		work_fifo.Resize(MaxMidiWorkFifoSize);

		plugin->Activate(iroundf(config.sample_rate_hz));

		renderer = std::thread(&MidiDeviceClapBase::Render, this);
		set_thread_name(renderer, config.renderer_thread_name.c_str());
	} catch (...) {
		work_fifo.Stop();
		audio_frame_fifo.Stop();
		Resume();
		if (renderer.joinable()) {
			renderer.join();
		}
		if (mixer_channel) {
			MIXER_DeregisterChannel(mixer_channel);
			mixer_channel.reset();
		}
		MIXER_UnlockMixerThread();
		throw;
	}
	MIXER_UnlockMixerThread();
}

MidiDeviceClapBase::~MidiDeviceClapBase()
{
	if (!mixer_channel) {
		return;
	}

	if (had_underruns) {
		LOG_WARNING(
		        "%s: Fix underruns by lowering the CPU load or increasing "
		        "the 'prebuffer' or 'blocksize' setting",
		        log_prefix.c_str());
	}

	MIXER_LockMixerThread();
	mixer_channel->Enable(false);
	work_fifo.Stop();
	audio_frame_fifo.Stop();

	// A paused renderer waits on the pauser, not on the stopped work FIFO.
	Resume();
	if (renderer.joinable()) {
		renderer.join();
	}
	MIXER_DeregisterChannel(mixer_channel);
	mixer_channel.reset();
	MIXER_UnlockMixerThread();
}

int MidiDeviceClapBase::GetNumPendingAudioFrames()
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

// The request to play the channel message is placed in the MIDI work FIFO
void MidiDeviceClapBase::SendMidiMessage(const MidiMessage& msg)
{
	std::vector<uint8_t> message(msg.data.begin(), msg.data.end());

	MidiWork work{std::move(message),
	              GetNumPendingAudioFrames(),
	              MessageType::Channel,
	              PIC_AtomicIndex()};

	work_fifo.Enqueue(std::move(work));
}

// The request to play the sysex message is placed in the MIDI work FIFO
void MidiDeviceClapBase::SendSysExMessage(uint8_t* sysex, size_t len)
{
	std::vector<uint8_t> message(sysex, sysex + len);

	MidiWork work{std::move(message),
	              GetNumPendingAudioFrames(),
	              MessageType::SysEx,
	              PIC_AtomicIndex()};

	work_fifo.Enqueue(std::move(work));
}

// The callback operates at the audio frame-level, steadily adding samples to
// the mixer until the requested numbers of audio frames is met.
void MidiDeviceClapBase::MixerCallback(const int requested_audio_frames)
{
	assert(mixer_channel);

	// Report buffer underruns
	constexpr auto warning_percent = 5.0f;

	if (const auto percent_full = audio_frame_fifo.GetPercentFull();
	    percent_full < warning_percent) {
		if (underrun_warning_iteration++ % 100 == 0) {
			LOG_WARNING("%s: Audio buffer underrun", log_prefix.c_str());
		}
		had_underruns = true;
	}

	// A short read means the fifo was stopped for a pause (or a genuine
	// underrun): add whatever we got and pad the shortfall with silence.
	// Never hand `AddSamples_sfloat` fewer frames than it will read.
	const auto num_dequeued = audio_frame_fifo.BulkDequeue(mixer_audio_frames,
	                                                       requested_audio_frames);

	if (num_dequeued > 0) {
		mixer_channel->AddSamples_sfloat(check_cast<int>(num_dequeued),
		                                 &mixer_audio_frames[0][0]);

		last_rendered_ms = PIC_AtomicIndex();
	}
	if (check_cast<int>(num_dequeued) < requested_audio_frames) {
		mixer_channel->AddSilence();
	}
}

void MidiDeviceClapBase::RenderAudioFramesToFifo(const int num_audio_frames)
{
	assert(num_audio_frames > 0);

	// Maybe expand the vectors
	if (check_cast<int>(left_audio_frames.size()) < num_audio_frames) {
		left_audio_frames.resize(num_audio_frames);
		right_audio_frames.resize(num_audio_frames);
	}

	float* audio_out[] = {left_audio_frames.data(), right_audio_frames.data()};

	plugin->Process(audio_out, num_audio_frames, event_list);
	event_list.Clear();

	for (auto i = 0; i < num_audio_frames; ++i) {
		audio_frame_fifo.Enqueue(
		        {left_audio_frames[i], right_audio_frames[i]});
	}
}

// The next MIDI work task is processed, which includes rendering audio frames
// prior to sending channel and sysex messages to the plugin
void MidiDeviceClapBase::ProcessWorkFromFifo()
{
	const auto work = work_fifo.Dequeue();
	if (!work) {
		return;
	}

	// Detect if the work FIFO is heavily backlogged and enter the special
	// backlogged rendering mode. This happens in fast-forward mode if the
	// CLAP synth can't keep up with the sped-up CPU emulation.
	const auto delta_from_now    = PIC_AtomicIndex() - work->timestamp;
	constexpr auto OneSecondInMs = 1000.0;

	if (enable_backlog_mode && delta_from_now > OneSecondInMs) {
		is_work_fifo_backlogged = true;
	}

	if (work->num_pending_audio_frames > 0) {
		RenderAudioFramesToFifo(work->num_pending_audio_frames);
	}

	AddClapEvent(*work);
}

void MidiDeviceClapBase::AddClapEvent(const MidiWork& work)
{
	if (work.message_type == MessageType::Channel) {
		assert(work.message.size() >= MaxMidiMessageLen);
		event_list.AddMidiEvent(work.message, 0);

	} else {
		assert(work.message_type == MessageType::SysEx);
		event_list.AddMidiSysExEvent(work.message, 0);
	}
}

void MidiDeviceClapBase::RenderBacklogged()
{
	// This will only keep the MIDI events we must process (e.g. program
	// change and SysEx messages).
	ProcessWorkFromFifoBacklogged();

	// Drip-feed essential MIDI events while backlogged and render a sample
	// periodically to keep the synth ticking. The batch size is selected by
	// the derived device for its plugin's MIDI input capacity.
	//
	// A large batch at the end can overload a synth's MIDI input buffer and
	// leave instruments in the wrong state.
	//
	if (event_list.Size() > backlogged_event_batch_size) {
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
		for (uint8_t ch = 0; ch < NumMidiChannels; ++ch) {
			const uint8_t status = MidiStatus::ControlChange | ch;
			const std::vector<uint8_t> all_notes_off_msg = {
			        status, MidiChannelMode::AllNotesOff};

			event_list.AddMidiEvent(all_notes_off_msg, 0);
		}
	}
}

void MidiDeviceClapBase::ProcessWorkFromFifoBacklogged()
{
	const auto work = work_fifo.Dequeue();
	if (!work) {
		return;
	}

	// If we're in backlogged mode when fast-forward is activated, it means
	// the CLAP synth can't keep up with the sped-up CPU emulation.
	// Therefore, we need to minimise the work to catch up.
	//
	// We can't just *not* process any MIDI events at all; we need to keep
	// processing program change, control change, etc. events, otherwise
	// there's a real chance the instrument sounds will be wrong when we
	// resume normal playback. But we can drop all MIDI notes and bypass the
	// actual audio rendering; we'll just render a few samples from time to
	// time to keep the CLAP synth ticking along. This lets us catch up and
	// stay in sync with the CPU emulation.
	//
	if (const auto status = get_midi_status(work->message[0]);
	    get_midi_message_type(status) == MessageType::Channel) {

		if (status == MidiStatus::NoteOn || status == MidiStatus::NoteOff) {
			// Drop all MIDI note messages as we won't render any audio
			return;
		}
	}

	AddClapEvent(*work);
}

// Keep the FIFO populated with freshly rendered buffers
void MidiDeviceClapBase::Render()
{
	while (work_fifo.IsRunning()) {
		if (pauser.ParkIfPaused(audio_frame_fifo)) {
			continue;
		}

		if (is_work_fifo_backlogged) {
			RenderBacklogged();

		} else {
			constexpr auto OneFrame = 1;
			work_fifo.IsEmpty() ? RenderAudioFramesToFifo(OneFrame)
			                    : ProcessWorkFromFifo();
		}
	}
}

void MidiDeviceClapBase::Pause()
{
	pauser.Pause();
}

void MidiDeviceClapBase::Resume()
{
	pauser.Resume();
}
