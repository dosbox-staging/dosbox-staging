/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2021  The DOSBox Staging Team
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

#include "midi_oss.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>

#include "midi.h"
#include "string_utils.h"
#include "support.h"

#define SEQ_MIDIPUTC 5

MidiHandler_oss::~MidiHandler_oss()
{
	if (is_open)
		close(device);
}

bool MidiHandler_oss::Open(const char *conf)
{
	Close();
	char devname[512];
	safe_strcpy(devname, (is_empty(conf) ? "/dev/sequencer" : conf));
	char *devfind = strrchr(devname, ',');
	if (devfind) {
		*devfind++ = '\0';
		device_num = atoi(devfind);
	} else {
		device_num = 0;
	}
	device = open(devname, O_WRONLY, 0);
	is_open = (device >= 0);
	return is_open;
}

void MidiHandler_oss::Close()
{
	if (!is_open)
		return;

	HaltSequence();

	close(device);
	is_open = false;
}

void MidiHandler_oss::PlayMsg(const uint8_t *msg)
{
	const uint8_t len = MIDI_evt_len[*msg];
	uint8_t buf[128];
	assert(len * 4 <= sizeof(buf));
	size_t pos = 0;
	for (uint8_t i = 0; i < len; i++) {
		buf[pos++] = SEQ_MIDIPUTC;
		buf[pos++] = *msg;
		buf[pos++] = device_num;
		buf[pos++] = 0;
		msg++;
	}
	write(device, buf, pos);
}

void MidiHandler_oss::PlaySysex(uint8_t *sysex, size_t len)
{
	uint8_t buf[MIDI_SYSEX_SIZE * 4];
	assert(len <= MIDI_SYSEX_SIZE);
	size_t pos = 0;
	for (size_t i = 0; i < len; i++) {
		buf[pos++] = SEQ_MIDIPUTC;
		buf[pos++] = *sysex++;
		buf[pos++] = device_num;
		buf[pos++] = 0;
	}
	write(device, buf, pos);
}
