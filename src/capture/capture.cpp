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

#include "capture_audio.h"
#include "capture_midi.h"
#include "capture_video.h"
#include "checks.h"
#include "control.h"
#include "fs_utils.h"
#include "image/image_capturer.h"
#include "mapper.h"
#include "sdlmain.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"

#include <SDL.h>

CHECK_NARROWING();

static struct {
	std_fs::path path = {};

	struct {
		CaptureState audio = {};
		CaptureState midi  = {};
		CaptureState video = {};
	} state = {};

	struct {
		int32_t audio              = 1;
		int32_t midi               = 1;
		int32_t raw_opl_stream     = 1;
		int32_t rad_opl_instrument = 1;
		int32_t video              = 1;
		int32_t image              = 1;
		int32_t serial_log         = 1;
	} next_index = {};
} capture = {};

static std::unique_ptr<ImageCapturer> image_capturer = {};

bool CAPTURE_IsCapturingAudio()
{
	return capture.state.audio != CaptureState::Off;
}

bool CAPTURE_IsCapturingImage()
{
	if (image_capturer) {
		return image_capturer->IsCaptureRequested();
	}
	return false;
}

bool CAPTURE_IsCapturingPostRenderImage()
{
	if (image_capturer) {
		return image_capturer->IsRenderedCaptureRequested();
	}
	return false;
}

bool CAPTURE_IsCapturingMidi()
{
	return capture.state.midi != CaptureState::Off;
}

bool CAPTURE_IsCapturingVideo()
{
	return capture.state.video != CaptureState::Off;
}

static const char* capture_type_to_string(const CaptureType type)
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

	default: assertm(false, "Unknown CaptureType"); return "";
	}
}

static const char* capture_type_to_basename(const CaptureType type)
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

	default: assertm(false, "Unknown CaptureType"); return "";
	}
}

static const char* capture_type_to_extension(const CaptureType type)
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

	default: assertm(false, "Unknown CaptureType"); return "";
	}
}

static const char* capture_type_to_postfix(const CaptureType type)
{
	switch (type) {
	case CaptureType::RawImage: return "-raw";
	case CaptureType::RenderedImage: return "-rendered";
	default: return "";
	}
}

int32_t get_next_capture_index(const CaptureType type)
{
	switch (type) {
	case CaptureType::Audio: return capture.next_index.audio++;
	case CaptureType::Midi: return capture.next_index.midi++;

	case CaptureType::RawOplStream:
		return capture.next_index.raw_opl_stream++;

	case CaptureType::RadOplInstruments:
		return capture.next_index.rad_opl_instrument++;

	case CaptureType::Video: return capture.next_index.video++;

	case CaptureType::RawImage:
	case CaptureType::UpscaledImage:
	case CaptureType::RenderedImage: return capture.next_index.image++;

	case CaptureType::SerialLog: return capture.next_index.serial_log++;

	default: assertm(false, "Unknown CaptureType"); return 0;
	}
}

std_fs::path generate_capture_filename(const CaptureType type, const int32_t index)
{
	const auto filename = format_string("%s%04d%s%s",
	                                    capture_type_to_basename(type),
	                                    index,
	                                    capture_type_to_postfix(type),
	                                    capture_type_to_extension(type));
	return {capture.path / filename};
}

FILE* CAPTURE_CreateFile(const CaptureType type, std::optional<std_fs::path> path)
{
	std::string path_str = {};
	if (path) {
		path_str = path->string();
	} else {
		const auto index = get_next_capture_index(type);
		path_str = generate_capture_filename(type, index).string();
	}

	FILE* handle = fopen(path_str.c_str(), "wb");
	if (handle) {
		LOG_MSG("CAPTURE: Capturing %s to '%s'",
		        capture_type_to_string(type),
		        path_str.c_str());
	} else {
		LOG_WARNING("CAPTURE: Failed to create file '%s' for capturing %s",
		            path_str.c_str(),
		            capture_type_to_string(type));
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
	switch (capture.state.video) {
	case CaptureState::Off:
		capture.state.video = CaptureState::Pending;
		break;
	case CaptureState::Pending:
	case CaptureState::InProgress:
		LOG_WARNING("CAPTURE: Already capturing video output");
		break;
	}
#else
	LOG_WARNING(NoAviSupportMessage);
#endif
}

void CAPTURE_StopVideoCapture()
{
#if (C_SSHOT)
	switch (capture.state.video) {
	case CaptureState::Off:
		LOG_WARNING("CAPTURE: Not capturing video output");
		break;
	case CaptureState::Pending:
		// It's very hard to hit this branch; handling it for
		// completeness only
		LOG_MSG("CAPTURE: Cancelling pending video output capture");
		capture.state.video = CaptureState::Off;
		break;
	case CaptureState::InProgress:
		capture_video_finalise();
		capture.state.video = CaptureState::Off;
		LOG_MSG("CAPTURE: Stopped capturing video output");
	}
#else
	LOG_WARNING(NoAviSupportMessage);
#endif
}

void CAPTURE_AddFrame([[maybe_unused]] const RenderedImage& image,
                      [[maybe_unused]] const float frames_per_second)
{
#if (C_SSHOT)
	if (image_capturer) {
		image_capturer->MaybeCaptureImage(image);
	}

	switch (capture.state.video) {
	case CaptureState::Off: break;
	case CaptureState::Pending:
		capture.state.video = CaptureState::InProgress;
		[[fallthrough]];
	case CaptureState::InProgress:
		capture_video_add_frame(image, frames_per_second);
		break;
	}
#endif
}

void CAPTURE_AddPostRenderImage([[maybe_unused]] const RenderedImage& image)
{
	if (image_capturer) {
		image_capturer->CapturePostRenderImage(image);
	}
}

void CAPTURE_AddAudioData(const uint32_t sample_rate, const uint32_t num_sample_frames,
                          const int16_t* sample_frames)
{
#if (C_SSHOT)
	switch (capture.state.video) {
	case CaptureState::Off: break;
	case CaptureState::Pending:
		capture.state.video = CaptureState::InProgress;
		[[fallthrough]];
	case CaptureState::InProgress:
		capture_video_add_audio_data(sample_rate,
		                             num_sample_frames,
		                             sample_frames);
		break;
	}

#endif
	switch (capture.state.audio) {
	case CaptureState::Off: break;
	case CaptureState::Pending:
		capture.state.audio = CaptureState::InProgress;
		[[fallthrough]];
	case CaptureState::InProgress:
		capture_audio_add_data(sample_rate, num_sample_frames, sample_frames);
		break;
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

	switch (capture.state.audio) {
	case CaptureState::Off:
		// Capturing the audio output will start in the next few
		// milliseconds when CAPTURE_AddAudioData is called
		capture.state.audio = CaptureState::Pending;
		break;
	case CaptureState::Pending:
		// It's practically impossible to hit this branch; handling it
		// for completeness only
		capture.state.audio = CaptureState::Off;
		LOG_MSG("CAPTURE: Cancelled pending audio output capture");
		break;
	case CaptureState::InProgress:
		capture_audio_finalise();
		capture.state.audio = CaptureState::Off;
		LOG_MSG("CAPTURE: Stopped capturing audio output");
		break;
	}
}

static void handle_capture_midi_event(bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}

	switch (capture.state.midi) {
	case CaptureState::Off:
		capture.state.midi = CaptureState::Pending;

		// We need to log this because the actual sending of MIDI data
		// might happen much later
		LOG_MSG("CAPTURE: Preparing to capture MIDI output; "
		        "capturing will start on the first MIDI message");
		break;
	case CaptureState::Pending:
		capture.state.midi = CaptureState::Off;
		LOG_MSG("CAPTURE: Stopped capturing MIDI output before any "
		        "MIDI message was output (no MIDI file has been created)");
		break;
	case CaptureState::InProgress:
		capture_midi_finalise();
		capture.state.midi = CaptureState::Off;
		LOG_MSG("CAPTURE: Stopped capturing MIDI output");
		break;
	}
}

#if (C_SSHOT)
static void handle_capture_grouped_screenshot_event(const bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}
	if (image_capturer) {
		image_capturer->RequestGroupedCapture();
	}
}

static void handle_capture_single_raw_screenshot_event(const bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}
	if (image_capturer) {
		image_capturer->RequestRawCapture();
	}
}

static void handle_capture_single_upscaled_screenshot_event(const bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}
	if (image_capturer) {
		image_capturer->RequestUpscaledCapture();
	}
}

static void handle_capture_single_rendered_screenshot_event(const bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}
	if (image_capturer) {
		image_capturer->RequestRenderedCapture();
	}
}

static void handle_capture_video_event(bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}
	if (capture.state.video != CaptureState::Off) {
		CAPTURE_StopVideoCapture();
	} else if (capture.state.video == CaptureState::Off) {
		CAPTURE_StartVideoCapture();
	}
}
#endif

static void capture_destroy([[maybe_unused]] Section* sec)
{
	if (capture.state.audio == CaptureState::InProgress) {
		capture_audio_finalise();
		capture.state.audio = CaptureState::Off;
	}
	if (capture.state.midi == CaptureState::InProgress) {
		capture_midi_finalise();
		capture.state.midi = CaptureState::Off;
	}
#if (C_SSHOT)
	// When destructed, the threaded image capturer instances do a blocking
	// wait until all pending capture tasks are processed.
	image_capturer = {};

	if (capture.state.video == CaptureState::InProgress) {
		capture_video_finalise();
		capture.state.video = CaptureState::Off;
	}
#endif
}

static std::optional<int32_t> find_highest_capture_index(const CaptureType type)
{
	// Find existing capture file with the highest index
	std::string filename_start = capture_type_to_basename(type);
	lowcase(filename_start);

	const auto ext        = capture_type_to_extension(type);
	int32_t highest_index = 0;
	std::error_code ec    = {};

	for (const auto& entry : std_fs::directory_iterator(capture.path, ec)) {
		if (ec) {
			LOG_WARNING("CAPTURE: Cannot open directory '%s': %s",
			            capture.path.c_str(),
			            ec.message().c_str());
			return {};
		}
		if (!entry.is_regular_file(ec) || entry.path().extension() != ext) {
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

static void set_next_capture_index(const CaptureType type, int32_t index)
{
	switch (type) {
	case CaptureType::Audio: capture.next_index.audio = index; break;
	case CaptureType::Midi: capture.next_index.midi = index; break;

	case CaptureType::RawOplStream:
		capture.next_index.raw_opl_stream = index;
		break;

	case CaptureType::RadOplInstruments:
		capture.next_index.rad_opl_instrument = index;
		break;

	case CaptureType::Video: capture.next_index.video = index; break;

	case CaptureType::RawImage:
	case CaptureType::UpscaledImage:
	case CaptureType::RenderedImage:
		capture.next_index.image = index;
		break;

	case CaptureType::SerialLog:
		capture.next_index.serial_log = index;
		break;

	default: assertm(false, "Unknown CaptureType");
	}
}

static bool create_capture_directory()
{
	std::error_code ec = {};

	if (!std_fs::exists(capture.path, ec)) {
		if (!std_fs::create_directory(capture.path, ec)) {
			LOG_WARNING("CAPTURE: Can't create directory '%s': %s",
			            capture.path.c_str(),
			            ec.message().c_str());
			return false;
		}
		std_fs::permissions(capture.path, std_fs::perms::owner_all, ec);
	}
	return true;
}

static void capture_init(Section* sec)
{
	assert(sec);
	const Section_prop* secprop = dynamic_cast<Section_prop*>(sec);
	assert(secprop);

	Prop_path* capture_path = secprop->Get_path("capture_dir");
	assert(capture_path);

	// We can safely change the capture output path even if capturing of any
	// type is in progress.
	capture.path = std_fs::path(capture_path->realpath);
	if (capture.path.empty()) {
		LOG_WARNING("CAPTURE: No value specified for `capture_dir`; defaulting to 'capture' "
		            "in the current working directory");
		capture.path = "capture";
	}

#if (C_SSHOT)
	const std::string prefs = secprop->Get_string("default_image_capture_formats");

	image_capturer = std::make_unique<ImageCapturer>(prefs);
#endif

	constexpr auto changeable_at_runtime = true;
	sec->AddDestroyFunction(&capture_destroy, changeable_at_runtime);

	if (!create_capture_directory()) {
		return;
	}

	constexpr CaptureType all_capture_types[] = {CaptureType::Audio,
	                                             CaptureType::Midi,
	                                             CaptureType::RawOplStream,
	                                             CaptureType::RadOplInstruments,
	                                             CaptureType::Video,
	                                             CaptureType::RawImage,
	                                             CaptureType::UpscaledImage,
	                                             CaptureType::RenderedImage,
	                                             CaptureType::SerialLog};

	for (auto type : all_capture_types) {
		const auto index = find_highest_capture_index(type);
		if (index) {
			set_next_capture_index(type, *index + 1);
		} else {
			return;
		}
	}
}

static void init_key_mappings()
{
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
	MAPPER_AddHandler(handle_capture_grouped_screenshot_event,
	                  SDL_SCANCODE_F5,
	                  PRIMARY_MOD,
	                  "scrshot",
	                  "Screenshot");

	MAPPER_AddHandler(handle_capture_single_raw_screenshot_event,
	                  SDL_SCANCODE_UNKNOWN,
	                  PRIMARY_MOD,
	                  "rawshot",
	                  "Raw Screenshot");

	MAPPER_AddHandler(handle_capture_single_upscaled_screenshot_event,
	                  SDL_SCANCODE_UNKNOWN,
	                  PRIMARY_MOD,
	                  "upscshot",
	                  "Upsc Screenshot");

	MAPPER_AddHandler(handle_capture_single_rendered_screenshot_event,
	                  SDL_SCANCODE_F5,
	                  MMOD2,
	                  "rendshot",
	                  "Rend Screenshot");

	MAPPER_AddHandler(handle_capture_video_event,
	                  SDL_SCANCODE_F7,
	                  PRIMARY_MOD,
	                  "video",
	                  "Rec. Video");
#endif
}

static void init_capture_dosbox_settings(Section_prop& secprop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto* path_prop = secprop.Add_path("capture_dir", when_idle, "capture");
	path_prop->Set_help(
	        "Directory where the various captures are saved, such as audio, video, MIDI\n"
	        "and screenshot captures. ('capture' in the current working directory by\n"
	        "default).");
	assert(path_prop);

	auto* str_prop = secprop.Add_string("default_image_capture_formats",
	                                    when_idle,
	                                    "upscaled");
	str_prop->Set_help(
	        "Set the capture format of the default screenshot action ('upscaled' by\n"
	        "default):\n"
	        "  raw:       The content of the raw framebuffer is captured\n"
	        "             (legacy behaviour; this always results in square pixels).\n"
	        "             The filenames of raw screenshots end with '_raw'\n"
	        "             (e.g. 'image0001_raw.png').\n"
	        "  upscaled:  The image is bilinear-sharp upscaled so the height is around\n"
	        "             1200 pixels and the correct aspect ratio is maintained\n"
	        "             (depending on the 'aspect' setting). The vertical scaling factor\n"
	        "             is always an integer. For example, 320x200 content is upscaled\n"
	        "             to 1600x1200 (5:6 integer scaling), 640x480 to 1920x1440\n"
	        "             (3:3 integer scaling), and 640x350 to 1867x1400 (2.9165:4\n"
	        "             scaling, integer vertically and fractional horizontally).\n"
	        "  rendered:  The post-rendered, post-shader image shown on the screen is\n"
	        "             captured. The filenames of rendered screenshots end with\n"
	        "             '_rendered' (e.g. 'image0001_rendered.png').\n"
	        "If multiple formats are specified separated by spaces, a single default\n"
	        "screenshot action will result in multiple image files being saved in all\n"
	        "specified formats. In addition to the default screenshot action, keybindings\n"
	        "for taking a single capture of a specific format are also available.");
	assert(str_prop);
}

void CAPTURE_AddConfigSection(const config_ptr_t& conf)
{
	assert(conf);

	constexpr auto changeable_at_runtime = true;

	Section_prop* sec = conf->AddSection_prop("capture",
	                                          &capture_init,
	                                          changeable_at_runtime);
	assert(sec);
	init_capture_dosbox_settings(*sec);
	init_key_mappings();
}

