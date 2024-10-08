/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_SOUNDCANVAS_H
#define DOSBOX_SOUNDCANVAS_H

#include "midi.h"
#include "midi_device.h"

#include <memory>
#include <optional>

#include "../audio/clap/event_list.h"
#include "../audio/clap/plugin.h"
#include "mixer.h"
#include "rwqueue.h"

enum class SoundCanvasModel {
	// Roland SC-55
	Sc55_100,
	Sc55_110,
	Sc55_120,
	Sc55_121,
	Sc55_200,

	// Roland SC-55mk2
	Sc55mk2_100,
	Sc55mk2_101,
};

struct PluginAndModel {
	std::unique_ptr<Clap::Plugin> plugin = nullptr;
	SoundCanvasModel model = {};
};

class MidiDeviceSoundCanvas final : public MidiDevice {
public:
	MidiDeviceSoundCanvas() = default;
	~MidiDeviceSoundCanvas() override;

	// prevent copying
	MidiDeviceSoundCanvas(const MidiDeviceSoundCanvas&) = delete;
	// prevent assignment
	MidiDeviceSoundCanvas& operator=(const MidiDeviceSoundCanvas&) = delete;

	std::string GetName() const override
	{
		return "soundcanvas";
	}

	MidiDevice::Type GetType() const override
	{
		return MidiDevice::Type::Internal;
	}

	ListDevicesResult ListDevices(Program* caller) override;

	bool Initialise(const char* conf) override;

	void SendMessage(const MidiMessage& msg) override;
	void SendSysExMessage(uint8_t* sysex, size_t len) override;

private:
	std::optional<SoundCanvasModel> active_model = {};

	PluginAndModel TryLoadPlugin(const SoundCanvasModel model);
	PluginAndModel LoadModel(const std::string& model_name);

	void MixerCallback(const int requested_audio_frames);
	void ProcessWorkFromFifo();

	int GetNumPendingAudioFrames();
	void RenderAudioFramesToFifo(const int num_frames = 1);
	void Render();

	// Managed objects
	MixerChannelPtr mixer_channel = nullptr;
	RWQueue<AudioFrame> audio_frame_fifo{1};
	RWQueue<MidiWork> work_fifo{1};

	struct {
		std::unique_ptr<Clap::Plugin> plugin = nullptr;
		Clap::EventList event_list           = {};
	} clap = {};

	std::thread renderer = {};

	// Used to track the balance of time between the last mixer callback
	// versus the current MIDI SysEx or Msg event.
	double last_rendered_ms   = 0.0;
	double ms_per_audio_frame = 0.0;

	bool had_underruns = false;
	bool is_open       = false;
};

const clap_plugin_t* SOUNDCANVAS_LoadPlugin(const SoundCanvasModel model);

#endif // DOSBOX_SOUNDCANVAS_H

