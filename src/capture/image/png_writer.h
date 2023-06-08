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

#include "fraction.h"
#include "image_saver.h"

#include <png.h>
#include <zlib.h>

// A row-based PNG writer that also writes the pixel aspect ratio of the image
// into the standard pHYs PNG chunk.
class PngWriter {
public:
	PngWriter() = default;
	~PngWriter();

	// If `source_image_info` is specified, a textual description of the
	// source image is written to the standard tEXt PNG chunk under the
	// "Source" keyword. For example:
	//
	// source resolution: 640x350; source pixel aspect ratio: 35:48
	// (1:1.371429)
	//
	bool InitRgb888(FILE* fp, const ImageInfo& image_info,
	                const std::optional<ImageInfo>& source_image_info = {});

	bool InitIndexed8(FILE* fp, const ImageInfo& image_info,
	                  const std::optional<ImageInfo>& source_image_info,
	                  const uint8_t* palette_data);

	void WriteRow(std::vector<uint8_t>::const_iterator row);

	// prevent copying
	PngWriter(const PngWriter&) = delete;
	// prevent assignment
	PngWriter& operator=(const PngWriter&) = delete;

private:
	bool Init(FILE* fp);
	void SetPngCompressionsParams();

	void WritePngInfo(const ImageInfo& image_info,
	                  const std::optional<ImageInfo>& source_image_info,
	                  const bool is_paletted, const uint8_t* palette_data);

	void FinalisePng();

	png_structp png_ptr    = nullptr;
	png_infop png_info_ptr = nullptr;
};

#endif

