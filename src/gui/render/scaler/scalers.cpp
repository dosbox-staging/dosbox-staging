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

uint8_t Scaler_Aspect[SCALER_MAXHEIGHT]        = {};
uint16_t Scaler_ChangedLines[SCALER_MAXHEIGHT] = {};

Bitu Scaler_ChangedLineIndex = 0;

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
	if ((Scaler_ChangedLineIndex & 1) == changed) {
		Scaler_ChangedLines[Scaler_ChangedLineIndex] += count;
	} else {
		Scaler_ChangedLines[++Scaler_ChangedLineIndex] = count;
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

ScalerSimpleBlock_t ScaleNormal1x = {
        1,
        1,
        {Normal1x_8, Normal1x_15, Normal1x_16, Normal1x_24, Normal1x_32, Normal1x_9}
};

// Renders double-wide DOS video modes
ScalerSimpleBlock_t ScaleNormalDw = {
        2,
        1,
        {NormalDw_8, NormalDw_15, NormalDw_16, NormalDw_24, NormalDw_32, NormalDw_9}
};

// Renders double-high DOS video modes
ScalerSimpleBlock_t ScaleNormalDh = {
        1,
        2,
        {NormalDh_8, NormalDh_15, NormalDh_16, NormalDh_24, NormalDh_32, NormalDh_9}
};

ScalerSimpleBlock_t ScaleNormal2x = {
        2,
        2,
        {Normal2x_8, Normal2x_15, Normal2x_16, Normal2x_24, Normal2x_32, Normal2x_9}
};

