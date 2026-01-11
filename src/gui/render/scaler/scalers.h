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

// The additional 30 pixels are for some tweaked text modes (e.g., Q200x25x8
// used by Necromancer's DOS Navigator).
constexpr int ScalerMaxWidth  = 1600 + 30;

constexpr int ScalerMaxHeight = 1200;

#define SCALER_BLOCKSIZE 16

enum ScalerMode : uint8_t {
	scalerMode8,  // 0
	scalerMode15, // 1
	scalerMode16, // 2
	scalerMode32, // 3
};

extern int scaler_changed_line_index;
extern int scaler_changed_lines[];

union ScalerSourceCache {
	uint32_t b32[ScalerMaxHeight][ScalerMaxWidth];
};

extern ScalerSourceCache scaler_source_cache;

typedef void (*ScalerLineHandler)(const void* src);

struct Scaler {
	int x_scale = 0;
	int y_scale = 0;

	ScalerLineHandler line_handlers[6] = {};
};

/* Simple scalers */
extern Scaler Scale1x;
extern Scaler ScaleHoriz2x;
extern Scaler ScaleVert2x;
extern Scaler Scale2x;
#endif
