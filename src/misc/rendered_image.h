// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDERED_IMAGE_H
#define DOSBOX_RENDERED_IMAGE_H

#include <array>
#include <cstring>

#include "hardware/video/vga.h"
#include "misc/video.h"
#include "utils/rgb888.h"

// A frame of the emulated video output that's passed to the rendering backend
// or to the image and video capturers.
//
// Also used for passing the post-shader output read back from the frame buffer
// to the image capturer.
//
struct RenderedImage {
	ImageInfo params = {};

	// If true, the image is stored flipped vertically, starting from the
	// bottom row
	bool is_flipped_vertically = false;

	// Bytes per row
	int pitch = 0;

	// (width * height) number of pixels stored in the pixel format defined
	// by pixel_format
	uint8_t* image_data = nullptr;

	std::array<Rgb888, NumVgaColors> palette = {};

	inline bool is_paletted() const
	{
		return (params.pixel_format == PixelFormat::Indexed8);
	}

	RenderedImage deep_copy() const
	{
		RenderedImage copy = *this;

		// Deep-copy image and palette data
		const auto image_data_num_bytes = static_cast<uint32_t>(
		        params.height * pitch);

		copy.image_data = new uint8_t[image_data_num_bytes];

		assert(image_data);
		std::memcpy(copy.image_data, image_data, image_data_num_bytes);

		copy.palette = palette;

		return copy;
	}

	void free()
	{
		if (image_data) {
			delete[] image_data;
			image_data = nullptr;
		}
	}
};

#endif // DOSBOX_RENDERED_IMAGE_H
