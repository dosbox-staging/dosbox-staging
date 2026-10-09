// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MIDI_CLAP_H
#define DOSBOX_MIDI_CLAP_H

#include "clap_synth.h"
#include "midi_synth.h"

#include "audio/clap/library.h"
#include "dos/programs/more_output.h"

class MidiDeviceClap final : public MidiSynth {
public:
	MidiDeviceClap();
	~MidiDeviceClap()
	{
		Shutdown();
	}

	MidiDeviceClap(const MidiDeviceClap&)            = delete;
	MidiDeviceClap& operator=(const MidiDeviceClap&) = delete;

	std::string GetName() const override
	{
		return MidiDeviceName::Clap;
	}

	Type GetType() const override
	{
		return Type::Internal;
	}

	const Clap::PluginInfo& GetPluginInfo() const
	{
		return plugin_info;
	}

private:
	void ProcessWorkItem(const MidiWork& work) override;
	void RenderAudioFramesToFifo(const int num_frames) override;
	void CloseSynth() override {}
	void CloseRenderer() override
	{
		clap.StopProcessing();
	}

	ClapSynth clap               = {};
	Clap::PluginInfo plugin_info = {};
};

void CLAP_ListDevices(const MidiDeviceClap* device, MoreOutputStrings& output);

#endif // DOSBOX_MIDI_CLAP_H
