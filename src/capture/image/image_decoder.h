// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_IMAGE_DECODER_H
#define DOSBOX_IMAGE_DECODER_H

#include <cassert>
#include <vector>

#include "gui/render/render.h"
#include "hardware/video/vga.h"
#include "misc/support.h"
#include "utils/byteorder.h"
#include "utils/checks.h"
#include "utils/mem_host.h"
#include "utils/rgb555.h"
#include "utils/rgb565.h"
#include "utils/rgb888.h"

class ImageDecoder {
public:
	ImageDecoder()  = default;
	~ImageDecoder() = default;

	// Set `row_skip_count` to 1 reconstruct the raw image when the image
	// has been height-doubled.
	//
	// Set `pixel_skip_count` to 1 reconstruct the raw image when the image
	// has been width-doubled.
	//
	void Init(const RenderedImage& image, const int row_skip_count,
	          const int pixel_skip_count);

	inline Rgb888 GetNextPixel()
	{
		assert(pos - curr_row_start < image.pitch);

		const auto b = *(pos + 0);
		const auto g = *(pos + 1);
		const auto r = *(pos + 2);

		for (auto i = 0; i <= pixel_skip_count; ++i) {
			switch (image.params.pixel_format) {
			case PixelFormat::BGR24_ByteArray: pos += 3; break;
			case PixelFormat::BGRX32_ByteArray: pos += 4; break;
			default: assertm(false, "Invalid PixelFormat value");
			}
		}

		return {r, g, b};
	}

	void AdvanceRow();

	// prevent copying
	ImageDecoder(const ImageDecoder&) = delete;
	// prevent assignment
	ImageDecoder& operator=(const ImageDecoder&) = delete;

private:
	RenderedImage image = {};

	int row_skip_count   = 0;
	int pixel_skip_count = 0;

	const uint8_t* curr_row_start = nullptr;
	const uint8_t* pos            = nullptr;
};

#endif // DOSBOX_IMAGE_DECODER_H
