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
uint8_t CaptureState;

std::string capture_generate_filename(const char* type, const char* ext)
{
	if (capturedir.empty()) {
		LOG_WARNING("CAPTURE: Please specify a capture directory");
		return 0;
	}

	char file_start[16];
	dir_information* dir;
	/* Find a filename to open */
	dir = open_directory(capturedir.c_str());
	if (!dir) {
		// Try creating it first
		if (create_dir(capturedir, 0700, OK_IF_EXISTS) != 0) {
			LOG_WARNING("CAPTURE: Can't create directory '%s' for capturing %s, reason: %s",
			            capturedir.c_str(),
			            type,
			            safe_strerror(errno).c_str());
			return 0;
		}
		dir = open_directory(capturedir.c_str());
		if (!dir) {
			LOG_WARNING("CAPTURE: Can't open directory '%s' for capturing %s",
			            capturedir.c_str(),
			            type);
			return 0;
		}
	}
	safe_strcpy(file_start, RunningProgram);
	lowcase(file_start);
	strcat(file_start, "_");
	bool is_directory;
	char tempname[CROSS_LEN];
	bool testRead = read_directory_first(dir, tempname, is_directory);
	int last      = 0;
	for (; testRead;
	     testRead = read_directory_next(dir, tempname, is_directory)) {
		char* test = strstr(tempname, ext);
		if (!test || strlen(test) != strlen(ext))
			continue;
		*test = 0;
		if (strncasecmp(tempname, file_start, strlen(file_start)) != 0)
			continue;
		const int num = atoi(&tempname[strlen(file_start)]);
		if (num >= last)
			last = num + 1;
	}
	close_directory(dir);

	char file_name[CROSS_LEN];
	snprintf(file_name,
	         CROSS_LEN,
	         "%s%c%s%03d%s",
	         capturedir.c_str(),
	         CROSS_FILESPLIT,
	         file_start,
	         last,
	         ext);

	return file_name;
}

FILE* CAPTURE_CreateFile(const char* type, const char* ext)
{
	const auto file_name = capture_generate_filename(type, ext);

	FILE* handle = fopen(file_name.c_str(), "wb");
	if (handle) {
		LOG_MSG("CAPTURE: Capturing %s to '%s'", type, file_name.c_str());
	} else {
		LOG_WARNING("CAPTURE: Failed to open '%s' for capturing %s",
		            file_name.c_str(),
		            type);
	}
	return handle;
}

#if (!C_SSHOT)
constexpr auto NoAviSupportMessage =
        "CAPTURE: Can't capture video output: AVI support has not been compiled in";
#endif

void CAPTURE_VideoStart()
{
#if (C_SSHOT)
	if (CaptureState & CAPTURE_VIDEO) {
		LOG_WARNING("CAPTURE: Already capturing video output");
	} else {
		const auto pressed = true;
		handle_video_event(pressed);
	}
#else
	LOG_WARNING(NoAviSupportMessage);
#endif
}

void CAPTURE_VideoStop()
{
#if (C_SSHOT)
	if (CaptureState & CAPTURE_VIDEO) {
		const auto pressed = true;
		handle_video_event(pressed);
	} else {
		LOG_WARNING("CAPTURE: Not capturing video output");
	}
#else
	LOG_WARNING(NoAviSupportMessage);
#endif
}

void CAPTURE_AddImage([[maybe_unused]] const uint16_t width,
                      [[maybe_unused]] const uint16_t height,
                      [[maybe_unused]] const uint8_t bits_per_pixel,
                      [[maybe_unused]] const uint16_t pitch,
                      [[maybe_unused]] const uint8_t capture_flags,
                      [[maybe_unused]] const float frames_per_second,
                      [[maybe_unused]] const uint8_t* image_data,
                      [[maybe_unused]] const uint8_t* palette_data)
{
#if (C_SSHOT)
	auto image_height = height;
	if (capture_flags & CAPTURE_FLAG_DBLH) {
		image_height *= 2;
	}
	auto image_width = width;
	if (capture_flags & CAPTURE_FLAG_DBLW) {
		image_width *= 2;
	}
	if (image_height > SCALER_MAXHEIGHT) {
		return;
	}
	if (image_width > SCALER_MAXWIDTH) {
		return;
	}

	if (CaptureState & CAPTURE_IMAGE) {
		capture_image(image_width,
		              image_height,
		              bits_per_pixel,
		              pitch,
		              capture_flags,
		              image_data,
		              palette_data);
		CaptureState &= ~CAPTURE_IMAGE;
	}
	if (CaptureState & CAPTURE_VIDEO) {
		capture_video(image_width,
		              image_height,
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
static void handle_screenshot_event(const bool pressed)
{
	if (!pressed) {
		return;
	}
	CaptureState |= CAPTURE_IMAGE;
}
#endif

void CAPTURE_AddWave(const uint32_t freq, const uint32_t len, const int16_t* data)
{
#if (C_SSHOT)
	if (CaptureState & CAPTURE_VIDEO) {
		capture_video_add_wave(freq, len, data);
	}
#endif
	if (CaptureState & CAPTURE_WAVE) {
		capture_audio_add_wave(freq, len, data);
	}
}

void CAPTURE_AddMidi(const bool sysex, const size_t len, const uint8_t* data)
{
	capture_add_midi(sysex, len, data);
}

void CAPTURE_Destroy([[maybe_unused]] Section* sec)
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

	MAPPER_AddHandler(handle_screenshot_rendered_surface,
	                  SDL_SCANCODE_F5,
	                  MMOD2,
	                  "rendshot",
	                  "Rend Screenshot");
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

