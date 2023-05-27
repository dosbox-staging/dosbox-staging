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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

#include "std_filesystem.h"

#include "capture_audio.h"
#include "capture_midi.h"
#include "capture_video.h"
#include "cross.h"
#include "fs_utils.h"
#include "image_capturer.h"
#include "mapper.h"
#include "render.h"
#include "sdlmain.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"

#include <SDL.h>
#if C_OPENGL
#include <SDL_opengl.h>
#endif

#if (C_SSHOT)
static ImageCapturer image_capturer = {};
#endif

static std_fs::path capture_path;

static bool capturing_audio = false;
static bool capturing_midi  = false;
static bool capturing_opl   = false;
static bool capturing_image = false;
static bool capturing_video = false;

struct capture_index_t {
	int32_t audio              = -1;
	int32_t midi               = -1;
	int32_t raw_opl_stream     = -1;
	int32_t rad_opl_instrument = -1;
	int32_t video              = -1;
	int32_t image              = -1;
	int32_t serial_log         = -1;
};

static capture_index_t next_capture_index = {};

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

static std::string capture_type_to_string(const CaptureType type)
{
	switch (type) {
	case CaptureType::Audio: return "audio output";
	case CaptureType::Midi: return "MIDI output";
	case CaptureType::RawOplStream: return "rawl OPL output";
	case CaptureType::RadOplInstruments: return "RAD capture";

	case CaptureType::Video: return "video output";

	case CaptureType::RawImage: return "raw image";
	case CaptureType::UpscaledImage: return "upscaled image";
	case CaptureType::RenderedImage: return "rendered image";

	case CaptureType::SerialLog: return "serial log";
	}
}

static std::string capture_type_to_basename(const CaptureType type)
{
	switch (type) {
	case CaptureType::Audio: return "audio";
	case CaptureType::Midi: return "midi";
	case CaptureType::RawOplStream: return "rawopl";
	case CaptureType::RadOplInstruments: return "oplinstr";

	case CaptureType::Video: return "video";

	case CaptureType::RawImage:
	case CaptureType::UpscaledImage:
	case CaptureType::RenderedImage: return "image";

	case CaptureType::SerialLog: return "serial";
	}
}

static std::string capture_type_to_extension(const CaptureType type)
{
	switch (type) {
	case CaptureType::Audio: return ".wav";
	case CaptureType::Midi: return ".mid";
	case CaptureType::RawOplStream: return ".dro";
	case CaptureType::RadOplInstruments: return ".rad";

	case CaptureType::Video: return ".avi";

	case CaptureType::RawImage:
	case CaptureType::UpscaledImage:
	case CaptureType::RenderedImage: return ".png";

	case CaptureType::SerialLog: return ".serlog";
	}
}

static int32_t get_next_capture_index(const CaptureType type)
{
	switch (type) {
	case CaptureType::Audio: return next_capture_index.audio;
	case CaptureType::Midi: return next_capture_index.midi;

	case CaptureType::RawOplStream:
		return next_capture_index.raw_opl_stream;

	case CaptureType::RadOplInstruments:
		return next_capture_index.rad_opl_instrument;

	case CaptureType::Video: return next_capture_index.video;

	case CaptureType::RawImage:
	case CaptureType::UpscaledImage:
	case CaptureType::RenderedImage: return next_capture_index.image;

	case CaptureType::SerialLog: return next_capture_index.serial_log;
	}
}

static void set_next_capture_index(const CaptureType type, int32_t index)
{
	switch (type) {
	case CaptureType::Audio: next_capture_index.audio = index; break;
	case CaptureType::Midi: next_capture_index.midi = index; break;

	case CaptureType::RawOplStream:
		next_capture_index.raw_opl_stream = index;
		break;

	case CaptureType::RadOplInstruments:
		next_capture_index.rad_opl_instrument = index;
		break;

	case CaptureType::Video: next_capture_index.video = index; break;

	case CaptureType::RawImage:
	case CaptureType::UpscaledImage:
	case CaptureType::RenderedImage:
		next_capture_index.image = index;
		break;

	case CaptureType::SerialLog:
		next_capture_index.serial_log = index;
		break;
	}
}

static bool maybe_create_capture_directory(const CaptureType type)
{
	std::error_code ec = {};

	if (!std_fs::exists(capture_path, ec)) {
		if (!std_fs::create_directory(capture_path, ec)) {
			LOG_WARNING("CAPTURE: Can't create directory '%s' for capturing %s: %s",
			            capture_path.c_str(),
			            capture_type_to_string(type).c_str(),
			            ec.message().c_str());
			return false;
		}
		std_fs::permissions(capture_path, std_fs::perms::owner_all, ec);

		next_capture_index = {};
	}
	return true;
}

static std::optional<int32_t> find_highest_capture_index(const CaptureType type)
{
	// Find existing capture file with the highest index
	std::string filename_start = capture_type_to_basename(type);
	lowcase(filename_start);

	const auto capture_type = capture_type_to_string(type);
	const auto ext          = capture_type_to_extension(type);
	int32_t highest_index   = 0;
	std::error_code ec      = {};

	for (const auto& entry : std_fs::directory_iterator(capture_path, ec)) {
		if (ec) {
			LOG_WARNING("CAPTURE: Cannot open directory '%s' for capturing %s: %s",
			            capture_path.c_str(),
			            capture_type.c_str(),
			            ec.message().c_str());
			return {};
		}
		if (!entry.is_regular_file(ec)) {
			continue;
		}
		if (entry.path().extension() != ext) {
			continue;
		}
		auto stem = entry.path().stem().string();
		lowcase(stem);
		if (starts_with(stem, filename_start)) {
			const auto index = to_int(strip_prefix(stem, filename_start));
			highest_index = std::max(highest_index, *index);
		}
	}
	return highest_index;
}

static std::optional<int32_t> generate_capture_index(const CaptureType type)
{
	if (!maybe_create_capture_directory(type)) {
		return {};
	}
	auto index = get_next_capture_index(type);
	if (index < 0) {
		// This should happen only once per capture type at startup
		const auto highest_index = find_highest_capture_index(type);
		if (!highest_index) {
			return {};
		}
		index = *highest_index;
	}
	++index;
	set_next_capture_index(type, index);
	return index;
}

static std_fs::path generate_capture_filename(const CaptureType type,
                                              const int32_t index)
{
	const auto filename = format_string("%s%04d%s",
	                                    capture_type_to_basename(type).c_str(),
	                                    index,
	                                    capture_type_to_extension(type).c_str());
	return {capture_path / filename};
}

// TODO should be internal to the src/capture module
FILE* CAPTURE_CreateFile(const CaptureType type)
{
	const auto index = generate_capture_index(type);
	if (!index) {
		return nullptr;
	}
	const auto path     = generate_capture_filename(type, *index);
	const auto path_str = path.string();

	FILE* handle = fopen(path_str.c_str(), "wb");
	if (handle) {
		LOG_MSG("CAPTURE: Capturing %s to '%s'",
		        capture_type_to_string(type).c_str(),
		        path_str.c_str());
	} else {
		LOG_WARNING("CAPTURE: Failed to create file '%s' for capturing %s",
		            path_str.c_str(),
		            capture_type_to_string(type).c_str());
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
		image_capturer.CaptureImage(image, CapturedImageType::Upscaled);
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

#if (C_SSHOT)
	const auto image = GFX_GetRenderedOutput();
	if (!image) {
		return;
	}
	image_capturer.CaptureImage(*image, CapturedImageType::Rendered);
#endif
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

	capture_path = std_fs::path(proppath->realpath);
	if (capture_path.empty()) {
		LOG_MSG("CAPTURE: Capture path not specified; defaulting to 'capture'");
		capture_path = "capture";
	}

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

#if (C_SSHOT)
	MAPPER_AddHandler(handle_capture_rendered_screenshot_event,
	                  SDL_SCANCODE_F5,
	                  MMOD2,
	                  "rendshot",
	                  "Rend Screenshot");

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

