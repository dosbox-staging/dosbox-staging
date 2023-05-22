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

#include <cmath>

#include "byteorder.h"
#include "math_utils.h"
#include "rgb.h"
#include "rgb555.h"
#include "rgb565.h"

void ImageScaler::Init(const RenderedImage image)
{
	assert(image.width > 0);
	assert(image.height > 0);

	assert(image.bits_per_pixel == 8 || image.bits_per_pixel == 15 ||
	       image.bits_per_pixel == 16 || image.bits_per_pixel == 24 ||
	       image.bits_per_pixel == 32);

	assert(image.pitch >= image.width);
	assert(image.one_per_pixel_aspect_ratio >= 0.0);
	assert(image.image_data);

	input = image;

	if (IsInputPaletted()) {
		assert(input.palette_data);
	}

	UpdateOutputParams();
	AllocateBuffers();

	input_curr_row_start = input.image_data;
}

static bool is_integer(float f)
{
	return fabs(f - round(f)) < 0.0001;
}

void ImageScaler::UpdateOutputParams()
{
	constexpr auto target_output_height = 1200;

	// Slight fudge factor to push 350-line modes into 4x vertical scaling
	constexpr auto fudge_offset = 0.1;

	// Calculate integer vertical scaling factor so the
	// resulting output image height is roughly around
	// 1200px. See TODO table for further details.
	output.vert_scale = static_cast<uint8_t>(
	        round(static_cast<float>(target_output_height) / input.height +
	              fudge_offset));

	output.vert_scale_mode = ScaleMode::Integer;

	// Determine horizontal scaling factor & scaling mode
	output.horiz_scale = output.vert_scale / input.one_per_pixel_aspect_ratio;

	output.one_per_horiz_scale = input.one_per_pixel_aspect_ratio /
	                             output.vert_scale;

	if (input.double_width) {
		output.horiz_scale *= 2;
	}
	if (input.double_height) {
		output.vert_scale *= 2;
	}

	assert(output.vert_scale >= 1.0);
	assert(output.horiz_scale >= 1.0);

	if (is_integer(output.horiz_scale)) {
		output.horiz_scale = static_cast<float>(
		        static_cast<uint16_t>(output.horiz_scale));

		output.horiz_scale_mode = ScaleMode::Integer;
	} else {
		output.horiz_scale_mode = ScaleMode::Fractional;
	}

	// Determine scaled output dimensions, taking the double
	// width/height input flags into account
	output.width  = input.width * output.horiz_scale;
	output.height = input.height * output.vert_scale;

	assert(output.width >= input.width);
	assert(output.height >= input.height);

	// Determine pixel format
	const auto only_integer_scaling = output.horiz_scale_mode ==
	                                          ScaleMode::Integer &&
	                                  output.vert_scale_mode == ScaleMode::Integer;

	if (only_integer_scaling && IsInputPaletted()) {
		output.pixel_format = PixelFormat::Indexed8;
	} else {
		output.pixel_format = PixelFormat::Rgb888;
	}

	output.curr_row   = 0;
	output.row_repeat = 0;

	LOG_MSG("ImageScaler: params:\n"
	        "    input.width:         %8d\n"
	        "    input.height:        %8d\n"
	        "    input.double_width:  %8s\n"
	        "    input.double_height: %8s\n"
	        "    input.one_per_par:   %8f\n"
	        "    input.bits_per_pixel:%8d\n"
	        "    input.pitch:         %8d\n"
	        "    ---------------------------\n"
	        "    output.width:            %8d\n"
	        "    output.height:           %8d\n"
	        "    output.vert_scale:       %8d\n"
	        "    output.horiz_scale:      %8f\n"
	        "    output.horiz_scale_mode: %8d\n"
	        "    output.vert_scale_mode:  %8d\n"
	        "    output.pixel_format:     %8d\n",
	        input.width,
	        input.height,
	        input.double_width ? "yes" : "no",
	        input.double_height ? "yes" : "no",
	        input.one_per_pixel_aspect_ratio,
	        input.bits_per_pixel,
	        input.pitch,
	        output.width,
	        output.height,
	        output.vert_scale,
	        output.horiz_scale,
	        static_cast<uint8_t>(output.horiz_scale_mode),
	        static_cast<uint8_t>(output.vert_scale_mode),
	        static_cast<uint8_t>(output.pixel_format));
}

void ImageScaler::AllocateBuffers()
{
	uint8_t bytes_per_pixel = {};
	switch (output.pixel_format) {
	case PixelFormat::Indexed8: bytes_per_pixel = 8;
	case PixelFormat::Rgb888: bytes_per_pixel = 24;
	}

	output.row_buf.resize(output.width * bytes_per_pixel);

	// Pad by 1 pixel at the end so we can handle the last pixel of the row
	// without branching (the interpolator operates on the current and the
	// next pixel).
	linear_row_buf.resize((input.width + 1) * ComponentsPerRgbPixel);
}

inline bool ImageScaler::IsInputPaletted() const
{
	return (input.bits_per_pixel == 8);
}

uint16_t ImageScaler::GetOutputWidth() const
{
	return output.width;
}

uint16_t ImageScaler::GetOutputHeight() const
{
	return output.height;
}

PixelFormat ImageScaler::GetOutputPixelFormat() const
{
	return output.pixel_format;
}

inline Rgb888 ImageScaler::DecodeNextPalettedInputPixel()
{
	assert(IsInputPaletted());

	const auto pal_index = *input_pos++;

	const auto r = input.palette_data[pal_index * 4 + 0];
	const auto g = input.palette_data[pal_index * 4 + 1];
	const auto b = input.palette_data[pal_index * 4 + 2];

	return {r, g, b};
}

inline Rgb888 ImageScaler::DecodeNextRgbInputPixel()
{
	Rgb888 pixel = {};

	switch (input.bits_per_pixel) {
	case 15: { // BGR555
		const auto p = host_to_le(
		        *reinterpret_cast<const uint16_t*>(input_pos));
		pixel = Rgb555(p).ToRgb888();
		input_pos += 2;
	} break;

	case 16: { // BGR565
		const auto p = host_to_le(
		        *reinterpret_cast<const uint16_t*>(input_pos));
		pixel = Rgb565(p).ToRgb888();
		input_pos += 2;
	} break;

	case 24:   // BGR888
	case 32: { // XBGR8888
		const auto b = *(input_pos++);
		const auto g = *(input_pos++);
		const auto r = *(input_pos++);

		pixel = {r, g, b};
	} break;
	}

	if (input.bits_per_pixel == 32) {
		// skip padding byte
		++input_pos;
	}

	return pixel;
}

void ImageScaler::DecodeNextRowToLinearRgb()
{
	input_pos = input_curr_row_start;
	auto out  = linear_row_buf.begin();

	for (auto x = 0; x < input.width; ++x) {
		const auto pixel = IsInputPaletted()
		                         ? DecodeNextPalettedInputPixel()
		                         : DecodeNextRgbInputPixel();

		*out++ = srgb8_to_linear_lut(pixel.red);
		*out++ = srgb8_to_linear_lut(pixel.green);
		*out++ = srgb8_to_linear_lut(pixel.blue);
	}

	input_curr_row_start += input.pitch;
}

void ImageScaler::SetRowRepeat()
{
	// Optimisation: output row "vertical integer scale factor" number
	// of times instead of repeatedly processing it.
	if (output.vert_scale_mode == ScaleMode::Integer) {
		output.row_repeat = output.vert_scale - 1;
	} else {
		output.row_repeat = 1;
	}
}

void ImageScaler::GenerateNextIntegerUpscaledOutputRow()
{
	input_pos = input_curr_row_start;
	auto out  = output.row_buf.begin();

	for (auto x = 0; x < input.width; ++x) {
		auto pixels_to_write = output.horiz_scale;

		if (IsInputPaletted()) {
			const auto pixel = *input_pos++;
			while (pixels_to_write--) {
				*out++ = pixel;
			}
		} else {
			const auto pixel = DecodeNextRgbInputPixel();
			while (pixels_to_write--) {
				*out++ = pixel.red;
				*out++ = pixel.green;
				*out++ = pixel.blue;
			}
		}
	}

	input_curr_row_start += input.pitch;

	SetRowRepeat();
}

void ImageScaler::GenerateNextSharpUpscaledOutputRow()
{
	auto row_start = linear_row_buf.begin();
	auto out       = output.row_buf.begin();

	for (auto x = 0; x < output.width; ++x) {
		const auto x0       = x * output.one_per_horiz_scale;
		const auto floor_x0 = static_cast<uint16_t>(x0);
		assert(floor_x0 < input.width);

		const auto row_offs = floor_x0 * ComponentsPerRgbPixel;
		auto pixel_addr     = row_start + row_offs;

		// Current pixel
		const auto r0 = *pixel_addr++;
		const auto g0 = *pixel_addr++;
		const auto b0 = *pixel_addr++;

		// Next horizontal pixel
		const auto r1 = *pixel_addr++;
		const auto g1 = *pixel_addr++;
		const auto b1 = *pixel_addr++;

		// Calculate linear interpolation factor `t` between the current
		// and the next pixel so that the interpolation "band" is one pixel
		// wide at the edges of the pixel.
		const auto x1 = x0 + output.one_per_horiz_scale;
		const auto t = std::max(x1 - (floor_x0 + 1), 0.0f) * output.horiz_scale;

		const auto out_r = lerp(r0, r1, t);
		const auto out_g = lerp(g0, g1, t);
		const auto out_b = lerp(b0, b1, t);

		*out++ = linear_to_srgb8_lut(out_r);
		*out++ = linear_to_srgb8_lut(out_g);
		*out++ = linear_to_srgb8_lut(out_b);
	}

	SetRowRepeat();
}

std::vector<uint8_t>::const_iterator ImageScaler::GetNextOutputRow()
{
	if (output.curr_row >= output.height) {
		return output.row_buf.end();
	}

	if (output.row_repeat == 0) {
		if (output.horiz_scale_mode == ScaleMode::Integer &&
		    output.vert_scale_mode == ScaleMode::Integer) {
			GenerateNextIntegerUpscaledOutputRow();
		} else {
			DecodeNextRowToLinearRgb();
			GenerateNextSharpUpscaledOutputRow();
		}
	} else {
		--output.row_repeat;
	}

	++output.curr_row;

	return output.row_buf.begin();
}

