// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#if DBPP == 8
#	define PSIZE 1
#	define PTYPE uint8_t
#	define WC    scalerWriteCache.b8
// #define FC scalerFrameCache.b8
#	define FC         (*(scalerFrameCache_t*)(&scalerSourceCache.b32[400][0])).b8
#	define redMask    0
#	define greenMask  0
#	define blueMask   0
#	define redBits    0
#	define greenBits  0
#	define blueBits   0
#	define redShift   0
#	define greenShift 0
#	define blueShift  0
#elif DBPP == 15 || DBPP == 16
#	define PSIZE 2
#	define PTYPE uint16_t
#	define WC    scalerWriteCache.b16
// #define FC scalerFrameCache.b16
#	define FC    (*(scalerFrameCache_t*)(&scalerSourceCache.b32[400][0])).b16
#	if DBPP == 15
#		define redMask    0x7C00
#		define greenMask  0x03E0
#		define blueMask   0x001F
#		define redBits    5
#		define greenBits  5
#		define blueBits   5
#		define redShift   10
#		define greenShift 5
#		define blueShift  0
#	elif DBPP == 16
#		define redMask    0xF800
#		define greenMask  0x07E0
#		define blueMask   0x001F
#		define redBits    5
#		define greenBits  6
#		define blueBits   5
#		define redShift   11
#		define greenShift 5
#		define blueShift  0
#	endif
#elif DBPP == 32
#	define PSIZE      4
#	define PTYPE      uint32_t
#	define WC         scalerWriteCache.b32
// #define FC scalerFrameCache.b32
#	define FC         (*(scalerFrameCache_t*)(&scalerSourceCache.b32[400][0])).b32
#	define redMask    0xff0000
#	define greenMask  0x00ff00
#	define blueMask   0x0000ff
#	define redBits    8
#	define greenBits  8
#	define blueBits   8
#	define redShift   16
#	define greenShift 8
#	define blueShift  0
#endif

#define redblueMask (redMask | blueMask)

#if SBPP == 8 || SBPP == 9
#	define SC scalerSourceCache.b8
#	if DBPP == 8
#		define PMAKE(_VAL) (_VAL)
#	elif DBPP == 15
#		define PMAKE(_VAL) render.pal.lut.b16[_VAL]
#	elif DBPP == 16
#		define PMAKE(_VAL) render.pal.lut.b16[_VAL]
#	elif DBPP == 32
#		define PMAKE(_VAL) render.pal.lut.b32[_VAL]
#	endif
#	define SRCTYPE uint8_t
#endif

#if SBPP == 15
#	define SC scalerSourceCache.b16
#	ifdef WORDS_BIGENDIAN
#		if DBPP == 15 // GGGBBBBBxRRRRRGG -> xRRRRRGGGGGBBBBB
#			define PMAKE(_VAL) (((_VAL >> 8) & 0x00FF) | ((_VAL << 8) & 0xFF00))
#		elif DBPP == 16 // gggBBBBBxRRRRRGg -> RRRRRGggggGBBBBB
#			define PMAKE(_VAL) \
				(((_VAL >> 8) & 0x001F) | ((_VAL >> 7) & 0x01C0) | \
				 ((_VAL << 9) & 0xFE00) | ((_VAL << 4) & 0x0020))
#		elif DBPP == 32 // GggBBBbbxRRRrrGG -> 00000000RRRrrRRRGGGggGGGBBBbbBBB
#			define PMAKE(_VAL) \
				(((_VAL << 17) & 0x00F80000) | ((_VAL << 12) & 0x00070000) | \
				 ((_VAL << 14) & 0x0000C000) | ((_VAL >> 2) & 0x00003800) | \
				 ((_VAL << 9) & 0x00000600) | ((_VAL >> 7) & 0x00000100) | \
				 ((_VAL >> 5) & 0x000000F8) | ((_VAL >> 10) & 0x00000007))
#		endif
#	else
#		if DBPP == 15
#			define PMAKE(_VAL) (_VAL)
#		elif DBPP == 16 // xRRRRRGggggBBBBB -> RRRRRGggggGBBBBB
#			define PMAKE(_VAL) \
				((_VAL & 31) | ((_VAL & ~31) << 1) | ((_VAL & 0x0200) >> 4))
#		elif DBPP == 32 // xRRRrrGGGggBBBbb -> RRRrrRRRGGGggGGGBBBbbBBB
#			define PMAKE(_VAL) \
				(((_VAL & (31 << 10)) << 9) | ((_VAL & (31 << 5)) << 6) | \
				 ((_VAL & 31) << 3) | ((_VAL & (7 << 12)) << 4) | \
				 ((_VAL & (7 << 7)) << 1) | ((_VAL & (7 << 2)) >> 2))
#		endif
#	endif
#	define SRCTYPE uint16_t
#endif

#if SBPP == 16
#	define SC scalerSourceCache.b16
#	ifdef WORDS_BIGENDIAN
#		if DBPP == 15 // GGgBBBBBRRRRRGGG -> 0RRRRRGGGGGBBBBB
#			define PMAKE(_VAL) \
				(((_VAL >> 8) & 0x001F) | ((_VAL >> 9) & 0x0060) | \
				 ((_VAL << 7) & 0x7F80))
#		elif DBPP == 16 // GGGBBBBBRRRRRGGG -> RRRRRGGGGGGBBBBB
#			define PMAKE(_VAL) (((_VAL >> 8) & 0x00FF) | ((_VAL << 8) & 0xFF00))
#		elif DBPP == 32 // gggBBBbbRRRrrGGg -> RRRrrRRRGGggggGGBBBbbBBB
#			define PMAKE(_VAL) \
				(((_VAL << 16) & 0x00F80000) | ((_VAL << 11) & 0x00070000) | \
				 ((_VAL << 13) & 0x0000E000) | ((_VAL >> 3) & 0x00001C00) | \
				 ((_VAL << 7) & 0x00000300) | ((_VAL >> 5) & 0x000000F8) | \
				 ((_VAL >> 10) & 0x00000007))
#		endif
#	else
#		if DBPP == 15 // RRRRRGGGGGgBBBBB -> 0RRRRRGGGGGBBBBB
#			define PMAKE(_VAL) (((_VAL & ~63) >> 1) | (_VAL & 31))
#		elif DBPP == 16
#			define PMAKE(_VAL) (_VAL)
#		elif DBPP == 32 // RRRrrGGggggBBBbb -> RRRrrRRRGGggggGGBBBbbBBB
#			define PMAKE(_VAL) \
				(((_VAL & (31 << 11)) << 8) | ((_VAL & (63 << 5)) << 5) | \
				 ((_VAL & 0xE01F) << 3) | ((_VAL & (3 << 9)) >> 1) | \
				 ((_VAL & (7 << 2)) >> 2))
#		endif
#	endif
#	define SRCTYPE uint16_t
#endif

#if SBPP == 24
#	define SC scalerSourceCache.b32
#	if DBPP == 15
#		define PMAKE(_VAL) \
			(PTYPE)(((_VAL & (31 << 19)) >> 9) | ((_VAL & (31 << 11)) >> 6) | \
			        ((_VAL & (31 << 3)) >> 3))
#	elif DBPP == 16
#		define PMAKE(_VAL) \
			(PTYPE)(((_VAL & (31 << 19)) >> 8) | ((_VAL & (63 << 10)) >> 4) | \
			        ((_VAL & (31 << 3)) >> 3))
#	elif DBPP == 32
#		define PMAKE(_VAL) (_VAL)
#	endif
#	include "rgb888.h"
#	define SRCTYPE Rgb888
#endif

#if SBPP == 32
#	define SC scalerSourceCache.b32
#	ifdef WORDS_BIGENDIAN
#		if DBPP == 15 // BBBBBbbbGGGGGgggRRRRRrrrxxxxxxxx -> 0RRRRRGGGGGBBBBB
#			define PMAKE(_VAL) \
				(PTYPE)(((_VAL >> 27) & 0x001F) | ((_VAL >> 14) & 0x03E0) | \
				        ((_VAL >> 1) & 0x7C00))
#		elif DBPP == 16 // BBBBBbbbGGGGGGggRRRRRrrrxxxxxxxx -> RRRRRGGGGGGBBBBB
#			define PMAKE(_VAL) \
				(PTYPE)(((_VAL >> 27) & 0x001F) | ((_VAL >> 13) & 0x07E0) | \
				        (_VAL & 0xF800))
#		elif DBPP == 32 // BBBBBBBBGGGGGGGGRRRRRRRRxxxxxxxx -> RRRRRRRRGGGGGGGGBBBBBBBB
#			define PMAKE(_VAL) \
				(((_VAL >> 24) & 0x000000FF) | ((_VAL >> 8) & 0x0000FF00) | \
				 ((_VAL << 8) & 0x00FF0000))
#		endif
#	else
#		if DBPP == 15 // RRRRRrrrGGGGGgggBBBBBbbb -> 0RRRRRGGGGGBBBBB
#			define PMAKE(_VAL) \
				(PTYPE)(((_VAL & (31 << 19)) >> 9) | ((_VAL & (31 << 11)) >> 6) | \
				        ((_VAL & (31 << 3)) >> 3))
#		elif DBPP == 16 // RRRRRrrrGGGGGGggBBBBBbbb -> RRRRRGGGGGGBBBBB
#			define PMAKE(_VAL) \
				(PTYPE)(((_VAL & (31 << 19)) >> 8) | ((_VAL & (63 << 10)) >> 5) | \
				        ((_VAL & (31 << 3)) >> 3))
#		elif DBPP == 32
#			define PMAKE(_VAL) (_VAL)
#		endif
#	endif
#	define SRCTYPE uint32_t
#endif

//  C0 C1 C2 D3
//  C3 C4 C5 D4
//  C6 C7 C8 D5
//  D0 D1 D2 D6

#define C0 fc[-1]
#define C1 fc[+0]
#define C2 fc[+1]
#define C3 fc[-1]
#define C4 fc[+0]
#define C5 fc[+1]
#define C6 fc[-1]
#define C7 fc[+0]
#define C8 fc[+1]

#define D0 fc[-1]
#define D1 fc[+0]
#define D2 fc[+1]
#define D3 fc[+2]
#define D4 fc[+2]
#define D5 fc[+2]
#define D6 fc[+2]

#if (SBPP != 9) || (DBPP != 8)

/* Simple scalers */
#	define SCALERNAME   Normal1x
#	define SCALERWIDTH  1
#	define SCALERHEIGHT 1
#	define SCALERFUNC   line0[0] = P;
#	include "simple.h"

#	undef SCALERNAME
#	undef SCALERWIDTH
#	undef SCALERHEIGHT
#	undef SCALERFUNC

#	define SCALERNAME   Normal2x
#	define SCALERWIDTH  2
#	define SCALERHEIGHT 2
#	define SCALERFUNC \
       line0[0] = P; \
       line0[1] = P; \
       line1[0] = P;                                                           \
       line1[1] = P;
#	include "simple.h"

#	undef SCALERNAME
#	undef SCALERWIDTH
#	undef SCALERHEIGHT
#	undef SCALERFUNC

#	define SCALERNAME   NormalDw
#	define SCALERWIDTH  2
#	define SCALERHEIGHT 1
#	define SCALERFUNC \
		line0[0] = P; \
		line0[1] = P;
#	include "simple.h"

#	undef SCALERNAME
#	undef SCALERWIDTH
#	undef SCALERHEIGHT
#	undef SCALERFUNC

#	define SCALERNAME   NormalDh
#	define SCALERWIDTH  1
#	define SCALERHEIGHT 2
#	define SCALERFUNC \
		line0[0] = P; \
		line1[0] = P;
#	include "simple.h"
#	undef SCALERNAME
#	undef SCALERWIDTH
#	undef SCALERHEIGHT
#	undef SCALERFUNC

#endif // (SBPP != 9) || (DBPP != 8)

#undef PSIZE
#undef PTYPE
#undef PMAKE
#undef WC
#undef LC
#undef FC
#undef SC
#undef redMask
#undef greenMask
#undef blueMask
#undef redblueMask
#undef redBits
#undef greenBits
#undef blueBits
#undef redShift
#undef greenShift
#undef blueShift
#undef SRCTYPE
