// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"
#include "hardware/inout.h"
#include "hardware/pic.h"
#include "vga.h"
#include <cmath>

void vga_write_p3d4(io_port_t port, io_val_t value, io_width_t);
uint8_t vga_read_p3d4(io_port_t port, io_width_t);
void vga_write_p3d5(io_port_t port, io_val_t value, io_width_t);
uint8_t vga_read_p3d5(io_port_t port, io_width_t);

uint8_t vga_read_p3da(io_port_t, io_width_t)
{
	uint8_t retval = 4; // bit 2 set, needed by Blues Brothers
	const auto timeInFrame = PIC_FullIndex() - vga.draw.delay.framestart;

	vga.attr.is_address_mode = true;
	vga.tandy.pcjr_flipflop=false;

	// 3DAh (R):  Status Register
	// bit   0  Horizontal or Vertical blanking
	//       3  Vertical sync

	if (timeInFrame >= vga.draw.delay.vrstart &&
		timeInFrame <= vga.draw.delay.vrend)
		retval |= 8;
	if (timeInFrame >= vga.draw.delay.vdend) {
		retval |= 1;
	} else {
		const auto timeInLine = fmod(timeInFrame, vga.draw.delay.htotal);
		if (timeInLine >= vga.draw.delay.hblkstart && 
			timeInLine <= vga.draw.delay.hblkend) {
			retval |= 1;
		}
	}
	return retval;
}

static void write_p3c2(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	/*
	   Bit  Description
	    0   If set: Color Emulation with base Address=3Dxh.
	        If not set: Mono Emulation with Base Address=3Bxh.

	        The even and odd port ranges: 3[d/b][0-7]h map to 3[d/b][4|5]h.
	        (mapping ref: https://bochs.sourceforge.io/techspec/PORTS.LST)

	   2-3  Clock Select. 0: 25MHz, 1: 28MHz

	    5   When in Odd/Even modes Select High 64k bank if set

	    6   Horizontal Sync Polarity. Negative if set

	    7   Vertical Sync Polarity. Negative if set
	        Bit 6-7 indicates the number of lines on the display:
	        1:  400, 2: 350, 3: 480
	        Note: Set to all zero on a hardware reset.
	        Note: This register can be read from port 3CCh.
	*/
	vga.misc_output = val;

	const bool is_color = val & 0x1;

	const io_port_t active_base = is_color ? 0x3d0 : 0x3b0;

	for (auto port = active_base; port <= active_base + 6; port += 2) {
		IO_RegisterWriteHandler(port, vga_write_p3d4, io_width_t::byte);
		IO_RegisterReadHandler(port, vga_read_p3d4, io_width_t::byte);

		IO_RegisterWriteHandler(port + 1, vga_write_p3d5, io_width_t::byte);
		IO_RegisterReadHandler(port + 1, vga_read_p3d5, io_width_t::byte);
	}

	const io_port_t inactive_base = is_color ? 0x3b0 : 0x3d0;

	for (auto port = inactive_base; port <= inactive_base + 6; port += 2) {
		IO_FreeWriteHandler(port, io_width_t::byte);
		IO_FreeReadHandler(port, io_width_t::byte);

		IO_FreeWriteHandler(port + 1, io_width_t::byte);
		IO_FreeReadHandler(port + 1, io_width_t::byte);
	}

	IO_RegisterReadHandler(active_base + 0xa, vga_read_p3da, io_width_t::byte);
	IO_FreeReadHandler(inactive_base + 0xa, io_width_t::byte);
}

static uint8_t read_p3cc(io_port_t, io_width_t)
{
	return vga.misc_output;
}

// VGA feature control register
static uint8_t read_p3ca(io_port_t, io_width_t)
{
	return 0;
}

static uint8_t read_p3c8(io_port_t, io_width_t)
{
	return 0x10;
}

static uint8_t read_p3c2(io_port_t, io_width_t)
{
	uint8_t retval = 0;

	if (is_machine_ega()) {
		retval = 0x0f;
	} else if (is_machine_vga_or_better()) {
		retval = 0x60;
	}

	if (is_machine_vga_or_better() ||
	    (((vga.misc_output >> 2) & 3) == 0) ||
	    (((vga.misc_output >> 2) & 3) == 3)) {
		retval |= 0x10;
	}

	if (vga.draw.vret_triggered) retval |= 0x80;
	return retval;
	/*
		0-3 0xF on EGA, 0x0 on VGA 
		4	Status of the switch selected by the Miscellaneous Output
			Register 3C2h bit 2-3. Switch high if set.
			(apparently always 1 on VGA)
		5	(EGA) Pin 19 of the Feature Connector (FEAT0) is high if set
		6	(EGA) Pin 17 of the Feature Connector (FEAT1) is high if set
			(default differs by card, ET4000 sets them both)
		7	If set IRQ 2 has happened due to Vertical Retrace.
			Should be cleared by IRQ 2 interrupt routine by clearing port 3d4h
			index 11h bit 4.
	*/
}

void VGA_SetupMisc()
{
	if (is_machine_ega_or_better()) {
		vga.draw.vret_triggered=false;
		IO_RegisterReadHandler(0x3c2, read_p3c2, io_width_t::byte);
		IO_RegisterWriteHandler(0x3c2, write_p3c2, io_width_t::byte);
		if (is_machine_vga_or_better()) {
			IO_RegisterReadHandler(0x3ca, read_p3ca, io_width_t::byte);
			IO_RegisterReadHandler(0x3cc, read_p3cc, io_width_t::byte);
		} else {
			IO_RegisterReadHandler(0x3c8, read_p3c8, io_width_t::byte);
		}
	} else if (is_machine_cga() || is_machine_pcjr_or_tandy()) {
		IO_RegisterReadHandler(0x3da, vga_read_p3da, io_width_t::byte);
	}
}
