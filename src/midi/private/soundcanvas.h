// SPDX-FileCopyrightText:  2024-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SOUNDCANVAS_H
#define DOSBOX_SOUNDCANVAS_H

#include "dos/programs/more_output.h"
#include "clap_base.h"

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

class MidiDeviceSoundCanvas final : public MidiDeviceClapBase {
public:
	// Throws `std::runtime_error` if the requested Sound Canvas model
	// cannot be loaded.
	MidiDeviceSoundCanvas();

	~MidiDeviceSoundCanvas() override;

	// prevent copying
	MidiDeviceSoundCanvas(const MidiDeviceSoundCanvas&) = delete;
	// prevent assignment
	MidiDeviceSoundCanvas& operator=(const MidiDeviceSoundCanvas&) = delete;

	std::string GetName() const override
	{
		return MidiDeviceName::SoundCanvas;
	}

	SoundCanvas::SynthModel GetModel() const;

private:
	void ConfigureMixerChannel(MixerChannelPtr& mixer_channel);

	SoundCanvas::SynthModel model = {};
};

void SOUNDCANVAS_ListDevices(MidiDeviceSoundCanvas* device, MoreOutputStrings& output);

#endif // DOSBOX_SOUNDCANVAS_H
