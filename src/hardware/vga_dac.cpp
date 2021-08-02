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

#include <cassert>

#include "inout.h"
#include "render.h"
#include "vga.h"
#include "mem.h"

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

static void VGA_DAC_SendColor(uint8_t index, uint8_t src)
{
	const Bit8u red = vga.dac.rgb[src].red;
	const Bit8u green = vga.dac.rgb[src].green;
	const Bit8u blue = vga.dac.rgb[src].blue;
	//Set entry in (little endian) 16bit output lookup table
	const auto pixel = static_cast<uint16_t>(((blue >> 1) & 0x1f) | (((green)&0x3f) << 5) |
	                                         (((red >> 1) & 0x1f) << 11));
	var_write(&vga.dac.xlat16[index], pixel);

	RENDER_SetPal(index, static_cast<uint8_t>((red << 2) | (red >> 4)),
	              static_cast<uint8_t>((green << 2) | (green >> 4)),
	              static_cast<uint8_t>((blue << 2) | (blue >> 4)));
}

static void VGA_DAC_UpdateColor(uint16_t index)
{
	const auto maskIndex = index & vga.dac.pel_mask;
	assert(maskIndex <= UINT8_MAX); // lookup into 256-byte table
	assert(index <= UINT8_MAX);     // lookup into 256-byte table
	VGA_DAC_SendColor(static_cast<uint8_t>(index), static_cast<uint8_t>(maskIndex));
}

static void write_p3c6(io_port_t, uint8_t val, io_width_t)
{
	if (vga.dac.pel_mask != val) {
		LOG(LOG_VGAMISC, LOG_NORMAL)("VGA:DCA:Pel Mask set to %X", val);
		vga.dac.pel_mask = val;
		for (uint16_t i = 0; i < 256; i++)
			VGA_DAC_UpdateColor( i );
	}
}

static uint8_t read_p3c6(io_port_t, io_width_t)
{
	return vga.dac.pel_mask;
}

static void write_p3c7(io_port_t, uint8_t val, io_width_t)
{
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

static void write_p3c8(io_port_t, uint8_t val, io_width_t)
{
	vga.dac.write_index = val;
	vga.dac.pel_index = 0;
	vga.dac.state = DAC_WRITE;
	vga.dac.read_index = val - 1;
}

static uint8_t read_p3c8(Bitu, io_width_t)
{
	return vga.dac.write_index;
}

static void write_p3c9(io_port_t, uint8_t val, io_width_t)
{
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
			VGA_DAC_UpdateColor( vga.dac.write_index );
			if ( GCC_UNLIKELY( vga.dac.pel_mask != 0xff)) {
				const auto index = vga.dac.write_index;
				if ( (index & vga.dac.pel_mask) == index ) {
					for (uint16_t i = index + 1u; i < 256; i++)
						if ( (i & vga.dac.pel_mask) == index )
							VGA_DAC_UpdateColor(static_cast<uint8_t>(i));
				}
			} 
			break;
		default:
			/* Check for attributes and DAC entry link */
			for (uint8_t i = 0; i < 16; i++) {
				if (vga.dac.combine[i]==vga.dac.write_index) {
					VGA_DAC_SendColor( i, vga.dac.write_index );
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
	Bit8u ret;
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

void VGA_DAC_CombineColor(Bit8u attr,Bit8u pal) {
	/* Check if this is a new color */
	vga.dac.combine[attr]=pal;
	switch (vga.mode) {
	case M_LIN8:
		break;
	case M_VGA:
		// used by copper demo; almost no video card seems to suport it
		if (!IS_VGA_ARCH || (svgaCard != SVGA_None))
			break;
		FALLTHROUGH;
	default:
		VGA_DAC_SendColor( attr, pal );
	}
}

void VGA_DAC_SetEntry(Bitu entry,Bit8u red,Bit8u green,Bit8u blue) {
	//Should only be called in machine != vga
	vga.dac.rgb[entry].red=red;
	vga.dac.rgb[entry].green=green;
	vga.dac.rgb[entry].blue=blue;
	for (uint8_t i = 0; i < 16; i++)
		if (vga.dac.combine[i]==entry)
			VGA_DAC_SendColor( i, i );
}

void VGA_SetupDAC(void) {
	vga.dac.first_changed=256;
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
