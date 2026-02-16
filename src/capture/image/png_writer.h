// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PNG_WRITER_H
#define DOSBOX_PNG_WRITER_H

#include <array>
#include <cstdlib>
#include <optional>
#include <vector>

#include <png.h>

#include "gui/render/render.h"
#include "hardware/video/vga.h"
#include "image_saver.h"
#include "utils/rgb888.h"

// A row-based PNG writer that also writes the pixel aspect ratio of the image
// into the standard pHYs PNG chunk.
class PngWriter {
public:
	PngWriter() = default;
	~PngWriter();

	bool InitRgb888(FILE* fp, const int width, const int height,
	                const Fraction& pixel_aspect_ratio,
	                const VideoMode& video_mode);

	bool InitIndexed8(FILE* fp, const int width, const int height,
	                  const Fraction& pixel_aspect_ratio,
	                  const VideoMode& video_mode,
	                  const std::array<Rgb888, NumVgaColors>& palette);

	void WriteRow(std::vector<uint8_t>::const_iterator row);

	// prevent copying
	PngWriter(const PngWriter&) = delete;
	// prevent assignment
	PngWriter& operator=(const PngWriter&) = delete;

private:
	bool Init(FILE* fp);
	void SetPngCompressionsParams();

	void WritePngInfo(const int width, const int height,
	                  const Fraction& pixel_aspect_ratio,
	                  const VideoMode& video_mode, const bool is_paletted,
	                  const std::array<Rgb888, NumVgaColors>& palette);

	void FinalisePng();

	png_structp png_ptr    = nullptr;
	png_infop png_info_ptr = nullptr;
};

#endif

