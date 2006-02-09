/*
 *  Copyright (C) 2002-2006  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


//TODO:
//Maybe just do the cache checking back into the simple scalers so they can 
//just handle it all in one go, but this seems to work well enough for now

#include "dosbox.h"
#include "render.h"
#include <string.h>

Bit8u Scaler_Aspect[SCALER_MAXHEIGHT];
Bit16u Scaler_ChangedLines[SCALER_MAXHEIGHT];
Bitu Scaler_ChangedLineIndex;

union {
	Bit32u b32 [4][SCALER_MAXWIDTH*3];
	Bit16u b16 [4][SCALER_MAXWIDTH*3];
	Bit8u b8 [4][SCALER_MAXWIDTH*3];
} scalerWriteCache;
scalerFrameCache_t scalerFrameCache;
scalerSourceCache_t scalerSourceCache;
scalerChangeCache_t scalerChangeCache;

#define _conc2(A,B) A ## B
#define _conc3(A,B,C) A ## B ## C
#define _conc4(A,B,C,D) A ## B ## C ## D
#define _conc5(A,B,C,D,E) A ## B ## C ## D ## E

#define conc2(A,B) _conc2(A,B)
#define conc3(A,B,C) _conc3(A,B,C)
#define conc4(A,B,C,D) _conc4(A,B,C,D)
#define conc2d(A,B) _conc3(A,_,B)
#define conc3d(A,B,C) _conc5(A,_,B,_,C)

static INLINE void BituMove( void *_dst, const void * _src, Bitu size) {
	Bitu * dst=(Bitu *)(_dst);
	const Bitu * src=(Bitu *)(_src);
	size/=sizeof(Bitu);
	for (Bitu x=0; x<size;x++)
		dst[x] = src[x];
}

#define BituMove2(_DST,_SRC,_SIZE)			\
{											\
	Bitu bsize=(_SIZE)/sizeof(Bitu);		\
	Bitu * bdst=(Bitu *)(_DST);				\
	Bitu * bsrc=(Bitu *)(_SRC);				\
	while (bsize--) *bdst++=*bsrc++;		\
}

#define interp_w2(P0,P1,W0,W1)															\
	((((P0&redblueMask)*W0+(P1&redblueMask)*W1)/(W0+W1)) & redblueMask) |	\
	((((P0&  greenMask)*W0+(P1&  greenMask)*W1)/(W0+W1)) & greenMask)
#define interp_w3(P0,P1,P2,W0,W1,W2)														\
	((((P0&redblueMask)*W0+(P1&redblueMask)*W1+(P2&redblueMask)*W2)/(W0+W1+W2)) & redblueMask) |	\
	((((P0&  greenMask)*W0+(P1&  greenMask)*W1+(P2&  greenMask)*W2)/(W0+W1+W2)) & greenMask)
#define interp_w4(P0,P1,P2,P3,W0,W1,W2,W3)														\
	((((P0&redblueMask)*W0+(P1&redblueMask)*W1+(P2&redblueMask)*W2+(P3&redblueMask)*W3)/(W0+W1+W2+W3)) & redblueMask) |	\
	((((P0&  greenMask)*W0+(P1&  greenMask)*W1+(P2&  greenMask)*W2+(P3&  greenMask)*W3)/(W0+W1+W2+W3)) & greenMask)


#define CC scalerChangeCache

/* Include the different rendering routines */
#define SBPP 8
#define DBPP 8
#include "render_templates.h"
#undef DBPP
#define DBPP 15
#include "render_templates.h"
#undef DBPP
#define DBPP 16
#include "render_templates.h"
#undef DBPP
#define DBPP 32
#include "render_templates.h"
#undef SBPP
#undef DBPP

#define SBPP 15
#define DBPP 15
#include "render_templates.h"
#undef DBPP
#define DBPP 16
#include "render_templates.h"
#undef DBPP
#define DBPP 32
#include "render_templates.h"
#undef SBPP
#undef DBPP

#define SBPP 16
#define DBPP 15
#include "render_templates.h"
#undef DBPP
#define DBPP 16
#include "render_templates.h"
#undef DBPP
#define DBPP 32
#include "render_templates.h"
#undef SBPP
#undef DBPP

#define SBPP 32
#define DBPP 15
#include "render_templates.h"
#undef DBPP
#define DBPP 16
#include "render_templates.h"
#undef DBPP
#define DBPP 32
#include "render_templates.h"
#undef SBPP
#undef DBPP

ScalerCacheBlock_t	ScalerCache_8 = {
	CacheSimple_8_8, CacheSimple_8_15, CacheSimple_8_16, CacheSimple_8_32,
	CacheComplex_8_8, CacheComplex_8_15, CacheComplex_8_16, CacheComplex_8_32,
};
ScalerCacheBlock_t	ScalerCache_8Pal = {
	0, CacheSimplePal_8_15, CacheSimplePal_8_16, CacheSimplePal_8_32,
	0, CacheComplexPal_8_15, CacheComplexPal_8_16, CacheComplexPal_8_32,
};
ScalerCacheBlock_t	ScalerCache_15 = {
	0, CacheSimple_15_15, CacheSimple_15_16, CacheSimple_15_32,
	0, CacheComplex_15_15, CacheComplex_15_16, CacheComplex_15_32,
};
ScalerCacheBlock_t	ScalerCache_16 = {
	0, CacheSimple_16_15, CacheSimple_16_16, CacheSimple_16_32,
	0, CacheComplex_16_15, CacheComplex_16_16, CacheComplex_16_32,
};
ScalerCacheBlock_t	ScalerCache_32 = {
	0, CacheSimple_32_15, CacheSimple_32_16, CacheSimple_32_32,
	0, CacheComplex_32_15, CacheComplex_32_16, CacheComplex_32_32,
};


/* 8 bpp input versions */
ScalerLineBlock_t ScaleNormal = {
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_8,
	ScaleFlagSimple,
	1,1,
	Normal_8_L,Normal_16_L,Normal_16_L,Normal_32_L,
	Normal_8_R,Normal_16_R,Normal_16_R,Normal_32_R
};

ScalerLineBlock_t ScaleNormalDw = {
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_8,
	ScaleFlagSimple,
	2,1,
	NormalDw_8_L,NormalDw_16_L,NormalDw_16_L,NormalDw_32_L,
	NormalDw_8_R,NormalDw_16_R,NormalDw_16_R,NormalDw_32_R
};

ScalerLineBlock_t ScaleNormalDh = {
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_8,
	ScaleFlagSimple,
	1,2,
	NormalDh_8_L,NormalDh_16_L,NormalDh_16_L,NormalDh_32_L,
	NormalDh_8_R,NormalDh_16_R,NormalDh_16_R,NormalDh_32_R
};


ScalerLineBlock_t ScaleNormal2x = {
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_8,
	ScaleFlagSimple,
	2,2,
	Normal2x_8_L,Normal2x_16_L,Normal2x_16_L,Normal2x_32_L,
	Normal2x_8_R,Normal2x_16_R,Normal2x_16_R,Normal2x_32_R
};

ScalerLineBlock_t ScaleNormal3x = {
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_8,
	ScaleFlagSimple,
	3,3,
	Normal3x_8_L,Normal3x_16_L,Normal3x_16_L,Normal3x_32_L,
	Normal3x_8_R,Normal3x_16_R,Normal3x_16_R,Normal3x_32_R
};

ScalerLineBlock_t ScaleAdvMame2x ={
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_8,
	0,
	2,2,
	AdvMame2x_8_L,AdvMame2x_16_L,AdvMame2x_16_L,AdvMame2x_32_L,
	AdvMame2x_8_R,AdvMame2x_16_R,AdvMame2x_16_R,AdvMame2x_32_R
};

ScalerLineBlock_t ScaleAdvMame3x = {
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_8,
	0,
	3,3,
	AdvMame3x_8_L,AdvMame3x_16_L,AdvMame3x_16_L,AdvMame3x_32_L,
	AdvMame3x_8_R,AdvMame3x_16_R,AdvMame3x_16_R,AdvMame3x_32_R
};

/* These need specific 15bpp versions */
ScalerLineBlock_t ScaleAdvInterp2x = {
	GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_32|GFX_RGBONLY,
	0,
	2,2,
	0,AdvInterp2x_15_L,AdvInterp2x_16_L,AdvInterp2x_32_L,
	0,AdvInterp2x_15_R,AdvInterp2x_16_R,AdvInterp2x_32_R
};

ScalerLineBlock_t ScaleAdvInterp3x = {
	GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_32|GFX_RGBONLY,
	0,
	3,3,
	0,AdvInterp3x_15_L,AdvInterp3x_16_L,AdvInterp3x_32_L,
	0,AdvInterp3x_15_R,AdvInterp3x_16_R,AdvInterp3x_32_R
};

ScalerLineBlock_t ScaleTV2x = {
	GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_32|GFX_RGBONLY,
	ScaleFlagSimple,
	2,2,
	0,TV2x_15_L,TV2x_16_L,TV2x_32_L,
	0,TV2x_15_R,TV2x_16_R,TV2x_32_R
};

ScalerLineBlock_t ScaleTV3x = {
	GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_32|GFX_RGBONLY,
	ScaleFlagSimple,
	3,3,
	0,TV3x_15_L,TV3x_16_L,TV3x_32_L,
	0,TV3x_15_R,TV3x_16_R,TV3x_32_R
};

ScalerLineBlock_t ScaleRGB2x = {
	GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_32|GFX_RGBONLY,
	ScaleFlagSimple,
	2,2,
	0,RGB2x_15_L,RGB2x_16_L,RGB2x_32_L,
	0,RGB2x_15_R,RGB2x_16_R,RGB2x_32_R
};

ScalerLineBlock_t ScaleRGB3x = {
	GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_32|GFX_RGBONLY,
	ScaleFlagSimple,
	3,3,
	0,RGB3x_15_L,RGB3x_16_L,RGB3x_32_L,
	0,RGB3x_15_R,RGB3x_16_R,RGB3x_32_R
};


ScalerLineBlock_t ScaleScan2x = {
	GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_32|GFX_RGBONLY,
	ScaleFlagSimple,
	2,2,
	0,Scan2x_15_L,Scan2x_16_L,Scan2x_32_L,
	0,Scan2x_15_R,Scan2x_16_R,Scan2x_32_R
};

ScalerLineBlock_t ScaleScan3x = {
	GFX_CAN_15|GFX_CAN_16|GFX_CAN_32|GFX_LOVE_32|GFX_RGBONLY,
	ScaleFlagSimple,
	3,3,
	0,Scan3x_15_L,Scan3x_16_L,Scan3x_32_L,
	0,Scan3x_15_R,Scan3x_16_R,Scan3x_32_R
};


