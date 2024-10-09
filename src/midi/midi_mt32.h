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

#ifndef DOSBOX_MIDI_MT32_H
#define DOSBOX_MIDI_MT32_H

#include "midi.h"
#include "midi_device.h"

#if C_MT32EMU

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#define MT32EMU_API_TYPE 3
#include <mt32emu/mt32emu.h>

#include "mixer.h"
#include "rwqueue.h"
#include "std_filesystem.h"

// forward declaration
class LASynthModel;

using ModelAndDir = std::pair<const LASynthModel*, std_fs::path>;

static_assert(MT32EMU_VERSION_MAJOR > 2 ||
                      (MT32EMU_VERSION_MAJOR == 2 && MT32EMU_VERSION_MINOR >= 5),
              "libmt32emu >= 2.5.0 required (using " MT32EMU_VERSION ")");

using Mt32ServicePtr = std::unique_ptr<MT32Emu::Service>;

class MidiDevice_mt32 final : public MidiDevice {
public:
	MidiDevice_mt32() = default;
	~MidiDevice_mt32() override;
	void Close() override;

	std::string GetName() const override
	{
		return "mt32";
	}

	MidiDeviceType GetDeviceType() const override
	{
		return MidiDeviceType::BuiltIn;
	}

	MIDI_RC ListAll(Program* caller) override;
	bool Open(const char* conf) override;
	void PlayMsg(const MidiMessage& msg) override;
	void PlaySysEx(uint8_t* sysex, size_t len) override;
	void PrintStats();

private:
	Mt32ServicePtr GetService();
	void MixerCallBack(const int requested_audio_frames);
	void ProcessWorkFromFifo();

	int GetNumPendingAudioFrames();
	void RenderAudioFramesToFifo(const int num_frames = 1);
	void Render();

	// Managed objects
	MixerChannelPtr channel = nullptr;
	RWQueue<AudioFrame> audio_frame_fifo{1};
	RWQueue<MidiWork> work_fifo{1};

	std::mutex service_mutex = {};
	Mt32ServicePtr service   = {};
	std::thread renderer     = {};

	std::optional<ModelAndDir> model_and_dir = {};

	// Used to track the balance of time between the last mixer callback
	// versus the current MIDI SysEx or Msg event.
	double last_rendered_ms   = 0.0;
	double ms_per_audio_frame = 0.0;

	bool had_underruns = false;
	bool is_open       = false;
};

#endif // C_MT32EMU

#endif
