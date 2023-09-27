/*
 *  SPDX-License-Identifier: BSD-3-Clause AND GPL-2.0-or-later
 *
 *  copyright-holders: Aaron Giles, kekko, Bernhard Schelling
 */

/*
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2011  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

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
#include <cassert>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include <SDL.h>
#include <SDL_cpuinfo.h> // for proper SSE defines for MSVC

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

#include "bitops.h"
#include "byteorder.h"
#include "control.h"
#include "cross.h"
#include "fraction.h"
#include "math_utils.h"
#include "mem.h"
#include "paging.h"
#include "pci_bus.h"
#include "pic.h"
#include "render.h"
#include "semaphore.h"
#include "setup.h"
#include "support.h"
#include "vga.h"

#ifndef DOSBOX_VOODOO_TYPES_H
#define DOSBOX_VOODOO_TYPES_H

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
	int min_x = 0; // minimum X, or left coordinate
	int max_x = 0; // maximum X, or right coordinate (inclusive)
	int min_y = 0; // minimum Y, or top coordinate
	int max_y = 0; // maximum Y, or bottom coordinate (inclusive)
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

#if defined(__SSE2__)
static int16_t sse2_scale_table[256][8];
#endif

inline rgb_t rgba_bilinear_filter(rgb_t rgb00, rgb_t rgb01, rgb_t rgb10,
                                  rgb_t rgb11, uint8_t u, uint8_t v)
{
#if defined(__SSE2__)
	const __m128i scale_u = *(__m128i*)sse2_scale_table[u];
	const __m128i scale_v = *(__m128i*)sse2_scale_table[v];
	return _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packs_epi32(_mm_srli_epi32(_mm_madd_epi16(_mm_max_epi16(
		_mm_slli_epi32(_mm_madd_epi16(_mm_unpacklo_epi8(_mm_unpacklo_epi8(_mm_cvtsi32_si128(rgb01), _mm_cvtsi32_si128(rgb00)), _mm_setzero_si128()), scale_u), 15),
		_mm_srli_epi32(_mm_madd_epi16(_mm_unpacklo_epi8(_mm_unpacklo_epi8(_mm_cvtsi32_si128(rgb11), _mm_cvtsi32_si128(rgb10)), _mm_setzero_si128()), scale_u), 1)),
		scale_v), 15), _mm_setzero_si128()), _mm_setzero_si128()));
#else
	uint32_t ag0, ag1, rb0, rb1;
	rb0 = (rgb00 & 0x00ff00ff) + ((((rgb01 & 0x00ff00ff) - (rgb00 & 0x00ff00ff)) * u) >> 8);
	rb1 = (rgb10 & 0x00ff00ff) + ((((rgb11 & 0x00ff00ff) - (rgb10 & 0x00ff00ff)) * u) >> 8);
	rgb00 >>= 8;
	rgb01 >>= 8;
	rgb10 >>= 8;
	rgb11 >>= 8;
	ag0 = (rgb00 & 0x00ff00ff) + ((((rgb01 & 0x00ff00ff) - (rgb00 & 0x00ff00ff)) * u) >> 8);
	ag1 = (rgb10 & 0x00ff00ff) + ((((rgb11 & 0x00ff00ff) - (rgb10 & 0x00ff00ff)) * u) >> 8);
	rb0 = (rb0 & 0x00ff00ff) + ((((rb1 & 0x00ff00ff) - (rb0 & 0x00ff00ff)) * v) >> 8);
	ag0 = (ag0 & 0x00ff00ff) + ((((ag1 & 0x00ff00ff) - (ag0 & 0x00ff00ff)) * v) >> 8);
	return ((ag0 << 8) & 0xff00ff00) | (rb0 & 0x00ff00ff);
#endif
}

struct poly_vertex
{
	float x = 0.0f; // X coordinate
	float y = 0.0f; // Y coordinate

	// float p[MAX_VERTEX_PARAMS]; // interpolated parameter values
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
enum
{
	VOODOO_1,
	VOODOO_1_DTMU,
	VOODOO_2,
};

enum { TRIANGLE_THREADS = 3, TRIANGLE_WORKERS = TRIANGLE_THREADS + 1 };

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

#define EXTRACT_565_TO_888(val, a, b, c) \
	(a) = (((val) >> 8) & 0xf8) | (((val) >> 13) & 0x07); \
	(b) = (((val) >> 3) & 0xfc) | (((val) >> 9) & 0x03); \
	(c) = (((val) << 3) & 0xf8) | (((val) >> 2) & 0x07);

#define EXTRACT_x555_TO_888(val, a, b, c) \
	(a) = (((val) >> 7) & 0xf8) | (((val) >> 12) & 0x07); \
	(b) = (((val) >> 2) & 0xf8) | (((val) >> 7) & 0x07); \
	(c) = (((val) << 3) & 0xf8) | (((val) >> 2) & 0x07);

#define EXTRACT_555x_TO_888(val, a, b, c) \
	(a) = (((val) >> 8) & 0xf8) | (((val) >> 13) & 0x07); \
	(b) = (((val) >> 3) & 0xf8) | (((val) >> 8) & 0x07); \
	(c) = (((val) << 2) & 0xf8) | (((val) >> 3) & 0x07);

#define EXTRACT_1555_TO_8888(val, a, b, c, d) \
	(a) = ((int16_t)(val) >> 15) & 0xff; \
	EXTRACT_x555_TO_888(val, b, c, d)

#define EXTRACT_5551_TO_8888(val, a, b, c, d) \
	EXTRACT_555x_TO_888(val, a, b, c)(d) = ((val) & 0x0001) ? 0xff : 0x00;

#define EXTRACT_x888_TO_888(val, a, b, c) \
	(a) = ((val) >> 16) & 0xff; \
	(b) = ((val) >> 8) & 0xff; \
	(c) = ((val) >> 0) & 0xff;

#define EXTRACT_888x_TO_888(val, a, b, c) \
	(a) = ((val) >> 24) & 0xff; \
	(b) = ((val) >> 16) & 0xff; \
	(c) = ((val) >> 8) & 0xff;

#define EXTRACT_8888_TO_8888(val, a, b, c, d) \
	(a) = ((val) >> 24) & 0xff; \
	(b) = ((val) >> 16) & 0xff; \
	(c) = ((val) >> 8) & 0xff; \
	(d) = ((val) >> 0) & 0xff;

#define EXTRACT_4444_TO_8888(val, a, b, c, d) \
	(a) = (((val) >> 8) & 0xf0) | (((val) >> 12) & 0x0f); \
	(b) = (((val) >> 4) & 0xf0) | (((val) >> 8) & 0x0f); \
	(c) = (((val) >> 0) & 0xf0) | (((val) >> 4) & 0x0f); \
	(d) = (((val) << 4) & 0xf0) | (((val) >> 0) & 0x0f);

#define EXTRACT_332_TO_888(val, a, b, c) \
	(a) = (((val) >> 0) & 0xe0) | (((val) >> 3) & 0x1c) | \
	      (((val) >> 6) & 0x03); \
	(b) = (((val) << 3) & 0xe0) | (((val) >> 0) & 0x1c) | \
	      (((val) >> 3) & 0x03); \
	(c) = (((val) << 6) & 0xc0) | (((val) << 4) & 0x30) | \
	      (((val) << 2) & 0xc0) | (((val) << 0) & 0x03);

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
	uint8_t b, g, r, a;
#else
	uint8_t a, r, g, b;
#endif
};

union voodoo_reg
{
	int32_t i;
	uint32_t u;
	float f;
	rgba rgb;
};

using rgb_union = voodoo_reg;

enum class StatsCollection {
	Accumulate,
	Reset,
};

// note that this structure is an even 64 bytes long
struct Stats {
	int32_t pixels_in   = 0; // pixels in statistic
	int32_t pixels_out  = 0; // pixels out statistic
	int32_t chroma_fail = 0; // chroma test fail statistic
	int32_t zfunc_fail  = 0; // z function test fail statistic
	int32_t afunc_fail  = 0; // alpha function test fail statistic
	// int32_t clip_fail;       // clipping fail statistic
	// int32_t stipple_count;   // stipple statistic
	int32_t filler[64 / 4 - 5] = {}; // pad this structure to 64 bytes
};
static_assert(sizeof(Stats) == 64);

struct fifo_state
{
	int32_t size = 0; // size of the FIFO
};

struct pci_state
{
	fifo_state fifo      = {};    // PCI FIFO
	uint32_t init_enable = 0;     // initEnable value
	bool op_pending      = false; // true if an operation is pending
};

struct ncc_table
{
	bool dirty = false; // is the texel lookup dirty?

	voodoo_reg* reg = nullptr; // pointer to our registers

	int32_t ir[4] = {}; // I values for R,G,B
	int32_t ig[4] = {};
	int32_t ib[4] = {};

	int32_t qr[4] = {}; // Q values for R,G,B
	int32_t qg[4] = {};
	int32_t qb[4] = {};

	int32_t y[16] = {}; // Y values

	rgb_t* palette  = nullptr; // pointer to associated RGB palette
	rgb_t* palettea = nullptr; // pointer to associated ARGB palette

	rgb_t texel[256] = {}; // texel lookup
};

using mem_buffer_t = std::unique_ptr<uint8_t[]>;

struct tmu_shared_state {
	constexpr void Initialize();

	rgb_t rgb332[256]     = {}; // RGB 3-3-2 lookup table
	rgb_t alpha8[256]     = {}; // alpha 8-bit lookup table
	rgb_t int8[256]       = {}; // intensity 8-bit lookup table
	rgb_t ai44[256]       = {}; // alpha, intensity 4-4 lookup table
	rgb_t rgb565[65536]   = {}; // RGB 5-6-5 lookup table
	rgb_t argb1555[65536] = {}; // ARGB 1-5-5-5 lookup table
	rgb_t argb4444[65536] = {}; // ARGB 4-4-4-4 lookup table
};

struct tmu_state {
	void Initialize(const tmu_shared_state& tmu_shared,
	                voodoo_reg* voodoo_registers, const int tmem);

	uint8_t* ram            = {}; // pointer to aligned RAM
	mem_buffer_t ram_buffer = {}; // Managed buffer backing the RAM
	uint32_t mask           = {}; // mask to apply to pointers
	voodoo_reg* reg         = {}; // pointer to our register base
	bool regdirty = {}; // true if the LOD/mode/base registers have changed

	enum {
		texaddr_mask  = 0x0fffff,
		texaddr_shift = 3,
	};
	// uint32_t	texaddr_mask= {}; // mask for texture address
	// uint8_t texaddr_shift= {}; // shift for texture address

	int64_t starts, startt = {}; // starting S,T (14.18)
	int64_t startw = {};         // starting W (2.30)
	int64_t dsdx, dtdx = {};     // delta S,T per X
	int64_t dwdx = {};           // delta W per X
	int64_t dsdy, dtdy = {};     // delta S,T per Y
	int64_t dwdy = {};           // delta W per Y

	int32_t lodmin, lodmax = {}; // min, max LOD values
	int32_t lodbias       = {};  // LOD bias
	uint32_t lodmask      = {};  // mask of available LODs
	uint32_t lodoffset[9] = {};  // offset of texture base for each LOD
	int32_t lodbasetemp   = {}; // lodbase calculated and used during raster
	int32_t detailmax     = {}; // detail clamp
	int32_t detailbias    = {}; // detail bias
	uint8_t detailscale   = {}; // detail scale

	uint32_t wmask = {}; // mask for the current texture width
	uint32_t hmask = {}; // mask for the current texture height

	// mask for bilinear resolution (0xf0 for V1,  0xff for V2)
	uint8_t bilinear_mask = {};

	ncc_table ncc[2] = {}; // two NCC tables

	const rgb_t* lookup    = {}; // currently selected lookup
	const rgb_t* texel[16] = {}; // texel lookups for each format

	rgb_t palette[256]  = {}; // palette lookup table
	rgb_t palettea[256] = {}; // palette+alpha lookup table
};

struct setup_vertex {
	float x = 0.0f; // X, Y coordinates
	float y = 0.0f;

	float a = 0.0f; // A, R, G, B values
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;

	float z  = 0.0f; // Z and broadcast W values
	float wb = 0.0f;

	float w0 = 0.0f; // W, S, T for TMU 0
	float s0 = 0.0f;
	float t0 = 0.0f;

	float w1 = 0.0f; // W, S, T for TMU 1
	float s1 = 0.0f;
	float t1 = 0.0f;
};

struct fbi_state
{
	uint8_t* ram = nullptr;       // pointer to aligned frame buffer RAM
	mem_buffer_t ram_buffer = {}; // Managed buffer backing the RAM

	uint32_t mask       = 0;  // mask to apply to pointers
	uint32_t rgboffs[3] = {}; // word offset to 3 RGB buffers
	uint32_t auxoffs    = 0;  // word offset to 1 aux buffer

	uint8_t frontbuf = 0; // front buffer index
	uint8_t backbuf  = 0; // back buffer index

	uint32_t yorigin = 0; // Y origin subtract value

	uint32_t width  = 0; // width of current frame buffer
	uint32_t height = 0; // height of current frame buffer

	// uint32_t	xoffs = 0; // horizontal offset (back porch)
	// uint32_t yoffs = 0;	// vertical offset (back porch)
	// uint32_t	vsyncscan = 0; // vertical sync scanline

	uint32_t rowpixels   = 0; // pixels per row
	uint32_t tile_width  = 0; // width of video tiles
	uint32_t tile_height = 0; // height of video tiles
	uint32_t x_tiles     = 0; // number of tiles in the X direction

	uint8_t vblank        = 0;     // VBLANK state
	bool vblank_dont_swap = false; // don't actually swap when we hit this
	                               // point
	bool vblank_flush_pending = false;

	// triangle setup info
	int16_t ax = 0; // vertex A x,y (12.4)
	int16_t ay = 0;

	int16_t bx = 0; // vertex B x,y (12.4)
	int16_t by = 0;

	int16_t cx = 0; // vertex C x,y (12.4)
	int16_t cy = 0;

	int32_t startr = 0; // starting R,G,B,A (12.12)
	int32_t startg = 0;
	int32_t startb = 0;
	int32_t starta = 0;

	int32_t startz = 0; // starting Z (20.12)

	int64_t startw = 0; // starting W (16.32)

	int32_t drdx = 0; // delta R,G,B,A per X
	int32_t dgdx = 0;
	int32_t dbdx = 0;
	int32_t dadx = 0;

	int32_t dzdx = 0; // delta Z per X

	int64_t dwdx = 0; // delta W per X

	int32_t drdy = 0; // delta R,G,B,A per Y
	int32_t dgdy = 0;
	int32_t dbdy = 0;
	int32_t dady = 0;

	int32_t dzdy = 0; // delta Z per Y

	int64_t dwdy = 0; // delta W per Y

	Stats lfb_stats = {}; // LFB-access statistics

	uint8_t sverts        = 0;  // number of vertices ready
	setup_vertex svert[3] = {}; // 3 setup vertices

	fifo_state fifo = {}; // framebuffer memory fifo

	uint8_t fogblend[64] = {}; // 64-entry fog table
	uint8_t fogdelta[64] = {}; // 64-entry fog table

	uint8_t fogdelta_mask = 0; // mask for for delta (0xff for V1, 0xfc for V2)

	// rgb_t clut[512]; // clut gamma data
};

struct dac_state
{
	uint8_t reg[8]      = {}; // 8 registers
	uint8_t read_result = 0;  // pending read result
};

#ifdef C_ENABLE_VOODOO_OPENGL
#ifndef GLhandleARB
#ifdef __APPLE__
typedef void* GLhandleARB;
#else
typedef unsigned int GLhandleARB;
#endif // __APPLE__
#endif // GLhandleARB

struct raster_info
{
	raster_info* next = nulltr; // pointer to next entry with the same hash
#ifdef C_ENABLE_VOODOO_DEBUG
	bool is_generic = false; // true if this is one of the generic rasterizers
	uint8_t display = 0;     // display index
	uint32_t hits   = 0;     // how many hits (pixels) we've used this for
	uint32_t polys  = 0;     // how many polys we've used this for
#endif
	uint32_t eff_color_path = 0; // effective fbzColorPath value
	uint32_t eff_alpha_mode = 0; // effective alphaMode value
	uint32_t eff_fog_mode   = 0; // effective fogMode value
	uint32_t eff_fbz_mode   = 0; // effective fbzMode value
	uint32_t eff_tex_mode_0 = 0; // effective textureMode value for TMU #0
	uint32_t eff_tex_mode_1 = 0; // effective textureMode value for TMU #1

	bool shader_ready              = false;
	GLhandleARB so_shader_program  = {};
	GLhandleARB so_vertex_shader   = {};
	GLhandleARB so_fragment_shader = {};
	int32_t* shader_ulocations     = nullptr;
};
#endif

struct draw_state
{
	double frame_start           = 0.0;
	double vfreq                 = 0.0;
	bool override_on             = false;
	bool screen_update_requested = false;
	bool screen_update_pending   = false;
};

struct voodoo_state;

struct TriangleWorker {
	TriangleWorker() = delete;
	TriangleWorker(voodoo_state* state) : vs(state) {}

	TriangleWorker(const TriangleWorker&)            = delete;
	TriangleWorker& operator=(const TriangleWorker&) = delete;

	void Run();
	void Work(const int32_t worktstart, const int32_t worktend);
	void ThreadFunc(const int32_t p);
	void Shutdown();

	std::array<std::thread, TRIANGLE_THREADS> threads = {};
	std::array<Semaphore, TRIANGLE_THREADS> sembegin  = {};

	Semaphore semdone = {};

	std::atomic_bool threads_active = false;

	poly_vertex v1 = {};
	poly_vertex v2 = {};
	poly_vertex v3 = {};

	voodoo_state* vs = nullptr;

	uint16_t* drawbuf = nullptr;

	int32_t v1y      = 0;
	int32_t v3y      = 0;
	int32_t totalpix = 0;
	int done_count   = 0;

	bool use_threads             = false;
	bool disable_bilinear_filter = true;
};

struct VoodooPageHandler : public PageHandler {
	VoodooPageHandler() = delete;
	VoodooPageHandler(voodoo_state* state);

	VoodooPageHandler(const VoodooPageHandler&)            = delete;
	VoodooPageHandler& operator=(const VoodooPageHandler&) = delete;

	void writeb(PhysPt addr, uint8_t val) override;
	void writew(PhysPt addr, uint16_t val) override;
	void writed(PhysPt addr, uint32_t val) override;

	uint8_t readb(PhysPt addr) override;
	uint16_t readw(PhysPt addr) override;
	uint32_t readd(PhysPt addr) override;

	voodoo_state* vs = nullptr;
};

struct VoodooInitPageHandler : public PageHandler {
	VoodooInitPageHandler() = delete;
	VoodooInitPageHandler(voodoo_state* state);

	VoodooInitPageHandler(const VoodooInitPageHandler&)            = delete;
	VoodooInitPageHandler& operator=(const VoodooInitPageHandler&) = delete;

	void writeb(PhysPt addr, uint8_t val) override;
	void writew(PhysPt addr, uint16_t val) override;
	void writed(PhysPt addr, uint32_t val) override;

	uint8_t readb(PhysPt addr) override;
	uint16_t readw(PhysPt addr) override;
	uint32_t readd(PhysPt addr) override;

	voodoo_state* vs = nullptr;
};

struct voodoo_state {
	voodoo_state()
	        : page_handler(std::make_unique<VoodooInitPageHandler>(this)),
	          tworker(this)
	{}
	~voodoo_state();
	voodoo_state(const voodoo_state&)            = delete;
	voodoo_state& operator=(const voodoo_state&) = delete;

	PageHandler* StartHandler();
	void Initialize();
	void Startup();

	void FastFillRaster(void* destbase, const int32_t y,
	                    const poly_extent* extent, const uint16_t* extra_dither);

	void WriteToAddress(const uint32_t addr, const uint32_t data,
	                    const uint32_t mask);
	void WriteToRegister(const uint32_t offset, uint32_t data);
	void WriteToFrameBuffer(uint32_t offset, uint32_t data, uint32_t mem_mask);
	int32_t WriteToTexture(const uint32_t offset, uint32_t data);

	uint32_t ReadFromAddress(const uint32_t offset);
	uint32_t ReadFromRegister(const uint32_t offset);
	uint32_t ReadFromFrameBuffer(uint32_t offset);

	void WriteToNccTable(ncc_table* n, uint32_t regnum, const uint32_t data);

	void Activate();
	void Leave();

	void VblankFlush();
	bool GetRetrace();
	void UpdateDimensions();
	void UpdateScreenStart();
	void CheckScreenUpdate();
	void VerticalTimer();
	void UpdateScreen();
	double GetHRetracePosition();
	double GetVRetracePosition();

	// Commands
	void ExecuteTriangleCmd();
	void ExecuteBeginTriangleCmd();
	void ExecuteDrawTriangleCmd();
	void ExecuteFastFillCmd();
	void ExecuteSwapBufferCmd(const uint32_t data);

	void SetupAndDrawTriangle();
	void SwapBuffers();
	void RecomputeVideoMemory();

	void ResetCounters();
	void SoftReset();

	void RasterGeneric(uint32_t TMUS, uint32_t TEXMODE0, uint32_t TEXMODE1,
	                   void* destbase, int32_t y, const poly_extent* extent,
	                   Stats& stats);

	void UpdateStatistics(const StatsCollection collection_action);

	std::unique_ptr<PageHandler> page_handler = {};

	uint8_t chipmask = {}; // mask for which chips are available

	voodoo_reg reg[0x400]    = {}; // raw registers
	const uint8_t* regaccess = {}; // register access array
	bool alt_regmap          = {}; // enable alternate register map?

	pci_state pci = {}; // PCI state
	dac_state dac = {}; // DAC state

	fbi_state fbi          = {}; // Frame Buffer Interface (FBI) states
	tmu_state tmu[MAX_TMU] = {}; // Texture Mapper Unit (TMU) states
	tmu_shared_state tmushare = {}; // TMU shared state
	uint32_t tmu_config       = {};

#ifdef C_ENABLE_VOODOO_OPENGL
	raster_info* AddRasterizer(const raster_info* cinfo);
	raster_info* FindRasterizer(const int texcount);

	uint16_t next_rasterizer = {}; // next rasterizer index
	raster_info rasterizer[MAX_RASTERIZERS] = {}; // array of rasterizers
	raster_info* raster_hash[RASTER_HASH_SIZE] = {}; // hash table of rasterizers
#endif

	std::array<Stats, TRIANGLE_WORKERS> thread_stats = {}; // per-thread
	                                                       // statistics

	bool send_config        = {};
	bool clock_enabled      = {};
	bool output_on          = {};
	bool active             = {};
	bool is_handler_started = false;
#ifdef C_ENABLE_VOODOO_OPENGL
	bool ogl                 = {};
	bool ogl_dimchange       = {};
	bool ogl_palette_changed = {};
#endif
#ifdef C_ENABLE_VOODOO_DEBUG
	const char* const* regnames; // register names array
#endif

	draw_state draw = {};
	TriangleWorker tworker;
};

#ifdef C_ENABLE_VOODOO_OPENGL
struct poly_extra_data
{
	voodoo_state* state = nullptr; // pointer back to the voodoo state
	raster_info* info   = nullptr; // pointer to rasterizer information

	int16_t ax = 0;
	int16_t ay = 0; // vertex A x,y (12.4)

	int32_t startr = 0; // starting R,G,B,A (12.12)
	int32_t startg = 0;
	int32_t startb = 0;
	int32_t starta = 0;

	int32_t startz = 0; // starting Z (20.12)

	int64_t startw = 0; // starting W (16.32)

	int32_t drdx = 0; // delta R,G,B,A per X
	int32_t dgdx = 0;
	int32_t dbdx = 0;
	int32_t dadx = 0;

	int32_t dzdx = 0; // delta Z per X

	int64_t dwdx = 0; // delta W per X

	int32_t drdy = 0; // delta R,G,B,A per Y
	int32_t dgdy = 0;
	int32_t dbdy = 0;
	int32_t dady = 0;

	int32_t dzdy = 0; // delta Z per Y

	int64_t dwdy = 0; // delta W per Y

	int64_t starts0 = 0; // starting S,T (14.18)
	int64_t startt0 = 0;

	int64_t startw0 = 0; // starting W (2.30)

	int64_t ds0dx = 0; // delta S,T per X
	int64_t dt0dx = 0;

	int64_t dw0dx = 0; // delta W per X

	int64_t ds0dy = 0; // delta S,T per Y
	int64_t dt0dy = 0;

	int64_t dw0dy = 0; // delta W per Y

	int32_t lodbase0 = 0; // used during rasterization

	int64_t starts1 = 0; // starting S,T (14.18)
	int64_t startt1 = 0;

	int64_t startw1 = 0; // starting W (2.30)

	int64_t ds1dx = 0; // delta S,T per X
	int64_t dt1dx = 0;

	int64_t dw1dx = 0; // delta W per X

	int64_t ds1dy = 0; // delta S,T per Y
	int64_t dt1dy = 0;

	int64_t dw1dy = 0; // delta W per Y

	int32_t lodbase1 = 0; // used during rasterization

	uint16_t dither[16] = {}; // dither matrix, for fastfill

	Bitu texcount       = 0;
	Bitu r_fbzColorPath = 0;
	Bitu r_fbzMode      = 0;
	Bitu r_alphaMode    = 0;
	Bitu r_fogMode      = 0;

	int32_t r_textureMode0 = 0;
	int32_t r_textureMode1 = 0;
};
#endif

inline uint8_t count_leading_zeros(uint32_t value)
{
#ifdef _MSC_VER
	DWORD idx = 0;
	return (_BitScanReverse(&idx, value) ? (uint8_t)(31 - idx) : (uint8_t)32);
#else
	return (value != 0u ? (uint8_t)__builtin_clz(value) : (uint8_t)32);
#endif
	//int32_t result;
	//
	//#if defined _MSC_VER
	//__asm
	//{
	//	bsr   eax,value
	//	jnz   skip
	//	mov   eax,63
	//skip:
	//	xor   eax,31
	//	mov   result,eax
	//}
	//#else
	//result = 32;
	//while(value > 0) {
	//	result--;
	//	value >>= 1;
	//}
	//#endif
	//return (uint8_t)result;
}

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
	if (GCC_UNLIKELY(temp == 0))
	{
		*log_2 = 1000 << LOG_OUTPUT_PREC;
		return neg ? 0x80000000 : 0x7fffffff;
	}

	/* determine how many leading zeros in the value and shift it up high */
	lz = count_leading_zeros(temp);
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

#define APPLY_CHROMAKEY(STATS, FBZMODE, COLOR) \
	do { \
		if (FBZMODE_ENABLE_CHROMAKEY(FBZMODE)) { \
			/* non-range version */ \
			if (!CHROMARANGE_ENABLE(reg[chromaRange].u)) { \
				if ((((COLOR).u ^ reg[chromaKey].u) & 0xffffff) == \
				    0) { \
					ADD_STAT_COUNT(STATS, chroma_fail) \
					goto skipdrawdepth; \
				} \
			} \
\
			/* tricky range version */ \
			else { \
				int32_t low, high, test; \
				int results = 0; \
\
				/* check blue */ \
				low     = reg[chromaKey].rgb.b; \
				high    = reg[chromaRange].rgb.b; \
				test    = (COLOR).rgb.b; \
				results = (test >= low && test <= high); \
				results ^= CHROMARANGE_BLUE_EXCLUSIVE( \
				        reg[chromaRange].u); \
				results <<= 1; \
\
				/* check green */ \
				low  = reg[chromaKey].rgb.g; \
				high = reg[chromaRange].rgb.g; \
				test = (COLOR).rgb.g; \
				results |= (test >= low && test <= high); \
				results ^= CHROMARANGE_GREEN_EXCLUSIVE( \
				        reg[chromaRange].u); \
				results <<= 1; \
\
				/* check red */ \
				low  = reg[chromaKey].rgb.r; \
				high = reg[chromaRange].rgb.r; \
				test = (COLOR).rgb.r; \
				results |= (test >= low && test <= high); \
				results ^= CHROMARANGE_RED_EXCLUSIVE( \
				        reg[chromaRange].u); \
\
				/* final result */ \
				if (CHROMARANGE_UNION_MODE(reg[chromaRange].u)) { \
					if (results != 0) { \
						ADD_STAT_COUNT(STATS, chroma_fail) \
						goto skipdrawdepth; \
					} \
				} else { \
					if (results == 7) { \
						ADD_STAT_COUNT(STATS, chroma_fail) \
						goto skipdrawdepth; \
					} \
				} \
			} \
		} \
	} while (0)

/*************************************
 *
 *  Alpha masking macro
 *
 *************************************/

#define APPLY_ALPHAMASK(STATS, FBZMODE, AA) \
	do { \
		if (FBZMODE_ENABLE_ALPHA_MASK(FBZMODE)) { \
			if (((AA) & 1) == 0) { \
				ADD_STAT_COUNT(STATS, afunc_fail) \
				goto skipdrawdepth; \
			} \
		} \
	} while (0)

/*************************************
 *
 *  Alpha testing macro
 *
 *************************************/

#define APPLY_ALPHATEST(STATS, ALPHAMODE, AA) \
	do { \
		if (ALPHAMODE_ALPHATEST(ALPHAMODE)) { \
			uint8_t alpharef = reg[alphaMode].rgb.a; \
			switch (ALPHAMODE_ALPHAFUNCTION(ALPHAMODE)) { \
			case 0: /* alphaOP = never */ \
				ADD_STAT_COUNT(STATS, afunc_fail) \
				goto skipdrawdepth; \
\
			case 1: /* alphaOP = less than */ \
				if ((AA) >= alpharef) { \
					ADD_STAT_COUNT(STATS, afunc_fail) \
					goto skipdrawdepth; \
				} \
				break; \
\
			case 2: /* alphaOP = equal */ \
				if ((AA) != alpharef) { \
					ADD_STAT_COUNT(STATS, afunc_fail) \
					goto skipdrawdepth; \
				} \
				break; \
\
			case 3: /* alphaOP = less than or equal */ \
				if ((AA) > alpharef) { \
					ADD_STAT_COUNT(STATS, afunc_fail) \
					goto skipdrawdepth; \
				} \
				break; \
\
			case 4: /* alphaOP = greater than */ \
				if ((AA) <= alpharef) { \
					ADD_STAT_COUNT(STATS, afunc_fail) \
					goto skipdrawdepth; \
				} \
				break; \
\
			case 5: /* alphaOP = not equal */ \
				if ((AA) == alpharef) { \
					ADD_STAT_COUNT(STATS, afunc_fail) \
					goto skipdrawdepth; \
				} \
				break; \
\
			case 6: /* alphaOP = greater than or equal */ \
				if ((AA) < alpharef) { \
					ADD_STAT_COUNT(STATS, afunc_fail) \
					goto skipdrawdepth; \
				} \
				break; \
\
			case 7: /* alphaOP = always */ break; \
			} \
		} \
	} while (0)

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

#define APPLY_FOGGING(FOGMODE, FBZCP, XX, DITHER4, RR, GG, BB, ITERZ, ITERW, ITERAXXX) \
	do { \
		if (FOGMODE_ENABLE_FOG(FOGMODE)) { \
			rgb_union fogcolor = reg[fogColor]; \
			int32_t fr, fg, fb; \
\
			/* constant fog bypasses everything else */ \
			if (FOGMODE_FOG_CONSTANT(FOGMODE)) { \
				fr = fogcolor.rgb.r; \
				fg = fogcolor.rgb.g; \
				fb = fogcolor.rgb.b; \
			} \
\
			/* non-constant fog comes from several sources */ \
			else { \
				int32_t fogblend = 0; \
\
				/* if fog_add is zero, we start with the fog \
				 * color */ \
				if (FOGMODE_FOG_ADD(FOGMODE) == 0) { \
					fr = fogcolor.rgb.r; \
					fg = fogcolor.rgb.g; \
					fb = fogcolor.rgb.b; \
				} else \
					fr = fg = fb = 0; \
\
				/* if fog_mult is zero, we subtract the \
				 * incoming color */ \
				if (FOGMODE_FOG_MULT(FOGMODE) == 0) { \
					fr -= (RR); \
					fg -= (GG); \
					fb -= (BB); \
				} \
\
				/* fog blending mode */ \
				switch (FOGMODE_FOG_ZALPHA(FOGMODE)) { \
				case 0: /* fog table */ \
				{ \
					int32_t delta = fbi.fogdelta[wfloat >> 10]; \
					int32_t deltaval; \
\
					/* perform the multiply against lower \
					 * 8 bits of wfloat */ \
					deltaval = (delta & fbi.fogdelta_mask) * \
					           ((wfloat >> 2) & 0xff); \
\
					/* fog zones allow for negating this \
					 * value */ \
					if (FOGMODE_FOG_ZONES(FOGMODE) && \
					    (delta & 2)) \
						deltaval = -deltaval; \
					deltaval >>= 6; \
\
					/* apply dither */ \
					if (FOGMODE_FOG_DITHER(FOGMODE)) \
						if (DITHER4) \
							deltaval += (DITHER4)[(XX) & 3]; \
					deltaval >>= 4; \
\
					/* add to the blending factor */ \
					fogblend = fbi.fogblend[wfloat >> 10] + \
					           deltaval; \
					break; \
				} \
\
				case 1: /* iterated A */ \
					fogblend = (ITERAXXX).rgb.a; \
					break; \
\
				case 2: /* iterated Z */ \
					CLAMPED_Z((ITERZ), FBZCP, fogblend); \
					fogblend >>= 8; \
					break; \
\
				case 3: /* iterated W - Voodoo 2 only */ \
					CLAMPED_W((ITERW), FBZCP, fogblend); \
					break; \
				} \
\
				/* perform the blend */ \
				fogblend++; \
				fr = (fr * fogblend) >> 8; \
				fg = (fg * fogblend) >> 8; \
				fb = (fb * fogblend) >> 8; \
			} \
\
			/* if fog_mult is 0, we add this to the original color */ \
			if (FOGMODE_FOG_MULT(FOGMODE) == 0) { \
				(RR) += fr; \
				(GG) += fg; \
				(BB) += fb; \
			} \
\
			/* otherwise this just becomes the new color */ \
			else { \
				(RR) = fr; \
				(GG) = fg; \
				(BB) = fb; \
			} \
\
			/* clamp */ \
			(RR) = clamp_to_uint8(RR); \
			(GG) = clamp_to_uint8(GG); \
			(BB) = clamp_to_uint8(BB); \
		} \
	} while (0)

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

#define PIXEL_PIPELINE_BEGIN( \
        STATS, XX, YY, FBZCOLORPATH, FBZMODE, ITERZ, ITERW, ZACOLOR, STIPPLE) \
	do { \
		int32_t depthval, wfloat; \
		int32_t prefogr, prefogg, prefogb; \
		int32_t r, g, b, a; \
\
		/* apply clipping */ \
		/* note that for perf reasons, we assume the caller has done \
		 * clipping */ \
\
		/* handle stippling */ \
		if (FBZMODE_ENABLE_STIPPLE(FBZMODE)) { \
			/* rotate mode */ \
			if (FBZMODE_STIPPLE_PATTERN(FBZMODE) == 0) { \
				(STIPPLE) = ((STIPPLE) << 1) | ((STIPPLE) >> 31); \
				if (((STIPPLE) & 0x80000000) == 0) { \
					goto skipdrawdepth; \
				} \
			} \
\
			/* pattern mode */ \
			else { \
				int stipple_index = (((YY) & 3) << 3) | \
				                    (~(XX) & 7); \
				if ((((STIPPLE) >> stipple_index) & 1) == 0) { \
					goto skipdrawdepth; \
				} \
			} \
		} \
\
		/* compute "floating point" W value (used for depth and fog) */ \
		if ((ITERW) & LONGTYPE(0xffff00000000)) \
			wfloat = 0x0000; \
		else { \
			uint32_t temp = (uint32_t)(ITERW); \
			if ((temp & 0xffff0000) == 0) \
				wfloat = 0xffff; \
			else { \
				const auto exp = count_leading_zeros(temp); \
				const auto right_shift = std::max(0, 19 - exp); \
				wfloat                 = ((exp << 12) | \
                                          ((~temp >> right_shift) & 0xfff)); \
				if (wfloat < 0xffff) \
					wfloat++; \
			} \
		} \
\
		/* compute depth value (W or Z) for this pixel */ \
		if (FBZMODE_WBUFFER_SELECT(FBZMODE) == 0) \
			CLAMPED_Z(ITERZ, FBZCOLORPATH, depthval); \
		else if (FBZMODE_DEPTH_FLOAT_SELECT(FBZMODE) == 0) \
			depthval = wfloat; \
		else { \
			if ((ITERZ) & 0xf0000000) \
				depthval = 0x0000; \
			else { \
				uint32_t temp = (ITERZ) << 4; \
				if ((temp & 0xffff0000) == 0) \
					depthval = 0xffff; \
				else { \
					const auto exp = count_leading_zeros(temp); \
					const auto right_shift = std::max(0, 19 - exp); \
					depthval = ((exp << 12) | \
					            ((~temp >> right_shift) & 0xfff)); \
					if (depthval < 0xffff) \
						depthval++; \
				} \
			} \
		} \
\
		/* add the bias */ \
		if (FBZMODE_ENABLE_DEPTH_BIAS(FBZMODE)) { \
			depthval += (int16_t)(ZACOLOR); \
			depthval = clamp_to_uint16(depthval); \
		} \
\
		/* handle depth buffer testing */ \
		if (FBZMODE_ENABLE_DEPTHBUF(FBZMODE)) { \
			int32_t depthsource; \
\
			/* the source depth is either the iterated W/Z+bias or \
			 * a */ \
			/* constant value */ \
			if (FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) == 0) \
				depthsource = depthval; \
			else \
				depthsource = (uint16_t)(ZACOLOR); \
\
			/* test against the depth buffer */ \
			switch (FBZMODE_DEPTH_FUNCTION(FBZMODE)) { \
			case 0: /* depthOP = never */ \
				ADD_STAT_COUNT(STATS, zfunc_fail) \
				goto skipdrawdepth; \
\
			case 1: /* depthOP = less than */ \
				if (depth) \
					if (depthsource >= depth[XX]) { \
						ADD_STAT_COUNT(STATS, zfunc_fail) \
						goto skipdrawdepth; \
					} \
				break; \
\
			case 2: /* depthOP = equal */ \
				if (depth) \
					if (depthsource != depth[XX]) { \
						ADD_STAT_COUNT(STATS, zfunc_fail) \
						goto skipdrawdepth; \
					} \
				break; \
\
			case 3: /* depthOP = less than or equal */ \
				if (depth) \
					if (depthsource > depth[XX]) { \
						ADD_STAT_COUNT(STATS, zfunc_fail) \
						goto skipdrawdepth; \
					} \
				break; \
\
			case 4: /* depthOP = greater than */ \
				if (depth) \
					if (depthsource <= depth[XX]) { \
						ADD_STAT_COUNT(STATS, zfunc_fail) \
						goto skipdrawdepth; \
					} \
				break; \
\
			case 5: /* depthOP = not equal */ \
				if (depth) \
					if (depthsource == depth[XX]) { \
						ADD_STAT_COUNT(STATS, zfunc_fail) \
						goto skipdrawdepth; \
					} \
				break; \
\
			case 6: /* depthOP = greater than or equal */ \
				if (depth) \
					if (depthsource < depth[XX]) { \
						ADD_STAT_COUNT(STATS, zfunc_fail) \
						goto skipdrawdepth; \
					} \
				break; \
\
			case 7: /* depthOP = always */ break; \
			} \
		}

#define PIXEL_PIPELINE_MODIFY( \
        DITHER, DITHER4, XX, FBZMODE, FBZCOLORPATH, ALPHAMODE, FOGMODE, ITERZ, ITERW, ITERAXXX) \
\
	/* perform fogging */ \
	prefogr = r; \
	prefogg = g; \
	prefogb = b; \
	APPLY_FOGGING(FOGMODE, FBZCOLORPATH, XX, DITHER4, r, g, b, ITERZ, ITERW, ITERAXXX); \
\
	/* perform alpha blending */ \
	APPLY_ALPHA_BLEND(FBZMODE, ALPHAMODE, XX, DITHER, r, g, b, a);

#define PIXEL_PIPELINE_FINISH(DITHER_LOOKUP, XX, dest, depth, FBZMODE) \
\
	/* write to framebuffer */ \
	if (FBZMODE_RGB_BUFFER_MASK(FBZMODE)) { \
		/* apply dithering */ \
		APPLY_DITHER(FBZMODE, XX, DITHER_LOOKUP, r, g, b); \
		(dest)[XX] = (uint16_t)((r << 11) | (g << 5) | b); \
	} \
\
	/* write to aux buffer */ \
	if ((depth) && FBZMODE_AUX_BUFFER_MASK(FBZMODE)) { \
		if (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0) \
			(depth)[XX] = (uint16_t)depthval; \
		else \
			(depth)[XX] = (uint16_t)a; \
	}

#define PIXEL_PIPELINE_END(STATS) \
\
	/* track pixel writes to the frame buffer regardless of mask */ \
	ADD_STAT_COUNT(STATS, pixels_out) \
\
	skipdrawdepth:; \
	} \
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

static uint8_t vtype   = VOODOO_1;

enum class PerformanceFlags : uint8_t {
	None                = 0,
	MultiThreading      = 1 << 0,
	NoBilinearFiltering = 1 << 1,
	All                 = (MultiThreading | NoBilinearFiltering),
};

static const char* describe_performance_flags(const PerformanceFlags flags)
{
	// Note: the descriptions are meant to be used as a status postfix
	switch (flags) {
	case PerformanceFlags::None:
		return " and no optimizations";
	case PerformanceFlags::MultiThreading:
		return " and multi-threading";
	case PerformanceFlags::NoBilinearFiltering:
		return " and no bilinear filtering";
	case PerformanceFlags::All:
		return ", multi-threading, and no biliear filtering";
	}
	assert(false);
	return "unknown performance flag";
}

static PerformanceFlags vperf = {};

#define LOG_VOODOO LOG_PCI
enum {
	LOG_VBLANK_SWAP = 0,
	LOG_REGISTERS   = 0,
	LOG_LFB         = 0,
	LOG_TEXTURE_RAM = 0,
	LOG_RASTERIZERS = 0,
};

/***************************************************************************
    RASTERIZER MANAGEMENT
***************************************************************************/

static dither_lut_t dither2_lookup = {};
static dither_lut_t dither4_lookup = {};

void voodoo_state::RasterGeneric(uint32_t TMUS, uint32_t TEXMODE0,
                                 uint32_t TEXMODE1, void* destbase, int32_t y,
                                 const poly_extent* extent, Stats& stats)
{
	const uint8_t* dither_lookup = nullptr;
	const uint8_t* dither4       = nullptr;
	const uint8_t* dither        = nullptr;

	int32_t scry = y;
	int32_t startx = extent->startx;
	int32_t stopx = extent->stopx;

	// Quick references
	const auto& tmu0 = tmu[0];
	const auto& tmu1 = tmu[1];

	const uint32_t r_fbzColorPath = reg[fbzColorPath].u;
	const uint32_t r_fbzMode      = reg[fbzMode].u;
	const uint32_t r_alphaMode    = reg[alphaMode].u;
	const uint32_t r_fogMode      = reg[fogMode].u;
	const uint32_t r_zaColor      = reg[zaColor].u;

	uint32_t r_stipple = reg[stipple].u;

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
		if (scry < (int32_t)((reg[clipLowYHighY].u >> 16) & 0x3ff) ||
		    scry >= (int32_t)(reg[clipLowYHighY].u & 0x3ff)) {
			stats.pixels_in += stopx - startx;
			//stats.clip_fail += stopx - startx;
			return;
		}

		/* X clipping */
		int32_t tempclip = (reg[clipLeftRight].u >> 16) & 0x3ff;
		if (startx < tempclip)
		{
			stats.pixels_in += tempclip - startx;
			startx = tempclip;
		}
		tempclip = reg[clipLeftRight].u & 0x3ff;
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
		PIXEL_PIPELINE_BEGIN(stats,
		                     x,
		                     y,
		                     r_fbzColorPath,
		                     r_fbzMode,
		                     iterz,
		                     iterw,
		                     r_zaColor,
		                     r_stipple);

		/* run the texture pipeline on TMU1 to produce a value in texel */
		/* note that they set LOD min to 8 to "disable" a TMU */

		if (TMUS >= 2 && tmu1.lodmin < (8 << 8)) {
			const tmu_state* const tmus = &tmu1;
			const rgb_t* const lookup = tmus->lookup;
			TEXTURE_PIPELINE(tmus, x, dither4, TEXMODE1, texel,
								lookup, tmus->lodbasetemp,
								iters1, itert1, iterw1, texel);
		}

		/* run the texture pipeline on TMU0 to produce a final */
		/* result in texel */
		/* note that they set LOD min to 8 to "disable" a TMU */
		if (TMUS >= 1 && tmu0.lodmin < (8 << 8)) {
			if (!send_config) {
				const tmu_state* const tmus = &tmu0;
				const rgb_t* const lookup = tmus->lookup;
				TEXTURE_PIPELINE(tmus, x, dither4, TEXMODE0, texel,
								lookup, tmus->lodbasetemp,
								iters0, itert0, iterw0, texel);
			} else { /* send config data to the frame buffer */
				texel.u = tmu_config;
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
			        c_other.u = reg[color1].u;
			        break;
			case 3:	/* reserved */
				c_other.u = 0;
				break;
		}

		// handle chroma key
		APPLY_CHROMAKEY(stats, r_fbzMode, c_other);

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
			        c_other.rgb.a = reg[color1].rgb.a;
			        break;
			case 3:	/* reserved */
				c_other.rgb.a = 0;
				break;
		}

		// Handle alpha mask
		APPLY_ALPHAMASK(stats, r_fbzMode, c_other.rgb.a);

		// Handle alpha test
		APPLY_ALPHATEST(stats, r_alphaMode, c_other.rgb.a);

		/* compute c_local */
		if (FBZCP_CC_LOCALSELECT_OVERRIDE(r_fbzColorPath) == 0)
		{
			if (FBZCP_CC_LOCALSELECT(r_fbzColorPath) == 0) {
				// iterated RGB
				c_local.u = iterargb.u;
			} else {
				// color0 RGB
				c_local.u = reg[color0].u;
			}
		}
		else
		{
			if ((texel.rgb.a & 0x80) == 0) {
				// iterated RGB
				c_local.u = iterargb.u;
			} else {
				// color0 RGB
				c_local.u = reg[color0].u;
			}
		}

		/* compute a_local */
		switch (FBZCP_CCA_LOCALSELECT(r_fbzColorPath))
		{
			case 0:		/* iterated alpha */
				c_local.rgb.a = iterargb.rgb.a;
				break;
			case 1:		/* color0 alpha */
			        c_local.rgb.a = reg[color0].rgb.a;
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

		// Pixel pipeline part 2 handles fog, alpha, and final output
		PIXEL_PIPELINE_MODIFY(dither,
		                      dither4,
		                      x,
		                      r_fbzMode,
		                      r_fbzColorPath,
		                      r_alphaMode,
		                      r_fogMode,
		                      iterz,
		                      iterw,
		                      iterargb);

		PIXEL_PIPELINE_FINISH(dither_lookup, x, dest, depth, r_fbzMode);

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
    Add a rasterizer to our hash table
-------------------------------------------------*/
raster_info* voodoo_state::AddRasterizer(const raster_info* cinfo)
{
	if (next_rasterizer >= MAX_RASTERIZERS) {
		E_Exit("Out of space for new rasterizers!");
		next_rasterizer = 0;
	}

	raster_info* info = &rasterizer[next_rasterizer++];
	int hash = compute_raster_hash(cinfo);

	/* make a copy of the info */
	*info = *cinfo;

#ifdef C_ENABLE_VOODOO_DEBUG
	/* fill in the data */
	info->hits = 0;
	info->polys = 0;
#endif

	/* hook us into the hash table */
	info->next        = raster_hash[hash];
	raster_hash[hash] = info;

	if (LOG_RASTERIZERS)
		LOG_DEBUG("VOODOO: Adding rasterizer @ %p : %08X %08X %08X %08X %08X %08X (hash=%d)\n",
				info->callback,
				info->eff_color_path, info->eff_alpha_mode, info->eff_fog_mode, info->eff_fbz_mode,
				info->eff_tex_mode_0, info->eff_tex_mode_1, hash);

	return info;
}

/*-------------------------------------------------
    Find a rasterizer that matches  our current parameters and return it,
creating a new one if necessary
-------------------------------------------------*/
raster_info* voodoo_state::FindRasterizer(const int texcount)
{
	// build an info struct with all the parameters
	raster_info curinfo    = {};
	curinfo.eff_color_path = normalize_color_path(reg[fbzColorPath].u);
	curinfo.eff_alpha_mode = normalize_alpha_mode(reg[alphaMode].u);
	curinfo.eff_fog_mode   = normalize_fog_mode(reg[fogMode].u);
	curinfo.eff_fbz_mode   = normalize_fbz_mode(reg[fbzMode].u);

	curinfo.eff_tex_mode_0 = (texcount >= 1)
	                               ? normalize_tex_mode(tmu[0].reg[textureMode].u)
	                               : 0xffffffff;

	curinfo.eff_tex_mode_1 = (texcount >= 2)
	                               ? normalize_tex_mode(tmu[1].reg[textureMode].u)
	                               : 0xffffffff;

	/* compute the hash */
	const auto hash = compute_raster_hash(&curinfo);

	// find the appropriate hash entry
	raster_info* prev = nullptr;
	for (auto info = raster_hash[hash]; info; prev = info, info = info->next) {
		if (info->eff_color_path == curinfo.eff_color_path &&
		    info->eff_alpha_mode == curinfo.eff_alpha_mode &&
		    info->eff_fog_mode == curinfo.eff_fog_mode &&
		    info->eff_fbz_mode == curinfo.eff_fbz_mode &&
		    info->eff_tex_mode_0 == curinfo.eff_tex_mode_0 &&
		    info->eff_tex_mode_1 == curinfo.eff_tex_mode_1) {
			// got it, move us to the head of the list
			if (prev) {
				prev->next = info->next;
				info->next = raster_hash[hash];
				raster_hash[hash] = info;
			}

			// return the result
			return info;
		}
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

	return AddRasterizer(&curinfo);
}
#endif


/***************************************************************************
    GENERIC RASTERIZERS
***************************************************************************/

/*-------------------------------------------------
    FastFillRaster - per-scanline
    implementation of the 'fastfill' command
-------------------------------------------------*/
void voodoo_state::FastFillRaster(void* destbase, const int32_t y,
                                  const poly_extent* extent,
                                  const uint16_t* extra_dither)
{
	Stats stats = {};
	const int32_t startx = extent->startx;
	int32_t stopx = extent->stopx;
	int scry;
	int x;

	/* determine the screen Y */
	scry = y;
	if (FBZMODE_Y_ORIGIN(reg[fbzMode].u)) {
		scry = (fbi.yorigin - y) & 0x3ff;
	}

	/* fill this RGB row */
	if (FBZMODE_RGB_BUFFER_MASK(reg[fbzMode].u)) {
		const uint16_t* ditherow = &extra_dither[(y & 3) * 4];

		const auto expanded = read_unaligned_uint64(
		        reinterpret_cast<const uint8_t*>(ditherow));

		uint16_t* dest = reinterpret_cast<uint16_t*>(destbase) +
		                 scry * fbi.rowpixels;

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
	if (FBZMODE_AUX_BUFFER_MASK(reg[fbzMode].u) &&
	    fbi.auxoffs != (uint32_t)(~0)) {
		const auto color = static_cast<uint16_t>(reg[zaColor].u & 0xffff);

		const uint64_t expanded = (static_cast<uint64_t>(color) << 48) |
		                          (static_cast<uint64_t>(color) << 32) |
		                          (static_cast<uint32_t>(color) << 16) |
		                          color;

		uint16_t* dest = reinterpret_cast<uint16_t*>(fbi.ram + fbi.auxoffs) +
		                 scry * fbi.rowpixels;

		if (fbi.auxoffs + 2 * (scry * fbi.rowpixels + stopx) >= fbi.mask) {
			stopx = (fbi.mask - fbi.auxoffs) / 2 - scry * fbi.rowpixels;
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

constexpr void tmu_shared_state::Initialize()
{
	/* build static 8-bit texel tables */
	for (int val = 0; val < 256; val++) {
		int r = 0;
		int g = 0;
		int b = 0;
		int a = 0;

		/* 8-bit RGB (3-3-2) */
		EXTRACT_332_TO_888(val, r, g, b);
		rgb332[val] = MAKE_ARGB(0xff, r, g, b);

		/* 8-bit alpha */
		alpha8[val] = MAKE_ARGB(val, val, val, val);

		/* 8-bit intensity */
		int8[val] = MAKE_ARGB(0xff, val, val, val);

		/* 8-bit alpha, intensity */
		a = ((val >> 0) & 0xf0) | ((val >> 4) & 0x0f);
		r = ((val << 4) & 0xf0) | ((val << 0) & 0x0f);
		ai44[val] = MAKE_ARGB(a, r, r, r);
	}

	/* build static 16-bit texel tables */
	for (int val = 0; val < 65536; val++) {
		int r = 0;
		int g = 0;
		int b = 0;
		int a = 0;

		/* table 10 = 16-bit RGB (5-6-5) */
		EXTRACT_565_TO_888(val, r, g, b);
		rgb565[val] = MAKE_ARGB(0xff, r, g, b);

		/* table 11 = 16 ARGB (1-5-5-5) */
		EXTRACT_1555_TO_8888(val, a, r, g, b);
		argb1555[val] = MAKE_ARGB(a, r, g, b);

		/* table 12 = 16-bit ARGB (4-4-4-4) */
		EXTRACT_4444_TO_8888(val, a, r, g, b);
		argb4444[val] = MAKE_ARGB(a, r, g, b);
	}
}

void tmu_state::Initialize(const tmu_shared_state& tmu_shared,
                           voodoo_reg* voodoo_registers, const int tmem)
{
	// Sanity check inputs
	assert(voodoo_registers);
	assert(tmem > 1);

	// Allocate and align the texture RAM to 64-bit, which is the maximum type written
	constexpr auto mem_alignment = sizeof(uint64_t);

	std::tie(ram_buffer,
	         ram) = make_unique_aligned_array<uint8_t>(mem_alignment, tmem);

	assert(reinterpret_cast<uintptr_t>(ram) % mem_alignment == 0);

	mask = (uint32_t)(tmem - 1);
	reg = voodoo_registers;
	regdirty = true;

	bilinear_mask = (vtype >= VOODOO_2) ? 0xff : 0xf0;

	// Mark the NCC tables dirty and configure their registers
	ncc[0].dirty = ncc[1].dirty = true;
	ncc[0].reg = &reg[nccTable + 0];
	ncc[1].reg = &reg[nccTable + 12];

	// create pointers to all the tables
	texel[0]  = tmu_shared.rgb332;
	texel[1]  = ncc[0].texel;
	texel[2]  = tmu_shared.alpha8;
	texel[3]  = tmu_shared.int8;
	texel[4]  = tmu_shared.ai44;
	texel[5]  = palette;
	texel[6]  = (vtype >= VOODOO_2) ? palettea : nullptr;
	texel[7]  = nullptr;
	texel[8]  = tmu_shared.rgb332;
	texel[9]  = ncc[0].texel;
	texel[10] = tmu_shared.rgb565;
	texel[11] = tmu_shared.argb1555;
	texel[12] = tmu_shared.argb4444;
	texel[13] = tmu_shared.int8;
	texel[14] = palette;
	texel[15] = nullptr;

	lookup = texel[0];

	// attach the palette to NCC table 0 */
	ncc[0].palette  = palette;
	ncc[0].palettea = (vtype >= VOODOO_2) ? palettea : nullptr;

	// Set up texture address calculations

	// texaddr_mask = 0x0fffff;
	// texaddr_shift = 3;

	lodmin = 0;
	lodmax = 0;
}

/*************************************
 *
 *  VBLANK management
 *
 *************************************/

void voodoo_state::SwapBuffers()
{
	// if (LOG_VBLANK_SWAP) LOG(LOG_VOODOO,LOG_WARN)("--- swap_buffers @
	// %d\n", video_screen_get_vpos(screen));

#ifdef C_ENABLE_VOODOO_OPENGL
	if (ogl && active) {
		voodoo_ogl_swap_buffer();
		return;
	}
#endif

	// Keep a history of swap intervals
	reg[fbiSwapHistory].u = (reg[fbiSwapHistory].u << 4);

	// Rotate the buffers
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

void voodoo_state::RecomputeVideoMemory()
{
	const uint32_t buffer_pages = FBIINIT2_VIDEO_BUFFER_OFFSET(reg[fbiInit2].u);

	const uint32_t fifo_start_page = FBIINIT4_MEMORY_FIFO_START_ROW(
	        reg[fbiInit4].u);

	uint32_t fifo_last_page = FBIINIT4_MEMORY_FIFO_STOP_ROW(reg[fbiInit4].u);
	uint32_t memory_config;
	int buf;

	// memory config is determined differently between V1 and V2
	memory_config = FBIINIT2_ENABLE_TRIPLE_BUF(reg[fbiInit2].u);
	if (vtype == VOODOO_2 && memory_config == 0) {
		memory_config = FBIINIT5_BUFFER_ALLOCATION(reg[fbiInit5].u);
	}

	// Tiles are 64x16/32; x_tiles specifies how many half-tiles
	fbi.tile_width  = (vtype < VOODOO_2) ? 64 : 32;
	fbi.tile_height = (vtype < VOODOO_2) ? 16 : 32;

	fbi.x_tiles = FBIINIT1_X_VIDEO_TILES(reg[fbiInit1].u);

	if (vtype == VOODOO_2)
	{
		fbi.x_tiles = (fbi.x_tiles << 1) |
		              (FBIINIT1_X_VIDEO_TILES_BIT5(reg[fbiInit1].u) << 5) |
		              (FBIINIT6_X_VIDEO_TILES_BIT0(reg[fbiInit6].u));
	}
	fbi.rowpixels = fbi.tile_width * fbi.x_tiles;

	// logerror("VOODOO.%d.VIDMEM: buffer_pages=%X  fifo=%X-%X  tiles=%X
	// rowpix=%d\n", index, buffer_pages, fifo_start_page,
	// fifo_last_page, fbi.x_tiles, fbi.rowpixels);

	// First RGB buffer always starts at 0
	fbi.rgboffs[0] = 0;

	// Second RGB buffer starts immediately afterwards
	fbi.rgboffs[1] = buffer_pages * 0x1000;

	// Remaining buffers are based on the config
	switch (memory_config)
	{
	case 3: // reserved
		LOG(LOG_VOODOO, LOG_WARN)
		("VOODOO.ERROR:Unexpected memory configuration in RecomputeVideoMemory!");
		[[fallthrough]];

	case 0: // 2 color buffers, 1 aux buffer
		fbi.rgboffs[2] = (uint32_t)(~0);
		fbi.auxoffs    = 2 * buffer_pages * 0x1000;
		break;

	case 1: // 3 color buffers, 0 aux buffers
		fbi.rgboffs[2] = 2 * buffer_pages * 0x1000;
		fbi.auxoffs    = (uint32_t)(~0);
		break;

	case 2: // 3 color buffers, 1 aux buffers
		fbi.rgboffs[2] = 2 * buffer_pages * 0x1000;
		fbi.auxoffs    = 3 * buffer_pages * 0x1000;
		break;
	}

	// Clamp the RGB buffers to video memory
	for (buf = 0; buf < 3; buf++) {
		if (fbi.rgboffs[buf] != (uint32_t)(~0) && fbi.rgboffs[buf] > fbi.mask) {
			fbi.rgboffs[buf] = fbi.mask;
		}
	}

	// Clamp the aux buffer to video memory
	if (fbi.auxoffs != (uint32_t)(~0) && fbi.auxoffs > fbi.mask) {
		fbi.auxoffs = fbi.mask;
	}

	// Compute the memory FIFO location and size
	if (fifo_last_page > fbi.mask / 0x1000) {
		fifo_last_page = fbi.mask / 0x1000;
	}

	// Is it valid and enabled?
	if (fifo_start_page <= fifo_last_page &&
	    FBIINIT0_ENABLE_MEMORY_FIFO(reg[fbiInit0].u)) {
		fbi.fifo.size = (fifo_last_page + 1 - fifo_start_page) * 0x1000 / 4;
		if (fbi.fifo.size > 65536 * 2) {
			fbi.fifo.size = 65536 * 2;
		}
	} else // If not, disable the FIFO
	{
		fbi.fifo.size = 0;
	}

	// Reset our front/back buffers if they are out of range
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

void voodoo_state::WriteToNccTable(ncc_table* n, uint32_t regnum, const uint32_t data)
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
			ogl_palette_changed = true;
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

static void sum_statistics(Stats& target, const Stats& source)
{
	target.pixels_in += source.pixels_in;
	target.pixels_out += source.pixels_out;
	target.chroma_fail += source.chroma_fail;
	target.zfunc_fail += source.zfunc_fail;
	target.afunc_fail += source.afunc_fail;
}

void voodoo_state::UpdateStatistics(const StatsCollection collection_action)
{
	auto accumulate = [this](const Stats& stats) {
		// Apply internal voodoo statistics
		reg[fbiPixelsIn].u += stats.pixels_in;
		reg[fbiPixelsOut].u += stats.pixels_out;
		reg[fbiChromaFail].u += stats.chroma_fail;
		reg[fbiZfuncFail].u += stats.zfunc_fail;
		reg[fbiAfuncFail].u += stats.afunc_fail;
	};
	// Maybe accumulate the threads and frame buffer stats
	if (collection_action == StatsCollection::Accumulate) {
		std::for_each(thread_stats.begin(), thread_stats.end(), accumulate);
		accumulate(fbi.lfb_stats);
	}
	// Always reset the stats
	thread_stats  = {};
	fbi.lfb_stats = {};
}

/***************************************************************************
COMMAND HANDLERS
***************************************************************************/

void TriangleWorker::Work(const int32_t worktstart, const int32_t worktend)
{
	/* determine the number of TMUs involved */
	uint32_t tmus     = 0;
	uint32_t texmode0 = 0;
	uint32_t texmode1 = 0;
	if (!FBIINIT3_DISABLE_TMUS(vs->reg[fbiInit3].u) &&
	    FBZCP_TEXTURE_ENABLE(vs->reg[fbzColorPath].u)) {
		tmus = 1;
		texmode0 = vs->tmu[0].reg[textureMode].u;
		if ((vs->chipmask & 0x04) != 0) {
			tmus = 2;
			texmode1 = vs->tmu[1].reg[textureMode].u;
		}
		if (disable_bilinear_filter) // force disable bilinear filter
		{
			texmode0 &= ~6;
			texmode1 &= ~6;
		}
	}

	/* compute the slopes for each portion of the triangle */
	const float dxdy_v1v2 = (v2.y == v1.y) ? 0.0f
	                                       : (v2.x - v1.x) / (v2.y - v1.y);
	const float dxdy_v1v3 = (v3.y == v1.y) ? 0.0f
	                                       : (v3.x - v1.x) / (v3.y - v1.y);
	const float dxdy_v2v3 = (v3.y == v2.y) ? 0.0f
	                                       : (v3.x - v2.x) / (v3.y - v2.y);

	Stats my_stats = {};

	const int32_t from = totalpix * worktstart / TRIANGLE_WORKERS;
	const int32_t to   = totalpix * worktend / TRIANGLE_WORKERS;

	for (int32_t curscan = v1y, scanend = v3y, sumpix = 0, lastsum = 0;
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

		vs->RasterGeneric(tmus, texmode0, texmode1, drawbuf, curscan, &extent, my_stats);
	}
	sum_statistics(vs->thread_stats[worktstart], my_stats);
}

void TriangleWorker::ThreadFunc(const int32_t p)
{
	for (const int32_t tnum = p; threads_active;) {
		sembegin[tnum].wait();
		if (threads_active) {
			Work(tnum, tnum + 1);
		}
		semdone.notify();
	}
}

void TriangleWorker::Shutdown()
{
	if (!threads_active) {
		return;
	}
	threads_active = false;
	for (size_t i = 0; i != TRIANGLE_THREADS; i++) {
		sembegin[i].notify();
	}

	for (size_t i = 0; i != TRIANGLE_THREADS; i++) {
		semdone.wait();
	}

	for (auto& thread : threads) {
		if (thread.joinable()) {
			thread.join();
		}
	}
}

void TriangleWorker::Run()
{
	if (!use_threads) {
		// do not use threaded calculation
		totalpix = 0xFFFFFFF;
		Work(0, TRIANGLE_WORKERS);
		return;
	}

	/* compute the slopes for each portion of the triangle */
	const float dxdy_v1v2 = (v2.y == v1.y) ? 0.0f
	                                       : (v2.x - v1.x) / (v2.y - v1.y);
	const float dxdy_v1v3 = (v3.y == v1.y) ? 0.0f
	                                       : (v3.x - v1.x) / (v3.y - v1.y);
	const float dxdy_v2v3 = (v3.y == v2.y) ? 0.0f
	                                       : (v3.x - v2.x) / (v3.y - v2.y);

	totalpix = 0;
	for (int32_t curscan = v1y, scanend = v3y; curscan != scanend; curscan++) {
		const float fully  = (float)(curscan) + 0.5f;
		const float startx = v1.x + (fully - v1.y) * dxdy_v1v3;

		/* compute the ending X based on which part of the triangle
		 * we're in */
		const float stopx = (fully < v2.y
		                             ? (v1.x + (fully - v1.y) * dxdy_v1v2)
		                             : (v2.x + (fully - v2.y) * dxdy_v2v3));

		/* clamp to full pixels */
		const int32_t istartx = round_coordinate(startx);
		const int32_t istopx  = round_coordinate(stopx);

		/* force start < stop */
		totalpix += (istartx > istopx ? istartx - istopx : istopx - istartx);
	}

	// Don't wake up threads for just a few pixels
	if (totalpix <= 200) {
		Work(0, TRIANGLE_WORKERS);
		return;
	}

	if (!threads_active) {
		threads_active = true;

		int worker_id = 0;
		for (auto& t : threads) {
			t = std::thread([this, worker_id] {
				this->ThreadFunc(worker_id);
			});
			++worker_id;
		}
	}
	for (auto& begin_semaphore : sembegin) {
		begin_semaphore.notify();
	}
	Work(TRIANGLE_THREADS, TRIANGLE_WORKERS);
	for (size_t i = 0; i != TRIANGLE_THREADS; i++) {
		semdone.wait();
	}
}

/*-------------------------------------------------
    triangle - execute the 'triangle'
    command
-------------------------------------------------*/
void voodoo_state::ExecuteTriangleCmd()
{
		// Quick references
	auto& tmu0 = tmu[0];
	auto& tmu1 = tmu[1];
	/* determine the number of TMUs involved */
	int texcount = 0;
	if (!FBIINIT3_DISABLE_TMUS(reg[fbiInit3].u) &&
	    FBZCP_TEXTURE_ENABLE(reg[fbzColorPath].u)) {
		texcount = 1;
		if ((chipmask & 0x04) != 0) {
			texcount = 2;
		}
	}

	/* perform subpixel adjustments */
	if (
#ifdef C_ENABLE_VOODOO_OPENGL
	        !ogl &&
#endif
	    FBZCP_CCA_SUBPIXEL_ADJUST(reg[fbzColorPath].u)) {
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

	raster_info* info = FindRasterizer(texcount);
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
	extra.r_fbzColorPath = reg[fbzColorPath].u;
	extra.r_fbzMode      = reg[fbzMode].u;
	extra.r_alphaMode    = reg[alphaMode].u;
	extra.r_fogMode      = reg[fogMode].u;

	extra.r_textureMode0 = tmu0.reg[textureMode].u;
	if (tmu1.ram != nullptr) {
		extra.r_textureMode1 = tmu1.reg[textureMode].u;
	}

#ifdef C_ENABLE_VOODOO_DEBUG
	info->polys++;
#endif

	if (ogl_palette_changed && ogl && active) {
		voodoo_ogl_invalidate_paltex();
		ogl_palette_changed = false;
	}

	if (ogl && active) {
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
	switch (FBZMODE_DRAW_BUFFER(reg[fbzMode].u)) {
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

	tworker.v1 = *v1, tworker.v2 = *v2, tworker.v3 = *v3;
	tworker.drawbuf = drawbuf;
	tworker.v1y = v1y;
	tworker.v3y = v3y;
	tworker.Run();

	/* update stats */
	reg[fbiTrianglesOut].u++;
}

/*-------------------------------------------------
    execute the 'beginTri' command
-------------------------------------------------*/
void voodoo_state::ExecuteBeginTriangleCmd()
{
	// Quick reference
	setup_vertex* sv = &fbi.svert[2];

	// extract all the data from registers
	sv->x  = reg[sVx].f;
	sv->y  = reg[sVy].f;
	sv->wb = reg[sWb].f;
	sv->w0 = reg[sWtmu0].f;
	sv->s0 = reg[sS_W0].f;
	sv->t0 = reg[sT_W0].f;
	sv->w1 = reg[sWtmu1].f;
	sv->s1 = reg[sS_Wtmu1].f;
	sv->t1 = reg[sT_Wtmu1].f;
	sv->a  = reg[sAlpha].f;
	sv->r  = reg[sRed].f;
	sv->g  = reg[sGreen].f;
	sv->b  = reg[sBlue].f;

	// spread it across all three verts and reset the count
	fbi.svert[0] = fbi.svert[1] = fbi.svert[2];
	fbi.sverts = 1;
}

/*-------------------------------------------------
    Process the setup parameters and render the triangle
-------------------------------------------------*/
void voodoo_state::SetupAndDrawTriangle()
{
	// Quick references
	auto& tmu0 = tmu[0];
	auto& tmu1 = tmu[1];

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
	if ((reg[sSetupMode].u & 0x20000) != 0u) {
		int culling_sign = (reg[sSetupMode].u >> 18) & 1;
		const auto divisor_sign = static_cast<int>(divisor < 0);

		/* if doing strips and ping pong is enabled, apply the ping pong */
		if ((reg[sSetupMode].u & 0x90000) == 0x00000) {
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
	if ((reg[sSetupMode].u & (1 << 0)) != 0u) {
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
	if ((reg[sSetupMode].u & (1 << 1)) != 0u) {
		fbi.starta = (int32_t)(vertex0.a * 4096.0f);
		fbi.dadx   = (int32_t)(((vertex0.a - vertex1.a) * dx1 -
                                      (vertex0.a - vertex2.a) * dx2) *
                                     tdiv);
		fbi.dady   = (int32_t)(((vertex0.a - vertex2.a) * dy1 -
                                      (vertex0.a - vertex1.a) * dy2) *
                                     tdiv);
	}

	/* set up Z */
	if ((reg[sSetupMode].u & (1 << 2)) != 0u) {
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
	if ((reg[sSetupMode].u & (1 << 3)) != 0u) {
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
	if ((reg[sSetupMode].u & (1 << 4)) != 0u) {
		tmu0.startw = tmu1.startw = (int64_t)(vertex0.w0 * 65536.0f * 65536.0f);
		tmu0.dwdx = tmu1.dwdx = (int64_t)(((vertex0.w0 - vertex1.w0) * dx1 -
		                                   (vertex0.w0 - vertex2.w0) * dx2) *
		                                  tdiv);
		tmu0.dwdy = tmu1.dwdy = (int64_t)(((vertex0.w0 - vertex2.w0) * dy1 -
		                                   (vertex0.w0 - vertex1.w0) * dy2) *
		                                  tdiv);
	}

	/* set up S0,T0 */
	if ((reg[sSetupMode].u & (1 << 5)) != 0u) {
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
	if ((reg[sSetupMode].u & (1 << 6)) != 0u) {
		tmu1.startw = (int64_t)(vertex0.w1 * 65536.0f * 65536.0f);
		tmu1.dwdx   = (int64_t)(((vertex0.w1 - vertex1.w1) * dx1 -
                                       (vertex0.w1 - vertex2.w1) * dx2) *
                                      tdiv);
		tmu1.dwdy   = (int64_t)(((vertex0.w1 - vertex2.w1) * dy1 -
                                       (vertex0.w1 - vertex1.w1) * dy2) *
                                      tdiv);
	}

	/* set up S1,T1 */
	if ((reg[sSetupMode].u & (1 << 7)) != 0u) {
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
	ExecuteTriangleCmd();
}

/*-------------------------------------------------
    Execute the 'DrawTri' command
-------------------------------------------------*/
void voodoo_state::ExecuteDrawTriangleCmd()
{
	// For strip mode, shuffle vertex 1 down to 0
	if ((reg[sSetupMode].u & (1 << 16)) == 0u) {
		fbi.svert[0] = fbi.svert[1];
	}

	// Copy 2 down to 1 regardless
	fbi.svert[1] = fbi.svert[2];

	// Extract all the data from registers
	setup_vertex* sv = &fbi.svert[2];

	sv->x  = reg[sVx].f;
	sv->y  = reg[sVy].f;
	sv->wb = reg[sWb].f;
	sv->w0 = reg[sWtmu0].f;
	sv->s0 = reg[sS_W0].f;
	sv->t0 = reg[sT_W0].f;
	sv->w1 = reg[sWtmu1].f;
	sv->s1 = reg[sS_Wtmu1].f;
	sv->t1 = reg[sT_Wtmu1].f;
	sv->a  = reg[sAlpha].f;
	sv->r  = reg[sRed].f;
	sv->g  = reg[sGreen].f;
	sv->b  = reg[sBlue].f;

	// If we have enough verts, go ahead and draw
	if (++fbi.sverts >= 3) {
		SetupAndDrawTriangle();
	}
}

/*-------------------------------------------------
    Execute the 'fastfill' command
-------------------------------------------------*/
void voodoo_state::ExecuteFastFillCmd()
{
	const int sx = (reg[clipLeftRight].u >> 16) & 0x3ff;
	const int ex = (reg[clipLeftRight].u >> 0) & 0x3ff;
	const int sy = (reg[clipLowYHighY].u >> 16) & 0x3ff;
	const int ey = (reg[clipLowYHighY].u >> 0) & 0x3ff;

	poly_extent extents[64] = {};

	// Align to 64-bit because that's the maximum type written
	alignas(sizeof(uint64_t)) static uint16_t dithermatrix[16] = {};

	uint16_t* drawbuf = nullptr;
	int x;
	int y;

	// If we're not clearing either, take no time
	if (!FBZMODE_RGB_BUFFER_MASK(reg[fbzMode].u) &&
	    !FBZMODE_AUX_BUFFER_MASK(reg[fbzMode].u)) {
		return;
	}

	// are we clearing the RGB buffer?
	if (FBZMODE_RGB_BUFFER_MASK(reg[fbzMode].u)) {
		// Determine the draw buffer
		const int destbuf = FBZMODE_DRAW_BUFFER(reg[fbzMode].u);
		switch (destbuf) {
		case 0: // front buffer
			drawbuf = (uint16_t*)(fbi.ram + fbi.rgboffs[fbi.frontbuf]);
			break;

		case 1: // back buffer
			drawbuf = (uint16_t*)(fbi.ram + fbi.rgboffs[fbi.backbuf]);
			break;

		default: // reserved
			break;
		}

		// Determine the dither pattern
		for (y = 0; y < 4; y++)
		{
			const uint8_t* dither_lookup = nullptr;
			const uint8_t* dither4       = nullptr;

			[[maybe_unused]] const uint8_t* dither = nullptr;

			COMPUTE_DITHER_POINTERS(reg[fbzMode].u, y);
			for (x = 0; x < 4; x++)
			{
				int r = reg[color1].rgb.r;
				int g = reg[color1].rgb.g;
				int b = reg[color1].rgb.b;

				APPLY_DITHER(reg[fbzMode].u, x, dither_lookup, r, g, b);
				dithermatrix[y*4 + x] = (uint16_t)((r << 11) | (g << 5) | b);
			}
		}
	}

	// Fill in a block of extents
	extents[0].startx = sx;
	extents[0].stopx = ex;

	constexpr auto num_extents = static_cast<int>(ARRAY_LEN(extents));

	for (auto extnum = 1; extnum < num_extents; ++extnum) {
		extents[extnum] = extents[0];
	}

#ifdef C_ENABLE_VOODOO_OPENGL
	if (ogl && active) {
		voodoo_ogl_fastfill();
		return;
	}
#endif

	// Iterate over blocks of extents
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

			// Force start < stop
			if (extent->startx > extent->stopx) {
				std::swap(extent->startx, extent->stopx);
			}

			// Set the extent and update the total pixel count
			FastFillRaster(dest, curscan, extent, dithermatrix);
		}
	}
}

/*-------------------------------------------------
    Execute the 'swapbuffer' command
-------------------------------------------------*/
void voodoo_state::ExecuteSwapBufferCmd(const uint32_t data)
{
	// Set the don't swap value for Voodoo 2
	fbi.vblank_dont_swap = ((data >> 9) & 1) > 0;

	SwapBuffers();
}


/*************************************
 *
 *  Chip reset
 *
 *************************************/

void voodoo_state::ResetCounters()
{
	UpdateStatistics(StatsCollection::Reset);

	reg[fbiPixelsIn].u   = 0;
	reg[fbiChromaFail].u = 0;
	reg[fbiZfuncFail].u  = 0;
	reg[fbiAfuncFail].u  = 0;
	reg[fbiPixelsOut].u  = 0;
}

void voodoo_state::SoftReset()
{
	ResetCounters();

	reg[fbiTrianglesOut].u = 0;
}

/*************************************
 *
 *  Voodoo register writes
 *
 *************************************/
void voodoo_state::WriteToRegister(const uint32_t offset, uint32_t data)
{
	auto chips = check_cast<uint8_t>((offset >> 8) & 0xf);

	int64_t data64;

	//LOG(LOG_VOODOO,LOG_WARN)("V3D:WR chip %x reg %x value %08x(%s)", chips, regnum<<2, data, voodoo_reg_name[regnum]);

	if (chips == 0) {
		chips = 0xf;
	}
	chips &= chipmask;

	/* the first 64 registers can be aliased differently */
	const auto is_aliased = (offset & 0x800c0) == 0x80000 && alt_regmap;

	const auto regnum = is_aliased ? register_alias_map[offset & 0x3f]
	                               : static_cast<uint8_t>(offset & 0xff);

	/* first make sure this register is readable */
	if ((regaccess[regnum] & REGISTER_WRITE) == 0) {
#ifdef C_ENABLE_VOODOO_DEBUG
		if (regnum <= 0xe0) {
			LOG(LOG_VOODOO, LOG_WARN)("VOODOO.ERROR:Invalid attempt to write %s\n", regnames[regnum]);
		} else
#endif
			LOG(LOG_VOODOO, LOG_WARN)("VOODOO.ERROR:Invalid attempt to write #%x\n", regnum);
		return;
	}

	/* switch off the register */
	switch (regnum) {
	/* Vertex data is 12.4 formatted fixed point */
	case fvertexAx: data = float_to_int32(data, 4); [[fallthrough]];
	case vertexAx:
		if ((chips & 1) != 0) {
			fbi.ax = (int16_t)(data & 0xffff);
		}
		break;

	case fvertexAy: data = float_to_int32(data, 4); [[fallthrough]];
	case vertexAy:
		if ((chips & 1) != 0) {
			fbi.ay = (int16_t)(data & 0xffff);
		}
		break;

	case fvertexBx: data = float_to_int32(data, 4); [[fallthrough]];
	case vertexBx:
		if ((chips & 1) != 0) {
			fbi.bx = (int16_t)(data & 0xffff);
		}
		break;

	case fvertexBy: data = float_to_int32(data, 4); [[fallthrough]];
	case vertexBy:
		if ((chips & 1) != 0) {
			fbi.by = (int16_t)(data & 0xffff);
		}
		break;

	case fvertexCx: data = float_to_int32(data, 4); [[fallthrough]];
	case vertexCx:
		if ((chips & 1) != 0) {
			fbi.cx = (int16_t)(data & 0xffff);
		}
		break;

	case fvertexCy: data = float_to_int32(data, 4); [[fallthrough]];
	case vertexCy:
		if ((chips & 1) != 0) {
			fbi.cy = (int16_t)(data & 0xffff);
		}
		break;

	/* RGB data is 12.12 formatted fixed point */
	case fstartR: data = float_to_int32(data, 12); [[fallthrough]];
	case startR:
		if ((chips & 1) != 0) {
			fbi.startr = (int32_t)(data << 8) >> 8;
		}
		break;

	case fstartG: data = float_to_int32(data, 12); [[fallthrough]];
	case startG:
		if ((chips & 1) != 0) {
			fbi.startg = (int32_t)(data << 8) >> 8;
		}
		break;

	case fstartB: data = float_to_int32(data, 12); [[fallthrough]];
	case startB:
		if ((chips & 1) != 0) {
			fbi.startb = (int32_t)(data << 8) >> 8;
		}
		break;

	case fstartA: data = float_to_int32(data, 12); [[fallthrough]];
	case startA:
		if ((chips & 1) != 0) {
			fbi.starta = (int32_t)(data << 8) >> 8;
		}
		break;

	case fdRdX: data = float_to_int32(data, 12); [[fallthrough]];
	case dRdX:
		if ((chips & 1) != 0) {
			fbi.drdx = (int32_t)(data << 8) >> 8;
		}
		break;

	case fdGdX: data = float_to_int32(data, 12); [[fallthrough]];
	case dGdX:
		if ((chips & 1) != 0) {
			fbi.dgdx = (int32_t)(data << 8) >> 8;
		}
		break;

	case fdBdX: data = float_to_int32(data, 12); [[fallthrough]];
	case dBdX:
		if ((chips & 1) != 0) {
			fbi.dbdx = (int32_t)(data << 8) >> 8;
		}
		break;

	case fdAdX: data = float_to_int32(data, 12); [[fallthrough]];
	case dAdX:
		if ((chips & 1) != 0) {
			fbi.dadx = (int32_t)(data << 8) >> 8;
		}
		break;

	case fdRdY: data = float_to_int32(data, 12); [[fallthrough]];
	case dRdY:
		if ((chips & 1) != 0) {
			fbi.drdy = (int32_t)(data << 8) >> 8;
		}
		break;

	case fdGdY: data = float_to_int32(data, 12); [[fallthrough]];
	case dGdY:
		if ((chips & 1) != 0) {
			fbi.dgdy = (int32_t)(data << 8) >> 8;
		}
		break;

	case fdBdY: data = float_to_int32(data, 12); [[fallthrough]];
	case dBdY:
		if ((chips & 1) != 0) {
			fbi.dbdy = (int32_t)(data << 8) >> 8;
		}
		break;

	case fdAdY: data = float_to_int32(data, 12); [[fallthrough]];
	case dAdY:
		if ((chips & 1) != 0) {
			fbi.dady = (int32_t)(data << 8) >> 8;
		}
		break;

	/* Z data is 20.12 formatted fixed point */
	case fstartZ: data = float_to_int32(data, 12); [[fallthrough]];
	case startZ:
		if ((chips & 1) != 0) {
			fbi.startz = (int32_t)data;
		}
		break;

	case fdZdX: data = float_to_int32(data, 12); [[fallthrough]];
	case dZdX:
		if ((chips & 1) != 0) {
			fbi.dzdx = (int32_t)data;
		}
		break;

	case fdZdY: data = float_to_int32(data, 12); [[fallthrough]];
	case dZdY:
		if ((chips & 1) != 0) {
			fbi.dzdy = (int32_t)data;
		}
		break;

	/* S,T data is 14.18 formatted fixed point, converted to 16.32
	 * internally */
	case fstartS:
		data64 = float_to_int64(data, 32);
		if ((chips & 2) != 0) {
			tmu[0].starts = data64;
		}
		if ((chips & 4) != 0) {
			tmu[1].starts = data64;
		}
		break;
	case startS:
		if ((chips & 2) != 0) {
			tmu[0].starts = (int64_t)(int32_t)data << 14;
		}
		if ((chips & 4) != 0) {
			tmu[1].starts = (int64_t)(int32_t)data << 14;
		}
		break;

	case fstartT:
		data64 = float_to_int64(data, 32);
		if ((chips & 2) != 0) {
			tmu[0].startt = data64;
		}
		if ((chips & 4) != 0) {
			tmu[1].startt = data64;
		}
		break;
	case startT:
		if ((chips & 2) != 0) {
			tmu[0].startt = (int64_t)(int32_t)data << 14;
		}
		if ((chips & 4) != 0) {
			tmu[1].startt = (int64_t)(int32_t)data << 14;
		}
		break;

	case fdSdX:
		data64 = float_to_int64(data, 32);
		if ((chips & 2) != 0) {
			tmu[0].dsdx = data64;
		}
		if ((chips & 4) != 0) {
			tmu[1].dsdx = data64;
		}
		break;
	case dSdX:
		if ((chips & 2) != 0) {
			tmu[0].dsdx = (int64_t)(int32_t)data << 14;
		}
		if ((chips & 4) != 0) {
			tmu[1].dsdx = (int64_t)(int32_t)data << 14;
		}
		break;

	case fdTdX:
		data64 = float_to_int64(data, 32);
		if ((chips & 2) != 0) {
			tmu[0].dtdx = data64;
		}
		if ((chips & 4) != 0) {
			tmu[1].dtdx = data64;
		}
		break;
	case dTdX:
		if ((chips & 2) != 0) {
			tmu[0].dtdx = (int64_t)(int32_t)data << 14;
		}
		if ((chips & 4) != 0) {
			tmu[1].dtdx = (int64_t)(int32_t)data << 14;
		}
		break;

	case fdSdY:
		data64 = float_to_int64(data, 32);
		if ((chips & 2) != 0) {
			tmu[0].dsdy = data64;
		}
		if ((chips & 4) != 0) {
			tmu[1].dsdy = data64;
		}
		break;
	case dSdY:
		if ((chips & 2) != 0) {
			tmu[0].dsdy = (int64_t)(int32_t)data << 14;
		}
		if ((chips & 4) != 0) {
			tmu[1].dsdy = (int64_t)(int32_t)data << 14;
		}
		break;

	case fdTdY:
		data64 = float_to_int64(data, 32);
		if ((chips & 2) != 0) {
			tmu[0].dtdy = data64;
		}
		if ((chips & 4) != 0) {
			tmu[1].dtdy = data64;
		}
		break;
	case dTdY:
		if ((chips & 2) != 0) {
			tmu[0].dtdy = (int64_t)(int32_t)data << 14;
		}
		if ((chips & 4) != 0) {
			tmu[1].dtdy = (int64_t)(int32_t)data << 14;
		}
		break;

	/* W data is 2.30 formatted fixed point, converted to 16.32 internally */
	case fstartW:
		data64 = float_to_int64(data, 32);
		if ((chips & 1) != 0) {
			fbi.startw = data64;
		}
		if ((chips & 2) != 0) {
			tmu[0].startw = data64;
		}
		if ((chips & 4) != 0) {
			tmu[1].startw = data64;
		}
		break;
	case startW:
		if ((chips & 1) != 0) {
			fbi.startw = (int64_t)(int32_t)data << 2;
		}
		if ((chips & 2) != 0) {
			tmu[0].startw = (int64_t)(int32_t)data << 2;
		}
		if ((chips & 4) != 0) {
			tmu[1].startw = (int64_t)(int32_t)data << 2;
		}
		break;

	case fdWdX:
		data64 = float_to_int64(data, 32);
		if ((chips & 1) != 0) {
			fbi.dwdx = data64;
		}
		if ((chips & 2) != 0) {
			tmu[0].dwdx = data64;
		}
		if ((chips & 4) != 0) {
			tmu[1].dwdx = data64;
		}
		break;
	case dWdX:
		if ((chips & 1) != 0) {
			fbi.dwdx = (int64_t)(int32_t)data << 2;
		}
		if ((chips & 2) != 0) {
			tmu[0].dwdx = (int64_t)(int32_t)data << 2;
		}
		if ((chips & 4) != 0) {
			tmu[1].dwdx = (int64_t)(int32_t)data << 2;
		}
		break;

	case fdWdY:
		data64 = float_to_int64(data, 32);
		if ((chips & 1) != 0) {
			fbi.dwdy = data64;
		}
		if ((chips & 2) != 0) {
			tmu[0].dwdy = data64;
		}
		if ((chips & 4) != 0) {
			tmu[1].dwdy = data64;
		}
		break;
	case dWdY:
		if ((chips & 1) != 0) {
			fbi.dwdy = (int64_t)(int32_t)data << 2;
		}
		if ((chips & 2) != 0) {
			tmu[0].dwdy = (int64_t)(int32_t)data << 2;
		}
		if ((chips & 4) != 0) {
			tmu[1].dwdy = (int64_t)(int32_t)data << 2;
		}
		break;

	/* setup bits */
	case sARGB:
		if ((chips & 1) != 0) {
			reg[sAlpha].f = (float)RGB_ALPHA(data);
			reg[sRed].f   = (float)RGB_RED(data);
			reg[sGreen].f = (float)RGB_GREEN(data);
			reg[sBlue].f  = (float)RGB_BLUE(data);
		}
		break;

	/* mask off invalid bits for different cards */
	case fbzColorPath:
		if (vtype < VOODOO_2) {
			data &= 0x0fffffff;
		}
		if ((chips & 1) != 0) {
			reg[fbzColorPath].u = data;
		}
		break;

	case fbzMode:
		if (vtype < VOODOO_2) {
			data &= 0x001fffff;
		}
		if ((chips & 1) != 0) {
#ifdef C_ENABLE_VOODOO_OPENGL
			if (ogl && active &&
			    (FBZMODE_Y_ORIGIN(reg[fbzMode].u) !=
			     FBZMODE_Y_ORIGIN(data))) {
				reg[fbzMode].u = data;
				voodoo_ogl_set_window(this);
			} else
#endif
			{
				reg[fbzMode].u = data;
			}
		}
		break;

	case fogMode:
		if (vtype < VOODOO_2) {
			data &= 0x0000003f;
		}
		if ((chips & 1) != 0) {
			reg[fogMode].u = data;
		}
		break;

	/* triangle drawing */
	case triangleCMD:
	case ftriangleCMD: ExecuteTriangleCmd(); break;

	case sBeginTriCMD: ExecuteBeginTriangleCmd(); break;

	case sDrawTriCMD: ExecuteDrawTriangleCmd(); break;

	/* other commands */
	case nopCMD:
		if ((data & 1) != 0u) {
			ResetCounters();
		}
		if ((data & 2) != 0u) {
			reg[fbiTrianglesOut].u = 0;
		}
		break;

	case fastfillCMD: ExecuteFastFillCmd(); break;

	case swapbufferCMD: ExecuteSwapBufferCmd(data); break;

	/* gamma table access -- Voodoo/Voodoo2 only */
	case clutData:
		/*
		if (chips & 1)
		{
			if (!FBIINIT1_VIDEO_TIMING_RESET(reg[fbiInit1].u))
			{
				int index = data >> 24;
				if (index <= 32)
				{
					// fbi.clut[index] = data;
				}
			}
			else
				LOG(LOG_VOODOO,LOG_WARN)("clutData ignored because video timing reset = 1\n");
		}
		*/
		break;

	/* external DAC access -- Voodoo/Voodoo2 only */
	case dacData:
		if ((chips & 1) != 0) {
			if ((data & 0x800) == 0u) {
				dacdata_w(&dac, (data >> 8) & 7, data & 0xff);
			} else {
				dacdata_r(&dac, (data >> 8) & 7);
			}
		}
		break;

	/* vertical sync rate -- Voodoo/Voodoo2 only */
	case hSync:
	case vSync:
	case backPorch:
	case videoDimensions:
		if ((chips & 1) != 0) {
			reg[regnum].u = data;
			if (reg[hSync].u != 0 && reg[vSync].u != 0 &&
			    reg[videoDimensions].u != 0) {
				const int hvis = reg[videoDimensions].u & 0x3ff;
				const int vvis = (reg[videoDimensions].u >> 16) &
				                 0x3ff;

#ifdef C_ENABLE_VOODOO_DEBUG
				int htotal = ((reg[hSync].u >> 16) & 0x3ff) +
				             1 + (reg[hSync].u & 0xff) + 1;
				int vtotal = ((reg[vSync].u >> 16) & 0xfff) +
				             (reg[vSync].u & 0xfff);

				int hbp = (reg[backPorch].u & 0xff) + 2;
				int vbp = (reg[backPorch].u >> 16) & 0xff;

				// attoseconds_t refresh =
				// video_screen_get_frame_period(screen).attoseconds;
				attoseconds_t refresh = 0;
				attoseconds_t stdperiod, medperiod, vgaperiod;
				attoseconds_t stddiff, meddiff, vgadiff;

				/* compute the new period for standard res,
				 * medium res, and VGA res */
				stdperiod = HZ_TO_ATTOSECONDS(15750) * vtotal;
				medperiod = HZ_TO_ATTOSECONDS(25000) * vtotal;
				vgaperiod = HZ_TO_ATTOSECONDS(31500) * vtotal;

				/* compute a diff against the current refresh
				 * period */
				stddiff = stdperiod - refresh;
				if (stddiff < 0) {
					stddiff = -stddiff;
				}
				meddiff = medperiod - refresh;
				if (meddiff < 0) {
					meddiff = -meddiff;
				}
				vgadiff = vgaperiod - refresh;
				if (vgadiff < 0) {
					vgadiff = -vgadiff;
				}

				LOG(LOG_VOODOO, LOG_WARN)
				("hSync=%08X  vSync=%08X  backPorch=%08X  videoDimensions=%08X\n",
				 reg[hSync].u,
				 reg[vSync].u,
				 reg[backPorch].u,
				 reg[videoDimensions].u);

				rectangle visarea;

				/* create a new visarea */
				visarea.min_x = hbp;
				visarea.max_x = hbp + hvis - 1;
				visarea.min_y = vbp;
				visarea.max_y = vbp + vvis - 1;

				/* keep within bounds */
				visarea.max_x = std::min(visarea.max_x, htotal - 1);
				visarea.max_y = std::min(visarea.max_y, vtotal - 1);
				LOG(LOG_VOODOO, LOG_WARN)
				("Horiz: %d-%d (%d total)  Vert: %d-%d (%d total) -- ",
				 visarea.min_x,
				 visarea.max_x,
				 htotal,
				 visarea.min_y,
				 visarea.max_y,
				 vtotal);

				/* configure the screen based on which one
				 * matches the closest */
				if (stddiff < meddiff && stddiff < vgadiff) {
					// video_screen_configure(screen, htotal, vtotal, &visarea, stdperiod);
					LOG(LOG_VOODOO, LOG_WARN)("Standard resolution, %f Hz\n", ATTOSECONDS_TO_HZ(stdperiod));
				} else if (meddiff < vgadiff) {
					// video_screen_configure(screen, htotal, vtotal, &visarea, medperiod);
					LOG(LOG_VOODOO, LOG_WARN)("Medium resolution, %f Hz\n", ATTOSECONDS_TO_HZ(medperiod));
				} else {
					// video_screen_configure(screen, htotal, vtotal, &visarea, vgaperiod);
					LOG(LOG_VOODOO, LOG_WARN)("VGA resolution, %f Hz\n", ATTOSECONDS_TO_HZ(vgaperiod));
				}
#endif

				/* configure the new framebuffer info */
				const uint32_t new_width  = (hvis + 1) & ~1;
				const uint32_t new_height = (vvis + 1) & ~1;

				if ((fbi.width != new_width) ||
				    (fbi.height != new_height)) {
					fbi.width  = new_width;
					fbi.height = new_height;
#ifdef C_ENABLE_VOODOO_OPENGL
					ogl_dimchange = true;
#endif
				}
				// fbi.xoffs = hbp;
				// fbi.yoffs = vbp;
				// fbi.vsyncscan = (reg[vSync].u >> 16)
				// & 0xfff;

				/* recompute the time of VBLANK */
				// adjust_vblank_timer(this);

				/* if changing dimensions, update video memory
				 * layout */
				if (regnum == videoDimensions) {
					RecomputeVideoMemory();
				}

				UpdateScreenStart();
			}
		}
		break;

	/* fbiInit0 can only be written if initEnable says we can --
	 * Voodoo/Voodoo2 only */
	case fbiInit0:
		if (((chips & 1) != 0) && INITEN_ENABLE_HW_INIT(pci.init_enable)) {
			const bool new_output_on = FBIINIT0_VGA_PASSTHRU(data);

			if (output_on != new_output_on) {
				output_on = new_output_on;
				UpdateScreenStart();
			}

			reg[fbiInit0].u = data;
			if (FBIINIT0_GRAPHICS_RESET(data)) {
				SoftReset();
			}
			RecomputeVideoMemory();
		}
		break;

	/* fbiInit5-7 are Voodoo 2-only; ignore them on anything else */
	case fbiInit5:
	case fbiInit6:
		if (vtype < VOODOO_2) {
			break;
		}
	/* else fall through... */

	/* fbiInitX can only be written if initEnable says we can --
	 * Voodoo/Voodoo2 only */
	/* most of these affect memory layout, so always recompute that when
	 * done */
	case fbiInit1:
	case fbiInit2:
	case fbiInit4:
		if (((chips & 1) != 0) && INITEN_ENABLE_HW_INIT(pci.init_enable)) {
			reg[regnum].u = data;
			RecomputeVideoMemory();
		}
		break;

	case fbiInit3:
		if (((chips & 1) != 0) && INITEN_ENABLE_HW_INIT(pci.init_enable)) {
			reg[regnum].u = data;
			alt_regmap    = (FBIINIT3_TRI_REGISTER_REMAP(data) > 0);
			fbi.yorigin = FBIINIT3_YORIGIN_SUBTRACT(reg[fbiInit3].u);
			RecomputeVideoMemory();
		}
		break;

	/* nccTable entries are processed and expanded immediately */
	case nccTable + 0:
	case nccTable + 1:
	case nccTable + 2:
	case nccTable + 3:
	case nccTable + 4:
	case nccTable + 5:
	case nccTable + 6:
	case nccTable + 7:
	case nccTable + 8:
	case nccTable + 9:
	case nccTable + 10:
	case nccTable + 11:
		if ((chips & 2) != 0) {
			WriteToNccTable(&tmu[0].ncc[0], regnum - nccTable, data);
		}
		if ((chips & 4) != 0) {
			WriteToNccTable(&tmu[1].ncc[0], regnum - nccTable, data);
		}
		break;

	case nccTable + 12:
	case nccTable + 13:
	case nccTable + 14:
	case nccTable + 15:
	case nccTable + 16:
	case nccTable + 17:
	case nccTable + 18:
	case nccTable + 19:
	case nccTable + 20:
	case nccTable + 21:
	case nccTable + 22:
	case nccTable + 23:
		if ((chips & 2) != 0) {
			WriteToNccTable(&tmu[0].ncc[1], regnum - (nccTable + 12), data);
		}
		if ((chips & 4) != 0) {
			WriteToNccTable(&tmu[1].ncc[1], regnum - (nccTable + 12), data);
		}
		break;

	/* fogTable entries are processed and expanded immediately */
	case fogTable + 0:
	case fogTable + 1:
	case fogTable + 2:
	case fogTable + 3:
	case fogTable + 4:
	case fogTable + 5:
	case fogTable + 6:
	case fogTable + 7:
	case fogTable + 8:
	case fogTable + 9:
	case fogTable + 10:
	case fogTable + 11:
	case fogTable + 12:
	case fogTable + 13:
	case fogTable + 14:
	case fogTable + 15:
	case fogTable + 16:
	case fogTable + 17:
	case fogTable + 18:
	case fogTable + 19:
	case fogTable + 20:
	case fogTable + 21:
	case fogTable + 22:
	case fogTable + 23:
	case fogTable + 24:
	case fogTable + 25:
	case fogTable + 26:
	case fogTable + 27:
	case fogTable + 28:
	case fogTable + 29:
	case fogTable + 30:
	case fogTable + 31:
		if ((chips & 1) != 0) {
			const int base = 2 * (regnum - fogTable);

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
		if ((chips & 2) != 0) {
			tmu[0].reg[regnum].u = data;
			tmu[0].regdirty = true;
		}
		if ((chips & 4) != 0) {
			tmu[1].reg[regnum].u = data;
			tmu[1].regdirty = true;
		}
		break;

	case trexInit1:
		/* send tmu config data to the frame buffer */
		send_config = (TREXINIT_SEND_TMU_CONFIG(data) > 0);
		goto default_case;
		break;

	case clipLowYHighY:
	case clipLeftRight:
		if ((chips & 1) != 0) {
			reg[0x000 + regnum].u = data;
		}
#ifdef C_ENABLE_VOODOO_OPENGL
		if (ogl) {
			voodoo_ogl_clip_window(this);
		}
#endif
		break;

	/* these registers are referenced in the renderer; we must wait for
	 * pending work before changing */
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
			reg[0x000 + regnum].u = data;
		}
		if ((chips & 2) != 0) {
			reg[0x100 + regnum].u = data;
		}
		if ((chips & 4) != 0) {
			reg[0x200 + regnum].u = data;
		}
		if ((chips & 8) != 0) {
			reg[0x300 + regnum].u = data;
		}
		break;
	}
}

/*************************************
 *
 *  Voodoo LFB writes
 *
 *************************************/
void voodoo_state::WriteToFrameBuffer(uint32_t offset, uint32_t data, uint32_t mem_mask)
{
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
	if (LFBMODE_BYTE_SWIZZLE_WRITES(reg[lfbMode].u)) {
		data     = bswap_u32(data);
		mem_mask = bswap_u32(mem_mask);
	}

	/* word swapping */
	if (LFBMODE_WORD_SWAP_WRITES(reg[lfbMode].u)) {
		data     = (data << 16) | (data >> 16);
		mem_mask = (mem_mask << 16) | (mem_mask >> 16);
	}

	/* extract default depth and alpha values */
	sw[0] = sw[1] = reg[zaColor].u & 0xffff;
	sa[0] = sa[1] = reg[zaColor].u >> 24;

	/* first extract A,R,G,B from the data */
	switch (LFBMODE_WRITE_FORMAT(reg[lfbMode].u) +
	        16 * LFBMODE_RGBA_LANES(reg[lfbMode].u)) {
	case 16 * 0 + 0: /* ARGB, 16-bit RGB 5-6-5 */
	case 16 * 2 + 0: /* RGBA, 16-bit RGB 5-6-5 */
		EXTRACT_565_TO_888(data, sr[0], sg[0], sb[0]);
		EXTRACT_565_TO_888(data >> 16, sr[1], sg[1], sb[1]);
		mask = LFB_RGB_PRESENT | (LFB_RGB_PRESENT << 4);
		offset <<= 1;
		break;
	case 16 * 1 + 0: /* ABGR, 16-bit RGB 5-6-5 */
	case 16 * 3 + 0: /* BGRA, 16-bit RGB 5-6-5 */
		EXTRACT_565_TO_888(data, sb[0], sg[0], sr[0]);
		EXTRACT_565_TO_888(data >> 16, sb[1], sg[1], sr[1]);
		mask = LFB_RGB_PRESENT | (LFB_RGB_PRESENT << 4);
		offset <<= 1;
		break;

	case 16 * 0 + 1: /* ARGB, 16-bit RGB x-5-5-5 */
		EXTRACT_x555_TO_888(data, sr[0], sg[0], sb[0]);
		EXTRACT_x555_TO_888(data >> 16, sr[1], sg[1], sb[1]);
		mask = LFB_RGB_PRESENT | (LFB_RGB_PRESENT << 4);
		offset <<= 1;
		break;
	case 16 * 1 + 1: /* ABGR, 16-bit RGB x-5-5-5 */
		EXTRACT_x555_TO_888(data, sb[0], sg[0], sr[0]);
		EXTRACT_x555_TO_888(data >> 16, sb[1], sg[1], sr[1]);
		mask = LFB_RGB_PRESENT | (LFB_RGB_PRESENT << 4);
		offset <<= 1;
		break;
	case 16 * 2 + 1: /* RGBA, 16-bit RGB x-5-5-5 */
		EXTRACT_555x_TO_888(data, sr[0], sg[0], sb[0]);
		EXTRACT_555x_TO_888(data >> 16, sr[1], sg[1], sb[1]);
		mask = LFB_RGB_PRESENT | (LFB_RGB_PRESENT << 4);
		offset <<= 1;
		break;
	case 16 * 3 + 1: /* BGRA, 16-bit RGB x-5-5-5 */
		EXTRACT_555x_TO_888(data, sb[0], sg[0], sr[0]);
		EXTRACT_555x_TO_888(data >> 16, sb[1], sg[1], sr[1]);
		mask = LFB_RGB_PRESENT | (LFB_RGB_PRESENT << 4);
		offset <<= 1;
		break;

	case 16 * 0 + 2: /* ARGB, 16-bit ARGB 1-5-5-5 */
		EXTRACT_1555_TO_8888(data, sa[0], sr[0], sg[0], sb[0]);
		EXTRACT_1555_TO_8888(data >> 16, sa[1], sr[1], sg[1], sb[1]);
		mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT |
		       ((LFB_RGB_PRESENT | LFB_ALPHA_PRESENT) << 4);
		offset <<= 1;
		break;
	case 16 * 1 + 2: /* ABGR, 16-bit ARGB 1-5-5-5 */
		EXTRACT_1555_TO_8888(data, sa[0], sb[0], sg[0], sr[0]);
		EXTRACT_1555_TO_8888(data >> 16, sa[1], sb[1], sg[1], sr[1]);
		mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT |
		       ((LFB_RGB_PRESENT | LFB_ALPHA_PRESENT) << 4);
		offset <<= 1;
		break;
	case 16 * 2 + 2: /* RGBA, 16-bit ARGB 1-5-5-5 */
		EXTRACT_5551_TO_8888(data, sr[0], sg[0], sb[0], sa[0]);
		EXTRACT_5551_TO_8888(data >> 16, sr[1], sg[1], sb[1], sa[1]);
		mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT |
		       ((LFB_RGB_PRESENT | LFB_ALPHA_PRESENT) << 4);
		offset <<= 1;
		break;
	case 16 * 3 + 2: /* BGRA, 16-bit ARGB 1-5-5-5 */
		EXTRACT_5551_TO_8888(data, sb[0], sg[0], sr[0], sa[0]);
		EXTRACT_5551_TO_8888(data >> 16, sb[1], sg[1], sr[1], sa[1]);
		mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT |
		       ((LFB_RGB_PRESENT | LFB_ALPHA_PRESENT) << 4);
		offset <<= 1;
		break;

	case 16 * 0 + 4: /* ARGB, 32-bit RGB x-8-8-8 */
		EXTRACT_x888_TO_888(data, sr[0], sg[0], sb[0]);
		mask = LFB_RGB_PRESENT;
		break;
	case 16 * 1 + 4: /* ABGR, 32-bit RGB x-8-8-8 */
		EXTRACT_x888_TO_888(data, sb[0], sg[0], sr[0]);
		mask = LFB_RGB_PRESENT;
		break;
	case 16 * 2 + 4: /* RGBA, 32-bit RGB x-8-8-8 */
		EXTRACT_888x_TO_888(data, sr[0], sg[0], sb[0]);
		mask = LFB_RGB_PRESENT;
		break;
	case 16 * 3 + 4: /* BGRA, 32-bit RGB x-8-8-8 */
		EXTRACT_888x_TO_888(data, sb[0], sg[0], sr[0]);
		mask = LFB_RGB_PRESENT;
		break;

	case 16 * 0 + 5: /* ARGB, 32-bit ARGB 8-8-8-8 */
		EXTRACT_8888_TO_8888(data, sa[0], sr[0], sg[0], sb[0]);
		mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT;
		break;
	case 16 * 1 + 5: /* ABGR, 32-bit ARGB 8-8-8-8 */
		EXTRACT_8888_TO_8888(data, sa[0], sb[0], sg[0], sr[0]);
		mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT;
		break;
	case 16 * 2 + 5: /* RGBA, 32-bit ARGB 8-8-8-8 */
		EXTRACT_8888_TO_8888(data, sr[0], sg[0], sb[0], sa[0]);
		mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT;
		break;
	case 16 * 3 + 5: /* BGRA, 32-bit ARGB 8-8-8-8 */
		EXTRACT_8888_TO_8888(data, sb[0], sg[0], sr[0], sa[0]);
		mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT;
		break;

	case 16 * 0 + 12: /* ARGB, 32-bit depth+RGB 5-6-5 */
	case 16 * 2 + 12: /* RGBA, 32-bit depth+RGB 5-6-5 */
		sw[0] = data >> 16;
		EXTRACT_565_TO_888(data, sr[0], sg[0], sb[0]);
		mask = LFB_RGB_PRESENT | LFB_DEPTH_PRESENT_MSW;
		break;
	case 16 * 1 + 12: /* ABGR, 32-bit depth+RGB 5-6-5 */
	case 16 * 3 + 12: /* BGRA, 32-bit depth+RGB 5-6-5 */
		sw[0] = data >> 16;
		EXTRACT_565_TO_888(data, sb[0], sg[0], sr[0]);
		mask = LFB_RGB_PRESENT | LFB_DEPTH_PRESENT_MSW;
		break;

	case 16 * 0 + 13: /* ARGB, 32-bit depth+RGB x-5-5-5 */
		sw[0] = data >> 16;
		EXTRACT_x555_TO_888(data, sr[0], sg[0], sb[0]);
		mask = LFB_RGB_PRESENT | LFB_DEPTH_PRESENT_MSW;
		break;
	case 16 * 1 + 13: /* ABGR, 32-bit depth+RGB x-5-5-5 */
		sw[0] = data >> 16;
		EXTRACT_x555_TO_888(data, sb[0], sg[0], sr[0]);
		mask = LFB_RGB_PRESENT | LFB_DEPTH_PRESENT_MSW;
		break;
	case 16 * 2 + 13: /* RGBA, 32-bit depth+RGB x-5-5-5 */
		sw[0] = data >> 16;
		EXTRACT_555x_TO_888(data, sr[0], sg[0], sb[0]);
		mask = LFB_RGB_PRESENT | LFB_DEPTH_PRESENT_MSW;
		break;
	case 16 * 3 + 13: /* BGRA, 32-bit depth+RGB x-5-5-5 */
		sw[0] = data >> 16;
		EXTRACT_555x_TO_888(data, sb[0], sg[0], sr[0]);
		mask = LFB_RGB_PRESENT | LFB_DEPTH_PRESENT_MSW;
		break;

	case 16 * 0 + 14: /* ARGB, 32-bit depth+ARGB 1-5-5-5 */
		sw[0] = data >> 16;
		EXTRACT_1555_TO_8888(data, sa[0], sr[0], sg[0], sb[0]);
		mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT | LFB_DEPTH_PRESENT_MSW;
		break;
	case 16 * 1 + 14: /* ABGR, 32-bit depth+ARGB 1-5-5-5 */
		sw[0] = data >> 16;
		EXTRACT_1555_TO_8888(data, sa[0], sb[0], sg[0], sr[0]);
		mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT | LFB_DEPTH_PRESENT_MSW;
		break;
	case 16 * 2 + 14: /* RGBA, 32-bit depth+ARGB 1-5-5-5 */
		sw[0] = data >> 16;
		EXTRACT_5551_TO_8888(data, sr[0], sg[0], sb[0], sa[0]);
		mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT | LFB_DEPTH_PRESENT_MSW;
		break;
	case 16 * 3 + 14: /* BGRA, 32-bit depth+ARGB 1-5-5-5 */
		sw[0] = data >> 16;
		EXTRACT_5551_TO_8888(data, sb[0], sg[0], sr[0], sa[0]);
		mask = LFB_RGB_PRESENT | LFB_ALPHA_PRESENT | LFB_DEPTH_PRESENT_MSW;
		break;

	case 16 * 0 + 15: /* ARGB, 16-bit depth */
	case 16 * 1 + 15: /* ARGB, 16-bit depth */
	case 16 * 2 + 15: /* ARGB, 16-bit depth */
	case 16 * 3 + 15: /* ARGB, 16-bit depth */
		sw[0] = data & 0xffff;
		sw[1] = data >> 16;
		mask  = LFB_DEPTH_PRESENT | (LFB_DEPTH_PRESENT << 4);
		offset <<= 1;
		break;

	default: /* reserved */ return;
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
	destbuf = LFBMODE_WRITE_BUFFER_SELECT(reg[lfbMode].u);
	//LOG(LOG_VOODOO,LOG_WARN)("destbuf %X lfbmode %X",destbuf, reg[lfbMode].u);

	assert(destbuf == 0 || destbuf == 1);
	switch (destbuf) {
	case 0: /* front buffer */
		dest = (uint16_t*)(fbi.ram + fbi.rgboffs[fbi.frontbuf]);
		destmax = (fbi.mask + 1 - fbi.rgboffs[fbi.frontbuf]) / 2;
		break;

	case 1: /* back buffer */
		dest = (uint16_t*)(fbi.ram + fbi.rgboffs[fbi.backbuf]);
		destmax = (fbi.mask + 1 - fbi.rgboffs[fbi.backbuf]) / 2;
		break;

	default: /* reserved */ return;
	}
	depth = (uint16_t*)(fbi.ram + fbi.auxoffs);
	depthmax = (fbi.mask + 1 - fbi.auxoffs) / 2;

	/* simple case: no pipeline */
	if (!LFBMODE_ENABLE_PIXEL_PIPELINE(reg[lfbMode].u)) {
		const uint8_t* dither_lookup = nullptr;
		const uint8_t* dither4       = nullptr;

		[[maybe_unused]] const uint8_t* dither = nullptr;

		uint32_t bufoffs;

		if (LOG_LFB != 0u) {
			LOG(LOG_VOODOO, LOG_WARN)
			("VOODOO.LFB:write raw mode %X (%d,%d) = %08X & %08X\n",
			 LFBMODE_WRITE_FORMAT(reg[lfbMode].u),
			 x,
			 y,
			 data,
			 mem_mask);
		}

		/* determine the screen Y */
		scry = y;
		if (LFBMODE_Y_ORIGIN(reg[lfbMode].u)) {
			scry = (fbi.yorigin - y) & 0x3ff;
		}

		/* advance pointers to the proper row */
		bufoffs = scry * fbi.rowpixels + x;

		/* compute dithering */
		COMPUTE_DITHER_POINTERS(reg[fbzMode].u, y);

		/* loop over up to two pixels */
		for (pix = 0; mask != 0; pix++) {
			/* make sure we care about this pixel */
			if ((mask & 0x0f) != 0) {
				const auto has_rgb = (mask & LFB_RGB_PRESENT) > 0;
				const auto has_alpha =
				        ((mask & LFB_ALPHA_PRESENT) > 0) &&
				        (FBZMODE_ENABLE_ALPHA_PLANES(
				                 reg[fbzMode].u) > 0);

				const auto has_depth = ((mask &
				                         (LFB_DEPTH_PRESENT |
				                          LFB_DEPTH_PRESENT_MSW)) &&
				                        !FBZMODE_ENABLE_ALPHA_PLANES(
				                                reg[fbzMode].u));
#ifdef C_ENABLE_VOODOO_OPENGL
				if (ogl && active) {
					if (has_rgb || has_alpha) {
						// if enabling dithering: output is 565 not 888 anymore
						//APPLY_DITHER(reg[fbzMode].u, x, dither_lookup, sr[pix], sg[pix], sb[pix]);
						voodoo_ogl_draw_pixel(x, scry+1, has_rgb, has_alpha, sr[pix], sg[pix], sb[pix], sa[pix]);
					}
					if (has_depth) {
						voodoo_ogl_draw_z(x, scry + 1, sw[pix]);
					}
				} else
#endif
				{
					/* write to the RGB buffer */
					if (has_rgb && bufoffs < destmax) {
						/* apply dithering and write to the screen */
						APPLY_DITHER(reg[fbzMode].u, x, dither_lookup, sr[pix], sg[pix], sb[pix]);
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
				reg[fbiPixelsOut].u++;
			}

			/* advance our pointers */
			bufoffs++;
			x++;
			mask >>= 4;
		}
	}

	/* tricky case: run the full pixel pipeline on the pixel */
	else {
		const uint8_t* dither_lookup = nullptr;
		const uint8_t* dither4       = nullptr;
		const uint8_t* dither        = nullptr;

		if (LOG_LFB != 0u) {
			LOG(LOG_VOODOO, LOG_WARN)
			("VOODOO.LFB:write pipelined mode %X (%d,%d) = %08X & %08X\n",
			 LFBMODE_WRITE_FORMAT(reg[lfbMode].u),
			 x,
			 y,
			 data,
			 mem_mask);
		}

		/* determine the screen Y */
		scry = y;
		if (FBZMODE_Y_ORIGIN(reg[fbzMode].u)) {
			scry = (fbi.yorigin - y) & 0x3ff;
		}

		/* advance pointers to the proper row */
		dest += scry * fbi.rowpixels;
		if (depth != nullptr) {
			depth += scry * fbi.rowpixels;
		}

		/* compute dithering */
		COMPUTE_DITHER_POINTERS(reg[fbzMode].u, y);

		int32_t blendr = 0;
		int32_t blendg = 0;
		int32_t blendb = 0;
		int32_t blenda = 0;

		/* loop over up to two pixels */
		Stats stats = {};
		for (pix = 0; mask != 0; pix++) {
			/* make sure we care about this pixel */
			if ((mask & 0x0f) != 0) {
				const int64_t iterw = static_cast<int64_t>(sw[pix]) << (30 - 16);
				const int32_t iterz = sw[pix] << 12;

				rgb_union color;

				/* apply clipping */
				if (FBZMODE_ENABLE_CLIPPING(reg[fbzMode].u)) {
					if (x < (int32_t)((reg[clipLeftRight].u >> 16) &
					                  0x3ff) ||
					    x >= (int32_t)(reg[clipLeftRight].u & 0x3ff) ||
					    scry < (int32_t)((reg[clipLowYHighY].u >> 16) &
					                     0x3ff) ||
					    scry >= (int32_t)(reg[clipLowYHighY].u &
					                      0x3ff)) {
						stats.pixels_in++;
						// stats.clip_fail++;
						goto nextpixel;
					}
				}

				// Pixel pipeline part 1 handles depth testing
				// and stippling
				// TODO: in the ogl case this macro doesn't
				// really work with depth testing
				PIXEL_PIPELINE_BEGIN(stats,
				                     x,
				                     y,
				                     reg[fbzColorPath].u,
				                     reg[fbzMode].u,
				                     iterz,
				                     iterw,
				                     reg[zaColor].u,
				                     reg[stipple].u);

				color.rgb.r = static_cast<uint8_t>(sr[pix]);
				color.rgb.g = static_cast<uint8_t>(sg[pix]);
				color.rgb.b = static_cast<uint8_t>(sb[pix]);
				color.rgb.a = static_cast<uint8_t>(sa[pix]);

				// Apply chroma key
				APPLY_CHROMAKEY(stats, reg[fbzMode].u, color);

				// Apply alpha mask, and alpha testing
				APPLY_ALPHAMASK(stats, reg[fbzMode].u, color.rgb.a);
				APPLY_ALPHATEST(stats,
				                reg[alphaMode].u,
				                color.rgb.a);

				/*
				if (FBZCP_CC_MSELECT(reg[fbzColorPath].u) != 0) LOG_DEBUG("VOODOO: lfbw fpp mselect %8x",FBZCP_CC_MSELECT(reg[fbzColorPath].u));
				if (FBZCP_CCA_MSELECT(reg[fbzColorPath].u) > 1) LOG_DEBUG("VOODOO: lfbw fpp mselect alpha %8x",FBZCP_CCA_MSELECT(reg[fbzColorPath].u));

				if (FBZCP_CC_REVERSE_BLEND(reg[fbzColorPath].u) != 0) {
					if (FBZCP_CC_MSELECT(reg[fbzColorPath].u) != 0) LOG_DEBUG("VOODOO: lfbw fpp rblend %8x",FBZCP_CC_REVERSE_BLEND(reg[fbzColorPath].u));
				}
				if (FBZCP_CCA_REVERSE_BLEND(reg[fbzColorPath].u) != 0) {
					if (FBZCP_CC_MSELECT(reg[fbzColorPath].u) != 0) LOG_DEBUG("VOODOO: lfbw fpp rblend alpha %8x",FBZCP_CCA_REVERSE_BLEND(reg[fbzColorPath].u));
				}
				*/

				rgb_union c_local;

				/* compute c_local */
				if (FBZCP_CC_LOCALSELECT_OVERRIDE(
				            reg[fbzColorPath].u) == 0) {
					if (FBZCP_CC_LOCALSELECT(
					            reg[fbzColorPath].u) == 0) /* iterated RGB */
					{
						// c_local.u = iterargb.u;
						c_local.rgb.r = static_cast<uint8_t>(
						        sr[pix]);
						c_local.rgb.g = static_cast<uint8_t>(
						        sg[pix]);
						c_local.rgb.b = static_cast<uint8_t>(
						        sb[pix]);
					} else {
						// color0 RGB
						c_local.u = reg[color0].u;
					}
				} else {
					LOG_DEBUG("VOODOO: lfbw fpp FBZCP_CC_LOCALSELECT_OVERRIDE set!");
					/*
					if (!(texel.rgb.a & 0x80))
					// iterated RGB c_local.u = iterargb.u;
					else
					// color0 RGB c_local.u = reg[color0].u;
					*/
				}

				/* compute a_local */
				switch (FBZCP_CCA_LOCALSELECT(reg[fbzColorPath].u)) {
				default:
				case 0: /* iterated alpha */
					// c_local.rgb.a = iterargb.rgb.a;
					c_local.rgb.a = static_cast<uint8_t>(sa[pix]);
					break;
				case 1: /* color0 alpha */
					c_local.rgb.a = reg[color0].rgb.a;
					break;
				case 2: /* clamped iterated Z[27:20] */
				{
					int temp;
					CLAMPED_Z(iterz, reg[fbzColorPath].u, temp);
					c_local.rgb.a = (uint8_t)temp;
					break;
				}
				case 3: /* clamped iterated W[39:32] */
				{
					// Voodoo 2 only
					int temp;
					CLAMPED_W(iterw, reg[fbzColorPath].u, temp);
					c_local.rgb.a = (uint8_t)temp;
					break;
				}
				}

				/* select zero or c_other */
				if (FBZCP_CC_ZERO_OTHER(reg[fbzColorPath].u) == 0) {
					r = sr[pix];
					g = sg[pix];
					b = sb[pix];
				} else {
					r = g = b = 0;
				}

				/* select zero or a_other */
				if (FBZCP_CCA_ZERO_OTHER(reg[fbzColorPath].u) == 0) {
					a = sa[pix];
				} else {
					a = 0;
				}

				/* subtract c_local */
				if (FBZCP_CC_SUB_CLOCAL(reg[fbzColorPath].u)) {
					r -= c_local.rgb.r;
					g -= c_local.rgb.g;
					b -= c_local.rgb.b;
				}

				/* subtract a_local */
				if (FBZCP_CCA_SUB_CLOCAL(reg[fbzColorPath].u)) {
					a -= c_local.rgb.a;
				}

				/* blend RGB */
				switch (FBZCP_CC_MSELECT(reg[fbzColorPath].u)) {
				default: /* reserved */
				case 0:  /* 0 */
					blendr = blendg = blendb = 0;
					break;
				case 1: /* c_local */
					blendr = c_local.rgb.r;
					blendg = c_local.rgb.g;
					blendb = c_local.rgb.b;
					// LOG_DEBUG("VOODOO: blend RGB c_local");
					break;
				case 2: /* a_other */
					// blendr = blendg = blendb = c_other.rgb.a;
					LOG_DEBUG("VOODOO: blend RGB a_other");
					break;
				case 3: /* a_local */
					blendr = blendg = blendb = c_local.rgb.a;
					LOG_DEBUG("VOODOO: blend RGB a_local");
					break;
				case 4: /* texture alpha */
					// blendr = blendg = blendb = texel.rgb.a;
					LOG_DEBUG("VOODOO: blend RGB texture alpha");
					break;
				case 5: /* texture RGB (Voodoo 2 only) */
					// blendr = texel.rgb.r;
					// blendg = texel.rgb.g;
					// blendb = texel.rgb.b;
					LOG_DEBUG("VOODOO: blend RGB texture RGB");
					break;
				}

				/* blend alpha */
				switch (FBZCP_CCA_MSELECT(reg[fbzColorPath].u)) {
				default: /* reserved */
				case 0: /* 0 */ blenda = 0; break;
				case 1: /* a_local */
					blenda = c_local.rgb.a;
					// LOG_DEBUG("VOODOO: blend alpha a_local");
					break;
				case 2: /* a_other */
					// blenda = c_other.rgb.a;
					LOG_DEBUG("VOODOO: blend alpha a_other");
					break;
				case 3: /* a_local */
					blenda = c_local.rgb.a;
					LOG_DEBUG("VOODOO: blend alpha a_local");
					break;
				case 4: /* texture alpha */
					// blenda = texel.rgb.a;
					LOG_DEBUG("VOODOO: blend alpha texture alpha");
					break;
				}

				/* reverse the RGB blend */
				if (!FBZCP_CC_REVERSE_BLEND(reg[fbzColorPath].u)) {
					blendr ^= 0xff;
					blendg ^= 0xff;
					blendb ^= 0xff;
				}

				/* reverse the alpha blend */
				if (!FBZCP_CCA_REVERSE_BLEND(reg[fbzColorPath].u)) {
					blenda ^= 0xff;
				}

				/* do the blend */
				r = (r * (blendr + 1)) >> 8;
				g = (g * (blendg + 1)) >> 8;
				b = (b * (blendb + 1)) >> 8;
				a = (a * (blenda + 1)) >> 8;

				/* add clocal or alocal to RGB */
				switch (FBZCP_CC_ADD_ACLOCAL(reg[fbzColorPath].u)) {
				case 3: /* reserved */
				case 0: /* nothing */ break;
				case 1: /* add c_local */
					r += c_local.rgb.r;
					g += c_local.rgb.g;
					b += c_local.rgb.b;
					break;
				case 2: /* add_alocal */
					r += c_local.rgb.a;
					g += c_local.rgb.a;
					b += c_local.rgb.a;
					break;
				}

				/* add clocal or alocal to alpha */
				if (FBZCP_CCA_ADD_ACLOCAL(reg[fbzColorPath].u)) {
					a += c_local.rgb.a;
				}

				/* clamp */
				r = clamp_to_uint8(r);
				g = clamp_to_uint8(g);
				b = clamp_to_uint8(b);
				a = clamp_to_uint8(a);

				/* invert */
				if (FBZCP_CC_INVERT_OUTPUT(reg[fbzColorPath].u)) {
					r ^= 0xff;
					g ^= 0xff;
					b ^= 0xff;
				}
				if (FBZCP_CCA_INVERT_OUTPUT(reg[fbzColorPath].u)) {
					a ^= 0xff;
				}

#ifdef C_ENABLE_VOODOO_OPENGL
				if (ogl && active) {
					if (FBZMODE_RGB_BUFFER_MASK(reg[fbzMode].u)) {
						// APPLY_DITHER(FBZMODE, XX,
						// DITHER_LOOKUP, r, g, b);
						voodoo_ogl_draw_pixel_pipeline(
						        x, scry + 1, r, g, b);
					}
					/*
					if (depth && FBZMODE_AUX_BUFFER_MASK(reg[fbzMode].u)) {
						if (FBZMODE_ENABLE_ALPHA_PLANES(reg[fbzMode].u) == 0)
							voodoo_ogl_draw_z(x, y, depthval&0xffff, depthval>>16);
						//else
						//	depth[XX] = a;
					}
					*/
				} else
#endif
				{
					// Pixel pipeline part 2 handles color
					// combine, fog, alpha, and final output
					PIXEL_PIPELINE_MODIFY(dither,
					                      dither4,
					                      x,
					                      reg[fbzMode].u,
					                      reg[fbzColorPath].u,
					                      reg[alphaMode].u,
					                      reg[fogMode].u,
					                      iterz,
					                      iterw,
					                      reg[zaColor]);

					PIXEL_PIPELINE_FINISH(dither_lookup,
					                      x,
					                      dest,
					                      depth,
					                      reg[fbzMode].u);
				}

				PIXEL_PIPELINE_END(stats);
			}
		nextpixel:
			/* advance our pointers */
			x++;
			mask >>= 4;
		}
		sum_statistics(fbi.lfb_stats, stats);
	}
}

/*************************************
 *
 *  Voodoo texture RAM writes
 *
 *************************************/
int32_t voodoo_state::WriteToTexture(const uint32_t offset, uint32_t data)
{
	const auto tmu_num = static_cast<uint8_t>((offset >> 19) & 0b11);

	// LOG(LOG_VOODOO,LOG_WARN)("V3D:write TMU%x offset %X value %X",
	// tmunum, offset, data);

	/* point to the right TMU */
	if ((chipmask & (2 << tmu_num)) == 0) {
		return 0;
	}

	const auto t = &tmu[tmu_num];
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
	if (TEXMODE_FORMAT(t->reg[textureMode].u) < 8) {
		int lod;
		int tt;
		int ts;
		uint32_t tbaseaddr;
		uint8_t* dest;

		/* extract info */
		lod = (offset >> 15) & 0x0f;
		tt  = (offset >> 7) & 0xff;

		/* old code has a bit about how this is broken in gauntleg
		 * unless we always look at TMU0 */
		if (TEXMODE_SEQ_8_DOWNLD(tmu[0].reg /*t->reg*/[textureMode].u)) {
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
			dest[BYTE4_XOR_LE(tbaseaddr + 0)] = static_cast<uint8_t>(
			        (data >> 0) & 0xff);
			changed = true;
		}
		if (dest[BYTE4_XOR_LE(tbaseaddr + 1)] != ((data >> 8) & 0xff)) {
			dest[BYTE4_XOR_LE(tbaseaddr + 1)] = static_cast<uint8_t>(
			        (data >> 8) & 0xff);
			changed = true;
		}
		if (dest[BYTE4_XOR_LE(tbaseaddr + 2)] != ((data >> 16) & 0xff)) {
			dest[BYTE4_XOR_LE(tbaseaddr + 2)] = static_cast<uint8_t>(
			        (data >> 16) & 0xff);
			changed = true;
		}
		if (dest[BYTE4_XOR_LE(tbaseaddr + 3)] != ((data >> 24) & 0xff)) {
			dest[BYTE4_XOR_LE(tbaseaddr + 3)] = static_cast<uint8_t>(
			        (data >> 24) & 0xff);
			changed = true;
		}

#ifdef C_ENABLE_VOODOO_OPENGL
		if (changed && ogl && active) {
			voodoo_ogl_texture_clear(t->lodoffset[lod], tmunum);
			voodoo_ogl_texture_clear(t->lodoffset[t->lodmin], tmunum);
		}
#endif
	}

	/* 16-bit texture case */
	else {
		int lod;
		int tt;
		int ts;
		uint32_t tbaseaddr;
		uint16_t* dest;

/* extract info */
#ifdef C_ENABLE_VOODOO_OPENGL
		tmunum = (offset >> 19) & 0x03;
#endif
		lod = (offset >> 15) & 0x0f;
		tt  = (offset >> 7) & 0xff;
		ts  = (offset << 1) & 0xfe;

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
		dest = (uint16_t*)t->ram;
		tbaseaddr &= t->mask;
		tbaseaddr >>= 1;

		[[maybe_unused]] bool changed = false;
		if (dest[BYTE_XOR_LE(tbaseaddr + 0)] != ((data >> 0) & 0xffff)) {
			dest[BYTE_XOR_LE(tbaseaddr + 0)] = static_cast<uint16_t>(
			        (data >> 0) & 0xffff);
			changed = true;
		}
		if (dest[BYTE_XOR_LE(tbaseaddr + 1)] != ((data >> 16) & 0xffff)) {
			dest[BYTE_XOR_LE(tbaseaddr + 1)] = static_cast<uint16_t>(
			        (data >> 16) & 0xffff);
			changed = true;
		}

#ifdef C_ENABLE_VOODOO_OPENGL
		if (changed && ogl && active) {
			voodoo_ogl_texture_clear(t->lodoffset[lod], tmunum);
			voodoo_ogl_texture_clear(t->lodoffset[t->lodmin], tmunum);
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
uint32_t voodoo_state::ReadFromRegister(const uint32_t offset)
{
	const auto regnum = static_cast<uint8_t>((offset) & 0xff);

	// LOG(LOG_VOODOO,LOG_WARN)("Voodoo:read chip %x reg %x (%s)", chips,
	// regnum<<2, voodoo_reg_name[regnum]);

	/* first make sure this register is readable */
	if ((regaccess[regnum] & REGISTER_READ) == 0) {
		return 0xffffffff;
	}

	/* default result is the FBI register value */
	auto result = reg[regnum].u;

	/* some registers are dynamic; compute them */
	switch (regnum) {
	case status:

		/* start with a blank slate */
		result = 0;

		/* bits 5:0 are the PCI FIFO free space */
		result |= 0x3f << 0;

		/* bit 6 is the vertical retrace */
		// result |= fbi.vblank << 6;
		result |= (GetRetrace() ? 0x40 : 0);

		if (pci.op_pending) {
			// bit 7 is FBI graphics engine busy
			// bit 8 is TREX busy
			// bit 9 is overall busy
			using namespace bit::literals;
			result |= (b7 | b8 | b9);
		}

		/* bits 11:10 specifies which buffer is visible */
		result |= fbi.frontbuf << 10;

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

		result |= ((uint32_t)(GetVRetracePosition() * 0x1fff)) & 0x1fff;
		result |= (((uint32_t)(GetHRetracePosition() * 0x7ff)) & 0x7ff)
		       << 16;

		break;

	/* bit 2 of the initEnable register maps this to dacRead */
	case fbiInit2:
		if (INITEN_REMAP_INIT_TO_DAC(pci.init_enable)) {
			result = dac.read_result;
		}
		break;

	/*
	case fbiInit3:
	if (INITEN_REMAP_INIT_TO_DAC(pci.init_enable))
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
		UpdateStatistics(StatsCollection::Accumulate);
		[[fallthrough]];
	case fbiTrianglesOut: result = reg[regnum].u & 0xffffff; break;
	}

	return result;
}

/*************************************
 *
 *  Handle an LFB read
 *
 *************************************/
uint32_t voodoo_state::ReadFromFrameBuffer(const uint32_t offset)
{
	// LOG(LOG_VOODOO,LOG_WARN)("Voodoo:read LFB offset %X", offset);
	uint16_t* buffer = {};
	uint32_t bufmax  = 0;
	uint32_t data    = 0;

	/* compute X,Y */
	const auto x = (offset << 1) & 0x3fe;
	const auto y = (offset >> 9) & 0x3ff;

	/* select the target buffer */
	const auto destbuf = LFBMODE_READ_BUFFER_SELECT(reg[lfbMode].u);
	switch (destbuf) {
	case 0: /* front buffer */
		buffer = (uint16_t*)(fbi.ram + fbi.rgboffs[fbi.frontbuf]);
		bufmax = (fbi.mask + 1 - fbi.rgboffs[fbi.frontbuf]) / 2;
		break;

	case 1: /* back buffer */
		buffer = (uint16_t*)(fbi.ram + fbi.rgboffs[fbi.backbuf]);
		bufmax = (fbi.mask + 1 - fbi.rgboffs[fbi.backbuf]) / 2;
		break;

	case 2: /* aux buffer */
		if (fbi.auxoffs == (uint32_t)(~0)) {
			return 0xffffffff;
		}
		buffer = (uint16_t*)(fbi.ram + fbi.auxoffs);
		bufmax = (fbi.mask + 1 - fbi.auxoffs) / 2;
		break;

	default: /* reserved */ return 0xffffffff;
	}

	/* determine the screen Y */
	auto scry = y;
	if (LFBMODE_Y_ORIGIN(reg[lfbMode].u)) {
		scry = (fbi.yorigin - y) & 0x3ff;
	}

#ifdef C_ENABLE_VOODOO_OPENGL
	if (ogl && active) {
		data = voodoo_ogl_read_pixel(x, scry + 1);
	} else
#endif
	{
		/* advance pointers to the proper row */
		const auto bufoffs = scry * fbi.rowpixels + x;
		if (bufoffs >= bufmax) {
			return 0xffffffff;
		}

		/* compute the data */
		data = buffer[bufoffs + 0] | (buffer[bufoffs + 1] << 16);
	}

	/* word swapping */
	if (LFBMODE_WORD_SWAP_READS(reg[lfbMode].u)) {
		data = (data << 16) | (data >> 16);
	}

	/* byte swizzling */
	if (LFBMODE_BYTE_SWIZZLE_READS(reg[lfbMode].u)) {
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

void voodoo_state::WriteToAddress(const uint32_t addr, const uint32_t data,
                                  const uint32_t mask)
{
	const auto offset = (addr >> 2) & offset_mask;

	if ((offset & offset_base) == 0) {
		WriteToRegister(offset, data);
	} else if ((offset & lfb_base) == 0) {
		WriteToFrameBuffer(offset, data, mask);
	} else {
		WriteToTexture(offset, data);
	}
}

uint32_t voodoo_state::ReadFromAddress(const uint32_t addr)
{
	const auto offset = (addr >> 2) & offset_mask;

	if ((offset & offset_base) == 0) {
		return ReadFromRegister(offset);
	}
	if ((offset & lfb_base) == 0) {
		return ReadFromFrameBuffer(offset);
	}
	return 0xffffffff;
}

/***************************************************************************
DEVICE INTERFACE
***************************************************************************/

/*-------------------------------------------------
device start callback
-------------------------------------------------*/
void voodoo_state::Initialize()
{
#ifdef C_ENABLE_VOODOO_OPENGL
	ogl = (emulation_type == VOODOO_EMU_TYPE_ACCELERATED);
#endif

	active = false;

	memset(reg, 0, sizeof(reg));

	fbi.vblank_flush_pending = false;
	pci.op_pending           = false;

	dac.read_result = 0;

	output_on     = false;
	clock_enabled = false;
#ifdef C_ENABLE_VOODOO_OPENGL
	ogl_dimchange = true;
#endif
	send_config = false;

	memset(dac.reg, 0, sizeof(dac.reg));

#ifdef C_ENABLE_VOODOO_OPENGL
	next_rasterizer = 0;
	// for (uint32_t rct=0; rct<MAX_RASTERIZERS; rct++)
	//	rasterizer[rct] = raster_info();
	memset(rasterizer, 0, sizeof(rasterizer));
	memset(raster_hash, 0, sizeof(raster_hash));
#endif

	UpdateStatistics(StatsCollection::Reset);

	alt_regmap = false;
#ifdef C_ENABLE_VOODOO_DEBUG
	regnames = voodoo_reg_name;
#endif

	if (*voodoo_reciplog == 0u) {
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

#if defined(__SSE2__)
		/* create sse2 scale table for rgba_bilinear_filter */
		for (int16_t i = 0; i != 256; i++) {
			sse2_scale_table[i][0] = sse2_scale_table[i][2] = sse2_scale_table[i][4] = sse2_scale_table[i][6] = i;
			sse2_scale_table[i][1] = sse2_scale_table[i][3] = sse2_scale_table[i][5] = sse2_scale_table[i][7] = 256 - i;
		}
#endif
	}

	tmu_config = 0x11; // revision 1

	uint32_t fbmemsize = 0;
	uint32_t tmumem0   = 0;
	uint32_t tmumem1   = 0;

	/* configure type-specific values */
	switch (vtype) {
	case VOODOO_1:
		regaccess = voodoo_register_access;
		fbmemsize = 2;
		tmumem0   = 2;
		break;

	case VOODOO_1_DTMU:
		regaccess = voodoo_register_access;
		fbmemsize = 4;
		tmumem0   = 4;
		tmumem1   = 4;
		break;

	/*
	As is now this crashes in Windows 9x trying to run a
	game with Voodoo 2 drivers installed (raster_generic
	tries to write into a frame buffer at an invalid memory
	location)
	
	case VOODOO_2:
		regaccess = voodoo2_register_access;
		fbmemsize = 4;
		tmumem0 = 4;
		tmumem1 = 4;
		tmu_config |= 0x800;
		break;
	*/

	default: break;
	}
	// Sanity-check sizes
	assert(fbmemsize > 0);
	assert(tmumem0 > 0);

	if (tmumem1 != 0) {
		tmu_config |= 0xc0; // two TMUs
	}

	chipmask = 0x01;

	/* set up the PCI FIFO */
	pci.fifo.size = 64 * 2;

	/* set up frame buffer */
	init_fbi(&fbi, fbmemsize << 20);

	fbi.rowpixels = fbi.width;

	tmu[0].ncc[0].palette  = nullptr;
	tmu[0].ncc[1].palette  = nullptr;
	tmu[1].ncc[0].palette  = nullptr;
	tmu[1].ncc[1].palette  = nullptr;
	tmu[0].ncc[0].palettea = nullptr;
	tmu[0].ncc[1].palettea = nullptr;
	tmu[1].ncc[0].palettea = nullptr;
	tmu[1].ncc[1].palettea = nullptr;

	tmu[0].ram    = nullptr;
	tmu[1].ram    = nullptr;
	tmu[0].lookup = nullptr;
	tmu[1].lookup = nullptr;

	/* build shared TMU tables */
	tmushare.Initialize();

	/* set up the TMUs */
	tmu[0].Initialize(tmushare, &reg[0x100], tmumem0 << 20);
	chipmask |= 0x02;
	if (tmumem1 != 0) {
		tmu[1].Initialize(tmushare, &reg[0x200], tmumem1 << 20);
		chipmask |= 0x04;
		tmu_config |= 0x40;
	}

	/* initialize some registers */
	pci.init_enable = 0;
	reg[fbiInit0].u = (uint32_t)((1 << 4) | (0x10 << 6));
	reg[fbiInit1].u = (uint32_t)((1 << 1) | (1 << 8) | (1 << 12) | (2 << 20));
	reg[fbiInit2].u = (uint32_t)((1 << 6) | (0x100 << 23));
	reg[fbiInit3].u = (uint32_t)((2 << 13) | (0xf << 17));
	reg[fbiInit4].u = (uint32_t)(1 << 0);

	/* do a soft reset to reset everything else */
	SoftReset();

	RecomputeVideoMemory();
}

void voodoo_state::VblankFlush()
{
#ifdef C_ENABLE_VOODOO_OPENGL
	if (ogl) {
		voodoo_ogl_vblank_flush();
	}
#endif
	fbi.vblank_flush_pending = false;
}

#ifdef C_ENABLE_VOODOO_OPENGL
void voodoo_state::SetWindow()
{
	if (ogl && active) {
		voodoo_ogl_set_window(this);
	}
}
#endif

void voodoo_state::Leave()
{
#ifdef C_ENABLE_VOODOO_OPENGL
	if (ogl) {
		voodoo_ogl_leave(true);
	}
#endif
	active = false;
}

void voodoo_state::Activate()
{
	active = true;

#ifdef C_ENABLE_VOODOO_OPENGL
	if (ogl) {
		if (voodoo_ogl_init(this)) {
			voodoo_ogl_clear();
		} else {
			ogl = false;
			LOG_MSG("VOODOO: OpenGL-based acceleration disabled");
		}
	}
#endif
}

#ifdef C_ENABLE_VOODOO_OPENGL
void voodoo_state::UpdateDimensions()
{
	ogl_dimchange = false;

	if (ogl) {
		voodoo_ogl_update_dimensions();
	}
}
#endif

voodoo_state* voodoo = nullptr;

static void call_vertical_timer(uint32_t /*val*/)
{
	assert(voodoo);
	voodoo->VerticalTimer();
}

void voodoo_state::VerticalTimer()
{
	draw.frame_start = PIC_FullIndex();
	PIC_AddEvent(call_vertical_timer, draw.vfreq);

	if (fbi.vblank_flush_pending) {
		VblankFlush();
#ifdef C_ENABLE_VOODOO_OPENGL
		if (GFX_LazyFullscreenRequested()) {
			ogl_dimchange = true;
		}
#endif
	}

#ifdef C_ENABLE_VOODOO_OPENGL
	if (!ogl)
#endif
	{
		if (!RENDER_StartUpdate()) {
			return; // frameskip
		}

#ifdef C_ENABLE_VOODOO_DEBUG
		rectangle r;
		r.min_x = r.min_y = 0;
		r.max_x = (int)fbi.width;
		r.max_y = (int)fbi.height;
#endif

		// draw all lines at once
		auto* viewbuf = (uint16_t*)(fbi.ram + fbi.rgboffs[fbi.frontbuf]);
		for (Bitu i = 0; i < fbi.height; i++) {
			RENDER_DrawLine((uint8_t*)viewbuf);
			viewbuf += fbi.rowpixels;
		}
		RENDER_EndUpdate(false);
	}
#ifdef C_ENABLE_VOODOO_OPENGL
	else {
		// ???
		SetWindow();
	}
#endif
}

bool voodoo_state::GetRetrace()
{
	// TODO proper implementation
	const auto time_in_frame = PIC_FullIndex() - draw.frame_start;
	const auto vfreq = draw.vfreq;
	if (vfreq <= 0) {
		return false;
	}
	if (clock_enabled && output_on) {
		if ((time_in_frame / vfreq) > 0.95) {
			return true;
		}
	} else if (output_on) {
		auto rtime = time_in_frame / vfreq;
		rtime = fmod(rtime, 1.0);
		if (rtime > 0.95) {
			return true;
		}
	}
	return false;
}

double voodoo_state::GetVRetracePosition()
{
	// TODO proper implementation
	const auto time_in_frame = PIC_FullIndex() - draw.frame_start;
	const auto vfreq = draw.vfreq;
	if (vfreq <= 0) {
		return 0.0;
	}
	if (clock_enabled && output_on) {
		return time_in_frame / vfreq;
	}
	if (output_on) {
		auto rtime = time_in_frame / vfreq;
		rtime = fmod(rtime, 1.0);
		return rtime;
	}
	return 0.0;
}

double voodoo_state::GetHRetracePosition()
{
	// TODO proper implementation
	const auto time_in_frame = PIC_FullIndex() - draw.frame_start;

	const auto hfreq = draw.vfreq * 100;

	if (hfreq <= 0) {
		return 0.0;
	}
	if (clock_enabled && output_on) {
		return time_in_frame / hfreq;
	}
	if (output_on) {
		auto rtime = time_in_frame / hfreq;
		rtime      = fmod(rtime, 1.0);
		return rtime;
	}
	return 0.0;
}

void voodoo_state::UpdateScreen()
{
	// abort drawing
	RENDER_EndUpdate(true);

	if ((!clock_enabled || !output_on) && draw.override_on) {
		// switching off
		PIC_RemoveEvents(call_vertical_timer);
		Leave();

		VGA_SetOverride(false);
		draw.override_on = false;
	}

	if ((clock_enabled && output_on) && !draw.override_on) {
		// switching on
		PIC_RemoveEvents(call_vertical_timer); // shouldn't be needed

		// TODO proper implementation of refresh rates and timings
		draw.vfreq = 1000.0 / 60.0;
		VGA_SetOverride(true);
		draw.override_on = true;

		Activate();

#ifdef C_ENABLE_VOODOO_OPENGL
		if (ogl) {
			ogl_dimchange = false;
		} else
#endif
		{
			constexpr auto is_double_width  = false;
			constexpr auto is_double_height = false;

			constexpr Fraction render_pixel_aspect_ratio = {1};

			VideoMode video_mode        = {};
			video_mode.bios_mode_number = 0;
			video_mode.width  = check_cast<uint16_t>(fbi.width);
			video_mode.height = check_cast<uint16_t>(fbi.height);
			video_mode.pixel_aspect_ratio = render_pixel_aspect_ratio;
			video_mode.graphics_standard = GraphicsStandard::Svga;
			video_mode.color_depth    = ColorDepth::HighColor16Bit;
			video_mode.is_custom_mode = false;
			video_mode.is_graphics_mode = true;

			const auto frames_per_second = 1000.0 / draw.vfreq;

			RENDER_SetSize(video_mode.width,
			               video_mode.height,
			               is_double_width,
			               is_double_height,
			               render_pixel_aspect_ratio,
			               PixelFormat::RGB565_Packed16,
			               frames_per_second,
			               video_mode);
		}

		VerticalTimer();
	}

#ifdef C_ENABLE_VOODOO_OPENGL
	if ((clock_enabled && output_on) && ogl_dimchange) {
		UpdateDimensions();
	}
#endif

	draw.screen_update_requested = false;
}

static void call_check_screen_update(uint32_t /*val*/)
{
	assert(voodoo);
	voodoo->CheckScreenUpdate();
}

void voodoo_state::CheckScreenUpdate()
{
	draw.screen_update_pending = false;
	if (draw.screen_update_requested) {
		draw.screen_update_pending = true;
		UpdateScreen();
		PIC_AddEvent(call_check_screen_update, 100.0);
	}
}

void voodoo_state::UpdateScreenStart()
{
	draw.screen_update_requested = true;
	if (!draw.screen_update_pending) {
		draw.screen_update_pending = true;
		PIC_AddEvent(call_check_screen_update, 0.0);
	}
}

VoodooPageHandler::VoodooPageHandler(voodoo_state* state) : vs(state)
{
	assert(vs);
	flags = PFLAG_NOCODE;
}

uint8_t VoodooPageHandler::readb([[maybe_unused]] PhysPt addr)
{
	// LOG_DEBUG("VOODOO: readb at %x", addr);
	return 0xff;
}

void VoodooPageHandler::writeb([[maybe_unused]] PhysPt addr,
                               [[maybe_unused]] uint8_t val)
{
	// LOG_DEBUG("VOODOO: writeb at %x", addr);
}

uint16_t VoodooPageHandler::readw(PhysPt addr)
{
	addr           = PAGING_GetPhysicalAddress(addr);
	const auto val = vs->ReadFromAddress(addr);

	// Is the address word-aligned?
	if ((addr & 0b11) == 0) {
		return static_cast<uint16_t>(val & 0xffff);
	}
	// The address must be byte-aligned
	assert((addr & 0b1) == 0);
	return static_cast<uint16_t>(val >> 16);
}

void VoodooPageHandler::writew(PhysPt addr, uint16_t val)
{
	addr = PAGING_GetPhysicalAddress(addr);

	// Is the address word-aligned?
	if ((addr & 0b11) == 0) {
		vs->WriteToAddress(addr, val, 0x0000ffff);
	}
	// The address must be byte-aligned
	assert((addr & 0b1) == 0);
	vs->WriteToAddress(addr, static_cast<uint32_t>(val << 16), 0xffff0000);
}

uint32_t VoodooPageHandler::readd(PhysPt addr)
{
	addr = PAGING_GetPhysicalAddress(addr);

	// Is the address word-aligned?
	if ((addr & 0b11) == 0) {
		return vs->ReadFromAddress(addr);
	}
	// The address must be byte-aligned
	assert((addr & 0b1) == 0);
	const auto low  = vs->ReadFromAddress(addr);
	const auto high = vs->ReadFromAddress(next_addr(addr));
	return check_cast<uint32_t>((low >> 16) | (high << 16));
}

void VoodooPageHandler::writed(PhysPt addr, uint32_t val)
{
	addr = PAGING_GetPhysicalAddress(addr);
	if ((addr & 3) == 0u) {
		vs->WriteToAddress(addr, val, 0xffffffff);
	} else if ((addr & 1) == 0u) {
		vs->WriteToAddress(addr, val << 16, 0xffff0000);
		vs->WriteToAddress(next_addr(addr), val, 0x0000ffff);
	} else {
		auto val1 = vs->ReadFromAddress(addr);
		auto val2 = vs->ReadFromAddress(next_addr(addr));
		if ((addr & 3) == 1) {
			val1 = (val1 & 0xffffff) | ((val & 0xff) << 24);
			val2 = (val2 & 0xff000000) | (val >> 8);
		} else if ((addr & 3) == 3) {
			val1 = (val1 & 0xff) | ((val & 0xffffff) << 8);
			val2 = (val2 & 0xffffff00) | (val >> 24);
		}
		vs->WriteToAddress(addr, val1, 0xffffffff);
		vs->WriteToAddress(next_addr(addr), val2, 0xffffffff);
	}
}

VoodooInitPageHandler::VoodooInitPageHandler(voodoo_state* state) : vs(state)
{
	assert(vs);
	flags = PFLAG_NOCODE;
}

uint8_t VoodooInitPageHandler::readb([[maybe_unused]] PhysPt addr)
{
	return 0xff;
}

uint16_t VoodooInitPageHandler::readw(PhysPt addr)
{
	return vs->StartHandler()->readw(addr);
}

uint32_t VoodooInitPageHandler::VoodooInitPageHandler::readd(PhysPt addr)
{
	return vs->StartHandler()->readd(addr);
}

void VoodooInitPageHandler::writeb([[maybe_unused]] PhysPt addr,
                                   [[maybe_unused]] uint8_t val)
{}

void VoodooInitPageHandler::writew(PhysPt addr, uint16_t val)
{
	vs->StartHandler()->writew(addr, val);
}

void VoodooInitPageHandler::writed(PhysPt addr, uint32_t val)
{
	vs->StartHandler()->writed(addr, val);
}

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

struct PCI_SSTDevice : public PCI_Device {
	enum : uint16_t {
		vendor          = 0x121a,
		device_voodoo_1 = 0x0001,
		device_voodoo_2 = 0x0002
	}; // 0x121a = 3dfx

	uint16_t oscillator_ctr = 0;
	uint16_t pci_ctr        = 0;

	PCI_SSTDevice() : PCI_Device(vendor, device_voodoo_1) {}

	void SetDeviceId(uint16_t _device_id)
	{
		device_id = _device_id;
	}

	Bits ParseReadRegister(uint8_t regnum) override
	{
		// LOG_DEBUG("VOODOO: SST ParseReadRegister %x",regnum);
		switch (regnum) {
		case 0x4c:
		case 0x4d:
		case 0x4e:
		case 0x4f:
			LOG_DEBUG("VOODOO: SST ParseReadRegister STATUS %x", regnum);
			break;
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57:
			if (vtype == VOODOO_2) {
				return -1;
			}
			break;
		}
		return regnum;
	}

	bool OverrideReadRegister(uint8_t regnum, uint8_t* rval,
	                          uint8_t* rval_mask) override
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
		// LOG_DEBUG("VOODOO: SST ParseWriteRegister %x:=%x",regnum,value);
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
			voodoo->StartHandler();
			voodoo->pci.init_enable = (uint32_t)(value & 7);
			break;
		case 0x41:
		case 0x42:
		case 0x43: return -1;
		case 0xc0:
			voodoo->StartHandler();
			voodoo->clock_enabled = true;
			voodoo->UpdateScreenStart();
			return -1;
		case 0xe0:
			voodoo->StartHandler();
			voodoo->clock_enabled = false;
			voodoo->UpdateScreenStart();
			return -1;
		default: break;
		}
		return value;
	}

	bool InitializeRegisters(uint8_t registers[256]) override
	{
		// init (3dfx voodoo)
		registers[0x08] = 0x02; // revision
		registers[0x09] = 0x00; // interface
		// registers[0x0a] = 0x00;	// subclass code
		registers[0x0a] = 0x00; // subclass code (video/graphics controller)
		// registers[0x0b] = 0x00;	// class code (generic)
		registers[0x0b] = 0x04; // class code (multimedia device)
		registers[0x0e] = 0x00; // header type (other)

		// reset
		registers[0x04] = 0x02; // command register (memory space enabled)
		registers[0x05] = 0x00;
		registers[0x06] = 0x80; // status register (fast back-to-back)
		registers[0x07] = 0x00;

		registers[0x3c] = 0xff; // no irq

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
			registers[0x41] = 0x40; // voodoo2 revision ID (rev4)
			registers[0x42] = 0x01;
			registers[0x43] = 0x00;
		}

		return true;
	}
};

voodoo_state::~voodoo_state()
{
	LOG_MSG("VOODOO: Shutting down");

#ifdef C_ENABLE_VOODOO_OPENGL
	if (ogl) {
		voodoo_ogl_shutdown(this);
	}
#endif

	active = false;
	tworker.Shutdown();

	PCI_RemoveDevice(PCI_SSTDevice::vendor, PCI_SSTDevice::device_voodoo_1);
}

PageHandler* voodoo_state::StartHandler()
{
	// This function is called delayed after booting only once a game
	// actually requests Voodoo support
	if (is_handler_started) {
		return page_handler.get();
	}

	Initialize();

	draw       = {};
	draw.vfreq = 1000.0 / 60.0;

	tworker.use_threads = (vperf == PerformanceFlags::MultiThreading ||
	                       vperf == PerformanceFlags::All);

	tworker.disable_bilinear_filter = (vperf == PerformanceFlags::NoBilinearFiltering ||
	                                   vperf == PerformanceFlags::All);

	// Switch the pagehandler now that has been allocated and is in use
	page_handler       = std::make_unique<VoodooPageHandler>(this);
	is_handler_started = true;

	PAGING_InitTLB();

	// Log the startup
	const auto ram_size_mb = (vtype == VOODOO_1_DTMU ? 12 : 4);

	const auto performance_msg = describe_performance_flags(vperf);

	LOG_MSG("VOODOO: Initialized with %d MB of RAM%s", ram_size_mb, performance_msg);

	return page_handler.get();
}

PageHandler* VOODOO_PCI_GetLFBPageHandler(const uintptr_t page)
{
	assert(voodoo);
	return (page >= (voodoo_current_lfb >> 12) &&
	                        page < (voodoo_current_lfb >> 12) + VOODOO_PAGES
	                ? voodoo->page_handler.get()
	                : nullptr);
}

void VOODOO_Configure(const ModuleLifecycle lifecycle, Section* section)
{
	static std::unique_ptr<voodoo_state> voodoo_instance = {};

	switch (lifecycle) {
	case ModuleLifecycle::Create: {
		auto* sec = dynamic_cast<Section_prop*>(section);

		// Only activate on SVGA machines
		if (machine != MCH_VGA || svgaCard == SVGA_None || !sec) {
			return;
		}

		switch (sec->Get_string("voodoo_memsize")[0]) {
		case '1': vtype = VOODOO_1_DTMU; break; // 12 MB
		case '4': vtype = VOODOO_1; break;      // 4 MB
		default: return;                        // disabled
		}

		// Check 64 KB alignment of LFB base
		static_assert((PciVoodooLfbBase & 0xffff) == 0);

		voodoo_current_lfb = PciVoodooLfbBase;
		vperf = static_cast<PerformanceFlags>(sec->Get_int("voodoo_perf"));

		voodoo_instance = std::make_unique<voodoo_state>();
		voodoo          = voodoo_instance.get();

		PCI_AddDevice(new PCI_SSTDevice());
	}

	// This module doesn't support reconfiguration at runtime
	case ModuleLifecycle::Reconfigure: break;

	case ModuleLifecycle::Destroy:
		voodoo_instance.reset();
		voodoo = nullptr;
		break;
	}
}
