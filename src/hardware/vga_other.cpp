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

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "inout.h"
#include "mapper.h"
#include "mem.h"
#include "pic.h"
#include "render.h"
#include "support.h"
#include "vga.h"

static void write_crtc_index_other(Bitu /*port*/,Bitu val,Bitu /*iolen*/) {
	vga.other.index=(Bit8u)val;
}

static Bitu read_crtc_index_other(Bitu /*port*/,Bitu /*iolen*/) {
	return vga.other.index;
}

static void write_crtc_data_other(Bitu /*port*/, Bitu data, Bitu /*iolen*/)
{
	// write_crtc_data_other() only accepts 8-bit data per its IO port registration
	auto val = static_cast<uint8_t>(data);

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
		if (machine == MCH_TANDY)
			vga.other.vsyncw = val >> 4;
		else
			// The MC6845 has a fixed v-sync width of 16 lines
			vga.other.vsyncw = 16;
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
		if (vga.other.vdend ^ val) VGA_StartResize();
		vga.other.vdend = val;
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
static uint8_t read_crtc_data_other(Bitu /*port*/, Bitu /*iolen*/)
{
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
		if (machine == MCH_TANDY)
			return static_cast<uint8_t>(vga.other.hsyncw | (vga.other.vsyncw << 4));
		else
			return vga.other.hsyncw;
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

static void write_lightpen(Bitu port,Bitu /*val*/,Bitu) {
	switch (port) {
	case 0x3db:	// Clear lightpen latch
		vga.other.lightpen_triggered = false;
		break;
	case 0x3dc:	// Preset lightpen latch
		if (!vga.other.lightpen_triggered) {
			vga.other.lightpen_triggered = true; // TODO: this shows at port 3ba/3da bit 1

			double timeInFrame = PIC_FullIndex()-vga.draw.delay.framestart;
			double timeInLine = fmod(timeInFrame,vga.draw.delay.htotal);
			Bitu current_scanline = (Bitu)(timeInFrame / vga.draw.delay.htotal);

			vga.other.lightpen = (Bit16u)((vga.draw.address_add/2) * (current_scanline/2));
			vga.other.lightpen +=
			        (Bit16u)((timeInLine / vga.draw.delay.hdend) *
			                 ((double)(vga.draw.address_add / 2)));
		}
		break;
	}
}

static double brightness = 0;
static double contrast = 100;
static double saturation = 100;
static double sharpness = 0;
static double hue_offset = 0;
static Bit8u cga_comp = 0;
static bool new_cga = 0;

static Bit8u herc_pal = 0;
static Bit8u mono_cga_pal = 0;
static Bit8u mono_cga_bright = 0;
static Bit8u mono_cga_palettes[8][16][3] =
{
	{ // 0 - green, 4-color-optimized contrast
		{0x00,0x00,0x00},{0x00,0x0d,0x03},{0x01,0x17,0x05},{0x01,0x1a,0x06},{0x02,0x28,0x09},{0x02,0x2c,0x0a},{0x03,0x39,0x0d},{0x03,0x3c,0x0e},
		{0x00,0x07,0x01},{0x01,0x13,0x04},{0x01,0x1f,0x07},{0x01,0x23,0x08},{0x02,0x31,0x0b},{0x02,0x35,0x0c},{0x05,0x3f,0x11},{0x0d,0x3f,0x17},
	},
	{ // 1 - green, 16-color-optimized contrast
		{0x00,0x00,0x00},{0x00,0x0d,0x03},{0x01,0x15,0x05},{0x01,0x17,0x05},{0x01,0x21,0x08},{0x01,0x24,0x08},{0x02,0x2e,0x0b},{0x02,0x31,0x0b},
		{0x01,0x22,0x08},{0x02,0x28,0x09},{0x02,0x30,0x0b},{0x02,0x32,0x0c},{0x03,0x39,0x0d},{0x03,0x3b,0x0e},{0x09,0x3f,0x14},{0x0d,0x3f,0x17},
	},
	{ // 2 - amber, 4-color-optimized contrast
		{0x00,0x00,0x00},{0x15,0x05,0x00},{0x20,0x0b,0x00},{0x24,0x0d,0x00},{0x33,0x18,0x00},{0x37,0x1b,0x00},{0x3f,0x26,0x01},{0x3f,0x2b,0x06},
		{0x0b,0x02,0x00},{0x1b,0x08,0x00},{0x29,0x11,0x00},{0x2e,0x14,0x00},{0x3b,0x1e,0x00},{0x3e,0x21,0x00},{0x3f,0x32,0x0a},{0x3f,0x38,0x0d},
	},
	{ // 3 - amber, 16-color-optimized contrast
		{0x00,0x00,0x00},{0x15,0x05,0x00},{0x1e,0x09,0x00},{0x21,0x0b,0x00},{0x2b,0x12,0x00},{0x2f,0x15,0x00},{0x38,0x1c,0x00},{0x3b,0x1e,0x00},
		{0x2c,0x13,0x00},{0x32,0x17,0x00},{0x3a,0x1e,0x00},{0x3c,0x1f,0x00},{0x3f,0x27,0x01},{0x3f,0x2a,0x04},{0x3f,0x36,0x0c},{0x3f,0x38,0x0d},
	},
	{ // 4 - grey, 4-color-optimized contrast
		{0x00,0x00,0x00},{0x0d,0x0d,0x0d},{0x15,0x15,0x15},{0x18,0x18,0x18},{0x24,0x24,0x24},{0x27,0x27,0x27},{0x33,0x33,0x33},{0x37,0x37,0x37},
		{0x08,0x08,0x08},{0x10,0x10,0x10},{0x1c,0x1c,0x1c},{0x20,0x20,0x20},{0x2c,0x2c,0x2c},{0x2f,0x2f,0x2f},{0x3b,0x3b,0x3b},{0x3f,0x3f,0x3f},
	},
	{ // 5 - grey, 16-color-optimized contrast
		{0x00,0x00,0x00},{0x0d,0x0d,0x0d},{0x12,0x12,0x12},{0x15,0x15,0x15},{0x1e,0x1e,0x1e},{0x20,0x20,0x20},{0x29,0x29,0x29},{0x2c,0x2c,0x2c},
		{0x1f,0x1f,0x1f},{0x23,0x23,0x23},{0x2b,0x2b,0x2b},{0x2d,0x2d,0x2d},{0x34,0x34,0x34},{0x36,0x36,0x36},{0x3d,0x3d,0x3d},{0x3f,0x3f,0x3f},
	},
	{ // 6 - paper-white, 4-color-optimized contrast
		{0x00,0x00,0x00},{0x0e,0x0f,0x10},{0x15,0x17,0x18},{0x18,0x1a,0x1b},{0x24,0x25,0x25},{0x27,0x28,0x28},{0x33,0x34,0x32},{0x37,0x38,0x35},
		{0x09,0x0a,0x0b},{0x11,0x12,0x13},{0x1c,0x1e,0x1e},{0x20,0x22,0x22},{0x2c,0x2d,0x2c},{0x2f,0x30,0x2f},{0x3c,0x3c,0x38},{0x3f,0x3f,0x3b},
	},
	{ // 7 - paper-white, 16-color-optimized contrast
		{0x00,0x00,0x00},{0x0e,0x0f,0x10},{0x13,0x14,0x15},{0x15,0x17,0x18},{0x1e,0x20,0x20},{0x20,0x22,0x22},{0x29,0x2a,0x2a},{0x2c,0x2d,0x2c},
		{0x1f,0x21,0x21},{0x23,0x25,0x25},{0x2b,0x2c,0x2b},{0x2d,0x2e,0x2d},{0x34,0x35,0x33},{0x37,0x37,0x34},{0x3e,0x3e,0x3a},{0x3f,0x3f,0x3b},
	},
};

static Bit8u byte_clamp(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static void update_cga16_color(void) {
// New algorithm by reenigne
// Works in all CGA modes/color settings and can simulate older and newer CGA revisions
	static const double tau = 6.28318531; // == 2*pi
	static unsigned char chroma_multiplexer[256] = {
		  2,  2,  2,  2, 114,174,  4,  3,   2,  1,133,135,   2,113,150,  4,
		133,  2,  1, 99, 151,152,  2,  1,   3,  2, 96,136, 151,152,151,152,
		  2, 56, 62,  4, 111,250,118,  4,   0, 51,207,137,   1,171,209,  5,
		140, 50, 54,100, 133,202, 57,  4,   2, 50,153,149, 128,198,198,135,
		 32,  1, 36, 81, 147,158,  1, 42,  33,  1,210,254,  34,109,169, 77,
		177,  2,  0,165, 189,154,  3, 44,  33,  0, 91,197, 178,142,144,192,
		  4,  2, 61, 67, 117,151,112, 83,   4,  0,249,255,   3,107,249,117,
		147,  1, 50,162, 143,141, 52, 54,   3,  0,145,206, 124,123,192,193,
		 72, 78,  2,  0, 159,208,  4,  0,  53, 58,164,159,  37,159,171,  1,
		248,117,  4, 98, 212,218,  5,  2,  54, 59, 93,121, 176,181,134,130,
		  1, 61, 31,  0, 160,255, 34,  1,   1, 58,197,166,   0,177,194,  2,
		162,111, 34, 96, 205,253, 32,  1,   1, 57,123,125, 119,188,150,112,
		 78,  4,  0, 75, 166,180, 20, 38,  78,  1,143,246,  42,113,156, 37,
		252,  4,  1,188, 175,129,  1, 37, 118,  4, 88,249, 202,150,145,200,
		 61, 59, 60, 60, 228,252,117, 77,  60, 58,248,251,  81,212,254,107,
		198, 59, 58,169, 250,251, 81, 80, 100, 58,154,250, 251,252,252,252};
	static double intensity[4] = {
		77.175381, 88.654656, 166.564623, 174.228438};

#define NEW_CGA(c,i,r,g,b) (((c)/0.72)*0.29 + ((i)/0.28)*0.32 + ((r)/0.28)*0.1 + ((g)/0.28)*0.22 + ((b)/0.28)*0.07)

	double mode_brightness;
	double mode_contrast;
	double mode_hue;
	double min_v;
	double max_v;
	if (!new_cga) {
		min_v = chroma_multiplexer[0] + intensity[0];
		max_v = chroma_multiplexer[255] + intensity[3];
	}
	else {
		double i0 = intensity[0];
		double i3 = intensity[3];
		min_v = NEW_CGA(chroma_multiplexer[0], i0, i0, i0, i0);
		max_v = NEW_CGA(chroma_multiplexer[255], i3, i3, i3, i3);
	}
	mode_contrast = 256/(max_v - min_v);
	mode_brightness = -min_v*mode_contrast;
	if (vga.mode == M_CGA_TEXT_COMPOSITE && (vga.tandy.mode_control & 1) != 0)
		mode_hue = 14;
	else
		mode_hue = 4;

	mode_contrast *= contrast/100;
	mode_brightness += brightness*5;
	double mode_saturation = (new_cga ? 5.8 : 2.9)*saturation/100;

	for (int x = 0; x < 1024; ++x) {
		int phase = x & 3;
		int right = (x >> 2) & 15;
		int left = (x >> 6) & 15;
		int rc = right;
		int lc = left;
		if ((vga.tandy.mode_control & 4) != 0) {
			rc = (right & 8) | ((right & 7) != 0 ? 7 : 0);
			lc = (left & 8) | ((left & 7) != 0 ? 7 : 0);
		}
		double c =
			chroma_multiplexer[((lc & 7) << 5) | ((rc & 7) << 2) | phase];
		double i = intensity[(left >> 3) | ((right >> 2) & 2)];
		double v;
		if (!new_cga)
			v = c + i;
	else {
			double r = intensity[((left >> 2) & 1) | ((right >> 1) & 2)];
			double g = intensity[((left >> 1) & 1) | (right & 2)];
			double b = intensity[(left & 1) | ((right << 1) & 2)];
			v = NEW_CGA(c, i, r, g, b);
		}
		CGA_Composite_Table[x] = static_cast<int>(v*mode_contrast + mode_brightness);
	}
#undef NEW_CGA

	double i = CGA_Composite_Table[6*68] - CGA_Composite_Table[6*68 + 2];
	double q = CGA_Composite_Table[6*68 + 1] - CGA_Composite_Table[6*68 + 3];

	double a = tau*(33 + 90 + hue_offset + mode_hue)/360.0;
	double c = cos(a);
	double s = sin(a);
	double r = 256*mode_saturation/sqrt(i*i+q*q);
	if ((vga.tandy.mode_control & 4) != 0)
		r = 0;

	double iq_adjust_i = -(i*c + q*s)*r;
	double iq_adjust_q = (q*c - i*s)*r;

	static const double ri = 0.9563;
	static const double rq = 0.6210;
	static const double gi = -0.2721;
	static const double gq = -0.6474;
	static const double bi = -1.1069;
	static const double bq = 1.7046;

	vga.ri = static_cast<int>(ri*iq_adjust_i + rq*iq_adjust_q);
	vga.rq = static_cast<int>(-ri*iq_adjust_q + rq*iq_adjust_i);
	vga.gi = static_cast<int>(gi*iq_adjust_i + gq*iq_adjust_q);
	vga.gq = static_cast<int>(-gi*iq_adjust_q + gq*iq_adjust_i);
	vga.bi = static_cast<int>(bi*iq_adjust_i + bq*iq_adjust_q);
	vga.bq = static_cast<int>(-bi*iq_adjust_q + bq*iq_adjust_i);
	vga.sharpness = static_cast<int>(sharpness*256/100);
}

static int which_control = 0;

static void LogControlValue() {
	update_cga16_color();
	switch (which_control) {
	case 0:
		LOG_MSG("%s model CGA selected", new_cga ? "Late" : "Early");
		break;
	case 1:
		LOG_MSG("Hue: %f",hue_offset);
		break;
	case 2:
		LOG_MSG("Saturation at %f",saturation);
		break;
	case 3:
		LOG_MSG("Contrast at %f",contrast);
		break;
	case 4:
		LOG_MSG("Brightness at %f",brightness);
		break;
	case 5:
		LOG_MSG("Sharpness at %f",sharpness);
		break;
 	}

}

static void IncreaseHue(bool pressed) {
	if (!pressed)
		return;
	hue_offset += 5.0;
	LogControlValue();
}
 



static void DecreaseHue(bool pressed) {
	if (!pressed)
		return;
	hue_offset -= 5.0;
	LogControlValue();
}

static void IncreaseSaturation(bool pressed) {
	if (!pressed)
		return;
	saturation += 5;
	LogControlValue();
}
 

static void DecreaseSaturation(bool pressed) {
	if (!pressed)
		return;
	saturation -= 5;
	LogControlValue();
}

static void IncreaseContrast(bool pressed) {
	if (!pressed)
		return;
	contrast += 5;
	LogControlValue();
}

static void DecreaseContrast(bool pressed) {
	if (!pressed)
		return;
	contrast -= 5;
	LogControlValue();
}

static void IncreaseBrightness(bool pressed) {
	if (!pressed)
		return;
	brightness += 5;
	LogControlValue();
}

static void DecreaseBrightness(bool pressed) {
	if (!pressed)
		return;
	brightness -= 5;
	LogControlValue();
}
 

static void IncreaseSharpness(bool pressed) {
	if (!pressed)
		return;
	sharpness += 5;
	LogControlValue();
}

static void DecreaseSharpness(bool pressed) {
	if (!pressed)
		return;
	sharpness -= 5;
	LogControlValue();
}
 



static void CGAModel(bool pressed) {
	if (!pressed) return;
	new_cga = !new_cga;
	LogControlValue();
}
 


static void IncreaseControlValue(bool pressed) {
	if (!pressed)
		return;
	switch (which_control) {
	case 0:
		CGAModel(true);
		break;
	case 1:
		IncreaseHue(true);
		break;
	case 2:
		IncreaseSaturation(true);
		break;
	case 3:
		IncreaseContrast(true);
		break;
	case 4:
		IncreaseBrightness(true);
		break;
	case 5:
		IncreaseSharpness(true);
		break;
 		}
}

static void DecreaseControlValue(bool pressed) {
	if (!pressed)
		return;
	switch (which_control) {
	case 0:
		CGAModel(true);
		break;
	case 1:
		DecreaseHue(true);
		break;
	case 2:
		DecreaseSaturation(true);
		break;
	case 3:
		DecreaseContrast(true);
		break;
	case 4:
		DecreaseBrightness(true);
		break;
	case 5:
		DecreaseSharpness(true);
		break;
 	}
 }


static void IncreaseWhichControl(bool pressed) {
 	if (!pressed)
 		return;



	which_control = (which_control + 1)%6;
	LogControlValue();
 }
 

static void DecreaseWhichControl(bool pressed) {
 	if (!pressed)
 		return;



	which_control = (which_control + 5)%6;
	LogControlValue();
 }
 

static void write_cga_color_select(uint8_t val) {
	vga.tandy.color_select=val;
	switch(vga.mode) {

	case  M_TANDY4:
	case  M_CGA4_COMPOSITE: {
		Bit8u base = (val & 0x10) ? 0x08 : 0;
		Bit8u bg = val & 0xf;
		if (vga.tandy.mode_control & 0x4)	// cyan red white
			VGA_SetCGA4Table(bg, 3+base, 4+base, 7+base);
		else if (val & 0x20)				// cyan magenta white
			VGA_SetCGA4Table(bg, 3+base, 5+base, 7+base);
		else								// green red brown
			VGA_SetCGA4Table(bg, 2+base, 4+base, 6+base);
		vga.tandy.border_color = bg;
		vga.attr.overscan_color = bg;
		break;
	}
	case M_TANDY2:
	case M_CGA2_COMPOSITE:
		VGA_SetCGA2Table(0,val & 0xf);
		vga.attr.overscan_color = 0;
		break;
	case M_TEXT:
		vga.tandy.border_color = val & 0xf;
		vga.attr.overscan_color = 0;
		break;
	default: //Else unhandled values warning
		break;
	}
}

static void write_cga(Bitu port, Bitu data, Bitu /*iolen*/)
{
	// The only data written is 8-bit per write_cga's IO port registration
	const auto val = static_cast<uint8_t>(data);
	switch (port) {
	case 0x3d8:
		vga.tandy.mode_control = val;
		vga.attr.disabled = (val&0x8)? 0: 1;
		if (vga.tandy.mode_control & 0x2) {		// graphics mode
			if (vga.tandy.mode_control & 0x10) {// highres mode
				if (cga_comp==1 || ((cga_comp==0 && !(val&0x4)) && !mono_cga)) {	// composite display

					VGA_SetMode(M_CGA2_COMPOSITE); // composite ntsc 640x200 16 color mode
					update_cga16_color();
				} else {
					VGA_SetMode(M_TANDY2);
				}
			} else {							// lowres mode
				if (cga_comp==1) {				// composite display

					VGA_SetMode(M_CGA4_COMPOSITE); // composite ntsc 640x200 16 color mode
					update_cga16_color();
				} else {
					VGA_SetMode(M_TANDY4);
				}
			}

			write_cga_color_select(vga.tandy.color_select);
		} else {
			if (cga_comp==1) {				// composite display
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

static void Composite(bool pressed) {
	if (!pressed) return;
	if (++cga_comp>2) cga_comp=0;
	LOG_MSG("Composite output: %s",(cga_comp==0)?"auto":((cga_comp==1)?"on":"off"));
	// switch RGB and Composite if in graphics mode
	if (vga.tandy.mode_control & 0x2 && machine == MCH_PCJR)
		PCJr_FindMode();
	else
		write_cga(0x3d8, vga.tandy.mode_control, 1);
}

static void tandy_update_palette() {
	// TODO mask off bits if needed
	if (machine == MCH_TANDY) {
		switch (vga.mode) {
		case M_TANDY2:
			VGA_SetCGA2Table(vga.attr.palette[0],
				vga.attr.palette[vga.tandy.color_select&0xf]);
			break;
		case M_TANDY4:
			if (vga.tandy.gfx_control & 0x8) {
				// 4-color high resolution - might be an idea to introduce M_TANDY4H
				VGA_SetCGA4Table( // function sets both medium and highres 4color tables
					vga.attr.palette[0], vga.attr.palette[1],
					vga.attr.palette[2], vga.attr.palette[3]);
			} else {
				Bit8u color_set = 0;
				Bit8u r_mask = 0xf;
				if (vga.tandy.color_select & 0x10) color_set |= 8; // intensity
				if (vga.tandy.color_select & 0x20) color_set |= 1; // Cyan Mag. White
				if (vga.tandy.mode_control & 0x04) {			// Cyan Red White
					color_set |= 1;
					r_mask &= ~1;
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
		update_cga16_color();
	}
}

void VGA_SetModeNow(VGAModes mode);

static void TANDY_FindMode()
{
	if (vga.tandy.mode_control & 0x2) {
		if (vga.tandy.gfx_control & 0x10) {
			if (vga.mode==M_TANDY4) {
				VGA_SetModeNow(M_TANDY16);
			} else VGA_SetMode(M_TANDY16);
		}
		else if (vga.tandy.gfx_control & 0x08) {
			VGA_SetMode(M_TANDY4);
		}
		else if (vga.tandy.mode_control & 0x10)
			VGA_SetMode(M_TANDY2);
		else {
			if (vga.mode==M_TANDY16) {
				VGA_SetModeNow(M_TANDY4);
			} else VGA_SetMode(M_TANDY4);
		}
		tandy_update_palette();
	} else {
		VGA_SetMode(M_TANDY_TEXT);
	}
}

static void PCJr_FindMode()
{
	new_cga = 1;
	if (vga.tandy.mode_control & 0x2) {
		if (vga.tandy.mode_control & 0x10) {
			/* bit4 of mode control 1 signals 16 colour graphics mode */
			if (vga.mode==M_TANDY4) VGA_SetModeNow(M_TANDY16); // TODO lowres mode only
			else VGA_SetMode(M_TANDY16);
		} else if (vga.tandy.gfx_control & 0x08) {
			/* bit3 of mode control 2 signals 2 colour graphics mode */
			if (cga_comp == 1 ||
			    (cga_comp == 0 && !(vga.tandy.mode_control & 0x4))) {
				VGA_SetMode(M_CGA2_COMPOSITE);
			} else {
				VGA_SetMode(M_TANDY2);
			}
		} else {
			/* otherwise some 4-colour graphics mode */
			const auto new_mode = (cga_comp == 1) ? M_CGA4_COMPOSITE
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

static void TandyCheckLineMask(void ) {
	if ( vga.tandy.extended_ram & 1 ) {
		vga.tandy.line_mask = 0;
	} else if ( vga.tandy.mode_control & 0x2) {
		vga.tandy.line_mask |= 1;
	}
	if ( vga.tandy.line_mask ) {
		vga.tandy.line_shift = 13;
		vga.tandy.addr_mask = (1 << 13) - 1;
	} else {
		vga.tandy.addr_mask = (Bitu)(~0);
		vga.tandy.line_shift = 0;
	}
}

static void write_tandy_reg(Bit8u val) {
	switch (vga.tandy.reg_index) {
	case 0x0:
		if (machine==MCH_PCJR) {
			vga.tandy.mode_control=val;
			VGA_SetBlinking(val & 0x20);
			PCJr_FindMode();
			if (val&0x8) vga.attr.disabled &= ~1;
			else vga.attr.disabled |= 1;
		} else {
			LOG(LOG_VGAMISC,LOG_NORMAL)("Unhandled Write %2X to tandy reg %X",val,vga.tandy.reg_index);
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
		vga.tandy.gfx_control=val;
		if (machine==MCH_TANDY) TANDY_FindMode();
		else PCJr_FindMode();
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

static void write_tandy(Bitu port, Bitu data, Bitu /*iolen*/)
{
	// only is passed 8-bit data given its IO port registration
	auto val = static_cast<uint8_t>(data);
	switch (port) {
	case 0x3d8:
		val &= 0x3f; // only bits 0-6 are used
		if (vga.tandy.mode_control ^ val) {
			vga.tandy.mode_control = val;
			if (val&0x8) vga.attr.disabled &= ~1;
			else vga.attr.disabled |= 1;
			TandyCheckLineMask();
			VGA_SetBlinking(val & 0x20);
			TANDY_FindMode();
			VGA_StartResize();
		}
		break;
	case 0x3d9:
		vga.tandy.color_select=val;
		tandy_update_palette();
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

static void write_pcjr(Bitu port,Bitu val,Bitu /*iolen*/) {
	switch (port) {
	case 0x3da:
		if (vga.tandy.pcjr_flipflop) write_tandy_reg((Bit8u)val);
		else {
			vga.tandy.reg_index=(Bit8u)val;
			if (vga.tandy.reg_index & 0x10)
				vga.attr.disabled |= 2;
			else vga.attr.disabled &= ~2;
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

		vga.tandy.line_mask = (Bit8u)(val >> 6);
		vga.tandy.draw_bank = val & ((vga.tandy.line_mask&2) ? 0x6 : 0x7);
		vga.tandy.mem_bank = (val >> 3) & 7;
		vga.tandy.draw_base = &MemBase[vga.tandy.draw_bank * 16 * 1024];
		vga.tandy.mem_base = &MemBase[vga.tandy.mem_bank * 16 * 1024];
		TandyCheckLineMask();
		VGA_SetupHandlers();
		break;
	}
}

static uint8_t palette_num(const char *colour)
{
	if (strcasecmp(colour, "green") == 0)
		return 0;
	if (strcasecmp(colour, "amber") == 0)
		return 1;
	if (strcasecmp(colour, "white") == 0)
		return 2;
	if (strcasecmp(colour, "paperwhite") == 0)
		return 3;
	return 2;
}

void VGA_SetMonoPalette(const char *colour)
{
	if (machine == MCH_HERC) {
		herc_pal = palette_num(colour);
		Herc_Palette();
		return;
	}
	if (machine == MCH_CGA && mono_cga) {
		mono_cga_pal = palette_num(colour);
		Mono_CGA_Palette();
		return;
	}
}

static void CycleMonoCGAPal(bool pressed) {
	if (!pressed) return;
	if (++mono_cga_pal>3) mono_cga_pal=0;
	Mono_CGA_Palette();
}

static void CycleMonoCGABright(bool pressed) {
	if (!pressed) return;
	if (++mono_cga_bright>1) mono_cga_bright=0;
	Mono_CGA_Palette();
}

void Mono_CGA_Palette(void) {
	for (Bit8u ct=0;ct<16;ct++) {
		VGA_DAC_SetEntry(ct,
						 mono_cga_palettes[2*mono_cga_pal+mono_cga_bright][ct][0],
						 mono_cga_palettes[2*mono_cga_pal+mono_cga_bright][ct][1],
						 mono_cga_palettes[2*mono_cga_pal+mono_cga_bright][ct][2]
		);
		VGA_DAC_CombineColor(ct,ct);
	}
}

static void CycleHercPal(bool pressed) {
	if (!pressed) return;
	if (++herc_pal>3) herc_pal=0;
	Herc_Palette();
	VGA_DAC_CombineColor(1,7);
}

void Herc_Palette()
{
	switch (herc_pal) {
	case 0: // Green
		VGA_DAC_SetEntry(0x7, 0x00, 0x26, 0x00);
		VGA_DAC_SetEntry(0xf, 0x00, 0x3f, 0x00);
		break;
	case 1: // Amber
		VGA_DAC_SetEntry(0x7, 0x34, 0x20, 0x00);
		VGA_DAC_SetEntry(0xf, 0x3f, 0x34, 0x00);
		break;
	case 2: // White
		VGA_DAC_SetEntry(0x7, 0x2a, 0x2a, 0x2a);
		VGA_DAC_SetEntry(0xf, 0x3f, 0x3f, 0x3f);
		break;
	case 3: // Paper-white
		VGA_DAC_SetEntry(0x7, 0x2d, 0x2e, 0x2d);
		VGA_DAC_SetEntry(0xf, 0x3f, 0x3f, 0x3b);
		break;
	}
}

static void write_hercules(Bitu port, uint8_t val, Bitu /*iolen*/)
{
	switch (port) {
	case 0x3b8: {
		// the protected bits can always be cleared but only be set if the
		// protection bits are set
		if (vga.herc.mode_control&0x2) {
			// already set
			if (!(val&0x2)) {
				vga.herc.mode_control &= ~0x2;
				VGA_SetMode(M_HERC_TEXT);
			}
		} else {
			// not set, can only set if protection bit is set
			if ((val & 0x2) && (vga.herc.enable_bits & 0x1)) {
				vga.herc.mode_control |= 0x2;
				VGA_SetMode(M_HERC_GFX);
			}
		}
		if (vga.herc.mode_control&0x80) {
			if (!(val&0x80)) {
				vga.herc.mode_control &= ~0x80;
				vga.tandy.draw_base = &vga.mem.linear[0];
			}
		} else {
			if ((val & 0x80) && (vga.herc.enable_bits & 0x2)) {
				vga.herc.mode_control |= 0x80;
				vga.tandy.draw_base = &vga.mem.linear[32*1024];
			}
		}
		vga.draw.blinking = (val&0x20)!=0;
		vga.herc.mode_control &= 0x82;
		vga.herc.mode_control |= val & ~0x82;
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

/* static Bitu read_hercules(Bitu port,Bitu iolen) {
	LOG_MSG("read from Herc port %x",port);
	return 0;
} */

Bitu read_herc_status(Bitu /*port*/,Bitu /*iolen*/) {
	// 3BAh (R):  Status Register
	// bit   0  Horizontal sync
	//       1  Light pen status (only some cards)
	//       3  Video signal
	//     4-6	000: Hercules
	//			001: Hercules Plus
	//			101: Hercules InColor
	//			111: Unknown clone
	//       7  Vertical sync inverted

	double timeInFrame = PIC_FullIndex()-vga.draw.delay.framestart;
	Bit8u retval=0x72; // Hercules ident; from a working card (Winbond W86855AF)
					// Another known working card has 0x76 ("KeysoGood", full-length)
	if (timeInFrame < vga.draw.delay.vrstart ||
		timeInFrame > vga.draw.delay.vrend) retval |= 0x80;

	double timeInLine=fmod(timeInFrame,vga.draw.delay.htotal);
	if (timeInLine >= vga.draw.delay.hrstart &&
		timeInLine <= vga.draw.delay.hrend) retval |= 0x1;

	// 688 Attack sub checks bit 3 - as a workaround have the bit enabled
	// if no sync active (corresponds to a completely white screen)
	if ((retval&0x81)==0x80) retval |= 0x8;
	return retval;
}

void VGA_SetupOther(void)
{
	memset(&vga.tandy, 0, sizeof(vga.tandy));
	vga.attr.disabled = 0;
	vga.config.bytes_skip=0;

	//Initialize values common for most machines, can be overwritten
	vga.tandy.draw_base = vga.mem.linear;
	vga.tandy.mem_base = vga.mem.linear;
	vga.tandy.addr_mask = 8*1024 - 1;
	vga.tandy.line_mask = 3;
	vga.tandy.line_shift = 13;

	if (machine==MCH_CGA || IS_TANDY_ARCH) {
		extern Bit8u int10_font_08[256 * 8];
		for (int i = 0; i < 256; ++i) {
			memcpy(&vga.draw.font[i * 32], &int10_font_08[i * 8], 8);
		}
		vga.draw.font_tables[0] = vga.draw.font_tables[1] = vga.draw.font;
	}
	if (machine==MCH_CGA || IS_TANDY_ARCH || machine==MCH_HERC) {
		IO_RegisterWriteHandler(0x3db,write_lightpen,IO_MB);
		IO_RegisterWriteHandler(0x3dc,write_lightpen,IO_MB);
	}
	if (machine==MCH_HERC) {
		extern Bit8u int10_font_14[256 * 14];
		for (int i = 0; i < 256; ++i) {
			memcpy(&vga.draw.font[i * 32], &int10_font_14[i * 14], 14);
		}
		vga.draw.font_tables[0] = vga.draw.font_tables[1] = vga.draw.font;
		MAPPER_AddHandler(CycleHercPal, SDL_SCANCODE_F11, 0,
		                  "hercpal", "Herc Pal");
	}
	if (machine==MCH_CGA) {
		IO_RegisterWriteHandler(0x3d8,write_cga,IO_MB);
		IO_RegisterWriteHandler(0x3d9,write_cga,IO_MB);
		if (!mono_cga) {
		MAPPER_AddHandler(IncreaseWhichControl,SDL_SCANCODE_F10,0, "select","Sel Ctl");
		MAPPER_AddHandler(DecreaseControlValue,SDL_SCANCODE_F11,MMOD2,"decval","Dec Val");
		MAPPER_AddHandler(IncreaseControlValue,SDL_SCANCODE_F11,0,"incval","Inc Val");
			MAPPER_AddHandler(Composite, SDL_SCANCODE_F12, 0,
			                  "cgacomp", "CGA Comp");
		} else {
			MAPPER_AddHandler(CycleMonoCGAPal, SDL_SCANCODE_F11, 0,
			                  "monocgapal", "Mono CGA Pal");
			MAPPER_AddHandler(CycleMonoCGABright, SDL_SCANCODE_F11, MMOD2,
			                  "monocgabright", "Mono CGA Bright");
		}
	}
	if (machine==MCH_TANDY) {
		write_tandy( 0x3df, 0x0, 0 );
		IO_RegisterWriteHandler(0x3d8,write_tandy,IO_MB);
		IO_RegisterWriteHandler(0x3d9,write_tandy,IO_MB);
		IO_RegisterWriteHandler(0x3da,write_tandy,IO_MB);
		IO_RegisterWriteHandler(0x3de,write_tandy,IO_MB);
		IO_RegisterWriteHandler(0x3df,write_tandy,IO_MB);
	}
	if (machine==MCH_PCJR) {
		//write_pcjr will setup base address
		write_pcjr( 0x3df, 0x7 | (0x7 << 3), 0 );
		IO_RegisterWriteHandler(0x3da,write_pcjr,IO_MB);
		IO_RegisterWriteHandler(0x3df,write_pcjr,IO_MB);
		MAPPER_AddHandler(CycleWhichControl, SDL_SCANCODE_F10, 0,
		                  "select", "Sel Ctl");
		MAPPER_AddHandler(DecreaseControlValue, SDL_SCANCODE_F11, MMOD2,
		                  "decval", "Dec Val");
		MAPPER_AddHandler(IncreaseControlValue, SDL_SCANCODE_F11, 0,
		                  "incval", "Inc Val");
		MAPPER_AddHandler(Composite, SDL_SCANCODE_F12, 0, "cgacomp",
		                  "CGA Comp");
	}
	if (machine == MCH_HERC) {
		Bitu base=0x3b0;
		for (uint8_t i = 0; i < 4; ++i) {
			// The registers are repeated as the address is not decoded properly;
			// The official ports are 3b4, 3b5
			const auto index_port = base + i * 2;
			IO_RegisterWriteHandler(index_port, write_crtc_index_other, IO_MB);
			IO_RegisterReadHandler(index_port, read_crtc_index_other, IO_MB);

			const auto data_port = index_port + 1;
			IO_RegisterWriteHandler(data_port, write_crtc_data_other, IO_MB);
			IO_RegisterReadHandler(data_port, read_crtc_data_other, IO_MB);
		}
		vga.herc.enable_bits=0;
		vga.herc.mode_control=0xa; // first mode written will be text mode
		vga.crtc.underline_location = 13;
		IO_RegisterWriteHandler(0x3b8,write_hercules,IO_MB);
		IO_RegisterWriteHandler(0x3bf,write_hercules,IO_MB);
		IO_RegisterReadHandler(0x3ba,read_herc_status,IO_MB);
	} else if (!IS_EGAVGA_ARCH) {
		Bitu base=0x3d0;
		for (Bitu port_ct=0; port_ct<4; port_ct++) {
			IO_RegisterWriteHandler(base+port_ct*2,write_crtc_index_other,IO_MB);
			IO_RegisterWriteHandler(base+port_ct*2+1,write_crtc_data_other,IO_MB);
			IO_RegisterReadHandler(base+port_ct*2,read_crtc_index_other,IO_MB);
			IO_RegisterReadHandler(base+port_ct*2+1,read_crtc_data_other,IO_MB);
		}
	}
}
