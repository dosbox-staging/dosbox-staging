/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_MIDI_FLUIDSYNTH_H
#define DOSBOX_MIDI_FLUIDSYNTH_H

#include "midi_device.h"

#if C_FLUIDSYNTH

#include <atomic>
#include <fluidsynth.h>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "mixer.h"
#include "rwqueue.h"
#include "std_filesystem.h"

class MidiDeviceFluidSynth final : public MidiDevice {
public:
	MidiDeviceFluidSynth() = default;
	~MidiDeviceFluidSynth() override;
	void PrintStats();

	std::string GetName() const override
	{
		return "fluidsynth";
	}

	MidiDeviceType GetDeviceType() const override
	{
		return MidiDeviceType::BuiltIn;
	}

	bool Open(const char* conf) override;
	void Close() override;
	void PlayMsg(const MidiMessage& msg) override;
	void PlaySysEx(uint8_t *sysex, size_t len) override;
	MIDI_RC ListAll(Program *caller) override;

private:
	void ApplyChannelMessage(const std::vector<uint8_t>& msg);
	void ApplySysExMessage(const std::vector<uint8_t>& msg);
	void MixerCallBack(const int requested_audio_frames);
	void ProcessWorkFromFifo();

	int GetNumPendingAudioFrames();
	void RenderAudioFramesToFifo(const int num_audio_frames = 1);
	void Render();

	using FluidSynthSettingsPtr =
	        std::unique_ptr<fluid_settings_t, decltype(&delete_fluid_settings)>;
	using FluidSynthPtr = std::unique_ptr<fluid_synth_t, decltype(&delete_fluid_synth)>;

	FluidSynthSettingsPtr settings{nullptr, &delete_fluid_settings};
	FluidSynthPtr synth{nullptr, &delete_fluid_synth};

	MixerChannelPtr mixer_channel = nullptr;
	RWQueue<AudioFrame> audio_frame_fifo{1};
	RWQueue<MidiWork> work_fifo{1};
	std::thread renderer = {};

	std::optional<std_fs::path> current_sf2_path = {};

	// Used to track the balance of time between the last mixer callback
	// versus the current MIDI SysEx or Msg event.
	double last_rendered_ms = 0.0;
	double ms_per_audio_frame = 0.0;

	bool had_underruns = false;
	bool is_open       = false;
};

#endif // C_FLUIDSYNTH

#endif
