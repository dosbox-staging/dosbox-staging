/*
 *  Copyright (C) 2002  The DOSBox Team
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
#include "vga.h"


/* This Should draw a complete 16 colour screen */

void VGA_Render_GFX_4(Bit8u * * data) {
	*data=vga.buffer;
	VGA_DrawGFX4_Full(vga.buffer,vga.draw.width);
	vga.config.retrace=true;
}

void VGA_Render_GFX_16(Bit8u * * data) {
	*data=&vga.buffer[vga.config.display_start*8+vga.config.pel_panning];
	vga.config.retrace=true;
}

void VGA_Render_GFX_256U(Bit8u * * data) {
	*data=&vga.mem.linear[vga.config.display_start*4+vga.config.pel_panning];
	vga.config.retrace=true;
}

void VGA_Render_GFX_256C(Bit8u * * data) {
	*data=memory+0xa0000;
	vga.config.retrace=true;
}

void VGA_Render_TEXT_16(Bit8u * * data) {
	*data=vga.buffer;
	VGA_DrawTEXT(vga.buffer,vga.draw.width);
	vga.config.retrace=true;
}




void VGA_DrawGFX4_Full(Bit8u * bitdata,Bitu next_line) {
	//TODO use vga memory handler
	Bit8u * reader=real_host(0xB800,0);
	Bit8u * draw;
	for (Bitu y=0;y<vga.draw.height;y++) {
		Bit8u * tempread;
		tempread=reader;
		if (y&1) {
			tempread+=8*1024;
			reader+=80;
		};
		draw=bitdata;
		for (Bit32u x=0;x<vga.draw.width>>2;x++) {
			Bit8u val=*(tempread++);
/*
			*(draw+0)=(val>>6)&3;
			*(draw+1)=(val>>4)&3;
			*(draw+2)=(val>>2)&3;
			*(draw+3)=(val)&3;
			draw+=4;
*/
			*(Bit32u *)draw=CGAWriteTable[val];
			draw+=4;
		}
		//TODO use scanline length and dword mode crap
		bitdata+=next_line;
	};
	vga.config.retrace=true;
}

void VGA_DrawGFX16_Full(Bit8u * bitdata,Bitu next_line) {
	Bit8u * reader=&vga.buffer[vga.config.display_start*8+vga.config.pel_panning];
	for (Bitu y=0;y<vga.draw.height;y++) {
		memcpy((void *)bitdata,(void *)reader,vga.draw.width);
		bitdata+=vga.draw.width+next_line;
		reader+=vga.config.scan_len*16;
	}
	vga.config.retrace=true;
};


/* This Should draw a complete 256 colour screen */

void VGA_DrawGFX256_Full(Bit8u * bitdata,Bitu next_line) {
	Bit16u yreader=vga.config.display_start*1;
	/* Now add pel panning */
	for (Bitu y=0;y<vga.draw.height;y++) {
		Bit16u xreader=yreader;
		for (Bitu x=0;x<vga.draw.width>>2;x++) {
			for (Bit32u dx=0;dx<4;dx++) { 
				(*bitdata++)=vga.mem.paged[xreader][dx];
			}
			xreader++;
		}
		//TODO use scanline length and dword mode crap
		yreader+=vga.config.scan_len*2;
		bitdata+=next_line;
	};
	vga.config.retrace=true;
};

void VGA_DrawGFX256_Fast(Bit8u * bitdata,Bitu next_line) {
	/* For now just copy 64 kb of memory with pitch support */
	Bit8u * reader=memory+0xa0000;
	for (Bitu y=0;y<vga.draw.height;y++) {
		memcpy((void *)bitdata,(void *)reader,vga.draw.width);
		bitdata+=vga.draw.width+next_line;
		reader+=vga.draw.width;
	}
	//memcpy((void *)bitdata,(void *)(memory+0xa0000),320*200);
	vga.config.retrace=true;
};


void VGA_DrawTEXT(Bit8u * bitdata,Bitu next_line) {
	Bit8u * reader=real_off(0xB800,0);
	Bit8u * draw_start=bitdata;;
/* Todo Blinking and high intensity colors */
	for (Bitu cy=0;cy<(vga.draw.height/16);cy++) {
		Bit8u * draw_char=draw_start;	
		for (Bitu cx=0;cx<(vga.draw.width/8);cx++) {
			Bit8u c=*(reader++);
			Bit8u * findex=&vga_rom_16[c*16];
			Bit8u col=*(reader++);
			Bit8u fg=col & 0xF;
			Bit8u bg=(col>> 4);
			Bit8u * draw=draw_char;
			for (Bitu y=0;y<16;y++) {
				Bit8u bit_mask=*findex++;
				#include "font-switch.h"
				draw+=+next_line;
			};
			draw_char+=8;
		};
		draw_start+=16*next_line;
	};
	vga.config.retrace=true;

	/* Draw a cursor */
	if ((vga.draw.cursor_col*8)>=vga.draw.width) return;
	if ((vga.draw.cursor_row*16)>=vga.draw.height) return;
	Bit8u * cursor_draw=bitdata+(vga.draw.cursor_row*16+15)*vga.draw.width+vga.draw.cursor_col*8;
	if (vga.draw.cursor_count>8) {
		for (Bit8u loop=0;loop<8;loop++) *cursor_draw++=15;
	}
	vga.draw.cursor_count++;
	if (vga.draw.cursor_count>16) vga.draw.cursor_count=0;
};

