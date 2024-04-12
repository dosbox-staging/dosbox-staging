/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2024  The DOSBox Staging Team
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

void ImageDecoder::Init(const RenderedImage& _image, const uint8_t _row_skip_count,
                        const uint8_t _pixel_skip_count)
{
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
