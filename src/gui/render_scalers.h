/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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

#ifndef _RENDER_SCALERS_H
#define _RENDER_SCALERS_H

//#include "render.h"
#include "video.h"

#define SCALER_MAXWIDTH		1024
#define SCALER_MAXHEIGHT	768
#define SCALER_BLOCKSIZE	16

typedef enum {
	scalerMode8, scalerMode15, scalerMode16, scalerMode32
} scalerMode_t;

typedef enum {
	scalerOpNormal,
	scalerOpAdvMame,
	scalerOpAdvInterp,
	scalerOpTV,
	scalerOpRGB,
	scalerOpScan,
} scalerOperation_t;

typedef void (*ScalerCacheHandler_t)(const void *src);
typedef void (*ScalerLineHandler_t)(void);

extern Bit8u Scaler_Aspect[];
extern Bit8u diff_table[];
extern Bitu Scaler_ChangedLineIndex;
extern Bit16u Scaler_ChangedLines[];
extern Bit8u scalerChangeCache [SCALER_MAXHEIGHT][SCALER_MAXWIDTH / SCALER_BLOCKSIZE];
typedef union {
	Bit32u b32	[(SCALER_MAXHEIGHT+2)] [(SCALER_MAXWIDTH+2)];
	Bit16u b16	[(SCALER_MAXHEIGHT+2)] [(SCALER_MAXWIDTH+2)];
	Bit8u b8	[(SCALER_MAXHEIGHT+2)] [(SCALER_MAXWIDTH+2)];
} scalerFrameCache_t;
typedef union {
	Bit32u b32	[SCALER_MAXHEIGHT] [SCALER_MAXWIDTH];
	Bit16u b16	[SCALER_MAXHEIGHT] [SCALER_MAXWIDTH];
	Bit8u b8	[SCALER_MAXHEIGHT] [SCALER_MAXWIDTH];
} scalerSourceCache_t;

extern scalerFrameCache_t scalerFrameCache;
extern scalerSourceCache_t scalerSourceCache;

#define ScaleFlagSimple		0x001

typedef struct {
	Bitu gfxFlags;
	Bitu scaleFlags;
	Bitu xscale,yscale;
	ScalerLineHandler_t Linear[4];
	ScalerLineHandler_t Random[4];
} ScalerLineBlock_t;

typedef struct {
	Bitu gfxFlags;
	Bitu scaleFlags;
	Bitu xscale,yscale;
	ScalerLineHandler_t Linear[4][4];
	ScalerLineHandler_t Random[4][4];
} ScalerFullLineBlock_t;


typedef struct {
	ScalerCacheHandler_t simple[4];
	ScalerCacheHandler_t complex[4];
} ScalerCacheBlock_t;

extern ScalerLineBlock_t ScaleNormal;
extern ScalerLineBlock_t ScaleNormalDw;
extern ScalerLineBlock_t ScaleNormalDh;

#define SCALE_LEFT	0x1
#define SCALE_RIGHT	0x2
#define SCALE_FULL	0x4

extern ScalerLineBlock_t ScaleNormal2x;
extern ScalerLineBlock_t ScaleNormal3x;
extern ScalerLineBlock_t ScaleAdvMame2x;
extern ScalerLineBlock_t ScaleAdvMame3x;
extern ScalerLineBlock_t ScaleAdvInterp2x;

extern ScalerLineBlock_t ScaleTV2x;
extern ScalerLineBlock_t ScaleTV3x;
extern ScalerLineBlock_t ScaleRGB2x;
extern ScalerLineBlock_t ScaleRGB3x;
extern ScalerLineBlock_t ScaleScan2x;
extern ScalerLineBlock_t ScaleScan3x;

extern ScalerCacheBlock_t	ScalerCache_8;
extern ScalerCacheBlock_t	ScalerCache_8Pal;
extern ScalerCacheBlock_t	ScalerCache_15;
extern ScalerCacheBlock_t	ScalerCache_16;
extern ScalerCacheBlock_t	ScalerCache_32;

#endif
