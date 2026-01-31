// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "image_scaler.h"

#include <cmath>

#include "hardware/video/vga.h"
#include "misc/support.h"
#include "utils/byteorder.h"
#include "utils/checks.h"
#include "utils/math_utils.h"
#include "utils/rgb.h"
#include "utils/rgb888.h"

CHECK_NARROWING();

// #define DEBUG_IMAGE_SCALER

void ImageScaler::Init(const RenderedImage& image)
{
	input = image;

	// To reconstruct the raw image, we must skip every second row when
	// dealing with "baked-in" double scanning. "De-double-scanning" VGA
	// images has the beneficial side effect that we can use finer vertical
	// integer scaling steps, so it's worthwhile doing it.
	const auto row_skip_count = (image.params.rendered_double_scan ? 1 : 0);

	// "Baked-in" pixel doubling is only used for the 160x200 16-colour
	// Tandy/PCjr modes. We wouldn't gain anything by reconstructing the raw
	// 160-pixel-wide image when upscaling, so we'll just leave it be.
	const auto pixel_skip_count = 0;

	input_decoder.Init(image, row_skip_count, pixel_skip_count);

	UpdateOutputParamsUpscale();

	assert(output.width >= image.params.video_mode.width);
	assert(output.height >= image.params.video_mode.height);
	assertm(output.horiz_scale >= 1.0f, "ImageScaler can currently only upscale");
	assertm(output.vert_scale >= 1, "ImageScaler can currently only upscale");

	LogParams();

	AllocateBuffers();
}

static bool is_integer(const float f)
{
	return fabsf(f - roundf(f)) < 0.0001f;
}

void ImageScaler::UpdateOutputParamsUpscale()
{
	constexpr auto TargetOutputHeight = 1200;

	const auto& video_mode = input.params.video_mode;

	// Calculate initial integer vertical scaling factor so the resulting
	// output image height is roughly around 1200px.
	output.vert_scale = iroundf(static_cast<float>(TargetOutputHeight) /
	                            static_cast<float>(video_mode.height));

	output.vert_scaling_mode = PerAxisScaling::Integer;

	// Adjusting for a few special modes where the rendered width is twice
	// the video mode width:
	// - The Tandy/PCjr 160x200 is rendered as 320x200
	// - The Tandy 640x200 4-colour composite mode is rendered as 1280x200
	assert(input.params.width % video_mode.width == 0);
	const auto par_adjustment_factor = (input.params.width / video_mode.width);

	const auto pixel_aspect_ratio = video_mode.pixel_aspect_ratio /
	                                par_adjustment_factor;

	// Calculate horizontal scale factor, and potentially refine the results
	// by bumping up the vertical scale factor iteratively.
	for (;;) {
		auto horiz_scale_fract = pixel_aspect_ratio * output.vert_scale;

		output.horiz_scale = horiz_scale_fract.ToFloat();
		output.one_per_horiz_scale = horiz_scale_fract.Inverse().ToFloat();

		output.width = iroundf(static_cast<float>(input.params.width) *
		                       output.horiz_scale);

		output.height = video_mode.height * output.vert_scale;

		if (is_integer(output.horiz_scale)) {
			// Ensure the upscaled image is at least 1000px high for
			// 1:1 pixel aspect ratio images.

			constexpr auto MinUpscaledHeight = 1000;

			if (output.height < MinUpscaledHeight) {
				++output.vert_scale;
			} else {
				break;
			}
		} else {
			// Ensure fractional horizontal scale factors are
			// above 2.0, otherwise we'd get bad looking horizontal
			// blur.

			constexpr auto MinHorizScaleFactor = 2.0f;

			if (output.horiz_scale < MinHorizScaleFactor) {
				++output.vert_scale;
			} else {
				break;
			}
		}
	}

	if (is_integer(output.horiz_scale)) {
		output.horiz_scale        = roundf(output.horiz_scale);
		output.horiz_scaling_mode = PerAxisScaling::Integer;
	} else {
		output.horiz_scaling_mode = PerAxisScaling::Fractional;
	}

	// Determine pixel format
	const auto only_integer_scaling =
	        ((output.horiz_scaling_mode == PerAxisScaling::Integer) &&
	         (output.vert_scaling_mode == PerAxisScaling::Integer));

	if (only_integer_scaling && input.is_paletted()) {
		output.pixel_format = OutputPixelFormat::Indexed8;
	} else {
		output.pixel_format = OutputPixelFormat::Rgb888;
	}

	output.curr_row   = 0;
	output.row_repeat = 0;
}

void ImageScaler::LogParams()
{
#ifdef DEBUG_IMAGE_SCALER
	auto pixel_format_to_string = [](const OutputPixelFormat pf) -> std::string {
		switch (pf) {
		case OutputPixelFormat::Indexed8: return "Indexed8";
		case OutputPixelFormat::Rgb888: return "RGB888";
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

	const auto& src        = input.params;
	const auto& video_mode = input.params.video_mode;

	LOG_DEBUG(
	        "ImageScaler params:\n"
	        "    input.width:                %10d\n"
	        "    input.height:               %10d\n"
	        "    input.double_width:         %10s\n"
	        "    input.double_height:        %10s\n"
	        "    input.PAR:                  1:%1.6f (%d:%d)\n"
	        "    input.pixel_format:         %10s\n"
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
	        src.width,
	        src.height,
	        src.double_width ? "yes" : "no",
	        src.double_height ? "yes" : "no",
	        src.pixel_aspect_ratio.Inverse().ToDouble(),
	        static_cast<int32_t>(src.pixel_aspect_ratio.Num()),
	        static_cast<int32_t>(src.pixel_aspect_ratio.Denom()),
	        to_string(src.pixel_format),
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
#endif
}

void ImageScaler::AllocateBuffers()
{
	int bytes_per_pixel = {};
	switch (output.pixel_format) {
	case OutputPixelFormat::Indexed8: bytes_per_pixel = 8; break;
	case OutputPixelFormat::Rgb888: bytes_per_pixel = 24; break;
	default: assert(false);
	}

	output.row_buf.resize(static_cast<size_t>(output.width) *
	                      static_cast<size_t>(bytes_per_pixel));

	// Pad by 1 pixel at the end so we can handle the last pixel of the row
	// without branching (the interpolator operates on the current and the
	// next pixel).
	linear_row_buf.resize((input.params.width + 1u) * ComponentsPerRgbPixel);
}

int ImageScaler::GetOutputWidth() const
{
	return output.width;
}

int ImageScaler::GetOutputHeight() const
{
	return output.height;
}

OutputPixelFormat ImageScaler::GetOutputPixelFormat() const
{
	return output.pixel_format;
}

void ImageScaler::DecodeNextRowToLinearRgb()
{
	auto out = linear_row_buf.begin();

	for (auto x = 0; x < input.params.width; ++x) {
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
		output.row_repeat = output.vert_scale - 1;
	} else {
		output.row_repeat = 1;
	}
}

void ImageScaler::GenerateNextIntegerUpscaledOutputRow()
{
	auto out = output.row_buf.begin();

	for (auto x = 0; x < input.params.width; ++x) {
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
		const auto floor_x0 = ifloor(x0);
		assert(floor_x0 < input.params.width);

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

		const auto t = std::max(x1 - (static_cast<float>(floor_x0) + 1.0f),
		                        0.0f) *
		               output.horiz_scale;

		const auto out_r = std::lerp(r0, r1, t);
		const auto out_g = std::lerp(g0, g1, t);
		const auto out_b = std::lerp(b0, b1, t);

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
