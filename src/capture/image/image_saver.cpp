// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "image_saver.h"

#include "capture/capture.h"
#include "hardware/video/vga.h"
#include "misc/image_decoder.h"
#include "misc/support.h"
#include "png_writer.h"
#include "utils/bgrx8888.h"
#include "utils/checks.h"

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
		LOG_WARNING(
		        "CAPTURE: Cannot capture image while image capturer "
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
	default: assertm(false, "Invalid CaptureImageType value"); return {};
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

void ImageSaver::SaveRawImage(const RenderedImage& image)
{
	PngWriter png_writer = {};

	const auto& src = image.params;

	// To reconstruct the raw image, we must skip every second row when
	// dealing with "baked-in" double scanning.
	const auto row_skip_count = (src.rendered_double_scan ? 1 : 0);

	// To reconstruct the raw image, we must skip every second pixel when
	// dealing with "baked-in" pixel doubling.
	const auto pixel_skip_count = (src.rendered_pixel_doubling ? 1 : 0);

	const auto output_width  = src.width / (pixel_skip_count + 1);
	const auto output_height = src.height / (row_skip_count + 1);

	ImageDecoder image_decoder(image, row_skip_count, pixel_skip_count);

	// Write the pixel aspect ratio of the video mode into the PNG pHYs
	// chunk for raw images.
	const auto pixel_aspect_ratio = src.video_mode.pixel_aspect_ratio;

	if (image.is_paletted()) {
		if (!png_writer.InitIndexed8(outfile,
		                             output_width,
		                             output_height,
		                             pixel_aspect_ratio,
		                             src.video_mode,
		                             image.palette)) {
			return;
		}
	} else {
		if (!png_writer.InitRgb888(outfile,
		                           output_width,
		                           output_height,
		                           pixel_aspect_ratio,
		                           src.video_mode)) {
			return;
		}
	}

	constexpr auto MaxBytesPerPixel = 3;
	row_output_buf.resize(static_cast<size_t>(output_width) *
	               static_cast<size_t>(MaxBytesPerPixel));

	auto rows_to_write = output_height;
	while (rows_to_write--) {
		auto out = row_output_buf.begin();

		if (image.is_paletted()) {
			image_decoder.GetNextRowAsIndexed8Pixels(out);

		} else {
			row_decode_buf.resize(output_width);
			image_decoder.GetNextRowAsBgrx32Pixels(
			        row_decode_buf.begin());

			for (const auto pixel : row_decode_buf) {
				const auto color = Bgrx8888(pixel);

				*(out + 0) = color.Red();
				*(out + 1) = color.Green();
				*(out + 2) = color.Blue();

				out += 3;
			}
		}
		png_writer.WriteRow(row_output_buf.begin());
	}
}

static constexpr auto SquarePixelAspectRatio = Fraction{1};

void ImageSaver::SaveUpscaledImage(const RenderedImage& image)
{
	PngWriter png_writer = {};

	image_scaler.Init(image);

	// Always write 1:1 pixel aspect ratio into the PNG pHYs chunk for
	// upscaled images as the "non-squaredness" is "baked into" the image
	// data.
	switch (image_scaler.GetOutputPixelFormat()) {
	case OutputPixelFormat::Indexed8:
		if (!png_writer.InitIndexed8(outfile,
		                             image_scaler.GetOutputWidth(),
		                             image_scaler.GetOutputHeight(),
		                             SquarePixelAspectRatio,
		                             image.params.video_mode,
		                             image.palette)) {
			return;
		}
		break;

	case OutputPixelFormat::Rgb888:
		if (!png_writer.InitRgb888(outfile,
		                           image_scaler.GetOutputWidth(),
		                           image_scaler.GetOutputHeight(),
		                           SquarePixelAspectRatio,
		                           image.params.video_mode)) {
			return;
		}
		break;
	}

	auto rows_to_write = image_scaler.GetOutputHeight();
	while (rows_to_write--) {
		auto row = image_scaler.GetNextOutputRow();
		png_writer.WriteRow(row);
	}
}

void ImageSaver::SaveRenderedImage(const RenderedImage& image)
{
	PngWriter png_writer = {};

	const auto& src = image.params;

	// Always write 1:1 pixel aspect ratio into the PNG pHYs chunk for
	// rendered images as the "non-squaredness" is "baked into" the image
	// data.
	if (!png_writer.InitRgb888(outfile,
	                           check_cast<uint16_t>(src.width),
	                           check_cast<uint16_t>(src.height),
	                           SquarePixelAspectRatio,
	                           src.video_mode)) {
		return;
	}

	// We always write the final rendered image displayed on the host
	// monitor as-is.
	const auto row_skip_count   = 0;
	const auto pixel_skip_count = 0;

	ImageDecoder image_decoder(image, row_skip_count, pixel_skip_count);

	constexpr auto BytesPerPixel = 3;
	row_output_buf.resize(static_cast<size_t>(src.width) *
	               static_cast<size_t>(BytesPerPixel));

	auto rows_to_write = src.height;
	while (rows_to_write--) {
		auto out = row_output_buf.begin();

		row_decode_buf.resize(src.width);
		image_decoder.GetNextRowAsBgrx32Pixels(row_decode_buf.begin());

		for (const auto pixel : row_decode_buf) {
			const auto color = Bgrx8888(pixel);

			*(out + 0) = color.Red();
			*(out + 1) = color.Green();
			*(out + 2) = color.Blue();

			out += 3;
		}

		png_writer.WriteRow(row_output_buf.begin());
	}
}

void ImageSaver::CloseOutFile()
{
	if (outfile) {
		fclose(outfile);
		outfile = nullptr;
	}
}

