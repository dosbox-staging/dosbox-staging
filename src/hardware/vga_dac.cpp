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

#include "../ints/int10.h"
#include "inout.h"
#include "mem.h"
#include "reelmagic.h"
#include "render.h"
#include "rgb.h"
#include "vga.h"

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

enum { DacRead, DacWrite };

static bool is_cga_color(const Rgb666 color)
{
	return std::find(palette.cga16.cbegin(), palette.cga16.cend(), color) !=
	       palette.cga16.cend();
}

static bool is_ega_color(const Rgb666 color)
{
	return std::find(palette.ega.cbegin(), palette.ega.cend(), color) !=
	       palette.ega.cend();
}

static void vga_dac_send_color(const uint8_t palette_idx, const uint8_t color_idx)
{
	const auto rgb666 = vga.dac.rgb[color_idx];

	// We might be in the middle of a mode change, so we can't use
	// VGA_GetCurrentVideoMode() here. That's because the INT 10H mode
	// change BIOS routine needs to set up the Palette and Color Registers
	// which will trigger this function.
	const auto bios_mode_number = CurMode->mode;

	// In the automatic "video mode specific" CRT emulation mode, we want
	// "true EGA" games (EGA modes with the default EGA palette on VGA) to
	// use the single-scanline EGA shader. But VGA games that just happen to
	// use EGA modes but with a 18-bit VGA palette should be rendered with
	// the double-scanned VGA shader.
	//
	// This is accomplished by setting the `ega_mode_with_vga_colors` flag
	// to true when the first non-EGA palette colour is set after a mode
	// switch.
	//
	// Note that custom CGA colours (via the `cga_colors` setting) are
	// handled correctly as well.
	//
	if (machine == MCH_VGA && !vga.ega_mode_with_vga_colors &&
	    bios_mode_number <= MaxEgaBiosModeNumber) {
		bool non_ega_color = false;

		const auto is_640x350_16color_mode = (bios_mode_number == 0x10);

		if (is_640x350_16color_mode) {
			// The 640x350 16-colour EGA mode (mode 10h) is special:
			// the 16 colors can be freely chosen from a gamut of 64
			// colours (6-bit RGB).
			non_ega_color = !is_ega_color(rgb666);
		} else {
			// In all other EGA modes, the fixed "canonical
			// 16-element CGA palette" (as emulated by VGA cards) is
			// used.
			non_ega_color = !is_cga_color(rgb666);
		}

		if (non_ega_color) {
			vga.ega_mode_with_vga_colors = true;

			// If we're inside a mode change, the
			// `ega_mode_with_vga_color` will be taken into account
			// in VGA_GetCurrentVideoMode() which concludes the mode
			// change process.
			//
			// But if a palette entry was set to a non-EGA colour
			// after the mode change was completed, we need to
			// notify the renderer so it can re-init itself and
			// potentially switch the current shader.
			if (!vga.mode_change_in_progress) {
				RENDER_NotifyEgaModeWithVgaPalette();
			}
		}
	}

	const auto r8 = rgb6_to_8_lut(rgb666.red);
	const auto g8 = rgb6_to_8_lut(rgb666.green);
	const auto b8 = rgb6_to_8_lut(rgb666.blue);

	// Map the source color into palette's requested index
	vga.dac.palette_map[palette_idx].Set(b8, g8, r8);

	ReelMagic_RENDER_SetPalette(palette_idx, r8, g8, b8);
}

static void vga_dac_update_color(const uint8_t palette_idx)
{
	const uint8_t color_idx = palette_idx & vga.dac.pel_mask;

	vga_dac_send_color(palette_idx, color_idx);
}

static void write_p3c6(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	if (vga.dac.pel_mask != val) {
#if 0
		LOG_MSG("VGA:DCA: PEL mask set to %Xh", val);
#endif
		vga.dac.pel_mask = val;

		for (auto i = 0; i < NumVgaColors; ++i) {
			const auto palette_idx = check_cast<uint8_t>(i);
			vga_dac_update_color(palette_idx);
		}
	}
}

static uint8_t read_p3c6(io_port_t, io_width_t)
{
	return vga.dac.pel_mask;
}

static void write_p3c7(io_port_t, io_val_t value, io_width_t)
{
	const auto val      = check_cast<uint8_t>(value);
	vga.dac.read_index  = val;
	vga.dac.pel_index   = 0;
	vga.dac.state       = DacRead;
	vga.dac.write_index = val + 1;
}

static uint8_t read_p3c7(io_port_t, io_width_t)
{
	if (vga.dac.state == DacRead) {
		return 0x3;
	} else {
		return 0x0;
	}
}

static void write_p3c8(io_port_t, io_val_t value, io_width_t)
{
	const auto val      = check_cast<uint8_t>(value);
	vga.dac.write_index = val;
	vga.dac.pel_index   = 0;
	vga.dac.state       = DacWrite;
	vga.dac.read_index  = val - 1;
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
		vga.dac.rgb[vga.dac.write_index].red = val;

		vga.dac.pel_index = 1;
		break;

	case 1:
		vga.dac.rgb[vga.dac.write_index].green = val;

		vga.dac.pel_index = 2;
		break;

	case 2:
		vga.dac.rgb[vga.dac.write_index].blue = val;
		switch (vga.mode) {
		case M_VGA:
		case M_LIN8:
			vga_dac_update_color(vga.dac.write_index);

			if (GCC_UNLIKELY(vga.dac.pel_mask != 0xff)) {
				const auto index = vga.dac.write_index;

				if ((index & vga.dac.pel_mask) == index) {
					for (auto i = index + 1u; i < NumVgaColors;
					     ++i) {
						const auto palette_idx =
						        check_cast<uint8_t>(i);

						if ((palette_idx &
						     vga.dac.pel_mask) == index) {
							vga_dac_update_color(
							        palette_idx);
						}
					}
				}
			}
			break;

		default:
			// Check for attributes and DAC entry link
			for (uint8_t i = 0; i < NumCgaColors; ++i) {
				const auto palette_idx = i;
				const auto color_idx   = vga.dac.write_index;

				if (vga.dac.combine[palette_idx] == color_idx) {
					vga_dac_send_color(palette_idx, color_idx);
				}
			}
		}

		++vga.dac.write_index;
		// vga.dac.read_index = vga.dac.write_index - ;
		// disabled as it breaks Wari

		vga.dac.pel_index = 0;
		break;

	default:
#if 0
		LOG_WARNING("Invalid VGA PEL index: %d", vga.dac.pel_index);
#endif
		break;
	};
}

static uint8_t read_p3c9(io_port_t, io_width_t)
{
	switch (vga.dac.pel_index) {
	case 0:
		vga.dac.pel_index = 1;
		return vga.dac.rgb[vga.dac.read_index].red;

	case 1:
		vga.dac.pel_index = 2;
		return vga.dac.rgb[vga.dac.read_index].green;

	case 2: {
		vga.dac.pel_index = 0;

		auto blue = vga.dac.rgb[vga.dac.read_index].blue;
		++vga.dac.read_index;
		// vga.dac.write_index=vga.dac.read_index+1;
		// disabled as it breaks wari
		return blue;
	}

	default:
#if 0
		LOG_WARNING("Invalid VGA PEL index: %d", vga.dac.pel_index);
#endif
		return {};
	}
}

void VGA_DAC_CombineColor(const uint8_t palette_idx, const uint8_t color_idx)
{
	vga.dac.combine[palette_idx] = color_idx;

	if (vga.mode != M_LIN8) {
		// Used by copper demo; almost no video card seems to support it
		vga_dac_send_color(palette_idx, color_idx);
	}
}

void VGA_DAC_SetEntry(const uint8_t color_idx, const uint8_t red,
                      const uint8_t green, const uint8_t blue)
{
	// Should only be called for non-VGA machine types
	vga.dac.rgb[color_idx].red   = red;
	vga.dac.rgb[color_idx].green = green;
	vga.dac.rgb[color_idx].blue  = blue;

	for (uint8_t i = 0; i < NumCgaColors; ++i) {
		const auto palette_idx = i;
		if (vga.dac.combine[palette_idx] == color_idx) {
			vga_dac_send_color(palette_idx, palette_idx);
		}
	}
}

void VGA_SetupDAC(void)
{
	vga.dac.bits        = 6;
	vga.dac.pel_mask    = 0xff;
	vga.dac.pel_index   = 0;
	vga.dac.state       = DacRead;
	vga.dac.read_index  = 0;
	vga.dac.write_index = 0;

	if (IS_VGA_ARCH) {
		// Set up the DAC IO port handlers
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
