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
 * This algorithm was based on the scale2x/advmame2x effect.
 * http://scale2x.sourceforge.net/scale2x.html
 */

#ifndef __SCALE2X_H
#define __SCALE2X_H

template <Bitu sbpp,Bitu dbpp>
static void AdvMame2x_line(Bit8u * dst, const Bit8u * src0, const Bit8u * src1, const Bit8u * src2, Bitu count) {
	AddDst<dbpp>(dst,ConvBPP<sbpp,dbpp>(src1[0]));
	AddDst<dbpp>(dst,ConvBPP<sbpp,dbpp>((src1[1] == src0[0] && src2[0] != src0[0]) ? src0[0] : src1[0]));
	src0++;src1++;src2++;count-=2;
	/* central pixels */
	while (count) {
		AddDst<dbpp>(dst,ConvBPP<sbpp,dbpp>((src1[-1] == src0[0] && src2[0] != src0[0] && src1[1] != src0[0]) ? src0[0] : src1[0]));
		AddDst<dbpp>(dst,ConvBPP<sbpp,dbpp>((src1[1] == src0[0] && src2[0] != src0[0] && src1[-1] != src0[0]) ? src0[0] : src1[0]));
		src0++;src1++;src2++;count--;
	}
	/* last pixel */
	AddDst<dbpp>(dst,ConvBPP<sbpp,dbpp>((src1[-1] == src0[0] && src2[0] != src0[0]) ? src0[0] : src1[0]));
	AddDst<dbpp>(dst,ConvBPP<sbpp,dbpp>(src1[0]));
}

template <Bitu sbpp,Bitu dbpp>
static void AdvMame2x(Bit8u * src,Bitu x,Bitu y,Bitu _dx,Bitu _dy) {
	_dy=render.advmame2x.hindex[y+_dy];
	y=render.advmame2x.hindex[y];
	Bit8u * dest=render.op.pixels+render.op.pitch*y;
	src-=render.advmame2x.line_starts[y][0];
	src+=x;
	for (;y<_dy;y++) {
		Bit8u * src0=src+render.advmame2x.line_starts[y][0];
		Bit8u * src1=src+render.advmame2x.line_starts[y][1];
		Bit8u * src2=src+render.advmame2x.line_starts[y][2];
		AdvMame2x_line<sbpp,dbpp>(dest,src0,src1,src2,_dx);
		dest+=render.op.pitch;
	}
}


static RENDER_Part_Handler AdvMame2x_8_Table[4]={
	AdvMame2x<8,8>,AdvMame2x<8,16>,AdvMame2x<8,24>,AdvMame2x<8,32>
};


#endif
