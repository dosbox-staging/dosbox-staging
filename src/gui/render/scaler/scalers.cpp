// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later


//TODO:
//Maybe just do the cache checking back into the simple scalers so they can
//just handle it all in one go, but this seems to work well enough for now

#include "dosbox.h"

#include "gui/private/common.h"

#include "gui/render/render.h"

#include <cstring>

uint8_t Scaler_Aspect[SCALER_MAXHEIGHT]        = {};
uint16_t Scaler_ChangedLines[SCALER_MAXHEIGHT] = {};

Bitu Scaler_ChangedLineIndex = 0;

scalerSourceCache_t scalerSourceCache;

#define _conc7(A,B,C,D,E,F,G) A ## B ## C ## D ## E ## F ## G

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


/* Include the different rendering routines */
#define SBPP 8
#define DBPP 32
#include "templates.h"
#undef SBPP
#undef DBPP

/* SBPP 9 is a special case with palette check support */
#define SBPP 9
#define DBPP 32
#include "templates.h"
#undef SBPP
#undef DBPP

#define SBPP 15
#define DBPP 32
#include "templates.h"
#undef SBPP
#undef DBPP

#define SBPP 16
#define DBPP 32
#include "templates.h"
#undef SBPP
#undef DBPP

#define SBPP 24
#define DBPP 32
#include "templates.h"
#undef SBPP
#undef DBPP

#define SBPP 32
#define DBPP 32
#include "templates.h"
#undef SBPP
#undef DBPP

// clang-format off

ScalerSimpleBlock_t ScaleNormal1x = {
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32,
	1,1,{
{	Normal1x_8_32_R },
{	Normal1x_15_32_R},
{	Normal1x_16_32_R},
{	Normal1x_24_32_R},
{	Normal1x_32_32_R},
{	Normal1x_9_32_R }
}};

// Renders double-wide DOS video modes
ScalerSimpleBlock_t ScaleNormalDw = {
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32,
	2,1,{
{	NormalDw_8_32_R },
{	NormalDw_15_32_R},
{	NormalDw_16_32_R},
{	NormalDw_24_32_R},
{	NormalDw_32_32_R},
{	NormalDw_9_32_R }
}};

// Renders double-high DOS video modes
ScalerSimpleBlock_t ScaleNormalDh = {
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32,
	1,2,{
{	NormalDh_8_32_R },
{	NormalDh_15_32_R},
{	NormalDh_16_32_R},
{	NormalDh_24_32_R},
{	NormalDh_32_32_R},
{	NormalDh_9_32_R }
}};

ScalerSimpleBlock_t ScaleNormal2x = {
	GFX_CAN_8|GFX_CAN_15|GFX_CAN_16|GFX_CAN_32,
	2,2,{
{   Normal2x_8_32_R },
{   Normal2x_15_32_R},
{   Normal2x_16_32_R},
{   Normal2x_24_32_R},
{   Normal2x_32_32_R},
{   Normal2x_9_32_R },
}};

// clang-format on
