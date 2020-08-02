/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2002-2020  The DOSBox Team
 *  Copyright (C) 2020-2020  The dosbox-staging team
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

#ifndef DOSBOX_MIDI_H
#define DOSBOX_MIDI_H

#include "dosbox.h"

#include "programs.h"

class MidiHandler {
public:
	MidiHandler();

	MidiHandler(const MidiHandler &) = delete; // prevent copying
	MidiHandler &operator=(const MidiHandler &) = delete; // prevent assignment

	virtual ~MidiHandler() = default;

	virtual bool Open(const char * /*conf*/) {
		LOG_MSG("MIDI: No working MIDI device found/selected.");
		return true;
	}
	virtual void Close(void) {};
	virtual void PlayMsg(Bit8u * /*msg*/) {};
	virtual void PlaySysex(Bit8u * /*sysex*/,Bitu /*len*/) {};
	virtual const char * GetName(void) { return "none"; };
	virtual void ListAll(Program * base) {};

	MidiHandler *next;
};

#define SYSEX_SIZE 8192

void MIDI_Init(Section *sec);
bool MIDI_Available();
void MIDI_ListAll(Program *output_handler);
void MIDI_RawOutByte(uint8_t data);

#endif
