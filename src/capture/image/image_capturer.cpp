// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "image_capturer.h"

#include <cassert>
#include <string>

#include "config/setup.h"
#include "misc/std_filesystem.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

ImageCapturer::ImageCapturer(const std::string& grouped_mode_prefs)
{
	ConfigureGroupedMode(grouped_mode_prefs);

	for (auto& image_saver : image_savers) {
		image_saver.Open();
	}

	LOG_MSG("CAPTURE: Image capturer started");
}

void ImageCapturer::ConfigureGroupedMode(const std::string& prefs)
{
	constexpr const char* DefaultSetting = "upscaled";

	auto set_defaults = [&] {
		grouped_mode.wants_raw      = false;
		grouped_mode.wants_upscaled = true;
		grouped_mode.wants_rendered = false;

		set_section_property_value("capture",
		                           "default_image_capture_formats",
		                           DefaultSetting);
	};

	grouped_mode.wants_raw      = false;
	grouped_mode.wants_upscaled = false;
	grouped_mode.wants_rendered = false;

	const auto formats = split_with_empties(prefs, ' ');
	if (formats.empty()) {
		LOG_WARNING(
		        "CAPTURE: 'default_image_capture_formats' not specified, "
		        "using '%s'",
		        DefaultSetting);
		set_defaults();
		return;
	}
	if (formats.size() > 3) {
		LOG_WARNING(
		        "CAPTURE: Invalid 'default_image_capture_formats' setting: '%s'. "
		        "Must not contain more than 3 formats, using '%s'.",
		        prefs.c_str(),
		        DefaultSetting);
		set_defaults();
		return;
	}

	for (const auto& format : formats) {
		if (format == "raw") {
			grouped_mode.wants_raw = true;
		} else if (format == "upscaled") {
			grouped_mode.wants_upscaled = true;
		} else if (format == "rendered") {
			grouped_mode.wants_rendered = true;
		} else {
			LOG_WARNING(
			        "CAPTURE: Invalid 'default_image_capture_formats' setting: '%s'. "
			        "Valid formats are 'raw', 'upscaled', and 'rendered'; "
			        "using '%s'.",
			        format.c_str(),
			        DefaultSetting);
			set_defaults();
			return;
		}
	}
}

ImageCapturer::~ImageCapturer()
{
	for (auto& image_saver : image_savers) {
		image_saver.Close();
	}

	LOG_MSG("CAPTURE: Image capturer shutting down");
}

bool ImageCapturer::IsCaptureRequested() const
{
	return state.raw != CaptureState::Off ||
	       state.upscaled != CaptureState::Off ||
	       state.rendered != CaptureState::Off ||
	       state.grouped != CaptureState::Off;
}

bool ImageCapturer::IsRenderedCaptureRequested() const
{
	return state.rendered != CaptureState::Off ||
	       (state.grouped != CaptureState::Off && grouped_mode.wants_rendered);
}

void ImageCapturer::MaybeCaptureImage(const RenderedImage& image)
{
	// No new image capture requests until we finish queuing the current
	// grouped capture request, otherwise we can get into all sorts of race
	// conditions.
	if (state.grouped == CaptureState::InProgress) {
		return;
	}

	bool capture_raw      = false;
	bool capture_upscaled = false;
	bool capture_rendered = false;

	if (state.grouped == CaptureState::Off) {
		// We're in regular single image capture mode
		capture_raw      = (state.raw != CaptureState::Off);
		capture_upscaled = (state.upscaled != CaptureState::Off);
		capture_rendered = (state.rendered != CaptureState::Off);

		// Clear the state flags
		state.raw      = CaptureState::Off;
		state.upscaled = CaptureState::Off;
		// The `rendered` state is cleared in the
		// CAPTURE_AddPostRenderImage callback
	} else {
		assert(state.grouped == CaptureState::Pending);
		state.grouped = CaptureState::InProgress;

		if (grouped_mode.wants_raw) {
			capture_raw = true;
		}
		if (grouped_mode.wants_upscaled) {
			capture_upscaled = true;
		}
		if (grouped_mode.wants_rendered) {
			capture_rendered = true;
			// If rendered capture is wanted, the state will be
			// cleared in the CapturePostRenderImage() callback...
		} else {
			// ...otherwise we're clearing it now.
			state.grouped = CaptureState::Off;
		}
	}

	if (!(capture_raw || capture_upscaled || capture_rendered)) {
		return;
	}

	// We can pass in any of the image types; it doesn't matter which.
	const auto index = get_next_capture_index(CaptureType::RawImage);
	if (!index) {
		return;
	}
	if (capture_raw) {
		GetNextImageSaver().QueueImage(
		        image.deep_copy(),
		        CapturedImageType::Raw,
		        generate_capture_filename(CaptureType::RawImage, index));
	}
	if (capture_upscaled) {
		GetNextImageSaver().QueueImage(
		        image.deep_copy(),
		        CapturedImageType::Upscaled,
		        generate_capture_filename(CaptureType::UpscaledImage, index));
	}

	if (capture_rendered) {
		rendered_path = generate_capture_filename(CaptureType::RenderedImage,
		                                          index);
	}
}

void ImageCapturer::CapturePostRenderImage(const RenderedImage& image)
{
	GetNextImageSaver().QueueImage(image, CapturedImageType::Rendered, rendered_path);

	state.rendered = CaptureState::Off;

	// In grouped capture mode, adding the post-render image is
	// always the last step, so we can safely clear the flag here.
	state.grouped = CaptureState::Off;
}

ImageSaver& ImageCapturer::GetNextImageSaver()
{
	++current_image_saver_index;
	current_image_saver_index %= NumImageSavers;
	return image_savers[current_image_saver_index];
}

void ImageCapturer::RequestRawCapture()
{
	if (state.raw != CaptureState::Off) {
		return;
	}
	state.raw = CaptureState::Pending;
}

void ImageCapturer::RequestUpscaledCapture()
{
	if (state.upscaled != CaptureState::Off) {
		return;
	}
	state.upscaled = CaptureState::Pending;
}

void ImageCapturer::RequestRenderedCapture()
{
	if (state.rendered != CaptureState::Off) {
		return;
	}
	state.rendered = CaptureState::Pending;
}

void ImageCapturer::RequestGroupedCapture()
{
	if (state.grouped != CaptureState::Off) {
		return;
	}
	state.grouped = CaptureState::Pending;
}
