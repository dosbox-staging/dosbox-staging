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

#include "image_decoder.h"

#include "render.h"
#include "rgb888.h"

enum class ScalingMode { DoublingOnly, AspectRatioPreservingUpscale };

enum class PixelFormat { Indexed8, Rgb888 };

enum class PerAxisScaling { Integer, Fractional };

// Row-based image scaler. Currently, it can only upscale in two modes:
//
// - DoublingOnly: Simple width/height doubling, according to the
//   `double_width` and `double_height` image flags. If the input is paletted,
//   the output will be paletted too. Non-paletted images are converted to
//   RGB888.
//
// - AspectRatioPreservingUpscale: Always upscale by an auto-selected integral
//   scaling factor vertically so the resulting output height is around
//   1200px, then horizontally either by an integral or a fractional factor so
//   the input aspect ratio is preserved. "Sharp-bilinear" interpolation is
//   used horizontally to preserve the pixeled look as much as possible.
//   Paletted images are kept paletted only if both the horizontal and
//   vertical scaling factors are integral, otherwise the output is RGB888.
//
// Usage:
//
// - Call `Init()` with the input image and the desired scaling mode.
//
// - Use `GetOutputWidth()`, `GetOutputHeight()`, and `GetOutputPixelFormat()`
//   to query the output parameters decided by the upscaler.
//
// - Call `GetNextOutputRow()` repeatedly to get the upscaled output until the
//   end of the iterator is reached.
//
// - Call `Init()` again to process another image (no need to destruct &
//   re-create).
//
class ImageScaler {
public:
	ImageScaler()  = default;
	~ImageScaler() = default;

	void Init(const RenderedImage& image, const ScalingMode mode);

	std::vector<uint8_t>::const_iterator GetNextOutputRow();

	uint16_t GetOutputWidth() const;
	uint16_t GetOutputHeight() const;
	PixelFormat GetOutputPixelFormat() const;

	// prevent copying
	ImageScaler(const ImageScaler&) = delete;
	// prevent assignment
	ImageScaler& operator=(const ImageScaler&) = delete;

private:
	static constexpr uint8_t ComponentsPerRgbPixel = 3;

	void UpdateOutputParamsDoublingOnly();
	void UpdateOutputParamsUpscale();
	void LogParams();
	void AllocateBuffers();

	void DecodeNextRowToLinearRgb();

	void SetRowRepeat();

	void GenerateNextIntegerUpscaledOutputRow();
	void GenerateNextSharpUpscaledOutputRow();

	RenderedImage input        = {};
	ImageDecoder input_decoder = {};

	std::vector<float> linear_row_buf = {};

	struct {
		uint16_t width  = 0;
		uint16_t height = 0;

		float horiz_scale                 = 0;
		float one_per_horiz_scale         = 0;
		uint8_t vert_scale                = 0;
		PerAxisScaling horiz_scaling_mode = {};
		PerAxisScaling vert_scaling_mode  = {};

		PixelFormat pixel_format = {};

		uint16_t curr_row  = 0;
		uint8_t row_repeat = 0;

		std::vector<uint8_t> row_buf = {};
	} output = {};
};

#endif // DOSBOX_IMAGE_SCALER_H
