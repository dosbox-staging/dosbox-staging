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

#ifndef DOSBOX_VGA_H
#define DOSBOX_VGA_H

#include "dosbox.h"

#include "inout.h"
#include "control.h"

//Don't enable keeping changes and mapping lfb probably...
#define VGA_LFB_MAPPED
//#define VGA_KEEP_CHANGES
#define VGA_CHANGE_SHIFT	9

class PageHandler;

enum VGAModes {
	M_CGA2 = 1 << 0,
	M_CGA4 = 1 << 1,
	M_EGA = 1 << 2,
	M_VGA = 1 << 3,
	M_LIN4 = 1 << 4,
	M_LIN8 = 1 << 5,
	M_LIN15 = 1 << 6,
	M_LIN16 = 1 << 7,
	M_LIN24 = 1 << 8,
	M_LIN32 = 1 << 9,
	M_TEXT = 1 << 10,
	M_HERC_GFX = 1 << 11,
	M_HERC_TEXT = 1 << 12,
	M_TANDY2 = 1 << 13,
	M_TANDY4 = 1 << 14,
	M_TANDY16 = 1 << 15,
	M_TANDY_TEXT = 1 << 16,
	M_CGA16 = 1 << 17,
	M_CGA2_COMPOSITE = 1 << 18,
	M_CGA4_COMPOSITE = 1 << 19,
	M_CGA_TEXT_COMPOSITE = 1 << 20,
	// bits 20 through 30 for more modes
	M_ERROR = 1 << 31,
};

constexpr uint16_t EGA_HALF_CLOCK = 1 << 0;
constexpr uint16_t EGA_LINE_DOUBLE = 1 << 1;
constexpr uint16_t VGA_PIXEL_DOUBLE = 1 << 2;
constexpr uint16_t VGA_DOUBLE_CLOCK = 1 << 3;

#define CLK_25 25175
#define CLK_28 28322

#define MIN_VCO	180000
#define MAX_VCO 360000

#define S3_CLOCK_REF	14318	/* KHz */
#define S3_CLOCK(_M,_N,_R)	((S3_CLOCK_REF * ((_M) + 2)) / (((_N) + 2) * (1 << (_R))))
#define S3_MAX_CLOCK	150000	/* KHz */

#define S3_XGA_1024 0x00
#define S3_XGA_1152 0x01
#define S3_XGA_640  0x40
#define S3_XGA_800  0x80
#define S3_XGA_1280 0xc0
#define S3_XGA_1600 0x81
#define S3_XGA_WMASK \
	(S3_XGA_640 | S3_XGA_800 | S3_XGA_1024 | S3_XGA_1152 | S3_XGA_1280 | S3_XGA_1600)

#define S3_XGA_8BPP  0x00
#define S3_XGA_16BPP 0x10
#define S3_XGA_32BPP 0x30
#define S3_XGA_CMASK (S3_XGA_8BPP|S3_XGA_16BPP|S3_XGA_32BPP)

struct VGA_Internal {
	bool attrindex = false;
};

struct VGA_Config {
	/* Memory handlers */
	Bitu mh_mask = 0;

	/* Video drawing */
	uint32_t display_start = 0;
	Bitu real_start = 0;
	bool retrace = false; /* A retrace is active */
	Bitu scan_len = 0;
	Bitu cursor_start = 0;

	/* Some other screen related variables */
	Bitu line_compare = 0;
	bool chained = false; /* Enable or Disabled Chain 4 Mode */
	bool compatible_chain4 = false;

	/* Pixel Scrolling */
	Bit8u pel_panning = 0; /* Amount of pixels to skip when starting
	                          horizontal line */
	Bit8u hlines_skip = 0;
	Bit8u bytes_skip = 0;
	Bit8u addr_shift = 0;

	/* Specific stuff memory write/read handling */

	Bit8u read_mode = 0;
	Bit8u write_mode = 0;
	Bit8u read_map_select = 0;
	Bit8u color_dont_care = 0;
	Bit8u color_compare = 0;
	Bit8u data_rotate = 0;
	Bit8u raster_op = 0;

	Bit32u full_bit_mask = 0;
	Bit32u full_map_mask = 0;
	Bit32u full_not_map_mask = 0;
	Bit32u full_set_reset = 0;
	Bit32u full_not_enable_set_reset = 0;
	Bit32u full_enable_set_reset = 0;
	Bit32u full_enable_and_set_reset = 0;
};

enum Drawmode { PART, DRAWLINE, EGALINE };

struct VGA_Draw {
	bool resizing = false;
	Bitu width = 0;
	Bitu height = 0;
	uint32_t blocks = 0;
	Bitu address = 0;
	uint16_t panning = 0;
	Bitu bytes_skip = 0;
	Bit8u *linear_base = nullptr;
	Bitu linear_mask = 0;
	Bitu address_add = 0;
	uint32_t line_length = 0;
	uint32_t address_line_total = 0;
	Bitu address_line = 0;
	uint32_t lines_total = 0;
	Bitu vblank_skip = 0;
	uint32_t lines_done = 0;
	Bitu lines_scaled = 0;
	Bitu split_line = 0;
	uint32_t parts_total = 0;
	uint32_t parts_lines = 0;
	uint32_t parts_left = 0;
	Bitu byte_panning_shift = 0;
	struct {
		double framestart = 0;
		double vrstart = 0, vrend = 0;     // V-retrace
		double hrstart = 0, hrend = 0;     // H-retrace
		double hblkstart = 0, hblkend = 0; // H-blanking
		double vblkstart = 0, vblkend = 0; // V-Blanking
		double vdend = 0, vtotal = 0;
		double hdend = 0, htotal = 0;
		double parts = 0;
	} delay;
	Bitu bpp = 0;
	double aspect_ratio = 0;
	bool double_scan = false;
	bool doublewidth = false;
	bool doubleheight = false;
	Bit8u font[64 * 1024] = {};
	Bit8u *font_tables[2] = {nullptr, nullptr};
	Bitu blinking = 0;
	bool blink = false;
	bool char9dot = false;
	struct {
		Bitu address = 0;
		Bit8u sline = 0;
		uint8_t eline = 0;
		Bit8u count = 0;
		uint8_t delay = 0;
		Bit8u enabled = 0;
	} cursor;
	Drawmode mode;
	bool vret_triggered = false;
	bool vga_override = false;
};

struct VGA_HWCURSOR {
	Bit8u curmode = 0;
	Bit16u originx = 0;
	uint16_t originy = 0;
	Bit8u fstackpos = 0;
	uint8_t bstackpos = 0;
	Bit8u forestack[4] = {};
	Bit8u backstack[4] = {};
	Bit16u startaddr = 0;
	Bit8u posx = 0;
	uint8_t posy = 0;
	Bit8u mc[64][64] = {};
};

struct VGA_S3 {
	Bit8u reg_lock1 = 0;
	Bit8u reg_lock2 = 0;
	Bit8u reg_31 = 0;
	Bit8u reg_35 = 0;
	Bit8u reg_36 = 0; // RAM size
	Bit8u reg_3a = 0; // 4/8/doublepixel bit in there
	Bit8u reg_40 = 0; // 8415/A functionality register
	Bit8u reg_41 = 0; // BIOS flags
	Bit8u reg_43 = 0;
	Bit8u reg_45 = 0; // Hardware graphics cursor
	Bit8u reg_50 = 0;
	Bit8u reg_51 = 0;
	Bit8u reg_52 = 0;
	Bit8u reg_55 = 0;
	Bit8u reg_58 = 0;
	Bit8u reg_6b = 0; // LFB BIOS scratchpad
	Bit8u ex_hor_overflow = 0;
	Bit8u ex_ver_overflow = 0;
	Bit16u la_window = 0;
	Bit8u misc_control_2 = 0;
	Bit8u ext_mem_ctrl = 0;
	Bitu xga_screen_width = 0;
	VGAModes xga_color_mode = {};
	struct clk_t {
		uint8_t r = 0;
		uint8_t n = 1;
		uint8_t m = 1;
	} clk[4], mclk;
	struct pll_t {
		Bit8u lock = 0;
		Bit8u cmd = 0;
	} pll;
	VGA_HWCURSOR hgc = {};
};

struct VGA_HERC {
	Bit8u mode_control = 0;
	Bit8u enable_bits = 0;
};

struct VGA_OTHER {
	Bit8u index = 0;
	Bit8u htotal = 0;
	Bit8u hdend = 0;
	Bit8u hsyncp = 0;
	Bit8u hsyncw = 0;
	Bit8u vtotal = 0;
	Bit8u vdend = 0;
	Bit8u vadjust = 0;
	Bit8u vsyncp = 0;
	Bit8u vsyncw = 0;
	Bit8u max_scanline = 0;
	Bit16u lightpen = 0;
	bool lightpen_triggered = false;
	Bit8u cursor_start = 0;
	Bit8u cursor_end = 0;
};

struct VGA_TANDY {
	Bit8u pcjr_flipflop = 0;
	Bit8u mode_control = 0;
	Bit8u color_select = 0;
	Bit8u disp_bank = 0;
	Bit8u reg_index = 0;
	Bit8u gfx_control = 0;
	Bit8u palette_mask = 0;
	Bit8u extended_ram = 0;
	Bit8u border_color = 0;
	Bit8u line_mask = 0;
	uint8_t line_shift = 0;
	Bit8u draw_bank = 0;
	uint8_t mem_bank = 0;
	Bit8u *draw_base = nullptr;
	uint8_t *mem_base = nullptr;
	Bitu addr_mask = 0;
};

struct VGA_Seq {
	Bit8u index = 0;
	Bit8u reset = 0;
	Bit8u clocking_mode = 0;
	Bit8u map_mask = 0;
	Bit8u character_map_select = 0;
	Bit8u memory_mode = 0;
};

struct VGA_Attr {
	Bit8u palette[16] = {};
	Bit8u mode_control = 0;
	Bit8u horizontal_pel_panning = 0;
	Bit8u overscan_color = 0;
	Bit8u color_plane_enable = 0;
	Bit8u color_select = 0;
	Bit8u index = 0;
	Bit8u disabled = 0; // Used for disabling the screen.
	                    // Bit0: screen disabled by attribute controller
	                    // index Bit1: screen disabled by sequencer index 1
	                    // bit 5 These are put together in one variable for
	                    // performance reasons: the line drawing function is
	                    // called maybe 60*480=28800 times/s, and we only
	                    // need to check one variable for zero this way.
};

struct VGA_Crtc {
	Bit8u horizontal_total = 0;
	Bit8u horizontal_display_end = 0;
	Bit8u start_horizontal_blanking = 0;
	Bit8u end_horizontal_blanking = 0;
	Bit8u start_horizontal_retrace = 0;
	Bit8u end_horizontal_retrace = 0;
	Bit8u vertical_total = 0;
	Bit8u overflow = 0;
	Bit8u preset_row_scan = 0;
	Bit8u maximum_scan_line = 0;
	Bit8u cursor_start = 0;
	Bit8u cursor_end = 0;
	Bit8u start_address_high = 0;
	Bit8u start_address_low = 0;
	Bit8u cursor_location_high = 0;
	Bit8u cursor_location_low = 0;
	Bit8u vertical_retrace_start = 0;
	Bit8u vertical_retrace_end = 0;
	Bit8u vertical_display_end = 0;
	Bit8u offset = 0;
	Bit8u underline_location = 0;
	Bit8u start_vertical_blanking = 0;
	Bit8u end_vertical_blanking = 0;
	Bit8u mode_control = 0;
	Bit8u line_compare = 0;

	Bit8u index = 0;
	bool read_only = false;
};

struct VGA_Gfx {
	Bit8u index = 0;
	Bit8u set_reset = 0;
	Bit8u enable_set_reset = 0;
	Bit8u color_compare = 0;
	Bit8u data_rotate = 0;
	Bit8u read_map_select = 0;
	Bit8u mode = 0;
	Bit8u miscellaneous = 0;
	Bit8u color_dont_care = 0;
	Bit8u bit_mask = 0;
};

struct RGBEntry {
	Bit8u red = 0;
	Bit8u green = 0;
	Bit8u blue = 0;
};

struct VGA_Dac {
	Bit8u bits = 0; /* DAC bits, usually 6 or 8 */
	Bit8u pel_mask = 0;
	Bit8u pel_index = 0;
	Bit8u state = 0;
	Bit8u write_index = 0;
	Bit8u read_index = 0;
	Bitu first_changed = 0;
	Bit8u combine[16] = {};
	RGBEntry rgb[0x100] = {};
	Bit16u xlat16[256] = {};
};

struct VGA_SVGA {
	Bitu readStart = 0, writeStart = 0;
	Bitu bankMask = 0;
	Bitu bank_read_full = 0;
	Bitu bank_write_full = 0;
	Bit8u bank_read = 0;
	Bit8u bank_write = 0;
	Bitu bank_size = 0;
};

union VGA_Latch {
	Bit32u d = 0;
	Bit8u b[4];
};

struct VGA_Memory {
	Bit8u *linear = nullptr;
	Bit8u *linear_orgptr = nullptr;
};

struct VGA_Changes {
	//Add a few more just to be safe
	Bit8u *map = nullptr; /* allocated dynamically: [(VGA_MEMORY >> VGA_CHANGE_SHIFT) + 32] */
	Bit8u checkMask = 0;
	uint8_t frame = 0;
	uint8_t writeMask = 0;
	bool active = 0;
	Bit32u clearMask = 0;
	Bit32u start = 0, last = 0;
	Bit32u lastAddress = 0;
};

struct VGA_LFB {
	Bit32u page = 0;
	Bit32u addr = 0;
	Bit32u mask = 0;
	PageHandler *handler = nullptr;
};

struct VGA_Type {
	VGAModes mode = {}; /* The mode the vga system is in */
	Bit8u misc_output = 0;
	VGA_Draw draw = {};
	VGA_Config config = {};
	VGA_Internal internal = {};
	/* Internal module groups */
	VGA_Seq seq = {};
	VGA_Attr attr = {};
	VGA_Crtc crtc = {};
	VGA_Gfx gfx = {};
	VGA_Dac dac = {};
	VGA_Latch latch = {};
	VGA_S3 s3 = {};
	VGA_SVGA svga = {};
	VGA_HERC herc = {};
	VGA_TANDY tandy = {};
	VGA_OTHER other = {};
	VGA_Memory mem = {};
	Bit32u vmemwrap = 0;      /* this is assumed to be power of 2 */
	Bit8u *fastmem = nullptr; /* memory for fast (usually 16-color)
	                             rendering, always twice as big as vmemsize */
	Bit8u *fastmem_orgptr = nullptr;
	Bit32u vmemsize = 0;
#ifdef VGA_KEEP_CHANGES
	VGA_Changes changes = {};
#endif
	VGA_LFB lfb = {};
	// Composite video mode parameters
	int ri = 0, rq = 0, gi = 0, gq = 0, bi = 0, bq = 0;
	int sharpness = 0;
};

/* Hercules Palette function */
void Herc_Palette(void);

/* CGA Mono Palette function */
void Mono_CGA_Palette(void);

void VGA_SetMonoPalette(const char *colour);

/* Functions for different resolutions */
void VGA_SetMode(VGAModes mode);
void VGA_DetermineMode(void);
void VGA_SetupHandlers(void);
void VGA_StartResize(Bitu delay=50);
void VGA_SetupDrawing(uint32_t val);
void VGA_CheckScanLength(void);
void VGA_ChangedBank(void);

/* Some DAC/Attribute functions */
void VGA_DAC_CombineColor(Bit8u attr,Bit8u pal);
void VGA_DAC_SetEntry(Bitu entry,Bit8u red,Bit8u green,Bit8u blue);
void VGA_ATTR_SetPalette(Bit8u index,Bit8u val);

enum EGAMonitorMode { CGA, EGA, MONO };

void VGA_ATTR_SetEGAMonitorPalette(EGAMonitorMode m);

/* The VGA Subfunction startups */
void VGA_SetupAttr(void);
void VGA_SetupMemory(Section* sec);
void VGA_SetupDAC(void);
void VGA_SetupCRTC(void);
void VGA_SetupMisc(void);
void VGA_SetupGFX(void);
void VGA_SetupSEQ(void);
void VGA_SetupOther(void);
void VGA_SetupXGA(void);
void VGA_AddCompositeSettings(Config &conf);

/* Some Support Functions */
void VGA_SetClock(Bitu which, uint32_t target);
void VGA_DACSetEntirePalette(void);
void VGA_StartRetrace(void);
void VGA_StartUpdateLFB(void);
void VGA_SetBlinking(uint8_t enabled);
void VGA_SetCGA2Table(Bit8u val0,Bit8u val1);
void VGA_SetCGA4Table(Bit8u val0,Bit8u val1,Bit8u val2,Bit8u val3);
void VGA_ActivateHardwareCursor(void);
void VGA_KillDrawing(void);

void VGA_SetOverride(bool vga_override);
void VGA_LogInitialization(const char* adapter_name, const char* ram_type);

extern VGA_Type vga;

/* Support for modular SVGA implementation */
/* Video mode extra data to be passed to FinishSetMode_SVGA().
   This structure will be in flux until all drivers (including S3)
   are properly separated. Right now it contains only three overflow
   fields in S3 format and relies on drivers re-interpreting those.
   For reference:
   ver_overflow:X|line_comp10|X|vretrace10|X|vbstart10|vdispend10|vtotal10
   hor_overflow:X|X|X|hretrace8|X|hblank8|hdispend8|htotal8
   offset is not currently used by drivers (useful only for S3 itself)
   It also contains basic int10 mode data - number, vtotal, htotal
   */
struct VGA_ModeExtraData {
	Bit8u ver_overflow = 0;
	Bit8u hor_overflow = 0;
	Bitu offset = 0;
	Bitu modeNo = 0;
	uint32_t htotal = 0;
	uint32_t vtotal = 0;
};

// Vector function prototypes
typedef void (*tWritePort)(io_port_t reg, uint8_t val, io_width_t width);
typedef uint8_t (*tReadPort)(io_port_t reg, io_width_t width);
typedef void (*tFinishSetMode)(io_port_t crtc_base, VGA_ModeExtraData *modeData);
typedef void (*tDetermineMode)();
typedef void (*tSetClock)(Bitu which, uint32_t target);
typedef uint32_t (*tGetClock)();
typedef bool (*tHWCursorActive)();
typedef bool (*tAcceptsMode)(Bitu modeNo);

struct SVGA_Driver {
	tWritePort write_p3d5;
	tReadPort read_p3d5;
	tWritePort write_p3c5;
	tReadPort read_p3c5;
	tWritePort write_p3c0;
	tReadPort read_p3c1;
	tWritePort write_p3cf;
	tReadPort read_p3cf;

	tFinishSetMode set_video_mode;
	tDetermineMode determine_mode;
	tSetClock set_clock;
	tGetClock get_clock;
	tHWCursorActive hardware_cursor_active;
	tAcceptsMode accepts_mode;
};

extern SVGA_Driver svga;

void SVGA_Setup_S3Trio(void);
void SVGA_Setup_TsengET4K(void);
void SVGA_Setup_TsengET3K(void);
void SVGA_Setup_ParadisePVGA1A(void);
void SVGA_Setup_Driver(void);

// Amount of video memory required for a mode, implemented in int10_modes.cpp
uint32_t VideoModeMemSize(uint16_t mode);

extern Bit32u ExpandTable[256];
extern Bit32u FillTable[16];
extern Bit32u CGA_2_Table[16];
extern Bit32u CGA_4_Table[256];
extern Bit32u CGA_4_HiRes_Table[256];
extern Bit32u CGA_16_Table[256];
extern int CGA_Composite_Table[1024];
extern Bit32u TXT_Font_Table[16];
extern Bit32u TXT_FG_Table[16];
extern Bit32u TXT_BG_Table[16];
extern Bit32u Expand16Table[4][16];
extern Bit32u Expand16BigTable[0x10000];

#endif
