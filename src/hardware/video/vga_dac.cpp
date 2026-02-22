// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include <cassert>

#include "vga.h"

#include "gui/render/render.h"
#include "hardware/memory.h"
#include "hardware/port.h"
#include "hardware/video/reelmagic/reelmagic.h"
#include "ints/int10.h"
#include "utils/rgb.h"

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

// In the automatic "video mode specific" CRT emulation mode (`shader =
// crt-auto`), we want "true EGA" games on emulated VGA adapters to use the
// single scanline EGA shader. "True EGA" games set up an EGA mode and don't
// change the palette to use 18-bit VGA colours. These games look identical on
// VGA and EGA, except for the VGA double scanning.
//
// Some games (most notably Amiga and Atari ST ports) "repurpose" the
// 16-colour EGA modes on VGA: they set up an EGA mode first, then change the
// default CGA/EGA palette to a custom set of sixteen 18-bit RGB colours (many
// Amiga and Atari ST games use a 16-colour palette out of the 4096 (Amiga) or
// 512 (Atari ST) available colours). As these games can only run on VGA
// adapters, we double scan them in `crt-auto` mode, although it can be argued
// that this case calls for single scanning to mimic the single-scanned home
// computer monitor look. But we're emulating PC compatibles here and the aim
// is to accurately replicate how people experienced these games on PC
// hardware, so double scanning it is.
//
// Detecting EGA modes using custom VGA colours is accomplished by setting the
// `ega_mode_with_vga_colors` flag to true when the first non-EGA palette
// colour is set after a mode change has been completed.
//
// Note that custom CGA colours (via the `cga_colors` config setting) are
// handled correctly as well.
//
static void vga_dac_send_color(const uint8_t palette_idx, const uint8_t color_idx)
{
	const auto rgb666 = vga.dac.rgb[color_idx];

	constexpr auto ega_mode_640x350_16color = 0x10;
#if 0
	const auto log_warning = (CurMode->mode == ega_mode_640x350_16color)
	                               ? !is_ega_color(rgb666)
	                               : !is_cga_color(rgb666);

	const auto msg = format_str("palette_idx: %d, color_idx: %d",
	                            palette_idx,
	                            color_idx);
	if (log_warning) {
		LOG_WARNING("VGA: %s, color: %02x %02x %02x",
		            msg.c_str(),
		            rgb666.red,
		            rgb666.green,
		            rgb666.blue);
	} else {
		LOG_TRACE("VGA: %s", msg.c_str());
	}
#endif
	// We only want to trigger the "VGA DAC colours in EGA mode" detection
	// logic when we're outside of a video mode change. Mode changes also
	// set up the default CGA and EGA palette appropriate for the given
	// mode, and that would only confuse and complicate the detection logic.
	//
	// In theory, if a program completely bypassed the INT 10h set video
	// mode call and performed the mode change 100% itself by writing to the
	// VGA registers directly, that would cause this logic not to trigger.
	// Fortunately, no commercial game developers seemed to use such
	// horrible practices.
	//
	if (is_machine_vga_or_better() && !INT10_VideoModeChangeInProgress() &&
	    !vga.ega_mode_with_vga_colors) {

		// Even thought the video mode change has been completed at this
		// point at the BIOS interrupt level, the actual changing of the
		// resolution is probably yet to be performed (that's delayed by
		// up to ~50 ms to let the VGA register state "stabilise" before
		// calculating the new timings), so we can't call
		// VGA_GetCurrentVideoMode() here.
		const auto curr_mode = CurMode->mode;

		const auto is_non_ega_color = [&]() {
			if (curr_mode == ega_mode_640x350_16color) {
				// The 640x350 16-colour EGA mode (mode 10h) is
				// special: the 16 colors can be freely chosen
				// from a gamut of 64 colours (6-bit RGB).
				return !is_ega_color(rgb666);
			} else {
				// In all other EGA modes, the fixed "canonical
				// 16-element CGA palette" (as emulated by VGA
				// cards) is used.
				return !is_cga_color(rgb666);
			}
		};

		if (curr_mode <= MaxEgaBiosModeNumber &&
		    is_non_ega_color()) {

			vga.ega_mode_with_vga_colors = true;

			LOG_DEBUG(
			        "VGA: EGA mode with VGA palette detected, "
			        "notifying renderer");

			// Notify the renderer so it can re-init itself and
			// potentially switch the current shader (i.e., from an
			// EGA shader to a VGA one).
			RENDER_NotifyEgaModeWithVgaPalette();
		}
	}

	const auto r8 = rgb6_to_8_lut(rgb666.red);
	const auto g8 = rgb6_to_8_lut(rgb666.green);
	const auto b8 = rgb6_to_8_lut(rgb666.blue);

	// Map the source color into palette's requested index
	vga.dac.palette_map[palette_idx] = Bgrx8888(r8, g8, b8);

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

			if (vga.dac.pel_mask != 0xff) {
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

	switch (vga.mode) {
	case M_LIN8: break;
	case M_VGA:
		// Mimic the legacy palette behaviour when emulating the Paradise
		// card (the oldest SVGA card we emulate). This fixes the wrong
		// colours appearing in some rare titles (e.g., Spell It Plus).
		if (svga_type == SvgaType::Paradise) {
			break;
		}
		[[fallthrough]];
	default: vga_dac_send_color(palette_idx, color_idx);
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

	if (is_machine_vga_or_better()) {
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
