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

#ifndef DOSBOX_MIDI_OSS_H
#define DOSBOX_MIDI_OSS_H

#include "midi_device.h"

class MidiDevice_oss final : public MidiDevice {
private:
	int device = 0;
	uint8_t device_num = 0;
	bool is_open = false;

public:
	MidiDevice_oss() : MidiDevice() {}

	MidiDevice_oss(const MidiDevice_oss &) = delete; // prevent copying
	MidiDevice_oss &operator=(const MidiDevice_oss &) = delete; // prevent assignment

	~MidiDevice_oss() override;

	std::string GetName() const override
	{
		return "oss";
	}

	MidiDeviceType GetDeviceType() const override
	{
		return MidiDeviceType::External;
	}

	bool Open(const char *conf) override;

	void Close() override;

	void PlayMsg(const MidiMessage& msg) override;

	void PlaySysEx(uint8_t *sysex, size_t len) override;
};

#endif
