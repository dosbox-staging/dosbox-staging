// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef _RENDER_SCALERS_H
#define _RENDER_SCALERS_H

#include "gui/render/render.h"

#include "misc/video.h"

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
	scalerMode8,  // 0
	scalerMode15, // 1
	scalerMode16, // 2
	scalerMode32, // 3
};

typedef void (*ScalerLineHandler_t)(const void* src);

extern uint8_t Scaler_Aspect[];
extern Bitu Scaler_ChangedLineIndex;
extern uint16_t Scaler_ChangedLines[];

union scalerSourceCache_t {
	uint32_t b32	[SCALER_MAXHEIGHT] [SCALER_MAXWIDTH];
};

extern scalerSourceCache_t scalerSourceCache;

typedef ScalerLineHandler_t ScalerLineBlock_t[6][4];

struct ScalerSimpleBlock_t {
	uint8_t gfxFlags         = 0;
	uint8_t xscale           = 0;
	uint8_t yscale           = 0;
	ScalerLineBlock_t Random = {};
};

/* Simple scalers */
extern ScalerSimpleBlock_t ScaleNormal1x;
extern ScalerSimpleBlock_t ScaleNormalDw;
extern ScalerSimpleBlock_t ScaleNormalDh;
extern ScalerSimpleBlock_t ScaleNormal2x;
#endif
