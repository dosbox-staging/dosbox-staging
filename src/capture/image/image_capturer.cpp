/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2024  The DOSBox Staging Team
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

#include "image_capturer.h"

#include <cassert>
#include <string>

#include "std_filesystem.h"

#include "checks.h"
#include "setup.h"
#include "string_utils.h"

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
	if (formats.size() == 0) {
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

	bool do_raw      = false;
	bool do_upscaled = false;
	bool do_rendered = false;

	if (state.grouped == CaptureState::Off) {
		// We're in regular single image capture mode
		do_raw      = (state.raw != CaptureState::Off);
		do_upscaled = (state.upscaled != CaptureState::Off);
		do_rendered = (state.rendered != CaptureState::Off);

		// Clear the state flags
		state.raw      = CaptureState::Off;
		state.upscaled = CaptureState::Off;
		// The `rendered` state is cleared in the
		// CAPTURE_AddPostRenderImage callback
	} else {
		assert(state.grouped == CaptureState::Pending);
		state.grouped = CaptureState::InProgress;

		if (grouped_mode.wants_raw) {
			do_raw = true;
		}
		if (grouped_mode.wants_upscaled) {
			do_upscaled = true;
		}
		if (grouped_mode.wants_rendered) {
			do_rendered = true;
			// If rendered capture is wanted, the state will be
			// cleared in the CapturePostRenderImagecallback...
		} else {
			// ...otherwise we clear it now
			state.grouped = CaptureState::Off;
		}
	}

	if (!(do_raw || do_upscaled || do_rendered)) {
		return;
	}

	// We can pass in any of the image types, it doesn't matter which
	const auto index = get_next_capture_index(CaptureType::RawImage);
	if (!index) {
		return;
	}
	if (do_raw) {
		GetNextImageSaver().QueueImage(
		        image.deep_copy(),
		        CapturedImageType::Raw,
		        generate_capture_filename(CaptureType::RawImage, index));
	}
	if (do_upscaled) {
		GetNextImageSaver().QueueImage(
		        image.deep_copy(),
		        CapturedImageType::Upscaled,
		        generate_capture_filename(CaptureType::UpscaledImage, index));
	}

	if (do_rendered) {
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
