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

#ifndef DOSBOX_IMAGE_SCALER_H
#define DOSBOX_IMAGE_SCALER_H

#include <vector>

#include "render.h"
#include "rgb888.h"

enum class ScaleMode { Integer, Fractional };

enum class PixelFormat { Indexed8, Rgb888 };

class ImageScaler {
public:
	void Init(const RenderedImage_t params);

	std::vector<uint8_t>::const_iterator GetNextOutputRow();

	uint16_t GetOutputWidth() const;
	uint16_t GetOutputHeight() const;
	PixelFormat GetOutputPixelFormat() const;

private:
	static constexpr auto ComponentsPerRgbPixel = 3;

	void UpdateOutputParams();
	void AllocateBuffers();

	bool IsInputPaletted() const;

	Rgb888 DecodeNextPalettedInputPixel();
	Rgb888 DecodeNextRgbInputPixel();
	void DecodeNextRowToLinearRgb();

	void SetRowRepeat();
	void GenerateNextIntegerUpscaledOutputRow();
	void GenerateNextSharpUpscaledOutputRow();

	RenderedImage_t input = {};

	const uint8_t* input_curr_row_start = nullptr;
	const uint8_t* input_pos            = nullptr;

	std::vector<float> linear_row_buf = {};

	struct {
		uint16_t width  = 0;
		uint16_t height = 0;

		float horiz_scale          = 0;
		float one_per_horiz_scale  = 0;
		uint8_t vert_scale         = 0.0f;
		ScaleMode horiz_scale_mode = {};
		ScaleMode vert_scale_mode  = {};

		PixelFormat pixel_format = {};

		uint16_t curr_row  = 0;
		uint8_t row_repeat = 0;

		std::vector<uint8_t> row_buf = {};
	} output = {};
};

#endif

