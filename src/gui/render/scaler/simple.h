// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/mem_unaligned.h"

#if SCALER_MAX_MUL_HEIGHT < SCALERHEIGHT
#error "Scaler goes too high"
#endif

#if SCALER_MAX_MUL_WIDTH < SCALERWIDTH
#error "Scaler goes too wide"
#endif

static void conc2d(SCALERNAME, SBPP)(const void* s)
{
	/* Clear the complete line marker */
	Bitu hadChange = 0;
	auto src       = static_cast<const SRCTYPE*>(s);
	auto cache     = reinterpret_cast<SRCTYPE*>(render.scale.cacheRead);

	render.scale.cacheRead += render.scale.cachePitch;

	PTYPE* line0 = (PTYPE*)(render.scale.outWrite);
#if (SBPP == 9)
	for (Bits x = render.src.width; x > 0;) {
		if (std::memcmp(src, cache, sizeof(uint32_t)) == 0 &&
		    (render.pal.modified[src[0]] | render.pal.modified[src[1]] |
		     render.pal.modified[src[2]] | render.pal.modified[src[3]]) == 0) {

			x -= 4;
			src += 4;
			cache += 4;
			line0 += 4 * SCALERWIDTH;
#else
	constexpr uint8_t address_step = sizeof(Bitu) / sizeof(SRCTYPE);

	for (Bits x = render.src.width; x > 0;) {
		const auto src_ptr = reinterpret_cast<const uint8_t*>(src);
		const auto src_val = read_unaligned_size_t(src_ptr);

		const auto cache_ptr = reinterpret_cast<uint8_t*>(cache);
		const auto cache_val = read_unaligned_size_t(cache_ptr);

		if (src_val == cache_val) {
			x -= address_step;
			src += address_step;

			cache += address_step;
			line0 += address_step * SCALERWIDTH;
#endif
		} else {
#if (SCALERHEIGHT > 1)
			PTYPE* line1 = (PTYPE*)(((uint8_t*)line0) +
			                        render.scale.outPitch);
#endif
			hadChange = 1;
			for (Bitu i = ((x > 32) ? 32 : x); i > 0; i--, x--) {
				const SRCTYPE S = *src;

				*cache = S;
				src++;
				cache++;

				const PTYPE P = PMAKE(S);
				SCALERFUNC;

				line0 += SCALERWIDTH;
#if (SCALERHEIGHT > 1)
				line1 += SCALERWIDTH;
#endif
			}
		}
	}

	Bitu scaleLines = Scaler_Aspect[render.scale.outLine++];
	if (scaleLines - SCALERHEIGHT && hadChange) {

		BituMove(render.scale.outWrite + render.scale.outPitch * SCALERHEIGHT,
		         render.scale.outWrite +
		                 render.scale.outPitch * (SCALERHEIGHT - 1),
		         render.src.width * SCALERWIDTH * PSIZE);
	}

	ScalerAddLines(hadChange, scaleLines);
}
