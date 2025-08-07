// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "capture.h"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <mutex>

#include "capture_audio.h"
#include "capture_midi.h"
#include "capture_video.h"
#include "checks.h"
#include "control.h"
#include "fs_utils.h"
#include "image/image_capturer.h"
#include "mapper.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"

#include <SDL.h>

CHECK_NARROWING();

static struct {
	std_fs::path path     = {};
	bool path_initialised = false;

	struct {
		std::atomic<CaptureState> audio = {};
		std::atomic<CaptureState> midi  = {};
		std::atomic<CaptureState> video = {};
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

	void reset()
	{
		path.clear();
		path_initialised = false;
		state.audio = CaptureState::Off;
		state.midi = CaptureState::Off;
		state.video = CaptureState::Off;
		next_index = {};
	}
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

static bool create_capture_directory()
{
	std::error_code ec = {};

	if (!std_fs::exists(capture.path, ec)) {
		if (!std_fs::create_directory(capture.path, ec)) {
			LOG_WARNING("CAPTURE: Can't create directory '%s': %s",
			            capture.path.string().c_str(),
			            ec.message().c_str());
			return false;
		}
		std_fs::permissions(capture.path, std_fs::perms::owner_all, ec);
	}
	return true;
}

static std::optional<int32_t> find_highest_capture_index(const CaptureType type)
{
	// Find existing capture file with the highest index
	std::string filename_start = capture_type_to_basename(type);
	lowcase(filename_start);

	int32_t highest_index = 0;
	std::error_code ec    = {};

	for (const auto& entry : std_fs::directory_iterator(capture.path, ec)) {
		if (ec) {
			LOG_WARNING("CAPTURE: Cannot open directory '%s': %s",
			            capture.path.string().c_str(),
			            ec.message().c_str());
			return {};
		}
		if (!entry.is_regular_file(ec)) {
			continue;
		}
		auto stem = entry.path().stem().string();
		lowcase(stem);

		if (stem.starts_with(filename_start)) {
			auto index_str = strip_prefix(stem, filename_start);

			// Strip "-raw" or "-rendered" postfix if it's there
			if (const auto dash_pos = index_str.find('-');
			    dash_pos != std::string::npos) {
				index_str = index_str.substr(0, dash_pos);
			}
			const auto index = parse_int(index_str);
			if (index) {
				highest_index = std::max(highest_index, *index);
			}
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

static bool maybe_create_capture_dir_and_init_capture_indices()
{
	static std::mutex mutex = {};
	std::lock_guard<std::mutex> lock(mutex);

	if (capture.path_initialised) {
		return true;
	}
	if (!create_capture_directory()) {
		return false;
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
			return false;
		}
	}

	capture.path_initialised = true;

	return true;
}

int32_t get_next_capture_index(const CaptureType type)
{
	if (!maybe_create_capture_dir_and_init_capture_indices()) {
		return 0;
	}

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
	const auto filename = format_str("%s%04d%s%s",
	                                    capture_type_to_basename(type),
	                                    index,
	                                    capture_type_to_postfix(type),
	                                    capture_type_to_extension(type));
	return {capture.path / filename};
}

FILE* CAPTURE_CreateFile(const CaptureType type,
                         const std::optional<std_fs::path>& path)
{
	if (!maybe_create_capture_dir_and_init_capture_indices()) {
		return nullptr;
	}

	std::string path_str = {};
	if (path) {
		path_str = path->string();
	} else {
		const auto index = get_next_capture_index(type);
		path_str = generate_capture_filename(type, index).string();
	}

	FILE* handle = open_file(path_str.c_str(), "wb");
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

void CAPTURE_StartVideoCapture()
{
	switch (capture.state.video) {
	case CaptureState::Off:
		capture.state.video = CaptureState::Pending;
		GFX_NotifyVideoCaptureStatus(true);
		break;
	case CaptureState::Pending:
	case CaptureState::InProgress:
		LOG_WARNING("CAPTURE: Already capturing video output");
		break;
	}
}

void CAPTURE_StopVideoCapture()
{
	switch (capture.state.video) {
	case CaptureState::Off:
		LOG_WARNING("CAPTURE: Not capturing video output");
		break;
	case CaptureState::Pending:
		// It's very hard to hit this branch; handling it for
		// completeness only
		LOG_MSG("CAPTURE: Cancelling pending video output capture");
		capture.state.video = CaptureState::Off;
		GFX_NotifyVideoCaptureStatus(false);
		break;
	case CaptureState::InProgress:
		capture_video_finalise();
		capture.state.video = CaptureState::Off;
		GFX_NotifyVideoCaptureStatus(false);
		LOG_MSG("CAPTURE: Stopped capturing video output");
	}
}

void CAPTURE_AddFrame(const RenderedImage& image, const float frames_per_second)
{
	LOG_DEBUG("CAPTURE_AddFrame");

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
}

void CAPTURE_AddPostRenderImage(const RenderedImage& image)
{
	if (image_capturer) {
		image_capturer->CapturePostRenderImage(image);
	}
}

void CAPTURE_AddAudioData(const uint32_t sample_rate, const uint32_t num_sample_frames,
                          const int16_t* sample_frames)
{
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
	if (capture.state.midi == CaptureState::Pending) {
		capture.state.midi = CaptureState::InProgress;
	}
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

static void capture_destroy(Section* /*sec*/)
{
	if (capture.state.audio == CaptureState::InProgress) {
		capture_audio_finalise();
		capture.state.audio = CaptureState::Off;
	}
	if (capture.state.midi == CaptureState::InProgress) {
		capture_midi_finalise();
		capture.state.midi = CaptureState::Off;
	}
	// When destructed, the threaded image capturer instances do a blocking
	// wait until all pending capture tasks are processed.
	image_capturer = {};

	if (capture.state.video == CaptureState::InProgress) {
		capture_video_finalise();
		capture.state.video = CaptureState::Off;
	}

	capture.reset();
}

static void capture_init(Section* sec)
{
	assert(sec);
	const SectionProp* secprop = dynamic_cast<SectionProp*>(sec);
	if (!secprop) {
		return;
	}

	PropPath* capture_path = secprop->GetPath("capture_dir");
	assert(capture_path);

	// We can safely change the capture output path even if capturing of any
	// type is in progress.
	capture.path = capture_path->realpath;
	if (capture.path.empty()) {
		LOG_WARNING("CAPTURE: No value specified for `capture_dir`; defaulting to 'capture' "
		            "in the current working directory");
		capture.path = "capture";
	}

	const std::string prefs = secprop->GetString("default_image_capture_formats");

	image_capturer = std::make_unique<ImageCapturer>(prefs);

	constexpr auto changeable_at_runtime = true;
	sec->AddDestroyFunction(&capture_destroy, changeable_at_runtime);
}

static void init_key_mappings()
{
	MAPPER_AddHandler(handle_capture_audio_event,
	                  SDL_SCANCODE_F6,
	                  PRIMARY_MOD,
	                  "recwave",
	                  "Rec. Audio");

	MAPPER_AddHandler(handle_capture_midi_event,
	                  SDL_SCANCODE_F6,
	                  PRIMARY_MOD | MMOD2,
	                  "caprawmidi",
	                  "Rec. MIDI");

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
}

static void init_capture_dosbox_settings(SectionProp& secprop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto* path_prop = secprop.AddPath("capture_dir", when_idle, "capture");
	path_prop->SetHelp(
	        "Directory where the various captures are saved, such as audio, video, MIDI\n"
	        "and screenshot captures. ('capture' in the current working directory by\n"
	        "default).");
	assert(path_prop);

	auto* str_prop = secprop.AddString("default_image_capture_formats",
	                                    when_idle,
	                                    "upscaled");
	str_prop->SetHelp(
	        "Set the capture format of the default screenshot action ('upscaled' by\n"
	        "default):\n"
	        "  upscaled:  The image is bilinear-sharp upscaled and the correct aspect\n"
	        "             ratio is maintained, depending on the 'aspect' setting. The\n"
	        "             vertical scaling factor is always an integer. For example,\n"
	        "             320x200 content is upscaled to 1600x1200 (5:6 integer scaling),\n"
	        "             640x480 to 1920x1440 (3:3 integer scaling), and 640x350 to\n"
	        "             1400x1050 (2.1875:3 scaling; fractional horizontally and\n"
	        "             integer vertically). The filenames of upscaled screenshots\n"
	        "             have no postfix (e.g. 'image0001.png').\n"
	        "  rendered:  The post-rendered, post-shader image shown on the screen is\n"
	        "             captured. The filenames of rendered screenshots end with\n"
	        "             '-rendered' (e.g. 'image0001-rendered.png').\n"
	        "  raw:       The contents of the raw framebuffer is captured (this always\n"
	        "             results in square pixels). The filenames of raw screenshots\n"
	        "             end with '-raw' (e.g. 'image0001-raw.png').\n"
	        "If multiple formats are specified separated by spaces, the default\n"
	        "screenshot action will save multiple images in the specified formats.\n"
	        "Keybindings for taking single screenshots in specific formats are also\n"
	        "available.");
	assert(str_prop);
}

void CAPTURE_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	constexpr auto changeable_at_runtime = true;

	SectionProp* sec = conf->AddSectionProp("capture",
	                                          &capture_init,
	                                          changeable_at_runtime);
	assert(sec);
	init_capture_dosbox_settings(*sec);
	init_key_mappings();
}

