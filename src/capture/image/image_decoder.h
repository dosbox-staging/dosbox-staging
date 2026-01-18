// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_IMAGE_DECODER_H
#define DOSBOX_IMAGE_DECODER_H

#include <cassert>
#include <vector>

#include "hardware/video/vga.h"
#include "misc/rendered_image.h"
#include "misc/support.h"
#include "utils/byteorder.h"
#include "utils/checks.h"
#include "utils/mem_host.h"
#include "utils/rgb555.h"
#include "utils/rgb565.h"
#include "utils/rgb888.h"

class ImageDecoder {
public:
	ImageDecoder()  = default;
	~ImageDecoder() = default;

	// Set `row_skip_count` to 1 reconstruct the raw image when the input
	// has "baked-in" double scanning.
	//
	// Set `pixel_skip_count` to 1 reconstruct the raw image when the input
	// has "baked-in" pixel doubling.
	//
	void Init(const RenderedImage& image, const int row_skip_count,
	          const int pixel_skip_count);

	inline uint8_t GetNextIndexed8Pixel()
	{
		assert(image.is_paletted());
		assert(pos - curr_row_start < image.pitch);

		const auto pal_index = *pos;

		IncrementPos();

		return pal_index;
	}

	inline Rgb888 GetNextPixelAsRgb888()
	{
		assert(pos - curr_row_start < image.pitch);

		if (image.is_paletted()) {
			return GetNextPalettedPixelAsRgb888();
		} else {
			return GetNextRgbPixelAsRgb888();
		}
	}

	void AdvanceRow();

	// prevent copying
	ImageDecoder(const ImageDecoder&) = delete;
	// prevent assignment
	ImageDecoder& operator=(const ImageDecoder&) = delete;

private:
	RenderedImage image = {};

	int row_skip_count   = 0;
	int pixel_skip_count = 0;

	const uint8_t* curr_row_start = nullptr;
	const uint8_t* pos            = nullptr;

	inline void IncrementPos()
	{
		for (auto i = 0; i <= pixel_skip_count; ++i) {
			switch (image.params.pixel_format) {
			case PixelFormat::Indexed8: ++pos; break;
			case PixelFormat::RGB555_Packed16:
			case PixelFormat::RGB565_Packed16: pos += 2; break;
			case PixelFormat::BGR24_ByteArray: pos += 3; break;
			case PixelFormat::BGRX32_ByteArray: pos += 4; break;
			default: assertm(false, "Invalid PixelFormat value");
			}
		}
	}

	inline Rgb888 GetNextPalettedPixelAsRgb888()
	{
		const auto pal_index = *pos;
		const auto color     = image.palette[pal_index];

		IncrementPos();

		return color;
	}

	inline Rgb888 GetNextRgbPixelAsRgb888()
	{
		Rgb888 pixel = {};

		switch (image.params.pixel_format) {
		case PixelFormat::RGB555_Packed16: {
			const auto p = host_readw(pos);

			pixel = Rgb555(p).ToRgb888();
		} break;

		case PixelFormat::RGB565_Packed16: {
			const auto p = host_readw(pos);

			pixel = Rgb565(p).ToRgb888();
		} break;

		case PixelFormat::BGR24_ByteArray:
		case PixelFormat::BGRX32_ByteArray: {
			const auto b = *(pos + 0);
			const auto g = *(pos + 1);
			const auto r = *(pos + 2);

			pixel = {r, g, b};
		} break;

		default: assertm(false, "Invalid PixelFormat value");
		}

		IncrementPos();

		return pixel;
	}
};

#endif // DOSBOX_IMAGE_DECODER_H
