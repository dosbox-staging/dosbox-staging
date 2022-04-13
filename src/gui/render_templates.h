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

#if DBPP == 8
#define PSIZE 1
#define PTYPE uint8_t
#define WC scalerWriteCache.b8
//#define FC scalerFrameCache.b8
#define FC (*(scalerFrameCache_t*)(&scalerSourceCache.b32[400][0])).b8
#define redMask		0
#define	greenMask	0
#define blueMask	0
#define redBits		0
#define greenBits	0
#define blueBits	0
#define redShift	0
#define greenShift	0
#define blueShift	0
#elif DBPP == 15 || DBPP == 16
#define PSIZE 2
#define PTYPE uint16_t
#define WC scalerWriteCache.b16
//#define FC scalerFrameCache.b16
#define FC (*(scalerFrameCache_t*)(&scalerSourceCache.b32[400][0])).b16
#if DBPP == 15
#define	redMask		0x7C00
#define	greenMask	0x03E0
#define	blueMask	0x001F
#define redBits		5
#define greenBits	5
#define blueBits	5
#define redShift	10
#define greenShift	5
#define blueShift	0
#elif DBPP == 16
#define redMask		0xF800
#define greenMask	0x07E0
#define blueMask	0x001F
#define redBits		5
#define greenBits	6
#define blueBits	5
#define redShift	11
#define greenShift	5
#define blueShift	0
#endif
#elif DBPP == 32
#define PSIZE 4
#define PTYPE uint32_t
#define WC scalerWriteCache.b32
//#define FC scalerFrameCache.b32
#define FC (*(scalerFrameCache_t*)(&scalerSourceCache.b32[400][0])).b32
#define redMask		0xff0000
#define greenMask	0x00ff00
#define blueMask	0x0000ff
#define redBits		8
#define greenBits	8
#define blueBits	8
#define redShift	16
#define greenShift	8
#define blueShift	0
#endif

#define redblueMask (redMask | blueMask)


#if SBPP == 8 || SBPP == 9
#define SC scalerSourceCache.b8
#if DBPP == 8
#define PMAKE(_VAL) (_VAL)
#elif DBPP == 15
#define PMAKE(_VAL) render.pal.lut.b16[_VAL]
#elif DBPP == 16
#define PMAKE(_VAL) render.pal.lut.b16[_VAL]
#elif DBPP == 32
#define PMAKE(_VAL) render.pal.lut.b32[_VAL]
#endif
#define SRCTYPE uint8_t
#endif

#if SBPP == 15
#define SC scalerSourceCache.b16
#ifdef WORDS_BIGENDIAN
#if DBPP == 15   // GGGBBBBBxRRRRRGG -> xRRRRRGGGGGBBBBB
#define PMAKE(_VAL) (((_VAL>>8)&0x00FF)|((_VAL<<8)&0xFF00))
#elif DBPP == 16 // gggBBBBBxRRRRRGg -> RRRRRGggggGBBBBB
#define PMAKE(_VAL) (((_VAL>>8)&0x001F)|((_VAL>>7)&0x01C0)|((_VAL<<9)&0xFE00)|((_VAL<<4)&0x0020))
#elif DBPP == 32 // GggBBBbbxRRRrrGG -> 00000000RRRrrRRRGGGggGGGBBBbbBBB
#define PMAKE(_VAL) (((_VAL<<17)&0x00F80000)|((_VAL<<12)&0x00070000)|((_VAL<<14)&0x0000C000)|((_VAL>>2)&0x00003800)|((_VAL<<9)&0x00000600)|((_VAL>>7)&0x00000100)|((_VAL>>5)&0x000000F8)|((_VAL>>10)&0x00000007))
#endif
#else
#if DBPP == 15
#define PMAKE(_VAL) (_VAL)
#elif DBPP == 16 // xRRRRRGggggBBBBB -> RRRRRGggggGBBBBB
#define PMAKE(_VAL) ((_VAL & 31)|((_VAL & ~31)<<1)|((_VAL&0x0200)>>4))
#elif DBPP == 32 // xRRRrrGGGggBBBbb -> RRRrrRRRGGGggGGGBBBbbBBB
#define PMAKE(_VAL)  (((_VAL&(31<<10))<<9)|((_VAL&(31<<5))<<6)|((_VAL&31)<<3)|((_VAL&(7<<12))<<4)|((_VAL&(7<<7))<<1)|((_VAL&(7<<2))>>2))
#endif
#endif
#define SRCTYPE uint16_t
#endif

#if SBPP == 16
#define SC scalerSourceCache.b16
#ifdef WORDS_BIGENDIAN
#if DBPP == 15   // GGgBBBBBRRRRRGGG -> 0RRRRRGGGGGBBBBB
#define PMAKE(_VAL) (((_VAL>>8)&0x001F)|((_VAL>>9)&0x0060)|((_VAL<<7)&0x7F80))
#elif DBPP == 16 // GGGBBBBBRRRRRGGG -> RRRRRGGGGGGBBBBB
#define PMAKE(_VAL) (((_VAL>>8)&0x00FF)|((_VAL<<8)&0xFF00))
#elif DBPP == 32 // gggBBBbbRRRrrGGg -> RRRrrRRRGGggggGGBBBbbBBB
#define PMAKE(_VAL) (((_VAL<<16)&0x00F80000)|((_VAL<<11)&0x00070000)|((_VAL<<13)&0x0000E000)|((_VAL>>3)&0x00001C00)|((_VAL<<7)&0x00000300)|((_VAL>>5)&0x000000F8)|((_VAL>>10)&0x00000007))
#endif
#else
#if DBPP == 15   // RRRRRGGGGGgBBBBB -> 0RRRRRGGGGGBBBBB
#define PMAKE(_VAL) (((_VAL&~63)>>1)|(_VAL&31))
#elif DBPP == 16
#define PMAKE(_VAL) (_VAL)
#elif DBPP == 32 // RRRrrGGggggBBBbb -> RRRrrRRRGGggggGGBBBbbBBB
#define PMAKE(_VAL)  (((_VAL&(31<<11))<<8)|((_VAL&(63<<5))<<5)|((_VAL&0xE01F)<<3)|((_VAL&(3<<9))>>1)|((_VAL&(7<<2))>>2))
#endif
#endif
#define SRCTYPE uint16_t
#endif

#if SBPP == 24
#define SC scalerSourceCache.b32
#if DBPP == 15
#define PMAKE(_VAL) (PTYPE)(((_VAL & (31 << 19)) >> 9) | ((_VAL & (31 << 11)) >> 6) | ((_VAL & (31 << 3)) >> 3))
#elif DBPP == 16
#define PMAKE(_VAL) (PTYPE)(((_VAL & (31 << 19)) >> 8) | ((_VAL & (63 << 10)) >> 4) | ((_VAL & (31 << 3)) >> 3))
#elif DBPP == 32
#define PMAKE(_VAL) (_VAL)
#endif
#include "rgb24.h"
#define SRCTYPE rgb24
#endif

#if SBPP == 32
#define SC scalerSourceCache.b32
#ifdef WORDS_BIGENDIAN
#if DBPP == 15   // BBBBBbbbGGGGGgggRRRRRrrrxxxxxxxx -> 0RRRRRGGGGGBBBBB
#define PMAKE(_VAL) (PTYPE)(((_VAL>>27)&0x001F)|((_VAL>>14)&0x03E0)|((_VAL>>1)&0x7C00))
#elif DBPP == 16 // BBBBBbbbGGGGGGggRRRRRrrrxxxxxxxx -> RRRRRGGGGGGBBBBB
#define PMAKE(_VAL) (PTYPE)(((_VAL>>27)&0x001F)|((_VAL>>13)&0x07E0)|(_VAL&0xF800))
#elif DBPP == 32 // BBBBBBBBGGGGGGGGRRRRRRRRxxxxxxxx -> RRRRRRRRGGGGGGGGBBBBBBBB
#define PMAKE(_VAL) (((_VAL>>24)&0x000000FF)|((_VAL>>8)&0x0000FF00)|((_VAL<<8)&0x00FF0000))
#endif
#else
#if DBPP == 15   // RRRRRrrrGGGGGgggBBBBBbbb -> 0RRRRRGGGGGBBBBB
#define PMAKE(_VAL) (PTYPE)(((_VAL&(31<<19))>>9)|((_VAL&(31<<11))>>6)|((_VAL&(31<<3))>>3))
#elif DBPP == 16 // RRRRRrrrGGGGGGggBBBBBbbb -> RRRRRGGGGGGBBBBB
#define PMAKE(_VAL) (PTYPE)(((_VAL&(31<<19))>>8)|((_VAL&(63<<10))>>5)|((_VAL&(31<<3))>>3))
#elif DBPP == 32
#define PMAKE(_VAL) (_VAL)
#endif
#endif
#define SRCTYPE uint32_t
#endif

//  C0 C1 C2 D3
//  C3 C4 C5 D4
//  C6 C7 C8 D5
//  D0 D1 D2 D6

#define C0 fc[-1 - SCALER_COMPLEXWIDTH]
#define C1 fc[+0 - SCALER_COMPLEXWIDTH]
#define C2 fc[+1 - SCALER_COMPLEXWIDTH]
#define C3 fc[-1 ]
#define C4 fc[+0 ]
#define C5 fc[+1 ]
#define C6 fc[-1 + SCALER_COMPLEXWIDTH]
#define C7 fc[+0 + SCALER_COMPLEXWIDTH]
#define C8 fc[+1 + SCALER_COMPLEXWIDTH]

#define D0 fc[-1 + 2*SCALER_COMPLEXWIDTH]
#define D1 fc[+0 + 2*SCALER_COMPLEXWIDTH]
#define D2 fc[+1 + 2*SCALER_COMPLEXWIDTH]
#define D3 fc[+2 - SCALER_COMPLEXWIDTH]
#define D4 fc[+2]
#define D5 fc[+2 + SCALER_COMPLEXWIDTH]
#define D6 fc[+2 + 2*SCALER_COMPLEXWIDTH]

#if (SBPP != 9) || (DBPP != 8)

#if RENDER_USE_ADVANCED_SCALERS>1
static void conc3d(Cache,SBPP,DBPP) (const void * s) {
#ifdef RENDER_NULL_INPUT
	if (!s) {
		render.scale.cacheRead += render.scale.cachePitch;
		render.scale.inLine++;
		render.scale.complexHandler();
		return;
	}
#endif
	const SRCTYPE * src = (SRCTYPE*)s;
	PTYPE *fc= &FC[render.scale.inLine+1][1];
	SRCTYPE *sc = (SRCTYPE*)(render.scale.cacheRead);
	render.scale.cacheRead += render.scale.cachePitch;
	Bitu b;
	bool hadChange = false;
	/* This should also copy the surrounding pixels but it looks nice enough without */
	for (b=0;b<render.scale.blocks;b++) {
#if (SBPP == 9)
		for (Bitu x=0;x<SCALER_BLOCKSIZE;x++) {
			PTYPE pixel = PMAKE(src[x]);
			if (pixel != fc[x]) {
#else 
		for (Bitu x=0;x<SCALER_BLOCKSIZE;x+=sizeof(Bitu)/sizeof(SRCTYPE)) {
			if (*(Bitu const*)&src[x] != *(Bitu*)&sc[x]) {
#endif
				do {
					fc[x] = PMAKE(src[x]);
					sc[x] = src[x];
					x++;
				} while (x<SCALER_BLOCKSIZE);
				hadChange = true;
				/* Change the surrounding blocks */
				CC[render.scale.inLine+0][1+b-1] |= SCALE_RIGHT;
				CC[render.scale.inLine+0][1+b+0] |= SCALE_FULL;
				CC[render.scale.inLine+0][1+b+1] |= SCALE_LEFT;
				CC[render.scale.inLine+1][1+b-1] |= SCALE_RIGHT;
				CC[render.scale.inLine+1][1+b+0] |= SCALE_FULL;
				CC[render.scale.inLine+1][1+b+1] |= SCALE_LEFT;
				CC[render.scale.inLine+2][1+b-1] |= SCALE_RIGHT;
				CC[render.scale.inLine+2][1+b+0] |= SCALE_FULL;
				CC[render.scale.inLine+2][1+b+1] |= SCALE_LEFT;
				continue;
			}
		}
		fc += SCALER_BLOCKSIZE;
		sc += SCALER_BLOCKSIZE;
		src += SCALER_BLOCKSIZE;
	}
	if (hadChange) {
		CC[render.scale.inLine+0][0] = 1;
		CC[render.scale.inLine+1][0] = 1;
		CC[render.scale.inLine+2][0] = 1;
	}
	render.scale.inLine++;
	render.scale.complexHandler();
}
#endif

/* Simple scalers */
#define SCALERNAME		Normal1x
#define SCALERWIDTH		1
#define SCALERHEIGHT	1
#define SCALERFUNC								\
	line0[0] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		Normal2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC								\
	line0[0] = P;								\
	line0[1] = P;								\
	line1[0] = P;								\
	line1[1] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		Normal3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#define SCALERFUNC								\
	line0[0] = P;								\
	line0[1] = P;								\
	line0[2] = P;								\
	line1[0] = P;								\
	line1[1] = P;								\
	line1[2] = P;								\
	line2[0] = P;								\
	line2[1] = P;								\
	line2[2] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		NormalDw
#define SCALERWIDTH		2
#define SCALERHEIGHT	1
#define SCALERFUNC								\
	line0[0] = P;								\
	line0[1] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		NormalDh
#define SCALERWIDTH		1
#define SCALERHEIGHT	2
#define SCALERFUNC								\
	line0[0] = P;								\
	line1[0] = P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#endif // (SBPP != 9) || (DBPP != 8)

#if (DBPP > 8)

#if RENDER_USE_ADVANCED_SCALERS>0

#define SCALERNAME		TV2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC									\
{													\
	Bitu halfpixel=(((P & redblueMask) * 5) >> 3) & redblueMask;	\
	halfpixel|=(((P & greenMask) * 5) >> 3) & greenMask;			\
	line0[0]=P;							\
	line0[1]=P;							\
	line1[0]=halfpixel;						\
	line1[1]=halfpixel;						\
}
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		TV3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#define SCALERFUNC							\
{											\
	Bitu halfpixel=(((P & redblueMask) * 5) >> 3) & redblueMask;	\
	halfpixel|=(((P & greenMask) * 5) >> 3) & greenMask;			\
	line0[0]=P;								\
	line0[1]=P;								\
	line0[2]=P;								\
	line1[0]=halfpixel;						\
	line1[1]=halfpixel;						\
	line1[2]=halfpixel;						\
	halfpixel=(((P & redblueMask) * 5) >> 4) & redblueMask;	\
	halfpixel|=(((P & greenMask) * 5) >> 4) & greenMask;			\
	line2[0]=halfpixel;						\
	line2[1]=halfpixel;						\
	line2[2]=halfpixel;						\
}
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		RGB2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC						\
	line0[0]=P & redMask;				\
	line0[1]=P & greenMask;			\
	line1[0]=P & blueMask;				\
	line1[1]=P;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		RGB3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#define SCALERFUNC						\
	line0[0]=P;							\
	line0[1]=P & greenMask;				\
	line0[2]=P & blueMask;				\
	line1[0]=P & greenMask;				\
	line1[1]=P & redMask; 						\
	line1[2]=P;				\
	line2[0]=P;				\
	line2[1]=P & blueMask;				\
	line2[2]=P & redMask;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		Scan2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC						\
	line0[0]=P;							\
	line0[1]=P;							\
	line1[0]=0;							\
	line1[1]=0;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		Scan3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#define SCALERFUNC			\
	line0[0]=P;				\
	line0[1]=P;				\
	line0[2]=P;				\
	line1[0]=P;				\
	line1[1]=P;				\
	line1[2]=P;				\
	line2[0]=0;				\
	line2[1]=0;				\
	line2[2]=0;
#include "render_simple.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#endif		//#if RENDER_USE_ADVANCED_SCALERS>0

#endif		//#if (DBPP > 8)

/* Complex scalers */

#if RENDER_USE_ADVANCED_SCALERS>2

#if (SBPP == DBPP) 


#if (DBPP > 8)

#if (DBPP != 15)

#include "render_templates_hq.h"

#define SCALERNAME		HQ2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#include "render_templates_hq2x.h"
#define SCALERFUNC		conc2d(Hq2x,SBPP)(line0, line1, fc)
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		HQ3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#include "render_templates_hq3x.h"
#define SCALERFUNC		conc2d(Hq3x,SBPP)(line0, line1, line2, fc)
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#include "render_templates_sai.h"

#define SCALERNAME		Super2xSaI
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC		conc2d(Super2xSaI,SBPP)(line0, line1, fc)
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		SuperEagle
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC		conc2d(SuperEagle,SBPP)(line0, line1, fc)
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		_2xSaI
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC		conc2d(_2xSaI,SBPP)(line0, line1, fc)
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#endif // (DBPP != 15)

#define SCALERNAME		AdvInterp2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC												\
	if (C1 != C7 && C3 != C5) {									\
		line0[0] = C3 == C1 ? interp_w2(C3,C4,5U,3U) : C4;		\
		line0[1] = C1 == C5 ? interp_w2(C5,C4,5U,3U) : C4;		\
		line1[0] = C3 == C7 ? interp_w2(C3,C4,5U,3U) : C4;		\
		line1[1] = C7 == C5 ? interp_w2(C5,C4,5U,3U) : C4;		\
	} else {													\
		line0[0] = line0[1] = C4;								\
		line1[0] = line1[1] = C4;								\
	}
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

//TODO, come up with something better for this one
#define SCALERNAME		AdvInterp3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#define SCALERFUNC												\
	if ((C1 != C7) && (C3 != C5)) {													\
		line0[0] = C3 == C1 ?  interp_w2(C3,C4,5U,3U) : C4;												\
		line0[1] = (C3 == C1 && C4 != C2) || (C5 == C1 && C4 != C0) ? C1 : C4;		\
		line0[2] = C5 == C1 ?  interp_w2(C5,C4,5U,3U) : C4;												\
		line1[0] = (C3 == C1 && C4 != C6) || (C3 == C7 && C4 != C0) ? C3 : C4;		\
		line1[1] = C4;																\
		line1[2] = (C5 == C1 && C4 != C8) || (C5 == C7 && C4 != C2) ? C5 : C4;		\
		line2[0] = C3 == C7 ?  interp_w2(C3,C4,5U,3U) : C4;												\
		line2[1] = (C3 == C7 && C4 != C8) || (C5 == C7 && C4 != C6) ? C7 : C4;		\
		line2[2] = C5 == C7 ?  interp_w2(C5,C4,5U,3U) : C4;												\
	} else {																		\
		line0[0] = line0[1] = line0[2] = C4;										\
		line1[0] = line1[1] = line1[2] = C4;										\
		line2[0] = line2[1] = line2[2] = C4;										\
	}
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#endif // #if (DBPP > 8)

#if (DBPP != 15)

#define SCALERNAME		AdvMame2x
#define SCALERWIDTH		2
#define SCALERHEIGHT	2
#define SCALERFUNC												\
	if (C1 != C7 && C3 != C5) {									\
		line0[0] = C3 == C1 ? C3 : C4;							\
		line0[1] = C1 == C5 ? C5 : C4;							\
		line1[0] = C3 == C7 ? C3 : C4;							\
		line1[1] = C7 == C5 ? C5 : C4;							\
	} else {													\
		line0[0] = line0[1] = C4;								\
		line1[0] = line1[1] = C4;								\
	}
#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME		AdvMame3x
#define SCALERWIDTH		3
#define SCALERHEIGHT	3
#define SCALERFUNC																	\
	if ((C1 != C7) && (C3 != C5)) {													\
		line0[0] = C3 == C1 ?  C3 : C4;												\
		line0[1] = (C3 == C1 && C4 != C2) || (C5 == C1 && C4 != C0) ? C1 : C4;		\
		line0[2] = C5 == C1 ?  C5 : C4;												\
		line1[0] = (C3 == C1 && C4 != C6) || (C3 == C7 && C4 != C0) ? C3 : C4;		\
		line1[1] = C4;																\
		line1[2] = (C5 == C1 && C4 != C8) || (C5 == C7 && C4 != C2) ? C5 : C4;		\
		line2[0] = C3 == C7 ?  C3 : C4;												\
		line2[1] = (C3 == C7 && C4 != C8) || (C5 == C7 && C4 != C6) ? C7 : C4;		\
		line2[2] = C5 == C7 ?  C5 : C4;												\
	} else {																		\
		line0[0] = line0[1] = line0[2] = C4;										\
		line1[0] = line1[1] = line1[2] = C4;										\
		line2[0] = line2[1] = line2[2] = C4;										\
	}

#include "render_loops.h"
#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#endif // (DBPP != 15)

#endif // (SBPP == DBPP) && !defined (CACHEWITHPAL)

#endif // #if RENDER_USE_ADVANCED_SCALERS>2

#undef PSIZE
#undef PTYPE
#undef PMAKE
#undef WC
#undef LC
#undef FC
#undef SC
#undef redMask
#undef greenMask
#undef blueMask
#undef redblueMask
#undef redBits
#undef greenBits
#undef blueBits
#undef redShift
#undef greenShift
#undef blueShift
#undef SRCTYPE
