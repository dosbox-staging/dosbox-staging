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

#include "vga.h"

#include <cassert>
#include <cstring>
#include <string>
#include <utility>

#include "../ints/int10.h"
#include "logging.h"
#include "math_utils.h"
#include "pic.h"
#include "string_utils.h"
#include "video.h"

VgaType vga;
SVGA_Driver svga;

uint32_t CGA_2_Table[16];
uint32_t CGA_4_Table[256];
uint32_t CGA_4_HiRes_Table[256];
int CGA_Composite_Table[1024];
uint32_t TXT_Font_Table[16];
uint32_t TXT_FG_Table[16];
uint32_t TXT_BG_Table[16];
uint32_t ExpandTable[256];
uint32_t Expand16Table[4][16];
uint32_t FillTable[16];

void VGA_LogInitialization(const char *adapter_name,
                           const char *ram_type,
                           const size_t num_modes)
{
	const auto mem_in_kb = vga.vmemsize / 1024;
	LOG_INFO("VIDEO: Initialised %s with %d %s of %s supporting %d modes",
	         adapter_name, mem_in_kb < 1024 ? mem_in_kb : mem_in_kb / 1024,
	         mem_in_kb < 1024 ? "KB" : "MB", ram_type,
	         check_cast<int16_t>(num_modes));
}

void VGA_SetModeNow(VGAModes mode) {
	if (vga.mode == mode) return;
	vga.mode=mode;
	VGA_SetupHandlers();
	VGA_StartResizeAfter(0);
}


void VGA_SetMode(VGAModes mode) {
	if (vga.mode == mode) return;
	vga.mode=mode;
	VGA_SetupHandlers();
	VGA_StartResize();
}

void VGA_DetermineMode(void) {
	if (svga.determine_mode) {
		svga.determine_mode();
		return;
	}
	/* Test for VGA output active or direct colour modes */
	switch (vga.s3.misc_control_2 >> 4) {
	case 0:
		if (vga.attr.mode_control.is_graphics_enabled) {
			if (IS_VGA_ARCH && (vga.gfx.mode & 0x40)) {
				// access above 256k?
				if (vga.s3.reg_31 & 0x8) VGA_SetMode(M_LIN8);
				else VGA_SetMode(M_VGA);
			}
			else if (vga.gfx.mode & 0x20) VGA_SetMode(M_CGA4);
			else if ((vga.gfx.miscellaneous & 0x0c)==0x0c) VGA_SetMode(M_CGA2);
			else {
				// access above 256k?
				if (vga.s3.reg_31 & 0x8) VGA_SetMode(M_LIN4);
				else VGA_SetMode(M_EGA);
			}
		} else {
			VGA_SetMode(M_TEXT);
		}
		break;
	case 1:VGA_SetMode(M_LIN8);break;
	case 3:VGA_SetMode(M_LIN15);break;
	case 5:VGA_SetMode(M_LIN16);break;
	case 7: VGA_SetMode(M_LIN24); break;
	case 13:VGA_SetMode(M_LIN32);break;
	}
}

const char* to_string(const GraphicsStandard g)
{
	switch (g) {
	case GraphicsStandard::Hercules: return "Hercules";
	case GraphicsStandard::Cga: return "CGA";
	case GraphicsStandard::Pcjr: return "PCjr";
	case GraphicsStandard::Tga: return "Tandy";
	case GraphicsStandard::Ega: return "EGA";
	case GraphicsStandard::Vga: return "VGA";
	case GraphicsStandard::Svga: return "SVGA";
	case GraphicsStandard::Vesa: return "VESA";
	default: assertm(false, "Invalid GraphicsStandard"); return "";
	}
}

const char* to_string(const ColorDepth c)
{
	switch (c) {
	case ColorDepth::Monochrome: return "monochrome";
	case ColorDepth::Composite: return "composite";
	case ColorDepth::IndexedColor2: return "2-colour";
	case ColorDepth::IndexedColor4: return "4-colour";
	case ColorDepth::IndexedColor16: return "16-colour";
	case ColorDepth::IndexedColor256: return "256-colour";
	case ColorDepth::HighColor15Bit: return "15-bit high colour";
	case ColorDepth::HighColor16Bit: return "16-bit high colour";
	case ColorDepth::TrueColor24Bit: return "24-bit true colour";
	default: assertm(false, "Invalid ColorDepth"); return "";
	}
}

// Return a human-readable description of the video mode, e.g.:
//   - "CGA 640x200 16-colour text mode 03h"
//   - "EGA 640x350 16-colour graphics mode 10h"
//   - "VGA 720x400 16-colour text mode 03h"
//   - "VGA 320x200 256-colour graphics mode 13h"
//   - "VGA 360x240 256-colour graphics mode"
//   - "VESA 800x600 256-colour graphics mode 103h"
std::string to_string(const VideoMode& video_mode)
{
	const char* mode_type = (video_mode.is_graphics_mode ? "graphics mode"
	                                                     : "text mode");

	const auto mode_number = (video_mode.is_custom_mode
	                                  ? ""
	                                  : format_string(" %02Xh",
	                                                  video_mode.bios_mode_number));

	return format_string("%s %dx%d %s %s%s",
	                     to_string(video_mode.graphics_standard),
	                     video_mode.width,
	                     video_mode.height,
	                     to_string(video_mode.color_depth),
	                     mode_type,
	                     mode_number.c_str());
}

const char* to_string(const VGAModes mode)
{
	switch (mode) {
	case M_CGA2: return "M_CGA2";
	case M_CGA4: return "M_CGA4";
	case M_EGA: return "M_EGA";
	case M_VGA: return "M_VGA";
	case M_LIN4: return "M_LIN4";
	case M_LIN8: return "M_LIN8";
	case M_LIN15: return "M_LIN15";
	case M_LIN16: return "M_LIN16";
	case M_LIN24: return "M_LIN24";
	case M_LIN32: return "M_LIN32";
	case M_TEXT: return "M_TEXT";
	case M_HERC_GFX: return "M_HERC_GFX";
	case M_HERC_TEXT: return "M_HERC_TEXT";
	case M_TANDY2: return "M_TANDY2";
	case M_TANDY4: return "M_TANDY4";
	case M_TANDY16: return "M_TANDY16";
	case M_TANDY_TEXT: return "M_TANDY_TEXT";
	case M_CGA16: return "M_CGA16";
	case M_CGA2_COMPOSITE: return "M_CGA2_COMPOSITE";
	case M_CGA4_COMPOSITE: return "M_CGA4_COMPOSITE";
	case M_CGA_TEXT_COMPOSITE: return "M_CGA_TEXT_COMPOSITE";
	case M_ERROR: return "M_ERROR";
	default: assertm(false, "Invalid VGAMode"); return "";
	}
}

void VGA_StartResize()
{
	// Once requested, start the VGA resize within half the current VGA mode's
	// frame time, typically between 4ms and 8ms. The goal is to mimick the time
	// taken for video card to process and establish its new state based on the
	// CRTC registers.
	//
	// If this duration is too long, games like Earthworm Jim and Prehistorik 2
	// might have subtle visible glitches. If this gets too short, emulation
	// might lockup because the VGA state needs to change across some finite
	// duration.
	//
	constexpr auto max_frame_period_ms = 1000.0 /*ms*/ / 50 /*Hz*/;
	constexpr auto min_frame_period_ms = 1000.0 /*ms*/ / 120 /*Hz*/;

	const auto half_frame_period_ms = clamp(vga.draw.delay.vtotal,
	                                        min_frame_period_ms,
	                                        max_frame_period_ms) / 2;

	VGA_StartResizeAfter(static_cast<int16_t>(half_frame_period_ms));
}

void VGA_StartResizeAfter(const uint16_t delay_ms)
{
	if (vga.draw.resizing) {
		return;
	}

	vga.draw.resizing = true;
	if (delay_ms == 0) {
		VGA_SetupDrawing(0);
	} else {
		PIC_AddEvent(VGA_SetupDrawing, delay_ms);
	}
}

void VGA_SetHostRate(const double refresh_hz)
{
	// may come from user content, so always clamp it
	constexpr auto min_rate = static_cast<double>(RefreshRateMin);
	constexpr auto max_rate = static_cast<double>(RefreshRateMax);
	vga.draw.host_refresh_hz = clamp(refresh_hz,min_rate, max_rate);
}

void VGA_SetRatePreference(const std::string &pref)
{
	if (pref == "default") {
		vga.draw.dos_rate_mode = VgaRateMode::Default;
		LOG_MSG("VIDEO: Using the DOS video mode's frame rate");

	} else if (pref == "host") {
		vga.draw.dos_rate_mode = VgaRateMode::Host;
		LOG_MSG("VIDEO: Matching the DOS graphical frame rate to the host");

	} else if (const auto rate = to_finite<double>(pref); std::isfinite(rate)) {
		vga.draw.dos_rate_mode = VgaRateMode::Custom;
		constexpr auto min_rate = static_cast<double>(RefreshRateMin);
		constexpr auto max_rate = static_cast<double>(RefreshRateMax);
		vga.draw.custom_refresh_hz = clamp(rate, min_rate, max_rate);
		LOG_MSG("VIDEO: Using a custom DOS graphical frame rate of %.3g Hz",
		        vga.draw.custom_refresh_hz);

	} else {
		vga.draw.dos_rate_mode = VgaRateMode::Default;
		LOG_WARNING("VIDEO: Unknown frame rate setting: %s, using default",
		            pref.c_str());
	}
}

double VGA_GetPreferredRate()
{
	switch (vga.draw.dos_rate_mode) {
	case VgaRateMode::Default:
		// If another device is overriding our VGA card, then use its rate
		return vga.draw.vga_override ? vga.draw.override_refresh_hz
		                             : vga.draw.dos_refresh_hz;
	case VgaRateMode::Host:
		assert(vga.draw.host_refresh_hz > RefreshRateMin);
		return vga.draw.host_refresh_hz;
	case VgaRateMode::Custom:
		assert(vga.draw.custom_refresh_hz >= RefreshRateMin);
		assert(vga.draw.custom_refresh_hz <= RefreshRateMax);
		return vga.draw.custom_refresh_hz;
	}
	return vga.draw.dos_refresh_hz;
}

void VGA_SetClock(const Bitu which, const uint32_t desired_clock)
{
	if (svga.set_clock) {
		svga.set_clock(which, desired_clock);
		return;
	}

	// Ensure the target clock is within the S3's clock range
	const auto clock = clamp(static_cast<int>(desired_clock), S3_CLOCK_REF,
	                         S3_MAX_CLOCK);

	// The clk parameters (r, n, m) will be populated with those that find a
	// clock closest to the desired_clock clock.
	VgaS3::clk_t best_clk;
	auto best_error = clock;

	uint8_t r = 0;
	for (r = 0; r <= 3; ++r) {
		// Is r out of bounds?
		const auto f_vco = clock * (1 << r);
		if (MIN_VCO <= f_vco && f_vco <= MAX_VCO)
			break;
	}
	for (uint8_t n = 1; n <= 31; ++n) {
		// Is m out of bounds?
		const auto m = (clock * (n + 2) * (1 << r) + (S3_CLOCK_REF / 2)) /
		                       S3_CLOCK_REF - 2;
		if (m > 127)
			continue;

		// Do the parameters produce a clock further away than
		// the best combination?
		const auto candidate_clock = S3_CLOCK(m, n, r);
		const auto error = abs(candidate_clock - clock);
		if (error >= best_error)
			continue;

		// Save the improved clock paramaters
		best_error = error;
		best_clk.r = r;
		best_clk.m = static_cast<uint8_t>(m);
		best_clk.n = n;
	}
	// LOG_MSG("VGA: Clock[%lu] r=%u, n=%u, m=%u (desired_clock = %u, actual = %u KHz",
	//         which, best_clk.r, best_clk.n, best_clk.m, desired_clock,
	//         S3_CLOCK(best_clk.m, best_clk.n, best_clk.r));

	// Save the best clock and then program the S3 chip.
	assert(which < ARRAY_LEN(vga.s3.clk));
	vga.s3.clk[which] = best_clk;
	VGA_StartResize();
}

void VGA_SetCGA2Table(uint8_t val0,uint8_t val1) {
	uint8_t total[2]={ val0,val1};
	for (Bitu i=0;i<16;i++) {
		CGA_2_Table[i]=
#ifdef WORDS_BIGENDIAN
			(total[(i >> 0) & 1] << 0  ) | (total[(i >> 1) & 1] << 8  ) |
			(total[(i >> 2) & 1] << 16 ) | (total[(i >> 3) & 1] << 24 );
#else 
			(total[(i >> 3) & 1] << 0  ) | (total[(i >> 2) & 1] << 8  ) |
			(total[(i >> 1) & 1] << 16 ) | (total[(i >> 0) & 1] << 24 );
#endif
	}
}

void VGA_SetCGA4Table(uint8_t val0,uint8_t val1,uint8_t val2,uint8_t val3) {
	uint8_t total[4]={ val0,val1,val2,val3};
	for (Bitu i=0;i<256;i++) {
		CGA_4_Table[i]=
#ifdef WORDS_BIGENDIAN
			(total[(i >> 0) & 3] << 0  ) | (total[(i >> 2) & 3] << 8  ) |
			(total[(i >> 4) & 3] << 16 ) | (total[(i >> 6) & 3] << 24 );
#else
			(total[(i >> 6) & 3] << 0  ) | (total[(i >> 4) & 3] << 8  ) |
			(total[(i >> 2) & 3] << 16 ) | (total[(i >> 0) & 3] << 24 );
#endif
		CGA_4_HiRes_Table[i]=
#ifdef WORDS_BIGENDIAN
			(total[((i >> 0) & 1) | ((i >> 3) & 2)] << 0  ) | (total[((i >> 1) & 1) | ((i >> 4) & 2)] << 8  ) |
			(total[((i >> 2) & 1) | ((i >> 5) & 2)] << 16 ) | (total[((i >> 3) & 1) | ((i >> 6) & 2)] << 24 );
#else
			(total[((i >> 3) & 1) | ((i >> 6) & 2)] << 0  ) | (total[((i >> 2) & 1) | ((i >> 5) & 2)] << 8  ) |
			(total[((i >> 1) & 1) | ((i >> 4) & 2)] << 16 ) | (total[((i >> 0) & 1) | ((i >> 3) & 2)] << 24 );
#endif
	}	
}

void VGA_ForceSquarePixels(const bool enable)
{
	vga.draw.force_square_pixels = enable;
}

void VGA_EnableVgaDoubleScanning(const bool enable)
{
	if (machine != MCH_VGA) {
		return;
	}
	if (enable && !vga.draw.double_scanning_enabled) {
		LOG_MSG("VGA: Double-scanning VGA video modes enabled");
	}
	if (!enable && vga.draw.double_scanning_enabled) {
		LOG_MSG("VGA: Forcing single-scanning of double-scanned VGA video modes");
	}
	vga.draw.double_scanning_enabled = enable;
}

void VGA_EnablePixelDoubling(const bool enable)
{
	if (enable && !vga.draw.pixel_doubling_enabled) {
		LOG_MSG("VGA: Pixel-doubling enabled");
	}
	if (!enable && vga.draw.pixel_doubling_enabled) {
		LOG_MSG("VGA: Forcing no pixel-doubling");
	}
	vga.draw.pixel_doubling_enabled = enable;
}

void VGA_Init(Section* sec)
{
	vga.draw.resizing = false;
	vga.mode          = M_ERROR; // For first init
	SVGA_Setup_Driver();
	VGA_SetupMemory(sec);
	VGA_SetupMisc();
	VGA_SetupDAC();
	VGA_SetupGFX();
	VGA_SetupSEQ();
	VGA_SetupAttr();
	VGA_SetupOther();
	VGA_SetupXGA();
	VGA_SetClock(0,CLK_25);
	VGA_SetClock(1,CLK_28);
/* Generate tables */
	VGA_SetCGA2Table(0,1);
	VGA_SetCGA4Table(0,1,2,3);
	Bitu i,j;
	for (i=0;i<256;i++) {
		ExpandTable[i]=i | (i << 8)| (i <<16) | (i << 24);
	}
	for (i=0;i<16;i++) {
		TXT_FG_Table[i]=i | (i << 8)| (i <<16) | (i << 24);
		TXT_BG_Table[i]=i | (i << 8)| (i <<16) | (i << 24);
#ifdef WORDS_BIGENDIAN
		FillTable[i]=
			((i & 1) ? 0xff000000 : 0) |
			((i & 2) ? 0x00ff0000 : 0) |
			((i & 4) ? 0x0000ff00 : 0) |
			((i & 8) ? 0x000000ff : 0) ;
		TXT_Font_Table[i]=
			((i & 1) ? 0x000000ff : 0) |
			((i & 2) ? 0x0000ff00 : 0) |
			((i & 4) ? 0x00ff0000 : 0) |
			((i & 8) ? 0xff000000 : 0) ;
#else 
		FillTable[i]=
			((i & 1) ? 0x000000ff : 0) |
			((i & 2) ? 0x0000ff00 : 0) |
			((i & 4) ? 0x00ff0000 : 0) |
			((i & 8) ? 0xff000000 : 0) ;
		TXT_Font_Table[i]=	
			((i & 1) ? 0xff000000 : 0) |
			((i & 2) ? 0x00ff0000 : 0) |
			((i & 4) ? 0x0000ff00 : 0) |
			((i & 8) ? 0x000000ff : 0) ;
#endif
	}
	for (j=0;j<4;j++) {
		for (i=0;i<16;i++) {
#ifdef WORDS_BIGENDIAN
			Expand16Table[j][i] =
				((i & 1) ? 1 << j : 0) |
				((i & 2) ? 1 << (8 + j) : 0) |
				((i & 4) ? 1 << (16 + j) : 0) |
				((i & 8) ? 1 << (24 + j) : 0);
#else
			Expand16Table[j][i] =
				((i & 1) ? 1 << (24 + j) : 0) |
				((i & 2) ? 1 << (16 + j) : 0) |
				((i & 4) ? 1 << (8 + j) : 0) |
				((i & 8) ? 1 << j : 0);
#endif
		}
	}
}

void SVGA_Setup_Driver(void) {
	memset(&svga, 0, sizeof(SVGA_Driver));

	switch(svgaCard) {
	case SVGA_S3Trio:
		SVGA_Setup_S3Trio();
		break;
	case SVGA_TsengET4K:
		SVGA_Setup_TsengET4K();
		break;
	case SVGA_TsengET3K:
		SVGA_Setup_TsengET3K();
		break;
	case SVGA_ParadisePVGA1A:
		SVGA_Setup_ParadisePVGA1A();
		break;
	default:
		vga.vmemsize = vga.vmemwrap = 256*1024;
		break;
	}
}

const VideoMode& VGA_GetCurrentVideoMode() {
	return vga.draw.render.video_mode;
}

