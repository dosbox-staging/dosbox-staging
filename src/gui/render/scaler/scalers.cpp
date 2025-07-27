// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later


//TODO:
//Maybe just do the cache checking back into the simple scalers so they can
//just handle it all in one go, but this seems to work well enough for now

#include "dosbox.h"
#include "render.h"
#include <cstring>

uint8_t Scaler_Aspect[SCALER_MAXHEIGHT]        = {};
uint16_t Scaler_ChangedLines[SCALER_MAXHEIGHT] = {};

Bitu Scaler_ChangedLineIndex = 0;

static union {
	 //The +1 is a at least for the normal scalers not needed. (-1 is enough)
	 uint32_t b32[SCALER_MAX_MUL_HEIGHT + 1][SCALER_MAXWIDTH];
	 uint16_t b16[SCALER_MAX_MUL_HEIGHT + 1][SCALER_MAXWIDTH];
	 uint8_t b8[SCALER_MAX_MUL_HEIGHT + 1][SCALER_MAXWIDTH] = {};
} scalerWriteCache;
//scalerFrameCache_t scalerFrameCache;
scalerSourceCache_t scalerSourceCache;
#if RENDER_USE_ADVANCED_SCALERS>1
scalerChangeCache_t scalerChangeCache;
#endif

#define _conc2(A,B) A ## B
#define _conc3(A,B,C) A ## B ## C
#define _conc4(A,B,C,D) A ## B ## C ## D
#define _conc5(A,B,C,D,E) A ## B ## C ## D ## E
#define _conc7(A,B,C,D,E,F,G) A ## B ## C ## D ## E ## F ## G

#define conc2(A,B) _conc2(A,B)
#define conc3(A,B,C) _conc3(A,B,C)
#define conc4(A,B,C,D) _conc4(A,B,C,D)
#define conc2d(A,B) _conc3(A,_,B)
#define conc3d(A,B,C) _conc5(A,_,B,_,C)
#define conc4d(A,B,C,D) _conc7(A,_,B,_,C,_,D)

static inline void BituMove( void *_dst, const void * _src, Bitu size) {
	 auto dst = static_cast<Bitu*>(_dst);
	 auto src = static_cast<const Bitu*>(_src);
	 size /= sizeof(Bitu);
	 for (Bitu x = 0; x < size; x++) {
		 dst[x] = src[x];
	 }
}

static inline void ScalerAddLines( Bitu changed, Bitu count ) {
	if ((Scaler_ChangedLineIndex & 1) == changed ) {
		Scaler_ChangedLines[Scaler_ChangedLineIndex] += count;
	} else {
		Scaler_ChangedLines[++Scaler_ChangedLineIndex] = count;
	}
	render.scale.outWrite += render.scale.outPitch * count;
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
#include "templates.h"
#undef DBPP
#define DBPP 15
#include "templates.h"
#undef DBPP
#define DBPP 16
#include "templates.h"
#undef DBPP
#define DBPP 32
#include "templates.h"
#undef SBPP
#undef DBPP

/* SBPP 9 is a special case with palette check support */
#define SBPP 9
#define DBPP 8
#include "templates.h"
#undef DBPP
#define DBPP 15
#include "templates.h"
#undef DBPP
#define DBPP 16
#include "templates.h"
#undef DBPP
#define DBPP 32
#include "templates.h"
#undef SBPP
#undef DBPP

#define SBPP 15
#define DBPP 15
#include "templates.h"
#undef DBPP
#define DBPP 16
#include "templates.h"
#undef DBPP
#define DBPP 32
#include "templates.h"
#undef SBPP
#undef DBPP

#define SBPP 16
#define DBPP 15
#include "templates.h"
#undef DBPP
#define DBPP 16
#include "templates.h"
#undef DBPP
#define DBPP 32
#include "templates.h"
#undef SBPP
#undef DBPP

#define SBPP 24
#define DBPP 15
#include "templates.h"
#undef DBPP
#define DBPP 16
#include "templates.h"
#undef DBPP
#define DBPP 32
#include "templates.h"
#undef SBPP
#undef DBPP

#define SBPP 32
#define DBPP 15
#include "templates.h"
#undef DBPP
#define DBPP 16
#include "templates.h"
#undef DBPP
#define DBPP 32
#include "templates.h"
#undef SBPP
#undef DBPP

// clang-format off

ScalerSimpleBlock_t ScaleNormal1x = {
	"Normal",
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32,
	1,1,{
{	Normal1x_8_8_L,		Normal1x_8_15_L ,	Normal1x_8_16_L ,	Normal1x_8_32_L },
{	       nullptr,		Normal1x_15_15_L,	Normal1x_15_16_L,	Normal1x_15_32_L},
{	       nullptr,		Normal1x_16_15_L,	Normal1x_16_16_L,	Normal1x_16_32_L},
{	       nullptr,		Normal1x_24_15_L,	Normal1x_24_16_L,	Normal1x_24_32_L},
{	       nullptr,		Normal1x_32_15_L,	Normal1x_32_16_L,	Normal1x_32_32_L},
{	Normal1x_8_8_L,		Normal1x_9_15_L ,	Normal1x_9_16_L ,	Normal1x_9_32_L }
},{
{	Normal1x_8_8_R,		Normal1x_8_15_R ,	Normal1x_8_16_R ,	Normal1x_8_32_R },
{	       nullptr,		Normal1x_15_15_R,	Normal1x_15_16_R,	Normal1x_15_32_R},
{	       nullptr,		Normal1x_16_15_R,	Normal1x_16_16_R,	Normal1x_16_32_R},
{	       nullptr,		Normal1x_24_15_R,	Normal1x_24_16_R,	Normal1x_24_32_R},
{	       nullptr,		Normal1x_32_15_R,	Normal1x_32_16_R,	Normal1x_32_32_R},
{	Normal1x_8_8_R,		Normal1x_9_15_R ,	Normal1x_9_16_R ,	Normal1x_9_32_R }
}};

// Renders double-wide DOS video modes
ScalerSimpleBlock_t ScaleNormalDw = {
	"Normal",
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32,
	2,1,{
{	NormalDw_8_8_L,		NormalDw_8_15_L ,	NormalDw_8_16_L ,	NormalDw_8_32_L },
{	       nullptr,		NormalDw_15_15_L,	NormalDw_15_16_L,	NormalDw_15_32_L},
{	       nullptr,		NormalDw_16_15_L,	NormalDw_16_16_L,	NormalDw_16_32_L},
{	       nullptr,		NormalDw_24_15_L,	NormalDw_24_16_L,	NormalDw_24_32_L},
{	       nullptr,		NormalDw_32_15_L,	NormalDw_32_16_L,	NormalDw_32_32_L},
{	NormalDw_8_8_L,		NormalDw_9_15_L ,	NormalDw_9_16_L ,	NormalDw_9_32_L }
},{
{	NormalDw_8_8_R,		NormalDw_8_15_R ,	NormalDw_8_16_R ,	NormalDw_8_32_R },
{	       nullptr,		NormalDw_15_15_R,	NormalDw_15_16_R,	NormalDw_15_32_R},
{	       nullptr,		NormalDw_16_15_R,	NormalDw_16_16_R,	NormalDw_16_32_R},
{	       nullptr,		NormalDw_24_15_R,	NormalDw_24_16_R,	NormalDw_24_32_R},
{	       nullptr,		NormalDw_32_15_R,	NormalDw_32_16_R,	NormalDw_32_32_R},
{	NormalDw_8_8_R,		NormalDw_9_15_R ,	NormalDw_9_16_R ,	NormalDw_9_32_R }
}};

// Renders double-high DOS video modes
ScalerSimpleBlock_t ScaleNormalDh = {
	"Normal",
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32,
	1,2,{
{	NormalDh_8_8_L,		NormalDh_8_15_L ,	NormalDh_8_16_L ,	NormalDh_8_32_L },
{	       nullptr,		NormalDh_15_15_L,	NormalDh_15_16_L,	NormalDh_15_32_L},
{	       nullptr,		NormalDh_16_15_L,	NormalDh_16_16_L,	NormalDh_16_32_L},
{	       nullptr,		NormalDh_24_15_L,	NormalDh_24_16_L,	NormalDh_24_32_L},
{	       nullptr,		NormalDh_32_15_L,	NormalDh_32_16_L,	NormalDh_32_32_L},
{	NormalDh_8_8_L,		NormalDh_9_15_L ,	NormalDh_9_16_L ,	NormalDh_9_32_L }
},{
{	NormalDh_8_8_R,		NormalDh_8_15_R ,	NormalDh_8_16_R ,	NormalDh_8_32_R },
{	       nullptr,		NormalDh_15_15_R,	NormalDh_15_16_R,	NormalDh_15_32_R},
{	       nullptr,		NormalDh_16_15_R,	NormalDh_16_16_R,	NormalDh_16_32_R},
{	       nullptr,		NormalDh_24_15_R,	NormalDh_24_16_R,	NormalDh_24_32_R},
{	       nullptr,		NormalDh_32_15_R,	NormalDh_32_16_R,	NormalDh_32_32_R},
{	NormalDh_8_8_R,		NormalDh_9_15_R ,	NormalDh_9_16_R ,	NormalDh_9_32_R }
}};

ScalerSimpleBlock_t ScaleNormal2x = {
	"Normal2x",
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32,
	2,2,{
{   Normal2x_8_8_L,     Normal2x_8_15_L,    Normal2x_8_16_L,    Normal2x_8_32_L },
{          nullptr,     Normal2x_15_15_L,   Normal2x_15_16_L,   Normal2x_15_32_L},
{          nullptr,     Normal2x_16_15_L,   Normal2x_16_16_L,   Normal2x_16_32_L},
{          nullptr,     Normal2x_24_15_L,   Normal2x_24_16_L,   Normal2x_24_32_L},
{          nullptr,     Normal2x_32_15_L,   Normal2x_32_16_L,   Normal2x_32_32_L},
{   Normal2x_8_8_L,     Normal2x_9_15_L ,   Normal2x_9_16_L,    Normal2x_9_32_L }
},{
{   Normal2x_8_8_R,     Normal2x_8_15_R ,   Normal2x_8_16_R,    Normal2x_8_32_R },
{          nullptr,     Normal2x_15_15_R,   Normal2x_15_16_R,   Normal2x_15_32_R},
{          nullptr,     Normal2x_16_15_R,   Normal2x_16_16_R,   Normal2x_16_32_R},
{          nullptr,     Normal2x_24_15_R,   Normal2x_24_16_R,   Normal2x_24_32_R},
{          nullptr,     Normal2x_32_15_R,   Normal2x_32_16_R,   Normal2x_32_32_R},
{   Normal2x_8_8_R,     Normal2x_9_15_R ,   Normal2x_9_16_R,    Normal2x_9_32_R },
}};

// clang-format on
