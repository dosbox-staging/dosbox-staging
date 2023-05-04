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
#include "render.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"

#include "capture_audio.h"
#include "capture_image.h"
#include "capture_midi.h"
#include "capture_video.h"

static std::string capturedir;
extern const char* RunningProgram;
Bitu CaptureState;

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


void CAPTURE_AddWave(uint32_t freq, uint32_t len, int16_t * data) {
#if (C_SSHOT)
	if (CaptureState & CAPTURE_VIDEO) {
		capture_video_add_wave(freq, len, data);
	}
#endif
	if (CaptureState & CAPTURE_WAVE) {
		capture_audio_add_wave(freq, len, data);
	}
}

void CAPTURE_AddMidi(bool sysex, Bitu len, uint8_t * data) {
	capture_add_midi(sysex, len, data);
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

