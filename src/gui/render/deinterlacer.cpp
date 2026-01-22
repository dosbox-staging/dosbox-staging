// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "deinterlacer.h"

#include <vector>

#include "misc/image_decoder.h"
#include "utils/checks.h"

CHECK_NARROWING();

void Deinterlacer::ThresholdInput(const uint32_t* src, bit_buffer& dest) const
{
	auto in_line  = src;
	auto out_line = dest.data() + BufferOffset + buffer_pitch;

	for (auto y = 0; y < image.height; ++y) {
		auto in  = in_line;
		auto out = out_line;

		for (auto x = 0; x < image.width / PixelsPerBitBufferElement; ++x) {
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

				out_buf |= static_cast<uint64_t>(bits) << (n * 8);
			}
			*out = out_buf;
			++out;
		}

		in_line += image.pitch_pixels;
		out_line += buffer_pitch;
	}
}

void Deinterlacer::DownshiftAndXor(bit_buffer& src, bit_buffer& dest) const
{
	// Copy src into dest as a starting point (less than 1 us)
	dest = src;

	auto in_line = src.begin() + BufferOffset + buffer_pitch;

	// Start writing from the second line
	auto out_line = dest.begin() + BufferOffset + buffer_pitch * 2;

	for (auto y = 0; y < (image.height - 1); ++y) {
		auto in  = in_line;
		auto out = out_line;

		for (auto x = 0; x < image.width / PixelsPerBitBufferElement; ++x) {
			*out ^= *in;
			++in;
			++out;
		}

		in_line += buffer_pitch;
		out_line += buffer_pitch;
	}
}

void Deinterlacer::ErodeHorizontal(bit_buffer& src, bit_buffer& dest) const
{
	auto in_line  = src.begin() + buffer_pitch;
	auto out_line = dest.begin() + buffer_pitch;

	for (auto y = 0; y < image.height; ++y) {
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

		for (auto x = 0; x < image.width / PixelsPerBitBufferElement + 1;
		     ++x) {

			const auto next = *in;
			++in;

			// Shift in the last pixel of the previous chunk from
			// the right
			const auto prev_pixel63 =
			        (prev & (static_cast<uint64_t>(1) << 63)) >> 63;

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

void Deinterlacer::ErodeVertical(bit_buffer& src, bit_buffer& dest) const
{
	auto in_line  = src.begin() + BufferOffset + buffer_pitch;
	auto out_line = dest.begin() + BufferOffset + buffer_pitch;

	for (auto y = 0; y < image.height; ++y) {
		auto in  = in_line;
		auto out = out_line;

		for (auto x = 0; x < image.width / PixelsPerBitBufferElement; ++x) {
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

void Deinterlacer::DilateHorizontal(bit_buffer& src, bit_buffer& dest) const
{
	auto in_line  = src.begin() + buffer_pitch;
	auto out_line = dest.begin() + buffer_pitch;

	for (auto y = 0; y < image.height; ++y) {
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

		for (auto x = 0; x < image.width / PixelsPerBitBufferElement + 1;
		     ++x) {

			const auto next = *in;
			++in;

			// "Shift in" the last pixel of the previous chunk
			const auto prev_pixel63 =
			        (prev & (static_cast<uint64_t>(1) << 63)) >> 63;

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

void Deinterlacer::DilateVertical(bit_buffer& src, bit_buffer& dest) const
{
	auto in_line  = src.begin() + BufferOffset + buffer_pitch;
	auto out_line = dest.begin() + BufferOffset + buffer_pitch;

	for (auto y = 0; y < image.height; ++y) {
		auto in  = in_line;
		auto out = out_line;

		for (auto x = 0; x < image.width / PixelsPerBitBufferElement; ++x) {
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
	auto scale = [&](uint32_t c) {
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

		// clear lowest set bit
		m &= (m - 1);
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
                                 const DeinterlacingStrength strength) const
{
	std::vector<uint32_t> src(image.width * image.height);

	constexpr auto BytesPerPixel = sizeof(uint32_t);
	std::memcpy(src.data(), pixel_data, image.width * image.height * BytesPerPixel);

	auto in_line = src.data();

	// Start writing to the destination image from the second line
	auto out_line = pixel_data + image.pitch_pixels;

	// Start reading the mask from the second line
	auto mask_line = mask.data() + BufferOffset + buffer_pitch * 2;

	const auto rgb_scale_factor = to_rgb_scale_factor(strength);

	for (auto y = 0; y < (image.height - 1); ++y) {
		auto mask_pos = mask_line;

		for (auto x = 0; x < image.width / PixelsPerBitBufferElement; ++x) {
			const uint64_t m = mask_pos[x];
			if (m) {
				// 64 pixels = 64 uint32_t
				apply_masked_bleed_64(m,
				                      in_line + x * 64,
				                      out_line + x * 64,
				                      rgb_scale_factor);
			}
		}

		in_line += image.pitch_pixels;
		out_line += image.pitch_pixels;

		mask_line += buffer_pitch;
	}
}

void Deinterlacer::DecodeInputImage(const RenderedImage& input_image)
{
	const auto& p = input_image.params;

	// Convert input image to 32-bit BGRX format and get rid of
	// "baked-in" pixel and line doubling.
	const auto pixel_skip_count = (p.width / p.video_mode.width) - 1;
	const auto row_skip_count   = (p.height / p.video_mode.height) - 1;

	image.width        = p.width / (pixel_skip_count + 1);
	image.height       = p.height / (row_skip_count + 1);
	image.pitch_pixels = image.width;

	decoded_image.resize(image.height * image.pitch_pixels);

	// Convert pixel data
	ImageDecoder image_decoder(input_image, row_skip_count, pixel_skip_count);

	auto out_line = decoded_image.begin();

	for (auto y = 0; y < image.height; ++y) {
		auto out = out_line;
		image_decoder.GetNextRowAsBgrx32Pixels(out);

		out_line += image.pitch_pixels;
	}
}

bool Deinterlacer::SetUpInputImage(const RenderedImage& input_image)
{
	if (input_image.params.pixel_format == PixelFormat::BGRX32_ByteArray) {

		// Not undefined behaviour because the original image buffer was
		// an uint32_t vector
		image.width        = input_image.params.width;
		image.height       = input_image.params.height;
		image.pitch_pixels = input_image.pitch / sizeof(uint32_t);

		image.data = reinterpret_cast<uint32_t*>(input_image.image_data);

		return true;

	} else {
		DecodeInputImage(input_image);
		image.data = decoded_image.data();

		return false;
	}
}

// Automatically deinterlace FMV videos in the input image that have every
// second scanline black. Non-interlaced areas will be left intact.
//
RenderedImage Deinterlacer::LineDeinterlace(const RenderedImage& input_image,
                                            const DeinterlacingStrength strength)
{
	// `SetUpInputImage()` returns true if the image had to be decoded into
	// a temporary buffer.
	auto process_in_place = SetUpInputImage(input_image);

	// We store 64 1-bit pixels per uint64_t, plus 1 uint64_t for
	// padding at the end of each row. We also store two padding
	// rows at the top and bottom.
	const auto bufsize = (image.width / PixelsPerBitBufferElement + BufferOffset) *
	                     (image.height + 2);

	buffer1.resize(bufsize);
	buffer2.resize(bufsize);

	buffer_pitch = image.width / PixelsPerBitBufferElement + BufferOffset;

	// Run a threshold pass on the original image to generate a 1-bit
	// mask. The mask bitplane is 0 for black pixels and 1 for non-black
	// pixels. Interlaced areas will show up as alternating lines of 1s
	// and 0s.
	//
	ThresholdInput(image.data, buffer1);

	// Make a copy of the 1-bit mask, shift it one pixel down, and XOR it
	// with the unshifted original mask. This will cause interlaced areas
	// to become contiguous area filled with 1s. Non interlaced areas
	// will largely disappear, except at their top and bottom edges we're
	// left with a 1-pixel border.
	//
	DownshiftAndXor(buffer1, buffer2);

	// Do a morphological erosion operation with 1-pixel radius on the
	// resulting mask. This will "erode away" the 1-pixel top/bottom
	// borders of the non-interlaced areas, and will get rid various
	// other small leftover junk as well.
	//
	ErodeHorizontal(buffer2, buffer1);
	ErodeVertical(buffer1, buffer2);

	// Do a morphological dilate operation with 1-pixel radius on the
	// resulting mask to "grow back" the original interlaced areas.
	//
	DilateHorizontal(buffer2, buffer1);
	DilateVertical(buffer1, buffer2);

	// Now we have a bitmask that has large contiguous areas filled with 1s
	// where we need to perform the deinterlacing. We'll combine the
	// original image with this mask (we'll fill the "missing" black lines
	// with the content above them), and apply an optional scaling factor to
	// the "reconstructed" lines. Dimming the reconstructed lines a bit
	// gives the deinterlaced image the illusion a higher resolution
	// compared to just duplicating the lines. In fact, this is not even
	// merely an illusion because the dimmed lines effectively introduce an
	// anti-aliasing effect.
	//
	CombineOutput(image.data, buffer2, strength);

	if (process_in_place) {
		return input_image;

	} else {
		// Create a new `RenderedImage` if we didn't process the input
		// image in-place
		constexpr auto BytesPerPixel = sizeof(uint32_t);

		// This won't copy the pixel data, just the container
		RenderedImage new_image = input_image;

		// We're always outputting a BGRX32 image if we had to decode it
		new_image.params.pixel_format = PixelFormat::BGRX32_ByteArray;
		new_image.pitch               = image.width * BytesPerPixel;

		new_image.image_data = reinterpret_cast<uint8_t*>(
		        decoded_image.data());

		std::memcpy(new_image.image_data,
		            image.data,
		            image.width * image.height * BytesPerPixel);

		return new_image;
	}
}

RenderedImage Deinterlacer::Deinterlace(const RenderedImage& input_image,
                                        const DeinterlacingStrength strength)
{
	const auto mode = input_image.params.video_mode;

	if (mode.is_graphics_mode &&
	    (mode.color_depth >= ColorDepth::IndexedColor256) &&
	    (mode.height >= 400)) {

		return LineDeinterlace(input_image, strength);
	} else {
		return input_image;
	}
}
