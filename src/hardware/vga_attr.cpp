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
#include "inout.h"
#include "vga.h"

#include "../ints/int10.h"

#define attr(blah) vga.attr.blah

static void update_palette_mappings()
{
	for (uint8_t i = 0; i < NumCgaColors; ++i)
		VGA_ATTR_SetPalette(i, vga.attr.palette[i]);
}

void VGA_ATTR_SetEGAMonitorPalette(EGAMonitorMode m)
{
	// palette bit assignment:
	// bit | pin | EGA        | CGA       | monochrome
	// ----+-----+------------+-----------+------------
	// 0   | 5   | blue       | blue      | nc
	// 1   | 4   | green      | green*    | nc
	// 2   | 3   | red        | red*      | nc
	// 3   | 7   | blue sec.  | nc        | video
	// 4   | 6   | green sec. | intensity | intensity
	// 5   | 2   | red sec.   | nc        | nc
	// 6-7 | not used
	// * additive color brown instead of yellow
	switch (m) {
	case CGA: {
		// LOG_MSG("Monitor CGA");
		size_t i = 0;
		for (const auto& color : palette.cga64) {
			vga.dac.rgb[i++] = color;
		}
	} break;
	case EGA: {
		// LOG_MSG("Monitor EGA");
		size_t i = 0;
		for (const auto& color : palette.ega) {
			vga.dac.rgb[i++] = color;
		}
	} break;
	case MONO: {
		// LOG_MSG("Monitor MONO");
		size_t i = 0;
		for (const auto& color : palette.mono_text) {
			vga.dac.rgb[i++] = color;
		}
	} break;
	}

	update_palette_mappings();
}

void VGA_ATTR_SetPalette(uint8_t index, uint8_t val)
{
	// the attribute table stores only 6 bits
	val &= 63; 
	vga.attr.palette[index] = val;

	// apply the plane mask
	val = vga.attr.palette[index & vga.attr.color_plane_enable];

	// replace bits 4-5 if configured
	if (vga.attr.mode_control.palette_bits_5_4_select)
		val = static_cast<uint8_t>((val & 0xf) | (vga.attr.color_select << 4));

	// set bits 6 and 7 (not relevant for EGA)
	val |= (vga.attr.color_select & 0xc) << 4;

	// apply
	VGA_DAC_CombineColor(index,val);
}

uint8_t read_p3c0(io_port_t, io_width_t)
{
	// Wcharts, Win 3.11 & 95 SVGA
	uint8_t retval = attr(index) & 0x1f;
	if (!(attr(disabled) & 0x1)) retval |= 0x20;
	return retval;
}

void write_p3c0(io_port_t, io_val_t value, io_width_t)
{
	auto val = check_cast<uint8_t>(value);
	if (!vga.internal.attrindex) {
		attr(index)=val & 0x1F;
		vga.internal.attrindex=true;
		if (val & 0x20) attr(disabled) &= ~1;
		else attr(disabled) |= 1;
		/* 
			0-4	Address of data register to write to port 3C0h or read from port 3C1h
			5	If set screen output is enabled and the palette can not be modified,
				if clear screen output is disabled and the palette can be modified.
		*/
		return;
	} else {
		vga.internal.attrindex=false;
		switch (attr(index)) {
			/* Palette */
		case 0x00:		case 0x01:		case 0x02:		case 0x03:
		case 0x04:		case 0x05:		case 0x06:		case 0x07:
		case 0x08:		case 0x09:		case 0x0a:		case 0x0b:
		case 0x0c:		case 0x0d:		case 0x0e:		case 0x0f:
			if (attr(disabled) & 0x1)
				VGA_ATTR_SetPalette(attr(index), val);
			/*
			        0-5	Index into the 256 color DAC table. May be modified by 3C0h index
			        10h and 14h.
			*/
			break;

		case 0x10: {
			// Not really correct, but should do it
			AttributeModeControlRegister new_value = {val};
			if (!IS_VGA_ARCH) {
				new_value.is_pixel_panning_enabled = 0;
				new_value.is_8bit_color_enabled    = 0;
				new_value.palette_bits_5_4_select  = 0;
			}

			const AttributeModeControlRegister has_changed = {
			        check_cast<uint8_t>(vga.attr.mode_control.data ^
			                            new_value.data)};

			vga.attr.mode_control.data = new_value.data;

			if (has_changed.palette_bits_5_4_select) {
				update_palette_mappings();
			}
			if (has_changed.is_blink_enabled) {
				VGA_SetBlinking(vga.attr.mode_control.is_blink_enabled);
			}
			if (has_changed.is_graphics_enabled ||
			    has_changed.is_8bit_color_enabled) {
				VGA_DetermineMode();
			}
			if (has_changed.is_line_graphics_enabled) {
				// Recompute the panning value
				if (vga.mode == M_TEXT) {
					const uint8_t pan_reg = vga.attr.horizontal_pel_panning;
					if (pan_reg > 7) {
						vga.config.pel_panning = 0;
					} else if (vga.attr.mode_control.is_line_graphics_enabled) {
						// 9-dot wide characters
						vga.config.pel_panning = pan_reg + 1u;
					} else {
						// 8-dot characters
						vga.config.pel_panning = pan_reg;
					}
				}
			}
			break;
		}

		case 0x11:	/* Overscan Color Register */
			attr(overscan_color) = val;
			/* 0-5  Color of screen border. Color is defined as in the palette registers. */
			break;
		case 0x12:	/* Color Plane Enable Register */
			/* Why disable colour planes? */
			/* To support weird modes. */
			if ((attr(color_plane_enable)^val) & 0xf) {
				// in case the plane enable bits change...
				attr(color_plane_enable) = val;
				update_palette_mappings();
			} else
				attr(color_plane_enable) = val;
			/* 
				0	Bit plane 0 is enabled if set.
				1	Bit plane 1 is enabled if set.
				2	Bit plane 2 is enabled if set.
				3	Bit plane 3 is enabled if set.
				4-5	Video Status MUX. Diagnostics use only.
					Two attribute bits appear on bits 4 and 5 of the Input Status
					Register 1 (3dAh). 0: Bit 2/0, 1: Bit 5/4, 2: bit 3/1, 3: bit 7/6
			*/
			break;
		case 0x13:	/* Horizontal PEL Panning Register */
			attr(horizontal_pel_panning)=val & 0xF;
			switch (vga.mode) {
			case M_TEXT:
				if (val > 7) {
					vga.config.pel_panning = 0;
				} else if (vga.attr.mode_control.is_line_graphics_enabled) {
					// 9-dot wide characters
					vga.config.pel_panning = val + 1u;
				} else { // 8-dot characters
					vga.config.pel_panning = val;
				}
				break;
			case M_VGA:
			case M_LIN8:
				vga.config.pel_panning=(val & 0x7)/2;
				break;
			case M_LIN16:
			default:
				vga.config.pel_panning=(val & 0x7);
			}
			if (machine==MCH_EGA)
				// On the EGA panning can be programmed for every scanline:
				vga.draw.panning = vga.config.pel_panning;
			/*
				0-3	Indicates number of pixels to shift the display left
					Value  9bit textmode   256color mode   Other modes
					0          1               0              0
					1          2              n/a             1
					2          3               1              2
					3          4              n/a             3
					4          5               2              4
					5          6              n/a             5
					6          7               3              6
					7          8              n/a             7
					8          0              n/a            n/a
			*/
			break;
		case 0x14:	/* Color Select Register */
			if (!IS_VGA_ARCH) {
				attr(color_select)=0;
				break;
			}
			if (attr(color_select) ^ val) {
				attr(color_select) = val;
				update_palette_mappings();
			}
			/*
				0-1	If 3C0h index 10h bit 7 is set these 2 bits are used as bits 4-5 of
					the index into the DAC table.
				2-3	These 2 bits are used as bit 6-7 of the index into the DAC table
					except in 256 color mode.
					Note: this register does not affect 256 color modes.
			*/
			break;
		default:
			if (svga.write_p3c0) {
				svga.write_p3c0(attr(index), val, io_width_t::byte);
				break;
			}
			LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:ATTR:Write to unknown Index %2X",attr(index));
			break;
		}
	}
}

uint8_t read_p3c1(io_port_t, io_width_t)
{
	//	vga.internal.attrindex=false;
	switch (attr(index)) {
		/* Palette */
	case 0x00:		case 0x01:		case 0x02:		case 0x03:
	case 0x04:		case 0x05:		case 0x06:		case 0x07:
	case 0x08:		case 0x09:		case 0x0a:		case 0x0b:
	case 0x0c:		case 0x0d:		case 0x0e:		case 0x0f:
		return attr(palette[attr(index)]);
	case 0x10: return vga.attr.mode_control.data;
	case 0x11:	/* Overscan Color Register */
		return attr(overscan_color);
	case 0x12:	/* Color Plane Enable Register */
		return attr(color_plane_enable);
	case 0x13:	/* Horizontal PEL Panning Register */
		return attr(horizontal_pel_panning);
	case 0x14:	/* Color Select Register */
		return attr(color_select);
	default:
		if (svga.read_p3c1)
			return svga.read_p3c1(attr(index), io_width_t::byte);
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:ATTR:Read from unknown Index %2X",attr(index));
	}
	return 0;
}

void VGA_SetupAttr() {
	if (IS_EGAVGA_ARCH) {
		IO_RegisterWriteHandler(0x3c0, write_p3c0, io_width_t::byte);
		if (machine==MCH_EGA)
			IO_RegisterWriteHandler(0x3c1, write_p3c0,
			                        io_width_t::byte); // alias on EGA
		if (IS_VGA_ARCH) {
			IO_RegisterReadHandler(0x3c0, read_p3c0, io_width_t::byte);
			IO_RegisterReadHandler(0x3c1, read_p3c1, io_width_t::byte);
		}
	}
}
