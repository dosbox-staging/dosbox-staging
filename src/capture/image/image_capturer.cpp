/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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
	auto set_defaults = [&] {
		grouped_mode.wants_raw      = false;
		grouped_mode.wants_upscaled = true;
		grouped_mode.wants_rendered = false;
	};

	grouped_mode.wants_raw      = false;
	grouped_mode.wants_upscaled = false;
	grouped_mode.wants_rendered = false;

	const auto formats = split(prefs, ' ');
	if (formats.size() == 0) {
		LOG_WARNING("CAPTURE: 'default_image_capture_formats' not specified; "
		            "defaulting to 'upscaled'");
		set_defaults();
		return;
	}
	if (formats.size() > 3) {
		LOG_WARNING("CAPTURE: Invalid 'default_image_capture_formats' setting: '%s'. "
		            "Must not contain more than 3 formats; defaulting to 'upscaled'.",
		            prefs.c_str());
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
			LOG_WARNING("CAPTURE: Invalid image capture format specified for "
			            "'default_image_capture_formats': '%s'. "
			            "Valid formats are 'raw', 'upscaled', and 'rendered'; "
			            "defaulting to 'upscaled'.",
			            format.c_str());
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

void ImageCapturer::MaybeCaptureImage(const RenderedImage& image,
                                      const VideoMode& video_mode)
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
		        video_mode,
		        CapturedImageType::Raw,
		        generate_capture_filename(CaptureType::RawImage, index));
	}
	if (do_upscaled) {
		GetNextImageSaver().QueueImage(
		        image.deep_copy(),
		        video_mode,
		        CapturedImageType::Upscaled,
		        generate_capture_filename(CaptureType::UpscaledImage, index));
	}

	if (do_rendered) {
		rendered_path = generate_capture_filename(CaptureType::RenderedImage,
		                                          index);

		// We need to propagate the video mode to the PNG writer so we
		// can include the source image metadata.
		rendered_video_mode = video_mode;
	}
}

void ImageCapturer::CapturePostRenderImage(const RenderedImage& image)
{
	GetNextImageSaver().QueueImage(image,
	                               rendered_video_mode,
	                               CapturedImageType::Rendered,
	                               rendered_path);

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

uint8_t get_double_scan_row_skip_count(const RenderedImage& image,
                                       const VideoMode& video_mode)
{
	// Double-scanning can be either:
	//
	// 1) "baked" into the image; `image.image_data` contains twice as many
	// rows (e.g. `video_mode.height` is 200 and `image.height` 400),
	//
	// 2) or it can be "faked" with `image.double_height` set to true, in
	// which case `video_mode.height` equals `image.height` (e.g. both are
	// 200) and the height-doubling happens as a post-processing step on
	// `image.image_data` just before the final output.
	//
	// For case 2, there's nothing to do; the image data itself is not
	// double-scanned. For case 1, we need to reconstruct the raw,
	// non-double-scanned image to serve as the basis for our further output
	// and scaling operations, so we must skip every second row if we're
	// dealing with "baked in" double-scanning.
	//
	// This function returns `0` for case 1 images (faked double-scan), and
	// `1` for case 2 images (baked-in double-scan).
	//
	assert(image.height >= video_mode.height);
	assert(image.height % video_mode.height == 0);

	return check_cast<uint8_t>(image.height / video_mode.height - 1);
}
