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
#include "checks.h"
#include "math_utils.h"
#include "rgb.h"
#include "support.h"

CHECK_NARROWING();

// to avoid circular dependency
uint8_t get_double_scan_row_skip_count(const RenderedImage&, const VideoMode&);

void ImageScaler::Init(const RenderedImage& image, const VideoMode& video_mode)
{
	input = image;

	this->video_mode = video_mode;

	auto row_skip_count = get_double_scan_row_skip_count(image, video_mode);
	input_decoder.Init(image, row_skip_count);

	UpdateOutputParamsUpscale();

	assert(output.width >= video_mode.width);
	assert(output.height >= video_mode.height);
	assertm(output.horiz_scale >= 1.0f, "ImageScaler can currently only upscale");
	assertm(output.vert_scale >= 1, "ImageScaler can currently only upscale");

	// LogParams();

	AllocateBuffers();
}

static bool is_integer(const float f)
{
	return fabsf(f - roundf(f)) < 0.0001f;
}

void ImageScaler::UpdateOutputParamsUpscale()
{
	constexpr auto target_output_height = 1200;

	// Calculate initial integer vertical scaling factor so the resulting
	// output image height is roughly around 1200px.
	output.vert_scale = static_cast<uint8_t>(roundf(
	        static_cast<float>(target_output_height) / video_mode.height));

	output.vert_scaling_mode = PerAxisScaling::Integer;

	// Adjusting for a few special modes where the rendered width is twice the
	// video mode width:
	// - The Tandy/PCjr 160x200 is rendered as 320x200
	// - The Tandy 640x200 4-colour composite mode is rendered as 1280x200
	assert(input.width % video_mode.width == 0);
	const auto par_adjustment_factor = (input.width / video_mode.width);

	const auto pixel_aspect_ratio = video_mode.pixel_aspect_ratio /
	                                par_adjustment_factor;

	// Calculate horizontal scale factor, and potentially refine the results
	// by bumping up the vertical scale factor iteratively.
	for (;;) {
		auto horiz_scale_fract = pixel_aspect_ratio * output.vert_scale;

		output.horiz_scale = horiz_scale_fract.ToFloat();
		output.one_per_horiz_scale = horiz_scale_fract.Inverse().ToFloat();

		output.width = static_cast<uint16_t>(
		        roundf(input.width * output.horiz_scale));

		output.height = static_cast<uint16_t>(video_mode.height *
		                                      output.vert_scale);

		if (is_integer(output.horiz_scale)) {
			// Ensure the upscaled image is at least 1000px high for
			// 1:1 pixel aspect ratio images.
			if (output.height < 1000) {
				++output.vert_scale;
			} else {
				break;
			}
		} else {
			// Ensure fractional horizontal scale factors are
			// above 2.0, otherwise we'd get bad looking horizontal
			// blur.
			if (output.horiz_scale < 2.0f) {
				++output.vert_scale;
			} else {
				break;
			}
		}
	}

	if (is_integer(output.horiz_scale)) {
		output.horiz_scale = static_cast<float>(
		        static_cast<uint16_t>(output.horiz_scale));

		output.horiz_scaling_mode = PerAxisScaling::Integer;
	} else {
		output.horiz_scaling_mode = PerAxisScaling::Fractional;
	}

	// Determine pixel format
	const auto only_integer_scaling =
	        ((output.horiz_scaling_mode == PerAxisScaling::Integer) &&
	         (output.vert_scaling_mode == PerAxisScaling::Integer));

	if (only_integer_scaling && input.is_paletted()) {
		output.pixel_format = PixelFormat::Indexed8;
	} else {
		output.pixel_format = PixelFormat::Rgb888;
	}

	output.curr_row   = 0;
	output.row_repeat = 0;
}

void ImageScaler::LogParams()
{
	auto pixel_format_to_string = [](const PixelFormat pf) -> std::string {
		switch (pf) {
		case PixelFormat::Indexed8: return "Indexed8";
		case PixelFormat::Rgb888: return "RGB888";
		default: assert(false); return {};
		}
	};

	auto scale_mode_to_string = [](const PerAxisScaling s) -> std::string {
		switch (s) {
		case PerAxisScaling::Integer: return "Integer";
		case PerAxisScaling::Fractional: return "Fractional";
		default: assert(false); return {};
		}
	};

	LOG_MSG("ImageScaler params:\n"
	        "    input.width:                %10d\n"
	        "    input.height:               %10d\n"
	        "    input.double_width:         %10s\n"
	        "    input.double_height:        %10s\n"
	        "    input.PAR:                  1:%1.6f (%d:%d)\n"
	        "    input.bits_per_pixel:       %10d\n"
	        "    input.pitch:                %10d\n"
	        "    --------------------------------------\n"
	        "    video_mode.width:           %10d\n"
	        "    video_mode.height:          %10d\n"
	        "    video_mode.PAR:             1:%1.6f (%d:%d)\n"
	        "    --------------------------------------\n"
	        "    output.width:               %10d\n"
	        "    output.height:              %10d\n"
	        "    output.horiz_scale:         %10f\n"
	        "    output.vert_scale:          %10d\n"
	        "    output.horiz_scaling_mode:  %10s\n"
	        "    output.vert_scaling_mode:   %10s\n"
	        "    output.pixel_format:        %10s\n",
	        input.width,
	        input.height,
	        input.double_width ? "yes" : "no",
	        input.double_height ? "yes" : "no",
	        input.pixel_aspect_ratio.Inverse().ToDouble(),
	        static_cast<int32_t>(input.pixel_aspect_ratio.Num()),
	        static_cast<int32_t>(input.pixel_aspect_ratio.Denom()),
	        input.bits_per_pixel,
	        input.pitch,

	        video_mode.width,
	        video_mode.height,
	        video_mode.pixel_aspect_ratio.Inverse().ToDouble(),
	        static_cast<int32_t>(video_mode.pixel_aspect_ratio.Num()),
	        static_cast<int32_t>(video_mode.pixel_aspect_ratio.Denom()),

	        output.width,
	        output.height,
	        output.horiz_scale,
	        output.vert_scale,
	        scale_mode_to_string(output.horiz_scaling_mode).c_str(),
	        scale_mode_to_string(output.vert_scaling_mode).c_str(),
	        pixel_format_to_string(output.pixel_format).c_str());
}

void ImageScaler::AllocateBuffers()
{
	uint8_t bytes_per_pixel = {};
	switch (output.pixel_format) {
	case PixelFormat::Indexed8: bytes_per_pixel = 8; break;
	case PixelFormat::Rgb888: bytes_per_pixel = 24; break;
	default: assert(false);
	}

	output.row_buf.resize(static_cast<size_t>(output.width) *
	                      static_cast<size_t>(bytes_per_pixel));

	// Pad by 1 pixel at the end so we can handle the last pixel of the row
	// without branching (the interpolator operates on the current and the
	// next pixel).
	linear_row_buf.resize((input.width + 1u) * ComponentsPerRgbPixel);
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

void ImageScaler::DecodeNextRowToLinearRgb()
{
	auto out = linear_row_buf.begin();

	for (auto x = 0; x < input.width; ++x) {
		const auto pixel = input_decoder.GetNextPixelAsRgb888();

		*out++ = srgb8_to_linear_lut(pixel.red);
		*out++ = srgb8_to_linear_lut(pixel.green);
		*out++ = srgb8_to_linear_lut(pixel.blue);
	}

	input_decoder.AdvanceRow();
}

void ImageScaler::SetRowRepeat()
{
	// Optimisation: output row "vertical integer scale factor" number
	// of times instead of repeatedly processing it.
	if (output.vert_scaling_mode == PerAxisScaling::Integer) {
		output.row_repeat = static_cast<uint8_t>(output.vert_scale - 1);
	} else {
		output.row_repeat = 1;
	}
}

void ImageScaler::GenerateNextIntegerUpscaledOutputRow()
{
	auto out = output.row_buf.begin();

	for (auto x = 0; x < input.width; ++x) {
		auto pixels_to_write = static_cast<uint32_t>(output.horiz_scale);

		if (input.is_paletted()) {
			const auto pixel = input_decoder.GetNextIndexed8Pixel();
			while (pixels_to_write--) {
				*out++ = pixel;
			}
		} else {
			const auto pixel = input_decoder.GetNextPixelAsRgb888();
			while (pixels_to_write--) {
				*out++ = pixel.red;
				*out++ = pixel.green;
				*out++ = pixel.blue;
			}
		}
	}

	input_decoder.AdvanceRow();
	SetRowRepeat();
}

void ImageScaler::GenerateNextSharpUpscaledOutputRow()
{
	auto row_start = linear_row_buf.begin();
	auto out       = output.row_buf.begin();

	for (auto x = 0; x < output.width; ++x) {
		const auto x0 = static_cast<float>(x) * output.one_per_horiz_scale;
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
		// and the next pixel so that the interpolation "band" is one
		// pixel wide at most at the edges of the pixel.
		const auto x1 = x0 + output.one_per_horiz_scale;
		const auto t  = std::max(x1 - (floor_x0 + 1.0f), 0.0f) *
		               output.horiz_scale;

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
		if (output.horiz_scaling_mode == PerAxisScaling::Integer &&
		    output.vert_scaling_mode == PerAxisScaling::Integer) {
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
