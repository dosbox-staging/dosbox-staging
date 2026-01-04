// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

// TODO:
// Maybe just do the cache checking back into the simple scalers so they can
// just handle it all in one go, but this seems to work well enough for now

#include "dosbox.h"

#include "gui/private/common.h"
#include "gui/render/render.h"

#include <cstring>

uint8_t scaler_aspect[SCALER_MAXHEIGHT]        = {};
uint16_t scaler_changed_lines[SCALER_MAXHEIGHT] = {};

Bitu scaler_changed_line_index = 0;

scalerSourceCache_t scalerSourceCache;

#define _conc3(A, B, C) A##B##C
#define conc2d(A, B)    _conc3(A, _, B)

static inline void BituMove(void* _dst, const void* _src, Bitu size)
{
	auto dst = static_cast<Bitu*>(_dst);
	auto src = static_cast<const Bitu*>(_src);

	size /= sizeof(Bitu);

	for (Bitu x = 0; x < size; x++) {
		dst[x] = src[x];
	}
}

static inline void ScalerAddLines(Bitu changed, Bitu count)
{
	if ((scaler_changed_line_index & 1) == changed) {
		scaler_changed_lines[scaler_changed_line_index] += count;
	} else {
		scaler_changed_lines[++scaler_changed_line_index] = count;
	}
	render.scale.outWrite += render.scale.outPitch * count;
}

/* Include the different rendering routines */
#define SBPP 8
#include "templates.h"
#undef SBPP

/* SBPP 9 is a special case with palette check support */
#define SBPP 9
#include "templates.h"
#undef SBPP

#define SBPP 15
#include "templates.h"
#undef SBPP

#define SBPP 16
#include "templates.h"
#undef SBPP

#define SBPP 24
#include "templates.h"
#undef SBPP

#define SBPP 32
#include "templates.h"
#undef SBPP

// clang-format off

Scaler Scale1x = {
        1,
        1,
        {Scale1x_8,  Scale1x_15, Scale1x_16,
         Scale1x_24, Scale1x_32, Scale1x_9}
};

// Renders double-wide DOS video modes
Scaler ScaleHoriz2x = {
        2,
        1,
        {ScaleHoriz2x_8,  ScaleHoriz2x_15, ScaleHoriz2x_16,
         ScaleHoriz2x_24, ScaleHoriz2x_32, ScaleHoriz2x_9}
};

// Renders double-high DOS video modes
Scaler ScaleVert2x = {
        1,
        2,
        {ScaleVert2x_8,  ScaleVert2x_15, ScaleVert2x_16,
         ScaleVert2x_24, ScaleVert2x_32, ScaleVert2x_9}
};

Scaler Scale2x = {
        2,
        2,
        {Scale2x_8 , Scale2x_15, Scale2x_16,
         Scale2x_24, Scale2x_32, Scale2x_9}
};

// clang-format on
