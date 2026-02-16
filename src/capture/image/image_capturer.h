// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_IMAGE_CAPTURER_H
#define DOSBOX_IMAGE_CAPTURER_H

#include <array>
#include <string>

#include "capture/capture.h"
#include "gui/render/render.h"
#include "image_saver.h"
#include "misc/std_filesystem.h"

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

	void MaybeCaptureImage(const RenderedImage& image);
	void CapturePostRenderImage(const RenderedImage& image);

	// prevent copying
	ImageCapturer(const ImageCapturer&) = delete;
	// prevent assignment
	ImageCapturer& operator=(const ImageCapturer&) = delete;

private:
	struct State {
		// If `grouped` is not `Off`, the `raw`, `upscaled` and
		// `rendered` single-capture states are `Off`.
		//
		// Conversely, if `grouped` is not `Off`, the rest of the
		// single-capture states are `Off.`
		//
		CaptureState raw      = CaptureState::Off;
		CaptureState upscaled = CaptureState::Off;
		CaptureState rendered = CaptureState::Off;

		CaptureState grouped = CaptureState::Off;
	};
	State state = {};

	struct GroupedMode {
		// True if we need to capture the raw output in group capture mode
		bool wants_raw = false;

		// True if we need to capture the upscaled output in group
		// capture mode
		bool wants_upscaled = false;

		// True if we need to capture the rendered output in group
		// capture mode
		bool wants_rendered = false;
	};
	GroupedMode grouped_mode = {};

	std_fs::path rendered_path = {};

	static constexpr auto NumImageSavers                = 3;
	size_t current_image_saver_index                    = 0;
	std::array<ImageSaver, NumImageSavers> image_savers = {};

	void ConfigureGroupedMode(const std::string& prefs);

	ImageSaver& GetNextImageSaver();
};

#endif // DOSBOX_IMAGE_CAPTURER_H
