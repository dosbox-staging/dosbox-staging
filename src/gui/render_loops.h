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

#if defined (SCALERLINEAR)
static void conc3d(SCALERNAME,SBPP,L)(void) {
#else
static void conc3d(SCALERNAME,SBPP,R)(void) {
#endif
//Skip the first one for multiline input scalers
	if (!render.scale.outLine) {
		render.scale.outLine++;
		return;
	}
lastagain:
	if (!CC[render.scale.outLine][0]) {
#if defined(SCALERLINEAR) 
		Bitu scaleLines = SCALERHEIGHT;
#else
		Bitu scaleLines = Scaler_Aspect[ render.scale.outLine ];
#endif
		ScalerAddLines( 0, scaleLines );
		if (++render.scale.outLine == render.scale.inHeight)
			goto lastagain;
		return;
	}
	/* Clear the complete line marker */
	CC[render.scale.outLine][0] = 0;
	const PTYPE * fc = &FC[render.scale.outLine][1];
	PTYPE * line0=(PTYPE *)(render.scale.outWrite);
	uint8_t * changed = &CC[render.scale.outLine][1];
	Bitu b;
	for (b=0;b<render.scale.blocks;b++) {
#if (SCALERHEIGHT > 1)
		PTYPE* line1;
#endif
		/* Clear this block being dirty marker */
		const Bitu changeType = changed[b];
		changed[b] = 0;
		switch (changeType) {
		case 0:
			line0 += SCALERWIDTH * SCALER_BLOCKSIZE;
			fc += SCALER_BLOCKSIZE;
			continue;
		case SCALE_LEFT:
#if (SCALERHEIGHT > 1)
			line1 = (PTYPE*)(((uint8_t*)line0) + render.scale.outPitch);
#endif
			SCALERFUNC;
			line0 += SCALERWIDTH * SCALER_BLOCKSIZE;
			fc += SCALER_BLOCKSIZE;
			break;
		case SCALE_LEFT | SCALE_RIGHT:
#if (SCALERHEIGHT > 1)
			line1 = (PTYPE*)(((uint8_t*)line0) + render.scale.outPitch);
#endif
			SCALERFUNC;
			[[fallthrough]];
		case SCALE_RIGHT:
#if (SCALERHEIGHT > 1)
			line1 = (PTYPE*)(((uint8_t*)line0) + render.scale.outPitch);
#endif
			fc += SCALER_BLOCKSIZE -1;
			SCALERFUNC;
			line0 += SCALERWIDTH;
			fc++;
			break;
		default:
#if defined(SCALERLINEAR)
#if (SCALERHEIGHT > 1) 
			line1 = WC[0];
#	endif
#else
#if (SCALERHEIGHT > 1) 
			line1 = (PTYPE *)(((uint8_t*)line0)+ render.scale.outPitch);
#	endif
#endif //defined(SCALERLINEAR)
			for (Bitu i = 0; i<SCALER_BLOCKSIZE;i++) {
				SCALERFUNC;
				line0 += SCALERWIDTH;
#if (SCALERHEIGHT > 1)
				line1 += SCALERWIDTH;
#endif
				fc++;
			}
#if defined(SCALERLINEAR)
#if (SCALERHEIGHT > 1) 
			BituMove((uint8_t*)(&line0[-SCALER_BLOCKSIZE*SCALERWIDTH])+render.scale.outPitch  ,WC[0], SCALER_BLOCKSIZE *SCALERWIDTH*PSIZE);
#	endif
#endif //defined(SCALERLINEAR)
			break;
		}
	}
#if defined(SCALERLINEAR) 
	Bitu scaleLines = SCALERHEIGHT;
#else
	Bitu scaleLines = Scaler_Aspect[ render.scale.outLine ];
	if ( ((Bits)(scaleLines - SCALERHEIGHT)) > 0 ) {
		BituMove( render.scale.outWrite + render.scale.outPitch * SCALERHEIGHT,
			render.scale.outWrite + render.scale.outPitch * (SCALERHEIGHT-1),
			render.src.width * SCALERWIDTH * PSIZE);
	}
#endif
	ScalerAddLines( 1, scaleLines );
	if (++render.scale.outLine == render.scale.inHeight)
		goto lastagain;
}

#if !defined(SCALERLINEAR) 
#define SCALERLINEAR 1
#include "render_loops.h"
#undef SCALERLINEAR
#endif
