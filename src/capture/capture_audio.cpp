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

#include <stdlib.h>
#include <stdio.h>

#include "mem.h"
#include "setup.h"

#define WAVE_BUF 16*1024

static struct {
	FILE *handle = nullptr;
	uint16_t buf[WAVE_BUF][2] = {};
	uint32_t used = 0;
	uint32_t length = 0;
	uint32_t freq = 0;
} wave = {};

static uint8_t wavheader[]={
	'R','I','F','F',	0x0,0x0,0x0,0x0,		/* uint32_t Riff Chunk ID /  uint32_t riff size */
	'W','A','V','E',	'f','m','t',' ',		/* uint32_t Riff Format  / uint32_t fmt chunk id */
	0x10,0x0,0x0,0x0,	0x1,0x0,0x2,0x0,		/* uint32_t fmt size / uint16_t encoding/ uint16_t channels */
	0x0,0x0,0x0,0x0,	0x0,0x0,0x0,0x0,		/* uint32_t freq / uint32_t byterate */
	0x4,0x0,0x10,0x0,	'd','a','t','a',		/* uint16_t byte-block / uint16_t bits / uint16_t data chunk id */
	0x0,0x0,0x0,0x0,							/* uint32_t data size */
};

void handle_wave_event(const bool pressed) {
	if (!pressed)
		return;
	/* Check for previously opened wave file */
	if (wave.handle) {
		LOG_MSG("Stopped capturing wave output.");
		/* Write last piece of audio in buffer */
		fwrite(wave.buf,1,wave.used*4,wave.handle);
		wave.length+=wave.used*4;
		/* Fill in the header with useful information */
		host_writed(&wavheader[0x04],wave.length+sizeof(wavheader)-8);
		host_writed(&wavheader[0x18],wave.freq);
		host_writed(&wavheader[0x1C],wave.freq*4);
		host_writed(&wavheader[0x28],wave.length);

		fseek(wave.handle,0,0);
		fwrite(wavheader,1,sizeof(wavheader),wave.handle);
		fclose(wave.handle);
		wave.handle=0;
		CaptureState |= CAPTURE_WAVE;
	}
	CaptureState ^= CAPTURE_WAVE;
}

void capture_audio_add_wave(const uint32_t freq, const uint32_t len, const int16_t * data) {
	if (!wave.handle) {
		wave.handle=CAPTURE_CreateFile("Wave Output",".wav");
		if (!wave.handle) {
			CaptureState &= ~CAPTURE_WAVE;
			return;
		}
		wave.length = 0;
		wave.used = 0;
		wave.freq = freq;
		fwrite(wavheader,1,sizeof(wavheader),wave.handle);
	}
	const int16_t * read = data;
	auto remaining = len;
	while (remaining > 0 ) {
		uint32_t left = WAVE_BUF - wave.used;
		if (!left) {
			fwrite(wave.buf,1,4*WAVE_BUF,wave.handle);
			wave.length += 4*WAVE_BUF;
			wave.used = 0;
			left = WAVE_BUF;
		}
		if (left > remaining)
			left = remaining;
		memcpy( &wave.buf[wave.used], read, left*4);
		wave.used += left;
		read += left*2;
		remaining -= left;
	}
}

