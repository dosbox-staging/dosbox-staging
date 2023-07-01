/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_PNG_WRITER_H
#define DOSBOX_PNG_WRITER_H

#include <cstdlib>
#include <optional>
#include <vector>

#include "image_saver.h"

#include "render.h"

#include <png.h>
#include <zlib.h>

// A row-based PNG writer that also writes the pixel aspect ratio of the image
// into the standard pHYs PNG chunk.
class PngWriter {
public:
	PngWriter() = default;
	~PngWriter();

	bool InitRgb888(FILE* fp, const uint16_t width, const uint16_t height,
	                const Fraction& pixel_aspect_ratio,
	                const VideoMode& video_mode);

	bool InitIndexed8(FILE* fp, const uint16_t width, const uint16_t height,
	                  const Fraction& pixel_aspect_ratio,
	                  const VideoMode& video_mode, const uint8_t* palette_data);

	void WriteRow(std::vector<uint8_t>::const_iterator row);

	// prevent copying
	PngWriter(const PngWriter&) = delete;
	// prevent assignment
	PngWriter& operator=(const PngWriter&) = delete;

private:
	bool Init(FILE* fp);
	void SetPngCompressionsParams();

	void WritePngInfo(const uint16_t width, const uint16_t height,
	                  const Fraction& pixel_aspect_ratio,
	                  const VideoMode& video_mode, const bool is_paletted,
	                  const uint8_t* palette_data);

	void FinalisePng();

	png_structp png_ptr    = nullptr;
	png_infop png_info_ptr = nullptr;
};

#endif

