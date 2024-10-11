/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2023  The DOSBox Staging Team
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

MidiDeviceOss::MidiDeviceOss(const char* conf)
{
	char devname[512];
	safe_strcpy(devname, (is_empty(conf) ? "/dev/sequencer" : conf));
	char* devfind = strrchr(devname, ',');

	if (devfind) {
		*devfind++ = '\0';
		device_num = atoi(devfind);
	} else {
		device_num = 0;
	}

	device = open(devname, O_WRONLY, 0);

	const auto is_open = (device >= 0);
	if (!is_open) {
		const auto msg = "MIDI:OSS: Error opening device";
		LOG_WARNING("%s", msg);
		throw std::runtime_error(msg);
	}
}

MidiDeviceOss::~MidiDeviceOss()
{
	Reset();
	close(device);
}

void MidiDeviceOss::SendMidiMessage(const MidiMessage& msg)
{
	const auto len = MIDI_message_len_by_status[msg.status()];

	uint8_t buf[128];
	assert(len * 4 <= sizeof(buf));

	size_t pos = 0;
	for (uint8_t i = 0; i < len; i++) {
		buf[pos++] = SEQ_MIDIPUTC;
		buf[pos++] = msg[i];
		buf[pos++] = device_num;
		buf[pos++] = 0;
	}

	const auto rcode = write(device, buf, pos);
	if (!rcode) {
		LOG_WARNING("MIDI:OSS: Failed to play message");
	}
}

void MidiDeviceOss::SendSysExMessage(uint8_t* sysex, size_t len)
{
	uint8_t buf[MaxMidiSysExSize * 4];
	assert(len <= MaxMidiSysExSize);

	size_t pos = 0;
	for (size_t i = 0; i < len; i++) {
		buf[pos++] = SEQ_MIDIPUTC;
		buf[pos++] = *sysex++;
		buf[pos++] = device_num;
		buf[pos++] = 0;
	}

	const auto rcode = write(device, buf, pos);
	if (!rcode) {
		LOG_WARNING("MIDI:OSS: Failed to write SysEx message");
	}
}

void MIDI_OSS_ListDevices([[maybe_unused]] MidiDeviceOss* device,
                          [[maybe_unused]] Program* caller)
{
	caller->WriteOut("  %s\n\n", MSG_Get("MIDI_DEVICE_LIST_NOT_SUPPORTED"));
}

