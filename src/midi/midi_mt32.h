// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

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

#include "audio/mixer.h"
#include "misc/std_filesystem.h"
#include "utils/rwqueue.h"

// forward declaration
class LASynthModel;

using ModelAndDir = std::pair<const LASynthModel*, std_fs::path>;

static_assert(MT32EMU_VERSION_MAJOR > 2 ||
                      (MT32EMU_VERSION_MAJOR == 2 && MT32EMU_VERSION_MINOR >= 5),
              "libmt32emu >= 2.5.0 required (using " MT32EMU_VERSION ")");

class MidiDeviceMt32 final : public MidiDevice {
public:
	// Throws `std::runtime_error` if the MIDI device cannot be initialiased
	// (e.g., the requested MT-32 ROM cannot be loaded).
	MidiDeviceMt32();

	~MidiDeviceMt32() override;

	std::string GetName() const override
	{
		return MidiDeviceName::Mt32;
	}

	Type GetType() const override
	{
		return MidiDevice::Type::Internal;
	}

	void SendMidiMessage(const MidiMessage& msg) override;
	void SendSysExMessage(uint8_t* sysex, size_t len) override;

	void PrintStats();

	ModelAndDir GetModelAndDir();
	mt32emu_rom_info GetRomInfo();

private:
	void MixerCallback(const int requested_audio_frames);
	void ProcessWorkFromFifo();

	int GetNumPendingAudioFrames();
	void RenderAudioFramesToFifo(const int num_frames = 1);
	void Render();

	// Managed objects
	MixerChannelPtr channel = nullptr;
	RWQueue<AudioFrame> audio_frame_fifo{1};
	RWQueue<MidiWork> work_fifo{1};

	std::mutex service_mutex                  = {};
	std::unique_ptr<MT32Emu::Service> service = {};
	std::thread renderer                      = {};

	ModelAndDir model_and_dir = {};

	// Used to track the balance of time between the last mixer callback
	// versus the current MIDI SysEx or Msg event.
	double last_rendered_ms   = 0.0;
	double ms_per_audio_frame = 0.0;

	bool had_underruns = false;
};

void MT32_ListDevices(MidiDeviceMt32* device, Program* caller);

#endif // C_MT32EMU

#endif
