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
                            const std::optional<std_fs::path>& path,
                            const std::optional<ImageInfo>& source_image_info_override)
{
	if (!image_fifo.IsRunning()) {
		LOG_WARNING("CAPTURE: Cannot create screenshots while image capturer"
		            "is shutting down");
		return;
	}

	SaveImageTask task = {image, type, path, source_image_info_override};
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
	case CapturedImageType::Rendered:
		assertm(task.source_image_info_override,
		        "source_image_info_override must be provided for rendered images");
		if (task.source_image_info_override) {
			SaveRenderedImage(task.image,
			                  *task.source_image_info_override);
		}
		break;
	}

	CloseOutFile();
}

static void write_upscaled_png(FILE* outfile, PngWriter& png_writer,
                               ImageScaler& image_scaler, const ImageInfo& image_info,
                               const std::optional<ImageInfo>& source_image_info,
                               const uint8_t* palette_data)
{
	switch (image_scaler.GetOutputPixelFormat()) {
	case PixelFormat::Indexed8:
		if (!png_writer.InitIndexed8(
		            outfile, image_info, source_image_info, palette_data)) {
			return;
		}
		break;
	case PixelFormat::Rgb888:
		if (!png_writer.InitRgb888(outfile, image_info, source_image_info)) {
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

void ImageSaver::SaveRawImage(const RenderedImage& image)
{
	PngWriter png_writer = {};

	image_scaler.Init(image, ScalingMode::DoublingOnly);

	const ImageInfo image_info = {image_scaler.GetOutputWidth(),
	                              image_scaler.GetOutputHeight(),
	                              image.pixel_aspect_ratio};

	constexpr std::optional<ImageInfo> no_source_image_info = {};

	write_upscaled_png(outfile,
	                   png_writer,
	                   image_scaler,
	                   image_info,
	                   no_source_image_info,
	                   image.palette_data);
}

void ImageSaver::SaveUpscaledImage(const RenderedImage& image)
{
	PngWriter png_writer = {};

	image_scaler.Init(image, ScalingMode::AspectRatioPreservingUpscale);

	const auto square_pixel_aspect_ratio = Fraction{1};

	const ImageInfo image_info = {image_scaler.GetOutputWidth(),
	                              image_scaler.GetOutputHeight(),
	                              square_pixel_aspect_ratio};

	const ImageInfo source_image_info = {image.width,
	                                     image.height,
	                                     image.pixel_aspect_ratio};
	write_upscaled_png(outfile,
	                   png_writer,
	                   image_scaler,
	                   image_info,
	                   source_image_info,
	                   image.palette_data);
}

void ImageSaver::SaveRenderedImage(const RenderedImage& image,
                                   const ImageInfo& source_image_info)
{
	PngWriter png_writer = {};

	const ImageInfo image_info = {image.width, image.height, image.pixel_aspect_ratio};

	if (!png_writer.InitRgb888(outfile, image_info, source_image_info)) {
		return;
	};

	image_decoder.Init(image);

	constexpr uint8_t BytesPerPixel = 3;
	row_buf.resize(static_cast<size_t>(image.width) *
	               static_cast<size_t>(BytesPerPixel));

	auto rows_to_write = image.height;
	while (rows_to_write--) {
		auto out = row_buf.begin();

		for (auto x = 0; x < image.width; ++x) {
			const auto pixel = image_decoder.GetNextRgb888Pixel();

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

