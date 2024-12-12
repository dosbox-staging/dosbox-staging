/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2020  The DOSBox Team
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
