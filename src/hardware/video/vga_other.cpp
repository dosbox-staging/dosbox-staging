// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "vga.h"

#include "config/config.h"
#include "gui/mapper.h"
#include "gui/render/render.h"
#include "hardware/memory.h"
#include "hardware/pic.h"
#include "hardware/port.h"
#include "hardware/video/reelmagic/reelmagic.h"
#include "ints/int10.h"
#include "utils/bitops.h"
#include "utils/checks.h"
#include "utils/math_utils.h"
#include "utils/rgb888.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

static void write_crtc_index_other(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	// only receives 8-bit data per its IO port registration
	vga.other.index = val;
}

static uint8_t read_crtc_index_other(io_port_t, io_width_t)
{
	// only returns 8-bit data per its IO port registration
	return vga.other.index;
}

static void write_crtc_data_other(io_port_t, io_val_t value, io_width_t)
{
	// only receives 8-bit data per its IO port registration
	auto val = check_cast<uint8_t>(value);

	switch (vga.other.index) {
	case 0x00:		//Horizontal total
		if (vga.other.htotal ^ val) VGA_StartResize();
		vga.other.htotal = val;
		break;
	case 0x01:		//Horizontal displayed chars
		if (vga.other.hdend ^ val) VGA_StartResize();
		vga.other.hdend = val;
		break;
	case 0x02:		//Horizontal sync position
		vga.other.hsyncp = val;
		break;
	case 0x03:		//Horizontal sync width
		if (is_machine_tandy()) {
			vga.other.vsyncw = val >> 4;
		} else {
			// The MC6845 has a fixed v-sync width of 16 lines
			vga.other.vsyncw = 16;
		}
		vga.other.hsyncw = val & 0xf;
		break;
	case 0x04:		//Vertical total
		if (vga.other.vtotal ^ val) VGA_StartResize();
		vga.other.vtotal = val;
		break;
	case 0x05:		//Vertical display adjust
		if (vga.other.vadjust ^ val) VGA_StartResize();
		vga.other.vadjust = val;
		break;
	case 0x06:		//Vertical rows
		// Impossible Mission II sets this to zero briefly
		// This leads to a divide by zero crash if VGA resize code is run
		if (val > 0 && val != vga.other.vdend) {
			vga.other.vdend = val;
			// The default half-frame period delay leads to flickering in the level start zoom effect of Impossible Mission II on Tandy
			VGA_StartResizeAfter(50);
		}
		break;
	case 0x07:		//Vertical sync position
		vga.other.vsyncp = val;
		break;
	case 0x09:		//Max scanline
		val &= 0x1f; // VGADOC says bit 0-3 but the MC6845 datasheet says bit 0-4
 		if (vga.other.max_scanline ^ val) VGA_StartResize();
		vga.other.max_scanline = val;
		break;
	case 0x0A:	/* Cursor Start Register */
		vga.other.cursor_start = val & 0x3f;
		vga.draw.cursor.sline = val & 0x1f;
		vga.draw.cursor.enabled = ((val & 0x60) != 0x20);
		break;
	case 0x0B:	/* Cursor End Register */
		vga.other.cursor_end = val & 0x1f;
		vga.draw.cursor.eline = val & 0x1f;
		break;
	case 0x0C:	/* Start Address High Register */
		// Bit 12 (depending on video mode) and 13 are actually masked too,
		// but so far no need to implement it.
		vga.config.display_start = (vga.config.display_start & 0x00FF) |
		                           static_cast<uint16_t>((val & 0x3F) << 8);
		break;
	case 0x0D:	/* Start Address Low Register */
		vga.config.display_start=(vga.config.display_start & 0xFF00) | val;
		break;
	case 0x0E:	/*Cursor Location High Register */
		vga.config.cursor_start &= 0x00ff;
		vga.config.cursor_start |= static_cast<uint16_t>(val << 8);
		break;
	case 0x0F: /* Cursor Location Low Register */
		vga.config.cursor_start &= 0xff00;
		vga.config.cursor_start |= val;
		break;
	case 0x10:	/* Light Pen High */
		vga.other.lightpen &= 0xff;
		vga.other.lightpen |= (val & 0x3f)<<8;		// only 6 bits
		break;
	case 0x11:	/* Light Pen Low */
		vga.other.lightpen &= 0xff00;
		vga.other.lightpen |= val;
		break;
	default:
		LOG(LOG_VGAMISC, LOG_NORMAL)("MC6845:Write %u to illegal index %x", val, vga.other.index);
	}
}
static uint8_t read_crtc_data_other(io_port_t, io_width_t)
{
	// only returns 8-bit data per its IO port registration
	switch (vga.other.index) {
	case 0x00:		//Horizontal total
		return vga.other.htotal;
	case 0x01:		//Horizontal displayed chars
		return vga.other.hdend;
	case 0x02:		//Horizontal sync position
		return vga.other.hsyncp;
	case 0x03:		//Horizontal and vertical sync width
		// hsyncw and vsyncw should only be populated with their lower 4-bits
		assert(vga.other.hsyncw >> 4 == 0);
		assert(vga.other.vsyncw >> 4 == 0);
		if (is_machine_tandy()) {
			return static_cast<uint8_t>(vga.other.hsyncw | (vga.other.vsyncw << 4));
		} else {
			return vga.other.hsyncw;
		}
	case 0x04:		//Vertical total
		return vga.other.vtotal;
	case 0x05:		//Vertical display adjust
		return vga.other.vadjust;
	case 0x06:		//Vertical rows
		return vga.other.vdend;
	case 0x07:		//Vertical sync position
		return vga.other.vsyncp;
	case 0x09:		//Max scanline
		return vga.other.max_scanline;
	case 0x0A:	/* Cursor Start Register */
		return vga.other.cursor_start;
	case 0x0B:	/* Cursor End Register */
		return vga.other.cursor_end;
	case 0x0C:	/* Start Address High Register */
		return static_cast<uint8_t>(vga.config.display_start >> 8);
	case 0x0D:	/* Start Address Low Register */
		return static_cast<uint8_t>(vga.config.display_start & 0xff);
	case 0x0E:	/*Cursor Location High Register */
		return static_cast<uint8_t>(vga.config.cursor_start >> 8);
	case 0x0F:	/* Cursor Location Low Register */
		return static_cast<uint8_t>(vga.config.cursor_start & 0xff);
	case 0x10:	/* Light Pen High */
		return static_cast<uint8_t>(vga.other.lightpen >> 8);
	case 0x11:	/* Light Pen Low */
		return static_cast<uint8_t>(vga.other.lightpen & 0xff);
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("MC6845:Read from illegal index %x",vga.other.index);
	}
	return static_cast<uint8_t>(~0);
}

static void write_lightpen(io_port_t port, io_val_t, io_width_t)
{
	// only receives 8-bit data per its IO port registration
	switch (port) {
	case 0x3db:	// Clear lightpen latch
		vga.other.lightpen_triggered = false;
		break;
	case 0x3dc:	// Preset lightpen latch
		if (!vga.other.lightpen_triggered) {
			vga.other.lightpen_triggered = true; // TODO: this shows at port 3ba/3da bit 1

			const auto timeInFrame = PIC_FullIndex() - vga.draw.delay.framestart;
			const auto timeInLine = fmod(timeInFrame, vga.draw.delay.htotal);
			auto current_scanline = (Bitu)(timeInFrame / vga.draw.delay.htotal);

			vga.other.lightpen = (uint16_t)((vga.draw.address_add/2) * (current_scanline/2));
			vga.other.lightpen += static_cast<uint16_t>(
			        (timeInLine / vga.draw.delay.hdend) *
			        (static_cast<double>(vga.draw.address_add / 2)));
		}
		break;
	}
}

class Knob {
public:
	Knob() = delete;
	Knob(const int default_value, const int min_value, const int max_value)
	        : def(default_value),
	          min(min_value),
	          max(max_value),
	          val(default_value)
	{
		assert(def == val);
		assert(min <= val && val <= max);
		assert(min <= max);
	}
	// modify the value
	int get() const { return val; }
	void set(int new_val) { val = wrap(new_val, min, max); }
	void turn(int amount) { set(val + amount); }
	void reset() { set(def); }

	// read-only functions to get constraints
	float AsFloat() const
	{
		return static_cast<float>(val);
	}

	int GetDefaultValue() const
	{
		return def;
	}
	int GetMinValue() const
	{
		return min;
	}
	int GetMaxValue() const
	{
		return max;
	}

private:
	const int def; // "default" is a C++ keyword (don't use it)
	const int min;
	const int max;
	int val;
};

static Knob hue(0, -360, 360);
static Knob convergence(0, -50, 50);

enum class CompositeState {
	Auto = 0,
	On,
	Off,
};

static CompositeState cga_comp   = CompositeState::Auto;
static bool is_composite_new_era = false;

static MonochromePalette hercules_palette = {};
static MonochromePalette mono_cga_palette = {};

// clang-format off

// Monochrome CGA palettes with contrast optimised for 4-colour CGA graphics modes
static constexpr Rgb888 mono_cga_graphics_palettes[NumMonochromePalettes][NumCgaColors] = {
	{
		// 0 - Amber
		{0x00, 0x00, 0x00}, {0x15, 0x05, 0x00}, {0x20, 0x0b, 0x00}, {0x24, 0x0d, 0x00},
		{0x33, 0x18, 0x00}, {0x37, 0x1b, 0x00}, {0x3f, 0x26, 0x01}, {0x3f, 0x2b, 0x06},
		{0x0b, 0x02, 0x00}, {0x1b, 0x08, 0x00}, {0x29, 0x11, 0x00}, {0x2e, 0x14, 0x00},
		{0x3b, 0x1e, 0x00}, {0x3e, 0x21, 0x00}, {0x3f, 0x32, 0x0a}, {0x3f, 0x38, 0x0d},
	}, {
		// 1 - Green
		{0x00, 0x00, 0x00}, {0x00, 0x0d, 0x03}, {0x01, 0x17, 0x05}, {0x01, 0x1a, 0x06},
		{0x02, 0x28, 0x09}, {0x02, 0x2c, 0x0a}, {0x03, 0x39, 0x0d}, {0x03, 0x3c, 0x0e},
		{0x00, 0x07, 0x01}, {0x01, 0x13, 0x04}, {0x01, 0x1f, 0x07}, {0x01, 0x23, 0x08},
		{0x02, 0x31, 0x0b}, {0x02, 0x35, 0x0c}, {0x05, 0x3f, 0x11}, {0x0d, 0x3f, 0x17},
	}, {
		// 2 - White
		{0x00, 0x00, 0x00}, {0x0d, 0x0d, 0x0d}, {0x15, 0x15, 0x15}, {0x18, 0x18, 0x18},
		{0x24, 0x24, 0x24}, {0x27, 0x27, 0x27}, {0x33, 0x33, 0x33}, {0x37, 0x37, 0x37},
		{0x08, 0x08, 0x08}, {0x10, 0x10, 0x10}, {0x1c, 0x1c, 0x1c}, {0x20, 0x20, 0x20},
		{0x2c, 0x2c, 0x2c}, {0x2f, 0x2f, 0x2f}, {0x3b, 0x3b, 0x3b}, {0x3f, 0x3f, 0x3f},
	}, {
		// 3 - Paperwhite
		{0x00, 0x00, 0x00}, {0x0e, 0x0f, 0x10}, {0x15, 0x17, 0x18}, {0x18, 0x1a, 0x1b},
		{0x24, 0x25, 0x25}, {0x27, 0x28, 0x28}, {0x33, 0x34, 0x32}, {0x37, 0x38, 0x35},
		{0x09, 0x0a, 0x0b}, {0x11, 0x12, 0x13}, {0x1c, 0x1e, 0x1e}, {0x20, 0x22, 0x22},
		{0x2c, 0x2d, 0x2c}, {0x2f, 0x30, 0x2f}, {0x3c, 0x3c, 0x38}, {0x3f, 0x3f, 0x3b},
	}
};

// Monochrome CGA palettes with contrast optimised for 16-colour CGA text modes
static constexpr Rgb888 mono_cga_text_palettes[NumMonochromePalettes][NumCgaColors] = {
	{
		// 0 - Amber
		{0x00, 0x00, 0x00}, {0x15, 0x05, 0x00}, {0x1e, 0x09, 0x00}, {0x21, 0x0b, 0x00},
		{0x2b, 0x12, 0x00}, {0x2f, 0x15, 0x00}, {0x38, 0x1c, 0x00}, {0x3b, 0x1e, 0x00},
		{0x2c, 0x13, 0x00}, {0x32, 0x17, 0x00}, {0x3a, 0x1e, 0x00}, {0x3c, 0x1f, 0x00},
		{0x3f, 0x27, 0x01}, {0x3f, 0x2a, 0x04}, {0x3f, 0x36, 0x0c}, {0x3f, 0x38, 0x0d},
	}, {
		// 1 - Green
		{0x00, 0x00, 0x00}, {0x00, 0x0d, 0x03}, {0x01, 0x15, 0x05}, {0x01, 0x17, 0x05},
		{0x01, 0x21, 0x08}, {0x01, 0x24, 0x08}, {0x02, 0x2e, 0x0b}, {0x02, 0x31, 0x0b},
		{0x01, 0x22, 0x08}, {0x02, 0x28, 0x09}, {0x02, 0x30, 0x0b}, {0x02, 0x32, 0x0c},
		{0x03, 0x39, 0x0d}, {0x03, 0x3b, 0x0e}, {0x09, 0x3f, 0x14}, {0x0d, 0x3f, 0x17},
	}, {
		// 2 - White
		{0x00, 0x00, 0x00}, {0x0d, 0x0d, 0x0d}, {0x12, 0x12, 0x12}, {0x15, 0x15, 0x15},
		{0x1e, 0x1e, 0x1e}, {0x20, 0x20, 0x20}, {0x29, 0x29, 0x29}, {0x2c, 0x2c, 0x2c},
		{0x1f, 0x1f, 0x1f}, {0x23, 0x23, 0x23}, {0x2b, 0x2b, 0x2b}, {0x2d, 0x2d, 0x2d},
		{0x34, 0x34, 0x34}, {0x36, 0x36, 0x36}, {0x3d, 0x3d, 0x3d}, {0x3f, 0x3f, 0x3f},
	}, {
		// 3 - Paperwhite
		{0x00, 0x00, 0x00}, {0x0e, 0x0f, 0x10}, {0x13, 0x14, 0x15}, {0x15, 0x17, 0x18},
		{0x1e, 0x20, 0x20}, {0x20, 0x22, 0x22}, {0x29, 0x2a, 0x2a}, {0x2c, 0x2d, 0x2c},
		{0x1f, 0x21, 0x21}, {0x23, 0x25, 0x25}, {0x2b, 0x2c, 0x2b}, {0x2d, 0x2e, 0x2d},
		{0x34, 0x35, 0x33}, {0x37, 0x37, 0x34}, {0x3e, 0x3e, 0x3a}, {0x3f, 0x3f, 0x3b},
	}
};
// clang-format on

static constexpr float get_rgbi_coefficient(const bool is_new_cga,
                                            const uint8_t overscan)
{
	const auto r_coefficient = is_new_cga ? 0.10f : 0.0f;
	const auto g_coefficient = is_new_cga ? 0.22f : 0.0f;
	const auto b_coefficient = is_new_cga ? 0.07f : 0.0f;
	const auto i_coefficient = is_new_cga ? 0.32f : 0.28f;

	const auto r = (overscan & 4) ? r_coefficient : 0.0f;
	const auto g = (overscan & 2) ? g_coefficient : 0.0f;
	const auto b = (overscan & 1) ? b_coefficient : 0.0f;
	const auto i = (overscan & 8) ? i_coefficient : 0.0f;
	return r + g + b + i;
}

constexpr auto Brightness = 0.0f;
constexpr auto Contrast   = 100.0f;
constexpr auto Saturation = 100.0f;

static void update_cga16_color_pcjr()
{
	assert(is_machine_pcjr());

	// First composite algorithm based on code by reenigne updated by
	// NewRisingSun and tailored for PCjr-only composite modes (for DOSBox
	// Staging).
	constexpr auto tau = 6.28318531f;    // == 2*pi
	constexpr auto ns = 567.0f / 440.0f; // degrees of hue shift per nanosecond

	const auto tv_brightness = Brightness / 100.0f;
	const auto tv_saturation = Saturation / 100.0f;
	const auto tv_contrast   = (1 - tv_brightness) * Contrast / 100.0f;

	const bool bw = vga.tandy.mode.is_black_and_white_mode;
	const bool bpp1 = vga.tandy.mode_control.is_pcjr_640x200_2_color_graphics;

	std::array<float, NumCgaColors> rgbi_coefficients = {};
	for (uint8_t c = 0; c < rgbi_coefficients.size(); c++)
		rgbi_coefficients[c] = get_rgbi_coefficient(is_composite_new_era, c);

	// The pixel clock delay calculation is not accurate for 2bpp, but the
	// difference is small and a more accurate calculation would be too slow.
	constexpr auto rgbi_pixel_delay = 15.5f * ns;

	constexpr float chroma_pixel_delays[8] = {
	        0.0f,       // Black:   no chroma
	        35.0f * ns, // Blue:    no XORs
	        44.5f * ns, // Green:   XOR on rising and falling edges
	        39.5f * ns, // Cyan:    XOR on falling but not rising edge
	        44.5f * ns, // Red:     XOR on rising and falling edges
	        39.5f * ns, // Magenta: XOR on falling but not/ rising edge
	        44.5f * ns, // Yellow:  XOR on rising and falling edges
	        39.5f * ns  // White:   XOR on falling but not rising edge
	};

	constexpr uint8_t overscan = 15;
	constexpr auto cp_d = chroma_pixel_delays[overscan & 7];
	const auto rgbi_d = rgbi_coefficients[overscan];
	const auto chroma_coefficient = is_composite_new_era ? 0.29f : 0.72f;

	constexpr auto burst_delay = 60.0f * ns;
	const auto color_delay = bpp1 ? 0.0f : 25.0f * ns;

	const float pixel_clock_delay = (cp_d * chroma_coefficient + rgbi_pixel_delay * rgbi_d) /
	                                        (chroma_coefficient + rgbi_d) +
	                                burst_delay + color_delay;

	const float hue_adjust = (-(90.0f - 33.0f) - hue.AsFloat() + pixel_clock_delay) *
	                         tau / 360.0f;
	float chroma_signals[8][4];
	for (uint8_t i = 0; i < 4; i++) {
		chroma_signals[0][i] = 0;
		chroma_signals[7][i] = 1;
		for (uint8_t j = 0; j < 6; j++) {
			constexpr float phases[6] = {270.0f - 21.5f * ns,  // blue
			                             135.0f - 29.5f * ns,  // green
			                             180.0f - 21.5f * ns,  // cyan
			                             000.0f - 21.5f * ns,  // red
			                             315.0f - 29.5f * ns,  // magenta
			                             090.0f - 21.5f * ns}; // yellow/burst

			// All the duty cycle fractions are the same, just under 0.5 as the rising edge is delayed 2ns
			// more than the falling edge.
			constexpr float duty = 0.5f - 2 * ns / 360.0f;

			// We have a rectangle wave with period 1 (in units of the reciprocal of the color burst
			// frequency) and duty cycle fraction "duty" and phase "phase". We band-limit this wave to
			// frequency 2 and sample it at intervals of 1/4. We model our band-limited wave with

			// 4 frequency components:
			//   f(x) = a + b*sin(x*tau) + c*cos(x*tau) +
			//   d*sin(x*2*tau)
			// Then:
			//   a =   integral(0, 1, f(x)*dx) = duty
			//   b = 2*integral(0, 1, f(x)*sin(x*tau)*dx) =
			//   2*integral(0, duty, sin(x*tau)*dx) =
			//   2*(1-cos(x*tau))/tau c = 2*integral(0, 1,
			//   f(x)*cos(x*tau)*dx) = 2*integral(0, duty,
			//   cos(x*tau)*dx) = 2*sin(duty*tau)/tau d =
			//   2*integral(0, 1, f(x)*sin(x*2*tau)*dx) =
			//   2*integral(0, duty, sin(x*4*pi)*dx) =
			//   2*(1-cos(2*tau*duty))/(2*tau)

			constexpr auto a = duty;
			const auto b = 2.0f * (1.0f - cosf(duty * tau)) / tau;
			const auto c = 2.0f * sinf(duty * tau) / tau;
			const auto d = 2.0f * (1.0f - cosf(duty * 2 * tau)) / (2 * tau);
			const auto x = (phases[j] + 21.5f * ns + pixel_clock_delay) / 360.0f + i / 4.0f;

			chroma_signals[j + 1][i] = a + b * sinf(x * tau) + c * cosf(x * tau) + d * sinf(x * 2 * tau);
		}
	}

	// PCjr CGA palette table
	const std::array<uint8_t, 4> pcjr_palette = {
	        vga.attr.palette[0],
	        vga.attr.palette[1],
	        vga.attr.palette[2],
	        vga.attr.palette[3],
	};

	for (uint8_t x = 0; x < 4; ++x) { // Position of pixel in question
		const bool even = !(x & 1);
		for (uint8_t bits = 0; bits < (even ? 0x10 : 0x40); ++bits) {
			float Y = 0.0f, I = 0.0f, Q = 0.0f;
			for (uint8_t p = 0; p < 4; ++p) { // Position within color carrier cycle
				// generate pixel pattern.
				uint8_t rgbi = 0;
				if (bpp1) {
					rgbi = ((bits >> (3 - p)) & (even ? 1 : 2)) != 0 ? overscan : 0;
				} else {
					const size_t i = even ? (bits >> (2 - (p & 2))) & 3
					                      : (bits >> (4 - ((p + 1) & 6))) & 3;
					assert(i < pcjr_palette.size());
					rgbi = pcjr_palette[i];
				}
				const uint8_t c = (bw && rgbi & 7) ? 7 : rgbi & 7;

				// calculate composite output
				const auto chroma = chroma_signals[c][(p + x) & 3] * chroma_coefficient;
				const auto composite = chroma + rgbi_coefficients[rgbi];

				Y += composite;
				if (!bw) { // burst on
					I += composite * 2 * cosf(hue_adjust + (p + x) * tau / 4.0f);
					Q += composite * 2 * sinf(hue_adjust + (p + x) * tau / 4.0f);
				}
			}

			Y = clamp(tv_brightness + tv_contrast * Y / 4.0f, 0.0f, 1.0f);
			I = clamp(tv_saturation * tv_contrast * I / 4.0f, -0.5957f, 0.5957f);
			Q = clamp(tv_saturation * tv_contrast * Q / 4.0f, -0.5226f, 0.5226f);

			constexpr auto gamma = 2.2f;

			auto normalize_and_apply_gamma = [=](float v) {
				const auto normalized = clamp((v - 0.075f) / (1 - 0.075f), 0.0f, 1.0f);
				return powf(normalized, gamma);
			};
			const auto R = normalize_and_apply_gamma(Y + 0.9563f * I + 0.6210f * Q);
			const auto G = normalize_and_apply_gamma(Y - 0.2721f * I - 0.6474f * Q);
			const auto B = normalize_and_apply_gamma(Y - 1.1069f * I + 1.7046f * Q);

			auto to_linear_rgb = [=](const float v) -> uint8_t {
				// Only operate on reasonably positive v-values
				if (!std::isnormal(v) || v <= 0.0f)
					return 0;
				// switch to linear RGB space and scale to the 8-bit range
				constexpr int max_8bit = UINT8_MAX;
				const auto linear = powf(v, 1.0f / gamma) * max_8bit;
				const auto clamped = clamp(iroundf(linear), 0, max_8bit);
				return check_cast<uint8_t>(clamped);
			};
			const auto r = to_linear_rgb(1.5073f * R - 0.3725f * G - 0.0832f * B);
			const auto g = to_linear_rgb(-0.0275f * R + 0.9350f * G + 0.0670f * B);
			const auto b = to_linear_rgb(-0.0272f * R - 0.0401f * G + 1.1677f * B);

			const uint8_t index = bits | ((x & 1) == 0 ? 0x30 : 0x80) | ((x & 2) == 0 ? 0x40 : 0);
			ReelMagic_RENDER_SetPalette(index, r, g, b);
		}
	}
}

template <typename chroma_t>
static constexpr float new_cga_v(const chroma_t c, const float i, const float r,
                                 const float g, const float b)
{
	const auto c_weighted = 0.29f * c / 0.72f;
	const auto i_weighted = 0.32f * i / 0.28f;
	const auto r_weighted = 0.10f * r / 0.28f;
	const auto g_weighted = 0.22f * g / 0.28f;
	const auto b_weighted = 0.07f * b / 0.28f;

	return c_weighted + i_weighted + r_weighted + g_weighted + b_weighted;
}

static void update_cga16_color()
{
	// New algorithm by reenigne
	// Works in all CGA modes/color settings and can simulate older and
	// newer CGA revisions
	constexpr auto tau = static_cast<float>(2 * M_PI);

	constexpr uint8_t chroma_multiplexer[256] = {
	        2,   2,   2,   2,   114, 174, 4,   3,   2,   1,   133, 135, 2,
	        113, 150, 4,   133, 2,   1,   99,  151, 152, 2,   1,   3,   2,
	        96,  136, 151, 152, 151, 152, 2,   56,  62,  4,   111, 250, 118,
	        4,   0,   51,  207, 137, 1,   171, 209, 5,   140, 50,  54,  100,
	        133, 202, 57,  4,   2,   50,  153, 149, 128, 198, 198, 135, 32,
	        1,   36,  81,  147, 158, 1,   42,  33,  1,   210, 254, 34,  109,
	        169, 77,  177, 2,   0,   165, 189, 154, 3,   44,  33,  0,   91,
	        197, 178, 142, 144, 192, 4,   2,   61,  67,  117, 151, 112, 83,
	        4,   0,   249, 255, 3,   107, 249, 117, 147, 1,   50,  162, 143,
	        141, 52,  54,  3,   0,   145, 206, 124, 123, 192, 193, 72,  78,
	        2,   0,   159, 208, 4,   0,   53,  58,  164, 159, 37,  159, 171,
	        1,   248, 117, 4,   98,  212, 218, 5,   2,   54,  59,  93,  121,
	        176, 181, 134, 130, 1,   61,  31,  0,   160, 255, 34,  1,   1,
	        58,  197, 166, 0,   177, 194, 2,   162, 111, 34,  96,  205, 253,
	        32,  1,   1,   57,  123, 125, 119, 188, 150, 112, 78,  4,   0,
	        75,  166, 180, 20,  38,  78,  1,   143, 246, 42,  113, 156, 37,
	        252, 4,   1,   188, 175, 129, 1,   37,  118, 4,   88,  249, 202,
	        150, 145, 200, 61,  59,  60,  60,  228, 252, 117, 77,  60,  58,
	        248, 251, 81,  212, 254, 107, 198, 59,  58,  169, 250, 251, 81,
	        80,  100, 58,  154, 250, 251, 252, 252, 252,
	};

	constexpr float intensity[4] = {
	        77.175381f,
	        88.654656f,
	        166.564623f,
	        174.228438f,
	};

	constexpr auto i0 = intensity[0];
	constexpr auto i3 = intensity[3];

	const auto min_v = is_composite_new_era
	                           ? new_cga_v(chroma_multiplexer[0], i0, i0, i0, i0)
	                           : chroma_multiplexer[0] + i0;

	const auto max_v = is_composite_new_era
	                           ? new_cga_v(chroma_multiplexer[255], i3, i3, i3, i3)
	                           : chroma_multiplexer[255] + i3;

	const auto mode_contrast = 2.56f * Contrast / (max_v - min_v);
	const auto mode_brightness = Brightness * 5 - 256 * min_v / (max_v - min_v);

	const bool in_tandy_text_mode = (vga.mode == M_CGA_TEXT_COMPOSITE) &&
	                                (vga.tandy.mode.is_high_bandwidth);
	const auto mode_hue = in_tandy_text_mode ? 14.0f : 4.0f;

	const auto mode_saturation = Saturation *
	                             (is_composite_new_era ? 5.8f : 2.9f) / 100;

	// Update the Composite CGA palette
	const bool in_tandy_mode_4 = vga.tandy.mode.is_black_and_white_mode;
	for (uint16_t x = 0; x < 1024; ++x) {
		const uint16_t right = (x >> 2) & 15;
		const uint16_t rc = in_tandy_mode_4
		                            ? (right & 8) |
		                                      ((right & 7) != 0 ? 7 : 0)
		                            : right;

		const uint16_t left = (x >> 6) & 15;
		const uint16_t lc = in_tandy_mode_4
		                            ? (left & 8) | ((left & 7) != 0 ? 7 : 0)
		                            : left;

		const uint16_t phase = x & 3;
		const float c = chroma_multiplexer[((lc & 7) << 5) | ((rc & 7) << 2) | phase];

		const float i = intensity[(left >> 3) | ((right >> 2) & 2)];

		if (is_composite_new_era) {
			const float r = intensity[((left >> 2) & 1) | ((right >> 1) & 2)];
			const float g = intensity[((left >> 1) & 1) | (right & 2)];
			const float b = intensity[(left & 1) | ((right << 1) & 2)];
			const auto v = new_cga_v(c, i, r, g, b);
			CGA_Composite_Table[x] = static_cast<int>(
			        v * mode_contrast + mode_brightness);
		} else {
			const auto v = c + i;
			CGA_Composite_Table[x] = static_cast<int>(
			        v * mode_contrast + mode_brightness);
		}
	}

	const auto i = static_cast<float>(CGA_Composite_Table[6 * 68] -
	                                  CGA_Composite_Table[6 * 68 + 2]);
	const auto q = static_cast<float>(CGA_Composite_Table[6 * 68 + 1] -
	                                  CGA_Composite_Table[6 * 68 + 3]);

	const auto a = tau * (33 + 90 + hue.AsFloat() + mode_hue) / 360.0f;
	const auto c = cosf(a);
	const auto s = sinf(a);

	const auto r = in_tandy_mode_4
	                       ? 0.0f
	                       : 256 * mode_saturation / sqrt(i * i + q * q);

	const auto iq_adjust_i = -(i * c + q * s) * r;
	const auto iq_adjust_q = (q * c - i * s) * r;

	constexpr auto ri = 0.9563f;
	constexpr auto rq = 0.6210f;
	constexpr auto gi = -0.2721f;
	constexpr auto gq = -0.6474f;
	constexpr auto bi = -1.1069f;
	constexpr auto bq = 1.7046f;

	// clang-format off
	vga.composite.ri = static_cast<int32_t>( ri * iq_adjust_i + rq * iq_adjust_q);
	vga.composite.rq = static_cast<int32_t>(-ri * iq_adjust_q + rq * iq_adjust_i);
	vga.composite.gi = static_cast<int32_t>( gi * iq_adjust_i + gq * iq_adjust_q);
	vga.composite.gq = static_cast<int32_t>(-gi * iq_adjust_q + gq * iq_adjust_i);
	vga.composite.bi = static_cast<int32_t>( bi * iq_adjust_i + bq * iq_adjust_q);
	vga.composite.bq = static_cast<int32_t>(-bi * iq_adjust_q + bq * iq_adjust_i);
	// clang-format on

	vga.composite.sharpness = convergence.get() * 256 / 100;
}

enum CrtKnob { Era = 0, Hue, Convergence, LastValue };

static auto crt_knob = CrtKnob::Era;

static void log_crt_knob_value()
{
	switch (crt_knob) {
	case CrtKnob::Era:
		LOG_MSG("COMPOSITE: %s-era CGA selected",
		        is_composite_new_era ? "New" : "Old");
		break;

	case CrtKnob::Hue:
		LOG_MSG("COMPOSITE: composite_hue =  %d", hue.get());
		break;

	case CrtKnob::Convergence:
		LOG_MSG("COMPOSITE: composite_convergence =  %d", convergence.get());
		break;

	case CrtKnob::LastValue:
		assertm(false, "Should not reach CRT knob end marker");
		break;
	}
}

static void turn_crt_knob(bool pressed, const int amount)
{
	if (!pressed) {
		return;
	}

	switch (crt_knob) {
	case CrtKnob::Era: is_composite_new_era = !is_composite_new_era; break;
	case CrtKnob::Hue: hue.turn(amount); break;
	case CrtKnob::Convergence: convergence.turn(amount); break;
	case CrtKnob::LastValue:
		assertm(false, "Should not reach CRT knob end marker");
		break;
	}

	if (is_machine_pcjr()) {
		update_cga16_color_pcjr();
	} else {
		update_cga16_color();
	}

	log_crt_knob_value();
}

static void turn_crt_knob_positive(bool pressed)
{
	turn_crt_knob(pressed, 5);
}

static void turn_crt_knob_negative(bool pressed)
{
	turn_crt_knob(pressed, -5);
}

static void select_next_crt_knob(bool pressed)
{
	if (!pressed)
		return;

	auto next_knob = static_cast<uint8_t>(crt_knob) + 1;

	// PCjr doesn't have a convergence knob
	if (is_machine_pcjr() && next_knob >= CrtKnob::Convergence) {
		next_knob++;
	}

	crt_knob = static_cast<CrtKnob>(next_knob % CrtKnob::LastValue);

	log_crt_knob_value();
}

static void write_cga_color_select(uint8_t val)
{
	// only receives 8-bit data per its IO port registration
	vga.tandy.color_select=val;
	switch(vga.mode) {
	case M_TANDY4:
	case M_CGA4_COMPOSITE: {
		uint8_t base = (val & 0x10) ? 0x08 : 0;
		uint8_t bg = val & 0xf;
		if (vga.tandy.mode.is_black_and_white_mode) // cyan red white
			VGA_SetCGA4Table(bg, 3 + base, 4 + base, 7 + base);
		else if (val & 0x20) // cyan magenta white
			VGA_SetCGA4Table(bg, 3 + base, 5 + base, 7 + base);
		else // green red brown
			VGA_SetCGA4Table(bg, 2 + base, 4 + base, 6 + base);
		vga.tandy.border_color = bg;
		vga.attr.overscan_color = bg;
		break;
	}
	case M_TANDY2:
	case M_CGA2_COMPOSITE:
		VGA_SetCGA2Table(0,val & 0xf);
		vga.attr.overscan_color = 0;
		break;
	case M_CGA16: update_cga16_color_pcjr(); break;
	case M_TEXT:
		vga.tandy.border_color = val & 0xf;
		vga.attr.overscan_color = 0;
		break;
	default: //Else unhandled values warning
		break;
	}
}

static void write_cga(io_port_t port, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	// only receives 8-bit data per its IO port registration
	switch (port) {
	case 0x3d8:
		vga.tandy.mode.data = val;
		vga.attr.disabled = (val&0x8)? 0: 1;
		if (vga.tandy.mode.is_graphics_enabled) {
			if (vga.tandy.mode.is_tandy_640_dot_graphics) {
				if (cga_comp == CompositeState::On ||
				    ((cga_comp == CompositeState::Auto && !(val & 0x4)) &&
				     !is_machine_cga_mono())) {

					// composite ntsc 640x200 16 color mode
					if (is_machine_pcjr()) {
						VGA_SetMode(M_CGA16);
					} else {
						VGA_SetMode(M_CGA2_COMPOSITE);
						update_cga16_color();
					}
				} else {
					VGA_SetMode(M_TANDY2);
				}
			} else {							// lowres mode
				if (cga_comp == CompositeState::On) {
					// composite ntsc 640x200 16 color mode
					if (is_machine_pcjr()) {
						VGA_SetMode(M_CGA16);
					} else {
						VGA_SetMode(M_CGA4_COMPOSITE);
						update_cga16_color();
					}
				} else if (!is_machine_pcjr()) {
					VGA_SetMode(M_TANDY4);
				}
			}

			write_cga_color_select(vga.tandy.color_select);
		} else {
			if (cga_comp == CompositeState::On) { // composite display
				VGA_SetMode(M_CGA_TEXT_COMPOSITE);
				update_cga16_color();
			} else {
				VGA_SetMode(M_TANDY_TEXT);
			}
		}
		VGA_SetBlinking(val & 0x20);
		break;
	case 0x3d9: // color select
		write_cga_color_select(val);
		break;
	}
}

static void PCJr_FindMode();

static void apply_composite_state()
{
	// Switch RGB and Composite if in graphics mode
	if (is_machine_pcjr() && vga.tandy.mode.is_graphics_enabled) {
		PCJr_FindMode();
	} else {
		write_cga(0x3d8, vga.tandy.mode.data, io_width_t::byte);
	}

/* 	if (vga.tandy.mode.is_graphics_enabled) {
		if (is_machine_pcjr()) {
			PCJr_FindMode();
		} else {
			write_cga(0x3d8, vga.tandy.mode.data, io_width_t::byte);
		}
	}
 */

}

static void toggle_cga_composite_mode(bool pressed)
{
	if (!pressed) {
		return;
	}

	// Step through the composite modes
	if (cga_comp == CompositeState::Auto) {
		cga_comp = CompositeState::On;
	} else if (cga_comp == CompositeState::On) {
		cga_comp = CompositeState::Off;
	} else {
		cga_comp = CompositeState::Auto;
	}

	LOG_MSG("COMPOSITE: State is %s",
	        (cga_comp == CompositeState::Auto
	                 ? "auto"
	                 : (cga_comp == CompositeState::On ? "on" : "off")));

	apply_composite_state();
}

static void tandy_update_palette() {
	using namespace bit::literals;

	// TODO mask off bits if needed
	if (is_machine_tandy()) {
		switch (vga.mode) {
		case M_TANDY2:
			VGA_SetCGA2Table(vga.attr.palette[0],
				vga.attr.palette[vga.tandy.color_select&0xf]);
			break;
		case M_TANDY4:
			if (vga.tandy.mode_control.is_tandy_640x200_4_color_graphics) {
				VGA_SetCGA4Table( // function sets both medium and highres 4color tables
					vga.attr.palette[0], vga.attr.palette[1],
					vga.attr.palette[2], vga.attr.palette[3]);
			} else {
				uint8_t color_set = 0;
				uint8_t r_mask = 0xf;
				if (bit::is(vga.tandy.color_select, b4)) {
					bit::set(color_set, b3); // intensity
				}
				if (bit::is(vga.tandy.color_select, b5)) {
					bit::set(color_set, b0); // Cyan Mag. White
				}
				if (vga.tandy.mode.is_black_and_white_mode) { // Cyan Red White
					bit::set(color_set, b0);
					bit::clear(r_mask, b0);
				}
				VGA_SetCGA4Table(
					vga.attr.palette[vga.tandy.color_select&0xf],
					vga.attr.palette[(2|color_set)& vga.tandy.palette_mask],
					vga.attr.palette[(4|(color_set& r_mask))& vga.tandy.palette_mask],
					vga.attr.palette[(6|color_set)& vga.tandy.palette_mask]);
			}
			break;
		default:
			break;
		}
	} else {
		// PCJr
		switch (vga.mode) {
		case M_TANDY2:
			VGA_SetCGA2Table(vga.attr.palette[0],vga.attr.palette[1]);
			break;
		case M_TANDY4:
			VGA_SetCGA4Table(
				vga.attr.palette[0], vga.attr.palette[1],
				vga.attr.palette[2], vga.attr.palette[3]);
			break;
		default:
			break;
		}
		if (is_machine_pcjr()) {
			update_cga16_color_pcjr();
		} else {
			update_cga16_color();
		}
	}
}

void VGA_SetModeNow(VGAModes mode);

static void TANDY_FindMode()
{
	if (vga.tandy.mode.is_graphics_enabled) {
		if (vga.tandy.mode_control.is_tandy_16_color_enabled) {
			if (vga.mode==M_TANDY4) {
				VGA_SetModeNow(M_TANDY16);
			} else VGA_SetMode(M_TANDY16);
		} else if (vga.tandy.mode_control.is_tandy_640x200_4_color_graphics) {
			if (cga_comp == CompositeState::On) {
				// composite ntsc 640x200 16 color mode
				VGA_SetMode(M_CGA4_COMPOSITE);
				update_cga16_color();
			} else {
				VGA_SetMode(M_TANDY4);
			}
		} else if (vga.tandy.mode.is_tandy_640_dot_graphics) {
			if (cga_comp == CompositeState::On) {
				// composite ntsc 640x200 16 color mode
				VGA_SetMode(M_CGA2_COMPOSITE);
				update_cga16_color();
			} else {
				VGA_SetMode(M_TANDY2);
			}
		} else {
			// otherwise some 4-colour graphics mode
			const auto new_mode = (cga_comp == CompositeState::On)
			                            ? M_CGA4_COMPOSITE
			                            : M_TANDY4;
			if (vga.mode == M_TANDY16) {
				VGA_SetModeNow(new_mode);
			} else {
				VGA_SetMode(new_mode);
			}
		}
		tandy_update_palette();
	} else {
		VGA_SetMode(M_TANDY_TEXT);
	}
}

static void PCJr_FindMode()
{
	assert(is_machine_pcjr());

	if (vga.tandy.mode.is_graphics_enabled) {
		if (vga.tandy.mode.is_pcjr_16_color_graphics) {
			if (vga.mode == M_TANDY4) {
				VGA_SetModeNow(M_TANDY16); // TODO lowres mode only
			} else {
				VGA_SetMode(M_TANDY16);
			}
		} else if (vga.tandy.mode_control.is_pcjr_640x200_2_color_graphics) {
			// bit3 of mode control 2 signals 2 colour graphics mode
			if (cga_comp == CompositeState::On ||
			    (cga_comp == CompositeState::Auto &&
			     !(vga.tandy.mode.is_black_and_white_mode))) {
				VGA_SetMode(M_CGA16);
			} else {
				VGA_SetMode(M_TANDY2);
			}
		} else {
			// otherwise some 4-colour graphics mode
			const auto new_mode = (cga_comp == CompositeState::On)
			                            ? M_CGA16
			                            : M_TANDY4;
			if (vga.mode == M_TANDY16) {
				VGA_SetModeNow(new_mode);
			} else {
				VGA_SetMode(new_mode);
			}
		}
		if (vga.mode == M_CGA16) {
			update_cga16_color_pcjr();
		}
		tandy_update_palette();
	} else {
		VGA_SetMode(M_TANDY_TEXT);
	}
}

static void TandyCheckLineMask(void ) {
	using namespace bit::literals;

	if ( vga.tandy.extended_ram & 1 ) {
		vga.tandy.line_mask = 0;
	} else if (vga.tandy.mode.is_graphics_enabled) {
		bit::set(vga.tandy.line_mask, b0);
	}
	if ( vga.tandy.line_mask ) {
		vga.tandy.line_shift = 13;
		vga.tandy.addr_mask = (1 << 13) - 1;
	} else {
		vga.tandy.addr_mask = (Bitu)(~0);
		vga.tandy.line_shift = 0;
	}
}

static void write_tandy_reg(uint8_t val)
{
	using namespace bit::literals;

	// only receives 8-bit data per its IO port registration
	switch (vga.tandy.reg_index) {
	case 0x0:
		if (is_machine_pcjr()) {
			vga.tandy.mode.data = val;
			VGA_SetBlinking(val & 0x20);
			PCJr_FindMode();
			if (bit::is(val, b3)) {
				bit::clear(vga.attr.disabled, b0);
			} else {
				bit::set(vga.attr.disabled, b0);
			}
		} else {
			LOG(LOG_VGAMISC, LOG_NORMAL)("Unhandled Write %2X to tandy reg %X", val, vga.tandy.reg_index);
		}
		break;
	case 0x1:	/* Palette mask */
		vga.tandy.palette_mask = val;
		tandy_update_palette();
		break;
	case 0x2:	/* Border color */
		vga.tandy.border_color=val;
		break;
	case 0x3:	/* More control */
		vga.tandy.mode_control.data = val;
		if (is_machine_tandy()) {
			TANDY_FindMode();
		} else {
			PCJr_FindMode();
		}
		break;
	case 0x5:	/* Extended ram page register */
		// Bit 0 enables extended ram
		// Bit 7 Switches clock, 0 -> cga 28.6 , 1 -> mono 32.5
		vga.tandy.extended_ram = val;
		//This is a bit of a hack to enable mapping video memory differently for highres mode
		TandyCheckLineMask();
		VGA_SetupHandlers();
		break;
	default:
		if ((vga.tandy.reg_index & 0xf0) == 0x10) { // color palette
			vga.attr.palette[vga.tandy.reg_index-0x10] = val&0xf;
			tandy_update_palette();
		} else
			LOG(LOG_VGAMISC,LOG_NORMAL)("Unhandled Write %2X to tandy reg %X",val,vga.tandy.reg_index);
	}
}

static void write_tandy(io_port_t port, io_val_t value, io_width_t)
{
	using namespace bit::literals;

	auto val = check_cast<uint8_t>(value);
	// only receives 8-bit data per its IO port registration
	switch (port) {
	case 0x3d8:
		bit::clear(val, b7 | b6); // only bits 0-5 are used
		if (vga.tandy.mode.data ^ val) {
			vga.tandy.mode.data = val;
			if (bit::is(val, b3)) {
				bit::clear(vga.attr.disabled, b0);
			} else {
				bit::set(vga.attr.disabled, b0);
			}
			TandyCheckLineMask();
			VGA_SetBlinking(val & 0x20);
			TANDY_FindMode();
			VGA_StartResize();
		}
		break;
	case 0x3d9:
		vga.tandy.color_select=val;
		tandy_update_palette();
		// Re-apply the composite mode after updating the palette
		if (cga_comp == CompositeState::On) {
			apply_composite_state();
		}
		break;
	case 0x3da:
		vga.tandy.reg_index = val;
		//if (val&0x10) vga.attr.disabled |= 2;
		//else vga.attr.disabled &= ~2;
		break;
//	case 0x3dd:	//Extended ram page address register:
//		break;
	case 0x3de: write_tandy_reg(val); break;
	case 0x3df:
		// CRT/processor page register
		// See the comments on the PCJr version of this register.
		// A difference to it is:
		// Bit 3-5: Processor page CPU_PG
		// The remapped range is 32kB instead of 16. Therefore CPU_PG bit 0
		// appears to be ORed with CPU A14 (to preserve some sort of
		// backwards compatibility?), resulting in odd pages being mapped
		// as 2x16kB. Implemeted in vga_memory.cpp Tandy handler.

		vga.tandy.line_mask = val >> 6;
		vga.tandy.draw_bank = val & ((vga.tandy.line_mask&2) ? 0x6 : 0x7);
		vga.tandy.mem_bank = (val >> 3) & 7;
		TandyCheckLineMask();
		VGA_SetupHandlers();
		break;
	}
}

static void write_pcjr(io_port_t port, io_val_t value, io_width_t)
{
	using namespace bit::literals;

	// only receives 8-bit data per its IO port registration
	const auto val = check_cast<uint8_t>(value);
	switch (port) {
	case 0x3da:
		if (vga.tandy.pcjr_flipflop)
			write_tandy_reg(val);
		else {
			vga.tandy.reg_index = val;
			if (bit::is(vga.tandy.reg_index, b4)) {
				bit::set(vga.attr.disabled, b1);
			} else {
				bit::clear(vga.attr.disabled, b1);
			}
		}
		vga.tandy.pcjr_flipflop=!vga.tandy.pcjr_flipflop;
		break;
	case 0x3df:
		// CRT/processor page register

		// Bit 0-2: CRT page PG0-2
		// In one- and two bank modes, bit 0-2 select the 16kB memory
		// area of system RAM that is displayed on the screen.
		// In 4-banked modes, bit 1-2 select the 32kB memory area.
		// Bit 2 only has effect when the PCJR upgrade to 128k is installed.

		// Bit 3-5: Processor page CPU_PG
		// Selects the 16kB area of system RAM that is mapped to
		// the B8000h IBM PC video memory window. Since A14-A16 of the
		// processor are unconditionally replaced with these bits when
		// B8000h is accessed, the 16kB area is mapped to the 32kB
		// range twice in a row. (Scuba Venture writes across the boundary)

		// Bit 6-7: Video Address mode
		// 0: CRTC addresses A0-12 directly, accessing 8k characters
		//    (+8k attributes). Used in text modes (one bank).
		//    PG0-2 in effect. 16k range.
		// 1: CRTC A12 is replaced with CRTC RA0 (see max_scanline).
		//    This results in the even/odd scanline two bank system.
		//    PG0-2 in effect. 16k range.
		// 2: Documented as unused. CRTC addresses A0-12, PG0 is replaced
		//    with RA1. Looks like nonsense.
		//    PG1-2 in effect. 32k range which cannot be used completely.
		// 3: CRTC A12 is replaced with CRTC RA0, PG0 is replaced with
		//    CRTC RA1. This results in the 4-bank mode.
		//    PG1-2 in effect. 32k range.

		vga.tandy.line_mask = val >> 6;
		vga.tandy.draw_bank = val & ((vga.tandy.line_mask&2) ? 0x6 : 0x7);
		vga.tandy.mem_bank = (val >> 3) & 7;
		vga.tandy.draw_base = &MemBase[vga.tandy.draw_bank * 16 * 1024];
		vga.tandy.mem_base = &MemBase[vga.tandy.mem_bank * 16 * 1024];
		TandyCheckLineMask();
		VGA_SetupHandlers();
		break;
	}
}

static constexpr int NumHerculesColors = 2;

// clang-format off
static constexpr Rgb888 hercules_palettes[NumMonochromePalettes][NumHerculesColors] = {
	{
		// 0 - Amber
		{0x34, 0x20, 0x00}, {0x3f, 0x34, 0x00}
	}, {
		// 1 - Green
		{0x00, 0x26, 0x00}, {0x00, 0x3f, 0x00}
	}, {
		// 2 - White
		{0x2a, 0x2a, 0x2a}, {0x3f, 0x3f, 0x3f}
	}, {
		// 3 - Paperwhite
		{0x2d, 0x2e, 0x2d}, {0x3f, 0x3f, 0x3b}
	}
};
// clang-format on

void VGA_SetMonochromePalette(const enum MonochromePalette _palette)
{
	if (is_machine_hercules()) {
		hercules_palette = _palette;
		VGA_SetHerculesPalette();

	} else if (is_machine_cga_mono()) {
		mono_cga_palette = _palette;
		VGA_SetMonochromeCgaPalette();
	}
}

static MonochromePalette cycle_forward(const MonochromePalette _palette)
{
	auto value = (enum_val(_palette) + 1) % NumMonochromePalettes;
	return static_cast<MonochromePalette>(value);
}

static void cycle_mono_cga_palette(bool pressed)
{
	if (!pressed) {
		return;
	}

	mono_cga_palette = cycle_forward(mono_cga_palette);
	VGA_SetMonochromeCgaPalette();

	RENDER_SyncMonochromePaletteSetting(mono_cga_palette);
}

void VGA_SetMonochromeCgaPalette()
{
	for (uint8_t color_idx = 0; color_idx < NumCgaColors; ++color_idx) {
		const auto color = [&] {
			const auto palette_idx = enum_val(mono_cga_palette);
			if (INT10_IsTextMode(*CurMode)) {
				return mono_cga_text_palettes[palette_idx][color_idx];
			} else {
				return mono_cga_graphics_palettes[palette_idx][color_idx];
			}
		}();

		VGA_DAC_SetEntry(color_idx, color.red, color.green, color.blue);
		VGA_DAC_CombineColor(color_idx, color_idx);
	}
}

static void cycle_hercules_palette(bool pressed)
{
	if (!pressed) {
		return;
	}

	hercules_palette = cycle_forward(hercules_palette);
	VGA_SetHerculesPalette();

	RENDER_SyncMonochromePaletteSetting(hercules_palette);
}

void VGA_SetHerculesPalette()
{
	const auto palette_idx = enum_val(hercules_palette);
	const auto dark_color  = hercules_palettes[palette_idx][0];
	const auto light_color = hercules_palettes[palette_idx][1];

	VGA_DAC_SetEntry(0x7, dark_color.red, dark_color.green, dark_color.blue);
	VGA_DAC_SetEntry(0xf, light_color.red, light_color.green, light_color.blue);

	VGA_DAC_CombineColor(0, 0);
	VGA_DAC_CombineColor(1, 7);
}

Rgb888 VGA_GetBlackLevelColor()
{
	if (is_machine_hercules()) {
		const auto palette_idx = enum_val(hercules_palette);
		const auto dark_color  = hercules_palettes[palette_idx][0];
		return dark_color;

	} else if (is_machine_cga_mono()) {
		const auto palette_idx = enum_val(mono_cga_palette);
		// The colour at index 5 has the same average luminosity as the
		// "dark" Hercules colour
		const auto color = mono_cga_graphics_palettes[palette_idx][5];
		return color;

	} else {
		// Use neutral dark grey for all other video standards (no
		// colour tint)
		return Rgb888{40, 40, 40};
	}
}

static void write_hercules(io_port_t port, io_val_t value, io_width_t)
{
	using namespace bit::literals;

	const auto val = check_cast<uint8_t>(value);
	switch (port) {
	case 0x3b8: {
		// the protected bits can always be cleared but only be set if
		// the protection bits are set
		if (bit::is(vga.herc.mode_control, b1)) {
			// already set
			if (bit::cleared(val, b1)) {
				bit::clear(vga.herc.mode_control, b1);
				VGA_SetMode(M_HERC_TEXT);
			}
		} else {
			// not set, can only set if protection bit is set
			if (bit::is(val, b1) && bit::is(vga.herc.enable_bits, b0)) {
				bit::set(vga.herc.mode_control, b1);
				VGA_SetMode(M_HERC_GFX);
			}
		}
		if (bit::is(vga.herc.mode_control, b7)) {
			if (bit::cleared(val, b7)) {
				bit::clear(vga.herc.mode_control, b7);
				vga.tandy.draw_base = &vga.mem.linear[0];
			}
		} else {
			if (bit::is(val, b7) && bit::is(vga.herc.enable_bits, b1)) {
				bit::set(vga.herc.mode_control, b7);
				vga.tandy.draw_base = &vga.mem.linear[32 * 1024];
			}
		}
		vga.draw.blinking = bit::is(val, b5);
		bit::retain(vga.herc.mode_control, b7 | b1);
		bit::set(vga.herc.mode_control, bit::mask_off(val, b7 | b1));
		break;
	}
	case 0x3bf:
		if ( vga.herc.enable_bits ^ val) {
			vga.herc.enable_bits=val;
			// Bit 1 enables the upper 32k of video memory,
			// so update the handlers
			VGA_SetupHandlers();
		}
		break;
	}
}

static uint8_t read_herc_status(io_port_t, io_width_t)
{
	// only returns 8-bit data per its IO port registration

	// 3BAh (R):  Status Register
	// bit   0  Horizontal sync
	//       1  Light pen status (only some cards)
	//       3  Video signal
	//     4-6	000: Hercules
	//			001: Hercules Plus
	//			101: Hercules InColor
	//			111: Unknown clone
	//       7  Vertical sync inverted

	const auto timeInFrame = PIC_FullIndex() - vga.draw.delay.framestart;
	uint8_t retval = 0x72; // Hercules ident; from a working card (Winbond
	                       // W86855AF) Another known working card has 0x76
	                       // ("KeysoGood", full-length)
	if (timeInFrame < vga.draw.delay.vrstart || timeInFrame > vga.draw.delay.vrend)
		retval |= 0x80;

	const auto timeInLine = fmod(timeInFrame, vga.draw.delay.htotal);
	if (timeInLine >= vga.draw.delay.hrstart &&
		timeInLine <= vga.draw.delay.hrend) retval |= 0x1;

	// 688 Attack sub checks bit 3 - as a workaround have the bit enabled
	// if no sync active (corresponds to a completely white screen)
	if ((retval&0x81)==0x80) retval |= 0x8;
	return retval;
}

void VGA_SetupOther()
{
	// Reset our Tandy struct
	vga.tandy.pcjr_flipflop     = 0;
	vga.tandy.mode.data         = 0;
	vga.tandy.color_select      = 0;
	vga.tandy.disp_bank         = 0;
	vga.tandy.reg_index         = 0;
	vga.tandy.mode_control.data = 0;
	vga.tandy.palette_mask      = 0;
	vga.tandy.extended_ram      = 0;
	vga.tandy.border_color      = 0;
	vga.tandy.line_mask         = 0;
	vga.tandy.line_shift        = 0;
	vga.tandy.draw_bank         = 0;
	vga.tandy.mem_bank          = 0;
	vga.tandy.draw_base         = nullptr;
	vga.tandy.mem_base          = nullptr;
	vga.tandy.addr_mask         = 0;

	vga.attr.disabled = 0;
	vga.config.bytes_skip=0;

	//Initialize values common for most machines, can be overwritten
	vga.tandy.draw_base = vga.mem.linear;
	vga.tandy.mem_base = vga.mem.linear;
	vga.tandy.addr_mask = 8*1024 - 1;
	vga.tandy.line_mask = 3;
	vga.tandy.line_shift = 13;

	if (is_machine_cga() || is_machine_pcjr_or_tandy()) {
		extern uint8_t int10_font_08[256 * 8];
		for (int i = 0; i < 256; ++i) {
			memcpy(&vga.draw.font[i * 32], &int10_font_08[i * 8], 8);
		}
		vga.draw.font_tables[0] = vga.draw.font_tables[1] = vga.draw.font;
	}
	if (is_machine_hercules() || is_machine_cga() || is_machine_pcjr_or_tandy()) {
		IO_RegisterWriteHandler(0x3db, write_lightpen, io_width_t::byte);
		IO_RegisterWriteHandler(0x3dc, write_lightpen, io_width_t::byte);
	}
	if (is_machine_hercules()) {
		extern uint8_t int10_font_14[256 * 14];
		for (int i = 0; i < 256; ++i) {
			memcpy(&vga.draw.font[i * 32], &int10_font_14[i * 14], 14);
		}
		vga.draw.font_tables[0] = vga.draw.font_tables[1] = vga.draw.font;
		MAPPER_AddHandler(cycle_hercules_palette, SDL_SCANCODE_F11, 0,
		                  "hercpal", "Herc Pal");
	}
	if (is_machine_cga()) {
		IO_RegisterWriteHandler(0x3d8, write_cga, io_width_t::byte);
		IO_RegisterWriteHandler(0x3d9, write_cga, io_width_t::byte);
		if (is_machine_cga_mono()) {
			MAPPER_AddHandler(cycle_mono_cga_palette,
			                  SDL_SCANCODE_F11,
			                  0,
			                  "monocgapal",
			                  "Mono CGA Pal");
		}
	}
	if (is_machine_tandy()) {
		write_tandy(0x3df, 0x0, io_width_t::byte);
		IO_RegisterWriteHandler(0x3d8, write_tandy, io_width_t::byte);
		IO_RegisterWriteHandler(0x3d9, write_tandy, io_width_t::byte);
		IO_RegisterWriteHandler(0x3da, write_tandy, io_width_t::byte);
		IO_RegisterWriteHandler(0x3de, write_tandy, io_width_t::byte);
		IO_RegisterWriteHandler(0x3df, write_tandy, io_width_t::byte);
	}
	if (is_machine_pcjr()) {
		//write_pcjr will setup base address
		write_pcjr(0x3df, 0x7 | (0x7 << 3), io_width_t::byte);
		IO_RegisterWriteHandler(0x3da, write_pcjr, io_width_t::byte);
		IO_RegisterWriteHandler(0x3df, write_pcjr, io_width_t::byte);
	}
	// Add composite hotkeys for CGA, Tandy, and PCjr
	if (is_machine_cga_color() || is_machine_pcjr_or_tandy()) {
		MAPPER_AddHandler(select_next_crt_knob,
		                  SDL_SCANCODE_F10,
		                  0,
		                  "comp_sel",
		                  "CompSelKnob");

		MAPPER_AddHandler(turn_crt_knob_positive,
		                  SDL_SCANCODE_F11,
		                  0,
		                  "comp_inc",
		                  "CompIncKnob");

		MAPPER_AddHandler(turn_crt_knob_negative,
		                  SDL_SCANCODE_F11,
		                  MMOD2,
		                  "comp_dec",
		                  "CompDecKnob");

		MAPPER_AddHandler(toggle_cga_composite_mode,
		                  SDL_SCANCODE_F12,
		                  0,
		                  "cgacomp",
		                  "CGA Comp");
	}

	auto register_crtc_port_handlers_at_base = [](const io_port_t base) {
		for (io_port_t i = 0; i < 4; ++i) {
			const auto index_port = check_cast<io_port_t>(base + 2 * i);
			IO_RegisterWriteHandler(index_port, write_crtc_index_other, io_width_t::byte);
			IO_RegisterReadHandler(index_port, read_crtc_index_other, io_width_t::byte);

			const auto data_port = check_cast<io_port_t>(index_port + 1);
			IO_RegisterWriteHandler(data_port, write_crtc_data_other, io_width_t::byte);
			IO_RegisterReadHandler(data_port, read_crtc_data_other, io_width_t::byte);
		}
	};

	if (is_machine_hercules()) {
		vga.herc.enable_bits = 0;
		vga.herc.mode_control = 0xa; // first mode written will be text mode
		vga.crtc.underline_location = 13;
		IO_RegisterWriteHandler(0x3b8, write_hercules, io_width_t::byte);
		IO_RegisterWriteHandler(0x3bf, write_hercules, io_width_t::byte);
		IO_RegisterReadHandler(0x3ba, read_herc_status, io_width_t::byte);
		register_crtc_port_handlers_at_base(0x3b0);
	} else if (!is_machine_ega_or_better()) {
		register_crtc_port_handlers_at_base(0x3d0);
	}
}

void COMPOSITE_Init()
{
	const auto section = get_section("composite");
	assert(section);

	const auto state = section->GetString("composite");

	if (state == "auto") {
		cga_comp = CompositeState::Auto;
	} else {
		const auto state_has_bool = parse_bool_setting(state);
		if (state_has_bool) {
			cga_comp = *state_has_bool ? CompositeState::On
			                           : CompositeState::Off;
		} else {
			LOG_WARNING("COMPOSITE: Invalid 'composite' setting: '%s', using 'off'",
			            state.c_str());
			cga_comp = CompositeState::Off;
		}
	}

	const auto era_choice = section->GetString("era");
	is_composite_new_era  = era_choice == "new" ||
	                       (is_machine_pcjr() && era_choice == "auto");

	hue.set(section->GetInt("hue"));
	convergence.set(section->GetInt("convergence"));

	if (cga_comp == CompositeState::On) {
		LOG_MSG("COMPOSITE: %s-era composite mode enabled",
		        (is_composite_new_era ? "New" : "Old"));
	}
}

static void notify_composite_setting_updated([[maybe_unused]] SectionProp& section,
                                             [[maybe_unused]] const std::string& prop_name)
{
	COMPOSITE_Init();
}

static void init_composite_settings(SectionProp& section)
{
	using enum Property::Changeable::Value;

	auto str_prop = section.AddString("composite", WhenIdle, "auto");
	str_prop->SetValues({"auto", "on", "off"});
	str_prop->SetHelp(
	        "Enable CGA composite monitor emulation ('auto' by default). Only available for\n"
	        "'cga', 'pcjr', and 'tandy' machine types. This allows the emulation of NTSC\n"
	        "artifact colours from the raw CGA RBGI image data, just like on a real NTSC CGA\n"
	        "composite monitor. Possible values:\n"
	        "\n"
	        "  off:   Disable composite emulation.\n"
	        "\n"
	        "  on:    Enable composite emulation in all video modes.\n"
	        "\n"
	        "  auto:  Automatically enable composite emulation for the 640x400 composite\n"
	        "         mode if the game uses it (default). You need to enable composite mode\n"
	        "         manually for the 320x200 mode.\n"
	        "\n"
	        "Note: Fine-tune the composite emulation settings (e.g., 'composite_hue') using\n"
	        "      the composite hotkeys, then copy the new settings from the logs into your\n"
	        "      config.");

	str_prop = section.AddString("era", WhenIdle, "auto");
	str_prop->SetValues({"auto", "old", "new"});
	str_prop->SetHelp(
	        "Era of CGA composite monitor to emulate ('auto' by default).\n"
	        "Possible values:\n"
	        "\n"
	        "  auto:  PCjr uses 'new', and CGA/Tandy use 'old' (default)\n"
	        "  old:   Emulate an early NTSC IBM CGA composite monitor model.\n"
	        "  new:   Emulate a late NTSC IBM CGA composite monitor model.");

	auto int_prop = section.AddInt("hue", WhenIdle, hue.GetDefaultValue());
	int_prop->SetMinMax(hue.GetMinValue(), hue.GetMaxValue());
	int_prop->SetHelp(format_str(
	        "Set the hue of the CGA composite colours (%d by default).\n"
	        "Valid range is %d to %d. For example, adjust until the sky appears blue and\n"
	        "the grass green in the game. This emulates the tint knob of CGA composite\n"
	        "monitors which often had to be adjusted for each game.",
	        hue.GetDefaultValue(),
	        hue.GetMinValue(),
	        hue.GetMaxValue()));

	int_prop = section.AddInt("convergence",
	                          WhenIdle,
	                          convergence.GetDefaultValue());
	int_prop->SetMinMax(convergence.GetMinValue(), convergence.GetMaxValue());
	int_prop->SetHelp(
	        format_str("Set the sharpness of the CGA composite image (%d by default).\n"
	                   "Valid range is %d to %d.",
	                   convergence.GetDefaultValue(),
	                   convergence.GetMinValue(),
	                   convergence.GetMaxValue()));
}

void COMPOSITE_AddConfigSection(Config& conf)
{
	auto section = conf.AddSection("composite");

	section->AddUpdateHandler(notify_composite_setting_updated);

	init_composite_settings(*section);
}
