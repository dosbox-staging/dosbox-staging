// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MIDI_CLAP_H
#define DOSBOX_MIDI_CLAP_H

#include "clap_base.h"
#include "dos/programs/more_output.h"

class MidiDeviceClap final : public MidiDeviceClapBase {
public:
	// Throws std::runtime_error if the configured plugin cannot be loaded.
	MidiDeviceClap();

	std::string GetName() const override
	{
		return MidiDeviceName::Clap;
	}

	const Clap::PluginInfo& GetPluginInfo() const
	{
		return plugin_info;
	}

private:
	Clap::PluginInfo plugin_info = {};
};

void CLAP_ListDevices(MidiDeviceClap* device, MoreOutputStrings& output);

#endif // DOSBOX_MIDI_CLAP_H
