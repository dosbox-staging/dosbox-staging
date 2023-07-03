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

class ImageDecoder {
public:
	ImageDecoder()  = default;
	~ImageDecoder() = default;

	void Init(const RenderedImage& image);

	inline uint8_t GetNextIndexed8Pixel()
	{
		assert(pos - curr_row_start < image.pitch);
		return *pos++;
	}

	inline Rgb888 GetNextRgb888Pixel()
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

	const uint8_t* curr_row_start = nullptr;
	const uint8_t* pos            = nullptr;

	inline Rgb888 GetNextPalettedPixelAsRgb888()
	{
		assert(pos - curr_row_start < image.pitch);
		const auto pal_index = *pos++;

		const auto r = image.palette_data[pal_index * 4 + 0];
		const auto g = image.palette_data[pal_index * 4 + 1];
		const auto b = image.palette_data[pal_index * 4 + 2];

		return {r, g, b};
	}

	inline Rgb888 GetNextRgbPixelAsRgb888()
	{
		assert(pos - curr_row_start < image.pitch);

		Rgb888 pixel = {};

		switch (image.bits_per_pixel) {
		case 15: { // BGR555
			const auto p = host_to_le(
			        *reinterpret_cast<const uint16_t*>(pos));
			pixel = Rgb555(p).ToRgb888();
			pos += 2;
		} break;

		case 16: { // BGR565
			const auto p = host_to_le(
			        *reinterpret_cast<const uint16_t*>(pos));
			pixel = Rgb565(p).ToRgb888();
			pos += 2;
		} break;

		case 24:   // BGR888
		case 32: { // XBGR8888
			const auto b = *(pos++);
			const auto g = *(pos++);
			const auto r = *(pos++);

			pixel = {r, g, b};
		} break;
		default: assertm(false, "Invalid bits_per_pixel value");
		}

		if (image.bits_per_pixel == 32) {
			// skip padding byte
			++pos;
		}

		return pixel;
	}
};

#endif // DOSBOX_IMAGE_DECODER_H
