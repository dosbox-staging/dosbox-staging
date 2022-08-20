/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dosbox.h"

#include <cstring>
#include <cmath>

#include "../ints/int10.h"
#include "math_utils.h"
#include "mem_unaligned.h"
#include "pic.h"
#include "render.h"
#include "../gui/render_scalers.h"
#include "vga.h"
#include "video.h"

//#undef C_DEBUG
//#define C_DEBUG 1
//#define LOG(X,Y) LOG_MSG

#define VGA_PARTS 4

typedef uint8_t * (* VGA_Line_Handler)(Bitu vidstart, Bitu line);

static VGA_Line_Handler VGA_DrawLine;
alignas(vga_memalign) static uint8_t TempLine[SCALER_MAXWIDTH * 4];

static uint8_t * VGA_Draw_1BPP_Line(Bitu vidstart, Bitu line) {
	const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);

	uint16_t i = 0;
	for (Bitu x = vga.draw.blocks; x > 0; --x, ++vidstart) {
		Bitu val = base[(vidstart & (8 * 1024 -1))];
		write_unaligned_uint32_at(TempLine, i++, CGA_2_Table[val >> 4]);
		write_unaligned_uint32_at(TempLine, i++, CGA_2_Table[val & 0xf]);
	}
	return TempLine;
}

static uint8_t * VGA_Draw_2BPP_Line(Bitu vidstart, Bitu line) {
	const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);

	uint16_t i = 0;
	for (Bitu x = 0; x < vga.draw.blocks; ++x) {
		Bitu val = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		write_unaligned_uint32_at(TempLine, i++, CGA_4_Table[val]);
	}
	return TempLine;
}

static uint8_t * VGA_Draw_2BPPHiRes_Line(Bitu vidstart, Bitu line) {
	const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);

	uint16_t i = 0;
	for (Bitu x = 0; x < vga.draw.blocks; ++x) {
		Bitu val1 = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		Bitu val2 = base[vidstart & vga.tandy.addr_mask];
		++vidstart;
		write_unaligned_uint32_at(TempLine, i++,
		                          CGA_4_HiRes_Table[(val1 >> 4) | (val2 & 0xf0)]);
		write_unaligned_uint32_at( TempLine, i++,
		                          CGA_4_HiRes_Table[(val1 & 0x0f) | ((val2 & 0x0f) << 4)]);
	}
	return TempLine;
}

static uint8_t *VGA_Draw_CGA16_Line(Bitu vidstart, Bitu line)
{
	// TODO: De-Bitu the VGA API as line indexes are well-under 65k/16-bit
	assert(vidstart <= UINT16_MAX);
	assert(line <= UINT16_MAX);

	static uint8_t temp[643] = {0};
	const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask)
	                                             << vga.tandy.line_shift);

	auto read_cga16_offset = [=](uint16_t offset) -> uint8_t {
		const auto index = static_cast<uint16_t>(vidstart) + offset;
		constexpr auto index_mask = static_cast<uint16_t>(8 * 1024 - 1);
		return base[index & index_mask];
	};

	// There are 640 hdots in each line of the screen.
	// The color of an even hdot always depends on only 4 bits of video RAM.
	// The color of an odd hdot depends on 4 bits of video RAM in
	// 1-hdot-per-pixel modes and 6 bits of video RAM in 2-hdot-per-pixel
	// modes. We always assume 6 and use duplicate palette entries in
	// 1-hdot-per-pixel modes so that we can use the same routine for all
	// composite modes.
	temp[1] = (read_cga16_offset(0) >> 6) & 3;

	for (uint16_t x = 2; x < 640; x += 2) {
		temp[x] = (temp[x - 1] & 0xf);
		temp[x + 1] = (temp[x] << 2) |
		              (((read_cga16_offset(x >> 3)) >> (6 - (x & 6))) & 3);
	}
	temp[640] = temp[639] & 0xf;
	temp[641] = temp[640] << 2;
	temp[642] = temp[641] & 0xf;

	for (uint32_t i = 2, j = 0, x = 0; x < vga.draw.blocks * 2; i += 4, ++j, ++x) {
		constexpr uint32_t foundation = 0xc0708030; // colors are OR'd
		                                            // on top of this
		const uint32_t pixel = temp[i] | (temp[i + 1] << 8) | (temp[i + 2] << 16) | (temp[i + 3] << 24);
		write_unaligned_uint32_at(TempLine, j, foundation | pixel);
	}
	return TempLine;
}

static uint8_t byte_clamp(int v)
{
	v >>= 13;
	return v < 0 ? 0u : (v > 255 ? 255u : static_cast<uint8_t>(v));
}

static uint8_t *Composite_Process(uint8_t border, uint32_t blocks, bool doublewidth)
{
	static int temp[SCALER_MAXWIDTH + 10] = {0};
	static int atemp[SCALER_MAXWIDTH + 2] = {0};
	static int btemp[SCALER_MAXWIDTH + 2] = {0};

	int w = blocks * 4;

	if (doublewidth) {
		uint8_t *source = TempLine + w - 1;
		uint8_t *dest = TempLine + w * 2 - 2;
		for (int x = 0; x < w; ++x) {
			*dest = *source;
			*(dest + 1) = *source;
			--source;
			dest -= 2;
		}
		blocks *= 2;
		w *= 2;
	}

	// Simulate CGA composite output
	int *o = temp;
	auto push_pixel = [&o](const int v) {
		*o = v;
		++o;
	};

	uint8_t *rgbi = TempLine;
	int *b = &CGA_Composite_Table[border * 68];
	for (int x = 0; x < 4; ++x)
		push_pixel(b[(x + 3) & 3]);
	push_pixel(CGA_Composite_Table[(border << 6) | ((*rgbi) << 2) | 3]);
	for (int x = 0; x < w - 1; ++x) {
		push_pixel(CGA_Composite_Table[(rgbi[0] << 6) | (rgbi[1] << 2) | (x & 3)]);
		++rgbi;
	}
	push_pixel(CGA_Composite_Table[((*rgbi) << 6) | (border << 2) | 3]);
	for (int x = 0; x < 5; ++x)
		push_pixel(b[x & 3]);

	if ((vga.tandy.mode_control & 4) != 0) {
		// Decode
		int *i = temp + 5;
		uint16_t idx = 0;
		for (uint32_t x = 0; x < blocks * 4; ++x) {
			int c = (i[0] + i[0]) << 3;
			int d = (i[-1] + i[1]) << 3;
			int y = ((c + d) << 8) + vga.sharpness * (c - d);
			++i;
			write_unaligned_uint32_at(TempLine, idx++,
			                          byte_clamp(y) * 0x10101);
		}
	} else {
		// Store chroma
		int *i = temp + 4;
		int *ap = atemp + 1;
		int *bp = btemp + 1;
		for (int x = -1; x < w + 1; ++x) {
			ap[x] = i[-4] - left_shift_signed(i[-2] - i[0] + i[2], 1) + i[4];
			bp[x] = left_shift_signed(i[-3] - i[-1] + i[1] - i[3], 1);
			++i;
		}

		// Decode
		i = temp + 5;
		i[-1] = (i[-1] << 3) - ap[-1];
		i[0] = (i[0] << 3) - ap[0];

		uint16_t idx = 0;
		auto COMPOSITE_CONVERT = [&](const int I, const int Q) {
			i[1] = (i[1] << 3) - ap[1];
			const int c = i[0] + i[0];
			const int d = i[-1] + i[1];
			const int y = left_shift_signed(c + d, 8) + vga.sharpness * (c - d);
			const int rr = y + vga.ri * (I) + vga.rq * (Q);
			const int gg = y + vga.gi * (I) + vga.gq * (Q);
			const int bb = y + vga.bi * (I) + vga.bq * (Q);
			++i;
			++ap;
			++bp;
			const auto srgb = (byte_clamp(rr) << 16) |
			                  (byte_clamp(gg) << 8) | byte_clamp(bb);
			write_unaligned_uint32_at(TempLine, idx++, srgb);
		};

		for (uint32_t x = 0; x < blocks; ++x) {
			COMPOSITE_CONVERT(ap[0], bp[0]);
			COMPOSITE_CONVERT(-bp[0], ap[0]);
			COMPOSITE_CONVERT(-ap[0], -bp[0]);
			COMPOSITE_CONVERT(bp[0], -ap[0]);
		}
	}
	return TempLine;
}

static uint8_t *VGA_TEXT_Draw_Line(Bitu vidstart, Bitu line);

static uint8_t *VGA_CGA_TEXT_Composite_Draw_Line(Bitu vidstart, Bitu line)
{
	VGA_TEXT_Draw_Line(vidstart, line);
	return Composite_Process(vga.tandy.color_select & 0x0f, vga.draw.blocks * 2,
	                         (vga.tandy.mode_control & 0x1) == 0);
}

static uint8_t *VGA_Draw_CGA2_Composite_Line(Bitu vidstart, Bitu line)
{
	VGA_Draw_1BPP_Line(vidstart, line);
	return Composite_Process(0, vga.draw.blocks * 2, false);
}

static uint8_t *VGA_Draw_CGA4_Composite_Line(Bitu vidstart, Bitu line)
{
	VGA_Draw_2BPP_Line(vidstart, line);
	return Composite_Process(vga.tandy.color_select & 0x0f, vga.draw.blocks, true);
}

static uint8_t * VGA_Draw_4BPP_Line(Bitu vidstart, Bitu line) {
	const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	uint8_t* draw=TempLine;
	Bitu end = vga.draw.blocks*2;
	while(end) {
		uint8_t byte = base[vidstart & vga.tandy.addr_mask];
		*draw++=vga.attr.palette[byte >> 4];
		*draw++=vga.attr.palette[byte & 0x0f];
		++vidstart;
		--end;
	}
	return TempLine;
}

static uint8_t * VGA_Draw_4BPP_Line_Double(Bitu vidstart, Bitu line) {
	const uint8_t *base = vga.tandy.draw_base + ((line & vga.tandy.line_mask) << vga.tandy.line_shift);
	uint8_t* draw=TempLine;
	Bitu end = vga.draw.blocks;
	while(end) {
		uint8_t byte = base[vidstart & vga.tandy.addr_mask];
		uint8_t data = vga.attr.palette[byte >> 4];
		*draw++ = data; *draw++ = data;
		data = vga.attr.palette[byte & 0x0f];
		*draw++ = data; *draw++ = data;
		++vidstart;
		--end;
	}
	return TempLine;
}

#ifdef VGA_KEEP_CHANGES
static uint8_t * VGA_Draw_Changes_Line(Bitu vidstart, Bitu line) {
	Bitu checkMask = vga.changes.checkMask;
	uint8_t *map = vga.changes.map;
	Bitu start = (vidstart >> VGA_CHANGE_SHIFT);
	Bitu end = ((vidstart + vga.draw.line_length ) >> VGA_CHANGE_SHIFT);
	for (; start <= end; ++start) {
		if ( map[start] & checkMask ) {
			Bitu offset = vidstart & vga.draw.linear_mask;
			if (vga.draw.linear_mask-offset < vga.draw.line_length)
				memcpy(vga.draw.linear_base+vga.draw.linear_mask+1, vga.draw.linear_base, vga.draw.line_length);
			uint8_t *ret = &vga.draw.linear_base[ offset ];
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

static uint8_t * VGA_Draw_Linear_Line(Bitu vidstart, Bitu /*line*/) {
	Bitu offset = vidstart & vga.draw.linear_mask;
	uint8_t* ret = &vga.draw.linear_base[offset];
	
	// in case (vga.draw.line_length + offset) has bits set that
	// are not set in the mask: ((x|y)!=y) equals (x&~y)
	if (GCC_UNLIKELY((vga.draw.line_length + offset)& ~vga.draw.linear_mask)) {
		// this happens, if at all, only once per frame (1 of 480 lines)
		// in some obscure games
		Bitu end = (offset + vga.draw.line_length) & vga.draw.linear_mask;
		
		// assuming lines not longer than 4096 pixels
		Bitu wrapped_len = end & 0xFFF;
		Bitu unwrapped_len = vga.draw.line_length-wrapped_len;
		
		// unwrapped chunk: to top of memory block
		memcpy(TempLine, &vga.draw.linear_base[offset], unwrapped_len);
		// wrapped chunk: from base of memory block
		memcpy(&TempLine[unwrapped_len], vga.draw.linear_base, wrapped_len);

		ret = TempLine;
	}

#if !defined(C_UNALIGNED_MEMORY)
	if (GCC_UNLIKELY( ((Bitu)ret) & (sizeof(Bitu)-1)) ) {
		memcpy( TempLine, ret, vga.draw.line_length );
		return TempLine;
	}
#endif
	return ret;
}

static uint8_t *VGA_Draw_Xlat16_Linear_Line(Bitu vidstart, Bitu /*line*/)
{
	const auto offset = vidstart & vga.draw.linear_mask;
	const uint8_t *ret = &vga.draw.linear_base[offset];
	const uint16_t *xlat16 = vga.dac.xlat16;

	// see VGA_Draw_Linear_Line
	if (GCC_UNLIKELY((vga.draw.line_length + offset)& ~vga.draw.linear_mask)) {
		const auto end = (offset + vga.draw.line_length) &
		                 vga.draw.linear_mask;

		// assuming lines not longer than 4096 pixels
		const auto wrapped_len = end & 0xFFF;
		const auto unwrapped_len = vga.draw.line_length - wrapped_len;

		// unwrapped chunk: to top of memory block
		for (uintptr_t i = 0; i < unwrapped_len; ++i)
			write_unaligned_uint16_at(TempLine, i, xlat16[ret[i]]);

		// wrapped chunk: from base of memory block
		for (uintptr_t i = 0; i < wrapped_len; ++i)
			write_unaligned_uint16_at(TempLine, i + unwrapped_len,
			                          xlat16[vga.draw.linear_base[i]]);

	} else {
		for (uintptr_t i = 0; i < vga.draw.line_length; ++i) {
			write_unaligned_uint16_at(TempLine, i, xlat16[ret[i]]);
		}
	}
	return TempLine;
}

//Test version, might as well keep it
/* static uint8_t * VGA_Draw_Chain_Line(Bitu vidstart, Bitu line) {
	Bitu i = 0;
	for ( i = 0; i < vga.draw.width;i++ ) {
		Bitu addr = vidstart + i;
		TempLine[i] = vga.mem.linear[((addr&~3)<<2)+(addr&3)];
	}
	return TempLine;
} */

static uint8_t * VGA_Draw_VGA_Line_HWMouse( Bitu vidstart, Bitu /*line*/) {
	if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
		// HW Mouse not enabled, use the tried and true call
		return &vga.mem.linear[vidstart];

	Bitu lineat = (vidstart-(vga.config.real_start<<2)) / vga.draw.width;
	if ((vga.s3.hgc.posx >= vga.draw.width) ||
		(lineat < vga.s3.hgc.originy) || 
		(lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
		// the mouse cursor *pattern* is not on this line
		return &vga.mem.linear[ vidstart ];
	} else {
		// Draw mouse cursor: cursor is a 64x64 pattern which is shifted (inside the
		// 64x64 mouse cursor space) to the right by posx pixels and up by posy pixels.
		// This is used when the mouse cursor partially leaves the screen.
		// It is arranged as bitmap of 16bits of bitA followed by 16bits of bitB, each
		// AB bits corresponding to a cursor pixel. The whole map is 8kB in size.
		memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width);
		// the index of the bit inside the cursor bitmap we start at:
		Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
		// convert to video memory addr and bit index
		// start adjusted to the pattern structure (thus shift address by 2 instead of 3)
		// Need to get rid of the third bit, so "/8 *2" becomes ">> 2 & ~1"
		Bitu cursorMemStart = ((sourceStartBit >> 2)& ~1) + (((uint32_t)vga.s3.hgc.startaddr) << 10);
		Bitu cursorStartBit = sourceStartBit & 0x7;
		// stay at the right position in the pattern
		if (cursorMemStart & 0x2)
			--cursorMemStart;
		Bitu cursorMemEnd = cursorMemStart + ((64-vga.s3.hgc.posx) >> 2);
		uint8_t* xat = &TempLine[vga.s3.hgc.originx]; // mouse data start pos. in scanline
		for (Bitu m = cursorMemStart; m < cursorMemEnd;
		     (m & 1) ? (m += 3) : ++m) {
			// for each byte of cursor data
			uint8_t bitsA = vga.mem.linear[m];
			uint8_t bitsB = vga.mem.linear[m+2];
			for (uint8_t bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) {
				// for each bit
				cursorStartBit=0; // only the first byte has some bits cut off
				if (bitsA&bit) {
					if (bitsB&bit) *xat ^= 0xFF; // Invert screen data
					//else Transparent
				} else if (bitsB&bit) {
					*xat = vga.s3.hgc.forestack[0]; // foreground color
				} else {
					*xat = vga.s3.hgc.backstack[0];
				}
				xat++;
			}
		}
		return TempLine;
	}
}

static uint8_t * VGA_Draw_LIN16_Line_HWMouse(Bitu vidstart, Bitu /*line*/) {
	if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
		return &vga.mem.linear[vidstart];

	Bitu lineat = ((vidstart-(vga.config.real_start<<2)) >> 1) / vga.draw.width;
	if ((vga.s3.hgc.posx >= vga.draw.width) ||
		(lineat < vga.s3.hgc.originy) || 
		(lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
		return &vga.mem.linear[vidstart];
	} else {
		memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width*2);
		Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
 		Bitu cursorMemStart = ((sourceStartBit >> 2)& ~1) + (((uint32_t)vga.s3.hgc.startaddr) << 10);
		Bitu cursorStartBit = sourceStartBit & 0x7;
		if (cursorMemStart & 0x2) cursorMemStart--;
		Bitu cursorMemEnd = cursorMemStart + ((64-vga.s3.hgc.posx) >> 2);

		uint16_t i = vga.s3.hgc.originx;
		for (Bitu m = cursorMemStart; m < cursorMemEnd;
		     (m & 1) ? (m += 3) : ++m) {
			// for each byte of cursor data
			uint8_t bitsA = vga.mem.linear[m];
			uint8_t bitsB = vga.mem.linear[m+2];
			for (uint8_t bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) {
				// for each bit
				cursorStartBit=0;
				if (bitsA&bit) {
					// byte order doesn't matter here as all bits get flipped
					if (bitsB & bit) {
						const auto xat = read_unaligned_uint16_at(TempLine, i);
						write_unaligned_uint16_at(TempLine, i, xat ^ 0xffff);
					}
					// else Transparent
				} else if (bitsB & bit) {
					// Source as well as destination are uint8_t arrays, 
					// so this should work out endian-wise?
					const auto fore = read_unaligned_uint16(vga.s3.hgc.forestack);
					write_unaligned_uint16_at(TempLine, i, fore);
				} else {
					const auto back = read_unaligned_uint16(vga.s3.hgc.backstack);
					write_unaligned_uint16_at(TempLine, i, back);
				}
				++i;
			}
		}
		return TempLine;
	}
}

static uint8_t * VGA_Draw_LIN32_Line_HWMouse(Bitu vidstart, Bitu /*line*/) {
	if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
		return &vga.mem.linear[vidstart];

	Bitu lineat = ((vidstart-(vga.config.real_start<<2)) >> 2) / vga.draw.width;
	if ((vga.s3.hgc.posx >= vga.draw.width) ||
		(lineat < vga.s3.hgc.originy) || 
		(lineat > (vga.s3.hgc.originy + (63U-vga.s3.hgc.posy))) ) {
		return &vga.mem.linear[ vidstart ];
	} else {
		memcpy(TempLine, &vga.mem.linear[ vidstart ], vga.draw.width*4);
		Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx; 
		Bitu cursorMemStart = ((sourceStartBit >> 2)& ~1) + (((uint32_t)vga.s3.hgc.startaddr) << 10);
		Bitu cursorStartBit = sourceStartBit & 0x7;
		if (cursorMemStart & 0x2)
			--cursorMemStart;
		Bitu cursorMemEnd = cursorMemStart + ((64-vga.s3.hgc.posx) >> 2);
		uint16_t i = vga.s3.hgc.originx;
		for (Bitu m = cursorMemStart; m < cursorMemEnd;
		     (m & 1) ? (m += 3) : ++m) {
			// for each byte of cursor data
			uint8_t bitsA = vga.mem.linear[m];
			uint8_t bitsB = vga.mem.linear[m+2];
			for (uint8_t bit=(0x80 >> cursorStartBit); bit != 0; bit >>= 1) { // for each bit
				cursorStartBit=0;
				if (bitsA&bit) {
					if (bitsB & bit) {
						const auto xat = read_unaligned_uint32_at(TempLine, i);
						write_unaligned_uint32_at(TempLine, i, xat ^ 0xffff);
					}
					// else Transparent
				} else if (bitsB & bit) {
					const auto fore = read_unaligned_uint32(vga.s3.hgc.forestack);
					write_unaligned_uint32_at(TempLine, i, fore);
				} else {
					const auto back = read_unaligned_uint32(vga.s3.hgc.backstack);
					write_unaligned_uint32_at(TempLine, i, back);
				}
				++i;
			}
		}
		return TempLine;
	}
}

static const uint8_t* VGA_Text_Memwrap(Bitu vidstart) {
	vidstart &= vga.draw.linear_mask;
	Bitu line_end = 2 * vga.draw.blocks;
	if (GCC_UNLIKELY((vidstart + line_end) > vga.draw.linear_mask)) {
		// wrapping in this line
		Bitu break_pos = (vga.draw.linear_mask - vidstart) + 1;
		// need a temporary storage - TempLine/2 is ok for a bit more than 132 columns
		memcpy(&TempLine[sizeof(TempLine)/2], &vga.tandy.draw_base[vidstart], break_pos);
		memcpy(&TempLine[sizeof(TempLine)/2 + break_pos],&vga.tandy.draw_base[0], line_end - break_pos);
		return &TempLine[sizeof(TempLine)/2];
	} else return &vga.tandy.draw_base[vidstart];
}

static bool SkipCursor(Bitu vidstart, Bitu line)
{
	return !vga.draw.cursor.enabled || !(vga.draw.cursor.count & 0x10) ||
	       (line < vga.draw.cursor.sline) || (line > vga.draw.cursor.eline) ||
	       (vga.draw.cursor.address < vidstart);
}

static uint32_t FontMask[2]={0xffffffff,0x0};
static uint8_t *VGA_TEXT_Draw_Line(Bitu vidstart, Bitu line)
{
	uint16_t i = 0;
	const uint8_t* vidmem = VGA_Text_Memwrap(vidstart);
	for (Bitu cx = 0; cx < vga.draw.blocks; ++cx) {
		Bitu chr=vidmem[cx*2];
		Bitu col=vidmem[cx*2+1];
		Bitu font=vga.draw.font_tables[(col >> 3)&1][chr*32+line];
		uint32_t mask1=TXT_Font_Table[font>>4] & FontMask[col >> 7];
		uint32_t mask2=TXT_Font_Table[font&0xf] & FontMask[col >> 7];
		uint32_t fg=TXT_FG_Table[col&0xf];
		uint32_t bg=TXT_BG_Table[col>>4];
		write_unaligned_uint32_at(TempLine, i++, (fg & mask1) | (bg & ~mask1));
		write_unaligned_uint32_at(TempLine, i++, (fg & mask2) | (bg & ~mask2));
	}
	if (SkipCursor(vidstart, line))
		return TempLine;
	const Bitu font_addr = (vga.draw.cursor.address - vidstart) >> 1;
	if (font_addr < vga.draw.blocks) {
		uint32_t *draw = (uint32_t *)&TempLine[font_addr * 8];
		uint32_t att=TXT_FG_Table[vga.tandy.draw_base[vga.draw.cursor.address+1]&0xf];
		*draw++ = att;
		*draw++ = att;
	}
	return TempLine;
}

static uint8_t *VGA_TEXT_Herc_Draw_Line(Bitu vidstart, Bitu line)
{
	uint16_t i = 0;
	const uint8_t* vidmem = VGA_Text_Memwrap(vidstart);

	for (Bitu cx = 0; cx < vga.draw.blocks; ++cx) {
		Bitu chr=vidmem[cx*2];
		Bitu attrib=vidmem[cx*2+1];
		if (!(attrib&0x77)) {
			// 00h, 80h, 08h, 88h produce black space
			write_unaligned_uint32_at(TempLine, i++, 0);
			write_unaligned_uint32_at(TempLine, i++, 0);
		} else {
			uint32_t bg, fg;
			bool underline=false;
			if ((attrib&0x77)==0x70) {
				bg = TXT_BG_Table[0x7];
				if (attrib&0x8) fg = TXT_FG_Table[0xf];
				else fg = TXT_FG_Table[0x0];
			} else {
				if (((Bitu)(vga.crtc.underline_location&0x1f)==line) && ((attrib&0x77)==0x1)) underline=true;
				bg = TXT_BG_Table[0x0];
				if (attrib&0x8) fg = TXT_FG_Table[0xf];
				else fg = TXT_FG_Table[0x7];
			}
			uint32_t mask1, mask2;
			if (GCC_UNLIKELY(underline)) mask1 = mask2 = FontMask[attrib >> 7];
			else {
				Bitu font=vga.draw.font_tables[0][chr*32+line];
				mask1=TXT_Font_Table[font>>4] & FontMask[attrib >> 7]; // blinking
				mask2=TXT_Font_Table[font&0xf] & FontMask[attrib >> 7];
			}
			write_unaligned_uint32_at(TempLine, i++, (fg & mask1) | (bg & ~mask1));
			write_unaligned_uint32_at(TempLine, i++, (fg & mask2) | (bg & ~mask2));
		}
	}
	if (SkipCursor(vidstart, line))
		return TempLine;
	const Bitu font_addr = (vga.draw.cursor.address - vidstart) >> 1;
	if (font_addr < vga.draw.blocks) {
		uint32_t *draw = (uint32_t *)&TempLine[font_addr * 8];
		uint8_t attr = vga.tandy.draw_base[vga.draw.cursor.address+1];
		uint32_t cg;
		if (attr&0x8) {
			cg = TXT_FG_Table[0xf];
		} else if ((attr&0x77)==0x70) {
			cg = TXT_FG_Table[0x0];
		} else {
			cg = TXT_FG_Table[0x7];
		}
		*draw++ = cg;
		*draw++ = cg;
	}
	return TempLine;
}
// combined 8/9-dot wide text mode 16bpp line drawing function
static uint8_t *VGA_TEXT_Xlat16_Draw_Line(Bitu vidstart, Bitu line)
{
	// keep it aligned:
	uint16_t idx = 16 - vga.draw.panning;
	const uint8_t* vidmem = VGA_Text_Memwrap(vidstart); // pointer to chars+attribs
	Bitu blocks = vga.draw.blocks;
	if (vga.draw.panning)
		++blocks; // if the text is panned part of an
		          // additional character becomes visible
	while (blocks--) { // for each character in the line
		Bitu chr = *vidmem++;
		Bitu attr = *vidmem++;
		// the font pattern
		Bitu font = vga.draw.font_tables[(attr >> 3)&1][(chr<<5)+line];
		
		Bitu background = attr >> 4;
		// if blinking is enabled bit7 is not mapped to attributes
		if (vga.draw.blinking) background &= ~0x8;
		// choose foreground color if blinking not set for this cell or blink on
		Bitu foreground = (vga.draw.blink || (!(attr&0x80)))?
			(attr&0xf):background;
		// underline: all foreground [freevga: 0x77, previous 0x7]
		if (GCC_UNLIKELY(((attr&0x77) == 0x01) &&
			(vga.crtc.underline_location&0x1f)==line))
				background = foreground;
		if (vga.draw.char9dot) {
			font <<=1; // 9 pixels
			// extend to the 9th pixel if needed
			if ((font&0x2) && (vga.attr.mode_control&0x04) &&
				(chr>=0xc0) && (chr<=0xdf)) font |= 1;
			for (int n = 0; n < 9; ++n) {
				write_unaligned_uint16_at(
				        TempLine, idx++,
				        vga.dac.xlat16[(font & 0x100) ? foreground : background]);
				font <<= 1;
			}
		} else {
			for (int n = 0; n < 8; ++n) {
				write_unaligned_uint16_at(
				        TempLine, idx++,
				        vga.dac.xlat16[(font & 0x80) ? foreground : background]);
				font <<= 1;
			}
		}
	}
	// draw the text mode cursor if needed
	if (!SkipCursor(vidstart, line)) {
		// the adress of the attribute that makes up the cell the cursor is in
		const Bitu attr_addr = (vga.draw.cursor.address - vidstart) >> 1;
		if (attr_addr < vga.draw.blocks) {
			Bitu index = attr_addr * (vga.draw.char9dot? 18:16);
			uint16_t *draw = (uint16_t *)(&TempLine[index]) + 16 -
			               vga.draw.panning;

			Bitu foreground = vga.tandy.draw_base[vga.draw.cursor.address+1] & 0xf;
			for (int i = 0; i < 8; ++i) {
				*draw++ = vga.dac.xlat16[foreground];
			}
		}
	}
	return TempLine + 32;
}

#ifdef VGA_KEEP_CHANGES
static inline void VGA_ChangesEnd(void ) {
	if ( vga.changes.active ) {
//		vga.changes.active = false;
		Bitu end = vga.draw.address >> VGA_CHANGE_SHIFT;
		Bitu total = 4 + end - vga.changes.start;
		uint32_t clearMask = vga.changes.clearMask;
		total >>= 2;
		uint32_t *clear = (uint32_t *)&vga.changes.map[  vga.changes.start & ~3 ];
		while ( total-- ) {
			clear[0] &= clearMask;
			++clear;
		}
	}
}
#endif


static void VGA_ProcessSplit() {
	if (vga.attr.mode_control&0x20) {
		vga.draw.address=0;
		// reset panning to 0 here so we don't have to check for 
		// it in the character draw functions. It will be set back
		// to its proper value in v-retrace
		vga.draw.panning=0; 
	} else {
		// In text mode only the characters are shifted by panning, not the address;
		// this is done in the text line draw function.
		vga.draw.address = vga.draw.byte_panning_shift*vga.draw.bytes_skip;
		if ((vga.mode!=M_TEXT)&&(machine!=MCH_EGA)) vga.draw.address += vga.draw.panning;
	}
	vga.draw.address_line=0;
}

static uint8_t bg_color_index = 0; // screen-off black index
static void VGA_DrawSingleLine(uint32_t /*blah*/)
{
	if (GCC_UNLIKELY(vga.attr.disabled)) {
		switch(machine) {
		case MCH_PCJR:
			// Displays the border color when screen is disabled
			bg_color_index = vga.tandy.border_color;
			break;
		case MCH_TANDY:
			// Either the PCJr way or the CGA way
			if (vga.tandy.gfx_control& 0x4) { 
				bg_color_index = vga.tandy.border_color;
			} else if (vga.mode==M_TANDY4) 
				bg_color_index = vga.attr.palette[0];
			else bg_color_index = 0;
			break;
		case MCH_CGA:
			// the background color
			bg_color_index = vga.attr.overscan_color;
			break;
		case MCH_EGA:
		case MCH_VGA:
			// DoWhackaDo, Alien Carnage, TV sports Football
			// when disabled by attribute index bit 5:
			//  ET3000, ET4000, Paradise display the border color
			//  S3 displays the content of the currently selected attribute register
			// when disabled by sequencer the screen is black "257th color"
			
			// the DAC table may not match the bits of the overscan register
			// so use black for this case too...
			//if (vga.attr.disabled& 2) {
			if (vga.dac.xlat16[bg_color_index] != 0) {

				// check some assumptions about the translation table
				constexpr auto xlat16_len = ARRAY_LEN(vga.dac.xlat16);
				static_assert(xlat16_len == 256,
				              "The code below assumes the table is 256 elements long");

				for (uint16_t i = 0; i < xlat16_len; ++i)
					if (vga.dac.xlat16[i] == 0) {
						bg_color_index = static_cast<uint8_t>(i);
						break;
					}
			}
			//} else 
            //    bg_color_index = vga.attr.overscan_color;
			break;
		default:
			bg_color_index = 0;
			break;
		}
		if (vga.draw.bpp==8) {
			memset(TempLine, bg_color_index, sizeof(TempLine));
		} else if (vga.draw.bpp == 16) {
			uint16_t value = vga.dac.xlat16[bg_color_index];
			for (size_t i = 0; i < sizeof(TempLine) / 2; ++i) {
				write_unaligned_uint16_at(TempLine, i, value);
			}
		}
		RENDER_DrawLine(TempLine);
	} else {
		uint8_t * data=VGA_DrawLine( vga.draw.address, vga.draw.address_line );	
		RENDER_DrawLine(data);
	}

	++vga.draw.address_line;
	if (vga.draw.address_line>=vga.draw.address_line_total) {
		vga.draw.address_line=0;
		vga.draw.address+=vga.draw.address_add;
	}
	++vga.draw.lines_done;
	if (vga.draw.split_line==vga.draw.lines_done) VGA_ProcessSplit();
	if (vga.draw.lines_done < vga.draw.lines_total) {
		PIC_AddEvent(VGA_DrawSingleLine, vga.draw.delay.htotal);
	} else RENDER_EndUpdate(false);
}

static void VGA_DrawEGASingleLine(uint32_t /*blah*/)
{
	if (GCC_UNLIKELY(vga.attr.disabled)) {
		memset(TempLine, 0, sizeof(TempLine));
		RENDER_DrawLine(TempLine);
	} else {
		Bitu address = vga.draw.address;
		if (vga.mode!=M_TEXT) address += vga.draw.panning;
		uint8_t * data=VGA_DrawLine(address, vga.draw.address_line );	
		RENDER_DrawLine(data);
	}

	++vga.draw.address_line;
	if (vga.draw.address_line>=vga.draw.address_line_total) {
		vga.draw.address_line=0;
		vga.draw.address+=vga.draw.address_add;
	}
	++vga.draw.lines_done;
	if (vga.draw.split_line==vga.draw.lines_done) VGA_ProcessSplit();
	if (vga.draw.lines_done < vga.draw.lines_total) {
		PIC_AddEvent(VGA_DrawEGASingleLine, vga.draw.delay.htotal);
	} else RENDER_EndUpdate(false);
}

static void VGA_DrawPart(uint32_t lines)
{
	while (lines--) {
		uint8_t * data=VGA_DrawLine( vga.draw.address, vga.draw.address_line );
		RENDER_DrawLine(data);
		++vga.draw.address_line;
		if (vga.draw.address_line>=vga.draw.address_line_total) {
			vga.draw.address_line=0;
			vga.draw.address+=vga.draw.address_add;
		}
		++vga.draw.lines_done;
		if (vga.draw.split_line==vga.draw.lines_done) {
#ifdef VGA_KEEP_CHANGES
			VGA_ChangesEnd( );
#endif
			VGA_ProcessSplit();
#ifdef VGA_KEEP_CHANGES
			vga.changes.start = vga.draw.address >> VGA_CHANGE_SHIFT;
#endif
		}
	}
	if (--vga.draw.parts_left) {
		PIC_AddEvent(VGA_DrawPart, vga.draw.delay.parts,
		             (vga.draw.parts_left != 1)
		                     ? vga.draw.parts_lines
		                     : (vga.draw.lines_total - vga.draw.lines_done));
	} else {
#ifdef VGA_KEEP_CHANGES
		VGA_ChangesEnd();
#endif
		RENDER_EndUpdate(false);
	}
}

void VGA_SetBlinking(const uint8_t enabled)
{
	LOG(LOG_VGA, LOG_NORMAL)("Blinking %u", enabled);
	if (enabled) {
		vga.draw.blinking = 1; // used to -1 but blinking is unsigned
		vga.attr.mode_control|=0x08;
		vga.tandy.mode_control|=0x20;
	} else {
		vga.draw.blinking = 0;
		vga.attr.mode_control&=~0x08;
		vga.tandy.mode_control&=~0x20;
	}
	const uint8_t b = (enabled ? 0 : 8);
	for (uint8_t i = 0; i < 8; ++i)
		TXT_BG_Table[i + 8] = (b + i) | ((b + i) << 8) |
		                      ((b + i) << 16) | ((b + i) << 24);
}

#ifdef VGA_KEEP_CHANGES
static void inline VGA_ChangesStart( void ) {
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
	++vga.changes.frame;
	vga.changes.writeMask = 1 << (vga.changes.frame & 7);
}
#endif

static void VGA_VertInterrupt(uint32_t /*val*/)
{
	if ((!vga.draw.vret_triggered) &&
	    ((vga.crtc.vertical_retrace_end & 0x30) == 0x10)) {
		vga.draw.vret_triggered=true;
		if (GCC_UNLIKELY(machine==MCH_EGA)) PIC_ActivateIRQ(9);
	}
}

static void VGA_Other_VertInterrupt(uint32_t val)
{
	if (val)
		PIC_ActivateIRQ(5);
	else PIC_DeActivateIRQ(5);
}

static void VGA_DisplayStartLatch(uint32_t /*val*/)
{
	vga.config.real_start = vga.config.display_start & (vga.vmemwrap - 1);
	vga.draw.bytes_skip = vga.config.bytes_skip;
}

static void VGA_PanningLatch(uint32_t /*val*/)
{
	vga.draw.panning = vga.config.pel_panning;
}

static void VGA_VerticalTimer(uint32_t /*val*/)
{
	vga.draw.delay.framestart = PIC_FullIndex();
	PIC_AddEvent(VGA_VerticalTimer, vga.draw.delay.vtotal);

	switch(machine) {
	case MCH_PCJR:
	case MCH_TANDY:
		// PCJr: Vsync is directly connected to the IRQ controller
		// Some earlier Tandy models are said to have a vsync interrupt too
		PIC_AddEvent(VGA_Other_VertInterrupt, vga.draw.delay.vrstart, 1);
		PIC_AddEvent(VGA_Other_VertInterrupt, vga.draw.delay.vrend, 0);
		// fall-through
	case MCH_CGA:
	case MCH_HERC:
		// MC6845-powered graphics: Loading the display start latch happens somewhere
		// after vsync off and before first visible scanline, so probably here
		VGA_DisplayStartLatch(0);
		break;
	case MCH_VGA:
		PIC_AddEvent(VGA_DisplayStartLatch, vga.draw.delay.vrstart);
		PIC_AddEvent(VGA_PanningLatch, vga.draw.delay.vrend);
		// EGA: 82c435 datasheet: interrupt happens at display end
		// VGA: checked with scope; however disabled by default by
		// jumper on VGA boards add a little amount of time to make sure
		// the last drawpart has already fired
		PIC_AddEvent(VGA_VertInterrupt, vga.draw.delay.vdend + 0.005);
		break;
	case MCH_EGA:
		PIC_AddEvent(VGA_DisplayStartLatch, vga.draw.delay.vrend);
		PIC_AddEvent(VGA_VertInterrupt, vga.draw.delay.vdend + 0.005);
		break;
	default:
		E_Exit("This new machine needs implementation in VGA_VerticalTimer too.");
		break;
	}

	//Check if we can actually render, else skip the rest (frameskip)
	++vga.draw.cursor.count; // Do this here, else the cursor speed depends
	                         // on the frameskip
	if (!RENDER_StartUpdate())
		return;

	vga.draw.address_line = vga.config.hlines_skip;
	if (IS_EGAVGA_ARCH) {
		vga.draw.split_line = (vga.config.line_compare + 1) / vga.draw.lines_scaled;
		if ((svgaCard==SVGA_S3Trio) && (vga.config.line_compare==0)) vga.draw.split_line=0;
		vga.draw.split_line -= vga.draw.vblank_skip;
	} else {
		vga.draw.split_line = 0x10000;	// don't care
	}
	vga.draw.address = vga.config.real_start;
	vga.draw.byte_panning_shift = 0;
	// go figure...
	if (machine==MCH_EGA) {
		if (vga.draw.doubleheight) // Spacepigs EGA Megademo
			vga.draw.split_line*=2;
		++vga.draw.split_line; // EGA adds one buggy scanline
	}
//	if (machine==MCH_EGA) vga.draw.split_line = ((((vga.config.line_compare&0x5ff)+1)*2-1)/vga.draw.lines_scaled);
#ifdef VGA_KEEP_CHANGES
	bool startaddr_changed=false;
#endif
	switch (vga.mode) {
	case M_EGA:
		if (!(vga.crtc.mode_control&0x1)) vga.draw.linear_mask &= ~0x10000;
		else vga.draw.linear_mask |= 0x10000;
		[[fallthrough]];
	case M_LIN4:
		vga.draw.byte_panning_shift = 8;
		vga.draw.address += vga.draw.bytes_skip;
		vga.draw.address *= vga.draw.byte_panning_shift;
		if (machine!=MCH_EGA) vga.draw.address += vga.draw.panning;
#ifdef VGA_KEEP_CHANGES
		startaddr_changed=true;
#endif
		break;
	case M_VGA:
		if (vga.config.compatible_chain4 && (vga.crtc.underline_location & 0x40)) {
			vga.draw.linear_base = vga.fastmem;
			vga.draw.linear_mask = 0xffff;
		} else {
			vga.draw.linear_base = vga.mem.linear;
			vga.draw.linear_mask = vga.vmemwrap - 1;
		}
		[[fallthrough]];
	case M_LIN8:
	case M_LIN15:
	case M_LIN24:
	case M_LIN16:
	case M_LIN32:
		vga.draw.byte_panning_shift = 4;
		vga.draw.address += vga.draw.bytes_skip;
		vga.draw.address *= vga.draw.byte_panning_shift;
		vga.draw.address += vga.draw.panning;
#ifdef VGA_KEEP_CHANGES
		startaddr_changed=true;
#endif
		break;
	case M_TEXT:
		vga.draw.byte_panning_shift = 2;
		vga.draw.address += vga.draw.bytes_skip;
		// fall-through
	case M_TANDY_TEXT:
	case M_CGA_TEXT_COMPOSITE:
	case M_HERC_TEXT:
		if (machine==MCH_HERC) vga.draw.linear_mask = 0xfff; // 1 page
		else if (IS_EGAVGA_ARCH) vga.draw.linear_mask = 0x7fff; // 8 pages
		else vga.draw.linear_mask = 0x3fff; // CGA, Tandy 4 pages
		vga.draw.cursor.address=vga.config.cursor_start*2;
		vga.draw.address *= 2;
		//vga.draw.cursor.count++; //Moved before the frameskip test.
		/* check for blinking and blinking change delay */
		FontMask[1]=(vga.draw.blinking & (vga.draw.cursor.count >> 4)) ?
			0 : 0xffffffff;
		/* if blinking is enabled, 'blink' will toggle between true
		 * and false. Otherwise it's true */
		vga.draw.blink = ((vga.draw.blinking & (vga.draw.cursor.count >> 4))
			|| !vga.draw.blinking) ? true:false;
		break;
	case M_HERC_GFX:
	case M_CGA2:
	case M_CGA4: vga.draw.address = (vga.draw.address * 2) & 0x1fff; break;
	case M_CGA16:
	case M_CGA2_COMPOSITE:
	case M_CGA4_COMPOSITE:
	case M_TANDY2:
	case M_TANDY4:
	case M_TANDY16: vga.draw.address *= 2; break;
	default:
		break;
	}
	if (GCC_UNLIKELY(vga.draw.split_line==0)) VGA_ProcessSplit();
#ifdef VGA_KEEP_CHANGES
	if (startaddr_changed) VGA_ChangesStart();
#endif

	// check if some lines at the top off the screen are blanked
	double draw_skip = 0.0;
	if (GCC_UNLIKELY(vga.draw.vblank_skip)) {
		draw_skip = vga.draw.delay.htotal * static_cast<double>(vga.draw.vblank_skip);
		vga.draw.address += vga.draw.address_add * vga.draw.vblank_skip / vga.draw.address_line_total;
	}

	// add the draw event
	switch (vga.draw.mode) {
	case PART:
		if (GCC_UNLIKELY(vga.draw.parts_left)) {
			LOG(LOG_VGAMISC, LOG_NORMAL)("Parts left: %u", vga.draw.parts_left);
			PIC_RemoveEvents(VGA_DrawPart);
			RENDER_EndUpdate(true);
		}
		vga.draw.lines_done = 0;
		vga.draw.parts_left = vga.draw.parts_total;
		PIC_AddEvent(VGA_DrawPart, vga.draw.delay.parts + draw_skip, vga.draw.parts_lines);
		break;
	case DRAWLINE:
	case EGALINE:
		if (GCC_UNLIKELY(vga.draw.lines_done < vga.draw.lines_total)) {
			LOG(LOG_VGAMISC, LOG_NORMAL)("Lines left: %d",
			                             static_cast<int>(vga.draw.lines_total - vga.draw.lines_done));
			if (vga.draw.mode == EGALINE)
				PIC_RemoveEvents(VGA_DrawEGASingleLine);
			else
				PIC_RemoveEvents(VGA_DrawSingleLine);
			RENDER_EndUpdate(true);
		}
		vga.draw.lines_done = 0;
		if (vga.draw.mode==EGALINE)
			PIC_AddEvent(VGA_DrawEGASingleLine,
			             vga.draw.delay.htotal / 4.0 + draw_skip);
		else
			PIC_AddEvent(VGA_DrawSingleLine,
			             vga.draw.delay.htotal / 4.0 + draw_skip);
		break;
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
	case M_LIN24:
	case M_LIN32:
		vga.draw.address_add=vga.config.scan_len*8;
		break;
	case M_TEXT:
		vga.draw.address_add=vga.config.scan_len*4;
		break;
	case M_CGA2:
	case M_CGA4:
	case M_CGA16: vga.draw.address_add = 80; break;
	case M_TANDY2:
		if (machine == MCH_PCJR) {
			vga.draw.address_add = vga.draw.blocks / 4;
			break;
		}
		[[fallthrough]];
	case M_CGA2_COMPOSITE: vga.draw.address_add = vga.draw.blocks; break;
	case M_TANDY4:
	case M_CGA4_COMPOSITE: vga.draw.address_add = vga.draw.blocks; break;
	case M_TANDY16:
		vga.draw.address_add=vga.draw.blocks;
		break;
	case M_TANDY_TEXT:
	case M_CGA_TEXT_COMPOSITE:
	case M_HERC_TEXT:
		vga.draw.address_add=vga.draw.blocks*2;
		break;
	case M_HERC_GFX:
		vga.draw.address_add=vga.draw.blocks;
		break;
	default:
		vga.draw.address_add=vga.draw.blocks*8;
		break;
	}
}

void VGA_ActivateHardwareCursor(void) {
	bool hwcursor_active=false;
	if (svga.hardware_cursor_active) {
		if (svga.hardware_cursor_active()) hwcursor_active=true;
	}
	if (hwcursor_active) {
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

static void maybe_aspect_correct_tall_modes(double &current_ratio)
{
	// If we're in a mode that's wider than it is tall, then do nothing
	if (CurMode->swidth >= CurMode->sheight || current_ratio > 0.9)
		return;

	// We're in a tall mode

	if (!render.aspect) {
		LOG_INFO("VGA: Tall resolution (%ux%u) may appear stretched wide, per 'aspect = false'",
	         CurMode->swidth, CurMode->sheight);
		// by default, the current ratio is already stretched out
		return;
	}

	LOG_INFO("VGA: Tall resolution (%ux%u) will not be wide-stretched, per 'aspect = true'",
	         CurMode->swidth, CurMode->sheight);
	current_ratio *= 2;
}

void VGA_SetupDrawing(uint32_t /*val*/)
{
	if (vga.mode == M_ERROR) {
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_PanningLatch);
		PIC_RemoveEvents(VGA_DisplayStartLatch);
		return;
	}
	// set the drawing mode
	switch (machine) {
	case MCH_CGA:
	case MCH_PCJR:
	case MCH_TANDY:
		vga.draw.mode = DRAWLINE;
		break;
	case MCH_EGA:
		// Note: The Paradise SVGA uses the same panning mechanism as EGA
		vga.draw.mode = EGALINE;
		break;
	case MCH_VGA:
		if (svgaCard==SVGA_None) {
			vga.draw.mode = DRAWLINE;
			break;
		}
		// fall-through
	default:
		vga.draw.mode = PART;
		break;
	}

	/* Calculate the FPS for this screen */
	uint32_t clock;
	uint32_t htotal, hdend, hbstart, hbend, hrstart, hrend;
	uint32_t vtotal, vdend, vbstart, vbend, vrstart, vrend;
	uint32_t vblank_skip;
	if (IS_EGAVGA_ARCH) {
		htotal = vga.crtc.horizontal_total;
		hdend = vga.crtc.horizontal_display_end;
		hbend = vga.crtc.end_horizontal_blanking&0x1F;
		hbstart = vga.crtc.start_horizontal_blanking;
		hrstart = vga.crtc.start_horizontal_retrace;

		vtotal= vga.crtc.vertical_total | ((vga.crtc.overflow & 1) << 8);
		vdend = vga.crtc.vertical_display_end | ((vga.crtc.overflow & 2)<<7);
		vbstart = vga.crtc.start_vertical_blanking | ((vga.crtc.overflow & 0x08) << 5);
		vrstart = vga.crtc.vertical_retrace_start + ((vga.crtc.overflow & 0x04) << 6);
		
		if (IS_VGA_ARCH) {
			// additional bits only present on vga cards
			htotal |= (vga.s3.ex_hor_overflow & 0x1) << 8;
			htotal += 3;
			hdend |= (vga.s3.ex_hor_overflow & 0x2) << 7;
			hbend |= (vga.crtc.end_horizontal_retrace&0x80) >> 2;
			hbstart |= (vga.s3.ex_hor_overflow & 0x4) << 6;
			hrstart |= (vga.s3.ex_hor_overflow & 0x10) << 4;
			
			vtotal |= (vga.crtc.overflow & 0x20) << 4;
			vtotal |= (vga.s3.ex_ver_overflow & 0x1) << 10;
			vdend |= (vga.crtc.overflow & 0x40) << 3; 
			vdend |= (vga.s3.ex_ver_overflow & 0x2) << 9;
			vbstart |= (vga.crtc.maximum_scan_line & 0x20) << 4;
			vbstart |= (vga.s3.ex_ver_overflow & 0x4) << 8;
			vrstart |= ((vga.crtc.overflow & 0x80) << 2);
			vrstart |= (vga.s3.ex_ver_overflow & 0x10) << 6;
			vbend = vga.crtc.end_vertical_blanking & 0x7f;
		} else { // EGA
			vbend = vga.crtc.end_vertical_blanking & 0x1f;
		}
		htotal += 2;
		vtotal += 2;
		hdend += 1;
		vdend += 1;

		hbend = hbstart + ((hbend - hbstart) & 0x3F);
		hrend = vga.crtc.end_horizontal_retrace & 0x1f;
		hrend = (hrend - hrstart) & 0x1f;
		
		if ( !hrend ) hrend = hrstart + 0x1f + 1;
		else hrend = hrstart + hrend;

		vrend = vga.crtc.vertical_retrace_end & 0xF;
		vrend = ( vrend - vrstart)&0xF;
		
		if ( !vrend) vrend = vrstart + 0xf + 1;
		else vrend = vrstart + vrend;

		// Special case vbstart==0:
		// Most graphics cards agree that lines zero to vbend are
		// blanked. ET4000 doesn't blank at all if vbstart==vbend.
		// ET3000 blanks lines 1 to vbend (255/6 lines).
		if (vbstart != 0) {
			vbstart += 1;
			vbend = (vbend - vbstart) & 0x7f;
			if ( !vbend) vbend = vbstart + 0x7f + 1;
			else vbend = vbstart + vbend;
		}
		vbend++;
			
		if (svga.get_clock) {
			clock = svga.get_clock();
		} else {
			switch ((vga.misc_output >> 2) & 3) {
			case 0:	
				clock = (machine==MCH_EGA) ? 14318180 : 25175000;
				break;
			case 1:
			default:
				clock = (machine==MCH_EGA) ? 16257000 : 28322000;
				break;
			}
		}

		// Adjust the VGA clock frequency based on the Clocking Mode Register's
		// 9/8 Dot Mode. See Timing Model: https://wiki.osdev.org/VGA_Hardware
		clock /= (vga.seq.clocking_mode & 1) ? 8 : 9;

		// Adjust the horizontal frequency if in pixel-doubling mode (clock/2)
		if (vga.seq.clocking_mode & 0x8) {
			htotal *= 2;
		}
		vga.draw.address_line_total = (vga.crtc.maximum_scan_line & 0x1f) + 1;
		if (IS_VGA_ARCH && (svgaCard==SVGA_None) && (vga.mode==M_EGA || vga.mode==M_VGA)) {
			// vgaonly; can't use with CGA because these use address_line for their
			// own purposes.
			// Set the low resolution modes to have as many lines as are scanned - 
			// Quite a few demos change the max_scanline register at display time
			// to get SFX: Majic12 show, Magic circle, Copper, GBU, Party91
			if ( vga.crtc.maximum_scan_line&0x80) vga.draw.address_line_total*=2;
			vga.draw.double_scan=false;
		}
		else if (IS_VGA_ARCH) vga.draw.double_scan=(vga.crtc.maximum_scan_line&0x80)>0;
		else vga.draw.double_scan=(vtotal==262);
	} else {
		htotal = vga.other.htotal + 1;
		hdend = vga.other.hdend;
		hbstart = hdend;
		hbend = htotal;
		hrstart = vga.other.hsyncp;
		hrend = hrstart + vga.other.hsyncw;

		vga.draw.address_line_total = vga.other.max_scanline + 1;
		vtotal = vga.draw.address_line_total * (vga.other.vtotal+1)+vga.other.vadjust;
		vdend = vga.draw.address_line_total * vga.other.vdend;
		vrstart = vga.draw.address_line_total * vga.other.vsyncp;
		vrend = vrstart + 16; // vsync width is fixed to 16 lines on the MC6845 TODO Tandy
		vbstart = vdend;
		vbend = vtotal;
		vga.draw.double_scan=false;
		switch (machine) {
		case MCH_CGA:
		case TANDY_ARCH_CASE:
			clock=((vga.tandy.mode_control & 1) ? 14318180 : (14318180/2))/8;
			break;
		case MCH_HERC:
			if (vga.herc.mode_control & 0x2) clock=16000000/16;
			else clock=16000000/8;
			break;
		default:
			clock = 14318180;
			break;
		}
		// in milliseconds
		vga.draw.delay.hdend = static_cast<double>(hdend) * 1000.0 /
		                       static_cast<double>(clock);
	}
#if C_DEBUG
	LOG(LOG_VGA, LOG_NORMAL)("h total %u end %u blank (%u/%u) retrace (%u/%u)",
	                         htotal, hdend, hbstart, hbend, hrstart, hrend);
	LOG(LOG_VGA, LOG_NORMAL)("v total %u end %u blank (%u/%u) retrace (%u/%u)",
	                         vtotal, vdend, vbstart, vbend, vrstart, vrend);
#endif

	// The screen refresh frequency and clock settings, per the DOS-mode
	vga.draw.dos_refresh_hz = static_cast<double>(clock) / (vtotal * htotal);
	const auto fps = VGA_GetPreferredRate();
	const auto f_clock = fps * vtotal * htotal;

	// Horizontal total (that's how long a line takes with whistles and bells)
	vga.draw.delay.htotal = static_cast<double>(htotal) * 1000.0 / f_clock; //  milliseconds
	// Start and End of horizontal blanking
	vga.draw.delay.hblkstart = static_cast<double>(hbstart) * 1000.0 / f_clock; //  milliseconds
	vga.draw.delay.hblkend = static_cast<double>(hbend) * 1000.0 / f_clock;
	// Start and End of horizontal retrace
	vga.draw.delay.hrstart = static_cast<double>(hrstart) * 1000.0 / f_clock;
	vga.draw.delay.hrend = static_cast<double>(hrend) * 1000.0 / f_clock;
	// Start and End of vertical blanking
	vga.draw.delay.vblkstart = static_cast<double>(vbstart) * vga.draw.delay.htotal;
	vga.draw.delay.vblkend = static_cast<double>(vbend) * vga.draw.delay.htotal;
	// Start and End of vertical retrace pulse
	vga.draw.delay.vrstart = static_cast<double>(vrstart) * vga.draw.delay.htotal;
	vga.draw.delay.vrend = static_cast<double>(vrend) * vga.draw.delay.htotal;

	// Vertical blanking tricks
	vblank_skip = 0;
	if (IS_VGA_ARCH) { // others need more investigation
		if (vbstart < vtotal) { // There will be no blanking at all otherwise
			if (vbend > vtotal) {
				// blanking wraps to the start of the screen
				vblank_skip = vbend&0x7f;
				
				// on blanking wrap to 0, the first line is not blanked
				// this is used by the S3 BIOS and other S3 drivers in some SVGA modes
				if ((vbend&0x7f)==1) vblank_skip = 0;
				
				// it might also cut some lines off the bottom
				if (vbstart < vdend) {
					vdend = vbstart;
				}
				LOG(LOG_VGA, LOG_WARN)("Blanking wrap to line %u", vblank_skip);
			} else if (vbstart <= 1) {
				// blanking is used to cut lines at the start of the screen
				vblank_skip = vbend;
				LOG(LOG_VGA, LOG_WARN)("Upper %u lines of the screen blanked", vblank_skip);
			} else if (vbstart < vdend) {
				if (vbend < vdend) {
					// the game wants a black bar somewhere on the screen
					LOG(LOG_VGA, LOG_WARN)("Unsupported blanking: line %u-%u", vbstart, vbend);
				} else {
					// blanking is used to cut off some lines from the bottom
					vdend = vbstart;
				}
			}
			vdend -= vblank_skip;
		}
	}
	// Display end
	vga.draw.delay.vdend = static_cast<double>(vdend) * vga.draw.delay.htotal;

	// EGA frequency dependent monitor palette
	if (machine == MCH_EGA) {
		if (vga.misc_output & 1) {
			// EGA card is in color mode
			if ((1.0 / vga.draw.delay.htotal) > 19.0) {
				// 64 color EGA mode
				VGA_ATTR_SetEGAMonitorPalette(EGA);
			} else {
				// 16 color CGA mode compatibility
				VGA_ATTR_SetEGAMonitorPalette(CGA);
			}
		} else {
			// EGA card in monochrome mode
			// It is not meant to be autodetected that way, you either
			// have a monochrome or color monitor connected and
			// the EGA switches configured appropriately.
			// But this would only be a problem if a program sets 
			// the adapter to monochrome mode and still expects color output.
			// Such a program should be shot to the moon...
			VGA_ATTR_SetEGAMonitorPalette(MONO);
		}
	}

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
	const auto pwidth = 100.0 / static_cast<double>(htotal);
	// Base pixel height around vertical totals of modes that have 100
	// clocks horizontal Different sync values gives different scaling of
	// the whole vertical range VGA monitor just seems to thighten or widen
	// the whole vertical range
	double pheight;
	double target_total = 449.0;
	Bitu sync = vga.misc_output >> 6;
	const auto f_vtotal = static_cast<double>(vtotal);
	switch ( sync ) {
	case 0:		// This is not defined in vga specs,
				// Kiet, seems to be slightly less than 350 on my monitor
		//340 line mode, filled with 449 total
		pheight = (480.0 / 340.0) * (target_total / f_vtotal);
		break;
	case 1:		//400 line mode, filled with 449 total
		pheight = (480.0 / 400.0) * (target_total / f_vtotal);
		break;
	case 2:		//350 line mode, filled with 449 total
		//This mode seems to get regular 640x400 timing and goes for a loong retrace
		//Depends on the monitor to stretch the screen
		pheight = (480.0 / 350.0) * (target_total / f_vtotal);
		break;
	case 3:		//480 line mode, filled with 525 total
	default:
		//Allow 527 total ModeX to have exact 1:1 aspect
		target_total = (vga.mode == M_VGA && f_vtotal == 527) ? 527.0 : 525.0;
		pheight = (480.0 / 480.0) * (target_total / f_vtotal);
		break;
	}

	double aspect_ratio;
	if (machine == MCH_EGA)
		aspect_ratio = (CurMode->swidth * 3.0) / (CurMode->sheight * 4.0);
	else
		aspect_ratio = pheight / pwidth;

	vga.draw.delay.parts = vga.draw.delay.vdend / static_cast<double>(vga.draw.parts_total);
	vga.draw.resizing = false;
	vga.draw.vret_triggered=false;

	//Check to prevent useless black areas
	if (hbstart<hdend) hdend=hbstart;
	if ((!IS_VGA_ARCH) && (vbstart<vdend)) vdend=vbstart;

	auto width = hdend;
	auto height = vdend;
	bool doubleheight = false;
	bool doublewidth = false;

	unsigned bpp;
	switch (vga.mode) {
	case M_LIN15:
		bpp = 15;
		break;
	case M_LIN16:
		bpp = 16;
		break;
	case M_LIN24:
		bpp = 24;
		break;
	case M_LIN32:
	case M_CGA2_COMPOSITE:
	case M_CGA4_COMPOSITE:
	case M_CGA_TEXT_COMPOSITE: bpp = 32; break;
	default:
		bpp = 8;
		break;
	}
	vga.draw.linear_base = vga.mem.linear;
	vga.draw.linear_mask = vga.vmemwrap - 1;
	switch (vga.mode) {
	case M_VGA:
		doublewidth=true;
		width<<=2;
		if (IS_VGA_ARCH && (CurMode->type == M_VGA || svgaCard == SVGA_None)) {
			bpp = 16;
			VGA_DrawLine = VGA_Draw_Xlat16_Linear_Line;
		} else {
			VGA_DrawLine = VGA_Draw_Linear_Line;
		}
		break;
	case M_LIN8:
		if (vga.crtc.mode_control & 0x8) {
			width >>=1;
		} else if (svgaCard == SVGA_S3Trio && !(vga.s3.reg_3a & 0x10)) {
			doublewidth=true;
			width >>=1;
		}
		// fall-through
	case M_LIN24:
	case M_LIN32:
		width<<=3;
		if (vga.crtc.mode_control & 0x8) {
 			doublewidth = true;
			maybe_aspect_correct_tall_modes(aspect_ratio);
			if (vga.mode == M_LIN32) {
				// Modes 10f/190/191/192
				switch (CurMode->mode) {
				case 0x10f: aspect_ratio *= 2.0; break;
				case 0x190: aspect_ratio *= 2.0; break;
				case 0x191: aspect_ratio *= 2.0; break;
				case 0x192: aspect_ratio *= 2.0; break;
				};
			}
		}
		/* Use HW mouse cursor drawer if enabled */
		VGA_ActivateHardwareCursor();
		break;
	case M_LIN15:
 	case M_LIN16:
		// 15/16 bpp modes double the horizontal values
		width<<=2;
		// tall VESA modes
		maybe_aspect_correct_tall_modes(aspect_ratio);
		if ((vga.crtc.mode_control & 0x8))
			doublewidth = true;
		else
			aspect_ratio /= 2.0;
		/* Use HW mouse cursor drawer if enabled */
		VGA_ActivateHardwareCursor();
		break;
	case M_LIN4:
		doublewidth=(vga.seq.clocking_mode & 0x8) > 0;
		vga.draw.blocks = width;
		width<<=3;
		VGA_DrawLine=VGA_Draw_Linear_Line;
		vga.draw.linear_base = vga.fastmem;
		vga.draw.linear_mask = (static_cast<uint64_t>(vga.vmemwrap) << 1) - 1;
		break;
	case M_EGA:
		doublewidth=(vga.seq.clocking_mode & 0x8) > 0;
		vga.draw.blocks = width;
		width<<=3;
		if ((IS_VGA_ARCH) && (svgaCard==SVGA_None)) {
			// This would also be required for EGA in Spacepigs Megademo
			bpp = 16;
			VGA_DrawLine = VGA_Draw_Xlat16_Linear_Line;
		} else VGA_DrawLine=VGA_Draw_Linear_Line;

		vga.draw.linear_base = vga.fastmem;
		vga.draw.linear_mask = (static_cast<uint64_t>(vga.vmemwrap) << 1) - 1;
		break;
	case M_CGA16:
		aspect_ratio = 1.2;
		doubleheight = true;
		vga.draw.blocks = width * 2;
		width <<= 4;
		VGA_DrawLine = VGA_Draw_CGA16_Line;
		break;
	case M_CGA2_COMPOSITE:
		aspect_ratio = 1.2;
		doubleheight=true;
		vga.draw.blocks=width*2;
		width<<=4;
		VGA_DrawLine = VGA_Draw_CGA2_Composite_Line;
		break;
	case M_CGA4_COMPOSITE:
		aspect_ratio = 1.2;
		doubleheight = true;
		vga.draw.blocks = width * 2;
		width <<= 4;
		VGA_DrawLine = VGA_Draw_CGA4_Composite_Line;
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
		vga.draw.blocks=width;
		doublewidth=(vga.seq.clocking_mode & 0x8) > 0;
		if ((IS_VGA_ARCH) && (svgaCard==SVGA_None)) {
			// vgaonly: allow 9-pixel wide fonts
			if (vga.seq.clocking_mode&0x01) {
				vga.draw.char9dot = false;
				width*=8;
			} else {
				vga.draw.char9dot = true;
				width*=9;
				aspect_ratio *= 1.125;
			}
			VGA_DrawLine=VGA_TEXT_Xlat16_Draw_Line;
			bpp = 16;
		} else {
			// not vgaonly: force 8-pixel wide fonts
			width*=8; // 8 bit wide text font
			vga.draw.char9dot = false;
			VGA_DrawLine=VGA_TEXT_Draw_Line;
		}
		break;
	case M_HERC_GFX:
		vga.draw.blocks=width*2;
		width*=16;
		assert(width > 0);
		assert(height > 0);
		aspect_ratio = (static_cast<double>(width) /
		                static_cast<double>(height)) *
		               (3.0 / 4.0);
		VGA_DrawLine = VGA_Draw_1BPP_Line;
		break;

		/*
		Mode Control Registers 1 and 2 bit values for Tandy and PCJr (if different)
		~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		| Colors | Res     | double | MC1: 4 | 3   | 2+bw | 1+gfx | 0+Hi | MC2: 4 | 3     | 2   | 1   | 0 |
		|--------|---------|--------|--------|-----|------|-------|------|--------|-------|-----|-----|---|
		| 2      | 640x200 | yes    | 1 (0)  | -   | 0    | 1     | 0    | 0 (-)  | 0 (1) | -   | -   | - |
		| 4-gray | 320x200 | no     | 0      | -   | 1    | 1     | 0    | 0 (-)  | 0     | -   | -   | - |
		| 4      | 320x200 | no     | 0      | -   | 0    | 1     | 0    | 0 (-)  | 0     | -   | -   | - |
		| 4      | 640x200 | yes    | 1 (0)  | -   | 0    | 1     | 1    | 0 (-)  | 1 (0) | -   | -   | - |
		| 16     | 120x200 | no     | 0 (1)  | -   | 0    | 1     | 0    | 1 (-)  | 0     | -   | -   | - |
		| 16     | 320x200 | no     | 0 (1)  | -   | 0    | 1     | 1    | 1 (-)  | 0     | -   | -   | - |

		MC1 stands for the Mode control 1 register, bits four through
		zero. MC2 stands for the Mode control 2 register, bits four
		through zero.

		Dosbox uses vga.tandy.mode_control and vga.tandy.gfx_control to
		represent the state of these registers. They are used
		interchangably for Tandy and PCJr modes.

		References:
		-
		http://www.thealmightyguru.com/Wiki/images/3/3b/Tandy_1000_-_Manual_-_Technical_Reference.pdf:
		pg 58.
		-
		http://bitsavers.trailing-edge.com/pdf/ibm/pc/pc_jr/PCjr_Technical_Reference_Nov83.pdf:
		pg 2-59 to 2-69.
		*/

	case M_TANDY2:
		aspect_ratio = 1.2;
		doubleheight = true;
		VGA_DrawLine = VGA_Draw_1BPP_Line;

		if (machine == MCH_PCJR) {
			doublewidth = (vga.tandy.gfx_control & 0x8) == 0;
			vga.draw.blocks = width * (doublewidth ? 4 : 8);
			width = vga.draw.blocks * 2;
		} else {
			doublewidth = (vga.tandy.mode_control & 0x10) == 0;
			vga.draw.blocks = width * (doublewidth ? 1 : 2);
			width = vga.draw.blocks * 8;
		}
		break;
	case M_TANDY4:
		aspect_ratio = 1.2;
		doubleheight = true;
		if (machine == MCH_TANDY)
			doublewidth = (vga.tandy.mode_control & 0b10000) == 0b00000;
		else
			doublewidth = (vga.tandy.mode_control & 0b00001) == 0b00000;

		vga.draw.blocks = width * 2;
		width = vga.draw.blocks * 4;
		if ((machine == MCH_TANDY && (vga.tandy.gfx_control & 0b01000)) ||
		    (machine == MCH_PCJR && (vga.tandy.mode_control == 0b01011)))
			VGA_DrawLine = VGA_Draw_2BPPHiRes_Line;
		else
			VGA_DrawLine = VGA_Draw_2BPP_Line;
		break;
	case M_TANDY16:
		aspect_ratio = 1.2;
		doubleheight=true;
		vga.draw.blocks=width*2;
		if (vga.tandy.mode_control & 0x1) {
			if ((machine == MCH_TANDY) &&
			    (vga.tandy.mode_control & 0b10000)) {
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
		aspect_ratio = 1.2;
		doubleheight=true;
		vga.draw.blocks=width;
		width<<=3;
		VGA_DrawLine=VGA_TEXT_Draw_Line;
		break;
	case M_CGA_TEXT_COMPOSITE:
		aspect_ratio = 1.2;
		doubleheight = true;
		vga.draw.blocks = width;
		width <<= (((vga.tandy.mode_control & 0x1) != 0) ? 3 : 4);
		VGA_DrawLine = VGA_CGA_TEXT_Composite_Draw_Line;
		break;
	case M_HERC_TEXT:
		aspect_ratio = 480.0 / 350.0;
		vga.draw.blocks=width;
		width<<=3;
		VGA_DrawLine=VGA_TEXT_Herc_Draw_Line;
		break;
	default:
		LOG(LOG_VGA,LOG_ERROR)("Unhandled VGA mode %d while checking for resolution",vga.mode);
		break;
	}
	VGA_CheckScanLength();
	if (vga.draw.double_scan) {
		if (IS_VGA_ARCH) {
			vblank_skip /= 2;
			height/=2;
		}
		doubleheight=true;
	}
	vga.draw.vblank_skip = vblank_skip;
		
	if (!(IS_VGA_ARCH && (svgaCard==SVGA_None) && (vga.mode==M_EGA || vga.mode==M_VGA))) {
		//Only check for extra double height in vga modes
		//(line multiplying by address_line_total)
		if (!doubleheight && (vga.mode<M_TEXT) && !(vga.draw.address_line_total & 1)) {
			vga.draw.address_line_total/=2;
			doubleheight=true;
			height/=2;
		}
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
	   Cheap hack to just make all > 640x480 modes have square pixels
	*/
	if ( width >= 640 && height >= 480 ) {
		aspect_ratio = 1.0;
	}
//	LOG_MSG("ht %d vt %d ratio %f", htotal, vtotal, aspect_ratio );

	bool fps_changed = false;
	// need to change the vertical timing?
	if (fabs(vga.draw.delay.vtotal - 1000.0 / fps) > 0.0001) {
		fps_changed = true;
		vga.draw.delay.vtotal = 1000.0 / fps;
		VGA_KillDrawing();
		PIC_RemoveEvents(VGA_Other_VertInterrupt);
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_PanningLatch);
		PIC_RemoveEvents(VGA_DisplayStartLatch);
		VGA_VerticalTimer(0);
	}

#if C_DEBUG
	LOG(LOG_VGA, LOG_NORMAL)("h total %2.5f (%3.2fkHz) blank(%02.5f/%02.5f) retrace(%02.5f/%02.5f)",
	                         vga.draw.delay.htotal, (1.0 / vga.draw.delay.htotal), vga.draw.delay.hblkstart,
	                         vga.draw.delay.hblkend, vga.draw.delay.hrstart, vga.draw.delay.hrend);
	LOG(LOG_VGA, LOG_NORMAL)("v total %2.5f (%3.2fHz) blank(%02.5f/%02.5f) retrace(%02.5f/%02.5f)",
	                         vga.draw.delay.vtotal, (1000.0 / vga.draw.delay.vtotal), vga.draw.delay.vblkstart,
	                         vga.draw.delay.vblkend, vga.draw.delay.vrstart, vga.draw.delay.vrend);
#endif

	// need to resize the output window?
	if ((width != vga.draw.width) || (height != vga.draw.height) ||
	    (vga.draw.doublewidth != doublewidth) || (vga.draw.doubleheight != doubleheight) ||
	    (fabs(aspect_ratio - vga.draw.aspect_ratio) > 0.0001) ||
	    (vga.draw.bpp != bpp) || fps_changed) {
		VGA_KillDrawing();

		vga.draw.width = width;
		vga.draw.height = height;
		vga.draw.doublewidth = doublewidth;
		vga.draw.doubleheight = doubleheight;
		vga.draw.aspect_ratio = aspect_ratio;
		vga.draw.bpp = bpp;
		if (doubleheight) vga.draw.lines_scaled=2;
		else vga.draw.lines_scaled=1;

#if C_DEBUG
		LOG(LOG_VGA, LOG_NORMAL)("VGA: Width %u, Height %u, fps %.3f", width, height, fps);
		LOG(LOG_VGA, LOG_NORMAL)("VGA: %s width, %s height aspect %.3f",
		                         doublewidth ? "double" : "normal",
		                         doubleheight ? "double" : "normal", aspect_ratio);
#endif
		RENDER_SetSize(width, height, bpp, fps, aspect_ratio,
		               doublewidth, doubleheight);
	}
}

void VGA_KillDrawing(void) {
	PIC_RemoveEvents(VGA_DrawPart);
	PIC_RemoveEvents(VGA_DrawSingleLine);
	PIC_RemoveEvents(VGA_DrawEGASingleLine);
	vga.draw.parts_left = 0;
	vga.draw.lines_done = ~0;
	RENDER_EndUpdate(true);
}
