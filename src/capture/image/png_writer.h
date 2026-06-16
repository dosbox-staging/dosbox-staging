// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PNG_WRITER_H
#define DOSBOX_PNG_WRITER_H

#include <array>
#include <cstdlib>
#include <vector>

#include <png.h>

#include "gui/render/render.h"
#include "hardware/video/vga.h"
#include "image_saver.h"
#include "utils/rgb888.h"

// Screenshot / video-frame metadata that drives the pHYs and Source text
// PNG chunks. Encapsulates the two pixel-aspect-ratio policies the image-
// capture pipeline uses: pass-through (raw screenshots) and 1:1-because-
// baked-in (upscaled / rendered screenshots).
//
// Printer output uses the bare PngWriter::Init* overloads and never
// constructs this; pages have no pixel aspect ratio or source video
// mode.
class PngMetadata {
public:
	// Raw image: PAR is the source video mode's actual pixel aspect
	// ratio.
	static PngMetadata FromVideoMode(const VideoMode& mode);

	// Upscaled / rendered image: PAR is forced to 1:1 because the
	// upscaler bakes the non-squareness into the pixels. The original
	// mode is still carried so the Source text chunk can describe the
	// source resolution.
	static PngMetadata FromVideoModeAsSquare(const VideoMode& mode);

	const Fraction& PixelAspectRatio() const
	{
		return pixel_aspect_ratio;
	}
	const VideoMode& SourceVideoMode() const
	{
		return video_mode;
	}

private:
	PngMetadata(const Fraction& par, const VideoMode& mode)
	        : pixel_aspect_ratio(par),
	          video_mode(mode)
	{}

	Fraction pixel_aspect_ratio = {};
	VideoMode video_mode        = {};
};

// A row-based PNG writer.
//
// Two flavours of Init* per pixel format:
//
//   - The bare overload writes a minimal PNG (IHDR, PLTE for indexed,
//     IDAT, IEND). Used by printer page output.
//
//   - The metadata overload also writes pHYs (pixel aspect ratio),
//     gAMA, and tEXt chunks describing the source video mode. Used by
//     screenshot / video-frame capture.
class PngWriter {
public:
	PngWriter() = default;
	~PngWriter();

	bool InitRgb888(FILE* fp, int width, int height);
	bool InitRgb888(FILE* fp, int width, int height, const PngMetadata& metadata);

	bool InitIndexed8(FILE* fp, int width, int height,
	                  const std::array<Rgb888, NumVgaColors>& palette);
	bool InitIndexed8(FILE* fp, int width, int height,
	                  const std::array<Rgb888, NumVgaColors>& palette,
	                  const PngMetadata& metadata);

	// Pre-Init setter. Default is Z_DEFAULT_COMPRESSION (level 6), the
	// sweet spot used by capture. Printer page output overrides with
	// Z_BEST_COMPRESSION (level 9) since pages favour file size over
	// encode speed.
	void SetCompressionLevel(int level);

	void WriteRow(std::vector<uint8_t>::const_iterator row);

	// prevent copying
	PngWriter(const PngWriter&) = delete;
	// prevent assignment
	PngWriter& operator=(const PngWriter&) = delete;

private:
	bool Init(FILE* fp);
	void SetPngCompressionsParams();

	void WritePngInfo(int width, int height, bool is_paletted,
	                  const std::array<Rgb888, NumVgaColors>& palette,
	                  const PngMetadata* metadata);

	void FinalisePng();

	png_structp png_ptr    = nullptr;
	png_infop png_info_ptr = nullptr;

	int compression_level = -1; // resolved in SetPngCompressionsParams
};

#endif
