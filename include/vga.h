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

#include <utility>

#include "bit_view.h"
#include "control.h"
#include "fraction.h"
#include "inout.h"

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

constexpr auto M_TEXT_MODES = M_TEXT | M_HERC_TEXT | M_TANDY_TEXT | M_CGA_TEXT_COMPOSITE;

constexpr auto vesa_2_0_modes_start = 0x120;

constexpr uint16_t EGA_HALF_CLOCK = 1 << 0;
constexpr uint16_t EGA_LINE_DOUBLE = 1 << 1;
constexpr uint16_t VGA_PIXEL_DOUBLE = 1 << 2;

// Refresh rate constants
constexpr auto RefreshRateMin            = 23;
constexpr auto RefreshRateHostVrrLfc     = 48;
constexpr auto RefreshRateHostDefault    = 60;
constexpr auto RefreshRateDosDefault     = 70;
constexpr auto InterpolatingVrrMinRateHz = 140;
constexpr auto RefreshRateMax            = 1000;

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
	uint8_t pel_panning = 0; /* Amount of pixels to skip when starting
	                          horizontal line */
	uint8_t hlines_skip = 0;
	uint8_t bytes_skip = 0;
	uint8_t addr_shift = 0;

	/* Specific stuff memory write/read handling */

	uint8_t read_mode = 0;
	uint8_t write_mode = 0;
	uint8_t read_map_select = 0;
	uint8_t color_dont_care = 0;
	uint8_t color_compare = 0;
	uint8_t data_rotate = 0;
	uint8_t raster_op = 0;

	uint32_t full_bit_mask = 0;
	uint32_t full_map_mask = 0;
	uint32_t full_not_map_mask = 0;
	uint32_t full_set_reset = 0;
	uint32_t full_not_enable_set_reset = 0;
	uint32_t full_enable_set_reset = 0;
	uint32_t full_enable_and_set_reset = 0;
};

enum Drawmode { PART, DRAWLINE, EGALINE };

enum class VgaRateMode { Default, Host, Custom };

enum PixelsPerChar : int8_t {
	Eight = 8,
	Nine  = 9,
};

enum class VgaSub350LineHandling : int8_t {
	DoubleScan,
	SingleScan,
	ForceSingleScan,
};

struct VGA_Draw {
	bool resizing = false;
	Bitu width = 0;
	Bitu height = 0;
	uint32_t blocks = 0;
	Bitu address = 0;
	uint16_t panning = 0;
	Bitu bytes_skip = 0;
	uint8_t *linear_base = nullptr;
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

	// when drawing in parts, how many many 'chunks' should we draw at a
	// time? a value of 1 is the entire frame where as a value of 2 will
	// draw the top then the bottom, 4 will draw in quarters, and so on.
	int parts_total = 0;

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
		double per_line_ms = 0;
	} delay = {};

	// clang-format off
	//
	// Non-paletted RGB image data is stored as a continuous stream of BGR
	// pixel values in memory.
	//
	// Valid values are the following:
	//
	//  8 - Indexed8   up to 256-colour, paletted;
	//                 stored as packed uint8 data
	//
	// 15 - BGR555     32k hi-colour, 5 bits per red/blue/green component;
	//                 stored as packed uint16 data with highest bit unused
	//
	// 16 - BGR565     64k hi-colour, 5 bits for red/blue, 6 bit for green;
	//                 stored as packed uint16 data
	//
	// 24 - BGR888     16M (24-bit) true-colour, 8 bits per red/blue/green component;
	//                 stored as packed 24-bit data
	//
	// 32 - BGRX8888   24-bit true-colour; 8 bits per red/blue/green component;
	//                 stored as packed uint32 data with highest 8 bits unused
	//
	// clang-format on
	Bitu bpp = 0;

	double host_refresh_hz = RefreshRateHostDefault;
	double dos_refresh_hz = RefreshRateDosDefault;
	double custom_refresh_hz = RefreshRateDosDefault;
	VgaRateMode dos_rate_mode = VgaRateMode::Default;
	Fraction pixel_aspect_ratio = {};
	bool double_scan = false;
	bool doublewidth = false;
	bool doubleheight = false;
	VgaSub350LineHandling vga_sub_350_line_handling = {};
	uint8_t font[64 * 1024] = {};
	uint8_t *font_tables[2] = {nullptr, nullptr};
	Bitu blinking = 0;
	bool blink = false;
	PixelsPerChar pixels_per_character = PixelsPerChar::Eight;
	struct {
		Bitu address = 0;
		uint8_t sline = 0;
		uint8_t eline = 0;
		uint8_t count = 0;
		uint8_t delay = 0;
		uint8_t enabled = 0;
	} cursor = {};
	Drawmode mode = {};
	bool vret_triggered = false;
	bool vga_override = false;
};

struct VGA_HWCURSOR {
	uint8_t curmode = 0;
	uint16_t originx = 0;
	uint16_t originy = 0;
	uint8_t fstackpos = 0;
	uint8_t bstackpos = 0;
	uint8_t forestack[4] = {};
	uint8_t backstack[4] = {};
	uint16_t startaddr = 0;
	uint8_t posx = 0;
	uint8_t posy = 0;
	uint8_t mc[64][64] = {};
};

struct VGA_S3 {
	uint8_t reg_lock1 = 0;
	uint8_t reg_lock2 = 0;
	uint8_t reg_31 = 0;
	uint8_t reg_35 = 0;
	uint8_t reg_36 = 0; // RAM size
	uint8_t reg_3a = 0; // 4/8/doublepixel bit in there
	uint8_t reg_40 = 0; // 8415/A functionality register
	uint8_t reg_41 = 0; // BIOS flags
	uint8_t reg_42 = 0;
	uint8_t reg_43 = 0;
	uint8_t reg_45 = 0; // Hardware graphics cursor
	uint8_t reg_50 = 0;
	uint8_t reg_51 = 0;
	uint8_t reg_52 = 0;
	uint8_t reg_55 = 0;
	uint8_t reg_58 = 0;
	uint8_t reg_6b = 0; // LFB BIOS scratchpad
	uint8_t ex_hor_overflow = 0;
	uint8_t ex_ver_overflow = 0;
	uint16_t la_window = 0;
	uint8_t misc_control_2 = 0;
	uint8_t ext_mem_ctrl = 0;
	uint16_t xga_screen_width = 0; // from 640 to 1600
	VGAModes xga_color_mode = {};
	struct clk_t {
		uint8_t r = 0;
		uint8_t n = 1;
		uint8_t m = 1;
	};
	clk_t clk[4] = {};
	clk_t mclk = {};
	struct pll_t {
		uint8_t lock = 0; // Extended Sequencer Access Rgister SR8 (pp. 124)
		uint8_t control_2 = 0; // CLKSYN Control 2 Register SR15 (pp. 130)
		uint8_t control = 0; // RAMDAC/CLKSYN Control Register SRI8 (pp. 132)
	};
	pll_t pll = {};
	VGA_HWCURSOR hgc = {};
};

struct VGA_HERC {
	uint8_t mode_control = 0;
	uint8_t enable_bits = 0;
};

struct VGA_OTHER {
	uint8_t index = 0;
	uint8_t htotal = 0;
	uint8_t hdend = 0;
	uint8_t hsyncp = 0;
	uint8_t hsyncw = 0;
	uint8_t vtotal = 0;
	uint8_t vdend = 0;
	uint8_t vadjust = 0;
	uint8_t vsyncp = 0;
	uint8_t vsyncw = 0;
	uint8_t max_scanline = 0;
	uint16_t lightpen = 0;
	bool lightpen_triggered = false;
	uint8_t cursor_start = 0;
	uint8_t cursor_end = 0;
};

struct VGA_TANDY {
	uint8_t pcjr_flipflop = 0;
	uint8_t mode_control = 0;
	uint8_t color_select = 0;
	uint8_t disp_bank = 0;
	uint8_t reg_index = 0;
	uint8_t gfx_control = 0;
	uint8_t palette_mask = 0;
	uint8_t extended_ram = 0;
	uint8_t border_color = 0;
	uint8_t line_mask = 0;
	uint8_t line_shift = 0;
	uint8_t draw_bank = 0;
	uint8_t mem_bank = 0;
	uint8_t *draw_base = nullptr;
	uint8_t *mem_base = nullptr;
	Bitu addr_mask = 0;
};

// VGA sequence - 8-bit clocking mode register (Index 01h)
// Ref: http://www.osdever.net/FreeVGA/vga/seqreg.htm
union ClockingModeRegister {
	uint8_t data = 0;
	// Characters are drawn 8 pixels wide (or 9 if cleared)
	bit_view<0, 1> is_eight_dot_mode;

	// When this bit and bit 4 are set to 0, the video serializers are
	// loaded every character clock. When this bit is set to 1, the video
	// serializers are loaded every other character clock, which is useful
	// when 16 bits are fetched per cycle and chained together in the shift
	// registers. The Type 2 video behaves as if this bit is set to 0;
	// therefore, programs should set it to 0.
	bit_view<2, 1> is_loading_alternating_characters;

	// When set to 0, this bit selects the normal dot clocks derived from
	// the sequencer master clock input. When this bit is set to 1, the
	// master clock will be divided by 2 to generate the dot clock. All
	// other timings are affected because they are derived from the dot
	// clock. The dot clock divided by 2 is used for 320 and 360 horizontal
	// PEL modes.
	bit_view<3, 1> is_pixel_doubling;

	//	When the Shift 4 field and the Shift Load Field are set to 0,
	//the video serializers are loaded every character clock. When the Shift
	//4 field is set to 1, the video serializers are loaded every forth
	//character clock, which is useful when 32 bits are fetched per cycle
	//and chained together in the shift registers
	bit_view<4, 1> is_shift_4_enabled;

	// When set to 1, this bit turns off the display and assigns maximum
	// memory bandwidth to the system. Although the display is blanked, the
	// synchronization pulses are maintained. This bit can be used for rapid
	// full-screen updates.
	bit_view<5, 1> is_screen_disabled;
};

struct VGA_Seq {
	uint8_t index = 0;
	uint8_t reset = 0;

	ClockingModeRegister clocking_mode = {};
	// Let the user force the clocking mode's 8/9-dot-mode bit high
	bool wants_vga_8dot_font = false;

	uint8_t map_mask = 0;
	uint8_t character_map_select = 0;
	uint8_t memory_mode = 0;
};

struct VGA_Attr {
	uint8_t palette[16] = {};
	uint8_t mode_control = 0;
	uint8_t horizontal_pel_panning = 0;
	uint8_t overscan_color = 0;
	uint8_t color_plane_enable = 0;
	uint8_t color_select = 0;
	uint8_t index = 0;
	uint8_t disabled = 0; // Used for disabling the screen.
	                    // Bit0: screen disabled by attribute controller
	                    // index Bit1: screen disabled by sequencer index 1
	                    // bit 5 These are put together in one variable for
	                    // performance reasons: the line drawing function is
	                    // called maybe 60*480=28800 times/s, and we only
	                    // need to check one variable for zero this way.
};

struct VGA_Crtc {
	uint8_t horizontal_total = 0;
	uint8_t horizontal_display_end = 0;
	uint8_t start_horizontal_blanking = 0;
	uint8_t end_horizontal_blanking = 0;
	uint8_t start_horizontal_retrace = 0;
	uint8_t end_horizontal_retrace = 0;
	uint8_t vertical_total = 0;
	uint8_t overflow = 0;
	uint8_t preset_row_scan = 0;
	uint8_t maximum_scan_line = 0;
	uint8_t cursor_start = 0;
	uint8_t cursor_end = 0;
	uint8_t start_address_high = 0;
	uint8_t start_address_low = 0;
	uint8_t cursor_location_high = 0;
	uint8_t cursor_location_low = 0;
	uint8_t vertical_retrace_start = 0;
	uint8_t vertical_retrace_end = 0;
	uint8_t vertical_display_end = 0;
	uint8_t offset = 0;
	uint8_t underline_location = 0;
	uint8_t start_vertical_blanking = 0;
	uint8_t end_vertical_blanking = 0;
	uint8_t mode_control = 0;
	uint8_t line_compare = 0;

	uint8_t index = 0;
	bool read_only = false;
};

struct VGA_Gfx {
	uint8_t index = 0;
	uint8_t set_reset = 0;
	uint8_t enable_set_reset = 0;
	uint8_t color_compare = 0;
	uint8_t data_rotate = 0;
	uint8_t read_map_select = 0;
	uint8_t mode = 0;
	uint8_t miscellaneous = 0;
	uint8_t color_dont_care = 0;
	uint8_t bit_mask = 0;
};

struct RGBEntry {
	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;
};

constexpr auto num_cga_colors = 16;
typedef std::array<RGBEntry, num_cga_colors> cga_colors_t;

struct VGA_Dac {
	RGBEntry rgb[0x100]       = {};
	uint32_t palette_map[256] = {};

	uint8_t combine[16] = {};

	// DAC 8-bit registers
	uint8_t bits        = 0; /* DAC bits, usually 6 or 8 */
	uint8_t pel_mask    = 0;
	uint8_t pel_index   = 0;
	uint8_t state       = 0;
	uint8_t write_index = 0;
	uint8_t read_index  = 0;
};

struct VGA_SVGA {
	Bitu readStart = 0, writeStart = 0;
	Bitu bankMask = 0;
	Bitu bank_read_full = 0;
	Bitu bank_write_full = 0;
	uint8_t bank_read = 0;
	uint8_t bank_write = 0;
	Bitu bank_size = 0;
};

union VGA_Latch {
	uint32_t d = 0;
	uint8_t b[4];
};

struct VGA_Memory {
	uint8_t* linear = {};
};

struct VGA_Changes {
	//Add a few more just to be safe
	uint8_t *map = nullptr; /* allocated dynamically: [(VGA_MEMORY >> VGA_CHANGE_SHIFT) + 32] */
	uint8_t checkMask = 0;
	uint8_t frame = 0;
	uint8_t writeMask = 0;
	bool active = 0;
	uint32_t clearMask = 0;
	uint32_t start = 0, last = 0;
	uint32_t lastAddress = 0;
};

struct VGA_LFB {
	uint32_t page = 0;
	uint32_t addr = 0;
	uint32_t mask = 0;
	PageHandler *handler = nullptr;
};

struct VGA_Type {
	VGAModes mode = {}; /* The mode the vga system is in */
	uint8_t misc_output = 0;
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
	// this is assumed to be power of 2
	uint32_t vmemwrap = 0;
	 // memory for fast (usually 16-color) rendering, 
	 // always twice as big as vmemsize
	uint8_t* fastmem  = {};
	uint32_t vmemsize = 0;
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

void VGA_StartResize();
void VGA_StartResizeAfter(const uint16_t delay_ms);

void VGA_SetupDrawing(uint32_t val);
void VGA_CheckScanLength(void);
void VGA_ChangedBank(void);

/* Some DAC/Attribute functions */
void VGA_DAC_CombineColor(uint8_t attr,uint8_t pal);
void VGA_DAC_SetEntry(Bitu entry,uint8_t red,uint8_t green,uint8_t blue);
void VGA_ATTR_SetPalette(uint8_t index,uint8_t val);

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

/* Some support functions */

// Get the current video mode's type and numeric ID
std::pair<VGAModes, uint16_t> VGA_GetCurrentMode();

// Describes the given video mode's type and ID, ie: "VGA, "256 color"
std::pair<const char*, const char*> VGA_DescribeMode(const VGAModes video_mode_type,
                                                     const uint16_t video_mode_id,
                                                     const uint16_t width,
                                                     const uint16_t height);

void VGA_SetClock(Bitu which, uint32_t target);

// Save, get, and limit refresh and clock functions
void VGA_SetHostRate(const double refresh_hz);
void VGA_SetRatePreference(const std::string &pref);
double VGA_GetPreferredRate();

void VGA_DACSetEntirePalette(void);
void VGA_StartRetrace(void);
void VGA_StartUpdateLFB(void);
void VGA_SetBlinking(uint8_t enabled);
void VGA_SetCGA2Table(uint8_t val0,uint8_t val1);
void VGA_SetCGA4Table(uint8_t val0,uint8_t val1,uint8_t val2,uint8_t val3);
uint8_t VGA_ActivateHardwareCursor();
void VGA_KillDrawing(void);

void VGA_SetOverride(bool vga_override);
void VGA_LogInitialization(const char *adapter_name,
                           const char *ram_type,
                           const size_t num_modes);

void VGA_SetVgaSub350LineHandling(const VgaSub350LineHandling vga_sub_350_line_handling);
bool VGA_IsDoubleScanningSub350LineModes();

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
	uint8_t ver_overflow = 0;
	uint8_t hor_overflow = 0;
	Bitu offset = 0;
	Bitu modeNo = 0;
	uint32_t htotal = 0;
	uint32_t vtotal = 0;
};

// Vector function prototypes
typedef void (*tWritePort)(io_port_t reg, io_val_t value, io_width_t width);
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

extern uint32_t ExpandTable[256];
extern uint32_t FillTable[16];
extern uint32_t CGA_2_Table[16];
extern uint32_t CGA_4_Table[256];
extern uint32_t CGA_4_HiRes_Table[256];
extern uint32_t CGA_16_Table[256];
extern int CGA_Composite_Table[1024];
extern uint32_t TXT_Font_Table[16];
extern uint32_t TXT_FG_Table[16];
extern uint32_t TXT_BG_Table[16];
extern uint32_t Expand16Table[4][16];
extern uint32_t Expand16BigTable[0x10000];

#endif
