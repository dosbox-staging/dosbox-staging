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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "cross.h"
#include "fs_utils.h"
#include "mapper.h"
#include "mem.h"
#include "pic.h"
#include "render.h"
#include "rgb24.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"

#if (C_SSHOT)
#include "../libs/zmbv/zmbv.h"
#endif

#include "capture_image.h"
#include "capture_video.h"

static std::string capturedir;
extern const char* RunningProgram;
Bitu CaptureState;

#define WAVE_BUF 16*1024
#define MIDI_BUF 4*1024

static struct {
	struct {
		FILE *handle = nullptr;
		uint16_t buf[WAVE_BUF][2] = {};
		uint32_t used = 0;
		uint32_t length = 0;
		uint32_t freq = 0;
	} wave = {};

	struct {
		FILE *handle = nullptr;
		uint8_t buffer[MIDI_BUF] = {0};
		uint32_t used = 0;
		uint32_t done = 0;
		uint32_t last = 0;
	} midi = {};
} capture = {};

std::string CAPTURE_GenerateFilename(const char *type, const char *ext)
{
	if (capturedir.empty()) {
		LOG_MSG("Please specify a capture directory");
		return 0;
	}

	char file_start[16];
	dir_information *dir;
	/* Find a filename to open */
	dir = open_directory(capturedir.c_str());
	if (!dir) {
		// Try creating it first
		if (create_dir(capturedir, 0700, OK_IF_EXISTS) != 0) {
			LOG_WARNING("Can't create dir '%s' for capturing: %s",
			            capturedir.c_str(),
			            safe_strerror(errno).c_str());
			return 0;
		}
		dir = open_directory(capturedir.c_str());
		if (!dir) {
			LOG_MSG("Can't open dir %s for capturing %s",
			        capturedir.c_str(),
			        type);
			return 0;
		}
	}
	safe_strcpy(file_start, RunningProgram);
	lowcase(file_start);
	strcat(file_start,"_");
	bool is_directory;
	char tempname[CROSS_LEN];
	bool testRead = read_directory_first(dir, tempname, is_directory );
	int last = 0;
	for ( ; testRead; testRead = read_directory_next(dir, tempname, is_directory) ) {
		char * test=strstr(tempname,ext);
		if (!test || strlen(test)!=strlen(ext))
			continue;
		*test=0;
		if (strncasecmp(tempname,file_start,strlen(file_start))!=0) continue;
		const int num = atoi(&tempname[strlen(file_start)]);
		if (num >= last)
			last = num + 1;
	}
	close_directory( dir );
	char file_name[CROSS_LEN];
	sprintf(file_name, "%s%c%s%03d%s",
	        capturedir.c_str(), CROSS_FILESPLIT, file_start, last, ext);
	return file_name;
}

FILE *CAPTURE_CreateFile(const char *type, const char *ext)
{
	const auto file_name = CAPTURE_GenerateFilename(type, ext);

	FILE *handle = fopen(file_name.c_str(), "wb");
	if (handle) {
		LOG_MSG("Capturing %s to %s", type, file_name.c_str());
	} else {
		LOG_MSG("Failed to open %s for capturing %s", file_name.c_str(), type);
	}
	return handle;
}

void CAPTURE_VideoStart() {
#if (C_SSHOT)
	if (CaptureState & CAPTURE_VIDEO) {
		LOG_MSG("Already capturing video.");
	} else {
		const auto pressed = true;
		handle_video_event(pressed);
	}
#else
	LOG_MSG("Avi capturing has not been compiled in");
#endif
}

void CAPTURE_VideoStop() {
#if (C_SSHOT)
	if (CaptureState & CAPTURE_VIDEO) {
		const auto pressed = true;
		handle_video_event(pressed);
	} else {
		LOG_MSG("Not capturing video.");
	}
#else
	LOG_MSG("Avi capturing has not been compiled in");
#endif
}

void CAPTURE_AddImage([[maybe_unused]] uint16_t width,
                      [[maybe_unused]] uint16_t height,
                      [[maybe_unused]] uint8_t bits_per_pixel,
                      [[maybe_unused]] uint16_t pitch,
                      [[maybe_unused]] uint8_t capture_flags,
                      [[maybe_unused]] float frames_per_second,
                      [[maybe_unused]] uint8_t* image_data,
                      [[maybe_unused]] uint8_t* palette_data)
{
#if (C_SSHOT)
	if (capture_flags & CAPTURE_FLAG_DBLH) {
		height *= 2;
	}
	if (capture_flags & CAPTURE_FLAG_DBLW) {
		width *= 2;
	}
	if (height > SCALER_MAXHEIGHT) {
		return;
	}
	if (width > SCALER_MAXWIDTH) {
		return;
	}

	if (CaptureState & CAPTURE_IMAGE) {
		capture_image(width, height, bits_per_pixel, pitch, capture_flags, image_data, palette_data);
		CaptureState &= ~CAPTURE_IMAGE;
	}
	if (CaptureState & CAPTURE_VIDEO) {
		capture_video(width,
		              height,
		              bits_per_pixel,
		              pitch,
		              capture_flags,
		              frames_per_second,
		              image_data,
		              palette_data);
		CaptureState &= ~CAPTURE_VIDEO;
	}
#endif
}

#if (C_SSHOT)
static void handle_screenshot_event(bool pressed) {
	if (!pressed) {
		return;
	}
	CaptureState |= CAPTURE_IMAGE;
}
#endif


/* WAV capturing */
static uint8_t wavheader[]={
	'R','I','F','F',	0x0,0x0,0x0,0x0,		/* uint32_t Riff Chunk ID /  uint32_t riff size */
	'W','A','V','E',	'f','m','t',' ',		/* uint32_t Riff Format  / uint32_t fmt chunk id */
	0x10,0x0,0x0,0x0,	0x1,0x0,0x2,0x0,		/* uint32_t fmt size / uint16_t encoding/ uint16_t channels */
	0x0,0x0,0x0,0x0,	0x0,0x0,0x0,0x0,		/* uint32_t freq / uint32_t byterate */
	0x4,0x0,0x10,0x0,	'd','a','t','a',		/* uint16_t byte-block / uint16_t bits / uint16_t data chunk id */
	0x0,0x0,0x0,0x0,							/* uint32_t data size */
};

void CAPTURE_AddWave(uint32_t freq, uint32_t len, int16_t * data) {
#if (C_SSHOT)
	if (CaptureState & CAPTURE_VIDEO) {
		capture_video_add_wave(freq, len, data);
	}
#endif
	if (CaptureState & CAPTURE_WAVE) {
		if (!capture.wave.handle) {
			capture.wave.handle=CAPTURE_CreateFile("Wave Output",".wav");
			if (!capture.wave.handle) {
				CaptureState &= ~CAPTURE_WAVE;
				return;
			}
			capture.wave.length = 0;
			capture.wave.used = 0;
			capture.wave.freq = freq;
			fwrite(wavheader,1,sizeof(wavheader),capture.wave.handle);
		}
		int16_t * read = data;
		while (len > 0 ) {
			Bitu left = WAVE_BUF - capture.wave.used;
			if (!left) {
				fwrite(capture.wave.buf,1,4*WAVE_BUF,capture.wave.handle);
				capture.wave.length += 4*WAVE_BUF;
				capture.wave.used = 0;
				left = WAVE_BUF;
			}
			if (left > len)
				left = len;
			memcpy( &capture.wave.buf[capture.wave.used], read, left*4);
			capture.wave.used += left;
			read += left*2;
			len -= left;
		}
	}
}

static void handle_wave_event(bool pressed) {
	if (!pressed)
		return;
	/* Check for previously opened wave file */
	if (capture.wave.handle) {
		LOG_MSG("Stopped capturing wave output.");
		/* Write last piece of audio in buffer */
		fwrite(capture.wave.buf,1,capture.wave.used*4,capture.wave.handle);
		capture.wave.length+=capture.wave.used*4;
		/* Fill in the header with useful information */
		host_writed(&wavheader[0x04],capture.wave.length+sizeof(wavheader)-8);
		host_writed(&wavheader[0x18],capture.wave.freq);
		host_writed(&wavheader[0x1C],capture.wave.freq*4);
		host_writed(&wavheader[0x28],capture.wave.length);

		fseek(capture.wave.handle,0,0);
		fwrite(wavheader,1,sizeof(wavheader),capture.wave.handle);
		fclose(capture.wave.handle);
		capture.wave.handle=0;
		CaptureState |= CAPTURE_WAVE;
	}
	CaptureState ^= CAPTURE_WAVE;
}

/* MIDI capturing */

static uint8_t midi_header[]={
	'M','T','h','d',			/* uint32_t, Header Chunk */
	0x0,0x0,0x0,0x6,			/* uint32_t, Chunk Length */
	0x0,0x0,					/* uint16_t, Format, 0=single track */
	0x0,0x1,					/* uint16_t, Track Count, 1 track */
	0x01,0xf4,					/* uint16_t, Timing, 2 beats/second with 500 frames */
	'M','T','r','k',			/* uint32_t, Track Chunk */
	0x0,0x0,0x0,0x0,			/* uint32_t, Chunk Length */
	//Track data
};


static void raw_midi_add(uint8_t data) {
	capture.midi.buffer[capture.midi.used++]=data;
	if (capture.midi.used >= MIDI_BUF ) {
		capture.midi.done += capture.midi.used;
		fwrite(capture.midi.buffer,1,MIDI_BUF,capture.midi.handle);
		capture.midi.used = 0;
	}
}

static void raw_midi_add_number(uint32_t val) {
	if (val & 0xfe00000) raw_midi_add((uint8_t)(0x80|((val >> 21) & 0x7f)));
	if (val & 0xfffc000) raw_midi_add((uint8_t)(0x80|((val >> 14) & 0x7f)));
	if (val & 0xfffff80) raw_midi_add((uint8_t)(0x80|((val >> 7) & 0x7f)));
	raw_midi_add((uint8_t)(val & 0x7f));
}

void CAPTURE_AddMidi(bool sysex, Bitu len, uint8_t * data) {
	if (!capture.midi.handle) {
		capture.midi.handle=CAPTURE_CreateFile("Raw Midi",".mid");
		if (!capture.midi.handle) {
			return;
		}
		fwrite(midi_header,1,sizeof(midi_header),capture.midi.handle);
		capture.midi.last=PIC_Ticks;
	}
	uint32_t delta=PIC_Ticks-capture.midi.last;
	capture.midi.last=PIC_Ticks;
	raw_midi_add_number(delta);
	if (sysex) {
		raw_midi_add( 0xf0 );
		raw_midi_add_number(static_cast<uint32_t>(len));
	}
	for (Bitu i=0;i<len;i++)
		raw_midi_add(data[i]);
}

static void handle_midi_event(bool pressed) {
	if (!capture.midi.handle) {
		return;
	}
	if (!pressed)
		return;
	/* Check for previously opened wave file */
	if (capture.midi.handle) {
		LOG_MSG("Stopping raw midi saving and finalizing file.");
		//Delta time
		raw_midi_add(0x00);
		//End of track event
		raw_midi_add(0xff);
		raw_midi_add(0x2F);
		raw_midi_add(0x00);
		/* clear out the final data in the buffer if any */
		fwrite(capture.midi.buffer,1,capture.midi.used,capture.midi.handle);
		capture.midi.done+=capture.midi.used;
		if (fseek(capture.midi.handle, 18, SEEK_SET) != 0) {
			LOG_WARNING("CAPTURE: Failed seeking in MIDI capture file: %s",
			            strerror(errno));
			CaptureState ^= CAPTURE_MIDI;
			return;
		}
		uint8_t size[4];
		size[0]=(uint8_t)(capture.midi.done >> 24);
		size[1]=(uint8_t)(capture.midi.done >> 16);
		size[2]=(uint8_t)(capture.midi.done >> 8);
		size[3]=(uint8_t)(capture.midi.done >> 0);
		fwrite(&size,1,4,capture.midi.handle);
		fclose(capture.midi.handle);
		capture.midi.handle=0;
		CaptureState &= ~CAPTURE_MIDI;
		return;
	}
	CaptureState ^= CAPTURE_MIDI;
	if (CaptureState & CAPTURE_MIDI) {
		LOG_MSG("Preparing for raw midi capture, will start with first data.");
		capture.midi.used=0;
		capture.midi.done=0;
		capture.midi.handle=0;
	} else {
		LOG_MSG("Stopped capturing raw midi before any data arrived.");
	}
}

void CAPTURE_Destroy([[maybe_unused]] Section *sec)
{
	const auto pressed = true;
#if (C_SSHOT)
	handle_video_event(pressed);
#endif
	handle_wave_event(pressed);
	handle_midi_event(pressed);
}

void CAPTURE_Init(Section* sec)
{
	assert(sec);

	const Section_prop* conf = dynamic_cast<Section_prop*>(sec);

	Prop_path* proppath = conf->Get_path("captures");
	capturedir          = proppath->realpath;
	CaptureState        = 0;

	MAPPER_AddHandler(handle_wave_event,
	                  SDL_SCANCODE_F6,
	                  PRIMARY_MOD,
	                  "recwave",
	                  "Rec. Audio");

	MAPPER_AddHandler(handle_midi_event, SDL_SCANCODE_UNKNOWN, 0, "caprawmidi", "Rec. MIDI");
#if (C_SSHOT)
	MAPPER_AddHandler(handle_screenshot_event,
	                  SDL_SCANCODE_F5,
	                  PRIMARY_MOD,
	                  "scrshot",
	                  "Screenshot");

	MAPPER_AddHandler(handle_video_event,
	                  SDL_SCANCODE_F7,
	                  PRIMARY_MOD,
	                  "video",
	                  "Rec. Video");
#endif

	constexpr auto changeable_at_runtime = true;
	sec->AddDestroyFunction(&CAPTURE_Destroy, changeable_at_runtime);
}

