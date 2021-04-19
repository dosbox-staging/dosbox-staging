/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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

#include "midi_handler.h"

class MidiHandler_oss final : public MidiHandler {
private:
	int device = 0;
	uint8_t device_num = 0;
	bool is_open = false;

public:
	MidiHandler_oss() : MidiHandler() {}

	MidiHandler_oss(const MidiHandler_oss &) = delete; // prevent copying
	MidiHandler_oss &operator=(const MidiHandler_oss &) = delete; // prevent assignment

	~MidiHandler_oss() override;

	const char *GetName() const override { return "oss"; }

	bool Open(const char *conf) override;

	void Close() override;

	void PlayMsg(const uint8_t *msg) override;

	void PlaySysex(uint8_t *sysex, size_t len) override;
};

#endif
