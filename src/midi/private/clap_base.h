// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MIDI_CLAP_BASE_H
#define DOSBOX_MIDI_CLAP_BASE_H

#include "midi_device.h"
#include "synth_render_pauser.h"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "audio/clap/event_list.h"
#include "audio/clap/plugin.h"
#include "audio/mixer.h"
#include "utils/rwqueue.h"

// Common MIDI input, CLAP rendering, and mixer lifecycle for internal CLAP
// synths. Derived devices select the plugin and configure its mixer channel.
class MidiDeviceClapBase : public MidiDevice {
public:
	~MidiDeviceClapBase() override;

	MidiDeviceClapBase(const MidiDeviceClapBase&)            = delete;
	MidiDeviceClapBase& operator=(const MidiDeviceClapBase&) = delete;

	Type GetType() const override
	{
		return Type::Internal;
	}

	void SendMidiMessage(const MidiMessage& msg) override;
	void SendSysExMessage(uint8_t* sysex, size_t len) override;

	void Pause() override;
	void Resume() override;

	// Set process-wide plugin options before loading or enumerating plugins.
	static void SetEnvironmentVariable(const std::string& name,
	                                   const std::string& value,
	                                   const std::string& log_prefix = "CLAP");

protected:
	MidiDeviceClapBase() = default;

	struct Config {
		float sample_rate_hz                                    = 0.0f;
		std::string mixer_channel_name                          = {};
		std::set<ChannelFeature> channel_features               = {};
		std::string log_prefix                                  = {};
		std::string renderer_thread_name                        = {};
		std::function<void(MixerChannelPtr&)> configure_channel = {};
		bool enable_backlog_mode                                = false;
		size_t backlogged_event_batch_size                      = 10;
	};

	// Call once from the derived constructor after selecting a plugin. The
	// channel configuration runs while the mixer is locked, before playback.
	void Initialize(std::unique_ptr<Clap::Plugin> plugin, const Config& config);

private:
	void MixerCallback(int requested_audio_frames);
	void ProcessWorkFromFifo();
	void ProcessWorkFromFifoBacklogged();

	int GetNumPendingAudioFrames();
	void RenderAudioFramesToFifo(int num_audio_frames);
	void Render();
	void RenderBacklogged();

	void AddClapEvent(const MidiWork& work);

	MixerChannelPtr mixer_channel        = nullptr;
	RWQueue<AudioFrame> audio_frame_fifo = {1};
	RWQueue<MidiWork> work_fifo          = {1};

	std::unique_ptr<Clap::Plugin> plugin = nullptr;
	Clap::EventList event_list           = {};

	std::thread renderer     = {};
	SynthRenderPauser pauser = {};

	// Buffer storage belongs to this device so separate CLAP synths can
	// render and mix concurrently.
	std::vector<AudioFrame> mixer_audio_frames = {};
	std::vector<float> left_audio_frames       = {};
	std::vector<float> right_audio_frames      = {};

	std::string log_prefix             = {};
	bool enable_backlog_mode           = false;
	size_t backlogged_event_batch_size = 10;
	double last_rendered_ms            = 0.0;
	double ms_per_audio_frame          = 0.0;
	int underrun_warning_iteration     = 0;
	bool had_underruns                 = false;
	bool is_work_fifo_backlogged       = false;
};

#endif // DOSBOX_MIDI_CLAP_BASE_H
