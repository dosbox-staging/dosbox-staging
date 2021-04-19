/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_MIDI_ALSA_H
#define DOSBOX_MIDI_ALSA_H

#include "midi_handler.h"

#if C_ALSA

#include <alsa/asoundlib.h>

struct alsa_address {
	int client;
	int port;
};

class MidiHandler_alsa final : public MidiHandler {
private:
	snd_seq_event_t ev = {};
	snd_seq_t *seq_handle = nullptr;
	alsa_address seq = {-1, -1}; // address of input port we're connected to
	int output_port = 0;

	void send_event(int do_flush);

public:
	MidiHandler_alsa() : MidiHandler() {}

	MidiHandler_alsa(const MidiHandler_alsa &) = delete; // prevent copying
	MidiHandler_alsa &operator=(const MidiHandler_alsa &) = delete; // prevent assignment

	const char *GetName() const override { return "alsa"; }
	bool Open(const char *conf) override;
	void Close() override;
	void PlayMsg(const uint8_t *msg) override;
	void PlaySysex(uint8_t *sysex, size_t len) override;
	MIDI_RC ListAll(Program *caller) override;
};

#endif // C_ALSA

#endif
