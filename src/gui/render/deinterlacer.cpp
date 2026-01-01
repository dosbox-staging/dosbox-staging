// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
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
	auto in       = src;
	auto out_line = dest.data() + buffer_offset + buffer_pitch;

	for (auto y = 0; y < image_height; ++y) {
		auto out = out_line;

		for (auto x = 0; x < image_width / PixelsPerBitBufferElement; ++x) {
			uint64_t out_buf = 0;

			for (auto bit_idx = 0; bit_idx < PixelsPerBitBufferElement;
			     ++bit_idx) {
				// Make sure the alpha component is set
				// to zero
				const auto a = *in & 0x00ffffff;
				++in;

				// Non-black pixels are set to 1 in the bit
				// mask. We convert the pixels by row, top to
				// down, left to right. When converting the
				// first 64 pixels of a row, the LSB of the mask
				// uint64_t is the first pixel, and the MSB is
				// the 64th pixel.
				out_buf |= (a > 0) ? ((uint64_t)1 << bit_idx) : 0;
			}
			*out = out_buf;
			++out;
		}

		out_line += buffer_pitch;
	}
}

void Deinterlacer::DownshiftAndXor(bit_buffer& src, bit_buffer& dest)
{
	// Copy src into dest as a starting point (less than 1 us)
	dest = src;

	auto in_line = src.data() + buffer_offset + buffer_pitch;

	// Start writing from the second line
	auto out_line = dest.data() + buffer_offset + buffer_pitch * 2;

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
	auto in_line  = src.data() + buffer_offset + buffer_pitch;
	auto out_line = dest.data() + buffer_offset + buffer_pitch;

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
	auto in_line  = src.data() + buffer_offset + buffer_pitch;
	auto out_line = dest.data() + buffer_offset + buffer_pitch;

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

void Deinterlacer::CombineOutput(uint32_t* pixel_data, bit_buffer& mask)
{
	std::vector<uint32_t> src(image_width * image_height);

	constexpr auto BytesPerPixel = 4;
	std::memcpy(src.data(), pixel_data, image_width * image_height * BytesPerPixel);

	auto in = src.data();

	// Start writing to the destinateion image from the second line
	auto out = pixel_data + image_width;

	// Start reading the mask from the second line
	auto mask_line = mask.data() + buffer_offset + buffer_pitch * 2;

	for (auto y = 0; y < (image_height - 1); ++y) {
		auto mask_in = mask_line;

		for (auto x = 0; x < image_width / PixelsPerBitBufferElement; ++x) {
			auto mask_elem = *mask_in;

			for (auto bit_idx = 0; bit_idx < PixelsPerBitBufferElement;
			     ++bit_idx) {

				if (mask_elem & 1) {
					const auto in_buf = *in;

					// Deinterlacing strength params
					//
					// low     1 / 2
					// medium  2 / 3
					// high    4 / 5
					// subtle  8 / 9
					// full    1 / 1

					uint64_t scaled = 0;
					scaled |= (in_buf & 0xff) * 8 / 9;

					scaled |= (((in_buf >> 8) & 0xff) * 8 / 9)
					       << 8;

					scaled |= (((in_buf >> 16) & 0xff) * 8 / 9)
					       << 16;

					*out |= scaled;
				}
				mask_elem >>= 1;
				++out;
				++in;
			}
			++mask_in;
		}
		mask_line += buffer_pitch;
	}
}

void Deinterlacer::Deinterlace(uint32_t* pixel_data, const int _image_width,
                               const int _image_height)
{
	image_width  = _image_width;
	image_height = _image_height;

	// We store 64 1-bit pixels per uint64_t, plus 1 uint64_t for padding at
	// the end of each row. We also store two padding rows at the top and
	// bottom.
	const auto bufsize = (image_width / PixelsPerBitBufferElement + buffer_offset) *
	                     (image_height + 2);

	buffer1.resize(bufsize);
	buffer2.resize(bufsize);

	buffer_pitch = image_width / PixelsPerBitBufferElement + buffer_offset;

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

	CombineOutput(pixel_data, buffer2);
}

