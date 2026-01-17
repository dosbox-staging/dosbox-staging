// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "deinterlacer.h"

#include <vector>

#include "gui/render/render.h"
#include "utils/checks.h"

CHECK_NARROWING();

Deinterlacer::Deinterlacer() {}

Deinterlacer::~Deinterlacer() {}

void Deinterlacer::ThresholdInput(uint32_t* src, bit_buffer& dest)
{
	auto in_line  = src;
	auto out_line = dest.data() + BufferOffset + buffer_pitch;

	for (auto y = 0; y < image_height; ++y) {
		auto in  = in_line;
		auto out = out_line;

		for (auto x = 0; x < image_width / PixelsPerBitBufferElement; ++x) {
			uint64_t out_buf = 0;

			// Build the 64-bit mask 8 pixels at a time to reduce
			// loop overhead.
			for (auto n = 0; n < 8; ++n) {
				// Make sure the alpha component is set to zero
				constexpr auto mask = 0x00ffffff;

				const auto a1 = in[0] & mask;
				const auto a2 = in[1] & mask;
				const auto a3 = in[2] & mask;
				const auto a4 = in[3] & mask;
				const auto a5 = in[4] & mask;
				const auto a6 = in[5] & mask;
				const auto a7 = in[6] & mask;
				const auto a8 = in[7] & mask;

				in += 8;

				// Non-black pixels are set to 1 in the bit
				// mask. We convert the pixels by row, top to
				// down, left to right. When converting the
				// first 64 pixels of a row, the LSB of the mask
				// uint64_t is the first pixel, and the MSB is
				// the 64th pixel.
				//
				const uint8_t bits =
				        static_cast<uint8_t>(((a1 != 0) << 0)) |
				        static_cast<uint8_t>(((a2 != 0) << 1)) |
				        static_cast<uint8_t>(((a3 != 0) << 2)) |
				        static_cast<uint8_t>(((a4 != 0) << 3)) |
				        static_cast<uint8_t>(((a5 != 0) << 4)) |
				        static_cast<uint8_t>(((a6 != 0) << 5)) |
				        static_cast<uint8_t>(((a7 != 0) << 6)) |
				        static_cast<uint8_t>(((a8 != 0) << 7));

				out_buf |= (uint64_t)bits << (n * 8);
			}
			*out = out_buf;
			++out;
		}

		in_line += image_pitch_pixels;
		out_line += buffer_pitch;
	}
}

void Deinterlacer::DownshiftAndXor(bit_buffer& src, bit_buffer& dest)
{
	// Copy src into dest as a starting point (less than 1 us)
	dest = src;

	auto in_line = src.data() + BufferOffset + buffer_pitch;

	// Start writing from the second line
	auto out_line = dest.data() + BufferOffset + buffer_pitch * 2;

	for (auto y = 0; y < (image_height - 1); ++y) {
		auto in  = in_line;
		auto out = out_line;

		for (auto x = 0; x < image_width / PixelsPerBitBufferElement; ++x) {
			*out ^= *in;
			++in;
			++out;
		}

		in_line += buffer_pitch;
		out_line += buffer_pitch;
	}
}

void Deinterlacer::ErodeHorizontal(bit_buffer& src, bit_buffer& dest)
{
	auto in_line  = src.data() + buffer_pitch;
	auto out_line = dest.data() + buffer_pitch;

	for (auto y = 0; y < image_height; ++y) {
		auto in  = in_line;
		auto out = out_line;

		// We process the input horizontally in 64-pixel chunks.
		// This is the layout of a single chunk in an uint64_t:
		//
		//    bits         pixels
		//
		//    0-7    pixels N    to N+7
		//    8-15   pixels N+8  to N+15
		//   16-23   pixels N+16 to N+23
		//    ...            ...
		//   48-55   pixels N+48 to N+55
		//   56-63   pixels N+56 to N+63
		//
		uint64_t curr = *in++;
		uint64_t prev = 0;

		for (auto x = 0; x < image_width / PixelsPerBitBufferElement + 1;
		     ++x) {

			const auto next = *in;
			++in;

			// Shift in the last pixel of the previous chunk from
			// the right
			const auto prev_pixel63 = (prev & ((uint64_t)1 << 63)) >> 63;
			const auto left_neighbours = (curr << 1) | prev_pixel63;

			// Shift in the first pixel of the next chunk from the left
			const auto next_pixel1      = (next & 1) << 63;
			const auto right_neighbours = next_pixel1 | curr >> 1;

			*out = left_neighbours & curr & right_neighbours;
			++out;

			prev = curr;
			curr = next;
		}

		in_line += buffer_pitch;
		out_line += buffer_pitch;
	}
}

void Deinterlacer::ErodeVertical(bit_buffer& src, bit_buffer& dest)
{
	auto in_line  = src.data() + BufferOffset + buffer_pitch;
	auto out_line = dest.data() + BufferOffset + buffer_pitch;

	for (auto y = 0; y < image_height; ++y) {
		auto in  = in_line;
		auto out = out_line;

		for (auto x = 0; x < image_width / PixelsPerBitBufferElement; ++x) {
			const auto prev = *(in - buffer_pitch);
			const auto curr = *in;
			const auto next = *(in + buffer_pitch);

			*out = prev & curr & next;

			++in;
			++out;
		}

		in_line += buffer_pitch;
		out_line += buffer_pitch;
	}
}

void Deinterlacer::DilateHorizontal(bit_buffer& src, bit_buffer& dest)
{
	auto in_line  = src.data() + buffer_pitch;
	auto out_line = dest.data() + buffer_pitch;

	for (auto y = 0; y < image_height; ++y) {
		auto in  = in_line;
		auto out = out_line;

		// We process the input horizontally in 64-pixel chunks.
		// This is the layout of a single chunk in an uint64_t:
		//
		//    bits         pixels
		//
		//    0-7    pixels N    to N+7
		//    8-15   pixels N+8  to N+15
		//   16-23   pixels N+16 to N+23
		//    ...            ...
		//   48-55   pixels N+48 to N+55
		//   56-63   pixels N+56 to N+63
		//
		uint64_t curr = *in++;
		uint64_t prev = 0;

		for (auto x = 0; x < image_width / PixelsPerBitBufferElement + 1;
		     ++x) {

			const auto next = *in;
			++in;

			// "Shift in" the last pixel of the previous chunk
			const auto prev_pixel63 = (prev & ((uint64_t)1 << 63)) >> 63;
			const auto left_neighbours = (curr << 1) | prev_pixel63;

			// "Shift in" the firs pixel of the next chunk
			const auto next_pixel1      = (next & 1) << 63;
			const auto right_neighbours = next_pixel1 | curr >> 1;

			*out = left_neighbours | curr | right_neighbours;
			++out;

			prev = curr;
			curr = next;
		}

		in_line += buffer_pitch;
		out_line += buffer_pitch;
	}
}

void Deinterlacer::DilateVertical(bit_buffer& src, bit_buffer& dest)
{
	auto in_line  = src.data() + BufferOffset + buffer_pitch;
	auto out_line = dest.data() + BufferOffset + buffer_pitch;

	for (auto y = 0; y < image_height; ++y) {
		auto in  = in_line;
		auto out = out_line;

		for (auto x = 0; x < image_width / PixelsPerBitBufferElement; ++x) {
			const auto prev = *(in - buffer_pitch);
			const auto curr = *in;
			const auto next = *(in + buffer_pitch);

			*out = prev | curr | next;

			++in;
			++out;
		}

		in_line += buffer_pitch;
		out_line += buffer_pitch;
	}
}

static inline uint32_t scale_rgb(uint32_t color, const int factor)
{
	// Scale RGB component values by factor/256 with rounding
	auto scale = [&](uint32_t c) -> uint32_t {
		return (c * factor + 128) >> 8; // 0..255
	};

	const auto r = scale(color & 0xff);
	const auto g = (scale((color >> 8) & 0xff) << 8);
	const auto b = (scale((color >> 16) & 0xff) << 16);
	return r | g | b;
}

void apply_masked_bleed_64(uint64_t m, const uint32_t* in, uint32_t* out,
                           const int rgb_scale_factor)
{
	while (m) {
		const auto k      = std::countr_zero(m);
		const auto in_buf = in[k];
		const auto scaled = scale_rgb(in_buf, rgb_scale_factor);
		out[k] |= scaled;

		m &= (m - 1); // clear lowest set bit
	}
}

static int to_rgb_scale_factor(const DeinterlacingStrength strength)
{
	using enum DeinterlacingStrength;

	switch (strength) {
	case Light: return 153;  // ~0.6x scaling
	case Medium: return 204; // ~0.8x scaling
	case Strong: return 230; // ~0.9x scaling
	case Full: return 256;   // 1.0x scaling (no scaling)
	default:
		assertm(false, "Invalid DeinterlacingStrength value");
		return 0;
	}
}

void Deinterlacer::CombineOutput(uint32_t* pixel_data, bit_buffer& mask,
                                 const DeinterlacingStrength strength)
{
	std::vector<uint32_t> src(image_width * image_height);

	constexpr auto BytesPerPixel = 4;
	std::memcpy(src.data(), pixel_data, image_width * image_height * BytesPerPixel);

	auto in_line = src.data();

	// Start writing to the destination image from the second line
	auto out_line = pixel_data + image_pitch_pixels;

	// Start reading the mask from the second line
	auto mask_line = mask.data() + BufferOffset + buffer_pitch * 2;

	const auto rgb_scale_factor = to_rgb_scale_factor(strength);

	for (auto y = 0; y < (image_height - 1); ++y) {
		auto mask = mask_line;

		for (auto x = 0; x < image_width / PixelsPerBitBufferElement; ++x) {
			const uint64_t m = mask[x];
			if (m) {
				// 64 pixels = 64 uint32_t
				apply_masked_bleed_64(m,
				                      in_line + x * 64,
				                      out_line + x * 64,
				                      rgb_scale_factor);
			}
		}

		in_line += image_pitch_pixels;
		out_line += image_pitch_pixels;

		mask_line += buffer_pitch;
	}
}

void Deinterlacer::Deinterlace(RenderedImage& image,
                               const DeinterlacingStrength strength)
{
	assert(image.params.pixel_format == PixelFormat::BGRX32_ByteArray);

	image_width        = image.params.width;
	image_height       = image.params.height;
	image_pitch_pixels = image.pitch / sizeof(uint32_t);

	// Not undefined behaviour because the original image buffer was an
	// uint32_t vector
	auto pixel_data = reinterpret_cast<uint32_t*>(image.image_data);

	// We store 64 1-bit pixels per uint64_t, plus 1 uint64_t for padding at
	// the end of each row. We also store two padding rows at the top and
	// bottom.
	const auto bufsize = (image_width / PixelsPerBitBufferElement + BufferOffset) *
	                     (image_height + 2);

	buffer1.resize(bufsize);
	buffer2.resize(bufsize);

	buffer_pitch = image_width / PixelsPerBitBufferElement + BufferOffset;

	ThresholdInput(pixel_data, buffer1);
	DownshiftAndXor(buffer1, buffer2);

	for (auto i = 0; i < 2; ++i) {
		ErodeHorizontal(buffer2, buffer1);
		ErodeVertical(buffer1, buffer2);
	}
	for (auto i = 0; i < 2; ++i) {
		DilateHorizontal(buffer2, buffer1);
		DilateVertical(buffer1, buffer2);
	}

	// buffer2 now contains the mask for the interlaced area

	CombineOutput(pixel_data, buffer2, strength);
}
