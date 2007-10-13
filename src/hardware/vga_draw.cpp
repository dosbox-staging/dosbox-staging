/*
 *  Copyright (C) 2002-2007  The DOSBox Team
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

/* $Id: vga_draw.cpp,v 1.86 2007-10-13 16:34:06 c2woody Exp $ */

#include <string.h>
#include <math.h>
#include "dosbox.h"
#include "video.h"
#include "render.h"
#include "../gui/render_scalers.h"
#include "vga.h"
#include "pic.h"

#define VGA_PARTS 4

typedef Bit8u * (* VGA_Line_Handler)(Bitu vidstart, Bitu line);

static VGA_Line_Handler VGA_DrawLine;
static Bit8u TempLine[SCALER_MAXWIDTH * 4];

static Bit8u * VGA_Draw_1BPP_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u *draw = (Bit32u *)TempLine;
	for (Bitu x=vga.draw.blocks;x>0;x--, vidstart++) {
		Bitu val = base[(vidstart & (8 * 1024 -1))];
		*draw++=CGA_2_Table[val >> 4];
		*draw++=CGA_2_Table[val & 0xf];
	}
	return TempLine;
}

static Bit8u * VGA_Draw_2BPP_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val = base[vidstart & vga.tandy.addr_mask];
		vidstart++;
		*draw++=CGA_4_Table[val];
	}
	return TempLine;
}

static Bit8u * VGA_Draw_2BPPHiRes_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val1 = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		Bitu val2 = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		*draw++=CGA_4_HiRes_Table[(val1>>4)|(val2&0xf0)];
		*draw++=CGA_4_HiRes_Table[(val1&0x0f)|((val2&0x0f)<<4)];
	}
	return TempLine;
}

static Bitu temp[643]={0};

static Bit8u * VGA_Draw_CGA16_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	const Bit8u *reader = base + vidstart;
	Bit32u * draw=(Bit32u *)TempLine;
	//Generate a temporary bitline to calculate the avarage
	//over bit-2  bit-1  bit  bit+1.
	//Combine this number with the current colour to get 
	//an unigue index in the pallete. Or it with bit 7 as they are stored 
	//in the upperpart to keep them from interfering the regular cga stuff

	for(Bitu x = 0; x < 640; x++)
		temp[x+2] = (( reader[(x>>3)] >> (7-(x&7)) )&1) << 4;
		//shift 4 as that is for the index.
	Bitu i = 0,temp1,temp2,temp3,temp4;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val1 = *reader++;
		Bitu val2 = val1&0xf;
		val1 >>= 4;

		temp1 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		temp2 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		temp3 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		temp4 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;

		*draw++ = 0x80808080|(temp1|val1) |
		          ((temp2|val1) << 8) |
		          ((temp3|val1) <<16) |
		          ((temp4|val1) <<24);
		temp1 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		temp2 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		temp3 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		temp4 = temp[i] + temp[i+1] + temp[i+2] + temp[i+3]; i++;
		*draw++ = 0x80808080|(temp1|val2) |
		          ((temp2|val2) << 8) |
		          ((temp3|val2) <<16) |
		          ((temp4|val2) <<24);
	}
	return TempLine;
}

static Bit8u * VGA_Draw_4BPP_Line(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val1 = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		Bitu val2 = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		*draw++=(val1 & 0x0f) << 8  |
				(val1 & 0xf0) >> 4  |
				(val2 & 0x0f) << 24 |
				(val2 & 0xf0) << 12;
	}
	return TempLine;
}

static Bit8u * VGA_Draw_4BPP_Line_Double(Bitu vidstart, Bitu line) {
	const Bit8u *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		*draw++=(val & 0xf0) >> 4  |
				(val & 0xf0) << 4  |
				(val & 0x0f) << 16 |
				(val & 0x0f) << 24;
	}
	return TempLine;
}

#ifdef VGA_KEEP_CHANGES
static Bit8u * VGA_Draw_Changes_Line(Bitu vidstart, Bitu line) {
	Bitu checkMask = vga.changes.checkMask;
	Bit8u *map = vga.changes.map;
	Bitu start = (vidstart >> VGA_CHANGE_SHIFT);
	Bitu end = ((vidstart + vga.draw.line_length ) >> VGA_CHANGE_SHIFT);
	for (; start <= end;start++) {
		if ( map[start] & checkMask ) {
			Bit8u *ret = &vga.draw.linear_base[ vidstart & vga.draw.linear_mask ];
#if !defined(C_UNALIGNED_MEMORY)
			if (GCC_UNLIKELY( ((Bitu)ret) & (sizeof(Bitu)-1)) ) {
				memcpy( TempLine, ret, vga.draw.line_length );
				return TempLine;
			}
#endif
			return ret;
		}
	}
//	memset( TempLine, 0x30, vga.changes.lineWidth );
//	return TempLine;
	return 0;
}

#endif

static Bit8u * VGA_Draw_Linear_Line(Bitu vidstart, Bitu line) {
	Bit8u *ret = &vga.draw.linear_base[ vidstart & vga.draw.linear_mask ];
#if !defined(C_UNALIGNED_MEMORY)
	if (GCC_UNLIKELY( ((Bitu)ret) & (sizeof(Bitu)-1)) ) {
		memcpy( TempLine, ret, vga.draw.line_length );
		return TempLine;
	}
#endif
	return ret;
}

//Test version, might as well keep it
static Bit8u * VGA_Draw_Chain_Line(Bitu vidstart, Bitu line) {
	Bitu i = 0;
	for ( i = 0; i < vga.draw.width;i++ ) {
		Bitu addr = vidstart + i;
		TempLine[i] = vga.mem.linear[((addr&~3)<<2)+(addr&3)];
	}
	return TempLine;
}

static Bit8u * VGA_Draw_VGA_Line_HWMouse( Bitu vidstart, Bitu line) {
	if(vga.s3.hgc.curmode & 0x1) {
		Bitu lineat = vidstart / vga.draw.width;
		if((lineat < vga.s3.hgc.originy) || (lineat > (vga.s3.hgc.originy + 63U))) {
			return &vga.mem.linear[ vidstart ];
		} else {
			memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width);
			/* Draw mouse cursor */
			Bits moff = ((Bits)lineat - (Bits)vga.s3.hgc.originy) + (Bits)vga.s3.hgc.posy;
			if(moff>63) return &vga.mem.linear[ vidstart ];
			if(moff<0) moff+=64;
			Bitu xat = vga.s3.hgc.originx;
			Bitu m, mapat;
			Bits r, z;
			mapat = 0;

			Bitu mouseaddr = (Bit32u)vga.s3.hgc.startaddr * (Bit32u)1024;
			mouseaddr+=(moff * 16);

			Bit16u bitsA, bitsB;
			Bit8u mappoint;
			for(m=0;m<4;m++) {
				bitsA = *(Bit16u *)&vga.mem.linear[mouseaddr];
				mouseaddr+=2;
				bitsB = *(Bit16u *)&vga.mem.linear[mouseaddr];
				mouseaddr+=2;
				z = 7;
				for(r=15;r>=0;--r) {
					mappoint = (((bitsA >> z) & 0x1) << 1) | ((bitsB >> z) & 0x1);
					if(mapat >= vga.s3.hgc.posx) {
						switch(mappoint) {
					case 0:
						TempLine[xat] = vga.s3.hgc.backstack[0];
						break;
					case 1:
						TempLine[xat] = vga.s3.hgc.forestack[0];
						break;
					case 2:
						//Transparent
						break;
					case 3:
						// Invert screen data
						TempLine[xat] = ~TempLine[xat];
						break;
				}
				xat++;
					}
					mapat++;
					--z;
					if(z<0) z=15;
				}
			}
			return TempLine;
		}
	} else {
		/* HW Mouse not enabled, use the tried and true call */
		return &vga.mem.linear[ vidstart ];
	}
}

static Bit8u * VGA_Draw_LIN16_Line_HWMouse(Bitu vidstart,   Bitu line) {
	if(vga.s3.hgc.curmode & 0x1) {
		Bitu lineat = (vidstart >> 1) / vga.draw.width;
		if((lineat < vga.s3.hgc.originy) || (lineat > (vga.s3.hgc.originy + 63U))) {
			return &vga.mem.linear[ vidstart ];
		} else {
			memcpy(TempLine, &vga.mem.linear[ vidstart ], 2*vga.draw.width);
			/* Draw mouse cursor */
			Bits moff = ((Bits)lineat - (Bits)vga.s3.hgc.originy) + (Bits)vga.s3.hgc.posy;
			if(moff>63) return &vga.mem.linear[ vidstart ];
			if(moff<0) moff+=64;
			Bitu xat = 2*vga.s3.hgc.originx;
			Bitu m, mapat;
			Bits r, z;
			mapat = 0;

			Bitu mouseaddr = (Bit32u)vga.s3.hgc.startaddr * (Bit32u)1024;
			mouseaddr+=(moff * 16);

			Bit16u bitsA, bitsB;
			Bit8u mappoint;
			for(m=0;m<4;m++) {
				bitsA = *(Bit16u *)&vga.mem.linear[mouseaddr];
				mouseaddr+=2;
				bitsB = *(Bit16u *)&vga.mem.linear[mouseaddr];
				mouseaddr+=2;
				z = 7;
				for(r=15;r>=0;--r) {
					mappoint = (((bitsA >> z) & 0x1) << 1) | ((bitsB >> z) & 0x1);
					if(mapat >= vga.s3.hgc.posx) {
						switch(mappoint) {
					case 0:
						TempLine[xat]   = vga.s3.hgc.backstack[0];
						TempLine[xat+1] = vga.s3.hgc.backstack[1];
						break;
					case 1:
						TempLine[xat]   = vga.s3.hgc.forestack[0];
						TempLine[xat+1] = vga.s3.hgc.forestack[1];
						break;
					case 2:
						//Transparent
						break;
					case 3:
						// Invert screen data
						TempLine[xat]   = ~TempLine[xat];
						TempLine[xat+1] = ~TempLine[xat+1];
						break;
				}
				xat+=2;
					}
					mapat++;
					--z;
					if(z<0) z=15;
				}
			}
			return TempLine;
		}
	} else {
		/* HW Mouse not enabled, use the tried and true call */
		return &vga.mem.linear[ vidstart ];
	}
}

static Bit8u * VGA_Draw_LIN32_Line_HWMouse(Bitu vidstart,   Bitu line) {
	if(vga.s3.hgc.curmode & 0x1) {
		Bitu lineat = (vidstart >> 2) / vga.draw.width;
		if((lineat < vga.s3.hgc.originy) || (lineat > (vga.s3.hgc.originy + 63U))) {
			return &vga.mem.linear[ vidstart ];
		} else {
			memcpy(TempLine, &vga.mem.linear[ vidstart ], 4*vga.draw.width);
			/* Draw mouse cursor */
			Bits moff = ((Bits)lineat - (Bits)vga.s3.hgc.originy) + (Bits)vga.s3.hgc.posy;
			if(moff>63) return &vga.mem.linear[ vidstart ];
			if(moff<0) moff+=64;
			Bitu xat = 4*vga.s3.hgc.originx;
			Bitu m, mapat;
			Bits r, z;
			mapat = 0;

			Bitu mouseaddr = (Bit32u)vga.s3.hgc.startaddr * (Bit32u)1024;
			mouseaddr+=(moff * 16);

			Bit16u bitsA, bitsB;
			Bit8u mappoint;
			for(m=0;m<4;m++) {
				bitsA = *(Bit16u *)&vga.mem.linear[mouseaddr];
				mouseaddr+=2;
				bitsB = *(Bit16u *)&vga.mem.linear[mouseaddr];
				mouseaddr+=2;
				z = 7;
				for(r=15;r>=0;--r) {
					mappoint = (((bitsA >> z) & 0x1) << 1) | ((bitsB >> z) & 0x1);
					if(mapat >= vga.s3.hgc.posx) {
						switch(mappoint) {
					case 0:
						TempLine[xat]   = vga.s3.hgc.backstack[0];
						TempLine[xat+1] = vga.s3.hgc.backstack[1];
						TempLine[xat+2] = vga.s3.hgc.backstack[2];
						TempLine[xat+3] = 255;
						break;
					case 1:
						TempLine[xat]   = vga.s3.hgc.forestack[0];
						TempLine[xat+1] = vga.s3.hgc.forestack[1];
						TempLine[xat+2] = vga.s3.hgc.forestack[2];
						TempLine[xat+3] = 255;
						break;
					case 2:
						//Transparent
						break;
					case 3:
						// Invert screen data
						TempLine[xat]   = ~TempLine[xat];
						TempLine[xat+1] = ~TempLine[xat+1];
						TempLine[xat+2] = ~TempLine[xat+2];
						TempLine[xat+2] = ~TempLine[xat+3];
						break;
				}
				xat+=4;
					}
					mapat++;
					--z;
					if(z<0) z=15;
				}
			}
			return TempLine;
		}
	} else {
		/* HW Mouse not enabled, use the tried and true call */
		return &vga.mem.linear[ vidstart ];
	}
}

static Bit32u FontMask[2]={0xffffffff,0x0};
static Bit8u * VGA_TEXT_Draw_Line(Bitu vidstart, Bitu line) {
	Bits font_addr;
	Bit32u * draw=(Bit32u *)TempLine;
	const Bit8u *vidmem = &vga.tandy.draw_base[vidstart];
	for (Bitu cx=0;cx<vga.draw.blocks;cx++) {
		Bitu chr=vidmem[cx*2];
		Bitu col=vidmem[cx*2+1];
		Bitu font=vga.draw.font_tables[(col >> 3)&1][chr*32+line];
		Bit32u mask1=TXT_Font_Table[font>>4] & FontMask[col >> 7];
		Bit32u mask2=TXT_Font_Table[font&0xf] & FontMask[col >> 7];
		Bit32u fg=TXT_FG_Table[col&0xf];
		Bit32u bg=TXT_BG_Table[col>>4];
		*draw++=fg&mask1 | bg&~mask1;
		*draw++=fg&mask2 | bg&~mask2;
	}
	if (!vga.draw.cursor.enabled || !(vga.draw.cursor.count&0x8)) goto skip_cursor;
	font_addr = (vga.draw.cursor.address-vidstart) >> 1;
	if (font_addr>=0 && font_addr<(Bits)vga.draw.blocks) {
		if (line<vga.draw.cursor.sline) goto skip_cursor;
		if (line>vga.draw.cursor.eline) goto skip_cursor;
		draw=(Bit32u *)&TempLine[font_addr*8];
		Bit32u att=TXT_FG_Table[vga.tandy.draw_base[vga.draw.cursor.address+1]&0xf];
		*draw++=att;*draw++=att;
	}
skip_cursor:
	return TempLine;
}

static Bit8u * VGA_TEXT_Draw_Line_9(Bitu vidstart, Bitu line) {
	Bits font_addr;
	Bit8u * draw=(Bit8u *)TempLine;
	Bit8u pel_pan=vga.config.pel_panning;
	if ((vga.attr.mode_control&0x20) && (vga.draw.lines_done>=vga.draw.split_line)) pel_pan=0;
	const Bit8u *vidmem = &vga.tandy.draw_base[vidstart];
	Bit8u chr=vidmem[0];
	Bit8u col=vidmem[1];
	Bit8u font=(vga.draw.font_tables[(col >> 3)&1][chr*32+line])<<pel_pan;
	Bit8u fg=col&0xf;
	Bit8u bg=(Bit8u)(TXT_BG_Table[col>>4]&0xff);
	Bitu draw_blocks=vga.draw.blocks;
	draw_blocks++;
	for (Bitu cx=1;cx<draw_blocks;cx++) {
		if (pel_pan) {
			chr=vidmem[cx*2];
			col=vidmem[cx*2+1];
			font|=vga.draw.font_tables[(col >> 3)&1][chr*32+line]>>(8-pel_pan);
			fg=col&0xf;
			bg=(Bit8u)(TXT_BG_Table[col>>4]&0xff);
		} else {
			chr=vidmem[(cx-1)*2];
			col=vidmem[(cx-1)*2+1];
			font=vga.draw.font_tables[(col >> 3)&1][chr*32+line];
			fg=col&0xf;
			bg=(Bit8u)(TXT_BG_Table[col>>4]&0xff);
		}
		if (FontMask[col>>7]==0) font=0;
		*draw++=(font&0x80)?fg:bg;		*draw++=(font&0x40)?fg:bg;
		*draw++=(font&0x20)?fg:bg;		*draw++=(font&0x10)?fg:bg;
		*draw++=(font&0x08)?fg:bg;		*draw++=(font&0x04)?fg:bg;
		*draw++=(font&0x02)?fg:bg;
		Bit8u last=(font&0x01)?fg:bg;
		*draw++=last;
		*draw++=((chr<0xc0) || (chr>0xdf)) ? bg : last;
		if (pel_pan)
			font=(vga.draw.font_tables[(col >> 3)&1][chr*32+line])<<pel_pan;
	}
	if (!vga.draw.cursor.enabled || !(vga.draw.cursor.count&0x8)) goto skip_cursor;
	font_addr = (vga.draw.cursor.address-vidstart) >> 1;
	if (font_addr>=0 && font_addr<(Bits)vga.draw.blocks) {
		if (line<vga.draw.cursor.sline) goto skip_cursor;
		if (line>vga.draw.cursor.eline) goto skip_cursor;
		draw=&TempLine[font_addr*9];
		Bit8u fg=vga.tandy.draw_base[vga.draw.cursor.address+1]&0xf;
		*draw++=fg;		*draw++=fg;		*draw++=fg;		*draw++=fg;
		*draw++=fg;		*draw++=fg;		*draw++=fg;		*draw++=fg;
	}
skip_cursor:
	return TempLine;
}

static void VGA_VerticalDisplayEnd(Bitu val) {
//	vga.config.retrace=true;
	vga.config.real_start=vga.config.display_start & ((2*1024*1024)-1);
}

static void VGA_HorizontalTimer(void) {

}

#ifdef VGA_KEEP_CHANGES
static INLINE void VGA_ChangesEnd(void ) {
	if ( vga.changes.active ) {
//		vga.changes.active = false;
		Bitu end = vga.draw.address >> VGA_CHANGE_SHIFT;
		Bitu total = 4 + end - vga.changes.start;
		Bit32u clearMask = vga.changes.clearMask;
		total >>= 2;
		Bit32u *clear = (Bit32u *)&vga.changes.map[  vga.changes.start & ~3 ];
		while ( total-- ) {
			clear[0] &= clearMask;
			clear++;
		}
	}
}
#endif


static void VGA_DrawPart(Bitu lines) {
	while (lines--) {
		Bit8u * data=VGA_DrawLine( vga.draw.address, vga.draw.address_line );
		RENDER_DrawLine(data);
		vga.draw.address_line++;
		if (vga.draw.address_line>=vga.draw.address_line_total) {
			vga.draw.address_line=0;
			vga.draw.address+=vga.draw.address_add;
		}
//		vga.draw.lines_done++;
		if (vga.draw.split_line==vga.draw.lines_done++) {
#ifdef VGA_KEEP_CHANGES
			VGA_ChangesEnd( );
#endif
			vga.draw.address=0;
			if(!(vga.attr.mode_control&0x20))
				vga.draw.address += vga.config.pel_panning;
			vga.draw.address_line=0;
#ifdef VGA_KEEP_CHANGES
			vga.changes.start = vga.draw.address >> VGA_CHANGE_SHIFT;
#endif
		}
	}
	if (--vga.draw.parts_left) {
		PIC_AddEvent(VGA_DrawPart,(float)vga.draw.delay.parts,
			 (vga.draw.parts_left!=1) ? vga.draw.parts_lines  : (vga.draw.lines_total - vga.draw.lines_done));
	} else {
#ifdef VGA_KEEP_CHANGES
		VGA_ChangesEnd();
#endif
		RENDER_EndUpdate();
	}
}

void VGA_SetBlinking(Bitu enabled) {
	Bitu b;
	LOG(LOG_VGA,LOG_NORMAL)("Blinking %d",enabled);
	if (enabled) {
		b=0;vga.draw.blinking=1; //used to -1 but blinking is unsigned
		vga.attr.mode_control|=0x08;
		vga.tandy.mode_control|=0x20;
	} else {
		b=8;vga.draw.blinking=0;
		vga.attr.mode_control&=~0x08;
		vga.tandy.mode_control&=~0x20;
	}
	for (Bitu i=0;i<8;i++) TXT_BG_Table[i+8]=(b+i) | ((b+i) << 8)| ((b+i) <<16) | ((b+i) << 24);
}

#ifdef VGA_KEEP_CHANGES
static void INLINE VGA_ChangesStart( void ) {
	vga.changes.start = vga.draw.address >> VGA_CHANGE_SHIFT;
	vga.changes.last = vga.changes.start;
	if ( vga.changes.lastAddress != vga.draw.address ) {
//		LOG_MSG("Address");
		VGA_DrawLine = VGA_Draw_Linear_Line;
		vga.changes.lastAddress = vga.draw.address;
	} else if ( render.fullFrame ) {
//		LOG_MSG("Full Frame");
		VGA_DrawLine = VGA_Draw_Linear_Line;
	} else {
//		LOG_MSG("Changes");
		VGA_DrawLine = VGA_Draw_Changes_Line;
	}
	vga.changes.active = true;
	vga.changes.checkMask = vga.changes.writeMask;
	vga.changes.clearMask = ~( 0x01010101 << (vga.changes.frame & 7));
	vga.changes.frame++;
	vga.changes.writeMask = 1 << (vga.changes.frame & 7);
}
#endif


static void VGA_VerticalTimer(Bitu val) {
	double error = vga.draw.delay.framestart;
	vga.draw.delay.framestart = PIC_FullIndex();
	error = vga.draw.delay.framestart - error - vga.draw.delay.vtotal;
//	 if (abs(error) > 0.001 ) 
//		 LOG_MSG("vgaerror: %f",error);
	PIC_AddEvent( VGA_VerticalTimer, (float)vga.draw.delay.vtotal );
	PIC_AddEvent( VGA_VerticalDisplayEnd, (float)vga.draw.delay.vrstart );
	if ( GCC_UNLIKELY( vga.draw.parts_left )) {
		LOG(LOG_VGAMISC,LOG_NORMAL)( "Parts left: %d", vga.draw.parts_left );
		PIC_RemoveEvents( &VGA_DrawPart );
		RENDER_EndUpdate();
		vga.draw.parts_left = 0;
	}
	//Check if we can actually render, else skip the rest
	if (!RENDER_StartUpdate())
		return;
	//TODO Maybe check for an active frame on parts_left and clear that first?
	vga.draw.parts_left = vga.draw.parts_total;
	vga.draw.lines_done = 0;
//	vga.draw.address=vga.config.display_start;
	vga.draw.address = vga.config.real_start;
	vga.draw.address_line = vga.config.hlines_skip;
	vga.draw.split_line = (vga.config.line_compare/vga.draw.lines_scaled);
	// go figure...
	if (machine==MCH_EGA) vga.draw.split_line*=2;
//	if (machine==MCH_EGA) vga.draw.split_line = ((((vga.config.line_compare&0x5ff)+1)*2-1)/vga.draw.lines_scaled);
	switch (vga.mode) {
	case M_EGA:
	case M_LIN4:
		vga.draw.address *= 8;
		vga.draw.address += vga.config.pel_panning;
#ifdef VGA_KEEP_CHANGES
		VGA_ChangesStart();
#endif
		break;
	case M_VGA:
		if(vga.config.compatible_chain4 && (vga.crtc.underline_location & 0x40)) {
			vga.draw.linear_base = vga.mem.linear + VGA_CACHE_OFFSET;
			vga.draw.linear_mask = 0xffff;
		} else {
			vga.draw.linear_base = vga.mem.linear;
			vga.draw.linear_mask = VGA_MEMORY - 1;
		}
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		vga.draw.address *= 4;
		vga.draw.address += vga.config.pel_panning;
#ifdef VGA_KEEP_CHANGES
		VGA_ChangesStart();
#endif
		break;
	case M_TEXT:
		if ((IS_VGA_ARCH) && (svgaCard==SVGA_None)) vga.draw.address = vga.config.real_start * 2;
		else vga.draw.address = vga.config.display_start * 2;
	case M_TANDY_TEXT:
	case M_HERC_TEXT:
		vga.draw.cursor.address=vga.config.cursor_start*2;
		vga.draw.cursor.count++;
		/* check for blinking and blinking change delay */
		FontMask[1]=(vga.draw.blinking & (vga.draw.cursor.count >> 4)) ?
			0 : 0xffffffff;
		break;
	case M_HERC_GFX:
		break;
	case M_CGA4:case M_CGA2:
		vga.draw.address=(vga.draw.address*2)&0x1fff;
		break;
	case M_CGA16:
	case M_TANDY2:case M_TANDY4:case M_TANDY16:
		vga.draw.address *= 2;
		break;
	}
	//VGA_DrawPart( vga.draw.parts_lines );
	PIC_AddEvent(VGA_DrawPart,(float)vga.draw.delay.parts,vga.draw.parts_lines);
//	PIC_AddEvent(VGA_DrawPart,(float)(vga.draw.delay.parts/2),vga.draw.parts_lines); //Else tearline in Tyrian and second reality
	if (GCC_UNLIKELY(machine==MCH_EGA)) {
		if (!(vga.crtc.vertical_retrace_end&0x20)) {
			PIC_ActivateIRQ(2);
			vga.draw.vret_triggered=true;
		}
	}
}

void VGA_CheckScanLength(void) {
	switch (vga.mode) {
	case M_EGA:
	case M_LIN4:
		vga.draw.address_add=vga.config.scan_len*16;
		break;
	case M_VGA:
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		vga.draw.address_add=vga.config.scan_len*8;
		break;
	case M_TEXT:
		vga.draw.address_add=vga.config.scan_len*4;
		break;
	case M_CGA2:
	case M_CGA4:
	case M_CGA16:
		vga.draw.address_add=80;
		return;
	case M_TANDY2:
		vga.draw.address_add=vga.draw.blocks/4;
		break;
	case M_TANDY4:
		vga.draw.address_add=vga.draw.blocks;
		break;
	case M_TANDY16:
		vga.draw.address_add=vga.draw.blocks;
		break;
	case M_TANDY_TEXT:
		vga.draw.address_add=vga.draw.blocks*2;
		break;
	case M_HERC_TEXT:
		vga.draw.address_add=vga.draw.blocks*2;
		break;
	case M_HERC_GFX:
		vga.draw.address_add=vga.draw.blocks;
		break;
	}
}

void VGA_ActivateHardwareCursor(void) {
	if(vga.s3.hgc.curmode & 0x1) {
		switch(vga.mode) {
		case M_LIN32:
			VGA_DrawLine=VGA_Draw_LIN32_Line_HWMouse;
			break;
		case M_LIN15:
		case M_LIN16:
			VGA_DrawLine=VGA_Draw_LIN16_Line_HWMouse;
			break;
		default:
			VGA_DrawLine=VGA_Draw_VGA_Line_HWMouse;
		}
	} else {
		VGA_DrawLine=VGA_Draw_Linear_Line;
	}
}

void VGA_SetupDrawing(Bitu val) {
	if (vga.mode==M_ERROR) {
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_VerticalDisplayEnd);
		return;
	}
	/* Calculate the FPS for this screen */
	float fps; Bitu clock;
	Bitu htotal, hdend, hbstart, hbend, hrstart, hrend;
	Bitu vtotal, vdend, vbstart, vbend, vrstart, vrend;
	if (IS_EGAVGA_ARCH) {
		htotal = 2 + vga.crtc.horizontal_total;
		if (IS_VGA_ARCH) htotal += 3;
		hdend = 1 + vga.crtc.horizontal_display_end;
		hbstart = vga.crtc.start_horizontal_blanking;
		hbend = vga.crtc.end_horizontal_blanking&0x1F;
		if (IS_VGA_ARCH) hbend |= (vga.crtc.end_horizontal_retrace&0x80)>>2;
		hbend = hbstart + ((hbend - hbstart) & 0x3F);
		hrstart = vga.crtc.start_horizontal_retrace;
		hrend = vga.crtc.end_horizontal_retrace & 0x1f;
		hrend = (hrend - hrstart) & 0x1f;
		if ( !hrend )
			hrend = hrstart + 0x1f + 1;
		else 
			hrend = hrstart + hrend;

		vtotal= 2 + vga.crtc.vertical_total | ((vga.crtc.overflow & 1) << 8);
		vdend = 1 + vga.crtc.vertical_display_end | ((vga.crtc.overflow & 2)<<7);
		vrstart = vga.crtc.vertical_retrace_start + ((vga.crtc.overflow & 0x04) << 6);
		if (IS_VGA_ARCH) {
			// additional bits only present on vga cards
			vtotal |= (vga.crtc.overflow & 0x20) << 4;
			vdend |= ((vga.crtc.overflow & 0x40) << 3) | 
					((vga.s3.ex_ver_overflow & 0x2) << 9);
			vrstart |= ((vga.crtc.overflow & 0x80) << 2);
		}

		vrend = vga.crtc.vertical_retrace_end & 0xF;
		vrend = ( vrend - vrstart)&0xF;
		if ( !vrend)
			vrend = vrstart + 0xf + 1;
		else 
			vrend = vrstart + vrend;
		
		vbstart = vga.crtc.start_vertical_blanking | ((vga.crtc.overflow & 0x08) << 5);
		if (IS_VGA_ARCH) {
			vbstart |= ((vga.crtc.maximum_scan_line & 0x20) << 4);
			vbend = vga.crtc.end_vertical_blanking & 0x3f;
		} else {
			vbend = vga.crtc.end_vertical_blanking & 0xf;
		}
		vbend = (vbend - vbstart) & 0x3f;
		if ( !vbend)
			vbend = vbstart + 0x3f + 1;
		else
			vbend = vbstart + vbend;
			

		switch (svgaCard) {
		case SVGA_S3Trio:
			clock = SVGA_S3_GetClock();
			break;
		default:
			switch ((vga.misc_output >> 2) & 3) {
			case 0:	
				clock = 25175000;
				break;
			case 1:
				clock = 28322000;
				break;
			}
			break;
		}

		/* Check for 8 for 9 character clock mode */
		if (vga.seq.clocking_mode & 1 ) clock/=8; else clock/=9;
		/* Check for pixel doubling, master clock/2 */
		if (vga.seq.clocking_mode & 0x8) {
			htotal*=2;
		}
		vga.draw.address_line_total=(vga.crtc.maximum_scan_line&0xf)+1;
		/* Check for dual transfer whatever thing,master clock/2 */
		if (vga.s3.pll.cmd & 0x10) clock/=2;

		if (IS_VGA_ARCH) vga.draw.double_scan=(vga.crtc.maximum_scan_line&0x80)>0;
		else vga.draw.double_scan=(vtotal==262);
	} else {
		htotal = vga.other.htotal + 1;
		hdend = vga.other.hdend;
		hbstart = hdend;
		hbend = htotal;
		hrstart = vga.other.hsyncp;
		hrend = hrstart + (vga.other.syncw & 0xf) ;

		vga.draw.address_line_total = vga.other.max_scanline + 1;
		vtotal = vga.draw.address_line_total * (vga.other.vtotal+1)+vga.other.vadjust;
		vdend = vga.draw.address_line_total * vga.other.vdend;
		vrstart = vga.draw.address_line_total * vga.other.vsyncp;
		vrend = (vga.other.syncw >> 4);
		if (!vrend)
			vrend = vrstart + 0xf + 1;
		else 
			vrend = vrstart + vrend;
		vbstart = vdend;
		vbend = vtotal;
		vga.draw.double_scan=false;
		switch (machine) {
		case MCH_CGA:
		case TANDY_ARCH_CASE:
			clock=((vga.tandy.mode_control & 1) ? 14318180 : (14318180/2))/8;
			break;
		case MCH_HERC:
			if (vga.herc.mode_control & 0x2) clock=14318180/16;
			else clock=14318180/8;
			break;
		}
	}
#if C_DEBUG
	LOG(LOG_VGA,LOG_NORMAL)("h total %d end %d blank (%d/%d) retrace (%d/%d)",
		htotal, hdend, hbstart, hbend, hrstart, hrend );
	LOG(LOG_VGA,LOG_NORMAL)("v total %d end %d blank (%d/%d) retrace (%d/%d)",
		vtotal, vdend, vbstart, vbend, vrstart, vrend );
#endif
	if (!htotal) return;
	if (!vtotal) return;
	
	fps=(float)clock/(vtotal*htotal);
	// The time a complete video frame takes
	vga.draw.delay.vtotal = (1000.0 * (double)(vtotal*htotal)) / (double)clock; 
	// Horizontal total (that's how long a line takes with whistles and bells)
	vga.draw.delay.htotal = htotal*1000.0/clock; //in milliseconds
	// Start and End of horizontal blanking
	vga.draw.delay.hblkstart = hbstart*1000.0/clock; //in milliseconds
	vga.draw.delay.hblkend = hbend*1000.0/clock; 
	vga.draw.delay.hrstart = 0;
	
	// Start and End of vertical blanking
	vga.draw.delay.vblkstart = vbstart * vga.draw.delay.htotal;
	vga.draw.delay.vblkend = vbend * vga.draw.delay.htotal;
	// Start and End of vertical retrace pulse
	vga.draw.delay.vrstart = vrstart * vga.draw.delay.htotal;
	vga.draw.delay.vrend = vrend * vga.draw.delay.htotal;
	// Display end
	vga.draw.delay.vdend = vdend * vga.draw.delay.htotal;

	/*
	// just curious
	LOG_MSG("H total %d, V Total %d",htotal,vtotal);
	LOG_MSG("H D End %d, V D End %d",hdend,vdend);
	LOG_MSG("vrstart: %d, vrend: %d\n",vrstart,vrend);
	LOG_MSG("htotal:    %2.6f, vtotal:  %2.6f,\n"\
		    "hblkstart: %2.6f, hblkend: %2.6f,\n"\
		    "vblkstart: %2.6f, vblkend: %2.6f,\n"\
			"vrstart:   %2.6f, vrend:   %2.6f,\n"\
			"vdispend:  %2.6f",
		vga.draw.delay.htotal,    vga.draw.delay.vtotal,
		vga.draw.delay.hblkstart, vga.draw.delay.hblkend,
		vga.draw.delay.vblkstart, vga.draw.delay.vblkend,
		vga.draw.delay.vrstart,   vga.draw.delay.vrend,
		vga.draw.delay.vdend);
	*/
	vga.draw.parts_total=VGA_PARTS;
	/*
      6  Horizontal Sync Polarity. Negative if set
      7  Vertical Sync Polarity. Negative if set
         Bit 6-7 indicates the number of lines on the display:
            1:  400, 2: 350, 3: 480
	*/
	//Try to determine the pixel size, aspect correct is based around square pixels

	//Base pixel width around 100 clocks horizontal
	//For 9 pixel text modes this should be changed, but we don't support that anyway :)
	//Seems regular vga only listens to the 9 char pixel mode with character mode enabled
	double pwidth = 100.0 / htotal;
	//Base pixel height around vertical totals of modes that have 100 clocks horizontal
	//Different sync values gives different scaling of the whole vertical range
	//VGA monitor just seems to thighten or widen the whole vertical range
	double pheight;
	Bitu sync = vga.misc_output >> 6;
	switch ( sync ) {
	case 0:		// This is not defined in vga specs,
				// Kiet, seems to be slightly less than 350 on my monitor
		//340 line mode, filled with 449 total
		pheight = (480.0 / 340.0) * ( 449.0 / vtotal );
		break;
	case 1:		//400 line mode, filled with 449 total
		pheight = (480.0 / 400.0) * ( 449.0 / vtotal );
		break;
	case 2:		//350 line mode, filled with 449 total
		//This mode seems to get regular 640x400 timing and goes for a loong retrace
		//Depends on the monitor to stretch the screen
		pheight = (480.0 / 350.0) * ( 449.0 / vtotal );
		break;
	case 3:		//480 line mode, filled with 525 total
		pheight = (480.0 / 480.0) * ( 525.0 / vtotal );
		break;
	}

	double aspect_ratio = pheight / pwidth;

	vga.draw.delay.parts = vga.draw.delay.vdend/vga.draw.parts_total;
	vga.draw.resizing=false;
	vga.draw.vret_triggered=false;

	//Check to prevent useless black areas
	if (hbstart<hdend) hdend=hbstart;
	if (vbstart<vdend) vdend=vbstart;

	Bitu width=hdend;
	Bitu height=vdend;
	bool doubleheight=false;
	bool doublewidth=false;

	//Set the bpp
	Bitu bpp;
	switch (vga.mode) {
	case M_LIN15:
		bpp = 15;
		break;
	case M_LIN16:
		bpp = 16;
		break;
	case M_LIN32:
		bpp = 32;
		break;
	default:
		bpp = 8;
		break;
	}
	vga.draw.linear_base = vga.mem.linear;
	vga.draw.linear_mask = VGA_MEMORY - 1;
	switch (vga.mode) {
	case M_VGA:
		doublewidth=true;
		width<<=2;
		VGA_DrawLine = VGA_Draw_Linear_Line;
		break;
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		width<<=3;
		if (vga.crtc.mode_control & 0x8) {
			doublewidth = true;
			width >>= 1;
		}
		/* Use HW mouse cursor drawer if enabled */
		VGA_ActivateHardwareCursor();
		break;
	case M_LIN4:
		doublewidth=(vga.seq.clocking_mode & 0x8) > 0;
		vga.draw.blocks = width;
		width<<=3;
		VGA_DrawLine=VGA_Draw_Linear_Line;
		vga.draw.linear_base = vga.mem.linear + VGA_CACHE_OFFSET;
		vga.draw.linear_mask = 1024 * 1024 - 1;
		break;
	case M_EGA:
		doublewidth=(vga.seq.clocking_mode & 0x8) > 0;
		vga.draw.blocks = width;
		width<<=3;
		VGA_DrawLine=VGA_Draw_Linear_Line;
		vga.draw.linear_base = vga.mem.linear + VGA_CACHE_OFFSET;
		vga.draw.linear_mask = 512 * 1024 - 1;
		break;
	case M_CGA16:
		doubleheight=true;
		vga.draw.blocks=width*2;
		width<<=4;
		VGA_DrawLine=VGA_Draw_CGA16_Line;
		break;
	case M_CGA4:
		doublewidth=true;
		vga.draw.blocks=width*2;
		width<<=3;
		VGA_DrawLine=VGA_Draw_2BPP_Line;
		break;
	case M_CGA2:
		doubleheight=true;
		vga.draw.blocks=2*width;
		width<<=3;
		VGA_DrawLine=VGA_Draw_1BPP_Line;
		break;
	case M_TEXT:
		aspect_ratio=1.0;
		vga.draw.blocks=width;
		doublewidth=(vga.seq.clocking_mode & 0x8) > 0;
		if ((IS_VGA_ARCH) && (svgaCard==SVGA_None) && (vga.attr.mode_control&0x04)) {
			width*=9;				/* 9 bit wide text font */
			VGA_DrawLine=VGA_TEXT_Draw_Line_9;
		} else {
			width<<=3;				/* 8 bit wide text font */
			VGA_DrawLine=VGA_TEXT_Draw_Line;
		}
		break;
	case M_HERC_GFX:
		aspect_ratio=1.5;
		vga.draw.blocks=width*2;
		width*=16;
		VGA_DrawLine=VGA_Draw_1BPP_Line;
		break;
	case M_TANDY2:
		aspect_ratio=1.2;
		doubleheight=true;
		if (machine==MCH_PCJR) doublewidth=(vga.tandy.gfx_control & 0x8)==0x00;
		else doublewidth=(vga.tandy.mode_control & 0x10)==0;
		vga.draw.blocks=width * (doublewidth ? 4:8);
		width=vga.draw.blocks*2;
		VGA_DrawLine=VGA_Draw_1BPP_Line;
		break;
	case M_TANDY4:
		aspect_ratio=1.2;
		doubleheight=true;
		if (machine==MCH_TANDY) doublewidth=(vga.tandy.mode_control & 0x10)==0;
		else doublewidth=(vga.tandy.mode_control & 0x01)==0x00;
		vga.draw.blocks=width * 2;
		width=vga.draw.blocks*4;
		if ((machine==MCH_TANDY && (vga.tandy.gfx_control & 0x8)) ||
			(machine==MCH_PCJR && (vga.tandy.mode_control==0x0b)))
			VGA_DrawLine=VGA_Draw_2BPPHiRes_Line;
		else VGA_DrawLine=VGA_Draw_2BPP_Line;
		break;
	case M_TANDY16:
		aspect_ratio=1.2;
		doubleheight=true;
		vga.draw.blocks=width*2;
		if (vga.tandy.mode_control & 0x1) {
			if (( machine==MCH_TANDY ) && ( vga.tandy.mode_control & 0x10 )) {
				doublewidth = false;
				vga.draw.blocks*=2;
				width=vga.draw.blocks*2;
			} else {
				doublewidth = true;
				width=vga.draw.blocks*2;
			}
			VGA_DrawLine=VGA_Draw_4BPP_Line;
		} else {
			doublewidth=true;
			width=vga.draw.blocks*4;
			VGA_DrawLine=VGA_Draw_4BPP_Line_Double;
		}
		break;
	case M_TANDY_TEXT:
		doublewidth=(vga.tandy.mode_control & 0x1)==0;
	case M_HERC_TEXT:
		aspect_ratio=1;
		doubleheight=(vga.mode!=M_HERC_TEXT);
		vga.draw.blocks=width;
		width<<=3;
		VGA_DrawLine=VGA_TEXT_Draw_Line;
		break;
	default:
		LOG(LOG_VGA,LOG_ERROR)("Unhandled VGA mode %d while checking for resolution",vga.mode);
	};
	VGA_CheckScanLength();
	if (vga.draw.double_scan) {
		if (IS_VGA_ARCH) height/=2;
		doubleheight=true;
	}
	//Only check for extra double height in vga modes
	if (!doubleheight && (vga.mode<M_TEXT) && !(vga.draw.address_line_total & 1)) {
		vga.draw.address_line_total/=2;
		doubleheight=true;
		height/=2;
	}
	vga.draw.lines_total=height;
	vga.draw.parts_lines=vga.draw.lines_total/vga.draw.parts_total;
	vga.draw.line_length = width * ((bpp + 1) / 8);
#ifdef VGA_KEEP_CHANGES
	vga.changes.active = false;
	vga.changes.frame = 0;
	vga.changes.writeMask = 1;
#endif
    /* 
	   Cheap hack to just make all > 640x480 modes have 4:3 aspect ratio
	*/
	if ( width >= 640 && height >= 480 ) {
		aspect_ratio = ((float)width / (float)height) * ( 3.0 / 4.0);
	}
//	LOG_MSG("ht %d vt %d ratio %f", htotal, vtotal, aspect_ratio );

	if (( width != vga.draw.width) || (height != vga.draw.height) ||
		(aspect_ratio != vga.draw.aspect_ratio) || 
		(vga.mode != vga.lastmode)) {
		vga.lastmode = vga.mode;
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_VerticalDisplayEnd);
		PIC_RemoveEvents(VGA_DrawPart);
		vga.draw.width = width;
		vga.draw.height = height;
		vga.draw.doublewidth = doublewidth;
		vga.draw.doubleheight = doubleheight;
		vga.draw.aspect_ratio = aspect_ratio;
		if (doubleheight) vga.draw.lines_scaled=2;
		else vga.draw.lines_scaled=1;
#if C_DEBUG
		LOG(LOG_VGA,LOG_NORMAL)("Width %d, Height %d, fps %f",width,height,fps);
		LOG(LOG_VGA,LOG_NORMAL)("%s width, %s height aspect %f",
			doublewidth ? "double":"normal",doubleheight ? "double":"normal",aspect_ratio);
#endif
		RENDER_SetSize(width,height,bpp,fps,aspect_ratio,doublewidth,doubleheight);
		vga.draw.delay.framestart = PIC_FullIndex();
		PIC_AddEvent( VGA_VerticalTimer , (float)vga.draw.delay.vtotal );
	}
};

void VGA_KillDrawing(void) {
	PIC_RemoveEvents(VGA_DrawPart);
}
