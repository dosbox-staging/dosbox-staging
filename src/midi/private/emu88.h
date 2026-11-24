// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_EMU88_H
#define DOSBOX_EMU88_H

#include "midi_synth.h"

#include <string>

#include "clap_synth.h"
#include "dos/programs/more_output.h"

// Roland MIDI sound modules emulated by 88emu: the MT-32 and CM family, and
// the Sound Canvas line from the SC-55 to the SC-8850. 88emu runs the original
// firmware of each device, and ships as the '88emuClapHeadless.clap' CLAP
// library with one plugin per device.
namespace Emu88 {

constexpr auto LibraryName = "88emuClapHeadless.clap";

// Every plugin ID of the library starts with this
constexpr auto PluginIdPrefix = "com.theusualsuspects.88emuclapheadless.";

enum class Family {
	// LA synthesis: MT-32, CM-32L/LN, and the CM-32P and CM-64
	Mt32,

	// The SC-55 generation, including its card and module variants
	Sc55,

	// The SC-88 generation
	Sc88,

	// The SC-8820 and SC-8850
	Sc8850,

	// Anything else 88emu emulates
	Other,
};

struct SynthModel {
	// The value of the '88emu_model' setting
	const char* config_name = {};

	const char* display_name = {};

	// Appended to PluginIdPrefix
	const char* plugin_id_suffix = {};

	Family family = {};
};

} // namespace Emu88

class MidiDeviceEmu88 final : public MidiSynth {
public:
	MidiDeviceEmu88();

	~MidiDeviceEmu88()
	{
		Shutdown();
	}

	MidiDeviceEmu88(const MidiDeviceEmu88&)            = delete;
	MidiDeviceEmu88& operator=(const MidiDeviceEmu88&) = delete;

	std::string GetName() const override
	{
		return MidiDeviceName::Emu88;
	}

	Type GetType() const override
	{
		return MidiDevice::Type::Internal;
	}

	const Emu88::SynthModel& GetModel() const;

private:
	void ProcessWorkItem(const MidiWork& work) override;
	void RenderAudioFramesToFifo(const int num_frames) override;

	void CloseSynth() override {}

	void CloseRenderer() override
	{
		clap.StopProcessing();
	}

	ClapSynth clap = {};

	const Emu88::SynthModel* model = nullptr;
};

void EMU88_ListDevices(MidiDeviceEmu88* device, MoreOutputStrings& output);

#endif // DOSBOX_EMU88_H
