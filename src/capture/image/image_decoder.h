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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_IMAGE_DECODER_H
#define DOSBOX_IMAGE_DECODER_H

#include <cassert>
#include <vector>

#include "byteorder.h"
#include "checks.h"
#include "render.h"
#include "rgb555.h"
#include "rgb565.h"
#include "rgb888.h"
#include "support.h"
#include "vga.h"

class ImageDecoder {
public:
	ImageDecoder()  = default;
	~ImageDecoder() = default;

	// Set `row_skip_count` to 1 to de-double-scan an image with "baked in"
	// double-scanning.
	void Init(const RenderedImage& image, const uint8_t row_skip_count);

	inline uint8_t GetNextIndexed8Pixel()
	{
		assert(image.is_paletted());
		assert(pos - curr_row_start < image.pitch);
		return *pos++;
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

	uint8_t row_skip_count = 0;

	const uint8_t* curr_row_start = nullptr;
	const uint8_t* pos            = nullptr;

	inline void IncrementPos()
	{
		switch (image.params.pixel_format) {
		case PixelFormat::Indexed8: ++pos; break;
		case PixelFormat::BGR555:
		case PixelFormat::BGR565: pos += 2; break;
		case PixelFormat::BGR888: pos += 3; break;
		case PixelFormat::BGRX8888: pos += 4; break;
		default: assertm(false, "Invalid pixel_format value");
		}
	}

	inline Rgb888 GetNextPalettedPixelAsRgb888()
	{
		const auto pal_index = *pos;

		const auto r = image.palette_data[pal_index * 4 + 0];
		const auto g = image.palette_data[pal_index * 4 + 1];
		const auto b = image.palette_data[pal_index * 4 + 2];

		IncrementPos();

		return {r, g, b};
	}

	inline Rgb888 GetNextRgbPixelAsRgb888()
	{
		Rgb888 pixel = {};

		switch (image.params.pixel_format) {
		case PixelFormat::BGR555: {
			const auto p = host_to_le(
			        *reinterpret_cast<const uint16_t*>(pos));
			pixel = Rgb555(p).ToRgb888();
		} break;

		case PixelFormat::BGR565: {
			const auto p = host_to_le(
			        *reinterpret_cast<const uint16_t*>(pos));
			pixel = Rgb565(p).ToRgb888();
		} break;

		case PixelFormat::BGR888:
		case PixelFormat::BGRX8888: {
			const auto b = *(pos + 0);
			const auto g = *(pos + 1);
			const auto r = *(pos + 2);

			pixel = {r, g, b};
		} break;
		default: assertm(false, "Invalid pixel_format value");
		}

		IncrementPos();

		return pixel;
	}
};

#endif // DOSBOX_IMAGE_DECODER_H
