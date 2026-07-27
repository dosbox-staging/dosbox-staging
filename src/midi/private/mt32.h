// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MT32_H
#define DOSBOX_MT32_H

#include "midi_synth.h"

#if C_MT32EMU

#include <memory>
#include <string>

#define MT32EMU_API_TYPE 3
#include <mt32emu/mt32emu.h>

#include "dos/programs/more_output.h"
#include "midi/midi.h"
#include "misc/std_filesystem.h"

// forward declaration
class LASynthModel;

using ModelAndDir = std::pair<const LASynthModel*, std_fs::path>;

static_assert(MT32EMU_VERSION_MAJOR > 2 ||
                      (MT32EMU_VERSION_MAJOR == 2 && MT32EMU_VERSION_MINOR >= 5),
              "libmt32emu >= 2.5.0 required (using " MT32EMU_VERSION ")");

class MidiDeviceMt32 final : public MidiSynth {
public:
	// Throws `std::runtime_error` if the MIDI device cannot be initialiased
	// (e.g., the requested MT-32 ROM cannot be loaded).
	MidiDeviceMt32();

	~MidiDeviceMt32()
	{
		Shutdown();
	}

	// prevent copying
	MidiDeviceMt32(const MidiDeviceMt32&) = delete;
	// prevent assignment
	MidiDeviceMt32& operator=(const MidiDeviceMt32&) = delete;

	std::string GetName() const override
	{
		return MidiDeviceName::Mt32;
	}

	Type GetType() const override
	{
		return MidiDevice::Type::Internal;
	}

	std::mutex service_mutex = {};

	using Mt32SynthPtr = std::unique_ptr<MT32Emu::Service>;

	void PrintStats();

	ModelAndDir GetModelAndDir();
	mt32emu_rom_info GetRomInfo();

private:
	void MixerCallback(const int requested_audio_frames);

	void ProcessWorkItem(const MidiWork& work) override;
	void RenderAudioFramesToFifo(const int num_frames) override;

	void CloseSynth() override;

	Mt32SynthPtr service = {};

	ModelAndDir model_and_dir = {};
};

void MT32_ListDevices(MidiDeviceMt32* device, MoreOutputStrings& output);

#endif // C_MT32EMU

#endif // DOSBOX_MT32_H
