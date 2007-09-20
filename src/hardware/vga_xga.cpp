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

#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "vga.h"
#include <math.h>
#include <stdio.h>

#define XGA_SCREEN_WIDTH vga.draw.width

#define XGA_SHOW_COMMAND_TRACE 0

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
	Bit16u control1;
	Bit16u control2;
	Bit16u read_sel;

	struct XGA_WaitCmd {
		bool newline;
		bool wait;
		Bit16u cmd;
		Bit16u curx, cury;
		Bit16u x1, y1, x2, y2, sizex, sizey;
		Bit32u data; /* transient data passed by multiple calls */
		Bitu datasize;
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
		case 0xd:
			xga.control2 = dataval;
			break;
		case 0xe:
			xga.control1 = dataval;
			break;
		case 0xf:
			xga.read_sel = dataval;
			break;
		default:
			LOG_MSG("XGA: Unhandled multifunction command %x", regselect);
			break;
	}
}

Bitu XGA_Read_Multifunc()
{
	switch(xga.read_sel++) {
		case 0: return xga.MIPcount;
		case 1: return xga.scissors.y1;
		case 2: return xga.scissors.x1;
		case 3: return xga.scissors.y2;
		case 4: return xga.scissors.x2;
		case 5: return xga.pix_cntl;
		case 6: return xga.control1;
		case 7: return 0; // TODO
		case 8: return 0; // TODO
		case 9: return 0; // TODO
		case 10: return xga.control2;
		default: return 0;
	}
}


void XGA_DrawPoint(Bitu x, Bitu y, Bitu c) {

	if(!(xga.curcommand & 0x1)) return;
	if(!(xga.curcommand & 0x10)) return;

	if(x < xga.scissors.x1) return;
	if(x > xga.scissors.x2) return;
	if(y < xga.scissors.y1) return;
	if(y > xga.scissors.y2) return;

	Bit32u memaddr = (y * XGA_SCREEN_WIDTH) + x;
	/* Need to zero out all unused bits in modes that have any (15-bit or "32"-bit -- the last
	   one is actually 24-bit. Without this step there may be some graphics corruption (mainly,
	   during windows dragging. */
	switch(vga.mode) {
		case M_LIN8: vga.mem.linear[memaddr] = c; break;
		case M_LIN15: ((Bit16u*)(vga.mem.linear))[memaddr] = c&0x7fff; break;
		case M_LIN16: ((Bit16u*)(vga.mem.linear))[memaddr] = c; break;
		case M_LIN32: ((Bit32u*)(vga.mem.linear))[memaddr] = c&0x00ffffff; break;
	}

}

Bitu XGA_GetPoint(Bitu x, Bitu y) {
	Bit32u memaddr = (y * XGA_SCREEN_WIDTH) + x;
	switch(vga.mode) {
	case M_LIN8: return vga.mem.linear[memaddr];
	case M_LIN15: case M_LIN16: return ((Bit16u*)(vga.mem.linear))[memaddr];
	case M_LIN32: return ((Bit32u*)(vga.mem.linear))[memaddr];
	}
	return 0;
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
			destval = 0xffffffff;
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
	Bitu srcval;
	Bitu destval;
	Bitu dstdata;
	Bits i;

	Bits dx, sx, sy;

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
				dstdata = XGA_GetPoint(xat,yat);

				destval = XGA_GetMixResult(mixmode, srcval, dstdata);

                XGA_DrawPoint(xat,yat, destval);
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
	Bitu srcval;
	Bitu destval;
	Bitu dstdata;
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
						dstdata = XGA_GetPoint(xat,yat);
					} else {
						dstdata = XGA_GetPoint(yat,xat);
					}

					destval = XGA_GetMixResult(mixmode, srcval, dstdata);

					if(steep) {
						XGA_DrawPoint(xat,yat, destval);
					} else {
						XGA_DrawPoint(yat,xat, destval);
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

void XGA_DrawRectangle(Bitu val) {
	Bit32u xat, yat;
	Bitu srcval;
	Bitu destval;
	Bitu dstdata;

	Bits srcx, srcy, dx, dy;

	dx = -1;
	dy = -1;

	if(((val >> 5) & 0x01) != 0) dx = 1;
	if(((val >> 7) & 0x01) != 0) dy = 1;

	srcy = xga.cury;

	for(yat=0;yat<=xga.MIPcount;yat++) {
		srcx = xga.curx;
		for(xat=0;xat<=xga.MAPcount;xat++) {
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
					dstdata = XGA_GetPoint(srcx,srcy);

					destval = XGA_GetMixResult(mixmode, srcval, dstdata);

                    XGA_DrawPoint(srcx,srcy, destval);
					break;
				default: 
					LOG_MSG("XGA: DrawRect: Needs mixmode %x", mixmode);
					break;
	}
			srcx += dx;
		}
		srcy += dy;
	}
	xga.curx = srcx;
	xga.cury = srcy;

	//LOG_MSG("XGA: Draw rect (%d, %d)-(%d, %d), %d", x1, y1, x2, y2, xga.forecolor);
}

bool XGA_CheckX(void) {
	bool newline = false;
	if(!xga.waitcmd.newline) {
	if(xga.waitcmd.curx > xga.waitcmd.x2) {
		xga.waitcmd.curx = xga.waitcmd.x1;
		xga.waitcmd.cury++;
		newline = true;
		xga.waitcmd.newline = true;
		if(xga.waitcmd.cury > xga.waitcmd.y2) xga.waitcmd.wait = false;
	}
	} else {
        xga.waitcmd.newline = false;
	}
	return newline;
}


void XGA_DrawWait(Bitu val, Bitu len) {
	if(!xga.waitcmd.wait) return;

	//if(!(xga.curcommand & 0x2)) return;

	Bitu mixmode = (xga.pix_cntl >> 6) & 0x3;
	Bitu srcval;
	Bitu destval;
	Bitu dstdata;
	//Bitu tmpval;
	Bits bitneed;

	switch(xga.waitcmd.cmd) {
		case 2: /* Rectangle */
			switch(mixmode) {
				case 0x00: /* FOREMIX always used */
					mixmode = xga.foremix;
					Bitu t;
					for(t=0;t<len;t++) {
						switch((mixmode >> 5) & 0x03) {
							case 0x00: /* Src is background color */
								srcval = xga.backcolor;
								break;
							case 0x01: /* Src is foreground color */
								srcval = xga.forecolor;
								break;
							case 0x02: /* Src is pixel data from PIX_TRANS register */
								/* This register is 16 bit. In theory, it is possible to access it as 8-bit
								   or 32-bit but all calls from Win3 drivers are 16-bit. 8-bit color modes
								   would work regardless of the access size (although something else in this
								   function may break), other color modes may require more complex code to
								   collect transient data or break incoming data in chunks. */
								if(vga.mode == M_LIN8)
									srcval = (val >> (8 * t)) & 0xff;
								else if(vga.mode == M_LIN32) { /* May need transient data */
									if(xga.waitcmd.datasize == 0) {
										xga.waitcmd.data = val;
										xga.waitcmd.datasize = 2;
										return;
									} else {
										srcval = (val<<16)|xga.waitcmd.data;
										xga.waitcmd.data = 0;
										xga.waitcmd.datasize = 0;
										t = len; /* All data used */
									}
								}
								else {
									srcval = val;
									t = len; /* All data used */
								}
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



						dstdata = XGA_GetPoint(xga.waitcmd.curx, xga.waitcmd.cury);

						destval = XGA_GetMixResult(mixmode, srcval, dstdata);

						//LOG_MSG("XGA: DrawPattern: Mixmode: %x srcval: %x", mixmode, srcval);
						
						XGA_DrawPoint(xga.waitcmd.curx++, xga.waitcmd.cury, destval);

						XGA_CheckX();
						if(xga.waitcmd.newline) break;
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

						Bitu dstdata = XGA_GetPoint(xga.waitcmd.curx, xga.waitcmd.cury);

						destval = XGA_GetMixResult(mixmode, srcval, dstdata);

						XGA_DrawPoint(xga.waitcmd.curx, xga.waitcmd.cury, destval);

					--i;
						if(i < 0) break;
						//--bitneed;
						//if(bitneed < 0) break;

					xga.waitcmd.curx++;
						XGA_CheckX();
						if(xga.waitcmd.newline) break;
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
//	Bit32u xmass, xmod, xdist, memrec;
	//Bit8u *srcptr;
	//Bit8u *destptr;
	//Bit8u *destline;
	//Bit8u *srcline;
	Bitu srcdata;
	Bitu dstdata;

	Bitu srcval;
	Bitu destval;

	Bits srcx, srcy, tarx, tary, dx, dy;
	//bool incx = false;
	//bool incy = false; 

	dx = -1;
	dy = -1;

	//if(((val >> 5) & 0x01) != 0) incx = true;
	//if(((val >> 7) & 0x01) != 0) incy = true;

	if(((val >> 5) & 0x01) != 0) dx = 1;
	if(((val >> 7) & 0x01) != 0) dy = 1;

	//Bit32u srcaddr = (xga.cury * (Bit32u)XGA_SCREEN_WIDTH) + xga.curx;
	//Bit32u destaddr = (xga.desty * (Bit32u)XGA_SCREEN_WIDTH) + xga.destx;
	srcx = xga.curx;
	srcy = xga.cury;
	tarx = xga.destx;
	tary = xga.desty;

	//srcptr = &vga.mem.linear[srcaddr];
	//destptr = &vga.mem.linear[destaddr];

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
			//LOG_MSG("XGA: Srcdata: %x, Forecolor %x, Backcolor %x, Foremix: %x Backmix: %x", srcdata, xga.forecolor, xga.backcolor, xga.foremix, xga.backmix);
			break;
		default:
			LOG_MSG("XGA: DrawPattern: Unknown mix select register");
			break;
	}


	/* Copy source to video ram */
	for(yat=0;yat<=xga.MIPcount ;yat++) {
		srcx = xga.curx;
		tarx = xga.destx;

		for(xat=0;xat<=xga.MAPcount;xat++) {
			srcdata = XGA_GetPoint(srcx, srcy);
			dstdata = XGA_GetPoint(tarx, tary);

			if(mixselect == 0x3) {
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
			XGA_DrawPoint(tarx, tary, destval);

			srcx += dx;
			tarx += dx;
		}
		srcy += dy;
		tary += dy;
	}

}

void XGA_DrawPattern(Bitu val) {
	Bitu srcdata;
	Bitu dstdata;

	Bitu srcval;
	Bitu destval;

	Bits xat, yat, srcx, srcy, tarx, tary, dx, dy;

	dx = -1;
	dy = -1;

	if(((val >> 5) & 0x01) != 0) dx = 1;
	if(((val >> 7) & 0x01) != 0) dy = 1;

	srcx = xga.curx;
	srcy = xga.cury;

	tary = xga.desty;

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
			//LOG_MSG("XGA: Srcdata: %x, Forecolor %x, Backcolor %x, Foremix: %x Backmix: %x", srcdata, xga.forecolor, xga.backcolor, xga.foremix, xga.backmix);
			break;
		default:
			LOG_MSG("XGA: DrawPattern: Unknown mix select register");
			break;
	}

	for(yat=0;yat<=xga.MIPcount;yat++) {
		tarx = xga.destx;
		for(xat=0;xat<=xga.MAPcount;xat++) {

			srcdata = XGA_GetPoint(srcx + (tarx & 0x7), srcy + (tary & 0x7));
			dstdata = XGA_GetPoint(tarx, tary);
			

			if(mixselect == 0x3) {
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

			XGA_DrawPoint(tarx, tary, destval);
			
			tarx += dx;
		}
		tary += dy;

	}

}

void XGA_DrawCmd(Bitu val, Bitu len) {
	Bit16u cmd;
	cmd = val >> 13;
#if XGA_SHOW_COMMAND_TRACE == 1
	LOG_MSG("XGA: Draw command %x", cmd);
#endif
	xga.curcommand = val;
	switch(cmd) {
		case 1: /* Draw line */
			if((val & 0x100) == 0) {
				if((val & 0x8) == 0) {
#if XGA_SHOW_COMMAND_TRACE == 1
					LOG_MSG("XGA: Drawing Bresenham line");
#endif
                    XGA_DrawLineBresenham(val);
				} else {
#if XGA_SHOW_COMMAND_TRACE == 1
					LOG_MSG("XGA: Drawing vector line");
#endif
					XGA_DrawLineVector(val);
				}
			} else {
				LOG_MSG("XGA: Wants line drawn from PIX_TRANS register!");
			}
			break;
		case 2: /* Rectangle fill */
			if((val & 0x100) == 0) {
				xga.waitcmd.wait = false;
				XGA_DrawRectangle(val);
#if XGA_SHOW_COMMAND_TRACE == 1
				LOG_MSG("XGA: Draw immediate rect");
#endif
			} else {
				
				xga.waitcmd.newline = true;
				xga.waitcmd.wait = true;
				xga.waitcmd.curx = xga.curx;
				xga.waitcmd.cury = xga.cury;
				xga.waitcmd.x1 = xga.curx;
				xga.waitcmd.y1 = xga.cury;
				xga.waitcmd.x2 = xga.curx + xga.MAPcount;
				xga.waitcmd.y2 = xga.cury + xga.MIPcount + 1;
				xga.waitcmd.sizex = xga.MAPcount;
				xga.waitcmd.sizey = xga.MIPcount + 1;
				xga.waitcmd.cmd = 2;

				xga.waitcmd.data = 0;
				xga.waitcmd.datasize = 0;

#if XGA_SHOW_COMMAND_TRACE == 1
				LOG_MSG("XGA: Draw wait rect, width %d, heigth %d", xga.MAPcount, xga.MIPcount+1);
#endif
			}
			break;
		case 6: /* BitBLT */
			XGA_BlitRect(val);
#if XGA_SHOW_COMMAND_TRACE == 1
			LOG_MSG("XGA: Blit Rect");
#endif
			break;
		case 7: /* Pattern fill */
			XGA_DrawPattern(val);
#if XGA_SHOW_COMMAND_TRACE == 1
			LOG_MSG("XGA: Pattern fill");
#endif
			break;
		default:
			LOG_MSG("XGA: Unhandled draw command %x", cmd);
			break;

	}
}

void XGA_SetDualReg(Bit32u& reg, Bitu val)
{
	switch(vga.mode) {
	case M_LIN8: reg = val&0x000000ff; break;
	case M_LIN15: case M_LIN16: reg = val&0x0000ffff; break;
	case M_LIN32: {
			if(xga.control1 & 0x200)
				reg = val;
			else if(xga.control1 & 0x10)
				reg = (reg&0x0000ffff)|(val<<16);
			else
				reg = (reg&0xffff0000)|(val&0x0000ffff);
			xga.control1 ^= 0x10;
		}
		break;
	}
}

Bitu XGA_GetDualReg(Bit32u reg)
{
	switch(vga.mode) {
	case M_LIN8: return reg&0x000000ff;
	case M_LIN15: case M_LIN16: return reg&0x0000ffff;
	case M_LIN32: {
			if(xga.control1 & 0x200)
				return reg;
			xga.control1 ^= 0x10;
			if(xga.control1 & 0x10)
				return reg&0x0000ffff;
			else
				return reg>>16;
		}
	}
	return 0;
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
			XGA_SetDualReg(xga.backcolor, val);
			break;
		case 0xa6e8:
			XGA_SetDualReg(xga.forecolor, val);
			break;
		case 0xaae8:
			XGA_SetDualReg(xga.writemask, val);
			break;
		case 0xaee8:
			XGA_SetDualReg(xga.readmask, val);
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
	//LOG_MSG("XGA: Read from port %x, len %x", port, len);
	switch(port) {
		case 0x9ae8:
			return 0x0;
		case 0x9ae9:
			if(xga.waitcmd.wait) {
				return 0x4;
			} else {
				return 0x0;
			}
		case 0xbee8:
			return XGA_Read_Multifunc();
		case 0xa2e8:
			return XGA_GetDualReg(xga.backcolor);
			break;
		case 0xa6e8:
			return XGA_GetDualReg(xga.forecolor);
			break;
		case 0xaae8:
			return XGA_GetDualReg(xga.writemask);
			break;
		case 0xaee8:
			return XGA_GetDualReg(xga.readmask);
			break;
		default:
			LOG_MSG("XGA: Read from port %x, len %x", port, len);
			return 0x0;
	}
}

void VGA_SetupXGA(void) {
	if (!IS_VGA_ARCH) return;

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
