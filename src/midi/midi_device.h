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

#ifndef DOSBOX_MIDI_DEVICE_H
#define DOSBOX_MIDI_DEVICE_H

#include "midi.h"

#include <cstdint>

enum class MIDI_RC : int {
	OK                            = 0,
	ERR_DEVICE_NOT_CONFIGURED     = -1,
	ERR_DEVICE_LIST_NOT_SUPPORTED = -2,
};

class MidiDevice {
public:
	enum class Type { BuiltIn, External };

	MidiDevice() {}
	MidiDevice(const MidiDevice&) = delete;            // prevent copying
	MidiDevice& operator=(const MidiDevice&) = delete; // prevent assignment

	virtual ~MidiDevice() = default;

	virtual std::string GetName() const = 0;

	virtual Type GetType() const
	{
		return Type::External;
	}

	virtual bool Open([[maybe_unused]] const char* conf)
	{
		LOG_WARNING("MIDI: No working MIDI device found/selected.");
		return true;
	}

	virtual void Reset();

	virtual void Close()
	{
		Reset();
	}

	virtual void SendMidiMessage([[maybe_unused]] const MidiMessage& msg) {}
	virtual void SendSysExMessage([[maybe_unused]] uint8_t* sysex,
	                       [[maybe_unused]] size_t len)
	{}

	virtual MIDI_RC ListAll(Program*)
	{
		return MIDI_RC::ERR_DEVICE_LIST_NOT_SUPPORTED;
	}
};

void MIDI_RegisterHandler(std::unique_ptr<MidiDevice> handler);
MidiDevice* MIDI_GetHandler(const std::string_view name);
void MIDI_DeregisterHandler(const std::string_view name);

#endif
