/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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

#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "vga.h"
#include <math.h>
#include <stdio.h>

#define XGA_SCREEN_WIDTH vga.draw.width

struct XGAStatus {
	struct scissorreg {
		Bit16u x1, y1, x2, y2;
	} scissors;

	Bit32u readmask;
	Bit32u writemask;

	Bit32u forecolor;
	Bit32u backcolor;

	Bitu curcommand;

	Bit16u foremix;
	Bit16u backmix;

	Bit16u curx, cury;
	Bit16u destx, desty;

	Bit16u ErrTerm;
	Bit16u MIPcount;
	Bit16u MAPcount;

	Bit16u pix_cntl;
	Bit16u read_sel;

	struct XGA_WaitCmd {
		bool newline;
		bool wait;
		Bit16u cmd;
		Bit16u curx, cury;
		Bit16u x1, y1, x2, y2;
	} waitcmd;

} xga;

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

	if(!(xga.curcommand & 0x10)) return;

	if(x < xga.scissors.x1) return;
	if(x > xga.scissors.x2) return;
	if(y < xga.scissors.y1) return;
	if(y > xga.scissors.y2) return;

	Bit32u memaddr = (y * XGA_SCREEN_WIDTH) + x;
	vga.mem.linear[memaddr] = c;

}

Bit8u XGA_GetPoint8(Bitu x, Bitu y) {
	Bit32u memaddr = (y * XGA_SCREEN_WIDTH) + x;
	return vga.mem.linear[memaddr];


}

void XGA_DrawPoint16(Bitu x, Bitu y, Bit16u c) {
	Bit16u *memptr;
	Bit32u memaddr = (y * XGA_SCREEN_WIDTH) + x;
	memptr = (Bit16u *)&vga.mem.linear[memaddr];
	*memptr = c;
}

Bitu XGA_GetMixResult(Bitu mixmode, Bitu srcval, Bitu dstdata) {
	Bitu destval = 0;
	switch(mixmode &  0xf) {
		case 0x00: /* not DST */
			destval = ~dstdata;
			break;
		case 0x01: /* 0 (false) */
			destval = 0;
			break;
		case 0x02: /* 1 (true) */
			destval = 0xff;
			break;
		case 0x03: /* 2 DST */
			destval = dstdata;
			break;
		case 0x04: /* not SRC */
			destval = ~srcval;
			break;
		case 0x05: /* SRC xor DST */
			destval = srcval ^ dstdata;
			break;
		case 0x06: /* not (SRC xor DST) */
			destval = ~(srcval ^ dstdata);
			break;
		case 0x07: /* SRC */
			destval = srcval;
			break;
		case 0x08: /* not (SRC and DST) */
			destval = ~(srcval & dstdata);
			break;
		case 0x09: /* (not SRC) or DST */
			destval = (~srcval) | dstdata;
			break;
		case 0x0a: /* SRC or (not DST) */
			destval = srcval | (~dstdata);
			break;
		case 0x0b: /* SRC or DST */
			destval = srcval | dstdata;
			break;
		case 0x0c: /* SRC and DST */
			destval = srcval & dstdata;
			break;
		case 0x0d: /* SRC and (not DST) */
			destval = srcval & (~dstdata);
			break;
		case 0x0e: /* (not SRC) and DST */
			destval = (~srcval) & dstdata;
			break;
		case 0x0f: /* not (SRC or DST) */
			destval = ~(srcval | dstdata);
			break;
		default:
			LOG_MSG("XGA: GetMixResult: Unknown mix.  Shouldn't be able to get here!");
			break;
	}
	return destval;

}

void XGA_DrawLineVector(Bitu val) {
	Bits xat, yat;
	Bit8u srcval;
	Bit8u destval;
	Bit8u dstdata;
	Bits i;

	Bits dx, sx, dy, sy, e;

	dx = xga.MAPcount; 
	xat = xga.curx;
	yat = xga.cury;

	switch((val >> 5) & 0x7) {
		case 0x00: /* 0 degrees */
			sx = 1;
			sy = 0;
			break;
		case 0x01: /* 45 degrees */
			sx = 1;
			sy = -1;
			break;
		case 0x02: /* 90 degrees */
			sx = 0;
			sy = -1;
			break;
		case 0x03: /* 135 degrees */
			sx = -1;
			sy = -1;
			break;
		case 0x04: /* 180 degrees */
			sx = -1;
			sy = 0;
			break;
		case 0x05: /* 225 degrees */
			sx = -1;
			sy = 1;
			break;
		case 0x06: /* 270 degrees */
			sx = 0;
			sy = 1;
			break;
		case 0x07: /* 315 degrees */
			sx = 1;
			sy = 1;
			break;
		default:  // Should never get here
			sx = 0;
			sy = 0;
			break;
	}

	//for(yat=y1;yat<=y2;yat++) {
	//	for(xat=x1;xat<=x2;xat++) {
	for (i=0;i<=dx;i++) {
		Bitu mixmode = (xga.pix_cntl >> 6) & 0x3;
		switch (mixmode) {
			case 0x00: /* FOREMIX always used */
				mixmode = xga.foremix;
				switch((mixmode >> 5) & 0x03) {
					case 0x00: /* Src is background color */
						srcval = xga.backcolor;
						break;
					case 0x01: /* Src is foreground color */
						srcval = xga.forecolor;
						break;
					case 0x02: /* Src is pixel data from PIX_TRANS register */
						//srcval = tmpval;
						LOG_MSG("XGA: DrawRect: Wants data from PIX_TRANS register");
						break;
					case 0x03: /* Src is bitmap data */
						LOG_MSG("XGA: DrawRect: Wants data from srcdata");
						//srcval = srcdata;
						break;
					default:
						LOG_MSG("XGA: DrawRect: Shouldn't be able to get here!");
						break;
				}
				dstdata = XGA_GetPoint8(xat,yat);

				destval = XGA_GetMixResult(mixmode, srcval, dstdata);

                XGA_DrawPoint8(xat,yat, destval);
				break;
			default: 
				LOG_MSG("XGA: DrawLine: Needs mixmode %x", mixmode);
				break;
		}
		xat += sx;
		yat += sy;
	}

	xga.curx = xat-1;
	xga.cury = yat;
	//	}
	//}

}

void XGA_DrawLineBresenham(Bitu val) {
	Bits xat, yat;
	Bit8u srcval;
	Bit8u destval;
	Bit8u dstdata;
	Bits i;
	Bits tmpswap;
	bool steep;

#define SWAP(a,b) tmpswap = a; a = b; b = tmpswap;

	Bits dx, sx, dy, sy, e, dmajor, dminor;

	// Probably a lot easier way to do this, but this works.

	dminor = (Bits)((Bit16s)xga.desty) >> 1;
	dmajor = -((Bits)((Bit16s)xga.destx) - (dminor << 1)) >> 1;
	
	dx = dmajor;
	if((val >> 5) & 0x1) {
        sx = 1;
	} else {
		sx = -1;
	}
	dy = dminor;
	if((val >> 7) & 0x1) {
        sy = 1;
	} else {
		sy = -1;
	}
	e = (Bits)((Bit16s)xga.ErrTerm);
	xat = xga.curx;
	yat = xga.cury;

	if((val >> 6) & 0x1) {
		steep = false;
		SWAP(xat, yat);
		SWAP(sx, sy);
	} else {
		steep = true;
	}
    
	//LOG_MSG("XGA: Bresenham: ASC %d, LPDSC %d, sx %d, sy %d, err %d, steep %d, length %d, dmajor %d, dminor %d", dx, dy, sx, sy, e, steep, xga.MAPcount, dmajor, dminor);

	//for(yat=y1;yat<=y2;yat++) {
	//	for(xat=x1;xat<=x2;xat++) {
	for (i=0;i<=xga.MAPcount;i++) { 
			Bitu mixmode = (xga.pix_cntl >> 6) & 0x3;
			switch (mixmode) {
				case 0x00: /* FOREMIX always used */
					mixmode = xga.foremix;
					switch((mixmode >> 5) & 0x03) {
						case 0x00: /* Src is background color */
							srcval = xga.backcolor;
							break;
						case 0x01: /* Src is foreground color */
							srcval = xga.forecolor;
							break;
						case 0x02: /* Src is pixel data from PIX_TRANS register */
							//srcval = tmpval;
							LOG_MSG("XGA: DrawRect: Wants data from PIX_TRANS register");
							break;
						case 0x03: /* Src is bitmap data */
							LOG_MSG("XGA: DrawRect: Wants data from srcdata");
							//srcval = srcdata;
							break;
						default:
							LOG_MSG("XGA: DrawRect: Shouldn't be able to get here!");
							break;
					}

					if(steep) {
						dstdata = XGA_GetPoint8(xat,yat);
					} else {
						dstdata = XGA_GetPoint8(yat,xat);
					}

					destval = XGA_GetMixResult(mixmode, srcval, dstdata);

					if(steep) {
						XGA_DrawPoint8(xat,yat, destval);
					} else {
						XGA_DrawPoint8(yat,xat, destval);
					}

					break;
				default: 
					LOG_MSG("XGA: DrawLine: Needs mixmode %x", mixmode);
					break;
			}
			while (e >= 0) {
				yat += sy;
				e -= (dx << 1);
			}
			xat += sx;
			e += (dy << 1);
	}

	if(steep) {
		xga.curx = xat;
		xga.cury = yat;
	} else {
		xga.curx = yat;
		xga.cury = xat;
	}
	//	}
	//}
	
}

void XGA_DrawRectangle(Bitu x1, Bitu y1, Bitu x2, Bitu y2) {
	Bit32u xat, yat;
	Bit32u xmass, xmod, xdist, ydist;
	Bit32u *memptr;
	Bit8u *smallptr;
	Bit32u c;
	Bit8u smallc;
	Bit8u srcval;
	Bit8u destval;
	Bit8u dstdata;

	xdist = (x2 -x1);
	ydist = (y2 -y1);
	xmass = (xdist) & 0xfffffffb;
	xmod = (xdist) & 0x3;

	smallc = (xga.forecolor & 0xff);

	c = (smallc) | ((smallc) << 8) | ((smallc) << 16) | ((smallc) << 24);

	for(yat=y1;yat<=y2;yat++) {
		for(xat=x1;xat<=x2;xat++) {
			Bitu mixmode = (xga.pix_cntl >> 6) & 0x3;
			switch (mixmode) {
				case 0x00: /* FOREMIX always used */
					mixmode = xga.foremix;
					switch((mixmode >> 5) & 0x03) {
						case 0x00: /* Src is background color */
							srcval = xga.backcolor;
							break;
						case 0x01: /* Src is foreground color */
							srcval = xga.forecolor;
							break;
						case 0x02: /* Src is pixel data from PIX_TRANS register */
							//srcval = tmpval;
							LOG_MSG("XGA: DrawRect: Wants data from PIX_TRANS register");
							break;
						case 0x03: /* Src is bitmap data */
							LOG_MSG("XGA: DrawRect: Wants data from srcdata");
							//srcval = srcdata;
							break;
						default:
							LOG_MSG("XGA: DrawRect: Shouldn't be able to get here!");
							break;
					}
					dstdata = XGA_GetPoint8(xat,yat);

					destval = XGA_GetMixResult(mixmode, srcval, dstdata);

                    XGA_DrawPoint8(xat,yat, destval);
					break;
				default: 
					LOG_MSG("XGA: DrawRect: Needs mixmode %x", mixmode);
					break;
	}
		}
	}
	xga.curx = xat;
	xga.cury = yat;

	//LOG_MSG("XGA: Draw rect (%d, %d)-(%d, %d), %d", x1, y1, x2, y2, xga.forecolor);
}

bool XGA_CheckX(void) {
	bool newline = false;
	if(xga.waitcmd.curx > xga.waitcmd.x2) {
		xga.waitcmd.curx = xga.waitcmd.x1;
		xga.waitcmd.cury++;
		newline = true;
		xga.waitcmd.newline = true;
		if(xga.waitcmd.cury > xga.waitcmd.y2) xga.waitcmd.wait = false;
	}
	return newline;
}


void XGA_DrawWait(Bitu val, Bitu len) {
	if(!xga.waitcmd.wait) return;
	Bitu mixmode = (xga.pix_cntl >> 6) & 0x3;
	Bit8u srcval;
	Bit8u destval;
	Bit8u dstdata;
	Bitu tmpval;
	Bits bitneed;

	switch(xga.waitcmd.cmd) {
		case 2: /* Rectangle */
			switch(mixmode) {
				case 0x00: /* FOREMIX always used */
					mixmode = xga.foremix;
					int t;
					for(t=0;t<len;t++) {
						tmpval = (val >> (8 * t)) & 0xff;
						switch((mixmode >> 5) & 0x03) {
							case 0x00: /* Src is background color */
								srcval = xga.backcolor;
								break;
							case 0x01: /* Src is foreground color */
								srcval = xga.forecolor;
								break;
							case 0x02: /* Src is pixel data from PIX_TRANS register */
								srcval = tmpval;
								//LOG_MSG("XGA: DrawBlitWait: Wants data from PIX_TRANS register");
						break;
							case 0x03: /* Src is bitmap data */
								LOG_MSG("XGA: DrawBlitWait: Wants data from srcdata");
								//srcval = srcdata;
						break;
							default:
								LOG_MSG("XGA: DrawBlitWait: Shouldn't be able to get here!");
						break;
				}



						dstdata = XGA_GetPoint8(xga.waitcmd.curx, xga.waitcmd.cury);

						destval = XGA_GetMixResult(mixmode, srcval, dstdata);

						//LOG_MSG("XGA: DrawPattern: Mixmode: %x srcval: %x", mixmode, srcval);
						
						XGA_DrawPoint8(xga.waitcmd.curx++, xga.waitcmd.cury, destval);

						//XGA_CheckX();
			}
					break;
				case 0x02: /* Data from PIX_TRANS selects the mix */
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
				

					bitneed = ((Bits)xga.waitcmd.x2 - (Bits)xga.waitcmd.curx) ;
					//xga.waitcmd.curx = xga.waitcmd.x1;
					xga.waitcmd.newline = false;

					
					i = bitcount -1 ;
					//bitneed = i;
					
				for(;bitneed>=0;--bitneed) {
					//for(;i>=0;--i) {
					Bitu bitval = (val >> i) & 0x1;
						//Bitu bitval = (val >> bitneed) & 0x1;

					//XGA_DrawPoint8(xga.waitcmd.curx, xga.waitcmd.cury, i);
						Bitu mixmode = 0x67;

						if(bitval) {
							mixmode = xga.foremix;
						} else {
							mixmode = xga.backmix;
						}

						switch((mixmode >> 5) & 0x03) {
							case 0x00: /* Src is background color */
								srcval = xga.backcolor;
								break;
							case 0x01: /* Src is foreground color */
								srcval = xga.forecolor;
								break;
							case 0x02: /* Src is pixel data from PIX_TRANS register */
								LOG_MSG("XGA: DrawBlitWait: Wants data from PIX_TRANS register");
								break;
							case 0x03: /* Src is bitmap data */
								LOG_MSG("XGA: DrawBlitWait: Wants data from srcdata");
								//srcval = srcdata;
								break;
							default:
								LOG_MSG("XGA: DrawBlitWait: Shouldn't be able to get here!");
								break;
						}

						Bit8u dstdata = XGA_GetPoint8(xga.waitcmd.curx, xga.waitcmd.cury);

						destval = XGA_GetMixResult(mixmode, srcval, dstdata);

						XGA_DrawPoint8(xga.waitcmd.curx, xga.waitcmd.cury, destval);

					--i;
						if(i < 0) break;
						//--bitneed;
						//if(bitneed < 0) break;

					xga.waitcmd.curx++;
						XGA_CheckX();
				}
					//xga.waitcmd.cury++;
				
			
				if(xga.waitcmd.cury > xga.waitcmd.y2) xga.waitcmd.wait = false;
					break;
				default:
					LOG_MSG("XGA: DrawBlitWait: Unhandled mixmode: %d", mixmode);
					break;
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

	if(((val >> 5) & 0x01) != 0) incx = true;
	if(((val >> 7) & 0x01) != 0) incy = true;

	xdist = xga.MAPcount;

	smallc = (xga.forecolor & 0xff);
	memrec = 0;
	Bit32u srcaddr = (xga.cury * (Bit32u)XGA_SCREEN_WIDTH) + xga.curx;
	Bit32u destaddr = (xga.desty * (Bit32u)XGA_SCREEN_WIDTH) + xga.destx;

	srcptr = &vga.mem.linear[srcaddr];
	destptr = &vga.mem.linear[destaddr];

	/* Copy source to video ram */
	for(yat=0;yat<=xga.MIPcount ;yat++) {
		srcline = srcptr;
		destline = destptr;
		for(xat=0;xat<=xga.MAPcount;xat++) {
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
			srcptr+=XGA_SCREEN_WIDTH;
			destptr+=XGA_SCREEN_WIDTH;
		} else {
			srcptr-=XGA_SCREEN_WIDTH;
			destptr-=XGA_SCREEN_WIDTH;
		}
	}

	//LOG_MSG("XGA: Blit (%d, %d)-(%d, %d) to (%d, %d)-(%d, %d), incx %d, incy %d", xga.curx, xga.cury, xga.curx + xdist, xga.cury + xga.MIPcount, xga.destx, xga.desty, xga.destx + xdist, xga.desty + xga.MIPcount, incx, incy);

}

void XGA_DrawPattern(void) {
	Bit32u xat, yat, y1, y2, sx, sy, addx, addy;
	Bit32u xmass, xmod, xdist;
	Bit32u *memptr;
	Bit8u *smallptr;
	Bit8u smallc;
	Bit8u srcdata;
	Bit8u dstdata;

	Bit8u srcval;
	Bit8u destval;

	y1 = xga.desty;
	y2 = xga.desty + xga.MIPcount;
	xdist = xga.MAPcount;
	sx = xga.curx;
	sy = xga.cury;
	addx = 0;
	addy = 0;


	for(yat=y1;yat<=y2;yat++) {
		for(xat=0;xat<=xdist;xat++) {

			Bitu usex = xga.destx + xat;
			Bitu usey = yat;

			srcdata = XGA_GetPoint8(sx + (usex & 0x7), sy + (usey & 0x7));
			dstdata = XGA_GetPoint8(usex, usey);
			Bitu mixselect = (xga.pix_cntl >> 6) & 0x3;
			Bitu mixmode = 0x67; /* Source is bitmap data, mix mode is src */
			switch(mixselect) {
				case 0x00: /* Foreground mix is always used */
					mixmode = xga.foremix;
					break;
				case 0x02: /* CPU Data determines mix used */
					LOG_MSG("XGA: DrawPattern: Mixselect data from PIX_TRANS register");
					break;
				case 0x03: /* Video memory determines mix */
					if(srcdata == xga.forecolor) {
						mixmode = xga.foremix;
					} else {
						if(srcdata == xga.backcolor) {
							mixmode = xga.backmix;
						} else {
							/* Best guess otherwise */
							mixmode = 0x67; /* Source is bitmap data, mix mode is src */
						}
					}
					//LOG_MSG("XGA: Srcdata: %x, Forecolor %x, Backcolor %x, Foremix: %x Backmix: %x", srcdata, xga.forecolor, xga.backcolor, xga.foremix, xga.backmix);
					break;
				default:
					LOG_MSG("XGA: DrawPattern: Unknown mix select register");
					break;
			}
			switch((mixmode >> 5) & 0x03) {
				case 0x00: /* Src is background color */
					srcval = xga.backcolor;
					break;
				case 0x01: /* Src is foreground color */
					srcval = xga.forecolor;
					break;
				case 0x02: /* Src is pixel data from PIX_TRANS register */
					LOG_MSG("XGA: DrawPattern: Wants data from PIX_TRANS register");
					break;
				case 0x03: /* Src is bitmap data */
					srcval = srcdata;
					break;
				default:
					LOG_MSG("XGA: DrawPattern: Shouldn't be able to get here!");
					break;
			}

			destval = XGA_GetMixResult(mixmode, srcval, dstdata);

			//LOG_MSG("XGA: DrawPattern: Mixmode: %x Mixselect: %x", mixmode, mixselect);

			//*smallptr++ = destval;
			XGA_DrawPoint8(usex, usey, destval);

			
		}

	}

}

void XGA_DrawCmd(Bitu val, Bitu len) {
	Bit16u cmd;
	cmd = val >> 13;
	//LOG_MSG("XGA: Draw command %x", cmd);
	xga.curcommand = val;
	switch(cmd) {
		case 1: /* Draw line */
			if((val & 0x100) == 0) {
				if((val & 0x8) == 0) {
					//LOG_MSG("XGA: Drawing Bresenham line");
                    XGA_DrawLineBresenham(val);
				} else {
					//LOG_MSG("XGA: Drawing vector line");
					XGA_DrawLineVector(val);
				}
			} else {
				LOG_MSG("XGA: Wants line drawn from PIX_TRANS register!");
			}
			break;
		case 2: /* Rectangle fill */
			if((val & 0x100) == 0) {
				xga.waitcmd.wait = false;
				XGA_DrawRectangle(xga.curx, xga.cury, xga.curx + xga.MAPcount, xga.cury + xga.MIPcount);
			} else {
				xga.waitcmd.newline = true;
				xga.waitcmd.wait = true;
				xga.waitcmd.curx = xga.curx;
				xga.waitcmd.cury = xga.cury;
				xga.waitcmd.x1 = xga.curx;
				xga.waitcmd.y1 = xga.cury;
				xga.waitcmd.x2 = xga.curx + xga.MAPcount;
				xga.waitcmd.y2 = xga.cury + xga.MIPcount + 1;
				xga.waitcmd.cmd = 2;
				//LOG_MSG("XGA: Draw wait rect (%d, %d)-(%d, %d)", xga.waitcmd.x1, xga.waitcmd.y1, xga.waitcmd.x2, xga.waitcmd.y2);
			}
			break;
		case 6: /* BitBLT */
			XGA_BlitRect(val);
			break;
		case 7: /* Pattern fill */
			XGA_DrawPattern();
			//LOG_MSG("XGA: Pattern fill (%d, %d)-(%d, %d) to (%d, %d)-(%d, %d)", xga.curx, xga.cury, xga.curx + 8, xga.cury + 8, xga.destx, xga.desty, xga.destx + xga.MAPcount, xga.desty + xga.MIPcount);
			break;
		default:
			LOG_MSG("XGA: Unhandled draw command %x", cmd);
			break;

	}
}

void XGA_Write(Bitu port, Bitu val, Bitu len) {
	switch(port) {
		case 0x92e8:
			xga.ErrTerm = val;
			break;
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
		case 0xb6e8:
			xga.backmix = val;
			break;
		case 0xbae8:
			xga.foremix = val;
			break;
		case 0xbee8:
			XGA_Write_Multifunc(val, len);
			break;
		case 0x0e2e0:
			if(!xga.waitcmd.newline) {
				xga.waitcmd.curx = xga.waitcmd.x1;
				xga.waitcmd.cury++;
				xga.waitcmd.newline = true;
			}
			XGA_DrawWait(val, len);
			if(xga.waitcmd.cury > xga.waitcmd.y2) xga.waitcmd.wait = false;
			break;
		case 0xe2e8:
			xga.waitcmd.newline = false;
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

	// Dump the cursor to the log
	/*
	LOG_MSG("--- New cursor ---");
	for(t=0;t<64;t++) {
		char outstr[66];
		memset(outstr,0,66);
		for(r=0;r<64;r++) {
			switch(vga.s3.hgc.mc[t][r]) {
				case 0x00:
					outstr[r] = 'o';
					break;
				case 0x01:
					outstr[r] = 'O';
					break;
				case 0x02:
					outstr[r] = '.';
					break;
				case 0x03:
					outstr[r] = '¦';
					break;
				default:
					outstr[r] = ' ';
					break;

			}
		}
		LOG_MSG("%s", outstr);
	}
	*/

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

	IO_RegisterWriteHandler(0xe2e0,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xe2e0,&XGA_Read,IO_MB | IO_MW | IO_MD);

	IO_RegisterWriteHandler(0xe2ea,&XGA_Write,IO_MB | IO_MW | IO_MD);
	IO_RegisterReadHandler(0xe2ea,&XGA_Read,IO_MB | IO_MW | IO_MD);



}

