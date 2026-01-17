// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#if SBPP == 8 || SBPP == 9
#define PMAKE(_VAL) render.palette.lut[_VAL]
#define SRCTYPE     uint8_t
#endif

#if SBPP == 15
#// xRRRrrGGGggBBBbb -> RRRrrRRRGGGggGGGBBBbbBBB
#define PMAKE(_VAL) \
	(((_VAL & (31 << 10)) << 9) | ((_VAL & (31 << 5)) << 6) | \
	 ((_VAL & 31) << 3) | ((_VAL & (7 << 12)) << 4) | \
	 ((_VAL & (7 << 7)) << 1) | ((_VAL & (7 << 2)) >> 2))
#define SRCTYPE uint16_t
#endif

#if SBPP == 16
#// RRRrrGGggggBBBbb -> RRRrrRRRGGggggGGBBBbbBBB
#define PMAKE(_VAL) \
	(((_VAL & (31 << 11)) << 8) | ((_VAL & (63 << 5)) << 5) | \
	 ((_VAL & 0xE01F) << 3) | ((_VAL & (3 << 9)) >> 1) | \
	 ((_VAL & (7 << 2)) >> 2))
#define SRCTYPE uint16_t
#endif

#if SBPP == 24
#define PMAKE(_VAL) (_VAL)
#include "utils/rgb888.h"
#define SRCTYPE Rgb888
#endif

#if SBPP == 32
#define PMAKE(_VAL) (_VAL)
#define SRCTYPE     uint32_t
#endif

// Simple scalers
#define SCALERNAME   Scale1x
#define SCALERWIDTH  1
#define SCALERHEIGHT 1
#define SCALERFUNC   out_line0[0] = P;
#include "simple.h"

#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME   Scale2x
#define SCALERWIDTH  2
#define SCALERHEIGHT 2
#define SCALERFUNC \
	out_line0[0] = P; \
	out_line0[1] = P; \
	out_line1[0] = P; \
	out_line1[1] = P;
#include "simple.h"

#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME   ScaleHoriz2x
#define SCALERWIDTH  2
#define SCALERHEIGHT 1
#define SCALERFUNC \
	out_line0[0] = P; \
	out_line0[1] = P;
#include "simple.h"

#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME   ScaleVert2x
#define SCALERWIDTH  1
#define SCALERHEIGHT 2
#define SCALERFUNC \
	out_line0[0] = P; \
	out_line1[0] = P;
#include "simple.h"

#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#undef PMAKE
#undef SRCTYPE
