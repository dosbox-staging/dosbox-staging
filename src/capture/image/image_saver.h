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

#ifndef DOSBOX_IMAGE_SAVER_H
#define DOSBOX_IMAGE_SAVER_H

#include <atomic>
#include <optional>
#include <thread>
#include <vector>

#include "std_filesystem.h"

#include "image_decoder.h"
#include "image_scaler.h"
#include "render.h"
#include "rwqueue.h"

enum class CapturedImageType { Raw, Upscaled, Rendered };

struct SaveImageTask {
	RenderedImage image              = {};
	VideoMode video_mode             = {};
	CapturedImageType image_type     = {};
	std::optional<std_fs::path> path = {};
};

// Threaded image capturer; capture requests are placed in a FIFO queue then
// are processed in order.
//
// The image capturer _frees_ the passed in RenderedImage after the image was
// saved. Therefore, you might need to pass in deep-copied RenderedImage
// instances (with the pixel and palette data deep-copied) to avoid freeing
// the internal render buffers.
//
// We could make the image capturer 100% safe by always making a deep-copy of
// the passed in RenderedImage before storing it in the work queue. But this
// would not allow for an important optimisation step: post-render/post-shader
// images (which can get very large at 4K resolutions; ~24 MB for a fullscreen
// 4K capture) would always need to be copied as well.
//
// For post-render/post-shader images, a copy is actually not needed, and for
// the rest of the cases the copy is generally cheap because we're only copying
// the contents of the interal render buffer (64K for 320x200/256, 300K for
// 640x480/256, and ~1.2MB for 640x480 true-colour).
//
// All downstream processing is row-based, meaning the image scaler and the
// image writer are operating in row-sized chunks. This is crucial to keep the
// memory usage low and to avoid cache thrashing: the memory requirements to
// upscale a single 720x400 image to 1600x1200 would be ~10MB if all buffers
// stored full images (the intermediary 32-bit float linear RGB buffer, plus
// the final RBG888 output buffer). With the row-based approach, the memory
// requirement is only 13K (!) and we get much better cache utilisation.
//
// Also, we're running multiple image capture worker threads in parallel, so
// that would add a multiplier to the memory usage.
//
class ImageSaver {
public:
	ImageSaver() = default;
	~ImageSaver();

	void Open();
	void Close();

	// IMPORTANT: The capturer _frees_ the passed in RenderedImage after the
	// image was saved. Consider the implications carefully; you might need
	// to pass in a deep-copied copy of the RenderedImage instance, because
	// you cannot know when exactly in the future will it be freed.
	void QueueImage(const RenderedImage& image, const VideoMode& video_mode,
	                const CapturedImageType type,
	                const std::optional<std_fs::path>& path);

	// prevent copying
	ImageSaver(const ImageSaver&) = delete;
	// prevent assignment
	ImageSaver& operator=(const ImageSaver&) = delete;

private:
	static constexpr auto MaxQueuedImages = 10;

	void SaveQueuedImages();
	void SaveImage(const SaveImageTask& task);

	void SaveRawImage(const RenderedImage& image, const VideoMode& video_mode);

	void SaveUpscaledImage(const RenderedImage& image,
	                       const VideoMode& video_mode);

	void SaveRenderedImage(const RenderedImage& image,
	                       const VideoMode& video_mode);

	void CloseOutFile();

	RWQueue<SaveImageTask> image_fifo{MaxQueuedImages};
	std::thread renderer = {};
	bool is_open         = false;

	ImageScaler image_scaler = {};

	ImageDecoder image_decoder   = {};
	std::vector<uint8_t> row_buf = {};

	FILE* outfile = nullptr;
};

#endif // DOSBOX_IMAGE_SAVER_H

