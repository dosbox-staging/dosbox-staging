/*
 * This file is part of the Scale2x project.
 *
 * Copyright (C) 2001-2002 Andrea Mazzoleni
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * In addition, as a special exception, Andrea Mazzoleni
 * gives permission to link the code of this program with
 * the MAME library (or with modified versions of MAME that use the
 * same license as MAME), and distribute linked combinations including
 * the two.  You must obey the GNU General Public License in all
 * respects for all of the code used other than MAME.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 */

/*
 * This file contains a C and MMX implentation of the Scale2x effect.
 *
 * You can found an high level description of the effect at :
 *
 * http://scale2x.sourceforge.net/scale2x.html
 *
 * Alternatively at the previous license terms, you are allowed to use this
 * code in your program with these conditions:
 * - the program is not used in commercial activities.
 * - the whole source code of the program is released with the binary.
 * - derivative works of the program are allowed.
 */

/* 
 * Made some changes to only support the 8-bit version.
 * Also added mulitple destination bpp targets.
 */

#ifndef __SCALE2X_H
#define __SCALE2X_H

#include <assert.h>

/***************************************************************************/
/* basic types */

typedef Bit8u scale2x_uint8;
typedef Bit16u scale2x_uint16;
typedef Bit32u scale2x_uint32;

#if !defined(__GNUC__) || !defined(__i386__)

#define SCALE2X_NORMAL 1

static void scale2x_line_8(scale2x_uint8* dst0, scale2x_uint8* dst1, const scale2x_uint8* src0, const scale2x_uint8* src1, const scale2x_uint8* src2, unsigned count)
{
	assert(count >= 2);

	/* first pixel */
	dst0[0] = src1[0];
	dst1[0] = src1[0];
	if (src1[1] == src0[0] && src2[0] != src0[0])
		dst0[1] = src0[0];
	else
		dst0[1] = src1[0];
	if (src1[1] == src2[0] && src0[0] != src2[0])
		dst1[1] = src2[0];
	else
		dst1[1] = src1[0];
	++src0;
	++src1;
	++src2;
	dst0 += 2;
	dst1 += 2;

	/* central pixels */
	count -= 2;
	while (count) {
		if (src1[-1] == src0[0] && src2[0] != src0[0] && src1[1] != src0[0])
			dst0[0] = src0[0];
		else
			dst0[0] = src1[0];
		if (src1[1] == src0[0] && src2[0] != src0[0] && src1[-1] != src0[0])
			dst0[1] = src0[0];
		else
			dst0[1] = src1[0];

		if (src1[-1] == src2[0] && src0[0] != src2[0] && src1[1] != src2[0])
			dst1[0] = src2[0];
		else
			dst1[0] = src1[0];
		if (src1[1] == src2[0] && src0[0] != src2[0] && src1[-1] != src2[0])
			dst1[1] = src2[0];
		else
			dst1[1] = src1[0];

		++src0;
		++src1;
		++src2;
		dst0 += 2;
		dst1 += 2;
		--count;
	}

	/* last pixel */
	if (src1[-1] == src0[0] && src2[0] != src0[0])
		dst0[0] = src0[0];
	else
		dst0[0] = src1[0];
	if (src1[-1] == src2[0] && src0[0] != src2[0])
		dst1[0] = src2[0];
	else
		dst1[0] = src1[0];
	dst0[1] = src1[0];
	dst1[1] = src1[0];
}

static void scale2x_line_16(scale2x_uint16* dst0, scale2x_uint16* dst1, const scale2x_uint8* src0, const scale2x_uint8* src1, const scale2x_uint8* src2, unsigned count)
{
	assert(count >= 2);

	/* first pixel */
	dst0[0] = render.pal.lookup.bpp16[src1[0]];
	dst1[0] = render.pal.lookup.bpp16[src1[0]];
	if (src1[1] == src0[0] && src2[0] != src0[0])
		dst0[1] = render.pal.lookup.bpp16[src0[0]];
	else
		dst0[1] = render.pal.lookup.bpp16[src1[0]];
	if (src1[1] == src2[0] && src0[0] != src2[0])
		dst1[1] = render.pal.lookup.bpp16[src2[0]];
	else
		dst1[1] = render.pal.lookup.bpp16[src1[0]];
	++src0;
	++src1;
	++src2;
	dst0 += 2;
	dst1 += 2;

	/* central pixels */
	count -= 2;
	while (count) {
		if (src1[-1] == src0[0] && src2[0] != src0[0] && src1[1] != src0[0])
			dst0[0] = render.pal.lookup.bpp16[src0[0]];
		else
			dst0[0] = render.pal.lookup.bpp16[src1[0]];
		if (src1[1] == src0[0] && src2[0] != src0[0] && src1[-1] != src0[0])
			dst0[1] = render.pal.lookup.bpp16[src0[0]];
		else
			dst0[1] = render.pal.lookup.bpp16[src1[0]];

		if (src1[-1] == src2[0] && src0[0] != src2[0] && src1[1] != src2[0])
			dst1[0] = render.pal.lookup.bpp16[src2[0]];
		else
			dst1[0] = render.pal.lookup.bpp16[src1[0]];
		if (src1[1] == src2[0] && src0[0] != src2[0] && src1[-1] != src2[0])
			dst1[1] = render.pal.lookup.bpp16[src2[0]];
		else
			dst1[1] = render.pal.lookup.bpp16[src1[0]];

		++src0;
		++src1;
		++src2;
		dst0 += 2;
		dst1 += 2;
		--count;
	}

	/* last pixel */
	if (src1[-1] == src0[0] && src2[0] != src0[0])
		dst0[0] = render.pal.lookup.bpp16[src0[0]];
	else
		dst0[0] = render.pal.lookup.bpp16[src1[0]];
	if (src1[-1] == src2[0] && src0[0] != src2[0])
		dst1[0] = render.pal.lookup.bpp16[src2[0]];
	else
		dst1[0] = render.pal.lookup.bpp16[src1[0]];
	dst0[1] = render.pal.lookup.bpp16[src1[0]];
	dst1[1] = render.pal.lookup.bpp16[src1[0]];
}

static void scale2x_line_32(scale2x_uint32* dst0, scale2x_uint32* dst1, const scale2x_uint8* src0, const scale2x_uint8* src1, const scale2x_uint8* src2, unsigned count)
{
	assert(count >= 2);

	/* first pixel */
	dst0[0] = render.pal.lookup.bpp32[src1[0]];
	dst1[0] = render.pal.lookup.bpp32[src1[0]];
	if (src1[1] == src0[0] && src2[0] != src0[0])
		dst0[1] = render.pal.lookup.bpp32[src0[0]];
	else
		dst0[1] = render.pal.lookup.bpp32[src1[0]];
	if (src1[1] == src2[0] && src0[0] != src2[0])
		dst1[1] = render.pal.lookup.bpp32[src2[0]];
	else
		dst1[1] = render.pal.lookup.bpp32[src1[0]];
	++src0;
	++src1;
	++src2;
	dst0 += 2;
	dst1 += 2;

	/* central pixels */
	count -= 2;
	while (count) {
		if (src1[-1] == src0[0] && src2[0] != src0[0] && src1[1] != src0[0])
			dst0[0] = render.pal.lookup.bpp32[src0[0]];
		else
			dst0[0] = render.pal.lookup.bpp32[src1[0]];
		if (src1[1] == src0[0] && src2[0] != src0[0] && src1[-1] != src0[0])
			dst0[1] = render.pal.lookup.bpp32[src0[0]];
		else
			dst0[1] = render.pal.lookup.bpp32[src1[0]];

		if (src1[-1] == src2[0] && src0[0] != src2[0] && src1[1] != src2[0])
			dst1[0] = render.pal.lookup.bpp32[src2[0]];
		else
			dst1[0] = render.pal.lookup.bpp32[src1[0]];
		if (src1[1] == src2[0] && src0[0] != src2[0] && src1[-1] != src2[0])
			dst1[1] = render.pal.lookup.bpp32[src2[0]];
		else
			dst1[1] = render.pal.lookup.bpp32[src1[0]];

		++src0;
		++src1;
		++src2;
		dst0 += 2;
		dst1 += 2;
		--count;
	}

	/* last pixel */
	if (src1[-1] == src0[0] && src2[0] != src0[0])
		dst0[0] = render.pal.lookup.bpp32[src0[0]];
	else
		dst0[0] = render.pal.lookup.bpp32[src1[0]];
	if (src1[-1] == src2[0] && src0[0] != src2[0])
		dst1[0] = render.pal.lookup.bpp32[src2[0]];
	else
		dst1[0] = render.pal.lookup.bpp32[src1[0]];
	dst0[1] = render.pal.lookup.bpp32[src1[0]];
	dst1[1] = render.pal.lookup.bpp32[src1[0]];
}

static void Scale2x_8(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	if (dy<3) return;
	Bit8u * dest=(Bit8u *)render.op.pixels+2*y*render.op.pitch;
	/* First line */
	scale2x_line_8(dest,dest+render.op.pitch,src,src,src+render.src.pitch,dx);
	dest+=render.op.pitch*2;
	src+=render.src.pitch;
	dy-=2;
	/* Middle part */
	for (;dy>0;dy--) {
		scale2x_line_8((Bit8u *)dest,(Bit8u *)(dest+render.op.pitch),src-render.src.pitch,src,src+render.src.pitch,dx);
		dest+=render.op.pitch*2;
		src+=render.src.pitch;
	}
	/* Last Line */
	scale2x_line_8((Bit8u *)dest,(Bit8u *)(dest+render.op.pitch),src-render.src.pitch,src,src,dx);
}

static void Scale2x_16(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	if (dy<3) return;
	Bit8u * dest=(Bit8u *)render.op.pixels+2*y*render.op.pitch;
	/* First line */
	scale2x_line_16((Bit16u *)dest,(Bit16u *)(dest+render.op.pitch),src,src,src+render.src.pitch,dx);
	dest+=render.op.pitch*2;
	src+=render.src.pitch;
	dy-=2;
	/* Middle part */
	for (;dy>0;dy--) {
		scale2x_line_16((Bit16u *)dest,(Bit16u *)(dest+render.op.pitch),src-render.src.pitch,src,src+render.src.pitch,dx);
		dest+=render.op.pitch*2;
		src+=render.src.pitch;
	}
	/* Last Line */
	scale2x_line_16((Bit16u *)dest,(Bit16u *)(dest+render.op.pitch),src-render.src.pitch,src,src,dx);
}

static void Scale2x_32(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	if (dy<3) return;
	Bit8u * dest=(Bit8u *)render.op.pixels+2*y*render.op.pitch;
	/* First line */
	scale2x_line_32((Bit32u *)dest,(Bit32u *)(dest+render.op.pitch),src,src,src+render.src.pitch,dx);
	dest+=render.op.pitch*2;
	src+=render.src.pitch;
	dy-=2;
	/* Middle part */
	for (;dy>0;dy--) {
		scale2x_line_32((Bit32u *)dest,(Bit32u *)(dest+render.op.pitch),src-render.src.pitch,src,src+render.src.pitch,dx);
		dest+=render.op.pitch*2;
		src+=render.src.pitch;
	}
	/* Last Line */
	scale2x_line_32((Bit32u *)dest,(Bit32u *)(dest+render.op.pitch),src-render.src.pitch,src,src,dx);
}

#else

#define SCALE2X_MMX 1

static __inline__ void scale2x_8_mmx_single(scale2x_uint8* dst, const scale2x_uint8* src0, const scale2x_uint8* src1, const scale2x_uint8* src2, unsigned count)
{
	assert(count >= 16);
	assert(count % 8 == 0);

	/* always do the first and last run */
	count -= 2*8;

	__asm__ __volatile__(
/* first run */
		/* set the current, current_pre, current_next registers */
		"movq 0(%1),%%mm0\n"
		"movq 0(%1),%%mm7\n"
		"movq 8(%1),%%mm1\n"
		"psllq $56,%%mm0\n"
		"psllq $56,%%mm1\n"
		"psrlq $56,%%mm0\n"
		"movq %%mm7,%%mm2\n"
		"movq %%mm7,%%mm3\n"
		"psllq $8,%%mm2\n"
		"psrlq $8,%%mm3\n"
		"por %%mm2,%%mm0\n"
		"por %%mm3,%%mm1\n"

		/* current_upper */
		"movq (%0),%%mm6\n"

		/* compute the upper-left pixel for dst on %%mm2 */
		/* compute the upper-right pixel for dst on %%mm4 */
		"movq %%mm0,%%mm2\n"
		"movq %%mm1,%%mm4\n"
		"movq %%mm0,%%mm3\n"
		"movq %%mm1,%%mm5\n"
		"pcmpeqb %%mm6,%%mm2\n"
		"pcmpeqb %%mm6,%%mm4\n"
		"pcmpeqb (%2),%%mm3\n"
		"pcmpeqb (%2),%%mm5\n"
		"pandn %%mm2,%%mm3\n"
		"pandn %%mm4,%%mm5\n"
		"movq %%mm0,%%mm2\n"
		"movq %%mm1,%%mm4\n"
		"pcmpeqb %%mm1,%%mm2\n"
		"pcmpeqb %%mm0,%%mm4\n"
		"pandn %%mm3,%%mm2\n"
		"pandn %%mm5,%%mm4\n"
		"movq %%mm2,%%mm3\n"
		"movq %%mm4,%%mm5\n"
		"pand %%mm6,%%mm2\n"
		"pand %%mm6,%%mm4\n"
		"pandn %%mm7,%%mm3\n"
		"pandn %%mm7,%%mm5\n"
		"por %%mm3,%%mm2\n"
		"por %%mm5,%%mm4\n"

		/* set *dst */
		"movq %%mm2,%%mm3\n"
		"punpcklbw %%mm4,%%mm2\n"
		"punpckhbw %%mm4,%%mm3\n"
		"movq %%mm2,(%3)\n"
		"movq %%mm3,8(%3)\n"

		/* next */
		"addl $8,%0\n"
		"addl $8,%1\n"
		"addl $8,%2\n"
		"addl $16,%3\n"

/* central runs */
		"shrl $3,%4\n"
		"jz 1f\n"

		"0:\n"

		/* set the current, current_pre, current_next registers */
		"movq -8(%1),%%mm0\n"
		"movq (%1),%%mm7\n"
		"movq 8(%1),%%mm1\n"
		"psrlq $56,%%mm0\n"
		"psllq $56,%%mm1\n"
		"movq %%mm7,%%mm2\n"
		"movq %%mm7,%%mm3\n"
		"psllq $8,%%mm2\n"
		"psrlq $8,%%mm3\n"
		"por %%mm2,%%mm0\n"
		"por %%mm3,%%mm1\n"

		/* current_upper */
		"movq (%0),%%mm6\n"

		/* compute the upper-left pixel for dst on %%mm2 */
		/* compute the upper-right pixel for dst on %%mm4 */
		"movq %%mm0,%%mm2\n"
		"movq %%mm1,%%mm4\n"
		"movq %%mm0,%%mm3\n"
		"movq %%mm1,%%mm5\n"
		"pcmpeqb %%mm6,%%mm2\n"
		"pcmpeqb %%mm6,%%mm4\n"
		"pcmpeqb (%2),%%mm3\n"
		"pcmpeqb (%2),%%mm5\n"
		"pandn %%mm2,%%mm3\n"
		"pandn %%mm4,%%mm5\n"
		"movq %%mm0,%%mm2\n"
		"movq %%mm1,%%mm4\n"
		"pcmpeqb %%mm1,%%mm2\n"
		"pcmpeqb %%mm0,%%mm4\n"
		"pandn %%mm3,%%mm2\n"
		"pandn %%mm5,%%mm4\n"
		"movq %%mm2,%%mm3\n"
		"movq %%mm4,%%mm5\n"
		"pand %%mm6,%%mm2\n"
		"pand %%mm6,%%mm4\n"
		"pandn %%mm7,%%mm3\n"
		"pandn %%mm7,%%mm5\n"
		"por %%mm3,%%mm2\n"
		"por %%mm5,%%mm4\n"

		/* set *dst */
		"movq %%mm2,%%mm3\n"
		"punpcklbw %%mm4,%%mm2\n"
		"punpckhbw %%mm4,%%mm3\n"
		"movq %%mm2,(%3)\n"
		"movq %%mm3,8(%3)\n"

		/* next */
		"addl $8,%0\n"
		"addl $8,%1\n"
		"addl $8,%2\n"
		"addl $16,%3\n"

		"decl %4\n"
		"jnz 0b\n"
		"1:\n"

/* final run */
		/* set the current, current_pre, current_next registers */
		"movq (%1),%%mm1\n"
		"movq (%1),%%mm7\n"
		"movq -8(%1),%%mm0\n"
		"psrlq $56,%%mm1\n"
		"psrlq $56,%%mm0\n"
		"psllq $56,%%mm1\n"
		"movq %%mm7,%%mm2\n"
		"movq %%mm7,%%mm3\n"
		"psllq $8,%%mm2\n"
		"psrlq $8,%%mm3\n"
		"por %%mm2,%%mm0\n"
		"por %%mm3,%%mm1\n"

		/* current_upper */
		"movq (%0),%%mm6\n"

		/* compute the upper-left pixel for dst on %%mm2 */
		/* compute the upper-right pixel for dst on %%mm4 */
		"movq %%mm0,%%mm2\n"
		"movq %%mm1,%%mm4\n"
		"movq %%mm0,%%mm3\n"
		"movq %%mm1,%%mm5\n"
		"pcmpeqb %%mm6,%%mm2\n"
		"pcmpeqb %%mm6,%%mm4\n"
		"pcmpeqb (%2),%%mm3\n"
		"pcmpeqb (%2),%%mm5\n"
		"pandn %%mm2,%%mm3\n"
		"pandn %%mm4,%%mm5\n"
		"movq %%mm0,%%mm2\n"
		"movq %%mm1,%%mm4\n"
		"pcmpeqb %%mm1,%%mm2\n"
		"pcmpeqb %%mm0,%%mm4\n"
		"pandn %%mm3,%%mm2\n"
		"pandn %%mm5,%%mm4\n"
		"movq %%mm2,%%mm3\n"
		"movq %%mm4,%%mm5\n"
		"pand %%mm6,%%mm2\n"
		"pand %%mm6,%%mm4\n"
		"pandn %%mm7,%%mm3\n"
		"pandn %%mm7,%%mm5\n"
		"por %%mm3,%%mm2\n"
		"por %%mm5,%%mm4\n"

		/* set *dst */
		"movq %%mm2,%%mm3\n"
		"punpcklbw %%mm4,%%mm2\n"
		"punpckhbw %%mm4,%%mm3\n"
		"movq %%mm2,(%3)\n"
		"movq %%mm3,8(%3)\n"

		"emms"

		: "+r" (src0), "+r" (src1), "+r" (src2), "+r" (dst), "+r" (count)
		:
		: "cc"
	);
}

static void scale2x_line_8_mmx(scale2x_uint8* dst0, scale2x_uint8* dst1, const scale2x_uint8* src0, const scale2x_uint8* src1, const scale2x_uint8* src2, unsigned count)
{
	assert(count >= 16);
	assert(count % 8 == 0);

	scale2x_8_mmx_single(dst0, src0, src1, src2, count);
	scale2x_8_mmx_single(dst1, src2, src1, src0, count);
}

static void Scale2x_8_mmx(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	if (dy<3) return;
	Bit8u * dest=(Bit8u *)render.op.pixels+2*y*render.op.pitch;
	/* First line */
	scale2x_line_8_mmx(dest,dest+render.op.pitch,src,src,src+render.src.pitch,dx);
	dest+=render.op.pitch*2;
	src+=render.src.pitch;
	dy-=2;
	/* Middle part */
	for (;dy>0;dy--) {
		scale2x_line_8_mmx((Bit8u *)dest,(Bit8u *)(dest+render.op.pitch),src-render.src.pitch,src,src+render.src.pitch,dx);
		dest+=render.op.pitch*2;
		src+=render.src.pitch;
	}
	/* Last Line */
	scale2x_line_8_mmx((Bit8u *)dest,(Bit8u *)(dest+render.op.pitch),src-render.src.pitch,src,src,dx);
}

#endif

static void Render_Scale2x_CallBack(Bitu width,Bitu height,Bitu bpp,Bitu pitch,Bitu flags) {
	if (!(flags & MODE_SET)) return;
	render.op.width=width;
	render.op.height=height;
	render.op.bpp=bpp;
	render.op.pitch=pitch;
	render.op.type=OP_Scale2x;
#if defined(SCALE2X_NORMAL)
	switch (bpp) {
	case 8:	
		render.op.part_handler=Scale2x_8;
		break;
	case 16:	
		render.op.part_handler=Scale2x_16;;
		break;
	case 32:	
		render.op.part_handler=Scale2x_32;
		break;
	default:
		E_Exit("RENDER:Unsupported display depth of %d",bpp);
		break;
	}
#elif defined(SCALE2X_MMX)
	assert (bpp==8);
	render.op.part_handler=Scale2x_8_mmx;
#endif
	RENDER_ResetPal();
}

#endif
