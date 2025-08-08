// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "capture.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "midi/midi.h"
#include "hardware/pic.h"
#include "support.h"

static struct {
	FILE* handle = nullptr;

	std::array<uint8_t, 4 * 1024> buffer = {};

	uint32_t bytes_used    = 0;
	uint32_t bytes_written = 0;
	uint32_t last_tick     = 0;
} midi = {};

// clang-format off
static uint8_t midi_header[] = {
	'M',  'T',  'h', 'd', // uint32 - Chunk ID
	0x0,  0x0,  0x0, 0x6, // uint32 - Chunk length
	0x0,  0x0,            // uint16 - Format, 0 = single track
	0x0,  0x1,            // uint16 - Track count, 1 track
	0x01, 0xf4,           // uint16 - Timing, 2 beats/second with 500 frames
	'M',  'T',  'r', 'k', // uint32 - Track chunk
	0x0,  0x0,  0x0, 0x0, // uint32 - Chunk length
};
// clang-format on

static void raw_midi_add(const uint8_t data)
{
	midi.buffer[midi.bytes_used++] = data;

	if (midi.bytes_used >= midi.buffer.size()) {
		midi.bytes_written += midi.bytes_used;
		fwrite(midi.buffer.data(), 1, midi.buffer.size(), midi.handle);
		midi.bytes_used = 0;
	}
}

static void raw_midi_add_number(const uint32_t val)
{
	if (val & 0xfe00000) {
		raw_midi_add((uint8_t)(0x80 | ((val >> 21) & 0x7f)));
	}
	if (val & 0xfffc000) {
		raw_midi_add((uint8_t)(0x80 | ((val >> 14) & 0x7f)));
	}
	if (val & 0xfffff80) {
		raw_midi_add((uint8_t)(0x80 | ((val >> 7) & 0x7f)));
	}
	raw_midi_add((uint8_t)(val & 0x7f));
}

static void create_midi_file()
{
	midi.handle = CAPTURE_CreateFile(CaptureType::Midi);
	if (!midi.handle) {
		return;
	}
	fwrite(midi_header, 1, sizeof(midi_header), midi.handle);
	midi.last_tick = PIC_Ticks;
}

void capture_midi_add_data(const bool sysex, const size_t len, const uint8_t* data)
{
	if (!midi.handle) {
		create_midi_file();
	}
	if (!midi.handle) {
		return;
	}

	uint32_t delta = PIC_Ticks - midi.last_tick;
	midi.last_tick = PIC_Ticks;

	raw_midi_add_number(delta);

	if (sysex) {
		raw_midi_add(MidiStatus::SystemMessage);
		raw_midi_add_number(static_cast<uint32_t>(len));
	}
	for (size_t i = 0; i < len; ++i) {
		raw_midi_add(data[i]);
	}
}

void capture_midi_finalise()
{
	if (!midi.handle) {
		return;
	}
	// Delta time
	raw_midi_add(0x00);

	// End of track event
	raw_midi_add(0xff);
	raw_midi_add(0x2f);
	raw_midi_add(0x00);

	// Flush buffer
	fwrite(midi.buffer.data(), 1, midi.bytes_used, midi.handle);
	midi.bytes_written += midi.bytes_used;

	constexpr auto midi_header_size_offset = 18;
	if (fseek(midi.handle, midi_header_size_offset, SEEK_SET) != 0) {
		LOG_WARNING("CAPTURE: Failed to seek in captured MIDI file '%s'",
		            safe_strerror(errno).c_str());
		fclose(midi.handle);
		midi = {};
		return;
	}

	uint8_t size[4];

	size[0] = (uint8_t)(midi.bytes_written >> 24);
	size[1] = (uint8_t)(midi.bytes_written >> 16);
	size[2] = (uint8_t)(midi.bytes_written >> 8);
	size[3] = (uint8_t)(midi.bytes_written >> 0);
	fwrite(&size, 1, 4, midi.handle);

	fclose(midi.handle);
	midi = {};
	return;
}

