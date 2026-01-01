// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "image_scaler.h"

#include "utils/checks.h"

CHECK_NARROWING();

void ImageDecoder::Init(const RenderedImage& _image, const int _row_skip_count,
                        const int _pixel_skip_count)
{
	// For raw and upscaled captures we're getting pixel data in BGRX32
	// format from the scalers.
	//
	// For rendered captures, we're retrieving the pixel data from the
	// framebuffer in BGR24 format
	//
	// TODO Is there any performance advantage in doing this? Surely GPUs
	// operate on packed 32-bit RGBA data internally anyway.
	//
	assert(_image.params.pixel_format == PixelFormat::BGR24_ByteArray ||
	       _image.params.pixel_format == PixelFormat::BGRX32_ByteArray);

	assert(_image.params.width > 0);
	assert(_image.params.height > 0);

	assert(_image.pitch >= _image.params.width);
	assert(_image.params.pixel_aspect_ratio.ToDouble() >= 0.0);
	assert(_image.image_data);

	if (_image.is_paletted()) {
		assert(_image.palette_data);
	}

	if (_image.is_flipped_vertically) {
		curr_row_start = _image.image_data +
		                 (_image.params.height - 1) * _image.pitch;
	} else {
		curr_row_start = _image.image_data;
	}
	pos = curr_row_start;

	image            = _image;
	row_skip_count   = _row_skip_count;
	pixel_skip_count = _pixel_skip_count;
}

void ImageDecoder::AdvanceRow()
{
	auto rows_to_advance = row_skip_count + 1;

	if (image.is_flipped_vertically) {
		curr_row_start -= image.pitch * rows_to_advance;
	} else {
		curr_row_start += image.pitch * rows_to_advance;
	}

	pos = curr_row_start;
}
