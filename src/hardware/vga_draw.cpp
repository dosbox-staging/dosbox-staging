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

#include <string.h>
#include "dosbox.h"
#include "video.h"
#include "vga.h"

//TODO Make the full draw like the vga really does from video memory.

void VGA_DrawGFX2_Fast(Bit8u * bitdata,Bitu pitch) {
	Bit8u * reader=HostMake(0xB800,0);
	Bit8u * flip=HostMake(0xB800,8*1024);
	Bit8u * draw;
	for (Bitu y=0;y<vga.draw.height;y++) {
		Bit8u * tempread;
		tempread=reader;
		if (y&1) {
			tempread+=8*1024;
			reader+=80;
		};
		draw=bitdata;
		//TODO Look up table like in 4color mode
		for (Bit32u x=vga.draw.width>>3;x>0;x--) {
			Bit8u val=*(tempread++);
			*(draw+0)=(val>>7)&1;
			*(draw+1)=(val>>6)&1;
			*(draw+2)=(val>>5)&1;
			*(draw+3)=(val>>4)&1;
			*(draw+4)=(val>>3)&1;
			*(draw+5)=(val>>2)&1;
			*(draw+6)=(val>>1)&1;
			*(draw+7)=(val>>0)&1;
			draw+=8;
		}
		bitdata+=pitch;
	}
}

void VGA_DrawGFX4_Fast(Bit8u * bitdata,Bitu pitch) {
	Bit8u * reader=HostMake(0xB800,vga.config.display_start*2);
	Bit8u * flip=HostMake(0xB800,8*1024);
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
		for (Bit32u x=0;x<vga.draw.width>>2;x++) {
			Bit8u val=*(tempread++);
			*(Bit32u *)draw=CGAWriteTable[val];
			draw+=4;
		}
		bitdata+=pitch;
	}
}

/* Draw the screen using the lookup buffer */
void VGA_DrawGFX16_Fast(Bit8u * bitdata,Bitu next_line) {
	Bit8u * reader=&vga.buffer[vga.config.display_start*8+vga.config.pel_panning];
	for (Bitu y=0;y<vga.draw.height;y++) {
		memcpy((void *)bitdata,(void *)reader,vga.draw.width);
		bitdata+=vga.draw.width+next_line;
		reader+=vga.config.scan_len*16;
	}
}

void VGA_DrawGFX256U_Fast(Bit8u * bitdata,Bitu next_line) {
	Bit16u yreader=vga.config.display_start*1;
	/* TODO add pel panning */
	for (Bitu y=0;y<vga.draw.height;y++) {
		Bit16u xreader=yreader;
		for (Bitu x=0;x<vga.draw.width>>2;x++) {
			for (Bit32u dx=0;dx<4;dx++) { 
				(*bitdata++)=vga.mem.paged[xreader][dx];
			}
			xreader++;
		}
		yreader+=vga.config.scan_len*2;
		bitdata+=next_line;
	}
}

void VGA_DrawTEXT(Bit8u * bitdata,Bitu pitch) {
	Bit8u * reader=HostMake(0xB800,0);
	Bit8u * draw_start=bitdata;
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
				draw+=pitch;
			};
			draw_char+=8;
		};
		draw_start+=16*pitch;
	};
    if(!(vga.internal.cursor & 0x2000)) {
		/* Draw a cursor */
		if (((Bitu)vga.draw.cursor.col*8)>=vga.draw.width) return;
		if (((Bitu)vga.draw.cursor.row*16)>=vga.draw.height) return;
		Bit8u * cursor_draw=bitdata+(vga.draw.cursor.row*16+15)*pitch+vga.draw.cursor.col*8;
		if (vga.draw.cursor.count>8) {
			for (Bit8u loop=0;loop<8;loop++) *cursor_draw++=15;
		}
		vga.draw.cursor.count++;
		if (vga.draw.cursor.count>16) vga.draw.cursor.count=0;
    }
};

