// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_IMAGE_DECODER_H
#define DOSBOX_IMAGE_DECODER_H

#include <cassert>
#include <vector>

#include "hardware/video/vga.h"
#include "misc/rendered_image.h"
#include "misc/support.h"
#include "utils/byteorder.h"
#include "utils/checks.h"
#include "utils/mem_host.h"
#include "utils/rgb555.h"
#include "utils/rgb565.h"
#include "utils/rgb888.h"

// Row-based image decoder to convert an image in any pixel format to
// 8-bit indexed or 32-bit BGRX pixel data.
//
class ImageDecoder {
public:
	// Set `row_skip_count` to 1 reconstruct the raw image when the input
	// has "baked-in" double scanning.
	//
	// Set `pixel_skip_count` to 1 reconstruct the raw image when the input
	// has "baked-in" pixel doubling.
	//
	ImageDecoder(const RenderedImage& image, const int row_skip_count,
	             const int pixel_skip_count);

	~ImageDecoder() = default;

	// Decodes the next row into the `out` vector as 8-bit indexed pixel
	// data. `out` must be large enough to hold the data.
	//
	// Decodes the first row when called for the first time after
	// construction.
	//
	void GetNextRowAsIndexed8Pixels(std::vector<uint8_t>::iterator out);

	// Decodes the next row into the `out` vector as 32-bit BGRX pixel data.
	// `out` must be large enough to hold the data.
	//
	// Decodes the first row when called for the first time after
	// construction.
	//
	void GetNextRowAsBgrx32Pixels(std::vector<uint32_t>::iterator out);

	// prevent copying
	ImageDecoder(const ImageDecoder&) = delete;
	// prevent assignment
	ImageDecoder& operator=(const ImageDecoder&) = delete;

private:
	void GetBgrx32RowFromIndexed8(std::vector<uint32_t>::iterator out);
	void GetBgrx32RowFromRgb555(std::vector<uint32_t>::iterator out);
	void GetBgrx32RowFromRgb565(std::vector<uint32_t>::iterator out);
	void GetBgrx32RowFromBgr24(std::vector<uint32_t>::iterator out);
	void GetBgrx32RowFromBgrx32(std::vector<uint32_t>::iterator out);

	void AdvanceRow();

	RenderedImage image = {};

	int row_skip_count   = 0;
	int pixel_skip_count = 0;

	const uint8_t* in_row_start = nullptr;
	const uint8_t* in_pos       = nullptr;

	int out_width = 0;
};

#endif // DOSBOX_IMAGE_DECODER_H
