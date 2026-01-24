// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "image_decoder.h"

#include "utils/bgrx8888.h"
#include "utils/checks.h"

CHECK_NARROWING();

ImageDecoder::ImageDecoder(const RenderedImage& _image,
                           const int _row_skip_count, const int _pixel_skip_count)
        : image(_image),
          row_skip_count(_row_skip_count),
          pixel_skip_count(_pixel_skip_count)
{
	assert(_image.params.width > 0);
	assert(_image.params.height > 0);

	assert(_image.pitch >= _image.params.width);
	assert(_image.params.pixel_aspect_ratio.ToDouble() >= 0.0);
	assert(_image.image_data);

	if (_image.is_flipped_vertically) {
		in_row_start = _image.image_data +
		               (_image.params.height - 1) * _image.pitch;
	} else {
		in_row_start = _image.image_data;
	}
	in_pos = in_row_start;

	out_width = image.params.width / (pixel_skip_count + 1);
}

void ImageDecoder::GetNextRowAsIndexed8Pixels(std::vector<uint8_t>::iterator out)
{
	assert(image.is_paletted());

	for (auto x = 0; x < out_width; ++x) {
		const auto palette_index = *in_pos;

		*out = palette_index;

		++in_pos;
		++out;
	}

	AdvanceRow();
}

void ImageDecoder::GetNextRowAsBgrx32Pixels(std::vector<uint32_t>::iterator out)
{
	using enum PixelFormat;

	switch (image.params.pixel_format) {
	case Indexed8: GetBgrx32RowFromIndexed8(out); break;
	case RGB555_Packed16: GetBgrx32RowFromRgb555(out); break;
	case RGB565_Packed16: GetBgrx32RowFromRgb565(out); break;
	case BGR24_ByteArray: GetBgrx32RowFromBgr24(out); break;
	case BGRX32_ByteArray: GetBgrx32RowFromBgrx32(out); break;
	default: assertm(false, "Invalid PixelFormat value");
	}

	AdvanceRow();
}

void ImageDecoder::GetBgrx32RowFromIndexed8(std::vector<uint32_t>::iterator out)
{
	assert(image.is_paletted());

	for (auto x = 0; x < out_width; ++x) {
		const auto palette_index = *in_pos;
		const auto color         = image.palette[palette_index];

		*out = color;

		in_pos += (pixel_skip_count + 1);
		++out;
	}
}

void ImageDecoder::GetBgrx32RowFromRgb555(std::vector<uint32_t>::iterator out)
{
	assert(!image.is_paletted());

	for (auto x = 0; x < out_width; ++x) {
		const auto p     = host_readw(in_pos);
		const auto color = Rgb555(p).ToRgb888();

		*out = Bgrx8888{color.red, color.green, color.blue};

		in_pos += 2 * (pixel_skip_count + 1);
		++out;
	}
}

void ImageDecoder::GetBgrx32RowFromRgb565(std::vector<uint32_t>::iterator out)
{
	assert(!image.is_paletted());

	for (auto x = 0; x < out_width; ++x) {
		const auto p     = host_readw(in_pos);
		const auto color = Rgb565(p).ToRgb888();

		*out = Bgrx8888{color.red, color.green, color.blue};

		in_pos += 2 * (pixel_skip_count + 1);
		++out;
	}
}

void ImageDecoder::GetBgrx32RowFromBgr24(std::vector<uint32_t>::iterator out)
{
	assert(!image.is_paletted());

	for (auto x = 0; x < out_width; ++x) {
		const auto b = *(in_pos + 0);
		const auto g = *(in_pos + 1);
		const auto r = *(in_pos + 2);

		*out = Bgrx8888{r, g, b};

		in_pos += 3 * (pixel_skip_count + 1);
		++out;
	}
}

void ImageDecoder::GetBgrx32RowFromBgrx32(std::vector<uint32_t>::iterator out)
{
	assert(!image.is_paletted());

	for (auto x = 0; x < out_width; ++x) {
		const auto b = *(in_pos + 0);
		const auto g = *(in_pos + 1);
		const auto r = *(in_pos + 2);

		*out = Bgrx8888{r, g, b};

		in_pos += 4 * (pixel_skip_count + 1);
		++out;
	}
}

void ImageDecoder::AdvanceRow()
{
	auto rows_to_advance = row_skip_count + 1;

	if (image.is_flipped_vertically) {
		in_row_start -= image.pitch * rows_to_advance;
	} else {
		in_row_start += image.pitch * rows_to_advance;
	}

	in_pos = in_row_start;
}
