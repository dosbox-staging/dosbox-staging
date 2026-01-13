// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/mem_unaligned.h"

static void conc2d(SCALERNAME, SBPP)(const void* s)
{
	// Clear the complete line marker
	int had_change = 0;
	auto src       = static_cast<const SRCTYPE*>(s);
	auto cache     = reinterpret_cast<SRCTYPE*>(render.scale.cache_read);

	render.scale.cache_read += render.scale.cache_pitch;

	auto line0 = reinterpret_cast<uint32_t*>(render.scale.out_write);

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
			line0 += 4 * SCALERWIDTH;
#else

	constexpr auto PixelsPerStep = sizeof(uint64_t) / sizeof(SRCTYPE);

	for (int x = render.src.width; x > 0;) {
		const auto src_ptr = reinterpret_cast<const uint8_t*>(src);
		const auto src_val = read_unaligned_uint64(src_ptr);

		const auto cache_ptr = reinterpret_cast<uint8_t*>(cache);
		const auto cache_val = read_unaligned_uint64(cache_ptr);

		if (src_val == cache_val) {
			x -= PixelsPerStep;
			src += PixelsPerStep;

			cache += PixelsPerStep;
			line0 += PixelsPerStep * SCALERWIDTH;
#endif
		} else {
#if (SCALERHEIGHT > 1)
			auto line1 = reinterpret_cast<uint32_t*>(
			        reinterpret_cast<uint8_t*>(line0) +
			        render.scale.out_pitch);
#endif
			had_change = 1;

			// If there's a difference, convert up to 32 pixels
			// before diffing again
			for (int i = ((x > 32) ? 32 : x); i > 0;) {
				const SRCTYPE S = *src;

				*cache = S;
				++src;
				++cache;

				const uint32_t P = PMAKE(S);
				SCALERFUNC;

				line0 += SCALERWIDTH;
#if (SCALERHEIGHT > 1)
				line1 += SCALERWIDTH;
#endif
				--x;
				--i;
			}
		}
	}

	scaler_add_lines(had_change, render.scale.y_scale);
}
