// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_VGA_H
#define DOSBOX_VGA_H

#include "dosbox.h"

#include <string>
#include <utility>

#include "bgrx8888.h"
#include "bit_view.h"
#include "control.h"
#include "fraction.h"
#include "inout.h"
#include "rgb666.h"
#include "video.h"

//Don't enable keeping changes and mapping lfb probably...
#define VGA_LFB_MAPPED
//#define VGA_KEEP_CHANGES
#define VGA_CHANGE_SHIFT	9

class PageHandler;

// These tags are assigned to video modes primarily based on their memory
// organisation, *not* the name of the graphics adapter that first introduced
// them. That's why for example all planar 16-colour modes get the M_EGA tag,
// including the 640x480 16-colour VGA mode, and M_VGA is only used for the
// "chunky" ("chained") 320x200 256-colour 13H VGA mode and its many tweaked
// "Mode X" variants, while all other 256-colour SVGA/VESA modes get the
// M_LIN8 tag.
//
enum VGAModes {
	// 640x200 monochrome CGA mode on EGA & VGA
	M_CGA2 = 1 << 0,

	// 320x200 4-colour CGA mode on EGA & VGA
	M_CGA4 = 1 << 1,

	// 640x480 monochrome EGA mode
	// All 16-colour EGA and VGA modes
	M_EGA = 1 << 2,

	// 320x200 256-colour "chunky" or "chained" VGA mode (mode 13h)
	// Also its numerous tweaked "Mode X" variants (e.g. 320x240, 360x240,
	// 320x400, 256x256, etc.) that use mode 13h as a starting point.
	M_VGA = 1 << 3,

	// 16-colour planar SVGA & VESA modes
	M_LIN4 = 1 << 4,

	// 256-colour planar SVGA & VESA modes (other than mode 13h)
	M_LIN8 = 1 << 5,

	// 15-bit (5:5:5) high colour (32K-colour) VESA modes
	M_LIN15 = 1 << 6,

	// 16-bit (5:6:5) high colour (65K-colour) VESA modes
	M_LIN16 = 1 << 7,

	// 24-bit (8:8:8) true colour (16.7M-colour) VESA modes
	M_LIN24 = 1 << 8,

	// 32-bit (8:8:8:8) true colour (16.7M-colour) VESA modes
	//
	// Same as 24-bit (8:8:8) as the last 8-bit component is simply unused.
	// Many graphics cards preferred the 32-bit true colour mode for faster
	// video memory access due to 32-bit memory alignment.
	M_LIN32 = 1 << 9,

	// All EGA, VGA, SVGA & VESA text modes
	M_TEXT = 1 << 10,

	// Hercules graphics mode
	M_HERC_GFX = 1 << 11,

	// Hercules text mode
	M_HERC_TEXT = 1 << 12,

	// 640x200 monochrome CGA mode on CGA, Tandy & PCjr
	M_TANDY2 = 1 << 13,

	// 320x200 4-colour CGA mode on CGA, Tandy & PCjr
	// 640x200 4-colour mode on Tandy & PCjr
	M_TANDY4 = 1 << 14,

	// 160x200 and 320x200 16-colour modes on Tandy & PCjr
	M_TANDY16 = 1 << 15,

	// CGA, Tandy & PCjr text modes
	M_TANDY_TEXT = 1 << 16,

	// Composite output in 320x200 4-colour CGA mode on PCjr only
	M_CGA16 = 1 << 17,

	// Composite output in 640x200 monochrome CGA mode on CGA, Tandy & PCjr
	M_CGA2_COMPOSITE = 1 << 18,

	// Composite output in 320x200 & 640x200 4-colour modes on CGA & Tandy
	M_CGA4_COMPOSITE = 1 << 19,

	// Composite output in text modes on CGA, Tandy & PCjr
	M_CGA_TEXT_COMPOSITE = 1 << 20,

	M_ERROR = 1 << 31,
};

constexpr auto NumCgaColors = 16;
constexpr auto NumVgaColors = 256;

constexpr auto NumVgaSequencerRegisters = 0x05;
constexpr auto NumVgaGraphicsRegisters  = 0x09;
constexpr auto NumVgaAttributeRegisters = 0x15;

constexpr auto vesa_2_0_modes_start = 0x120;

constexpr uint16_t EGA_HALF_CLOCK = 1 << 0;
constexpr uint16_t EGA_LINE_DOUBLE = 1 << 1;
constexpr uint16_t VGA_PIXEL_DOUBLE = 1 << 2;

// Refresh rate constants
constexpr auto RefreshRateMin        = 23;
constexpr auto RefreshRateDosDefault = 70;
constexpr auto RefreshRateMax        = 1000;

// Ref: https://en.wikipedia.org/wiki/Crystal_oscillator_frequencies
//
// CGA pixel clock (4x the NTSC colour-burst frequency of 3.579545 MHz)
constexpr auto CgaPixelClockHz = 14318180;

// MDA & EGA 640x350 @ 60Hz pixel clock
constexpr auto EgaPixelClockHz = 16257000;

// VGA pixel clock for 640 pixel wide modes
// (e.g., 640x480 @ 60Hz and 320/640x200/350/400 @ 70Hz)
constexpr auto Vga640PixelClockHz = 25175000;

// VGA pixel clock for 720 pixel wide modes (e.g., 720x350/400 @ 70Hz)
constexpr auto Vga720PixelClockHz = 28322000;

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

struct VgaConfig {
	// Memory handlers
	Bitu mh_mask = 0;

	// Video drawing
	uint32_t display_start = 0;
	Bitu real_start        = 0;
	bool retrace           = false; /* A retrace is active */
	Bitu scan_len          = 0;
	Bitu cursor_start      = 0;

	// Some other screen related variables
	Bitu line_compare      = 0;

	// Enable or Disabled Chain 4 Mode
	bool chained           = false;
	bool compatible_chain4 = false;

	// Pixel Scrolling
	// Amount of pixels to skip when starting horizontal line
	uint8_t pel_panning = 0;
	uint8_t hlines_skip = 0;
	uint8_t bytes_skip  = 0;
	uint8_t addr_shift  = 0;

	// Specific stuff memory write/read handling
	uint8_t read_mode       = 0;
	uint8_t write_mode      = 0;
	uint8_t read_map_select = 0;
	uint8_t color_dont_care = 0;
	uint8_t color_compare   = 0;
	uint8_t data_rotate     = 0;
	uint8_t raster_op       = 0;

	uint32_t full_bit_mask             = 0;
	uint32_t full_map_mask             = 0;
	uint32_t full_not_map_mask         = 0;
	uint32_t full_set_reset            = 0;
	uint32_t full_not_enable_set_reset = 0;
	uint32_t full_enable_set_reset     = 0;
	uint32_t full_enable_and_set_reset = 0;
};

enum Drawmode { PART, DRAWLINE, EGALINE };

enum class VgaRateMode { Default, Custom };

enum PixelsPerChar : int8_t {
	Eight = 8,
	Nine  = 9,
};

enum class PixelFormat : uint8_t;

struct VgaDraw {
	bool resizing = false;

	ImageInfo image_info = {};

	uint32_t blocks             = 0;
	Bitu address                = 0;
	uint16_t panning            = 0;
	Bitu bytes_skip             = 0;
	uint8_t* linear_base        = nullptr;
	Bitu linear_mask            = 0;
	Bitu address_add            = 0;
	uint32_t line_length        = 0;
	uint32_t address_line_total = 0;
	Bitu address_line           = 0;
	uint32_t lines_total        = 0;
	Bitu vblank_skip            = 0;
	uint32_t lines_done         = 0;
	Bitu lines_scaled           = 0;
	Bitu split_line             = 0;

	bool is_double_scanning = false;

	// When drawing in parts, how many many 'chunks' should we draw at a
	// time? A value of 1 is the entire frame where as a value of 2 will
	// draw the top then the bottom, 4 will draw in quarters, and so on.
	int parts_total = 0;

	uint32_t parts_lines    = 0;
	uint32_t parts_left     = 0;
	Bitu byte_panning_shift = 0;

	struct {
		double framestart = 0;
		double vrstart = 0, vrend = 0;     // V-retrace
		double hrstart = 0, hrend = 0;     // H-retrace
		double hblkstart = 0, hblkend = 0; // H-blanking
		double vblkstart = 0, vblkend = 0; // V-Blanking
		double vdend = 0, vtotal = 0;
		double hdend = 0, htotal = 0;
		double parts       = 0;
		double per_line_ms = 0;
	} delay = {};

	double dos_refresh_hz = RefreshRateDosDefault;

	// The override rate corresponds to the override VGA mode where another
	// device can take over video output in place of the VGA card, such as
	// Voodoo.
	double override_refresh_hz = RefreshRateDosDefault;

	double custom_refresh_hz  = RefreshRateDosDefault;
	VgaRateMode dos_rate_mode = VgaRateMode::Default;

	// If true, double-scanned VGA modes are allowed to be drawn as
	// double-scanned. For example, the 13h 320x200 mode is drawn as 640x400
	// (assuming pixel doubling is also allowed).
	//
	// If false, double-scanned VGA modes are forced to be drawn as
	// single-scanned. In other words, video modes are drawn at their "nominal
	// height". E.g., the 13h 320x200 mode is drawn as 640x200 (assuming pixel
	// doubling is allowed). The exception to this are the special custom
	// VGA modes used in some demos that use odd number of scanline repeats
	// (e.g., 3 or 5); these are always drawn as scan-tripled, quintupled,
	// etc. even if this flag is false.
	//
	// Single scanning is forced by the arcade shaders to achieve the
	// single-scanned 15 kHz CRT look for double-scanned VGA modes, or by
	// shaders that treat pixels as flat adjacent rectangles (e.g., the
	// "sharp" shader and the "no bilinear" output modes; the double-scanned
	// and force single-scanned output is exactly identical in these cases,
	// but single scanning is more performant which matter on low-powered
	// devices).
	bool scan_doubling_allowed   = false;

	// If true, less than 640-pixel wide modes are allowed to be draw
	// pixel-doubled. Used in conjunction with bilinear interpolation or shaders,
	// this emulates the low dot pitch of PC monitors. For example, 320x200 is
	// drawn as 640x400 (assuming scan doubling is also enabled).
	//
	// If false, no pixel doubling is performed; the content is always drawn
	// at the "nomimal width" of the video mode.
	bool pixel_doubling_allowed  = false;

	uint8_t font[64 * 1024] = {};
	uint8_t* font_tables[2] = {nullptr, nullptr};

	Bitu blinking                      = 0;
	bool blink                         = false;
	PixelsPerChar pixels_per_character = PixelsPerChar::Eight;

	struct {
		Bitu address    = 0;
		uint8_t sline   = 0;
		uint8_t eline   = 0;
		uint8_t count   = 0;
		uint8_t delay   = 0;
		uint8_t enabled = 0;
	} cursor = {};

	Drawmode mode       = {};
	bool vret_triggered = false;
	bool vga_override   = false;
};

struct VGA_HWCURSOR {
	uint8_t curmode      = 0;
	uint16_t originx     = 0;
	uint16_t originy     = 0;
	uint8_t fstackpos    = 0;
	uint8_t bstackpos    = 0;
	uint8_t forestack[4] = {};
	uint8_t backstack[4] = {};
	uint16_t startaddr   = 0;
	uint8_t posx         = 0;
	uint8_t posy         = 0;
	uint8_t mc[64][64]   = {};
};

struct VgaS3 {
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
	uint8_t reg_63 = 0;
	uint8_t reg_6b = 0; // LFB BIOS scratchpad

	uint8_t ex_hor_overflow = 0;
	uint8_t ex_ver_overflow = 0;

	uint16_t la_window        = 0;
	uint8_t misc_control_2    = 0;
	uint8_t ext_mem_ctrl      = 0;
	uint16_t xga_screen_width = 0; // from 640 to 1600

	VGAModes xga_color_mode = {};

	struct clk_t {
		uint8_t r = 0;
		uint8_t n = 1;
		uint8_t m = 1;
	};

	clk_t clk[4] = {};
	clk_t mclk   = {};

	struct pll_t {
		// Extended Sequencer Access Register SR8 (pp. 124)
		uint8_t lock = 0;

		// CLKSYN Control 2 Register SR15 (pp. 130)
		uint8_t control_2 = 0;

		// RAMDAC/CLKSYN Control Register SRI8 (pp. 132)
		uint8_t control = 0;
	};

	pll_t pll        = {};
	VGA_HWCURSOR hgc = {};
};

struct VgaHerc {
	uint8_t mode_control = 0;
	uint8_t enable_bits  = 0;
};

struct VgaOther {
	uint8_t index = 0;

	uint8_t htotal  = 0;
	uint8_t hdend   = 0;
	uint8_t hsyncp  = 0;
	uint8_t hsyncw  = 0;
	uint8_t vtotal  = 0;
	uint8_t vdend   = 0;
	uint8_t vadjust = 0;
	uint8_t vsyncp  = 0;
	uint8_t vsyncw  = 0;

	uint8_t max_scanline = 0;

	uint16_t lightpen       = 0;
	bool lightpen_triggered = false;

	uint8_t cursor_start = 0;
	uint8_t cursor_end   = 0;
};

// clang-format off

// The Tandy and PCjr graphics registers are very similar with only a few
// differences, therefore DOSBox uses a unified structure to represent both.
// "Tandy" is preferred in the namings and is the default. The words "tandy"
// and "pcjr" are only present in a bitfield's name when the bits have
// different meanings on the two machines.
//
// The below table summarises the state of the two registers that control the
// selected video mode. Tandy values comes first, the PCjr values are only
// noted in parenthesis if they're different.
//
// MR  = Mode Register ("Mode Control Register 1" on the PCjr)
// MCR = Mode Control Register ("Mode Control Register 2" on the PCjr)
// ------------------------------------------------------------------------
// | Colours | Res     ||MR: b4 640|b2 bw|b1 gfx| b0 hi||MCR: b4  |b3     |
// |         |         ||   (16col)|     |      | bandw||   16col |640x200|
// |---------|---------||----------|-----|------|------||---------|-------|
// | 2       | 640x200 || 1 (0)    | 0   | 1    | 0    || 0 (-)   | 0 (1) |
// | 4-gray  | 320x200 || 0        | 1   | 1    | 0    || 0 (-)   | 0     |
// | 4       | 320x200 || 0        | 0   | 1    | 0    || 0 (-)   | 0     |
// | 4       | 640x200 || 1 (0)    | 0   | 1    | 1    || 0 (-)   | 1 (0) |
// | 16      | 160x200 || 0 (1)    | 0   | 1    | 0    || 1 (-)   | 0     |
// | 16      | 320x200 || 0 (1)    | 0   | 1    | 1    || 1 (-)   | 0     |
//
// References:
// -
// http://www.thealmightyguru.com/Wiki/images/3/3b/Tandy_1000_-_Manual_-_Technical_Reference.pdf:
// pg 58.
// -
// http://bitsavers.trailing-edge.com/pdf/ibm/pc/pc_jr/PCjr_Technical_Reference_Nov83.pdf:
// pg 2-59 to 2-69.

// clang-format on

// Tandy Mode Register (3D8h)
// (PCjr Mode Control 1 Register (address 00))
union TandyModeRegister {
	uint8_t data = 0;

	// 1 for 80 character text and high-bandwidth graphics (640x200 4-colour
	// and 320x200 16-colour)
	// 0 for 40 character text and all other graphics modes
	bit_view<0, 1> is_high_bandwidth;

	// 1 for graphics modes, 0 for text modes
	bit_view<1, 1> is_graphics_enabled;

	// 1 for black and white output
	// Tandy: A different colour palette is selected by this bit in 320x200 (cyan-red-white)
	// 4-color graphics mode (also on PCjr or only on Tandy?)
	bit_view<2, 1> is_black_and_white_mode;

	// 1 when the video signal is enabled. When the video signal is disabled,
	// the screen is forced to the border colour.
	bit_view<3, 1> is_video_enabled;

	// Tandy: 1 in 640x200 graphics modes
	// PCjr: 1 in all 16-colour graphics modes (160x200 and 320x200)
	bit_view<4, 1> is_tandy_640_dot_graphics;
	bit_view<4, 1> is_pcjr_16_color_graphics;

	// Tandy: This control bit is used in the alpha mode only.
	// 1 selects blinking if the attribute bit is set (bit 7).
	// 0 selects 16 background colours. (With blinking selected, only 8
	// background colours are available.)
	bit_view<5, 1> is_tandy_blink_enabled;
};

// Tandy Mode Control Register (address 03h)
// (PCjr Mode Control 2 Register (address 03h))
union TandyModeControlRegister {
	uint8_t data = 0;

	// If enabled in a text mode, the highest bit of the attribute byte
	// serves as the blink enabled flag.
	//
	// If the enable-blink bit is on in a graphics mode, the high-order
	// address of the palette (PA3) is replaced with the character-blink
	// rate. This causes displayed colours to switch between two sets of
	// colours.
	//
	// If the colours in the lower half of the palette are the same as in the
	// upper half of the palette, no colour changes will occur. If the colours
	// in the upper half of the palette are different from the lower half of
	// the palette, the colours will alternately change between the 2 palette
	// colours at the blink rate.
	//
	// Only eight colours are available in the 16-colour modes when using this
	// feature. Bit 3 of the palette mask has no effect on this mode.
	bit_view<1, 1> is_pcjr_blink_enabled;

	// Tandy: This bit enables the border colours register. For PC compatibility
	// this bit should be 0. For PCjr compatibility this bit should be 1.
	// (Interestingly, the PCjr manual states this bit should be always 0).
	bit_view<2, 1> is_tandy_border_enabled;

	// Tandy: 1 for the 640x200 4-colour graphics mode
	// PCjr: 1 in the 640x200 2-colour graphics mode only
	bit_view<3, 1> is_tandy_640x200_4_color_graphics;
	bit_view<3, 1> is_pcjr_640x200_2_color_graphics;

	// 1 for 16 colour modes, 0 for all other modes
	bit_view<4, 1> is_tandy_16_color_enabled;
};

struct VgaTandy {
	uint8_t pcjr_flipflop                 = 0;
	TandyModeRegister mode                = {};
	uint8_t color_select                  = 0;
	uint8_t disp_bank                     = 0;
	uint8_t reg_index                     = 0;
	TandyModeControlRegister mode_control = {};
	uint8_t palette_mask                  = 0;
	uint8_t extended_ram                  = 0;
	uint8_t border_color                  = 0;
	uint8_t line_mask                     = 0;
	uint8_t line_shift                    = 0;
	uint8_t draw_bank                     = 0;
	uint8_t mem_bank                      = 0;
	uint8_t* draw_base                    = nullptr;
	uint8_t* mem_base                     = nullptr;
	Bitu addr_mask                        = 0;
};

// CRTC Maximum Scan Line Register (Index 09h)
// Ref: http://www.osdever.net/FreeVGA/vga/crtcreg.htm#09
union MaximumScanLineRegister {
	uint8_t data = 0;

	// In text modes, this field is programmed with the character height - 1
	// (scan line numbers are zero based.) In graphics modes, a non-zero value
	// in this field will cause each scan line to be repeated by the value of
	// this field + 1 (0: single line, 1: doubled, 2: tripled, etc).
	//
	// This is independent of bit 7 (Scan Doubling), except in CGA modes which
	// seems to require this field to be 1 and bit 7 to be set to work.
	bit_view<0, 5> maximum_scan_line;

	// Specifies bit 9 of the Start Vertical Blanking field.
	bit_view<5, 1> start_vertical_blanking_bit9;

	// Specifies bit 9 of the Line Compare field.
	bit_view<6, 1> line_compare_bit9;

	// When this bit is set to 1, 200-scan-line video data is converted to
	// 400-scan-line output. To do this, the clock in the row scan counter is
	// divided by 2, which allows the 200-line modes to be displayed as 400
	// lines on the display (this is called double scanning; each line is
	// displayed twice). When this bit is set to 0, the clock to the row scan
	// counter is equal to the horizontal scan rate.
	bit_view<7, 1> is_scan_doubling_enabled;
};

// Sequencer Clocking Mode Register (Index 01h)
// Ref: http://www.osdever.net/FreeVGA/vga/seqreg.htm
union ClockingModeRegister {
	uint8_t data = 0;
	// Characters are drawn 8 pixels wide (or 9 if cleared)
	// This selects between 8-dot and 9-dot fonts, which also switches the
	// horizontal resolution from 640 pixels to 720 in 80-column modes, and
	// from 1056 to 1188 in 132-column modes.
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
	// the video serializers are loaded every character clock. When the Shift
	// 4 field is set to 1, the video serializers are loaded every forth
	// character clock, which is useful when 32 bits are fetched per cycle
	// and chained together in the shift registers.
	bit_view<4, 1> is_shift_4_enabled;

	// When set to 1, this bit turns off the display and assigns maximum
	// memory bandwidth to the system. Although the display is blanked, the
	// synchronization pulses are maintained. This bit can be used for rapid
	// full-screen updates.
	bit_view<5, 1> is_screen_disabled;
};

// Graphics Mode Register (Index 05h)
// Ref: http://www.osdever.net/FreeVGA/vga/graphreg.htm#05
union GraphicsModeRegister {
	uint8_t data = 0;

	// This field selects between four write modes, simply known as Write
	// Modes 0-3 (see reference for details).
	bit_view<0, 2> write_mode;

	// This field selects between two read modes, simply known as Read Mode 0,
	// and Read Mode 1 (see reference for details).
	bit_view<3, 1> read_mode;

	// When set to 1, this bit selects the odd/even addressing mode used by
	// the IBM Color/Graphics Monitor Adapter. Normally, the value here
	// follows the value of Memory Mode register bit 2 in the sequencer.
	bit_view<4, 1> is_host_odd_even;

	// When set to 1, this bit directs the shift registers in the graphics
	// controller to format the serial data stream with even-numbered bits
	// from both maps on even-numbered maps, and odd-numbered bits from both
	// maps on the odd-numbered maps. This bit is used for modes 4 and 5.
	bit_view<5, 1> shift_register_interleave_mode;

	// When set to 0, this bit allows bit 5 to control the loading of the
	// shift registers. When set to 1, this bit causes the shift registers to
	// be loaded in a manner that supports the 256-color mode.
	bit_view<6, 1> is_256_color_shift_mode;
};

// Attribute Mode Control Register (Index 10h)
// Ref: http://www.osdever.net/FreeVGA/vga/attrreg.htm#10
union AttributeModeControlRegister {
	uint8_t data = 0;

	// 1 in graphics modes, 0 in text modes
	bit_view<0, 1> is_graphics_enabled;

	// When this bit is set to 1, monochrome emulation mode is selected. When
	// this bit is set to 0, colour emulation mode is selected. It is present
	// and programmable in all of the hardware but it apparently does nothing.
	// The internal palette is used to provide monochrome emulation instead.
	bit_view<1, 1> is_monochrome_emulation_enabled;

	// This field is used in 9 bit wide character modes to provide continuity
	// for the horizontal line characters in the range C0h-DFh. If this field
	// is set to 0, then the 9th column of these characters is replicated from
	// the 8th column of the character. Otherwise, if it is set to 1 then the
	// 9th column is set to the background like the rest of the characters.
	bit_view<2, 1> is_line_graphics_enabled;

	// When this bit is set to 0, the most-significant bit of the attribute
	// selects the background intensity (allows 16 colours for background).
	// When set to 1, this bit enables blinking.
	bit_view<3, 1> is_blink_enabled;

	// This field allows the upper half of the screen to pan independently of
	// the lower screen.
	//
	// If this field is set to 0 then nothing special occurs during a
	// successful line compare (see the Line Compare field.)
	//
	// If this field is set to 1, then upon a successful line compare, the
	// bottom portion of the screen is displayed as if the Pixel Shift Count
	// and Byte Panning fields are set to 0. The PEL panning register (3C0h
	// index 13h) is temporarily set to 0 from when the line compare causes a
	// wrap around until the next vertical retrace when the register is
	// automatically reloaded with the old value, else the PEL panning
	// register ignores line compares.
	bit_view<5, 1> is_pixel_panning_enabled;

	// When this bit is set to 1, the video data is sampled so that eight bits
	// are available to select a colour in the 256-color mode (0x13). This bit
	// is set to 0 in all other modes.
	bit_view<6, 1> is_8bit_color_enabled;

	// This bit selects the source for the P5 and P4 video bits that act as
	// inputs to the video DAC. When this bit is set to 0, P5 and P4 are the
	// outputs of the Internal Palette registers. When this bit is set to 1,
	// P5 and P4 are bits 1 and 0 of the Color Select register.
	bit_view<7, 1> palette_bits_5_4_select;
};

// CRTC Mode Control Register (Index 17h)
// Ref: http://www.osdever.net/FreeVGA/vga/crtcreg.htm#17
union CrtcModeControlRegister {
	uint8_t data = 0;

	// This bit selects the source of bit 13 of the output multiplexer. When
	// this bit is set to 0, bit 0 of the row scan counter is the source, and
	// when this bit is set to 1, bit 13 of the address counter is the source.
	// The CRT controller used on the IBM Color/Graphics Adapter was capable
	// of using 128 horizontal scan-line addresses. For the VGA to obtain
	// 640-by-200 graphics resolution, the CRT controller is  programmed for
	// 100 horizontal scan lines with two scan-line addresses per character
	// row. Row scan  address bit 0 becomes the most-significant address bit
	// to the display buffer. Successive scan lines of  the display image are
	// displaced in 8KB of memory. This bit allows compatibility with the
	// graphics modes of earlier adapters.
	//
	// If clear use CGA compatible memory addressing system by substituting
	// character row scan counter bit 0 for address bit 13, thus creating 2
	// banks for even and odd scan lines.
	bit_view<0, 1> map_display_address_13;

	// This bit selects the source of bit 14 of the output multiplexer. When
	// this bit is set to 0, bit 1 of the row scan counter is the source. When
	// this bit is set to 1, the bit 14 of the address counter is the source.
	//
	// If clear use Hercules compatible memory addressing system by
	// substituting character row scan counter bit 1 for address bit 14, thus
	// creating 4 banks.
	bit_view<1, 1> map_display_address_14;

	// This bit selects the clock that controls the vertical timing counter.
	// The clocking is either the horizontal retrace clock or horizontal
	// retrace clock divided by 2. When this bit is set to 1. the horizontal
	// retrace clock is divided by 2. Dividing the clock effectively doubles
	// the vertical resolution of the CRT controller. The vertical counter has
	// a maximum resolution of 1024 scan lines because the vertical total
	// value is 10-bits wide. If the vertical counter is clocked with the
	// horizontal retrace divided by 2, the vertical resolution is doubled to
	// 2048 scan lines.
	bit_view<2, 1> div_scan_line_clock_by_2;

	// When this bit is set to 0, the address counter uses the character
	// clock. When this bit is set to 1, the address counter uses the
	// character clock input divided by 2. This bit is used to create either a
	// byte or word refresh address for the display buffer.
	bit_view<3, 1> div_memory_address_clock_by_2;

	// This bit selects the memory-address bit, bit MA 13 or MA 15, that
	// appears on the output pin MA 0, in the word address mode. If the VGA is
	// not in the word address mode, bit 0 from the address counter appears on
	// the output pin, MA 0. When set to 1, this bit selects MA 15. In
	// odd/even mode, this bit should be set to 1 because 256KB of video
	// memory is installed on the system board. (Bit MA 13 is selected in
	// applications where only 64KB is present. This function maintains
	// compatibility with the IBM Color/Graphics Monitor Adapter.)
	//
	// When in Word Mode bit 15 is rotated to bit 0 if this bit is set else
	// bit 13 is rotated into bit 0.
	bit_view<5, 1> address_wrap_select;

	// When this bit is set to 0, the word mode is selected. The word mode
	// shifts the memory-address counter bits to the left by one bit; the
	// most-significant bit of the counter appears on the least-significant
	// bit of the memory address outputs.  The doubleword bit in the Underline
	// Location register (0x14) also controls the addressing. When the
	// doubleword bit is 0, the word/byte bit selects the mode. When the
	// doubleword bit is set to 1, the addressing is shifted by two bits. When
	// set to 1, bit 6 selects the byte address mode.
	//
	// If clear system is in word mode. Addresses are rotated 1 position up
	// bringing either bit 13 or 15 into bit 0.
	bit_view<6, 1> word_byte_mode_select;

	// When set to 0, this bit disables the horizontal and vertical retrace
	// signals and forces them to an inactive level. When set to 1, this bit
	// enables the horizontal and vertical retrace signals. This bit does not
	// reset any other registers or signal outputs.
	//
	// Clearing this bit will reset the display system until the bit is set
	// again.
	bit_view<7, 1> is_sync_enabled;
};

// Attribute Controller Registers
// ==============================
//
// The Attribute Registers consist of:
// - the sixteen Palette Registers,
// - the Mode Control Register,
// - the Overscan Color Register,
// - the Color Plane Enable Register,
// - and the Horizontal Pixel Panning Register.
//
// The VGA also includes the Color Select Register.

// Attribute Address Register
// --------------------------
// (write port: 3C0h on EGA & VGA, read port: 3C1h on VGA only)
//
// Selects the Attribute Controller Registers that will be selected during a
// write operation for EGA/VGA, or a read operation for VGA.
//
// The attribute controller has only one port dedicated to it at 3C0h. An
// internal flip-flop is used to multiplex this port to load either this Address
// Attribute Register or one of the Attribute Registers:
//
// - When the flip-flop is in the clear state, it causes port writes
//   3COh to be directed to this Address Attribute Register.
//
// - When the flip-flop is in the set state, data written to this port is
//   directed to whichever Attribute Register index is loaded into the
//   attribute_address field of this register.
//
union AttributeAddressRegister {
	uint8_t data = 0;

	// Points to one of the Attribute Address Registers:
	// 00h-0Fh - Palette Registers 0-15
	// 10h     - Mode Control Register
	// 11h     - Overscan Color Register
	// 12h     - Color Plane Enable Register
	// 13h     - Horizontal Pixel Panning Register
	// 14h     - Color Plane Enable (VGA only)
	bit_view<0, 5> attribute_address;

	// Determines whether the palette dual-ported RAM should be accessed by
	// the host or by the EGA display memory.
	//
	// 0: Allows the host to access the palette RAM. Disables the display
	// memory from gaining access to the palette.
	//
	// 1: Allows the display memory to access the palette RAM. Disables the
	// host from gaining access to the palette.
	bit_view<5, 1> palette_address_source;
};

// Palette Registers (index 00h-0Fh)
union PaletteRegister {
	uint8_t data = 0;

	// On EGA, the values describe the colours directly. sr, sg, and sb stand
	// for the "secondary" RGB values, forming basically a 2-bit RGB colour
	// codes:
	//
	// sr,sg,sb   r,g,b    saturation
	//     0        0          0%
	//     0        1         33%
	//     1        0         66%
	//     1        1        100%
	bit_view<0, 1> b;
	bit_view<1, 1> g;
	bit_view<2, 1> r;
	bit_view<3, 1> sb;
	bit_view<4, 1> sg;
	bit_view<5, 1> sr;

	// On VGA, this is treated as 6-bit index that addresses the first 64
	// Color Registers that store the actual 18-bit RGB colours.
	bit_view<0, 6> index;
};

struct VgaSeq {
	uint8_t index = 0;
	uint8_t reset = 0;

	ClockingModeRegister clocking_mode = {};

	// Let the user force the clocking mode's 8/9-dot-mode bit high
	bool wants_vga_8dot_font = false;

	uint8_t map_mask = 0;
	uint8_t character_map_select = 0;
	uint8_t memory_mode = 0;
};

struct VgaAttr {
	// Internal flip-flop is used to multiplex the 3C0h port to load either
	// the Attribute Address Register or one of the Attribute Registers.
	bool is_address_mode = true;

	// The index of the register selected by Attribute Address Register.
	// The next byte write to 3C0h will be loaded into this register.
	uint8_t index = 0;

	// On EGA: 2-bit RGB colour values
	// On VGA: indices into the first 64 Color Registers
	uint8_t palette[16] = {};

	AttributeModeControlRegister mode_control = {};

	uint8_t horizontal_pel_panning = 0;
	uint8_t overscan_color         = 0;
	uint8_t color_plane_enable     = 0;
	uint8_t color_select           = 0;

	// Used for disabling the screen.
	//
	// Bit0: screen disabled by attribute controller index
	// Bit1: screen disabled by sequencer index 1 bit 5
	//
	// These are put together in one variable for performance reasons: the
	// line drawing function is called maybe 60*480=28800 times/s, and we
	// only need to check one variable for zero this way.
	uint8_t disabled = 0;
};

struct VgaCrtc {
	uint8_t horizontal_total       = 0;
	uint8_t horizontal_display_end = 0;

	uint8_t start_horizontal_blanking = 0;
	uint8_t end_horizontal_blanking   = 0;
	uint8_t start_horizontal_retrace  = 0;
	uint8_t end_horizontal_retrace    = 0;

	uint8_t vertical_total  = 0;
	uint8_t overflow        = 0;
	uint8_t preset_row_scan = 0;

	MaximumScanLineRegister maximum_scan_line = {};

	uint8_t cursor_start = 0;
	uint8_t cursor_end   = 0;

	uint8_t start_address_high = 0;
	uint8_t start_address_low  = 0;

	uint8_t cursor_location_high = 0;
	uint8_t cursor_location_low  = 0;

	uint8_t vertical_retrace_start = 0;
	uint8_t vertical_retrace_end   = 0;
	uint8_t vertical_display_end   = 0;

	uint8_t offset                  = 0;
	uint8_t underline_location      = 0;
	uint8_t start_vertical_blanking = 0;
	uint8_t end_vertical_blanking   = 0;

	CrtcModeControlRegister mode_control = {};

	uint8_t line_compare = 0;

	uint8_t index  = 0;
	bool read_only = false;
};

struct VgaGfx {
	uint8_t index            = 0;
	uint8_t set_reset        = 0;
	uint8_t enable_set_reset = 0;
	uint8_t color_compare    = 0;
	uint8_t data_rotate      = 0;
	uint8_t read_map_select  = 0;
	uint8_t mode             = 0;
	uint8_t miscellaneous    = 0;
	uint8_t color_dont_care  = 0;
	uint8_t bit_mask         = 0;
};

using cga_colors_t = std::array<Rgb666, NumCgaColors>;

struct VgaDac {
	Rgb666 rgb[NumVgaColors]           = {};
	Bgrx8888 palette_map[NumVgaColors] = {};

	uint8_t combine[16] = {};

	// DAC 8-bit registers
	uint8_t bits        = 0; /* DAC bits, usually 6 or 8 */
	uint8_t pel_mask    = 0;
	uint8_t pel_index   = 0;
	uint8_t state       = 0;
	uint8_t write_index = 0;
	uint8_t read_index  = 0;
};

struct VgaSvga {
	Bitu readStart = 0, writeStart = 0;
	Bitu bankMask        = 0;
	Bitu bank_read_full  = 0;
	Bitu bank_write_full = 0;
	uint8_t bank_read    = 0;
	uint8_t bank_write   = 0;
	Bitu bank_size       = 0;
};

union VgaLatch {
	uint32_t d = 0;
	uint8_t b[4];
};

struct VgaMemory {
	uint8_t* linear = {};
};

struct VGA_Changes {
	// Add a few more just to be safe
	// Allocated dynamically: [(VGA_MEMORY >> VGA_CHANGE_SHIFT) + 32]
	uint8_t* map = nullptr;

	uint8_t checkMask    = 0;
	uint8_t frame        = 0;
	uint8_t writeMask    = 0;
	bool active          = 0;
	uint32_t clearMask   = 0;
	uint32_t start       = 0;
	uint32_t last        = 0;
	uint32_t lastAddress = 0;
};

struct VgaLfb {
	uint32_t page = 0;
	uint32_t addr = 0;
	uint32_t mask = 0;

	PageHandler* handler = nullptr;
};

struct VgaType {
	// The mode the vga system is in
	VGAModes mode = {};

	uint8_t misc_output  = 0;
	VgaDraw draw         = {};
	VgaConfig config     = {};

	// Internal module groups
	VgaSeq seq     = {};
	VgaAttr attr   = {};
	VgaCrtc crtc   = {};
	VgaGfx gfx     = {};
	VgaDac dac     = {};
	VgaLatch latch = {};
	VgaS3 s3       = {};
	VgaSvga svga   = {};
	VgaHerc herc   = {};
	VgaTandy tandy = {};
	VgaOther other = {};
	VgaMemory mem  = {};

	// This is assumed to be power of 2
	uint32_t vmemwrap = 0;

	// Memory for fast (usually 16-colour) rendering,
	// always twice as big as vmemsize
	uint8_t* fastmem  = {};
	uint32_t vmemsize = 0;

	// How much delay to add to video memory I/O in nanoseconds
	uint16_t vmem_delay_ns = 0;

#ifdef VGA_KEEP_CHANGES
	VgaChanges changes = {};
#endif

	VgaLfb lfb = {};

	// Composite video mode parameters
	struct {
		int32_t ri = 0;
		int32_t rq = 0;
		int32_t gi = 0;
		int32_t gq = 0;
		int32_t bi = 0;
		int32_t bq = 0;

		int32_t sharpness = 0;
	} composite = {};

	// This flag is used to detect if a 200-lines EGA mode on VGA uses
	// custom 18-bit VGA colours. When the first such colour is encountered
	// when setting up the palette, we set this flag to true and potentially
	// switch to a VGA shader if an adaptive CRT shader is active. After
	// that, we stop checking palette changes until the next screen mode
	// change.
	bool ega_mode_with_vga_colors = false;
};

// Hercules & CGA monochrome palette
enum class MonochromePalette : uint8_t {
	Amber      = 0,
	Green      = 1,
	White      = 2,
	Paperwhite = 3
};

constexpr auto NumMonochromePalettes = enum_val(MonochromePalette::Paperwhite) + 1;

void VGA_SetMonochromePalette(const enum MonochromePalette);
void VGA_SetHerculesPalette();
void VGA_SetMonochromeCgaPalette();

// Functions for different resolutions
void VGA_SetMode(VGAModes mode);
void VGA_DetermineMode(void);
void VGA_SetupHandlers(void);
const char* to_string(const VGAModes mode);

void VGA_StartResize();
void VGA_StartResizeAfter(const uint16_t delay_ms);

void VGA_SetupDrawing(uint32_t val);
void VGA_CheckScanLength(void);
void VGA_ChangedBank(void);

const VideoMode& VGA_GetCurrentVideoMode();

// DAC/Attribute functions
void VGA_DAC_CombineColor(const uint8_t palette_idx, const uint8_t color_idx);

void VGA_DAC_SetEntry(const uint8_t color_idx, const uint8_t red,
                      const uint8_t green, const uint8_t blue);

void VGA_ATTR_SetPalette(const uint8_t index, const PaletteRegister value);

enum class EgaMonitorMode { Cga, Ega, Mono };

void VGA_ATTR_SetEGAMonitorPalette(const EgaMonitorMode m);

// The VGA subfunction startups
void VGA_SetupAttr(void);
void VGA_SetupMemory(Section* sec);
void VGA_SetupDAC(void);
void VGA_SetupCRTC(void);
void VGA_SetupMisc(void);
void VGA_SetupGFX(void);
void VGA_SetupSEQ(void);
void VGA_SetupOther(void);
void VGA_SetupXGA(void);
void VGA_AddCompositeSettings(Config& conf);

// Some support functions
struct VideoModeBlock;

void VGA_SetClock(Bitu which, uint32_t target);

void VGA_SetRefreshRateMode(const std::string& pref);
double VGA_GetRefreshRate();

void VGA_DACSetEntirePalette(void);
void VGA_StartRetrace(void);
void VGA_StartUpdateLFB(void);
void VGA_SetBlinking(uint8_t enabled);
void VGA_SetCGA2Table(uint8_t val0, uint8_t val1);
void VGA_SetCGA4Table(uint8_t val0, uint8_t val1, uint8_t val2, uint8_t val3);
PixelFormat VGA_ActivateHardwareCursor();
void VGA_KillDrawing(void);

void VGA_SetOverride(const bool vga_override, const double override_refresh_hz = 0);
void VGA_LogInitialization(const char* adapter_name, const char* ram_type,
                           const size_t num_modes);

void VGA_AllowVgaScanDoubling(const bool allow);
void VGA_AllowPixelDoubling(const bool allow);

extern VgaType vga;

// Support for modular SVGA implementation

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
	Bitu offset          = 0;
	Bitu modeNo          = 0;
	uint32_t htotal      = 0;
	uint32_t vtotal      = 0;
};

// Vector function prototypes
typedef void (*tWritePort)(io_port_t reg, io_val_t value, io_width_t width);
typedef uint8_t (*tReadPort)(io_port_t reg, io_width_t width);
typedef void (*tFinishSetMode)(io_port_t crtc_base, VGA_ModeExtraData* modeData);
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

void SVGA_Setup_S3();
void SVGA_Setup_TsengEt4k();
void SVGA_Setup_TsengEt3k();
void SVGA_Setup_Paradise();
void SVGA_Setup_Driver();

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
