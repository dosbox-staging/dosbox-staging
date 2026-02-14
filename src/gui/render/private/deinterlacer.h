// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_DEINTERLACER_H
#define DOSBOX_RENDER_DEINTERLACER_H

#include <cstdint>
#include <vector>

#include "misc/rendered_image.h"

enum class DeinterlacingStrength { Off, Light, Medium, Strong, Full };

/*
 * Image processor to remove alternating black lines from fake-interlaced FMV
 * videos in games (also referred to as "deinterlacing"). This was a trick to
 * get away with storing a half-resolution stream vertically and to go easy on
 * the CPU on less powerful machines when decoding and displaying the video, so
 * it's not really interlacing but the name stuck.
 *
 * Handles 400-line or higher (S)VGA and VESA modes with 256 colours or more.
 *
 * There are special code paths to handle both the pre and post scaler variants
 * of these video modes (pre-scaler for image and video capturing, post-scaler
 * for the actual video output).
 */
class Deinterlacer {
public:
	Deinterlacer()  = default;
	~Deinterlacer() = default;

	// Expects packed RGBA pixel data (one uint32_t per pixel, no extra
	// padding per line).
	RenderedImage Deinterlace(const RenderedImage& image,
	                          const DeinterlacingStrength strength);

	// prevent copying
	Deinterlacer(const Deinterlacer&) = delete;
	// prevent assignment
	Deinterlacer& operator=(const Deinterlacer&) = delete;

private:
	// Bit buffer storing bits as 64-bit integers
	using bit_buffer = std::vector<uint64_t>;

	const int PixelsPerBitBufferElement = 64;

	// Line deinterlace methods
	RenderedImage LineDeinterlace(const RenderedImage& input_image,
	                              const DeinterlacingStrength strength);

	bool SetUpInputImage(const RenderedImage& image);
	void DecodeInputImage(const RenderedImage& input_image);

	uint32_t DetectBackgroundColor(const uint32_t* pixel_data) const;

	void ThresholdInput(const uint32_t* pixel_data, bit_buffer& dest,
	                    const uint32_t bg_color) const;

	void DownshiftAndXor(bit_buffer& src, bit_buffer& dest) const;

	void ErodeHorizontal(bit_buffer& src, bit_buffer& dest) const;
	void ErodeVertical(bit_buffer& src, bit_buffer& dest) const;

	void DilateHorizontal(bit_buffer& src, bit_buffer& dest) const;
	void DilateVertical(bit_buffer& src, bit_buffer& dest) const;

	void CombineOutput(uint32_t* pixel_data, bit_buffer& mask,
	                   const DeinterlacingStrength strength) const;

	// Dot deinterlace methods
	RenderedImage DotDeinterlace(const RenderedImage& input_image,
	                             const DeinterlacingStrength strength);

	void WriteDotDeinterlacedOutput2x2(std::vector<uint32_t>::const_iterator in_line,
	                                   uint8_t*& out_line, int out_pitch,
	                                   int rgb_scale_factor) const;

	void WriteDotDeinterlacedOutput2x4(std::vector<uint32_t>::const_iterator in_line,
	                                   uint8_t*& out_line, int out_pitch,
	                                   int rgb_scale_factor) const;

	void WriteDotDeinterlacedOutput4x2(std::vector<uint32_t>::const_iterator in_line,
	                                   uint8_t*& out_line, int out_pitch,
	                                   int rgb_scale_factor) const;

	void WriteDotDeinterlacedOutput4x4(std::vector<uint32_t>::const_iterator in_line,
	                                   uint8_t*& out_line, int out_pitch,
	                                   int rgb_scale_factor) const;

	// Information about the input image
	struct {
		int width        = 0;
		int height       = 0;
		int pitch_pixels = 0;
		uint32_t* data   = {};
	} image = {};

	// Decoded image for non-BGRX32 images if the image is not processed in
	// place
	std::vector<uint32_t> decoded_image = {};

	// Temporary work buffers holding 1-bit image data
	bit_buffer buffer1 = {};
	bit_buffer buffer2 = {};

	// Number of uint64_t's before the start of the actual image data in
	// each row
	const int BufferOffset = 1;

	// Number of uint64_t's between two consecutive rows
	int buffer_pitch = 0;
};

#endif // DOSBOX_RENDER_DEINTERLACER_H
