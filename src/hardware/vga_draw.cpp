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
#include "dosbox.h"
#include "video.h"
#include "render.h"
#include "vga.h"
#include "pic.h"

//TODO Make the full draw like the vga really does from video memory.

#define FIXED_CGA_SIZED	1

static void VGA_HERC_Draw(Bit8u * bitdata,Bitu pitch) {
	Bit8u * reader=&vga.mem.linear[(vga.herc.mode_control & 0x80) ? 8*1024 : 0];
	for (Bitu y=0;y<vga.draw.height;y++) {
		Bit8u * tempread=reader+((y & 3) * 8 * 1024);
		Bit8u * draw=bitdata;
		for (Bitu x=vga.draw.width>>3;x>0;x--) {
			Bit8u val=*(tempread++);
			*(Bit32u *)(draw+0)=CGA_2_Table[val >> 4];
			*(Bit32u *)(draw+4)=CGA_2_Table[val & 0xf];
			draw+=8;
		}
		if ((y & 3)==3) reader+=90;
		bitdata+=pitch;
	}
}

static void VGA_CGA2_Draw(Bit8u * bitdata,Bitu pitch) {
	Bit8u * reader=&vga.mem.linear[0];
	Bit8u * flip=&vga.mem.linear[8*1024];
	Bit8u * draw;
	for (Bitu y=0;y<vga.draw.height;y++) {
		Bit8u * tempread=reader;
		if (y&1) {
			tempread+=8*1024;
			reader+=80;
		};
		draw=bitdata;
		for (Bitu x=vga.draw.width>>3;x>0;x--) {
			Bit8u val=*(tempread++);
			*(Bit32u *)(draw+0)=CGA_2_Table[val >> 4];
			*(Bit32u *)(draw+4)=CGA_2_Table[val & 0xf];
			draw+=8;
		}
		bitdata+=pitch;
	}
}

static void VGA_CGA4_Draw(Bit8u * bitdata,Bitu pitch) {
	Bit8u * reader=&vga.mem.linear[0];
	Bit8u * flip=&vga.mem.linear[8*1024];
	Bit8u * draw;
	for (Bitu y=0;y<vga.draw.height;y++) {
		Bit8u * tempread;
		tempread=reader;
		if (y&1) {
			tempread+=8*1024;
			reader+=80;
			if (reader>=flip) reader-=8*1024;
		}
		draw=bitdata;
		for (Bitu x=0;x<vga.draw.width>>2;x++) {
			Bit8u val=*(tempread++);
			*(Bit32u *)draw=CGA_4_Table[val];
			draw+=4;
		}
		bitdata+=pitch;
	}
}


static Bit8u convert16[16]={
	0x0,0x2,0x1,0x3,0x5,0x7,0x4,0x9,
	0x6,0xa,0x8,0xb,0xd,0xe,0xc,0xf
};

static void VGA_CGA16_Draw(Bit8u * bitdata,Bitu pitch) {
	Bit8u * reader=&vga.mem.linear[0];
	Bit8u * flip=&vga.mem.linear[8*1024];
	Bit8u * draw;
	for (Bitu y=0;y<200;y++) {
		Bit8u * tempread;
		tempread=reader;
		if (y&1) {
			tempread+=8*1024;
			reader+=80;
			if (reader>=flip) reader-=8*1024;
		}
		draw=bitdata;
		for (Bitu x=0;x<80;x++) {
			Bit8u val=*(tempread++);

			Bit32u full=convert16[(val & 0xf0) >> 4] | convert16[val & 0xf] << 16;
			full|=full<<8;
			*(Bit32u *)draw=full;
			draw+=4;
		}
		bitdata+=pitch;
	}
}

static void VGA_TANDY16_Draw(Bit8u * bitdata,Bitu pitch) {
	Bit8u * reader=&vga.mem.linear[(vga.tandy.disp_bank << 14) + vga.config.display_start*2];

	for (Bitu y=0;y<vga.draw.height;y++) {
		Bit8u * tempread=reader+((y & 3) * 8 * 1024);
		Bit8u * draw=bitdata;
		for (Bitu x=0;x<vga.draw.width>>2;x++) {
			Bit8u val1=*(tempread++);
			Bit8u val2=*(tempread++);
			Bit32u full=(val1 & 0x0f) << 8  |
						(val1 & 0xf0) >> 4  |
						(val2 & 0x0f) << 24 |
						(val2 & 0xf0) << 12;
			*(Bit32u *)draw=full;
			draw+=4;
		}
		bitdata+=pitch;
		if ((y & 3)==3)reader+=160;
	}
}


void VGA_TEXT_Draw(Bit8u * bitdata,Bitu start,Bitu panning,Bitu rows) {
	Bit8u * reader=&vga.mem.linear[start*2];
	Bit8u * draw_start=bitdata;
/* Todo Blinking and high intensity colors */
	Bitu next_charline=vga.draw.font_height*vga.draw.width;
	Bitu next_line=vga.draw.width;
	Bitu next_start=(vga.config.scan_len*2)-vga.draw.cols;
	for (Bitu cy=rows;cy>0;cy--) {
		Bit8u * draw_char=draw_start;	
		/* Do first character keeping track of panning */
		{
			Bit8u c=*(reader++);
			Bit8u * findex=&vga.draw.font[c*32];
			Bit8u col=*(reader++);
			Bit8u fg=col & 0xF;
			Bit8u bg=(col>> 4);
			Bit8u * draw_line=draw_char;
			Bit8u bit_index=1 << (7-panning);
			for (Bitu y=vga.draw.font_height;y>0;y--) {
				Bit8u * draw=draw_line;
				draw_line+=next_line;
				Bit8u bit=bit_index;
				Bit8u bit_mask=*findex++;
				while (bit) {
					if (bit_mask & bit) *draw=fg;
					else *draw=bg;
					draw++;bit>>=1;
				}
			}
			draw_char+=8-panning;
		}
		for (Bitu cx=vga.draw.cols-1;cx>0;cx--) {
			Bit8u c=*(reader++);
			Bit8u * findex=&vga.draw.font[c*32];
			Bit8u col=*(reader++);
			Bit8u fg=col & 0xF;
			Bit8u bg=(col>> 4);
			Bit8u * draw=draw_char;
			for (Bitu y=vga.draw.font_height;y>0;y--) {
				Bit8u bit_mask=*findex++;
				#include "font-switch.h"
				draw+=next_line;
			}
			draw_char+=8;
		}
		/* Do last character if needed */
		if (panning) {
			Bit8u c=*(reader);
			Bit8u * findex=&vga.draw.font[c*32];
			Bit8u col=*(reader+1);
			Bit8u fg=col & 0xF;
			Bit8u bg=(col>> 4);
			Bit8u * draw_line=draw_char;
			Bit8u bit_index=1 << panning;
			for (Bitu y=vga.draw.font_height;y>0;y--) {
				Bit8u * draw=draw_line;
				draw_line+=next_line;
				Bit8u bit=bit_index;
				Bit8u bit_mask=*findex++;
				while (bit) {
					if (bit_mask & bit) *draw=fg;
					else *draw=bg;
					draw++;bit>>=1;
				}
			}
		}
		draw_start+=next_charline;
		reader+=next_start;
	}
/* Cursor handling */
	vga.draw.cursor.count++;
	if (vga.draw.cursor.count>16) vga.draw.cursor.count=0;

    if(vga.draw.cursor.enabled && (vga.draw.cursor.count>8)) {	/* Draw a cursor if enabled */
		Bits cur_start=vga.config.cursor_start-start;
		if (cur_start<0) return;

		Bitu row=cur_start / (vga.config.scan_len*2);
		Bitu col=cur_start % (vga.config.scan_len*2);
		Bit32u att=vga.mem.linear[vga.config.cursor_start*2+1]&0xf;
		att=(att << 8) | att;
		att=(att << 16) | att;

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
}

static void EndRetrace(void) {
	/* start the actual display update now */
	RENDER_DoUpdate();
	vga.config.retrace=false;
}

static void VGA_BlankTimer() {
	PIC_AddEvent(VGA_BlankTimer,vga.draw.blank);
	PIC_AddEvent(EndRetrace,667);
	/* Setup a timer to destroy the vertical retrace bit in a few microseconds */
	vga.config.real_start=vga.config.display_start;
	vga.config.retrace=true;
}


void VGA_DrawHandler(RENDER_Part_Handler part_handler) {
	Bit8u * buf,* bufsplit;
	/* Draw the current frame */
	if (!vga.draw.resizing) {
		if (vga.config.line_compare<vga.draw.lines) {
			Bitu stop=vga.config.line_compare/vga.draw.scaleh;
			if (stop>=vga.draw.height){
				LOG(LOG_VGAGFX,LOG_NORMAL)("Split at %d",stop);
				goto drawnormal;
			}
			switch (vga.mode) {
			case M_EGA16:
				buf=&vga.mem.linear[512*1024+vga.config.real_start*8+vga.config.pel_panning];
				bufsplit=&vga.mem.linear[512*1024];
				break;
			case M_VGA:
			case M_LIN8:
				buf=&vga.mem.linear[vga.config.real_start*4+vga.config.pel_panning];
				bufsplit=vga.mem.linear;
				break;
			case M_TEXT2:
			case M_TEXT16:
				{
					Bitu first_rows=stop/vga.draw.font_height;
					if (vga.config.hlines_skip) first_rows++;
					if (stop%vga.draw.font_height) first_rows++;
					Bitu next_rows=(vga.draw.height-stop)/vga.draw.font_height;
					if ((vga.draw.height-stop)%vga.draw.font_height) next_rows++;
					VGA_TEXT_Draw(&vga.mem.linear[512*1024],vga.config.real_start,vga.config.pel_panning,first_rows);
					VGA_TEXT_Draw(&vga.mem.linear[1024*1024],0,0,next_rows);
					buf=&vga.mem.linear[512*1024+vga.config.hlines_skip*vga.draw.width];
					bufsplit=&vga.mem.linear[1024*1024];
				}
				break;
			default:
				LOG(LOG_VGAGFX,LOG_NORMAL)("VGA:Unhandled split screen mode %d",vga.mode);
				goto norender;
			}
			if (stop) part_handler(buf,0,0,vga.draw.width,stop);
			if (vga.draw.height-stop) part_handler(bufsplit,0,stop,vga.draw.width,vga.draw.height-stop);
		} else {
drawnormal:
			switch (vga.mode) {
			case M_HERC:
				VGA_HERC_Draw(&vga.mem.linear[512*1024],vga.draw.width);
				buf=&vga.mem.linear[512*1024];
				break;
			case M_CGA2:
				VGA_CGA2_Draw(&vga.mem.linear[512*1024],vga.draw.width);
				buf=&vga.mem.linear[512*1024];
				break;
			case M_CGA4:
				VGA_CGA4_Draw(&vga.mem.linear[512*1024],vga.draw.width);
				buf=&vga.mem.linear[512*1024];
				break;
			case M_CGA16:
				VGA_CGA16_Draw(&vga.mem.linear[512*1024],vga.draw.width);
				buf=&vga.mem.linear[512*1024];
				break;
			case M_TANDY16:
				VGA_TANDY16_Draw(&vga.mem.linear[512*1024],vga.draw.width);
				buf=&vga.mem.linear[512*1024];
				break;
			case M_EGA16:
				buf=&vga.mem.linear[512*1024+vga.config.real_start*8+vga.config.pel_panning];
				break;
			case M_VGA:
			case M_LIN8:
				buf=&vga.mem.linear[vga.config.real_start*4+vga.config.pel_panning];
				break;
			case M_TEXT2:
			case M_TEXT16:
				{
					Bitu rows=vga.draw.rows;
					if (vga.config.hlines_skip) rows++;
					VGA_TEXT_Draw(&vga.mem.linear[512*1024],vga.config.real_start,vga.config.pel_panning,rows);
					buf=&vga.mem.linear[512*1024+vga.config.hlines_skip*vga.draw.width];
				}
				break;
			default:
				return;
			}
			part_handler(buf,0,0,vga.draw.width,vga.draw.height);
		}
norender:;
	}

}


void VGA_SetupDrawing(void) {
	/* Calculate the FPS for this screen */
	double fps;
	Bitu vtotal=2 + vga.crtc.vertical_total | 
		((vga.crtc.overflow & 1) << 8) | ((vga.crtc.overflow & 0x20) << 4)
		;
	Bitu htotal=5 + vga.crtc.horizontal_total;
	Bitu vdispend = 1 + (vga.crtc.vertical_display_end | 
		((vga.crtc.overflow & 2)<<7) | ((vga.crtc.overflow & 0x40) << 3) | 
		((vga.s3.ex_ver_overflow & 0x2) << 9));
	Bitu hdispend = 1 + (vga.crtc.horizontal_display_end);
	
	Bitu hbstart = vga.crtc.start_horizontal_blanking;
	Bitu vbstart = vga.crtc.start_vertical_blanking | ((vga.crtc.overflow & 0x08) << 5) | ((vga.crtc.maximum_scan_line & 0x20) << 4) ;
	
	if (hbstart<hdispend) 
		hdispend=hbstart;
	if (vbstart<vdispend) 
		vdispend=vbstart;
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
	double correct_ratio=(100.0/525.0);
	double aspect_ratio=((double)htotal/((double)vtotal)/correct_ratio);
	
	vga.draw.resizing=false;
	Bitu width,height,pitch;
	Bitu scalew=1;
	Bitu scaleh=1;

	vga.draw.lines=height=vdispend;
	width=hdispend;
	vga.draw.double_height=vga.config.vline_double;
	vga.draw.double_width=(vga.seq.clocking_mode & 0x8)>0;
	vga.draw.font_height=vga.config.vline_height+1;
	switch (vga.mode) {
	case M_VGA:
		vga.draw.double_width=true;		//Hack since 256 color modes use 2 clocks for a pixel
		/* Don't know might do this different sometime, will have to do for now */
		scaleh*=vga.draw.font_height;
		width<<=2;
		pitch=vga.config.scan_len*8;
		break;
	case M_LIN8:
		width<<=3;
		scaleh*=vga.draw.font_height;
		pitch=vga.config.scan_len*8;
		break;
	case M_EGA16:
		width<<=3;
		pitch=vga.config.scan_len*16;
		scaleh*=vga.draw.font_height;
		break;
	case M_CGA4:
	case M_CGA16:							//Let is use 320x200 res and double pixels myself
		vga.draw.double_width=true;			//Hack if there's a runtime switch
		vga.draw.double_height=true;		//Hack if there's a runtime switch
		width<<=2;
		pitch=width;
		break;
	case M_CGA2:
		vga.draw.double_width=false;		//Hack if there's a runtime switch
		width<<=3;
		pitch=width;
		break;
	case M_HERC:
		vga.draw.double_height=false;		//Hack if there's a runtime switch
		width*=9;
		height=384;
		pitch=width;
		aspect_ratio=1.0;
		break;
	case M_TANDY16:
		width<<=3;
		pitch=width;
		break;
	case M_TEXT2:
	case M_TEXT16:
		aspect_ratio=1.0;
		vga.draw.font_height=vga.config.vline_height+1;
		if (vga.draw.font_height<4 && (machine<MCH_VGA || machine==MCH_AUTO)) {
			vga.draw.font_height=4;
		};
		vga.draw.cols=width;
		vga.draw.rows=(height/vga.draw.font_height);
		if (height % vga.draw.font_height) vga.draw.rows++;
		width<<=3;				/* 8 bit wide text font */
		if (width>640) width=640;
		if (height>480) height=480;
		pitch=width;
		break;
	default:
		LOG(LOG_VGA,LOG_ERROR)("Unhandled VGA type %d while checking for resolution");
	};
	if (vga.draw.double_height) {
		scaleh*=2;
	}
	height/=scaleh;
	if (vga.draw.double_width) {
		/* Double width is dividing main clock, the width should be correct already for this */
		scalew*=2;
	}
	if (( width != vga.draw.width) || (height != vga.draw.height) || (pitch != vga.draw.pitch)) {
		PIC_RemoveEvents(VGA_BlankTimer);
		vga.draw.width=width;
		vga.draw.height=height;
		vga.draw.pitch=pitch;
		vga.draw.scaleh=scaleh;

		LOG(LOG_VGA,LOG_NORMAL)("Width %d, Height %d, fps %f",width,height,fps);
		LOG(LOG_VGA,LOG_NORMAL)("Scalew %d, Scaleh %d aspect %f",scalew,scaleh,aspect_ratio);
		RENDER_SetSize(width,height,8,pitch,aspect_ratio,scalew,scaleh,&VGA_DrawHandler);
		vga.draw.blank=(Bitu)(1000000/fps);
		PIC_AddEvent(VGA_BlankTimer,vga.draw.blank);
	}
};
