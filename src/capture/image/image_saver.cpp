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

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "image_saver.h"

#include "../capture.h"
#include "checks.h"
#include "png_writer.h"
#include "support.h"

CHECK_NARROWING();

ImageSaver::~ImageSaver()
{
	Close();
}

void ImageSaver::Open()
{
	if (is_open) {
		Close();
	}

	const auto worker_function = std::bind(&ImageSaver::SaveQueuedImages, this);
	renderer = std::thread(worker_function);
	set_thread_name(renderer, "dosbox:imgcap");

	is_open = true;
}

void ImageSaver::Close()
{
	if (!is_open) {
		return;
	}

	// Stop queuing new images
	image_fifo.Stop();

	// Let the renderer finish saving pending images
	if (renderer.joinable()) {
		renderer.join();
	}

	is_open = false;
}

void ImageSaver::QueueImage(const RenderedImage& image, const CapturedImageType type,
                            const std::optional<std_fs::path>& path)
{
	if (!image_fifo.IsRunning()) {
		LOG_WARNING("CAPTURE: Cannot create screenshots while image capturer"
		            "is shutting down");
		return;
	}

	SaveImageTask task = {image, type, path};
	image_fifo.Enqueue(std::move(task));
}

void ImageSaver::SaveQueuedImages()
{
	while (auto task = image_fifo.Dequeue()) {
		SaveImage(*task);
		task->image.free();
	}
}

static CaptureType to_capture_type(const CapturedImageType type)
{
	switch (type) {
	case CapturedImageType::Raw: return CaptureType::RawImage;
	case CapturedImageType::Upscaled: return CaptureType::UpscaledImage;
	case CapturedImageType::Rendered: return CaptureType::RenderedImage;
	default: assertm(false, "Invalid CaptureImageType"); return {};
	}
}

void ImageSaver::SaveImage(const SaveImageTask& task)
{
	CaptureType capture_type = to_capture_type(task.image_type);

	outfile = CAPTURE_CreateFile(capture_type, task.path);
	if (!outfile) {
		return;
	}

	switch (task.image_type) {
	case CapturedImageType::Raw: SaveRawImage(task.image); break;
	case CapturedImageType::Upscaled: SaveUpscaledImage(task.image); break;
	case CapturedImageType::Rendered: SaveRenderedImage(task.image); break;
	}

	CloseOutFile();
}

static void write_upscaled_png(FILE* outfile, PngWriter& png_writer,
                               ImageScaler& image_scaler, const uint16_t width,
                               const uint16_t height,
                               const Fraction& pixel_aspect_ratio,
                               const VideoMode& video_mode,
                               const uint8_t* palette_data)
{
	switch (image_scaler.GetOutputPixelFormat()) {
	case OutputPixelFormat::Indexed8:
		if (!png_writer.InitIndexed8(outfile,
		                             width,
		                             height,
		                             pixel_aspect_ratio,
		                             video_mode,
		                             palette_data)) {
			return;
		}
		break;
	case OutputPixelFormat::Rgb888:
		if (!png_writer.InitRgb888(
		            outfile, width, height, pixel_aspect_ratio, video_mode)) {
			return;
		}
		break;
	};

	auto rows_to_write = image_scaler.GetOutputHeight();
	while (rows_to_write--) {
		auto row = image_scaler.GetNextOutputRow();
		png_writer.WriteRow(row);
	}
}

// to avoid circular dependency
uint8_t get_double_scan_row_skip_count(const RenderedImage&);

void ImageSaver::SaveRawImage(const RenderedImage& image)
{
	PngWriter png_writer = {};

	auto row_skip_count = get_double_scan_row_skip_count(image);
	image_decoder.Init(image, row_skip_count);

	const auto& src = image.params;

	const auto raw_image_height = src.video_mode.height;

	// Write the pixel aspect ratio of the video mode into the PNG pHYs
	// chunk for raw images
	const auto pixel_aspect_ratio = src.video_mode.pixel_aspect_ratio;

	if (image.is_paletted()) {
		if (!png_writer.InitIndexed8(outfile,
		                             src.width,
		                             raw_image_height,
		                             pixel_aspect_ratio,
		                             src.video_mode,
		                             image.palette_data)) {
			return;
		};
	} else {
		if (!png_writer.InitRgb888(outfile,
		                           src.width,
		                           raw_image_height,
		                           pixel_aspect_ratio,
		                           src.video_mode)) {
			return;
		};
	}

	constexpr uint8_t MaxBytesPerPixel = 3;
	row_buf.resize(static_cast<size_t>(src.width) *
	               static_cast<size_t>(MaxBytesPerPixel));

	auto rows_to_write = raw_image_height;
	while (rows_to_write--) {
		auto out = row_buf.begin();

		auto pixels_to_write = src.width;
		if (image.is_paletted()) {
			while (pixels_to_write--) {
				const auto pixel = image_decoder.GetNextIndexed8Pixel();

				*out++ = pixel;
			}
		} else {
			while (pixels_to_write--) {
				const auto pixel = image_decoder.GetNextPixelAsRgb888();

				*out++ = pixel.red;
				*out++ = pixel.green;
				*out++ = pixel.blue;
			}
		}
		png_writer.WriteRow(row_buf.begin());
		image_decoder.AdvanceRow();
	}
}

static constexpr auto square_pixel_aspect_ratio = Fraction{1};

void ImageSaver::SaveUpscaledImage(const RenderedImage& image)
{
	PngWriter png_writer = {};

	image_scaler.Init(image);

	// Always write 1:1 pixel aspect ratio into the PNG pHYs chunk for
	// upscaled images as the "non-squaredness" is "baked into" the image
	// data.
	write_upscaled_png(outfile,
	                   png_writer,
	                   image_scaler,
	                   image_scaler.GetOutputWidth(),
	                   image_scaler.GetOutputHeight(),
	                   square_pixel_aspect_ratio,
	                   image.params.video_mode,
	                   image.palette_data);
}

void ImageSaver::SaveRenderedImage(const RenderedImage& image)
{
	PngWriter png_writer = {};

	const auto& src = image.params;

	// Always write 1:1 pixel aspect ratio into the PNG pHYs chunk for
	// rendered images as the "non-squaredness" is "baked into" the image
	// data.
	if (!png_writer.InitRgb888(outfile,
	                           src.width,
	                           src.height,
	                           square_pixel_aspect_ratio,
	                           src.video_mode)) {
		return;
	};

	const auto row_skip_count = 0;
	image_decoder.Init(image, row_skip_count);

	constexpr uint8_t BytesPerPixel = 3;
	row_buf.resize(static_cast<size_t>(src.width) *
	               static_cast<size_t>(BytesPerPixel));

	auto rows_to_write = src.height;
	while (rows_to_write--) {
		auto out = row_buf.begin();

		auto pixels_to_write = src.width;
		while (pixels_to_write--) {
			const auto pixel = image_decoder.GetNextPixelAsRgb888();

			*out++ = pixel.red;
			*out++ = pixel.green;
			*out++ = pixel.blue;
		}
		png_writer.WriteRow(row_buf.begin());
		image_decoder.AdvanceRow();
	}
}

void ImageSaver::CloseOutFile()
{
	if (outfile) {
		fclose(outfile);
		outfile = nullptr;
	}
}

