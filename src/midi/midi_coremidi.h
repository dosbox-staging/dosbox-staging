// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MIDI_COREMIDI_H
#define DOSBOX_MIDI_COREMIDI_H

#include "midi_device.h"

#if C_COREMIDI

#include <CoreMIDI/MIDIServices.h>
#include <string>

class MidiDeviceCoreMidi final : public MidiDevice {
public:
	// Throws `std::runtime_error` if the MIDI device cannot be initialiased
	MidiDeviceCoreMidi(const char* conf);

	~MidiDeviceCoreMidi() override;

	std::string GetName() const override
	{
		return MidiDeviceName::CoreMidi;
	}

	Type GetType() const override
	{
		return MidiDevice::Type::External;
	}

	void SendMidiMessage(const MidiMessage& msg) override;

	void SendSysExMessage(uint8_t* sysex, size_t len) override;

private:
	MIDIPortRef m_port         = {};
	MIDIClientRef m_client     = {};
	MIDIEndpointRef m_endpoint = {};
	MIDIPacket* m_pCurPacket   = {};
};

void COREMIDI_ListDevices(MidiDeviceCoreMidi* device, Program* caller);

#endif // C_COREMIDI

#endif
