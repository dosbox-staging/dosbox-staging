/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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

#define LOOPSIZE 4

#define SIZE_8	1
#define SIZE_16	2
#define SIZE_32	4

#define MAKE_8(FROM)	Bit8u val=*(Bit8u *)(FROM);
#define MAKE_16(FROM)	Bit16u val=render.pal.lookup.bpp16[*(Bit8u *)(FROM)];
#define MAKE_32(FROM)	Bit32u val=render.pal.lookup.bpp32[*(Bit8u *)(FROM)];

#define SAVE_8(WHERE)	*(Bit8u *)(WHERE)=val;
#define SAVE_16(WHERE)	*(Bit16u *)(WHERE)=val;
#define SAVE_32(WHERE)	*(Bit32u *)(WHERE)=val;

#define LINES_DN	1
#define LINES_DW	1
#define LINES_DH	2
#define LINES_DB	2

#define PIXELS_DN	1
#define PIXELS_DW	2
#define PIXELS_DH	1
#define PIXELS_DB	2

#define NORMAL_DN(BPP,FROM,DEST)								\
	MAKE_ ## BPP(FROM);											\
	SAVE_ ## BPP(DEST);											\

#define NORMAL_DW(BPP,FROM,DEST)								\
	MAKE_ ## BPP (FROM);										\
	SAVE_ ## BPP (DEST);										\
	SAVE_ ## BPP (DEST + SIZE_ ## BPP);							\

#define NORMAL_DH(BPP,FROM,DEST)								\
	MAKE_ ## BPP (FROM);										\
	SAVE_ ## BPP (DEST);										\
	SAVE_ ## BPP ((DEST) + render.op.pitch);					\
 

#define NORMAL_DB(BPP,FROM,DEST)								\
	MAKE_ ## BPP (FROM);										\
	SAVE_ ## BPP (DEST);										\
	SAVE_ ## BPP ((DEST)+SIZE_ ## BPP );						\
	SAVE_ ## BPP ((DEST)+render.op.pitch );						\
	SAVE_ ## BPP ((DEST)+render.op.pitch+SIZE_ ## BPP );		\


#define NORMAL_LOOP(COUNT,FUNC,BPP)					\
	if (COUNT>0) {NORMAL_ ## FUNC (BPP,(src+0),(dest+0 * PIXELS_ ## FUNC * SIZE_ ## BPP ))	}\
	if (COUNT>1) {NORMAL_ ## FUNC (BPP,(src+1),(dest+1 * PIXELS_ ## FUNC * SIZE_ ## BPP ))	}\
	if (COUNT>2) {NORMAL_ ## FUNC (BPP,(src+2),(dest+2 * PIXELS_ ## FUNC * SIZE_ ## BPP ))	}\
	if (COUNT>3) {NORMAL_ ## FUNC (BPP,(src+3),(dest+3 * PIXELS_ ## FUNC * SIZE_ ## BPP ))	}\
	if (COUNT>4) {NORMAL_ ## FUNC (BPP,(src+4),(dest+4 * PIXELS_ ## FUNC * SIZE_ ## BPP ))	}\
	if (COUNT>5) {NORMAL_ ## FUNC (BPP,(src+5),(dest+5 * PIXELS_ ## FUNC * SIZE_ ## BPP ))	}\
	if (COUNT>6) {NORMAL_ ## FUNC (BPP,(src+6),(dest+6 * PIXELS_ ## FUNC * SIZE_ ## BPP ))	}\
	if (COUNT>7) {NORMAL_ ## FUNC (BPP,(src+7),(dest+7 * PIXELS_ ## FUNC * SIZE_ ## BPP ))	}\

#define MAKENORMAL(FUNC,BPP)	\
static void Normal_ ## FUNC ## _ ##BPP(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {		\
	Bit8u * dest=(Bit8u *)render.op.pixels+y*LINES_ ## FUNC*render.op.pitch+x*PIXELS_ ## FUNC * SIZE_ ## BPP;		\
	Bitu next_src=render.src.pitch-dx;							\
	Bitu next_dest=(LINES_ ## FUNC*render.op.pitch) - (dx*PIXELS_ ## FUNC * SIZE_ ## BPP);	\
	dx/=LOOPSIZE;	\
	for (;dy>0;dy--) {											\
		for (Bitu tempx=dx;tempx>0;tempx--) {					\
			NORMAL_LOOP(LOOPSIZE,FUNC,BPP);							\
			src+=LOOPSIZE;dest+=LOOPSIZE*PIXELS_ ## FUNC * SIZE_ ## BPP;			\
		}														\
		src+=next_src;dest+=next_dest;							\
	}															\
}	

MAKENORMAL(DW,8);
MAKENORMAL(DB,8);
  
MAKENORMAL(DN,16);
MAKENORMAL(DW,16);
MAKENORMAL(DH,16);
MAKENORMAL(DB,16);

MAKENORMAL(DN,32);
MAKENORMAL(DW,32);
MAKENORMAL(DH,32);
MAKENORMAL(DB,32);

/* Special versions for the 8-bit ones that can do direct line copying */

static void Normal_DN_8(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+y*render.op.pitch+x;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=render.op.pitch-dx;
	Bitu rem=dx&3;dx>>=2;
	for (;dy>0;dy--) {
        Bitu tempx;
		for (tempx=dx;tempx>0;tempx--) {
			Bit32u temp=*(Bit32u *)src;src+=4;
			*(Bit32u *)dest=temp;
			dest+=4;
		}
		for (tempx=rem;tempx>0;tempx--) {
			*dest++=*src++;
		}
		src+=next_src;dest+=next_dest;
	}
}

static void Normal_DH_8(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy) {
	Bit8u * dest=(Bit8u *)render.op.pixels+2*y*render.op.pitch+x;
	Bitu next_src=render.src.pitch-dx;
	Bitu next_dest=(2*render.op.pitch)-dx;
	Bitu rem=dx&3;dx>>=2;
	for (;dy>0;dy--) {
	        Bitu tempx;
		for (tempx=dx;tempx>0;tempx--) {
			Bit32u temp=*(Bit32u *)src;src+=4;
			*(Bit32u *)dest=temp;
			*(Bit32u *)(dest+render.op.pitch)=temp;
			dest+=4;
		}
		for (tempx=rem;tempx>0;tempx--) {
			*dest=*src;
			*(dest+render.op.pitch)=*src;
			dest++;
		}
		src+=next_src;dest+=next_dest;
	}
}

static RENDER_Part_Handler Render_Normal_8_Table[4]= {
	Normal_DN_8,Normal_DW_8,Normal_DH_8,Normal_DB_8,
};

static RENDER_Part_Handler Render_Normal_16_Table[4]= {
	Normal_DN_16,Normal_DW_16,Normal_DH_16,Normal_DB_16,
};

static RENDER_Part_Handler Render_Normal_32_Table[4]= {
	Normal_DN_32,Normal_DW_32,Normal_DH_32,Normal_DB_32,
};

static void Render_Normal_CallBack(Bitu width,Bitu height,Bitu bpp,Bitu pitch,Bitu flags) {
	if (!(flags & MODE_SET)) return;
	render.op.width=width;
	render.op.height=height;
	render.op.bpp=bpp;
	render.op.pitch=pitch;
	render.op.type=OP_None;
	switch (bpp) {
	case 8:	
		render.op.part_handler=Render_Normal_8_Table[render.src.flags];
		break;
	case 16:	
		render.op.part_handler=Render_Normal_16_Table[render.src.flags];
		break;
	case 32:	
		render.op.part_handler=Render_Normal_32_Table[render.src.flags];
		break;
	default:
		E_Exit("RENDER:Unsupported display depth of %d",bpp);
		break;
	}
	RENDER_ResetPal();
}
