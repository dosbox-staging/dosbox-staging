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

static Bit8u VGA_DrawBuffer[2048];

typedef Bit8u * (* VGA_Line_Handler)(Bitu vidstart,Bitu panning,Bitu line);

static VGA_Line_Handler VGA_DrawLine;

static Bit8u * VGA_HERC_Draw_Line(Bitu vidstart,Bitu panning,Bitu line) {
	Bit8u * reader=&vga.mem.linear[vidstart+(line * 8 * 1024)];
	Bit32u * draw=(Bit32u *)&VGA_DrawBuffer[0];
	for (Bitu x=vga.draw.blocks;x>0;x--) {
		Bitu val=*reader++;
		*draw++=CGA_2_Table[val >> 4];
		*draw++=CGA_2_Table[val & 0xf];
	}
	return VGA_DrawBuffer;
}

static Bit8u * VGA_CGA2_Draw_Line(Bitu vidstart,Bitu panning,Bitu line) {
	Bit8u * reader=&vga.mem.linear[vidstart+(line * 8 * 1024)];
	Bit32u * draw=(Bit32u *)&VGA_DrawBuffer[0];
	for (Bitu x=vga.draw.blocks;x>0;x--) {
		Bitu val=*reader++;
		*draw++=CGA_2_Table[val >> 4];
		*draw++=CGA_2_Table[val & 0xf];
	}
	return VGA_DrawBuffer;
}

static Bit8u * VGA_CGA4_Draw_Line(Bitu vidstart,Bitu panning,Bitu line) {
	Bit8u * reader=&vga.mem.linear[vidstart+(line * 8 * 1024)];
	Bit32u * draw=(Bit32u *)&VGA_DrawBuffer[0];
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		*draw++=CGA_4_Table[*reader++];
	}
	return VGA_DrawBuffer;
}


static Bit8u convert16[16]={
	0x0,0x2,0x1,0x3,0x5,0x7,0x4,0x9,
	0x6,0xa,0x8,0xb,0xd,0xe,0xc,0xf
};

static Bit8u * VGA_CGA16_Draw_Line(Bitu vidstart,Bitu panning,Bitu line) {
	Bit8u * reader=&vga.mem.linear[vidstart+(line * 8 * 1024)];
	Bit32u * draw=(Bit32u *)&VGA_DrawBuffer[0];
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val=*reader++;
		Bit32u full=convert16[val >> 4] | convert16[val & 0xf] << 16;
		*draw++=full|=full<<8;
	}
	return VGA_DrawBuffer;
}

static Bit8u * VGA_TANDY16_Draw_Line(Bitu vidstart,Bitu panning,Bitu line) {
	Bit8u * reader=&vga.mem.linear[(vga.tandy.disp_bank << 14) + vidstart + (line * 8 * 1024)];
	Bit32u * draw=(Bit32u *)&VGA_DrawBuffer[0];
	for (Bitu x=0;x<vga.draw.blocks;x++) {
		Bitu val1=*reader++;Bitu val2=*reader++;
		*draw++=(val1 & 0x0f) << 8  |
				(val1 & 0xf0) >> 4  |
				(val2 & 0x0f) << 24 |
				(val2 & 0xf0) << 12;
	}
	return VGA_DrawBuffer;
}

static Bit8u * VGA_EGA_Draw_Line(Bitu vidstart,Bitu panning,Bitu line) {
	return &vga.mem.linear[512*1024+vidstart*8+panning];
}
static Bit8u * VGA_VGA_Draw_Line(Bitu vidstart,Bitu panning,Bitu line) {
	return &vga.mem.linear[vidstart*4+panning/2];
}

static Bit8u * VGA_TEXT_Draw_Line(Bitu vidstart,Bitu panning,Bitu line) {
	Bit8u * draw=VGA_DrawBuffer;
	Bit8u * vidmem=&vga.mem.linear[vidstart];
	for (Bitu cx=0;cx<vga.draw.blocks;cx++) {
		Bitu chr=vidmem[cx*2];
		Bitu font=vga.draw.font[chr*32+line];
		Bit32u mask1=TXT_Font_Table[font>>4];
		Bit32u mask2=TXT_Font_Table[font&0xf];
		Bitu col=vidmem[cx*2+1];
		Bit32u fg=TXT_FG_Table[col&0xf];
		Bit32u bg=TXT_BG_Table[col>>4];
		*(Bit32u*)draw=fg&mask1 | bg&~mask1;
		draw+=4;
		*(Bit32u*)draw=fg&mask2 | bg&~mask2;
		draw+=4;
	}
	return VGA_DrawBuffer;
}

void VGA_TEXT_Draw(Bit8u * bitdata,Bitu start,Bitu panning,Bitu lines) {
#if 0
	Bit8u * reader=&vga.mem.linear[start*2];Bitu rows=lines/vga.draw.font_height;
	for (Bitu cy=rows;cy>0;cy--) {
		for (Bitu y=0;y<vga.draw.font_height;y++) {
			Bit8u * draw=bitdata;
			bitdata+=vga.draw.width;
			for (Bitu cx=0;cx<vga.draw.cols;cx++) {
				Bit8u chr=reader[cx*2];
				Bit8u font=vga.draw.font[chr*32+y];
				Bit32u mask1=TXT_Font_Table[font>>4];
				Bit32u mask2=TXT_Font_Table[font&0xf];
				Bit8u col=reader[cx*2+1];
				Bit32u fg=TXT_FG_Table[col&0xf];
				Bit32u bg=TXT_BG_Table[col>>4];
				*(Bit32u*)draw=fg&mask1 | bg&~mask1;
				draw+=4;
				*(Bit32u*)draw=fg&mask2 | bg&~mask2;
				draw+=4;
			}
		}
		reader+=(vga.config.scan_len*4);
	}
/* Cursor handling */
    if(vga.draw.cursor.enabled && (vga.draw.cursor.count>8)) {	/* Draw a cursor if enabled */
		Bits cur_start=vga.config.cursor_start-start;
		if (cur_start<0) return;

		Bitu row=cur_start / (vga.config.scan_len*2);
		Bitu col=cur_start % (vga.config.scan_len*2);
		Bit32u att=vga.mem.linear[vga.config.cursor_start*2+1]&0xf;
		att=TXT_BG_Table[att];

		if ((col*8)>=vga.draw.width) return;
		if ((row*vga.draw.font_height)>=vga.draw.height) return;
		if (vga.draw.cursor.sline>=vga.draw.font_height) return;
		if (vga.draw.cursor.sline>vga.draw.cursor.eline) return;
		Bit8u * cursor_draw=bitdata+(row*vga.draw.font_height+vga.draw.cursor.sline)*vga.draw.width+col*8;
	
		for (Bits loop=vga.draw.cursor.eline-vga.draw.cursor.sline;loop>=0;loop--) {
			*((Bit32u *)cursor_draw)=att;
			*((Bit32u *)(cursor_draw+4))=att;
			cursor_draw+=vga.draw.width;
		}
    }
#endif
}

static void VGA_VerticalDisplayEnd(void) {
	vga.config.retrace=true;
	vga.config.real_start=vga.config.display_start;
}

static void VGA_HorizontalTimer(void) {

}

static void VGA_DrawPart(void) {
	Bitu subline=0;Bitu vidofs=vga.config.real_start;
	Bit8u * draw=0;
	while (vga.draw.lines_left) {
		vga.draw.lines_left--;
		Bit8u * data=VGA_DrawLine(vga.draw.address,vga.draw.panning,vga.draw.address_line);
		RENDER_DrawLine(data);
		vga.draw.address_line++;
		if (vga.draw.address_line>=vga.draw.address_line_total) {
			vga.draw.address_line=0;
			vga.draw.address+=vga.draw.address_add;
		}
		if (vga.draw.split_line==vga.draw.lines_left) {
			vga.draw.address=0;vga.draw.panning=0;
		}
	}
	RENDER_EndUpdate();
//	vga.draw.parts_left--;
//	if (vga.draw.parts_left) PIC_AddEvent(VGA_DrawPart,vga.draw.micro.parts);
}

static void VGA_VerticalTimer(void) {
	vga.config.retrace=false;
	vga.draw.cursor.count++;vga.draw.cursor.count&=0xf;
	PIC_AddEvent(VGA_VerticalTimer,vga.draw.micro.vtotal);
	PIC_AddEvent(VGA_VerticalDisplayEnd,vga.draw.micro.vend);
	vga.draw.parts_left=4;
	vga.draw.lines_left=vga.draw.lines_total;
	vga.draw.address=vga.config.real_start;
	vga.draw.address_line=vga.config.hlines_skip;
	vga.draw.split_line=vga.draw.lines_total-(vga.config.line_compare/vga.draw.lines_scaled);
	vga.draw.panning=vga.config.pel_panning;
	if (RENDER_StartUpdate()) {
		VGA_DrawPart();
	}
}

void VGA_SetupDrawing(void) {
	/* Calculate the FPS for this screen */
	double fps;
	Bitu vtotal=2 + vga.crtc.vertical_total | 
		((vga.crtc.overflow & 1) << 8) | ((vga.crtc.overflow & 0x20) << 4);
	Bitu htotal=5 + vga.crtc.horizontal_total;
	Bitu vdispend = 1 + (vga.crtc.vertical_display_end | 
		((vga.crtc.overflow & 2)<<7) | ((vga.crtc.overflow & 0x40) << 3) | 
		((vga.s3.ex_ver_overflow & 0x2) << 9));
	Bitu hdispend = 1 + (vga.crtc.horizontal_display_end);
	
	Bitu hbstart = vga.crtc.start_horizontal_blanking;
	Bitu vbstart = vga.crtc.start_vertical_blanking | ((vga.crtc.overflow & 0x08) << 5) |
		((vga.crtc.maximum_scan_line & 0x20) << 4) ;

	Bitu hrstart = vga.crtc.start_horizontal_retrace;
	Bitu vrstart = vga.crtc.vertical_retrace_start + ((vga.crtc.overflow & 0x04) << 6) |
		((vga.crtc.overflow & 0x80) << 2);

	if (hbstart<hdispend) hdispend=hbstart;
	if (vbstart<vdispend) vdispend=vbstart;

	Bitu clock=(vga.misc_output >> 2) & 3;
	clock=1000*S3_CLOCK(vga.s3.clk[clock].m,vga.s3.clk[clock].n,vga.s3.clk[clock].r);
	/* Check for 8 for 9 character clock mode */
	if (vga.seq.clocking_mode & 1 ) clock/=8; else clock/=9;
	/* Check for pixel doubling, master clock/2 */
	if (vga.seq.clocking_mode & 0x8) {
		htotal*=2;
	}
	/* Check for dual transfer whatever thing,master clock/2 */
	if (vga.s3.pll.cmd & 0x10) clock/=2;
	
	LOG(LOG_VGA,LOG_NORMAL)("H total %d, V Total %d",htotal,vtotal);
	LOG(LOG_VGA,LOG_NORMAL)("H D End %d, V D End %d",hdispend,vdispend);
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
	Bitu width,height,pitch;
	Bitu scalew=1;
	Bitu scaleh=1;

	width=hdispend;
	height=vdispend;
	vga.draw.double_scan=false;
	vga.draw.font_height=(vga.crtc.maximum_scan_line&0xf)+1;
	switch (vga.mode) {
	case M_VGA:
		scalew=2;
		scaleh*=vga.draw.font_height;
		if (vga.crtc.maximum_scan_line&0x80) scaleh*=2;
		vga.draw.lines_scaled=scaleh;
		height/=scaleh;
		width<<=2;
		pitch=vga.config.scan_len*8;
		vga.draw.address_add=vga.config.scan_len*2;
		vga.draw.address_line_total=1;
		VGA_DrawLine=VGA_VGA_Draw_Line;
		break;
	case M_LIN8:
		width<<=3;
		scaleh*=vga.draw.font_height;
		pitch=vga.config.scan_len*4;
		vga.draw.address_add=vga.config.scan_len*2;
		vga.draw.lines_scaled=scaleh;
		vga.draw.address_line_total=1;
		VGA_DrawLine=VGA_VGA_Draw_Line;
		break;
	case M_EGA16:
		width<<=3;
		pitch=vga.config.scan_len*16;
		scaleh*=vga.draw.font_height;
		if (vga.crtc.maximum_scan_line&0x80) scaleh*=2;
		vga.draw.lines_scaled=scaleh;
		height/=scaleh;
		if (vga.seq.clocking_mode & 0x8) scalew*=2;
		vga.draw.address_add=vga.config.scan_len*2;
		vga.draw.address_line_total=1;
		VGA_DrawLine=VGA_EGA_Draw_Line;
		break;
	case M_CGA4:
	case M_CGA16:							//Let is use 320x200 res and double pixels myself
		scaleh=2;scalew=2;
		vga.draw.blocks=width;
		width<<=2;
		pitch=width;
		vga.draw.lines_scaled=1;
		vga.draw.address_line_total=2;
		vga.draw.address_add=80;			//CGA doesn't have an offset reg
		VGA_DrawLine=(vga.mode == M_CGA4) ? VGA_CGA4_Draw_Line : VGA_CGA16_Draw_Line;
		break;
	case M_CGA2:
		scaleh=2;
		vga.draw.address_line_total=2;
		vga.draw.blocks=width;
		width<<=3;
		pitch=width;
		vga.draw.address_line_total=2;
		vga.draw.address_add=80;			//CGA doesn't have an offset reg
		vga.draw.lines_scaled=1;
		VGA_DrawLine=VGA_CGA2_Draw_Line;
		break;
	case M_HERC:
		vga.draw.address_line_total=4;
		width*=9;
		vga.draw.blocks=width/8;
		vga.draw.address_add=width/8;
		vga.draw.lines_scaled=1;
		height=348;
		pitch=width;
		aspect_ratio=1.5;
		VGA_DrawLine=VGA_HERC_Draw_Line;
		break;
	case M_TANDY16:
		scaleh=2;scalew=2;
		vga.draw.blocks=width*2;
		vga.draw.address_add=160;
		vga.draw.address_line_total=4;
		vga.draw.lines_scaled=1;
		width<<=2;
		pitch=width;
		VGA_DrawLine=VGA_TANDY16_Draw_Line;
		break;
	case M_TEXT2:
	case M_TEXT16:
		aspect_ratio=1.0;
		vga.draw.address_line_total=vga.draw.font_height;
		if (vga.draw.font_height<4 && (machine<MCH_VGA || machine==MCH_AUTO)) {
			vga.draw.font_height=4;
		};
		vga.draw.blocks=width;
		vga.draw.address_add=vga.config.scan_len*4;
		if (vga.seq.clocking_mode & 0x8) scalew*=2;
		if (vga.crtc.maximum_scan_line&0x80) scaleh*=2;
		vga.draw.lines_scaled=scaleh;
		height/=scaleh;
		width<<=3;				/* 8 bit wide text font */
		if (width>640) width=640;
		if (height>480) height=480;
		pitch=width;
		VGA_DrawLine=VGA_TEXT_Draw_Line;
		break;
	default:
		LOG(LOG_VGA,LOG_ERROR)("Unhandled VGA type %d while checking for resolution");
	};
	vga.draw.lines_total=height;
	if (( width != vga.draw.width) || (height != vga.draw.height) || (pitch != vga.draw.pitch)) {
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_VerticalDisplayEnd);
		vga.draw.width=width;
		vga.draw.height=height;
		vga.draw.pitch=pitch;
		vga.draw.scaleh=scaleh;

		LOG(LOG_VGA,LOG_NORMAL)("Width %d, Height %d, fps %f",width,height,fps);
		LOG(LOG_VGA,LOG_NORMAL)("Scalew %d, Scaleh %d aspect %f",scalew,scaleh,aspect_ratio);
		RENDER_SetSize(width,height,8,pitch,aspect_ratio,scalew,scaleh);
		PIC_AddEvent(VGA_VerticalTimer,vga.draw.micro.vtotal);
	}
};
