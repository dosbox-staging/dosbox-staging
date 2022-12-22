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

#ifndef _RENDER_SCALERS_H
#define _RENDER_SCALERS_H

//#include "render.h"
#include "video.h"

// Allow double-width and double-height scaling for low resolution modes
#define SCALER_MAX_MUL_WIDTH  2
#define SCALER_MAX_MUL_HEIGHT 2

constexpr uint16_t SCALER_MAXHEIGHT = 1200;
constexpr uint16_t SCALER_MAXWIDTH  = 1600 + 30;
//
// The additional 30 pixels of width accommodates the full range that tweaked
// text modes (such as Q200x25x8 used by Necromancer's DOS Navigator) are
// capable of writing.


#define SCALER_BLOCKSIZE	16

enum ScalerMode : uint8_t {
	scalerMode8,
	scalerMode15,
	scalerMode16,
	scalerMode32,
};

typedef void (*ScalerLineHandler_t)(const void* src);

extern uint8_t Scaler_Aspect[];
extern uint8_t diff_table[];
extern Bitu Scaler_ChangedLineIndex;
extern uint16_t Scaler_ChangedLines[];

union scalerSourceCache_t {
	uint32_t b32	[SCALER_MAXHEIGHT] [SCALER_MAXWIDTH];
	uint16_t b16	[SCALER_MAXHEIGHT] [SCALER_MAXWIDTH];
	uint8_t b8	[SCALER_MAXHEIGHT] [SCALER_MAXWIDTH];
};

extern scalerSourceCache_t scalerSourceCache;

typedef ScalerLineHandler_t ScalerLineBlock_t[6][4];

struct ScalerSimpleBlock_t {
	const char* name         = {};
	uint8_t gfxFlags         = 0;
	uint8_t xscale           = 0;
	uint8_t yscale           = 0;
	ScalerLineBlock_t Linear = {};
	ScalerLineBlock_t Random = {};
};

#define SCALE_LEFT	0x1
#define SCALE_RIGHT	0x2
#define SCALE_FULL	0x4

/* Simple scalers */
extern ScalerSimpleBlock_t ScaleNormal1x;
extern ScalerSimpleBlock_t ScaleNormalDw;
extern ScalerSimpleBlock_t ScaleNormalDh;
extern ScalerSimpleBlock_t ScaleNormal2x;
#endif
