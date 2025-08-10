// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>

#include "cpu/cpu.h"
#include "debugger.h"
#include "hardware/pic.h"
#include "vga.h"

void VGA_MapMMIO(void);
void VGA_UnmapMMIO(void);

void vga_write_p3d5(io_port_t port, io_val_t value, io_width_t);

void vga_write_p3d4(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	vga.crtc.index = val;
}

uint8_t vga_read_p3d4(io_port_t, io_width_t)
{
	return vga.crtc.index;
}

void vga_write_p3d5(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	// if (vga.crtc.index > 0x18) {
	// 	LOG_MSG("VGA crtc write %" sBitfs(X) " to reg %X", val, vga.crtc.index)
	// }

	switch (vga.crtc.index) {
	case 0x00: // Horizontal Total Register
		if (vga.crtc.read_only) {
			break;
		}
		vga.crtc.horizontal_total = val;

		// 0-7  Horizontal Total Character Clocks minus 5.
		break;

	case 0x01: // Horizontal Display End Register
		if (vga.crtc.read_only) {
			break;
		}
		if (val != vga.crtc.horizontal_display_end) {
			vga.crtc.horizontal_display_end = val;
			VGA_StartResize();
		}

		// 0-7  Number of Character Clocks Displayed minus 1.
		break;

	case 0x02: // Start Horizontal Blanking Register
		if (vga.crtc.read_only) {
			break;
		}
		vga.crtc.start_horizontal_blanking = val;

		// 0-7  The count at which Horizontal Blanking starts.
		break;

	case 0x03: // End Horizontal Blanking Register
		if (vga.crtc.read_only) {
			break;
		}
		vga.crtc.end_horizontal_blanking = val;

		// 0-4	Horizontal Blanking ends when the last 6 bits of the character
		// 	    counter equals this field. Bit 5 is at 3d4h index 5 bit 7.
		// 5-6	Number of character clocks to delay start of display after
		//      Horizontal Total has been reached.
		// 7 	Access to Vertical Retrace registers if set. If clear reads to
		//      3d4h index 10h and 11h access the Lightpen read back registers
		//      (?).
		break;

	case 0x04: // Start Horizontal Retrace Register
		if (vga.crtc.read_only) {
			break;
		}
		vga.crtc.start_horizontal_retrace = val;

		// 0-7  Horizontal Retrace starts when the Character Counter reaches
		//      this value.
		break;

	case 0x05: // End Horizontal Retrace Register
		if (vga.crtc.read_only) {
			break;
		}
		vga.crtc.end_horizontal_retrace = val;

		// 0-4	Horizontal Retrace ends when the last 5 bits of the character
		//      counter equals this value.
		// 5-6	Number of character clocks to delay start of display after
		//      Horizontal Retrace.
		// 7	bit 5 of the End Horizontal Blanking count (See 3d4h index 3
		//      bit 0-4)
		break;

	case 0x06: // Vertical Total Register
		if (vga.crtc.read_only) {
			break;
		}
		if (val != vga.crtc.vertical_total) {
			vga.crtc.vertical_total = val;
			VGA_StartResize();
		}

		// 0-7	Lower 8 bits of the Vertical Total. Bit 8 is
		// 		found in 3d4h index 7 bit 0. Bit 9 is found in 3d4h index 7
		//      bit 5.
		//
		// Note: For the VGA this value is the number of scan lines in the
		// display minus 2.
		break;

	case 0x07: // Overflow Register
		// Line compare bit ignores read only
		vga.config.line_compare = (vga.config.line_compare & 0x6ff) |
		                          (val & 0x10) << 4;
		if (vga.crtc.read_only) {
			break;
		}
		if ((vga.crtc.overflow ^ val) & 0xd6) {
			vga.crtc.overflow = val;
			VGA_StartResize();
		} else {
			vga.crtc.overflow = val;
		}

		// 0  Bit 8 of Vertical Total (3d4h index 6)
		// 1  Bit 8 of Vertical Display End (3d4h index 12h)
		// 2  Bit 8 of Vertical Retrace Start (3d4h index 10h)
		// 3  Bit 8 of Start Vertical Blanking (3d4h index 15h)
		// 4  Bit 8 of Line Compare Register (3d4h index 18h)
		// 5  Bit 9 of Vertical Total (3d4h index 6)
		// 6  Bit 9 of Vertical Display End (3d4h index 12h)
		// 7  Bit 9 of Vertical Retrace Start (3d4h index 10h)
		break;

	case 0x08: // Preset Row Scan Register
		vga.crtc.preset_row_scan  = val;
		vga.config.hlines_skip = val & 31;

		if (is_machine_vga_or_better()) {
			vga.config.bytes_skip = (val >> 5) & 3;
		} else {
			vga.config.bytes_skip = 0;
		}
		// LOG_DEBUG("Skip lines %d bytes %d",vga.config.hlines_skip,vga.config.bytes_skip);

		// 0-4	Number of lines we have scrolled down in the
		//      first character row. Provides Smooth Vertical Scrolling.
		// 5-6  Number of bytes to skip at the start of scanline. Provides
		//      Smooth Horizontal Scrolling together with the Horizontal Panning
		// 		Register (3C0h index 13h).
		break;

	case 0x09: {
		if (is_machine_vga_or_better()) {
			vga.config.line_compare = (vga.config.line_compare & 0x5ff) |
			                          (val & 0x40) << 3;
		}

		const auto old_val = vga.crtc.maximum_scan_line;
		const auto new_val = MaximumScanLineRegister{val};

		vga.crtc.maximum_scan_line.data = new_val.data;

		// Start resize if any bit except `line_compare_bit9` has been
		// changed.
		if ((old_val.maximum_scan_line != new_val.maximum_scan_line) ||
		    (old_val.start_vertical_blanking_bit9 !=
		     new_val.start_vertical_blanking_bit9) ||
		    (old_val.is_scan_doubling_enabled !=
		     new_val.is_scan_doubling_enabled)) {
			VGA_StartResize();
		}
	} break;

	case 0x0a: // Cursor Start Register
		vga.crtc.cursor_start    = val;
		vga.draw.cursor.sline = val & 0x1f;
		if (is_machine_vga_or_better()) {
			vga.draw.cursor.enabled = !(val & 0x20);
		} else {
			vga.draw.cursor.enabled = true;
		}

		// 0-4	First scanline of cursor within character.
		// 5	Turns Cursor off if set.
		break;

	case 0x0b: // Cursor End Register
		vga.crtc.cursor_end      = val;
		vga.draw.cursor.eline = val & 0x1f;
		vga.draw.cursor.delay = (val >> 5) & 0x3;

		// 0-4	Last scanline of cursor within character.
		// 5-6	Delay of cursor data in character clocks.
		break;

	case 0x0c: // Start Address High Register
		vga.crtc.start_address_high = val;
		vga.config.display_start = (vga.config.display_start & 0xFF00FF) |
		                           (val << 8);

		// 0-7  Upper 8 bits of the start address of the display buffer.
		break;

	case 0x0d: // Start Address Low Register
		vga.crtc.start_address_low = val;
		vga.config.display_start = (vga.config.display_start & 0xFFFF00) | val;

		//0-7	Lower 8 bits of the start address of the display buffer.
		break;

	case 0x0e: // Cursor Location High Register
		vga.crtc.cursor_location_high = val;
		vga.config.cursor_start &= 0xff00ff;
		vga.config.cursor_start |= val << 8;

		//0-7  Upper 8 bits of the address of the cursor.
		break;

	case 0x0f: // Cursor Location Low Register
		// TODO update cursor on screen
		vga.crtc.cursor_location_low = val;
		vga.config.cursor_start &= 0xffff00;
		vga.config.cursor_start |= val;

		//0-7  Lower 8 bits of the address of the cursor.
		break;

	case 0x10: // Vertical Retrace Start Register
		vga.crtc.vertical_retrace_start = val;
		//
		// 0-7	Lower 8 bits of Vertical Retrace Start. Vertical Retrace
		//	    starts when the line counter reaches this value.
		//
		// Bit 8 is found in 3d4h index 7 bit 2.
		// Bit 9 is found in 3d4h index 7 bit 7.
		break;

	case 0x11: // Vertical Retrace End Register
		vga.crtc.vertical_retrace_end = val;

		if (is_machine_ega_or_better() && !(val & 0x10)) {
			vga.draw.vret_triggered = false;
			if (is_machine_ega()) {
				PIC_DeActivateIRQ(9);
			}
		}
		if (is_machine_vga_or_better()) {
			vga.crtc.read_only = (val & 128) > 0;
		} else {
			vga.crtc.read_only = false;
		}

		// 0-3	Vertical Retrace ends when the last 4 bits of the line counter
		//      equals this value.
		// 4	if clear Clears pending Vertical Interrupts.
		// 5	Vertical Interrupts (IRQ 2) disabled if set. Can usually be
		//      left disabled, but some systems (including PS/2) require it to
		//      be enabled.
		// 6	If set selects 5 refresh cycles per scanline rather than 3.
		// 7	Disables writing to registers 0-7 if set 3d4h index 7 bit 4 is
		//      not affected by this bit.
		break;

	case 0x12: // Vertical Display End Register
		if (val != vga.crtc.vertical_display_end) {
			if (abs(static_cast<int>(
			            (Bits)val - (Bits)vga.crtc.vertical_display_end)) <
			    3) {
				// delay small vde changes a bit to avoid screen
				// resizing if they are reverted in a short
				// timeframe
				PIC_RemoveEvents(VGA_SetupDrawing);
				vga.draw.resizing          = false;
				vga.crtc.vertical_display_end = val;
				VGA_StartResizeAfter(150);
			} else {
				vga.crtc.vertical_display_end = val;
				VGA_StartResize();
			}
		}

		// 0-7	Lower 8 bits of Vertical Display End. The display ends when
		// 	    the line counter reaches this value.
		//
		// Bit 8 is found in 3d4h index 7 bit 1.
		// Bit 9 is found in 3d4h index 7 bit 6.
		break;

	case 0x13: // Offset register
		vga.crtc.offset = val;
		vga.config.scan_len &= 0x300;
		vga.config.scan_len |= val;
		VGA_CheckScanLength();

		// 0-7	Number of bytes in a scanline / K, where K is 2
		//      for byte mode, 4 for word mode and 8 for Double Word mode.
		break;

	case 0x14: // Underline Location Register
		vga.crtc.underline_location = val;
		if (is_machine_vga_or_better()) {
			// Byte,word,dword mode
			if (vga.crtc.underline_location & 0x20) {
				vga.config.addr_shift = 2;
			} else if (vga.crtc.mode_control.word_byte_mode_select) {
				vga.config.addr_shift = 0;
			} else {
				vga.config.addr_shift = 1;
			}
		} else {
			vga.config.addr_shift = 1;
		}

		// 0-4	Position of underline within Character cell.
		// 5	If set memory address is only changed every fourth character
		// 		clock.
		// 6	Double Word mode addressing if set.
		break;

	case 0x15: // Start Vertical Blank Register
		if (val != vga.crtc.start_vertical_blanking) {
			vga.crtc.start_vertical_blanking = val;
			VGA_StartResize();
		}

		// 0-7	Lower 8 bits of Vertical Blank Start. Vertical blanking starts
		//      when the line counter reaches this value.
		//
		// Bit 8 is found in 3d4h index 7 bit 3.
		break;

	case 0x16: //  End Vertical Blank Register
		if (val != vga.crtc.end_vertical_blanking) {
			vga.crtc.end_vertical_blanking = val;
			VGA_StartResize();
		}

		// 0-6	Vertical blanking stops when the lower 7 bits of
		//      the line counter equals this field. Some SVGA chips uses all 8
		//      bits! IBM actually says bits 0-7.
		break;

	case 0x17:
		vga.crtc.mode_control.data = val;
		vga.tandy.line_mask        = (~val) & 3;

		// Byte, word, dword mode
		if (vga.crtc.underline_location & 0x20) {
			vga.config.addr_shift = 2;
		} else if (vga.crtc.mode_control.word_byte_mode_select) {
			vga.config.addr_shift = 0;
		} else {
			vga.config.addr_shift = 1;
		}

		if (vga.tandy.line_mask) {
			vga.tandy.line_shift = 13;
			vga.tandy.addr_mask  = (1 << 13) - 1;
		} else {
			vga.tandy.addr_mask  = ~0;
			vga.tandy.line_shift = 0;
		}

		// Should we really need to do a determinemode here?
		// VGA_DetermineMode();
		break;

	case 0x18: // Line Compare Register
		vga.crtc.line_compare = val;
		vga.config.line_compare = (vga.config.line_compare & 0x700) | val;

		// 0-7	Lower 8 bits of the Line Compare. When the Line
		//      counter reaches this value, the display address wraps to 0.
		//      Provides Split Screen facilities.
		//
		// Bit 8 is found in 3d4h index 7 bit 4.
		// Bit 9 is found in 3d4h index 9 bit 6.
		break;

	default:
		if (svga.write_p3d5) {
			svga.write_p3d5(vga.crtc.index, val, io_width_t::byte);
		} else {
			LOG(LOG_VGAMISC, LOG_NORMAL)
			("VGA:CRTC:Write to unknown index %X", vga.crtc.index);
		}
		break;
	}
}

uint8_t vga_read_p3d5(io_port_t, io_width_t)
{
	//	LOG_MSG("VGA crtc read from reg %X",vga.crtc.index);
	switch (vga.crtc.index) {
	case 0x00: return vga.crtc.horizontal_total;
	case 0x01: return vga.crtc.horizontal_display_end;
	case 0x02: return vga.crtc.start_horizontal_blanking;
	case 0x03: return vga.crtc.end_horizontal_blanking;
	case 0x04: return vga.crtc.start_horizontal_retrace;
	case 0x05: return vga.crtc.end_horizontal_retrace;
	case 0x06: return vga.crtc.vertical_total;
	case 0x07: return vga.crtc.overflow;
	case 0x08: return vga.crtc.preset_row_scan;
	case 0x09: return vga.crtc.maximum_scan_line.data;
	case 0x0A: return vga.crtc.cursor_start;
	case 0x0B: return vga.crtc.cursor_end;
	case 0x0C: return vga.crtc.start_address_high;
	case 0x0D: return vga.crtc.start_address_low;
	case 0x0E: return vga.crtc.cursor_location_high;
	case 0x0F: return vga.crtc.cursor_location_low;
	case 0x10: return vga.crtc.vertical_retrace_start;
	case 0x11: return vga.crtc.vertical_retrace_end;
	case 0x12: return vga.crtc.vertical_display_end;
	case 0x13: return vga.crtc.offset;
	case 0x14: return vga.crtc.underline_location;
	case 0x15: return vga.crtc.start_vertical_blanking;
	case 0x16: return vga.crtc.end_vertical_blanking;
	case 0x17: return vga.crtc.mode_control.data;
	case 0x18: return vga.crtc.line_compare;
	default:
		if (svga.read_p3d5) {
			return svga.read_p3d5(vga.crtc.index, io_width_t::byte);
		} else {
			LOG(LOG_VGAMISC, LOG_NORMAL)
			("VGA:CRTC:Read from unknown index %X", vga.crtc.index);
			return 0x0;
		}
	}
}
