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
#include <utility>

#include "../ints/int10.h"
#include "logging.h"
#include "math_utils.h"
#include "pic.h"
#include "video.h"

VGA_Type vga;
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

// Get the current video mode's type and numeric ID
std::pair<VGAModes, uint16_t> VGA_GetCurrentMode()
{
	return {CurMode->type, CurMode->mode};
}

// Describes the given video mode's type and ID, ie: "VGA, "256 colour"
std::pair<const char*, const char*> VGA_DescribeMode(const VGAModes video_mode_type,
                                                     const uint16_t video_mode_id,
                                                     const uint16_t width,
                                                     const uint16_t height)
{
	auto maybe_cga_on_tandy = [=]() {
		constexpr uint16_t cga_max_mode = 0x006;
		return video_mode_id <= cga_max_mode ? "CGA" : "Tandy";
	};

	auto maybe_mcga_if_320x200 = [=]() {
		constexpr uint16_t mcga_width  = 320;
		constexpr uint16_t mcga_height = 200;

		const auto is_single_scan_mcga = (width == mcga_width &&
		                                  height == mcga_height);
		const auto is_double_scan_mcga = (width == (mcga_width * 2) &&
		                                  height == (mcga_height * 2));

		if (is_single_scan_mcga || is_double_scan_mcga) {
			return "MCGA";
		}
		return "VGA";
	};

	// clang-format off
	switch (video_mode_type) {
	case M_HERC_TEXT:          return {"Text",     "monochrome"};
	case M_HERC_GFX:           return {"Hercules", "monochrome"};
	case M_TEXT:
	case M_TANDY_TEXT:
	case M_CGA_TEXT_COMPOSITE: return {"Text",  "16-colour"};
	case M_CGA2_COMPOSITE:
	case M_CGA4_COMPOSITE:     return {"CGA",   "composite"};
	case M_CGA2:               return {"CGA",   "2-colour"};
	case M_CGA4:               return {"CGA",   "4-colour"};
	case M_CGA16:              return {"CGA",   "16-colour"};
	case M_TANDY2:             return {maybe_cga_on_tandy(), "2-colour"};
	case M_TANDY4:             return {maybe_cga_on_tandy(), "4-colour"};
	case M_TANDY16:            return {maybe_cga_on_tandy(), "16-colour"};
	case M_EGA: // see comment below
	    switch (video_mode_id) {
	    case 0x011:            return {"MCGA",  "2-colour"};
	    case 0x012:            return {"VGA",   "16-colour"};
	    default:               return {"EGA",   "16-colour"};
	    }
	case M_VGA:                return {maybe_mcga_if_320x200(), "256-colour"};
	case M_LIN4:               return {"VESA",  "16-colour"};
	case M_LIN8:               return {"VESA",  "256-colour"};
	case M_LIN15:              return {"VESA",  "15-bit"};
	case M_LIN16:              return {"VESA",  "16-bit"};
	case M_LIN24:
	case M_LIN32:              return {"VESA",  "24-bit"};

	case M_ERROR:
	default:
		// Should not occur; log the values and inform the user 
		LOG_ERR("VIDEO: Unknown mode: %u with ID: %u",
		        static_cast<uint32_t>(video_mode_type),
		        video_mode_id);
		return {"Unknown mode", "Unknown colour-depth"};
	}
	// clang-format on

	// Modes 11h and 12h were supported by high-end EGA cards and because of
	// that operate internally more like EGA modes (so DOSBox uses the EGA
	// type for them), however they were classified as VGA from a standards
	// perspective, so we report them as such.
	// References:
	// [1] IBM VGA Technical Reference, Mode of Operation, pp 2-12, 19
	// March, 1992. [2] "IBM PC Family- BIOS Video Modes",
	// http://minuszerodegrees.net/video/bios_video_modes.htm
}

void VGA_LogInitialization(const char *adapter_name,
                           const char *ram_type,
                           const size_t num_modes)
{
	const auto mem_in_kib = vga.vmemsize / 1024;
	LOG_INFO("VIDEO: Initialised %s with %d %s of %s supporting %d modes",
	         adapter_name, mem_in_kib < 1024 ? mem_in_kib : mem_in_kib / 1024,
	         mem_in_kib < 1024 ? "KB" : "MB", ram_type,
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
		if (vga.attr.mode_control & 1) { // graphics mode
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
		return vga.draw.dos_refresh_hz;
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

// Are we using a VGA card, in a sub-350 line mode, and asked to draw the
// double-scanned lines? (This is a helper function to avoid repeating this
// logic in the VGA drawing and CRTC areas).
bool VGA_IsDoubleScanningSub350LineModes()
{
	return IS_VGA_ARCH &&
	       vga.draw.vga_sub_350_line_handling ==
	               VgaSub350LineHandling::DoubleScan &&
	       (vga.mode == M_EGA || vga.mode == M_VGA);

	// TODO: Non-composite CGA modes should be included here too, as VGA
	// cards also line-doubled those modes, however more work is needed to
	// correctly draw double-scanned CGA lines.
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
	VGA_S3::clk_t best_clk;
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

void VGA_SetVgaSub350LineHandling(const VgaSub350LineHandling vga_sub_350_line_handling)
{
	if (vga.draw.vga_sub_350_line_handling ==
	    VgaSub350LineHandling::ForceSingleScan) {
		return;
	}
	vga.draw.vga_sub_350_line_handling = vga_sub_350_line_handling;
}

static void set_vga_single_scanning_pref()
{
	const auto conf    = control->GetSection("dosbox");
	const auto section = dynamic_cast<Section_prop*>(conf);

	if (section && section->Get_bool("force_vga_single_scan")) {
		vga.draw.vga_sub_350_line_handling = VgaSub350LineHandling::ForceSingleScan;
		LOG_MSG("VIDEO: Single-scanning sub-350 line modes for VGA machine types");
	} else {
		vga.draw.vga_sub_350_line_handling = {};
	}
}

void VGA_Init(Section* sec)
{
	set_vga_single_scanning_pref();

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
