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

#define AM2XBUF 16

static struct {
	Bits buf[AM2XBUF][4];
	Bitu buf_used;Bitu buf_pos;
	Bit8u cmd_data[4096];	//1024 lines should be enough?
	Bit8u * cmd_index;
	Bit8u * cache[4];
	Bitu cache_index;
} am2x;


static void AdvMame2x_AddLine(Bits s0,Bits s1,Bits s2) {
	if (s0<0) s0=0;
	if (s1<0) s1=0;
	if (s2<0) s2=0;
	if (s0>=(Bits)render.src.height) s0=render.src.height-1;
	if (s1>=(Bits)render.src.height) s1=render.src.height-1;
	if (s2>=(Bits)render.src.height) s2=render.src.height-1;
	Bitu pos=(am2x.buf_used+am2x.buf_pos)&(AM2XBUF-1);
	am2x.buf[pos][0]=s0;
	am2x.buf[pos][1]=s1;
	am2x.buf[pos][2]=s2;
	s0=s0 > s1 ? s0 : s1;
	s0=s0 > s2 ? s0 : s2;
	am2x.buf[pos][3]=s0;
	am2x.buf_used++;
}

static void AdvMame2x_CheckLines(Bits last) {
	Bitu lines=0;Bit8u * line_count=am2x.cmd_index++;
	while (am2x.buf_used) {
		if (am2x.buf[am2x.buf_pos][3]>last) break;
		*am2x.cmd_index++=am2x.buf[am2x.buf_pos][0]&3;
		*am2x.cmd_index++=am2x.buf[am2x.buf_pos][1]&3;
		*am2x.cmd_index++=am2x.buf[am2x.buf_pos][2]&3;
 		am2x.buf_used--;lines++;
		am2x.buf_pos=(am2x.buf_pos+1)&(AM2XBUF-1);
    }
	*line_count=lines;
}

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
static void AdvMame2x(Bit8u * src) {
	RENDER_TempLine=render_line_cache[render.op.line&3];
	am2x.cache[render.op.line&3]=src;
	Bitu lines=*am2x.cmd_index++;
	while (lines--) {
		Bit8u * src0=am2x.cache[*am2x.cmd_index++];
		Bit8u * src1=am2x.cache[*am2x.cmd_index++];
		Bit8u * src2=am2x.cache[*am2x.cmd_index++];
		AdvMame2x_line<sbpp,dbpp>(render.op.pixels,src0,src1,src2,render.src.width);
		render.op.pixels+=render.op.pitch;
	}
	render.op.line++;
}


static RENDER_Line_Handler AdvMame2x_8_Table[4]={
	AdvMame2x<8,8>,AdvMame2x<8,16>,AdvMame2x<8,24>,AdvMame2x<8,32>
};


#endif
