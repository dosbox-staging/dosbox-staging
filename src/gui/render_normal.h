/*
 *  Copyright (C) 2002-2004  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

static Bit8u normal_cache[RENDER_MAXWIDTH*2*4];

template <Bitu sbpp,Bitu dbpp,bool xdouble>
static void Normal(Bit8u * src) {
	Bitu line_size=LineSize<dbpp>(render.src.width) * (xdouble ? 2 : 1);
	Bit8u * line;
	if (sbpp == dbpp && !xdouble) {
		line=src;
		BituMove(render.op.pixels,line,line_size);
	} else {
		Bit8u * line_dst=&normal_cache[0];
		Bit8u * real_dst=render.op.pixels;
		line=line_dst;
		Bit8u * temp_src=src;
		for (Bitu tempx=render.src.width;tempx;tempx--) {
			Bitu val=ConvBPP<sbpp,dbpp>(LoadSrc<sbpp>(temp_src));
			AddDst<dbpp>(line_dst,val);
			AddDst<dbpp>(real_dst,val);
			if (xdouble) {
				AddDst<dbpp>(line_dst,val);
				AddDst<dbpp>(real_dst,val);
			}
		}
	}
	render.op.pixels+=render.op.pitch;
	for (Bitu lines=render.normal.hlines[render.op.line++];lines;lines--) {
		BituMove(render.op.pixels,line,line_size);
		render.op.pixels+=render.op.pitch;
	}
}


static RENDER_Line_Handler Normal_8[4]={
	Normal<8,8 ,false>,Normal<8,16,false>,
	Normal<8,24,false>,Normal<8,32,false>,
};

static RENDER_Line_Handler Normal_2x_8[4]={
	Normal<8,8 ,true>,Normal<8,16,true>,
	Normal<8,24,true>,Normal<8,32,true>,
};
