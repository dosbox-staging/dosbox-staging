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
#include "inout.h"
#include "vga.h"
#include <stdio.h>

struct XGAStatus {
	struct scissorreg {
		Bit16u x1, y1, x2, y2;
	} scissors;

	Bit32u readmask;
	Bit32u writemask;

	Bit32u forecolor;
	Bit32u backcolor;

	Bit16u foremix;

	Bit16u curx, cury;
	Bit16u destx, desty;

	Bit16u MIPcount;
	Bit16u MAPcount;

	Bit16u pix_cntl;
	Bit16u read_sel;

	struct XGA_WaitCmd {
		bool wait;
		Bit16u cmd;
		Bit16u curx, cury;
		Bit16u x1, y1, x2, y2;
	} waitcmd;

} xga;

Bit8u tmpvram[2048 * 1024];

void XGA_Write_Multifunc(Bitu val, Bitu len) {
	Bitu regselect = val >> 12;
	Bitu dataval = val & 0xfff;
	switch(regselect) {
		case 0:
			xga.MIPcount = dataval;
			break;
		case 1:
			xga.scissors.y1 = dataval;
			break;
		case 2:
			xga.scissors.x1 = dataval;
			break;
		case 3:
			xga.scissors.y2 = dataval;
			break;
		case 4:
			xga.scissors.x2 = dataval;
			break;
		case 0xa:
			xga.pix_cntl = dataval;
			break;
		case 0xf:
			xga.read_sel = dataval;
			break;
		default:
			LOG_MSG("XGA: Unhandled multifunction command %x", regselect);
			break;
	}
}

void XGA_DrawPoint8(Bitu x, Bitu y, Bit8u c) {
	Bit32u memaddr = (y * 640) + x;
	vga.mem.linear[memaddr] = c;

}

Bit8u XGA_GetPoint8(Bitu x, Bitu y) {
	Bit32u memaddr = (y * 640) + x;
	return vga.mem.linear[memaddr];


}

void XGA_DrawPoint16(Bitu x, Bitu y, Bit16u c) {
	Bit16u *memptr;
	Bit32u memaddr = (y * 640) + x;
	memptr = (Bit16u *)&vga.mem.linear[memaddr];
	*memptr = c;
}

void XGA_DrawRectangle(Bitu x1, Bitu y1, Bitu x2, Bitu y2) {
	Bit32u xat, yat;
	Bit32u xmass, xmod, xdist;
	Bit32u *memptr;
	Bit8u *smallptr;
	Bit32u c;
	Bit8u smallc;

	xdist = (x2 -x1);
	xmass = (xdist) & 0xfffffffb;
	xmod = (xdist) & 0x3;

	smallc = (xga.forecolor & 0xff);
	c = (smallc) | ((smallc) << 8) | ((smallc) << 16) | ((smallc) << 24);

	for(yat=y1;yat<=y2;yat++) {
		Bit32u memaddr = (yat * (Bit32u)640) + x1; 
		smallptr = &vga.mem.linear[memaddr];
		for(xat=0;xat<xdist;xat++) *smallptr++ = smallc;
	}
	/*
	for(yat=y1;yat<=y2;yat++) {
		Bit32u memaddr = (yat * (Bit32u)640) + x1;
		memptr = (Bit32u *)&vga.mem.linear[memaddr];
		for(xat=0;xat<xmass;xat+=4) *memptr++ = c;
		if(xmod!=0) {
			smallptr = (Bit8u *)memptr;
			for(xat=0;xat<xmod;xat++) *smallptr++ = smallc;
		}
	}
	*/
	LOG_MSG("XGA: Draw rect (%d, %d)-(%d, %d), %d", x1, y1, x2, y2, xga.forecolor);
}

bool XGA_CheckX(void) {
	bool newline = false;
	if(xga.waitcmd.curx > xga.waitcmd.x2) {
		xga.waitcmd.curx = xga.waitcmd.x1;
		xga.waitcmd.cury++;
		newline = true;
		if(xga.waitcmd.cury > xga.waitcmd.y2) xga.waitcmd.wait = false;
	}
	return newline;
}

void XGA_DrawWait(Bitu val, Bitu len) {
	if(!xga.waitcmd.wait) return;
	Bitu mixmode = (xga.pix_cntl >> 6) & 0x3;

	switch(xga.waitcmd.cmd) {
		case 2: /* Rectangle */
			if(mixmode == 0) { /* FOREMIX always used */
				switch(len) {
					case 1:
						XGA_DrawPoint8(xga.waitcmd.curx++, xga.waitcmd.cury, val);
						break;
					case 2:
						XGA_DrawPoint8(xga.waitcmd.curx++, xga.waitcmd.cury, (val & 0xff));
						XGA_CheckX();
						XGA_DrawPoint8(xga.waitcmd.curx++, xga.waitcmd.cury, (val >> 8));
						break;
					case 4:
						XGA_DrawPoint8(xga.waitcmd.curx++, xga.waitcmd.cury, (val & 0xff));
						XGA_CheckX();
						XGA_DrawPoint8(xga.waitcmd.curx++, xga.waitcmd.cury, ((val >> 8) & 0xff));
						XGA_CheckX();
						XGA_DrawPoint8(xga.waitcmd.curx++, xga.waitcmd.cury, ((val >> 16) & 0xff));
						XGA_CheckX();
						XGA_DrawPoint8(xga.waitcmd.curx++, xga.waitcmd.cury, ((val >> 24) & 0xff));
						break;
				}
				XGA_CheckX();
			}
			if(mixmode == 2) { /* Data from PIX_TRANS selects the mix */
				Bitu bitcount;
				int i;
				switch(len) {
					case 1:
						bitcount = 8;
						break;
					case 2:
						bitcount = 16;
						val = ((val & 0xff) << 8) | ((val >> 8) & 0xff);
						break;
					case 4:
						bitcount = 32;
						break;
				}
				

				Bits bitneed = ((Bits)xga.waitcmd.x2 - (Bits)xga.waitcmd.x1) + 1;
				xga.waitcmd.curx = xga.waitcmd.x1;
				i = 15;
				for(;bitneed>=0;--bitneed) {
					Bitu bitval = (val >> i) & 0x1;
					//XGA_DrawPoint8(xga.waitcmd.curx, xga.waitcmd.cury, i);
					if(bitval != 0) XGA_DrawPoint8(xga.waitcmd.curx, xga.waitcmd.cury, xga.forecolor);
					--i;
					xga.waitcmd.curx++;
				}
				xga.waitcmd.cury++;
				
			
				if(xga.waitcmd.cury > xga.waitcmd.y2) xga.waitcmd.wait = false;
			}
			break;
		default:
			LOG_MSG("XGA: Unhandled draw command %x", xga.waitcmd.cmd);
			break;
	}

}

void XGA_BlitRect(Bitu val) {
	Bit32u xat, yat;
	Bit32u xmass, xmod, xdist, memrec;
	Bit8u *srcptr;
	Bit8u *destptr;
	Bit8u *destline;
	Bit8u *srcline;

	Bit32u c;
	Bit8u smallc;
	Bit8u tmpclr;
	bool incx = false;
	bool incy = false; 

	if((val >> 5) != 0) incx = true;
	if((val >> 7) != 0) incy = true;

	xdist = xga.MAPcount;

	smallc = (xga.forecolor & 0xff);
	memrec = 0;
	Bit32u srcaddr = (xga.cury * (Bit32u)640) + xga.curx;
	Bit32u destaddr = (xga.desty * (Bit32u)640) + xga.destx;

	srcptr = &vga.mem.linear[srcaddr];
	destptr = &vga.mem.linear[destaddr];

	/* Copy source to video ram */
	for(yat=0;yat<=xga.MIPcount ;yat++) {
		srcline = srcptr;
		destline = destptr;
		for(xat=0;xat<xga.MAPcount;xat++) {
			*destline = *srcline;
			//LOG_MSG("Copy (%d, %d) to (%d, %d)", sx, sy, tx, ty);
			if(incx) {
				destline++;
				srcline++;
			} else {
				--destline;
				--srcline;
			}
		}
		if(incy) {
			srcptr+=640;
			destptr+=640;
		} else {
			srcptr-=640;
			destptr-=640;
		}
	}

	LOG_MSG("XGA: Blit (%d, %d)-(%d, %d) to (%d, %d)-(%d, %d), incx %d, incy %d", xga.curx, xga.cury, xga.curx + xdist, xga.cury + xga.MIPcount, xga.destx, xga.desty, xga.destx + xdist, xga.desty + xga.MIPcount, incx, incy);

}

void XGA_DrawPattern(void) {
	Bit32u xat, yat, y1, y2, sx, sy, addx, addy;
	Bit32u xmass, xmod, xdist;
	Bit32u *memptr;
	Bit8u *smallptr;
	Bit8u smallc;

	y1 = xga.desty;
	y2 = xga.desty + xga.MIPcount;
	xdist = xga.MAPcount;
	sx = xga.curx;
	sy = xga.cury;
	addx = 0;
	addy = 0;

	for(yat=y1;yat<=y2;yat++) {
		Bit32u memaddr = (yat * (Bit32u)640) + xga.destx;
		smallptr = &vga.mem.linear[memaddr];
		for(xat=0;xat<xdist;xat++) {
			*smallptr++ = XGA_GetPoint8(sx + addx, sy + addy);
			addx++;
			if(addx>7) addx=0;
		}
		addy++;
		if(addy>7) addy=0;
	}

}

void XGA_DrawCmd(Bitu val, Bitu len) {
	Bit16u cmd;
	cmd = val >> 13;
	LOG_MSG("XGA: Draw command %x", cmd);
	switch(cmd) {
		case 2: /* Rectangle fill */
			if((val & 0x100) == 0) {
				xga.waitcmd.wait = false;
				XGA_DrawRectangle(xga.curx, xga.cury, xga.curx + xga.MAPcount, xga.cury + xga.MIPcount);
			} else {
				xga.waitcmd.wait = true;
				xga.waitcmd.curx = xga.curx;
				xga.waitcmd.cury = xga.cury;
				xga.waitcmd.x1 = xga.curx;
				xga.waitcmd.y1 = xga.cury;
				xga.waitcmd.x2 = xga.curx + xga.MAPcount;
				xga.waitcmd.y2 = xga.cury + xga.MIPcount;
				xga.waitcmd.cmd = 2;
				LOG_MSG("XGA: Draw wait rect (%d, %d)-(%d, %d)", xga.waitcmd.x1, xga.waitcmd.y1, xga.waitcmd.x2, xga.waitcmd.y2);
			}
			break;
		case 6: /* BitBLT */
			XGA_BlitRect(val);
			break;
		case 7: /* Pattern fill */
			XGA_DrawPattern();
			LOG_MSG("XGA: Pattern fill (%d, %d)-(%d, %d) to (%d, %d)-(%d, %d)", xga.curx, xga.cury, xga.curx + 8, xga.cury + 8, xga.destx, xga.desty, xga.destx + xga.MAPcount, xga.desty + xga.MIPcount);
			break;
		default:
			LOG_MSG("XGA: Unhandled draw command %x", cmd);
			break;

	}
}

void XGA_Write(Bitu port, Bitu val, Bitu len) {
	switch(port) {
		case 0x96e8:
			xga.MAPcount = val;
			break;
		case 0x9ae8:
			XGA_DrawCmd(val, len);
			break;
		case 0xa2e8:
			xga.backcolor = val;
			break;
		case 0xa6e8:
			xga.forecolor = val;
			break;
		case 0xaae8:
			xga.writemask = val;
			break;
		case 0xaee8:
			xga.readmask = val;
			break;
		case 0x82e8:
			xga.cury = val;
			break;
		case 0x86e8:
			xga.curx = val;
			break;
		case 0x8ae8:
			xga.desty = val;
			break;
		case 0x8ee8:
			xga.destx = val;
			break;
		case 0xbae8:
			xga.foremix = val;
			break;
		case 0xbee8:
			XGA_Write_Multifunc(val, len);
			break;
		case 0xe2e8:
			XGA_DrawWait(val, len);
			break;
		default:
			LOG_MSG("XGA: Wrote to port %x with %x, len %x", port, val, len);
			break;
	}
	
}

Bitu XGA_Read(Bitu port, Bitu len) {
	LOG_MSG("XGA: Read from port %x, len %x", port, len);
	switch(port) {
		case 0x9ae8:
			return 0x0;
		case 0x9ae9:
			if(xga.waitcmd.wait) {
				return 0x4;
			} else {
				return 0x0;
			}
		case 0xa2e8:
			return xga.backcolor;
		default:
			LOG_MSG("XGA: Read from port %x, len %x", port, len);
			return 0x0;
	}
}

void XGA_UpdateHWC(void) {
	Bitu mouseaddr = (Bit32u)vga.s3.hgc.startaddr * (Bit32u)1024;
	Bits x, y, t, m, xat, r, z;
	x = vga.s3.hgc.originx;
	y = vga.s3.hgc.originy;
	Bit16u bitsA, bitsB;
	Bit16u ab, bb;
	
	/* Read mouse cursor */
	for(t=0;t<64;t++) {
		xat = 0;
		for(m=0;m<4;m++) {
            bitsA = *(Bit16u *)&vga.mem.linear[mouseaddr];
			mouseaddr+=2;
			bitsB = *(Bit16u *)&vga.mem.linear[mouseaddr];
			mouseaddr+=2;
			z = 7;
			for(r=15;r>=0;--r) {
				vga.s3.hgc.mc[t][xat] = (((bitsA >> z) & 0x1) << 1) | ((bitsB >> z) & 0x1);
				xat++;
				--z;
				if(z<0) z=15;
			}
		}
	}

}

void VGA_SetupXGA(void) {
	if (machine!=MCH_VGA) return;

	memset(&xga, 0, sizeof(XGAStatus));

	IO_RegisterWriteHandler(0x42e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x42e8,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x46e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x4ae8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	
	IO_RegisterWriteHandler(0x82e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x82e8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x82e9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x82e9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x86e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x86e8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x86e9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x86e9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x8ae8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x8ae8,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x8ee8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x8ee8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x8ee9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x8ee9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x92e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x92e8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x92e9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x92e9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x96e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x96e8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x96e9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x96e9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x9ae8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x9ae8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x9ae9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x9ae9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0x9ee8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x9ee8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0x9ee9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0x9ee9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xa2e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xa2e8,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xa6e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xa6e8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0xa6e9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xa6e9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xaae8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xaae8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0xaae9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xaae9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xaee8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xaee8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0xaee9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xaee9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xb2e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xb2e8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0xb2e9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xb2e9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xb6e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xb6e8,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xbee8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xbee8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0xbee9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xbee9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xbae8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xbae8,&XGA_Read,IO_MB | IO_MW | IO_MD);
	IO_RegisterWriteHandler(0xbae9,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xbae9,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xe2e8,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xe2e8,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xe2ea,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xe2ea,&XGA_Read,IO_MB | IO_MW | IO_MD);



}

