// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/mem_unaligned.h"

static void conc2d(SCALERNAME, SBPP)(const void* src_line_data)
{
	// Clear the complete line marker
	int had_change = 0;

	// `src_line_data` contains a scanline worth of pixel data.
	//
	// SRCTYPE can be uint8_t, uint16_t, Rgb888 (packed 3-byte struct), and
	// uint32_t.
	//
	// All screen mode widths are multiples of 8, therefore we can access
	// this data one SRCTYPE at a time, regardless of pixel format (pixel
	// can be stored on 1 to 4 bytes).
	//
	auto src = static_cast<const SRCTYPE*>(src_line_data);

	// Same goes for the line cache data which contains the scanline without
	// any extra padding.
	auto cache = reinterpret_cast<SRCTYPE*>(render.scale.cache_read);

	render.scale.cache_read += render.scale.cache_pitch;

	// `out_write` points to a buffer aligned to an 8-byte boundary at
	// least, and the output pixel format is 32-bit BGRX.
	//
	auto out_line0 = reinterpret_cast<uint32_t*>(render.scale.out_write);

#if (SBPP == 9)
	for (int x = render.src.width; x > 0;) {

		if (std::memcmp(src, cache, sizeof(uint32_t)) == 0 &&
		    (render.palette.modified[src[0]] |
		     render.palette.modified[src[1]] |
		     render.palette.modified[src[2]] |
		     render.palette.modified[src[3]]) == 0) {

			x -= 4;
			src += 4;
			cache += 4;
			out_line0 += 4 * SCALERWIDTH;
#else

	// SRCTYPE can be uint8_t, uint16_t, Rgb888 (packed 3-byte struct), and
	// uint32_t.
	constexpr auto BytesPerPixel = sizeof(SRCTYPE);

	// From the above follows for the BGR24 (24-bit true color) pixel
	// format, `PixelPerStep` will be 2 (8 div 3), but we'll be comparing
	// the cache 2.66 pixels at a time (with an "overhang" into the 3rd next
	// pixel).
	//
	// Because neither the scanline and therefore nor the cache have extra end
	// of row padding bytes (rows are tightly packed), this will sometimes
	// trigger a "false difference" when comparing the last two pixels of a
	// row. But in practice this is not a big deal because we're at the end of
	// the line anyway.
	//
	// Both the scanline source data and cache buffers are guaranteed to have a
	// couple of extra bytes at the end of the rows even in the highest
	// resolutions, so we can read a few bytes past the end safely.
	//
	constexpr auto PixelsPerStep = sizeof(uint64_t) / BytesPerPixel;

	for (int x = render.src.width; x > 0;) {
		// See comment at the top of this function why reading the
		// source and writing to the case 8 bytes at a time is safe.
		//
		const auto src_ptr = reinterpret_cast<const uint8_t*>(src);
		const auto src_val = read_unaligned_uint64(src_ptr);

		const auto cache_ptr = reinterpret_cast<uint8_t*>(cache);
		const auto cache_val = read_unaligned_uint64(cache_ptr);

		if (src_val == cache_val) {
			x -= PixelsPerStep;
			src += PixelsPerStep;

			cache += PixelsPerStep;

			// SCALERWIDTH is 1 with no pixel doubling, and 2 with
			// pixel doubling enabled.
			out_line0 += PixelsPerStep * SCALERWIDTH;
#endif
		} else {
#if (SCALERHEIGHT > 1)
			auto out_line1 = reinterpret_cast<uint32_t*>(
			        reinterpret_cast<uint8_t*>(out_line0) +
			        render.scale.out_pitch);
#endif
			had_change = 1;

			// If there's a difference between the current and
			// previous frame in this scanline for this group of
			// pixels, convert up to 32 pixels before starting
			// diffing again (this is an optimisation step to speed
			// up the diffing; there's no need to be super exact and
			// compare every single pixel).
			//
			for (int i = ((x > 32) ? 32 : x); i > 0;) {
				const SRCTYPE S = *src;

				*cache = S;
				++src;
				++cache;

				const uint32_t P = PMAKE(S);
				SCALERFUNC;

				out_line0 += SCALERWIDTH;
#if (SCALERHEIGHT > 1)
				out_line1 += SCALERWIDTH;
#endif
				--x;
				--i;
			}
		}
	}

	scaler_add_lines(had_change, render.scale.y_scale);
}
