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
static void Normal(Bit8u * src,Bitu x,Bitu y,Bitu _dx,Bitu _dy) {
	Bit8u * dst=render.op.pixels+(render.normal.hindex[y]*render.op.pitch);
	Bitu line_size=LineSize<dbpp>(_dx) * (xdouble ? 2 : 1);
	src+=x;
	Bit8u * line;
	for (;_dy;_dy--) {
		if (sbpp == dbpp && !xdouble) {
			line=src;
			BituMove(dst,line,line_size);
		} else {
			Bit8u * line_dst=&normal_cache[0];
			Bit8u * real_dst=dst;
			line=line_dst;
			Bit8u * temp_src=src;
			for (Bitu tempx=_dx;tempx;tempx--) {
				Bitu val=ConvBPP<sbpp,dbpp>(LoadSrc<sbpp>(temp_src));
				AddDst<dbpp>(line_dst,val);
				AddDst<dbpp>(real_dst,val);
				if (xdouble) {
					AddDst<dbpp>(line_dst,val);
					AddDst<dbpp>(real_dst,val);
				}
			}
		}
		dst+=render.op.pitch;
		for (Bitu lines=render.normal.hlines[y++];lines;lines--) {
			BituMove(dst,line,line_size);
			dst+=render.op.pitch;
		}
		src+=render.src.pitch;
	}
}


static RENDER_Part_Handler Normal_SINGLE_8[4]={
	Normal<8,8 ,false>,Normal<8,16,false>,
	Normal<8,24,false>,Normal<8,32,false>,
};

static RENDER_Part_Handler Normal_DOUBLE_8[4]={
	Normal<8,8 ,true>,Normal<8,16,true>,
	Normal<8,24,true>,Normal<8,32,true>,
};
