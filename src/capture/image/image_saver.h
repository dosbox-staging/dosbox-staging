// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_IMAGE_SAVER_H
#define DOSBOX_IMAGE_SAVER_H

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "image_scaler.h"

#include "misc/image_decoder.h"
#include "misc/rendered_image.h"
#include "misc/std_filesystem.h"
#include "utils/rwqueue.h"

enum class CapturedImageType { Raw, Upscaled, Rendered };

struct SaveImageTask {
	RenderedImage image              = {};
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
	void QueueImage(const RenderedImage& image, const CapturedImageType type,
	                const std::optional<std_fs::path>& path);

	// prevent copying
	ImageSaver(const ImageSaver&) = delete;
	// prevent assignment
	ImageSaver& operator=(const ImageSaver&) = delete;

private:
	static constexpr auto MaxQueuedImages = 10;

	void SaveQueuedImages();
	void SaveImage(const SaveImageTask& task);

	void SaveRawImage(const RenderedImage& image);
	void SaveUpscaledImage(const RenderedImage& image);
	void SaveRenderedImage(const RenderedImage& image);

	void CloseOutFile();

	RWQueue<SaveImageTask> image_fifo{MaxQueuedImages};
	std::thread renderer = {};
	bool is_open         = false;

	ImageScaler image_scaler = {};

	std::vector<uint32_t> row_decode_buf = {};
	std::vector<uint8_t> row_output_buf  = {};

	FILE* outfile = nullptr;
};

#endif // DOSBOX_IMAGE_SAVER_H

