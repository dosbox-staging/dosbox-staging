// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>

#include "../gui/render/scaler/scalers.h"
#include "../ints/int10.h"
#include "bitops.h"
#include "math_utils.h"
#include "mem_unaligned.h"
#include "pic.h"
#include "reelmagic.h"
#include "render.h"
#include "rgb565.h"
#include "vga.h"
#include "video.h"

// #define DEBUG_VGA_DRAW

typedef uint8_t* (*VGA_Line_Handler)(Bitu vidstart, Bitu line);

static VGA_Line_Handler VGA_DrawLine;

// Confirm the maximum dimensions accomodate VGA's pixel and scan doubling
constexpr auto max_pixel_doubled_width = 512;
constexpr auto max_scan_doubled_height = 400;
static_assert(SCALER_MAXWIDTH >= SCALER_MAX_MUL_WIDTH * max_pixel_doubled_width);
static_assert(SCALER_MAXHEIGHT >= SCALER_MAX_MUL_HEIGHT * max_scan_doubled_height);

constexpr auto max_pixel_bytes = sizeof(uint32_t);
constexpr auto max_line_bytes  = SCALER_MAXWIDTH * max_pixel_bytes;

// The line buffer can be written in units up to RGB888 pixels (32-bit) size
alignas(uint32_t) static std::array<uint8_t, max_line_bytes> templine_buffer;
static auto TempLine = templine_buffer.data();

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

static uint8_t *Composite_Process(uint8_t border, uint32_t blocks, bool double_width)
{
	static int temp[SCALER_MAXWIDTH + 10] = {0};
	static int atemp[SCALER_MAXWIDTH + 2] = {0};
	static int btemp[SCALER_MAXWIDTH + 2] = {0};

	int w = blocks * 4;

	if (double_width) {
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

	if (vga.tandy.mode.is_black_and_white_mode) {
		// Decode
		int *i = temp + 5;
		uint16_t idx = 0;
		for (uint32_t x = 0; x < blocks * 4; ++x) {
			int c = (i[0] + i[0]) << 3;
			int d = (i[-1] + i[1]) << 3;
			int y = ((c + d) << 8) + vga.composite.sharpness * (c - d);
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

		auto composite_convert = [&](const int ii, const int q) {
			i[1]        = (i[1] << 3) - ap[1];
			const int c = i[0] + i[0];
			const int d = i[-1] + i[1];

			const int y = left_shift_signed(c + d, 8) +
			              vga.composite.sharpness * (c - d);

			const int rr = y + vga.composite.ri * (ii) +
			               vga.composite.rq * (q);

			const int gg = y + vga.composite.gi * (ii) +
			               vga.composite.gq * (q);

			const int bb = y + vga.composite.bi * (ii) +
			               vga.composite.bq * (q);
			++i;
			++ap;
			++bp;

			const auto srgb = (byte_clamp(rr) << 16) |
			                  (byte_clamp(gg) << 8) | byte_clamp(bb);

			write_unaligned_uint32_at(TempLine, idx++, srgb);
		};

		for (uint32_t x = 0; x < blocks; ++x) {
			composite_convert(ap[0], bp[0]);
			composite_convert(-bp[0], ap[0]);
			composite_convert(-ap[0], -bp[0]);
			composite_convert(bp[0], -ap[0]);
		}
	}
	return TempLine;
}

static uint8_t *VGA_TEXT_Draw_Line(Bitu vidstart, Bitu line);

static uint8_t *VGA_CGA_TEXT_Composite_Draw_Line(Bitu vidstart, Bitu line)
{
	VGA_TEXT_Draw_Line(vidstart, line);
	return Composite_Process(vga.tandy.color_select & 0x0f,
	                         vga.draw.blocks * 2,
	                         !vga.tandy.mode.is_high_bandwidth);
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
			if (((Bitu)ret) & (sizeof(Bitu) - 1)) {
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
	if ((vga.draw.line_length + offset) & ~vga.draw.linear_mask) {
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
	if (((Bitu)ret) & (sizeof(Bitu) - 1)) {
		memcpy( TempLine, ret, vga.draw.line_length );
		return TempLine;
	}
#endif
	return ret;
}

static uint8_t* draw_unwrapped_line_from_dac_palette(Bitu vidstart,
                                                     [[maybe_unused]] const Bitu line = 0)
{
	// Quick references
	static constexpr auto palette_map        = vga.dac.palette_map;
	static constexpr uint8_t bytes_per_pixel = sizeof(palette_map[0]);
	const auto linear_mask                   = vga.draw.linear_mask;
	const auto linear_addr                   = vga.draw.linear_base;

	// Video mode-specific line variables
	const auto pixels_in_line = static_cast<uint16_t>(vga.draw.line_length /
	                                                  bytes_per_pixel);
	const auto video_end = vidstart + pixels_in_line;

	// The line address is where the RGB888 palettized pixel is written.
	// It's incremented forward per pixel.
	auto line_addr = reinterpret_cast<uint32_t*>(TempLine);

	auto linear_pos = vidstart;

	// Draw in batches of four to let the host pipeline deeper.
	constexpr auto num_repeats = 4;
	assert(pixels_in_line % num_repeats == 0);

	// This function typically runs on 640+-wide lines and is a rendering
	// bottleneck.
	while (linear_pos < video_end) {
		auto repeats = num_repeats;
		while (repeats--) {
			const auto masked_pos    = linear_pos++ & linear_mask;
			const auto palette_index = *(linear_addr + masked_pos);
			*line_addr++ = *(palette_map + palette_index);
		}
	}

	return TempLine;
}

static uint8_t* draw_linear_line_from_dac_palette(Bitu vidstart, Bitu /*line*/)
{
	const auto offset                 = vidstart & vga.draw.linear_mask;
	constexpr auto palette_map        = vga.dac.palette_map;
	constexpr uint8_t bytes_per_pixel = sizeof(palette_map[0]);

	// The line address is where the RGB888 palettized pixel is written.
	// It's incremented forward per pixel.
	auto line_addr = TempLine;

	// The palette index iterator is used to lookup the DAC palette colour.
	// It starts at the current VGA line's offset and is incremented per
	// pixel.
	auto palette_index_it = vga.draw.linear_base + offset;

	// Pixels remaining starts as the total pixels in this current line and
	// is decremented per pixel rendered. It acts as a lower-bound cutoff
	// regardless of how long the wrapped and unwrapped regions are.
	auto pixels_remaining = check_cast<uint16_t>(vga.draw.line_length /
	                                             bytes_per_pixel);

	// If the screen is disabled, just paint black. This fixes screen
	// fades in titles like Alien Carnage.
	if (vga.seq.clocking_mode.is_screen_disabled) {
		memset(line_addr, 0, vga.draw.line_length);
		return TempLine;
	}

	// see VGA_Draw_Linear_Line
	if ((vga.draw.line_length + offset) & ~vga.draw.linear_mask) {
		// Note: To exercise these wrapped scenarios, run:
		// 1. Dangerous Dave: jump on the tree at the start.
		// 2. Commander Keen 4: move to left of the first hill on stage 1.

		const auto end = (vga.draw.line_length + offset) &
		                 vga.draw.linear_mask;

		// assuming lines not longer than 4096 pixels
		const auto wrapped_len   = static_cast<uint16_t>(end & 0xFFF);
		const auto unwrapped_len = check_cast<uint16_t>(
		        vga.draw.line_length - wrapped_len);

		// unwrapped chunk: to top of memory block
		auto palette_index_end = palette_index_it +
		                         std::min(unwrapped_len, pixels_remaining);
		while (palette_index_it != palette_index_end) {
			memcpy(line_addr,
			       palette_map + *palette_index_it++,
			       bytes_per_pixel);
			line_addr += bytes_per_pixel;
			--pixels_remaining;
		}

		// wrapped chunk: from the base of the memory block
		palette_index_it  = vga.draw.linear_base;
		palette_index_end = palette_index_it +
		                    std::min(wrapped_len, pixels_remaining);
		while (palette_index_it != palette_index_end) {
			memcpy(line_addr,
			       palette_map + *palette_index_it++,
			       bytes_per_pixel);
			line_addr += bytes_per_pixel;
			--pixels_remaining;
		}

	} else {
		auto palette_index_end = palette_index_it + pixels_remaining;
		while (palette_index_it != palette_index_end) {
			memcpy(line_addr,
			       palette_map + *palette_index_it++,
			       bytes_per_pixel);
			line_addr += bytes_per_pixel;
		}
	}
	return TempLine;
}

enum CursorOp : uint8_t {
	Foreground  = 0b10,
	Background  = 0b00,
	Transparent = 0b01,
	Invert      = 0b11,
};

static uint8_t* draw_unwrapped_line_from_dac_palette_with_hwcursor(Bitu vidstart, Bitu /*line*/)
{
	using namespace bit::literals;

	// Draw the underlying line without the cursor
	const auto line_addr = reinterpret_cast<uint32_t*>(
	        draw_unwrapped_line_from_dac_palette(vidstart));

	// Quick references to hardware cursor properties
	const auto& cursor = vga.s3.hgc;

	const auto line_at_y = (vidstart - (vga.config.real_start << 2)) /
	                       vga.draw.image_info.width;

	// Draw mouse cursor
	// ~~~~~~~~~~~~~~~~~
	// The cursor is a 64x64 pattern which is shifted (inside the 64x64 mouse
	// cursor space) to the right by posx pixels and up by posy pixels.
	//
	// This is used when the mouse cursor partially leaves the screen. It is
	// arranged as bitmap of 16bits of bit A followed by 16bits of bit B, each AB
	// bits corresponding to a cursor pixel. The whole map is 8kB in size.

	constexpr auto bitmap_width_bits   = 64;
	constexpr auto bitmap_last_y_index = 63u;

	// Is the mouse cursor pattern on this line?
	if (cursor.posx >= vga.draw.image_info.width || line_at_y < cursor.originy ||
	    line_at_y > (cursor.originy + (bitmap_last_y_index - cursor.posy))) {
		return reinterpret_cast<uint8_t*>(line_addr);
	}

	// the index of the bit inside the cursor bitmap we start at:
	const auto source_start_bit = ((line_at_y - cursor.originy) + cursor.posy) *
	                                      bitmap_width_bits + cursor.posx;
	const auto cursor_start_bit = source_start_bit & 0x7;
	uint8_t cursor_bit          = b7 >> cursor_start_bit;

	// Convert to video memory addr and bit index
	// start adjusted to the pattern structure (thus shift address by 2
	// instead of 3) Need to get rid of the third bit, so "/8 *2" becomes
	// ">> 2 & ~1"
	auto mem_start = ((source_start_bit >> 2) & ~1u) +
	                 (static_cast<uint32_t>(cursor.startaddr) << 10);

	// Stay at the right position in the pattern
	if (mem_start & 0x2) {
		--mem_start;
	}
	const auto mem_end = mem_start + ((bitmap_width_bits - cursor.posx) >> 2);

	constexpr uint8_t mem_delta[] = {1, 3};

	// Iterated for all bitmap positions
	auto cursor_addr = line_addr + cursor.originx;

	const auto fg_colour = *(vga.dac.palette_map + *cursor.forestack);
	const auto bg_colour = *(vga.dac.palette_map + *cursor.backstack);

	// for each byte of cursor data
	for (auto m = mem_start; m < mem_end; m += mem_delta[m & 1]) {
		const auto bits_a = vga.mem.linear[m];
		const auto bits_b = vga.mem.linear[m + 2];

		while (cursor_bit != 0) {
			uint8_t op = {};
			bit::set_to(op, b0, bit::is(bits_a, cursor_bit));
			bit::set_to(op, b1, bit::is(bits_b, cursor_bit));

			switch (static_cast<CursorOp>(op)) {
			case CursorOp::Foreground: *cursor_addr = fg_colour; break;
			case CursorOp::Background: *cursor_addr = bg_colour; break;
			case CursorOp::Invert:
				bit::flip_all(*cursor_addr);
				break;
			case CursorOp::Transparent: break;
			};
			cursor_addr++;
			cursor_bit >>= 1;
		}
		cursor_bit = b7;
	}
	return reinterpret_cast<uint8_t*>(line_addr);
}

static uint8_t * VGA_Draw_LIN16_Line_HWMouse(Bitu vidstart, Bitu /*line*/) {
	if (!svga.hardware_cursor_active || !svga.hardware_cursor_active())
		return &vga.mem.linear[vidstart];

	Bitu lineat = ((vidstart - (vga.config.real_start << 2)) >> 1) /
	              vga.draw.image_info.width;

	if ((vga.s3.hgc.posx >= vga.draw.image_info.width) ||
	    (lineat < vga.s3.hgc.originy) ||
	    (lineat > (vga.s3.hgc.originy + (63U - vga.s3.hgc.posy)))) {
		return &vga.mem.linear[vidstart];
	} else {
		memcpy(TempLine,
		       &vga.mem.linear[vidstart],
		       vga.draw.image_info.width * 2);

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

	Bitu lineat = ((vidstart - (vga.config.real_start << 2)) >> 2) /
	              vga.draw.image_info.width;

	if ((vga.s3.hgc.posx >= vga.draw.image_info.width) ||
	    (lineat < vga.s3.hgc.originy) ||
	    (lineat > (vga.s3.hgc.originy + (63U - vga.s3.hgc.posy)))) {
		return &vga.mem.linear[ vidstart ];
	} else {
		memcpy(TempLine,
		       &vga.mem.linear[vidstart],
		       vga.draw.image_info.width * 4);

		Bitu sourceStartBit = ((lineat - vga.s3.hgc.originy) + vga.s3.hgc.posy)*64 + vga.s3.hgc.posx;
		Bitu cursorMemStart = ((sourceStartBit >> 2)& ~1) + (((uint32_t)vga.s3.hgc.startaddr) << 10);
		Bitu cursorStartBit = sourceStartBit & 0x7;

		if (cursorMemStart & 0x2) {
			--cursorMemStart;
		}

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
	vidstart = vidstart & vga.draw.linear_mask;
	Bitu line_end = 2 * vga.draw.blocks;
	if ((vidstart + line_end) > vga.draw.linear_mask) {
		// wrapping in this line
		Bitu break_pos = (vga.draw.linear_mask - vidstart) + 1;
		// need a temporary storage - TempLine/2 is ok for a bit more than 132 columns
		memcpy(&TempLine[templine_buffer.size() / 2],
		       &vga.tandy.draw_base[vidstart],
		       break_pos);
		memcpy(&TempLine[templine_buffer.size() / 2 + break_pos],
		       &vga.tandy.draw_base[0],
		       line_end - break_pos);
		return &TempLine[templine_buffer.size() / 2];
	} else {
		return &vga.tandy.draw_base[vidstart];
	}
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
			if (underline) {
				mask1 = mask2 = FontMask[attrib >> 7];
			} else {
				Bitu font = vga.draw.font_tables[0][chr * 32 + line];
				mask1 = TXT_Font_Table[font >> 4] &
				        FontMask[attrib >> 7]; // blinking
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
// combined 8/9-dot wide text mode line drawing function
static uint8_t* draw_text_line_from_dac_palette(Bitu vidstart, Bitu line)
{
	// pointer to chars+attribs
	const uint8_t* vidmem  = VGA_Text_Memwrap(vidstart);
	const auto palette_map = vga.dac.palette_map;

	auto blocks = vga.draw.blocks;
	if (vga.draw.panning) {
		++blocks; // if the text is panned part of an
		          // additional character becomes visible
	}

	// The first write-index into the draw buffer. Increasing this shifts
	// the console text right (and vice-versa)
	const uint16_t draw_idx_start = 8 + vga.draw.panning;

	// This holds the to-be-written pixel offset, and is incremented per
	// pixel and also per character block.
	auto draw_idx = draw_idx_start;

	while (blocks--) { // for each character in the line
		const auto chr  = *vidmem++;
		const auto attr = *vidmem++;
		// the font pattern
		uint16_t font = vga.draw.font_tables[(attr >> 3) & 1][(chr << 5) + line];

		uint8_t bg_palette_idx = attr >> 4;
		// if blinking is enabled bit7 is not mapped to attributes
		if (vga.draw.blinking) {
			bg_palette_idx &= ~0x8;
		}
		// choose foreground color if blinking not set for this cell or
		// blink on
		const uint8_t fg_palette_idx = (vga.draw.blink || (attr & 0x80) == 0)
		                                     ? (attr & 0xf)
		                                     : bg_palette_idx;

		// underline: all foreground [freevga: 0x77, previous 0x7]
		if (((attr & 0x77) == 0x01) &&
		    (vga.crtc.underline_location & 0x1f) == line) {
			bg_palette_idx = fg_palette_idx;
		}

		// The font's bits will indicate which color is used per pixel
		const auto fg_colour = palette_map[fg_palette_idx];
		const auto bg_colour = palette_map[bg_palette_idx];

		if (vga.seq.clocking_mode.is_eight_dot_mode) {
			for (auto n = 0; n < 8; ++n) {
				const auto color = (font & 0x80) ? fg_colour
				                                 : bg_colour;
				write_unaligned_uint32_at(TempLine, draw_idx++, color);
				font <<= 1;
			}
		} else {
			font <<= 1; // 9 pixels
			// Extend to the 9th pixel if needed
			if ((font & 0x2) &&
			    vga.attr.mode_control.is_line_graphics_enabled &&
			    (chr >= 0xc0) && (chr <= 0xdf)) {
				font |= 1;
			}
			for (auto n = 0; n < 9; ++n) {
				const auto color = (font & 0x100) ? fg_colour
				                                  : bg_colour;
				write_unaligned_uint32_at(TempLine, draw_idx++, color);
				font <<= 1;
			}
		}
	}
	// draw the text mode cursor if needed
	if (!SkipCursor(vidstart, line)) {
		// the adress of the attribute that makes up the cell the cursor is in
		const auto attr_addr = check_cast<uint16_t>(
		        (vga.draw.cursor.address - vidstart) >> 1);
		if (attr_addr < vga.draw.blocks) {
			const auto fg_palette_idx =
			        vga.tandy.draw_base[vga.draw.cursor.address + 1] & 0xf;
			const auto fg_colour = palette_map[fg_palette_idx];

			constexpr auto bytes_per_pixel = sizeof(fg_colour);

			// The cursor block's byte-offset into the rendering buffer.
			const auto cursor_draw_offset = check_cast<uint16_t>(
			        attr_addr * vga.draw.pixels_per_character * bytes_per_pixel);

			auto draw_addr = &TempLine[cursor_draw_offset];

			draw_idx = draw_idx_start;
			for (uint8_t n = 0; n < 8; ++n) {
				write_unaligned_uint32_at(draw_addr, draw_idx++, fg_colour);
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

static void VGA_ProcessSplit()
{
	if (vga.attr.mode_control.is_pixel_panning_enabled) {
		vga.draw.address = 0;
		// reset panning to 0 here so we don't have to check for
		// it in the character draw functions. It will be set back
		// to its proper value in v-retrace
		vga.draw.panning = 0;
	} else {
		// In text mode only the characters are shifted by panning, not
		// the address; this is done in the text line draw function.
		vga.draw.address = vga.draw.byte_panning_shift * vga.draw.bytes_skip;
		if ((vga.mode != M_TEXT) && !is_machine_ega()) {
			vga.draw.address += vga.draw.panning;
		}
	}
	vga.draw.address_line = 0;
}

static uint8_t bg_color_index = 0; // screen-off black index
static void VGA_DrawSingleLine(uint32_t /*blah*/)
{
	if (vga.attr.disabled) {
		switch(machine) {
		case MachineType::Pcjr:
			// Displays the border color when screen is disabled
			bg_color_index = vga.tandy.border_color;
			break;
		case MachineType::Tandy:
			// Either the PCJr way or the CGA way
			if (vga.tandy.mode_control.is_tandy_border_enabled) {
				bg_color_index = vga.tandy.border_color;
			} else if (vga.mode==M_TANDY4)
				bg_color_index = vga.attr.palette[0];
			else bg_color_index = 0;
			break;
		case MachineType::CgaMono:
		case MachineType::CgaColor:
			// the background color
			bg_color_index = vga.attr.overscan_color;
			break;
		case MachineType::Ega:
		case MachineType::Vga:
			// DoWhackaDo, Alien Carnage, TV sports Football
			// when disabled by attribute index bit 5:
			//  ET3000, ET4000, Paradise display the border color
			//  S3 displays the content of the currently selected attribute register
			// when disabled by sequencer the screen is black "257th color"

			// the DAC table may not match the bits of the overscan register
			// so use black for this case too...
			//if (vga.attr.disabled& 2) {
			if (constexpr uint32_t black_rgb888 = 0;
			    vga.dac.palette_map[bg_color_index] != black_rgb888) {
				// check some assumptions about the palette map
				constexpr auto palette_map_len = ARRAY_LEN(
				        vga.dac.palette_map);
				static_assert(palette_map_len == 256,
				              "The code below assumes the table is 256 elements long");

				for (uint16_t i = 0; i < palette_map_len; ++i)
					if (vga.dac.palette_map[i] == black_rgb888) {
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
		if (vga.draw.image_info.pixel_format == PixelFormat::Indexed8) {
			std::fill(templine_buffer.begin(),
			          templine_buffer.end(),
			          bg_color_index);

		} else if (vga.draw.image_info.pixel_format == PixelFormat::RGB565_Packed16) {
			// convert the palette colour to a 16-bit value for writing
			const auto bg_pal_color = vga.dac.palette_map[bg_color_index];
			const auto bg_565_pixel = Rgb565(bg_pal_color.Red8(),
			                                 bg_pal_color.Green8(),
			                                 bg_pal_color.Blue8())
			                                  .pixel;

			const auto line_length = templine_buffer.size() /
			                         sizeof(uint16_t);
			size_t i = 0;
			while (i < line_length) {
				write_unaligned_uint16_at(TempLine, i++, bg_565_pixel);
			}
		} else if (vga.draw.image_info.pixel_format ==
		           PixelFormat::BGRX32_ByteArray) {
			const auto background_color = vga.dac.palette_map[bg_color_index];
			const auto line_length = templine_buffer.size() / sizeof(uint32_t);
			size_t i = 0;
			while (i < line_length) {
				write_unaligned_uint32_at(TempLine, i++, background_color);
			}
		}
		ReelMagic_RENDER_DrawLine(TempLine);
	} else {
		uint8_t * data=VGA_DrawLine( vga.draw.address, vga.draw.address_line );
		ReelMagic_RENDER_DrawLine(data);
	}

	++vga.draw.address_line;
	if (vga.draw.address_line>=vga.draw.address_line_total) {
		vga.draw.address_line=0;
		vga.draw.address+=vga.draw.address_add;
	}
	++vga.draw.lines_done;
	if (vga.draw.split_line==vga.draw.lines_done) VGA_ProcessSplit();
	if (vga.draw.lines_done < vga.draw.lines_total) {
		PIC_AddEvent(VGA_DrawSingleLine, vga.draw.delay.per_line_ms);
	} else RENDER_EndUpdate(false);
}

static void VGA_DrawEGASingleLine(uint32_t /*blah*/)
{
	if (vga.attr.disabled) {
		std::fill(templine_buffer.begin(), templine_buffer.end(), 0);
		ReelMagic_RENDER_DrawLine(TempLine);
	} else {
		Bitu address = vga.draw.address;
		if (vga.mode!=M_TEXT) address += vga.draw.panning;
		uint8_t * data=VGA_DrawLine(address, vga.draw.address_line );
		ReelMagic_RENDER_DrawLine(data);
	}

	++vga.draw.address_line;
	if (vga.draw.address_line>=vga.draw.address_line_total) {
		vga.draw.address_line=0;
		vga.draw.address+=vga.draw.address_add;
	}
	++vga.draw.lines_done;
	if (vga.draw.split_line==vga.draw.lines_done) VGA_ProcessSplit();
	if (vga.draw.lines_done < vga.draw.lines_total) {
		PIC_AddEvent(VGA_DrawEGASingleLine, vga.draw.delay.per_line_ms);
	} else RENDER_EndUpdate(false);
}

static void VGA_DrawPart(uint32_t lines)
{
	while (lines--) {
		uint8_t * data=VGA_DrawLine( vga.draw.address, vga.draw.address_line );
		ReelMagic_RENDER_DrawLine(data);
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
		vga.attr.mode_control.is_blink_enabled = 1;
		vga.tandy.mode.is_tandy_blink_enabled  = 1;
	} else {
		vga.draw.blinking                      = 0;
		vga.attr.mode_control.is_blink_enabled = 0;
		vga.tandy.mode.is_tandy_blink_enabled  = 0;
	}
	const uint8_t b = (enabled ? 0 : 8);
	for (uint8_t i = 0; i < 8; ++i) {
		TXT_BG_Table[i + 8] = (b + i) | ((b + i) << 8) |
		                      ((b + i) << 16) | ((b + i) << 24);
	}
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
		if (is_machine_ega()) {
			PIC_ActivateIRQ(9);
		}
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
	case MachineType::Pcjr:
	case MachineType::Tandy:
		// PCJr: Vsync is directly connected to the IRQ controller
		// Some earlier Tandy models are said to have a vsync interrupt too
		PIC_AddEvent(VGA_Other_VertInterrupt, vga.draw.delay.vrstart, 1);
		PIC_AddEvent(VGA_Other_VertInterrupt, vga.draw.delay.vrend, 0);
		// fall-through
	case MachineType::Hercules:
	case MachineType::CgaMono:
	case MachineType::CgaColor:
		// MC6845-powered graphics: Loading the display start latch
		// happens somewhere after vsync off and before first visible
		// scanline, so probably here
		VGA_DisplayStartLatch(0);
		break;
	case MachineType::Vga:
		PIC_AddEvent(VGA_DisplayStartLatch, vga.draw.delay.vrstart);
		PIC_AddEvent(VGA_PanningLatch, vga.draw.delay.vrend);
		// EGA: 82c435 datasheet: interrupt happens at display end
		// VGA: checked with scope; however disabled by default by
		// jumper on VGA boards add a little amount of time to make sure
		// the last drawpart has already fired
		PIC_AddEvent(VGA_VertInterrupt, vga.draw.delay.vdend + 0.005);
		break;
	case MachineType::Ega:
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
	if (vga.draw.vga_override || !ReelMagic_RENDER_StartUpdate()) {
		return;
	}

	vga.draw.address_line = vga.config.hlines_skip;
	if (is_machine_ega_or_better()) {
		vga.draw.split_line = (vga.config.line_compare + 1) / vga.draw.lines_scaled;
		if ((svga_type == SvgaType::S3) && (vga.config.line_compare == 0)) {
			vga.draw.split_line = 0;
		}
		vga.draw.split_line -= vga.draw.vblank_skip;
	} else {
		vga.draw.split_line = 0x10000;	// don't care
	}
	vga.draw.address = vga.config.real_start;
	vga.draw.byte_panning_shift = 0;
	// go figure...
	if (is_machine_ega()) {
		if (vga.draw.image_info.double_height) { // Spacepigs EGA Megademo
			vga.draw.split_line *= 2;
		}
		++vga.draw.split_line; // EGA adds one buggy scanline
	}
//	if (is_machine_ega()) vga.draw.split_line = ((((vga.config.line_compare&0x5ff)+1)*2-1)/vga.draw.lines_scaled);
#ifdef VGA_KEEP_CHANGES
	bool startaddr_changed=false;
#endif
	switch (vga.mode) {
	case M_EGA:
		if (!(vga.crtc.mode_control.map_display_address_13)) {
			vga.draw.linear_mask &= ~0x10000;
		} else {
			vga.draw.linear_mask |= 0x10000;
		}
		[[fallthrough]];
	case M_LIN4:
		vga.draw.byte_panning_shift = 8;
		vga.draw.address += vga.draw.bytes_skip;
		vga.draw.address *= vga.draw.byte_panning_shift;
		if (!is_machine_ega()) {
			vga.draw.address += vga.draw.panning;
		}
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
		if (is_machine_hercules()) {
			vga.draw.linear_mask = 0xfff; // 1 page
		} else if (is_machine_ega_or_better()) {
			vga.draw.linear_mask = 0x7fff; // 8 pages
		} else {
			vga.draw.linear_mask = 0x3fff; // CGA, Tandy 4 pages
		}
		vga.draw.cursor.address=vga.config.cursor_start*2;
		vga.draw.address *= 2;

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
	if (vga.draw.split_line == 0) {
		VGA_ProcessSplit();
	}
#ifdef VGA_KEEP_CHANGES
	if (startaddr_changed) VGA_ChangesStart();
#endif

	// check if some lines at the top off the screen are blanked
	double draw_skip = 0.0;
	if (vga.draw.vblank_skip) {
		draw_skip = vga.draw.delay.htotal * static_cast<double>(vga.draw.vblank_skip);
		vga.draw.address += vga.draw.address_add *
		                    (vga.draw.vblank_skip /
		                     vga.draw.address_line_total);
	}

	// add the draw event
	switch (vga.draw.mode) {
	case PART:
		if (vga.draw.parts_left) {
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
		if (vga.draw.lines_done < vga.draw.lines_total) {
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
			             vga.draw.delay.per_line_ms + draw_skip);
		else
			PIC_AddEvent(VGA_DrawSingleLine,
			             vga.draw.delay.per_line_ms + draw_skip);
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
		if (is_machine_pcjr()) {
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

// If the hardware mouse cursor is activated, this function changes the VGA line
// drawing function-pointers to call the more complicated hardware cusror
// routines (for the given color depth).
//
// If the hardware cursor isn't activated, the simply fallback to the normal
// line-drawing routines for a given bit-depth.
//
// Finally, return the current mode's bits per line buffer value.
PixelFormat VGA_ActivateHardwareCursor()
{
	PixelFormat pixel_format = {};

	const bool use_hw_cursor = (svga.hardware_cursor_active &&
	                            svga.hardware_cursor_active());

	switch (vga.mode) {
	case M_LIN32: // 32-bit true-colour VESA
		pixel_format = PixelFormat::BGRX32_ByteArray;

		VGA_DrawLine = use_hw_cursor ? VGA_Draw_LIN32_Line_HWMouse
		                             : VGA_Draw_Linear_Line;

		// Use the "VGA_Draw_Linear_Line" routine that skips the DAC
		// 8-bit palette LUT and prepares the true-colour pixels for
		// rendering.
		break;
	case M_LIN24: // 24-bit true-colour VESA
		pixel_format = PixelFormat::BGR24_ByteArray;

		VGA_DrawLine = use_hw_cursor ? VGA_Draw_LIN32_Line_HWMouse
		                             : VGA_Draw_Linear_Line;
		break;
	case M_LIN16: // 16-bit high-colour VESA
		pixel_format = PixelFormat::RGB565_Packed16;

		VGA_DrawLine = use_hw_cursor ? VGA_Draw_LIN16_Line_HWMouse
		                             : VGA_Draw_Linear_Line;
		break;
	case M_LIN15: // 15-bit high-colour VESA
		pixel_format = PixelFormat::RGB555_Packed16;

		VGA_DrawLine = use_hw_cursor ? VGA_Draw_LIN16_Line_HWMouse
		                             : VGA_Draw_Linear_Line;
		break;
	case M_LIN8: // 8-bit and below
	default:
		pixel_format = PixelFormat::BGRX32_ByteArray;

		// Use routines that treats the 8-bit pixel values as
		// indexes into the DAC's palette LUT. The palette LUT
		// is populated with 32-bit RGB's (XRGB888) pre-scaled
		// from 18-bit RGB666 values that have been written by
		// the user-space software via DAC IO write registers.
		//
		VGA_DrawLine = use_hw_cursor
		                     ? draw_unwrapped_line_from_dac_palette_with_hwcursor
		                     : draw_unwrapped_line_from_dac_palette;
		break;
	}
	return pixel_format;
}

// A single point to set total drawn lines and update affected delay values
static void setup_line_drawing_delays(const uint32_t total_lines)
{
	const auto conf    = control->GetSection("dosbox");
	const auto section = static_cast<SectionProp*>(conf);
	assert(section);

	if (vga.draw.mode == PART && !section->GetBool("vga_render_per_scanline")) {
		// Render the screen in 4 parts; this was the legacy DOSBox behaviour.
		// A few games needs this (e.g., Deus, Ishar 3, Robinson's Requiem,
		// Time Travelers) and would crash at startup with per-scanline
		// rendering enabled. This is most likely due to some VGA emulation
		// deficiency.
		vga.draw.parts_total = 4;
	} else {
		vga.draw.parts_total = total_lines;
	}

	vga.draw.delay.parts = vga.draw.delay.vdend / vga.draw.parts_total;

	assert(total_lines > 0 && total_lines <= SCALER_MAXHEIGHT);
	vga.draw.lines_total = total_lines;

	assert(vga.draw.parts_total > 0);
	vga.draw.parts_lines = total_lines / vga.draw.parts_total;

	assert(vga.draw.delay.vdend > 0.0);
	vga.draw.delay.per_line_ms = vga.draw.delay.vdend / total_lines;
}

// Determines pixel size as a pair of fractions (width and height)
static std::pair<Fraction, Fraction> determine_pixel_size(const uint32_t htotal,
                                                          const uint32_t vtotal)
{
	// bit 6 - Horizontal Sync Polarity. Negative if set
	// bit 7 - Vertical Sync Polarity. Negative if set
	//
	// Bits 6-7 indicate the number of displayed lines:
	//   1: 400, 2: 350, 3: 480
	auto horiz_sync_polarity = vga.misc_output >> 6;

	// TODO We do default to 9-pixel characters in standard VGA text modes,
	// meaning the below original assumptions might be incorrect for VGA
	// text modes:
	//
	// Base pixel width around 100 clocks horizontal
	// For 9 pixel text modes this should be changed, but we don't support
	// that anyway :) Seems regular vga only listens to the 9 char pixel
	// mode with character mode enabled
	const Fraction pwidth = {100, htotal};

	// Base pixel height around vertical totals of modes that have 100
	// clocks horizontally. Different sync values gives different scaling of
	// the whole vertical range; VGA monitors just seems to thighten or
	// widen the whole vertical range.
	Fraction pheight      = {};
	uint16_t target_total = 449;

	switch (horiz_sync_polarity) {
	case 0: // 340-line mode, filled with 449 total lines
		// This is not defined in vga specs,
		// Kiet, seems to be slightly less than 350 on my monitor

		pheight = Fraction(480, 340) * Fraction(target_total, vtotal);
		break;

	case 1: // 400-line mode, filled with 449 total lines
		pheight = Fraction(480, 400) * Fraction(target_total, vtotal);
		break;

	case 2: // 350-line mode, filled with 449 total lines
		// This mode seems to get regular 640x400 timings and goes for a
		// long retrace. Depends on the monitor to stretch the image.
		pheight = Fraction(480, 350) * Fraction(target_total, vtotal);
		break;

	case 3: // 480-line mode, filled with 525 total lines
	default:
		// TODO This seems like an arbitrary hack and should probably be
		// removed in the future. But it should make much of a difference in
		// the grand scheme of things, so leaving it alone for now.
		//
		// Allow 527 total lines ModeX modes to have exact 1:1 aspect
		// ratio.
		target_total = (vga.mode == M_VGA && vtotal == 527) ? 527 : 525;
		pheight = Fraction(480, 480) * Fraction(target_total, vtotal);
		break;
	}

	return {pwidth, pheight};
}

struct DisplayTimings {
	uint32_t total          = 0;
	uint32_t display_end    = 0;
	uint32_t blanking_start = 0;
	uint32_t blanking_end   = 0;
	uint32_t retrace_start  = 0;
	uint32_t retrace_end    = 0;
};

struct VgaTimings {
	uint32_t clock        = 0;
	DisplayTimings horiz  = {};
	DisplayTimings vert   = {};
};

// This function reads various VGA registers to calculate the display timings,
// but does not modify any of them as a side-effect.
static VgaTimings calculate_vga_timings()
{
	uint32_t clock       = 0;
	DisplayTimings horiz = {};
	DisplayTimings vert  = {};

	if (is_machine_ega_or_better()) {
		horiz.total          = vga.crtc.horizontal_total;
		horiz.display_end    = vga.crtc.horizontal_display_end;
		horiz.blanking_end   = vga.crtc.end_horizontal_blanking & 0x1F;
		horiz.blanking_start = vga.crtc.start_horizontal_blanking;
		horiz.retrace_start  = vga.crtc.start_horizontal_retrace;

		vert.total = vga.crtc.vertical_total |
		             ((vga.crtc.overflow & 1) << 8);

		vert.display_end = vga.crtc.vertical_display_end |
		                   ((vga.crtc.overflow & 2) << 7);

		vert.blanking_start = vga.crtc.start_vertical_blanking |
		                      ((vga.crtc.overflow & 0x08) << 5);

		vert.retrace_start = vga.crtc.vertical_retrace_start +
		                     ((vga.crtc.overflow & 0x04) << 6);

		if (is_machine_vga_or_better()) {
			// Additional bits only present on VGA cards
			horiz.total |= (vga.s3.ex_hor_overflow & 0x1) << 8;
			horiz.total += 3;
			horiz.display_end |= (vga.s3.ex_hor_overflow & 0x2) << 7;
			horiz.blanking_end |= (vga.crtc.end_horizontal_retrace & 0x80) >> 2;
			horiz.blanking_start |= (vga.s3.ex_hor_overflow & 0x4) << 6;
			horiz.retrace_start |= (vga.s3.ex_hor_overflow & 0x10) << 4;

			vert.total |= (vga.crtc.overflow & 0x20) << 4;
			vert.total |= (vga.s3.ex_ver_overflow & 0x1) << 10;
			vert.display_end |= (vga.crtc.overflow & 0x40) << 3;
			vert.display_end |= (vga.s3.ex_ver_overflow & 0x2) << 9;
			vert.blanking_start |= vga.crtc.maximum_scan_line.start_vertical_blanking_bit9 << 4;
			vert.blanking_start |= (vga.s3.ex_ver_overflow & 0x4) << 8;
			vert.retrace_start |= ((vga.crtc.overflow & 0x80) << 2);
			vert.retrace_start |= (vga.s3.ex_ver_overflow & 0x10) << 6;
			vert.blanking_end = vga.crtc.end_vertical_blanking & 0x7f;
		} else { // EGA
			vert.blanking_end = vga.crtc.end_vertical_blanking & 0x1f;
		}

		horiz.total += 2;
		vert.total += 2;
		horiz.display_end += 1;
		vert.display_end += 1;

		horiz.blanking_end = horiz.blanking_start +
		                     ((horiz.blanking_end - horiz.blanking_start) &
		                      0x3F);
		horiz.retrace_end = vga.crtc.end_horizontal_retrace & 0x1f;
		horiz.retrace_end = (horiz.retrace_end - horiz.retrace_start) & 0x1f;

		if (!horiz.retrace_end) {
			horiz.retrace_end = horiz.retrace_start + 0x1f + 1;
		} else {
			horiz.retrace_end = horiz.retrace_start + horiz.retrace_end;
		}

		vert.retrace_end = vga.crtc.vertical_retrace_end & 0xF;
		vert.retrace_end = (vert.retrace_end - vert.retrace_start) & 0xF;

		if (!vert.retrace_end) {
			vert.retrace_end = vert.retrace_start + 0xf + 1;
		} else {
			vert.retrace_end = vert.retrace_start + vert.retrace_end;
		}

		// Special case for vert.blanking_start == 0:
		// Most graphics cards agree that lines zero to vertical blanking end
		// are blanked.
		// Tsend ET4000 doesn't blank at all if vert.blanking_start == vert.blanking_end.
		// ET3000 blanks lines 1 to vert.blanking_end (255/6 lines).
		if (vert.blanking_start != 0) {
			vert.blanking_start += 1;
			vert.blanking_end = (vert.blanking_end - vert.blanking_start) & 0x7f;
			if (!vert.blanking_end) {
				vert.blanking_end = vert.blanking_start + 0x7f + 1;
			} else {
				vert.blanking_end = vert.blanking_start + vert.blanking_end;
			}
		}
		vert.blanking_end++;

		if (svga.get_clock) {
			clock = svga.get_clock();
		} else {
			switch ((vga.misc_output >> 2) & 3) {
			case 0:
				clock = is_machine_ega() ? CgaPixelClockHz : Vga640PixelClockHz;
				break;
			case 1:
			default:
				clock = is_machine_ega() ? EgaPixelClockHz : Vga720PixelClockHz;
				break;
			}
		}

		// Adjust the VGA clock frequency based on the Clocking Mode
		// Register's 9/8 Dot Mode. See Timing Model:
		// https://wiki.osdev.org/VGA_Hardware
		clock /= vga.seq.clocking_mode.is_eight_dot_mode
		               ? PixelsPerChar::Eight
		               : PixelsPerChar::Nine;

		// Adjust the horizontal frequency if in pixel doubling mode
		// (clock/2)
		if (vga.seq.clocking_mode.is_pixel_doubling) {
			horiz.total *= 2;
		}

	} else {
		horiz.total          = vga.other.htotal + 1;
		horiz.display_end    = vga.other.hdend;
		horiz.blanking_start = horiz.display_end;
		horiz.blanking_end   = horiz.total;
		horiz.retrace_start  = vga.other.hsyncp;
		horiz.retrace_end    = horiz.retrace_start + vga.other.hsyncw;

		vert.total = vga.draw.address_line_total * (vga.other.vtotal + 1) +
		             vga.other.vadjust;

		vert.display_end = vga.draw.address_line_total * vga.other.vdend;
		vert.retrace_start = vga.draw.address_line_total * vga.other.vsyncp;
		vert.retrace_end = vert.retrace_start + 16; // vsync width is
		                                            // fixed to 16 lines
		                                            // on the MC6845
		                                            // TODO Tandy
		vert.blanking_start = vert.display_end;
		vert.blanking_end   = vert.total;

		switch (machine) {
		case MachineType::CgaMono:
		case MachineType::CgaColor:
		case MachineType::Pcjr:
		case MachineType::Tandy:
			clock = ((vga.tandy.mode.is_high_bandwidth)
			                 ? CgaPixelClockHz
			                 : (CgaPixelClockHz / 2)) /
			        8;
			break;
		case MachineType::Hercules:
			if (vga.herc.mode_control & 0x2) {
				clock = 16000000 / 16;
			} else {
				clock = 16000000 / 8;
			}
			break;
		default: clock = CgaPixelClockHz; break;
		}
	}

	// LOG_MSG("VGA: h total %u end %u blank (%u/%u) retrace (%u/%u)",
	//         horiz.total,
	//         horiz.display_end,
	//         horiz.blanking_start,
	//         horiz.blanking_end,
	//         horiz.retrace_start,
	//         horiz.retrace_end);

	// LOG_MSG("VGA: v total %u end %u blank (%u/%u) retrace (%u/%u)",
	//         vert.total,
	//         vert.display_end,
	//         vert.blanking_start,
	//         vert.blanking_end,
	//         vert.retrace_start,
	//         vert.retrace_end);

	return {clock, horiz, vert};
}

struct UpdatedTimings {
	uint32_t horiz_display_end = 0;
	uint32_t vert_display_end  = 0;
	uint32_t vblank_skip       = 0;
};

static UpdatedTimings update_vga_timings(const VgaTimings& timings)
{
	const auto vert  = timings.vert;
	const auto horiz = timings.horiz;

	const auto fps     = VGA_GetRefreshRate();
	const auto f_clock = fps * vert.total * horiz.total;

	// Horizontal total (that's how long a line takes with whistles and bells)
	vga.draw.delay.htotal = static_cast<double>(horiz.total) * 1000.0 /
	                        f_clock; //  milliseconds
									 //
	// Start and End of horizontal blanking
	vga.draw.delay.hblkstart = static_cast<double>(horiz.blanking_start) *
	                           1000.0 / f_clock; //  milliseconds
	vga.draw.delay.hblkend = static_cast<double>(horiz.blanking_end) *
	                         1000.0 / f_clock;

	// Start and End of horizontal retrace
	vga.draw.delay.hrstart = static_cast<double>(horiz.retrace_start) *
	                         1000.0 / f_clock;
	vga.draw.delay.hrend = static_cast<double>(horiz.retrace_end) * 1000.0 /
	                       f_clock;

	// Start and End of vertical blanking
	vga.draw.delay.vblkstart = static_cast<double>(vert.blanking_start) *
	                           vga.draw.delay.htotal;
	vga.draw.delay.vblkend = static_cast<double>(vert.blanking_end) *
	                         vga.draw.delay.htotal;

	// Start and End of vertical retrace pulse
	vga.draw.delay.vrstart = static_cast<double>(vert.retrace_start) *
	                         vga.draw.delay.htotal;
	vga.draw.delay.vrend = static_cast<double>(vert.retrace_end) *
	                       vga.draw.delay.htotal;

	// Vertical blanking tricks
	auto vert_display_end  = vert.display_end;
	auto horiz_display_end = horiz.display_end;

	uint32_t vblank_skip = 0;
	if (is_machine_vga_or_better()) {
		// Others need more investigation
		if (vert.blanking_start < vert.total) {
			// There will be no blanking at all otherwise
			//
			if (vert.blanking_end > vert.total) {
				// blanking wraps to the start of the screen
				vblank_skip = vert.blanking_end & 0x7f;

				// On blanking wrap to 0, the first line is not blanked this
				// is used by the S3 BIOS and other S3 drivers in some SVGA
				// modes
				if ((vert.blanking_end & 0x7f) == 1) {
					vblank_skip = 0;
				}

				// It might also cut some lines off the bottom
				if (vert.blanking_start < vert.display_end) {
					vert_display_end = vert.blanking_start;
				}
				LOG(LOG_VGA, LOG_WARN)
				("Blanking wrap to line %u", vblank_skip);

			} else if (vert.blanking_start <= 1) {
				// Blanking is used to cut lines at the start of the screen
				vblank_skip = vert.blanking_end;
				LOG(LOG_VGA, LOG_WARN)
				("Upper %u lines of the screen blanked", vblank_skip);

			} else if (vert.blanking_start < vert.display_end) {
				if (vert.blanking_end < vert.display_end) {
					// The game wants a black bar somewhere on the screen
					LOG(LOG_VGA, LOG_WARN)
					("Unsupported blanking: line %u-%u",
					 vert.blanking_start,
					 vert.blanking_end);
				} else {
					// Blanking is used to cut off some
					// lines from the bottom
					vert_display_end = vert.blanking_start;
				}
			}
			vert_display_end -= vblank_skip;
		}
	}
	// Display end
	vga.draw.delay.vdend = static_cast<double>(vert_display_end) *
	                       vga.draw.delay.htotal;

	// Check to prevent useless black areas
	if (horiz.blanking_start < horiz.display_end) {
		horiz_display_end = horiz.blanking_start;
	}
	if (!is_machine_vga_or_better() && (vert.blanking_start < vert_display_end)) {
		vert_display_end = vert.blanking_start;
	}

	return {horiz_display_end, vert_display_end, vblank_skip};
}

static bool is_vga_scan_doubling_bit_set()
{
	// Scan doubling on VGA can be achieved in two ways:
	//
	// 1) 16-colour VGA modes and all CGA, EGA and VESA modes that require
	//    scan doubling set the Scan Doubling bit of the Maximum Scanline
	//    Register to 1. This method works with text modes too.
	//
	// 2) Mode 13h (320x200 256-colours) and its endless tweak-mode variants
	//    (e.g., 320x240, 320x400, 360x240, 256x256, 320x191, etc.) set the
	//    Maximum Scan Line value of the Maximum Scan Line register to 1 (0
	//    means no line doubling, 1 means line doubling, 2 tripling, 3
	//    quadrupling, and so on).
	//
	//    Note this value is used for a different purpose in text modes (it
	//    contains the height of the character cell in pixels minus 1), so
	//    this second method only works in graphics modes.
	//
	// We're only checking for method #1 here.
	//
	return is_machine_vga_or_better() &&
	       vga.crtc.maximum_scan_line.is_scan_doubling_enabled;
}

static constexpr auto display_aspect_ratio = Fraction(4, 3);

static Fraction calc_pixel_aspect_from_dimensions(const uint16_t width,
                                                  const uint16_t height,
                                                  const bool double_width,
                                                  const bool double_height)
{
	const auto storage_aspect_ratio =
	        Fraction(static_cast<int64_t>(width) * (double_width ? 2 : 1),
	                 static_cast<int64_t>(height) * (double_height ? 2 : 1));

	return display_aspect_ratio / storage_aspect_ratio;
}

static Fraction calc_pixel_aspect_from_timings(const VgaTimings& timings)
{
	const auto [pwidth, pheight] = determine_pixel_size(timings.horiz.total,
	                                                    timings.vert.total);
	return {pwidth / pheight};
}

constexpr auto pixel_aspect_1280x1024 = Fraction(4, 3) / Fraction(1280, 1024);

// Can change the following VGA state:
//
//   vga.draw.address_line_total
//   vga.draw.blocks
//   vga.draw.delay.hdend
//   vga.draw.dos_refresh_hz
//   vga.draw.line_length
//   vga.draw.linear_base
//   vga.draw.linear_mask
//   vga.draw.mode
//   vga.draw.resizing
//   vga.draw.uses_vga_palette
//   vga.draw.vblank_skip
//   vga.draw.vret_triggered
//
ImageInfo setup_drawing()
{
	// Set the drawing mode
	switch (machine) {
	case MachineType::CgaMono:
	case MachineType::CgaColor:
	case MachineType::Pcjr:
	case MachineType::Tandy:
		vga.draw.mode = DRAWLINE;
		break;
	case MachineType::Ega:
		// Paradise SVGA uses the same panning mechanism as EGA
		vga.draw.mode = EGALINE;
		break;
	case MachineType::Vga:
	default:
		vga.draw.mode = PART;
		break;
	}

	// We need to set the address_line_total according to the scan doubling
	// state on VGA before calling calculate_vga_timings(). Then we can
	// divide address_line_total later if we decide to do "fake"
	// scan doubling only.
	if (is_machine_ega_or_better()) {
		vga.draw.address_line_total = vga.crtc.maximum_scan_line.maximum_scan_line +
		                              1;
	} else {
		vga.draw.address_line_total = vga.other.max_scanline + 1;
	}

	// We always start from disabled then set the flag to true later if needed
	vga.draw.is_double_scanning = false;

	const auto vga_timings = calculate_vga_timings();

	if (is_vga_scan_doubling_bit_set()) {
		const auto fake_double_scanned_mode = (vga.mode == M_CGA2 ||
		                                       vga.mode == M_CGA4 ||
		                                       vga.mode == M_TEXT);
		if (!fake_double_scanned_mode) {
			vga.draw.address_line_total *= 2;
		}
	}

	if (!is_machine_ega_or_better()) {
		// in milliseconds
		vga.draw.delay.hdend = static_cast<double>(
		                               vga_timings.horiz.display_end) *
		                       1000.0 /
		                       static_cast<double>(vga_timings.clock);
	}

	// The screen refresh frequency and clock settings, per the DOS-mode
	vga.draw.dos_refresh_hz = static_cast<double>(vga_timings.clock) /
	                          (vga_timings.vert.total * vga_timings.horiz.total);

	const auto updated_timings = update_vga_timings(vga_timings);

	// EGA frequency dependent monitor palette
	if (is_machine_ega()) {
		if (vga.misc_output & 1) {
			// EGA card is in color mode
			if ((1.0 / vga.draw.delay.htotal) > 19.0) {
				// 64 color EGA mode
				VGA_ATTR_SetEGAMonitorPalette(EgaMonitorMode::Ega);
			} else {
				// 16 color CGA mode compatibility
				VGA_ATTR_SetEGAMonitorPalette(EgaMonitorMode::Cga);
			}
		} else {
			// EGA card in monochrome mode
			// It is not meant to be autodetected that way, you
			// either have a monochrome or color monitor connected
			// and the EGA switches configured appropriately. But
			// this would only be a problem if a program sets the
			// adapter to monochrome mode and still expects color
			// output. Such a program should be shot to the moon...
			VGA_ATTR_SetEGAMonitorPalette(EgaMonitorMode::Mono);
		}
	}

	vga.draw.resizing       = false;
	vga.draw.vret_triggered = false;

	const auto horiz_end = updated_timings.horiz_display_end;
	const auto vert_end  = updated_timings.vert_display_end;

	// Determine video mode info, render dimensions, and source & render
	// pixel aspect ratios
	uint32_t render_width  = 0;
	uint32_t render_height = 0;

	// If true, the rendered image will be doubled horizontally post-render
	// via a scaler. This is done to achieve a (mostly) constant "emulated
	// dot pitch" for CRT shaders (or, to be more exact, to avoid the dot
	// pitch falling too low for less than ~640 pixel wide modes).
	bool double_width = false;

	// If true, the rendered image will be doubled vertically post-render
	// via a scaler (to fake double scanning for CGA modes on VGA, and for
	// double-scanned VESA modes).
	bool double_height = false;

	// If true, we're rendering a double-scanned VGA mode as single-scanned
	// (so rendering half the lines only, e.g., only 200 lines for the
	// double-scanned 320x200 13h VGA mode).
	bool forced_single_scan = false;

	// If true, we're dealing with "baked-in" double scanning, i.e., when
	// 320x200 is rendered as 320x400.
	bool rendered_double_scan = false;

	// If true, we're dealing with "baked-in" pixel doubling, i.e. when 
	// 160x200 is rendered as 320x200.
	bool rendered_pixel_doubling = false;

	Fraction render_pixel_aspect_ratio = {1};

	VideoMode video_mode = {};

	PixelFormat pixel_format;
	switch (vga.mode) {
	case M_LIN15: pixel_format = PixelFormat::RGB555_Packed16; break;
	case M_LIN16: pixel_format = PixelFormat::RGB565_Packed16; break;
	case M_LIN24: pixel_format = PixelFormat::BGR24_ByteArray; break;
	case M_LIN32:
	case M_CGA2_COMPOSITE:
	case M_CGA4_COMPOSITE:
	case M_CGA_TEXT_COMPOSITE:
		pixel_format = PixelFormat::BGRX32_ByteArray;
		break;
	default: pixel_format = PixelFormat::Indexed8; break;
	}

	switch (vga.mode) {
	case M_LIN4:
	case M_EGA:
		vga.draw.linear_base = vga.fastmem;
		vga.draw.linear_mask = (static_cast<uint64_t>(vga.vmemwrap) << 1) - 1;
		break;
	default:
		vga.draw.linear_base = vga.mem.linear;
		vga.draw.linear_mask = vga.vmemwrap - 1;
		break;
	}

#ifdef DEBUG_VGA_DRAW
	LOG_DEBUG("VGA: vga.mode: %s, graphics_enabled: %d, scan_doubling: %d, max_scan_line: %d",
	          to_string(vga.mode),
	          static_cast<uint8_t>(vga.attr.mode_control.is_graphics_enabled),
	          static_cast<uint8_t>(vga.crtc.maximum_scan_line.is_scan_doubling_enabled),
	          static_cast<uint8_t>(vga.crtc.maximum_scan_line.maximum_scan_line));
#endif

	const auto bios_mode_number = CurMode->mode;

	video_mode.bios_mode_number = bios_mode_number;

	auto pcjr_or_tga = [=]() {
		return is_machine_pcjr() ? GraphicsStandard::Pcjr : GraphicsStandard::Tga;
	};

	auto cga_pcjr_or_tga = [=]() {
		constexpr auto first_non_cga_mode = 0x08;
		if (bios_mode_number < first_non_cga_mode) {
			return GraphicsStandard::Cga;
		}
		return pcjr_or_tga();
	};

	// All Tandy modes have a height of 200.
	// Some games (ex. Impossible Mission II) fiddle with vga.other.vdend
	// The result of this should be rendering a short (by height) image in the horizonal center with black on the top/bottom.
	// Use this hard-coded value when calculating pixel aspect ratio so this effect looks correct.
	constexpr uint16_t CgaTandyAspectHeight = 200;

	switch (vga.mode) {
	case M_LIN4:
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN24:
	case M_LIN32: {
		// SVGA & VESA modes
		const auto is_pixel_doubling = vga.crtc.mode_control.div_memory_address_clock_by_2;

		video_mode.is_graphics_mode  = true;
		video_mode.graphics_standard = VESA_IsVesaMode(bios_mode_number)
		                                     ? GraphicsStandard::Vesa
		                                     : GraphicsStandard::Svga;

		switch (vga.mode) {
		case M_LIN8:
			// 256-colour SVGA & VESA modes (other than mode 13h)
			video_mode.color_depth = ColorDepth::IndexedColor256;

			if (is_pixel_doubling ||
			    (svga_type == SvgaType::S3 && !(vga.s3.reg_3a & 0x10))) {
				video_mode.width = horiz_end * 4;
			} else {
				video_mode.width = horiz_end * 8;
			}
			break;
		case M_LIN24:
			// 24-bit true colour (16.7M-colour) VESA modes
		case M_LIN32:
			// 32-bit true colour (16.7M-colour) VESA modes
			video_mode.color_depth = ColorDepth::TrueColor24Bit;
			video_mode.width       = horiz_end * 8;
			break;
		case M_LIN15:
			// 15-bit high colour (32K-colour) VESA modes
			video_mode.color_depth = ColorDepth::HighColor15Bit;
			video_mode.width       = horiz_end * 4;
			break;
		case M_LIN16:
			// 16-bit high colour (65K-colour) VESA modes
			video_mode.color_depth = ColorDepth::HighColor16Bit;
			video_mode.width       = horiz_end * 4;
			break;
		case M_LIN4:
			// 16-colour SVGA & VESA modes
			vga.draw.blocks        = horiz_end;
			video_mode.width       = horiz_end * 8;
			video_mode.color_depth = ColorDepth::IndexedColor16;
			break;
		default: assert(false);
		}

		double_width = is_pixel_doubling && vga.draw.pixel_doubling_allowed;

		// No need to actually render double-scanned for VGA modes other
		// than 13h (and its tweak-mode variants; we'll just fake it with
		// `double_height`.
		if (is_vga_scan_doubling_bit_set()) {
			video_mode.is_double_scanned_mode = true;

			vga.draw.is_double_scanning = true;
			vga.draw.address_line_total /= 2;

			video_mode.height  = vert_end / 2;
			double_height      = vga.draw.scan_doubling_allowed;
			forced_single_scan = !vga.draw.scan_doubling_allowed;
		} else {
			video_mode.height = vert_end;
		}

		render_width  = video_mode.width;
		render_height = video_mode.height;

		const auto is_1280x1024_mode = (CurMode->swidth == 1280 &&
		                                CurMode->sheight == 1024);

		if (is_1280x1024_mode) {
			render_pixel_aspect_ratio = pixel_aspect_1280x1024;
		} else {
			render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
			        render_width, render_height, double_width, double_height);
		}

		if (vga.mode == M_LIN4) {
			VGA_DrawLine = VGA_Draw_Linear_Line;
		} else {
			// Use HW mouse cursor drawer if enabled
			pixel_format = VGA_ActivateHardwareCursor();
		}
	} break;

	case M_VGA: {
		// Only used for the "chunky" (or "chained") 320x200
		// 256-colour 13h VGA mode and its many tweaked "Mode X"
		// ("unchained") variants. These are outliers among other VGA
		// modes; the 640x480 / 16-color 12h mode is tagged with M_EGA
		// (because it's planar like all EGA modes), and all other VGA
		// modes with M_LIN4, M_LIN8, etc.

		video_mode.is_graphics_mode  = true;
		video_mode.graphics_standard = GraphicsStandard::Vga;
		video_mode.color_depth       = ColorDepth::IndexedColor256;

		const auto num_scanline_repeats = vga.crtc.maximum_scan_line.maximum_scan_line;

		// We assume the two scanline doubling methods cannot be stacked, but
		// not sure if this is true.
		video_mode.is_double_scanned_mode = (num_scanline_repeats > 0 ||
		                                     is_vga_scan_doubling_bit_set());

		render_pixel_aspect_ratio = calc_pixel_aspect_from_timings(vga_timings);

		video_mode.width = horiz_end * 4;
		render_width     = video_mode.width;

		// We only render "baked-in" double scanning (when we literally
		// render twice as many rows) for the M_VGA modes and M_EGA
		// modes on emulated VGA adapters only; for everything else, we
		// "fake double-scan" on VGA (render single-scanned, then double
		// the image vertically with a scaler).

		if (video_mode.is_double_scanned_mode) {
			video_mode.height  = vert_end / 2;

			// Some rare demos set up odd Maximum Scan Line CRTC register
			// values; for example, Show by Majic 12 uses the value 4 during
			// the zoom-rotator part in the intro to set up "scanline
			// quintupling". That's right, every scanline is repeated 4 times,
			// resulting in a total number of 5 scanlines per "logical pixel"!
			//
			// We're forcing such scanline repeating even in forced single
			// scan mode to yield correct results.
			const auto is_odd_address_line_total = vga.draw.address_line_total & 1;

			if (vga.draw.scan_doubling_allowed || is_odd_address_line_total) {
				vga.draw.is_double_scanning = true;
				render_height        = video_mode.height * 2;
				rendered_double_scan = true;
				forced_single_scan   = false;
			} else {
				vga.draw.address_line_total /= 2;
				render_height = video_mode.height;
				render_pixel_aspect_ratio /= 2;
				forced_single_scan = true;
			}
		} else { // single scan
			video_mode.height = vert_end;
			render_height     = video_mode.height;
		}

		// Mode 13h and practically all tweak-modes use pixel doubling.
		// Note this is not accomplished via dividing the memory address
		// clock by 2 like in other SVGA/VESA modes, but by some complex
		// interaction between various VGA registers that is specific to
		// mode 13h. So we'll just assume that pixel doubling is always
		// on for M_VGA.
		//
		// More information here:
		// https://github.com/joncampbell123/dosbox-x/issues/951
		//
		if (vga.draw.pixel_doubling_allowed) {
			double_width = true;
		} else {
			render_pixel_aspect_ratio *= 2;
		}

		const auto is_reelmagic_vga_passthrough = !ReelMagic_IsVideoMixerEnabled();
		if (is_reelmagic_vga_passthrough) {
			// The ReelMagic video mixer expects linear VGA drawing
			// (i.e.: Return to Zork's house intro), so limit the use
			// of 18-bit palettized LUT routine to non-mixed output.
			pixel_format = PixelFormat::BGRX32_ByteArray;

			VGA_DrawLine = draw_linear_line_from_dac_palette;
		} else {
			VGA_DrawLine = VGA_Draw_Linear_Line;
		}
	} break;

	case M_EGA:
		// 640x480 2-colour VGA mode
		// All 16-colour EGA and VGA modes

		video_mode.is_graphics_mode = true;

		// Modes 11h and 12h were supported by high-end EGA cards and
		// because of that operate internally more like EGA modes (so
		// DOSBox uses the EGA type for them), however they were
		// classified as VGA from a standards perspective, so we report
		// them as such.
		//
		// References:
		// [1] IBM VGA Technical Reference, Mode of Operation, pp 2-12,
		// 19 March, 1992. [2] "IBM PC Family- BIOS Video Modes",
		// http://minuszerodegrees.net/video/bios_video_modes.htm
		//
		switch (bios_mode_number) {
		case 0x011: // 640x480 2-colour VGA mode
			video_mode.graphics_standard = GraphicsStandard::Vga;
			video_mode.color_depth = ColorDepth::IndexedColor2;
			break;
		case 0x012: // 640x480 16-colour VGA mode
			video_mode.graphics_standard = GraphicsStandard::Vga;
			video_mode.color_depth = ColorDepth::IndexedColor16;
			break;
		default: // EGA modes
			video_mode.graphics_standard = GraphicsStandard::Ega;
			video_mode.color_depth = ColorDepth::IndexedColor16;
		}

		vga.draw.blocks = horiz_end;

		video_mode.width = horiz_end * 8;
		render_width     = video_mode.width;

		double_width = vga.seq.clocking_mode.is_pixel_doubling &&
		               vga.draw.pixel_doubling_allowed;

		if (is_machine_vga_or_better()) {
			render_pixel_aspect_ratio = calc_pixel_aspect_from_timings(
			        vga_timings);

			// We only render "baked-in" double scanning (when we literally
			// render twice as many rows) for the M_VGA modes and M_EGA modes
			// on emulated VGA adapters only; for everything else, we "fake
			// double-scan" on VGA (render single-scanned, then double the
			// image vertically with a scaler).
			//
			const auto num_scanline_repeats =
			        vga.crtc.maximum_scan_line.maximum_scan_line;

			// We assume the two scanline doubling methods cannot be
			// stacked, but not sure if this is true.
			video_mode.is_double_scanned_mode =
			        (num_scanline_repeats > 0 ||
			         is_vga_scan_doubling_bit_set());

			if (video_mode.is_double_scanned_mode) {
				video_mode.height = vert_end / 2;
				forced_single_scan = !vga.draw.scan_doubling_allowed;

				if (vga.draw.scan_doubling_allowed) {
					vga.draw.is_double_scanning = true;
					render_height = video_mode.height * 2;
					rendered_double_scan = true;
				} else {
					vga.draw.address_line_total /= 2;
					render_height = video_mode.height;
					render_pixel_aspect_ratio /= 2;
				}
			} else { // single scan
				video_mode.height = vert_end;
				render_height     = video_mode.height;
			}

			if (vga.seq.clocking_mode.is_pixel_doubling &&
			    !vga.draw.pixel_doubling_allowed) {
				render_pixel_aspect_ratio *= 2;
			}

			video_mode.has_vga_colors = vga.ega_mode_with_vga_colors;

		} else { // M_EGA
			video_mode.height = vert_end;
			render_height     = video_mode.height;

			render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
			        render_width, render_height, double_width, double_height);
		}

		if (is_machine_vga_or_better()) {
			pixel_format = PixelFormat::BGRX32_ByteArray;

			VGA_DrawLine = draw_linear_line_from_dac_palette;
		} else {
			VGA_DrawLine = VGA_Draw_Linear_Line;
		}
		break;

	case M_TANDY16:
		// 160x200 and 320x200 16-colour modes on Tandy & PCjr
		vga.draw.blocks = horiz_end * 2;

		video_mode.is_graphics_mode  = true;
		video_mode.graphics_standard = pcjr_or_tga();
		video_mode.color_depth       = ColorDepth::IndexedColor16;

		if (vga.tandy.mode.is_high_bandwidth) {
			if (is_machine_tandy() && vga.tandy.mode.is_tandy_640_dot_graphics) {
				vga.draw.blocks  = horiz_end * 4;
				video_mode.width = horiz_end * 8;
				render_width     = video_mode.width;
			} else {
				double_width = vga.draw.pixel_doubling_allowed;
				video_mode.width = horiz_end * 4;
				render_width     = video_mode.width;
			}
			VGA_DrawLine = VGA_Draw_4BPP_Line;

		} else { // low-bandwidth
			double_width     = vga.draw.pixel_doubling_allowed;
			video_mode.width = horiz_end * 4;
			render_width     = video_mode.width * 2;
			rendered_pixel_doubling = true;

			VGA_DrawLine = VGA_Draw_4BPP_Line_Double;
		}

		video_mode.height = vert_end;
		render_height     = video_mode.height;

		render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
		        render_width, CgaTandyAspectHeight, double_width, double_height);
		break;

	case M_TANDY4:
		// 320x200 4-colour CGA mode on CGA, Tandy & PCjr
		// 640x200 4-colour mode on Tandy & PCjr

		video_mode.is_graphics_mode  = true;
		video_mode.graphics_standard = cga_pcjr_or_tga();
		video_mode.color_depth       = is_machine_cga_mono()
		                                     ? ColorDepth::Monochrome
		                                     : ColorDepth::IndexedColor4;

		vga.draw.blocks = horiz_end * 2;

		video_mode.width  = horiz_end * 8;
		video_mode.height = vert_end;

		/*
		        TODO This bit is not set for the 640x200
		        Tandy mode; this cannot be correct. This would
		        result in width doubling the 640x200 mode. A
		        simple 'width < 640' check will suffice instead
		        until someone investigates this...

		        if (is_machine_tandy()) {
		                double_width =
		   (vga.tandy.mode.is_tand_640_dot_graphics) == 0; } else {
		   double_width = (vga.tandy.mode.is_high_bandwidth) == 0;
		        }
		*/

		double_width = (video_mode.width < 640) &&
		               vga.draw.pixel_doubling_allowed;

		render_width  = video_mode.width;
		render_height = video_mode.height;

		render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
		        render_width, CgaTandyAspectHeight, double_width, double_height);

		// TODO this seems like overkill; could be probably simplified a
		// lot
		if ((is_machine_tandy() &&
		     vga.tandy.mode_control.is_tandy_640x200_4_color_graphics) ||
		    (is_machine_pcjr() &&
		     (vga.tandy.mode.is_high_bandwidth && vga.tandy.mode.is_graphics_enabled &&
		      !vga.tandy.mode.is_black_and_white_mode &&
		      vga.tandy.mode.is_video_enabled &&
		      !vga.tandy.mode.is_pcjr_16_color_graphics))) {
			VGA_DrawLine = VGA_Draw_2BPPHiRes_Line;
		} else {
			VGA_DrawLine = VGA_Draw_2BPP_Line;
		}
		break;

	case M_TANDY2:
		// 640x200 monochrome CGA mode on CGA, Tandy & PCjr

		video_mode.is_graphics_mode  = true;
		video_mode.graphics_standard = cga_pcjr_or_tga();
		video_mode.color_depth       = is_machine_cga_mono()
		                                     ? ColorDepth::Monochrome
		                                     : ColorDepth::IndexedColor2;

		if (is_machine_pcjr()) {
			vga.draw.blocks = horiz_end * (vga.tandy.mode_control.is_pcjr_640x200_2_color_graphics
			                                       ? 8
			                                       : 4);
			video_mode.width = vga.draw.blocks * 2;

			double_width = !vga.tandy.mode_control.is_pcjr_640x200_2_color_graphics &&
			               vga.draw.pixel_doubling_allowed;

		} else { // Tandy
			vga.draw.blocks = horiz_end * (vga.tandy.mode.is_tandy_640_dot_graphics
			                                       ? 2
			                                       : 1);
			video_mode.width = vga.draw.blocks * 8;

			double_width = !vga.tandy.mode.is_tandy_640_dot_graphics &&
			               vga.draw.pixel_doubling_allowed;
		}

		video_mode.height = vert_end;

		render_width  = video_mode.width;
		render_height = video_mode.height;

		render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
		        render_width, CgaTandyAspectHeight, double_width, double_height);

		VGA_DrawLine = VGA_Draw_1BPP_Line;
		break;

	case M_CGA2:
		// 640x200 2-colour CGA mode on EGA & VGA
		// (2-colour and not monochrome because the background is always
		// black, but the foreground colour can be changed to any of the
		// 16 CGA colours)
	case M_CGA4:
		// 320x200 4-colour CGA mode on EGA & VGA

		video_mode.is_graphics_mode  = true;
		video_mode.graphics_standard = GraphicsStandard::Cga;

		if (is_machine_cga_mono()) {
			video_mode.color_depth = ColorDepth::Monochrome;
		} else {
			video_mode.color_depth = (vga.mode == M_CGA2)
			                               ? ColorDepth::IndexedColor2
			                               : ColorDepth::IndexedColor4;
		}

		vga.draw.blocks = horiz_end * 2;

		video_mode.width = horiz_end * 8;
		render_width     = video_mode.width;

		double_width = vga.seq.clocking_mode.is_pixel_doubling &&
		               vga.draw.pixel_doubling_allowed;

		if (is_machine_vga_or_better()) {
			video_mode.is_double_scanned_mode = true;

			video_mode.height = vert_end / 2;
			render_height     = video_mode.height;

			double_height = vga.draw.scan_doubling_allowed;

			// We never render true double-scanned CGA modes; we
			// always fake it even if double scanning is requested
			forced_single_scan = true;

			render_pixel_aspect_ratio = calc_pixel_aspect_from_timings(
			        vga_timings);

			if (!vga.draw.scan_doubling_allowed) {
				render_pixel_aspect_ratio /= 2;
			}
			if (vga.seq.clocking_mode.is_pixel_doubling &&
			    !vga.draw.pixel_doubling_allowed) {
				render_pixel_aspect_ratio *= 2;
			}

		} else { // M_EGA
			video_mode.height = vert_end;
			render_height     = video_mode.height;

			render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
			        render_width, render_height, double_width, double_height);
		}

		if (vga.mode == M_CGA2) {
			VGA_DrawLine = VGA_Draw_1BPP_Line;
		} else {
			VGA_DrawLine = VGA_Draw_2BPP_Line;
		}
		break;

	case M_CGA16:
		// Composite output in 320x200 4-colour CGA mode on PCjr only

		video_mode.is_graphics_mode  = true;
		video_mode.graphics_standard = GraphicsStandard::Pcjr;
		video_mode.color_depth       = ColorDepth::Composite;

		vga.draw.blocks = horiz_end * 2;

		video_mode.width  = horiz_end * 8;
		video_mode.height = vert_end;

		double_width = vga.draw.pixel_doubling_allowed;

		// Composite emulation is rendered at 2x the horizontal resolution
		render_width  = video_mode.width * 2;
		render_height = video_mode.height;

		render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
		        render_width, render_height, double_width, double_height);

		VGA_DrawLine = VGA_Draw_CGA16_Line;
		break;

	case M_CGA4_COMPOSITE:
		// Composite output in 320x200 & 640x200 4-colour modes on CGA &
		// Tandy
		video_mode.is_graphics_mode  = true;
		video_mode.graphics_standard = cga_pcjr_or_tga();
		video_mode.color_depth       = ColorDepth::Composite;

		vga.draw.blocks = horiz_end * 2;

		video_mode.width  = horiz_end * 8;
		video_mode.height = vert_end;

		// Composite emulation is rendered at 2x the horizontal resolution
		render_width  = video_mode.width * 2;
		render_height = video_mode.height;

		render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
		        render_width, CgaTandyAspectHeight, double_width, double_height);

		VGA_DrawLine = VGA_Draw_CGA4_Composite_Line;
		break;

	case M_CGA2_COMPOSITE:
		// Composite output in 640x200 monochrome CGA mode on CGA, Tandy
		// & PCjr
		video_mode.is_graphics_mode  = true;
		video_mode.graphics_standard = cga_pcjr_or_tga();
		video_mode.color_depth       = ColorDepth::Composite;

		vga.draw.blocks = horiz_end * 2;

		video_mode.width  = horiz_end * 16;
		video_mode.height = vert_end;

		render_width  = video_mode.width;
		render_height = video_mode.height;

		render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
		        render_width, CgaTandyAspectHeight, double_width, double_height);

		VGA_DrawLine = VGA_Draw_CGA2_Composite_Line;
		break;

	case M_HERC_GFX:
		// Hercules graphics mode
		video_mode.is_graphics_mode  = true;
		video_mode.graphics_standard = GraphicsStandard::Hercules;
		video_mode.color_depth       = ColorDepth::Monochrome;

		vga.draw.blocks = horiz_end * 2;

		video_mode.width  = horiz_end * 16;
		video_mode.height = vert_end;

		render_width  = video_mode.width;
		render_height = video_mode.height;

		render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
		        render_width, render_height, double_width, double_height);

		VGA_DrawLine = VGA_Draw_1BPP_Line;
		break;

	case M_TEXT:
		// All EGA, VGA, SVGA & VESA text modes
		video_mode.is_graphics_mode = false;

		switch (machine) {
		case MachineType::Ega:
			video_mode.graphics_standard = GraphicsStandard::Ega;
			break;
		case MachineType::Vga: {
			constexpr auto max_vga_text_mode_number = 0x07;
			if (bios_mode_number <= max_vga_text_mode_number) {
				video_mode.graphics_standard = GraphicsStandard::Vga;
			} else {
				video_mode.graphics_standard =
				        VESA_IsVesaMode(bios_mode_number)
				                ? GraphicsStandard::Vesa
				                : GraphicsStandard::Svga;
			}
		} break;
		default: assert(false);
		}

		video_mode.color_depth = ColorDepth::IndexedColor16;

		vga.draw.blocks = horiz_end;

		double_width = vga.seq.clocking_mode.is_pixel_doubling &&
		               vga.draw.pixel_doubling_allowed;

		if (is_machine_vga_or_better()) {
			vga.draw.pixels_per_character = vga.seq.clocking_mode.is_eight_dot_mode
			                                      ? PixelsPerChar::Eight
			                                      : PixelsPerChar::Nine;

			pixel_format = PixelFormat::BGRX32_ByteArray;

			render_pixel_aspect_ratio = calc_pixel_aspect_from_timings(
			        vga_timings);

			// Text mode double scanning can only be done by setting
			// the Double Scanning bit.
			video_mode.is_double_scanned_mode = is_vga_scan_doubling_bit_set();

			video_mode.width = horiz_end * vga.draw.pixels_per_character;

			if (video_mode.is_double_scanned_mode) {
				video_mode.height = vert_end / 2;

				if (vga.draw.scan_doubling_allowed) {
					double_height = true;
				} else {
					render_pixel_aspect_ratio /= 2;
				}
			} else { // single scan
				video_mode.height = vert_end;
			}

			render_width  = video_mode.width;
			render_height = video_mode.height;

			if (vga.seq.clocking_mode.is_pixel_doubling &&
			    !vga.draw.pixel_doubling_allowed) {
				render_pixel_aspect_ratio *= 2;
			}

			VGA_DrawLine = draw_text_line_from_dac_palette;

		} else { // M_EGA
			vga.draw.pixels_per_character = PixelsPerChar::Eight;

			video_mode.width  = horiz_end * vga.draw.pixels_per_character;
			video_mode.height = vert_end;

			render_width  = video_mode.width;
			render_height = video_mode.height;

			render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
			        render_width, render_height, double_width, double_height);

			VGA_DrawLine = VGA_TEXT_Draw_Line;
		}

		render_pixel_aspect_ratio *= {PixelsPerChar::Eight,
		                              vga.draw.pixels_per_character};
		break;

	case M_TANDY_TEXT:
		// CGA, Tandy & PCjr text modes
		video_mode.is_graphics_mode  = false;
		video_mode.graphics_standard = cga_pcjr_or_tga();
		video_mode.color_depth       = is_machine_cga_mono()
		                                     ? ColorDepth::Monochrome
		                                     : ColorDepth::IndexedColor16;

		vga.draw.blocks = horiz_end;

		video_mode.width  = horiz_end * 8;
		video_mode.height = vert_end;

		render_width  = video_mode.width;
		render_height = video_mode.height;

		double_width = !vga.tandy.mode.is_high_bandwidth &&
		               vga.draw.pixel_doubling_allowed;

		render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
		        render_width, render_height, double_width, double_height);

		VGA_DrawLine = VGA_TEXT_Draw_Line;
		break;

	case M_CGA_TEXT_COMPOSITE:
		// Composite output in text modes on CGA, Tandy & PCjr
		video_mode.is_graphics_mode  = false;
		video_mode.graphics_standard = cga_pcjr_or_tga();
		video_mode.color_depth       = ColorDepth::Composite;

		vga.draw.blocks = horiz_end;

		video_mode.width = horiz_end *
		                   (vga.tandy.mode.is_high_bandwidth ? 8 : 16);
		video_mode.height = vert_end;

		// Composite emulation is rendered at 2x the horizontal resolution
		render_width  = video_mode.width;
		render_height = video_mode.height;

		render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
		        render_width, render_height, double_width, double_height);

		VGA_DrawLine = VGA_CGA_TEXT_Composite_Draw_Line;
		break;

	case M_HERC_TEXT:
		// Hercules text mode
		video_mode.is_graphics_mode  = false;
		video_mode.graphics_standard = GraphicsStandard::Hercules;
		video_mode.color_depth       = ColorDepth::Monochrome;

		vga.draw.blocks = horiz_end;

		video_mode.width  = horiz_end * 8;
		video_mode.height = vert_end;

		render_width  = video_mode.width;
		render_height = video_mode.height;

		render_pixel_aspect_ratio = calc_pixel_aspect_from_dimensions(
		        render_width, render_height, double_width, double_height);

		VGA_DrawLine = VGA_TEXT_Herc_Draw_Line;
		break;

	default:
		LOG_WARNING("VGA: Unhandled video mode %02Xh", vga.mode);
		// Set the dimensions to something semi-reasonable so at least
		// we get a chance to see what's going on
		video_mode.width  = horiz_end;
		video_mode.height = vert_end;

		render_width  = video_mode.width;
		render_height = video_mode.height;
		break;
	}

	VGA_CheckScanLength();

	auto vblank_skip = updated_timings.vblank_skip;
	if (is_machine_vga_or_better() &&
	    (vga.mode == M_CGA2 || vga.mode == M_CGA4)) {
		vblank_skip /= 2;
	}

	// 'render_pixel_aspect_ratio' has any post-render width/height doubling
	// already factored in, so we need to multiply it by
	// 'render_per_video_mode_scale' to derive the video mode's pixel aspect
	// ratio. It's just less redundant and error prone to derive the video
	// mode PAR this way.
	const auto final_render_width = (render_width * (double_width ? 2 : 1));
	const auto final_render_height = (render_height * (double_height ? 2 : 1));

	const auto render_per_video_mode_scale =
	        Fraction(final_render_width / video_mode.width,
	                 final_render_height / video_mode.height);

	switch (RENDER_GetAspectRatioCorrectionMode()) {
	case AspectRatioCorrectionMode::Auto:
		// Derive video mode pixel aspect ratio from the render PAR
		video_mode.pixel_aspect_ratio = render_pixel_aspect_ratio *
		                                render_per_video_mode_scale;
		break;

	case AspectRatioCorrectionMode::SquarePixels:
		// Override PARs if square pixels are forced in aspect ratio
		// correction disabled mode
		render_pixel_aspect_ratio = render_per_video_mode_scale.Inverse();
		video_mode.pixel_aspect_ratio = {1};
		break;

	case AspectRatioCorrectionMode::Stretch: {
		// Stretch image to the viewport and calculate the resulting PARs
		const auto viewport_px = GFX_GetViewportSizeInPixels();

		const Fraction viewport_aspect_ratio = {iroundf(viewport_px.w),
		                                        iroundf(viewport_px.h)};

		const Fraction final_render_aspect_ratio = {final_render_width,
		                                            final_render_height};

		render_pixel_aspect_ratio = viewport_aspect_ratio /
		                            final_render_aspect_ratio;

		video_mode.pixel_aspect_ratio = render_pixel_aspect_ratio *
		                                render_per_video_mode_scale;
	} break;

	default:
		assertm(false, "Invalid AspectRatioCorrectionMode value");
		return {};
	}

	// Try to determine if this is a custom mode
	video_mode.is_custom_mode = (CurMode->swidth != video_mode.width ||
	                             CurMode->sheight != video_mode.height);

	vga.draw.vblank_skip = vblank_skip;
	setup_line_drawing_delays(render_height);

	vga.draw.line_length = render_width *
	                       ((get_bits_per_pixel(pixel_format) + 1) / 8);

#ifdef VGA_KEEP_CHANGES
	vga.changes.active    = false;
	vga.changes.frame     = 0;
	vga.changes.writeMask = 1;
#endif

#ifdef DEBUG_VGA_DRAW
	LOG_DEBUG("VGA: horiz.total: %d, vert.total: %d",
	          vga_timings.horiz.total,
	          vga_timings.vert.total);

	LOG_DEBUG("VGA: RENDER: width: %d, height: %d, dblw: %d, dblh: %d, PAR: %lld:%lld (1:%g)",
	          render_width,
	          render_height,
	          double_width,
	          double_height,
	          render_pixel_aspect_ratio.Num(),
	          render_pixel_aspect_ratio.Denom(),
	          render_pixel_aspect_ratio.Inverse().ToDouble());

	LOG_DEBUG("VGA: forced_single_scan: %d, rendered_double_scan: %d, rendered_pixel_doubling: %d",
	          forced_single_scan,
	          rendered_double_scan,
	          rendered_pixel_doubling);

	LOG_DEBUG("VGA: VIDEO_MODE: width: %d, height: %d, PAR: %lld:%lld (1:%g)",
	          video_mode.width,
	          video_mode.height,
	          video_mode.pixel_aspect_ratio.Num(),
	          video_mode.pixel_aspect_ratio.Denom(),
	          video_mode.pixel_aspect_ratio.Inverse().ToDouble());

	LOG_DEBUG("VGA: h total %2.5f (%3.2fkHz) blank(%02.5f/%02.5f) retrace(%02.5f/%02.5f)",
	          vga.draw.delay.htotal,
	          (1.0 / vga.draw.delay.htotal),
	          vga.draw.delay.hblkstart,
	          vga.draw.delay.hblkend,
	          vga.draw.delay.hrstart,
	          vga.draw.delay.hrend);

	LOG_DEBUG("VGA: v total %2.5f (%3.2fHz) blank(%02.5f/%02.5f) retrace(%02.5f/%02.5f)",
	          vga.draw.delay.vtotal,
	          (1000.0 / vga.draw.delay.vtotal),
	          vga.draw.delay.vblkstart,
	          vga.draw.delay.vblkend,
	          vga.draw.delay.vrstart,
	          vga.draw.delay.vrend);
#endif

	ImageInfo img_info = {};

	img_info.width                   = render_width;
	img_info.height                  = render_height;
	img_info.double_width            = double_width;
	img_info.double_height           = double_height;
	img_info.forced_single_scan      = forced_single_scan;
	img_info.rendered_double_scan    = rendered_double_scan;
	img_info.rendered_pixel_doubling = rendered_pixel_doubling;
	img_info.pixel_aspect_ratio      = render_pixel_aspect_ratio;
	img_info.pixel_format            = pixel_format;
	img_info.video_mode              = video_mode;

	return img_info;
}

void VGA_SetupDrawing(uint32_t /*val*/)
{
	if (vga.mode == M_ERROR) {
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_PanningLatch);
		PIC_RemoveEvents(VGA_DisplayStartLatch);
		return;
	}

	auto image_info = setup_drawing();

	// need to change the vertical timing?
	bool fps_changed = false;
	const auto fps   = VGA_GetRefreshRate();

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

	static VideoMode previous_video_mode = {};

	if (previous_video_mode != image_info.video_mode ||
	    vga.draw.image_info != image_info || fps_changed) {
		VGA_KillDrawing();

		constexpr auto reinit_render = false;

		const auto shader_changed =
		        RENDER_MaybeAutoSwitchShader(GFX_GetCanvasSizeInPixels(),
		                                     image_info.video_mode,
		                                     reinit_render);

		if (shader_changed) {
			image_info = setup_drawing();
		}

		vga.draw.image_info = image_info;

		if (image_info.width > SCALER_MAXWIDTH ||
		    image_info.height > SCALER_MAXHEIGHT) {
			LOG_ERR("VGA: The calculated video resolution %ux%u will be "
			        "limited to the maximum of %ux%u",
			        image_info.width,
			        image_info.height,
			        SCALER_MAXWIDTH,
			        SCALER_MAXHEIGHT);

			vga.draw.image_info.width = std::min(image_info.width,
			                                 static_cast<uint16_t>(
			                                         SCALER_MAXWIDTH));

			vga.draw.image_info.height = std::min(image_info.height,
			                                  static_cast<uint16_t>(
			                                          SCALER_MAXHEIGHT));
		}

		vga.draw.lines_scaled = image_info.forced_single_scan ? 2 : 1;

		if (!vga.draw.vga_override) {
			ReelMagic_RENDER_SetSize(image_info, fps);
		}

		previous_video_mode = image_info.video_mode;
	}
}

void VGA_KillDrawing(void) {
	PIC_RemoveEvents(VGA_DrawPart);
	PIC_RemoveEvents(VGA_DrawSingleLine);
	PIC_RemoveEvents(VGA_DrawEGASingleLine);
	vga.draw.parts_left = 0;
	vga.draw.lines_done = ~0;
	if (!vga.draw.vga_override) RENDER_EndUpdate(true);
}

void VGA_SetOverride(const bool vga_override, const double override_refresh_hz)
{
	if (vga.draw.vga_override != vga_override) {
		if (vga_override) {
			VGA_KillDrawing();
			vga.draw.vga_override=true;
			vga.draw.override_refresh_hz = override_refresh_hz;
		} else {
			vga.draw.vga_override=false;
			vga.draw.image_info.width = 0; // change it so the output window gets updated
			VGA_SetupDrawing(0);
		}
	}
}
