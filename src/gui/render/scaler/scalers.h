// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_SCALERS_H
#define DOSBOX_RENDER_SCALERS_H

#include "gui/render/render.h"

#include "misc/video.h"

// Make sure ScalerMaxWidth remains a multiple of 8
constexpr int ScalerWidthExtraPadding = 8 * 5;

constexpr int ScalerMaxWidth  = 1600 + ScalerWidthExtraPadding;
constexpr int ScalerMaxHeight = 1200;

extern int scaler_changed_line_index;
extern int scaler_changed_lines[];

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

#endif // DOSBOX_RENDER_SCALERS_H
