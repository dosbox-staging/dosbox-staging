// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-FileCopyrightText:  Aaron Giles
// SPDX-FileCopyrightText:  kekko
// SPDX-FileCopyrightText:  Bernhard Schelling
// SPDX-License-Identifier: BSD-3-Clause AND GPL-2.0-or-later

/***************************************************************************/
/*        Portion of this software comes with the following license:       */
/***************************************************************************/
/*
    Copyright Aaron Giles
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

        * Redistributions of source code must retain the above copyright
          notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
          notice, this list of conditions and the following disclaimer in
          the documentation and/or other materials provided with the
          distribution.
        * Neither the name 'MAME' nor the names of its contributors may be
          used to endorse or promote products derived from this software
          without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY AARON GILES ''AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL AARON GILES BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

/*
  3dfx Voodoo Graphics SST-1/2 emulator by Aaron Giles

  DOSBox integration by kekko (https://www.vogons.org/viewtopic.php?f=41&t=41853)

  Code cleanups and multi-threaded triangle rendering by Bernhard Schelling

  DOSBox Staging team:
   - Separated OpenGL from software render via preprocessor statements
   - Migrated to C++ threading
   - Modernized and cleaned up some (not all) code

  TODO: Import and adapt Aaron's latest MAME Voodoo sources.
*/

#include "dosbox.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include <SDL.h>
#include <SDL_cpuinfo.h> // for proper SSE defines for MSVC

#include "vga.h"

#include "config/config.h"
#include "config/setup.h"
#include "cpu/paging.h"
#include "gui/render.h"
#include "hardware/memory.h"
#include "hardware/pci_bus.h"
#include "hardware/pic.h"
#include "misc/cross.h"
#include "misc/support.h"
#include "simde/x86/sse2.h"
#include "utils/bitops.h"
#include "utils/byteorder.h"
#include "utils/fraction.h"
#include "utils/math_utils.h"

#ifndef DOSBOX_VOODOO_TYPES_H
#define DOSBOX_VOODOO_TYPES_H

// #define DEBUG_VOODOO 1

/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

using attoseconds_t = int64_t;

#define ATTOSECONDS_PER_SECOND_SQRT		((attoseconds_t)1000000000)
#define ATTOSECONDS_PER_SECOND			(ATTOSECONDS_PER_SECOND_SQRT * ATTOSECONDS_PER_SECOND_SQRT)

/* convert between hertz (as a double) and attoseconds */
#define ATTOSECONDS_TO_HZ(x)			((double)ATTOSECONDS_PER_SECOND / (double)(x))
#define HZ_TO_ATTOSECONDS(x)			((attoseconds_t)(ATTOSECONDS_PER_SECOND / (x)))

#define MAX_VERTEX_PARAMS					6

/* poly_extent describes start/end points for a scanline, along with per-scanline parameters */
struct poly_extent
{
	int32_t startx = 0; // starting X coordinate (inclusive)
	int32_t stopx  = 0; // ending X coordinate (exclusive)
};

/* an rgb_t is a single combined R,G,B (and optionally alpha) value */
using rgb_t = uint32_t;

/* an rgb15_t is a single combined 15-bit R,G,B value */
using rgb15_t = uint16_t;

/* macros to assemble rgb_t values */
#define MAKE_ARGB(a,r,g,b)	((((a) & 0xff) << 24) | (((r) & 0xff) << 16) | (((g) & 0xff) << 8) | ((b) & 0xff))
#define MAKE_RGB(r,g,b)		(MAKE_ARGB(255,r,g,b))

/* macros to extract components from rgb_t values */
#define RGB_ALPHA(rgb)		(((rgb) >> 24) & 0xff)
#define RGB_RED(rgb)		(((rgb) >> 16) & 0xff)
#define RGB_GREEN(rgb)		(((rgb) >> 8) & 0xff)
#define RGB_BLUE(rgb)		((rgb) & 0xff)

/* common colors */
#define RGB_BLACK			(MAKE_ARGB(255,0,0,0))
#define RGB_WHITE			(MAKE_ARGB(255,255,255,255))

/***************************************************************************
    inline FUNCTIONS
***************************************************************************/

// Debug log wrapper that only logs if 'DEBUG_VOODOO' is defined above)
template <typename... Args>
constexpr void maybe_log_debug([[maybe_unused]] const char* format,
                               [[maybe_unused]] Args... args)
{
	#ifdef DEBUG_VOODOO

	const auto prefixed_format = std::string("VOODOO: ") + format;
	LOG_DEBUG(prefixed_format.c_str(), std::forward<Args>(args)...);

	#endif
	// Otherwise this is a no-op
}

/*-------------------------------------------------
    pal5bit - convert a 5-bit value to 8 bits
-------------------------------------------------*/
inline uint8_t pal5bit(uint8_t bits)
{
	bits &= 0x1f;
	return (bits << 3) | (bits >> 2);
}

#ifdef C_ENABLE_VOODOO_DEBUG
/* rectangles describe a bitmap portion */
struct rectangle
{
	int				min_x;			/* minimum X, or left coordinate */
	int				max_x;			/* maximum X, or right coordinate (inclusive) */
	int				min_y;			/* minimum Y, or top coordinate */
	int				max_y;			/* maximum Y, or bottom coordinate (inclusive) */
};
#endif

#define ACCESSING_BITS_0_15				((mem_mask & 0x0000ffff) != 0)
#define ACCESSING_BITS_16_31			((mem_mask & 0xffff0000) != 0)

//// constants for expression endianness
#ifndef WORDS_BIGENDIAN
#define NATIVE_ENDIAN_VALUE_LE_BE(leval,beval)	(leval)
#else
#define NATIVE_ENDIAN_VALUE_LE_BE(leval,beval)	(beval)
#endif

#define BYTE4_XOR_LE(a) 				((a) ^ NATIVE_ENDIAN_VALUE_LE_BE(0,3))

#define BYTE_XOR_LE(a)  				((a) ^ NATIVE_ENDIAN_VALUE_LE_BE(0,1))

#define profiler_mark_start(x)	do { } while (0)
#define profiler_mark_end()		do { } while (0)

inline int32_t mul_32x32_shift(int32_t a, int32_t b, int8_t shift)
{
	return (int32_t)(((int64_t)a * (int64_t)b) >> shift);
}

static simde__m128i sse2_scale_table[256];

inline rgb_t rgba_bilinear_filter(rgb_t rgb00, rgb_t rgb01, rgb_t rgb10,
                                  rgb_t rgb11, uint8_t u, uint8_t v)
{
	const simde__m128i scale_u = sse2_scale_table[u];
	const simde__m128i scale_v = sse2_scale_table[v];
	return simde_mm_cvtsi128_si32(simde_mm_packus_epi16(
	        simde_mm_packs_epi32(
	                simde_mm_srli_epi32(
	                        simde_mm_madd_epi16(
	                                simde_mm_max_epi16(
	                                        simde_mm_slli_epi32(
	                                                simde_mm_madd_epi16(
	                                                        simde_mm_unpacklo_epi8(
	                                                                simde_mm_unpacklo_epi8(
	                                                                        simde_mm_cvtsi32_si128(rgb01),
	                                                                        simde_mm_cvtsi32_si128(rgb00)),
	                                                                simde_mm_setzero_si128()),
	                                                        scale_u),
	                                                15),
	                                        simde_mm_srli_epi32(
	                                                simde_mm_madd_epi16(
	                                                        simde_mm_unpacklo_epi8(
	                                                                simde_mm_unpacklo_epi8(
	                                                                        simde_mm_cvtsi32_si128(rgb11),
	                                                                        simde_mm_cvtsi32_si128(rgb10)),
	                                                                simde_mm_setzero_si128()),
	                                                        scale_u),
	                                                1)),
	                                scale_v),
	                        15),
	                simde_mm_setzero_si128()),
	        simde_mm_setzero_si128()));
}

struct poly_vertex
{
	float		x;							/* X coordinate */
	float		y;							/* Y coordinate */
	//float		p[MAX_VERTEX_PARAMS];		/* interpolated parameter values */
};
#endif //DOSBOX_VOODOO_TYPES_H

#ifndef DOSBOX_VOODOO_DATA_H
#define DOSBOX_VOODOO_DATA_H


/*************************************
 *
 *  Misc. constants
 *
 *************************************/

/* enumeration specifying which model of Voodoo we are emulating */
enum VoodooModel
{
	VOODOO_1,
	VOODOO_1_DTMU,
	VOODOO_2,
};

/* maximum number of TMUs */
#define MAX_TMU					2

#ifdef C_ENABLE_VOODOO_OPENGL
/* maximum number of rasterizers */
#define MAX_RASTERIZERS			1024

/* size of the rasterizer hash table */
#define RASTER_HASH_SIZE		97
#endif

/* flags for LFB writes */
#define LFB_RGB_PRESENT			1
#define LFB_ALPHA_PRESENT		2
#define LFB_DEPTH_PRESENT		4
#define LFB_DEPTH_PRESENT_MSW	8

/* flags for the register access array */
#define REGISTER_READ			0x01		/* reads are allowed */
#define REGISTER_WRITE			0x02		/* writes are allowed */
#define REGISTER_PIPELINED		0x04		/* writes are pipelined */
#define REGISTER_FIFO			0x08		/* writes go to FIFO */
#define REGISTER_WRITETHRU		0x10		/* writes are valid even for CMDFIFO */

/* shorter combinations to make the table smaller */
#define REG_R					(REGISTER_READ)
#define REG_W					(REGISTER_WRITE)
#define REG_WT					(REGISTER_WRITE | REGISTER_WRITETHRU)
#define REG_RW					(REGISTER_READ | REGISTER_WRITE)
#define REG_RWT					(REGISTER_READ | REGISTER_WRITE | REGISTER_WRITETHRU)
#define REG_RP					(REGISTER_READ | REGISTER_PIPELINED)
#define REG_WP					(REGISTER_WRITE | REGISTER_PIPELINED)
#define REG_RWP					(REGISTER_READ | REGISTER_WRITE | REGISTER_PIPELINED)
#define REG_RWPT				(REGISTER_READ | REGISTER_WRITE | REGISTER_PIPELINED | REGISTER_WRITETHRU)
#define REG_RF					(REGISTER_READ | REGISTER_FIFO)
#define REG_WF					(REGISTER_WRITE | REGISTER_FIFO)
#define REG_RWF					(REGISTER_READ | REGISTER_WRITE | REGISTER_FIFO)
#define REG_RPF					(REGISTER_READ | REGISTER_PIPELINED | REGISTER_FIFO)
#define REG_WPF					(REGISTER_WRITE | REGISTER_PIPELINED | REGISTER_FIFO)
#define REG_RWPF				(REGISTER_READ | REGISTER_WRITE | REGISTER_PIPELINED | REGISTER_FIFO)

/* lookup bits is the log2 of the size of the reciprocal/log table */
#define RECIPLOG_LOOKUP_BITS	9

/* fast reciprocal+log2 lookup */
static uint32_t voodoo_reciplog[(2 << RECIPLOG_LOOKUP_BITS) + 2];

/* input precision is how many fraction bits the input value has; this is a 64-bit number */
#define RECIPLOG_INPUT_PREC		32

/* lookup precision is how many fraction bits each table entry contains */
#define RECIPLOG_LOOKUP_PREC	22

/* output precision is how many fraction bits the result should have */
#define RECIP_OUTPUT_PREC		15
#define LOG_OUTPUT_PREC			8


/*************************************
 *
 *  Dithering tables
 *
 *************************************/

static constexpr uint8_t dither_matrix_4x4[16] =
        {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

static constexpr uint8_t dither_matrix_2x2[16] =
        {2, 10, 2, 10, 14, 6, 14, 6, 2, 10, 2, 10, 14, 6, 14, 6};

/*************************************
 *
 *  Macros for extracting pixels
 *
 *************************************/

#define EXTRACT_565_TO_888(val, a, b, c)					\
	(a) = (((val) >> 8) & 0xf8) | (((val) >> 13) & 0x07);	\
	(b) = (((val) >> 3) & 0xfc) | (((val) >> 9) & 0x03);	\
	(c) = (((val) << 3) & 0xf8) | (((val) >> 2) & 0x07);	\

#define EXTRACT_x555_TO_888(val, a, b, c)					\
	(a) = (((val) >> 7) & 0xf8) | (((val) >> 12) & 0x07);	\
	(b) = (((val) >> 2) & 0xf8) | (((val) >> 7) & 0x07);	\
	(c) = (((val) << 3) & 0xf8) | (((val) >> 2) & 0x07);	\

#define EXTRACT_555x_TO_888(val, a, b, c)					\
	(a) = (((val) >> 8) & 0xf8) | (((val) >> 13) & 0x07);	\
	(b) = (((val) >> 3) & 0xf8) | (((val) >> 8) & 0x07);	\
	(c) = (((val) << 2) & 0xf8) | (((val) >> 3) & 0x07);	\

#define EXTRACT_1555_TO_8888(val, a, b, c, d)				\
	(a) = ((int16_t)(val) >> 15) & 0xff;						\
	EXTRACT_x555_TO_888(val, b, c, d)						\

#define EXTRACT_5551_TO_8888(val, a, b, c, d)				\
	EXTRACT_555x_TO_888(val, a, b, c)						\
	(d) = ((val) & 0x0001) ? 0xff : 0x00;					\

#define EXTRACT_x888_TO_888(val, a, b, c)					\
	(a) = ((val) >> 16) & 0xff;								\
	(b) = ((val) >> 8) & 0xff;								\
	(c) = ((val) >> 0) & 0xff;								\

#define EXTRACT_888x_TO_888(val, a, b, c)					\
	(a) = ((val) >> 24) & 0xff;								\
	(b) = ((val) >> 16) & 0xff;								\
	(c) = ((val) >> 8) & 0xff;								\

#define EXTRACT_8888_TO_8888(val, a, b, c, d)				\
	(a) = ((val) >> 24) & 0xff;								\
	(b) = ((val) >> 16) & 0xff;								\
	(c) = ((val) >> 8) & 0xff;								\
	(d) = ((val) >> 0) & 0xff;								\

#define EXTRACT_4444_TO_8888(val, a, b, c, d)				\
	(a) = (((val) >> 8) & 0xf0) | (((val) >> 12) & 0x0f);	\
	(b) = (((val) >> 4) & 0xf0) | (((val) >> 8) & 0x0f);	\
	(c) = (((val) >> 0) & 0xf0) | (((val) >> 4) & 0x0f);	\
	(d) = (((val) << 4) & 0xf0) | (((val) >> 0) & 0x0f);	\

#define EXTRACT_332_TO_888(val, a, b, c)					\
	(a) = (((val) >> 0) & 0xe0) | (((val) >> 3) & 0x1c) | (((val) >> 6) & 0x03); \
	(b) = (((val) << 3) & 0xe0) | (((val) >> 0) & 0x1c) | (((val) >> 3) & 0x03); \
	(c) = (((val) << 6) & 0xc0) | (((val) << 4) & 0x30) | (((val) << 2) & 0xc0) | (((val) << 0) & 0x03); \

/*************************************
 *
 *  Macros for extracting bitfields
 *
 *************************************/

#define INITEN_ENABLE_HW_INIT(val)			(((val) >> 0) & 1)
#define INITEN_ENABLE_PCI_FIFO(val)			(((val) >> 1) & 1)
#define INITEN_REMAP_INIT_TO_DAC(val)		(((val) >> 2) & 1)
#define INITEN_ENABLE_SNOOP0(val)			(((val) >> 4) & 1)
#define INITEN_SNOOP0_MEMORY_MATCH(val)		(((val) >> 5) & 1)
#define INITEN_SNOOP0_READWRITE_MATCH(val)	(((val) >> 6) & 1)
#define INITEN_ENABLE_SNOOP1(val)			(((val) >> 7) & 1)
#define INITEN_SNOOP1_MEMORY_MATCH(val)		(((val) >> 8) & 1)
#define INITEN_SNOOP1_READWRITE_MATCH(val)	(((val) >> 9) & 1)
#define INITEN_SLI_BUS_OWNER(val)			(((val) >> 10) & 1)
#define INITEN_SLI_ODD_EVEN(val)			(((val) >> 11) & 1)
#define INITEN_SECONDARY_REV_ID(val)		(((val) >> 12) & 0xf)	/* voodoo 2 only */
#define INITEN_MFCTR_FAB_ID(val)			(((val) >> 16) & 0xf)	/* voodoo 2 only */
#define INITEN_ENABLE_PCI_INTERRUPT(val)	(((val) >> 20) & 1)		/* voodoo 2 only */
#define INITEN_PCI_INTERRUPT_TIMEOUT(val)	(((val) >> 21) & 1)		/* voodoo 2 only */
#define INITEN_ENABLE_NAND_TREE_TEST(val)	(((val) >> 22) & 1)		/* voodoo 2 only */
#define INITEN_ENABLE_SLI_ADDRESS_SNOOP(val) (((val) >> 23) & 1)	/* voodoo 2 only */
#define INITEN_SLI_SNOOP_ADDRESS(val)		(((val) >> 24) & 0xff)	/* voodoo 2 only */

#define FBZCP_CC_RGBSELECT(val)				(((val) >> 0) & 3)
#define FBZCP_CC_ASELECT(val)				(((val) >> 2) & 3)
#define FBZCP_CC_LOCALSELECT(val)			(((val) >> 4) & 1)
#define FBZCP_CCA_LOCALSELECT(val)			(((val) >> 5) & 3)
#define FBZCP_CC_LOCALSELECT_OVERRIDE(val)	(((val) >> 7) & 1)
#define FBZCP_CC_ZERO_OTHER(val)			(((val) >> 8) & 1)
#define FBZCP_CC_SUB_CLOCAL(val)			(((val) >> 9) & 1)
#define FBZCP_CC_MSELECT(val)				(((val) >> 10) & 7)
#define FBZCP_CC_REVERSE_BLEND(val)			(((val) >> 13) & 1)
#define FBZCP_CC_ADD_ACLOCAL(val)			(((val) >> 14) & 3)
#define FBZCP_CC_INVERT_OUTPUT(val)			(((val) >> 16) & 1)
#define FBZCP_CCA_ZERO_OTHER(val)			(((val) >> 17) & 1)
#define FBZCP_CCA_SUB_CLOCAL(val)			(((val) >> 18) & 1)
#define FBZCP_CCA_MSELECT(val)				(((val) >> 19) & 7)
#define FBZCP_CCA_REVERSE_BLEND(val)		(((val) >> 22) & 1)
#define FBZCP_CCA_ADD_ACLOCAL(val)			(((val) >> 23) & 3)
#define FBZCP_CCA_INVERT_OUTPUT(val)		(((val) >> 25) & 1)
#define FBZCP_CCA_SUBPIXEL_ADJUST(val)		(((val) >> 26) & 1)
#define FBZCP_TEXTURE_ENABLE(val)			(((val) >> 27) & 1)
#define FBZCP_RGBZW_CLAMP(val)				(((val) >> 28) & 1)		/* voodoo 2 only */
#define FBZCP_ANTI_ALIAS(val)				(((val) >> 29) & 1)		/* voodoo 2 only */

#define ALPHAMODE_ALPHATEST(val)			(((val) >> 0) & 1)
#define ALPHAMODE_ALPHAFUNCTION(val)		(((val) >> 1) & 7)
#define ALPHAMODE_ALPHABLEND(val)			(((val) >> 4) & 1)
#define ALPHAMODE_ANTIALIAS(val)			(((val) >> 5) & 1)
#define ALPHAMODE_SRCRGBBLEND(val)			(((val) >> 8) & 15)
#define ALPHAMODE_DSTRGBBLEND(val)			(((val) >> 12) & 15)
#define ALPHAMODE_SRCALPHABLEND(val)		(((val) >> 16) & 15)
#define ALPHAMODE_DSTALPHABLEND(val)		(((val) >> 20) & 15)
#define ALPHAMODE_ALPHAREF(val)				(((val) >> 24) & 0xff)

#define FOGMODE_ENABLE_FOG(val)				(((val) >> 0) & 1)
#define FOGMODE_FOG_ADD(val)				(((val) >> 1) & 1)
#define FOGMODE_FOG_MULT(val)				(((val) >> 2) & 1)
#define FOGMODE_FOG_ZALPHA(val)				(((val) >> 3) & 3)
#define FOGMODE_FOG_CONSTANT(val)			(((val) >> 5) & 1)
#define FOGMODE_FOG_DITHER(val)				(((val) >> 6) & 1)		/* voodoo 2 only */
#define FOGMODE_FOG_ZONES(val)				(((val) >> 7) & 1)		/* voodoo 2 only */

#define FBZMODE_ENABLE_CLIPPING(val)		(((val) >> 0) & 1)
#define FBZMODE_ENABLE_CHROMAKEY(val)		(((val) >> 1) & 1)
#define FBZMODE_ENABLE_STIPPLE(val)			(((val) >> 2) & 1)
#define FBZMODE_WBUFFER_SELECT(val)			(((val) >> 3) & 1)
#define FBZMODE_ENABLE_DEPTHBUF(val)		(((val) >> 4) & 1)
#define FBZMODE_DEPTH_FUNCTION(val)			(((val) >> 5) & 7)
#define FBZMODE_ENABLE_DITHERING(val)		(((val) >> 8) & 1)
#define FBZMODE_RGB_BUFFER_MASK(val)		(((val) >> 9) & 1)
#define FBZMODE_AUX_BUFFER_MASK(val)		(((val) >> 10) & 1)
#define FBZMODE_DITHER_TYPE(val)			(((val) >> 11) & 1)
#define FBZMODE_STIPPLE_PATTERN(val)		(((val) >> 12) & 1)
#define FBZMODE_ENABLE_ALPHA_MASK(val)		(((val) >> 13) & 1)
#define FBZMODE_DRAW_BUFFER(val)			(((val) >> 14) & 3)
#define FBZMODE_ENABLE_DEPTH_BIAS(val)		(((val) >> 16) & 1)
#define FBZMODE_Y_ORIGIN(val)				(((val) >> 17) & 1)
#define FBZMODE_ENABLE_ALPHA_PLANES(val)	(((val) >> 18) & 1)
#define FBZMODE_ALPHA_DITHER_SUBTRACT(val)	(((val) >> 19) & 1)
#define FBZMODE_DEPTH_SOURCE_COMPARE(val)	(((val) >> 20) & 1)
#define FBZMODE_DEPTH_FLOAT_SELECT(val)		(((val) >> 21) & 1)		/* voodoo 2 only */

#define LFBMODE_WRITE_FORMAT(val)			(((val) >> 0) & 0xf)
#define LFBMODE_WRITE_BUFFER_SELECT(val)	(((val) >> 4) & 3)
#define LFBMODE_READ_BUFFER_SELECT(val)		(((val) >> 6) & 3)
#define LFBMODE_ENABLE_PIXEL_PIPELINE(val)	(((val) >> 8) & 1)
#define LFBMODE_RGBA_LANES(val)				(((val) >> 9) & 3)
#define LFBMODE_WORD_SWAP_WRITES(val)		(((val) >> 11) & 1)
#define LFBMODE_BYTE_SWIZZLE_WRITES(val)	(((val) >> 12) & 1)
#define LFBMODE_Y_ORIGIN(val)				(((val) >> 13) & 1)
#define LFBMODE_WRITE_W_SELECT(val)			(((val) >> 14) & 1)
#define LFBMODE_WORD_SWAP_READS(val)		(((val) >> 15) & 1)
#define LFBMODE_BYTE_SWIZZLE_READS(val)		(((val) >> 16) & 1)

#define CHROMARANGE_BLUE_EXCLUSIVE(val)		(((val) >> 24) & 1)
#define CHROMARANGE_GREEN_EXCLUSIVE(val)	(((val) >> 25) & 1)
#define CHROMARANGE_RED_EXCLUSIVE(val)		(((val) >> 26) & 1)
#define CHROMARANGE_UNION_MODE(val)			(((val) >> 27) & 1)
#define CHROMARANGE_ENABLE(val)				(((val) >> 28) & 1)

#define FBIINIT0_VGA_PASSTHRU(val)			(((val) >> 0) & 1)
#define FBIINIT0_GRAPHICS_RESET(val)		(((val) >> 1) & 1)
#define FBIINIT0_FIFO_RESET(val)			(((val) >> 2) & 1)
#define FBIINIT0_SWIZZLE_REG_WRITES(val)	(((val) >> 3) & 1)
#define FBIINIT0_STALL_PCIE_FOR_HWM(val)	(((val) >> 4) & 1)
#define FBIINIT0_PCI_FIFO_LWM(val)			(((val) >> 6) & 0x1f)
#define FBIINIT0_LFB_TO_MEMORY_FIFO(val)	(((val) >> 11) & 1)
#define FBIINIT0_TEXMEM_TO_MEMORY_FIFO(val) (((val) >> 12) & 1)
#define FBIINIT0_ENABLE_MEMORY_FIFO(val)	(((val) >> 13) & 1)
#define FBIINIT0_MEMORY_FIFO_HWM(val)		(((val) >> 14) & 0x7ff)
#define FBIINIT0_MEMORY_FIFO_BURST(val)		(((val) >> 25) & 0x3f)

#define FBIINIT1_PCI_DEV_FUNCTION(val)		(((val) >> 0) & 1)
#define FBIINIT1_PCI_WRITE_WAIT_STATES(val)	(((val) >> 1) & 1)
#define FBIINIT1_MULTI_SST1(val)			(((val) >> 2) & 1)		/* not on voodoo 2 */
#define FBIINIT1_ENABLE_LFB(val)			(((val) >> 3) & 1)
#define FBIINIT1_X_VIDEO_TILES(val)			(((val) >> 4) & 0xf)
#define FBIINIT1_VIDEO_TIMING_RESET(val)	(((val) >> 8) & 1)
#define FBIINIT1_SOFTWARE_OVERRIDE(val)		(((val) >> 9) & 1)
#define FBIINIT1_SOFTWARE_HSYNC(val)		(((val) >> 10) & 1)
#define FBIINIT1_SOFTWARE_VSYNC(val)		(((val) >> 11) & 1)
#define FBIINIT1_SOFTWARE_BLANK(val)		(((val) >> 12) & 1)
#define FBIINIT1_DRIVE_VIDEO_TIMING(val)	(((val) >> 13) & 1)
#define FBIINIT1_DRIVE_VIDEO_BLANK(val)		(((val) >> 14) & 1)
#define FBIINIT1_DRIVE_VIDEO_SYNC(val)		(((val) >> 15) & 1)
#define FBIINIT1_DRIVE_VIDEO_DCLK(val)		(((val) >> 16) & 1)
#define FBIINIT1_VIDEO_TIMING_VCLK(val)		(((val) >> 17) & 1)
#define FBIINIT1_VIDEO_CLK_2X_DELAY(val)	(((val) >> 18) & 3)
#define FBIINIT1_VIDEO_TIMING_SOURCE(val)	(((val) >> 20) & 3)
#define FBIINIT1_ENABLE_24BPP_OUTPUT(val)	(((val) >> 22) & 1)
#define FBIINIT1_ENABLE_SLI(val)			(((val) >> 23) & 1)
#define FBIINIT1_X_VIDEO_TILES_BIT5(val)	(((val) >> 24) & 1)		/* voodoo 2 only */
#define FBIINIT1_ENABLE_EDGE_FILTER(val)	(((val) >> 25) & 1)
#define FBIINIT1_INVERT_VID_CLK_2X(val)		(((val) >> 26) & 1)
#define FBIINIT1_VID_CLK_2X_SEL_DELAY(val)	(((val) >> 27) & 3)
#define FBIINIT1_VID_CLK_DELAY(val)			(((val) >> 29) & 3)
#define FBIINIT1_DISABLE_FAST_READAHEAD(val) (((val) >> 31) & 1)

#define FBIINIT2_DISABLE_DITHER_SUB(val)	(((val) >> 0) & 1)
#define FBIINIT2_DRAM_BANKING(val)			(((val) >> 1) & 1)
#define FBIINIT2_ENABLE_TRIPLE_BUF(val)		(((val) >> 4) & 1)
#define FBIINIT2_ENABLE_FAST_RAS_READ(val)	(((val) >> 5) & 1)
#define FBIINIT2_ENABLE_GEN_DRAM_OE(val)	(((val) >> 6) & 1)
#define FBIINIT2_ENABLE_FAST_READWRITE(val)	(((val) >> 7) & 1)
#define FBIINIT2_ENABLE_PASSTHRU_DITHER(val) (((val) >> 8) & 1)
#define FBIINIT2_SWAP_BUFFER_ALGORITHM(val)	(((val) >> 9) & 3)
#define FBIINIT2_VIDEO_BUFFER_OFFSET(val)	(((val) >> 11) & 0x1ff)
#define FBIINIT2_ENABLE_DRAM_BANKING(val)	(((val) >> 20) & 1)
#define FBIINIT2_ENABLE_DRAM_READ_FIFO(val)	(((val) >> 21) & 1)
#define FBIINIT2_ENABLE_DRAM_REFRESH(val)	(((val) >> 22) & 1)
#define FBIINIT2_REFRESH_LOAD_VALUE(val)	(((val) >> 23) & 0x1ff)

#define FBIINIT3_TRI_REGISTER_REMAP(val)	(((val) >> 0) & 1)
#define FBIINIT3_VIDEO_FIFO_THRESH(val)		(((val) >> 1) & 0x1f)
#define FBIINIT3_DISABLE_TMUS(val)			(((val) >> 6) & 1)
#define FBIINIT3_FBI_MEMORY_TYPE(val)		(((val) >> 8) & 7)
#define FBIINIT3_VGA_PASS_RESET_VAL(val)	(((val) >> 11) & 1)
#define FBIINIT3_HARDCODE_PCI_BASE(val)		(((val) >> 12) & 1)
#define FBIINIT3_FBI2TREX_DELAY(val)		(((val) >> 13) & 0xf)
#define FBIINIT3_TREX2FBI_DELAY(val)		(((val) >> 17) & 0x1f)
#define FBIINIT3_YORIGIN_SUBTRACT(val)		(((val) >> 22) & 0x3ff)

#define FBIINIT4_PCI_READ_WAITS(val)		(((val) >> 0) & 1)
#define FBIINIT4_ENABLE_LFB_READAHEAD(val)	(((val) >> 1) & 1)
#define FBIINIT4_MEMORY_FIFO_LWM(val)		(((val) >> 2) & 0x3f)
#define FBIINIT4_MEMORY_FIFO_START_ROW(val)	(((val) >> 8) & 0x3ff)
#define FBIINIT4_MEMORY_FIFO_STOP_ROW(val)	(((val) >> 18) & 0x3ff)
#define FBIINIT4_VIDEO_CLOCKING_DELAY(val)	(((val) >> 29) & 7)		/* voodoo 2 only */

#define FBIINIT5_DISABLE_PCI_STOP(val)		(((val) >> 0) & 1)		/* voodoo 2 only */
#define FBIINIT5_PCI_SLAVE_SPEED(val)		(((val) >> 1) & 1)		/* voodoo 2 only */
#define FBIINIT5_DAC_DATA_OUTPUT_WIDTH(val)	(((val) >> 2) & 1)		/* voodoo 2 only */
#define FBIINIT5_DAC_DATA_17_OUTPUT(val)	(((val) >> 3) & 1)		/* voodoo 2 only */
#define FBIINIT5_DAC_DATA_18_OUTPUT(val)	(((val) >> 4) & 1)		/* voodoo 2 only */
#define FBIINIT5_GENERIC_STRAPPING(val)		(((val) >> 5) & 0xf)	/* voodoo 2 only */
#define FBIINIT5_BUFFER_ALLOCATION(val)		(((val) >> 9) & 3)		/* voodoo 2 only */
#define FBIINIT5_DRIVE_VID_CLK_SLAVE(val)	(((val) >> 11) & 1)		/* voodoo 2 only */
#define FBIINIT5_DRIVE_DAC_DATA_16(val)		(((val) >> 12) & 1)		/* voodoo 2 only */
#define FBIINIT5_VCLK_INPUT_SELECT(val)		(((val) >> 13) & 1)		/* voodoo 2 only */
#define FBIINIT5_MULTI_CVG_DETECT(val)		(((val) >> 14) & 1)		/* voodoo 2 only */
#define FBIINIT5_SYNC_RETRACE_READS(val)	(((val) >> 15) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_RHBORDER_COLOR(val)	(((val) >> 16) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_LHBORDER_COLOR(val)	(((val) >> 17) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_BVBORDER_COLOR(val)	(((val) >> 18) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_TVBORDER_COLOR(val)	(((val) >> 19) & 1)		/* voodoo 2 only */
#define FBIINIT5_DOUBLE_HORIZ(val)			(((val) >> 20) & 1)		/* voodoo 2 only */
#define FBIINIT5_DOUBLE_VERT(val)			(((val) >> 21) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_16BIT_GAMMA(val)	(((val) >> 22) & 1)		/* voodoo 2 only */
#define FBIINIT5_INVERT_DAC_HSYNC(val)		(((val) >> 23) & 1)		/* voodoo 2 only */
#define FBIINIT5_INVERT_DAC_VSYNC(val)		(((val) >> 24) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_24BIT_DACDATA(val)	(((val) >> 25) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_INTERLACING(val)	(((val) >> 26) & 1)		/* voodoo 2 only */
#define FBIINIT5_DAC_DATA_18_CONTROL(val)	(((val) >> 27) & 1)		/* voodoo 2 only */
#define FBIINIT5_RASTERIZER_UNIT_MODE(val)	(((val) >> 30) & 3)		/* voodoo 2 only */

#define FBIINIT6_WINDOW_ACTIVE_COUNTER(val)	(((val) >> 0) & 7)		/* voodoo 2 only */
#define FBIINIT6_WINDOW_DRAG_COUNTER(val)	(((val) >> 3) & 0x1f)	/* voodoo 2 only */
#define FBIINIT6_SLI_SYNC_MASTER(val)		(((val) >> 8) & 1)		/* voodoo 2 only */
#define FBIINIT6_DAC_DATA_22_OUTPUT(val)	(((val) >> 9) & 3)		/* voodoo 2 only */
#define FBIINIT6_DAC_DATA_23_OUTPUT(val)	(((val) >> 11) & 3)		/* voodoo 2 only */
#define FBIINIT6_SLI_SYNCIN_OUTPUT(val)		(((val) >> 13) & 3)		/* voodoo 2 only */
#define FBIINIT6_SLI_SYNCOUT_OUTPUT(val)	(((val) >> 15) & 3)		/* voodoo 2 only */
#define FBIINIT6_DAC_RD_OUTPUT(val)			(((val) >> 17) & 3)		/* voodoo 2 only */
#define FBIINIT6_DAC_WR_OUTPUT(val)			(((val) >> 19) & 3)		/* voodoo 2 only */
#define FBIINIT6_PCI_FIFO_LWM_RDY(val)		(((val) >> 21) & 0x7f)	/* voodoo 2 only */
#define FBIINIT6_VGA_PASS_N_OUTPUT(val)		(((val) >> 28) & 3)		/* voodoo 2 only */
#define FBIINIT6_X_VIDEO_TILES_BIT0(val)	(((val) >> 30) & 1)		/* voodoo 2 only */

#define FBIINIT7_GENERIC_STRAPPING(val)		(((val) >> 0) & 0xff)	/* voodoo 2 only */
#define FBIINIT7_CMDFIFO_ENABLE(val)		(((val) >> 8) & 1)		/* voodoo 2 only */
#define FBIINIT7_CMDFIFO_MEMORY_STORE(val)	(((val) >> 9) & 1)		/* voodoo 2 only */
#define FBIINIT7_DISABLE_CMDFIFO_HOLES(val)	(((val) >> 10) & 1)		/* voodoo 2 only */
#define FBIINIT7_CMDFIFO_READ_THRESH(val)	(((val) >> 11) & 0x1f)	/* voodoo 2 only */
#define FBIINIT7_SYNC_CMDFIFO_WRITES(val)	(((val) >> 16) & 1)		/* voodoo 2 only */
#define FBIINIT7_SYNC_CMDFIFO_READS(val)	(((val) >> 17) & 1)		/* voodoo 2 only */
#define FBIINIT7_RESET_PCI_PACKER(val)		(((val) >> 18) & 1)		/* voodoo 2 only */
#define FBIINIT7_ENABLE_CHROMA_STUFF(val)	(((val) >> 19) & 1)		/* voodoo 2 only */
#define FBIINIT7_CMDFIFO_PCI_TIMEOUT(val)	(((val) >> 20) & 0x7f)	/* voodoo 2 only */
#define FBIINIT7_ENABLE_TEXTURE_BURST(val)	(((val) >> 27) & 1)		/* voodoo 2 only */

#define TEXMODE_ENABLE_PERSPECTIVE(val)		(((val) >> 0) & 1)
#define TEXMODE_MINIFICATION_FILTER(val)	(((val) >> 1) & 1)
#define TEXMODE_MAGNIFICATION_FILTER(val)	(((val) >> 2) & 1)
#define TEXMODE_CLAMP_NEG_W(val)			(((val) >> 3) & 1)
#define TEXMODE_ENABLE_LOD_DITHER(val)		(((val) >> 4) & 1)
#define TEXMODE_NCC_TABLE_SELECT(val)		(((val) >> 5) & 1)
#define TEXMODE_CLAMP_S(val)				(((val) >> 6) & 1)
#define TEXMODE_CLAMP_T(val)				(((val) >> 7) & 1)
#define TEXMODE_FORMAT(val)					(((val) >> 8) & 0xf)
#define TEXMODE_TC_ZERO_OTHER(val)			(((val) >> 12) & 1)
#define TEXMODE_TC_SUB_CLOCAL(val)			(((val) >> 13) & 1)
#define TEXMODE_TC_MSELECT(val)				(((val) >> 14) & 7)
#define TEXMODE_TC_REVERSE_BLEND(val)		(((val) >> 17) & 1)
#define TEXMODE_TC_ADD_ACLOCAL(val)			(((val) >> 18) & 3)
#define TEXMODE_TC_INVERT_OUTPUT(val)		(((val) >> 20) & 1)
#define TEXMODE_TCA_ZERO_OTHER(val)			(((val) >> 21) & 1)
#define TEXMODE_TCA_SUB_CLOCAL(val)			(((val) >> 22) & 1)
#define TEXMODE_TCA_MSELECT(val)			(((val) >> 23) & 7)
#define TEXMODE_TCA_REVERSE_BLEND(val)		(((val) >> 26) & 1)
#define TEXMODE_TCA_ADD_ACLOCAL(val)		(((val) >> 27) & 3)
#define TEXMODE_TCA_INVERT_OUTPUT(val)		(((val) >> 29) & 1)
#define TEXMODE_TRILINEAR(val)				(((val) >> 30) & 1)
#define TEXMODE_SEQ_8_DOWNLD(val)			(((val) >> 31) & 1)

#define TEXLOD_LODMIN(val)					(((val) >> 0) & 0x3f)
#define TEXLOD_LODMAX(val)					(((val) >> 6) & 0x3f)
#define TEXLOD_LODBIAS(val)					(((val) >> 12) & 0x3f)
#define TEXLOD_LOD_ODD(val)					(((val) >> 18) & 1)
#define TEXLOD_LOD_TSPLIT(val)				(((val) >> 19) & 1)
#define TEXLOD_LOD_S_IS_WIDER(val)			(((val) >> 20) & 1)
#define TEXLOD_LOD_ASPECT(val)				(((val) >> 21) & 3)
#define TEXLOD_LOD_ZEROFRAC(val)			(((val) >> 23) & 1)
#define TEXLOD_TMULTIBASEADDR(val)			(((val) >> 24) & 1)
#define TEXLOD_TDATA_SWIZZLE(val)			(((val) >> 25) & 1)
#define TEXLOD_TDATA_SWAP(val)				(((val) >> 26) & 1)
#define TEXLOD_TDIRECT_WRITE(val)			(((val) >> 27) & 1)		/* Voodoo 2 only */

#define TEXDETAIL_DETAIL_MAX(val)			(((val) >> 0) & 0xff)
#define TEXDETAIL_DETAIL_BIAS(val)			(((val) >> 8) & 0x3f)
#define TEXDETAIL_DETAIL_SCALE(val)			(((val) >> 14) & 7)
#define TEXDETAIL_RGB_MIN_FILTER(val)		(((val) >> 17) & 1)		/* Voodoo 2 only */
#define TEXDETAIL_RGB_MAG_FILTER(val)		(((val) >> 18) & 1)		/* Voodoo 2 only */
#define TEXDETAIL_ALPHA_MIN_FILTER(val)		(((val) >> 19) & 1)		/* Voodoo 2 only */
#define TEXDETAIL_ALPHA_MAG_FILTER(val)		(((val) >> 20) & 1)		/* Voodoo 2 only */
#define TEXDETAIL_SEPARATE_RGBA_FILTER(val)	(((val) >> 21) & 1)		/* Voodoo 2 only */

#define TREXINIT_SEND_TMU_CONFIG(val)		(((val) >> 18) & 1)


/*************************************
 *
 *  Core types
 *
 *************************************/

using rgb_t = uint32_t;

struct rgba
{
#ifndef WORDS_BIGENDIAN
	uint8_t				b, g, r, a;
#else
	uint8_t				a, r, g, b;
#endif
};

union voodoo_reg
{
	int32_t				i;
	uint32_t				u;
	float				f;
	rgba				rgb;
};

using rgb_union = voodoo_reg;

/* note that this structure is an even 64 bytes long */
struct stats_block {
	int32_t pixels_in   = 0;
	int32_t pixels_out  = 0;
	int32_t chroma_fail = 0;
	int32_t zfunc_fail  = 0;
	int32_t afunc_fail  = 0;
	// int32_t clip_fail;       // clipping fail statistic
	// int32_t stipple_count;   // stipple statistic

	// pad this structure to 64 bytes
	int32_t filler[64 / 4 - 5] = {};
};
static_assert(sizeof(stats_block) == 64);

struct fifo_state
{
	int32_t				size;					/* size of the FIFO */
};

struct pci_state
{
	fifo_state			fifo;					/* PCI FIFO */
	uint32_t				init_enable;			/* initEnable value */
	bool				op_pending;				/* true if an operation is pending */
};

struct ncc_table
{
	bool				dirty;					/* is the texel lookup dirty? */
	voodoo_reg *		reg;					/* pointer to our registers */
	int32_t				ir[4], ig[4], ib[4];	/* I values for R,G,B */
	int32_t				qr[4], qg[4], qb[4];	/* Q values for R,G,B */
	int32_t				y[16];					/* Y values */
	rgb_t *				palette;				/* pointer to associated RGB palette */
	rgb_t *				palettea;				/* pointer to associated ARGB palette */
	rgb_t				texel[256];				/* texel lookup */
};

using mem_buffer_t = std::unique_ptr<uint8_t[]>;

struct tmu_state
{
	uint8_t*				ram;					/* pointer to aligned RAM */
	mem_buffer_t				ram_buffer;				/* Managed buffer backing the RAM */
	uint32_t				mask;					/* mask to apply to pointers */
	voodoo_reg *		reg;					/* pointer to our register base */
	bool				regdirty;				/* true if the LOD/mode/base registers have changed */

	enum { texaddr_mask = 0x0fffff, texaddr_shift = 3 };
	//uint32_t			texaddr_mask;			/* mask for texture address */
	//uint8_t				texaddr_shift;			/* shift for texture address */

	int64_t				starts, startt;			/* starting S,T (14.18) */
	int64_t				startw;					/* starting W (2.30) */
	int64_t				dsdx, dtdx;				/* delta S,T per X */
	int64_t				dwdx;					/* delta W per X */
	int64_t				dsdy, dtdy;				/* delta S,T per Y */
	int64_t				dwdy;					/* delta W per Y */

	int32_t				lodmin, lodmax;			/* min, max LOD values */
	int32_t				lodbias;				/* LOD bias */
	uint32_t				lodmask;				/* mask of available LODs */
	uint32_t				lodoffset[9];			/* offset of texture base for each LOD */
	int32_t				lodbasetemp;			/* lodbase calculated and used during raster */
	int32_t				detailmax;				/* detail clamp */
	int32_t				detailbias;				/* detail bias */
	uint8_t				detailscale;			/* detail scale */

	uint32_t				wmask;					/* mask for the current texture width */
	uint32_t				hmask;					/* mask for the current texture height */

	uint8_t				bilinear_mask;			/* mask for bilinear resolution (0xf0 for V1, 0xff for V2) */

	ncc_table			ncc[2];					/* two NCC tables */

	const rgb_t *		lookup;					/* currently selected lookup */
	const rgb_t *		texel[16];				/* texel lookups for each format */

	rgb_t				palette[256];			/* palette lookup table */
	rgb_t				palettea[256];			/* palette+alpha lookup table */
};

struct tmu_shared_state
{
	rgb_t				rgb332[256];			/* RGB 3-3-2 lookup table */
	rgb_t				alpha8[256];			/* alpha 8-bit lookup table */
	rgb_t				int8[256];				/* intensity 8-bit lookup table */
	rgb_t				ai44[256];				/* alpha, intensity 4-4 lookup table */

	rgb_t				rgb565[65536];			/* RGB 5-6-5 lookup table */
	rgb_t				argb1555[65536];		/* ARGB 1-5-5-5 lookup table */
	rgb_t				argb4444[65536];		/* ARGB 4-4-4-4 lookup table */
};

struct setup_vertex
{
	float				x, y;					/* X, Y coordinates */
	float				a, r, g, b;				/* A, R, G, B values */
	float				z, wb;					/* Z and broadcast W values */
	float				w0, s0, t0;				/* W, S, T for TMU 0 */
	float				w1, s1, t1;				/* W, S, T for TMU 1 */
};

struct fbi_state
{
	uint8_t*				ram;					/* pointer to aligned frame buffer RAM */
	mem_buffer_t				ram_buffer;				/* Managed buffer backing the RAM */

	uint32_t				mask;					/* mask to apply to pointers */
	uint32_t				rgboffs[3];				/* word offset to 3 RGB buffers */
	uint32_t				auxoffs;				/* word offset to 1 aux buffer */

	uint8_t				frontbuf;				/* front buffer index */
	uint8_t				backbuf;				/* back buffer index */

	uint32_t				yorigin;				/* Y origin subtract value */

	uint32_t				width;					/* width of current frame buffer */
	uint32_t				height;					/* height of current frame buffer */
	//uint32_t				xoffs;					/* horizontal offset (back porch) */
	//uint32_t				yoffs;					/* vertical offset (back porch) */
	//uint32_t				vsyncscan;				/* vertical sync scanline */
	uint32_t				rowpixels;				/* pixels per row */
	uint32_t				tile_width;				/* width of video tiles */
	uint32_t				tile_height;			/* height of video tiles */
	uint32_t				x_tiles;				/* number of tiles in the X direction */

	uint8_t				vblank;					/* VBLANK state */
	bool				vblank_dont_swap;		/* don't actually swap when we hit this point */
	bool				vblank_flush_pending;

	/* triangle setup info */
	int16_t				ax, ay;					/* vertex A x,y (12.4) */
	int16_t				bx, by;					/* vertex B x,y (12.4) */
	int16_t				cx, cy;					/* vertex C x,y (12.4) */
	int32_t				startr, startg, startb, starta; /* starting R,G,B,A (12.12) */
	int32_t				startz;					/* starting Z (20.12) */
	int64_t				startw;					/* starting W (16.32) */
	int32_t				drdx, dgdx, dbdx, dadx;	/* delta R,G,B,A per X */
	int32_t				dzdx;					/* delta Z per X */
	int64_t				dwdx;					/* delta W per X */
	int32_t				drdy, dgdy, dbdy, dady;	/* delta R,G,B,A per Y */
	int32_t				dzdy;					/* delta Z per Y */
	int64_t				dwdy;					/* delta W per Y */

	stats_block			lfb_stats;				/* LFB-access statistics */

	uint8_t				sverts;					/* number of vertices ready */
	setup_vertex		svert[3];				/* 3 setup vertices */

	fifo_state			fifo;					/* framebuffer memory fifo */

	uint8_t				fogblend[64];			/* 64-entry fog table */
	uint8_t				fogdelta[64];			/* 64-entry fog table */
	uint8_t				fogdelta_mask;			/* mask for for delta (0xff for V1, 0xfc for V2) */

	//rgb_t				clut[512];				/* clut gamma data */
};

struct dac_state
{
	uint8_t				reg[8];					/* 8 registers */
	uint8_t				read_result;			/* pending read result */
};

#ifdef C_ENABLE_VOODOO_OPENGL
#ifndef GLhandleARB
#ifdef __APPLE__
typedef void* GLhandleARB;
#else
typedef unsigned int GLhandleARB;
#endif /* __APPLE__ */
#endif /* GLhandleARB */

struct raster_info
{
	raster_info			*next;					/* pointer to next entry with the same hash */
#ifdef C_ENABLE_VOODOO_DEBUG
	bool				is_generic;				/* true if this is one of the generic rasterizers */
	uint8_t				display;				/* display index */
	uint32_t				hits;					/* how many hits (pixels) we've used this for */
	uint32_t				polys;					/* how many polys we've used this for */
#endif
	uint32_t				eff_color_path;			/* effective fbzColorPath value */
	uint32_t				eff_alpha_mode;			/* effective alphaMode value */
	uint32_t				eff_fog_mode;			/* effective fogMode value */
	uint32_t				eff_fbz_mode;			/* effective fbzMode value */
	uint32_t				eff_tex_mode_0;			/* effective textureMode value for TMU #0 */
	uint32_t				eff_tex_mode_1;			/* effective textureMode value for TMU #1 */

	bool				shader_ready;
	GLhandleARB			so_shader_program;
	GLhandleARB			so_vertex_shader;
	GLhandleARB			so_fragment_shader;
	int32_t*				shader_ulocations;
};
#endif

constexpr auto VoodooDefaultRefreshRateHz = 60.0;

struct draw_state {
	double frame_start           = 0.0;
	double frame_period_ms       = 1000.0 / VoodooDefaultRefreshRateHz;
	bool override_on             = false;
	bool screen_update_requested = false;
	bool screen_update_pending   = false;
};

struct triangle_worker
{
	triangle_worker(const int num_threads_)
	        : num_threads(num_threads_),
	          // I measured 4x the thread count to be the sweet spot, after which performance degrades.
	          // This gives about 20% more FPS in Descent II over the old 1x count.
	          num_work_units((num_threads + 1) * 4),
	          threads(num_threads)
	{
		assert(num_work_units > num_threads);
	}

	triangle_worker()                                  = delete;
	triangle_worker(const triangle_worker&)            = delete;
	triangle_worker& operator=(const triangle_worker&) = delete;

	const int num_threads = 0;
	const int num_work_units = 0;

	bool disable_bilinear_filter = {};

	std::atomic_bool threads_active = {};

	uint16_t* drawbuf = {};

	poly_vertex v1 = {};
	poly_vertex v2 = {};
	poly_vertex v3 = {};

	int32_t v1y      = 0;
	int32_t v3y      = 0;
	int32_t totalpix = 0;

	std::vector<std::thread> threads = {};

	// Worker threads start working when this gets reset to 0
	std::atomic<int> work_index = INT_MAX;

	std::atomic<int> done_count = 0;
};

struct voodoo_state
{
	voodoo_state(const int num_threads)
	        : tworker(num_threads),
	          thread_stats(tworker.num_work_units)
	{
		assert(!thread_stats.empty());
	}

	voodoo_state()                               = delete;
	voodoo_state(const voodoo_state&)            = delete;
	voodoo_state& operator=(const voodoo_state&) = delete;

	uint8_t chipmask = {}; /* mask for which chips are available */

	voodoo_reg reg[0x400]    = {}; /* raw registers */
	const uint8_t* regaccess = {}; /* register access array */
	bool alt_regmap          = {}; /* enable alternate register map? */

	pci_state pci = {}; /* PCI state */
	dac_state dac = {}; /* DAC state */

	fbi_state fbi             = {}; /* Frame Buffer Interface (FBI) states */
	tmu_state tmu[MAX_TMU]    = {}; /* Texture Mapper Unit (TMU) states */
	tmu_shared_state tmushare = {}; /* TMU shared state */
	uint32_t tmu_config       = {};

#ifdef C_ENABLE_VOODOO_OPENGL
	uint16_t next_rasterizer = {}; /* next rasterizer index */
	raster_info rasterizer[MAX_RASTERIZERS] = {}; /* array of rasterizers */
	raster_info* raster_hash[RASTER_HASH_SIZE] = {}; /* hash table of
	                                                    rasterizers */
#endif

	bool send_config   = {};
	bool clock_enabled = {};
	bool output_on     = {};
	bool active        = {};
#ifdef C_ENABLE_VOODOO_OPENGL
	bool ogl                 = {};
	bool ogl_dimchange       = {};
	bool ogl_palette_changed = {};
#endif
#ifdef C_ENABLE_VOODOO_DEBUG
	const char *const *	regnames;				/* register names array */
#endif

	draw_state draw = {};
	triangle_worker tworker;
	std::vector<stats_block> thread_stats = {};
};

#ifdef C_ENABLE_VOODOO_OPENGL
struct poly_extra_data
{
	voodoo_state *		state;					/* pointer back to the voodoo state */
	raster_info *		info;					/* pointer to rasterizer information */

	int16_t				ax, ay;					/* vertex A x,y (12.4) */
	int32_t				startr, startg, startb, starta; /* starting R,G,B,A (12.12) */
	int32_t				startz;					/* starting Z (20.12) */
	int64_t				startw;					/* starting W (16.32) */
	int32_t				drdx, dgdx, dbdx, dadx;	/* delta R,G,B,A per X */
	int32_t				dzdx;					/* delta Z per X */
	int64_t				dwdx;					/* delta W per X */
	int32_t				drdy, dgdy, dbdy, dady;	/* delta R,G,B,A per Y */
	int32_t				dzdy;					/* delta Z per Y */
	int64_t				dwdy;					/* delta W per Y */

	int64_t				starts0, startt0;		/* starting S,T (14.18) */
	int64_t				startw0;				/* starting W (2.30) */
	int64_t				ds0dx, dt0dx;			/* delta S,T per X */
	int64_t				dw0dx;					/* delta W per X */
	int64_t				ds0dy, dt0dy;			/* delta S,T per Y */
	int64_t				dw0dy;					/* delta W per Y */
	int32_t				lodbase0;				/* used during rasterization */

	int64_t				starts1, startt1;		/* starting S,T (14.18) */
	int64_t				startw1;				/* starting W (2.30) */
	int64_t				ds1dx, dt1dx;			/* delta S,T per X */
	int64_t				dw1dx;					/* delta W per X */
	int64_t				ds1dy, dt1dy;			/* delta S,T per Y */
	int64_t				dw1dy;					/* delta W per Y */
	int32_t				lodbase1;				/* used during rasterization */

	uint16_t				dither[16];				/* dither matrix, for fastfill */

	Bitu				texcount;
	Bitu				r_fbzColorPath;
	Bitu				r_fbzMode;
	Bitu				r_alphaMode;
	Bitu				r_fogMode;
	int32_t				r_textureMode0;
	int32_t				r_textureMode1;
};
#endif

/*************************************
 *
 *  Computes a fast 16.16 reciprocal
 *  of a 16.32 value; used for
 *  computing 1/w in the rasterizer.
 *
 *  Since it is trivial to also
 *  compute log2(1/w) = -log2(w) at
 *  the same time, we do that as well
 *  to 16.8 precision for LOD
 *  calculations.
 *
 *  On a Pentium M, this routine is
 *  20% faster than a 64-bit integer
 *  divide and also produces the log
 *  for free.
 *
 *************************************/

inline int64_t fast_reciplog(int64_t value, int32_t* log_2)
{
	uint32_t temp;
	uint32_t rlog;
	uint32_t interp;
	uint32_t *table;
	uint64_t recip;
	bool neg = false;
	int lz       = 0;
	int exponent = 0;

	/* always work with unsigned numbers */
	if (value < 0)
	{
		value = -value;
		neg = true;
	}

	/* if we've spilled out of 32 bits, push it down under 32 */
	if ((value & LONGTYPE(0xffff00000000)) != 0)
	{
		temp = (uint32_t)(value >> 16);
		exponent -= 16;
	} else {
		temp = (uint32_t)value;
	}

	/* if the resulting value is 0, the reciprocal is infinite */
	if (temp == 0) {
		*log_2 = 1000 << LOG_OUTPUT_PREC;
		return neg ? 0x80000000 : 0x7fffffff;
	}

	/* determine how many leading zeros in the value and shift it up high */
	lz = std::countl_zero(temp);
	temp <<= lz;
	exponent += lz;

	/* compute a pointer to the table entries we want */
	/* math is a bit funny here because we shift one less than we need to in order */
	/* to account for the fact that there are two uint32_t's per table entry */
	table = &voodoo_reciplog[(temp >> (31 - RECIPLOG_LOOKUP_BITS - 1)) & ((2 << RECIPLOG_LOOKUP_BITS) - 2)];

	/* compute the interpolation value */
	interp = (temp >> (31 - RECIPLOG_LOOKUP_BITS - 8)) & 0xff;

	/* do a linear interpolatation between the two nearest table values */
	/* for both the log and the reciprocal */
	rlog = (table[1] * (0x100 - interp) + table[3] * interp) >> 8;
	recip = (table[0] * (0x100 - interp) + table[2] * interp) >> 8;

	/* the log result is the fractional part of the log; round it to the output precision */
	rlog = (rlog + (1 << (RECIPLOG_LOOKUP_PREC - LOG_OUTPUT_PREC - 1))) >> (RECIPLOG_LOOKUP_PREC - LOG_OUTPUT_PREC);

	/* the exponent is the non-fractional part of the log; normally, we would subtract it from rlog */
	/* but since we want the log(1/value) = -log(value), we subtract rlog from the exponent */
	*log_2 = left_shift_signed(exponent - (31 - RECIPLOG_INPUT_PREC), LOG_OUTPUT_PREC) - rlog;

	/* adjust the exponent to account for all the reciprocal-related parameters to arrive at a final shift amount */
	exponent += (RECIP_OUTPUT_PREC - RECIPLOG_LOOKUP_PREC) -
	            (31 - RECIPLOG_INPUT_PREC);

	/* shift by the exponent */
	if (exponent < 0) {
		recip >>= -exponent;
	} else {
		recip <<= exponent;
	}

	/* on the way out, apply the original sign to the reciprocal */
	return neg ? -(int64_t)recip : (int64_t)recip;
}



/*************************************
 *
 *  Float-to-int conversions
 *
 *************************************/

inline int32_t float_to_int32(const uint32_t data, const int fixedbits)
{
	// Clamp the exponent to the type's shift limit
	constexpr auto max_shift = std::numeric_limits<int32_t>::digits;
	int exponent = ((data >> 23) & 0xff) - 127 - 23 + fixedbits;
	exponent = std::clamp(exponent, -max_shift, max_shift);

	int32_t result = (data & 0x7fffff) | 0x800000;

	if (exponent < 0) {
		if (exponent > -max_shift) {
			result >>= -exponent;
		} else {
			result = 0;
	}
	} else {
		result = clamp_to_int32(static_cast<int64_t>(result) << exponent);
	}
	if ((data & 0x80000000) != 0u) {
		result = -result;
	}
	return result;
}

inline int64_t float_to_int64(uint32_t data, int fixedbits)
{
	const int exponent = ((data >> 23) & 0xff) - 127 - 23 + fixedbits;
	int64_t result = (data & 0x7fffff) | 0x800000;
	if (exponent < 0)
	{
		if (exponent > -64) {
			result >>= -exponent;
		} else {
			result = 0;
		}
	}
	else
	{
		if (exponent < 64) {
			result <<= exponent;
		} else {
			result = LONGTYPE(0x7fffffffffffffff);
		}
	}
	if ((data & 0x80000000) != 0u) {
		result = -result;
	}
	return result;
}



#ifdef C_ENABLE_VOODOO_OPENGL
/*************************************
 *
 *  Rasterizer inlines
 *
 *************************************/

inline uint32_t normalize_color_path(uint32_t eff_color_path)
{
	/* ignore the subpixel adjust and texture enable flags */
	eff_color_path &= ~((1 << 26) | (1 << 27));

	return eff_color_path;
}

inline uint32_t normalize_alpha_mode(uint32_t eff_alpha_mode)
{
	/* always ignore alpha ref value */
	eff_alpha_mode &= ~(0xff << 24);

	/* if not doing alpha testing, ignore the alpha function and ref value */
	if (!ALPHAMODE_ALPHATEST(eff_alpha_mode))
		eff_alpha_mode &= ~(7 << 1);

	/* if not doing alpha blending, ignore the source and dest blending factors */
	if (!ALPHAMODE_ALPHABLEND(eff_alpha_mode))
		eff_alpha_mode &= ~((15 << 8) | (15 << 12) | (15 << 16) | (15 << 20));

	return eff_alpha_mode;
}

inline uint32_t normalize_fog_mode(uint32_t eff_fog_mode)
{
	/* if not doing fogging, ignore all the other fog bits */
	if (!FOGMODE_ENABLE_FOG(eff_fog_mode))
		eff_fog_mode = 0;

	return eff_fog_mode;
}

inline uint32_t normalize_fbz_mode(uint32_t eff_fbz_mode)
{
	/* ignore the draw buffer */
	eff_fbz_mode &= ~(3 << 14);

	return eff_fbz_mode;
}

inline uint32_t normalize_tex_mode(uint32_t eff_tex_mode)
{
	/* ignore the NCC table and seq_8_downld flags */
	eff_tex_mode &= ~((1 << 5) | (1 << 31));

	/* classify texture formats into 3 format categories */
	if (TEXMODE_FORMAT(eff_tex_mode) < 8)
		eff_tex_mode = (eff_tex_mode & ~(0xf << 8)) | (0 << 8);
	else if (TEXMODE_FORMAT(eff_tex_mode) >= 10 && TEXMODE_FORMAT(eff_tex_mode) <= 12)
		eff_tex_mode = (eff_tex_mode & ~(0xf << 8)) | (10 << 8);
	else
		eff_tex_mode = (eff_tex_mode & ~(0xf << 8)) | (8 << 8);

	return eff_tex_mode;
}

inline uint32_t compute_raster_hash(const raster_info* info)
{
	uint32_t hash;

	/* make a hash */
	hash = info->eff_color_path;
	hash = (hash << 1) | (hash >> 31);
	hash ^= info->eff_fbz_mode;
	hash = (hash << 1) | (hash >> 31);
	hash ^= info->eff_alpha_mode;
	hash = (hash << 1) | (hash >> 31);
	hash ^= info->eff_fog_mode;
	hash = (hash << 1) | (hash >> 31);
	hash ^= info->eff_tex_mode_0;
	hash = (hash << 1) | (hash >> 31);
	hash ^= info->eff_tex_mode_1;

	return hash % RASTER_HASH_SIZE;
}
#endif


/*************************************
 *
 *  Dithering macros
 *
 *************************************/

/* note that these equations and the dither matrixes have
   been confirmed to be exact matches to the real hardware */

constexpr uint8_t dither_rb(const int colour, const int amount)
{
	const auto dithered = (colour << 1) - (colour >> 4) + (colour >> 7) + amount;
	return check_cast<uint8_t>(dithered >> 4);
}

constexpr uint8_t dither_g(const int colour, const int amount)
{
	const auto dithered = (colour << 2) - (colour >> 4) + (colour >> 6) + amount;
	return check_cast<uint8_t>(dithered >> 4);
}

using dither_lut_t = std::array<uint8_t, 256 * 16 * 2>;

constexpr dither_lut_t generate_dither_lut(const uint8_t dither_amounts[])
{
	// To be populated
	dither_lut_t dither_lut = {};

	for (uint16_t i = 0; i < dither_lut.size(); ++i) {
		const auto x = (i >> 1) & 3;
		const auto y = (i >> 11) & 3;

		const auto color = static_cast<uint8_t>((i >> 3) & 0xff);
		const auto amount = dither_amounts[y * 4 + x];
		const auto use_rb = (i & 1) == 0;

		dither_lut[i] = use_rb ? dither_rb(color, amount)
		                       : dither_g(color, amount);
	}
	return dither_lut;
}

#define COMPUTE_DITHER_POINTERS(FBZMODE, YY)									\
do																				\
{																				\
	/* compute the dithering pointers */										\
	if (FBZMODE_ENABLE_DITHERING(FBZMODE))										\
	{																			\
		dither4 = &dither_matrix_4x4[((YY) & 3) * 4];							\
		if (FBZMODE_DITHER_TYPE(FBZMODE) == 0)									\
		{																		\
			dither = dither4;													\
			dither_lookup = &dither4_lookup[((YY) & 3) << 11];					\
		}																		\
		else																	\
		{																		\
			dither = &dither_matrix_2x2[((YY) & 3) * 4];						\
			dither_lookup = &dither2_lookup[((YY) & 3) << 11];					\
		}																		\
	}																			\
}																				\
while (0)

#define APPLY_DITHER(FBZMODE, XX, DITHER_LOOKUP, RR, GG, BB)					\
do																				\
{																				\
	/* apply dithering */														\
	if (FBZMODE_ENABLE_DITHERING(FBZMODE))										\
	{																			\
		/* look up the dither value from the appropriate matrix */				\
		const uint8_t *dith = &(DITHER_LOOKUP)[((XX) & 3) << 1];					\
																				\
		/* apply dithering to R,G,B */											\
		(RR) = dith[((RR) << 3) + 0];											\
		(GG) = dith[((GG) << 3) + 1];											\
		(BB) = dith[((BB) << 3) + 0];											\
	}																			\
	else																		\
	{																			\
		(RR) >>= 3;																\
		(GG) >>= 2;																\
		(BB) >>= 3;																\
	}																			\
}																				\
while (0)



/*************************************
 *
 *  Clamping macros
 *
 *************************************/

#define CLAMPED_ARGB(ITERR, ITERG, ITERB, ITERA, FBZCP, RESULT) \
	do { \
		int32_t red   = (ITERR) >> 12; \
		int32_t green = (ITERG) >> 12; \
		int32_t blue  = (ITERB) >> 12; \
		int32_t alpha = (ITERA) >> 12; \
\
		if (FBZCP_RGBZW_CLAMP(FBZCP) == 0) { \
			red &= 0xfff; \
			(RESULT).rgb.r = static_cast<uint8_t>(red); \
			if (red == 0xfff) \
				(RESULT).rgb.r = 0; \
			else if (red == 0x100) \
				(RESULT).rgb.r = 0xff; \
\
			green &= 0xfff; \
			(RESULT).rgb.g = static_cast<uint8_t>(green); \
			if (green == 0xfff) \
				(RESULT).rgb.g = 0; \
			else if (green == 0x100) \
				(RESULT).rgb.g = 0xff; \
\
			blue &= 0xfff; \
			(RESULT).rgb.b = static_cast<uint8_t>(blue); \
			if (blue == 0xfff) \
				(RESULT).rgb.b = 0; \
			else if (blue == 0x100) \
				(RESULT).rgb.b = 0xff; \
\
			alpha &= 0xfff; \
			(RESULT).rgb.a = static_cast<uint8_t>(alpha); \
			if (alpha == 0xfff) \
				(RESULT).rgb.a = 0; \
			else if (alpha == 0x100) \
				(RESULT).rgb.a = 0xff; \
		} else { \
			(RESULT).rgb.r = (red < 0)    ? 0 \
			             : (red > 0xff) ? 0xff \
			                            : static_cast<uint8_t>(red); \
			(RESULT).rgb.g = (green < 0) ? 0 \
			             : (green > 0xff) \
			                     ? 0xff \
			                     : static_cast<uint8_t>(green); \
			(RESULT).rgb.b = (blue < 0) ? 0 \
			             : (blue > 0xff) \
			                     ? 0xff \
			                     : static_cast<uint8_t>(blue); \
			(RESULT).rgb.a = (alpha < 0) ? 0 \
			             : (alpha > 0xff) \
			                     ? 0xff \
			                     : static_cast<uint8_t>(alpha); \
		} \
	} while (0)

#define CLAMPED_Z(ITERZ, FBZCP, RESULT)											\
do																				\
{																				\
	(RESULT) = (ITERZ) >> 12;													\
	if (FBZCP_RGBZW_CLAMP(FBZCP) == 0)											\
	{																			\
		(RESULT) &= 0xfffff;													\
		if ((RESULT) == 0xfffff)												\
			(RESULT) = 0;														\
		else if ((RESULT) == 0x10000)											\
			(RESULT) = 0xffff;													\
		else																	\
			(RESULT) &= 0xffff;													\
	}																			\
	else																		\
	{																			\
		(RESULT) = clamp_to_uint16(RESULT);										\
	}																			\
}																				\
while (0)


#define CLAMPED_W(ITERW, FBZCP, RESULT)											\
do																				\
{																				\
	(RESULT) = (int16_t)((ITERW) >> 32);											\
	if (FBZCP_RGBZW_CLAMP(FBZCP) == 0)											\
	{																			\
		(RESULT) &= 0xffff;														\
		if ((RESULT) == 0xffff)													\
			(RESULT) = 0;														\
		else if ((RESULT) == 0x100)												\
			(RESULT) = 0xff;													\
		(RESULT) &= 0xff;														\
	}																			\
	else																		\
	{																			\
		(RESULT) = clamp_to_uint8(RESULT);										\
	}																			\
}																				\
while (0)



/*************************************
 *
 *  Chroma keying macro
 *
 *************************************/
#define ADD_STAT_COUNT(STATS, STATNAME) (STATS).STATNAME++;
//#define ADD_STAT_COUNT(STATS, STATNAME)

#define APPLY_CHROMAKEY(VV, STATS, FBZMODE, COLOR)								\
do																				\
{																				\
	if (FBZMODE_ENABLE_CHROMAKEY(FBZMODE))										\
	{																			\
		/* non-range version */													\
		if (!CHROMARANGE_ENABLE((VV)->reg[chromaRange].u))						\
		{																		\
			if ((((COLOR).u ^ (VV)->reg[chromaKey].u) & 0xffffff) == 0)			\
			{																	\
				ADD_STAT_COUNT(STATS, chroma_fail)								\
				goto skipdrawdepth;												\
			}																	\
		}																		\
																				\
		/* tricky range version */												\
		else																	\
		{																		\
			int32_t low, high, test;												\
			int results = 0;													\
																				\
			/* check blue */													\
			low = (VV)->reg[chromaKey].rgb.b;									\
			high = (VV)->reg[chromaRange].rgb.b;								\
			test = (COLOR).rgb.b;													\
			results = (test >= low && test <= high);							\
			results ^= CHROMARANGE_BLUE_EXCLUSIVE((VV)->reg[chromaRange].u);	\
			results <<= 1;														\
																				\
			/* check green */													\
			low = (VV)->reg[chromaKey].rgb.g;									\
			high = (VV)->reg[chromaRange].rgb.g;								\
			test = (COLOR).rgb.g;													\
			results |= (test >= low && test <= high);							\
			results ^= CHROMARANGE_GREEN_EXCLUSIVE((VV)->reg[chromaRange].u);	\
			results <<= 1;														\
																				\
			/* check red */														\
			low = (VV)->reg[chromaKey].rgb.r;									\
			high = (VV)->reg[chromaRange].rgb.r;								\
			test = (COLOR).rgb.r;													\
			results |= (test >= low && test <= high);							\
			results ^= CHROMARANGE_RED_EXCLUSIVE((VV)->reg[chromaRange].u);		\
																				\
			/* final result */													\
			if (CHROMARANGE_UNION_MODE((VV)->reg[chromaRange].u))				\
			{																	\
				if (results != 0)												\
				{																\
					ADD_STAT_COUNT(STATS, chroma_fail)							\
					goto skipdrawdepth;											\
				}																\
			}																	\
			else																\
			{																	\
				if (results == 7)												\
				{																\
					ADD_STAT_COUNT(STATS, chroma_fail)							\
					goto skipdrawdepth;											\
				}																\
			}																	\
		}																		\
	}																			\
}																				\
while (0)



/*************************************
 *
 *  Alpha masking macro
 *
 *************************************/

#define APPLY_ALPHAMASK(VV, STATS, FBZMODE, AA)									\
do																				\
{																				\
	if (FBZMODE_ENABLE_ALPHA_MASK(FBZMODE))										\
	{																			\
		if (((AA) & 1) == 0)													\
		{																		\
			ADD_STAT_COUNT(STATS, afunc_fail)									\
			goto skipdrawdepth;													\
		}																		\
	}																			\
}																				\
while (0)



/*************************************
 *
 *  Alpha testing macro
 *
 *************************************/

#define APPLY_ALPHATEST(VV, STATS, ALPHAMODE, AA)								\
do																				\
{																				\
	if (ALPHAMODE_ALPHATEST(ALPHAMODE))											\
	{																			\
		uint8_t alpharef = (VV)->reg[alphaMode].rgb.a;							\
		switch (ALPHAMODE_ALPHAFUNCTION(ALPHAMODE))								\
		{																		\
			case 0:		/* alphaOP = never */									\
				ADD_STAT_COUNT(STATS, afunc_fail)								\
				goto skipdrawdepth;												\
																				\
			case 1:		/* alphaOP = less than */								\
				if ((AA) >= alpharef)											\
				{																\
					ADD_STAT_COUNT(STATS, afunc_fail)							\
					goto skipdrawdepth;											\
				}																\
				break;															\
																				\
			case 2:		/* alphaOP = equal */									\
				if ((AA) != alpharef)											\
				{																\
					ADD_STAT_COUNT(STATS, afunc_fail)							\
					goto skipdrawdepth;											\
				}																\
				break;															\
																				\
			case 3:		/* alphaOP = less than or equal */						\
				if ((AA) > alpharef)											\
				{																\
					ADD_STAT_COUNT(STATS, afunc_fail)							\
					goto skipdrawdepth;											\
				}																\
				break;															\
																				\
			case 4:		/* alphaOP = greater than */							\
				if ((AA) <= alpharef)											\
				{																\
					ADD_STAT_COUNT(STATS, afunc_fail)							\
					goto skipdrawdepth;											\
				}																\
				break;															\
																				\
			case 5:		/* alphaOP = not equal */								\
				if ((AA) == alpharef)											\
				{																\
					ADD_STAT_COUNT(STATS, afunc_fail)							\
					goto skipdrawdepth;											\
				}																\
				break;															\
																				\
			case 6:		/* alphaOP = greater than or equal */					\
				if ((AA) < alpharef)											\
				{																\
					ADD_STAT_COUNT(STATS, afunc_fail)							\
					goto skipdrawdepth;											\
				}																\
				break;															\
																				\
			case 7:		/* alphaOP = always */									\
				break;															\
		}																		\
	}																			\
}																				\
while (0)



/*************************************
 *
 *  Alpha blending macro
 *
 *************************************/

#define APPLY_ALPHA_BLEND(FBZMODE, ALPHAMODE, XX, DITHER, RR, GG, BB, AA) \
	do { \
		if (ALPHAMODE_ALPHABLEND(ALPHAMODE)) { \
			int dpix = dest[XX]; \
			int dr   = (dpix >> 8) & 0xf8; \
			int dg   = (dpix >> 3) & 0xfc; \
			int db   = (dpix << 3) & 0xf8; \
			int da = (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) && depth) \
			               ? depth[XX] \
			               : 0xff; \
			const int sr_val = (RR); \
			const int sg_val = (GG); \
			const int sb_val = (BB); \
			const int sa_val = (AA); \
\
			int ta     = 0; \
\
			/* apply dither subtraction */ \
			if ((FBZMODE_ALPHA_DITHER_SUBTRACT(FBZMODE)) && (DITHER)) { \
				/* look up the dither value from the \
				 * appropriate matrix */ \
				int dith = (DITHER)[(XX) & 3]; \
\
				/* subtract the dither value */ \
				dr = ((dr << 1) + 15 - dith) >> 1; \
				dg = ((dg << 2) + 15 - dith) >> 2; \
				db = ((db << 1) + 15 - dith) >> 1; \
			} \
\
			/* compute source portion */ \
			switch (ALPHAMODE_SRCRGBBLEND(ALPHAMODE)) { \
			default: /* reserved */ \
			case 0: /* AZERO */ (RR) = (GG) = (BB) = 0; break; \
\
			case 1: /* ASRC_ALPHA */ \
				(RR) = (sr_val * (sa_val + 1)) >> 8; \
				(GG) = (sg_val * (sa_val + 1)) >> 8; \
				(BB) = (sb_val * (sa_val + 1)) >> 8; \
				break; \
\
			case 2: /* A_COLOR */ \
				(RR) = (sr_val * (dr + 1)) >> 8; \
				(GG) = (sg_val * (dg + 1)) >> 8; \
				(BB) = (sb_val * (db + 1)) >> 8; \
				break; \
\
			case 3: /* ADST_ALPHA */ \
				(RR) = (sr_val * (da + 1)) >> 8; \
				(GG) = (sg_val * (da + 1)) >> 8; \
				(BB) = (sb_val * (da + 1)) >> 8; \
				break; \
\
			case 4: /* AONE */ break; \
\
			case 5: /* AOMSRC_ALPHA */ \
				(RR) = (sr_val * (0x100 - sa_val)) >> 8; \
				(GG) = (sg_val * (0x100 - sa_val)) >> 8; \
				(BB) = (sb_val * (0x100 - sa_val)) >> 8; \
				break; \
\
			case 6: /* AOM_COLOR */ \
				(RR) = (sr_val * (0x100 - dr)) >> 8; \
				(GG) = (sg_val * (0x100 - dg)) >> 8; \
				(BB) = (sb_val * (0x100 - db)) >> 8; \
				break; \
\
			case 7: /* AOMDST_ALPHA */ \
				(RR) = (sr_val * (0x100 - da)) >> 8; \
				(GG) = (sg_val * (0x100 - da)) >> 8; \
				(BB) = (sb_val * (0x100 - da)) >> 8; \
				break; \
\
			case 15: /* ASATURATE */ \
				ta   = (sa_val < (0x100 - da)) ? sa_val \
				                               : (0x100 - da); \
				(RR) = (sr_val * (ta + 1)) >> 8; \
				(GG) = (sg_val * (ta + 1)) >> 8; \
				(BB) = (sb_val * (ta + 1)) >> 8; \
				break; \
			} \
\
			/* add in dest portion */ \
			switch (ALPHAMODE_DSTRGBBLEND(ALPHAMODE)) { \
			default: /* reserved */ \
			case 0: /* AZERO */ break; \
\
			case 1: /* ASRC_ALPHA */ \
				(RR) += (dr * (sa_val + 1)) >> 8; \
				(GG) += (dg * (sa_val + 1)) >> 8; \
				(BB) += (db * (sa_val + 1)) >> 8; \
				break; \
\
			case 2: /* A_COLOR */ \
				(RR) += (dr * (sr_val + 1)) >> 8; \
				(GG) += (dg * (sg_val + 1)) >> 8; \
				(BB) += (db * (sb_val + 1)) >> 8; \
				break; \
\
			case 3: /* ADST_ALPHA */ \
				(RR) += (dr * (da + 1)) >> 8; \
				(GG) += (dg * (da + 1)) >> 8; \
				(BB) += (db * (da + 1)) >> 8; \
				break; \
\
			case 4: /* AONE */ \
				(RR) += dr; \
				(GG) += dg; \
				(BB) += db; \
				break; \
\
			case 5: /* AOMSRC_ALPHA */ \
				(RR) += (dr * (0x100 - sa_val)) >> 8; \
				(GG) += (dg * (0x100 - sa_val)) >> 8; \
				(BB) += (db * (0x100 - sa_val)) >> 8; \
				break; \
\
			case 6: /* AOM_COLOR */ \
				(RR) += (dr * (0x100 - sr_val)) >> 8; \
				(GG) += (dg * (0x100 - sg_val)) >> 8; \
				(BB) += (db * (0x100 - sb_val)) >> 8; \
				break; \
\
			case 7: /* AOMDST_ALPHA */ \
				(RR) += (dr * (0x100 - da)) >> 8; \
				(GG) += (dg * (0x100 - da)) >> 8; \
				(BB) += (db * (0x100 - da)) >> 8; \
				break; \
\
			case 15: /* A_COLORBEFOREFOG */ \
				(RR) += (dr * (prefogr + 1)) >> 8; \
				(GG) += (dg * (prefogg + 1)) >> 8; \
				(BB) += (db * (prefogb + 1)) >> 8; \
				break; \
			} \
\
			/* blend the source alpha */ \
			(AA) = 0; \
			if (ALPHAMODE_SRCALPHABLEND(ALPHAMODE) == 4) \
				(AA) = sa_val; \
\
			/* blend the dest alpha */ \
			if (ALPHAMODE_DSTALPHABLEND(ALPHAMODE) == 4) \
				(AA) += da; \
\
			/* clamp */ \
			(RR) = clamp_to_uint8(RR); \
			(GG) = clamp_to_uint8(GG); \
			(BB) = clamp_to_uint8(BB); \
			(AA) = clamp_to_uint8(AA); \
		} \
	} while (0)

/*************************************
 *
 *  Fogging macro
 *
 *************************************/

#define APPLY_FOGGING(VV, FOGMODE, FBZCP, XX, DITHER4, RR, GG, BB, ITERZ, ITERW, ITERAXXX)	\
do																				\
{																				\
	if (FOGMODE_ENABLE_FOG(FOGMODE))											\
	{																			\
		rgb_union fogcolor = (VV)->reg[fogColor];								\
		int32_t fr, fg, fb;														\
																				\
		/* constant fog bypasses everything else */								\
		if (FOGMODE_FOG_CONSTANT(FOGMODE))										\
		{																		\
			fr = fogcolor.rgb.r;												\
			fg = fogcolor.rgb.g;												\
			fb = fogcolor.rgb.b;												\
		}																		\
																				\
		/* non-constant fog comes from several sources */						\
		else																	\
		{																		\
			int32_t fogblend = 0;													\
																				\
			/* if fog_add is zero, we start with the fog color */				\
			if (FOGMODE_FOG_ADD(FOGMODE) == 0)									\
			{																	\
				fr = fogcolor.rgb.r;											\
				fg = fogcolor.rgb.g;											\
				fb = fogcolor.rgb.b;											\
			}																	\
			else																\
				fr = fg = fb = 0;												\
																				\
			/* if fog_mult is zero, we subtract the incoming color */			\
			if (FOGMODE_FOG_MULT(FOGMODE) == 0)									\
			{																	\
				fr -= (RR);														\
				fg -= (GG);														\
				fb -= (BB);														\
			}																	\
																				\
			/* fog blending mode */												\
			switch (FOGMODE_FOG_ZALPHA(FOGMODE))								\
			{																	\
				case 0:		/* fog table */										\
				{																\
					int32_t delta = (VV)->fbi.fogdelta[wfloat >> 10];				\
					int32_t deltaval;												\
																				\
					/* perform the multiply against lower 8 bits of wfloat */	\
					deltaval = (delta & (VV)->fbi.fogdelta_mask) *				\
								((wfloat >> 2) & 0xff);							\
																				\
					/* fog zones allow for negating this value */				\
					if (FOGMODE_FOG_ZONES(FOGMODE) && (delta & 2))				\
						deltaval = -deltaval;									\
					deltaval >>= 6;												\
																				\
					/* apply dither */											\
					if (FOGMODE_FOG_DITHER(FOGMODE))							\
						if (DITHER4)											\
							deltaval += (DITHER4)[(XX) & 3];						\
					deltaval >>= 4;												\
																				\
					/* add to the blending factor */							\
					fogblend = (VV)->fbi.fogblend[wfloat >> 10] + deltaval;		\
					break;														\
				}																\
																				\
				case 1:		/* iterated A */									\
					fogblend = (ITERAXXX).rgb.a;									\
					break;														\
																				\
				case 2:		/* iterated Z */									\
					CLAMPED_Z((ITERZ), FBZCP, fogblend);						\
					fogblend >>= 8;												\
					break;														\
																				\
				case 3:		/* iterated W - Voodoo 2 only */					\
					CLAMPED_W((ITERW), FBZCP, fogblend);						\
					break;														\
			}																	\
																				\
			/* perform the blend */												\
			fogblend++;															\
			fr = (fr * fogblend) >> 8;											\
			fg = (fg * fogblend) >> 8;											\
			fb = (fb * fogblend) >> 8;											\
		}																		\
																				\
		/* if fog_mult is 0, we add this to the original color */				\
		if (FOGMODE_FOG_MULT(FOGMODE) == 0)										\
		{																		\
			(RR) += fr;															\
			(GG) += fg;															\
			(BB) += fb;															\
		}																		\
																				\
		/* otherwise this just becomes the new color */							\
		else																	\
		{																		\
			(RR) = fr;															\
			(GG) = fg;															\
			(BB) = fb;															\
		}																		\
																				\
		/* clamp */																\
		(RR) = clamp_to_uint8(RR);												\
		(GG) = clamp_to_uint8(GG);												\
		(BB) = clamp_to_uint8(BB);												\
	}																			\
}																				\
while (0)



/*************************************
 *
 *  Texture pipeline macro
 *
 *************************************/

#define TEXTURE_PIPELINE(TT, XX, DITHER4, TEXMODE, COTHER, LOOKUP, LODBASE, ITERS, ITERT, ITERW, RESULT) \
do																				\
{																				\
	int32_t blendr, blendg, blendb, blenda;										\
	int32_t tr, tg, tb, ta;														\
	int32_t s, t, lod, ilod;														\
	int64_t oow;																	\
	int32_t smax, tmax;															\
	uint32_t texbase;																\
	rgb_union c_local;															\
																				\
	/* determine the S/T/LOD values for this texture */							\
	if (TEXMODE_ENABLE_PERSPECTIVE(TEXMODE))									\
	{																			\
		oow = fast_reciplog((ITERW), &lod);										\
		s = (int32_t)((oow * (ITERS)) >> 29);										\
		t = (int32_t)((oow * (ITERT)) >> 29);										\
		lod += (LODBASE);														\
	}																			\
	else																		\
	{																			\
		s = (int32_t)((ITERS) >> 14);												\
		t = (int32_t)((ITERT) >> 14);												\
		lod = (LODBASE);														\
	}																			\
																				\
	/* clamp W */																\
	if (TEXMODE_CLAMP_NEG_W(TEXMODE) && (ITERW) < 0)							\
		s = t = 0;																\
																				\
	/* clamp the LOD */															\
	lod += (TT)->lodbias;														\
	if (TEXMODE_ENABLE_LOD_DITHER(TEXMODE))										\
		if (DITHER4)															\
			lod += (DITHER4)[(XX) & 3] << 4;										\
	if (lod < (TT)->lodmin)														\
		lod = (TT)->lodmin;														\
	if (lod > (TT)->lodmax)														\
		lod = (TT)->lodmax;														\
																				\
	/* now the LOD is in range; if we don't own this LOD, take the next one */	\
	ilod = lod >> 8;															\
	if (!(((TT)->lodmask >> ilod) & 1))											\
		ilod++;																	\
																				\
	/* fetch the texture base */												\
	texbase = (TT)->lodoffset[ilod];											\
																				\
	/* compute the maximum s and t values at this LOD */						\
	smax = (TT)->wmask >> ilod;													\
	tmax = (TT)->hmask >> ilod;													\
																				\
	/* determine whether we are point-sampled or bilinear */					\
	if ((lod == (TT)->lodmin && !TEXMODE_MAGNIFICATION_FILTER(TEXMODE)) ||		\
		(lod != (TT)->lodmin && !TEXMODE_MINIFICATION_FILTER(TEXMODE)))			\
	{																			\
		/* point sampled */														\
																				\
		uint32_t texel0;															\
																				\
		/* adjust S/T for the LOD and strip off the fractions */				\
		s >>= ilod + 18;														\
		t >>= ilod + 18;														\
																				\
		/* clamp/wrap S/T if necessary */										\
		if (TEXMODE_CLAMP_S(TEXMODE))											\
			s = std::clamp(s, 0, smax);											\
		if (TEXMODE_CLAMP_T(TEXMODE))											\
			t = std::clamp(t, 0, tmax);											\
		s &= smax;																\
		t &= tmax;																\
		t *= smax + 1;															\
																				\
		/* fetch texel data */													\
		if (TEXMODE_FORMAT(TEXMODE) < 8)										\
		{																		\
			texel0 = (TT)->ram[(texbase + t + s) & (TT)->mask];					\
			c_local.u = (LOOKUP)[texel0];										\
		}																		\
		else																	\
		{																		\
			texel0 = *(uint16_t *)&(TT)->ram[(texbase + 2*(t + s)) & (TT)->mask];	\
			if (TEXMODE_FORMAT(TEXMODE) >= 10 && TEXMODE_FORMAT(TEXMODE) <= 12)	\
				c_local.u = (LOOKUP)[texel0];									\
			else																\
				c_local.u = ((LOOKUP)[texel0 & 0xff] & 0xffffff) |				\
							((texel0 & 0xff00) << 16);							\
		}																		\
	}																			\
	else																		\
	{																			\
		/* bilinear filtered */													\
																				\
		uint32_t texel0, texel1, texel2, texel3;									\
		uint8_t sfrac, tfrac;														\
		int32_t s1, t1;															\
																				\
		/* adjust S/T for the LOD and strip off all but the low 8 bits of */	\
		/* the fraction */														\
		s >>= ilod + 10;														\
		t >>= ilod + 10;														\
																				\
		/* also subtract 1/2 texel so that (0.5,0.5) = a full (0,0) texel */	\
		s -= 0x80;																\
		t -= 0x80;																\
																				\
		/* extract the fractions */												\
		sfrac = (uint8_t)(s & (TT)->bilinear_mask);								\
		tfrac = (uint8_t)(t & (TT)->bilinear_mask);								\
																				\
		/* now toss the rest */													\
		s >>= 8;																\
		t >>= 8;																\
		s1 = s + 1;																\
		t1 = t + 1;																\
																				\
		/* clamp/wrap S/T if necessary */										\
		if (TEXMODE_CLAMP_S(TEXMODE))											\
		{																		\
			s = std::clamp(s, 0, smax);											\
			s1 = std::clamp(s1, 0, smax);										\
		}																		\
		if (TEXMODE_CLAMP_T(TEXMODE))											\
		{																		\
			t = std::clamp(t, 0, tmax);											\
			t1 = std::clamp(t1, 0, tmax);										\
		}																		\
		s &= smax;																\
		s1 &= smax;																\
		t &= tmax;																\
		t1 &= tmax;																\
		t *= smax + 1;															\
		t1 *= smax + 1;															\
																				\
		/* fetch texel data */													\
		if (TEXMODE_FORMAT(TEXMODE) < 8)										\
		{																		\
			texel0 = (TT)->ram[(texbase + t + s) & (TT)->mask];					\
			texel1 = (TT)->ram[(texbase + t + s1) & (TT)->mask];				\
			texel2 = (TT)->ram[(texbase + t1 + s) & (TT)->mask];				\
			texel3 = (TT)->ram[(texbase + t1 + s1) & (TT)->mask];				\
			texel0 = (LOOKUP)[texel0];											\
			texel1 = (LOOKUP)[texel1];											\
			texel2 = (LOOKUP)[texel2];											\
			texel3 = (LOOKUP)[texel3];											\
		}																		\
		else																	\
		{																		\
			texel0 = *(uint16_t *)&(TT)->ram[(texbase + 2*(t + s)) & (TT)->mask];	\
			texel1 = *(uint16_t *)&(TT)->ram[(texbase + 2*(t + s1)) & (TT)->mask];\
			texel2 = *(uint16_t *)&(TT)->ram[(texbase + 2*(t1 + s)) & (TT)->mask];\
			texel3 = *(uint16_t *)&(TT)->ram[(texbase + 2*(t1 + s1)) & (TT)->mask];\
			if (TEXMODE_FORMAT(TEXMODE) >= 10 && TEXMODE_FORMAT(TEXMODE) <= 12)	\
			{																	\
				texel0 = (LOOKUP)[texel0];										\
				texel1 = (LOOKUP)[texel1];										\
				texel2 = (LOOKUP)[texel2];										\
				texel3 = (LOOKUP)[texel3];										\
			}																	\
			else																\
			{																	\
				texel0 = ((LOOKUP)[texel0 & 0xff] & 0xffffff) | 				\
							((texel0 & 0xff00) << 16);							\
				texel1 = ((LOOKUP)[texel1 & 0xff] & 0xffffff) | 				\
							((texel1 & 0xff00) << 16);							\
				texel2 = ((LOOKUP)[texel2 & 0xff] & 0xffffff) | 				\
							((texel2 & 0xff00) << 16);							\
				texel3 = ((LOOKUP)[texel3 & 0xff] & 0xffffff) | 				\
							((texel3 & 0xff00) << 16);							\
			}																	\
		}																		\
																				\
		/* weigh in each texel */												\
		c_local.u = rgba_bilinear_filter(texel0, texel1, texel2, texel3, sfrac, tfrac);\
	}																			\
																				\
	/* select zero/other for RGB */												\
	if (!TEXMODE_TC_ZERO_OTHER(TEXMODE))										\
	{																			\
		tr = (COTHER).rgb.r;														\
		tg = (COTHER).rgb.g;														\
		tb = (COTHER).rgb.b;														\
	}																			\
	else																		\
		tr = tg = tb = 0;														\
																				\
	/* select zero/other for alpha */											\
	if (!TEXMODE_TCA_ZERO_OTHER(TEXMODE))										\
		ta = (COTHER).rgb.a;														\
	else																		\
		ta = 0;																	\
																				\
	/* potentially subtract c_local */											\
	if (TEXMODE_TC_SUB_CLOCAL(TEXMODE))											\
	{																			\
		tr -= c_local.rgb.r;													\
		tg -= c_local.rgb.g;													\
		tb -= c_local.rgb.b;													\
	}																			\
	if (TEXMODE_TCA_SUB_CLOCAL(TEXMODE))										\
		ta -= c_local.rgb.a;													\
																				\
	/* blend RGB */																\
	switch (TEXMODE_TC_MSELECT(TEXMODE))										\
	{																			\
		default:	/* reserved */												\
		case 0:		/* zero */													\
			blendr = blendg = blendb = 0;										\
			break;																\
																				\
		case 1:		/* c_local */												\
			blendr = c_local.rgb.r;												\
			blendg = c_local.rgb.g;												\
			blendb = c_local.rgb.b;												\
			break;																\
																				\
		case 2:		/* a_other */												\
			blendr = blendg = blendb = (COTHER).rgb.a;							\
			break;																\
																				\
		case 3:		/* a_local */												\
			blendr = blendg = blendb = c_local.rgb.a;							\
			break;																\
																				\
		case 4:		/* LOD (detail factor) */									\
			if ((TT)->detailbias <= lod)										\
				blendr = blendg = blendb = 0;									\
			else																\
			{																	\
				blendr = ((((TT)->detailbias - lod) << (TT)->detailscale) >> 8);\
				if (blendr > (TT)->detailmax)									\
					blendr = (TT)->detailmax;									\
				blendg = blendb = blendr;										\
			}																	\
			break;																\
																				\
		case 5:		/* LOD fraction */											\
			blendr = blendg = blendb = lod & 0xff;								\
			break;																\
	}																			\
																				\
	/* blend alpha */															\
	switch (TEXMODE_TCA_MSELECT(TEXMODE))										\
	{																			\
		default:	/* reserved */												\
		case 0:		/* zero */													\
			blenda = 0;															\
			break;																\
																				\
		case 1:		/* c_local */												\
			blenda = c_local.rgb.a;												\
			break;																\
																				\
		case 2:		/* a_other */												\
			blenda = (COTHER).rgb.a;												\
			break;																\
																				\
		case 3:		/* a_local */												\
			blenda = c_local.rgb.a;												\
			break;																\
																				\
		case 4:		/* LOD (detail factor) */									\
			if ((TT)->detailbias <= lod)										\
				blenda = 0;														\
			else																\
			{																	\
				blenda = ((((TT)->detailbias - lod) << (TT)->detailscale) >> 8);\
				if (blenda > (TT)->detailmax)									\
					blenda = (TT)->detailmax;									\
			}																	\
			break;																\
																				\
		case 5:		/* LOD fraction */											\
			blenda = lod & 0xff;												\
			break;																\
	}																			\
																				\
	/* reverse the RGB blend */													\
	if (!TEXMODE_TC_REVERSE_BLEND(TEXMODE))										\
	{																			\
		blendr ^= 0xff;															\
		blendg ^= 0xff;															\
		blendb ^= 0xff;															\
	}																			\
																				\
	/* reverse the alpha blend */												\
	if (!TEXMODE_TCA_REVERSE_BLEND(TEXMODE))									\
		blenda ^= 0xff;															\
																				\
	/* do the blend */															\
	tr = (tr * (blendr + 1)) >> 8;												\
	tg = (tg * (blendg + 1)) >> 8;												\
	tb = (tb * (blendb + 1)) >> 8;												\
	ta = (ta * (blenda + 1)) >> 8;												\
																				\
	/* add clocal or alocal to RGB */											\
	switch (TEXMODE_TC_ADD_ACLOCAL(TEXMODE))									\
	{																			\
		case 3:		/* reserved */												\
		case 0:		/* nothing */												\
			break;																\
																				\
		case 1:		/* add c_local */											\
			tr += c_local.rgb.r;												\
			tg += c_local.rgb.g;												\
			tb += c_local.rgb.b;												\
			break;																\
																				\
		case 2:		/* add_alocal */											\
			tr += c_local.rgb.a;												\
			tg += c_local.rgb.a;												\
			tb += c_local.rgb.a;												\
			break;																\
	}																			\
																				\
	/* add clocal or alocal to alpha */											\
	if (TEXMODE_TCA_ADD_ACLOCAL(TEXMODE))										\
		ta += c_local.rgb.a;													\
																				\
	/* clamp */																	\
	(RESULT).rgb.r = (tr < 0) ? 0 : (tr > 0xff) ? 0xff : (uint8_t)tr;				\
	(RESULT).rgb.g = (tg < 0) ? 0 : (tg > 0xff) ? 0xff : (uint8_t)tg;				\
	(RESULT).rgb.b = (tb < 0) ? 0 : (tb > 0xff) ? 0xff : (uint8_t)tb;				\
	(RESULT).rgb.a = (ta < 0) ? 0 : (ta > 0xff) ? 0xff : (uint8_t)ta;				\
																				\
	/* invert */																\
	if (TEXMODE_TC_INVERT_OUTPUT(TEXMODE))										\
		(RESULT).u ^= 0x00ffffff;													\
	if (TEXMODE_TCA_INVERT_OUTPUT(TEXMODE))										\
		(RESULT).rgb.a ^= 0xff;													\
}																				\
while (0)



/*************************************
 *
 *  Pixel pipeline macros
 *
 *************************************/

#define PIXEL_PIPELINE_BEGIN(VV, STATS, XX, YY, FBZCOLORPATH, FBZMODE, ITERZ, ITERW, ZACOLOR, STIPPLE)	\
do																				\
{																				\
	int32_t depthval, wfloat;														\
	int32_t prefogr, prefogg, prefogb;											\
	int32_t r, g, b, a;															\
																				\
	/* apply clipping */														\
	/* note that for perf reasons, we assume the caller has done clipping */	\
																				\
	/* handle stippling */														\
	if (FBZMODE_ENABLE_STIPPLE(FBZMODE))										\
	{																			\
		/* rotate mode */														\
		if (FBZMODE_STIPPLE_PATTERN(FBZMODE) == 0)								\
		{																		\
			(STIPPLE) = ((STIPPLE) << 1) | ((STIPPLE) >> 31);					\
			if (((STIPPLE) & 0x80000000) == 0)									\
			{																	\
				goto skipdrawdepth;												\
			}																	\
		}																		\
																				\
		/* pattern mode */														\
		else																	\
		{																		\
			int stipple_index = (((YY) & 3) << 3) | (~(XX) & 7);				\
			if ((((STIPPLE) >> stipple_index) & 1) == 0)						\
			{																	\
				goto skipdrawdepth;												\
			}																	\
		}																		\
	}																			\
																				\
	/* compute "floating point" W value (used for depth and fog) */				\
	if ((ITERW) & LONGTYPE(0xffff00000000))										\
		wfloat = 0x0000;														\
	else																		\
	{																			\
		uint32_t temp = (uint32_t)(ITERW);											\
		if ((temp & 0xffff0000) == 0)											\
			wfloat = 0xffff;													\
		else																	\
		{																		\
			const auto exp = std::countl_zero(temp);	                        \
			const auto right_shift = std::max(0, 19 - exp);	                    \
			wfloat = ((exp << 12) | ((~temp >> right_shift) & 0xfff));          \
			if (wfloat < 0xffff) wfloat++;										\
		}																		\
	}																			\
																				\
	/* compute depth value (W or Z) for this pixel */							\
	if (FBZMODE_WBUFFER_SELECT(FBZMODE) == 0)									\
		CLAMPED_Z(ITERZ, FBZCOLORPATH, depthval);								\
	else if (FBZMODE_DEPTH_FLOAT_SELECT(FBZMODE) == 0)							\
		depthval = wfloat;														\
	else																		\
	{																			\
		if ((ITERZ) & 0xf0000000)												\
			depthval = 0x0000;													\
		else																	\
		{																		\
			uint32_t temp = (ITERZ) << 4;											\
			if ((temp & 0xffff0000) == 0)										\
				depthval = 0xffff;												\
			else																\
			{																	\
				const auto exp = std::countl_zero(temp);						\
				const auto right_shift = std::max(0, 19 - exp);	                \
				depthval = ((exp << 12) | ((~temp >> right_shift) & 0xfff));    \
				if (depthval < 0xffff) depthval++;								\
			}																	\
		}																		\
	}																			\
																				\
	/* add the bias */															\
	if (FBZMODE_ENABLE_DEPTH_BIAS(FBZMODE))										\
	{																			\
		depthval += (int16_t)(ZACOLOR);											\
		depthval = clamp_to_uint16(depthval);									\
	}																			\
																				\
	/* handle depth buffer testing */											\
	if (FBZMODE_ENABLE_DEPTHBUF(FBZMODE))										\
	{																			\
		int32_t depthsource;														\
																				\
		/* the source depth is either the iterated W/Z+bias or a */				\
		/* constant value */													\
		if (FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) == 0)							\
			depthsource = depthval;												\
		else																	\
			depthsource = (uint16_t)(ZACOLOR);									\
																				\
		/* test against the depth buffer */										\
		switch (FBZMODE_DEPTH_FUNCTION(FBZMODE))								\
		{																		\
			case 0:		/* depthOP = never */									\
				ADD_STAT_COUNT(STATS, zfunc_fail)								\
				goto skipdrawdepth;												\
																				\
			case 1:		/* depthOP = less than */								\
				if (depth)														\
					if (depthsource >= depth[XX])								\
					{															\
						ADD_STAT_COUNT(STATS, zfunc_fail)						\
						goto skipdrawdepth;										\
					}															\
				break;															\
																				\
			case 2:		/* depthOP = equal */									\
				if (depth)														\
					if (depthsource != depth[XX])								\
					{															\
						ADD_STAT_COUNT(STATS, zfunc_fail)						\
						goto skipdrawdepth;										\
					}															\
				break;															\
																				\
			case 3:		/* depthOP = less than or equal */						\
				if (depth)														\
					if (depthsource > depth[XX])								\
					{															\
						ADD_STAT_COUNT(STATS, zfunc_fail)						\
						goto skipdrawdepth;										\
					}															\
				break;															\
																				\
			case 4:		/* depthOP = greater than */							\
				if (depth)														\
					if (depthsource <= depth[XX])								\
					{															\
						ADD_STAT_COUNT(STATS, zfunc_fail)						\
						goto skipdrawdepth;										\
					}															\
				break;															\
																				\
			case 5:		/* depthOP = not equal */								\
				if (depth)														\
					if (depthsource == depth[XX])								\
					{															\
						ADD_STAT_COUNT(STATS, zfunc_fail)						\
						goto skipdrawdepth;										\
					}															\
				break;															\
																				\
			case 6:		/* depthOP = greater than or equal */					\
				if (depth)														\
					if (depthsource < depth[XX])								\
					{															\
						ADD_STAT_COUNT(STATS, zfunc_fail)						\
						goto skipdrawdepth;										\
					}															\
				break;															\
																				\
			case 7:		/* depthOP = always */									\
				break;															\
		}																		\
	}


#define PIXEL_PIPELINE_MODIFY(VV, DITHER, DITHER4, XX, FBZMODE, FBZCOLORPATH, ALPHAMODE, FOGMODE, ITERZ, ITERW, ITERAXXX) \
																				\
	/* perform fogging */														\
	prefogr = r;																\
	prefogg = g;																\
	prefogb = b;																\
	APPLY_FOGGING(VV, FOGMODE, FBZCOLORPATH, XX, DITHER4, r, g, b,				\
					ITERZ, ITERW, ITERAXXX);									\
																				\
	/* perform alpha blending */												\
	APPLY_ALPHA_BLEND(FBZMODE, ALPHAMODE, XX, DITHER, r, g, b, a);


#define PIXEL_PIPELINE_FINISH(VV, DITHER_LOOKUP, XX, dest, depth, FBZMODE)		\
																				\
	/* write to framebuffer */													\
	if (FBZMODE_RGB_BUFFER_MASK(FBZMODE))										\
	{																			\
		/* apply dithering */													\
		APPLY_DITHER(FBZMODE, XX, DITHER_LOOKUP, r, g, b);						\
		(dest)[XX] = (uint16_t)((r << 11) | (g << 5) | b);							\
	}																			\
																				\
	/* write to aux buffer */													\
	if ((depth) && FBZMODE_AUX_BUFFER_MASK(FBZMODE))								\
	{																			\
		if (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0)							\
			(depth)[XX] = (uint16_t)depthval;										\
		else																	\
			(depth)[XX] = (uint16_t)a;												\
	}

#define PIXEL_PIPELINE_END(STATS)												\
																				\
	/* track pixel writes to the frame buffer regardless of mask */				\
	ADD_STAT_COUNT(STATS, pixels_out)											\
																				\
skipdrawdepth:																	\
	;																			\
}																				\
while (0)

#endif //DOSBOX_VOODOO_DATA_H

#ifndef DOSBOX_VOODOO_DEF_H
#define DOSBOX_VOODOO_DEF_H

/*************************************
 *
 *  Register constants
 *
 *************************************/

/* Codes to the right:
    R = readable
    W = writeable
    P = pipelined
    F = goes to FIFO
*/

/* 0x000 */
#define status			(0x000/4)	/* R  P  */
#define intrCtrl		(0x004/4)	/* RW P   -- Voodoo2/Banshee only */
#define vertexAx		(0x008/4)	/*  W PF */
#define vertexAy		(0x00c/4)	/*  W PF */
#define vertexBx		(0x010/4)	/*  W PF */
#define vertexBy		(0x014/4)	/*  W PF */
#define vertexCx		(0x018/4)	/*  W PF */
#define vertexCy		(0x01c/4)	/*  W PF */
#define startR			(0x020/4)	/*  W PF */
#define startG			(0x024/4)	/*  W PF */
#define startB			(0x028/4)	/*  W PF */
#define startZ			(0x02c/4)	/*  W PF */
#define startA			(0x030/4)	/*  W PF */
#define startS			(0x034/4)	/*  W PF */
#define startT			(0x038/4)	/*  W PF */
#define startW			(0x03c/4)	/*  W PF */

/* 0x040 */
#define dRdX			(0x040/4)	/*  W PF */
#define dGdX			(0x044/4)	/*  W PF */
#define dBdX			(0x048/4)	/*  W PF */
#define dZdX			(0x04c/4)	/*  W PF */
#define dAdX			(0x050/4)	/*  W PF */
#define dSdX			(0x054/4)	/*  W PF */
#define dTdX			(0x058/4)	/*  W PF */
#define dWdX			(0x05c/4)	/*  W PF */
#define dRdY			(0x060/4)	/*  W PF */
#define dGdY			(0x064/4)	/*  W PF */
#define dBdY			(0x068/4)	/*  W PF */
#define dZdY			(0x06c/4)	/*  W PF */
#define dAdY			(0x070/4)	/*  W PF */
#define dSdY			(0x074/4)	/*  W PF */
#define dTdY			(0x078/4)	/*  W PF */
#define dWdY			(0x07c/4)	/*  W PF */

/* 0x080 */
#define triangleCMD		(0x080/4)	/*  W PF */
#define fvertexAx		(0x088/4)	/*  W PF */
#define fvertexAy		(0x08c/4)	/*  W PF */
#define fvertexBx		(0x090/4)	/*  W PF */
#define fvertexBy		(0x094/4)	/*  W PF */
#define fvertexCx		(0x098/4)	/*  W PF */
#define fvertexCy		(0x09c/4)	/*  W PF */
#define fstartR			(0x0a0/4)	/*  W PF */
#define fstartG			(0x0a4/4)	/*  W PF */
#define fstartB			(0x0a8/4)	/*  W PF */
#define fstartZ			(0x0ac/4)	/*  W PF */
#define fstartA			(0x0b0/4)	/*  W PF */
#define fstartS			(0x0b4/4)	/*  W PF */
#define fstartT			(0x0b8/4)	/*  W PF */
#define fstartW			(0x0bc/4)	/*  W PF */

/* 0x0c0 */
#define fdRdX			(0x0c0/4)	/*  W PF */
#define fdGdX			(0x0c4/4)	/*  W PF */
#define fdBdX			(0x0c8/4)	/*  W PF */
#define fdZdX			(0x0cc/4)	/*  W PF */
#define fdAdX			(0x0d0/4)	/*  W PF */
#define fdSdX			(0x0d4/4)	/*  W PF */
#define fdTdX			(0x0d8/4)	/*  W PF */
#define fdWdX			(0x0dc/4)	/*  W PF */
#define fdRdY			(0x0e0/4)	/*  W PF */
#define fdGdY			(0x0e4/4)	/*  W PF */
#define fdBdY			(0x0e8/4)	/*  W PF */
#define fdZdY			(0x0ec/4)	/*  W PF */
#define fdAdY			(0x0f0/4)	/*  W PF */
#define fdSdY			(0x0f4/4)	/*  W PF */
#define fdTdY			(0x0f8/4)	/*  W PF */
#define fdWdY			(0x0fc/4)	/*  W PF */

/* 0x100 */
#define ftriangleCMD	(0x100/4)	/*  W PF */
#define fbzColorPath	(0x104/4)	/* RW PF */
#define fogMode			(0x108/4)	/* RW PF */
#define alphaMode		(0x10c/4)	/* RW PF */
#define fbzMode			(0x110/4)	/* RW  F */
#define lfbMode			(0x114/4)	/* RW  F */
#define clipLeftRight	(0x118/4)	/* RW  F */
#define clipLowYHighY	(0x11c/4)	/* RW  F */
#define nopCMD			(0x120/4)	/*  W  F */
#define fastfillCMD		(0x124/4)	/*  W  F */
#define swapbufferCMD	(0x128/4)	/*  W  F */
#define fogColor		(0x12c/4)	/*  W  F */
#define zaColor			(0x130/4)	/*  W  F */
#define chromaKey		(0x134/4)	/*  W  F */
#define chromaRange		(0x138/4)	/*  W  F  -- Voodoo2/Banshee only */
#define userIntrCMD		(0x13c/4)	/*  W  F  -- Voodoo2/Banshee only */

/* 0x140 */
#define stipple			(0x140/4)	/* RW  F */
#define color0			(0x144/4)	/* RW  F */
#define color1			(0x148/4)	/* RW  F */
#define fbiPixelsIn		(0x14c/4)	/* R     */
#define fbiChromaFail	(0x150/4)	/* R     */
#define fbiZfuncFail	(0x154/4)	/* R     */
#define fbiAfuncFail	(0x158/4)	/* R     */
#define fbiPixelsOut	(0x15c/4)	/* R     */
#define fogTable		(0x160/4)	/*  W  F */

/* 0x1c0 */
#define cmdFifoBaseAddr	(0x1e0/4)	/* RW     -- Voodoo2 only */
#define cmdFifoBump		(0x1e4/4)	/* RW     -- Voodoo2 only */
#define cmdFifoRdPtr	(0x1e8/4)	/* RW     -- Voodoo2 only */
#define cmdFifoAMin		(0x1ec/4)	/* RW     -- Voodoo2 only */
#define colBufferAddr	(0x1ec/4)	/* RW     -- Banshee only */
#define cmdFifoAMax		(0x1f0/4)	/* RW     -- Voodoo2 only */
#define colBufferStride	(0x1f0/4)	/* RW     -- Banshee only */
#define cmdFifoDepth	(0x1f4/4)	/* RW     -- Voodoo2 only */
#define auxBufferAddr	(0x1f4/4)	/* RW     -- Banshee only */
#define cmdFifoHoles	(0x1f8/4)	/* RW     -- Voodoo2 only */
#define auxBufferStride	(0x1f8/4)	/* RW     -- Banshee only */

/* 0x200 */
#define fbiInit4		(0x200/4)	/* RW     -- Voodoo/Voodoo2 only */
#define clipLeftRight1	(0x200/4)	/* RW     -- Banshee only */
#define vRetrace		(0x204/4)	/* R      -- Voodoo/Voodoo2 only */
#define clipTopBottom1	(0x204/4)	/* RW     -- Banshee only */
#define backPorch		(0x208/4)	/* RW     -- Voodoo/Voodoo2 only */
#define videoDimensions	(0x20c/4)	/* RW     -- Voodoo/Voodoo2 only */
#define fbiInit0		(0x210/4)	/* RW     -- Voodoo/Voodoo2 only */
#define fbiInit1		(0x214/4)	/* RW     -- Voodoo/Voodoo2 only */
#define fbiInit2		(0x218/4)	/* RW     -- Voodoo/Voodoo2 only */
#define fbiInit3		(0x21c/4)	/* RW     -- Voodoo/Voodoo2 only */
#define hSync			(0x220/4)	/*  W     -- Voodoo/Voodoo2 only */
#define vSync			(0x224/4)	/*  W     -- Voodoo/Voodoo2 only */
#define clutData		(0x228/4)	/*  W  F  -- Voodoo/Voodoo2 only */
#define dacData			(0x22c/4)	/*  W     -- Voodoo/Voodoo2 only */
#define maxRgbDelta		(0x230/4)	/*  W     -- Voodoo/Voodoo2 only */
#define hBorder			(0x234/4)	/*  W     -- Voodoo2 only */
#define vBorder			(0x238/4)	/*  W     -- Voodoo2 only */
#define borderColor		(0x23c/4)	/*  W     -- Voodoo2 only */

/* 0x240 */
#define hvRetrace		(0x240/4)	/* R      -- Voodoo2 only */
#define fbiInit5		(0x244/4)	/* RW     -- Voodoo2 only */
#define fbiInit6		(0x248/4)	/* RW     -- Voodoo2 only */
#define fbiInit7		(0x24c/4)	/* RW     -- Voodoo2 only */
#define swapPending		(0x24c/4)	/*  W     -- Banshee only */
#define leftOverlayBuf	(0x250/4)	/*  W     -- Banshee only */
#define rightOverlayBuf	(0x254/4)	/*  W     -- Banshee only */
#define fbiSwapHistory	(0x258/4)	/* R      -- Voodoo2/Banshee only */
#define fbiTrianglesOut	(0x25c/4)	/* R      -- Voodoo2/Banshee only */
#define sSetupMode		(0x260/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sVx				(0x264/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sVy				(0x268/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sARGB			(0x26c/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sRed			(0x270/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sGreen			(0x274/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sBlue			(0x278/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sAlpha			(0x27c/4)	/*  W PF  -- Voodoo2/Banshee only */

/* 0x280 */
#define sVz				(0x280/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sWb				(0x284/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sWtmu0			(0x288/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sS_W0			(0x28c/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sT_W0			(0x290/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sWtmu1			(0x294/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sS_Wtmu1		(0x298/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sT_Wtmu1		(0x29c/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sDrawTriCMD		(0x2a0/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sBeginTriCMD	(0x2a4/4)	/*  W PF  -- Voodoo2/Banshee only */

/* 0x2c0 */
#define bltSrcBaseAddr	(0x2c0/4)	/* RW PF  -- Voodoo2 only */
#define bltDstBaseAddr	(0x2c4/4)	/* RW PF  -- Voodoo2 only */
#define bltXYStrides	(0x2c8/4)	/* RW PF  -- Voodoo2 only */
#define bltSrcChromaRange (0x2cc/4)	/* RW PF  -- Voodoo2 only */
#define bltDstChromaRange (0x2d0/4)	/* RW PF  -- Voodoo2 only */
#define bltClipX		(0x2d4/4)	/* RW PF  -- Voodoo2 only */
#define bltClipY		(0x2d8/4)	/* RW PF  -- Voodoo2 only */
#define bltSrcXY		(0x2e0/4)	/* RW PF  -- Voodoo2 only */
#define bltDstXY		(0x2e4/4)	/* RW PF  -- Voodoo2 only */
#define bltSize			(0x2e8/4)	/* RW PF  -- Voodoo2 only */
#define bltRop			(0x2ec/4)	/* RW PF  -- Voodoo2 only */
#define bltColor		(0x2f0/4)	/* RW PF  -- Voodoo2 only */
#define bltCommand		(0x2f8/4)	/* RW PF  -- Voodoo2 only */
#define bltData			(0x2fc/4)	/*  W PF  -- Voodoo2 only */

/* 0x300 */
#define textureMode		(0x300/4)	/*  W PF */
#define tLOD			(0x304/4)	/*  W PF */
#define tDetail			(0x308/4)	/*  W PF */
#define texBaseAddr		(0x30c/4)	/*  W PF */
#define texBaseAddr_1	(0x310/4)	/*  W PF */
#define texBaseAddr_2	(0x314/4)	/*  W PF */
#define texBaseAddr_3_8	(0x318/4)	/*  W PF */
#define trexInit0		(0x31c/4)	/*  W  F  -- Voodoo/Voodoo2 only */
#define trexInit1		(0x320/4)	/*  W  F */
#define nccTable		(0x324/4)	/*  W  F */



/*************************************
 *
 *  Alias map of the first 64
 *  registers when remapped
 *
 *************************************/

static const uint8_t register_alias_map[0x40] =
{
	status,		0x004/4,	vertexAx,	vertexAy,
	vertexBx,	vertexBy,	vertexCx,	vertexCy,
	startR,		dRdX,		dRdY,		startG,
	dGdX,		dGdY,		startB,		dBdX,
	dBdY,		startZ,		dZdX,		dZdY,
	startA,		dAdX,		dAdY,		startS,
	dSdX,		dSdY,		startT,		dTdX,
	dTdY,		startW,		dWdX,		dWdY,

	triangleCMD,0x084/4,	fvertexAx,	fvertexAy,
	fvertexBx,	fvertexBy,	fvertexCx,	fvertexCy,
	fstartR,	fdRdX,		fdRdY,		fstartG,
	fdGdX,		fdGdY,		fstartB,	fdBdX,
	fdBdY,		fstartZ,	fdZdX,		fdZdY,
	fstartA,	fdAdX,		fdAdY,		fstartS,
	fdSdX,		fdSdY,		fstartT,	fdTdX,
	fdTdY,		fstartW,	fdWdX,		fdWdY
};



/*************************************
 *
 *  Table of per-register access rights
 *
 *************************************/

static const uint8_t voodoo_register_access[0x100] =
{
	/* 0x000 */
	REG_RP,		0,			REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x040 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x080 */
	REG_WPF,	0,			REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x0c0 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x100 */
	REG_WPF,	REG_RWPF,	REG_RWPF,	REG_RWPF,
	REG_RWF,	REG_RWF,	REG_RWF,	REG_RWF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		0,			0,

	/* 0x140 */
	REG_RWF,	REG_RWF,	REG_RWF,	REG_R,
	REG_R,		REG_R,		REG_R,		REG_R,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x180 */
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x1c0 */
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	0,			0,			0,			0,
	0,			0,			0,			0,

	/* 0x200 */
	REG_RW,		REG_R,		REG_RW,		REG_RW,
	REG_RW,		REG_RW,		REG_RW,		REG_RW,
	REG_W,		REG_W,		REG_W,		REG_W,
	REG_W,		0,			0,			0,

	/* 0x240 */
	0,			0,			0,			0,
	0,			0,			0,			0,
	0,			0,			0,			0,
	0,			0,			0,			0,

	/* 0x280 */
	0,			0,			0,			0,
	0,			0,			0,			0,
	0,			0,			0,			0,
	0,			0,			0,			0,

	/* 0x2c0 */
	0,			0,			0,			0,
	0,			0,			0,			0,
	0,			0,			0,			0,
	0,			0,			0,			0,

	/* 0x300 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x340 */
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x380 */
	REG_WF
};

// TODO bring this back when enabling voodoo2 code
#if 0 
static const uint8_t voodoo2_register_access[0x100] =
{
	/* 0x000 */
	REG_RP,		REG_RWPT,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x040 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x080 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x0c0 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x100 */
	REG_WPF,	REG_RWPF,	REG_RWPF,	REG_RWPF,
	REG_RWF,	REG_RWF,	REG_RWF,	REG_RWF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x140 */
	REG_RWF,	REG_RWF,	REG_RWF,	REG_R,
	REG_R,		REG_R,		REG_R,		REG_R,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x180 */
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x1c0 */
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_RWT,	REG_RWT,	REG_RWT,	REG_RWT,
	REG_RWT,	REG_RWT,	REG_RWT,	REG_RW,

	/* 0x200 */
	REG_RWT,	REG_R,		REG_RWT,	REG_RWT,
	REG_RWT,	REG_RWT,	REG_RWT,	REG_RWT,
	REG_WT,		REG_WT,		REG_WF,		REG_WT,
	REG_WT,		REG_WT,		REG_WT,		REG_WT,

	/* 0x240 */
	REG_R,		REG_RWT,	REG_RWT,	REG_RWT,
	0,			0,			REG_R,		REG_R,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x280 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	0,			0,
	0,			0,			0,			0,

	/* 0x2c0 */
	REG_RWPF,	REG_RWPF,	REG_RWPF,	REG_RWPF,
	REG_RWPF,	REG_RWPF,	REG_RWPF,	REG_RWPF,
	REG_RWPF,	REG_RWPF,	REG_RWPF,	REG_RWPF,
	REG_RWPF,	REG_RWPF,	REG_RWPF,	REG_WPF,

	/* 0x300 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x340 */
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x380 */
	REG_WF
};
#endif

#ifdef C_ENABLE_VOODOO_DEBUG
/*************************************
 *
 *  Register string table for debug
 *
 *************************************/

static const char *const voodoo_reg_name[] =
{
	/* 0x000 */
	"status",		"{intrCtrl}",	"vertexAx",		"vertexAy",
	"vertexBx",		"vertexBy",		"vertexCx",		"vertexCy",
	"startR",		"startG",		"startB",		"startZ",
	"startA",		"startS",		"startT",		"startW",
	/* 0x040 */
	"dRdX",			"dGdX",			"dBdX",			"dZdX",
	"dAdX",			"dSdX",			"dTdX",			"dWdX",
	"dRdY",			"dGdY",			"dBdY",			"dZdY",
	"dAdY",			"dSdY",			"dTdY",			"dWdY",
	/* 0x080 */
	"triangleCMD",	"reserved084",	"fvertexAx",	"fvertexAy",
	"fvertexBx",	"fvertexBy",	"fvertexCx",	"fvertexCy",
	"fstartR",		"fstartG",		"fstartB",		"fstartZ",
	"fstartA",		"fstartS",		"fstartT",		"fstartW",
	/* 0x0c0 */
	"fdRdX",		"fdGdX",		"fdBdX",		"fdZdX",
	"fdAdX",		"fdSdX",		"fdTdX",		"fdWdX",
	"fdRdY",		"fdGdY",		"fdBdY",		"fdZdY",
	"fdAdY",		"fdSdY",		"fdTdY",		"fdWdY",
	/* 0x100 */
	"ftriangleCMD",	"fbzColorPath",	"fogMode",		"alphaMode",
	"fbzMode",		"lfbMode",		"clipLeftRight","clipLowYHighY",
	"nopCMD",		"fastfillCMD",	"swapbufferCMD","fogColor",
	"zaColor",		"chromaKey",	"{chromaRange}","{userIntrCMD}",
	/* 0x140 */
	"stipple",		"color0",		"color1",		"fbiPixelsIn",
	"fbiChromaFail","fbiZfuncFail",	"fbiAfuncFail",	"fbiPixelsOut",
	"fogTable160",	"fogTable164",	"fogTable168",	"fogTable16c",
	"fogTable170",	"fogTable174",	"fogTable178",	"fogTable17c",
	/* 0x180 */
	"fogTable180",	"fogTable184",	"fogTable188",	"fogTable18c",
	"fogTable190",	"fogTable194",	"fogTable198",	"fogTable19c",
	"fogTable1a0",	"fogTable1a4",	"fogTable1a8",	"fogTable1ac",
	"fogTable1b0",	"fogTable1b4",	"fogTable1b8",	"fogTable1bc",
	/* 0x1c0 */
	"fogTable1c0",	"fogTable1c4",	"fogTable1c8",	"fogTable1cc",
	"fogTable1d0",	"fogTable1d4",	"fogTable1d8",	"fogTable1dc",
	"{cmdFifoBaseAddr}","{cmdFifoBump}","{cmdFifoRdPtr}","{cmdFifoAMin}",
	"{cmdFifoAMax}","{cmdFifoDepth}","{cmdFifoHoles}","reserved1fc",
	/* 0x200 */
	"fbiInit4",		"vRetrace",		"backPorch",	"videoDimensions",
	"fbiInit0",		"fbiInit1",		"fbiInit2",		"fbiInit3",
	"hSync",		"vSync",		"clutData",		"dacData",
	"maxRgbDelta",	"{hBorder}",	"{vBorder}",	"{borderColor}",
	/* 0x240 */
	"{hvRetrace}",	"{fbiInit5}",	"{fbiInit6}",	"{fbiInit7}",
	"reserved250",	"reserved254",	"{fbiSwapHistory}","{fbiTrianglesOut}",
	"{sSetupMode}",	"{sVx}",		"{sVy}",		"{sARGB}",
	"{sRed}",		"{sGreen}",		"{sBlue}",		"{sAlpha}",
	/* 0x280 */
	"{sVz}",		"{sWb}",		"{sWtmu0}",		"{sS/Wtmu0}",
	"{sT/Wtmu0}",	"{sWtmu1}",		"{sS/Wtmu1}",	"{sT/Wtmu1}",
	"{sDrawTriCMD}","{sBeginTriCMD}","reserved2a8",	"reserved2ac",
	"reserved2b0",	"reserved2b4",	"reserved2b8",	"reserved2bc",
	/* 0x2c0 */
	"{bltSrcBaseAddr}","{bltDstBaseAddr}","{bltXYStrides}","{bltSrcChromaRange}",
	"{bltDstChromaRange}","{bltClipX}","{bltClipY}","reserved2dc",
	"{bltSrcXY}",	"{bltDstXY}",	"{bltSize}",	"{bltRop}",
	"{bltColor}",	"reserved2f4",	"{bltCommand}",	"{bltData}",
	/* 0x300 */
	"textureMode",	"tLOD",			"tDetail",		"texBaseAddr",
	"texBaseAddr_1","texBaseAddr_2","texBaseAddr_3_8","trexInit0",
	"trexInit1",	"nccTable0.0",	"nccTable0.1",	"nccTable0.2",
	"nccTable0.3",	"nccTable0.4",	"nccTable0.5",	"nccTable0.6",
	/* 0x340 */
	"nccTable0.7",	"nccTable0.8",	"nccTable0.9",	"nccTable0.A",
	"nccTable0.B",	"nccTable1.0",	"nccTable1.1",	"nccTable1.2",
	"nccTable1.3",	"nccTable1.4",	"nccTable1.5",	"nccTable1.6",
	"nccTable1.7",	"nccTable1.8",	"nccTable1.9",	"nccTable1.A",
	/* 0x380 */
	"nccTable1.B"
};
#endif //C_ENABLE_VOODOO_DEBUG
#endif //DOSBOX_VOODOO_DEF_H

/*
    3dfx Voodoo Graphics SST-1/2 emulator

    --------------------------

    Specs:

    Voodoo 1 (SST1):
        2,4MB frame buffer RAM
        1,2,4MB texture RAM
        50MHz clock frequency
        clears @ 2 pixels/clock (RGB and depth simultaneously)
        renders @ 1 pixel/clock
        64 entry PCI FIFO
        memory FIFO up to 65536 entries

    Voodoo 2:
        2,4MB frame buffer RAM
        2,4,8,16MB texture RAM
        90MHz clock frquency
        clears @ 2 pixels/clock (RGB and depth simultaneously)
        renders @ 1 pixel/clock
        ultrafast clears @ 16 pixels/clock
        128 entry PCI FIFO
        memory FIFO up to 65536 entries

    --------------------------


iterated RGBA = 12.12 [24 bits]
iterated Z    = 20.12 [32 bits]
iterated W    = 18.32 [48 bits]

**************************************************************************/

static voodoo_state* v = nullptr;
static auto vtype = VOODOO_1;

static auto voodoo_bilinear_filtering = false;

#define LOG_VOODOO LOG_PCI
enum {
	LOG_VBLANK_SWAP = 0,
	LOG_REGISTERS   = 0,
	LOG_LFB         = 0,
	LOG_TEXTURE_RAM = 0,
	LOG_RASTERIZERS = 0,
};

/*************************************
 *
 *  Prototypes
 *
 *************************************/

/* drawing */
static void Voodoo_UpdateScreenStart();
static bool Voodoo_GetRetrace();
static double Voodoo_GetVRetracePosition();
static double Voodoo_GetHRetracePosition();

/***************************************************************************
    RASTERIZER MANAGEMENT
***************************************************************************/

static dither_lut_t dither2_lookup = {};
static dither_lut_t dither4_lookup = {};

static inline void raster_generic(const voodoo_state* vs, uint32_t TMUS, uint32_t TEXMODE0,
                                  uint32_t TEXMODE1, void* destbase, int32_t y,
                                  const poly_extent* extent, stats_block& stats)
{
	const uint8_t* dither_lookup = nullptr;
	const uint8_t* dither4       = nullptr;
	const uint8_t* dither        = nullptr;

	int32_t scry = y;
	int32_t startx = extent->startx;
	int32_t stopx = extent->stopx;

	// Quick references
	const auto regs  = vs->reg;
	const auto& fbi  = vs->fbi;
	const auto& tmu0 = vs->tmu[0];
	const auto& tmu1 = vs->tmu[1];

	const uint32_t r_fbzColorPath = regs[fbzColorPath].u;
	const uint32_t r_fbzMode      = regs[fbzMode].u;
	const uint32_t r_alphaMode    = regs[alphaMode].u;
	const uint32_t r_fogMode      = regs[fogMode].u;
	const uint32_t r_zaColor      = regs[zaColor].u;

	uint32_t r_stipple = regs[stipple].u;

	/* determine the screen Y */
	if (FBZMODE_Y_ORIGIN(r_fbzMode)) {
		scry = (fbi.yorigin - y) & 0x3ff;
	}

	/* compute the dithering pointers */
	if (FBZMODE_ENABLE_DITHERING(r_fbzMode))
	{
		dither4 = &dither_matrix_4x4[(y & 3) * 4];
		if (FBZMODE_DITHER_TYPE(r_fbzMode) == 0)
		{
			dither = dither4;
			dither_lookup = &dither4_lookup[(y & 3) << 11];
		}
		else
		{
			dither = &dither_matrix_2x2[(y & 3) * 4];
			dither_lookup = &dither2_lookup[(y & 3) << 11];
		}
	}

	/* apply clipping */
	if (FBZMODE_ENABLE_CLIPPING(r_fbzMode))
	{
		/* Y clipping buys us the whole scanline */
		if (scry < (int32_t)((regs[clipLowYHighY].u >> 16) & 0x3ff) ||
		    scry >= (int32_t)(regs[clipLowYHighY].u & 0x3ff)) {
			stats.pixels_in += stopx - startx;
			//stats.clip_fail += stopx - startx;
			return;
		}

		/* X clipping */
		int32_t tempclip = (regs[clipLeftRight].u >> 16) & 0x3ff;
		if (startx < tempclip)
		{
			stats.pixels_in += tempclip - startx;
			startx = tempclip;
		}
		tempclip = regs[clipLeftRight].u & 0x3ff;
		if (stopx >= tempclip)
		{
			stats.pixels_in += stopx - tempclip;
			stopx = tempclip - 1;
		}
	}

	/* get pointers to the target buffer and depth buffer */
	uint16_t* dest  = (uint16_t*)destbase + scry * fbi.rowpixels;
	uint16_t* depth = (fbi.auxoffs != (uint32_t)(~0))
	                        ? ((uint16_t*)(fbi.ram + fbi.auxoffs) +
	                           scry * fbi.rowpixels)
	                        : nullptr;

	/* compute the starting parameters */
	const int32_t dx = startx - (fbi.ax >> 4);
	const int32_t dy = y - (fbi.ay >> 4);

	int32_t iterr = fbi.startr + dy * fbi.drdy + dx * fbi.drdx;
	int32_t iterg = fbi.startg + dy * fbi.dgdy + dx * fbi.dgdx;
	int32_t iterb = fbi.startb + dy * fbi.dbdy + dx * fbi.dbdx;
	int32_t itera = fbi.starta + dy * fbi.dady + dx * fbi.dadx;
	int32_t iterz = fbi.startz + dy * fbi.dzdy + dx * fbi.dzdx;
	int64_t iterw = fbi.startw + dy * fbi.dwdy + dx * fbi.dwdx;
	int64_t iterw0 = 0;
	int64_t iterw1 = 0;
	int64_t iters0 = 0;
	int64_t iters1 = 0;
	int64_t itert0 = 0;
	int64_t itert1 = 0;
	if (TMUS >= 1)
	{
		iterw0 = tmu0.startw + dy * tmu0.dwdy + dx * tmu0.dwdx;
		iters0 = tmu0.starts + dy * tmu0.dsdy + dx * tmu0.dsdx;
		itert0 = tmu0.startt + dy * tmu0.dtdy + dx * tmu0.dtdx;
	}
	if (TMUS >= 2)
	{
		iterw1 = tmu1.startw + dy * tmu1.dwdy + dx * tmu1.dwdx;
		iters1 = tmu1.starts + dy * tmu1.dsdy + dx * tmu1.dsdx;
		itert1 = tmu1.startt + dy * tmu1.dtdy + dx * tmu1.dtdx;
	}

	/* loop in X */
	for (int32_t x = startx; x < stopx; x++)
	{
		rgb_union iterargb = { 0 };
		rgb_union texel = { 0 };

		/* pixel pipeline part 1 handles depth testing and stippling */
		PIXEL_PIPELINE_BEGIN(vs, stats, x, y, r_fbzColorPath, r_fbzMode, iterz, iterw, r_zaColor, r_stipple);

		/* run the texture pipeline on TMU1 to produce a value in texel */
		/* note that they set LOD min to 8 to "disable" a TMU */

		if (TMUS >= 2 && vs->tmu[1].lodmin < (8 << 8)) {
			const tmu_state* const tmus = &vs->tmu[1];
			const rgb_t* const lookup = tmus->lookup;
			TEXTURE_PIPELINE(tmus, x, dither4, TEXMODE1, texel,
								lookup, tmus->lodbasetemp,
								iters1, itert1, iterw1, texel);
		}

		/* run the texture pipeline on TMU0 to produce a final */
		/* result in texel */
		/* note that they set LOD min to 8 to "disable" a TMU */
		if (TMUS >= 1 && tmu0.lodmin < (8 << 8)) {
			if (!vs->send_config) {
				const tmu_state* const tmus = &tmu0;
				const rgb_t* const lookup = tmus->lookup;
				TEXTURE_PIPELINE(tmus, x, dither4, TEXMODE0, texel,
								lookup, tmus->lodbasetemp,
								iters0, itert0, iterw0, texel);
			} else {	/* send config data to the frame buffer */
				texel.u=vs->tmu_config;
			}
		}

		/* colorpath pipeline selects source colors and does blending */
		CLAMPED_ARGB(iterr, iterg, iterb, itera, r_fbzColorPath, iterargb);


		int32_t blendr;
		int32_t blendg;
		int32_t blendb;
		int32_t blenda;
		rgb_union c_other;
		rgb_union c_local;

		/* compute c_other */
		switch (FBZCP_CC_RGBSELECT(r_fbzColorPath))
		{
			case 0:		/* iterated RGB */
				c_other.u = iterargb.u;
				break;
			case 1:		/* texture RGB */
				c_other.u = texel.u;
				break;
			case 2:		/* color1 RGB */
			        c_other.u = regs[color1].u;
			        break;
			case 3:	/* reserved */
				c_other.u = 0;
				break;
		}

		/* handle chroma key */
		APPLY_CHROMAKEY(vs, stats, r_fbzMode, c_other);

		/* compute a_other */
		switch (FBZCP_CC_ASELECT(r_fbzColorPath))
		{
			case 0:		/* iterated alpha */
				c_other.rgb.a = iterargb.rgb.a;
				break;
			case 1:		/* texture alpha */
				c_other.rgb.a = texel.rgb.a;
				break;
			case 2:		/* color1 alpha */
			        c_other.rgb.a = regs[color1].rgb.a;
			        break;
			case 3:	/* reserved */
				c_other.rgb.a = 0;
				break;
		}

		/* handle alpha mask */
		APPLY_ALPHAMASK(vs, stats, r_fbzMode, c_other.rgb.a);

		/* handle alpha test */
		APPLY_ALPHATEST(vs, stats, r_alphaMode, c_other.rgb.a);

		/* compute c_local */
		if (FBZCP_CC_LOCALSELECT_OVERRIDE(r_fbzColorPath) == 0)
		{
			if (FBZCP_CC_LOCALSELECT(r_fbzColorPath) == 0) {
				// iterated RGB
				c_local.u = iterargb.u;
			} else {
				// color0 RGB
				c_local.u = regs[color0].u;
			}
		}
		else
		{
			if ((texel.rgb.a & 0x80) == 0) {
				// iterated RGB
				c_local.u = iterargb.u;
			} else {
				// color0 RGB
				c_local.u = regs[color0].u;
			}
		}

		/* compute a_local */
		switch (FBZCP_CCA_LOCALSELECT(r_fbzColorPath))
		{
			case 0:		/* iterated alpha */
				c_local.rgb.a = iterargb.rgb.a;
				break;
			case 1:		/* color0 alpha */
			        c_local.rgb.a = regs[color0].rgb.a;
			        break;
			case 2:		/* clamped iterated Z[27:20] */
			{
				int temp;
				CLAMPED_Z(iterz, r_fbzColorPath, temp);
				c_local.rgb.a = (uint8_t)temp;
				break;
			}
			case 3:		/* clamped iterated W[39:32] */
			{
				int temp;
				CLAMPED_W(iterw, r_fbzColorPath, temp);			/* Voodoo 2 only */
				c_local.rgb.a = (uint8_t)temp;
				break;
			}
		}

		/* select zero or c_other */
		if (FBZCP_CC_ZERO_OTHER(r_fbzColorPath) == 0)
		{
			r = c_other.rgb.r;
			g = c_other.rgb.g;
			b = c_other.rgb.b;
		} else {
			r = g = b = 0;
		}

		/* select zero or a_other */
		if (FBZCP_CCA_ZERO_OTHER(r_fbzColorPath) == 0) {
			a = c_other.rgb.a;
		} else {
			a = 0;
		}

		/* subtract c_local */
		if (FBZCP_CC_SUB_CLOCAL(r_fbzColorPath))
		{
			r -= c_local.rgb.r;
			g -= c_local.rgb.g;
			b -= c_local.rgb.b;
		}

		/* subtract a_local */
		if (FBZCP_CCA_SUB_CLOCAL(r_fbzColorPath)) {
			a -= c_local.rgb.a;
		}

		/* blend RGB */
		switch (FBZCP_CC_MSELECT(r_fbzColorPath))
		{
			default:	/* reserved */
			case 0:		/* 0 */
				blendr = blendg = blendb = 0;
				break;
			case 1:		/* c_local */
				blendr = c_local.rgb.r;
				blendg = c_local.rgb.g;
				blendb = c_local.rgb.b;
				break;
			case 2:		/* a_other */
				blendr = blendg = blendb = c_other.rgb.a;
				break;
			case 3:		/* a_local */
				blendr = blendg = blendb = c_local.rgb.a;
				break;
			case 4:		/* texture alpha */
				blendr = blendg = blendb = texel.rgb.a;
				break;
			case 5:		/* texture RGB (Voodoo 2 only) */
				blendr = texel.rgb.r;
				blendg = texel.rgb.g;
				blendb = texel.rgb.b;
				break;
		}

		/* blend alpha */
		switch (FBZCP_CCA_MSELECT(r_fbzColorPath))
		{
			default:	/* reserved */
			case 0:		/* 0 */
				blenda = 0;
				break;
			case 1:		/* a_local */
			case 3:
				blenda = c_local.rgb.a;
				break;
			case 2:		/* a_other */
				blenda = c_other.rgb.a;
				break;
			case 4:		/* texture alpha */
				blenda = texel.rgb.a;
				break;
		}

		/* reverse the RGB blend */
		if (!FBZCP_CC_REVERSE_BLEND(r_fbzColorPath))
		{
			blendr ^= 0xff;
			blendg ^= 0xff;
			blendb ^= 0xff;
		}

		/* reverse the alpha blend */
		if (!FBZCP_CCA_REVERSE_BLEND(r_fbzColorPath)) {
			blenda ^= 0xff;
		}

		/* do the blend */
		r = (r * (blendr + 1)) >> 8;
		g = (g * (blendg + 1)) >> 8;
		b = (b * (blendb + 1)) >> 8;
		a = (a * (blenda + 1)) >> 8;

		/* add clocal or alocal to RGB */
		switch (FBZCP_CC_ADD_ACLOCAL(r_fbzColorPath))
		{
			case 3:		/* reserved */
			case 0:		/* nothing */
				break;
			case 1:		/* add c_local */
				r += c_local.rgb.r;
				g += c_local.rgb.g;
				b += c_local.rgb.b;
				break;
			case 2:		/* add_alocal */
				r += c_local.rgb.a;
				g += c_local.rgb.a;
				b += c_local.rgb.a;
				break;
		}

		/* add clocal or alocal to alpha */
		if (FBZCP_CCA_ADD_ACLOCAL(r_fbzColorPath)) {
			a += c_local.rgb.a;
		}

		/* clamp */
		r = clamp_to_uint8(r);
		g = clamp_to_uint8(g);
		b = clamp_to_uint8(b);
		a = clamp_to_uint8(a);

		/* invert */
		if (FBZCP_CC_INVERT_OUTPUT(r_fbzColorPath))
		{
			r ^= 0xff;
			g ^= 0xff;
			b ^= 0xff;
		}
		if (FBZCP_CCA_INVERT_OUTPUT(r_fbzColorPath)) {
			a ^= 0xff;
		}

		/* pixel pipeline part 2 handles fog, alpha, and final output */
		PIXEL_PIPELINE_MODIFY(vs, dither, dither4, x,
							r_fbzMode, r_fbzColorPath, r_alphaMode, r_fogMode,
							iterz, iterw, iterargb);
		PIXEL_PIPELINE_FINISH(vs, dither_lookup, x, dest, depth, r_fbzMode);
		PIXEL_PIPELINE_END(stats);

		/* update the iterated parameters */
		iterr += fbi.drdx;
		iterg += fbi.dgdx;
		iterb += fbi.dbdx;
		itera += fbi.dadx;
		iterz += fbi.dzdx;
		iterw += fbi.dwdx;
		if (TMUS >= 1)
		{
			iterw0 += tmu0.dwdx;
			iters0 += tmu0.dsdx;
			itert0 += tmu0.dtdx;
		}
		if (TMUS >= 2)
		{
			iterw1 += tmu1.dwdx;
			iters1 += tmu1.dsdx;
			itert1 += tmu1.dtdx;
		}
	}
}

#ifdef C_ENABLE_VOODOO_OPENGL
/*-------------------------------------------------
    add_rasterizer - add a rasterizer to our
    hash table
-------------------------------------------------*/
static raster_info *add_rasterizer(voodoo_state *vs, const raster_info *cinfo)
{
	if (vs->next_rasterizer >= MAX_RASTERIZERS)
	{
		E_Exit("Out of space for new rasterizers!");
		vs->next_rasterizer = 0;
	}

	raster_info *info = &vs->rasterizer[vs->next_rasterizer++];
	int hash = compute_raster_hash(cinfo);

	/* make a copy of the info */
	*info = *cinfo;

#ifdef C_ENABLE_VOODOO_DEBUG
	/* fill in the data */
	info->hits = 0;
	info->polys = 0;
#endif

	/* hook us into the hash table */
	info->next = vs->raster_hash[hash];
	vs->raster_hash[hash] = info;

	if (LOG_RASTERIZERS)
		maybe_log_debug("Adding rasterizer @ %p : %08X %08X %08X %08X %08X %08X (hash=%d)\n",
		                info->callback,
		                info->eff_color_path,
		                info->eff_alpha_mode,
		                info->eff_fog_mode,
		                info->eff_fbz_mode,
		                info->eff_tex_mode_0,
		                info->eff_tex_mode_1,
		                hash);

	return info;
}

/*-------------------------------------------------
    find_rasterizer - find a rasterizer that
    matches  our current parameters and return
    it, creating a new one if necessary
-------------------------------------------------*/
static raster_info *find_rasterizer(voodoo_state *vs, int texcount)
{
	raster_info *info, *prev = NULL;
	raster_info curinfo;
	int hash;

	/* build an info struct with all the parameters */
	const auto regs = vs->reg;

	curinfo.eff_color_path = normalize_color_path(regs[fbzColorPath].u);
	curinfo.eff_alpha_mode = normalize_alpha_mode(regs[alphaMode].u);
	curinfo.eff_fog_mode   = normalize_fog_mode(regs[fogMode].u);
	curinfo.eff_fbz_mode   = normalize_fbz_mode(regs[fbzMode].u);
	curinfo.eff_tex_mode_0 = (texcount >= 1)
	                               ? normalize_tex_mode(
	                                         vs->tmu[0].reg[textureMode].u)
	                               : 0xffffffff;
	curinfo.eff_tex_mode_1 = (texcount >= 2) ? normalize_tex_mode(vs->tmu[1].reg[textureMode].u) : 0xffffffff;

	/* compute the hash */
	hash = compute_raster_hash(&curinfo);

	/* find the appropriate hash entry */
	for (info = vs->raster_hash[hash]; info; prev = info, info = info->next)
		if (info->eff_color_path == curinfo.eff_color_path &&
			info->eff_alpha_mode == curinfo.eff_alpha_mode &&
			info->eff_fog_mode == curinfo.eff_fog_mode &&
			info->eff_fbz_mode == curinfo.eff_fbz_mode &&
			info->eff_tex_mode_0 == curinfo.eff_tex_mode_0 &&
			info->eff_tex_mode_1 == curinfo.eff_tex_mode_1)
		{
			/* got it, move us to the head of the list */
			if (prev)
			{
				prev->next = info->next;
				info->next = vs->raster_hash[hash];
				vs->raster_hash[hash] = info;
			}

			/* return the result */
			return info;
		}

	/* generate a new one using the generic entry */
	curinfo.callback = (texcount == 0) ? raster_generic_0tmu : (texcount == 1) ? raster_generic_1tmu : raster_generic_2tmu;
#ifdef C_ENABLE_VOODOO_DEBUG
	curinfo.is_generic = true;
	curinfo.display = 0;
	curinfo.polys = 0;
	curinfo.hits = 0;
#endif
	curinfo.next = 0;
	curinfo.shader_ready = false;

	return add_rasterizer(vs, &curinfo);
}
#endif


/***************************************************************************
    GENERIC RASTERIZERS
***************************************************************************/

/*-------------------------------------------------
    raster_fastfill - per-scanline
    implementation of the 'fastfill' command
-------------------------------------------------*/
static void raster_fastfill(void *destbase, int32_t y, const poly_extent *extent, const uint16_t* extra_dither)
{
	stats_block stats = {};
	const int32_t startx = extent->startx;
	int32_t stopx = extent->stopx;
	int scry;
	int x;

	/* determine the screen Y */
	scry = y;
	if (FBZMODE_Y_ORIGIN(v->reg[fbzMode].u)) {
		scry = (v->fbi.yorigin - y) & 0x3ff;
	}

	/* fill this RGB row */
	if (FBZMODE_RGB_BUFFER_MASK(v->reg[fbzMode].u))
	{
		const uint16_t* ditherow = &extra_dither[(y & 3) * 4];

		const auto expanded = read_unaligned_uint64(
		        reinterpret_cast<const uint8_t*>(ditherow));

		uint16_t* dest = reinterpret_cast<uint16_t*>(destbase) +
		                 scry * v->fbi.rowpixels;

		for (x = startx; x < stopx && (x & 3) != 0; x++) {
			dest[x] = ditherow[x & 3];
		}
		for (; x < (stopx & ~3); x += 4) {
			write_unaligned_uint64(reinterpret_cast<uint8_t*>(&dest[x]), expanded);
		}
		for (; x < stopx; x++) {
			dest[x] = ditherow[x & 3];
		}
		stats.pixels_out += stopx - startx;
	}

	/* fill this dest buffer row */
	if (FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) && v->fbi.auxoffs != (uint32_t)(~0))
	{
		const auto color = static_cast<uint16_t>(v->reg[zaColor].u & 0xffff);

		const uint64_t expanded = (static_cast<uint64_t>(color) << 48) |
		                          (static_cast<uint64_t>(color) << 32) |
		                          (static_cast<uint32_t>(color) << 16) |
		                          color;

		uint16_t* dest = reinterpret_cast<uint16_t*>(v->fbi.ram +
		                                             v->fbi.auxoffs) +
		                 scry * v->fbi.rowpixels;

		if (v->fbi.auxoffs + 2 * (scry * v->fbi.rowpixels + stopx) >= v->fbi.mask) {
			stopx = (v->fbi.mask - v->fbi.auxoffs) / 2 - scry * v->fbi.rowpixels;
			if ((stopx < 0) || (stopx < startx)) {
				return;
			}
		}

		for (x = startx; x < stopx && (x & 3) != 0; x++) {
			dest[x] = color;
		}
		for (; x < (stopx & ~3); x += 4) {
			write_unaligned_uint64(reinterpret_cast<uint8_t*>(&dest[x]), expanded);
		}
		for (; x < stopx; x++) {
			dest[x] = color;
		}
	}
}


/*************************************
 *
 *  Common initialization
 *
 *************************************/

static void init_fbi(fbi_state* f, int fbmem)
{
	/* allocate frame buffer RAM and set pointers */
	assert(fbmem >= 1); //VOODOO: invalid frame buffer memory size requested

	// Align FBI memory to 64-bit, which is the maximum type written
	constexpr auto mem_alignment = sizeof(uint64_t);
	std::tie(f->ram_buffer, f->ram) = make_unique_aligned_array<uint8_t>(mem_alignment, fbmem);
	assert(reinterpret_cast<uintptr_t>(f->ram) % mem_alignment == 0);

	f->mask = (uint32_t)(fbmem - 1);
	f->rgboffs[0] = f->rgboffs[1] = f->rgboffs[2] = 0;
	f->auxoffs = (uint32_t)(~0);

	/* default to 0x0 */
	f->frontbuf = 0;
	f->backbuf = 1;
	f->width = 640;
	f->height = 480;
	//f->xoffs = 0;
	//f->yoffs = 0;

	//f->vsyncscan = 0;

	/* init the pens */
	//for (uint8_t pen = 0; pen < 32; pen++)
	//	v->fbi.clut[pen] = MAKE_ARGB(pen, pal5bit(pen), pal5bit(pen), pal5bit(pen));
	//v->fbi.clut[32] = MAKE_ARGB(32,0xff,0xff,0xff);

	/* allocate a VBLANK timer */
	f->vblank = 0u;

	/* initialize the memory FIFO */
	f->fifo.size = 0;

	/* set the fog delta mask */
	f->fogdelta_mask = (vtype < VOODOO_2) ? 0xff : 0xfc;

	f->yorigin = 0;

	f->sverts = 0;

	f->lfb_stats = {};

	memset(&f->fogblend, 0, sizeof(f->fogblend));
	memset(&f->fogdelta, 0, sizeof(f->fogdelta));
}

static void init_tmu_shared(tmu_shared_state *s)
{
	int val;

	/* build static 8-bit texel tables */
	for (val = 0; val < 256; val++)
	{
		int r;
		int g;
		int b;
		int a;

		/* 8-bit RGB (3-3-2) */
		EXTRACT_332_TO_888(val, r, g, b);
		s->rgb332[val] = MAKE_ARGB(0xff, r, g, b);

		/* 8-bit alpha */
		s->alpha8[val] = MAKE_ARGB(val, val, val, val);

		/* 8-bit intensity */
		s->int8[val] = MAKE_ARGB(0xff, val, val, val);

		/* 8-bit alpha, intensity */
		a = ((val >> 0) & 0xf0) | ((val >> 4) & 0x0f);
		r = ((val << 4) & 0xf0) | ((val << 0) & 0x0f);
		s->ai44[val] = MAKE_ARGB(a, r, r, r);
	}

	/* build static 16-bit texel tables */
	for (val = 0; val < 65536; val++)
	{
		int r;
		int g;
		int b;
		int a;

		/* table 10 = 16-bit RGB (5-6-5) */
		EXTRACT_565_TO_888(val, r, g, b);
		s->rgb565[val] = MAKE_ARGB(0xff, r, g, b);

		/* table 11 = 16 ARGB (1-5-5-5) */
		EXTRACT_1555_TO_8888(val, a, r, g, b);
		s->argb1555[val] = MAKE_ARGB(a, r, g, b);

		/* table 12 = 16-bit ARGB (4-4-4-4) */
		EXTRACT_4444_TO_8888(val, a, r, g, b);
		s->argb4444[val] = MAKE_ARGB(a, r, g, b);
	}
}

static void init_tmu(voodoo_state *vs, tmu_state *t, voodoo_reg *reg, int tmem)
{
	// Sanity check inputs
	assert(vs);
	assert(t);
	assert(reg);
	assert(tmem > 1);

	// Allocate and align the texture RAM to 64-bit, which is the maximum type written
	constexpr auto mem_alignment = sizeof(uint64_t);
	std::tie(t->ram_buffer, t->ram) = make_unique_aligned_array<uint8_t>(mem_alignment, tmem);
	assert(reinterpret_cast<uintptr_t>(t->ram) % mem_alignment == 0);

	t->mask = (uint32_t)(tmem - 1);
	t->reg = reg;
	t->regdirty = true;
	t->bilinear_mask = (vtype >= VOODOO_2) ? 0xff : 0xf0;

	/* mark the NCC tables dirty and configure their registers */
	t->ncc[0].dirty = t->ncc[1].dirty = true;
	t->ncc[0].reg = &t->reg[nccTable+0];
	t->ncc[1].reg = &t->reg[nccTable+12];

	/* create pointers to all the tables */
	t->texel[0] = vs->tmushare.rgb332;
	t->texel[1] = t->ncc[0].texel;
	t->texel[2] = vs->tmushare.alpha8;
	t->texel[3] = vs->tmushare.int8;
	t->texel[4] = vs->tmushare.ai44;
	t->texel[5] = t->palette;
	t->texel[6] = (vtype >= VOODOO_2) ? t->palettea : nullptr;
	t->texel[7] = nullptr;
	t->texel[8] = vs->tmushare.rgb332;
	t->texel[9] = t->ncc[0].texel;
	t->texel[10] = vs->tmushare.rgb565;
	t->texel[11] = vs->tmushare.argb1555;
	t->texel[12] = vs->tmushare.argb4444;
	t->texel[13] = vs->tmushare.int8;
	t->texel[14] = t->palette;
	t->texel[15] = nullptr;
	t->lookup = t->texel[0];

	/* attach the palette to NCC table 0 */
	t->ncc[0].palette = t->palette;
	t->ncc[0].palettea = (vtype >= VOODOO_2) ? t->palettea : nullptr;

	///* set up texture address calculations */
	//t->texaddr_mask = 0x0fffff;
	//t->texaddr_shift = 3;

	t->lodmin=0;
	t->lodmax=0;
}


/*************************************
 *
 *  VBLANK management
 *
 *************************************/

static void voodoo_swap_buffers(voodoo_state *vs)
{
	//if (LOG_VBLANK_SWAP) LOG(LOG_VOODOO,LOG_WARN)("--- swap_buffers @ %d\n", video_screen_get_vpos(vs->screen));

#ifdef C_ENABLE_VOODOO_OPENGL
	if (vs->ogl && vs->active) {
		voodoo_ogl_swap_buffer();
		return;
	}
#endif

	/* keep a history of swap intervals */
	const auto regs = vs->reg;

	regs[fbiSwapHistory].u = (regs[fbiSwapHistory].u << 4);

	/* rotate the buffers */
	auto& fbi = vs->fbi;

	if (vtype < VOODOO_2 || !fbi.vblank_dont_swap) {
		if (fbi.rgboffs[2] == (uint32_t)(~0)) {
			fbi.frontbuf = (uint8_t)(1 - fbi.frontbuf);
			fbi.backbuf  = (uint8_t)(1 - fbi.frontbuf);
		} else {
			fbi.frontbuf = static_cast<uint8_t>((fbi.frontbuf + 1) % 3);
			fbi.backbuf = static_cast<uint8_t>((fbi.frontbuf + 1) % 3);
		}
	}
}


/*************************************
 *
 *  Recompute video memory layout
 *
 *************************************/

static void recompute_video_memory(voodoo_state *vs)
{
	const auto regs = vs->reg;

	const uint32_t buffer_pages = FBIINIT2_VIDEO_BUFFER_OFFSET(regs[fbiInit2].u);

	const uint32_t fifo_start_page = FBIINIT4_MEMORY_FIFO_START_ROW(regs[fbiInit4].u);

	uint32_t fifo_last_page = FBIINIT4_MEMORY_FIFO_STOP_ROW(regs[fbiInit4].u);
	uint32_t memory_config;
	int buf;

	/* memory config is determined differently between V1 and V2 */
	memory_config = FBIINIT2_ENABLE_TRIPLE_BUF(regs[fbiInit2].u);
	if (vtype == VOODOO_2 && memory_config == 0) {
		memory_config = FBIINIT5_BUFFER_ALLOCATION(regs[fbiInit5].u);
	}

	/* tiles are 64x16/32; x_tiles specifies how many half-tiles */
	auto& fbi = vs->fbi;

	fbi.tile_width  = (vtype < VOODOO_2) ? 64 : 32;
	fbi.tile_height = (vtype < VOODOO_2) ? 16 : 32;

	fbi.x_tiles = FBIINIT1_X_VIDEO_TILES(regs[fbiInit1].u);

	if (vtype == VOODOO_2)
	{
		fbi.x_tiles = (fbi.x_tiles << 1) |
		              (FBIINIT1_X_VIDEO_TILES_BIT5(regs[fbiInit1].u) << 5) |
		              (FBIINIT6_X_VIDEO_TILES_BIT0(regs[fbiInit6].u));
	}
	fbi.rowpixels = fbi.tile_width * fbi.x_tiles;

	// logerror("VOODOO.%d.VIDMEM: buffer_pages=%X  fifo=%X-%X  tiles=%X
	// rowpix=%d\n", vs->index, buffer_pages, fifo_start_page,
	// fifo_last_page, fbi.x_tiles, fbi.rowpixels);

	/* first RGB buffer always starts at 0 */
	fbi.rgboffs[0] = 0;

	/* second RGB buffer starts immediately afterwards */
	fbi.rgboffs[1] = buffer_pages * 0x1000;

	/* remaining buffers are based on the config */
	switch (memory_config)
	{
	case 3: /* reserved */
		LOG(LOG_VOODOO,LOG_WARN)("VOODOO.ERROR:Unexpected memory configuration in recompute_video_memory!");
		[[fallthrough]];

	case 0:	/* 2 color buffers, 1 aux buffer */
		fbi.rgboffs[2] = (uint32_t)(~0);
		fbi.auxoffs    = 2 * buffer_pages * 0x1000;
		break;

	case 1:	/* 3 color buffers, 0 aux buffers */
		fbi.rgboffs[2] = 2 * buffer_pages * 0x1000;
		fbi.auxoffs    = (uint32_t)(~0);
		break;

	case 2:	/* 3 color buffers, 1 aux buffers */
		fbi.rgboffs[2] = 2 * buffer_pages * 0x1000;
		fbi.auxoffs    = 3 * buffer_pages * 0x1000;
		break;
	}

	/* clamp the RGB buffers to video memory */
	for (buf = 0; buf < 3; buf++) {
		if (fbi.rgboffs[buf] != (uint32_t)(~0) && fbi.rgboffs[buf] > fbi.mask) {
			fbi.rgboffs[buf] = fbi.mask;
		}
	}

	/* clamp the aux buffer to video memory */
	if (fbi.auxoffs != (uint32_t)(~0) && fbi.auxoffs > fbi.mask) {
		fbi.auxoffs = fbi.mask;
	}

	/* compute the memory FIFO location and size */
	if (fifo_last_page > fbi.mask / 0x1000) {
		fifo_last_page = fbi.mask / 0x1000;
	}

	/* is it valid and enabled? */
	if (fifo_start_page <= fifo_last_page &&
	    FBIINIT0_ENABLE_MEMORY_FIFO(regs[fbiInit0].u)) {
		fbi.fifo.size = (fifo_last_page + 1 - fifo_start_page) * 0x1000 / 4;
		if (fbi.fifo.size > 65536 * 2) {
			fbi.fifo.size = 65536 * 2;
		}
	} else /* if not, disable the FIFO */
	{
		fbi.fifo.size = 0;
	}

	/* reset our front/back buffers if they are out of range */
	if (fbi.rgboffs[2] == (uint32_t)(~0)) {
		if (fbi.frontbuf == 2) {
			fbi.frontbuf = 0;
		}
		if (fbi.backbuf == 2) {
			fbi.backbuf = 0;
		}
	}
}


/*************************************
 *
 *  NCC table management
 *
 *************************************/

static void ncc_table_write(ncc_table *n, uint32_t regnum, uint32_t data)
{
	/* I/Q entries reference the palette if the high bit is set */
	if (regnum >= 4 && ((data & 0x80000000) != 0u) && (n->palette != nullptr))
	{
		const uint32_t index = ((data >> 23) & 0xfe) | (regnum & 1);

		const rgb_t palette_entry = 0xff000000 | data;

		if (n->palette[index] != palette_entry) {
			/* set the ARGB for this palette index */
			n->palette[index] = palette_entry;
#ifdef C_ENABLE_VOODOO_OPENGL
			v->ogl_palette_changed = true;
#endif
		}

		/* if we have an ARGB palette as well, compute its value */
		if (n->palettea != nullptr)
		{
			const uint32_t a = ((data >> 16) & 0xfc) |
			                   ((data >> 22) & 0x03);

			const uint32_t r = ((data >> 10) & 0xfc) |
			                   ((data >> 16) & 0x03);

			const uint32_t g = ((data >> 4) & 0xfc) |
			                   ((data >> 10) & 0x03);

			const uint32_t b = ((data << 2) & 0xfc) |
			                   ((data >> 4) & 0x03);

			n->palettea[index] = MAKE_ARGB(a, r, g, b);
		}

		/* this doesn't dirty the table or go to the registers, so bail */
		return;
	}

	/* if the register matches, don't update */
	if (data == n->reg[regnum].u) {
		return;
	}
	n->reg[regnum].u = data;

	/* first four entries are packed Y values */
	if (regnum < 4)
	{
		regnum *= 4;
		n->y[regnum+0] = (data >>  0) & 0xff;
		n->y[regnum+1] = (data >>  8) & 0xff;
		n->y[regnum+2] = (data >> 16) & 0xff;
		n->y[regnum+3] = (data >> 24) & 0xff;
	}

	/* the second four entries are the I RGB values */
	else if (regnum < 8)
	{
		regnum &= 3;
		n->ir[regnum] = (int32_t)(data <<  5) >> 23;
		n->ig[regnum] = (int32_t)(data << 14) >> 23;
		n->ib[regnum] = (int32_t)(data << 23) >> 23;
	}

	/* the final four entries are the Q RGB values */
	else
	{
		regnum &= 3;
		n->qr[regnum] = (int32_t)(data <<  5) >> 23;
		n->qg[regnum] = (int32_t)(data << 14) >> 23;
		n->qb[regnum] = (int32_t)(data << 23) >> 23;
	}

	/* mark the table dirty */
	n->dirty = true;
}

static void ncc_table_update(ncc_table *n)
{
	int r;
	int g;
	int b;
	int i;

	/* generte all 256 possibilities */
	for (i = 0; i < 256; i++)
	{
		const int vi = (i >> 2) & 0x03;
		const int vq = (i >> 0) & 0x03;

		/* start with the intensity */
		r = g = b = n->y[(i >> 4) & 0x0f];

		/* add the coloring */
		r += n->ir[vi] + n->qr[vq];
		g += n->ig[vi] + n->qg[vq];
		b += n->ib[vi] + n->qb[vq];

		/* clamp */
		r = clamp_to_uint8(r);
		g = clamp_to_uint8(g);
		b = clamp_to_uint8(b);

		/* fill in the table */
		n->texel[i] = MAKE_ARGB(0xff, r, g, b);
	}

	/* no longer dirty */
	n->dirty = false;
}


/*************************************
 *
 *  Faux DAC implementation
 *
 *************************************/

static void dacdata_w(dac_state *d, uint8_t regnum, uint8_t data)
{
	d->reg[regnum] = data;
}

static void dacdata_r(dac_state *d, uint8_t regnum)
{
	uint8_t result = 0xff;

	/* switch off the DAC register requested */
	switch (regnum)
	{
		case 5:
			/* this is just to make startup happy */
			switch (d->reg[7])
			{
				case 0x01:	result = 0x55; break;
				case 0x07:	result = 0x71; break;
				case 0x0b:	result = 0x79; break;
			}
			break;

		default:
			result = d->reg[regnum];
			break;
	}

	/* remember the read result; it is fetched elsewhere */
	d->read_result = result;
}


/*************************************
 *
 *  Texuture parameter computation
 *
 *************************************/

static void recompute_texture_params(tmu_state *t)
{
	int bppscale;
	uint32_t base;
	int lod;

	/* extract LOD parameters */
	t->lodmin = TEXLOD_LODMIN(t->reg[tLOD].u) << 6;
	t->lodmax = TEXLOD_LODMAX(t->reg[tLOD].u) << 6;
	t->lodbias = (int8_t)(TEXLOD_LODBIAS(t->reg[tLOD].u) << 2) << 4;

	/* determine which LODs are present */
	t->lodmask = 0x1ff;
	if (TEXLOD_LOD_TSPLIT(t->reg[tLOD].u))
	{
		if (!TEXLOD_LOD_ODD(t->reg[tLOD].u)) {
			t->lodmask = 0x155;
		} else {
			t->lodmask = 0x0aa;
		}
	}

	/* determine base texture width/height */
	t->wmask = t->hmask = 0xff;
	if (TEXLOD_LOD_S_IS_WIDER(t->reg[tLOD].u)) {
		t->hmask >>= TEXLOD_LOD_ASPECT(t->reg[tLOD].u);
	} else {
		t->wmask >>= TEXLOD_LOD_ASPECT(t->reg[tLOD].u);
	}

	/* determine the bpp of the texture */
	bppscale = TEXMODE_FORMAT(t->reg[textureMode].u) >> 3;

	/* start with the base of LOD 0 */
	if (tmu_state::texaddr_shift == 0 && ((t->reg[texBaseAddr].u & 1) != 0u)) {
		LOG(LOG_VOODOO,LOG_WARN)("Tiled texture\n");
	}
	base = (t->reg[texBaseAddr].u & tmu_state::texaddr_mask) << tmu_state::texaddr_shift;
	t->lodoffset[0] = base & t->mask;

	/* LODs 1-3 are different depending on whether we are in multitex mode */
	/* Several Voodoo 2 games leave the upper bits of TLOD == 0xff, meaning we think */
	/* they want multitex mode when they really don't -- disable for now */
	if (false) // TEXLOD_TMULTIBASEADDR(t->reg[tLOD].u))
	{
		base = (t->reg[texBaseAddr_1].u & tmu_state::texaddr_mask) << tmu_state::texaddr_shift;
		t->lodoffset[1] = base & t->mask;
		base = (t->reg[texBaseAddr_2].u & tmu_state::texaddr_mask) << tmu_state::texaddr_shift;
		t->lodoffset[2] = base & t->mask;
		base = (t->reg[texBaseAddr_3_8].u & tmu_state::texaddr_mask) << tmu_state::texaddr_shift;
		t->lodoffset[3] = base & t->mask;
	} else {
		if ((t->lodmask & (1 << 0)) != 0u) {
			base += (((t->wmask >> 0) + 1) * ((t->hmask >> 0) + 1)) << bppscale;
		}
		t->lodoffset[1] = base & t->mask;
		if ((t->lodmask & (1 << 1)) != 0u) {
			base += (((t->wmask >> 1) + 1) * ((t->hmask >> 1) + 1)) << bppscale;
		}
		t->lodoffset[2] = base & t->mask;
		if ((t->lodmask & (1 << 2)) != 0u) {
			base += (((t->wmask >> 2) + 1) * ((t->hmask >> 2) + 1)) << bppscale;
		}
		t->lodoffset[3] = base & t->mask;
	}

	/* remaining LODs make sense */
	for (lod = 4; lod <= 8; lod++)
	{
		if ((t->lodmask & (1 << (lod - 1))) != 0u)
		{
			uint32_t size = ((t->wmask >> (lod - 1)) + 1) * ((t->hmask >> (lod - 1)) + 1);
			if (size < 4) {
				size = 4;
			}
			base += size << bppscale;
		}
		t->lodoffset[lod] = base & t->mask;
	}

	/* set the NCC lookup appropriately */
	t->texel[1] = t->texel[9] = t->ncc[TEXMODE_NCC_TABLE_SELECT(t->reg[textureMode].u)].texel;

	/* pick the lookup table */
	t->lookup = t->texel[TEXMODE_FORMAT(t->reg[textureMode].u)];

	/* compute the detail parameters */
	t->detailmax = TEXDETAIL_DETAIL_MAX(t->reg[tDetail].u);
	t->detailbias = (int8_t)(TEXDETAIL_DETAIL_BIAS(t->reg[tDetail].u) << 2) << 6;
	t->detailscale = TEXDETAIL_DETAIL_SCALE(t->reg[tDetail].u);

	/* no longer dirty */
	t->regdirty = false;

	/* check for separate RGBA filtering */
	assert(!TEXDETAIL_SEPARATE_RGBA_FILTER(t->reg[tDetail].u));
	//if (TEXDETAIL_SEPARATE_RGBA_FILTER(t->reg[tDetail].u))
	//	E_Exit("Separate RGBA filters!"); // voodoo 2 feature not implemented
}

static void prepare_tmu(tmu_state *t)
{
	int64_t texdx;
	int64_t texdy;
	int32_t lodbase;

	/* if the texture parameters are dirty, update them */
	if (t->regdirty)
	{
		recompute_texture_params(t);

		/* ensure that the NCC tables are up to date */
		if ((TEXMODE_FORMAT(t->reg[textureMode].u) & 7) == 1)
		{
			ncc_table *n = &t->ncc[TEXMODE_NCC_TABLE_SELECT(t->reg[textureMode].u)];
			t->texel[1] = t->texel[9] = n->texel;
			if (n->dirty) {
				ncc_table_update(n);
			}
		}
	}

	/* compute (ds^2 + dt^2) in both X and Y as 28.36 numbers */
	texdx = (t->dsdx >> 14) * (t->dsdx >> 14) + (t->dtdx >> 14) * (t->dtdx >> 14);
	texdy = (t->dsdy >> 14) * (t->dsdy >> 14) + (t->dtdy >> 14) * (t->dtdy >> 14);

	/* pick whichever is larger and shift off some high bits -> 28.20 */
	if (texdx < texdy) {
		texdx = texdy;
	}
	texdx >>= 16;

	/* use our fast reciprocal/log on this value; it expects input as a */
	/* 16.32 number, and returns the log of the reciprocal, so we have to */
	/* adjust the result: negative to get the log of the original value */
	/* plus 12 to account for the extra exponent, and divided by 2 to */
	/* get the log of the square root of texdx */
	(void)fast_reciplog(texdx, &lodbase);
	t->lodbasetemp = (-lodbase + (12 << 8)) / 2;
}

static inline int32_t round_coordinate(float value)
{
	// This is not proper rounding algorithm akin to std::lround (it works
	// incorrectly for values < 0.0); be extremely careful while adjusting
	// it.  Make sure that changes do not result in regression in
	// Build Engine games (Blood, Shadow Warrior).

	const auto rounded = static_cast<int32_t>(value); // round towards 0

	const auto has_remainder = value - static_cast<float>(rounded) > 0.5f;

	return rounded + static_cast<int32_t>(has_remainder);
}

/*************************************
 *
 *  Statistics management
 *
 *************************************/

static void sum_statistics(stats_block *target, const stats_block *source)
{
	target->pixels_in += source->pixels_in;
	target->pixels_out += source->pixels_out;
	target->chroma_fail += source->chroma_fail;
	target->zfunc_fail += source->zfunc_fail;
	target->afunc_fail += source->afunc_fail;
}

static void accumulate_statistics(voodoo_state *vs, const stats_block *stats)
{
	/* apply internal voodoo statistics */
	const auto regs = vs->reg;

	regs[fbiPixelsIn].u += stats->pixels_in;
	regs[fbiPixelsOut].u += stats->pixels_out;
	regs[fbiChromaFail].u += stats->chroma_fail;
	regs[fbiZfuncFail].u += stats->zfunc_fail;
	regs[fbiAfuncFail].u += stats->afunc_fail;
}

static void update_statistics(voodoo_state *vs, bool accumulate)
{
	/* accumulate/reset statistics from all units */
	for (auto& thread_stat : vs->thread_stats) {
		if (accumulate) {
			accumulate_statistics(vs, &thread_stat);
		}
	}
	std::fill(vs->thread_stats.begin(), vs->thread_stats.end(), stats_block());

	/* accumulate/reset statistics from the LFB */
	auto& fbi = vs->fbi;

	if (accumulate) {
		accumulate_statistics(vs, &fbi.lfb_stats);
	}
	fbi.lfb_stats = {};
}

/***************************************************************************
    COMMAND HANDLERS
***************************************************************************/

static void triangle_worker_work(const triangle_worker& tworker,
                                 const int32_t work_start, const int32_t work_end)
{
	/* determine the number of TMUs involved */
	uint32_t tmus     = 0;
	uint32_t texmode0 = 0;
	uint32_t texmode1 = 0;
	if (!FBIINIT3_DISABLE_TMUS(v->reg[fbiInit3].u) && FBZCP_TEXTURE_ENABLE(v->reg[fbzColorPath].u))
	{
		tmus = 1;
		texmode0 = v->tmu[0].reg[textureMode].u;
		if ((v->chipmask & 0x04) != 0)
		{
			tmus = 2;
			texmode1 = v->tmu[1].reg[textureMode].u;
		}
		if (tworker.disable_bilinear_filter) //force disable bilinear filter
		{
			texmode0 &= ~6;
			texmode1 &= ~6;
		}
	}

	/* compute the slopes for each portion of the triangle */
	const poly_vertex v1 = tworker.v1;
	const poly_vertex v2 = tworker.v2;
	const poly_vertex v3 = tworker.v3;

	const float dxdy_v1v2 = (v2.y == v1.y) ? 0.0f
	                                       : (v2.x - v1.x) / (v2.y - v1.y);
	const float dxdy_v1v3 = (v3.y == v1.y) ? 0.0f
	                                       : (v3.x - v1.x) / (v3.y - v1.y);
	const float dxdy_v2v3 = (v3.y == v2.y) ? 0.0f
	                                       : (v3.x - v2.x) / (v3.y - v2.y);

	stats_block my_stats = {};

	// The number of workers represents the total work, while the start and
	// end represent a fraction (up to 100%) of the total total.
	assert(work_end > 0 && tworker.num_work_units >= work_end);

	// The following suppresses div-by-0 false positive reported in Clang
	// analysis. This is confirmed fixed in Clang v18.
	const auto num_work_units = tworker.num_work_units ? tworker.num_work_units : 1;

	const int32_t from = tworker.totalpix * work_start / num_work_units;
	const int32_t to   = tworker.totalpix * work_end / num_work_units;

	for (int32_t curscan = tworker.v1y, scanend = tworker.v3y, sumpix = 0, lastsum = 0;
	     curscan != scanend && lastsum < to;
	     lastsum = sumpix, curscan++) {

		const float fully = (float)(curscan) + 0.5f;

		const float startx = v1.x + (fully - v1.y) * dxdy_v1v3;

		/* compute the ending X based on which part of the triangle we're in */
		const float stopx = (fully < v2.y
		                             ? (v1.x + (fully - v1.y) * dxdy_v1v2)
		                             : (v2.x + (fully - v2.y) * dxdy_v2v3));

		/* clamp to full pixels */
		poly_extent extent;
		extent.startx = round_coordinate(startx);
		extent.stopx = round_coordinate(stopx);

		/* force start < stop */
		if (extent.startx >= extent.stopx)
		{
			if (extent.startx == extent.stopx) {
				continue;
			}
			std::swap(extent.startx, extent.stopx);
		}

		sumpix += (extent.stopx - extent.startx);

		if (sumpix <= from) {
			continue;
		}
		if (lastsum < from) {
			extent.startx += (from - lastsum);
		}
		if (sumpix > to) {
			extent.stopx -= (sumpix - to);
		}

		raster_generic(v, tmus, texmode0, texmode1, tworker.drawbuf, curscan, &extent, my_stats);
	}
	sum_statistics(&v->thread_stats[work_start], &my_stats);
}

// NOTE (weirddan455): In case anyone wants to optimize this further on ARM:
//
// I was conservative with setting memory order on these atomic variables.
// I've set all loads to acquire, stores to release, and load+modify+store to acq_rel.
// x86 gets these semantics essentially for free due to it being a strongly ordered platform.
// On x86, if you ask for relaxed ordering, you'll get aquire/release anyway barring compiler re-ordering.
//
// ARM is weakly ordered though so there could be performance gains by relaxing some of these.
// I can't reliably test for that since I don't have the hardware.
// They can't all be made relaxed and it gets somewhat complicated to determine what you need.
//
// To anyone feeling adventurous, here are some resources:
//
// Rust's atomic guide. Rust uses the same semantics as C++ with regard to memory ordering and is good at grasping the basics.
// https://doc.rust-lang.org/nomicon/atomics.html
//
// cppreference for memory order. This one describes things more thoroughly and formally but is harder to understand:
// https://en.cppreference.com/w/cpp/atomic/memory_order
//
// We shouldn't ever need Sequentially Consistent memory ordering for this use-case.
// That's for edge cases like some lockless multiple producer multiple consumer queues.
//
// Loads should be either acquire or relaxed.
// Stores should be either release or relaxed.
// Fetch+Modify+Store operations (like fetch_add) can be acq_rel, acquire, release, or relaxed.
static int do_triangle_work(triangle_worker& tworker)
{
	// Extra load but this should ensure we don't overflow the index,
	// with the fetch_add below in case of spurious wake-ups.
	int i = tworker.work_index.load(std::memory_order_acquire);
	if (i >= tworker.num_work_units) {
		return i;
	}

	i = tworker.work_index.fetch_add(1, std::memory_order_acq_rel);
	if (i < tworker.num_work_units) {
		triangle_worker_work(tworker, i, i + 1);
		int done = tworker.done_count.fetch_add(1, std::memory_order_acq_rel) + 1;
		if (done >= tworker.num_work_units) {
			tworker.done_count.notify_all();
		}
	}

	// fetch_add returns the previous worker index.
	// We want to return the current.
	return i + 1;
}

static int triangle_worker_thread_func()
{
	triangle_worker& tworker = v->tworker;
	while (tworker.threads_active.load(std::memory_order_acquire)) {
		int i = do_triangle_work(tworker);
		if (i >= tworker.num_work_units) {
			tworker.work_index.wait(i, std::memory_order_acquire);
		}
	}
	return 0;
}

static void triangle_worker_shutdown(triangle_worker& tworker)
{
	if (!tworker.threads_active.load(std::memory_order_acquire)) {
		return;
	}
	tworker.threads_active.store(false, std::memory_order_release);
	tworker.work_index.store(0, std::memory_order_release);
	tworker.work_index.notify_all();

	for (auto& thread : tworker.threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}
}

static void triangle_worker_run(triangle_worker& tworker)
{
	if (!tworker.num_threads) {
		// do not use threaded calculation
		tworker.totalpix = 0xFFFFFFF;
		triangle_worker_work(tworker, 0, tworker.num_work_units);
		return;
	}

	/* compute the slopes for each portion of the triangle */
	const poly_vertex v1 = tworker.v1;
	const poly_vertex v2 = tworker.v2;
	const poly_vertex v3 = tworker.v3;

	const float dxdy_v1v2 = (v2.y == v1.y) ? 0.0f
	                                       : (v2.x - v1.x) / (v2.y - v1.y);
	const float dxdy_v1v3 = (v3.y == v1.y) ? 0.0f
	                                       : (v3.x - v1.x) / (v3.y - v1.y);
	const float dxdy_v2v3 = (v3.y == v2.y) ? 0.0f
	                                       : (v3.x - v2.x) / (v3.y - v2.y);

	int32_t pixsum = 0;
	for (int32_t curscan = tworker.v1y, scanend = tworker.v3y; curscan != scanend; curscan++)
	{
		const float fully  = (float)(curscan) + 0.5f;
		const float startx = v1.x + (fully - v1.y) * dxdy_v1v3;

		/* compute the ending X based on which part of the triangle we're in */
		const float stopx = (fully < v2.y
		                             ? (v1.x + (fully - v1.y) * dxdy_v1v2)
		                             : (v2.x + (fully - v2.y) * dxdy_v2v3));

		/* clamp to full pixels */
		const int32_t istartx = round_coordinate(startx);
		const int32_t istopx  = round_coordinate(stopx);

		/* force start < stop */
		pixsum += (istartx > istopx ? istartx - istopx : istopx - istartx);
	}
	tworker.totalpix = pixsum;

	// Don't wake up threads for just a few pixels
	if (tworker.totalpix <= 200)
	{
		triangle_worker_work(tworker, 0, tworker.num_work_units);
		return;
	}

	// The main thread is the only one who sets threads_active (here and in shutdown) so there is no race condition.
	// In the future, if this changes, this will need to be an atomic compare_exchange.
	// For now, this is better because 99% of the time threads_active == true.
	// We only spin up the threads once and a load is much faster than a compare_exchange.
	if (!tworker.threads_active.load(std::memory_order_acquire))
	{
		tworker.threads_active.store(true, std::memory_order_release);

		for (auto& triangle_worker : tworker.threads) {
			triangle_worker = std::thread([] {
				triangle_worker_thread_func();
			});
		}
	}

	tworker.done_count.store(0, std::memory_order_release);

	// Reseting this index triggers the worker threads to start working
	tworker.work_index.store(0, std::memory_order_release);
	tworker.work_index.notify_all();

	// Main thread also does the same work as the worker threads
	while (do_triangle_work(tworker) < tworker.num_work_units);

	// Wait until all work has been completed by the worker thread.
	int i;
	while ((i = tworker.done_count.load(std::memory_order_acquire)) < tworker.num_work_units) {
		tworker.done_count.wait(i, std::memory_order_acquire);
	}
}

/*-------------------------------------------------
    triangle - execute the 'triangle'
    command
-------------------------------------------------*/
static void triangle(voodoo_state *vs)
{
	// Quick references
	const auto regs = vs->reg;
	auto& fbi = vs->fbi;
	auto& tmu0 = vs->tmu[0];
	auto& tmu1 = vs->tmu[1];

	/* determine the number of TMUs involved */
	int texcount = 0;
	if (!FBIINIT3_DISABLE_TMUS(regs[fbiInit3].u) &&
	    FBZCP_TEXTURE_ENABLE(regs[fbzColorPath].u)) {
		texcount = 1;
		if ((vs->chipmask & 0x04) != 0) {
			texcount = 2;
		}
	}

	/* perform subpixel adjustments */
	if (
#ifdef C_ENABLE_VOODOO_OPENGL
	        !vs->ogl &&
#endif
	    FBZCP_CCA_SUBPIXEL_ADJUST(regs[fbzColorPath].u)) {
		const int32_t dx = 8 - (fbi.ax & 15);
		const int32_t dy = 8 - (fbi.ay & 15);

		/* adjust iterated R,G,B,A and W/Z */
		fbi.startr += (dy * fbi.drdy + dx * fbi.drdx) >> 4;
		fbi.startg += (dy * fbi.dgdy + dx * fbi.dgdx) >> 4;
		fbi.startb += (dy * fbi.dbdy + dx * fbi.dbdx) >> 4;
		fbi.starta += (dy * fbi.dady + dx * fbi.dadx) >> 4;
		fbi.startw += (dy * fbi.dwdy + dx * fbi.dwdx) >> 4;
		fbi.startz += mul_32x32_shift(dy, fbi.dzdy, 4) +
		              mul_32x32_shift(dx, fbi.dzdx, 4);

		/* adjust iterated W/S/T for TMU 0 */
		if (texcount >= 1)
		{
			tmu0.startw += (dy * tmu0.dwdy + dx * tmu0.dwdx) >> 4;
			tmu0.starts += (dy * tmu0.dsdy + dx * tmu0.dsdx) >> 4;
			tmu0.startt += (dy * tmu0.dtdy + dx * tmu0.dtdx) >> 4;

			/* adjust iterated W/S/T for TMU 1 */
			if (texcount >= 2)
			{
				tmu1.startw += (dy * tmu1.dwdy + dx * tmu1.dwdx) >> 4;
				tmu1.starts += (dy * tmu1.dsdy + dx * tmu1.dsdx) >> 4;
				tmu1.startt += (dy * tmu1.dtdy + dx * tmu1.dtdx) >> 4;
			}
		}
	}

	/* fill in the vertex data */
	poly_vertex vert[3];
	vert[0].x = (float)fbi.ax * (1.0f / 16.0f);
	vert[0].y = (float)fbi.ay * (1.0f / 16.0f);
	vert[1].x = (float)fbi.bx * (1.0f / 16.0f);
	vert[1].y = (float)fbi.by * (1.0f / 16.0f);
	vert[2].x = (float)fbi.cx * (1.0f / 16.0f);
	vert[2].y = (float)fbi.cy * (1.0f / 16.0f);

#ifdef C_ENABLE_VOODOO_OPENGL
	/* find a rasterizer that matches our current state */
	//    setup and create the work item
	poly_extra_data extra;
	/* fill in the extra data */
	extra.state = vs;

	raster_info *info  = find_rasterizer(vs, texcount);
	extra.info = info;

	/* fill in triangle parameters */
	extra.ax     = fbi.ax;
	extra.ay     = fbi.ay;
	extra.startr = fbi.startr;
	extra.startg = fbi.startg;
	extra.startb = fbi.startb;
	extra.starta = fbi.starta;
	extra.startz = fbi.startz;
	extra.startw = fbi.startw;
	extra.drdx   = fbi.drdx;
	extra.dgdx   = fbi.dgdx;
	extra.dbdx   = fbi.dbdx;
	extra.dadx   = fbi.dadx;
	extra.dzdx   = fbi.dzdx;
	extra.dwdx   = fbi.dwdx;
	extra.drdy   = fbi.drdy;
	extra.dgdy   = fbi.dgdy;
	extra.dbdy   = fbi.dbdy;
	extra.dady   = fbi.dady;
	extra.dzdy   = fbi.dzdy;
	extra.dwdy   = fbi.dwdy;

	/* fill in texture 0 parameters */
	if (texcount > 0)
	{
		extra.starts0  = tmu0.starts;
		extra.startt0  = tmu0.startt;
		extra.startw0  = tmu0.startw;
		extra.ds0dx    = tmu0.dsdx;
		extra.dt0dx    = tmu0.dtdx;
		extra.dw0dx    = tmu0.dwdx;
		extra.ds0dy    = tmu0.dsdy;
		extra.dt0dy    = tmu0.dtdy;
		extra.dw0dy    = tmu0.dwdy;
		extra.lodbase0 = tmu0.lodbasetemp;

		/* fill in texture 1 parameters */
		if (texcount > 1)
		{
			extra.starts1  = tmu1.starts;
			extra.startt1  = tmu1.startt;
			extra.startw1  = tmu1.startw;
			extra.ds1dx    = tmu1.dsdx;
			extra.dt1dx    = tmu1.dtdx;
			extra.dw1dx    = tmu1.dwdx;
			extra.ds1dy    = tmu1.dsdy;
			extra.dt1dy    = tmu1.dtdy;
			extra.dw1dy    = tmu1.dwdy;
			extra.lodbase1 = tmu1.lodbasetemp;
		}
	}

	extra.texcount = texcount;
	extra.r_fbzColorPath = regs[fbzColorPath].u;
	extra.r_fbzMode      = regs[fbzMode].u;
	extra.r_alphaMode    = regs[alphaMode].u;
	extra.r_fogMode      = regs[fogMode].u;

	extra.r_textureMode0 = tmu0.reg[textureMode].u;
	if (tmu1.ram != nullptr) {
		extra.r_textureMode1 = tmu1.reg[textureMode].u;
	}

#ifdef C_ENABLE_VOODOO_DEBUG
	info->polys++;
#endif

	if (vs->ogl_palette_changed && vs->ogl && vs->active) {
		voodoo_ogl_invalidate_paltex();
		vs->ogl_palette_changed = false;
	}

	if (vs->ogl && vs->active) {
		if (extra.info)
			voodoo_ogl_draw_triangle(&extra);
		return;
	}
#endif

	/* first sort by Y */
	const poly_vertex* v1 = &vert[0];
	const poly_vertex* v2 = &vert[1];
	const poly_vertex* v3 = &vert[2];
	if (v2->y < v1->y) {
		std::swap(v1, v2);
	}
	if (v3->y < v2->y)
	{
		std::swap(v2, v3);
		if (v2->y < v1->y) {
			std::swap(v1, v2);
		}
	}

	/* compute some integral X/Y vertex values */
	const int32_t v1y = round_coordinate(v1->y);
	const int32_t v3y = round_coordinate(v3->y);

	/* clip coordinates */
	if (v3y <= v1y) {
		return;
	}

	/* determine the draw buffer */
	uint16_t *drawbuf;
	switch (FBZMODE_DRAW_BUFFER(regs[fbzMode].u)) {
	case 0: /* front buffer */
		drawbuf = (uint16_t*)(fbi.ram + fbi.rgboffs[fbi.frontbuf]);
		break;

	case 1: /* back buffer */
		drawbuf = (uint16_t*)(fbi.ram + fbi.rgboffs[fbi.backbuf]);
		break;

	default: /* reserved */ return;
	}

	/* determine the number of TMUs involved */
	if (texcount >= 1)
	{
		prepare_tmu(&tmu0);
		if (texcount >= 2) {
			prepare_tmu(&tmu1);
		}
	}

	triangle_worker& tworker = vs->tworker;
	tworker.v1 = *v1, tworker.v2 = *v2, tworker.v3 = *v3;
	tworker.drawbuf = drawbuf;
	tworker.v1y = v1y;
	tworker.v3y = v3y;
	triangle_worker_run(tworker);

	/* update stats */
	regs[fbiTrianglesOut].u++;
}

/*-------------------------------------------------
    begin_triangle - execute the 'beginTri'
    command
-------------------------------------------------*/
static void begin_triangle(voodoo_state *vs)
{
	// Quick references
	const auto regs = vs->reg;
	auto& fbi = vs->fbi;

	setup_vertex* sv = &fbi.svert[2];

	/* extract all the data from registers */

	sv->x  = regs[sVx].f;
	sv->y  = regs[sVy].f;
	sv->wb = regs[sWb].f;
	sv->w0 = regs[sWtmu0].f;
	sv->s0 = regs[sS_W0].f;
	sv->t0 = regs[sT_W0].f;
	sv->w1 = regs[sWtmu1].f;
	sv->s1 = regs[sS_Wtmu1].f;
	sv->t1 = regs[sT_Wtmu1].f;
	sv->a  = regs[sAlpha].f;
	sv->r  = regs[sRed].f;
	sv->g  = regs[sGreen].f;
	sv->b  = regs[sBlue].f;

	/* spread it across all three verts and reset the count */
	fbi.svert[0] = fbi.svert[1] = fbi.svert[2];
	fbi.sverts = 1;
}

/*-------------------------------------------------
    setup_and_draw_triangle - process the setup
    parameters and render the triangle
-------------------------------------------------*/
static void setup_and_draw_triangle(voodoo_state *vs)
{
	// Quick references
	const auto regs = vs->reg;
	auto& fbi = vs->fbi;
	auto& tmu0 = vs->tmu[0];
	auto& tmu1 = vs->tmu[1];

	// Vertex references
	const auto& vertex0 = fbi.svert[0];
	const auto& vertex1 = fbi.svert[1];
	const auto& vertex2 = fbi.svert[2];

	/* grab the X/Ys at least */
	fbi.ax = (int16_t)(vertex0.x * 16.0f);
	fbi.ay = (int16_t)(vertex0.y * 16.0f);
	fbi.bx = (int16_t)(vertex1.x * 16.0f);
	fbi.by = (int16_t)(vertex1.y * 16.0f);
	fbi.cx = (int16_t)(vertex2.x * 16.0f);
	fbi.cy = (int16_t)(vertex2.y * 16.0f);

	/* compute the divisor */
	const auto divisor = 1.0f /
	                     ((vertex0.x - vertex1.x) * (vertex0.y - vertex2.y) -
	                      (vertex0.x - vertex2.x) * (vertex0.y - vertex1.y));

	/* backface culling */
	if ((regs[sSetupMode].u & 0x20000) != 0u) {
		int culling_sign = (regs[sSetupMode].u >> 18) & 1;
		const auto divisor_sign = static_cast<int>(divisor < 0);

		/* if doing strips and ping pong is enabled, apply the ping pong */
		if ((regs[sSetupMode].u & 0x90000) == 0x00000) {
			culling_sign ^= (fbi.sverts - 3) & 1;
		}

		/* if our sign matches the culling sign, we're done for */
		if (divisor_sign == culling_sign) {
			return;
		}
	}

	/* compute the dx/dy values */
	const auto dx1 = vertex0.y - vertex2.y;
	const auto dx2 = vertex0.y - vertex1.y;
	const auto dy1 = vertex0.x - vertex1.x;
	const auto dy2 = vertex0.x - vertex2.x;

	/* set up R,G,B */
	auto tdiv = divisor * 4096.0f;
	if ((regs[sSetupMode].u & (1 << 0)) != 0u) {
		fbi.startr = (int32_t)(vertex0.r * 4096.0f);
		fbi.drdx   = (int32_t)(((vertex0.r - vertex1.r) * dx1 -
                                      (vertex0.r - vertex2.r) * dx2) *
                                     tdiv);
		fbi.drdy   = (int32_t)(((vertex0.r - vertex2.r) * dy1 -
                                      (vertex0.r - vertex1.r) * dy2) *
                                     tdiv);
		fbi.startg = (int32_t)(vertex0.g * 4096.0f);
		fbi.dgdx   = (int32_t)(((vertex0.g - vertex1.g) * dx1 -
                                      (vertex0.g - vertex2.g) * dx2) *
                                     tdiv);
		fbi.dgdy   = (int32_t)(((vertex0.g - vertex2.g) * dy1 -
                                      (vertex0.g - vertex1.g) * dy2) *
                                     tdiv);
		fbi.startb = (int32_t)(vertex0.b * 4096.0f);
		fbi.dbdx   = (int32_t)(((vertex0.b - vertex1.b) * dx1 -
                                      (vertex0.b - vertex2.b) * dx2) *
                                     tdiv);
		fbi.dbdy   = (int32_t)(((vertex0.b - vertex2.b) * dy1 -
                                      (vertex0.b - vertex1.b) * dy2) *
                                     tdiv);
	}

	/* set up alpha */
	if ((regs[sSetupMode].u & (1 << 1)) != 0u) {
		fbi.starta = (int32_t)(vertex0.a * 4096.0f);
		fbi.dadx   = (int32_t)(((vertex0.a - vertex1.a) * dx1 -
                                      (vertex0.a - vertex2.a) * dx2) *
                                     tdiv);
		fbi.dady   = (int32_t)(((vertex0.a - vertex2.a) * dy1 -
                                      (vertex0.a - vertex1.a) * dy2) *
                                     tdiv);
	}

	/* set up Z */
	if ((regs[sSetupMode].u & (1 << 2)) != 0u) {
		fbi.startz = (int32_t)(vertex0.z * 4096.0f);
		fbi.dzdx   = (int32_t)(((vertex0.z - vertex1.z) * dx1 -
                                      (vertex0.z - vertex2.z) * dx2) *
                                     tdiv);
		fbi.dzdy   = (int32_t)(((vertex0.z - vertex2.z) * dy1 -
                                      (vertex0.z - vertex1.z) * dy2) *
                                     tdiv);
	}

	/* set up Wb */
	tdiv = divisor * 65536.0f * 65536.0f;
	if ((regs[sSetupMode].u & (1 << 3)) != 0u) {
		fbi.startw = tmu0.startw = tmu1.startw = (int64_t)(vertex0.wb * 65536.0f *
		                                                   65536.0f);
		fbi.dwdx = tmu0.dwdx = tmu1.dwdx =
		        (int64_t)(((vertex0.wb - vertex1.wb) * dx1 -
		                   (vertex0.wb - vertex2.wb) * dx2) *
		                  tdiv);
		fbi.dwdy = tmu0.dwdy = tmu1.dwdy =
		        (int64_t)(((vertex0.wb - vertex2.wb) * dy1 -
		                   (vertex0.wb - vertex1.wb) * dy2) *
		                  tdiv);
	}

	/* set up W0 */
	if ((regs[sSetupMode].u & (1 << 4)) != 0u) {
		tmu0.startw = tmu1.startw = (int64_t)(vertex0.w0 * 65536.0f * 65536.0f);
		tmu0.dwdx = tmu1.dwdx = (int64_t)(((vertex0.w0 - vertex1.w0) * dx1 -
		                                   (vertex0.w0 - vertex2.w0) * dx2) *
		                                  tdiv);
		tmu0.dwdy = tmu1.dwdy = (int64_t)(((vertex0.w0 - vertex2.w0) * dy1 -
		                                   (vertex0.w0 - vertex1.w0) * dy2) *
		                                  tdiv);
	}

	/* set up S0,T0 */
	if ((regs[sSetupMode].u & (1 << 5)) != 0u) {
		tmu0.starts = tmu1.starts = (int64_t)(vertex0.s0 * 65536.0f * 65536.0f);
		tmu0.dsdx = tmu1.dsdx = (int64_t)(((vertex0.s0 - vertex1.s0) * dx1 -
		                                   (vertex0.s0 - vertex2.s0) * dx2) *
		                                  tdiv);
		tmu0.dsdy = tmu1.dsdy = (int64_t)(((vertex0.s0 - vertex2.s0) * dy1 -
		                                   (vertex0.s0 - vertex1.s0) * dy2) *
		                                  tdiv);
		tmu0.startt = tmu1.startt = (int64_t)(vertex0.t0 * 65536.0f * 65536.0f);
		tmu0.dtdx = tmu1.dtdx = (int64_t)(((vertex0.t0 - vertex1.t0) * dx1 -
		                                   (vertex0.t0 - vertex2.t0) * dx2) *
		                                  tdiv);
		tmu0.dtdy = tmu1.dtdy = (int64_t)(((vertex0.t0 - vertex2.t0) * dy1 -
		                                   (vertex0.t0 - vertex1.t0) * dy2) *
		                                  tdiv);
	}

	/* set up W1 */
	if ((regs[sSetupMode].u & (1 << 6)) != 0u) {
		tmu1.startw = (int64_t)(vertex0.w1 * 65536.0f * 65536.0f);
		tmu1.dwdx   = (int64_t)(((vertex0.w1 - vertex1.w1) * dx1 -
                                       (vertex0.w1 - vertex2.w1) * dx2) *
                                      tdiv);
		tmu1.dwdy   = (int64_t)(((vertex0.w1 - vertex2.w1) * dy1 -
                                       (vertex0.w1 - vertex1.w1) * dy2) *
                                      tdiv);
	}

	/* set up S1,T1 */
	if ((regs[sSetupMode].u & (1 << 7)) != 0u) {
		tmu1.starts = (int64_t)(vertex0.s1 * 65536.0f * 65536.0f);
		tmu1.dsdx   = (int64_t)(((vertex0.s1 - vertex1.s1) * dx1 -
                                       (vertex0.s1 - vertex2.s1) * dx2) *
                                      tdiv);
		tmu1.dsdy   = (int64_t)(((vertex0.s1 - vertex2.s1) * dy1 -
                                       (vertex0.s1 - vertex1.s1) * dy2) *
                                      tdiv);
		tmu1.startt = (int64_t)(vertex0.t1 * 65536.0f * 65536.0f);
		tmu1.dtdx   = (int64_t)(((vertex0.t1 - vertex1.t1) * dx1 -
                                       (vertex0.t1 - vertex2.t1) * dx2) *
                                      tdiv);
		tmu1.dtdy   = (int64_t)(((vertex0.t1 - vertex2.t1) * dy1 -
                                       (vertex0.t1 - vertex1.t1) * dy2) *
                                      tdiv);
	}

	/* draw the triangle */
	triangle(vs);
}

/*-------------------------------------------------
    draw_triangle - execute the 'DrawTri'
    command
-------------------------------------------------*/
static void draw_triangle(voodoo_state *vs)
{
	const auto regs = vs->reg;
	auto& fbi = vs->fbi;

	setup_vertex* sv = &fbi.svert[2];

	/* for strip mode, shuffle vertex 1 down to 0 */
	if ((regs[sSetupMode].u & (1 << 16)) == 0u) {
		fbi.svert[0] = fbi.svert[1];
	}

	/* copy 2 down to 1 regardless */
	fbi.svert[1] = fbi.svert[2];

	/* extract all the data from registers */
	sv->x  = regs[sVx].f;
	sv->y  = regs[sVy].f;
	sv->wb = regs[sWb].f;
	sv->w0 = regs[sWtmu0].f;
	sv->s0 = regs[sS_W0].f;
	sv->t0 = regs[sT_W0].f;
	sv->w1 = regs[sWtmu1].f;
	sv->s1 = regs[sS_Wtmu1].f;
	sv->t1 = regs[sT_Wtmu1].f;
	sv->a  = regs[sAlpha].f;
	sv->r  = regs[sRed].f;
	sv->g  = regs[sGreen].f;
	sv->b  = regs[sBlue].f;

	/* if we have enough verts, go ahead and draw */
	if (++fbi.sverts >= 3) {
		setup_and_draw_triangle(vs);
	}
}

/*-------------------------------------------------
    fastfill - execute the 'fastfill'
    command
-------------------------------------------------*/
static void fastfill(voodoo_state *vs)
{
	const auto regs = vs->reg;

	const int sx = (regs[clipLeftRight].u >> 16) & 0x3ff;
	const int ex = (regs[clipLeftRight].u >> 0) & 0x3ff;
	const int sy = (regs[clipLowYHighY].u >> 16) & 0x3ff;
	const int ey = (regs[clipLowYHighY].u >> 0) & 0x3ff;

	poly_extent extents[64] = {};

	// Align to 64-bit because that's the maximum type written
	alignas(sizeof(uint64_t)) static uint16_t dithermatrix[16] = {};

	uint16_t* drawbuf = nullptr;
	int x;
	int y;

	/* if we're not clearing either, take no time */
	if (!FBZMODE_RGB_BUFFER_MASK(regs[fbzMode].u) &&
	    !FBZMODE_AUX_BUFFER_MASK(regs[fbzMode].u)) {
		return;
	}

	/* are we clearing the RGB buffer? */
	if (FBZMODE_RGB_BUFFER_MASK(regs[fbzMode].u)) {
		/* determine the draw buffer */
		const int destbuf = FBZMODE_DRAW_BUFFER(regs[fbzMode].u);
		switch (destbuf)
		{
			case 0:		/* front buffer */
				drawbuf = (uint16_t *)(vs->fbi.ram + vs->fbi.rgboffs[vs->fbi.frontbuf]);
				break;

			case 1:		/* back buffer */
				drawbuf = (uint16_t *)(vs->fbi.ram + vs->fbi.rgboffs[vs->fbi.backbuf]);
				break;

			default:	/* reserved */
				break;
		}

		/* determine the dither pattern */
		for (y = 0; y < 4; y++)
		{
			const uint8_t* dither_lookup = nullptr;
			const uint8_t* dither4       = nullptr;

			[[maybe_unused]] const uint8_t* dither = nullptr;

			COMPUTE_DITHER_POINTERS(regs[fbzMode].u, y);
			for (x = 0; x < 4; x++)
			{
				int r = regs[color1].rgb.r;
				int g = regs[color1].rgb.g;
				int b = regs[color1].rgb.b;

				APPLY_DITHER(regs[fbzMode].u, x, dither_lookup, r, g, b);
				dithermatrix[y*4 + x] = (uint16_t)((r << 11) | (g << 5) | b);
			}
		}
	}

	/* fill in a block of extents */
	extents[0].startx = sx;
	extents[0].stopx = ex;

	constexpr auto num_extents = static_cast<int>(ARRAY_LEN(extents));

	for (auto extnum = 1; extnum < num_extents; ++extnum) {
		extents[extnum] = extents[0];
	}

#ifdef C_ENABLE_VOODOO_OPENGL
	if (vs->ogl && vs->active) {
		voodoo_ogl_fastfill();
		return;
	}
#endif

	/* iterate over blocks of extents */
	for (y = sy; y < ey; y += num_extents) {
		const auto count = std::min<int>(ey - y, num_extents);
		void *dest = drawbuf;

		const int startscanline = y;
		const int numscanlines  = count;

		const int32_t v1yclip = startscanline;
		const int32_t v3yclip = startscanline + numscanlines;

		if (v3yclip - v1yclip <= 0) {
			return;
		}

		for (int32_t curscan = v1yclip; curscan < v3yclip; curscan++)
		{
			auto extent = &extents[curscan - startscanline];

			/* force start < stop */
			if (extent->startx > extent->stopx) {
				std::swap(extent->startx, extent->stopx);
			}

			/* set the extent and update the total pixel count */
			raster_fastfill(dest,curscan,extent,dithermatrix);
		}
	}
}

/*-------------------------------------------------
    swapbuffer - execute the 'swapbuffer'
    command
-------------------------------------------------*/
static void swapbuffer(voodoo_state *vs, uint32_t data)
{
	/* set the don't swap value for Voodoo 2 */
	vs->fbi.vblank_dont_swap = ((data >> 9) & 1)>0;

	voodoo_swap_buffers(vs);
}


/*************************************
 *
 *  Chip reset
 *
 *************************************/

static void reset_counters(voodoo_state *vs)
{
	update_statistics(vs, false);
	const auto regs = vs->reg;

	regs[fbiPixelsIn].u   = 0;
	regs[fbiChromaFail].u = 0;
	regs[fbiZfuncFail].u  = 0;
	regs[fbiAfuncFail].u  = 0;
	regs[fbiPixelsOut].u  = 0;
}

static void soft_reset(voodoo_state *vs)
{
	reset_counters(vs);
	vs->reg[fbiTrianglesOut].u = 0;
}


/*************************************
 *
 *  Voodoo register writes
 *
 *************************************/
static void register_w(uint32_t offset, uint32_t data)
{
	auto chips = check_cast<uint8_t>((offset >> 8) & 0xf);

	int64_t data64;

	//LOG(LOG_VOODOO,LOG_WARN)("V3D:WR chip %x reg %x value %08x(%s)", chips, regnum<<2, data, voodoo_reg_name[regnum]);

	if (chips == 0) {
		chips = 0xf;
	}
	chips &= v->chipmask;

	/* the first 64 registers can be aliased differently */
	const auto is_aliased = (offset & 0x800c0) == 0x80000 && v->alt_regmap;

	const auto regnum = is_aliased ? register_alias_map[offset & 0x3f]
	                               : static_cast<uint8_t>(offset & 0xff);

	/* first make sure this register is readable */
	if ((v->regaccess[regnum] & REGISTER_WRITE) == 0)
	{
#ifdef C_ENABLE_VOODOO_DEBUG
		if (regnum <= 0xe0) LOG(LOG_VOODOO,LOG_WARN)("VOODOO.ERROR:Invalid attempt to write %s\n", v->regnames[regnum]);
		else
#endif
		LOG(LOG_VOODOO,LOG_WARN)("VOODOO.ERROR:Invalid attempt to write #%x\n", regnum);
		return;
	}

	/* switch off the register */
	switch (regnum)
	{
		/* Vertex data is 12.4 formatted fixed point */
		case fvertexAx:
			data = float_to_int32(data, 4);
			[[fallthrough]];
		case vertexAx:
			if ((chips & 1) != 0) {
				v->fbi.ax = (int16_t)(data & 0xffff);
			}
			break;

		case fvertexAy:
			data = float_to_int32(data, 4);
			[[fallthrough]];
		case vertexAy:
			if ((chips & 1) != 0) {
				v->fbi.ay = (int16_t)(data & 0xffff);
			}
			break;

		case fvertexBx:
			data = float_to_int32(data, 4);
			[[fallthrough]];
		case vertexBx:
			if ((chips & 1) != 0) {
				v->fbi.bx = (int16_t)(data & 0xffff);
			}
			break;

		case fvertexBy:
			data = float_to_int32(data, 4);
			[[fallthrough]];
		case vertexBy:
			if ((chips & 1) != 0) {
				v->fbi.by = (int16_t)(data & 0xffff);
			}
			break;

		case fvertexCx:
			data = float_to_int32(data, 4);
			[[fallthrough]];
		case vertexCx:
			if ((chips & 1) != 0) {
				v->fbi.cx = (int16_t)(data & 0xffff);
			}
			break;

		case fvertexCy:
			data = float_to_int32(data, 4);
			[[fallthrough]];
		case vertexCy:
			if ((chips & 1) != 0) {
				v->fbi.cy = (int16_t)(data & 0xffff);
			}
			break;

		/* RGB data is 12.12 formatted fixed point */
		case fstartR:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case startR:
			if ((chips & 1) != 0) {
				v->fbi.startr = (int32_t)(data << 8) >> 8;
			}
			break;

		case fstartG:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case startG:
			if ((chips & 1) != 0) {
				v->fbi.startg = (int32_t)(data << 8) >> 8;
			}
			break;

		case fstartB:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case startB:
			if ((chips & 1) != 0) {
				v->fbi.startb = (int32_t)(data << 8) >> 8;
			}
			break;

		case fstartA:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case startA:
			if ((chips & 1) != 0) {
				v->fbi.starta = (int32_t)(data << 8) >> 8;
			}
			break;

		case fdRdX:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dRdX:
			if ((chips & 1) != 0) {
				v->fbi.drdx = (int32_t)(data << 8) >> 8;
			}
			break;

		case fdGdX:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dGdX:
			if ((chips & 1) != 0) {
				v->fbi.dgdx = (int32_t)(data << 8) >> 8;
			}
			break;

		case fdBdX:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dBdX:
			if ((chips & 1) != 0) {
				v->fbi.dbdx = (int32_t)(data << 8) >> 8;
			}
			break;

		case fdAdX:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dAdX:
			if ((chips & 1) != 0) {
				v->fbi.dadx = (int32_t)(data << 8) >> 8;
			}
			break;

		case fdRdY:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dRdY:
			if ((chips & 1) != 0) {
				v->fbi.drdy = (int32_t)(data << 8) >> 8;
			}
			break;

		case fdGdY:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dGdY:
			if ((chips & 1) != 0) {
				v->fbi.dgdy = (int32_t)(data << 8) >> 8;
			}
			break;

		case fdBdY:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dBdY:
			if ((chips & 1) != 0) {
				v->fbi.dbdy = (int32_t)(data << 8) >> 8;
			}
			break;

		case fdAdY:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dAdY:
			if ((chips & 1) != 0) {
				v->fbi.dady = (int32_t)(data << 8) >> 8;
			}
			break;

		/* Z data is 20.12 formatted fixed point */
		case fstartZ:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case startZ:
			if ((chips & 1) != 0) {
				v->fbi.startz = (int32_t)data;
			}
			break;

		case fdZdX:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dZdX:
			if ((chips & 1) != 0) {
				v->fbi.dzdx = (int32_t)data;
			}
			break;

		case fdZdY:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dZdY:
			if ((chips & 1) != 0) {
				v->fbi.dzdy = (int32_t)data;
			}
			break;

		/* S,T data is 14.18 formatted fixed point, converted to 16.32 internally */
		case fstartS:
			data64 = float_to_int64(data, 32);
			if ((chips & 2) != 0) {
				v->tmu[0].starts = data64;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].starts = data64;
			}
			break;
		case startS:
			if ((chips & 2) != 0) {
				v->tmu[0].starts = (int64_t)(int32_t)data << 14;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].starts = (int64_t)(int32_t)data << 14;
			}
			break;

		case fstartT:
			data64 = float_to_int64(data, 32);
			if ((chips & 2) != 0) {
				v->tmu[0].startt = data64;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].startt = data64;
			}
			break;
		case startT:
			if ((chips & 2) != 0) {
				v->tmu[0].startt = (int64_t)(int32_t)data << 14;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].startt = (int64_t)(int32_t)data << 14;
			}
			break;

		case fdSdX:
			data64 = float_to_int64(data, 32);
			if ((chips & 2) != 0) {
				v->tmu[0].dsdx = data64;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].dsdx = data64;
			}
			break;
		case dSdX:
			if ((chips & 2) != 0) {
				v->tmu[0].dsdx = (int64_t)(int32_t)data << 14;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].dsdx = (int64_t)(int32_t)data << 14;
			}
			break;

		case fdTdX:
			data64 = float_to_int64(data, 32);
			if ((chips & 2) != 0) {
				v->tmu[0].dtdx = data64;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].dtdx = data64;
			}
			break;
		case dTdX:
			if ((chips & 2) != 0) {
				v->tmu[0].dtdx = (int64_t)(int32_t)data << 14;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].dtdx = (int64_t)(int32_t)data << 14;
			}
			break;

		case fdSdY:
			data64 = float_to_int64(data, 32);
			if ((chips & 2) != 0) {
				v->tmu[0].dsdy = data64;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].dsdy = data64;
			}
			break;
		case dSdY:
			if ((chips & 2) != 0) {
				v->tmu[0].dsdy = (int64_t)(int32_t)data << 14;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].dsdy = (int64_t)(int32_t)data << 14;
			}
			break;

		case fdTdY:
			data64 = float_to_int64(data, 32);
			if ((chips & 2) != 0) {
				v->tmu[0].dtdy = data64;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].dtdy = data64;
			}
			break;
		case dTdY:
			if ((chips & 2) != 0) {
				v->tmu[0].dtdy = (int64_t)(int32_t)data << 14;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].dtdy = (int64_t)(int32_t)data << 14;
			}
			break;

		/* W data is 2.30 formatted fixed point, converted to 16.32 internally */
		case fstartW:
			data64 = float_to_int64(data, 32);
			if ((chips & 1) != 0) {
				v->fbi.startw = data64;
			}
			if ((chips & 2) != 0) {
				v->tmu[0].startw = data64;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].startw = data64;
			}
			break;
		case startW:
			if ((chips & 1) != 0) {
				v->fbi.startw = (int64_t)(int32_t)data << 2;
			}
			if ((chips & 2) != 0) {
				v->tmu[0].startw = (int64_t)(int32_t)data << 2;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].startw = (int64_t)(int32_t)data << 2;
			}
			break;

		case fdWdX:
			data64 = float_to_int64(data, 32);
			if ((chips & 1) != 0) {
				v->fbi.dwdx = data64;
			}
			if ((chips & 2) != 0) {
				v->tmu[0].dwdx = data64;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].dwdx = data64;
			}
			break;
		case dWdX:
			if ((chips & 1) != 0) {
				v->fbi.dwdx = (int64_t)(int32_t)data << 2;
			}
			if ((chips & 2) != 0) {
				v->tmu[0].dwdx = (int64_t)(int32_t)data << 2;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].dwdx = (int64_t)(int32_t)data << 2;
			}
			break;

		case fdWdY:
			data64 = float_to_int64(data, 32);
			if ((chips & 1) != 0) {
				v->fbi.dwdy = data64;
			}
			if ((chips & 2) != 0) {
				v->tmu[0].dwdy = data64;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].dwdy = data64;
			}
			break;
		case dWdY:
			if ((chips & 1) != 0) {
				v->fbi.dwdy = (int64_t)(int32_t)data << 2;
			}
			if ((chips & 2) != 0) {
				v->tmu[0].dwdy = (int64_t)(int32_t)data << 2;
			}
			if ((chips & 4) != 0) {
				v->tmu[1].dwdy = (int64_t)(int32_t)data << 2;
			}
			break;

		/* setup bits */
		case sARGB:
			if ((chips & 1) != 0)
			{
				v->reg[sAlpha].f = (float)RGB_ALPHA(data);
				v->reg[sRed].f = (float)RGB_RED(data);
				v->reg[sGreen].f = (float)RGB_GREEN(data);
				v->reg[sBlue].f = (float)RGB_BLUE(data);
			}
			break;

		/* mask off invalid bits for different cards */
		case fbzColorPath:
			if (vtype < VOODOO_2) {
				data &= 0x0fffffff;
			}
			if ((chips & 1) != 0) {
				v->reg[fbzColorPath].u = data;
			}
			break;

		case fbzMode:
			if (vtype < VOODOO_2) {
				data &= 0x001fffff;
			}
			if ((chips & 1) != 0) {
#ifdef C_ENABLE_VOODOO_OPENGL
				if (v->ogl && v->active && (FBZMODE_Y_ORIGIN(v->reg[fbzMode].u)!=FBZMODE_Y_ORIGIN(data))) {
					v->reg[fbzMode].u = data;
					voodoo_ogl_set_window(v);
				} else 
#endif
				{
					v->reg[fbzMode].u = data;
				}
			}
			break;

		case fogMode:
			if (vtype < VOODOO_2) {
				data &= 0x0000003f;
			}
			if ((chips & 1) != 0) {
				v->reg[fogMode].u = data;
			}
			break;

		/* triangle drawing */
		case triangleCMD:
		case ftriangleCMD:
			triangle(v);
			break;

		case sBeginTriCMD:
			begin_triangle(v);
			break;

		case sDrawTriCMD:
			draw_triangle(v);
			break;

		/* other commands */
		case nopCMD:
			if ((data & 1) != 0u) {
				reset_counters(v);
			}
			if ((data & 2) != 0u) {
				v->reg[fbiTrianglesOut].u = 0;
			}
			break;

		case fastfillCMD:
			fastfill(v);
			break;

		case swapbufferCMD:
			swapbuffer(v, data);
			break;

		/* gamma table access -- Voodoo/Voodoo2 only */
		case clutData:
			/*
			if (chips & 1)
			{
				if (!FBIINIT1_VIDEO_TIMING_RESET(v->reg[fbiInit1].u))
				{
					int index = data >> 24;
					if (index <= 32)
					{
						//v->fbi.clut[index] = data;
					}
				}
				else
					LOG(LOG_VOODOO,LOG_WARN)("clutData ignored because video timing reset = 1\n");
			}
			*/
			break;

		/* external DAC access -- Voodoo/Voodoo2 only */
		case dacData:
			if ((chips & 1) != 0)
			{
				if ((data & 0x800) == 0u) {
					dacdata_w(&v->dac, (data >> 8) & 7, data & 0xff);
				} else {
					dacdata_r(&v->dac, (data >> 8) & 7);
				}
			}
			break;

		/* vertical sync rate -- Voodoo/Voodoo2 only */
		case hSync:
		case vSync:
		case backPorch:
		case videoDimensions:
			if ((chips & 1) != 0)
			{
				v->reg[regnum].u = data;
				if (v->reg[hSync].u != 0 && v->reg[vSync].u != 0 && v->reg[videoDimensions].u != 0)
				{
					const int hvis = v->reg[videoDimensions].u & 0x3ff;
					const int vvis = (v->reg[videoDimensions].u >> 16) & 0x3ff;

#ifdef C_ENABLE_VOODOO_DEBUG
					int htotal = ((v->reg[hSync].u >> 16) & 0x3ff) + 1 + (v->reg[hSync].u & 0xff) + 1;
					int vtotal = ((v->reg[vSync].u >> 16) & 0xfff) + (v->reg[vSync].u & 0xfff);

					int hbp = (v->reg[backPorch].u & 0xff) + 2;
					int vbp = (v->reg[backPorch].u >> 16) & 0xff;

					//attoseconds_t refresh = video_screen_get_frame_period(v->screen).attoseconds;
					attoseconds_t refresh = 0;
					attoseconds_t stdperiod, medperiod, vgaperiod;
					attoseconds_t stddiff, meddiff, vgadiff;

					/* compute the new period for standard res, medium res, and VGA res */
					stdperiod = HZ_TO_ATTOSECONDS(15750) * vtotal;
					medperiod = HZ_TO_ATTOSECONDS(25000) * vtotal;
					vgaperiod = HZ_TO_ATTOSECONDS(31500) * vtotal;

					/* compute a diff against the current refresh period */
					stddiff = stdperiod - refresh;
					if (stddiff < 0) stddiff = -stddiff;
					meddiff = medperiod - refresh;
					if (meddiff < 0) meddiff = -meddiff;
					vgadiff = vgaperiod - refresh;
					if (vgadiff < 0) vgadiff = -vgadiff;
					
					LOG(LOG_VOODOO,LOG_WARN)("hSync=%08X  vSync=%08X  backPorch=%08X  videoDimensions=%08X\n",
						v->reg[hSync].u, v->reg[vSync].u, v->reg[backPorch].u, v->reg[videoDimensions].u);

					rectangle visarea;

					/* create a new visarea */
					visarea.min_x = hbp;
					visarea.max_x = hbp + hvis - 1;
					visarea.min_y = vbp;
					visarea.max_y = vbp + vvis - 1;

					/* keep within bounds */
					visarea.max_x = std::min(visarea.max_x, htotal - 1);
					visarea.max_y = std::min(visarea.max_y, vtotal - 1);
					LOG(LOG_VOODOO,LOG_WARN)("Horiz: %d-%d (%d total)  Vert: %d-%d (%d total) -- ", visarea.min_x, visarea.max_x, htotal, visarea.min_y, visarea.max_y, vtotal);

					/* configure the screen based on which one matches the closest */
					if (stddiff < meddiff && stddiff < vgadiff)
					{
						//video_screen_configure(v->screen, htotal, vtotal, &visarea, stdperiod);
						LOG(LOG_VOODOO,LOG_WARN)("Standard resolution, %f Hz\n", ATTOSECONDS_TO_HZ(stdperiod));
					}
					else if (meddiff < vgadiff)
					{
						//video_screen_configure(v->screen, htotal, vtotal, &visarea, medperiod);
						LOG(LOG_VOODOO,LOG_WARN)("Medium resolution, %f Hz\n", ATTOSECONDS_TO_HZ(medperiod));
					}
					else
					{
						//video_screen_configure(v->screen, htotal, vtotal, &visarea, vgaperiod);
						LOG(LOG_VOODOO,LOG_WARN)("VGA resolution, %f Hz\n", ATTOSECONDS_TO_HZ(vgaperiod));
					}
#endif

					/* configure the new framebuffer info */
					const uint32_t new_width = (hvis + 1) & ~1;
					const uint32_t new_height = (vvis + 1) & ~1;

					if ((v->fbi.width != new_width) || (v->fbi.height != new_height)) {
						v->fbi.width = new_width;
						v->fbi.height = new_height;
#ifdef C_ENABLE_VOODOO_OPENGL
						v->ogl_dimchange = true;
#endif
					}
					//v->fbi.xoffs = hbp;
					//v->fbi.yoffs = vbp;
					//v->fbi.vsyncscan = (v->reg[vSync].u >> 16) & 0xfff;

					/* recompute the time of VBLANK */
					//adjust_vblank_timer(v);

					/* if changing dimensions, update video memory layout */
					if (regnum == videoDimensions) {
						recompute_video_memory(v);
					}

					Voodoo_UpdateScreenStart();
				}
			}
			break;

		/* fbiInit0 can only be written if initEnable says we can -- Voodoo/Voodoo2 only */
		case fbiInit0:
			if (((chips & 1) != 0) && INITEN_ENABLE_HW_INIT(v->pci.init_enable))
			{
			    const bool new_output_on = FBIINIT0_VGA_PASSTHRU(data);

				if (v->output_on != new_output_on) {
					v->output_on = new_output_on;
					Voodoo_UpdateScreenStart();
				}

				v->reg[fbiInit0].u = data;
				if (FBIINIT0_GRAPHICS_RESET(data)) {
					soft_reset(v);
				}
				recompute_video_memory(v);
			}
			break;

		/* fbiInit5-7 are Voodoo 2-only; ignore them on anything else */
		case fbiInit5:
		case fbiInit6:
			if (vtype < VOODOO_2) {
				break;
			}
			/* else fall through... */

		/* fbiInitX can only be written if initEnable says we can -- Voodoo/Voodoo2 only */
		/* most of these affect memory layout, so always recompute that when done */
		case fbiInit1:
		case fbiInit2:
		case fbiInit4:
			if (((chips & 1) != 0) && INITEN_ENABLE_HW_INIT(v->pci.init_enable))
			{
				v->reg[regnum].u = data;
				recompute_video_memory(v);
			}
			break;

		case fbiInit3:
			if (((chips & 1) != 0) && INITEN_ENABLE_HW_INIT(v->pci.init_enable))
			{
				v->reg[regnum].u = data;
				v->alt_regmap = (FBIINIT3_TRI_REGISTER_REMAP(data) > 0);
				v->fbi.yorigin = FBIINIT3_YORIGIN_SUBTRACT(v->reg[fbiInit3].u);
				recompute_video_memory(v);
			}
			break;

		/* nccTable entries are processed and expanded immediately */
		case nccTable+0:
		case nccTable+1:
		case nccTable+2:
		case nccTable+3:
		case nccTable+4:
		case nccTable+5:
		case nccTable+6:
		case nccTable+7:
		case nccTable+8:
		case nccTable+9:
		case nccTable+10:
		case nccTable+11:
			if ((chips & 2) != 0) {
				ncc_table_write(&v->tmu[0].ncc[0], regnum - nccTable, data);
			}
			if ((chips & 4) != 0) {
				ncc_table_write(&v->tmu[1].ncc[0], regnum - nccTable, data);
			}
			break;

		case nccTable+12:
		case nccTable+13:
		case nccTable+14:
		case nccTable+15:
		case nccTable+16:
		case nccTable+17:
		case nccTable+18:
		case nccTable+19:
		case nccTable+20:
		case nccTable+21:
		case nccTable+22:
		case nccTable+23:
			if ((chips & 2) != 0) {
				ncc_table_write(&v->tmu[0].ncc[1], regnum - (nccTable + 12), data);
			}
			if ((chips & 4) != 0) {
				ncc_table_write(&v->tmu[1].ncc[1], regnum - (nccTable + 12), data);
			}
			break;

		/* fogTable entries are processed and expanded immediately */
		case fogTable+0:
		case fogTable+1:
		case fogTable+2:
		case fogTable+3:
		case fogTable+4:
		case fogTable+5:
		case fogTable+6:
		case fogTable+7:
		case fogTable+8:
		case fogTable+9:
		case fogTable+10:
		case fogTable+11:
		case fogTable+12:
		case fogTable+13:
		case fogTable+14:
		case fogTable+15:
		case fogTable+16:
		case fogTable+17:
		case fogTable+18:
		case fogTable+19:
		case fogTable+20:
		case fogTable+21:
		case fogTable+22:
		case fogTable+23:
		case fogTable+24:
		case fogTable+25:
		case fogTable+26:
		case fogTable+27:
		case fogTable+28:
		case fogTable+29:
		case fogTable+30:
		case fogTable+31:
			if ((chips & 1) != 0)
			{
				const int base = 2 * (regnum - fogTable);

				auto& fbi = v->fbi;
				fbi.fogdelta[base + 0] = static_cast<uint8_t>(data & 0xff);
				fbi.fogblend[base + 0] = static_cast<uint8_t>((data >> 8) & 0xff);
				fbi.fogdelta[base + 1] = static_cast<uint8_t>((data >> 16) & 0xff);
				fbi.fogblend[base + 1] = static_cast<uint8_t>((data >> 24) & 0xff);
			}
			break;

		/* texture modifications cause us to recompute everything */
		case textureMode:
		case tLOD:
		case tDetail:
		case texBaseAddr:
		case texBaseAddr_1:
		case texBaseAddr_2:
		case texBaseAddr_3_8:
			if ((chips & 2) != 0)
			{
				v->tmu[0].reg[regnum].u = data;
				v->tmu[0].regdirty = true;
			}
			if ((chips & 4) != 0)
			{
				v->tmu[1].reg[regnum].u = data;
				v->tmu[1].regdirty = true;
			}
			break;

		case trexInit1:
			/* send tmu config data to the frame buffer */
			v->send_config = (TREXINIT_SEND_TMU_CONFIG(data) > 0);
			goto default_case;
			break;

		case clipLowYHighY:
		case clipLeftRight:
			if ((chips & 1) != 0) {
				v->reg[0x000 + regnum].u = data;
			}
#ifdef C_ENABLE_VOODOO_OPENGL
		        if (v->ogl) {
				voodoo_ogl_clip_window(v);
			}
#endif
			break;

		/* these registers are referenced in the renderer; we must wait for pending work before changing */
		case chromaRange:
		case chromaKey:
		case alphaMode:
		case fogColor:
		case stipple:
		case zaColor:
		case color1:
		case color0:
			/* fall through to default implementation */

		/* by default, just feed the data to the chips */
		default:
default_case:
			if ((chips & 1) != 0) {
				v->reg[0x000 + regnum].u = data;
			}
			if ((chips & 2) != 0) {
				v->reg[0x100 + regnum].u = data;
			}
			if ((chips & 4) != 0) {
				v->reg[0x200 + regnum].u = data;
			}
			if ((chips & 8) != 0) {
				v->reg[0x300 + regnum].u = data;
			}
			break;
	}
}

/*************************************
 *
 *  Voodoo LFB writes
 *
 *************************************/
static void lfb_w(uint32_t offset, uint32_t data, uint32_t mem_mask) {
	//LOG(LOG_VOODOO,LOG_WARN)("V3D:WR LFB offset %X value %08X", offset, data);
	uint16_t* dest  = {};
	uint16_t* depth = {};

	uint32_t destmax  = 0;
	uint32_t depthmax = 0;

	int sr[2] = {};
	int sg[2] = {};
	int sb[2] = {};
	int sa[2] = {};
	int sw[2] = {};

	int x       = 0;
	int y       = 0;
	int scry    = 0;
	int mask    = 0;
	int pix     = 0;
	int destbuf = 0;

	/* byte swizzling */
	if (LFBMODE_BYTE_SWIZZLE_WRITES(v->reg[lfbMode].u))
	{
		data = bswap_u32(data);
		mem_mask = bswap_u32(mem_mask);
	}

	/* word swapping */
	if (LFBMODE_WORD_SWAP_WRITES(v->reg[lfbMode].u))
	{
		data = (data << 16) | (data >> 16);
		mem_mask = (mem_mask << 16) | (mem_mask >> 16);
	}

	/* extract default depth and alpha values */
	sw[0] = sw[1] = v->reg[zaColor].u & 0xffff;
	sa[0] = sa[1] = v->reg[zaColor].u >> 24;

	/* first extract A,R,G,B from the data */
	switch (LFBMODE_WRITE_FORMAT(v->reg[lfbMode].u) + 16 * LFBMODE_RGBA_LANES(v->reg[lfbMode].u))
	{
		case 16*0 + 0:		/* ARGB, 16-bit RGB 5-6-5 */
		case 16*2 + 0:		/* RGBA, 16-bit RGB 5-6-5 */
			EXTRACT_565_TO_888(data, sr[0], sg[0], sb[0]);
			EXTRACT_565_TO_888(data >> 16, sr[1], sg[1], sb[1]);
			mask = LFB_RGB_PRESENT | (LFB_RGB_PRESENT << 4);
			offset <<= 1;
			break;
		case 16*1 + 0:		/* ABGR, 16-bit RGB 5-6-5 */
		case 16*3 + 0:		/* BGRA, 16-bit RGB 5-6-5 */
			EXTRACT_565_TO_888(data, sb[0], sg[0], sr[0]);
			EXTRACT_565_TO_888(data >> 16, sb[1], sg[1], sr[1]);
			mask = LFB_RGB_PRESENT | (LFB_RGB_PRESENT << 4);
			offset <<= 1;
			break;

		case 16*0 + 1:		/* ARGB, 16-bit RGB x-5-5-5 */
			EXTRACT_x555_TO_888(data, sr[0], sg[0], sb[0]);
			EXTRACT_x555_TO_888(data >> 16, sr[1], sg[1], sb[1]);
			mask = LFB_RGB_PRESENT | (LFB_RGB_PRESENT << 4);
			offset <<= 1;
			break;
		case 16*1 + 1:		/* ABGR, 16-bit RGB x-5-5-5 */
			EXTRACT_x555_TO_888(data, sb[0], sg[0], sr[0]);
			EXTRACT_x555_TO_888(data >> 16, sb[1], sg[1], sr[1]);
			mask = LFB_RGB_PRESENT | (LFB_RGB_PRESENT << 4);
			offset <<= 1;
			break;
		case 16*2 + 1:		/* RGBA, 16-bit RGB x-5-5-5 */
			EXTRACT_555x_TO_888(data, sr[0], sg[0], sb[0]);
			EXTRACT_555x_TO_888(data >> 16, sr[1], sg[1], sb[1]);
			mask = LFB_RGB_PRESENT | (LFB_RGB_PRESENT << 4);
			offset <<= 1;
			break;
		case 16*3 + 1:		/* BGRA, 16-bit RGB x-5-5-5 */
			EXTRACT_555x_TO_888(data, sb[0], sg[0], sr[0]);
			EXTRACT_555x_TO_888(data >> 16, sb[1], sg[1], sr[1]);
			mask = LFB_RGB_PRESENT | (LFB_RGB_PRESENT << 4);
			offset <<= 1;
			break;

		case 16*0 + 2:		/* ARGB, 16-bit ARGB 1-5-5-5 */
			EXTRACT_1555_TO_8888(data, sa[0], sr[0], sg[0], sb[0]);
			EXTRACT_1555_TO_8888(data >> 16, sa[1], sr[1], sg[1], sb[1]);
			mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT | ((LFB_RGB_PRESENT | LFB_ALPHA_PRESENT) << 4);
			offset <<= 1;
			break;
		case 16*1 + 2:		/* ABGR, 16-bit ARGB 1-5-5-5 */
			EXTRACT_1555_TO_8888(data, sa[0], sb[0], sg[0], sr[0]);
			EXTRACT_1555_TO_8888(data >> 16, sa[1], sb[1], sg[1], sr[1]);
			mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT | ((LFB_RGB_PRESENT | LFB_ALPHA_PRESENT) << 4);
			offset <<= 1;
			break;
		case 16*2 + 2:		/* RGBA, 16-bit ARGB 1-5-5-5 */
			EXTRACT_5551_TO_8888(data, sr[0], sg[0], sb[0], sa[0]);
			EXTRACT_5551_TO_8888(data >> 16, sr[1], sg[1], sb[1], sa[1]);
			mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT | ((LFB_RGB_PRESENT | LFB_ALPHA_PRESENT) << 4);
			offset <<= 1;
			break;
		case 16*3 + 2:		/* BGRA, 16-bit ARGB 1-5-5-5 */
			EXTRACT_5551_TO_8888(data, sb[0], sg[0], sr[0], sa[0]);
			EXTRACT_5551_TO_8888(data >> 16, sb[1], sg[1], sr[1], sa[1]);
			mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT | ((LFB_RGB_PRESENT | LFB_ALPHA_PRESENT) << 4);
			offset <<= 1;
			break;

		case 16*0 + 4:		/* ARGB, 32-bit RGB x-8-8-8 */
			EXTRACT_x888_TO_888(data, sr[0], sg[0], sb[0]);
			mask = LFB_RGB_PRESENT;
			break;
		case 16*1 + 4:		/* ABGR, 32-bit RGB x-8-8-8 */
			EXTRACT_x888_TO_888(data, sb[0], sg[0], sr[0]);
			mask = LFB_RGB_PRESENT;
			break;
		case 16*2 + 4:		/* RGBA, 32-bit RGB x-8-8-8 */
			EXTRACT_888x_TO_888(data, sr[0], sg[0], sb[0]);
			mask = LFB_RGB_PRESENT;
			break;
		case 16*3 + 4:		/* BGRA, 32-bit RGB x-8-8-8 */
			EXTRACT_888x_TO_888(data, sb[0], sg[0], sr[0]);
			mask = LFB_RGB_PRESENT;
			break;

		case 16*0 + 5:		/* ARGB, 32-bit ARGB 8-8-8-8 */
			EXTRACT_8888_TO_8888(data, sa[0], sr[0], sg[0], sb[0]);
			mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT;
			break;
		case 16*1 + 5:		/* ABGR, 32-bit ARGB 8-8-8-8 */
			EXTRACT_8888_TO_8888(data, sa[0], sb[0], sg[0], sr[0]);
			mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT;
			break;
		case 16*2 + 5:		/* RGBA, 32-bit ARGB 8-8-8-8 */
			EXTRACT_8888_TO_8888(data, sr[0], sg[0], sb[0], sa[0]);
			mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT;
			break;
		case 16*3 + 5:		/* BGRA, 32-bit ARGB 8-8-8-8 */
			EXTRACT_8888_TO_8888(data, sb[0], sg[0], sr[0], sa[0]);
			mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT;
			break;

		case 16*0 + 12:		/* ARGB, 32-bit depth+RGB 5-6-5 */
		case 16*2 + 12:		/* RGBA, 32-bit depth+RGB 5-6-5 */
			sw[0] = data >> 16;
			EXTRACT_565_TO_888(data, sr[0], sg[0], sb[0]);
			mask = LFB_RGB_PRESENT | LFB_DEPTH_PRESENT_MSW;
			break;
		case 16*1 + 12:		/* ABGR, 32-bit depth+RGB 5-6-5 */
		case 16*3 + 12:		/* BGRA, 32-bit depth+RGB 5-6-5 */
			sw[0] = data >> 16;
			EXTRACT_565_TO_888(data, sb[0], sg[0], sr[0]);
			mask = LFB_RGB_PRESENT | LFB_DEPTH_PRESENT_MSW;
			break;

		case 16*0 + 13:		/* ARGB, 32-bit depth+RGB x-5-5-5 */
			sw[0] = data >> 16;
			EXTRACT_x555_TO_888(data, sr[0], sg[0], sb[0]);
			mask = LFB_RGB_PRESENT | LFB_DEPTH_PRESENT_MSW;
			break;
		case 16*1 + 13:		/* ABGR, 32-bit depth+RGB x-5-5-5 */
			sw[0] = data >> 16;
			EXTRACT_x555_TO_888(data, sb[0], sg[0], sr[0]);
			mask = LFB_RGB_PRESENT | LFB_DEPTH_PRESENT_MSW;
			break;
		case 16*2 + 13:		/* RGBA, 32-bit depth+RGB x-5-5-5 */
			sw[0] = data >> 16;
			EXTRACT_555x_TO_888(data, sr[0], sg[0], sb[0]);
			mask = LFB_RGB_PRESENT | LFB_DEPTH_PRESENT_MSW;
			break;
		case 16*3 + 13:		/* BGRA, 32-bit depth+RGB x-5-5-5 */
			sw[0] = data >> 16;
			EXTRACT_555x_TO_888(data, sb[0], sg[0], sr[0]);
			mask = LFB_RGB_PRESENT | LFB_DEPTH_PRESENT_MSW;
			break;

		case 16*0 + 14:		/* ARGB, 32-bit depth+ARGB 1-5-5-5 */
			sw[0] = data >> 16;
			EXTRACT_1555_TO_8888(data, sa[0], sr[0], sg[0], sb[0]);
			mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT | LFB_DEPTH_PRESENT_MSW;
			break;
		case 16*1 + 14:		/* ABGR, 32-bit depth+ARGB 1-5-5-5 */
			sw[0] = data >> 16;
			EXTRACT_1555_TO_8888(data, sa[0], sb[0], sg[0], sr[0]);
			mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT | LFB_DEPTH_PRESENT_MSW;
			break;
		case 16*2 + 14:		/* RGBA, 32-bit depth+ARGB 1-5-5-5 */
			sw[0] = data >> 16;
			EXTRACT_5551_TO_8888(data, sr[0], sg[0], sb[0], sa[0]);
			mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT | LFB_DEPTH_PRESENT_MSW;
			break;
		case 16*3 + 14:		/* BGRA, 32-bit depth+ARGB 1-5-5-5 */
			sw[0] = data >> 16;
			EXTRACT_5551_TO_8888(data, sb[0], sg[0], sr[0], sa[0]);
			mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT | LFB_DEPTH_PRESENT_MSW;
			break;

		case 16*0 + 15:		/* ARGB, 16-bit depth */
		case 16*1 + 15:		/* ARGB, 16-bit depth */
		case 16*2 + 15:		/* ARGB, 16-bit depth */
		case 16*3 + 15:		/* ARGB, 16-bit depth */
			sw[0] = data & 0xffff;
			sw[1] = data >> 16;
			mask = LFB_DEPTH_PRESENT | (LFB_DEPTH_PRESENT << 4);
			offset <<= 1;
			break;

		default:			/* reserved */
			return;
	}

	/* compute X,Y */
	x = (offset << 0) & ((1 << 10) - 1);
	y = (offset >> 10) & ((1 << 10) - 1);

	/* adjust the mask based on which half of the data is written */
	if (!ACCESSING_BITS_0_15) {
		mask &= ~(0x0f - LFB_DEPTH_PRESENT_MSW);
	}
	if (!ACCESSING_BITS_16_31) {
		mask &= ~(0xf0 + LFB_DEPTH_PRESENT_MSW);
	}

	/* select the target buffer */
	destbuf = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u);
	//LOG(LOG_VOODOO,LOG_WARN)("destbuf %X lfbmode %X",destbuf, v->reg[lfbMode].u);

	assert(destbuf == 0 || destbuf == 1);
	switch (destbuf) {
	case 0: /* front buffer */
		dest = (uint16_t*)(v->fbi.ram + v->fbi.rgboffs[v->fbi.frontbuf]);
			destmax = (v->fbi.mask + 1 - v->fbi.rgboffs[v->fbi.frontbuf]) / 2;
			break;

	case 1: /* back buffer */
		dest = (uint16_t*)(v->fbi.ram + v->fbi.rgboffs[v->fbi.backbuf]);
			destmax = (v->fbi.mask + 1 - v->fbi.rgboffs[v->fbi.backbuf]) / 2;
			break;

	default: /* reserved */ return;
	}
	depth = (uint16_t *)(v->fbi.ram + v->fbi.auxoffs);
	depthmax = (v->fbi.mask + 1 - v->fbi.auxoffs) / 2;

	/* simple case: no pipeline */
	if (!LFBMODE_ENABLE_PIXEL_PIPELINE(v->reg[lfbMode].u))
	{
		const uint8_t* dither_lookup = nullptr;
		const uint8_t* dither4       = nullptr;

		[[maybe_unused]] const uint8_t* dither = nullptr;

		uint32_t bufoffs;

		if (LOG_LFB != 0u) {
			LOG(LOG_VOODOO, LOG_WARN)
			("VOODOO.LFB:write raw mode %X (%d,%d) = %08X & %08X\n",
			 LFBMODE_WRITE_FORMAT(v->reg[lfbMode].u),
			 x,
			 y,
			 data,
			 mem_mask);
		}

		/* determine the screen Y */
		scry = y;
		if (LFBMODE_Y_ORIGIN(v->reg[lfbMode].u)) {
			scry = (v->fbi.yorigin - y) & 0x3ff;
		}

		/* advance pointers to the proper row */
		bufoffs = scry * v->fbi.rowpixels + x;

		/* compute dithering */
		COMPUTE_DITHER_POINTERS(v->reg[fbzMode].u, y);

		/* loop over up to two pixels */
		for (pix = 0; mask != 0; pix++)
		{
			/* make sure we care about this pixel */
			if ((mask & 0x0f) != 0)
			{
				const auto has_rgb = (mask & LFB_RGB_PRESENT) > 0;
				const auto has_alpha = ((mask & LFB_ALPHA_PRESENT) > 0) && (FBZMODE_ENABLE_ALPHA_PLANES(v->reg[fbzMode].u) > 0);
				const auto has_depth = ((mask & (LFB_DEPTH_PRESENT | LFB_DEPTH_PRESENT_MSW)) && !FBZMODE_ENABLE_ALPHA_PLANES(v->reg[fbzMode].u));
#ifdef C_ENABLE_VOODOO_OPENGL
				if (v->ogl && v->active) {
					if (has_rgb || has_alpha) {
						// if enabling dithering: output is 565 not 888 anymore
						//APPLY_DITHER(v->reg[fbzMode].u, x, dither_lookup, sr[pix], sg[pix], sb[pix]);
						voodoo_ogl_draw_pixel(x, scry+1, has_rgb, has_alpha, sr[pix], sg[pix], sb[pix], sa[pix]);
					}
					if (has_depth) {
						voodoo_ogl_draw_z(x, scry+1, sw[pix]);
					}
				} else 
#endif
				{
					/* write to the RGB buffer */
					if (has_rgb && bufoffs < destmax)
					{
						/* apply dithering and write to the screen */
						APPLY_DITHER(v->reg[fbzMode].u, x, dither_lookup, sr[pix], sg[pix], sb[pix]);
						dest[bufoffs] = (uint16_t)((sr[pix] << 11) | (sg[pix] << 5) | sb[pix]);
					}

					/* make sure we have an aux buffer to write to */
					if ((depth != nullptr) && bufoffs < depthmax)
					{
						/* write to the alpha buffer */
						if (has_alpha) {
							depth[bufoffs] = (uint16_t)sa[pix];
						}

						/* write to the depth buffer */
						if (has_depth) {
							depth[bufoffs] = (uint16_t)sw[pix];
						}
					}
				}

				/* track pixel writes to the frame buffer regardless of mask */
				v->reg[fbiPixelsOut].u++;
			}

			/* advance our pointers */
			bufoffs++;
			x++;
			mask >>= 4;
		}
	}

	/* tricky case: run the full pixel pipeline on the pixel */
	else
	{
		const uint8_t* dither_lookup = nullptr;
		const uint8_t* dither4       = nullptr;
		const uint8_t* dither        = nullptr;

		if (LOG_LFB != 0u) {
			LOG(LOG_VOODOO, LOG_WARN)
			("VOODOO.LFB:write pipelined mode %X (%d,%d) = %08X & %08X\n",
			 LFBMODE_WRITE_FORMAT(v->reg[lfbMode].u),
			 x,
			 y,
			 data,
			 mem_mask);
		}

		/* determine the screen Y */
		scry = y;
		if (FBZMODE_Y_ORIGIN(v->reg[fbzMode].u)) {
			scry = (v->fbi.yorigin - y) & 0x3ff;
		}

		/* advance pointers to the proper row */
		dest += scry * v->fbi.rowpixels;
		if (depth != nullptr) {
			depth += scry * v->fbi.rowpixels;
		}

		/* compute dithering */
		COMPUTE_DITHER_POINTERS(v->reg[fbzMode].u, y);

		int32_t blendr = 0;
		int32_t blendg = 0;
		int32_t blendb = 0;
		int32_t blenda = 0;

		/* loop over up to two pixels */
		stats_block stats = {};
		for (pix = 0; mask != 0; pix++)
		{
			/* make sure we care about this pixel */
			if ((mask & 0x0f) != 0)
			{
				const int64_t iterw = static_cast<int64_t>(sw[pix]) << (30 - 16);
				const int32_t iterz = sw[pix] << 12;

				rgb_union color;

				/* apply clipping */
				if (FBZMODE_ENABLE_CLIPPING(v->reg[fbzMode].u))
				{
					if (x < (int32_t)((v->reg[clipLeftRight].u >> 16) & 0x3ff) ||
						x >= (int32_t)(v->reg[clipLeftRight].u & 0x3ff) ||
						scry < (int32_t)((v->reg[clipLowYHighY].u >> 16) & 0x3ff) ||
						scry >= (int32_t)(v->reg[clipLowYHighY].u & 0x3ff))
					{
						stats.pixels_in++;
						//stats.clip_fail++;
						goto nextpixel;
					}
				}

				/* pixel pipeline part 1 handles depth testing and stippling */
				// TODO: in the v->ogl case this macro doesn't really work with depth testing
				PIXEL_PIPELINE_BEGIN(v, stats, x, y, v->reg[fbzColorPath].u, v->reg[fbzMode].u, iterz, iterw, v->reg[zaColor].u, v->reg[stipple].u);

				color.rgb.r = static_cast<uint8_t>(sr[pix]);
				color.rgb.g = static_cast<uint8_t>(sg[pix]);
				color.rgb.b = static_cast<uint8_t>(sb[pix]);
				color.rgb.a = static_cast<uint8_t>(sa[pix]);

				/* apply chroma key */
				APPLY_CHROMAKEY(v, stats, v->reg[fbzMode].u, color);

				/* apply alpha mask, and alpha testing */
				APPLY_ALPHAMASK(v, stats, v->reg[fbzMode].u, color.rgb.a);
				APPLY_ALPHATEST(v, stats, v->reg[alphaMode].u, color.rgb.a);

				/*
				if (FBZCP_CC_MSELECT(v->reg[fbzColorPath].u) != 0) maybe_log_debug("lfbw fpp mselect %8x",FBZCP_CC_MSELECT(v->reg[fbzColorPath].u));
				if (FBZCP_CCA_MSELECT(v->reg[fbzColorPath].u) > 1) maybe_log_debug("lfbw fpp mselect alpha %8x",FBZCP_CCA_MSELECT(v->reg[fbzColorPath].u));

				if (FBZCP_CC_REVERSE_BLEND(v->reg[fbzColorPath].u) != 0) {
					if (FBZCP_CC_MSELECT(v->reg[fbzColorPath].u) != 0) maybe_log_debug("lfbw fpp rblend %8x",FBZCP_CC_REVERSE_BLEND(v->reg[fbzColorPath].u));
				}
				if (FBZCP_CCA_REVERSE_BLEND(v->reg[fbzColorPath].u) != 0) {
					if (FBZCP_CC_MSELECT(v->reg[fbzColorPath].u) != 0) maybe_log_debug("lfbw fpp rblend alpha %8x",FBZCP_CCA_REVERSE_BLEND(v->reg[fbzColorPath].u));
				}
				*/

				rgb_union c_local;

				/* compute c_local */
				if (FBZCP_CC_LOCALSELECT_OVERRIDE(v->reg[fbzColorPath].u) == 0)
				{
					if (FBZCP_CC_LOCALSELECT(v->reg[fbzColorPath].u) == 0)	/* iterated RGB */
					{
						//c_local.u = iterargb.u;
						c_local.rgb.r = static_cast<uint8_t>(sr[pix]);
						c_local.rgb.g = static_cast<uint8_t>(sg[pix]);
						c_local.rgb.b = static_cast<uint8_t>(sb[pix]);
					} else {
						// color0 RGB
						c_local.u = v->reg[color0].u;
					}
				}
				else
				{
					maybe_log_debug("lfbw fpp FBZCP_CC_LOCALSELECT_OVERRIDE set!");
					/*
					if (!(texel.rgb.a & 0x80))					// iterated RGB
						c_local.u = iterargb.u;
					else											// color0 RGB
						c_local.u = v->reg[color0].u;
					*/
				}

				/* compute a_local */
				switch (FBZCP_CCA_LOCALSELECT(v->reg[fbzColorPath].u))
				{
					default:
					case 0:		/* iterated alpha */
						//c_local.rgb.a = iterargb.rgb.a;
						c_local.rgb.a = static_cast<uint8_t>(sa[pix]);
						break;
					case 1:		/* color0 alpha */
						c_local.rgb.a = v->reg[color0].rgb.a;
						break;
					case 2:		/* clamped iterated Z[27:20] */
					{
						int temp;
						CLAMPED_Z(iterz, v->reg[fbzColorPath].u, temp);
						c_local.rgb.a = (uint8_t)temp;
						break;
					}
					case 3:		/* clamped iterated W[39:32] */
					{
						int temp;
						CLAMPED_W(iterw, v->reg[fbzColorPath].u, temp);			/* Voodoo 2 only */
						c_local.rgb.a = (uint8_t)temp;
						break;
					}
				}

				/* select zero or c_other */
				if (FBZCP_CC_ZERO_OTHER(v->reg[fbzColorPath].u) == 0) {
					r = sr[pix];
					g = sg[pix];
					b = sb[pix];
				} else {
					r = g = b = 0;
				}

				/* select zero or a_other */
				if (FBZCP_CCA_ZERO_OTHER(v->reg[fbzColorPath].u) == 0) {
					a = sa[pix];
				} else {
					a = 0;
				}

				/* subtract c_local */
				if (FBZCP_CC_SUB_CLOCAL(v->reg[fbzColorPath].u))
				{
					r -= c_local.rgb.r;
					g -= c_local.rgb.g;
					b -= c_local.rgb.b;
				}

				/* subtract a_local */
				if (FBZCP_CCA_SUB_CLOCAL(v->reg[fbzColorPath].u)) {
					a -= c_local.rgb.a;
				}

				/* blend RGB */
				switch (FBZCP_CC_MSELECT(v->reg[fbzColorPath].u))
				{
					default:	/* reserved */
					case 0:		/* 0 */
						blendr = blendg = blendb = 0;
						break;
					case 1:		/* c_local */
						blendr = c_local.rgb.r;
						blendg = c_local.rgb.g;
						blendb = c_local.rgb.b;
						// maybe_log_debug("blend RGB c_local");
						break;
					case 2:		/* a_other */
						//blendr = blendg = blendb = c_other.rgb.a;
						maybe_log_debug("blend RGB a_other");
						break;
					case 3:		/* a_local */
						blendr = blendg = blendb = c_local.rgb.a;
						maybe_log_debug("blend RGB a_local");
						break;
					case 4:		/* texture alpha */
						//blendr = blendg = blendb = texel.rgb.a;
						maybe_log_debug("blend RGB texture alpha");
						break;
					case 5:		/* texture RGB (Voodoo 2 only) */
						//blendr = texel.rgb.r;
						//blendg = texel.rgb.g;
						//blendb = texel.rgb.b;
						maybe_log_debug("blend RGB texture RGB");
						break;
				}

				/* blend alpha */
				switch (FBZCP_CCA_MSELECT(v->reg[fbzColorPath].u))
				{
					default:	/* reserved */
					case 0:		/* 0 */
						blenda = 0;
						break;
					case 1:		/* a_local */
						blenda = c_local.rgb.a;
						// maybe_log_debug("blend alpha a_local");
						break;
					case 2:		/* a_other */
						//blenda = c_other.rgb.a;
						maybe_log_debug("blend alpha a_other");
						break;
					case 3:		/* a_local */
						blenda = c_local.rgb.a;
						maybe_log_debug("blend alpha a_local");
						break;
					case 4:		/* texture alpha */
						//blenda = texel.rgb.a;
						maybe_log_debug("blend alpha texture alpha");
						break;
				}

				/* reverse the RGB blend */
				if (!FBZCP_CC_REVERSE_BLEND(v->reg[fbzColorPath].u))
				{
					blendr ^= 0xff;
					blendg ^= 0xff;
					blendb ^= 0xff;
				}

				/* reverse the alpha blend */
				if (!FBZCP_CCA_REVERSE_BLEND(v->reg[fbzColorPath].u)) {
					blenda ^= 0xff;
				}

				/* do the blend */
				r = (r * (blendr + 1)) >> 8;
				g = (g * (blendg + 1)) >> 8;
				b = (b * (blendb + 1)) >> 8;
				a = (a * (blenda + 1)) >> 8;

				/* add clocal or alocal to RGB */
				switch (FBZCP_CC_ADD_ACLOCAL(v->reg[fbzColorPath].u))
				{
					case 3:		/* reserved */
					case 0:		/* nothing */
						break;
					case 1:		/* add c_local */
						r += c_local.rgb.r;
						g += c_local.rgb.g;
						b += c_local.rgb.b;
						break;
					case 2:		/* add_alocal */
						r += c_local.rgb.a;
						g += c_local.rgb.a;
						b += c_local.rgb.a;
						break;
				}

				/* add clocal or alocal to alpha */
				if (FBZCP_CCA_ADD_ACLOCAL(v->reg[fbzColorPath].u)) {
					a += c_local.rgb.a;
				}

				/* clamp */
				r = clamp_to_uint8(r);
				g = clamp_to_uint8(g);
				b = clamp_to_uint8(b);
				a = clamp_to_uint8(a);

				/* invert */
				if (FBZCP_CC_INVERT_OUTPUT(v->reg[fbzColorPath].u))
				{
					r ^= 0xff;
					g ^= 0xff;
					b ^= 0xff;
				}
				if (FBZCP_CCA_INVERT_OUTPUT(v->reg[fbzColorPath].u)) {
					a ^= 0xff;
				}

#ifdef C_ENABLE_VOODOO_OPENGL
				if (v->ogl && v->active) {
					if (FBZMODE_RGB_BUFFER_MASK(v->reg[fbzMode].u)) {
						//APPLY_DITHER(FBZMODE, XX, DITHER_LOOKUP, r, g, b);
						voodoo_ogl_draw_pixel_pipeline(x, scry+1, r, g, b);
					}
					/*
					if (depth && FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u)) {
						if (FBZMODE_ENABLE_ALPHA_PLANES(v->reg[fbzMode].u) == 0)
							voodoo_ogl_draw_z(x, y, depthval&0xffff, depthval>>16);
						//else
						//	depth[XX] = a;
					}
					*/
				} else 
#endif
				{
					/* pixel pipeline part 2 handles color combine, fog, alpha, and final output */
					PIXEL_PIPELINE_MODIFY(v, dither, dither4, x, v->reg[fbzMode].u, v->reg[fbzColorPath].u, v->reg[alphaMode].u, v->reg[fogMode].u, iterz, iterw, v->reg[zaColor]);

					PIXEL_PIPELINE_FINISH(v, dither_lookup, x, dest, depth, v->reg[fbzMode].u);
				}

				PIXEL_PIPELINE_END(stats);
			}
nextpixel:
			/* advance our pointers */
			x++;
			mask >>= 4;
		}
		sum_statistics(&v->fbi.lfb_stats, &stats);
	}
}

/*************************************
 *
 *  Voodoo texture RAM writes
 *
 *************************************/
static int32_t texture_w(uint32_t offset, uint32_t data) {
	const auto tmu_num = static_cast<uint8_t>((offset >> 19) & 0b11);

	// LOG(LOG_VOODOO,LOG_WARN)("V3D:write TMU%x offset %X value %X",
	// tmunum, offset, data);

	/* point to the right TMU */
	if ((v->chipmask & (2 << tmu_num)) == 0) {
		return 0;
	}

	const auto t = &v->tmu[tmu_num];
	assert(t);

	// Should always be indirect writes
	assert(TEXLOD_TDIRECT_WRITE(t->reg[tLOD].u) == 0);

	/* update texture info if dirty */
	if (t->regdirty) {
		recompute_texture_params(t);
	}

	/* swizzle the data */
	if (TEXLOD_TDATA_SWIZZLE(t->reg[tLOD].u)) {
		data = bswap_u32(data);
	}
	if (TEXLOD_TDATA_SWAP(t->reg[tLOD].u)) {
		data = (data >> 16) | (data << 16);
	}

	/* 8-bit texture case */
	if (TEXMODE_FORMAT(t->reg[textureMode].u) < 8)
	{
		int lod;
		int tt;
		int ts;
		uint32_t tbaseaddr;
		uint8_t *dest;

		/* extract info */
		lod = (offset >> 15) & 0x0f;
		tt = (offset >> 7) & 0xff;

		/* old code has a bit about how this is broken in gauntleg unless we always look at TMU0 */
		if (TEXMODE_SEQ_8_DOWNLD(v->tmu[0].reg /*t->reg*/[textureMode].u)) {
			ts = (offset << 2) & 0xfc;
		} else {
			ts = (offset << 1) & 0xfc;
		}

		/* validate parameters */
		if (lod > 8) {
			return 0;
		}

		/* compute the base address */
		tbaseaddr = t->lodoffset[lod];
		tbaseaddr += tt * ((t->wmask >> lod) + 1) + ts;

		if (LOG_TEXTURE_RAM != 0u) {
			LOG(LOG_VOODOO, LOG_WARN)
			("Texture 8-bit w: lod=%d s=%d t=%d data=%08X\n", lod, ts, tt, data);
		}

		/* write the four bytes in little-endian order */
		dest = t->ram;
		tbaseaddr &= t->mask;

		[[maybe_unused]] bool changed = false;
		if (dest[BYTE4_XOR_LE(tbaseaddr + 0)] != ((data >> 0) & 0xff)) {
			dest[BYTE4_XOR_LE(tbaseaddr + 0)] = static_cast<uint8_t>((data >> 0) & 0xff);
			changed = true;
		}
		if (dest[BYTE4_XOR_LE(tbaseaddr + 1)] != ((data >> 8) & 0xff)) {
			dest[BYTE4_XOR_LE(tbaseaddr + 1)] = static_cast<uint8_t>((data >> 8) & 0xff);
			changed = true;
		}
		if (dest[BYTE4_XOR_LE(tbaseaddr + 2)] != ((data >> 16) & 0xff)) {
			dest[BYTE4_XOR_LE(tbaseaddr + 2)] = static_cast<uint8_t>((data >> 16) & 0xff);
			changed = true;
		}
		if (dest[BYTE4_XOR_LE(tbaseaddr + 3)] != ((data >> 24) & 0xff)) {
			dest[BYTE4_XOR_LE(tbaseaddr + 3)] = static_cast<uint8_t>((data >> 24) & 0xff);
			changed = true;
		}

#ifdef C_ENABLE_VOODOO_OPENGL
		if (changed && v->ogl && v->active) {
			voodoo_ogl_texture_clear(t->lodoffset[lod],tmunum);
			voodoo_ogl_texture_clear(t->lodoffset[t->lodmin],tmunum);
		}
#endif
	}

	/* 16-bit texture case */
	else
	{
		int lod;
		int tt;
		int ts;
		uint32_t tbaseaddr;
		uint16_t *dest;

		/* extract info */
#ifdef C_ENABLE_VOODOO_OPENGL
		tmunum = (offset >> 19) & 0x03;
#endif
		lod = (offset >> 15) & 0x0f;
		tt = (offset >> 7) & 0xff;
		ts = (offset << 1) & 0xfe;

		/* validate parameters */
		if (lod > 8) {
			return 0;
		}

		/* compute the base address */
		tbaseaddr = t->lodoffset[lod];
		tbaseaddr += 2 * (tt * ((t->wmask >> lod) + 1) + ts);

		if (LOG_TEXTURE_RAM != 0u) {
			LOG(LOG_VOODOO, LOG_WARN)
			("Texture 16-bit w: lod=%d s=%d t=%d data=%08X\n", lod, ts, tt, data);
		}

		/* write the two words in little-endian order */
		dest = (uint16_t *)t->ram;
		tbaseaddr &= t->mask;
		tbaseaddr >>= 1;

		[[maybe_unused]] bool changed = false;
		if (dest[BYTE_XOR_LE(tbaseaddr + 0)] != ((data >> 0) & 0xffff)) {
			dest[BYTE_XOR_LE(tbaseaddr + 0)] = static_cast<uint16_t>((data >> 0) & 0xffff);
			changed = true;
		}
		if (dest[BYTE_XOR_LE(tbaseaddr + 1)] != ((data >> 16) & 0xffff)) {
			dest[BYTE_XOR_LE(tbaseaddr + 1)] = static_cast<uint16_t>((data >> 16) & 0xffff);
			changed = true;
		}

#ifdef C_ENABLE_VOODOO_OPENGL
		if (changed && v->ogl && v->active) {
			voodoo_ogl_texture_clear(t->lodoffset[lod],tmunum);
			voodoo_ogl_texture_clear(t->lodoffset[t->lodmin],tmunum);
		}
#endif
	}

	return 0;
}

/*************************************
 *
 *  Handle a register read
 *
 *************************************/
static uint32_t register_r(const uint32_t offset)
{
	using namespace bit::literals;

	const auto regnum = static_cast<uint8_t>((offset) & 0xff);

	//LOG(LOG_VOODOO,LOG_WARN)("Voodoo:read chip %x reg %x (%s)", chips, regnum<<2, voodoo_reg_name[regnum]);

	/* first make sure this register is readable */
	if ((v->regaccess[regnum] & REGISTER_READ) == 0)
	{
		return 0xffffffff;
	}

	/* default result is the FBI register value */
	auto result = v->reg[regnum].u;

	/* some registers are dynamic; compute them */
	switch (regnum)
	{
		case status:

			/* start with a blank slate */
			result = 0;

			/* bits 5:0 are the PCI FIFO free space */
			result |= 0x3f << 0;

			/* bit 6 is the vertical retrace */
			//result |= v->fbi.vblank << 6;
			result |= (Voodoo_GetRetrace() ? 0x40 : 0);

			if (v->pci.op_pending) {
				// bit 7 is FBI graphics engine busy
				// bit 8 is TREX busy
				// bit 9 is overall busy
				result |= (b7 | b8 | b9);
			}

			/* bits 11:10 specifies which buffer is visible */
			result |= v->fbi.frontbuf << 10;

			/* bits 27:12 indicate memory FIFO freespace */
			result |= 0xffff << 12;

			/* bits 30:28 are the number of pending swaps */
			// result |= 0 << 28; // TODO: pending swaps are not currently trackedgit

			/* bit 31 is not used */


			break;

		case hvRetrace:
			if (vtype < VOODOO_2) {
				break;
			}

			/* start with a blank slate */
			result = 0;

			result |= ((uint32_t)(Voodoo_GetVRetracePosition() * 0x1fff)) &
						0x1fff;
			result |= (((uint32_t)(Voodoo_GetHRetracePosition() * 0x7ff)) &
						0x7ff)
					<< 16;

			break;

		/* bit 2 of the initEnable register maps this to dacRead */
		case fbiInit2:
			if (INITEN_REMAP_INIT_TO_DAC(v->pci.init_enable)) {
				result = v->dac.read_result;
			}
			break;

		/*
		case fbiInit3:
			if (INITEN_REMAP_INIT_TO_DAC(v->pci.init_enable))
				result = 0;
			break;

		case fbiInit6:
			if (vtype < VOODOO_2)
				break;
			result &= 0xffffe7ff;
			result |= 0x1000;
			break;
		*/

		/* all counters are 24-bit only */
		case fbiPixelsIn:
		case fbiChromaFail:
		case fbiZfuncFail:
		case fbiAfuncFail:
		case fbiPixelsOut:
			update_statistics(v, true);
			[[fallthrough]];
		case fbiTrianglesOut:
			result = v->reg[regnum].u & 0xffffff;
			break;

	}

	return result;
}

/*************************************
 *
 *  Handle an LFB read
 *
 *************************************/
static uint32_t lfb_r(const uint32_t offset)
{
	//LOG(LOG_VOODOO,LOG_WARN)("Voodoo:read LFB offset %X", offset);
	uint16_t* buffer = {};
	uint32_t bufmax  = 0;
	uint32_t data    = 0;

	/* compute X,Y */
	const auto x = (offset << 1) & 0x3fe;
	const auto y = (offset >> 9) & 0x3ff;

	/* select the target buffer */
	const auto destbuf = LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u);
	switch (destbuf)
	{
		case 0:			/* front buffer */
			buffer = (uint16_t *)(v->fbi.ram + v->fbi.rgboffs[v->fbi.frontbuf]);
			bufmax = (v->fbi.mask + 1 - v->fbi.rgboffs[v->fbi.frontbuf]) / 2;
			break;

		case 1:			/* back buffer */
			buffer = (uint16_t *)(v->fbi.ram + v->fbi.rgboffs[v->fbi.backbuf]);
			bufmax = (v->fbi.mask + 1 - v->fbi.rgboffs[v->fbi.backbuf]) / 2;
			break;

		case 2:			/* aux buffer */
			if (v->fbi.auxoffs == (uint32_t)(~0)) {
				return 0xffffffff;
			}
			buffer = (uint16_t *)(v->fbi.ram + v->fbi.auxoffs);
			bufmax = (v->fbi.mask + 1 - v->fbi.auxoffs) / 2;
			break;

		default:		/* reserved */
			return 0xffffffff;
	}

	/* determine the screen Y */
	auto scry = y;
	if (LFBMODE_Y_ORIGIN(v->reg[lfbMode].u)) {
		scry = (v->fbi.yorigin - y) & 0x3ff;
	}

#ifdef C_ENABLE_VOODOO_OPENGL
	if (v->ogl && v->active) {
		data = voodoo_ogl_read_pixel(x, scry+1);
	} else
#endif
	{
		/* advance pointers to the proper row */
		const auto bufoffs = scry * v->fbi.rowpixels + x;
		if (bufoffs >= bufmax) {
			return 0xffffffff;
		}

		/* compute the data */
		data = buffer[bufoffs + 0] | (buffer[bufoffs + 1] << 16);
	}

	/* word swapping */
	if (LFBMODE_WORD_SWAP_READS(v->reg[lfbMode].u)) {
		data = (data << 16) | (data >> 16);
	}

	/* byte swizzling */
	if (LFBMODE_BYTE_SWIZZLE_READS(v->reg[lfbMode].u)) {
		data = bswap_u32(data);
	}

	if (LOG_LFB != 0u) {
		LOG(LOG_VOODOO, LOG_WARN)
		("VOODOO.LFB:read (%d,%d) = %08X\n", x, y, data);
	}
	return data;
}

constexpr uint32_t offset_mask = 0x3fffff;
constexpr uint32_t offset_base = 0xc00000 / 4;
constexpr uint32_t lfb_base    = 0x800000 / 4;

constexpr uint32_t next_addr(const uint32_t addr)
{
	constexpr uint8_t next_offset = 1 << 2;
	return addr + next_offset;
}

static void voodoo_w(const uint32_t addr, const uint32_t data, const uint32_t mask)
{
	const auto offset = (addr >> 2) & offset_mask;

	if ((offset & offset_base) == 0) {
		register_w(offset, data);
	} else if ((offset & lfb_base) == 0) {
		lfb_w(offset, data, mask);
	} else {
		texture_w(offset, data);
	}
}

static constexpr uint32_t voodoo_r(const uint32_t addr)
{
	const auto offset = (addr >> 2) & offset_mask;

	if ((offset & offset_base) == 0) {
		return register_r(offset);
	}
	if ((offset & lfb_base) == 0) {
		return lfb_r(offset);
	}
	return 0xffffffff;
}

// Get the number of total threads to use for Voodoo work based on the user's
// conf setting. By default we use up to 16 threads (which includes the main
// thread) however the user can customize this.

static int get_num_total_threads()
{
	constexpr auto MinThreads     = 1;
	constexpr auto MaxAutoThreads = 16;
	constexpr auto MaxThreads     = 128;

	constexpr auto SectionName = "voodoo";
	constexpr auto SettingName = "voodoo_threads";
	constexpr auto AutoSetting = "auto";

	const auto sec = dynamic_cast<SectionProp*>(control->GetSection(SectionName));
	const auto user_setting = sec ? sec->GetString(SettingName) : AutoSetting;

	if (const auto maybe_int = parse_int(user_setting)) {
		const auto valid_int = std::clamp(*maybe_int, MinThreads, MaxThreads);

		// Use a property to test and warn if the value's outside the range
		constexpr auto always_changeable = Property::Changeable::Always;
		auto range_property = PropInt(SettingName, always_changeable, valid_int);
		range_property.SetMinMax(MinThreads, MaxThreads);

		if (!range_property.IsValidValue(*maybe_int)) {
			set_section_property_value(SectionName,
			                           SettingName,
			                           std::to_string(valid_int));
		}
		return valid_int;
	}

	if (user_setting != AutoSetting) {
		LOG_WARNING("VOODOO: Invalid '%s' setting: '%s', using '%s'",
		            SettingName,
		            user_setting.c_str(),
		            AutoSetting);

		set_section_property_value(SectionName, SettingName, AutoSetting);
	}

	return std::clamp(SDL_GetCPUCount(), MinThreads, MaxAutoThreads);
}

/***************************************************************************
    DEVICE INTERFACE
***************************************************************************/

/*-------------------------------------------------
    device start callback
-------------------------------------------------*/
static void voodoo_init() {
	assert(!v);

	// Deduct 1 because the main thread is always present
	const auto num_additional_threads = get_num_total_threads() - 1;

	v = new voodoo_state(num_additional_threads);

#ifdef C_ENABLE_VOODOO_OPENGL
	v->ogl = (emulation_type == VOODOO_EMU_TYPE_ACCELERATED);
#endif

	v->active = false;

	memset(v->reg, 0, sizeof(v->reg));

	v->fbi.vblank_flush_pending = false;
	v->pci.op_pending = false;
	v->dac.read_result = 0;

	v->output_on = false;
	v->clock_enabled = false;
#ifdef C_ENABLE_VOODOO_OPENGL
	v->ogl_dimchange = true;
#endif
	v->send_config = false;

	memset(v->dac.reg, 0, sizeof(v->dac.reg));

#ifdef C_ENABLE_VOODOO_OPENGL
	v->next_rasterizer = 0;
	//for (uint32_t rct=0; rct<MAX_RASTERIZERS; rct++)
	//	v->rasterizer[rct] = raster_info();
	memset(v->rasterizer, 0, sizeof(v->rasterizer));
	memset(v->raster_hash, 0, sizeof(v->raster_hash));
#endif

	update_statistics(v, false);

	v->alt_regmap = false;
#ifdef C_ENABLE_VOODOO_DEBUG
	v->regnames = voodoo_reg_name;
#endif

	if (*voodoo_reciplog == 0u)
	{
		// Create a table of precomputed 1/n and log2(n) values where n
		// ranges from 1.0000 to 2.0000
		constexpr auto steps   = 1 << RECIPLOG_LOOKUP_BITS;
		constexpr double width = 1 << RECIPLOG_LOOKUP_PREC;
		auto lut_val = voodoo_reciplog;

		for (auto i = 0; i <= steps; ++i) {
			const double n = steps + i;

			const auto inverse_of_n = steps * width / n;
			*lut_val++ = static_cast<uint32_t>(inverse_of_n);

			const auto log2_of_n = std::log2(n / steps) * width;
			*lut_val++ = static_cast<uint32_t>(log2_of_n);
		}

		dither2_lookup = generate_dither_lut(dither_matrix_2x2);
		dither4_lookup = generate_dither_lut(dither_matrix_4x4);

		/* create sse2 scale table for rgba_bilinear_filter */
		for (int i = 0; i != 256; i++) {
			sse2_scale_table[i] = simde_mm_setr_epi16(
			        i, 256 - i, i, 256 - i, i, 256 - i, i, 256 - i);
		}
	}

	v->tmu_config = 0x11;	// revision 1

	uint32_t fbmemsize = 0;
	uint32_t tmumem0 = 0;
	uint32_t tmumem1 = 0;

	/* configure type-specific values */
	switch (vtype)
	{
		case VOODOO_1:
			v->regaccess = voodoo_register_access;
			fbmemsize = 2;
			tmumem0 = 2;
			break;

		case VOODOO_1_DTMU:
			v->regaccess = voodoo_register_access;
			fbmemsize = 4;
			tmumem0 = 4;
			tmumem1 = 4;
			break;

		/*
		// As is now this crashes in Windows 9x trying to run a game with Voodoo 2 drivers installed (raster_generic tries to write into a frame buffer at an invalid memory location)
		case VOODOO_2:
			v->regaccess = voodoo2_register_access;
			fbmemsize = 4;
			tmumem0 = 4;
			tmumem1 = 4;
			v->tmu_config |= 0x800;
			break;
		*/

	        default: break;
	}
	// Sanity-check sizes
	assert(fbmemsize > 0);
	assert(tmumem0 > 0);

	if (tmumem1 != 0) {
		v->tmu_config |= 0xc0;	// two TMUs
	}

	v->chipmask = 0x01;

	/* set up the PCI FIFO */
	v->pci.fifo.size = 64*2;

	/* set up frame buffer */
	init_fbi(&v->fbi, fbmemsize << 20);

	v->fbi.rowpixels = v->fbi.width;

	v->tmu[0].ncc[0].palette = nullptr;
	v->tmu[0].ncc[1].palette = nullptr;
	v->tmu[1].ncc[0].palette = nullptr;
	v->tmu[1].ncc[1].palette = nullptr;
	v->tmu[0].ncc[0].palettea = nullptr;
	v->tmu[0].ncc[1].palettea = nullptr;
	v->tmu[1].ncc[0].palettea = nullptr;
	v->tmu[1].ncc[1].palettea = nullptr;

	v->tmu[0].ram = nullptr;
	v->tmu[1].ram = nullptr;
	v->tmu[0].lookup = nullptr;
	v->tmu[1].lookup = nullptr;

	/* build shared TMU tables */
	init_tmu_shared(&v->tmushare);

	/* set up the TMUs */
	init_tmu(v, &v->tmu[0], &v->reg[0x100], tmumem0 << 20);
	v->chipmask |= 0x02;
	if (tmumem1 != 0)
	{
		init_tmu(v, &v->tmu[1], &v->reg[0x200], tmumem1 << 20);
		v->chipmask |= 0x04;
		v->tmu_config |= 0x40;
	}

	/* initialize some registers */
	v->pci.init_enable = 0;
	v->reg[fbiInit0].u = (uint32_t)((1 << 4) | (0x10 << 6));
	v->reg[fbiInit1].u = (uint32_t)((1 << 1) | (1 << 8) | (1 << 12) | (2 << 20));
	v->reg[fbiInit2].u = (uint32_t)((1 << 6) | (0x100 << 23));
	v->reg[fbiInit3].u = (uint32_t)((2 << 13) | (0xf << 17));
	v->reg[fbiInit4].u = (uint32_t)(1 << 0);

	/* do a soft reset to reset everything else */
	soft_reset(v);

	recompute_video_memory(v);
}

static void voodoo_vblank_flush()
{
#ifdef C_ENABLE_VOODOO_OPENGL
	if (v->ogl)
		voodoo_ogl_vblank_flush();
#endif
	v->fbi.vblank_flush_pending=false;
}

#ifdef C_ENABLE_VOODOO_OPENGL
static void voodoo_set_window(void) {
	if (v->ogl && v->active) {
		voodoo_ogl_set_window(v);
	}
}
#endif

static void voodoo_leave()
{
#ifdef C_ENABLE_VOODOO_OPENGL
	if (v->ogl) {
		voodoo_ogl_leave(true);
	}
#endif
	v->active = false;
}

static void voodoo_activate()
{
	v->active = true;

#ifdef C_ENABLE_VOODOO_OPENGL
	if (v->ogl) {
		if (voodoo_ogl_init(v)) {
			voodoo_ogl_clear();
		} else {
			v->ogl = false;
			LOG_MSG("VOODOO: acceleration disabled");
		}
	}
#endif
}

#ifdef C_ENABLE_VOODOO_OPENGL
static void voodoo_update_dimensions(void) {
	v->ogl_dimchange = false;

	if (v->ogl) {
		voodoo_ogl_update_dimensions();
	}
}
#endif

static void Voodoo_VerticalTimer(uint32_t /*val*/)
{
	v->draw.frame_start = PIC_FullIndex();
	PIC_AddEvent(Voodoo_VerticalTimer, v->draw.frame_period_ms);

	if (v->fbi.vblank_flush_pending) {
		voodoo_vblank_flush();
#ifdef C_ENABLE_VOODOO_OPENGL
		if (GFX_LazyFullscreenRequested()) {
			v->ogl_dimchange = true;
		}
#endif
	}

#ifdef C_ENABLE_VOODOO_OPENGL
	if (!v->ogl)
#endif
	{
		if (!RENDER_StartUpdate()) {
			return; // frameskip
		}

#ifdef C_ENABLE_VOODOO_DEBUG
		rectangle r;
		r.min_x = r.min_y = 0;
		r.max_x = (int)v->fbi.width;
		r.max_y = (int)v->fbi.height;
#endif

		// draw all lines at once
		auto* viewbuf = (uint16_t*)(v->fbi.ram +
		                            v->fbi.rgboffs[v->fbi.frontbuf]);
		for(Bitu i = 0; i < v->fbi.height; i++) {
			RENDER_DrawLine((uint8_t*)viewbuf);
			viewbuf += v->fbi.rowpixels;
		}
		RENDER_EndUpdate(false);
	}
#ifdef C_ENABLE_VOODOO_OPENGL
	else {
		// ???
		voodoo_set_window();
	}
#endif
}

static bool Voodoo_GetRetrace() {
	// TODO proper implementation
	const auto time_in_frame = PIC_FullIndex() - v->draw.frame_start;
	const auto frame_period_ms = v->draw.frame_period_ms;
	if (frame_period_ms <= 0) {
		return false;
	}
	if (v->clock_enabled && v->output_on) {
		if ((time_in_frame / frame_period_ms) > 0.95) {
			return true;
		}
	} else if (v->output_on) {
		auto rtime = time_in_frame / frame_period_ms;
		rtime = fmod(rtime, 1.0);
		if (rtime > 0.95) {
			return true;
		}
	}
	return false;
}

static double Voodoo_GetVRetracePosition() {
	// TODO proper implementation
	const auto time_in_frame = PIC_FullIndex() - v->draw.frame_start;
	const auto frame_period_ms = v->draw.frame_period_ms;
	if (frame_period_ms <= 0) {
		return 0.0;
	}
	if (v->clock_enabled && v->output_on) {
		return time_in_frame / frame_period_ms;
	}
	if (v->output_on) {
		auto rtime = time_in_frame / frame_period_ms;
		rtime = fmod(rtime, 1.0);
		return rtime;
	}
	return 0.0;
}

static double Voodoo_GetHRetracePosition() {
	// TODO proper implementation
	const auto time_in_frame = PIC_FullIndex() - v->draw.frame_start;

	const auto hfreq = v->draw.frame_period_ms * 100;

	if (hfreq <= 0) {
		return 0.0;
	}
	if (v->clock_enabled && v->output_on) {
		return time_in_frame/hfreq;
	}
	if (v->output_on) {
		auto rtime = time_in_frame / hfreq;
		rtime = fmod(rtime, 1.0);
		return rtime;
	}
	return 0.0;
}

static void Voodoo_UpdateScreen()
{
	// abort drawing
	RENDER_EndUpdate(true);

	if ((!v->clock_enabled || !v->output_on) && v->draw.override_on) {
		// switching off
		PIC_RemoveEvents(Voodoo_VerticalTimer);
		voodoo_leave();

		// Let the underlying VGA card resume rendering
		VGA_SetOverride(false);
		v->draw.override_on=false;
	}

	if ((v->clock_enabled && v->output_on) && !v->draw.override_on) {
		// switching on
		PIC_RemoveEvents(Voodoo_VerticalTimer); // shouldn't be needed

		// Indicate to the underlying VGA card that it should stop
		// rendering. This is equivalent to when the Voodoo card
		// switched from passive pass-thru mode to active output mode.
		//
		VGA_SetOverride(true, VoodooDefaultRefreshRateHz);

		v->draw.frame_period_ms = 1000.0 / VGA_GetRefreshRate();
		//
		// The user's 'dos_rate' preference controls the preferred rate.
		// When set to 'auto', we'll get back the Voodoo default rate.
		// Otherwise we'll get the user's custom rate.

		v->draw.override_on = true;

		voodoo_activate();

#ifdef C_ENABLE_VOODOO_OPENGL
		if (v->ogl) {
			v->ogl_dimchange = false;
		} else
#endif
		{
			ImageInfo image_info = {};

			const auto width  = check_cast<uint16_t>(v->fbi.width);
			const auto height = check_cast<uint16_t>(v->fbi.height);

			image_info.width                = width;
			image_info.height               = height;
			image_info.double_width         = false;
			image_info.double_height        = false;
			image_info.forced_single_scan   = false;
			image_info.rendered_double_scan = false;
			image_info.pixel_aspect_ratio   = {1};
			image_info.pixel_format = PixelFormat::RGB565_Packed16;

			VideoMode video_mode = {};

			video_mode.bios_mode_number   = 0;
			video_mode.is_custom_mode     = false;
			video_mode.is_graphics_mode   = true;
			video_mode.width              = width;
			video_mode.height             = height;
			video_mode.pixel_aspect_ratio = {1};
			video_mode.graphics_standard  = GraphicsStandard::Svga;
			video_mode.color_depth = ColorDepth::HighColor16Bit;
			video_mode.is_double_scanned_mode = false;
			video_mode.has_vga_colors         = false;

			image_info.video_mode = video_mode;

			const auto frames_per_second = static_cast<float>(
			        1000.0 / v->draw.frame_period_ms);

			constexpr auto reinit_render = false;
			RENDER_MaybeAutoSwitchShader(GFX_GetCanvasSizeInPixels(), video_mode, reinit_render);

			RENDER_SetSize(image_info, frames_per_second);
		}

		Voodoo_VerticalTimer(0);
	}

#ifdef C_ENABLE_VOODOO_OPENGL
	if ((v->clock_enabled && v->output_on) && v->ogl_dimchange) {
		voodoo_update_dimensions();
	}
#endif

	v->draw.screen_update_requested = false;
}

static void Voodoo_CheckScreenUpdate(uint32_t /*val*/)
{
	v->draw.screen_update_pending = false;
	if (v->draw.screen_update_requested) {
		v->draw.screen_update_pending = true;
		Voodoo_UpdateScreen();
		PIC_AddEvent(Voodoo_CheckScreenUpdate, 100.0);
	}
}

static void Voodoo_UpdateScreenStart() {
	v->draw.screen_update_requested = true;
	if (!v->draw.screen_update_pending) {
		v->draw.screen_update_pending = true;
		PIC_AddEvent(Voodoo_CheckScreenUpdate, 0.0);
	}
}

static void Voodoo_Startup();

static struct Voodoo_Real_PageHandler : public PageHandler {
	Voodoo_Real_PageHandler() { flags = PFLAG_NOCODE; }

	uint8_t readb([[maybe_unused]] PhysPt addr) override
	{
		// maybe_log_debug("readb at %x", addr);
		return 0xff;
	}

	void writeb([[maybe_unused]] PhysPt addr, [[maybe_unused]] uint8_t val) override
	{
		// maybe_log_debug("writeb at %x", addr);
	}

	uint16_t readw(PhysPt addr) override
	{
		addr = PAGING_GetPhysicalAddress(addr);
		const auto val = voodoo_r(addr);

		// Is the address word-aligned?
		if ((addr & 0b11) == 0) {
			return static_cast<uint16_t>(val & 0xffff);
		}
		// The address must be byte-aligned
		assert((addr & 0b1) == 0);
		return static_cast<uint16_t>(val >> 16);
	}

	void writew(PhysPt addr, const uint16_t val) override
	{
		addr = PAGING_GetPhysicalAddress(addr);

		// When writing 16-bit words bit 0 of the address must be
		// cleared, indicating the address is neither 8-bit nor 24-bit
		// aligned.

		assert((addr & 0b1) == 0);

		// With bit 0 is cleared, bit 1's state (set or cleared)
		// determines if the address is 16-bit or 32-bit aligned,
		// respectively. 16-bit alignment requires the value be written
		// in the next word where as 32-bit alignment allows the value
		// to be written without shifting. The shift is either 0 or 16.

		const auto shift = (addr & 0b10) << 3;

		const auto shifted_val = static_cast<uint32_t>(val) << shift;
		const auto shifted_mask = static_cast<uint32_t>(0xffff) << shift;

		voodoo_w(addr, shifted_val, shifted_mask);
	}

	uint32_t readd(PhysPt addr) override
	{
		addr = PAGING_GetPhysicalAddress(addr);

		// Is the address word-aligned?
		if ((addr & 0b11) == 0) {
			return voodoo_r(addr);
		}
		// The address must be byte-aligned
		assert((addr & 0b1) == 0);
		const auto low  = voodoo_r(addr);
		const auto high = voodoo_r(next_addr(addr));
		return check_cast<uint32_t>((low >> 16) | (high << 16));
	}

	void writed(PhysPt addr, uint32_t val) override
	{
		addr = PAGING_GetPhysicalAddress(addr);
		if ((addr&3) == 0u) {
			voodoo_w(addr, val, 0xffffffff);
		} else if ((addr&1) == 0u) {
			voodoo_w(addr, val << 16, 0xffff0000);
			voodoo_w(next_addr(addr), val, 0x0000ffff);
		} else {
			auto val1 = voodoo_r(addr);
			auto val2 = voodoo_r(next_addr(addr));
			if ((addr & 3) == 1) {
				val1 = (val1&0xffffff) | ((val&0xff)<<24);
				val2 = (val2&0xff000000) | (val>>8);
			} else if ((addr & 3) == 3) {
				val1 = (val1&0xff) | ((val&0xffffff)<<8);
				val2 = (val2&0xffffff00) | (val>>24);
			}
			voodoo_w(addr, val1, 0xffffffff);
			voodoo_w(next_addr(addr), val2, 0xffffffff);
		}
	}
} voodoo_real_pagehandler;

static struct Voodoo_Init_PageHandler : public PageHandler {
	Voodoo_Init_PageHandler() { flags = PFLAG_NOCODE; }

	uint8_t readb([[maybe_unused]] PhysPt addr) override
	{
		return 0xff;
	}

	uint16_t readw(PhysPt addr) override
	{
		Voodoo_Startup();
		return voodoo_real_pagehandler.readw(addr);
	}

	uint32_t readd(PhysPt addr) override
	{
		Voodoo_Startup();
		return voodoo_real_pagehandler.readd(addr);
	}

	void writeb([[maybe_unused]] PhysPt addr, [[maybe_unused]] uint8_t val) override
	{}

	void writew(PhysPt addr, uint16_t val) override
	{
		Voodoo_Startup();
		voodoo_real_pagehandler.writew(addr, val);
	}

	void writed(PhysPt addr, uint32_t val) override
	{
		Voodoo_Startup();
		voodoo_real_pagehandler.writed(addr, val);
	}

} voodoo_init_pagehandler;

enum {
	VOODOO_REG_PAGES = 1024,
	VOODOO_LFB_PAGES = 1024,
	VOODOO_TEX_PAGES = 2048
};

#define VOODOO_PAGES (VOODOO_REG_PAGES + VOODOO_LFB_PAGES + VOODOO_TEX_PAGES)
static_assert(PciVoodooLfbBase + (VOODOO_PAGES * MemPageSize) <= PciVoodooLfbLimit);

#ifdef C_ENABLE_VOODOO_OPENGL
#define VOODOO_EMU_TYPE_OFF         0
#define VOODOO_EMU_TYPE_SOFTWARE    1
#define VOODOO_EMU_TYPE_ACCELERATED 2
#endif

static uint32_t voodoo_current_lfb;
static PageHandler* voodoo_pagehandler;

struct PCI_SSTDevice : public PCI_Device {
	enum : uint16_t {
		vendor          = 0x121a,
		device_voodoo_1 = 0x0001,
		device_voodoo_2 = 0x0002
	}; // 0x121a = 3dfx

	uint16_t oscillator_ctr = 0;
	uint16_t pci_ctr = 0;

	PCI_SSTDevice() : PCI_Device(vendor, device_voodoo_1) {}

	void SetDeviceId(uint16_t _device_id)
	{
		device_id = _device_id;
	}

	Bits ParseReadRegister(uint8_t regnum) override
	{
		// maybe_log_debug("SST ParseReadRegister %x",regnum);
		switch (regnum) {
			case 0x4c:case 0x4d:case 0x4e:case 0x4f:
				maybe_log_debug("SST ParseReadRegister STATUS %x",regnum);
				break;
			case 0x54:case 0x55:case 0x56:case 0x57:
				if (vtype == VOODOO_2) {
					return -1;
				}
				break;
		}
		return regnum;
	}

	bool OverrideReadRegister(uint8_t regnum, uint8_t* rval, uint8_t* rval_mask) override
	{
		if (vtype != VOODOO_2) {
			return false;
		}
		switch (regnum) {
		case 0x54:
			oscillator_ctr++;
			pci_ctr--;
			*rval = static_cast<uint8_t>(
			        (oscillator_ctr | ((pci_ctr << 16) & 0x0fff0000)) &
			        0xff);
			*rval_mask = 0xff;
			return true;
		case 0x55:
			*rval = static_cast<uint8_t>(
			        ((oscillator_ctr | ((pci_ctr << 16) & 0x0fff0000)) >> 8) &
			        0xff);
			*rval_mask = 0xff;
			return true;
		case 0x56:
			*rval = static_cast<uint8_t>(
			        ((oscillator_ctr | ((pci_ctr << 16) & 0x0fff0000)) >> 16) &
			        0xff);
			*rval_mask = 0xff;
			return true;
		case 0x57:
			*rval = static_cast<uint8_t>(
			        ((oscillator_ctr | ((pci_ctr << 16) & 0x0fff0000)) >> 24) &
			        0xff);
			*rval_mask = 0x0f;
			return true;
		}
		return false;
	}

	Bits ParseWriteRegister(uint8_t regnum, uint8_t value) override
	{
		// maybe_log_debug("SST ParseWriteRegister %x:=%x",regnum,value);
		if ((regnum >= 0x14) && (regnum < 0x28)) {
			return -1; // base addresses are read-only
		}
		if ((regnum >= 0x30) && (regnum < 0x34)) {
			return -1; // expansion rom addresses are read-only
		}
		switch (regnum) {
		case 0x10:

			return (PCI_GetCFGData(this->PCIId(),
			                       this->PCISubfunction(),
			                       0x10) &
			        0x0f);
		case 0x11: return 0x00;
		case 0x12:
			return (value & 0x00); // -> 16mb addressable (whyever)
		case 0x13:
			voodoo_current_lfb = ((value << 24) & 0xffff0000);
			return value;
		case 0x40:
			Voodoo_Startup();
			v->pci.init_enable = (uint32_t)(value & 7);
			break;
		case 0x41:
		case 0x42:
		case 0x43: return -1;
		case 0xc0:
			Voodoo_Startup();
			v->clock_enabled = true;
			Voodoo_UpdateScreenStart();
			return -1;
		case 0xe0:
			Voodoo_Startup();
			v->clock_enabled = false;
			Voodoo_UpdateScreenStart();
			return -1;
		default: break;
		}
		return value;
	}

	bool InitializeRegisters(uint8_t registers[256]) override
	{
		// init (3dfx voodoo)
		registers[0x08] = 0x02;	// revision
		registers[0x09] = 0x00;	// interface
		//registers[0x0a] = 0x00;	// subclass code
		registers[0x0a] = 0x00;	// subclass code (video/graphics controller)
		//registers[0x0b] = 0x00;	// class code (generic)
		registers[0x0b] = 0x04;	// class code (multimedia device)
		registers[0x0e] = 0x00;	// header type (other)

		// reset
		registers[0x04] = 0x02;	// command register (memory space enabled)
		registers[0x05] = 0x00;
		registers[0x06] = 0x80;	// status register (fast back-to-back)
		registers[0x07] = 0x00;

		registers[0x3c] = 0xff;	// no irq

		// BAR0 - memory space, within first 4GB
		// Check 8-byte alignment of LFB base
		static_assert((PciVoodooLfbBase & 0xf) == 0);
		constexpr uint32_t address_space = PciVoodooLfbBase | 0x08;
		registers[0x10] = static_cast<uint8_t>(address_space & 0xff);
		registers[0x11] = static_cast<uint8_t>((address_space >> 8) & 0xff);
		registers[0x12] = static_cast<uint8_t>((address_space >> 16) & 0xff);
		registers[0x13] = static_cast<uint8_t>((address_space >> 24) & 0xff);

		if (vtype == VOODOO_2) {
			registers[0x40] = 0x00;
			registers[0x41] = 0x40;	// voodoo2 revision ID (rev4)
			registers[0x42] = 0x01;
			registers[0x43] = 0x00;
		}

		return true;
	}
};

static void voodoo_shutdown()
{
	if (!v) {
		return;
	}
	LOG_MSG("VOODOO: Shutting down");

#ifdef C_ENABLE_VOODOO_OPENGL
	if (v->ogl) {
		voodoo_ogl_shutdown(v);
	}
#endif

	v->active = false;
	triangle_worker_shutdown(v->tworker);

	delete v;
	v = nullptr;

	PCI_RemoveDevice(PCI_SSTDevice::vendor, PCI_SSTDevice::device_voodoo_1);
}

static void Voodoo_Startup() {
	// This function is called delayed after booting only once a game actually requests Voodoo support
	if (v != nullptr) {
		return;
	}

	voodoo_init();

	v->draw = {};

	v->tworker.disable_bilinear_filter = (voodoo_bilinear_filtering == false);

	// Switch the pagehandler now that v has been allocated and is in use
	voodoo_pagehandler = &voodoo_real_pagehandler;
	PAGING_InitTLB();
}

PageHandler* VOODOO_PCI_GetLFBPageHandler(Bitu page) {
	return (page >= (voodoo_current_lfb>>12) && page < (voodoo_current_lfb>>12) + VOODOO_PAGES ? voodoo_pagehandler : nullptr);
}


static void voodoo_destroy(Section* /*sec*/) {
	voodoo_shutdown();
}

static void voodoo_init(Section* sec)
{
	auto* section = dynamic_cast<SectionProp*>(sec);

	// Only activate on SVGA machines and when requested
	if (!is_machine_svga() || !section || !section->GetBool("voodoo")) {
		return;
	}
	//
	const std::string memsize_pref = section->GetString("voodoo_memsize");
	vtype = (memsize_pref == "4" ? VOODOO_1 : VOODOO_1_DTMU);

	voodoo_bilinear_filtering = section->GetBool("voodoo_bilinear_filtering");

	sec->AddDestroyFunction(&voodoo_destroy, false);

	// Check 64 KB alignment of LFB base
	static_assert((PciVoodooLfbBase & 0xffff) == 0);

	voodoo_current_lfb = PciVoodooLfbBase;
	voodoo_pagehandler = &voodoo_init_pagehandler;

	PCI_AddDevice(new PCI_SSTDevice());

	// Log the startup
	const auto num_threads = get_num_total_threads();

	LOG_MSG("VOODOO: Initialized with %s MB of RAM, %d %s, and %sbilinear filtering",
	        memsize_pref.c_str(),
	        num_threads,
	        num_threads == 1 ? "thread" : "threads",
	        (voodoo_bilinear_filtering ? "" : "no "));
}

static void init_voodoo_dosbox_settings(SectionProp& secprop)
{
	constexpr auto Deprecated  = Property::Changeable::Deprecated;
	constexpr auto OnlyAtStart = Property::Changeable::OnlyAtStart;
	constexpr auto WhenIdle    = Property::Changeable::WhenIdle;

	auto* bool_prop = secprop.AddBool("voodoo", WhenIdle, true);
	bool_prop->SetHelp(
	        "Enable 3dfx Voodoo emulation ('on' by default). This is authentic low-level\n"
	        "emulation of the Voodoo card without any OpenGL passthrough, so it requires a\n"
	        "powerful CPU. Most games need the DOS Glide driver called 'GLIDE2X.OVL' to be\n"
	        "in the path for 3dfx mode to work. Many games include their own Glide driver\n"
	        "variants, but for some you need to provide a suitable 'GLIDE2X.OVL' version.\n"
	        "A small number of games integrate the Glide driver into their code, so they\n"
	        "don't need 'GLIDE2X.OVL'.");

	auto* str_prop = secprop.AddString("voodoo_memsize", OnlyAtStart, "4");
	str_prop->SetValues({"4", "12"});
	str_prop->SetHelp(
	        "Set the amount of video memory for 3dfx Voodoo graphics. The memory is used by\n"
	        "the Frame Buffer Interface (FBI) and Texture Mapping Unit (TMU) as follows:\n"
	        "   4: 2 MB for the FBI and one TMU with 2 MB (default).\n"
	        "  12: 4 MB for the FBI and two TMUs, each with 4 MB.");

	// Deprecate the boolean Voodoo multithreading setting
	bool_prop = secprop.AddBool("voodoo_multithreading", Deprecated, false);
	bool_prop->SetHelp("Renamed to 'voodoo_threads'");

	str_prop = secprop.AddString("voodoo_threads", OnlyAtStart, "auto");
	str_prop->SetHelp(
	        "Use threads to improve 3dfx Voodoo performance:\n"
	        "  auto:     Use up to 16 threads based on available CPU cores (default).\n"
	        "  <value>:  Set a specific number of threads between 1 and 128.\n"
	        "Note: Setting this to a higher value than the number of logical CPUs your\n"
	        "      hardware supports is very likely to harm performance. This has been\n"
	        "      measured to scale well up to 8-16 threads, but it has not been tested\n"
	        "      on a many-core CPU. If you have a Threadripper or similar CPU, please\n"
	        "      let us know how it goes.");

	bool_prop = secprop.AddBool("voodoo_bilinear_filtering", OnlyAtStart, true);
	bool_prop->SetHelp(
	        "Use bilinear filtering to emulate the 3dfx Voodoo's texture smoothing effect\n"
	        "('on' by default). Bilinear filtering can impact frame rates on slower systems;\n"
	        "try turning it off if you're not getting adequate performance.");
}

void VOODOO_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	SectionProp* sec = conf->AddSectionProp("voodoo", &voodoo_init);
	assert(sec);
	init_voodoo_dosbox_settings(*sec);
}

