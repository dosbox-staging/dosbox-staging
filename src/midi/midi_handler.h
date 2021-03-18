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

#ifndef DOSBOX_MIDI_HANDLER_H
#define DOSBOX_MIDI_HANDLER_H

#include "midi.h"

#include <cstdint>

enum class MIDI_RC : int {
	OK = 0,
	ERR_DEVICE_NOT_CONFIGURED = -1,
	ERR_DEVICE_LIST_NOT_SUPPORTED = -2,
};

class MidiHandler {
public:
	MidiHandler();

	MidiHandler(const MidiHandler &) = delete;            // prevent copying
	MidiHandler &operator=(const MidiHandler &) = delete; // prevent assignment

	virtual ~MidiHandler() = default;

	virtual const char *GetName() const { return "none"; }

	virtual bool Open(MAYBE_UNUSED const char *conf)
	{
		LOG_MSG("MIDI: No working MIDI device found/selected.");
		return true;
	}

	virtual void Close() {}

	void HaltSequence()
	{
		uint8_t message[3] = {}; // see MIDI_evt_len for length lookup-table
		constexpr uint8_t all_notes_off = 0x7b;
		constexpr uint8_t all_controllers_off = 0x79;

		// from the first to last channel
		for (uint8_t channel = 0xb0; channel <= 0xbf; ++channel) {
			message[0] = channel;

			message[1] = all_notes_off;
			PlayMsg(message);

			message[1] = all_controllers_off;
			PlayMsg(message);
		}
	}

	virtual void PlayMsg(MAYBE_UNUSED const uint8_t *msg) {}

	virtual void PlaySysex(MAYBE_UNUSED uint8_t *sysex, MAYBE_UNUSED size_t len) {}

	virtual MIDI_RC ListAll(Program *)
	{
		return MIDI_RC::ERR_DEVICE_LIST_NOT_SUPPORTED;
	}

	MidiHandler *next;
};

#endif
