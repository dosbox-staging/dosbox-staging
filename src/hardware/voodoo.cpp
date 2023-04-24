/*
 *  SPDX-License-Identifier: BSD-3-Clause AND GPL-2.0-or-later
 *
 *  copyright-holders: Aaron Giles, kekko, Bernhard Schelling
 */

/*
 *  Copyright (C) 2023       The DOSBox Staging Team
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
*/

#include "dosbox.h"

#if C_VOODOO

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "control.h"
#include "cross.h"
#include "mem.h"
#include "paging.h"
#include "pci_bus.h"
#include "pic.h"
#include "render.h"
#include "setup.h"
#include "support.h"
#include "vga.h"

#ifndef DOSBOX_VOODOO_TYPES_H
#define DOSBOX_VOODOO_TYPES_H

/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

// FIXME Get rid of these typedefs
//
/* 8-bit values */
typedef uint8_t UINT8;
typedef int8_t INT8;

/* 16-bit values */
typedef uint16_t UINT16;
typedef int16_t INT16;

#ifndef _WINDOWS_
/* 32-bit values */
typedef uint32_t UINT32;
typedef int32_t INT32;

/* 64-bit values */
typedef uint64_t UINT64;
typedef int64_t INT64;
#endif

typedef INT64 attoseconds_t;

#define ATTOSECONDS_PER_SECOND_SQRT		((attoseconds_t)1000000000)
#define ATTOSECONDS_PER_SECOND			(ATTOSECONDS_PER_SECOND_SQRT * ATTOSECONDS_PER_SECOND_SQRT)

/* convert between hertz (as a double) and attoseconds */
#define ATTOSECONDS_TO_HZ(x)			((double)ATTOSECONDS_PER_SECOND / (double)(x))
#define HZ_TO_ATTOSECONDS(x)			((attoseconds_t)(ATTOSECONDS_PER_SECOND / (x)))

#define MAX_VERTEX_PARAMS					6

/* poly_extent describes start/end points for a scanline, along with per-scanline parameters */
struct poly_extent
{
	INT32		startx;						/* starting X coordinate (inclusive) */
	INT32		stopx;						/* ending X coordinate (exclusive) */
};

/* an rgb_t is a single combined R,G,B (and optionally alpha) value */
typedef UINT32 rgb_t;

/* an rgb15_t is a single combined 15-bit R,G,B value */
typedef UINT16 rgb15_t;

/* macros to assemble rgb_t values */
#define MAKE_ARGB(a,r,g,b)	((((rgb_t)(a) & 0xff) << 24) | (((rgb_t)(r) & 0xff) << 16) | (((rgb_t)(g) & 0xff) << 8) | ((rgb_t)(b) & 0xff))
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
inline UINT8 pal5bit(UINT8 bits)
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

/* Standard MIN/MAX macros */
#ifndef MIN
#define MIN(x,y)			((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x,y)			((x) > (y) ? (x) : (y))
#endif

/* Macros for normalizing data into big or little endian formats */
#define FLIPENDIAN_INT16(x)	(((((UINT16) (x)) >> 8) | ((x) << 8)) & 0xffff)
#define FLIPENDIAN_INT32(x)	((((UINT32) (x)) << 24) | (((UINT32) (x)) >> 24) | \
	(( ((UINT32) (x)) & 0x0000ff00) << 8) | (( ((UINT32) (x)) & 0x00ff0000) >> 8))
#define FLIPENDIAN_INT64(x)	\
	(												\
		(((((UINT64) (x)) >> 56) & ((UINT64) 0xFF)) <<  0)	|	\
		(((((UINT64) (x)) >> 48) & ((UINT64) 0xFF)) <<  8)	|	\
		(((((UINT64) (x)) >> 40) & ((UINT64) 0xFF)) << 16)	|	\
		(((((UINT64) (x)) >> 32) & ((UINT64) 0xFF)) << 24)	|	\
		(((((UINT64) (x)) >> 24) & ((UINT64) 0xFF)) << 32)	|	\
		(((((UINT64) (x)) >> 16) & ((UINT64) 0xFF)) << 40)	|	\
		(((((UINT64) (x)) >>  8) & ((UINT64) 0xFF)) << 48)	|	\
		(((((UINT64) (x)) >>  0) & ((UINT64) 0xFF)) << 56)		\
	)

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

inline INT32 mul_32x32_shift(INT32 a, INT32 b, INT8 shift)
{
	return (INT32)(((INT64)a * (INT64)b) >> shift);
}

#if !defined(__SSE2__) && (_M_IX86_FP == 2 || (defined(_M_AMD64) || defined(_M_X64)))
#define __SSE2__ 1
#endif
#if defined(__SSE2__) && __SSE2__
#include <emmintrin.h>
static INT16 sse2_scale_table[256][8];
#endif

inline rgb_t rgba_bilinear_filter(rgb_t rgb00, rgb_t rgb01, rgb_t rgb10,
                                  rgb_t rgb11, UINT8 u, UINT8 v)
{
#if defined(__SSE2__) && __SSE2__
	__m128i  scale_u = *(__m128i *)sse2_scale_table[u], scale_v = *(__m128i *)sse2_scale_table[v];
	return _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packs_epi32(_mm_srli_epi32(_mm_madd_epi16(_mm_max_epi16(
		_mm_slli_epi32(_mm_madd_epi16(_mm_unpacklo_epi8(_mm_unpacklo_epi8(_mm_cvtsi32_si128(rgb01), _mm_cvtsi32_si128(rgb00)), _mm_setzero_si128()), scale_u), 15),
		_mm_srli_epi32(_mm_madd_epi16(_mm_unpacklo_epi8(_mm_unpacklo_epi8(_mm_cvtsi32_si128(rgb11), _mm_cvtsi32_si128(rgb10)), _mm_setzero_si128()), scale_u), 1)),
		scale_v), 15), _mm_setzero_si128()), _mm_setzero_si128()));
#else
	UINT32 ag0, ag1, rb0, rb1;
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
static UINT32 voodoo_reciplog[(2 << RECIPLOG_LOOKUP_BITS) + 2];

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

static const UINT8 dither_matrix_4x4[16] =
{
	 0,  8,  2, 10,
	12,  4, 14,  6,
	 3, 11,  1,  9,
	15,  7, 13,  5
};

static const UINT8 dither_matrix_2x2[16] =
{
	 2, 10,  2, 10,
	14,  6, 14,  6,
	 2, 10,  2, 10,
	14,  6, 14,  6
};


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
	(a) = ((INT16)(val) >> 15) & 0xff;						\
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
 *  Misc. macros
 *
 *************************************/

/* macro for clamping a value between minimum and maximum values */
#define CLAMP(val,min,max)		do { if ((val) < (min)) { (val) = (min); } else if ((val) > (max)) { (val) = (max); } } while (0)

/* macro to compute the base 2 log for LOD calculations */
#define LOGB2(x)				(log((double)(x)) / log(2.0))


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

typedef UINT32 rgb_t;

struct rgba
{
#ifndef WORDS_BIGENDIAN
	UINT8				b, g, r, a;
#else
	UINT8				a, r, g, b;
#endif
};

union voodoo_reg
{
	INT32				i;
	UINT32				u;
	float				f;
	rgba				rgb;
};

typedef voodoo_reg rgb_union;

/* note that this structure is an even 64 bytes long */
struct stats_block {
	int32_t pixels_in;          // pixels in statistic
	int32_t pixels_out;         // pixels out statistic
	int32_t chroma_fail;        // chroma test fail statistic
	int32_t zfunc_fail;         // z function test fail statistic
	int32_t afunc_fail;         // alpha function test fail statistic
	// int32_t clip_fail;       // clipping fail statistic
	// int32_t stipple_count;   // stipple statistic
	int32_t filler[64 / 4 - 5]; // pad this structure to 64 bytes
};
static_assert(sizeof(stats_block) == 64);

struct fifo_state
{
	INT32				size;					/* size of the FIFO */
};

struct pci_state
{
	fifo_state			fifo;					/* PCI FIFO */
	UINT32				init_enable;			/* initEnable value */
	bool				op_pending;				/* true if an operation is pending */
};

struct ncc_table
{
	bool				dirty;					/* is the texel lookup dirty? */
	voodoo_reg *		reg;					/* pointer to our registers */
	INT32				ir[4], ig[4], ib[4];	/* I values for R,G,B */
	INT32				qr[4], qg[4], qb[4];	/* Q values for R,G,B */
	INT32				y[16];					/* Y values */
	rgb_t *				palette;				/* pointer to associated RGB palette */
	rgb_t *				palettea;				/* pointer to associated ARGB palette */
	rgb_t				texel[256];				/* texel lookup */
};

struct tmu_state
{
	UINT8 *				ram;					/* pointer to our RAM */
	UINT32				mask;					/* mask to apply to pointers */
	voodoo_reg *		reg;					/* pointer to our register base */
	bool				regdirty;				/* true if the LOD/mode/base registers have changed */

	enum { texaddr_mask = 0x0fffff, texaddr_shift = 3 };
	//UINT32			texaddr_mask;			/* mask for texture address */
	//UINT8				texaddr_shift;			/* shift for texture address */

	INT64				starts, startt;			/* starting S,T (14.18) */
	INT64				startw;					/* starting W (2.30) */
	INT64				dsdx, dtdx;				/* delta S,T per X */
	INT64				dwdx;					/* delta W per X */
	INT64				dsdy, dtdy;				/* delta S,T per Y */
	INT64				dwdy;					/* delta W per Y */

	INT32				lodmin, lodmax;			/* min, max LOD values */
	INT32				lodbias;				/* LOD bias */
	UINT32				lodmask;				/* mask of available LODs */
	UINT32				lodoffset[9];			/* offset of texture base for each LOD */
	INT32				lodbasetemp;			/* lodbase calculated and used during raster */
	INT32				detailmax;				/* detail clamp */
	INT32				detailbias;				/* detail bias */
	UINT8				detailscale;			/* detail scale */

	UINT32				wmask;					/* mask for the current texture width */
	UINT32				hmask;					/* mask for the current texture height */

	UINT8				bilinear_mask;			/* mask for bilinear resolution (0xf0 for V1, 0xff for V2) */

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
	UINT8 *				ram;					/* pointer to frame buffer RAM */
	UINT32				mask;					/* mask to apply to pointers */
	UINT32				rgboffs[3];				/* word offset to 3 RGB buffers */
	UINT32				auxoffs;				/* word offset to 1 aux buffer */

	UINT8				frontbuf;				/* front buffer index */
	UINT8				backbuf;				/* back buffer index */

	UINT32				yorigin;				/* Y origin subtract value */

	UINT32				width;					/* width of current frame buffer */
	UINT32				height;					/* height of current frame buffer */
	//UINT32				xoffs;					/* horizontal offset (back porch) */
	//UINT32				yoffs;					/* vertical offset (back porch) */
	//UINT32				vsyncscan;				/* vertical sync scanline */
	UINT32				rowpixels;				/* pixels per row */
	UINT32				tile_width;				/* width of video tiles */
	UINT32				tile_height;			/* height of video tiles */
	UINT32				x_tiles;				/* number of tiles in the X direction */

	UINT8				vblank;					/* VBLANK state */
	bool				vblank_dont_swap;		/* don't actually swap when we hit this point */
	bool				vblank_flush_pending;

	/* triangle setup info */
	INT16				ax, ay;					/* vertex A x,y (12.4) */
	INT16				bx, by;					/* vertex B x,y (12.4) */
	INT16				cx, cy;					/* vertex C x,y (12.4) */
	INT32				startr, startg, startb, starta; /* starting R,G,B,A (12.12) */
	INT32				startz;					/* starting Z (20.12) */
	INT64				startw;					/* starting W (16.32) */
	INT32				drdx, dgdx, dbdx, dadx;	/* delta R,G,B,A per X */
	INT32				dzdx;					/* delta Z per X */
	INT64				dwdx;					/* delta W per X */
	INT32				drdy, dgdy, dbdy, dady;	/* delta R,G,B,A per Y */
	INT32				dzdy;					/* delta Z per Y */
	INT64				dwdy;					/* delta W per Y */

	stats_block			lfb_stats;				/* LFB-access statistics */

	UINT8				sverts;					/* number of vertices ready */
	setup_vertex		svert[3];				/* 3 setup vertices */

	fifo_state			fifo;					/* framebuffer memory fifo */

	UINT8				fogblend[64];			/* 64-entry fog table */
	UINT8				fogdelta[64];			/* 64-entry fog table */
	UINT8				fogdelta_mask;			/* mask for for delta (0xff for V1, 0xfc for V2) */

	//rgb_t				clut[512];				/* clut gamma data */
};

struct dac_state
{
	UINT8				reg[8];					/* 8 registers */
	UINT8				read_result;			/* pending read result */
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
	UINT8				display;				/* display index */
	UINT32				hits;					/* how many hits (pixels) we've used this for */
	UINT32				polys;					/* how many polys we've used this for */
#endif
	UINT32				eff_color_path;			/* effective fbzColorPath value */
	UINT32				eff_alpha_mode;			/* effective alphaMode value */
	UINT32				eff_fog_mode;			/* effective fogMode value */
	UINT32				eff_fbz_mode;			/* effective fbzMode value */
	UINT32				eff_tex_mode_0;			/* effective textureMode value for TMU #0 */
	UINT32				eff_tex_mode_1;			/* effective textureMode value for TMU #1 */

	bool				shader_ready;
	GLhandleARB			so_shader_program;
	GLhandleARB			so_vertex_shader;
	GLhandleARB			so_fragment_shader;
	INT32*				shader_ulocations;
};
#endif

struct draw_state
{
	double frame_start;
	float vfreq;
	bool override_on;
	bool screen_update_requested;
	bool screen_update_pending;
};

struct triangle_worker
{
	bool threads_active, use_threads, disable_bilinear_filter;
	UINT16 *drawbuf;
	poly_vertex v1, v2, v3;
	INT32 v1y, v3y, totalpix;
	SDL_sem* sembegin[TRIANGLE_THREADS];
	volatile bool done[TRIANGLE_THREADS];
};

struct voodoo_state
{
	UINT8				chipmask;				/* mask for which chips are available */

	voodoo_reg			reg[0x400];				/* raw registers */
	const UINT8 *		regaccess;				/* register access array */
	bool				alt_regmap;				/* enable alternate register map? */

	pci_state			pci;					/* PCI state */
	dac_state			dac;					/* DAC state */

	fbi_state			fbi;					/* FBI states */
	tmu_state			tmu[MAX_TMU];			/* TMU states */
	tmu_shared_state	tmushare;				/* TMU shared state */
	UINT32				tmu_config;

#ifdef C_ENABLE_VOODOO_OPENGL
	UINT16				next_rasterizer;		/* next rasterizer index */
	raster_info			rasterizer[MAX_RASTERIZERS];	/* array of rasterizers */
	raster_info *		raster_hash[RASTER_HASH_SIZE];	/* hash table of rasterizers */
#endif

	stats_block			thread_stats[TRIANGLE_WORKERS];	/* per-thread statistics */

	bool				send_config;
	bool				clock_enabled;
	bool				output_on;
	bool				active;
#ifdef C_ENABLE_VOODOO_OPENGL
	bool				ogl;
	bool				ogl_dimchange;
	bool				ogl_palette_changed;
#endif
#ifdef C_ENABLE_VOODOO_DEBUG
	const char *const *	regnames;				/* register names array */
#endif

	draw_state			draw;
	triangle_worker		tworker;
};

#ifdef C_ENABLE_VOODOO_OPENGL
struct poly_extra_data
{
	voodoo_state *		state;					/* pointer back to the voodoo state */
	raster_info *		info;					/* pointer to rasterizer information */

	INT16				ax, ay;					/* vertex A x,y (12.4) */
	INT32				startr, startg, startb, starta; /* starting R,G,B,A (12.12) */
	INT32				startz;					/* starting Z (20.12) */
	INT64				startw;					/* starting W (16.32) */
	INT32				drdx, dgdx, dbdx, dadx;	/* delta R,G,B,A per X */
	INT32				dzdx;					/* delta Z per X */
	INT64				dwdx;					/* delta W per X */
	INT32				drdy, dgdy, dbdy, dady;	/* delta R,G,B,A per Y */
	INT32				dzdy;					/* delta Z per Y */
	INT64				dwdy;					/* delta W per Y */

	INT64				starts0, startt0;		/* starting S,T (14.18) */
	INT64				startw0;				/* starting W (2.30) */
	INT64				ds0dx, dt0dx;			/* delta S,T per X */
	INT64				dw0dx;					/* delta W per X */
	INT64				ds0dy, dt0dy;			/* delta S,T per Y */
	INT64				dw0dy;					/* delta W per Y */
	INT32				lodbase0;				/* used during rasterization */

	INT64				starts1, startt1;		/* starting S,T (14.18) */
	INT64				startw1;				/* starting W (2.30) */
	INT64				ds1dx, dt1dx;			/* delta S,T per X */
	INT64				dw1dx;					/* delta W per X */
	INT64				ds1dy, dt1dy;			/* delta S,T per Y */
	INT64				dw1dy;					/* delta W per Y */
	INT32				lodbase1;				/* used during rasterization */

	UINT16				dither[16];				/* dither matrix, for fastfill */

	Bitu				texcount;
	Bitu				r_fbzColorPath;
	Bitu				r_fbzMode;
	Bitu				r_alphaMode;
	Bitu				r_fogMode;
	INT32				r_textureMode0;
	INT32				r_textureMode1;
};
#endif

/*************************************
 *
 *  Inline FIFO management
 *
 *************************************/

inline INT32 fifo_space(fifo_state* f)
{
	INT32 items = 0;
	if (items < 0)
		items += f->size;
	return f->size - 1 - items;
}

inline UINT8 count_leading_zeros(UINT32 value)
{
#ifdef _MSC_VER
	DWORD idx = 0;
	return (_BitScanReverse(&idx, value) ? (UINT8)(31 - idx) : (UINT8)32);
#else
	return (value ? (UINT8)__builtin_clz(value) : (UINT8)32);
#endif
	//INT32 result;
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
	//return (UINT8)result;
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
	UINT32 temp, rlog;
	UINT32 interp;
	UINT32 *table;
	UINT64 recip;
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
	if (value & LONGTYPE(0xffff00000000))
	{
		temp = (UINT32)(value >> 16);
		exponent -= 16;
	}
	else
		temp = (UINT32)value;

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
	/* to account for the fact that there are two UINT32's per table entry */
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
	*log_2 = ((exponent - (31 - RECIPLOG_INPUT_PREC)) << LOG_OUTPUT_PREC) - rlog;

	/* adjust the exponent to account for all the reciprocal-related parameters to arrive at a final shift amount */
	exponent += (RECIP_OUTPUT_PREC - RECIPLOG_LOOKUP_PREC) -
	            (31 - RECIPLOG_INPUT_PREC);

	/* shift by the exponent */
	if (exponent < 0)
		recip >>= -exponent;
	else
		recip <<= exponent;

	/* on the way out, apply the original sign to the reciprocal */
	return neg ? -(INT64)recip : (INT64)recip;
}



/*************************************
 *
 *  Float-to-int conversions
 *
 *************************************/

inline INT32 float_to_int32(UINT32 data, int fixedbits)
{
	int exponent = ((data >> 23) & 0xff) - 127 - 23 + fixedbits;
	INT32 result = (data & 0x7fffff) | 0x800000;
	if (exponent < 0)
	{
		if (exponent > -32)
			result >>= -exponent;
		else
			result = 0;
	}
	else
	{
		if (exponent < 32)
			result <<= exponent;
		else
			result = 0x7fffffff;
	}
	if (data & 0x80000000)
		result = -result;
	return result;
}

inline INT64 float_to_int64(UINT32 data, int fixedbits)
{
	int exponent = ((data >> 23) & 0xff) - 127 - 23 + fixedbits;
	INT64 result = (data & 0x7fffff) | 0x800000;
	if (exponent < 0)
	{
		if (exponent > -64)
			result >>= -exponent;
		else
			result = 0;
	}
	else
	{
		if (exponent < 64)
			result <<= exponent;
		else
			result = LONGTYPE(0x7fffffffffffffff);
	}
	if (data & 0x80000000)
		result = -result;
	return result;
}



#ifdef C_ENABLE_VOODOO_OPENGL
/*************************************
 *
 *  Rasterizer inlines
 *
 *************************************/

inline UINT32 normalize_color_path(UINT32 eff_color_path)
{
	/* ignore the subpixel adjust and texture enable flags */
	eff_color_path &= ~((1 << 26) | (1 << 27));

	return eff_color_path;
}

inline UINT32 normalize_alpha_mode(UINT32 eff_alpha_mode)
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

inline UINT32 normalize_fog_mode(UINT32 eff_fog_mode)
{
	/* if not doing fogging, ignore all the other fog bits */
	if (!FOGMODE_ENABLE_FOG(eff_fog_mode))
		eff_fog_mode = 0;

	return eff_fog_mode;
}

inline UINT32 normalize_fbz_mode(UINT32 eff_fbz_mode)
{
	/* ignore the draw buffer */
	eff_fbz_mode &= ~(3 << 14);

	return eff_fbz_mode;
}

inline UINT32 normalize_tex_mode(UINT32 eff_tex_mode)
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

inline UINT32 compute_raster_hash(const raster_info* info)
{
	UINT32 hash;

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
#define DITHER_RB(val,dith)	((((val) << 1) - ((val) >> 4) + ((val) >> 7) + (dith)) >> 1)
#define DITHER_G(val,dith)	((((val) << 2) - ((val) >> 4) + ((val) >> 6) + (dith)) >> 2)

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
			dither_lookup = &dither4_lookup[(YY & 3) << 11];					\
		}																		\
		else																	\
		{																		\
			dither = &dither_matrix_2x2[((YY) & 3) * 4];						\
			dither_lookup = &dither2_lookup[(YY & 3) << 11];					\
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
		const UINT8 *dith = &DITHER_LOOKUP[((XX) & 3) << 1];					\
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

#define CLAMPED_ARGB(ITERR, ITERG, ITERB, ITERA, FBZCP, RESULT)					\
do																				\
{																				\
	INT32 r = (INT32)(ITERR) >> 12;												\
	INT32 g = (INT32)(ITERG) >> 12;												\
	INT32 b = (INT32)(ITERB) >> 12;												\
	INT32 a = (INT32)(ITERA) >> 12;												\
																				\
	if (FBZCP_RGBZW_CLAMP(FBZCP) == 0)											\
	{																			\
		r &= 0xfff;																\
		RESULT.rgb.r = r;														\
		if (r == 0xfff)															\
			RESULT.rgb.r = 0;													\
		else if (r == 0x100)													\
			RESULT.rgb.r = 0xff;												\
																				\
		g &= 0xfff;																\
		RESULT.rgb.g = g;														\
		if (g == 0xfff)															\
			RESULT.rgb.g = 0;													\
		else if (g == 0x100)													\
			RESULT.rgb.g = 0xff;												\
																				\
		b &= 0xfff;																\
		RESULT.rgb.b = b;														\
		if (b == 0xfff)															\
			RESULT.rgb.b = 0;													\
		else if (b == 0x100)													\
			RESULT.rgb.b = 0xff;												\
																				\
		a &= 0xfff;																\
		RESULT.rgb.a = a;														\
		if (a == 0xfff)															\
			RESULT.rgb.a = 0;													\
		else if (a == 0x100)													\
			RESULT.rgb.a = 0xff;												\
	}																			\
	else																		\
	{																			\
		RESULT.rgb.r = (r < 0) ? 0 : (r > 0xff) ? 0xff : (UINT8)r;				\
		RESULT.rgb.g = (g < 0) ? 0 : (g > 0xff) ? 0xff : (UINT8)g;				\
		RESULT.rgb.b = (b < 0) ? 0 : (b > 0xff) ? 0xff : (UINT8)b;				\
		RESULT.rgb.a = (a < 0) ? 0 : (a > 0xff) ? 0xff : (UINT8)a;				\
	}																			\
}																				\
while (0)


#define CLAMPED_Z(ITERZ, FBZCP, RESULT)											\
do																				\
{																				\
	(RESULT) = (INT32)(ITERZ) >> 12;											\
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
		CLAMP((RESULT), 0, 0xffff);												\
	}																			\
}																				\
while (0)


#define CLAMPED_W(ITERW, FBZCP, RESULT)											\
do																				\
{																				\
	(RESULT) = (INT16)((ITERW) >> 32);											\
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
		CLAMP((RESULT), 0, 0xff);												\
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
			if (((COLOR.u ^ (VV)->reg[chromaKey].u) & 0xffffff) == 0)			\
			{																	\
				ADD_STAT_COUNT(STATS, chroma_fail)								\
				goto skipdrawdepth;												\
			}																	\
		}																		\
																				\
		/* tricky range version */												\
		else																	\
		{																		\
			INT32 low, high, test;												\
			int results = 0;													\
																				\
			/* check blue */													\
			low = (VV)->reg[chromaKey].rgb.b;									\
			high = (VV)->reg[chromaRange].rgb.b;								\
			test = COLOR.rgb.b;													\
			results = (test >= low && test <= high);							\
			results ^= CHROMARANGE_BLUE_EXCLUSIVE((VV)->reg[chromaRange].u);	\
			results <<= 1;														\
																				\
			/* check green */													\
			low = (VV)->reg[chromaKey].rgb.g;									\
			high = (VV)->reg[chromaRange].rgb.g;								\
			test = COLOR.rgb.g;													\
			results |= (test >= low && test <= high);							\
			results ^= CHROMARANGE_GREEN_EXCLUSIVE((VV)->reg[chromaRange].u);	\
			results <<= 1;														\
																				\
			/* check red */														\
			low = (VV)->reg[chromaKey].rgb.r;									\
			high = (VV)->reg[chromaRange].rgb.r;								\
			test = COLOR.rgb.r;													\
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
		UINT8 alpharef = (VV)->reg[alphaMode].rgb.a;							\
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

#define APPLY_ALPHA_BLEND(FBZMODE, ALPHAMODE, XX, DITHER, RR, GG, BB, AA)		\
do																				\
{																				\
	if (ALPHAMODE_ALPHABLEND(ALPHAMODE))										\
	{																			\
		int dpix = dest[XX];													\
		int dr = (dpix >> 8) & 0xf8;											\
		int dg = (dpix >> 3) & 0xfc;											\
		int db = (dpix << 3) & 0xf8;											\
		int da = (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) && depth) ? depth[XX] : 0xff;		\
		int sr = (RR);															\
		int sg = (GG);															\
		int sb = (BB);															\
		int sa = (AA);															\
		int ta;																	\
																				\
		/* apply dither subtraction */											\
		if ((FBZMODE_ALPHA_DITHER_SUBTRACT(FBZMODE)) && DITHER)					\
		{																		\
			/* look up the dither value from the appropriate matrix */			\
			int dith = DITHER[(XX) & 3];										\
																				\
			/* subtract the dither value */										\
			dr = ((dr << 1) + 15 - dith) >> 1;									\
			dg = ((dg << 2) + 15 - dith) >> 2;									\
			db = ((db << 1) + 15 - dith) >> 1;									\
		}																		\
																				\
		/* compute source portion */											\
		switch (ALPHAMODE_SRCRGBBLEND(ALPHAMODE))								\
		{																		\
			default:	/* reserved */											\
			case 0:		/* AZERO */												\
				(RR) = (GG) = (BB) = 0;											\
				break;															\
																				\
			case 1:		/* ASRC_ALPHA */										\
				(RR) = (sr * (sa + 1)) >> 8;									\
				(GG) = (sg * (sa + 1)) >> 8;									\
				(BB) = (sb * (sa + 1)) >> 8;									\
				break;															\
																				\
			case 2:		/* A_COLOR */											\
				(RR) = (sr * (dr + 1)) >> 8;									\
				(GG) = (sg * (dg + 1)) >> 8;									\
				(BB) = (sb * (db + 1)) >> 8;									\
				break;															\
																				\
			case 3:		/* ADST_ALPHA */										\
				(RR) = (sr * (da + 1)) >> 8;									\
				(GG) = (sg * (da + 1)) >> 8;									\
				(BB) = (sb * (da + 1)) >> 8;									\
				break;															\
																				\
			case 4:		/* AONE */												\
				break;															\
																				\
			case 5:		/* AOMSRC_ALPHA */										\
				(RR) = (sr * (0x100 - sa)) >> 8;								\
				(GG) = (sg * (0x100 - sa)) >> 8;								\
				(BB) = (sb * (0x100 - sa)) >> 8;								\
				break;															\
																				\
			case 6:		/* AOM_COLOR */											\
				(RR) = (sr * (0x100 - dr)) >> 8;								\
				(GG) = (sg * (0x100 - dg)) >> 8;								\
				(BB) = (sb * (0x100 - db)) >> 8;								\
				break;															\
																				\
			case 7:		/* AOMDST_ALPHA */										\
				(RR) = (sr * (0x100 - da)) >> 8;								\
				(GG) = (sg * (0x100 - da)) >> 8;								\
				(BB) = (sb * (0x100 - da)) >> 8;								\
				break;															\
																				\
			case 15:	/* ASATURATE */											\
				ta = (sa < (0x100 - da)) ? sa : (0x100 - da);					\
				(RR) = (sr * (ta + 1)) >> 8;									\
				(GG) = (sg * (ta + 1)) >> 8;									\
				(BB) = (sb * (ta + 1)) >> 8;									\
				break;															\
		}																		\
																				\
		/* add in dest portion */												\
		switch (ALPHAMODE_DSTRGBBLEND(ALPHAMODE))								\
		{																		\
			default:	/* reserved */											\
			case 0:		/* AZERO */												\
				break;															\
																				\
			case 1:		/* ASRC_ALPHA */										\
				(RR) += (dr * (sa + 1)) >> 8;									\
				(GG) += (dg * (sa + 1)) >> 8;									\
				(BB) += (db * (sa + 1)) >> 8;									\
				break;															\
																				\
			case 2:		/* A_COLOR */											\
				(RR) += (dr * (sr + 1)) >> 8;									\
				(GG) += (dg * (sg + 1)) >> 8;									\
				(BB) += (db * (sb + 1)) >> 8;									\
				break;															\
																				\
			case 3:		/* ADST_ALPHA */										\
				(RR) += (dr * (da + 1)) >> 8;									\
				(GG) += (dg * (da + 1)) >> 8;									\
				(BB) += (db * (da + 1)) >> 8;									\
				break;															\
																				\
			case 4:		/* AONE */												\
				(RR) += dr;														\
				(GG) += dg;														\
				(BB) += db;														\
				break;															\
																				\
			case 5:		/* AOMSRC_ALPHA */										\
				(RR) += (dr * (0x100 - sa)) >> 8;								\
				(GG) += (dg * (0x100 - sa)) >> 8;								\
				(BB) += (db * (0x100 - sa)) >> 8;								\
				break;															\
																				\
			case 6:		/* AOM_COLOR */											\
				(RR) += (dr * (0x100 - sr)) >> 8;								\
				(GG) += (dg * (0x100 - sg)) >> 8;								\
				(BB) += (db * (0x100 - sb)) >> 8;								\
				break;															\
																				\
			case 7:		/* AOMDST_ALPHA */										\
				(RR) += (dr * (0x100 - da)) >> 8;								\
				(GG) += (dg * (0x100 - da)) >> 8;								\
				(BB) += (db * (0x100 - da)) >> 8;								\
				break;															\
																				\
			case 15:	/* A_COLORBEFOREFOG */									\
				(RR) += (dr * (prefogr + 1)) >> 8;								\
				(GG) += (dg * (prefogg + 1)) >> 8;								\
				(BB) += (db * (prefogb + 1)) >> 8;								\
				break;															\
		}																		\
																				\
		/* blend the source alpha */											\
		(AA) = 0;																\
		if (ALPHAMODE_SRCALPHABLEND(ALPHAMODE) == 4)							\
			(AA) = sa;															\
																				\
		/* blend the dest alpha */												\
		if (ALPHAMODE_DSTALPHABLEND(ALPHAMODE) == 4)							\
			(AA) += da;															\
																				\
		/* clamp */																\
		CLAMP((RR), 0x00, 0xff);												\
		CLAMP((GG), 0x00, 0xff);												\
		CLAMP((BB), 0x00, 0xff);												\
		CLAMP((AA), 0x00, 0xff);												\
	}																			\
}																				\
while (0)



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
		INT32 fr, fg, fb;														\
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
			INT32 fogblend = 0;													\
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
					INT32 delta = (VV)->fbi.fogdelta[wfloat >> 10];				\
					INT32 deltaval;												\
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
							deltaval += DITHER4[(XX) & 3];						\
					deltaval >>= 4;												\
																				\
					/* add to the blending factor */							\
					fogblend = (VV)->fbi.fogblend[wfloat >> 10] + deltaval;		\
					break;														\
				}																\
																				\
				case 1:		/* iterated A */									\
					fogblend = ITERAXXX.rgb.a;									\
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
		CLAMP((RR), 0x00, 0xff);												\
		CLAMP((GG), 0x00, 0xff);												\
		CLAMP((BB), 0x00, 0xff);												\
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
	INT32 blendr, blendg, blendb, blenda;										\
	INT32 tr, tg, tb, ta;														\
	INT32 s, t, lod, ilod;														\
	INT64 oow;																	\
	INT32 smax, tmax;															\
	UINT32 texbase;																\
	rgb_union c_local;															\
																				\
	/* determine the S/T/LOD values for this texture */							\
	if (TEXMODE_ENABLE_PERSPECTIVE(TEXMODE))									\
	{																			\
		oow = fast_reciplog((ITERW), &lod);										\
		s = (INT32)((oow * (ITERS)) >> 29);										\
		t = (INT32)((oow * (ITERT)) >> 29);										\
		lod += (LODBASE);														\
	}																			\
	else																		\
	{																			\
		s = (INT32)((ITERS) >> 14);												\
		t = (INT32)((ITERT) >> 14);												\
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
			lod += DITHER4[(XX) & 3] << 4;										\
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
		UINT32 texel0;															\
																				\
		/* adjust S/T for the LOD and strip off the fractions */				\
		s >>= ilod + 18;														\
		t >>= ilod + 18;														\
																				\
		/* clamp/wrap S/T if necessary */										\
		if (TEXMODE_CLAMP_S(TEXMODE))											\
			CLAMP(s, 0, smax);													\
		if (TEXMODE_CLAMP_T(TEXMODE))											\
			CLAMP(t, 0, tmax);													\
		s &= smax;																\
		t &= tmax;																\
		t *= smax + 1;															\
																				\
		/* fetch texel data */													\
		if (TEXMODE_FORMAT(TEXMODE) < 8)										\
		{																		\
			texel0 = *(UINT8 *)&(TT)->ram[(texbase + t + s) & (TT)->mask];		\
			c_local.u = (LOOKUP)[texel0];										\
		}																		\
		else																	\
		{																		\
			texel0 = *(UINT16 *)&(TT)->ram[(texbase + 2*(t + s)) & (TT)->mask];	\
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
		UINT32 texel0, texel1, texel2, texel3;									\
		UINT8 sfrac, tfrac;														\
		INT32 s1, t1;															\
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
		sfrac = (UINT8)(s & (TT)->bilinear_mask);								\
		tfrac = (UINT8)(t & (TT)->bilinear_mask);								\
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
			CLAMP(s, 0, smax);													\
			CLAMP(s1, 0, smax);													\
		}																		\
		if (TEXMODE_CLAMP_T(TEXMODE))											\
		{																		\
			CLAMP(t, 0, tmax);													\
			CLAMP(t1, 0, tmax);													\
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
			texel0 = *(UINT8 *)&(TT)->ram[(texbase + t + s) & (TT)->mask];		\
			texel1 = *(UINT8 *)&(TT)->ram[(texbase + t + s1) & (TT)->mask];		\
			texel2 = *(UINT8 *)&(TT)->ram[(texbase + t1 + s) & (TT)->mask];		\
			texel3 = *(UINT8 *)&(TT)->ram[(texbase + t1 + s1) & (TT)->mask];	\
			texel0 = (LOOKUP)[texel0];											\
			texel1 = (LOOKUP)[texel1];											\
			texel2 = (LOOKUP)[texel2];											\
			texel3 = (LOOKUP)[texel3];											\
		}																		\
		else																	\
		{																		\
			texel0 = *(UINT16 *)&(TT)->ram[(texbase + 2*(t + s)) & (TT)->mask];	\
			texel1 = *(UINT16 *)&(TT)->ram[(texbase + 2*(t + s1)) & (TT)->mask];\
			texel2 = *(UINT16 *)&(TT)->ram[(texbase + 2*(t1 + s)) & (TT)->mask];\
			texel3 = *(UINT16 *)&(TT)->ram[(texbase + 2*(t1 + s1)) & (TT)->mask];\
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
		tr = COTHER.rgb.r;														\
		tg = COTHER.rgb.g;														\
		tb = COTHER.rgb.b;														\
	}																			\
	else																		\
		tr = tg = tb = 0;														\
																				\
	/* select zero/other for alpha */											\
	if (!TEXMODE_TCA_ZERO_OTHER(TEXMODE))										\
		ta = COTHER.rgb.a;														\
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
			blendr = blendg = blendb = COTHER.rgb.a;							\
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
			blenda = COTHER.rgb.a;												\
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
	RESULT.rgb.r = (tr < 0) ? 0 : (tr > 0xff) ? 0xff : (UINT8)tr;				\
	RESULT.rgb.g = (tg < 0) ? 0 : (tg > 0xff) ? 0xff : (UINT8)tg;				\
	RESULT.rgb.b = (tb < 0) ? 0 : (tb > 0xff) ? 0xff : (UINT8)tb;				\
	RESULT.rgb.a = (ta < 0) ? 0 : (ta > 0xff) ? 0xff : (UINT8)ta;				\
																				\
	/* invert */																\
	if (TEXMODE_TC_INVERT_OUTPUT(TEXMODE))										\
		RESULT.u ^= 0x00ffffff;													\
	if (TEXMODE_TCA_INVERT_OUTPUT(TEXMODE))										\
		RESULT.rgb.a ^= 0xff;													\
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
	INT32 depthval, wfloat;														\
	INT32 prefogr, prefogg, prefogb;											\
	INT32 r, g, b, a;															\
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
		UINT32 temp = (UINT32)(ITERW);											\
		if ((temp & 0xffff0000) == 0)											\
			wfloat = 0xffff;													\
		else																	\
		{																		\
			int exp = count_leading_zeros(temp);								\
			wfloat = ((exp << 12) | ((~temp >> (19 - exp)) & 0xfff));			\
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
			UINT32 temp = (ITERZ) << 4;											\
			if ((temp & 0xffff0000) == 0)										\
				depthval = 0xffff;												\
			else																\
			{																	\
				int exp = count_leading_zeros(temp);							\
				depthval = ((exp << 12) | ((~temp >> (19 - exp)) & 0xfff));		\
				if (depthval < 0xffff) depthval++;								\
			}																	\
		}																		\
	}																			\
																				\
	/* add the bias */															\
	if (FBZMODE_ENABLE_DEPTH_BIAS(FBZMODE))										\
	{																			\
		depthval += (INT16)(ZACOLOR);											\
		CLAMP(depthval, 0, 0xffff);												\
	}																			\
																				\
	/* handle depth buffer testing */											\
	if (FBZMODE_ENABLE_DEPTHBUF(FBZMODE))										\
	{																			\
		INT32 depthsource;														\
																				\
		/* the source depth is either the iterated W/Z+bias or a */				\
		/* constant value */													\
		if (FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) == 0)							\
			depthsource = depthval;												\
		else																	\
			depthsource = (UINT16)(ZACOLOR);									\
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
		dest[XX] = (UINT16)((r << 11) | (g << 5) | b);							\
	}																			\
																				\
	/* write to aux buffer */													\
	if (depth && FBZMODE_AUX_BUFFER_MASK(FBZMODE))								\
	{																			\
		if (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0)							\
			depth[XX] = (UINT16)depthval;										\
		else																	\
			depth[XX] = (UINT16)a;												\
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

static const UINT8 register_alias_map[0x40] =
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

static const UINT8 voodoo_register_access[0x100] =
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


static const UINT8 voodoo2_register_access[0x100] =
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

static voodoo_state *v;
static UINT8 vtype = VOODOO_1, vperf;

/* fast dither lookup */
static UINT8 dither4_lookup[256*16*2];
static UINT8 dither2_lookup[256*16*2];

#define LOG_VOODOO LOG_PCI
#define LOG_VBLANK_SWAP		(0)
#define LOG_REGISTERS		(0)
#define LOG_LFB				(0)
#define LOG_TEXTURE_RAM		(0)
#define LOG_RASTERIZERS		(0)

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

static inline void raster_generic(const voodoo_state* v, UINT32 TMUS, UINT32 TEXMODE0,
                                  UINT32 TEXMODE1, void* destbase, INT32 y,
                                  const poly_extent* extent, stats_block& stats)
{
	const uint8_t* dither_lookup = nullptr;
	const uint8_t* dither4       = nullptr;
	const uint8_t* dither        = nullptr;

	INT32 scry = y;
	INT32 startx = extent->startx;
	INT32 stopx = extent->stopx;

	const fbi_state& fbi = v->fbi;
	const tmu_state& tmu0 = v->tmu[0];
	const tmu_state& tmu1 = v->tmu[1];
	UINT32 r_fbzColorPath = v->reg[fbzColorPath].u;
	UINT32 r_fbzMode = v->reg[fbzMode].u;
	UINT32 r_alphaMode = v->reg[alphaMode].u;
	UINT32 r_fogMode = v->reg[fogMode].u;
	UINT32 r_zaColor = v->reg[zaColor].u;
	UINT32 r_stipple = v->reg[stipple].u;

	/* determine the screen Y */
	if (FBZMODE_Y_ORIGIN(r_fbzMode))
		scry = (v->fbi.yorigin - y) & 0x3ff;

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
		if (scry < (INT32)((v->reg[clipLowYHighY].u >> 16) & 0x3ff) ||
			scry >= (INT32)(v->reg[clipLowYHighY].u & 0x3ff))
		{
			stats.pixels_in += stopx - startx;
			//stats.clip_fail += stopx - startx;
			return;
		}

		/* X clipping */
		INT32 tempclip = (v->reg[clipLeftRight].u >> 16) & 0x3ff;
		if (startx < tempclip)
		{
			stats.pixels_in += tempclip - startx;
			startx = tempclip;
		}
		tempclip = v->reg[clipLeftRight].u & 0x3ff;
		if (stopx >= tempclip)
		{
			stats.pixels_in += stopx - tempclip;
			stopx = tempclip - 1;
		}
	}

	/* get pointers to the target buffer and depth buffer */
	UINT16 *dest = (UINT16 *)destbase + scry * v->fbi.rowpixels;
	UINT16 *depth = (v->fbi.auxoffs != (UINT32)(~0)) ? ((UINT16 *)(v->fbi.ram + v->fbi.auxoffs) + scry * v->fbi.rowpixels) : NULL;

	/* compute the starting parameters */
	INT32 dx = startx - (fbi.ax >> 4);
	INT32 dy = y - (fbi.ay >> 4);
	INT32 iterr = fbi.startr + dy * fbi.drdy + dx * fbi.drdx;
	INT32 iterg = fbi.startg + dy * fbi.dgdy + dx * fbi.dgdx;
	INT32 iterb = fbi.startb + dy * fbi.dbdy + dx * fbi.dbdx;
	INT32 itera = fbi.starta + dy * fbi.dady + dx * fbi.dadx;
	INT32 iterz = fbi.startz + dy * fbi.dzdy + dx * fbi.dzdx;
	INT64 iterw = fbi.startw + dy * fbi.dwdy + dx * fbi.dwdx;
	INT64 iterw0 = 0, iterw1 = 0, iters0 = 0, iters1 = 0, itert0 = 0, itert1 = 0;
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
	for (INT32 x = startx; x < stopx; x++)
	{
		rgb_union iterargb = { 0 };
		rgb_union texel = { 0 };

		/* pixel pipeline part 1 handles depth testing and stippling */
		PIXEL_PIPELINE_BEGIN(v, stats, x, y, r_fbzColorPath, r_fbzMode, iterz, iterw, r_zaColor, r_stipple);

		/* run the texture pipeline on TMU1 to produce a value in texel */
		/* note that they set LOD min to 8 to "disable" a TMU */

		if (TMUS >= 2 && v->tmu[1].lodmin < (8 << 8)) {
			const tmu_state* const tmus = &v->tmu[1];
			const rgb_t* const lookup = tmus->lookup;
			TEXTURE_PIPELINE(tmus, x, dither4, TEXMODE1, texel,
								lookup, tmus->lodbasetemp,
								iters1, itert1, iterw1, texel);
		}

		/* run the texture pipeline on TMU0 to produce a final */
		/* result in texel */
		/* note that they set LOD min to 8 to "disable" a TMU */
		if (TMUS >= 1 && v->tmu[0].lodmin < (8 << 8)) {
			if (!v->send_config) {
				const tmu_state* const tmus = &v->tmu[0];
				const rgb_t* const lookup = tmus->lookup;
				TEXTURE_PIPELINE(tmus, x, dither4, TEXMODE0, texel,
								lookup, tmus->lodbasetemp,
								iters0, itert0, iterw0, texel);
			} else {	/* send config data to the frame buffer */
				texel.u=v->tmu_config;
			}
		}

		/* colorpath pipeline selects source colors and does blending */
		CLAMPED_ARGB(iterr, iterg, iterb, itera, r_fbzColorPath, iterargb);


		INT32 blendr, blendg, blendb, blenda;
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
				c_other.u = v->reg[color1].u;
				break;
			case 3:	/* reserved */
				c_other.u = 0;
				break;
		}

		/* handle chroma key */
		APPLY_CHROMAKEY(v, stats, r_fbzMode, c_other);

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
				c_other.rgb.a = v->reg[color1].rgb.a;
				break;
			case 3:	/* reserved */
				c_other.rgb.a = 0;
				break;
		}

		/* handle alpha mask */
		APPLY_ALPHAMASK(v, stats, r_fbzMode, c_other.rgb.a);

		/* handle alpha test */
		APPLY_ALPHATEST(v, stats, r_alphaMode, c_other.rgb.a);

		/* compute c_local */
		if (FBZCP_CC_LOCALSELECT_OVERRIDE(r_fbzColorPath) == 0)
		{
			if (FBZCP_CC_LOCALSELECT(r_fbzColorPath) == 0)	/* iterated RGB */
				c_local.u = iterargb.u;
			else											/* color0 RGB */
				c_local.u = v->reg[color0].u;
		}
		else
		{
			if (!(texel.rgb.a & 0x80))					/* iterated RGB */
				c_local.u = iterargb.u;
			else											/* color0 RGB */
				c_local.u = v->reg[color0].u;
		}

		/* compute a_local */
		switch (FBZCP_CCA_LOCALSELECT(r_fbzColorPath))
		{
			case 0:		/* iterated alpha */
				c_local.rgb.a = iterargb.rgb.a;
				break;
			case 1:		/* color0 alpha */
				c_local.rgb.a = v->reg[color0].rgb.a;
				break;
			case 2:		/* clamped iterated Z[27:20] */
			{
				int temp;
				CLAMPED_Z(iterz, r_fbzColorPath, temp);
				c_local.rgb.a = (UINT8)temp;
				break;
			}
			case 3:		/* clamped iterated W[39:32] */
			{
				int temp;
				CLAMPED_W(iterw, r_fbzColorPath, temp);			/* Voodoo 2 only */
				c_local.rgb.a = (UINT8)temp;
				break;
			}
		}

		/* select zero or c_other */
		if (FBZCP_CC_ZERO_OTHER(r_fbzColorPath) == 0)
		{
			r = c_other.rgb.r;
			g = c_other.rgb.g;
			b = c_other.rgb.b;
		}
		else
			r = g = b = 0;

		/* select zero or a_other */
		if (FBZCP_CCA_ZERO_OTHER(r_fbzColorPath) == 0)
			a = c_other.rgb.a;
		else
			a = 0;

		/* subtract c_local */
		if (FBZCP_CC_SUB_CLOCAL(r_fbzColorPath))
		{
			r -= c_local.rgb.r;
			g -= c_local.rgb.g;
			b -= c_local.rgb.b;
		}

		/* subtract a_local */
		if (FBZCP_CCA_SUB_CLOCAL(r_fbzColorPath))
			a -= c_local.rgb.a;

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
				blenda = c_local.rgb.a;
				break;
			case 2:		/* a_other */
				blenda = c_other.rgb.a;
				break;
			case 3:		/* a_local */
				blenda = c_local.rgb.a;
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
		if (!FBZCP_CCA_REVERSE_BLEND(r_fbzColorPath))
			blenda ^= 0xff;

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
		if (FBZCP_CCA_ADD_ACLOCAL(r_fbzColorPath))
			a += c_local.rgb.a;

		/* clamp */
		CLAMP(r, 0x00, 0xff);
		CLAMP(g, 0x00, 0xff);
		CLAMP(b, 0x00, 0xff);
		CLAMP(a, 0x00, 0xff);

		/* invert */
		if (FBZCP_CC_INVERT_OUTPUT(r_fbzColorPath))
		{
			r ^= 0xff;
			g ^= 0xff;
			b ^= 0xff;
		}
		if (FBZCP_CCA_INVERT_OUTPUT(r_fbzColorPath))
			a ^= 0xff;


		/* pixel pipeline part 2 handles fog, alpha, and final output */
		PIXEL_PIPELINE_MODIFY(v, dither, dither4, x,
							r_fbzMode, r_fbzColorPath, r_alphaMode, r_fogMode,
							iterz, iterw, iterargb);
		PIXEL_PIPELINE_FINISH(v, dither_lookup, x, dest, depth, r_fbzMode);
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
static raster_info *add_rasterizer(voodoo_state *v, const raster_info *cinfo)
{
	if (v->next_rasterizer >= MAX_RASTERIZERS)
	{
		E_Exit("Out of space for new rasterizers!");
		v->next_rasterizer = 0;
	}

	raster_info *info = &v->rasterizer[v->next_rasterizer++];
	int hash = compute_raster_hash(cinfo);

	/* make a copy of the info */
	*info = *cinfo;

#ifdef C_ENABLE_VOODOO_DEBUG
	/* fill in the data */
	info->hits = 0;
	info->polys = 0;
#endif

	/* hook us into the hash table */
	info->next = v->raster_hash[hash];
	v->raster_hash[hash] = info;

	if (LOG_RASTERIZERS)
		LOG_MSG("Adding rasterizer @ %p : %08X %08X %08X %08X %08X %08X (hash=%d)\n",
				info->callback,
				info->eff_color_path, info->eff_alpha_mode, info->eff_fog_mode, info->eff_fbz_mode,
				info->eff_tex_mode_0, info->eff_tex_mode_1, hash);

	return info;
}

/*-------------------------------------------------
    find_rasterizer - find a rasterizer that
    matches  our current parameters and return
    it, creating a new one if necessary
-------------------------------------------------*/
static raster_info *find_rasterizer(voodoo_state *v, int texcount)
{
	raster_info *info, *prev = NULL;
	raster_info curinfo;
	int hash;

	/* build an info struct with all the parameters */
	curinfo.eff_color_path = normalize_color_path(v->reg[fbzColorPath].u);
	curinfo.eff_alpha_mode = normalize_alpha_mode(v->reg[alphaMode].u);
	curinfo.eff_fog_mode = normalize_fog_mode(v->reg[fogMode].u);
	curinfo.eff_fbz_mode = normalize_fbz_mode(v->reg[fbzMode].u);
	curinfo.eff_tex_mode_0 = (texcount >= 1) ? normalize_tex_mode(v->tmu[0].reg[textureMode].u) : 0xffffffff;
	curinfo.eff_tex_mode_1 = (texcount >= 2) ? normalize_tex_mode(v->tmu[1].reg[textureMode].u) : 0xffffffff;

	/* compute the hash */
	hash = compute_raster_hash(&curinfo);

	/* find the appropriate hash entry */
	for (info = v->raster_hash[hash]; info; prev = info, info = info->next)
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
				info->next = v->raster_hash[hash];
				v->raster_hash[hash] = info;
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

	return add_rasterizer(v, &curinfo);
}
#endif


/***************************************************************************
    GENERIC RASTERIZERS
***************************************************************************/

/*-------------------------------------------------
    raster_fastfill - per-scanline
    implementation of the 'fastfill' command
-------------------------------------------------*/
static void raster_fastfill(void *destbase, INT32 y, const poly_extent *extent, const UINT16* extra_dither)
{
	stats_block stats = {};
	INT32 startx = extent->startx;
	INT32 stopx = extent->stopx;
	int scry, x;

	/* determine the screen Y */
	scry = y;
	if (FBZMODE_Y_ORIGIN(v->reg[fbzMode].u))
		scry = (v->fbi.yorigin - y) & 0x3ff;

	/* fill this RGB row */
	if (FBZMODE_RGB_BUFFER_MASK(v->reg[fbzMode].u))
	{
		const UINT16 *ditherow = &extra_dither[(y & 3) * 4];
		UINT64 expanded = *(UINT64 *)ditherow;
		UINT16 *dest = (UINT16 *)destbase + scry * v->fbi.rowpixels;

		for (x = startx; x < stopx && (x & 3) != 0; x++)
			dest[x] = ditherow[x & 3];
		for ( ; x < (stopx & ~3); x += 4)
			*(UINT64 *)&dest[x] = expanded;
		for ( ; x < stopx; x++)
			dest[x] = ditherow[x & 3];
		stats.pixels_out += stopx - startx;
	}

	/* fill this dest buffer row */
	if (FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) && v->fbi.auxoffs != (UINT32)(~0))
	{
		UINT16 color = (UINT16)(v->reg[zaColor].u & 0xffff);
		UINT64 expanded = ((UINT64)color << 48) | ((UINT64)color << 32) | (color << 16) | color;
		UINT16 *dest = (UINT16 *)(v->fbi.ram + v->fbi.auxoffs) + scry * v->fbi.rowpixels;

		if (v->fbi.auxoffs + 2 * (scry * v->fbi.rowpixels + stopx) >= v->fbi.mask) {
			stopx = (v->fbi.mask - v->fbi.auxoffs) / 2 - scry * v->fbi.rowpixels;
			if ((stopx < 0) || (stopx < startx)) return;
		}

		for (x = startx; x < stopx && (x & 3) != 0; x++)
			dest[x] = color;
		for ( ; x < (stopx & ~3); x += 4)
			*(UINT64 *)&dest[x] = expanded;
		for ( ; x < stopx; x++)
			dest[x] = color;
	}
}


/*************************************
 *
 *  Common initialization
 *
 *************************************/

static void init_fbi([[maybe_unused]] voodoo_state* v, fbi_state* f, int fbmem)
{
	/* allocate frame buffer RAM and set pointers */
	assert(fbmem >= 1); //VOODOO: invalid frame buffer memory size requested
	f->ram = (UINT8*)malloc(fbmem);
	f->mask = (UINT32)(fbmem - 1);
	f->rgboffs[0] = f->rgboffs[1] = f->rgboffs[2] = 0;
	f->auxoffs = (UINT32)(~0);

	/* default to 0x0 */
	f->frontbuf = 0;
	f->backbuf = 1;
	f->width = 640;
	f->height = 480;
	//f->xoffs = 0;
	//f->yoffs = 0;

	//f->vsyncscan = 0;

	/* init the pens */
	//for (UINT8 pen = 0; pen < 32; pen++)
	//	v->fbi.clut[pen] = MAKE_ARGB(pen, pal5bit(pen), pal5bit(pen), pal5bit(pen));
	//v->fbi.clut[32] = MAKE_ARGB(32,0xff,0xff,0xff);

	/* allocate a VBLANK timer */
	f->vblank = false;

	/* initialize the memory FIFO */
	f->fifo.size = 0;

	/* set the fog delta mask */
	f->fogdelta_mask = (vtype < VOODOO_2) ? 0xff : 0xfc;

	f->yorigin = 0;

	f->sverts = 0;

	memset(&f->lfb_stats, 0, sizeof(f->lfb_stats));
	memset(&f->fogblend, 0, sizeof(f->fogblend));
	memset(&f->fogdelta, 0, sizeof(f->fogdelta));
}

static void init_tmu_shared(tmu_shared_state *s)
{
	int val;

	/* build static 8-bit texel tables */
	for (val = 0; val < 256; val++)
	{
		int r, g, b, a;

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
		int r, g, b, a;

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

static void init_tmu(voodoo_state *v, tmu_state *t, voodoo_reg *reg, int tmem)
{
	if (tmem <= 1) E_Exit("VOODOO: invalid texture buffer memory size requested");
	/* allocate texture RAM */
	t->ram = (UINT8*)malloc(tmem);
	t->mask = (UINT32)(tmem - 1);
	t->reg = reg;
	t->regdirty = true;
	t->bilinear_mask = (vtype >= VOODOO_2) ? 0xff : 0xf0;

	/* mark the NCC tables dirty and configure their registers */
	t->ncc[0].dirty = t->ncc[1].dirty = true;
	t->ncc[0].reg = &t->reg[nccTable+0];
	t->ncc[1].reg = &t->reg[nccTable+12];

	/* create pointers to all the tables */
	t->texel[0] = v->tmushare.rgb332;
	t->texel[1] = t->ncc[0].texel;
	t->texel[2] = v->tmushare.alpha8;
	t->texel[3] = v->tmushare.int8;
	t->texel[4] = v->tmushare.ai44;
	t->texel[5] = t->palette;
	t->texel[6] = (vtype >= VOODOO_2) ? t->palettea : NULL;
	t->texel[7] = NULL;
	t->texel[8] = v->tmushare.rgb332;
	t->texel[9] = t->ncc[0].texel;
	t->texel[10] = v->tmushare.rgb565;
	t->texel[11] = v->tmushare.argb1555;
	t->texel[12] = v->tmushare.argb4444;
	t->texel[13] = v->tmushare.int8;
	t->texel[14] = t->palette;
	t->texel[15] = NULL;
	t->lookup = t->texel[0];

	/* attach the palette to NCC table 0 */
	t->ncc[0].palette = t->palette;
	t->ncc[0].palettea = (vtype >= VOODOO_2) ? t->palettea : NULL;

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

static void voodoo_swap_buffers(voodoo_state *v)
{
	//if (LOG_VBLANK_SWAP) LOG(LOG_VOODOO,LOG_WARN)("--- swap_buffers @ %d\n", video_screen_get_vpos(v->screen));

#ifdef C_ENABLE_VOODOO_OPENGL
	if (v->ogl && v->active) {
		voodoo_ogl_swap_buffer();
		return;
	}
#endif

	/* keep a history of swap intervals */
	v->reg[fbiSwapHistory].u = (v->reg[fbiSwapHistory].u << 4);

	/* rotate the buffers */
	if (vtype < VOODOO_2 || !v->fbi.vblank_dont_swap)
	{
		if (v->fbi.rgboffs[2] == (UINT32)(~0))
		{
			v->fbi.frontbuf = (UINT8)(1 - v->fbi.frontbuf);
			v->fbi.backbuf = (UINT8)(1 - v->fbi.frontbuf);
		}
		else
		{
			v->fbi.frontbuf = (v->fbi.frontbuf + 1) % 3;
			v->fbi.backbuf = (v->fbi.frontbuf + 1) % 3;
		}
	}
}


/*************************************
 *
 *  Recompute video memory layout
 *
 *************************************/

static void recompute_video_memory(voodoo_state *v)
{
	UINT32 buffer_pages = FBIINIT2_VIDEO_BUFFER_OFFSET(v->reg[fbiInit2].u);
	UINT32 fifo_start_page = FBIINIT4_MEMORY_FIFO_START_ROW(v->reg[fbiInit4].u);
	UINT32 fifo_last_page = FBIINIT4_MEMORY_FIFO_STOP_ROW(v->reg[fbiInit4].u);
	UINT32 memory_config;
	int buf;

	/* memory config is determined differently between V1 and V2 */
	memory_config = FBIINIT2_ENABLE_TRIPLE_BUF(v->reg[fbiInit2].u);
	if (vtype == VOODOO_2 && memory_config == 0)
		memory_config = FBIINIT5_BUFFER_ALLOCATION(v->reg[fbiInit5].u);

	/* tiles are 64x16/32; x_tiles specifies how many half-tiles */
	v->fbi.tile_width = (vtype < VOODOO_2) ? 64 : 32;
	v->fbi.tile_height = (vtype < VOODOO_2) ? 16 : 32;
	v->fbi.x_tiles = FBIINIT1_X_VIDEO_TILES(v->reg[fbiInit1].u);
	if (vtype == VOODOO_2)
	{
		v->fbi.x_tiles = (v->fbi.x_tiles << 1) |
						(FBIINIT1_X_VIDEO_TILES_BIT5(v->reg[fbiInit1].u) << 5) |
						(FBIINIT6_X_VIDEO_TILES_BIT0(v->reg[fbiInit6].u));
	}
	v->fbi.rowpixels = v->fbi.tile_width * v->fbi.x_tiles;

	//logerror("VOODOO.%d.VIDMEM: buffer_pages=%X  fifo=%X-%X  tiles=%X  rowpix=%d\n", v->index, buffer_pages, fifo_start_page, fifo_last_page, v->fbi.x_tiles, v->fbi.rowpixels);

	/* first RGB buffer always starts at 0 */
	v->fbi.rgboffs[0] = 0;

	/* second RGB buffer starts immediately afterwards */
	v->fbi.rgboffs[1] = buffer_pages * 0x1000;

	/* remaining buffers are based on the config */
	switch (memory_config)
	{
	case 3: /* reserved */
		LOG(LOG_VOODOO,LOG_WARN)("VOODOO.ERROR:Unexpected memory configuration in recompute_video_memory!");
		[[fallthrough]];

	case 0:	/* 2 color buffers, 1 aux buffer */
		v->fbi.rgboffs[2] = (UINT32)(~0);
		v->fbi.auxoffs = 2 * buffer_pages * 0x1000;
		break;

	case 1:	/* 3 color buffers, 0 aux buffers */
		v->fbi.rgboffs[2] = 2 * buffer_pages * 0x1000;
		v->fbi.auxoffs = (UINT32)(~0);
		break;

	case 2:	/* 3 color buffers, 1 aux buffers */
		v->fbi.rgboffs[2] = 2 * buffer_pages * 0x1000;
		v->fbi.auxoffs = 3 * buffer_pages * 0x1000;
		break;
	}

	/* clamp the RGB buffers to video memory */
	for (buf = 0; buf < 3; buf++)
		if (v->fbi.rgboffs[buf] != (UINT32)(~0) && v->fbi.rgboffs[buf] > v->fbi.mask)
			v->fbi.rgboffs[buf] = v->fbi.mask;

	/* clamp the aux buffer to video memory */
	if (v->fbi.auxoffs != (UINT32)(~0) && v->fbi.auxoffs > v->fbi.mask)
		v->fbi.auxoffs = v->fbi.mask;

	/* compute the memory FIFO location and size */
	if (fifo_last_page > v->fbi.mask / 0x1000)
		fifo_last_page = v->fbi.mask / 0x1000;

	/* is it valid and enabled? */
	if (fifo_start_page <= fifo_last_page && FBIINIT0_ENABLE_MEMORY_FIFO(v->reg[fbiInit0].u))
	{
		v->fbi.fifo.size = (fifo_last_page + 1 - fifo_start_page) * 0x1000 / 4;
		if (v->fbi.fifo.size > 65536*2)
			v->fbi.fifo.size = 65536*2;
	}
	else	/* if not, disable the FIFO */
	{
		v->fbi.fifo.size = 0;
	}

	/* reset our front/back buffers if they are out of range */
	if (v->fbi.rgboffs[2] == (UINT32)(~0))
	{
		if (v->fbi.frontbuf == 2)
			v->fbi.frontbuf = 0;
		if (v->fbi.backbuf == 2)
			v->fbi.backbuf = 0;
	}
}


/*************************************
 *
 *  NCC table management
 *
 *************************************/

static void ncc_table_write(ncc_table *n, UINT32 regnum, UINT32 data)
{
	/* I/Q entries reference the palette if the high bit is set */
	if (regnum >= 4 && (data & 0x80000000) && n->palette)
	{
		UINT32 index = ((data >> 23) & 0xfe) | (regnum & 1);

		rgb_t palette_entry = 0xff000000 | data;

		if (n->palette[index] != palette_entry) {
			/* set the ARGB for this palette index */
			n->palette[index] = palette_entry;
#ifdef C_ENABLE_VOODOO_OPENGL
			v->ogl_palette_changed = true;
#endif
		}

		/* if we have an ARGB palette as well, compute its value */
		if (n->palettea)
		{
			UINT32 a = ((data >> 16) & 0xfc) | ((data >> 22) & 0x03);
			UINT32 r = ((data >> 10) & 0xfc) | ((data >> 16) & 0x03);
			UINT32 g = ((data >>  4) & 0xfc) | ((data >> 10) & 0x03);
			UINT32 b = ((data <<  2) & 0xfc) | ((data >>  4) & 0x03);
			n->palettea[index] = MAKE_ARGB(a, r, g, b);
		}

		/* this doesn't dirty the table or go to the registers, so bail */
		return;
	}

	/* if the register matches, don't update */
	if (data == n->reg[regnum].u)
		return;
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
		n->ir[regnum] = (INT32)(data <<  5) >> 23;
		n->ig[regnum] = (INT32)(data << 14) >> 23;
		n->ib[regnum] = (INT32)(data << 23) >> 23;
	}

	/* the final four entries are the Q RGB values */
	else
	{
		regnum &= 3;
		n->qr[regnum] = (INT32)(data <<  5) >> 23;
		n->qg[regnum] = (INT32)(data << 14) >> 23;
		n->qb[regnum] = (INT32)(data << 23) >> 23;
	}

	/* mark the table dirty */
	n->dirty = true;
}

static void ncc_table_update(ncc_table *n)
{
	int r, g, b, i;

	/* generte all 256 possibilities */
	for (i = 0; i < 256; i++)
	{
		int vi = (i >> 2) & 0x03;
		int vq = (i >> 0) & 0x03;

		/* start with the intensity */
		r = g = b = n->y[(i >> 4) & 0x0f];

		/* add the coloring */
		r += n->ir[vi] + n->qr[vq];
		g += n->ig[vi] + n->qg[vq];
		b += n->ib[vi] + n->qb[vq];

		/* clamp */
		CLAMP(r, 0, 255);
		CLAMP(g, 0, 255);
		CLAMP(b, 0, 255);

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

static void dacdata_w(dac_state *d, UINT8 regnum, UINT8 data)
{
	d->reg[regnum] = data;
}

static void dacdata_r(dac_state *d, UINT8 regnum)
{
	UINT8 result = 0xff;

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
	UINT32 base;
	int lod;

	/* extract LOD parameters */
	t->lodmin = TEXLOD_LODMIN(t->reg[tLOD].u) << 6;
	t->lodmax = TEXLOD_LODMAX(t->reg[tLOD].u) << 6;
	t->lodbias = (INT8)(TEXLOD_LODBIAS(t->reg[tLOD].u) << 2) << 4;

	/* determine which LODs are present */
	t->lodmask = 0x1ff;
	if (TEXLOD_LOD_TSPLIT(t->reg[tLOD].u))
	{
		if (!TEXLOD_LOD_ODD(t->reg[tLOD].u))
			t->lodmask = 0x155;
		else
			t->lodmask = 0x0aa;
	}

	/* determine base texture width/height */
	t->wmask = t->hmask = 0xff;
	if (TEXLOD_LOD_S_IS_WIDER(t->reg[tLOD].u))
		t->hmask >>= TEXLOD_LOD_ASPECT(t->reg[tLOD].u);
	else
		t->wmask >>= TEXLOD_LOD_ASPECT(t->reg[tLOD].u);

	/* determine the bpp of the texture */
	bppscale = TEXMODE_FORMAT(t->reg[textureMode].u) >> 3;

	/* start with the base of LOD 0 */
	if (t->texaddr_shift == 0 && (t->reg[texBaseAddr].u & 1))
		LOG(LOG_VOODOO,LOG_WARN)("Tiled texture\n");
	base = (t->reg[texBaseAddr].u & t->texaddr_mask) << t->texaddr_shift;
	t->lodoffset[0] = base & t->mask;

	/* LODs 1-3 are different depending on whether we are in multitex mode */
	/* Several Voodoo 2 games leave the upper bits of TLOD == 0xff, meaning we think */
	/* they want multitex mode when they really don't -- disable for now */
	if (0)//TEXLOD_TMULTIBASEADDR(t->reg[tLOD].u))
	{
		base = (t->reg[texBaseAddr_1].u & t->texaddr_mask) << t->texaddr_shift;
		t->lodoffset[1] = base & t->mask;
		base = (t->reg[texBaseAddr_2].u & t->texaddr_mask) << t->texaddr_shift;
		t->lodoffset[2] = base & t->mask;
		base = (t->reg[texBaseAddr_3_8].u & t->texaddr_mask) << t->texaddr_shift;
		t->lodoffset[3] = base & t->mask;
	}
	else
	{
		if (t->lodmask & (1 << 0))
			base += (((t->wmask >> 0) + 1) * ((t->hmask >> 0) + 1)) << bppscale;
		t->lodoffset[1] = base & t->mask;
		if (t->lodmask & (1 << 1))
			base += (((t->wmask >> 1) + 1) * ((t->hmask >> 1) + 1)) << bppscale;
		t->lodoffset[2] = base & t->mask;
		if (t->lodmask & (1 << 2))
			base += (((t->wmask >> 2) + 1) * ((t->hmask >> 2) + 1)) << bppscale;
		t->lodoffset[3] = base & t->mask;
	}

	/* remaining LODs make sense */
	for (lod = 4; lod <= 8; lod++)
	{
		if (t->lodmask & (1 << (lod - 1)))
		{
			UINT32 size = ((t->wmask >> (lod - 1)) + 1) * ((t->hmask >> (lod - 1)) + 1);
			if (size < 4) size = 4;
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
	t->detailbias = (INT8)(TEXDETAIL_DETAIL_BIAS(t->reg[tDetail].u) << 2) << 6;
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
	INT64 texdx, texdy;
	INT32 lodbase;

	/* if the texture parameters are dirty, update them */
	if (t->regdirty)
	{
		recompute_texture_params(t);

		/* ensure that the NCC tables are up to date */
		if ((TEXMODE_FORMAT(t->reg[textureMode].u) & 7) == 1)
		{
			ncc_table *n = &t->ncc[TEXMODE_NCC_TABLE_SELECT(t->reg[textureMode].u)];
			t->texel[1] = t->texel[9] = n->texel;
			if (n->dirty)
				ncc_table_update(n);
		}
	}

	/* compute (ds^2 + dt^2) in both X and Y as 28.36 numbers */
	texdx = (INT64)(t->dsdx >> 14) * (INT64)(t->dsdx >> 14) + (INT64)(t->dtdx >> 14) * (INT64)(t->dtdx >> 14);
	texdy = (INT64)(t->dsdy >> 14) * (INT64)(t->dsdy >> 14) + (INT64)(t->dtdy >> 14) * (INT64)(t->dtdy >> 14);

	/* pick whichever is larger and shift off some high bits -> 28.20 */
	if (texdx < texdy)
		texdx = texdy;
	texdx >>= 16;

	/* use our fast reciprocal/log on this value; it expects input as a */
	/* 16.32 number, and returns the log of the reciprocal, so we have to */
	/* adjust the result: negative to get the log of the original value */
	/* plus 12 to account for the extra exponent, and divided by 2 to */
	/* get the log of the square root of texdx */
	(void)fast_reciplog(texdx, &lodbase);
	t->lodbasetemp = (-lodbase + (12 << 8)) / 2;
}

static inline INT32 round_coordinate(float value)
{
	return (value > 0 ? (INT32)(value + .4999999f) : (INT32)(value - .5f));
	//INT32 result = (INT32)floor(value);
	//return result + (value - (float)result > 0.5f);
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

static void accumulate_statistics(voodoo_state *v, const stats_block *stats)
{
	/* apply internal voodoo statistics */
	v->reg[fbiPixelsIn].u += stats->pixels_in;
	v->reg[fbiPixelsOut].u += stats->pixels_out;
	v->reg[fbiChromaFail].u += stats->chroma_fail;
	v->reg[fbiZfuncFail].u += stats->zfunc_fail;
	v->reg[fbiAfuncFail].u += stats->afunc_fail;
}

static void update_statistics(voodoo_state *v, bool accumulate)
{
	/* accumulate/reset statistics from all units */
	for (size_t i = 0; i != TRIANGLE_WORKERS; i++)
	{
		if (accumulate)
			accumulate_statistics(v, &v->thread_stats[i]);
	}
	memset(v->thread_stats, 0, sizeof(v->thread_stats));

	/* accumulate/reset statistics from the LFB */
	if (accumulate)
		accumulate_statistics(v, &v->fbi.lfb_stats);
	memset(&v->fbi.lfb_stats, 0, sizeof(v->fbi.lfb_stats));
}


/***************************************************************************
    COMMAND HANDLERS
***************************************************************************/

static void triangle_worker_work(triangle_worker& tworker, INT32 worktstart, INT32 worktend)
{
	/* determine the number of TMUs involved */
	UINT32 tmus = 0, texmode0 = 0, texmode1 = 0;
	if (!FBIINIT3_DISABLE_TMUS(v->reg[fbiInit3].u) && FBZCP_TEXTURE_ENABLE(v->reg[fbzColorPath].u))
	{
		tmus = 1;
		texmode0 = v->tmu[0].reg[textureMode].u;
		if (v->chipmask & 0x04)
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
	poly_vertex v1 = tworker.v1, v2 = tworker.v2, v3 = tworker.v3;
	float dxdy_v1v2 = (v2.y == v1.y) ? 0.0f : (v2.x - v1.x) / (v2.y - v1.y);
	float dxdy_v1v3 = (v3.y == v1.y) ? 0.0f : (v3.x - v1.x) / (v3.y - v1.y);
	float dxdy_v2v3 = (v3.y == v2.y) ? 0.0f : (v3.x - v2.x) / (v3.y - v2.y);

	stats_block my_stats = {};
	INT32 from = tworker.totalpix * worktstart / TRIANGLE_WORKERS;
	INT32 to   = tworker.totalpix * worktend   / TRIANGLE_WORKERS;
	for (INT32 curscan = tworker.v1y, scanend = tworker.v3y, sumpix = 0, lastsum = 0; curscan != scanend && lastsum < to; lastsum = sumpix, curscan++)
	{
		float fully = (float)(curscan) + 0.5f;
		float startx = v1.x + (fully - v1.y) * dxdy_v1v3;

		/* compute the ending X based on which part of the triangle we're in */
		float stopx = (fully < v2.y ? (v1.x + (fully - v1.y) * dxdy_v1v2) : (v2.x + (fully - v2.y) * dxdy_v2v3));

		/* clamp to full pixels */
		poly_extent extent;
		extent.startx = round_coordinate(startx);
		extent.stopx = round_coordinate(stopx);

		/* force start < stop */
		if (extent.startx >= extent.stopx)
		{
			if (extent.startx == extent.stopx) continue;
			std::swap(extent.startx, extent.stopx);
		}

		sumpix += (extent.stopx - extent.startx);

		if (sumpix <= from)
			continue;
		if (lastsum < from)
			extent.startx += (from - lastsum);
		if (sumpix > to)
			extent.stopx -= (sumpix - to);

		raster_generic(v, tmus, texmode0, texmode1, tworker.drawbuf, curscan, &extent, my_stats);
	}
	sum_statistics(&v->thread_stats[worktstart], &my_stats);
}

static int triangle_worker_thread_func(void* p)
{
	triangle_worker& tworker = v->tworker;
	for (INT32 tnum = (INT32)(size_t)p; tworker.threads_active;)
	{
		SDL_SemWait(tworker.sembegin[tnum]);
		if (tworker.threads_active)
			triangle_worker_work(tworker, tnum, tnum + 1);
		tworker.done[tnum] = true;
	}
	return 0;
}

static void triangle_worker_shutdown(triangle_worker& tworker)
{
	if (!tworker.threads_active) return;
	tworker.threads_active = false;
	for (size_t i = 0; i != TRIANGLE_THREADS; i++) tworker.done[i] = false;
	for (size_t i = 0; i != TRIANGLE_THREADS; i++) SDL_SemPost(tworker.sembegin[i]);
	recheckdone:
	for (size_t i = 0; i != TRIANGLE_THREADS; i++) if (!tworker.done[i]) goto recheckdone;
	for (size_t i = 0; i != TRIANGLE_THREADS; i++) SDL_DestroySemaphore(tworker.sembegin[i]);
}

static void triangle_worker_run(triangle_worker& tworker)
{
	if (!tworker.use_threads)
	{
		// do not use threaded calculation
		tworker.totalpix = 0xFFFFFFF;
		triangle_worker_work(tworker, 0, TRIANGLE_WORKERS);
		return;
	}

	/* compute the slopes for each portion of the triangle */
	poly_vertex v1 = tworker.v1, v2 = tworker.v2, v3 = tworker.v3;
	float dxdy_v1v2 = (v2.y == v1.y) ? 0.0f : (v2.x - v1.x) / (v2.y - v1.y);
	float dxdy_v1v3 = (v3.y == v1.y) ? 0.0f : (v3.x - v1.x) / (v3.y - v1.y);
	float dxdy_v2v3 = (v3.y == v2.y) ? 0.0f : (v3.x - v2.x) / (v3.y - v2.y);

	INT32 pixsum = 0;
	for (INT32 curscan = tworker.v1y, scanend = tworker.v3y; curscan != scanend; curscan++)
	{
		float fully = (float)(curscan) + 0.5f;
		float startx = v1.x + (fully - v1.y) * dxdy_v1v3;

		/* compute the ending X based on which part of the triangle we're in */
		float stopx = (fully < v2.y ? (v1.x + (fully - v1.y) * dxdy_v1v2) : (v2.x + (fully - v2.y) * dxdy_v2v3));

		/* clamp to full pixels */
		INT32 istartx = round_coordinate(startx), istopx = round_coordinate(stopx);

		/* force start < stop */
		pixsum += (istartx > istopx ? istartx - istopx : istopx - istartx);
	}
	tworker.totalpix = pixsum;

	// Don't wake up threads for just a few pixels
	if (tworker.totalpix <= 200)
	{
		triangle_worker_work(tworker, 0, TRIANGLE_WORKERS);
		return;
	}

	if (!tworker.threads_active)
	{
		tworker.threads_active = true;
		for (size_t i = 0; i != TRIANGLE_THREADS; i++) tworker.sembegin[i] = SDL_CreateSemaphore(0);
		for (size_t i = 0; i != TRIANGLE_THREADS; i++)
			SDL_CreateThread(triangle_worker_thread_func,
			                 "voodoo",
			                 (void*)i);
	}

	for (size_t i = 0; i != TRIANGLE_THREADS; i++) tworker.done[i] = false;
	for (size_t i = 0; i != TRIANGLE_THREADS; i++) SDL_SemPost(tworker.sembegin[i]);
	triangle_worker_work(tworker, TRIANGLE_THREADS, TRIANGLE_WORKERS);
	recheckdone:
	for (size_t i = 0; i != TRIANGLE_THREADS; i++) if (!tworker.done[i]) goto recheckdone;
}

/*-------------------------------------------------
    triangle - execute the 'triangle'
    command
-------------------------------------------------*/
static void triangle(voodoo_state *v)
{
	/* determine the number of TMUs involved */
	int texcount = 0;
	if (!FBIINIT3_DISABLE_TMUS(v->reg[fbiInit3].u) && FBZCP_TEXTURE_ENABLE(v->reg[fbzColorPath].u))
	{
		texcount = 1;
		if (v->chipmask & 0x04)
			texcount = 2;
	}

	/* perform subpixel adjustments */
	if (
#ifdef C_ENABLE_VOODOO_OPENGL
		!v->ogl &&
#endif
		FBZCP_CCA_SUBPIXEL_ADJUST(v->reg[fbzColorPath].u))
	{
		INT32 dx = 8 - (v->fbi.ax & 15);
		INT32 dy = 8 - (v->fbi.ay & 15);

		/* adjust iterated R,G,B,A and W/Z */
		v->fbi.startr += (dy * v->fbi.drdy + dx * v->fbi.drdx) >> 4;
		v->fbi.startg += (dy * v->fbi.dgdy + dx * v->fbi.dgdx) >> 4;
		v->fbi.startb += (dy * v->fbi.dbdy + dx * v->fbi.dbdx) >> 4;
		v->fbi.starta += (dy * v->fbi.dady + dx * v->fbi.dadx) >> 4;
		v->fbi.startw += (dy * v->fbi.dwdy + dx * v->fbi.dwdx) >> 4;
		v->fbi.startz += mul_32x32_shift(dy, v->fbi.dzdy, 4) + mul_32x32_shift(dx, v->fbi.dzdx, 4);

		/* adjust iterated W/S/T for TMU 0 */
		if (texcount >= 1)
		{
			v->tmu[0].startw += (dy * v->tmu[0].dwdy + dx * v->tmu[0].dwdx) >> 4;
			v->tmu[0].starts += (dy * v->tmu[0].dsdy + dx * v->tmu[0].dsdx) >> 4;
			v->tmu[0].startt += (dy * v->tmu[0].dtdy + dx * v->tmu[0].dtdx) >> 4;

			/* adjust iterated W/S/T for TMU 1 */
			if (texcount >= 2)
			{
				v->tmu[1].startw += (dy * v->tmu[1].dwdy + dx * v->tmu[1].dwdx) >> 4;
				v->tmu[1].starts += (dy * v->tmu[1].dsdy + dx * v->tmu[1].dsdx) >> 4;
				v->tmu[1].startt += (dy * v->tmu[1].dtdy + dx * v->tmu[1].dtdx) >> 4;
			}
		}
	}

	/* fill in the vertex data */
	poly_vertex vert[3];
	vert[0].x = (float)v->fbi.ax * (1.0f / 16.0f);
	vert[0].y = (float)v->fbi.ay * (1.0f / 16.0f);
	vert[1].x = (float)v->fbi.bx * (1.0f / 16.0f);
	vert[1].y = (float)v->fbi.by * (1.0f / 16.0f);
	vert[2].x = (float)v->fbi.cx * (1.0f / 16.0f);
	vert[2].y = (float)v->fbi.cy * (1.0f / 16.0f);

#ifdef C_ENABLE_VOODOO_OPENGL
	/* find a rasterizer that matches our current state */
	//    setup and create the work item
	poly_extra_data extra;
	/* fill in the extra data */
	extra.state = v;

	raster_info *info  = find_rasterizer(v, texcount);
	extra.info = info;

	/* fill in triangle parameters */
	extra.ax = v->fbi.ax;
	extra.ay = v->fbi.ay;
	extra.startr = v->fbi.startr;
	extra.startg = v->fbi.startg;
	extra.startb = v->fbi.startb;
	extra.starta = v->fbi.starta;
	extra.startz = v->fbi.startz;
	extra.startw = v->fbi.startw;
	extra.drdx = v->fbi.drdx;
	extra.dgdx = v->fbi.dgdx;
	extra.dbdx = v->fbi.dbdx;
	extra.dadx = v->fbi.dadx;
	extra.dzdx = v->fbi.dzdx;
	extra.dwdx = v->fbi.dwdx;
	extra.drdy = v->fbi.drdy;
	extra.dgdy = v->fbi.dgdy;
	extra.dbdy = v->fbi.dbdy;
	extra.dady = v->fbi.dady;
	extra.dzdy = v->fbi.dzdy;
	extra.dwdy = v->fbi.dwdy;

	/* fill in texture 0 parameters */
	if (texcount > 0)
	{
		extra.starts0 = v->tmu[0].starts;
		extra.startt0 = v->tmu[0].startt;
		extra.startw0 = v->tmu[0].startw;
		extra.ds0dx = v->tmu[0].dsdx;
		extra.dt0dx = v->tmu[0].dtdx;
		extra.dw0dx = v->tmu[0].dwdx;
		extra.ds0dy = v->tmu[0].dsdy;
		extra.dt0dy = v->tmu[0].dtdy;
		extra.dw0dy = v->tmu[0].dwdy;
		extra.lodbase0 = v->tmu[0].lodbasetemp;

		/* fill in texture 1 parameters */
		if (texcount > 1)
		{
			extra.starts1 = v->tmu[1].starts;
			extra.startt1 = v->tmu[1].startt;
			extra.startw1 = v->tmu[1].startw;
			extra.ds1dx = v->tmu[1].dsdx;
			extra.dt1dx = v->tmu[1].dtdx;
			extra.dw1dx = v->tmu[1].dwdx;
			extra.ds1dy = v->tmu[1].dsdy;
			extra.dt1dy = v->tmu[1].dtdy;
			extra.dw1dy = v->tmu[1].dwdy;
			extra.lodbase1 = v->tmu[1].lodbasetemp;
		}
	}

	extra.texcount = texcount;
	extra.r_fbzColorPath = v->reg[fbzColorPath].u;
	extra.r_fbzMode = v->reg[fbzMode].u;
	extra.r_alphaMode = v->reg[alphaMode].u;
	extra.r_fogMode = v->reg[fogMode].u;
	extra.r_textureMode0 = v->tmu[0].reg[textureMode].u;
	if (v->tmu[1].ram != NULL) extra.r_textureMode1 = v->tmu[1].reg[textureMode].u;

#ifdef C_ENABLE_VOODOO_DEBUG
	info->polys++;
#endif

	if (v->ogl_palette_changed && v->ogl && v->active) {
		voodoo_ogl_invalidate_paltex();
		v->ogl_palette_changed = false;
	}

	if (v->ogl && v->active) {
		if (extra.info)
			voodoo_ogl_draw_triangle(&extra);
		return;
	}
#endif

	/* first sort by Y */
	const poly_vertex *v1 = &vert[0], *v2 = &vert[1], *v3 = &vert[2];
	if (v2->y < v1->y)
	{
		std::swap(v1, v2);
	}
	if (v3->y < v2->y)
	{
		std::swap(v2, v3);
		if (v2->y < v1->y)
			std::swap(v1, v2);
	}

	/* compute some integral X/Y vertex values */
	INT32 v1y = round_coordinate(v1->y);
	INT32 v3y = round_coordinate(v3->y);

	/* clip coordinates */
	if (v3y <= v1y)
		return;

	/* determine the draw buffer */
	UINT16 *drawbuf;
	switch (FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u))
	{
		case 0:		/* front buffer */
			drawbuf = (UINT16 *)(v->fbi.ram + v->fbi.rgboffs[v->fbi.frontbuf]);
			break;

		case 1:		/* back buffer */
			drawbuf = (UINT16 *)(v->fbi.ram + v->fbi.rgboffs[v->fbi.backbuf]);
			break;

		default:	/* reserved */
			return;
	}

	/* determine the number of TMUs involved */
	if (texcount >= 1)
	{
		prepare_tmu(&v->tmu[0]);
		if (texcount >= 2)
			prepare_tmu(&v->tmu[1]);
	}

	triangle_worker& tworker = v->tworker;
	tworker.v1 = *v1, tworker.v2 = *v2, tworker.v3 = *v3;
	tworker.drawbuf = drawbuf;
	tworker.v1y = v1y;
	tworker.v3y = v3y;
	triangle_worker_run(tworker);

	/* update stats */
	v->reg[fbiTrianglesOut].u++;
}

/*-------------------------------------------------
    begin_triangle - execute the 'beginTri'
    command
-------------------------------------------------*/
static void begin_triangle(voodoo_state *v)
{
	setup_vertex *sv = &v->fbi.svert[2];

	/* extract all the data from registers */
	sv->x = v->reg[sVx].f;
	sv->y = v->reg[sVy].f;
	sv->wb = v->reg[sWb].f;
	sv->w0 = v->reg[sWtmu0].f;
	sv->s0 = v->reg[sS_W0].f;
	sv->t0 = v->reg[sT_W0].f;
	sv->w1 = v->reg[sWtmu1].f;
	sv->s1 = v->reg[sS_Wtmu1].f;
	sv->t1 = v->reg[sT_Wtmu1].f;
	sv->a = v->reg[sAlpha].f;
	sv->r = v->reg[sRed].f;
	sv->g = v->reg[sGreen].f;
	sv->b = v->reg[sBlue].f;

	/* spread it across all three verts and reset the count */
	v->fbi.svert[0] = v->fbi.svert[1] = v->fbi.svert[2];
	v->fbi.sverts = 1;
}

/*-------------------------------------------------
    setup_and_draw_triangle - process the setup
    parameters and render the triangle
-------------------------------------------------*/
static void setup_and_draw_triangle(voodoo_state *v)
{
	float dx1, dy1, dx2, dy2;
	float divisor, tdiv;

	/* grab the X/Ys at least */
	v->fbi.ax = (INT16)(v->fbi.svert[0].x * 16.0);
	v->fbi.ay = (INT16)(v->fbi.svert[0].y * 16.0);
	v->fbi.bx = (INT16)(v->fbi.svert[1].x * 16.0);
	v->fbi.by = (INT16)(v->fbi.svert[1].y * 16.0);
	v->fbi.cx = (INT16)(v->fbi.svert[2].x * 16.0);
	v->fbi.cy = (INT16)(v->fbi.svert[2].y * 16.0);

	/* compute the divisor */
	divisor = 1.0f / ((v->fbi.svert[0].x - v->fbi.svert[1].x) * (v->fbi.svert[0].y - v->fbi.svert[2].y) -
					  (v->fbi.svert[0].x - v->fbi.svert[2].x) * (v->fbi.svert[0].y - v->fbi.svert[1].y));

	/* backface culling */
	if (v->reg[sSetupMode].u & 0x20000)
	{
		int culling_sign = (v->reg[sSetupMode].u >> 18) & 1;
		int divisor_sign = (divisor < 0);

		/* if doing strips and ping pong is enabled, apply the ping pong */
		if ((v->reg[sSetupMode].u & 0x90000) == 0x00000)
			culling_sign ^= (v->fbi.sverts - 3) & 1;

		/* if our sign matches the culling sign, we're done for */
		if (divisor_sign == culling_sign)
			return;
	}

	/* compute the dx/dy values */
	dx1 = v->fbi.svert[0].y - v->fbi.svert[2].y;
	dx2 = v->fbi.svert[0].y - v->fbi.svert[1].y;
	dy1 = v->fbi.svert[0].x - v->fbi.svert[1].x;
	dy2 = v->fbi.svert[0].x - v->fbi.svert[2].x;

	/* set up R,G,B */
	tdiv = divisor * 4096.0f;
	if (v->reg[sSetupMode].u & (1 << 0))
	{
		v->fbi.startr = (INT32)(v->fbi.svert[0].r * 4096.0f);
		v->fbi.drdx = (INT32)(((v->fbi.svert[0].r - v->fbi.svert[1].r) * dx1 - (v->fbi.svert[0].r - v->fbi.svert[2].r) * dx2) * tdiv);
		v->fbi.drdy = (INT32)(((v->fbi.svert[0].r - v->fbi.svert[2].r) * dy1 - (v->fbi.svert[0].r - v->fbi.svert[1].r) * dy2) * tdiv);
		v->fbi.startg = (INT32)(v->fbi.svert[0].g * 4096.0f);
		v->fbi.dgdx = (INT32)(((v->fbi.svert[0].g - v->fbi.svert[1].g) * dx1 - (v->fbi.svert[0].g - v->fbi.svert[2].g) * dx2) * tdiv);
		v->fbi.dgdy = (INT32)(((v->fbi.svert[0].g - v->fbi.svert[2].g) * dy1 - (v->fbi.svert[0].g - v->fbi.svert[1].g) * dy2) * tdiv);
		v->fbi.startb = (INT32)(v->fbi.svert[0].b * 4096.0f);
		v->fbi.dbdx = (INT32)(((v->fbi.svert[0].b - v->fbi.svert[1].b) * dx1 - (v->fbi.svert[0].b - v->fbi.svert[2].b) * dx2) * tdiv);
		v->fbi.dbdy = (INT32)(((v->fbi.svert[0].b - v->fbi.svert[2].b) * dy1 - (v->fbi.svert[0].b - v->fbi.svert[1].b) * dy2) * tdiv);
	}

	/* set up alpha */
	if (v->reg[sSetupMode].u & (1 << 1))
	{
		v->fbi.starta = (INT32)(v->fbi.svert[0].a * 4096.0);
		v->fbi.dadx = (INT32)(((v->fbi.svert[0].a - v->fbi.svert[1].a) * dx1 - (v->fbi.svert[0].a - v->fbi.svert[2].a) * dx2) * tdiv);
		v->fbi.dady = (INT32)(((v->fbi.svert[0].a - v->fbi.svert[2].a) * dy1 - (v->fbi.svert[0].a - v->fbi.svert[1].a) * dy2) * tdiv);
	}

	/* set up Z */
	if (v->reg[sSetupMode].u & (1 << 2))
	{
		v->fbi.startz = (INT32)(v->fbi.svert[0].z * 4096.0);
		v->fbi.dzdx = (INT32)(((v->fbi.svert[0].z - v->fbi.svert[1].z) * dx1 - (v->fbi.svert[0].z - v->fbi.svert[2].z) * dx2) * tdiv);
		v->fbi.dzdy = (INT32)(((v->fbi.svert[0].z - v->fbi.svert[2].z) * dy1 - (v->fbi.svert[0].z - v->fbi.svert[1].z) * dy2) * tdiv);
	}

	/* set up Wb */
	tdiv = divisor * 65536.0f * 65536.0f;
	if (v->reg[sSetupMode].u & (1 << 3))
	{
		v->fbi.startw = v->tmu[0].startw = v->tmu[1].startw = (INT64)(v->fbi.svert[0].wb * 65536.0f * 65536.0f);
		v->fbi.dwdx = v->tmu[0].dwdx = v->tmu[1].dwdx = (INT64)(((v->fbi.svert[0].wb - v->fbi.svert[1].wb) * dx1 - (v->fbi.svert[0].wb - v->fbi.svert[2].wb) * dx2) * tdiv);
		v->fbi.dwdy = v->tmu[0].dwdy = v->tmu[1].dwdy = (INT64)(((v->fbi.svert[0].wb - v->fbi.svert[2].wb) * dy1 - (v->fbi.svert[0].wb - v->fbi.svert[1].wb) * dy2) * tdiv);
	}

	/* set up W0 */
	if (v->reg[sSetupMode].u & (1 << 4))
	{
		v->tmu[0].startw = v->tmu[1].startw = (INT64)(v->fbi.svert[0].w0 * 65536.0f * 65536.0f);
		v->tmu[0].dwdx = v->tmu[1].dwdx = (INT64)(((v->fbi.svert[0].w0 - v->fbi.svert[1].w0) * dx1 - (v->fbi.svert[0].w0 - v->fbi.svert[2].w0) * dx2) * tdiv);
		v->tmu[0].dwdy = v->tmu[1].dwdy = (INT64)(((v->fbi.svert[0].w0 - v->fbi.svert[2].w0) * dy1 - (v->fbi.svert[0].w0 - v->fbi.svert[1].w0) * dy2) * tdiv);
	}

	/* set up S0,T0 */
	if (v->reg[sSetupMode].u & (1 << 5))
	{
		v->tmu[0].starts = v->tmu[1].starts = (INT64)(v->fbi.svert[0].s0 * 65536.0f * 65536.0f);
		v->tmu[0].dsdx = v->tmu[1].dsdx = (INT64)(((v->fbi.svert[0].s0 - v->fbi.svert[1].s0) * dx1 - (v->fbi.svert[0].s0 - v->fbi.svert[2].s0) * dx2) * tdiv);
		v->tmu[0].dsdy = v->tmu[1].dsdy = (INT64)(((v->fbi.svert[0].s0 - v->fbi.svert[2].s0) * dy1 - (v->fbi.svert[0].s0 - v->fbi.svert[1].s0) * dy2) * tdiv);
		v->tmu[0].startt = v->tmu[1].startt = (INT64)(v->fbi.svert[0].t0 * 65536.0f * 65536.0f);
		v->tmu[0].dtdx = v->tmu[1].dtdx = (INT64)(((v->fbi.svert[0].t0 - v->fbi.svert[1].t0) * dx1 - (v->fbi.svert[0].t0 - v->fbi.svert[2].t0) * dx2) * tdiv);
		v->tmu[0].dtdy = v->tmu[1].dtdy = (INT64)(((v->fbi.svert[0].t0 - v->fbi.svert[2].t0) * dy1 - (v->fbi.svert[0].t0 - v->fbi.svert[1].t0) * dy2) * tdiv);
	}

	/* set up W1 */
	if (v->reg[sSetupMode].u & (1 << 6))
	{
		v->tmu[1].startw = (INT64)(v->fbi.svert[0].w1 * 65536.0f * 65536.0f);
		v->tmu[1].dwdx = (INT64)(((v->fbi.svert[0].w1 - v->fbi.svert[1].w1) * dx1 - (v->fbi.svert[0].w1 - v->fbi.svert[2].w1) * dx2) * tdiv);
		v->tmu[1].dwdy = (INT64)(((v->fbi.svert[0].w1 - v->fbi.svert[2].w1) * dy1 - (v->fbi.svert[0].w1 - v->fbi.svert[1].w1) * dy2) * tdiv);
	}

	/* set up S1,T1 */
	if (v->reg[sSetupMode].u & (1 << 7))
	{
		v->tmu[1].starts = (INT64)(v->fbi.svert[0].s1 * 65536.0f * 65536.0f);
		v->tmu[1].dsdx = (INT64)(((v->fbi.svert[0].s1 - v->fbi.svert[1].s1) * dx1 - (v->fbi.svert[0].s1 - v->fbi.svert[2].s1) * dx2) * tdiv);
		v->tmu[1].dsdy = (INT64)(((v->fbi.svert[0].s1 - v->fbi.svert[2].s1) * dy1 - (v->fbi.svert[0].s1 - v->fbi.svert[1].s1) * dy2) * tdiv);
		v->tmu[1].startt = (INT64)(v->fbi.svert[0].t1 * 65536.0f * 65536.0f);
		v->tmu[1].dtdx = (INT64)(((v->fbi.svert[0].t1 - v->fbi.svert[1].t1) * dx1 - (v->fbi.svert[0].t1 - v->fbi.svert[2].t1) * dx2) * tdiv);
		v->tmu[1].dtdy = (INT64)(((v->fbi.svert[0].t1 - v->fbi.svert[2].t1) * dy1 - (v->fbi.svert[0].t1 - v->fbi.svert[1].t1) * dy2) * tdiv);
	}

	/* draw the triangle */
	triangle(v);
}

/*-------------------------------------------------
    draw_triangle - execute the 'DrawTri'
    command
-------------------------------------------------*/
static void draw_triangle(voodoo_state *v)
{
	setup_vertex *sv = &v->fbi.svert[2];

	/* for strip mode, shuffle vertex 1 down to 0 */
	if (!(v->reg[sSetupMode].u & (1 << 16)))
		v->fbi.svert[0] = v->fbi.svert[1];

	/* copy 2 down to 1 regardless */
	v->fbi.svert[1] = v->fbi.svert[2];

	/* extract all the data from registers */
	sv->x = v->reg[sVx].f;
	sv->y = v->reg[sVy].f;
	sv->wb = v->reg[sWb].f;
	sv->w0 = v->reg[sWtmu0].f;
	sv->s0 = v->reg[sS_W0].f;
	sv->t0 = v->reg[sT_W0].f;
	sv->w1 = v->reg[sWtmu1].f;
	sv->s1 = v->reg[sS_Wtmu1].f;
	sv->t1 = v->reg[sT_Wtmu1].f;
	sv->a = v->reg[sAlpha].f;
	sv->r = v->reg[sRed].f;
	sv->g = v->reg[sGreen].f;
	sv->b = v->reg[sBlue].f;

	/* if we have enough verts, go ahead and draw */
	if (++v->fbi.sverts >= 3)
		setup_and_draw_triangle(v);
}

/*-------------------------------------------------
    fastfill - execute the 'fastfill'
    command
-------------------------------------------------*/
static void fastfill(voodoo_state *v)
{
	int sx = (v->reg[clipLeftRight].u >> 16) & 0x3ff;
	int ex = (v->reg[clipLeftRight].u >> 0) & 0x3ff;
	int sy = (v->reg[clipLowYHighY].u >> 16) & 0x3ff;
	int ey = (v->reg[clipLowYHighY].u >> 0) & 0x3ff;

	poly_extent extents[64];
	UINT16 dithermatrix[16];
	UINT16 *drawbuf = NULL;
	int x, y;

	/* if we're not clearing either, take no time */
	if (!FBZMODE_RGB_BUFFER_MASK(v->reg[fbzMode].u) && !FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u))
		return;

	/* are we clearing the RGB buffer? */
	if (FBZMODE_RGB_BUFFER_MASK(v->reg[fbzMode].u))
	{
		/* determine the draw buffer */
		int destbuf = FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u);
		switch (destbuf)
		{
			case 0:		/* front buffer */
				drawbuf = (UINT16 *)(v->fbi.ram + v->fbi.rgboffs[v->fbi.frontbuf]);
				break;

			case 1:		/* back buffer */
				drawbuf = (UINT16 *)(v->fbi.ram + v->fbi.rgboffs[v->fbi.backbuf]);
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

			COMPUTE_DITHER_POINTERS(v->reg[fbzMode].u, y);
			for (x = 0; x < 4; x++)
			{
				int r = v->reg[color1].rgb.r;
				int g = v->reg[color1].rgb.g;
				int b = v->reg[color1].rgb.b;

				APPLY_DITHER(v->reg[fbzMode].u, x, dither_lookup, r, g, b);
				dithermatrix[y*4 + x] = (UINT16)((r << 11) | (g << 5) | b);
			}
		}
	}

	/* fill in a block of extents */
	extents[0].startx = sx;
	extents[0].stopx = ex;
	for (auto extnum = 1u; extnum < ARRAY_LEN(extents); ++extnum)
		extents[extnum] = extents[0];

#ifdef C_ENABLE_VOODOO_OPENGL
	if (v->ogl && v->active) {
		voodoo_ogl_fastfill();
		return;
	}
#endif

	/* iterate over blocks of extents */
	for (y = sy; y < ey; y += ARRAY_LEN(extents))
	{
		int count = MIN(ey - y, ARRAY_LEN(extents));
		void *dest = drawbuf;
		int startscanline = y;
		int numscanlines = count;

		INT32 v1yclip = startscanline;
		INT32 v3yclip = startscanline + numscanlines;

		if (v3yclip - v1yclip <= 0)
			return;

		for (INT32 curscan = v1yclip; curscan < v3yclip; curscan++)
		{
			const poly_extent *extent = &extents[curscan - startscanline];
			INT32 istartx = extent->startx, istopx = extent->stopx;

			/* force start < stop */
			if (istartx > istopx)
			{
				INT32 temp = istartx;
				istartx = istopx;
				istopx = temp;
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
static void swapbuffer(voodoo_state *v, UINT32 data)
{
	/* set the don't swap value for Voodoo 2 */
	v->fbi.vblank_dont_swap = ((data >> 9) & 1)>0;

	voodoo_swap_buffers(v);
}


/*************************************
 *
 *  Chip reset
 *
 *************************************/

static void reset_counters(voodoo_state *v)
{
	update_statistics(v, false);
	v->reg[fbiPixelsIn].u = 0;
	v->reg[fbiChromaFail].u = 0;
	v->reg[fbiZfuncFail].u = 0;
	v->reg[fbiAfuncFail].u = 0;
	v->reg[fbiPixelsOut].u = 0;
}

static void soft_reset(voodoo_state *v)
{
	reset_counters(v);
	v->reg[fbiTrianglesOut].u = 0;
}


/*************************************
 *
 *  Voodoo register writes
 *
 *************************************/
static void register_w(uint32_t offset, uint32_t data)
{
	UINT32 regnum  = (offset) & 0xff;
	UINT32 chips   = (offset>>8) & 0xf;

	INT64 data64;

	//LOG(LOG_VOODOO,LOG_WARN)("V3D:WR chip %x reg %x value %08x(%s)", chips, regnum<<2, data, voodoo_reg_name[regnum]);

	if (chips == 0)
		chips = 0xf;
	chips &= v->chipmask;

	/* the first 64 registers can be aliased differently */
	if ((offset & 0x800c0) == 0x80000 && v->alt_regmap)
		regnum = register_alias_map[offset & 0x3f];
	else
		regnum = offset & 0xff;

	/* first make sure this register is readable */
	if (!(v->regaccess[regnum] & REGISTER_WRITE))
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
			if (chips & 1) v->fbi.ax = (INT16)(data&0xffff);
			break;

		case fvertexAy:
			data = float_to_int32(data, 4);
			[[fallthrough]];
		case vertexAy:
			if (chips & 1) v->fbi.ay = (INT16)(data&0xffff);
			break;

		case fvertexBx:
			data = float_to_int32(data, 4);
			[[fallthrough]];
		case vertexBx:
			if (chips & 1) v->fbi.bx = (INT16)(data&0xffff);
			break;

		case fvertexBy:
			data = float_to_int32(data, 4);
			[[fallthrough]];
		case vertexBy:
			if (chips & 1) v->fbi.by = (INT16)(data&0xffff);
			break;

		case fvertexCx:
			data = float_to_int32(data, 4);
			[[fallthrough]];
		case vertexCx:
			if (chips & 1) v->fbi.cx = (INT16)(data&0xffff);
			break;

		case fvertexCy:
			data = float_to_int32(data, 4);
			[[fallthrough]];
		case vertexCy:
			if (chips & 1) v->fbi.cy = (INT16)(data&0xffff);
			break;

		/* RGB data is 12.12 formatted fixed point */
		case fstartR:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case startR:
			if (chips & 1) v->fbi.startr = (INT32)(data << 8) >> 8;
			break;

		case fstartG:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case startG:
			if (chips & 1) v->fbi.startg = (INT32)(data << 8) >> 8;
			break;

		case fstartB:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case startB:
			if (chips & 1) v->fbi.startb = (INT32)(data << 8) >> 8;
			break;

		case fstartA:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case startA:
			if (chips & 1) v->fbi.starta = (INT32)(data << 8) >> 8;
			break;

		case fdRdX:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dRdX:
			if (chips & 1) v->fbi.drdx = (INT32)(data << 8) >> 8;
			break;

		case fdGdX:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dGdX:
			if (chips & 1) v->fbi.dgdx = (INT32)(data << 8) >> 8;
			break;

		case fdBdX:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dBdX:
			if (chips & 1) v->fbi.dbdx = (INT32)(data << 8) >> 8;
			break;

		case fdAdX:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dAdX:
			if (chips & 1) v->fbi.dadx = (INT32)(data << 8) >> 8;
			break;

		case fdRdY:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dRdY:
			if (chips & 1) v->fbi.drdy = (INT32)(data << 8) >> 8;
			break;

		case fdGdY:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dGdY:
			if (chips & 1) v->fbi.dgdy = (INT32)(data << 8) >> 8;
			break;

		case fdBdY:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dBdY:
			if (chips & 1) v->fbi.dbdy = (INT32)(data << 8) >> 8;
			break;

		case fdAdY:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dAdY:
			if (chips & 1) v->fbi.dady = (INT32)(data << 8) >> 8;
			break;

		/* Z data is 20.12 formatted fixed point */
		case fstartZ:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case startZ:
			if (chips & 1) v->fbi.startz = (INT32)data;
			break;

		case fdZdX:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dZdX:
			if (chips & 1) v->fbi.dzdx = (INT32)data;
			break;

		case fdZdY:
			data = float_to_int32(data, 12);
			[[fallthrough]];
		case dZdY:
			if (chips & 1) v->fbi.dzdy = (INT32)data;
			break;

		/* S,T data is 14.18 formatted fixed point, converted to 16.32 internally */
		case fstartS:
			data64 = float_to_int64(data, 32);
			if (chips & 2) v->tmu[0].starts = data64;
			if (chips & 4) v->tmu[1].starts = data64;
			break;
		case startS:
			if (chips & 2) v->tmu[0].starts = (INT64)(INT32)data << 14;
			if (chips & 4) v->tmu[1].starts = (INT64)(INT32)data << 14;
			break;

		case fstartT:
			data64 = float_to_int64(data, 32);
			if (chips & 2) v->tmu[0].startt = data64;
			if (chips & 4) v->tmu[1].startt = data64;
			break;
		case startT:
			if (chips & 2) v->tmu[0].startt = (INT64)(INT32)data << 14;
			if (chips & 4) v->tmu[1].startt = (INT64)(INT32)data << 14;
			break;

		case fdSdX:
			data64 = float_to_int64(data, 32);
			if (chips & 2) v->tmu[0].dsdx = data64;
			if (chips & 4) v->tmu[1].dsdx = data64;
			break;
		case dSdX:
			if (chips & 2) v->tmu[0].dsdx = (INT64)(INT32)data << 14;
			if (chips & 4) v->tmu[1].dsdx = (INT64)(INT32)data << 14;
			break;

		case fdTdX:
			data64 = float_to_int64(data, 32);
			if (chips & 2) v->tmu[0].dtdx = data64;
			if (chips & 4) v->tmu[1].dtdx = data64;
			break;
		case dTdX:
			if (chips & 2) v->tmu[0].dtdx = (INT64)(INT32)data << 14;
			if (chips & 4) v->tmu[1].dtdx = (INT64)(INT32)data << 14;
			break;

		case fdSdY:
			data64 = float_to_int64(data, 32);
			if (chips & 2) v->tmu[0].dsdy = data64;
			if (chips & 4) v->tmu[1].dsdy = data64;
			break;
		case dSdY:
			if (chips & 2) v->tmu[0].dsdy = (INT64)(INT32)data << 14;
			if (chips & 4) v->tmu[1].dsdy = (INT64)(INT32)data << 14;
			break;

		case fdTdY:
			data64 = float_to_int64(data, 32);
			if (chips & 2) v->tmu[0].dtdy = data64;
			if (chips & 4) v->tmu[1].dtdy = data64;
			break;
		case dTdY:
			if (chips & 2) v->tmu[0].dtdy = (INT64)(INT32)data << 14;
			if (chips & 4) v->tmu[1].dtdy = (INT64)(INT32)data << 14;
			break;

		/* W data is 2.30 formatted fixed point, converted to 16.32 internally */
		case fstartW:
			data64 = float_to_int64(data, 32);
			if (chips & 1) v->fbi.startw = data64;
			if (chips & 2) v->tmu[0].startw = data64;
			if (chips & 4) v->tmu[1].startw = data64;
			break;
		case startW:
			if (chips & 1) v->fbi.startw = (INT64)(INT32)data << 2;
			if (chips & 2) v->tmu[0].startw = (INT64)(INT32)data << 2;
			if (chips & 4) v->tmu[1].startw = (INT64)(INT32)data << 2;
			break;

		case fdWdX:
			data64 = float_to_int64(data, 32);
			if (chips & 1) v->fbi.dwdx = data64;
			if (chips & 2) v->tmu[0].dwdx = data64;
			if (chips & 4) v->tmu[1].dwdx = data64;
			break;
		case dWdX:
			if (chips & 1) v->fbi.dwdx = (INT64)(INT32)data << 2;
			if (chips & 2) v->tmu[0].dwdx = (INT64)(INT32)data << 2;
			if (chips & 4) v->tmu[1].dwdx = (INT64)(INT32)data << 2;
			break;

		case fdWdY:
			data64 = float_to_int64(data, 32);
			if (chips & 1) v->fbi.dwdy = data64;
			if (chips & 2) v->tmu[0].dwdy = data64;
			if (chips & 4) v->tmu[1].dwdy = data64;
			break;
		case dWdY:
			if (chips & 1) v->fbi.dwdy = (INT64)(INT32)data << 2;
			if (chips & 2) v->tmu[0].dwdy = (INT64)(INT32)data << 2;
			if (chips & 4) v->tmu[1].dwdy = (INT64)(INT32)data << 2;
			break;

		/* setup bits */
		case sARGB:
			if (chips & 1)
			{
				v->reg[sAlpha].f = (float)RGB_ALPHA(data);
				v->reg[sRed].f = (float)RGB_RED(data);
				v->reg[sGreen].f = (float)RGB_GREEN(data);
				v->reg[sBlue].f = (float)RGB_BLUE(data);
			}
			break;

		/* mask off invalid bits for different cards */
		case fbzColorPath:
			if (vtype < VOODOO_2)
				data &= 0x0fffffff;
			if (chips & 1) v->reg[fbzColorPath].u = data;
			break;

		case fbzMode:
			if (vtype < VOODOO_2)
				data &= 0x001fffff;
			if (chips & 1) {
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
			if (vtype < VOODOO_2)
				data &= 0x0000003f;
			if (chips & 1) v->reg[fogMode].u = data;
			break;

		/* triangle drawing */
		case triangleCMD:
			triangle(v);
			break;

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
			if (data & 1)
				reset_counters(v);
			if (data & 2)
				v->reg[fbiTrianglesOut].u = 0;
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
			if (chips & 1)
			{
				if (!(data & 0x800))
					dacdata_w(&v->dac, (data >> 8) & 7, data & 0xff);
				else
					dacdata_r(&v->dac, (data >> 8) & 7);
			}
			break;

		/* vertical sync rate -- Voodoo/Voodoo2 only */
		case hSync:
		case vSync:
		case backPorch:
		case videoDimensions:
			if (chips & 1)
			{
				v->reg[regnum].u = data;
				if (v->reg[hSync].u != 0 && v->reg[vSync].u != 0 && v->reg[videoDimensions].u != 0)
				{
#ifdef C_ENABLE_VOODOO_DEBUG
					int htotal = ((v->reg[hSync].u >> 16) & 0x3ff) + 1 + (v->reg[hSync].u & 0xff) + 1;
#endif
					int vtotal = ((v->reg[vSync].u >> 16) & 0xfff) + (v->reg[vSync].u & 0xfff);
					int hvis = v->reg[videoDimensions].u & 0x3ff;
					int vvis = (v->reg[videoDimensions].u >> 16) & 0x3ff;
#ifdef C_ENABLE_VOODOO_DEBUG
					int hbp = (v->reg[backPorch].u & 0xff) + 2;
					int vbp = (v->reg[backPorch].u >> 16) & 0xff;
#endif
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

#ifdef C_ENABLE_VOODOO_DEBUG
					rectangle visarea;

					/* create a new visarea */
					visarea.min_x = hbp;
					visarea.max_x = hbp + hvis - 1;
					visarea.min_y = vbp;
					visarea.max_y = vbp + vvis - 1;

					/* keep within bounds */
					visarea.max_x = MIN(visarea.max_x, htotal - 1);
					visarea.max_y = MIN(visarea.max_y, vtotal - 1);
					LOG(LOG_VOODOO,LOG_WARN)("Horiz: %d-%d (%d total)  Vert: %d-%d (%d total) -- ", visarea.min_x, visarea.max_x, htotal, visarea.min_y, visarea.max_y, vtotal);
#endif

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

					/* configure the new framebuffer info */
					UINT32 new_width = (hvis+1) & ~1;
					UINT32 new_height = (vvis+1) & ~1;
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
					if (regnum == videoDimensions)
						recompute_video_memory(v);

					Voodoo_UpdateScreenStart();
				}
			}
			break;

		/* fbiInit0 can only be written if initEnable says we can -- Voodoo/Voodoo2 only */
		case fbiInit0:
			if ((chips & 1) && INITEN_ENABLE_HW_INIT(v->pci.init_enable))
			{
				bool new_output_on = FBIINIT0_VGA_PASSTHRU(data);
				if (v->output_on != new_output_on) {
					v->output_on = new_output_on;
					Voodoo_UpdateScreenStart();
				}

				v->reg[fbiInit0].u = data;
				if (FBIINIT0_GRAPHICS_RESET(data))
					soft_reset(v);
				recompute_video_memory(v);
			}
			break;

		/* fbiInit5-7 are Voodoo 2-only; ignore them on anything else */
		case fbiInit5:
		case fbiInit6:
			if (vtype < VOODOO_2)
				break;
			/* else fall through... */

		/* fbiInitX can only be written if initEnable says we can -- Voodoo/Voodoo2 only */
		/* most of these affect memory layout, so always recompute that when done */
		case fbiInit1:
		case fbiInit2:
		case fbiInit4:
			if ((chips & 1) && INITEN_ENABLE_HW_INIT(v->pci.init_enable))
			{
				v->reg[regnum].u = data;
				recompute_video_memory(v);
			}
			break;

		case fbiInit3:
			if ((chips & 1) && INITEN_ENABLE_HW_INIT(v->pci.init_enable))
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
			if (chips & 2) ncc_table_write(&v->tmu[0].ncc[0], regnum - nccTable, data);
			if (chips & 4) ncc_table_write(&v->tmu[1].ncc[0], regnum - nccTable, data);
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
			if (chips & 2) ncc_table_write(&v->tmu[0].ncc[1], regnum - (nccTable+12), data);
			if (chips & 4) ncc_table_write(&v->tmu[1].ncc[1], regnum - (nccTable+12), data);
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
			if (chips & 1)
			{
				int base = 2 * (regnum - fogTable);
				v->fbi.fogdelta[base + 0] = (data >> 0) & 0xff;
				v->fbi.fogblend[base + 0] = (data >> 8) & 0xff;
				v->fbi.fogdelta[base + 1] = (data >> 16) & 0xff;
				v->fbi.fogblend[base + 1] = (data >> 24) & 0xff;
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
			if (chips & 2)
			{
				v->tmu[0].reg[regnum].u = data;
				v->tmu[0].regdirty = true;
			}
			if (chips & 4)
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
			if (chips & 1) v->reg[0x000 + regnum].u = data;
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
			if (chips & 1) v->reg[0x000 + regnum].u = data;
			if (chips & 2) v->reg[0x100 + regnum].u = data;
			if (chips & 4) v->reg[0x200 + regnum].u = data;
			if (chips & 8) v->reg[0x300 + regnum].u = data;
			break;
	}
}

/*************************************
 *
 *  Voodoo LFB writes
 *
 *************************************/
static void lfb_w(UINT32 offset, UINT32 data, UINT32 mem_mask) {
	//LOG(LOG_VOODOO,LOG_WARN)("V3D:WR LFB offset %X value %08X", offset, data);
	UINT16 *dest, *depth;
	UINT32 destmax, depthmax;

	int sr[2], sg[2], sb[2], sa[2], sw[2];
	int x, y, scry, mask;
	int pix, destbuf;

	/* byte swizzling */
	if (LFBMODE_BYTE_SWIZZLE_WRITES(v->reg[lfbMode].u))
	{
		data = FLIPENDIAN_INT32(data);
		mem_mask = FLIPENDIAN_INT32(mem_mask);
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
	if (!ACCESSING_BITS_0_15)
		mask &= ~(0x0f - LFB_DEPTH_PRESENT_MSW);
	if (!ACCESSING_BITS_16_31)
		mask &= ~(0xf0 + LFB_DEPTH_PRESENT_MSW);

	/* select the target buffer */
	destbuf = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u);
	//LOG(LOG_VOODOO,LOG_WARN)("destbuf %X lfbmode %X",destbuf, v->reg[lfbMode].u);
	switch (destbuf)
	{
		case 0:			/* front buffer */
			dest = (UINT16 *)(v->fbi.ram + v->fbi.rgboffs[v->fbi.frontbuf]);
			destmax = (v->fbi.mask + 1 - v->fbi.rgboffs[v->fbi.frontbuf]) / 2;
			break;

		case 1:			/* back buffer */
			dest = (UINT16 *)(v->fbi.ram + v->fbi.rgboffs[v->fbi.backbuf]);
			destmax = (v->fbi.mask + 1 - v->fbi.rgboffs[v->fbi.backbuf]) / 2;
			break;

		default:		/* reserved */
			E_Exit("reserved lfb write");
			return;
	}
	depth = (UINT16 *)(v->fbi.ram + v->fbi.auxoffs);
	depthmax = (v->fbi.mask + 1 - v->fbi.auxoffs) / 2;

	/* simple case: no pipeline */
	if (!LFBMODE_ENABLE_PIXEL_PIPELINE(v->reg[lfbMode].u))
	{
		const uint8_t* dither_lookup = nullptr;
		const uint8_t* dither4       = nullptr;

		[[maybe_unused]] const uint8_t* dither = nullptr;

		UINT32 bufoffs;

		if (LOG_LFB) LOG(LOG_VOODOO,LOG_WARN)("VOODOO.LFB:write raw mode %X (%d,%d) = %08X & %08X\n", LFBMODE_WRITE_FORMAT(v->reg[lfbMode].u), x, y, data, mem_mask);

		/* determine the screen Y */
		scry = y;
		if (LFBMODE_Y_ORIGIN(v->reg[lfbMode].u))
			scry = (v->fbi.yorigin - y) & 0x3ff;

		/* advance pointers to the proper row */
		bufoffs = scry * v->fbi.rowpixels + x;

		/* compute dithering */
		COMPUTE_DITHER_POINTERS(v->reg[fbzMode].u, y);

		/* loop over up to two pixels */
		for (pix = 0; mask; pix++)
		{
			/* make sure we care about this pixel */
			if (mask & 0x0f)
			{
				bool has_rgb = (mask & LFB_RGB_PRESENT) > 0;
				bool has_alpha = ((mask & LFB_ALPHA_PRESENT) > 0) && (FBZMODE_ENABLE_ALPHA_PLANES(v->reg[fbzMode].u) > 0);
				bool has_depth = ((mask & (LFB_DEPTH_PRESENT | LFB_DEPTH_PRESENT_MSW)) && !FBZMODE_ENABLE_ALPHA_PLANES(v->reg[fbzMode].u));
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
						dest[bufoffs] = (UINT16)((sr[pix] << 11) | (sg[pix] << 5) | sb[pix]);
					}

					/* make sure we have an aux buffer to write to */
					if (depth && bufoffs < depthmax)
					{
						/* write to the alpha buffer */
						if (has_alpha)
							depth[bufoffs] = (UINT16)sa[pix];

						/* write to the depth buffer */
						if (has_depth)
							depth[bufoffs] = (UINT16)sw[pix];
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

		if (LOG_LFB) LOG(LOG_VOODOO,LOG_WARN)("VOODOO.LFB:write pipelined mode %X (%d,%d) = %08X & %08X\n", LFBMODE_WRITE_FORMAT(v->reg[lfbMode].u), x, y, data, mem_mask);

		/* determine the screen Y */
		scry = y;
		if (FBZMODE_Y_ORIGIN(v->reg[fbzMode].u))
			scry = (v->fbi.yorigin - y) & 0x3ff;

		/* advance pointers to the proper row */
		dest += scry * v->fbi.rowpixels;
		if (depth)
			depth += scry * v->fbi.rowpixels;

		/* compute dithering */
		COMPUTE_DITHER_POINTERS(v->reg[fbzMode].u, y);

		int32_t blendr = 0;
		int32_t blendg = 0;
		int32_t blendb = 0;
		int32_t blenda = 0;

		/* loop over up to two pixels */
		stats_block stats = {};
		for (pix = 0; mask; pix++)
		{
			/* make sure we care about this pixel */
			if (mask & 0x0f)
			{
				INT64 iterw = sw[pix] << (30-16);
				INT32 iterz = sw[pix] << 12;
				rgb_union color;

				/* apply clipping */
				if (FBZMODE_ENABLE_CLIPPING(v->reg[fbzMode].u))
				{
					if (x < (INT32)((v->reg[clipLeftRight].u >> 16) & 0x3ff) ||
						x >= (INT32)(v->reg[clipLeftRight].u & 0x3ff) ||
						scry < (INT32)((v->reg[clipLowYHighY].u >> 16) & 0x3ff) ||
						scry >= (INT32)(v->reg[clipLowYHighY].u & 0x3ff))
					{
						stats.pixels_in++;
						//stats.clip_fail++;
						goto nextpixel;
					}
				}

				/* pixel pipeline part 1 handles depth testing and stippling */
				// TODO: in the v->ogl case this macro doesn't really work with depth testing
				PIXEL_PIPELINE_BEGIN(v, stats, x, y, v->reg[fbzColorPath].u, v->reg[fbzMode].u, iterz, iterw, v->reg[zaColor].u, v->reg[stipple].u);

				color.rgb.r = sr[pix];
				color.rgb.g = sg[pix];
				color.rgb.b = sb[pix];
				color.rgb.a = sa[pix];

				/* apply chroma key */
				APPLY_CHROMAKEY(v, stats, v->reg[fbzMode].u, color);

				/* apply alpha mask, and alpha testing */
				APPLY_ALPHAMASK(v, stats, v->reg[fbzMode].u, color.rgb.a);
				APPLY_ALPHATEST(v, stats, v->reg[alphaMode].u, color.rgb.a);

				/*
				if (FBZCP_CC_MSELECT(v->reg[fbzColorPath].u) != 0) LOG_MSG("lfbw fpp mselect %8x",FBZCP_CC_MSELECT(v->reg[fbzColorPath].u));
				if (FBZCP_CCA_MSELECT(v->reg[fbzColorPath].u) > 1) LOG_MSG("lfbw fpp mselect alpha %8x",FBZCP_CCA_MSELECT(v->reg[fbzColorPath].u));

				if (FBZCP_CC_REVERSE_BLEND(v->reg[fbzColorPath].u) != 0) {
					if (FBZCP_CC_MSELECT(v->reg[fbzColorPath].u) != 0) LOG_MSG("lfbw fpp rblend %8x",FBZCP_CC_REVERSE_BLEND(v->reg[fbzColorPath].u));
				}
				if (FBZCP_CCA_REVERSE_BLEND(v->reg[fbzColorPath].u) != 0) {
					if (FBZCP_CC_MSELECT(v->reg[fbzColorPath].u) != 0) LOG_MSG("lfbw fpp rblend alpha %8x",FBZCP_CCA_REVERSE_BLEND(v->reg[fbzColorPath].u));
				}
				*/

				rgb_union c_local;

				/* compute c_local */
				if (FBZCP_CC_LOCALSELECT_OVERRIDE(v->reg[fbzColorPath].u) == 0)
				{
					if (FBZCP_CC_LOCALSELECT(v->reg[fbzColorPath].u) == 0)	/* iterated RGB */
					{
						//c_local.u = iterargb.u;
						c_local.rgb.r = sr[pix];
						c_local.rgb.g = sg[pix];
						c_local.rgb.b = sb[pix];
					}
					else											/* color0 RGB */
						c_local.u = v->reg[color0].u;
				}
				else
				{
					LOG_MSG("lfbw fpp FBZCP_CC_LOCALSELECT_OVERRIDE set!");
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
						c_local.rgb.a = sa[pix];
						break;
					case 1:		/* color0 alpha */
						c_local.rgb.a = v->reg[color0].rgb.a;
						break;
					case 2:		/* clamped iterated Z[27:20] */
					{
						int temp;
						CLAMPED_Z(iterz, v->reg[fbzColorPath].u, temp);
						c_local.rgb.a = (UINT8)temp;
						break;
					}
					case 3:		/* clamped iterated W[39:32] */
					{
						int temp;
						CLAMPED_W(iterw, v->reg[fbzColorPath].u, temp);			/* Voodoo 2 only */
						c_local.rgb.a = (UINT8)temp;
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
				if (FBZCP_CCA_SUB_CLOCAL(v->reg[fbzColorPath].u))
					a -= c_local.rgb.a;

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
						//LOG_MSG("blend RGB c_local");
						break;
					case 2:		/* a_other */
						//blendr = blendg = blendb = c_other.rgb.a;
						LOG_MSG("blend RGB a_other");
						break;
					case 3:		/* a_local */
						blendr = blendg = blendb = c_local.rgb.a;
						LOG_MSG("blend RGB a_local");
						break;
					case 4:		/* texture alpha */
						//blendr = blendg = blendb = texel.rgb.a;
						LOG_MSG("blend RGB texture alpha");
						break;
					case 5:		/* texture RGB (Voodoo 2 only) */
						//blendr = texel.rgb.r;
						//blendg = texel.rgb.g;
						//blendb = texel.rgb.b;
						LOG_MSG("blend RGB texture RGB");
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
						//LOG_MSG("blend alpha a_local");
						break;
					case 2:		/* a_other */
						//blenda = c_other.rgb.a;
						LOG_MSG("blend alpha a_other");
						break;
					case 3:		/* a_local */
						blenda = c_local.rgb.a;
						LOG_MSG("blend alpha a_local");
						break;
					case 4:		/* texture alpha */
						//blenda = texel.rgb.a;
						LOG_MSG("blend alpha texture alpha");
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
				if (!FBZCP_CCA_REVERSE_BLEND(v->reg[fbzColorPath].u))
					blenda ^= 0xff;

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
				if (FBZCP_CCA_ADD_ACLOCAL(v->reg[fbzColorPath].u))
					a += c_local.rgb.a;

				/* clamp */
				CLAMP(r, 0x00, 0xff);
				CLAMP(g, 0x00, 0xff);
				CLAMP(b, 0x00, 0xff);
				CLAMP(a, 0x00, 0xff);

				/* invert */
				if (FBZCP_CC_INVERT_OUTPUT(v->reg[fbzColorPath].u))
				{
					r ^= 0xff;
					g ^= 0xff;
					b ^= 0xff;
				}
				if (FBZCP_CCA_INVERT_OUTPUT(v->reg[fbzColorPath].u))
					a ^= 0xff;

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
static INT32 texture_w(UINT32 offset, UINT32 data) {
	int tmunum = (offset >> 19) & 0x03;
	//LOG(LOG_VOODOO,LOG_WARN)("V3D:write TMU%x offset %X value %X", tmunum, offset, data);

	tmu_state *t;

	/* point to the right TMU */
	if (!(v->chipmask & (2 << tmunum)))
		return 0;
	t = &v->tmu[tmunum];

	if (TEXLOD_TDIRECT_WRITE(t->reg[tLOD].u))
		E_Exit("Texture direct write!");

	/* update texture info if dirty */
	if (t->regdirty)
		recompute_texture_params(t);

	/* swizzle the data */
	if (TEXLOD_TDATA_SWIZZLE(t->reg[tLOD].u))
		data = FLIPENDIAN_INT32(data);
	if (TEXLOD_TDATA_SWAP(t->reg[tLOD].u))
		data = (data >> 16) | (data << 16);

	/* 8-bit texture case */
	if (TEXMODE_FORMAT(t->reg[textureMode].u) < 8)
	{
		int lod, tt, ts;
		UINT32 tbaseaddr;
		UINT8 *dest;

		/* extract info */
		lod = (offset >> 15) & 0x0f;
		tt = (offset >> 7) & 0xff;

		/* old code has a bit about how this is broken in gauntleg unless we always look at TMU0 */
		if (TEXMODE_SEQ_8_DOWNLD(v->tmu[0].reg/*t->reg*/[textureMode].u))
			ts = (offset << 2) & 0xfc;
		else
			ts = (offset << 1) & 0xfc;

		/* validate parameters */
		if (lod > 8)
			return 0;

		/* compute the base address */
		tbaseaddr = t->lodoffset[lod];
		tbaseaddr += tt * ((t->wmask >> lod) + 1) + ts;

		if (LOG_TEXTURE_RAM) LOG(LOG_VOODOO,LOG_WARN)("Texture 8-bit w: lod=%d s=%d t=%d data=%08X\n", lod, ts, tt, data);

		/* write the four bytes in little-endian order */
		dest = t->ram;
		tbaseaddr &= t->mask;

		[[maybe_unused]] bool changed = false;
		if (dest[BYTE4_XOR_LE(tbaseaddr + 0)] != ((data >> 0) & 0xff)) {
			dest[BYTE4_XOR_LE(tbaseaddr + 0)] = (data >> 0) & 0xff;
			changed = true;
		}
		if (dest[BYTE4_XOR_LE(tbaseaddr + 1)] != ((data >> 8) & 0xff)) {
			dest[BYTE4_XOR_LE(tbaseaddr + 1)] = (data >> 8) & 0xff;
			changed = true;
		}
		if (dest[BYTE4_XOR_LE(tbaseaddr + 2)] != ((data >> 16) & 0xff)) {
			dest[BYTE4_XOR_LE(tbaseaddr + 2)] = (data >> 16) & 0xff;
			changed = true;
		}
		if (dest[BYTE4_XOR_LE(tbaseaddr + 3)] != ((data >> 24) & 0xff)) {
			dest[BYTE4_XOR_LE(tbaseaddr + 3)] = (data >> 24) & 0xff;
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
		int lod, tt, ts;
		UINT32 tbaseaddr;
		UINT16 *dest;

		/* extract info */
		tmunum = (offset >> 19) & 0x03;
		lod = (offset >> 15) & 0x0f;
		tt = (offset >> 7) & 0xff;
		ts = (offset << 1) & 0xfe;

		/* validate parameters */
		if (lod > 8)
			return 0;

		/* compute the base address */
		tbaseaddr = t->lodoffset[lod];
		tbaseaddr += 2 * (tt * ((t->wmask >> lod) + 1) + ts);

		if (LOG_TEXTURE_RAM) LOG(LOG_VOODOO,LOG_WARN)("Texture 16-bit w: lod=%d s=%d t=%d data=%08X\n", lod, ts, tt, data);

		/* write the two words in little-endian order */
		dest = (UINT16 *)t->ram;
		tbaseaddr &= t->mask;
		tbaseaddr >>= 1;

		[[maybe_unused]] bool changed = false;
		if (dest[BYTE_XOR_LE(tbaseaddr + 0)] != ((data >> 0) & 0xffff)) {
			dest[BYTE_XOR_LE(tbaseaddr + 0)] = (data >> 0) & 0xffff;
			changed = true;
		}
		if (dest[BYTE_XOR_LE(tbaseaddr + 1)] != ((data >> 16) & 0xffff)) {
			dest[BYTE_XOR_LE(tbaseaddr + 1)] = (data >> 16) & 0xffff;
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
static UINT32 register_r(UINT32 offset)
{
	UINT32 regnum  = (offset) & 0xff;

	//LOG(LOG_VOODOO,LOG_WARN)("Voodoo:read chip %x reg %x (%s)", chips, regnum<<2, voodoo_reg_name[regnum]);

	/* first make sure this register is readable */
	if (!(v->regaccess[regnum] & REGISTER_READ))
	{
		return 0xffffffff;
	}

	UINT32 result;

	/* default result is the FBI register value */
	result = v->reg[regnum].u;

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

			/* bit 7 is FBI graphics engine busy */
			if (v->pci.op_pending)
				result |= 1 << 7;

			/* bit 8 is TREX busy */
			if (v->pci.op_pending)
				result |= 1 << 8;

			/* bit 9 is overall busy */
			if (v->pci.op_pending)
				result |= 1 << 9;

			/* bits 11:10 specifies which buffer is visible */
			result |= v->fbi.frontbuf << 10;

			/* bits 27:12 indicate memory FIFO freespace */
			result |= 0xffff << 12;

			/* bits 30:28 are the number of pending swaps */
			result |= 0 << 28;

			/* bit 31 is not used */


			break;

		case hvRetrace:
			if (vtype < VOODOO_2)
				break;


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
			if (INITEN_REMAP_INIT_TO_DAC(v->pci.init_enable))
				result = v->dac.read_result;
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
static UINT32 lfb_r(UINT32 offset)
{
	//LOG(LOG_VOODOO,LOG_WARN)("Voodoo:read LFB offset %X", offset);
	UINT16 *buffer;
	UINT32 bufmax;
	UINT32 bufoffs;
	UINT32 data;
	int x, y, scry;
	UINT32 destbuf;

	/* compute X,Y */
	x = (offset << 1) & 0x3fe;
	y = (offset >> 9) & 0x3ff;

	/* select the target buffer */
	destbuf = LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u);
	switch (destbuf)
	{
		case 0:			/* front buffer */
			buffer = (UINT16 *)(v->fbi.ram + v->fbi.rgboffs[v->fbi.frontbuf]);
			bufmax = (v->fbi.mask + 1 - v->fbi.rgboffs[v->fbi.frontbuf]) / 2;
			break;

		case 1:			/* back buffer */
			buffer = (UINT16 *)(v->fbi.ram + v->fbi.rgboffs[v->fbi.backbuf]);
			bufmax = (v->fbi.mask + 1 - v->fbi.rgboffs[v->fbi.backbuf]) / 2;
			break;

		case 2:			/* aux buffer */
			if (v->fbi.auxoffs == (UINT32)(~0))
				return 0xffffffff;
			buffer = (UINT16 *)(v->fbi.ram + v->fbi.auxoffs);
			bufmax = (v->fbi.mask + 1 - v->fbi.auxoffs) / 2;
			break;

		default:		/* reserved */
			return 0xffffffff;
	}

	/* determine the screen Y */
	scry = y;
	if (LFBMODE_Y_ORIGIN(v->reg[lfbMode].u))
		scry = (v->fbi.yorigin - y) & 0x3ff;

#ifdef C_ENABLE_VOODOO_OPENGL
	if (v->ogl && v->active) {
		data = voodoo_ogl_read_pixel(x, scry+1);
	} else
#endif
	{
		/* advance pointers to the proper row */
		bufoffs = scry * v->fbi.rowpixels + x;
		if (bufoffs >= bufmax)
			return 0xffffffff;

		/* compute the data */
		data = buffer[bufoffs + 0] | (buffer[bufoffs + 1] << 16);
	}

	/* word swapping */
	if (LFBMODE_WORD_SWAP_READS(v->reg[lfbMode].u))
		data = (data << 16) | (data >> 16);

	/* byte swizzling */
	if (LFBMODE_BYTE_SWIZZLE_READS(v->reg[lfbMode].u))
		data = FLIPENDIAN_INT32(data);

	if (LOG_LFB) LOG(LOG_VOODOO,LOG_WARN)("VOODOO.LFB:read (%d,%d) = %08X\n", x, y, data);
	return data;
}

static void voodoo_w(UINT32 offset, UINT32 data, UINT32 mask) {
	if ((offset & (0xc00000/4)) == 0)
		register_w(offset, data);
	else if ((offset & (0x800000/4)) == 0)
		lfb_w(offset, data, mask);
	else
		texture_w(offset, data);
}

static UINT32 voodoo_r(UINT32 offset) {
	if ((offset & (0xc00000/4)) == 0)
		return register_r(offset);
	else if ((offset & (0x800000/4)) == 0)
		return lfb_r(offset);

	return 0xffffffff;
}


/***************************************************************************
    DEVICE INTERFACE
***************************************************************************/

/*-------------------------------------------------
    device start callback
-------------------------------------------------*/

static void voodoo_init() {
	assert(!v);
	v = new voodoo_state;
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
	//for (UINT32 rct=0; rct<MAX_RASTERIZERS; rct++)
	//	v->rasterizer[rct] = raster_info();
	memset(v->rasterizer, 0, sizeof(v->rasterizer));
	memset(v->raster_hash, 0, sizeof(v->raster_hash));
#endif

	update_statistics(v, false);

	v->alt_regmap = false;
#ifdef C_ENABLE_VOODOO_DEBUG
	v->regnames = voodoo_reg_name;
#endif

	if (!*voodoo_reciplog)
	{
		/* create a table of precomputed 1/n and log2(n) values */
		/* n ranges from 1.0000 to 2.0000 */
		for (UINT32 val = 0; val <= (1 << RECIPLOG_LOOKUP_BITS); val++)
		{
			UINT32 value = (1 << RECIPLOG_LOOKUP_BITS) + val;
			voodoo_reciplog[val*2 + 0] = (1 << (RECIPLOG_LOOKUP_PREC + RECIPLOG_LOOKUP_BITS)) / value;
			voodoo_reciplog[val*2 + 1] = (UINT32)(LOGB2((double)value / (double)(1 << RECIPLOG_LOOKUP_BITS)) * (double)(1 << RECIPLOG_LOOKUP_PREC));
		}

		/* create dithering tables */
		for (UINT32 val = 0; val < 256*16*2; val++)
		{
			int g = (val >> 0) & 1;
			int x = (val >> 1) & 3;
			int color = (val >> 3) & 0xff;
			int y = (val >> 11) & 3;

			if (!g)
			{
				dither4_lookup[val] = (UINT8)(DITHER_RB(color, dither_matrix_4x4[y * 4 + x]) >> 3);
				dither2_lookup[val] = (UINT8)(DITHER_RB(color, dither_matrix_2x2[y * 4 + x]) >> 3);
			}
			else
			{
				dither4_lookup[val] = (UINT8)(DITHER_G(color, dither_matrix_4x4[y * 4 + x]) >> 2);
				dither2_lookup[val] = (UINT8)(DITHER_G(color, dither_matrix_2x2[y * 4 + x]) >> 2);
			}
		}

		#if defined(__SSE2__) && __SSE2__
		/* create sse2 scale table for rgba_bilinear_filter */
		for (INT16 i = 0; i != 256; i++)
		{
			sse2_scale_table[i][0] = sse2_scale_table[i][2] = sse2_scale_table[i][4] = sse2_scale_table[i][6] = i;
			sse2_scale_table[i][1] = sse2_scale_table[i][3] = sse2_scale_table[i][5] = sse2_scale_table[i][7] = 256-i;
		}
		#endif
	}

	v->tmu_config = 0x11;	// revision 1

	UINT32 fbmemsize = 0;
	UINT32 tmumem0 = 0;
	UINT32 tmumem1 = 0;

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

		default:
			E_Exit("Unsupported voodoo card in voodoo_start!");
			break;
	}

	if (tmumem1 != 0)
		v->tmu_config |= 0xc0;	// two TMUs

	v->chipmask = 0x01;

	/* set up the PCI FIFO */
	v->pci.fifo.size = 64*2;

	/* set up frame buffer */
	init_fbi(v, &v->fbi, fbmemsize << 20);

	v->fbi.rowpixels = v->fbi.width;

	v->tmu[0].ncc[0].palette = NULL;
	v->tmu[0].ncc[1].palette = NULL;
	v->tmu[1].ncc[0].palette = NULL;
	v->tmu[1].ncc[1].palette = NULL;
	v->tmu[0].ncc[0].palettea = NULL;
	v->tmu[0].ncc[1].palettea = NULL;
	v->tmu[1].ncc[0].palettea = NULL;
	v->tmu[1].ncc[1].palettea = NULL;

	v->tmu[0].ram = NULL;
	v->tmu[1].ram = NULL;
	v->tmu[0].lookup = NULL;
	v->tmu[1].lookup = NULL;

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
	v->reg[fbiInit0].u = (UINT32)((1 << 4) | (0x10 << 6));
	v->reg[fbiInit1].u = (UINT32)((1 << 1) | (1 << 8) | (1 << 12) | (2 << 20));
	v->reg[fbiInit2].u = (UINT32)((1 << 6) | (0x100 << 23));
	v->reg[fbiInit3].u = (UINT32)((2 << 13) | (0xf << 17));
	v->reg[fbiInit4].u = (UINT32)(1 << 0);

	/* do a soft reset to reset everything else */
	soft_reset(v);

	recompute_video_memory(v);
}

static void voodoo_shutdown() {
	if (v!=NULL) {
#ifdef C_ENABLE_VOODOO_OPENGL
		if (v->ogl)
			voodoo_ogl_shutdown(v);
#endif
		free(v->fbi.ram);
		if (v->tmu[0].ram != NULL) {
			free(v->tmu[0].ram);
			v->tmu[0].ram = NULL;
		}
		if (v->tmu[1].ram != NULL) {
			free(v->tmu[1].ram);
			v->tmu[1].ram = NULL;
		}
		v->active=false;
		triangle_worker_shutdown(v->tworker);
		delete v;
		v = NULL;
	}
}

static void voodoo_vblank_flush(void) {
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

static void voodoo_leave(void) {
#ifdef C_ENABLE_VOODOO_OPENGL
	if (v->ogl) {
		voodoo_ogl_leave(true);
	}
#endif
	v->active = false;
}

static void voodoo_activate(void) {
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
	PIC_AddEvent( Voodoo_VerticalTimer, v->draw.vfreq );

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
		if (!RENDER_StartUpdate()) return; // frameskip

#ifdef C_ENABLE_VOODOO_DEBUG
		rectangle r;
		r.min_x = r.min_y = 0;
		r.max_x = (int)v->fbi.width;
		r.max_y = (int)v->fbi.height;
#endif

		// draw all lines at once
		uint16_t* viewbuf = (uint16_t*)(v->fbi.ram +
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
	double time_in_frame = PIC_FullIndex() - v->draw.frame_start;
	double vfreq = v->draw.vfreq;
	if (vfreq <= 0.0) return false;
	if (v->clock_enabled && v->output_on) {
		if ((time_in_frame/vfreq) > 0.95) return true;
	} else if (v->output_on) {
		double rtime = time_in_frame/vfreq;
		rtime = fmod(rtime, 1.0);
		if (rtime > 0.95) return true;
	}
	return false;
}

static double Voodoo_GetVRetracePosition() {
	// TODO proper implementation
	double time_in_frame = PIC_FullIndex() - v->draw.frame_start;
	double vfreq = v->draw.vfreq;
	if (vfreq <= 0.0) return 0.0;
	if (v->clock_enabled && v->output_on) {
		return time_in_frame/vfreq;
	} else if (v->output_on) {
		double rtime = time_in_frame/vfreq;
		rtime = fmod(rtime, 1.0);
		return rtime;
	}
	return 0.0;
}

static double Voodoo_GetHRetracePosition() {
	// TODO proper implementation
	double time_in_frame = PIC_FullIndex() - v->draw.frame_start;
	double hfreq = v->draw.vfreq*100.0;
	if (hfreq <= 0.0) return 0.0;
	if (v->clock_enabled && v->output_on) {
		return time_in_frame/hfreq;
	} else if (v->output_on) {
		double rtime = time_in_frame/hfreq;
		rtime = fmod(rtime, 1.0);
		return rtime;
	}
	return 0.0;
}

static void Voodoo_UpdateScreen(void) {
	// abort drawing
	RENDER_EndUpdate(true);

	if ((!v->clock_enabled || !v->output_on) && v->draw.override_on) {
		// switching off
		PIC_RemoveEvents(Voodoo_VerticalTimer);
		voodoo_leave();

		VGA_SetOverride(false);
		v->draw.override_on=false;
	}

	if ((v->clock_enabled && v->output_on) && !v->draw.override_on) {
		// switching on
		PIC_RemoveEvents(Voodoo_VerticalTimer); // shouldn't be needed
		
		// TODO proper implementation of refresh rates and timings
		v->draw.vfreq = 1000.0f/60.0f;
		VGA_SetOverride(true);
		v->draw.override_on=true;

		voodoo_activate();

#ifdef C_ENABLE_VOODOO_OPENGL
		if (v->ogl) {
			v->ogl_dimchange = false;
		} else
#endif
		{
			RENDER_SetSize(v->fbi.width, v->fbi.height, 16, 1000.0f / v->draw.vfreq, 1.0, false, false);
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
		PIC_AddEvent(Voodoo_CheckScreenUpdate, 100.0f);
	}
}

static void Voodoo_UpdateScreenStart() {
	v->draw.screen_update_requested = true;
	if (!v->draw.screen_update_pending) {
		v->draw.screen_update_pending = true;
		PIC_AddEvent(Voodoo_CheckScreenUpdate, 0.0f);
	}
}

static void Voodoo_Startup();

static struct Voodoo_Real_PageHandler : public PageHandler {
	Voodoo_Real_PageHandler() { flags = PFLAG_NOCODE; }

	uint8_t readb([[maybe_unused]] PhysPt addr)
	{
		// LOG_MSG("VOODOO: readb at %x", addr);
		return 0xff;
	}

	void writeb([[maybe_unused]] PhysPt addr, [[maybe_unused]] uint8_t val)
	{
		// LOG_MSG("VOODOO: writeb at %x", addr);
	}

	uint16_t readw(PhysPt addr)
	{
		addr = PAGING_GetPhysicalAddress(addr);
		Bitu retval=voodoo_r((addr>>2)&0x3FFFFF);
		if (!(addr & 3))
			retval &= 0xffff;
		else if (!(addr & 1))
			retval >>= 16;
		else
			E_Exit("voodoo readw unaligned");
		return retval;
	}

	void writew(PhysPt addr, uint16_t val)
	{
		addr = PAGING_GetPhysicalAddress(addr);
		if (!(addr & 3))
			voodoo_w((addr>>2)&0x3FFFFF,(UINT32)val,0x0000ffff);
		else if (!(addr & 1))
			voodoo_w((addr>>2)&0x3FFFFF,(UINT32)(val<<16),0xffff0000);
		else
			E_Exit("voodoo writew unaligned");
	}

	uint32_t readd(PhysPt addr)
	{
		addr = PAGING_GetPhysicalAddress(addr);
		if (!(addr&3)) {
			return voodoo_r((addr>>2)&0x3FFFFF);
		} else if (!(addr&1)) {
			Bitu low = voodoo_r((addr>>2)&0x3FFFFF);
			Bitu high = voodoo_r(((addr>>2)+1)&0x3FFFFF);
			return (low>>16) | (high<<16);
		} else {
			E_Exit("voodoo readd unaligned");
			return 0xffffffff;
		}
	}

	void writed(PhysPt addr, uint32_t val)
	{
		addr = PAGING_GetPhysicalAddress(addr);
		if (!(addr&3)) {
			voodoo_w((addr>>2)&0x3FFFFF,(UINT32)val,0xffffffff);
		} else if (!(addr&1)) {
			voodoo_w((addr>>2)&0x3FFFFF,(UINT32)(val<<16),0xffff0000);
			voodoo_w(((addr>>2)+1)&0x3FFFFF,(UINT32)val,0x0000ffff);
		} else {
			uint32_t val1 = voodoo_r((addr >> 2) & 0x3FFFFF);
			uint32_t val2 = voodoo_r(((addr >> 2) + 1) & 0x3FFFFF);
			if ((addr & 3) == 1) {
				val1 = (val1&0xffffff) | ((val&0xff)<<24);
				val2 = (val2&0xff000000) | ((UINT32)val>>8);
			} else if ((addr & 3) == 3) {
				val1 = (val1&0xff) | ((val&0xffffff)<<8);
				val2 = (val2&0xffffff00) | ((UINT32)val>>24);
			}
			voodoo_w((addr>>2)&0x3FFFFF,val1,0xffffffff);
			voodoo_w(((addr>>2)+1)&0x3FFFFF,val2,0xffffffff);
		}
	}
} voodoo_real_pagehandler;

static struct Voodoo_Init_PageHandler : public PageHandler {
	Voodoo_Init_PageHandler() { flags = PFLAG_NOCODE; }

	uint8_t readb([[maybe_unused]] PhysPt addr)
	{
		return 0xff;
	}

	uint16_t readw(PhysPt addr)
	{
		Voodoo_Startup();
		return voodoo_real_pagehandler.readw(addr);
	}

	uint32_t readd(PhysPt addr)
	{
		Voodoo_Startup();
		return voodoo_real_pagehandler.readd(addr);
	}

	void writeb([[maybe_unused]] PhysPt addr, [[maybe_unused]] uint8_t val)
	{}

	void writew(PhysPt addr, uint16_t val)
	{
		Voodoo_Startup();
		voodoo_real_pagehandler.writew(addr, val);
	}

	void writed(PhysPt addr, uint32_t val)
	{
		Voodoo_Startup();
		voodoo_real_pagehandler.writed(addr, val);
	}

} voodoo_init_pagehandler;

#define VOODOO_INITIAL_LFB	0xd0000000
#define VOODOO_REG_PAGES	1024
#define VOODOO_LFB_PAGES	1024
#define VOODOO_TEX_PAGES	2048
#define VOODOO_PAGES (VOODOO_REG_PAGES+VOODOO_LFB_PAGES+VOODOO_TEX_PAGES)

#ifdef C_ENABLE_VOODOO_OPENGL
#define VOODOO_EMU_TYPE_OFF			0
#define VOODOO_EMU_TYPE_SOFTWARE	1
#define VOODOO_EMU_TYPE_ACCELERATED	2
#endif

static uint32_t voodoo_current_lfb;
static PageHandler* voodoo_pagehandler;

struct PCI_SSTDevice : public PCI_Device {
	enum { vendor = 0x121a, device_voodoo_1 = 0x0001, device_voodoo_2 = 0x0002 }; // 0x121a = 3dfx
	uint16_t oscillator_ctr, pci_ctr;

	PCI_SSTDevice() : PCI_Device(vendor,device_voodoo_1), oscillator_ctr(0), pci_ctr(0) { }

	void SetDeviceId(uint16_t _device_id)
	{
		device_id = _device_id;
	}

	Bits ParseReadRegister(uint8_t regnum)
	{
		//LOG_MSG("SST ParseReadRegister %x",regnum);
		switch (regnum) {
			case 0x4c:case 0x4d:case 0x4e:case 0x4f:
				LOG_MSG("SST ParseReadRegister STATUS %x",regnum);
				break;
			case 0x54:case 0x55:case 0x56:case 0x57:
				if (vtype == VOODOO_2) return -1;
				break;
		}
		return regnum;
	}

	bool OverrideReadRegister(uint8_t regnum, uint8_t* rval, uint8_t* rval_mask)
	{
		if (vtype != VOODOO_2) return false;
		switch (regnum) {
			case 0x54:
				oscillator_ctr++;
				pci_ctr--;
				*rval=(oscillator_ctr | ((pci_ctr<<16) & 0x0fff0000)) & 0xff;
				*rval_mask=0xff;
				return true;
			case 0x55:
				*rval=((oscillator_ctr | ((pci_ctr<<16) & 0x0fff0000)) >> 8) & 0xff;
				*rval_mask=0xff;
				return true;
			case 0x56:
				*rval=((oscillator_ctr | ((pci_ctr<<16) & 0x0fff0000)) >> 16) & 0xff;
				*rval_mask=0xff;
				return true;
			case 0x57:
				*rval=((oscillator_ctr | ((pci_ctr<<16) & 0x0fff0000)) >> 24) & 0xff;
				*rval_mask=0x0f;
				return true;
		}
		return false;
	}

	Bits ParseWriteRegister(uint8_t regnum, uint8_t value)
	{
		//LOG_MSG("SST ParseWriteRegister %x:=%x",regnum,value);
		if ((regnum>=0x14) && (regnum<0x28)) return -1;	// base addresses are read-only
		if ((regnum>=0x30) && (regnum<0x34)) return -1;	// expansion rom addresses are read-only
		switch (regnum) {
			case 0x10:
			        uint8_t PCI_GetCFGData(Bits pci_id,
			                               Bits pci_subfunction,
			                               uint8_t regnum);
			        return (PCI_GetCFGData(this->PCIId(), this->PCISubfunction(), 0x10) & 0x0f);
			case 0x11:
				return 0x00;
			case 0x12:
				return (value&0x00);	// -> 16mb addressable (whyever)
			case 0x13:
				voodoo_current_lfb = ((value<<24)&0xffff0000);
				return value;
			case 0x40:
				Voodoo_Startup();
				v->pci.init_enable = (UINT32)(value&7);
				break;
			case 0x41:
			case 0x42:
			case 0x43:
				return -1;
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
			default:
				break;
		}
		return value;
	}

	bool InitializeRegisters(uint8_t registers[256])
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

		// memBaseAddr: size is 16MB
		uint32_t address_space = (((uint32_t)VOODOO_INITIAL_LFB) & 0xfffffff0) |
		                         0x08; // memory space, within first
		                               // 4GB, prefetchable
		registers[0x10] = (uint8_t)(address_space & 0xff); // base
		                                                   // addres 0
		registers[0x11] = (uint8_t)((address_space >> 8) & 0xff);
		registers[0x12] = (uint8_t)((address_space >> 16) & 0xff);
		registers[0x13] = (uint8_t)((address_space >> 24) & 0xff);

		if (vtype == VOODOO_2) {
			registers[0x40] = 0x00;
			registers[0x41] = 0x40;	// voodoo2 revision ID (rev4)
			registers[0x42] = 0x01;
			registers[0x43] = 0x00;
		}

		return true;
	}
};

static void Voodoo_Startup() {
	// This function is called delayed after booting only once a game actually requests Voodoo support
	if (v) return;

	voodoo_init();

	memset(&v->draw, 0, sizeof(v->draw));
	v->draw.vfreq = 1000.0f/60.0f;

	memset(&v->tworker, 0, sizeof(v->tworker));
	v->tworker.use_threads = !!(vperf & 1);
	v->tworker.disable_bilinear_filter = !!(vperf & 2);

	// Switch the pagehandler now that v has been allocated and is in use
	voodoo_pagehandler = &voodoo_real_pagehandler;
	PAGING_InitTLB();
}

PageHandler* VOODOO_PCI_GetLFBPageHandler(Bitu page) {
	return (page >= (voodoo_current_lfb>>12) && page < (voodoo_current_lfb>>12) + VOODOO_PAGES ? voodoo_pagehandler : NULL);
}

void VOODOO_Destroy(Section* /*sec*/) {
	voodoo_shutdown();
}

void VOODOO_Init(Section* sec) {
	// Only active on SVGA machines
	if (machine != MCH_VGA || svgaCard == SVGA_None) return;

	Section_prop * section = static_cast<Section_prop *>(sec);

	vtype = VOODOO_1_DTMU;
	switch (section->Get_string("voodoo")[0])
	{
		case '1': vtype = VOODOO_1_DTMU; break; //12mb
		case '4': vtype = VOODOO_1; break; //4mb
		default: return; // disabled
	}

	sec->AddDestroyFunction(&VOODOO_Destroy,false);

	voodoo_current_lfb = (VOODOO_INITIAL_LFB & 0xffff0000);
	voodoo_pagehandler = &voodoo_init_pagehandler;
	vperf = (UINT8)section->Get_int("voodoo_perf");

	PCI_AddDevice(new PCI_SSTDevice());
}

#endif
