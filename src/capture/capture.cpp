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
#include <mutex>
#include <optional>
#include <string>

#include "std_filesystem.h"

#include "capture_audio.h"
#include "capture_midi.h"
#include "capture_video.h"
#include "checks.h"
#include "control.h"
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

CHECK_NARROWING();

#if (C_SSHOT)
class ImageCapturers {
public:
	ImageCapturers();
	~ImageCapturers();
	ImageCapturer& GetNext();

private:
	static constexpr int NumImageCapturers           = 3;
	int current_capturer_index                       = 0;
	ImageCapturer image_capturers[NumImageCapturers] = {};
};

ImageCapturers::ImageCapturers()
{
	for (auto& image_capturer : image_capturers) {
		image_capturer.Open();
	}

	LOG_MSG("CAPTURE: Image capturer started");
}

ImageCapturers::~ImageCapturers()
{
	for (auto& image_capturer : image_capturers) {
		image_capturer.Close();
	}

	LOG_MSG("CAPTURE: Image capturer shutting down");
}

ImageCapturer& ImageCapturers::GetNext()
{
	++current_capturer_index;
	current_capturer_index %= NumImageCapturers;
	return image_capturers[current_capturer_index];
}

static std::unique_ptr<ImageCapturers> image_capturers = {};
#endif

enum class CaptureState { Off, Pending, InProgress };

static struct {
	std_fs::path path = {};

	struct {
		struct {
			CaptureState raw      = {};
			CaptureState upscaled = {};
			CaptureState rendered = {};
			CaptureState grouped  = {};

			std_fs::path rendered_path    = {};
			ImageInfo rendered_image_info = {};
		} image = {};

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

static struct {
	bool capture_raw      = false;
	bool capture_upscaled = true;
	bool capture_rendered = false;
} grouped_image_capture_formats = {};

bool CAPTURE_IsCapturingAudio()
{
	return capture.state.audio != CaptureState::Off;
}

bool CAPTURE_IsCapturingImage()
{
	return capture.state.image.raw != CaptureState::Off ||
	       capture.state.image.upscaled != CaptureState::Off ||
	       capture.state.image.rendered != CaptureState::Off ||
	       capture.state.image.grouped != CaptureState::Off;
}

bool CAPTURE_IsCapturingPostRenderImage()
{
	return capture.state.image.rendered != CaptureState::Off ||
	       (capture.state.image.grouped != CaptureState::Off &&
	        grouped_image_capture_formats.capture_rendered);
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

static int32_t get_next_capture_index(const CaptureType type)
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

static std_fs::path generate_capture_filename(const CaptureType type,
                                              const int32_t index)
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

#if (C_SSHOT)
static void handle_add_frame_image_capture(const RenderedImage& image)
{
	// No new image capture requests until we finish queuing the current
	// grouped capture request, otherwise we can get into all sorts of race
	// conditions.
	if (capture.state.image.grouped == CaptureState::InProgress) {
		return;
	}

	bool capture_raw      = false;
	bool capture_upscaled = false;
	bool capture_rendered = false;

	if (capture.state.image.grouped == CaptureState::Off) {
		// We're in regular single image capture mode
		capture_raw = capture.state.image.raw != CaptureState::Off;
		capture_upscaled = capture.state.image.upscaled != CaptureState::Off;
		capture_rendered = capture.state.image.rendered != CaptureState::Off;

		// Clear the state flags
		capture.state.image.raw      = CaptureState::Off;
		capture.state.image.upscaled = CaptureState::Off;
		// The `rendered` state is cleared in the
		// CAPTURE_AddPostRenderImage callback
	} else {
		assert(capture.state.image.grouped == CaptureState::Pending);
		capture.state.image.grouped = CaptureState::InProgress;

		if (grouped_image_capture_formats.capture_raw) {
			capture_raw = true;
		}
		if (grouped_image_capture_formats.capture_upscaled) {
			capture_upscaled = true;
		}
		if (grouped_image_capture_formats.capture_rendered) {
			capture_rendered = true;
			// If `rendered` is enabled, the state is cleared in the
			// CAPTURE_AddPostRenderImage callback...
		} else {
			// ...otherwise we clear it now
			capture.state.image.grouped = CaptureState::Off;
		}
	}

	if (!(capture_raw || capture_upscaled || capture_rendered)) {
		return;
	}

	// We can pass in any of the image types, it doesn't matter which
	const auto index = get_next_capture_index(CaptureType::RawImage);
	if (!index) {
		return;
	}
	if (capture_raw) {
		image_capturers->GetNext().CaptureImage(
		        image.deep_copy(),
		        CapturedImageType::Raw,
		        generate_capture_filename(CaptureType::RawImage, index));
	}
	if (capture_upscaled) {
		image_capturers->GetNext().CaptureImage(
		        image.deep_copy(),
		        CapturedImageType::Upscaled,
		        generate_capture_filename(CaptureType::UpscaledImage, index));
	}

	if (capture_rendered) {
		capture.state.image.rendered_path = generate_capture_filename(
		        CaptureType::RenderedImage, index);

		// We need to propagate the image info to the PNG writer
		// so we can include the source image metadata
		capture.state.image.rendered_image_info = {image.width,
		                                           image.height,
		                                           image.pixel_aspect_ratio};
	}
}
#endif

void CAPTURE_AddFrame([[maybe_unused]] const RenderedImage& image,
                      [[maybe_unused]] const float frames_per_second)
{
#if (C_SSHOT)
	handle_add_frame_image_capture(image);

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
#if (C_SSHOT)
	assert(image_capturers);
	image_capturers->GetNext().CaptureImage(image,
	                                        CapturedImageType::Rendered,
	                                        capture.state.image.rendered_path,
	                                        capture.state.image.rendered_image_info);

	capture.state.image.rendered = CaptureState::Off;

	// In grouped capture mode adding the post-render image is always the
	// last step, so we can safely clear the flag here
	capture.state.image.grouped = CaptureState::Off;
#endif
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
	if (capture.state.image.grouped != CaptureState::Off) {
		return;
	}
	capture.state.image.grouped = CaptureState::Pending;
}

static void handle_capture_single_raw_screenshot_event(const bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}
	if (capture.state.image.raw != CaptureState::Off) {
		return;
	}
	capture.state.image.raw = CaptureState::Pending;
}

static void handle_capture_single_upscaled_screenshot_event(const bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}
	if (capture.state.image.upscaled != CaptureState::Off) {
		return;
	}
	capture.state.image.upscaled = CaptureState::Pending;
}

static void handle_capture_single_rendered_screenshot_event(const bool pressed)
{
	// Ignore key-release events
	if (!pressed) {
		return;
	}
	if (capture.state.image.rendered != CaptureState::Off) {
		return;
	}
	capture.state.image.rendered = CaptureState::Pending;
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
	image_capturers = {};

	if (capture.state.video == CaptureState::InProgress) {
		capture_video_finalise();
		capture.state.video = CaptureState::Off;
	}
#endif
}

static void parse_default_image_capture_formats_setting(const Section_prop* secprop)
{
	const std::string capture_format_prefs = secprop->Get_string(
	        "default_image_capture_formats");

	grouped_image_capture_formats.capture_raw      = false;
	grouped_image_capture_formats.capture_upscaled = false;
	grouped_image_capture_formats.capture_rendered = false;

	const auto formats = split(capture_format_prefs, ' ');
	if (formats.size() == 0) {
		LOG_WARNING("CAPTURE: 'default_image_capture_formats' not specified; "
		            "defaulting to 'upscaled'");
		grouped_image_capture_formats = {};
		return;
	}
	if (formats.size() > 3) {
		LOG_WARNING("CAPTURE: Invalid 'default_image_capture_formats' setting: '%s'. "
		            "Must not contain more than 3 formats; defaulting to 'upscaled'.",
		            capture_format_prefs.c_str());
		grouped_image_capture_formats = {};
		return;
	}
	for (const auto& format : formats) {
		if (format == "raw") {
			grouped_image_capture_formats.capture_raw = true;
		} else if (format == "upscaled") {
			grouped_image_capture_formats.capture_upscaled = true;
		} else if (format == "rendered") {
			grouped_image_capture_formats.capture_rendered = true;
		} else {
			LOG_WARNING("CAPTURE: Invalid image capture format specified for "
			            "'default_image_capture_formats': '%s'. "
			            "Valid formats are 'raw', 'upscaled', and 'rendered'; "
			            "defaulting to 'upscaled'.",
			            format.c_str());
			grouped_image_capture_formats = {};
			return;
		}
	}
}

static std::optional<int32_t> find_highest_capture_index(const CaptureType type)
{
	// Find existing capture file with the highest index
	std::string filename_start = capture_type_to_basename(type);
	lowcase(filename_start);

	const auto ext          = capture_type_to_extension(type);
	int32_t highest_index   = 0;
	std::error_code ec      = {};

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

	parse_default_image_capture_formats_setting(secprop);

#if (C_SSHOT)
	image_capturers = std::make_unique<ImageCapturers>();
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

