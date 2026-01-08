// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#define PSIZE      4
#define PTYPE      uint32_t

#if SBPP == 8 || SBPP == 9
#define PMAKE(_VAL) render.pal.lut[_VAL]
#define SRCTYPE     uint8_t
#endif

#if SBPP == 15
#ifdef WORDS_BIGENDIAN
#// GggBBBbbxRRRrrGG -> 00000000RRRrrRRRGGGggGGGBBBbbBBB
#define PMAKE(_VAL) \
	(((_VAL << 17) & 0x00F80000) | ((_VAL << 12) & 0x00070000) | \
	 ((_VAL << 14) & 0x0000C000) | ((_VAL >> 2) & 0x00003800) | \
	 ((_VAL << 9) & 0x00000600) | ((_VAL >> 7) & 0x00000100) | \
	 ((_VAL >> 5) & 0x000000F8) | ((_VAL >> 10) & 0x00000007))
#else
#// xRRRrrGGGggBBBbb -> RRRrrRRRGGGggGGGBBBbbBBB
#define PMAKE(_VAL) \
	(((_VAL & (31 << 10)) << 9) | ((_VAL & (31 << 5)) << 6) | \
	 ((_VAL & 31) << 3) | ((_VAL & (7 << 12)) << 4) | \
	 ((_VAL & (7 << 7)) << 1) | ((_VAL & (7 << 2)) >> 2))
#endif
#define SRCTYPE uint16_t
#endif

#if SBPP == 16
#ifdef WORDS_BIGENDIAN
#// gggBBBbbRRRrrGGg -> RRRrrRRRGGggggGGBBBbbBBB
#define PMAKE(_VAL) \
	(((_VAL << 16) & 0x00F80000) | ((_VAL << 11) & 0x00070000) | \
	 ((_VAL << 13) & 0x0000E000) | ((_VAL >> 3) & 0x00001C00) | \
	 ((_VAL << 7) & 0x00000300) | ((_VAL >> 5) & 0x000000F8) | \
	 ((_VAL >> 10) & 0x00000007))
#else
#// RRRrrGGggggBBBbb -> RRRrrRRRGGggggGGBBBbbBBB
#define PMAKE(_VAL) \
	(((_VAL & (31 << 11)) << 8) | ((_VAL & (63 << 5)) << 5) | \
	 ((_VAL & 0xE01F) << 3) | ((_VAL & (3 << 9)) >> 1) | \
	 ((_VAL & (7 << 2)) >> 2))
#endif
#define SRCTYPE uint16_t
#endif

#if SBPP == 24
#define PMAKE(_VAL) (_VAL)
#include "utils/rgb888.h"
#define SRCTYPE Rgb888
#endif

#if SBPP == 32
#ifdef WORDS_BIGENDIAN
#// BBBBBBBBGGGGGGGGRRRRRRRRxxxxxxxx -> RRRRRRRRGGGGGGGGBBBBBBBB
#define PMAKE(_VAL) \
	(((_VAL >> 24) & 0x000000FF) | ((_VAL >> 8) & 0x0000FF00) | \
	 ((_VAL << 8) & 0x00FF0000))
#else
#define PMAKE(_VAL) (_VAL)
#endif
#define SRCTYPE uint32_t
#endif

/* Simple scalers */
#define SCALERNAME   Scale1x
#define SCALERWIDTH  1
#define SCALERHEIGHT 1
#define SCALERFUNC   line0[0] = P;
#include "simple.h"

#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME   Scale2x
#define SCALERWIDTH  2
#define SCALERHEIGHT 2
#define SCALERFUNC \
	line0[0] = P; \
	line0[1] = P; \
	line1[0] = P; \
	line1[1] = P;
#include "simple.h"

#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME   ScaleHoriz2x
#define SCALERWIDTH  2
#define SCALERHEIGHT 1
#define SCALERFUNC \
	line0[0] = P; \
	line0[1] = P;
#include "simple.h"

#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#define SCALERNAME   ScaleVert2x
#define SCALERWIDTH  1
#define SCALERHEIGHT 2
#define SCALERFUNC \
	line0[0] = P; \
	line1[0] = P;
#include "simple.h"

#undef SCALERNAME
#undef SCALERWIDTH
#undef SCALERHEIGHT
#undef SCALERFUNC

#undef PSIZE
#undef PTYPE
#undef PMAKE
#undef SRCTYPE
