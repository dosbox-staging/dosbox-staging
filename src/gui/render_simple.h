/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "mem_unaligned.h"

#if SCALER_MAX_MUL_HEIGHT < SCALERHEIGHT
#error "Scaler goes too high"
#endif

#if SCALER_MAX_MUL_WIDTH < SCALERWIDTH 
#error "Scaler goes too wide"
#endif

#if defined (SCALERLINEAR)
static void conc4d(SCALERNAME,SBPP,DBPP,L)(const void *s) {
#else
static void conc4d(SCALERNAME,SBPP,DBPP,R)(const void *s) {
#endif
#ifdef RENDER_NULL_INPUT
	if (!s) {
		render.scale.cacheRead += render.scale.cachePitch;
#if defined(SCALERLINEAR) 
		Bitu skipLines = SCALERHEIGHT;
#else
		Bitu skipLines = Scaler_Aspect[ render.scale.outLine++ ];
#endif
		ScalerAddLines( 0, skipLines );
		return;
	}
#endif
	/* Clear the complete line marker */
	Bitu hadChange = 0;
	const SRCTYPE *src = (SRCTYPE*)s;
	SRCTYPE *cache = (SRCTYPE*)(render.scale.cacheRead);
	render.scale.cacheRead += render.scale.cachePitch;
	PTYPE * line0=(PTYPE *)(render.scale.outWrite);
#if (SBPP == 9)
	for (Bits x=render.src.width;x>0;) {
		if (*(Bit32u const*)src == *(Bit32u*)cache && !(
			render.pal.modified[src[0]] | 
			render.pal.modified[src[1]] | 
			render.pal.modified[src[2]] | 
			render.pal.modified[src[3]] )) {
			x-=4;
			src+=4;
			cache+=4;
			line0+=4*SCALERWIDTH;
#else
	constexpr uint8_t address_step = sizeof(Bitu) / sizeof(SRCTYPE);

	for (Bits x = render.src.width; x > 0;) {
		const auto src_ptr = reinterpret_cast<const uint8_t *>(src);
		const auto src_val = read_unaligned_size_t(src_ptr);

		const auto cache_ptr = reinterpret_cast<uint8_t *>(cache);
		const auto cache_val = read_unaligned_size_t(cache_ptr);

		if (src_val == cache_val) {
			x -= address_step;
			src += address_step;
			cache += address_step;
			line0 += address_step * SCALERWIDTH;
#endif
		} else {
#if defined(SCALERLINEAR)
#if (SCALERHEIGHT > 1) 
			PTYPE *line1 = WC[0];
#endif
#if (SCALERHEIGHT > 2) 
			PTYPE *line2 = WC[1];
#endif
#if (SCALERHEIGHT > 3) 
			PTYPE *line3 = WC[2];
#endif
#if (SCALERHEIGHT > 4) 
			PTYPE *line4 = WC[3];
#endif
#else
#if (SCALERHEIGHT > 1) 
		PTYPE *line1 = (PTYPE *)(((Bit8u*)line0)+ render.scale.outPitch);
#endif
#if (SCALERHEIGHT > 2) 
		PTYPE *line2 = (PTYPE *)(((Bit8u*)line0)+ render.scale.outPitch * 2);
#endif
#if (SCALERHEIGHT > 3) 
		PTYPE *line3 = (PTYPE *)(((Bit8u*)line0)+ render.scale.outPitch * 3);
#endif
#if (SCALERHEIGHT > 4) 
		PTYPE *line4 = (PTYPE *)(((Bit8u*)line0)+ render.scale.outPitch * 4);
#endif
#endif //defined(SCALERLINEAR)
			hadChange = 1;
			for (Bitu i = x > 32 ? 32 : x;i>0;i--,x--) {
				const SRCTYPE S = *src;
				*cache = S;
				src++;cache++;
				const PTYPE P = PMAKE(S);
				SCALERFUNC;
				line0 += SCALERWIDTH;
#if (SCALERHEIGHT > 1) 
				line1 += SCALERWIDTH;
#endif
#if (SCALERHEIGHT > 2) 
				line2 += SCALERWIDTH;
#endif
#if (SCALERHEIGHT > 3) 
				line3 += SCALERWIDTH;
#endif
#if (SCALERHEIGHT > 4) 
				line4 += SCALERWIDTH;
#endif
			}
#if defined(SCALERLINEAR)
#if (SCALERHEIGHT > 1)
			Bitu copyLen = (Bitu)((Bit8u*)line1 - (Bit8u*)WC[0]);
			BituMove(((Bit8u*)line0)-copyLen+render.scale.outPitch  ,WC[0], copyLen );
#endif
#if (SCALERHEIGHT > 2) 
			BituMove(((Bit8u*)line0)-copyLen+render.scale.outPitch*2,WC[1], copyLen );
#endif
#if (SCALERHEIGHT > 3) 
			BituMove(((Bit8u*)line0)-copyLen+render.scale.outPitch*3,WC[2], copyLen );
#endif
#if (SCALERHEIGHT > 4) 
			BituMove(((Bit8u*)line0)-copyLen+render.scale.outPitch*4,WC[3], copyLen );
#endif

#endif //defined(SCALERLINEAR)
		}
	}
#if defined(SCALERLINEAR) 
	Bitu scaleLines = SCALERHEIGHT;
#else
	Bitu scaleLines = Scaler_Aspect[ render.scale.outLine++ ];
	if ( scaleLines - SCALERHEIGHT && hadChange ) {
		BituMove( render.scale.outWrite + render.scale.outPitch * SCALERHEIGHT,
			render.scale.outWrite + render.scale.outPitch * (SCALERHEIGHT-1),
			render.src.width * SCALERWIDTH * PSIZE);
	}
#endif
	ScalerAddLines( hadChange, scaleLines );
}

#if !defined(SCALERLINEAR) 
#define SCALERLINEAR 1
#include "render_simple.h"
#undef SCALERLINEAR
#endif
