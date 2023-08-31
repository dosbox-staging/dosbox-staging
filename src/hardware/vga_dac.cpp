/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
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

#include <cassert>

#include "inout.h"
#include "mem.h"
#include "reelmagic.h"
#include "render.h"
#include "rgb.h"
#include "vga.h"
#include "../ints/int10.h"

/*
3C6h (R/W):  PEL Mask
bit 0-7  This register is anded with the palette index sent for each dot.
         Should be set to FFh.

3C7h (R):  DAC State Register
bit 0-1  0 indicates the DAC is in Write Mode and 3 indicates Read mode.

3C7h (W):  PEL Address Read Mode
bit 0-7  The PEL data register (0..255) to be read from 3C9h.
Note: After reading the 3 bytes at 3C9h this register will increment,
      pointing to the next data register.

3C8h (R/W):  PEL Address Write Mode
bit 0-7  The PEL data register (0..255) to be written to 3C9h.
Note: After writing the 3 bytes at 3C9h this register will increment, pointing
      to the next data register.

3C9h (R/W):  PEL Data Register
bit 0-5  Color value
Note:  Each read or write of this register will cycle through first the
       registers for Red, Blue and Green, then increment the appropriate
       address register, thus the entire palette can be loaded by writing 0 to
       the PEL Address Write Mode register 3C8h and then writing all 768 bytes
       of the palette to this register.
*/

enum {DAC_READ,DAC_WRITE};

static void vga_dac_send_color(const uint8_t index, const uint8_t src)
{
	const auto& src_rgb666 = vga.dac.rgb[src];

	const auto r8 = rgb6_to_8_lut(src_rgb666.red);
	const auto g8 = rgb6_to_8_lut(src_rgb666.green);
	const auto b8 = rgb6_to_8_lut(src_rgb666.blue);

	// Map the source color into palette's requested index
	vga.dac.palette_map[index] = static_cast<uint32_t>((r8 << 16) |
	                                                   (g8 << 8) | b8);

	ReelMagic_RENDER_SetPalette(index, r8, g8, b8);
}

static void vga_dac_update_color(const uint16_t index)
{
	const auto maskIndex = index & vga.dac.pel_mask;
	assert(maskIndex <= UINT8_MAX); // lookup into 256-byte table
	assert(index <= UINT8_MAX);     // lookup into 256-byte table
	vga_dac_send_color(static_cast<uint8_t>(index), static_cast<uint8_t>(maskIndex));
}

static void write_p3c6(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	if (vga.dac.pel_mask != val) {
		LOG(LOG_VGAMISC, LOG_NORMAL)("VGA:DCA:Pel Mask set to %X", val);
		vga.dac.pel_mask = val;
		for (uint16_t i = 0; i < 256; i++)
			vga_dac_update_color( i );
	}
}

static uint8_t read_p3c6(io_port_t, io_width_t)
{
	return vga.dac.pel_mask;
}

static void write_p3c7(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	vga.dac.read_index = val;
	vga.dac.pel_index = 0;
	vga.dac.state = DAC_READ;
	vga.dac.write_index = val + 1;
}

static uint8_t read_p3c7(io_port_t, io_width_t)
{
	if (vga.dac.state == DAC_READ)
		return 0x3;
	else
		return 0x0;
}

static void write_p3c8(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	vga.dac.write_index = val;
	vga.dac.pel_index = 0;
	vga.dac.state = DAC_WRITE;
	vga.dac.read_index = val - 1;
}

static uint8_t read_p3c8(Bitu, io_width_t)
{
	return vga.dac.write_index;
}

static void write_p3c9(io_port_t, io_val_t value, io_width_t)
{
	auto val = check_cast<uint8_t>(value);
	val &= 0x3f;
	switch (vga.dac.pel_index) {
	case 0:
		vga.dac.rgb[vga.dac.write_index].red=val;
		vga.dac.pel_index=1;
		break;
	case 1:
		vga.dac.rgb[vga.dac.write_index].green=val;
		vga.dac.pel_index=2;
		break;
	case 2:
		vga.dac.rgb[vga.dac.write_index].blue=val;
		switch (vga.mode) {
		case M_VGA:
		case M_LIN8:
			vga_dac_update_color( vga.dac.write_index );
			if ( GCC_UNLIKELY( vga.dac.pel_mask != 0xff)) {
				const auto index = vga.dac.write_index;
				if ( (index & vga.dac.pel_mask) == index ) {
					for (uint16_t i = index + 1u; i < 256; i++)
						if ( (i & vga.dac.pel_mask) == index )
							vga_dac_update_color(static_cast<uint8_t>(i));
				}
			} 
			break;
		default:
			// Check for attributes and DAC entry link
			for (uint8_t i = 0; i < NumCgaColors; ++i) {
				if (vga.dac.combine[i] == vga.dac.write_index) {
					vga_dac_send_color(i, vga.dac.write_index);
				}
			}
		}
		vga.dac.write_index++;
//		vga.dac.read_index = vga.dac.write_index - 1;//disabled as it breaks Wari
		vga.dac.pel_index=0;
		break;
	default:
		LOG(LOG_VGAGFX,LOG_NORMAL)("VGA:DAC:Illegal Pel Index");			//If this can actually happen that will be the day
		break;
	};
}

static uint8_t read_p3c9(io_port_t, io_width_t)
{
	uint8_t ret;
	switch (vga.dac.pel_index) {
	case 0:
		ret=vga.dac.rgb[vga.dac.read_index].red;
		vga.dac.pel_index=1;
		break;
	case 1:
		ret=vga.dac.rgb[vga.dac.read_index].green;
		vga.dac.pel_index=2;
		break;
	case 2:
		ret=vga.dac.rgb[vga.dac.read_index].blue;
		vga.dac.read_index++;
		vga.dac.pel_index=0;
//		vga.dac.write_index=vga.dac.read_index+1;//disabled as it breaks wari
		break;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:DAC:Illegal Pel Index");			//If this can actually happen that will be the day
		ret=0;
		break;
	}
	return ret;
}

void VGA_DAC_CombineColor(const uint8_t attr, const uint8_t pal) {
	/* Check if this is a new color */
	vga.dac.combine[attr]=pal;
	if (vga.mode != M_LIN8) {
		// used by copper demo; almost no video card seems to suport it
		vga_dac_send_color( attr, pal );
	}
}

void VGA_DAC_SetEntry(const uint8_t entry, const uint8_t red,
                      const uint8_t green, const uint8_t blue)
{
	//Should only be called in machine != vga
	vga.dac.rgb[entry].red=red;
	vga.dac.rgb[entry].green=green;
	vga.dac.rgb[entry].blue=blue;

	for (uint8_t i = 0; i < NumCgaColors; ++i) {
		if (vga.dac.combine[i] == entry) {
			vga_dac_send_color(i, i);
		}
	}
}

void VGA_SetupDAC(void)
{
	vga.dac.bits=6;
	vga.dac.pel_mask=0xff;
	vga.dac.pel_index=0;
	vga.dac.state=DAC_READ;
	vga.dac.read_index=0;
	vga.dac.write_index=0;
	if (IS_VGA_ARCH) {
		/* Setup the DAC IO port Handlers */
		IO_RegisterWriteHandler(0x3c6, write_p3c6, io_width_t::byte);
		IO_RegisterReadHandler(0x3c6, read_p3c6, io_width_t::byte);
		IO_RegisterWriteHandler(0x3c7, write_p3c7, io_width_t::byte);
		IO_RegisterReadHandler(0x3c7, read_p3c7, io_width_t::byte);
		IO_RegisterWriteHandler(0x3c8, write_p3c8, io_width_t::byte);
		IO_RegisterReadHandler(0x3c8, read_p3c8, io_width_t::byte);
		IO_RegisterWriteHandler(0x3c9, write_p3c9, io_width_t::byte);
		IO_RegisterReadHandler(0x3c9, read_p3c9, io_width_t::byte);
	}
}
