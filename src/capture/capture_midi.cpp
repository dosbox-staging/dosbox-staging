/*
 *  Copyright (C) 2002-2023  The DOSBox Team
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

#include "capture.h"

#include <cerrno>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pic.h"

#define MIDI_BUF 4 * 1024

static struct {
	FILE* handle             = nullptr;
	uint8_t buffer[MIDI_BUF] = {0};
	uint32_t used            = 0;
	uint32_t done            = 0;
	uint32_t last            = 0;
} midi = {};

static uint8_t midi_header[] = {
        'M',  'T',  'h', 'd', /* uint32_t, Header Chunk */
        0x0,  0x0,  0x0, 0x6, /* uint32_t, Chunk Length */
        0x0,  0x0,            /* uint16_t, Format, 0=single track */
        0x0,  0x1,            /* uint16_t, Track Count, 1 track */
        0x01, 0xf4, /* uint16_t, Timing, 2 beats/second with 500 frames */
        'M',  'T',  'r', 'k', /* uint32_t, Track Chunk */
        0x0,  0x0,  0x0, 0x0, /* uint32_t, Chunk Length */
                              // Track data
};

static void raw_midi_add(const uint8_t data)
{
	midi.buffer[midi.used++] = data;
	if (midi.used >= MIDI_BUF) {
		midi.done += midi.used;
		fwrite(midi.buffer, 1, MIDI_BUF, midi.handle);
		midi.used = 0;
	}
}

static void raw_midi_add_number(const uint32_t val)
{
	if (val & 0xfe00000)
		raw_midi_add((uint8_t)(0x80 | ((val >> 21) & 0x7f)));
	if (val & 0xfffc000)
		raw_midi_add((uint8_t)(0x80 | ((val >> 14) & 0x7f)));
	if (val & 0xfffff80)
		raw_midi_add((uint8_t)(0x80 | ((val >> 7) & 0x7f)));
	raw_midi_add((uint8_t)(val & 0x7f));
}

void capture_add_midi(const bool sysex, const size_t len, const uint8_t* data)
{
	if (!midi.handle) {
		midi.handle = CAPTURE_CreateFile("MIDI output", ".mid");
		if (!midi.handle) {
			return;
		}
		fwrite(midi_header, 1, sizeof(midi_header), midi.handle);
		midi.last = PIC_Ticks;
	}
	uint32_t delta = PIC_Ticks - midi.last;
	midi.last      = PIC_Ticks;
	raw_midi_add_number(delta);
	if (sysex) {
		raw_midi_add(0xf0);
		raw_midi_add_number(static_cast<uint32_t>(len));
	}
	for (size_t i = 0; i < len; i++)
		raw_midi_add(data[i]);
}

void handle_midi_event(const bool pressed)
{
	if (!midi.handle) {
		return;
	}
	if (!pressed)
		return;
	/* Check for previously opened wave file */
	if (midi.handle) {
		LOG_MSG("CAPTURE: Stopping capturing MIDI output");
		// Delta time
		raw_midi_add(0x00);
		// End of track event
		raw_midi_add(0xff);
		raw_midi_add(0x2F);
		raw_midi_add(0x00);
		/* clear out the final data in the buffer if any */
		fwrite(midi.buffer, 1, midi.used, midi.handle);
		midi.done += midi.used;
		if (fseek(midi.handle, 18, SEEK_SET) != 0) {
			LOG_WARNING("CAPTURE: Failed to seek in captured MIDI file '%s'",
			            strerror(errno));
			CaptureState ^= CAPTURE_MIDI;
			return;
		}
		uint8_t size[4];
		size[0] = (uint8_t)(midi.done >> 24);
		size[1] = (uint8_t)(midi.done >> 16);
		size[2] = (uint8_t)(midi.done >> 8);
		size[3] = (uint8_t)(midi.done >> 0);
		fwrite(&size, 1, 4, midi.handle);
		fclose(midi.handle);
		midi.handle = 0;
		CaptureState &= ~CAPTURE_MIDI;
		return;
	}
	CaptureState ^= CAPTURE_MIDI;
	if (CaptureState & CAPTURE_MIDI) {
		LOG_MSG("CAPTURE: Preparing to capture MIDI output; capturing will start on the first MIDI message");
		midi.used   = 0;
		midi.done   = 0;
		midi.handle = 0;
	} else {
		LOG_MSG("CAPTURE: Stopped capturing MIDI output before any MIDI message was output (no capture file was created)");
	}
}

