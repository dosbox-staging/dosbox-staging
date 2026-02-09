// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/deinterlacer.h"

#include <vector>

#include "gui/render/render.h"
#include "misc/image_decoder.h"
#include "utils/checks.h"
#include "utils/mem_unaligned.h"

CHECK_NARROWING();

uint32_t Deinterlacer::DetectBackgroundColor(const uint32_t* pixel_data) const
{
	assert(pixel_data);
	assert(image.width > 0);
	assert(image.height >= 10);

	auto in_line = pixel_data;

	// Some games might use some other background colour than RGB(0,0,0);
	// e.g., Crusader: No Regret sets it to RGB(8,8,8), and the "black
	// scanlines" also have this color instead of 100% black.
	//
	// At least in this game, the widescreen video starts at line 120, and
	// lines 0-119 contain the background colour. We can turn this knowledge
	// into a detection heuristics: we assume the top left pixel color is
	// the background colour, and if the next 10 lines are filled with this
	// color, we conclude this must be the background colour.
	//
	// We might need to improve our heuristics if we come across similar
	// games with non-black backgrounds that break these assumptions.
	//
	const auto top_left_pixel_color = *in_line;

	// The fastest way is to create a row's worth of data filled with our
	// background colour candidate, and then memcmp it to the actual rows.
	std::array<uint32_t, ScalerMaxWidth> bg_color_row = {};
	for (auto x = 0; x < image.width; ++x) {
		bg_color_row[x] = top_left_pixel_color;
	}

	// Are the top first 10 lines filled with the background colour?
	auto bg_color_detected = [&] {
		for (auto y = 0; y < 10; ++y) {
			if (std::memcmp(bg_color_row.data(),
			                in_line + (image.pitch_pixels * y),
			                image.width * sizeof(uint32_t)) != 0) {

				return false;
			}
		}
		return true;
	}();

	// Return the detected colour if we succeeded, or revert to black
	constexpr uint32_t Black = 0;
	return bg_color_detected ? top_left_pixel_color : Black;
}

void Deinterlacer::ThresholdInput(const uint32_t* src, bit_buffer& dest,
                                  const uint32_t bg_color) const
{
	assert(src);

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
				        static_cast<uint8_t>(((a1 != bg_color) << 0)) |
				        static_cast<uint8_t>(((a2 != bg_color) << 1)) |
				        static_cast<uint8_t>(((a3 != bg_color) << 2)) |
				        static_cast<uint8_t>(((a4 != bg_color) << 3)) |
				        static_cast<uint8_t>(((a5 != bg_color) << 4)) |
				        static_cast<uint8_t>(((a6 != bg_color) << 5)) |
				        static_cast<uint8_t>(((a7 != bg_color) << 6)) |
				        static_cast<uint8_t>(((a8 != bg_color) << 7));

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

static int to_rgb_scale_factor_linear(const DeinterlacingStrength strength)
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

static int to_rgb_scale_factor_dot(const DeinterlacingStrength strength)
{
	using enum DeinterlacingStrength;

	switch (strength) {
	case Light: return 193;  // ~0.75 scaling
	case Medium: return 234; // ~0.91 scaling
	case Strong: return 245; // ~0.96 scaling
	case Full: return 256;   // 1.0x scaling (no scaling)
	default:
		assertm(false, "Invalid DeinterlacingStrength value");
		return 0;
	}
}

void Deinterlacer::CombineOutput(uint32_t* pixel_data, bit_buffer& mask,
                                 const DeinterlacingStrength strength) const
{
	assert(pixel_data);

	std::vector<uint32_t> src(image.width * image.height);

	constexpr auto BytesPerPixel = sizeof(uint32_t);
	std::memcpy(src.data(), pixel_data, image.width * image.height * BytesPerPixel);

	auto in_line = src.data();

	// Start writing to the destination image from the second line
	auto out_line = pixel_data + image.pitch_pixels;

	// Start reading the mask from the second line
	auto mask_line = mask.data() + BufferOffset + buffer_pitch * 2;

	const auto rgb_scale_factor = to_rgb_scale_factor_linear(strength);

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
	assert(input_image.image_data);
	assert(input_image.params.width > 0);
	assert(input_image.params.height > 0);

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

	// Attempt to detect the background colour of the input image based on
	// some heuristics.
	const auto bg_color = DetectBackgroundColor(image.data);

	// Run a threshold pass on the original image to generate a 1-bit
	// mask. The mask bitplane is 0 for black pixels and 1 for non-black
	// pixels. Interlaced areas will show up as alternating lines of 1s
	// and 0s.
	ThresholdInput(image.data, buffer1, bg_color);

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
	for (auto i = 0; i < 2; ++i) {
		ErodeHorizontal(buffer2, buffer1);
		ErodeVertical(buffer1, buffer2);
	}

	// Do a morphological dilate operation with 1-pixel radius on the
	// resulting mask to "grow back" the original interlaced areas.
	//
	for (auto i = 0; i < 2; ++i) {
		DilateHorizontal(buffer2, buffer1);
		DilateVertical(buffer1, buffer2);
	}

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

// Automatically deinterlace "dot pattern" FMV videos in the CD-ROM version
// of KGB and Dune. Non-interlaced areas will be left intact.
//
RenderedImage Deinterlacer::DotDeinterlace(const RenderedImage& input_image,
                                           const DeinterlacingStrength strength)
{
	assert(input_image.image_data);
	assert(input_image.params.width > 0);
	assert(input_image.params.height > 0);

	const auto& p = input_image.params;

	// Convert input image to 32-bit BGRX format and get rid of
	// "baked-in" pixel and line doubling. We do this because it's much
	// easier to perform deinterlacing on the "raw" output without any
	// width/height doubling, then reconstruct the width/height doubled
	// image at the output stage.
	//
	const auto pixel_skip_count = (p.width / p.video_mode.width) - 1;
	const auto row_skip_count   = (p.height / p.video_mode.height) - 1;

	image.width        = input_image.params.width / (pixel_skip_count + 1);
	image.height       = input_image.params.height / (row_skip_count + 1);
	image.pitch_pixels = image.width;

	// We'll decode two rows at a time
	decoded_image.resize(image.width * 2);

	ImageDecoder image_decoder(input_image, row_skip_count, pixel_skip_count);

	const auto rgb_scale_factor = to_rgb_scale_factor_dot(strength);

	// We're updating the input image in-place
	uint8_t* out_line    = input_image.image_data;
	const auto out_pitch = input_image.pitch;

	// The "dot deinterlacing" pattern looks like this:
	//
	//   row 1   P . P . P . P .
	//   row 2   . . . . . . . .
	//   row 3   P . P . P . P .
	//   row 4   . . . . . . . .
	//   ...
	//
	// 'P' represents a pixel that is either fully black (rgb(0,0,0)) or
	// not black.
	//
	// '.' represents fully black pixels (rgb(0,0,0))
	//
	// We decode and process the image in odd & even row-pairs (rows 1 & 2,
	// rows 3 & 4, etc.) If we can detect the above pattern on the current
	// row-pair, we do the deinterlacing and write the result back to the
	// input image. If we can't detect the pattern, we stop the process.
	//
	// This works because "dot interlaced" FMV content in KGB and Dune is
	// full-width and starts from the top of the screen. So we start
	// deinterlacing from the top and keep going until dot pattern can be
	// detected. This leaves the subtitles below the FMV video intact.
	//
	const auto double_width  = (pixel_skip_count != 0);
	const auto double_height = (row_skip_count != 0);

	// Start processing rows from the top and skip black rows until we reach
	// 1/3 of the screen height. The assumption is the interlaced image
	// should start in the top third of the screen.
	auto y = 0;

	while (y < image.height / 3) {
		auto in_line = decoded_image.begin();
		image_decoder.GetNextRowAsBgrx32Pixels(in_line);
		++y;

		uint64_t sum_pixels = 0;
		for (auto x = 0; x < image.width; ++x) {
			sum_pixels += *(in_line + x);
		}
		if (sum_pixels == 0) {
			// Advance the write position
			out_line += out_pitch * (double_height ? 2 : 1);
		} else {
			break;
		}
	}

	// Decode and process the image in row-pairs until we can detect the dot
	// pattern
	//
	bool first_iteration = true;

	while (y < (image.height - 1)) {
		// Decode row N, except for the first iteration
		auto in_line = decoded_image.begin();
		if (!first_iteration) {
			image_decoder.GetNextRowAsBgrx32Pixels(in_line);
			++y;
		} else {
			first_iteration = false;
		}

		uint64_t odd_line_sum_odd_pixels = 0;
		for (auto x = 1; x < image.width; x += 2) {
			odd_line_sum_odd_pixels += *(in_line + x);
		}
		if (odd_line_sum_odd_pixels > 0) {
			// Pattern doesn't fit, stop deinterlacing
			break;
		}

		// Decode row N+1
		in_line = decoded_image.begin() + image.width;
		image_decoder.GetNextRowAsBgrx32Pixels(in_line);
		++y;

		uint64_t even_line_sum_all_pixels = 0;
		for (auto x = 0; x < image.width; ++x) {
			even_line_sum_all_pixels += *(in_line + x);
		}
		if (even_line_sum_all_pixels > 0) {
			// Pattern doesn't fit, stop deinterlacing
			break;
		}

		// Write the two deinterlaced lines to the input image in-place,
		// taking the width/height doubling of the input image into
		// account.
		//
		in_line = decoded_image.begin();

		if (!double_width && !double_height) {
			WriteDotDeinterlacedOutput2x2(in_line,
			                              out_line,
			                              out_pitch,
			                              rgb_scale_factor);

		} else if (double_width && !double_height) {
			WriteDotDeinterlacedOutput4x2(in_line,
			                              out_line,
			                              out_pitch,
			                              rgb_scale_factor);

		} else if (!double_width && double_height) {
			WriteDotDeinterlacedOutput2x4(in_line,
			                              out_line,
			                              out_pitch,
			                              rgb_scale_factor);

		} else if (double_width && double_height) {
			WriteDotDeinterlacedOutput4x4(in_line,
			                              out_line,
			                              out_pitch,
			                              rgb_scale_factor);
		}
	}

	return input_image;
}

void Deinterlacer::WriteDotDeinterlacedOutput2x2(
        std::vector<uint32_t>::const_iterator in_line, uint8_t*& out_line,
        int out_pitch, int rgb_scale_factor) const
{
	auto out = out_line;

	// Upscale top-left input pixel P to a 2x2 area (P is are already
	// in the input buffer, so no need to write it again)
	//
	// row 1 -  P   1b
	// row 2 -  2a  2b

	for (auto x = 0; x < image.width; x += 2) {
		const auto p1 = *(in_line + x);

		// row 1
		write_unaligned_uint32(out + out_pitch * 0 + 4, p1); // pixel 1b

		const auto p2 = scale_rgb(p1, rgb_scale_factor);

		// row 2
		write_unaligned_uint32(out + out_pitch * 1 + 0, p2); // pixel 2a
		write_unaligned_uint32(out + out_pitch * 1 + 4, p2); // pixel 2b

		out += 8;
	}

	out_line += out_pitch * 2;
}

void Deinterlacer::WriteDotDeinterlacedOutput2x4(
        std::vector<uint32_t>::const_iterator in_line, uint8_t*& out_line,
        int out_pitch, int rgb_scale_factor) const
{
	auto out = out_line;

	// Upscale top-left input pixel P to a 2x4 area (all P pixels are
	// already in the input buffer, so no need to write them again)
	//
	// row 1 -  P   1b
	// row 2 -  P   2b
	// row 3 -  3a  3b
	// row 4 -  4a  4b

	for (auto x = 0; x < image.width; x += 2) {
		const auto p1 = *(in_line + x);

		// row 1
		write_unaligned_uint32(out + out_pitch * 0 + 4, p1); // pixel 1b

		// row 2
		write_unaligned_uint32(out + out_pitch * 1 + 4, p1); // pixel 2b

		const auto p2 = scale_rgb(p1, rgb_scale_factor);

		// row 3
		write_unaligned_uint32(out + out_pitch * 2 + 0, p2); // pixel 3a
		write_unaligned_uint32(out + out_pitch * 2 + 4, p2); // pixel 3b

		// row 4
		write_unaligned_uint32(out + out_pitch * 3 + 0, p2); // pixel 4a
		write_unaligned_uint32(out + out_pitch * 3 + 4, p2); // pixel 4b

		out += 8;
	}

	out_line += out_pitch * 4;
}

void Deinterlacer::WriteDotDeinterlacedOutput4x2(
        std::vector<uint32_t>::const_iterator in_line, uint8_t*& out_line,
        int out_pitch, int rgb_scale_factor) const
{
	auto out = out_line;

	// Upscale top-left input pixel P to a 4x2 area (all P pixels are
	// already in the input buffer, so no need to write them again)
	//
	// row 1 -  P   P   1c  1d
	// row 2 -  2a  2b  2c  2d

	for (auto x = 0; x < image.width; x += 2) {
		const auto p1 = *(in_line + x);

		// row 1
		write_unaligned_uint32(out + out_pitch * 0 + 8, p1); // pixel 1c
		write_unaligned_uint32(out + out_pitch * 0 + 12, p1); // pixel 1d

		const auto p2 = scale_rgb(p1, rgb_scale_factor);

		// row 2
		write_unaligned_uint32(out + out_pitch * 1 + 0, p2); // pixel 2a
		write_unaligned_uint32(out + out_pitch * 1 + 4, p2); // pixel 2b
		write_unaligned_uint32(out + out_pitch * 1 + 8, p2); // pixel 2c
		write_unaligned_uint32(out + out_pitch * 1 + 12, p2); // pixel 2d

		out += 16;
	}

	out_line += out_pitch * 2;
}

void Deinterlacer::WriteDotDeinterlacedOutput4x4(
        std::vector<uint32_t>::const_iterator in_line, uint8_t*& out_line,
        int out_pitch, int rgb_scale_factor) const
{
	auto out = out_line;

	// Upscale top-left input pixel P to a 4x4 area (all P pixels are
	// already in the input buffer, so no need to write them again)
	//
	// row 1 -  P   P   1c  1d
	// row 2 -  P   P   2c  2d
	// row 3 -  3a  3b  3c  3d
	// row 4 -  4a  4b  4c  4d

	for (auto x = 0; x < image.width; x += 2) {
		const auto p1 = *(in_line + x);

		// row 1
		write_unaligned_uint32(out + out_pitch * 0 + 8, p1); // pixel 1c
		write_unaligned_uint32(out + out_pitch * 0 + 12, p1); // pixel 1d

		// row 2
		write_unaligned_uint32(out + out_pitch * 1 + 8, p1); // pixel 2c
		write_unaligned_uint32(out + out_pitch * 1 + 12, p1); // pixel 2d

		const auto p2 = scale_rgb(p1, rgb_scale_factor);

		// row 3
		write_unaligned_uint32(out + out_pitch * 2 + 0, p2); // pixel 3a
		write_unaligned_uint32(out + out_pitch * 2 + 4, p2); // pixel 3b
		write_unaligned_uint32(out + out_pitch * 2 + 8, p2); // pixel 3c
		write_unaligned_uint32(out + out_pitch * 2 + 12, p2); // pixel 3d

		// row 4
		write_unaligned_uint32(out + out_pitch * 3 + 0, p2); // pixel 4a
		write_unaligned_uint32(out + out_pitch * 3 + 4, p2); // pixel 4b
		write_unaligned_uint32(out + out_pitch * 3 + 8, p2); // pixel 4c
		write_unaligned_uint32(out + out_pitch * 3 + 12, p2); // pixel 4d

		out += 16;
	}

	out_line += out_pitch * 4;
}

RenderedImage Deinterlacer::Deinterlace(const RenderedImage& input_image,
                                        const DeinterlacingStrength strength)
{
	const auto mode = input_image.params.video_mode;

	if (mode.is_graphics_mode &&
	    (mode.color_depth >= ColorDepth::IndexedColor256) &&
	    (mode.height >= 400)) {

		// Regular deinterlacing of 400+ line 256-colour,
		// high-colour, and true colour modes.
		return LineDeinterlace(input_image, strength);

	} else if (mode.bios_mode_number == 0x13) {
		// Special processing for KGB and Dune CD-ROM versions
		// that use the weird "dot pattern interlacing" in the
		// 320x200 13h VGA mode.
		return DotDeinterlace(input_image, strength);

	} else {
		return input_image;
	}
}
