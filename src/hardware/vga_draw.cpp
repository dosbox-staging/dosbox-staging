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

#include <string.h>
#include <math.h>
#include "dosbox.h"
#include "video.h"
#include "render.h"
#include "vga.h"
#include "pic.h"

#define VGA_PARTS 4

typedef Bit8u * (* VGA_Line_Handler)(Bitu vidstart,Bitu panning,Bitu line);

static VGA_Line_Handler VGA_DrawLine;
static Bit8u TempLine[1280];


static Bit8u * VGA_Draw_1BPP_Line(Bitu vidstart,Bitu panning,Bitu line) {
	line*=8*1024;Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=vga.draw.blocks;x>0;x--) {
		Bitu val=vga.mem.linear[vidstart+line];vidstart=(vidstart+1)&0x1fff;
		*draw++=CGA_2_Table[val >> 4];
		*draw++=CGA_2_Table[val & 0xf];
	}
	return TempLine;
}

static Bit8u * VGA_Draw_2BPP_Line(Bitu vidstart,Bitu panning,Bitu line) {
	line*=8*1024;Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val=vga.mem.linear[vidstart+line];vidstart=(vidstart+1)&0x1fff;
		*draw++=CGA_4_Table[val];
	}
	return TempLine;
}

static Bit8u convert16[16]={
	0x0,0x2,0x1,0x3,0x5,0x7,0x4,0x9,
	0x6,0xa,0x8,0xb,0xd,0xe,0xc,0xf
};

static Bit8u * VGA_Draw_4BPP_Line(Bitu vidstart,Bitu panning,Bitu line) {
	Bit8u * reader=&vga.mem.linear[(vga.tandy.disp_bank << 14) + vidstart + (line * 8 * 1024)];
	Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val1=*reader++;Bitu val2=*reader++;
		*draw++=(val1 & 0x0f) << 8  |
				(val1 & 0xf0) >> 4  |
				(val2 & 0x0f) << 24 |
				(val2 & 0xf0) << 12;
	}
	return TempLine;
}

static Bit8u * VGA_Draw_4BPP_Line_Double(Bitu vidstart,Bitu panning,Bitu line) {
	Bit8u * reader=&vga.mem.linear[(vga.tandy.disp_bank << 14) + vidstart + (line * 8 * 1024)];
	Bit32u * draw=(Bit32u *)TempLine;
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val1=*reader++;Bitu val2=*reader++;
		*draw++=(val1 & 0x0f) << 8  |
				(val1 & 0xf0) >> 4  |
				(val2 & 0x0f) << 24 |
				(val2 & 0xf0) << 12;
	}
	return TempLine;
}


static Bit8u * VGA_Draw_EGA_Line(Bitu vidstart,Bitu panning,Bitu line) {
	return &vga.mem.linear[512*1024+vidstart*8+panning];
}
static Bit8u * VGA_Draw_VGA_Line(Bitu vidstart,Bitu panning,Bitu line) {
	return &vga.mem.linear[vidstart*4+panning];
}

static Bit8u * VGA_Draw_VGA_Line_HWMouse(Bitu vidstart, Bitu panning, Bitu line) {
	if(vga.s3.hgc.curmode & 0x1) {
		Bitu lineat = vidstart / 160;
		if((lineat < vga.s3.hgc.originy) || (lineat > (vga.s3.hgc.originy + 63))) {
			return VGA_Draw_VGA_Line(vidstart, panning, line);
		} else {
			memcpy(TempLine, VGA_Draw_VGA_Line(vidstart, panning, line), 640);
			/* Draw mouse cursor */
			Bits moff = ((Bits)lineat - (Bits)vga.s3.hgc.originy) + (Bits)vga.s3.hgc.posy;
			if(moff>63) moff=moff-64;
			if(moff<0) moff+=64;
			Bitu xat = vga.s3.hgc.originx;
			Bitu m, mat;
			mat = vga.s3.hgc.posx;

			for(m=0;m<64;m++) {
				switch(vga.s3.hgc.mc[moff][mat]) {
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
						break;
				}
				xat++;
				mat++;
				if(mat>63) mat=0;
			}
			return TempLine;
		}
	} else {
		/* HW Mouse not enabled, use the tried and true call */
		return VGA_Draw_VGA_Line(vidstart, panning, line);
	}
}





static Bit32u FontMask[2]={0xffffffff,0x0};
static Bit8u * VGA_TEXT_Draw_Line(Bitu vidstart,Bitu panning,Bitu line) {
	Bit32u * draw=(Bit32u *)TempLine;
	Bit8u * vidmem=&vga.mem.linear[vidstart];
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
	Bits font_addr=(vga.config.cursor_start*2-vidstart)/2;
	if (!vga.draw.cursor.enabled || !(vga.draw.cursor.count&0x8)) goto skip_cursor;
	if (font_addr>=0 && font_addr<vga.draw.blocks) {
		if (line<vga.draw.cursor.sline) goto skip_cursor;
		if (line>vga.draw.cursor.eline) goto skip_cursor;
		draw=(Bit32u *)&TempLine[font_addr*8];
		Bit32u att=TXT_FG_Table[vga.mem.linear[vga.config.cursor_start*2+1]&0xf];
		*draw++=att;*draw++=att;
	}
skip_cursor:
	return TempLine;
}

static void VGA_VerticalDisplayEnd(Bitu val) {
	vga.config.retrace=true;
	vga.config.real_start=vga.config.display_start;
}

static void VGA_HorizontalTimer(void) {

}

static void VGA_DrawPart(Bitu lines) {
	Bitu subline=0;Bitu vidofs=vga.config.real_start;
	Bit8u * draw=0;
	while (lines--) {
		vga.draw.lines_done++;
		Bit8u * data=VGA_DrawLine(vga.draw.address,vga.draw.panning,vga.draw.address_line);
		RENDER_DrawLine(data);
		vga.draw.address_line++;
		if (vga.draw.address_line>=vga.draw.address_line_total) {
			vga.draw.address_line=0;
			vga.draw.address+=vga.draw.address_add;
		}
		if (vga.draw.split_line==vga.draw.lines_done) {
			vga.draw.address=0;vga.draw.panning=0;
			vga.draw.address_line=0;
		}
	}
	vga.draw.parts_left--;
	if (vga.draw.parts_left) {
		PIC_AddEvent(VGA_DrawPart,vga.draw.micro.parts,vga.draw.parts_lines);
	} else RENDER_EndUpdate();
}

void VGA_SetBlinking(Bitu enabled) {
	Bitu b;
	LOG(LOG_VGA,LOG_NORMAL)("Blinking %d",enabled);
	if (enabled) {
		b=0;vga.draw.blinking=1; //used to -1 but blinking is unsigned
		vga.attr.mode_control|=0x08;
		vga.tandy.mode_control&=~0x20;
	} else {
		b=8;vga.draw.blinking=0;
		vga.attr.mode_control&=~0x08;
		vga.tandy.mode_control|=0x20;
	}
	for (Bitu i=0;i<8;i++) TXT_BG_Table[i+8]=(b+i) | ((b+i) << 8)| ((b+i) <<16) | ((b+i) << 24);
}

static void VGA_VerticalTimer(Bitu val) {
	vga.config.retrace=false;
	PIC_AddEvent(VGA_VerticalTimer,vga.draw.micro.vtotal);
	PIC_AddEvent(VGA_VerticalDisplayEnd,vga.draw.micro.vend);
	vga.draw.parts_left=vga.draw.parts_total;
	vga.draw.lines_done=0;
	vga.draw.address=vga.config.real_start;
	vga.draw.address_line=vga.config.hlines_skip;
	vga.draw.split_line=(vga.config.line_compare/vga.draw.lines_scaled);
	vga.draw.panning=vga.config.pel_panning;
	switch (vga.mode) {
	case M_TEXT:
		vga.draw.cursor.count++;
		/* check for blinking and blinking change delay */
		FontMask[1]=(vga.attr.mode_control & (vga.draw.cursor.count >> 1) & 0x8) ?
			0 : 0xffffffff;
		vga.draw.address=(vga.draw.address*2);
		break;
	case M_CGA4:case M_CGA2:case M_CGA16:
	case M_TANDY2:case M_TANDY4:case M_TANDY16:
		vga.draw.address=(vga.draw.address*2)&0x1fff;
		break;
	}
	if (RENDER_StartUpdate()) {
		PIC_AddEvent(VGA_DrawPart,vga.draw.micro.parts,vga.draw.parts_lines);
	}
}

void VGA_CheckScanLength(void) {
	switch (vga.mode) {
	case M_EGA16:
	case M_VGA:
	case M_LIN8:
		vga.draw.address_add=vga.config.scan_len*2;
		break;
	case M_TEXT:
		vga.draw.address_add=vga.config.scan_len*4;
		break;
	case M_CGA2:
	case M_CGA4:
		vga.draw.address_add=80;
		return;
	case M_TANDY2:
		vga.draw.address_add=vga.draw.blocks/4;
		break;
	case M_TANDY4:
		vga.draw.address_add=vga.draw.blocks/2;
		break;
	case M_CGA16:
		vga.draw.address_add=vga.draw.blocks/2;
		return;
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

void VGA_SetupDrawing(Bitu val) {
	if (vga.mode==M_ERROR) {
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_VerticalDisplayEnd);
		return;
	}
	/* Calculate the FPS for this screen */
	double fps;Bitu clock;
	Bitu htotal,hdispend,hbstart,hrstart;
	Bitu vtotal,vdispend,vbstart,vrstart;
	if (machine==MCH_VGA) {
		vtotal=2 + vga.crtc.vertical_total | 
			((vga.crtc.overflow & 1) << 8) | ((vga.crtc.overflow & 0x20) << 4);
		htotal=5 + vga.crtc.horizontal_total;
		vdispend = 1 + (vga.crtc.vertical_display_end | 
			((vga.crtc.overflow & 2)<<7) | ((vga.crtc.overflow & 0x40) << 3) | 
			((vga.s3.ex_ver_overflow & 0x2) << 9));
		hdispend = 1 + (vga.crtc.horizontal_display_end);
		hbstart = vga.crtc.start_horizontal_blanking;
		vbstart = vga.crtc.start_vertical_blanking | ((vga.crtc.overflow & 0x08) << 5) |
			((vga.crtc.maximum_scan_line & 0x20) << 4) ;
		hrstart = vga.crtc.start_horizontal_retrace;
		vrstart = vga.crtc.vertical_retrace_start + ((vga.crtc.overflow & 0x04) << 6) |
			((vga.crtc.overflow & 0x80) << 2);
		if (hbstart<hdispend) hdispend=hbstart;
		if (vbstart<vdispend) vdispend=vbstart;

		clock=(vga.misc_output >> 2) & 3;
		clock=1000*S3_CLOCK(vga.s3.clk[clock].m,vga.s3.clk[clock].n,vga.s3.clk[clock].r);
		/* Check for 8 for 9 character clock mode */
		if (vga.seq.clocking_mode & 1 ) clock/=8; else clock/=9;
		/* Check for pixel doubling, master clock/2 */
		if (vga.seq.clocking_mode & 0x8) {
			htotal*=2;
		}
		vga.draw.font_height=(vga.crtc.maximum_scan_line&0xf)+1;
		/* Check for dual transfer whatever thing,master clock/2 */
		if (vga.s3.pll.cmd & 0x10) clock/=2;
	} else {
		vga.draw.font_height=vga.other.max_scanline+1;
		htotal=vga.other.htotal;
		hdispend=vga.other.hdend;
		hrstart=vga.other.hsyncp;
		vtotal=vga.draw.font_height*vga.other.vtotal+vga.other.vadjust;
		vdispend=vga.draw.font_height*vga.other.vdend;
		vrstart=vga.draw.font_height*vga.other.vsyncp;
		switch (machine) {
		case MCH_CGA:
		case MCH_TANDY:
			clock=((vga.tandy.mode_control & 1) ? 14318180 : (14318180/2))/8;
			break;
		case MCH_HERC:
			if (vga.herc.mode_control & 0x2) clock=14318180/16;
			else clock=14318180/8;
			break;
		}
	}
	LOG(LOG_VGA,LOG_NORMAL)("H total %d, V Total %d",htotal,vtotal);
	LOG(LOG_VGA,LOG_NORMAL)("H D End %d, V D End %d",hdispend,vdispend);
	if (!htotal) return;
	if (!vtotal) return;
	fps=clock/(vtotal*htotal);
	double linemicro=(1000000/fps);
	vga.draw.parts_total=VGA_PARTS;
	vga.draw.micro.vtotal=(Bitu)(linemicro);
	linemicro/=vtotal;		//Really make it the line_micro
	vga.draw.micro.vend=(Bitu)(linemicro*vrstart);
	vga.draw.micro.parts=(Bitu)((linemicro*vdispend)/vga.draw.parts_total);
	vga.draw.micro.htotal=(Bitu)(linemicro);
	vga.draw.micro.hend=(Bitu)((linemicro/htotal)*hrstart);


	double correct_ratio=(100.0/525.0);
	double aspect_ratio=((double)htotal/((double)vtotal)/correct_ratio);
	
	vga.draw.resizing=false;
	Bitu width,height;
	Bitu scalew=1;
	Bitu scaleh=1;
	width=hdispend;
	height=vdispend;
	vga.draw.double_scan=false;
	switch (vga.mode) {
	case M_VGA:
		scalew=2;scaleh*=vga.draw.font_height;
		if (vga.crtc.maximum_scan_line&0x80) scaleh*=2;
		vga.draw.lines_scaled=scaleh;
		vga.draw.address_line_total=1;
		height/=scaleh;width<<=2;
		VGA_DrawLine=VGA_Draw_VGA_Line;
		break;
	case M_LIN8:
		scaleh*=vga.draw.font_height;
		if (vga.crtc.maximum_scan_line&0x80) scaleh*=2;
		vga.draw.lines_scaled=scaleh;
		vga.draw.address_line_total=1;
		height/=scaleh;width<<=3;
		/* Use HW mouse cursor drawer if enabled */
		if(vga.s3.hgc.curmode & 0x1) {
			VGA_DrawLine=VGA_Draw_VGA_Line_HWMouse;
		} else {
			VGA_DrawLine=VGA_Draw_VGA_Line;
		}
		break;
	case M_EGA16:
		scaleh*=vga.draw.font_height;
		if (vga.crtc.maximum_scan_line&0x80) scaleh*=2;
		vga.draw.lines_scaled=scaleh;
		if (vga.seq.clocking_mode & 0x8) scalew*=2;
		width<<=3;height/=scaleh;
		vga.draw.address_line_total=1;
		VGA_DrawLine=VGA_Draw_EGA_Line;
		break;
	case M_CGA4:
		scaleh=2;scalew*=2;
		vga.draw.blocks=width*2;
		vga.draw.lines_scaled=1;
		vga.draw.address_line_total=2;
		width<<=3;height/=2;
		VGA_DrawLine=VGA_Draw_2BPP_Line;
		break;
	case M_CGA2:
		scaleh=2;
		vga.draw.blocks=width;
		vga.draw.lines_scaled=1;
		vga.draw.address_line_total=2;
		width<<=4;height/=2;
		VGA_DrawLine=VGA_Draw_1BPP_Line;
		break;
	case M_TEXT:
		aspect_ratio=1.0;
		vga.draw.address_line_total=vga.draw.font_height;
		vga.draw.blocks=width;
		if (vga.seq.clocking_mode & 0x8) scalew*=2;
		if (vga.crtc.maximum_scan_line&0x80) scaleh*=2;
		height/=scaleh;
		vga.draw.lines_scaled=scaleh;
		width<<=3;				/* 8 bit wide text font */
		VGA_DrawLine=VGA_TEXT_Draw_Line;
		break;
	case M_HERC_GFX:
		aspect_ratio=1.5;
		vga.draw.address_line_total=vga.draw.font_height;
		vga.draw.blocks=width*2;
		vga.draw.lines_scaled=1;
		width*=16;
		VGA_DrawLine=VGA_Draw_1BPP_Line;
		break;
	case M_TANDY2:
		scaleh=2;aspect_ratio=1.2;
		scalew=(vga.tandy.mode_control & 0x10) ? 1 : 2;
		vga.draw.blocks=width*8/scalew;
		vga.draw.address_line_total=vga.draw.font_height;
		vga.draw.lines_scaled=1;
		width=vga.draw.blocks*2;
		VGA_DrawLine=VGA_Draw_1BPP_Line;
		break;
	case M_TANDY4:
		scaleh=2;aspect_ratio=1.2;
		scalew=(vga.tandy.mode_control & 0x10) ? 1 : 2;
		vga.draw.blocks=width*8/scalew;
		vga.draw.address_line_total=vga.draw.font_height;
		vga.draw.lines_scaled=1;
		width=vga.draw.blocks*2;
		VGA_DrawLine=VGA_Draw_2BPP_Line;
		break;
	case M_TANDY16:
		scaleh=2;aspect_ratio=1.2;
		scalew=(vga.tandy.mode_control & 0x10) ? 1 : 2;
		vga.draw.blocks=width*4/scalew;
		vga.draw.address_line_total=vga.draw.font_height;
		vga.draw.lines_scaled=1;
		width=vga.draw.blocks*2;
		VGA_DrawLine=VGA_Draw_4BPP_Line;
		break;
	case M_TANDY_TEXT:
		scalew=(vga.tandy.mode_control & 0x1) ? 1 : 2;
	case M_HERC_TEXT:
		aspect_ratio=1;
		scaleh=(vga.mode==M_HERC_TEXT) ? 1 : 2;
		vga.draw.lines_scaled=1;
		vga.draw.address_line_total=vga.draw.font_height;
		vga.draw.blocks=width;
		vga.draw.lines_scaled=1;
		width<<=3;
		VGA_DrawLine=VGA_TEXT_Draw_Line;
		break;
	default:
		LOG(LOG_VGA,LOG_ERROR)("Unhandled VGA type %d while checking for resolution");
	};
	VGA_CheckScanLength();
	vga.draw.lines_total=height;
	vga.draw.parts_lines=vga.draw.lines_total/vga.draw.parts_total;
	if (( width != vga.draw.width) || (height != vga.draw.height)) {
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_VerticalDisplayEnd);
		vga.draw.width=width;
		vga.draw.height=height;
		vga.draw.scaleh=scaleh;

		LOG(LOG_VGA,LOG_NORMAL)("Width %d, Height %d, fps %f",width,height,fps);
		LOG(LOG_VGA,LOG_NORMAL)("Scalew %d, Scaleh %d aspect %f",scalew,scaleh,aspect_ratio);
		RENDER_SetSize(width,height,8,aspect_ratio,scalew>1,scaleh>1);
		PIC_AddEvent(VGA_VerticalTimer,vga.draw.micro.vtotal);
	}
};
