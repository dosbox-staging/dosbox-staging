// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_IMAGE_SCALER_H
#define DOSBOX_IMAGE_SCALER_H

#include <memory>
#include <vector>

#include "misc/image_decoder.h"
#include "misc/rendered_image.h"
#include "utils/rgb888.h"

enum class OutputPixelFormat { Indexed8, Rgb888 };

enum class PerAxisScaling { Integer, Fractional };

// Row-based image scaler. Always upscales by an auto-selected integral
// scaling factor vertically so both criteria are satisfied:
//
// - the scaling factor is at least 2:2 (except for 1600x1200 pixel modes;
//   we use 1:1 factor for those, so no upscaling)
//
// - the resulting output height is ideally around 1200px (this can go higher
//   to satisfy the minimum 2:2 scaling factor constraint)
//
// The horizontal scaling factor can be either an integer or a fraction so the
// input aspect ratio is preserved. "Sharp-bilinear" interpolation is used
// horizontally to preserve the pixeled look as much as possible. Paletted
// images are kept paletted only if both the horizontal and vertical scaling
// factors are integral, otherwise the output is RGB888.
//
// The scaling is always performed on the "raw", non-double-scanned and
// non-pixel-doubled input image.
//
// A few examples:
//
//      320x200  - upscaled to 1600x1200 (5:6 scaling factors)
//      320x240  - upscaled to 1600x1200 (5:5 scaling factors)
//      400x300  - upscaled to 1600x1200 (4:4 scaling factors)
//      640x350  - upscaled to 1400x1050 (2.1875:3 scaling factors)
//      640x480  - upscaled to 1920x1440 (3:3 scaling factors)
//      720x400  - upscaled to 1600x1200 (2.2222:3 scaling factors)
//      800x600  - upscaled to 1600x1200 (2:2 scaling factors)
//     1024x768  - upscaled to 2048x1536 (2:2 scaling factors)
//     1280x1024 - upscaled to 2731x2048 (2.1333:2 scaling factors)
//     1600x1200 - no upscaling (stays 1600x1200)
//
// Usage:
//
// - Call `Init()` with the input image and the source vide mode.
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

	void Init(const RenderedImage& image);

	std::vector<uint8_t>::const_iterator GetNextOutputRow();

	int GetOutputWidth() const;
	int GetOutputHeight() const;
	OutputPixelFormat GetOutputPixelFormat() const;

	// prevent copying
	ImageScaler(const ImageScaler&) = delete;
	// prevent assignment
	ImageScaler& operator=(const ImageScaler&) = delete;

private:
	static constexpr auto ComponentsPerRgbPixel = 3;

	void UpdateOutputParamsDoublingOnly();
	void UpdateOutputParamsUpscale();
	void LogParams();
	void AllocateBuffers();

	void DecodeNextRowToLinearRgb();

	void SetRowRepeat();

	void GenerateNextIntegerUpscaledOutputRow();
	void GenerateNextSharpUpscaledOutputRow();

	RenderedImage input                         = {};
	std::unique_ptr<ImageDecoder> input_decoder = {};

	std::vector<uint8_t> row_decode_buf_8   = {};
	std::vector<uint32_t> row_decode_buf_32 = {};

	std::vector<float> linear_row_buf = {};

	struct {
		int width  = 0;
		int height = 0;

		float horiz_scale                 = 0;
		float one_per_horiz_scale         = 0;
		int vert_scale                    = 0;
		PerAxisScaling horiz_scaling_mode = {};
		PerAxisScaling vert_scaling_mode  = {};

		OutputPixelFormat pixel_format = {};

		int curr_row   = 0;
		int row_repeat = 0;

		std::vector<uint8_t> row_buf = {};
	} output = {};
};

#endif // DOSBOX_IMAGE_SCALER_H
