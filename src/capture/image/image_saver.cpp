// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "image_saver.h"

#include "capture/capture.h"
#include "misc/support.h"
#include "png_writer.h"
#include "utils/checks.h"

CHECK_NARROWING();

// #define DEBUG_IMAGE_SCALER

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

static void write_upscaled_png(FILE* outfile, PngWriter& png_writer,
                               ImageScaler& image_scaler, const int width,
                               const int height, const Fraction& pixel_aspect_ratio,
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
	}

	auto rows_to_write = image_scaler.GetOutputHeight();
	while (rows_to_write--) {
		auto row = image_scaler.GetNextOutputRow();
		png_writer.WriteRow(row);
	}
}

void ImageSaver::LogRawCaptureParams([[maybe_unused]] const RenderedImage& image,
                                     [[maybe_unused]] const int row_skip_count,
                                     [[maybe_unused]] const int pixel_skip_count,
                                     [[maybe_unused]] const int output_width,
                                     [[maybe_unused]] const int output_height)
{
#ifdef DEBUG_IMAGE_SCALER
	const auto& src = image.params;

	LOG_DEBUG(
	        "ImageSave::SaveRawImage params:\n"
	        "    input.width:                %10d\n"
	        "    input.height:               %10d\n"
	        "    input.width_doubling:       %10s\n"
	        "    input.height_doubling:      %10s\n"
	        "    input.PAR:                  1:%1.6f (%d:%d)\n"
	        "    input.pixel_format:         %10s\n"
	        "    input.pitch:                %10d\n"
	        "    --------------------------------------\n"
	        "    video_mode.width:           %10d\n"
	        "    video_mode.height:          %10d\n"
	        "    video_mode.PAR:             1:%1.6f (%d:%d)\n"
	        "    --------------------------------------\n"
	        "    row_skip_count:             %10d\n"
	        "    pixel_skip_count:           %10d\n"
	        "    output_width:               %10d\n"
	        "    output_height:              %10d\n",
	        src.width,
	        src.height,
	        to_string(src.width_doubling),
	        to_string(src.height_doubling),
	        src.pixel_aspect_ratio.Inverse().ToDouble(),
	        static_cast<int32_t>(src.pixel_aspect_ratio.Num()),
	        static_cast<int32_t>(src.pixel_aspect_ratio.Denom()),
	        to_string(src.pixel_format),
	        image.pitch,

	        src.video_mode.width,
	        src.video_mode.height,
	        src.video_mode.pixel_aspect_ratio.Inverse().ToDouble(),
	        static_cast<int32_t>(src.video_mode.pixel_aspect_ratio.Num()),
	        static_cast<int32_t>(src.video_mode.pixel_aspect_ratio.Denom()),

	        row_skip_count,
	        pixel_skip_count,
	        output_width,
	        output_height);
#endif
}

void ImageSaver::SaveRawImage(const RenderedImage& image)
{
	PngWriter png_writer = {};

	const auto& src = image.params;

	// To reconstruct the raw image, we must skip every second row
	// when dealing with double scanning.
	const auto row_skip_count = (src.is_height_doubled() ? 1 : 0);

	// To reconstruct the raw image, we must skip every second pixel
	// when dealing with pixel doubling.
	const auto pixel_skip_count = (src.is_width_doubled() ? 1 : 0);

	const auto output_width  = src.width / (pixel_skip_count + 1);
	const auto output_height = src.height / (row_skip_count + 1);

	LogRawCaptureParams(image, row_skip_count, pixel_skip_count, output_width, output_height);

	image_decoder.Init(image, row_skip_count, pixel_skip_count);

	// Write the pixel aspect ratio of the video mode into the PNG pHYs
	// chunk for raw images.
	const auto pixel_aspect_ratio = src.video_mode.pixel_aspect_ratio;

	if (!png_writer.InitRgb888(outfile,
	                           output_width,
	                           output_height,
	                           pixel_aspect_ratio,
	                           src.video_mode)) {
		return;
	}

	constexpr auto MaxBytesPerPixel = 3;
	row_buf.resize(static_cast<size_t>(output_width) *
	               static_cast<size_t>(MaxBytesPerPixel));

	auto rows_to_write = output_height;
	while (rows_to_write--) {
		auto out = row_buf.begin();

		auto pixels_to_write = output_width;
		while (pixels_to_write--) {
			const auto pixel = image_decoder.GetNextPixel();

			*out++ = pixel.red;
			*out++ = pixel.green;
			*out++ = pixel.blue;
		}

		png_writer.WriteRow(row_buf.begin());
		image_decoder.AdvanceRow();
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
	write_upscaled_png(outfile,
	                   png_writer,
	                   image_scaler,
	                   image_scaler.GetOutputWidth(),
	                   image_scaler.GetOutputHeight(),
	                   SquarePixelAspectRatio,
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
	image_decoder.Init(image, row_skip_count, pixel_skip_count);

	constexpr auto BytesPerPixel = 3;
	row_buf.resize(static_cast<size_t>(src.width) *
	               static_cast<size_t>(BytesPerPixel));

	auto rows_to_write = src.height;
	while (rows_to_write--) {
		auto out = row_buf.begin();

		auto pixels_to_write = src.width;
		while (pixels_to_write--) {
			const auto pixel = image_decoder.GetNextPixel();

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

