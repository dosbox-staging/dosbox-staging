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

#ifndef DOSBOX_IMAGE_CAPTURER_H
#define DOSBOX_IMAGE_CAPTURER_H

#include <array>
#include <string>

#include "std_filesystem.h"

#include "../capture.h"
#include "image_saver.h"
#include "render.h"

// Image capturing works in a rather roundabout fashion... If capturing the
// next frame has been requested (e.g. by pressing one of the capture
// shortcuts), first we store the request. Then the renderer that generates
// the emulated output frame by frame queries whether an image or video
// capture request is in progress, and if so, it presents the frame to the
// capturer API. The API in turn calls MaybeCaptureImage() that queues the
// frame to be saved to disk, depending on the state of the capture request
// flags.
//
// The situation is complicated by the fact that the raw and upscaled captures
// can be queued immediately as soon as the next frame is presented to the
// capture API, but the post-render/post-shader capture must be done in a
// deferred fashion via a callback from SDL main after we read back the actual
// output from framebuffer (e.g. if we want to capture the post-CRT-shader
// output).
//
// An even further complication is that in "grouped capture" mode we may need
// to capture all three image types (raw, upscaled, and post-rendered). These
// need to be synchronised (as best as we can) so all images contain the same
// frame. Moreover, "grouped capture" requests must be "blocking" operations,
// meaning that all further capture requests must be denied until the "group
// capture" has been completed to prevent various race conditions...
//
// Most of this complexity is encapsulated in the MaybeCaptureImage() method.
//
class ImageCapturer {
public:
	ImageCapturer() = default;
	ImageCapturer(const std::string& grouped_mode_prefs);

	~ImageCapturer();

	void RequestRawCapture();
	void RequestUpscaledCapture();
	void RequestRenderedCapture();
	void RequestGroupedCapture();

	bool IsCaptureRequested() const;
	bool IsRenderedCaptureRequested() const;

	void MaybeCaptureImage(const RenderedImage& image,
	                       const VideoMode& video_mode);

	void CapturePostRenderImage(const RenderedImage& image);

	// prevent copying
	ImageCapturer(const ImageCapturer&) = delete;
	// prevent assignment
	ImageCapturer& operator=(const ImageCapturer&) = delete;

private:
	struct State {
		CaptureState raw      = {};
		CaptureState upscaled = {};
		CaptureState rendered = {};
		CaptureState grouped  = {};
	};
	State state = {};

	struct GroupedMode {
		bool wants_raw      = false;
		bool wants_upscaled = false;
		bool wants_rendered = false;
	};
	GroupedMode grouped_mode = {};

	std_fs::path rendered_path    = {};
	VideoMode rendered_video_mode = {};

	static constexpr auto NumImageSavers                = 3;
	size_t current_image_saver_index                    = 0;
	std::array<ImageSaver, NumImageSavers> image_savers = {};

	void ConfigureGroupedMode(const std::string& prefs);

	ImageSaver& GetNextImageSaver();
};

#endif // DOSBOX_IMAGE_CAPTURER_H
