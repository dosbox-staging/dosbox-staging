// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PNG_WRITER_H
#define DOSBOX_PNG_WRITER_H

#include <cstdlib>
#include <optional>
#include <vector>

#include <png.h>

#include "gui/render/render.h"
#include "image_saver.h"


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

