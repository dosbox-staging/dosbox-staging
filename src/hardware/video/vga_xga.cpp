// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "vga.h"

#include "cpu/callback.h"
#include "cpu/cpu.h"		// for 0x3da delay
#include "hardware/port.h"
#include "utils/bitops.h"

constexpr auto &XGA_SCREEN_WIDTH = vga.s3.xga_screen_width;
constexpr auto &XGA_COLOR_MODE = vga.s3.xga_color_mode;

#define XGA_SHOW_COMMAND_TRACE 0

// XGA-specific bit-depth constants that are used in bit-wise and switch operations
constexpr uint16_t XGA_8_BIT = 0x0005;
constexpr uint16_t XGA_15_BIT = 0x0006;
constexpr uint16_t XGA_16_BIT = 0x0007;
constexpr uint16_t XGA_32_BIT = 0x0008;

struct XGAStatus {
	struct scissorreg {
		uint16_t x1, y1, x2, y2;
	} scissors;

	uint32_t readmask;
	uint32_t writemask;

	uint32_t forecolor;
	uint32_t backcolor;

	uint32_t color_compare;

	Bitu curcommand;

	uint16_t foremix;
	uint16_t backmix;

	uint16_t curx, cury;
	uint16_t curx2, cury2;
	uint16_t destx, desty;
	uint16_t destx2, desty2;

	uint16_t ErrTerm;
	uint16_t MIPcount;
	uint16_t MAPcount;

	uint16_t pix_cntl;
	uint16_t control1;
	uint16_t control2;
	uint16_t read_sel;

	struct XGA_WaitCmd {
		bool newline;
		bool wait;
		uint16_t cmd;
		uint16_t curx, cury;
		uint16_t x1, y1, x2, y2, sizex, sizey;
		uint32_t data; /* transient data passed by multiple calls */
		Bitu datasize;
		Bitu buswidth;
	} waitcmd;

} xga;

static void XGA_Write_Multifunc(Bitu val)
{
	Bitu regselect = val >> 12;
	const auto dataval = static_cast<uint16_t>(val & 0xfff);
	switch(regselect) {
		case 0: // minor axis pixel count
			xga.MIPcount = dataval;
			break;
		case 1: // top scissors
			xga.scissors.y1 = dataval;
			break;
		case 2: // left
			xga.scissors.x1 = dataval;
			break;
		case 3: // bottom
			xga.scissors.y2 = dataval;
			break;
		case 4: // right
			xga.scissors.x2 = dataval;
			break;
		case 0xa: // data manip control
			xga.pix_cntl = dataval;
			break;
		case 0xd: // misc 2
			xga.control2 = dataval;
			break;
		case 0xe:
			xga.control1 = dataval;
			break;
		case 0xf:
			xga.read_sel = dataval;
			break;
		default:
		        LOG_MSG("XGA: Unhandled multifunction command "
		                "%#" PRIxPTR,
		                regselect);
		        break;
	        }
}

Bitu XGA_Read_Multifunc() {
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

	const auto memaddr = (y * XGA_SCREEN_WIDTH) + x;
	/* Need to zero out all unused bits in modes that have any (15-bit or "32"-bit -- the last
	   one is actually 24-bit. Without this step there may be some graphics corruption (mainly,
	   during windows dragging. */
	switch(XGA_COLOR_MODE) {
		case M_LIN8:
		        if (memaddr >= vga.vmemsize) {
			        break;
		        }
		        vga.mem.linear[memaddr] = c;
		        break;
	        case M_LIN15:
		        if (memaddr * 2 >= vga.vmemsize) {
			        break;
		        }
		        ((uint16_t*)(vga.mem.linear))[memaddr] = (uint16_t)(c & 0x7fff);
		        break;
	        case M_LIN16:
		        if (memaddr * 2 >= vga.vmemsize) {
			        break;
		        }
		        ((uint16_t*)(vga.mem.linear))[memaddr] = (uint16_t)(c & 0xffff);
		        break;
	        case M_LIN32:
		        if (memaddr * 4 >= vga.vmemsize) {
			        break;
		        }
		        ((uint32_t*)(vga.mem.linear))[memaddr] = c;
		        break;
	        default: break;
	}

}

static uint32_t get_point_mask()
{
	switch (XGA_COLOR_MODE) {
	case M_LIN8: return UINT8_MAX;
	case M_LIN15:
	case M_LIN16: return UINT16_MAX;
	case M_LIN32: return UINT32_MAX;
	default: break;
	}
	return 0;
}

Bitu XGA_GetPoint(Bitu x, Bitu y) {
	const auto memaddr = (y * XGA_SCREEN_WIDTH) + x;

	switch(XGA_COLOR_MODE) {
	case M_LIN8:
		if (memaddr >= vga.vmemsize) {
			break;
		}
		return vga.mem.linear[memaddr];
	case M_LIN15:
	case M_LIN16:
		if (memaddr * 2 >= vga.vmemsize) {
			break;
		}
		return ((uint16_t*)(vga.mem.linear))[memaddr];
	case M_LIN32:
		if (memaddr * 4 >= vga.vmemsize) {
			break;
		}
		return ((uint32_t*)(vga.mem.linear))[memaddr];
	default:
		break;
	}
	return 0;
}

static Bitu GetMixResult(uint32_t mixmode, Bitu srcval, Bitu dstdata)
{
	Bitu destval = 0;
	switch (mixmode & 0xf) {
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

static void XGA_DrawLineVector(const uint32_t val, const bool skip_last_pixel)
{
	// No work to do with a zero-length line
	if (!xga.MAPcount)
		return;

	int sx = 0;
	int sy = 0;
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


	assert(xga.MAPcount);
	const auto dx = xga.MAPcount - skip_last_pixel;
	auto xat = xga.curx;
	auto yat = xga.cury;
	auto srcval = decltype(xga.backcolor)(0);

	for (auto i = 0; i <= dx; ++i) {
		uint32_t mixmode = (xga.pix_cntl >> 6) & 0x3;
		Bitu dstdata;
		Bitu destval;
		switch (mixmode) {
			case 0x00: /* FOREMIX always used */
				mixmode = xga.foremix;
				switch ((mixmode >> 5) & 0x03) {
					case 0x00: /* Src is background color */
						srcval = xga.backcolor;
						break;
					case 0x01: /* Src is foreground color */
						srcval = xga.forecolor;
						break;
					case 0x02: /* Src is pixel data from PIX_TRANS register */
						//LOG_MSG("XGA: DrawRect: Wants data from PIX_TRANS register");
						srcval = 0;
						break;
					case 0x03: /* Src is bitmap data */
						LOG_MSG("XGA: DrawRect: Wants data from srcdata");
						srcval = 0;
						break;
					default:
						LOG_MSG("XGA: DrawRect: Shouldn't be able to get here!");
						srcval = 0;
						break;
				}
				dstdata = XGA_GetPoint(xat, yat);
				destval = GetMixResult(mixmode, srcval, dstdata);
				XGA_DrawPoint(xat, yat, destval);
				break;
			default: 
				LOG_MSG("XGA: DrawLine: Needs mixmode %x", mixmode);
				break;
		}
		xat += sx;
		yat += sy;
	}

#if 0
	LOG_MSG("XGA: DrawLineVector: (%d,%d) to (%d,%d), skip_last_pixel=%d",
	        xga.curx, xga.cury, xat, yat, skip_last_pixel);
#endif

	xga.curx = xat - 1;
	xga.cury = yat;
}

// NTS: The Windows 3.1 driver does not use this XGA command for horizontal and
// vertical lines
static void XGA_DrawLineBresenham(const uint32_t val, const bool skip_last_pixel)
{
	Bits xat, yat;
	Bitu srcval = 0;
	Bits i;
	Bits tmpswap;
	bool steep;

#define SWAP(a,b) tmpswap = a; a = b; b = tmpswap;

	Bits dx, sx, dy, sy, e, dmajor, dminor,destxtmp;

	// S3 Trio64 documentation: The "desty" register is both a destination Y
	// for BitBlt (hence the name) and "Line Parameter Axial Step Constant"
	// for line drawing, in case the name of the variable is confusing here.
	// The "desty" variable name exists as inherited from DOSBox SVN source
	// code.
	//
	// lpast = 2 * min(abs(dx),abs(dy))

	dminor = (Bits)((int16_t)xga.desty);
	if(xga.desty&0x2000) dminor |= ~0x1fff;
	dminor >>= 1;

	// S3 Trio64 documentation: The "destx" register is both a destination X
	// for BitBlt (hence the name) and "Line Parameter Diagonal Step
	// Constant" for line drawing, in case the name of the variable is
	// confusing here. The "destx" variable name exists as inherited from
	// DOSBox SVN source code.
	//
	// lpdst = 2 * min(abs(dx),abs(dy)) - max(abs(dx),abs(dy))

	destxtmp = (Bits)((int16_t)xga.destx);
	if (xga.destx & 0x2000)
		destxtmp |= ~0x1fff;

	dmajor = -(destxtmp - (dminor << 1)) >> 1;
	
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

	// S3 Trio64 documentation:
	// if x1 < x2: 2 * min(abs(dx),abs(dy)) - max(abs(dx),abs(dy))
	// if x1 >= x2: 2 * min(abs(dx),abs(dy)) - max(abs(dx),abs(dy)) - 1

	e = (Bits)((int16_t)xga.ErrTerm);
	if(xga.ErrTerm&0x2000) e |= ~0x1fff;
	xat = xga.curx;
	yat = xga.cury;

	if((val >> 6) & 0x1) {
		steep = false;
		SWAP(xat, yat);
		SWAP(sx, sy);
	} else {
		steep = true;
	}

#if 0
	LOG_MSG("XGA: Bresenham: ASC %ld, LPDSC %ld, sx %ld, sy %ld, err %ld,"
	" steep %ld, length %ld, dmajor %ld, dminor %ld, xstart %ld, ystart %ld, skip_last_pixel %u",
		(signed long)dx, (signed long)dy, (signed long)sx, (signed long)sy, (signed long)e,
		(signed long)steep, (signed long)xga.MAPcount, (signed long)dmajor, (signed long)dminor,
		(signed long)xat, (signed long)yat, skip_last_pixel);
#endif

	const auto run = xga.MAPcount - (xga.MAPcount && skip_last_pixel);

	for (i = 0; i <= run; ++i) {
		uint32_t mixmode = (xga.pix_cntl >> 6) & 0x3;
		Bitu dstdata;
		Bitu destval;
		switch (mixmode) {
		case 0x00: /* FOREMIX always used */
			mixmode = xga.foremix;
			switch ((mixmode >> 5) & 0x03) {
			case 0x00: /* Src is background color */
				srcval = xga.backcolor;
				break;
			case 0x01: /* Src is foreground color */
				srcval = xga.forecolor;
				break;
			case 0x02: /* Src is pixel data from PIX_TRANS register */
				// srcval = tmpval;
				LOG_MSG("XGA: DrawRect: Wants data from PIX_TRANS register");
				break;
			case 0x03: /* Src is bitmap data */
				LOG_MSG("XGA: DrawRect: Wants data from srcdata");
				// srcval = srcdata;
				break;
			default:
				LOG_MSG("XGA: DrawRect: Shouldn't be able to get here!");
				break;
			}

			if (steep)
				dstdata = XGA_GetPoint(xat, yat);
			else
				dstdata = XGA_GetPoint(yat, xat);

			destval = GetMixResult(mixmode, srcval, dstdata);

			if (steep) {
				XGA_DrawPoint(xat, yat, destval);
			} else {
				XGA_DrawPoint(yat, xat, destval);
			}

			break;
		default:
			LOG_MSG("XGA: DrawLine: Needs mixmode %x", mixmode);
			break;
		}
		while (e > 0) {
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
}

static void XGA_DrawRectangle(const uint32_t val, const bool skip_last_pixel)
{
	Bitu srcval = 0;
	Bits srcx, srcy, dx, dy;

	dx = -1;
	dy = -1;

	if(((val >> 5) & 0x01) != 0) dx = 1;
	if(((val >> 7) & 0x01) != 0) dy = 1;

	srcy = xga.cury;

	// Undocumented, but seen with Windows 3.1 drivers: Horizontal lines are
	// drawn with this XGA command and "skip last pixel" set, else they are
	// one pixel too wide (but don't underflow below zero).
	const auto xrun = xga.MAPcount - (xga.MAPcount && skip_last_pixel);

	for (auto yat = 0; yat <= xga.MIPcount; ++yat) {
		srcx = xga.curx;
		for (auto xat = 0; xat <= xrun; ++xat) {
			uint32_t mixmode = (xga.pix_cntl >> 6) & 0x3;
			Bitu dstdata;
			Bitu destval;
			switch (mixmode) {
				case 0x00: /* FOREMIX always used */
					mixmode = xga.foremix;
					switch ((mixmode >> 5) & 0x03) {
						case 0x00: /* Src is background color */
							srcval = xga.backcolor;
							break;
						case 0x01: /* Src is foreground color */
							srcval = xga.forecolor;
							break;
						case 0x02: /* Src is pixel data from PIX_TRANS register */
							LOG_MSG("XGA: DrawRect: Wants data from PIX_TRANS register");
							srcval = 0;
							break;
						case 0x03: /* Src is bitmap data */
							LOG_MSG("XGA: DrawRect: Wants data from srcdata");
							srcval = 0;
							break;
						default:
							LOG_MSG("XGA: DrawRect: Shouldn't be able to get here!");
							srcval = 0;
							break;
					}
					dstdata = XGA_GetPoint(srcx, srcy);
					destval = GetMixResult(mixmode, srcval, dstdata);
					XGA_DrawPoint(srcx, srcy, destval);
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

#if 0
	LOG_MSG("XGA: DrawRectangle: %d,%d,%d,%d skip_last_pixel=%d",
	        xga.curx, xga.cury, xga.MAPcount, xga.MIPcount, skip_last_pixel);
#endif
}

bool XGA_CheckX(void) {
	bool newline = false;
	if(!xga.waitcmd.newline) {
	
	if((xga.waitcmd.curx<2048) && xga.waitcmd.curx > (xga.waitcmd.x2)) {
		xga.waitcmd.curx = xga.waitcmd.x1;
		xga.waitcmd.cury++;
		xga.waitcmd.cury&=0x0fff;
		newline = true;
		xga.waitcmd.newline = true;
		if((xga.waitcmd.cury<2048)&&(xga.waitcmd.cury > xga.waitcmd.y2))
			xga.waitcmd.wait = false;
	} else if(xga.waitcmd.curx>=2048) {
		uint16_t realx = 4096-xga.waitcmd.curx;
		if(xga.waitcmd.x2>2047) { // x end is negative too
			uint16_t realxend=4096-xga.waitcmd.x2;
			if(realx==realxend) {
				xga.waitcmd.curx = xga.waitcmd.x1;
				xga.waitcmd.cury++;
				xga.waitcmd.cury&=0x0fff;
				newline = true;
				xga.waitcmd.newline = true;
				if((xga.waitcmd.cury<2048)&&(xga.waitcmd.cury > xga.waitcmd.y2))
					xga.waitcmd.wait = false;
			}
		} else { // else overlapping
			if(realx==xga.waitcmd.x2) {
				xga.waitcmd.curx = xga.waitcmd.x1;
				xga.waitcmd.cury++;
				xga.waitcmd.cury&=0x0fff;
				newline = true;
				xga.waitcmd.newline = true;
				if((xga.waitcmd.cury<2048)&&(xga.waitcmd.cury > xga.waitcmd.y2))
					xga.waitcmd.wait = false;
				}
			}
		}
	} else {
        xga.waitcmd.newline = false;
	}
	return newline;
}

static void DrawWaitSub(uint32_t mixmode, Bitu srcval)
{
	const Bitu dstdata = XGA_GetPoint(xga.waitcmd.curx, xga.waitcmd.cury);
	const Bitu destval = GetMixResult(mixmode, srcval, dstdata);
	//LOG_MSG("XGA: DrawPattern: Mixmode: %x srcval: %x", mixmode, srcval);

	XGA_DrawPoint(xga.waitcmd.curx, xga.waitcmd.cury, destval);
	xga.waitcmd.curx++;
	xga.waitcmd.curx&=0x0fff;
	XGA_CheckX();
}

void XGA_DrawWait(uint32_t val, io_width_t width)
{
	if (!xga.waitcmd.wait) {
		return;
	}

	uint32_t mixmode = (xga.pix_cntl >> 6) & 0x3;

	Bitu srcval;
	Bitu chunksize = 0;
	Bitu chunks    = 0;

	const uint8_t len = (width == io_width_t::dword  ? 4
	                     : width == io_width_t::word ? 2
	                                                 : 1);

	switch (xga.waitcmd.cmd) {
	case 2: /* Rectangle */
		switch (mixmode) {
		case 0x00: /* FOREMIX always used */
			mixmode = xga.foremix;

/*
			switch ((mixmode >> 5) & 0x03)
				case 0x00: // Src is background color
					srcval = xga.backcolor;
					break;
				case 0x01: // Src is foreground color
					srcval = xga.forecolor;
					break;
				case 0x02: // Src is pixel data from PIX_TRANS register
*/
			if (((mixmode >> 5) & 0x03) != 0x2) {
				// those cases don't seem to occur
				LOG_MSG("XGA: unsupported drawwait operation");
				break;
			}
			switch (xga.waitcmd.buswidth) {
			case XGA_8_BIT: // 8 bit
				DrawWaitSub(mixmode, val);
				break;
			case 0x20 | XGA_8_BIT: // 16 bit
				for (uint8_t i = 0; i < len; ++i) {
					DrawWaitSub(mixmode, (val >> (8 * i)) & 0xff);
					if (xga.waitcmd.newline)
						break;
				}
				break;
			case 0x40 | XGA_8_BIT: // 32 bit
				for (int i = 0; i < 4; ++i)
					DrawWaitSub(mixmode, (val >> (8 * i)) & 0xff);
				break;
			case (0x20 | XGA_32_BIT):
				if (len != 4) { // Win 3.11 864
					        // 'hack?'
					if (xga.waitcmd.datasize == 0) {
						// set it up to
						// wait for the
						// next word
						xga.waitcmd.data = val;
						xga.waitcmd.datasize = 2;
						return;
					} else {
						srcval = (val << 16) |
						         xga.waitcmd.data;
						xga.waitcmd.data = 0;
						xga.waitcmd.datasize = 0;
						DrawWaitSub(mixmode, srcval);
					}
					break;
				}
				[[fallthrough]];

			case 0x40 | XGA_32_BIT: // 32 bit
				DrawWaitSub(mixmode, val);
				break;
			case 0x20 | XGA_15_BIT: // 16 bit
			case 0x20 | XGA_16_BIT: // 16 bit
				DrawWaitSub(mixmode, val);
				break;
			case 0x40 | XGA_15_BIT: // 32 bit
			case 0x40 | XGA_16_BIT: // 32 bit
				DrawWaitSub(mixmode, val & 0xffff);
				if (!xga.waitcmd.newline)
					DrawWaitSub(mixmode, val >> 16);
				break;
			default:
				// Let's hope they never show up ;)
				LOG_MSG("XGA: "
				        "unsupported "
				        "bpp / "
				        "datawidth "
				        "combination "
				        "%#" PRIxPTR,
				        xga.waitcmd.buswidth);
				break;
			};
			break;

		case 0x02: // Data from PIX_TRANS selects the mix
			switch (xga.waitcmd.buswidth & 0x60) {
			case 0x0:
				chunksize = 8;
				chunks = 1;
				break;
			case 0x20: // 16 bit
				chunksize = 16;
				if (len == 4)
					chunks = 2;
				else
					chunks = 1;
				break;
			case 0x40: // 32 bit
				chunksize = 16;
				if (len == 4)
					chunks = 2;
				else
					chunks = 1;
				break;
			case 0x60: // undocumented guess (but
			           // works)
				chunksize = 8;
				chunks = 4;
				break;
			}

			for (Bitu k = 0; k < chunks; k++) { // chunks counter
				xga.waitcmd.newline = false;
				for (Bitu n = 0; n < chunksize; ++n) { // pixels
					// This formula can rule the world ;)
					const auto lshift = (((n & 0xF8) +
					                      (8 - (n & 0x7))) -
					                     1) + chunksize * k;
					const auto mask = static_cast<uint64_t>(1) << lshift;

					mixmode = (val & mask) ? xga.foremix
					                       : xga.backmix;

					switch ((mixmode >> 5) & 0x03) {
					case 0x00: // Src is background color
						srcval = xga.backcolor;
						break;
					case 0x01: // Src is foreground color
						srcval = xga.forecolor;
						break;
					default:
						LOG_MSG("XGA: DrawBlitWait: Unsupported src %x",
						        (mixmode >> 5) & 0x03);
						srcval = 0;
						break;
					}
					DrawWaitSub(mixmode, srcval);

					if ((xga.waitcmd.cury < 2048) &&
					    (xga.waitcmd.cury >= xga.waitcmd.y2)) {
						xga.waitcmd.wait = false;
						k = 1000; // no more chunks
						break;
					}
					// next chunk goes to next line
					if (xga.waitcmd.newline)
						break;
				} // pixels loop
			}         // chunks loop
			break;

		default:
			LOG_MSG("XGA: DrawBlitWait: Unhandled mixmode: %d", mixmode);
			break;
		} // mixmode switch
		break;
	default:
		LOG_MSG("XGA: Unhandled draw command %x", xga.waitcmd.cmd);
		break;
	}
}

void XGA_BlitRect(Bitu val) {
	uint32_t xat, yat;
	Bitu srcdata;
	Bitu dstdata;
	Bitu colorcmpdata;
	Bits srcx, srcy, tarx, tary, dx, dy;

	dx = -1;
	dy = -1;

	if(((val >> 5) & 0x01) != 0) dx = 1;
	if(((val >> 7) & 0x01) != 0) dy = 1;

	colorcmpdata = xga.color_compare & get_point_mask();

	Bitu mixselect = (xga.pix_cntl >> 6) & 0x3;
	uint32_t mixmode = 0x67; /* Source is bitmap data, mix mode is src */
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
			LOG_MSG("XGA: BlitRect: Unknown mix select register");
			break;
	}

	/* Copy source to video ram */
	srcy = xga.cury;
	tary = xga.desty;
	for (yat = 0; yat <= xga.MIPcount; yat++) {
		srcx = xga.curx;
		tarx = xga.destx;

		for(xat=0;xat<=xga.MAPcount;xat++) {
			srcdata = XGA_GetPoint(srcx, srcy);
			dstdata = XGA_GetPoint(tarx, tary);

			if (mixselect == 0x3) {
				if (srcdata == xga.forecolor) {
					mixmode = xga.foremix;
				} else {
					if (srcdata == xga.backcolor) {
						mixmode = xga.backmix;
					} else {
						/* Best guess otherwise */
						mixmode = 0x67; /* Source is bitmap data, mix mode is src */
					}
				}
			}

			Bitu srcval = 0;
			switch ((mixmode >> 5) & 0x03) {
				case 0x00: /* Src is background color */
					srcval = xga.backcolor;
					break;
				case 0x01: /* Src is foreground color */
					srcval = xga.forecolor;
					break;
				case 0x02: /* Src is pixel data from PIX_TRANS register */
					LOG_MSG("XGA: DrawPattern: Wants data from PIX_TRANS register");
					srcval = 0;
					break;
				case 0x03: /* Src is bitmap data */
					srcval = srcdata;
					break;
				default:
					LOG_MSG("XGA: DrawPattern: Shouldn't be able to get here!");
					srcval = 0;
					break;
			}
			// For more information, see the "S3 Vision864 Graphics
			// Accelerator" datasheet
			//
			// [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20Vision864%20Graphics%20Accelerator%20(1994-10).pdf]
			//
			// Page 203 for "Multifunction Control Miscellaneous
			// Register (MULT_MISC)" which this code holds as
			// xga.control1, and Page 198 for "Color Compare
			// Register (COLOR_CMP)" which this code holds as
			// xga.color_compare.

			// Always update if we're not comparing (COLOR_CMP is
			// bit 8). Otherwise, either update if the SRC_NE bit is
			// set with a matching colour or vice-versa (SRC_NE not
			// set with non-matching colour).

			using namespace bit::literals;

			if (bit::cleared(xga.control1, b8) ||
			    bit::is(xga.control1, b7) == (srcval == colorcmpdata)) {

				const auto destval = GetMixResult(mixmode, srcval, dstdata);

				// LOG_MSG("XGA: DrawPattern: Mixmode: %x Mixselect: %x", mixmode, mixselect);
				XGA_DrawPoint((Bitu)tarx, (Bitu)tary, destval);
			}

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


	Bits xat, yat, srcx, srcy, tarx, tary, dx, dy;

	dx = -1;
	dy = -1;

	if(((val >> 5) & 0x01) != 0) dx = 1;
	if(((val >> 7) & 0x01) != 0) dy = 1;

	srcx = xga.curx;
	srcy = xga.cury;

	tary = xga.desty;

	Bitu mixselect = (xga.pix_cntl >> 6) & 0x3;
	uint32_t mixmode = 0x67; /* Source is bitmap data, mix mode is src */
	switch (mixselect) {
		case 0x00: /* Foreground mix is always used */
			mixmode = xga.foremix;
			break;
		case 0x02: /* CPU Data determines mix used */
			LOG_MSG("XGA: DrawPattern: Mixselect data from PIX_TRANS register");
			break;
		case 0x03: /* Video memory determines mix */
			//LOG_MSG("XGA: Pixctl: %x, Srcdata: %x, Forecolor %x, Backcolor %x, Foremix: %x Backmix: %x",xga.pix_cntl, srcdata, xga.forecolor, xga.backcolor, xga.foremix, xga.backmix);
			break;
		default:
			LOG_MSG("XGA: DrawPattern: Unknown mix select register");
			break;
	}

	for(yat=0;yat<=xga.MIPcount;yat++) {
		tarx = xga.destx;
		for(xat=0;xat<=xga.MAPcount;xat++) {

			srcdata = XGA_GetPoint(srcx + (tarx & 0x7), srcy + (tary & 0x7));
			//LOG_MSG("patternpoint (%3d/%3d)v%x",srcx + (tarx & 0x7), srcy + (tary & 0x7),srcdata);
			dstdata = XGA_GetPoint(tarx, tary);

			if (mixselect == 0x3) {

				// S3 Trio32/Trio64 Integrated Graphics
				// Accelerators, section 13.2 Bitmap Access
				// Through The Graphics Engine.
				// [https://jon.nerdgrounds.com/jmcs/docs/browse/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20Trio32%e2%88%95Trio64%20Integrated%20Graphics%20Accelerators%20%281995%2d03%29%2epdf]

				// "If bits 7-6 are set to 11b, the current
				// display bit map is selected as the mask bit
				// source. The Read Mask" "register (AAE8H) is
				// set up to indicate the active planes. When
				// all bits of the read-enabled planes for a"
				// "pixel are a 1, the mask bit 'ONE' is
				// generated. If anyone of the read-enabled
				// planes is a 0, then a mask" "bit 'ZERO' is
				// generated. If the mask bit is 'ONE', the
				// Foreground Mix register is used. If the mask
				// bit is" "'ZERO', the Background Mix register
				// is used." Notice that when an application in
				// Windows 3.1 draws a black rectangle, I see
				// foreground=0 background=ff and in this loop,
				// srcdata=ff and readmask=ff. While the
				// original DOSBox SVN "guess" code here would
				// misattribute that to the background color
				// (and erroneously draw a white rectangle),
				// what should actually happen is that we use
				// the foreground color because
				// (srcdata&readmask)==readmask (all bits 1).

				// This fixes visual bugs when running
				// Windows 3.1 and Microsoft Creative Writer,
				// and navigating to the basement and clicking
				// around in the dark to reveal funny random
				// things, leaves white rectangles on the screen
				// where the image was when you released the
				// mouse. Creative Writer clears the image by
				// drawing a BLACK rectangle, while the DOSBox
				// SVN "guess" mistakenly chose the background
				// color and therefore a WHITE rectangle.

				if ((srcdata & xga.readmask) == xga.readmask) {
					mixmode = xga.foremix;
				} else {
					mixmode = xga.backmix;
				}
			}

			Bitu srcval = 0;
			switch ((mixmode >> 5) & 0x03) {
				case 0x00: /* Src is background color */
					srcval = xga.backcolor;
					break;
				case 0x01: /* Src is foreground color */
					srcval = xga.forecolor;
					break;
				case 0x02: /* Src is pixel data from PIX_TRANS register */
					LOG_MSG("XGA: DrawPattern: Wants data from PIX_TRANS register");
					srcval = 0;
					break;
				case 0x03: /* Src is bitmap data */
					srcval = srcdata;
					break;
				default:
					LOG_MSG("XGA: DrawPattern: Shouldn't be able to get here!");
					srcval = 0;
					break;
			}

			const Bitu destval = GetMixResult(mixmode, srcval, dstdata);
			XGA_DrawPoint(tarx, tary, destval);
			
			tarx += dx;
		}
		tary += dy;
	}
}

static void XGA_DrawCmd(const uint32_t val)
{
	uint16_t cmd;
	cmd = val >> 13;
#if XGA_SHOW_COMMAND_TRACE == 1
	//LOG_MSG("XGA: Draw command %x", cmd);
#endif
	xga.curcommand = val;

	// Do we skip drawing the last pixel? (bit 2), Trio64 documentation This
	// is needed to correctly draw polylines in Windows
	const auto skip_last_pixel = bit::is(val, bit::literals::b2);

	switch(cmd) {
		case 1: /* Draw line */
			if((val & 0x100) == 0) {
				if((val & 0x8) == 0) {
#if XGA_SHOW_COMMAND_TRACE == 1
					LOG_MSG("XGA: Drawing Bresenham line");
#endif
					XGA_DrawLineBresenham(val, skip_last_pixel);
				} else {
#if XGA_SHOW_COMMAND_TRACE == 1
					LOG_MSG("XGA: Drawing vector line");
#endif
					XGA_DrawLineVector(val, skip_last_pixel);
				}
			} else {
				LOG_MSG("XGA: Wants line drawn from PIX_TRANS register!");
			}
			break;
		case 2: /* Rectangle fill */
			if((val & 0x100) == 0) {
				xga.waitcmd.wait = false;
#if XGA_SHOW_COMMAND_TRACE == 1
				LOG_MSG("XGA: Draw immediate rect: xy(%3d/%3d), len(%3d/%3d)",
					xga.curx,xga.cury,xga.MAPcount,xga.MIPcount);
#endif
				XGA_DrawRectangle(val, skip_last_pixel);

			} else {
			        uint16_t xga_bit_depth = 0;
			        switch (vga.mode) {
			        case M_LIN8: xga_bit_depth = XGA_8_BIT; break;
			        case M_LIN15: xga_bit_depth = XGA_15_BIT; break;
			        case M_LIN16: xga_bit_depth = XGA_16_BIT; break;
			        case M_LIN32: xga_bit_depth = XGA_32_BIT; break;
					default:
						LOG_MSG("XGA: Draw rectangle: No XGA bit-depth matching mode %x", vga.mode);
						break;
			        };
			        assert(xga_bit_depth); // Unhandled bit-depth

			        xga.waitcmd.newline = true;
			        xga.waitcmd.wait = true;
			        xga.waitcmd.curx = xga.curx;
			        xga.waitcmd.cury = xga.cury;
			        xga.waitcmd.x1 = xga.curx;
			        xga.waitcmd.y1 = xga.cury;
			        xga.waitcmd.x2 = (uint16_t)((xga.curx + xga.MAPcount)&0x0fff);
				xga.waitcmd.y2 = (uint16_t)((xga.cury + xga.MIPcount + 1)&0x0fff);
				xga.waitcmd.sizex = xga.MAPcount;
				xga.waitcmd.sizey = xga.MIPcount + 1;
				xga.waitcmd.cmd = 2;
			        xga.waitcmd.buswidth = xga_bit_depth | ((val & 0x600) >> 4);
			        xga.waitcmd.data = 0;
			        xga.waitcmd.datasize = 0;

#if XGA_SHOW_COMMAND_TRACE == 1
				LOG_MSG("XGA: Draw wait rect, w/h(%3d/%3d), x/y1(%3d/%3d), x/y2(%3d/%3d), %4x",
					xga.MAPcount+1, xga.MIPcount+1,xga.curx,xga.cury,
					(xga.curx + xga.MAPcount)&0x0fff,
					(xga.cury + xga.MIPcount + 1)&0x0fff,val&0xffff);
#endif
			
			}
			break;
	        case 3: // Polygon fill
#if XGA_SHOW_COMMAND_TRACE == 1
		        LOG_MSG("XGA: Polygon fill (Trio64)");
#endif
				// From the datasheet
				// [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20Trio32%e2%88%95Trio64%20Integrated%20Graphics%20Accelerators%20%281995%2d03%29%2epdf]
				// Section 13.3.3.12 Polygon Fill Solid (Trio64 only)
				// The idea is that there are two current/dest X/Y pairs
				// and this command is used to draw the polygon top to
				// bottom as a series of trapezoids, sending new x/y
				// coordinates for each left or right edge as the
				// polygon continues. The acceleration function is
				// described as rendering to the minimum of the two Y
				// coordinates, and stopping. One side or the other is
				// updated, and the command starts the new edge and
				// continues the other edge.

				// The card requires that the first and last segments
				// have equal Y values, though not X values in order to
				// allow polygons with flat top and/or bottom.

				// That would imply that there's some persistent error
				// term here, and it would also imply that the card
				// updates current Y position to the minimum of either
				// side so the new coordinates continue properly.

				// NTS: The Windows 3.1 Trio64 driver likes to send this
				// command every single time it updates any coordinate,
				// contrary to the Trio64 datasheet that suggests setting
				// cur/dest X/Y and cur2/dest2 X/Y THEN sending this
				// command, then setting either dest X/Y and sending the
				// command until the polygon has been rasterized. We can
				// weed those out here by ignoring any command where the
				// cur/dest Y coordinates would result in no movement.

				// The Windows 3.1 driver also seems to use cur/dest X/Y
				// for the RIGHT side, and cur2/dest2 X/Y for the LEFT
				// side, which is completely opposite from the example
				// given in the datasheet. This also implies that
				// whatever order the vertices end up, they draw a span
				// when rasterizing, and the sides can cross one another
				// if necessary.

				// NTS: You can test this code by bringing up
				// Paintbrush, and drawing with the brush tool. Despite
				// drawing a rectangle, the S3 Trio64 driver uses the
				// Polygon fill command to draw it. More testing is
				// possible in Microsoft Word 2.0 using the
				// shapes/graphics editor, adding solid rectangles or
				// rounded rectangles (but not circles).
				/*
				//  Vertex at (*)
				//
				//                        *             *     *
				//                        +             +-----+
				//                       / \           /       \
				//                      /   \         /         \
				//                     /_____\ *     /___________\ *
				//                    /      /      /            |
				//                 * /______/    * /_____________|
				//                   \     /       \             |
				//                    \   /         \            |
				//                     \ /           \           |
				//                      +             \__________|
				//                      *             *          *
				//
				//  Windows 3.1 driver behavior suggests this is also
				//  possible?
				//
				//                    *
				//                   / \
				//                  /   \
				//                 /     \
				//              * /_______\
				//                \________\ *
				//                 \       /
				//                  \     /
				//                   \   /
				//                    \ /
				//                     X      <- crossover point
				//                    / \
				//                   /   \
				//                * /_____\
				//                  \      \
				//                   \______\
				//                   *       *
				*/

		        if (xga.cury < xga.desty && xga.cury2 < xga.desty2) {
#if XGA_SHOW_COMMAND_TRACE == 1
			        LOG_MSG("XGA: Polygon fill: leftside=(%d,%d)-(%d,%d) rightside=(%d,%d)-(%d,%d)",
			                xga.curx,
			                xga.cury,
			                xga.destx,
			                xga.desty,
			                xga.curx2,
			                xga.cury2,
			                xga.destx2,
			                xga.desty2);
#endif

			        // Not quite accurate, good enough for now.
			        xga.curx  = xga.destx;
			        xga.cury  = xga.desty;
			        xga.curx2 = xga.destx2;
			        xga.cury2 = xga.desty2;
		        } else {
#if XGA_SHOW_COMMAND_TRACE == 1
			        LOG_MSG("XGA: Polygon fill (nothing done)");
#endif
			        // Windows 3.1 Trio64 driver behavior suggests
			        // that if Y doesn't move, the X coordinate may
			        // change if cur Y == dest Y, else the result
			        // when actual rendering doesn't make sense.
			        if (xga.cury == xga.desty) {
				        xga.curx = xga.destx;
			        }
			        if (xga.cury2 == xga.desty2) {
				        xga.curx2 = xga.destx2;
			        }
		        }
		        break;
	        case 6: // BitBLT
#if XGA_SHOW_COMMAND_TRACE == 1
		        LOG_MSG("XGA: Blit Rect");
#endif
		        XGA_BlitRect(val);
		        break;
	        case 7: // Pattern fill
#if XGA_SHOW_COMMAND_TRACE == 1
		        LOG_MSG("XGA: Pattern fill: src(%3d/%3d), dest(%3d/%3d), fill(%3d/%3d)",
		                xga.curx,
		                xga.cury,
		                xga.destx,
		                xga.desty,
		                xga.MAPcount,
		                xga.MIPcount);
#endif
			XGA_DrawPattern(val);
			break;
		default:
			LOG_MSG("XGA: Unhandled draw command %x", cmd);
			break;
	}
}

void XGA_SetDualReg(uint32_t &reg, uint32_t val)
{
	switch (XGA_COLOR_MODE) {
	case M_LIN8: reg = (uint8_t)(val & 0xff); break;
	case M_LIN15:
	case M_LIN16:
		reg = (uint16_t)(val&0xffff); break;
	case M_LIN32:
		if (xga.control1 & 0x200)
			reg = val;
		else if (xga.control1 & 0x10)
			reg = (reg&0x0000ffff)|(val<<16);
		else
			reg = (reg&0xffff0000)|(val&0x0000ffff);
		xga.control1 ^= 0x10;
		break;
	default:
		break;
	}
}

uint32_t XGA_GetDualReg(uint32_t reg) {
	switch(XGA_COLOR_MODE) {
	case M_LIN8:
		return (uint8_t)(reg&0xff);
	case M_LIN15: case M_LIN16:
		return (uint16_t)(reg&0xffff);
	case M_LIN32:
		if (xga.control1 & 0x200) return reg;
		xga.control1 ^= 0x10;
		if (xga.control1 & 0x10) return reg&0x0000ffff;
		else return reg>>16;
	default:
		break;
	}
	return 0;
}

extern uint8_t vga_read_p3da(io_port_t port, io_width_t width);

extern void vga_write_p3d4(io_port_t port, io_val_t value, io_width_t width);
extern uint8_t vga_read_p3d4(io_port_t port, io_width_t width);

extern void vga_write_p3d5(io_port_t port, io_val_t value, io_width_t width);
extern uint8_t vga_read_p3d5(io_port_t port, io_width_t width);

// Writes can range from 8bit to 32bit
void XGA_Write(io_port_t port, io_val_t val, io_width_t width)
{
	//	LOG_MSG("XGA: Write to port %x, val %8x, len %x", port,val, len);

	switch (port) {
	case 0x8100: // drawing control: row (low word), column (high word)
		// "CUR_X" and "CUR_Y" (see PORT 82E8h,PORT 86E8h)
		xga.cury = val & 0x0fff;
		if (width == io_width_t::dword)
			xga.curx = (val >> 16) & 0x0fff;
		break;
	case 0x8102: xga.curx = val & 0x0fff; break;
	case 0x8104:
		// Drawing control: row (low word), column (high word)
		// "CUR_X2" and "CUR_Y2" (see PORT 82EAh,PORT 86EAh)
		xga.cury2 = static_cast<uint16_t>(val & 0x0fff);
		if (width == io_width_t::dword) {
			xga.curx2 = static_cast<uint16_t>((val >> 16) & 0x0fff);
		}
		break;
	case 0x8106:
		xga.curx2 = static_cast<uint16_t>(val & 0x0fff);
		break;
	case 0x8108: // DWORD drawing control: destination Y and axial step
		// constant (low word), destination X and axial step
		// constant (high word) (see PORT 8AE8h,PORT 8EE8h)
		xga.desty = val & 0x3fff;
		if (width == io_width_t::dword)
			xga.destx = (val >> 16) & 0x3fff;
		break;
	case 0x810a: xga.destx = val & 0x3fff; break;
	case 0x810c:
		// DWORD drawing control: destination Y and axial step
		// constant (low word), destination X and axial step
		// constant (high word) (see PORT 8AEAh,PORT 8EEAh)
		xga.desty2 = static_cast<uint16_t>(val & 0x3fff);
		if (width == io_width_t::dword) {
			xga.destx2 = static_cast<uint16_t>((val >> 16) & 0x3fff);
		}
		break;
	case 0x810e:
		xga.destx2 = static_cast<uint16_t>(val & 0x3fff);
		break;
	case 0x8110: // WORD error term (see PORT 92E8h)
		xga.ErrTerm = val & 0x3fff;
		break;

	case 0x8120: // packed MMIO: DWORD background color (see PORT A2E8h)
		xga.backcolor = val;
		break;
	case 0x8124: // packed MMIO: DWORD foreground color (see PORT A6E8h)
		xga.forecolor = val;
		break;
	case 0x8128: // DWORD	write mask (see PORT AAE8h)
		xga.writemask = val;
		break;
	case 0x812C: // DWORD	read mask (see PORT AEE8h)
		xga.readmask = val;
		break;
	case 0x8134: // packed MMIO: DWORD	background mix (low word) and
		// foreground mix (high word)	(see PORT B6E8h,PORT BAE8h)
		xga.backmix = val & 0xffff;
		if (width == io_width_t::dword)
			xga.foremix = (val >> 16);
		break;
	case 0x8136: xga.foremix = val; break;
	case 0x8138: // DWORD top scissors (low word) and left scissors (high
		// word) (see PORT BEE8h,#P1047)
		xga.scissors.y1 = val & 0x0fff;
		if (width == io_width_t::dword)
			xga.scissors.x1 = (val >> 16) & 0x0fff;
		break;
	case 0x813a: xga.scissors.x1 = val & 0x0fff; break;
	case 0x813C: // DWORD bottom scissors (low word) and right scissors
		// (high word) (see PORT BEE8h,#P1047)
		xga.scissors.y2 = val & 0x0fff;
		if (width == io_width_t::dword)
			xga.scissors.x2 = (val >> 16) & 0x0fff;
		break;
	case 0x813e: xga.scissors.x2 = val & 0x0fff; break;

	case 0x8140: // DWORD data manipulation control (low word) and
		// miscellaneous 2 (high word) (see PORT BEE8h,#P1047)
		xga.pix_cntl = val & 0xffff;
		if (width == io_width_t::dword)
			xga.control2 = (val >> 16) & 0x0fff;
		break;
	case 0x8144: // DWORD miscellaneous (low word) and read register select
		// (high word)(see PORT BEE8h,#P1047)
		xga.control1 = val & 0xffff;
		if (width == io_width_t::dword)
			xga.read_sel = (val >> 16) & 0x7;
		break;
	case 0x8148: // DWORD minor axis pixel count (low word) and major axis
		// pixel count (high word) (see PORT BEE8h,#P1047,PORT 96E8h)
		xga.MIPcount = val & 0x0fff;
		if (width == io_width_t::dword)
			xga.MAPcount = (val >> 16) & 0x0fff;
		break;
	case 0x814a: xga.MAPcount = val & 0x0fff; break;
	case 0x92e8: xga.ErrTerm = val & 0x3fff; break;
	case 0x96e8: xga.MAPcount = val & 0x0fff; break;
	case 0x9ae8:
	case 0x8118: // Trio64V+ packed MMIO
		XGA_DrawCmd(val);
		break;
	case 0xa2e8: XGA_SetDualReg(xga.backcolor, val); break;
	case 0xa6e8: XGA_SetDualReg(xga.forecolor, val); break;
	case 0xaae8: XGA_SetDualReg(xga.writemask, val); break;
	case 0xaee8: XGA_SetDualReg(xga.readmask, val); break;
	case 0x82e8: xga.cury = val & 0x0fff; break;
	case 0x86e8: xga.curx = val & 0x0fff; break;
	case 0x8ae8: xga.desty = val & 0x3fff; break;
	case 0x8ee8: xga.destx = val & 0x3fff; break;
	case 0xb2e8: XGA_SetDualReg(xga.color_compare, val); break;
	case 0xb6e8: xga.backmix = val; break;
	case 0xbae8: xga.foremix = val; break;
	case 0xbee8: XGA_Write_Multifunc(val); break;
	case 0xe2e8:
		xga.waitcmd.newline = false;
		XGA_DrawWait(val, width);
		break;
	case 0x83d4:
		if (width == io_width_t::byte)
			vga_write_p3d4(0, val, io_width_t::byte);
		else if (width == io_width_t::word) {
			LOG_WARNING("XGA: 16-bit write to vga_write_p3d4, vga_write_p3d5");
			vga_write_p3d4(0, val & 0xff, io_width_t::byte);
			vga_write_p3d5(0, val >> 8, io_width_t::byte);
		} else
			E_Exit("unimplemented XGA MMIO");
		break;
	case 0x83d5:
		if (width == io_width_t::byte)
			vga_write_p3d5(0, val, io_width_t::byte);
		else
			E_Exit("unimplemented XGA MMIO");
		break;
	default:
		if (port <= 0x4000) {
			// LOG_MSG("XGA: Wrote to port %4x with %08x, len %x", port, val, len);
			xga.waitcmd.newline = false;
			XGA_DrawWait(val, width);

		} else
			LOG_MSG("XGA: Wrote to port %x with %x, IO width=%d", port, val,
			        static_cast<int>(width));
		break;
	}
}

uint32_t XGA_Read(io_port_t port, io_width_t width)
{
	switch (port) {
	case 0x8118:
	case 0x9ae8:
		return 0x400; // nothing busy
		break;
	case 0x81ec: // S3 video data processor
		return 0x00007000;
		break;
	case 0x83da: {
		Bits delaycyc = CPU_CycleMax / 5000;
		if (CPU_Cycles < 3 * delaycyc) {
			delaycyc = 0;
		}
		CPU_Cycles -= delaycyc;
		CPU_IODelayRemoved += delaycyc;
		return vga_read_p3da(0, io_width_t::byte);
	} break;
	case 0x83d4:
		if (width == io_width_t::byte)
			return vga_read_p3d4(0, io_width_t::byte);
		else
			E_Exit("unimplemented XGA MMIO");
		break;
	case 0x83d5:
		if (width == io_width_t::byte)
			return vga_read_p3d5(0, io_width_t::byte);
		else
			E_Exit("unimplemented XGA MMIO");
		break;
	case 0x9ae9:
		if (xga.waitcmd.wait)
			return 0x4;
		else
			return 0x0;
	case 0xbee8: return XGA_Read_Multifunc();
	case 0xb2e8: return XGA_GetDualReg(xga.color_compare); break;
	case 0xa2e8: return XGA_GetDualReg(xga.backcolor); break;
	case 0xa6e8: return XGA_GetDualReg(xga.forecolor); break;
	case 0xaae8: return XGA_GetDualReg(xga.writemask); break;
	case 0xaee8: return XGA_GetDualReg(xga.readmask); break;
	default:
		// LOG_MSG("XGA: Read from port %x, len %x", port, static_cast<int>(width));
		break;
	}
	return 0xffffffff;
}

void VGA_SetupXGA()
{
	if (!is_machine_vga_or_better()) {
		return;
	}

	memset(&xga, 0, sizeof(XGAStatus));

	xga.scissors.y1 = 0;
	xga.scissors.x1 = 0;
	xga.scissors.y2 = 0xfff;
	xga.scissors.x2 = 0xfff;

	IO_RegisterWriteHandler(0x42e8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x42e8, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0x46e8, &XGA_Write, io_width_t::dword);
	IO_RegisterWriteHandler(0x4ae8, &XGA_Write, io_width_t::dword);

	IO_RegisterWriteHandler(0x82e8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x82e8, &XGA_Read, io_width_t::dword);
	IO_RegisterWriteHandler(0x82e9, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x82e9, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0x86e8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x86e8, &XGA_Read, io_width_t::dword);
	IO_RegisterWriteHandler(0x86e9, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x86e9, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0x8ae8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x8ae8, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0x8ee8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x8ee8, &XGA_Read, io_width_t::dword);
	IO_RegisterWriteHandler(0x8ee9, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x8ee9, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0x92e8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x92e8, &XGA_Read, io_width_t::dword);
	IO_RegisterWriteHandler(0x92e9, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x92e9, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0x96e8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x96e8, &XGA_Read, io_width_t::dword);
	IO_RegisterWriteHandler(0x96e9, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x96e9, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0x9ae8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x9ae8, &XGA_Read, io_width_t::dword);
	IO_RegisterWriteHandler(0x9ae9, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x9ae9, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0x9ee8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x9ee8, &XGA_Read, io_width_t::dword);
	IO_RegisterWriteHandler(0x9ee9, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0x9ee9, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0xa2e8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xa2e8, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0xa6e8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xa6e8, &XGA_Read, io_width_t::dword);
	IO_RegisterWriteHandler(0xa6e9, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xa6e9, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0xaae8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xaae8, &XGA_Read, io_width_t::dword);
	IO_RegisterWriteHandler(0xaae9, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xaae9, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0xaee8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xaee8, &XGA_Read, io_width_t::dword);
	IO_RegisterWriteHandler(0xaee9, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xaee9, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0xb2e8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xb2e8, &XGA_Read, io_width_t::dword);
	IO_RegisterWriteHandler(0xb2e9, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xb2e9, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0xb6e8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xb6e8, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0xbee8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xbee8, &XGA_Read, io_width_t::dword);
	IO_RegisterWriteHandler(0xbee9, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xbee9, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0xbae8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xbae8, &XGA_Read, io_width_t::dword);
	IO_RegisterWriteHandler(0xbae9, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xbae9, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0xe2e8, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xe2e8, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0xe2e0, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xe2e0, &XGA_Read, io_width_t::dword);

	IO_RegisterWriteHandler(0xe2ea, &XGA_Write, io_width_t::dword);
	IO_RegisterReadHandler(0xe2ea, &XGA_Read, io_width_t::dword);
}
