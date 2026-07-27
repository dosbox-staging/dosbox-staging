// SPDX-FileCopyrightText:  2024-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SOUNDCANVAS_H
#define DOSBOX_SOUNDCANVAS_H

#include "midi_synth.h"

#include <memory>
#include <optional>

#include "audio/clap/event_list.h"
#include "audio/clap/plugin.h"
#include "audio/mixer.h"
#include "dos/programs/more_output.h"

namespace SoundCanvas {

enum class Model {
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

struct SynthModel {
	Model model = {};

	const char* config_name        = {};
	const char* display_name_short = {};
	const char* display_name_long  = {};

	bool operator==(const SynthModel* other) const
	{
		return model == (*other).model;
	}
};

} // namespace SoundCanvas

class MidiDeviceSoundCanvas final : public MidiSynth {
public:
	// Throws `std::runtime_error` if the MIDI device cannot be
	// initialiased (e.g., the requested SoundFont cannot be loaded).
	MidiDeviceSoundCanvas();

	~MidiDeviceSoundCanvas()
	{
		Shutdown();
	}

	// prevent copying
	MidiDeviceSoundCanvas(const MidiDeviceSoundCanvas&) = delete;
	// prevent assignment
	MidiDeviceSoundCanvas& operator=(const MidiDeviceSoundCanvas&) = delete;

	std::string GetName() const override
	{
		return MidiDeviceName::SoundCanvas;
	}

	Type GetType() const override
	{
		return MidiDevice::Type::Internal;
	}

	SoundCanvas::SynthModel GetModel() const;

private:
	void MixerCallback(const int requested_audio_frames);

	void ProcessWorkItem(const MidiWork& work) override;
	void RenderAudioFramesToFifo(const int num_frames) override;

	void CloseSynth() override {}

	struct {
		std::unique_ptr<Clap::Plugin> plugin = nullptr;
		Clap::EventList event_list           = {};
	} clap = {};

	SoundCanvas::SynthModel model = {};
};

void SOUNDCANVAS_ListDevices(MidiDeviceSoundCanvas* device, MoreOutputStrings& output);

#endif // DOSBOX_SOUNDCANVAS_H
