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

#include "image_scaler.h"

#include "checks.h"

CHECK_NARROWING();

void ImageDecoder::Init(const RenderedImage& image, const uint8_t row_skip_count)
{
	assert(image.width > 0);
	assert(image.height > 0);

	assert(image.bits_per_pixel == 8 || image.bits_per_pixel == 15 ||
	       image.bits_per_pixel == 16 || image.bits_per_pixel == 24 ||
	       image.bits_per_pixel == 32);

	assert(image.pitch >= image.width);
	assert(image.pixel_aspect_ratio.ToDouble() >= 0.0);
	assert(image.image_data);

	if (image.is_paletted()) {
		assert(image.palette_data);
	}

	if (image.is_flipped_vertically) {
		curr_row_start = image.image_data + (image.height - 1) * image.pitch;
	} else {
		curr_row_start = image.image_data;
	}
	pos = curr_row_start;

	this->image          = image;
	this->row_skip_count = row_skip_count;
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
