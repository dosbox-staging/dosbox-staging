/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

#include "capture.h"

#include <cassert>
#include <cerrno>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cross.h"
#include "fs_utils.h"
#include "mapper.h"
#include "render.h"
#include "sdlmain.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"

#include "capture_audio.h"
#include "capture_midi.h"
#include "capture_video.h"
#include "image_capturer.h"

#include <SDL.h>
#if C_OPENGL
#include <SDL_opengl.h>
#endif
#if C_SDL_IMAGE
#include <SDL_image.h>
#endif


extern const char* RunningProgram;

#if (C_SSHOT)
static ImageCapturer image_capturer = {};
#endif

static std::string capture_dir;

static bool capturing_audio = false;
static bool capturing_image = false;
static bool capturing_midi  = false;
static bool capturing_opl   = false;
static bool capturing_video = false;

bool CAPTURE_IsCapturingAudio()
{
	return capturing_audio;
}

bool CAPTURE_IsCapturingImage()
{
	return capturing_image;
}

bool CAPTURE_IsCapturingMidi()
{
	return capturing_midi;
}

bool CAPTURE_IsCapturingOpl()
{
	return capturing_opl;
}

bool CAPTURE_IsCapturingVideo()
{
	return capturing_video;
}

std::string capture_generate_filename(const char* type, const char* ext)
{
	if (capture_dir.empty()) {
		LOG_WARNING("CAPTURE: Please specify a capture directory");
		return 0;
	}

	char file_start[16];
	dir_information* dir;
	/* Find a filename to open */
	dir = open_directory(capture_dir.c_str());
	if (!dir) {
		// Try creating it first
		if (create_dir(capture_dir, 0700, OK_IF_EXISTS) != 0) {
			LOG_WARNING("CAPTURE: Can't create directory '%s' for capturing %s, reason: %s",
			            capture_dir.c_str(),
			            type,
			            safe_strerror(errno).c_str());
			return 0;
		}
		dir = open_directory(capture_dir.c_str());
		if (!dir) {
			LOG_WARNING("CAPTURE: Can't open directory '%s' for capturing %s",
			            capture_dir.c_str(),
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
	         capture_dir.c_str(),
	         CROSS_FILESPLIT,
	         file_start,
	         last,
	         ext);

	return file_name;
}

// TODO should be internal
FILE* CAPTURE_CreateFile(const char* type, const char* ext)
{
	const auto file_name = capture_generate_filename(type, ext);

	FILE* handle = fopen(file_name.c_str(), "wb");
	if (handle) {
		LOG_MSG("CAPTURE: Capturing %s to '%s'", type, file_name.c_str());
	} else {
		LOG_WARNING("CAPTURE: Failed to create file '%s' for capturing %s",
		            file_name.c_str(),
		            type);
	}
	return handle;
}

#if (!C_SSHOT)
constexpr auto NoAviSupportMessage =
        "CAPTURE: Can't capture video output: AVI support has not been compiled in";
#endif

void CAPTURE_StartVideoCapture()
{
#if (C_SSHOT)
	if (capturing_video) {
		LOG_WARNING("CAPTURE: Already capturing video output");
	} else {
		// Capturing the videooutput will start in the next few
		// milliseconds when CAPTURE_AddImage is called
		capturing_video = true;
	}
#else
	LOG_WARNING(NoAviSupportMessage);
#endif
}

void CAPTURE_StopVideoCapture()
{
#if (C_SSHOT)
	if (capturing_video) {
		capture_video_finalise();
		capturing_video = false;
		LOG_MSG("CAPTURE: Stopped capturing video output");
	} else {
		LOG_WARNING("CAPTURE: Not capturing video output");
	}
#else
	LOG_WARNING(NoAviSupportMessage);
#endif
}

void CAPTURE_AddFrame([[maybe_unused]] const RenderedImage& image,
                      [[maybe_unused]] const float frames_per_second)
{
#if (C_SSHOT)
	if (capturing_image) {
		image_capturer.CaptureImage(image);
		capturing_image = false;
	}

	if (capturing_video) {
		capture_video_add_frame(image, frames_per_second);
	}
#endif
}

void CAPTURE_AddAudioData(const uint32_t sample_rate, const uint32_t num_sample_frames,
                          const int16_t* sample_frames)
{
#if (C_SSHOT)
	if (capturing_video) {
		capture_video_add_audio_data(sample_rate,
		                             num_sample_frames,
		                             sample_frames);
	}
#endif
	if (capturing_audio) {
		capture_audio_add_data(sample_rate, num_sample_frames, sample_frames);
	}
}

void CAPTURE_AddMidiData(const bool sysex, const size_t len, const uint8_t* data)
{
	capture_midi_add_data(sysex, len, data);
}

static void handle_capture_audio_event(bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}
	if (capturing_audio) {
		capture_audio_finalise();
		capturing_audio = false;
		LOG_MSG("CAPTURE: Stopped capturing audio output");
	} else {
		// Capturing the audio output will start in the next few
		// milliseconds when CAPTURE_AddAudioData is called
		capturing_audio = true;
	}
}

static void handle_capture_midi_event(bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}
	if (capturing_midi) {
		capture_midi_finalise();
		capturing_midi = false;
		LOG_MSG("CAPTURE: Stopped capturing MIDI output");
	} else {
		capturing_midi = true;
		// We need to log this because the actual sending of MIDI data
		// might happen much later
		LOG_MSG("CAPTURE: Preparing to capture MIDI output; "
		        "capturing will start on the first MIDI message");
	}
}

static void handle_capture_rendered_screenshot_event(const bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}

	const auto surface = GFX_GetRenderedSurface();
	if (!surface) {
		return;
	}

#if C_SDL_IMAGE
	const auto filename = capture_generate_filename("Screenshot", ".png");
	const auto is_saved = IMG_SavePNG(*surface, filename.c_str()) == 0;
#else
	const auto filename = capture_generate_filename("Screenshot", ".bmp");
	const auto is_saved = SDL_SaveBMP(*surface, filename.c_str()) == 0;
#endif
	SDL_FreeSurface(*surface);

	if (is_saved) {
		LOG_MSG("CAPTURE: Capturing rendered image output to '%s'",
		        filename.c_str());
	} else {
		LOG_MSG("CAPTURE: Failed capturing rendered image to '%s', reason: %s",
		        filename.c_str(),
		        SDL_GetError());
	}
}

#if (C_SSHOT)
static void handle_capture_raw_screenshot_event(const bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}
	capturing_image = true;
}
#endif

void handle_capture_video_event(bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}

	if (capturing_video) {
		CAPTURE_StopVideoCapture();
	} else {
		CAPTURE_StartVideoCapture();
	}
}

void capture_destroy([[maybe_unused]] Section* sec)
{
	if (capturing_audio) {
		capture_audio_finalise();
	}
	if (capturing_midi) {
		capture_midi_finalise();
	}
#if (C_SSHOT)
	image_capturer.Close();

	if (capturing_video) {
		capture_video_finalise();
	}
#endif
}

// TODO move raw OPL capture and serial log capture here too

void CAPTURE_Init(Section* sec)
{
	assert(sec);

	const Section_prop* conf = dynamic_cast<Section_prop*>(sec);
	assert(conf);

	Prop_path* proppath = conf->Get_path("captures");
	assert(proppath);

	capture_dir = proppath->realpath;

	capturing_audio = false;
	capturing_image = false;
	capturing_midi  = false;
	capturing_opl   = false;
	capturing_video = false;

#if (C_SSHOT)
	image_capturer.Open();
#endif

	MAPPER_AddHandler(handle_capture_audio_event,
	                  SDL_SCANCODE_F6,
	                  PRIMARY_MOD,
	                  "recwave",
	                  "Rec. Audio");

	MAPPER_AddHandler(handle_capture_midi_event,
	                  SDL_SCANCODE_UNKNOWN,
	                  0,
	                  "caprawmidi",
	                  "Rec. MIDI");

	MAPPER_AddHandler(handle_capture_rendered_screenshot_event,
	                  SDL_SCANCODE_F5,
	                  MMOD2,
	                  "rendshot",
	                  "Rend Screenshot");
#if (C_SSHOT)
	MAPPER_AddHandler(handle_capture_raw_screenshot_event,
	                  SDL_SCANCODE_F5,
	                  PRIMARY_MOD,
	                  "scrshot",
	                  "Screenshot");

	MAPPER_AddHandler(handle_capture_video_event,
	                  SDL_SCANCODE_F7,
	                  PRIMARY_MOD,
	                  "video",
	                  "Rec. Video");
#endif

	constexpr auto changeable_at_runtime = true;
	sec->AddDestroyFunction(&capture_destroy, changeable_at_runtime);
}

