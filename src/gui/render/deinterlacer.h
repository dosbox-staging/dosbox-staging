// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_DEINTERLACER_H
#define DOSBOX_RENDER_DEINTERLACER_H

#include <cstdint>
#include <vector>

// forward declaration
struct RenderedImage;

enum class DeinterlacingStrength { Off, Light, Medium, Strong, Full };

class Deinterlacer {
public:
	Deinterlacer();
	~Deinterlacer();

	// Expects packed RGBA pixel data (one uint32_t per pixel, no extra
	// padding per line).
	void Deinterlace(RenderedImage& image, const DeinterlacingStrength strength);

	// prevent copying
	Deinterlacer(const Deinterlacer&) = delete;
	// prevent assignment
	Deinterlacer& operator=(const Deinterlacer&) = delete;

private:
	typedef std::vector<uint64_t> bit_buffer;

	void ThresholdInput(uint32_t* pixel_data, bit_buffer& dest);
	void DownshiftAndXor(bit_buffer& src, bit_buffer& dest);

	void ErodeHorizontal(bit_buffer& src, bit_buffer& dest);
	void ErodeVertical(bit_buffer& src, bit_buffer& dest);

	void DilateHorizontal(bit_buffer& src, bit_buffer& dest);
	void DilateVertical(bit_buffer& src, bit_buffer& dest);

	void CombineOutput(uint32_t* pixel_data, bit_buffer& mask,
	                   const DeinterlacingStrength strength);

	bit_buffer buffer1 = {};
	bit_buffer buffer2 = {};

	const int PixelsPerBitBufferElement = 64;

	int image_width        = 0;
	int image_height       = 0;
	int image_pitch_pixels = 0;

	// Number of uint64_t's before the start of the actual image data in
	// each row
	const int BufferOffset = 1;

	// Number of uint64_t's between two consecutive rows
	int buffer_pitch = 0;
};

#endif // DOSBOX_RENDER_DEINTERLACER_H
