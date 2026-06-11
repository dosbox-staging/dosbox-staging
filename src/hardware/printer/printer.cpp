// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "printer.h"

#if C_PRINTER

#include "config/setup.h"
#include "gui/mapper.h"
#include "misc/cross.h"
#include "misc/support.h"
#include "printer_charmaps.h"
#include "printer_if.h"
#include <array>
#include <math.h>
#include <memory>
#include <optional>
#include <vector>

#include "misc/std_filesystem.h"
#include "printer_internal.h"
#include "printer_png.h"
#include "utils/string_utils.h"

#include "hardware/pic.h" // for timeout
#include "utils/checks.h"

CHECK_NARROWING();

static std::unique_ptr<Printer> default_printer = nullptr;

static uint16_t conf_dpi, conf_width, conf_height;
static uint64_t printer_timeout;
static bool timeout_dirty;
static std_fs::path document_path;
// static const char* font_path;
static std::string conf_output_device;
static bool conf_multipage_output;

// Printer::FillPalette lives in printer_glyph.cpp.

Printer::Printer(uint16_t dpi, const uint16_t width, const uint16_t height,
                 const std::string& output, bool multipage_output)
{
	if (FT_Init_FreeType(&ft_lib)) {
		LOG_ERR("PRINTER: Unable to init Freetype2. Printing disabled");
		page = nullptr;
	} else {
		this->dpi              = dpi;
		this->output           = output;
		this->multipage_output = multipage_output;

		default_page_width = static_cast<Real64>(width) /
		                     static_cast<Real64>(10);
		default_page_height = static_cast<Real64>(height) /
		                      static_cast<Real64>(10);

		// Create page
		page = SDL_CreateRGBSurface(SDL_SWSURFACE,
		                            static_cast<int>(default_page_width * dpi),
		                            static_cast<int>(default_page_height * dpi),
		                            8,
		                            0,
		                            0,
		                            0,
		                            0);

		// Set a grey palette
		SDL_Palette* palette = page->format->palette;

		for (uint64_t i = 0; i < 32; i++) {
			palette->colors[i].r = 255;
			palette->colors[i].g = 255;
			palette->colors[i].b = 255;
		}
		// 0 = all white needed for logic 000
		FillPalette(0, 0, 0, 1, palette);
		// 1 = magenta* 001
		FillPalette(0, 255, 0, 1, palette);
		// 2 = cyan*    010
		FillPalette(255, 0, 0, 2, palette);
		// 3 = "violet" 011
		FillPalette(255, 255, 0, 3, palette);
		// 4 = yellow*  100
		FillPalette(0, 0, 255, 4, palette);
		// 5 = red      101
		FillPalette(0, 255, 255, 5, palette);
		// 6 = green    110
		FillPalette(255, 0, 255, 6, palette);
		// 7 = black    111
		FillPalette(255, 255, 255, 7, palette);

		// yyyxxxxx bit pattern: yyy=color xxxxx = intensity: 31=max
		// Printing colors on top of each other ORs them and gets the
		// correct resulting color.
		// i.e. magenta on blank page yyy=001
		// then yellow on magenta 001 | 100 = 101 = red

		color = ColorBlack;

		cur_font  = nullptr;
		char_read = false;
		auto_feed = false;
		output_handle.reset();

		ResetPrinter();

		// Win32 host-printer pass-through removed (out of scope).
		LOG_MSG("PRINTER: Enabled");
	}
}

void Printer::ResetPrinterHard()
{
	char_read = false;
	ResetPrinter();
}

void Printer::ResetPrinter()
{
	color = ColorBlack;
	cur_x = cur_y = 0.0;
	esc_seen      = false;
	fs_seen       = false;
	esc_cmd       = 0;
	num_param = needed_param = 0;
	top_margin               = 0.0;
	left_margin              = 0.0;
	right_margin = page_width = default_page_width;
	bottom_margin = page_height = default_page_height;
	line_spacing                = static_cast<Real64>(1) / 6;
	cpi                         = 10.0;
	cur_char_table              = 1;
	style.data                  = 0;
	extra_intra_space           = 0.0;
	print_upper_contr           = true;
	bit_graph.rem_bytes         = 0;
	densk                       = 0;
	densl                       = 1;
	densy                       = 2;
	densz                       = 3;
	// Char table slot 0 is reserved for italics; slots 1..3 default to CP437.
	char_tables[0] = 0;
	char_tables[1] = char_tables[2] = char_tables[3] = 437;
	defined_unit                                     = -1;
	multipoint                                       = false;
	multi_point_size                                 = 0.0;
	multi_cpi                                        = 0.0;
	hmi                                              = -1.0;
	msb                                              = 255;
	num_print_as_char                                = 0;
	lq_typeface                                      = Typeface::Courier;

	SelectCodepage(char_tables[cur_char_table]);

	UpdateFont();

	NewPage(false, true);

	// Default tabs => Each eight characters
	for (uint64_t i = 0; i < 32; i++) {
		horiz_tabs[i] = static_cast<Real64>(i * 8) / static_cast<Real64>(cpi);
	}
	num_horiz_tabs = 32;

	num_vert_tabs = 255;
}

Printer::~Printer(void)
{
	FinishMultipage();
	if (page != nullptr) {
		SDL_FreeSurface(page);
		page = nullptr;
		FT_Done_FreeType(ft_lib);
	}
}

void Printer::SelectCodepage(const uint16_t codepage)
{
	const uint16_t* map_to_use = nullptr;

	uint64_t i = 0;
	while (charmap[i].codepage != 0) {
		if (charmap[i].codepage == codepage) {
			map_to_use = charmap[i].map;
			break;
		}
		i++;
	}
	if (map_to_use == nullptr) {
		LOG_WARNING("PRINTER: Unsupported codepage %i. Using CP437 instead.",
		            codepage);
		SelectCodepage(437);
		return;
	} /*
	 switch(cp)
	 {
	 case 0: // Italics, use cp437
	 case 437:
	         mapToUse = (uint16_t*)&cp437_map;
	         break;

	 case 737:
	         mapToUse = (uint16_t*)&cp737_map;
	         break;

	 case 775:
	         mapToUse = (uint16_t*)&cp775_map;
	         break;

	 case 850:
	         mapToUse = (uint16_t*)&cp850_map;
	         break;

	 case 852:
	         mapToUse = (uint16_t*)&cp852_map;
	         break;

	 case 855:
	         mapToUse = (uint16_t*)&cp855_map;
	         break;

	 case 857:
	         mapToUse = (uint16_t*)&cp857_map;
	         break;

	 case 860:
	         mapToUse = (uint16_t*)&cp860_map;
	         break;

	 case 861:
	         mapToUse = (uint16_t*)&cp861_map;
	         break;

	 case 863:
	         mapToUse = (uint16_t*)&cp863_map;
	         break;

	 case 864:
	         mapToUse = (uint16_t*)&cp864_map;
	         break;

	 case 865:
	         mapToUse = (uint16_t*)&cp865_map;
	         break;

	 case 866:
	         mapToUse = (uint16_t*)&cp866_map;
	         break;

	 default:
	         LOG(LOG_MISC,LOG_WARN)("Unsupported codepage %i. Using CP437
	 instead.", cp); mapToUse = (uint16_t*)&cp437_map;
	 }*/

	for (int i = 0; i < 256; i++) {
		cur_map[i] = map_to_use[i];
	}
}

// Printer::UpdateFont lives in printer_glyph.cpp.

bool Printer::ProcessCommandChar(const uint8_t ch)
{
	if (esc_seen || fs_seen) {
		esc_cmd = ch;
		if (fs_seen) {
			esc_cmd |= 0x800;
		}
		esc_seen = fs_seen = false;
		num_param          = 0;

		switch (esc_cmd) {
		case 0x02: // Undocumented
		case 0x0a: // Reverse line feed
		           // (ESC LF)
		case 0x0c: // Return to top of current page
		           // (ESC FF)
		case 0x0e: // Select double-width printing (one line)
		           // (ESC SO)
		case 0x0f: // Select condensed printing
		           // (ESC SI)
		case 0x23: // Cancel MSB control
		           // (ESC #)
		case 0x30: // Select 1/8-inch line spacing
		           // (ESC 0)
		case 0x31: // Select 7/60-inch line spacing
		           // (ESC 1)
		case 0x32: // Select 1/6-inch line spacing
		           // (ESC 2)
		case 0x34: // Select italic font
		           // (ESC 4)
		case 0x35: // Cancel italic font
		           // (ESC 5)
		case 0x36: // Enable printing of upper control codes
		           // (ESC 6)
		case 0x37: // Enable upper control codes
		           // (ESC 7)
		case 0x38: // Disable paper-out detector
		           // (ESC 8)
		case 0x39: // Enable paper-out detector
		           // (ESC 9)
		case 0x3c: // Unidirectional mode (one line)
		           // (ESC <)
		case 0x3d: // Set MSB to 0
		           // (ESC =)
		case 0x3e: // Set MSB to 1
		           // (ESC >)
		case 0x40: // Initialize printer
		           // (ESC @)
		case 0x45: // Select bold font
		           // (ESC E)
		case 0x46: // Cancel bold font
		           // (ESC F)
		case 0x47: // Select double-strike printing
		           // (ESC G)
		case 0x48: // Cancel double-strike printing
		           // (ESC H)
		case 0x4d: // Select 10.5-point, 12-cpi
		           // (ESC M)
		case 0x4f: // Cancel bottom margin [conflict]
		           // (ESC O)
		case 0x50: // Select 10.5-point, 10-cpi
		           // (ESC P)
		case 0x54: // Cancel superscript/subscript printing
		           // (ESC T)
		case 0x5e: // Enable printing of all character codes on next
		           // character	(ESC ^)
		case 0x67: // Select 10.5-point, 15-cpi
		           // (ESC g)

		case 0x834: // Select italic font
		            // (FS 4)	(= ESC 4)
		case 0x835: // Cancel italic font
		            // (FS 5)	(= ESC 5)
		case 0x846: // Select forward feed mode
		            // (FS F)
		case 0x852: // Select reverse feed mode
		            // (FS R)
			needed_param = 0;
			break;

		case 0x19: // Control paper loading/ejecting
		           // (ESC EM)
		case 0x20: // Set intercharacter space
		           // (ESC SP)
		case 0x21: // Master select
		           // (ESC !)
		case 0x2b: // Set n/360-inch line spacing
		           // (ESC +)
		case 0x2d: // Turn underline on/off
		           // (ESC -)
		case 0x2f: // Select vertical tab channel
		           // (ESC /)
		case 0x33: // Set n/180-inch line spacing
		           // (ESC 3)
		case 0x41: // Set n/60-inch line spacing
		           // (ESC A)
		case 0x43: // Set page length in lines
		           // (ESC C)
		case 0x49: // Select character type and print pitch
		           // (ESC I)
		case 0x4a: // Advance print position vertically
		           // (ESC J)
		case 0x4e: // Set bottom margin
		           // (ESC N)
		case 0x51: // Set right margin
		           // (ESC Q)
		case 0x52: // Select an international character set
		           // (ESC R)
		case 0x53: // Select superscript/subscript printing
		           // (ESC S)
		case 0x55: // Turn unidirectional mode on/off
		           // (ESC U)
		// case 0x56: // Repeat data
		// (ESC V)
		case 0x57: // Turn double-width printing on/off
		           // (ESC W)
		case 0x61: // Select justification
		           // (ESC a)
		case 0x66: // Absolute horizontal tab in columns [conflict]
		           // (ESC f)
		case 0x68: // Select double or quadruple size
		           // (ESC h)
		case 0x69: // Immediate print
		           // (ESC i)
		case 0x6a: // Reverse paper feed
		           // (ESC j)
		case 0x6b: // Select typeface
		           // (ESC k)
		case 0x6c: // Set left margin
		           // (ESC 1)
		case 0x70: // Turn proportional mode on/off
		           // (ESC p)
		case 0x72: // Select printing color
		           // (ESC r)
		case 0x73: // Low-speed mode on/off
		           // (ESC s)
		case 0x74: // Select character table
		           // (ESC t)
		case 0x77: // Turn double-height printing on/off
		           // (ESC w)
		case 0x78: // Select LQ or draft
		           // (ESC x)
		case 0x7e: // Select/Deselect slash zero
		           // (ESC ~)

		case 0x832: // Select 1/6-inch line spacing
		            // (FS 2)	(= ESC 2)
		case 0x833: // Set n/360-inch line spacing
		            // (FS 3)	(= ESC +)
		case 0x841: // Set n/60-inch line spacing
		            // (FS A)	(= ESC A)
		case 0x843: // Select LQ type style
		            // (FS C)	(= ESC k)
		case 0x845: // Select character width
		            // (FS E)
		case 0x849: // Select character table
		            // (FS I)	(= ESC t)
		case 0x853: // Select High Speed/High Density elite pitch
		            // (FS S)
		case 0x856: // Turn double-height printing on/off
		            // (FS V)	(= ESC w)
			needed_param = 1;
			break;

		case 0x24:  // Set absolute horizontal print position
		            // (ESC $)
		case 0x3f:  // Reassign bit-image mode
		            // (ESC ?)
		case 0x4b:  // Select 60-dpi graphics
		            // (ESC K)
		case 0x4c:  // Select 120-dpi graphics
		            // (ESC L)
		case 0x59:  // Select 120-dpi, double-speed graphics
		            // (ESC Y)
		case 0x5a:  // Select 240-dpi graphics
		            // (ESC Z)
		case 0x5c:  // Set relative horizontal print position
		            // (ESC \)
		case 0x63:  // Set horizontal motion index (HMI)	[conflict]
		            // (ESC c)
		case 0x65:  // Set vertical tab stops every n lines
		            // (ESC e)
		case 0x85a: // Print 24-bit hex-density graphics
		            // (FS Z)
			needed_param = 2;
			break;

		case 0x2a: // Select bit image
		           // (ESC *)
		case 0x58: // Select font by pitch and point [conflict]
		           // (ESC X)
			needed_param = 3;
			break;

		case 0x5b: // Select character height, width, line spacing
			needed_param = 7;
			break;

		case 0x62: // Set vertical tabs in VFU channels
		           // (ESC b)
		case 0x42: // Set vertical tabs
		           // (ESC B)
			num_vert_tabs = 0;
			return true;
		case 0x44: // Set horizontal tabs
		           // (ESC D)
			num_horiz_tabs = 0;
			return true;
		case 0x25: // Select user-defined set
		           // (ESC %)
		case 0x26: // Define user-defined characters
		           // (ESC &)
		case 0x3a: // Copy ROM to RAM
		           // (ESC :)
			LOG_ERR("PRINTER: User-defined characters not supported");
			return true;
		case 0x28: // Two bytes sequence
			return true;
		default:
			LOG_MSG("PRINTER: Unknown command %s (%02Xh) %c , unable to skip parameters.",
			        (esc_cmd & 0x800) ? "FS" : "ESC",
			        esc_cmd,
			        esc_cmd);

			needed_param = 0;
			esc_cmd      = 0;
			return true;
		}

		if (needed_param > 0) {
			return true;
		}
	}

	// Two bytes sequence
	if (esc_cmd == '(') {
		esc_cmd = 0x200 + ch;

		switch (esc_cmd) {
		case 0x242: // Bar code setup and print (ESC (B)
		case 0x25e: // Print data as characters (ESC (^)
			needed_param = 2;
			break;

		case 0x255: // Set unit (ESC (U)
			needed_param = 3;
			break;

		case 0x243: // Set page length in defined unit (ESC (C)
		case 0x256: // Set absolute vertical print position (ESC (V)
		case 0x276: // Set relative vertical print position (ESC (v)
			needed_param = 4;
			break;

		case 0x274: // Assign character table (ESC (t)
		case 0x22d: // Select line/score (ESC (-)
			needed_param = 5;
			break;

		case 0x263: // Set page format (ESC (c)
			needed_param = 6;
			break;

		default:
			// ESC ( commands are always followed by a "number of
			// parameters" word parameter
			// LOG(LOG_MISC,LOG_ERROR)
			LOG_MSG("PRINTER: Skipping unsupported command ESC ( %c (%02X).",
			        esc_cmd,
			        esc_cmd);
			needed_param = 2;
			esc_cmd      = 0x101;
			return true;
		}

		// Every case above sets needed_param to a positive value (the
		// default branch also returns directly), so we always have
		// pending parameters at this point.
		return true;
	}

	// Ignore VFU channel setting
	if (esc_cmd == 0x62) {
		esc_cmd = 0x42;
		return true;
	}

	// Collect vertical tabs
	if (esc_cmd == 0x42) {
		if (ch == 0 || (num_vert_tabs > 0 &&
		                vert_tabs[num_vert_tabs - 1] >
		                        static_cast<Real64>(ch) * line_spacing)) { // Done
			esc_cmd = 0;
		} else if (num_vert_tabs < 16) {
			vert_tabs[num_vert_tabs++] = static_cast<Real64>(ch) *
			                             line_spacing;
		}
	}

	// Collect horizontal tabs
	if (esc_cmd == 0x44) {
		if (ch == 0 || (num_horiz_tabs > 0 &&
		                horiz_tabs[num_horiz_tabs - 1] >
		                        static_cast<Real64>(ch) *
		                                (1 / static_cast<Real64>(cpi)))) { // Done
			esc_cmd = 0;
		} else if (num_horiz_tabs < 32) {
			horiz_tabs[num_horiz_tabs++] = static_cast<Real64>(ch) *
			                               (1 / static_cast<Real64>(cpi));
		}
	}

	if (num_param < needed_param) {
		params[num_param++] = ch;

		if (num_param < needed_param) {
			return true;
		}
	}

	if (esc_cmd != 0) {
		switch (esc_cmd) {
		case 0x02: // Undocumented
			// Ignore
			break;

		case 0x0e: // Select double-width printing (one line) (ESC SO)
			if (!multipoint) {
				hmi = -1;
				style.doublewidth_oneline = 1;
				UpdateFont();
			}
			break;

		case 0x0f: // Select condensed printing (ESC SI)
			if (!multipoint && (cpi != 15.0)) {
				hmi = -1;
				style.condensed = 1;
				UpdateFont();
			}
			break;

		case 0x19: // Control paper loading/ejecting (ESC EM)
			// We are not really loading paper, so most commands can
			// be ignored
			if (params[0] == 'R') {
				NewPage(true, false); // TODO resetx?
			}
			break;

		case 0x20: // Set intercharacter space (ESC SP)
			if (!multipoint) {
				extra_intra_space = static_cast<Real64>(params[0]) /
				                    (print_quality == PrintQuality::Draft
				                             ? 120
				                             : 180);
				hmi = -1;
				UpdateFont();
			}
			break;

		case 0x21: // Master select (ESC !)
			cpi = params[0] & 0x01 ? 12 : 10;

			// Reset the first seven style bits (prop..superscript) and
			// rebuild them from the master-select byte below.
			style.data &= 0xFF80;
			if (params[0] & 0x02) {
				style.prop = 1;
			}
			if (params[0] & 0x04) {
				style.condensed = 1;
			}
			if (params[0] & 0x08) {
				style.bold = 1;
			}
			if (params[0] & 0x10) {
				style.doublestrike = 1;
			}
			if (params[0] & 0x20) {
				style.doublewidth = 1;
			}
			if (params[0] & 0x40) {
				style.italics = 1;
			}
			if (params[0] & 0x80) {
				score = ScoreType::Single;
				style.underline = 1;
			}

			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		case 0x23: // Cancel MSB control (ESC #)
			msb = 255;
			break;

		case 0x24: // Set absolute horizontal print position (ESC $)
		{
			Real64 unitSize = defined_unit;
			if (unitSize < 0) {
				unitSize = static_cast<Real64>(60.0);
			}

			const Real64 newX = left_margin +
			                    (static_cast<Real64>(Param16(0)) /
			                     unitSize);
			if (newX <= right_margin) {
				cur_x = newX;
			}
		} break;

		case 0x85a: // Print 24-bit hex-density graphics (FS Z)
			SetupBitImage(40, static_cast<uint16_t>(Param16(0)));
			break;

		case 0x2a: // Select bit image (ESC *)
			SetupBitImage(params[0], static_cast<uint16_t>(Param16(1)));
			break;

		case 0x2b:  // Set n/360-inch line spacing (ESC +)
		case 0x833: // Set n/360-inch line spacing (FS 3)
			line_spacing = static_cast<Real64>(params[0]) / 360;
			break;

		case 0x2d: // Turn underline on/off (ESC -)
			if (params[0] == 0 || params[0] == 48) {
				style.underline = 0;
			}
			if (params[0] == 1 || params[0] == 49) {
				style.underline = 1;
				score = ScoreType::Single;
			}
			UpdateFont();
			break;

		case 0x2f: // Select vertical tab channel (ESC /)
			// Ignore
			break;

		case 0x30: // Select 1/8-inch line spacing (ESC 0)
			line_spacing = static_cast<Real64>(1) / 8;
			break;

		case 0x32: // Select 1/6-inch line spacing (ESC 2)
			line_spacing = static_cast<Real64>(1) / 6;
			break;

		case 0x33: // Set n/180-inch line spacing (ESC 3)
			line_spacing = static_cast<Real64>(params[0]) / 180;
			break;

		case 0x34: // Select italic font (ESC 4)
			style.italics = 1;
			UpdateFont();
			break;

		case 0x35: // Cancel italic font (ESC 5)
			style.italics = 0;
			UpdateFont();
			break;

		case 0x36: // Enable printing of upper control codes (ESC 6)
			print_upper_contr = true;
			break;

		case 0x37: // Enable upper control codes (ESC 7)
			print_upper_contr = false;
			break;

		case 0x3c: // Unidirectional mode (one line) (ESC <)
			// We don't have a print head, so just ignore this
			break;

		case 0x3d: // Set MSB to 0 (ESC =)
			msb = 0;
			break;

		case 0x3e: // Set MSB to 1 (ESC >)
			msb = 1;
			break;

		case 0x3f: // Reassign bit-image mode (ESC ?)
			if (params[0] == 75) {
				densk = params[1];
			}
			if (params[0] == 76) {
				densl = params[1];
			}
			if (params[0] == 89) {
				densy = params[1];
			}
			if (params[0] == 90) {
				densz = params[1];
			}
			break;

		case 0x40: // Initialize printer (ESC @)
			ResetPrinter();
			break;

		case 0x41: // Set n/60-inch line spacing
		case 0x841:
			line_spacing = static_cast<Real64>(params[0]) / 60;
			break;

		case 0x43: // Set page length in lines (ESC C)
			if (params[0] != 0) {
				page_height = bottom_margin = static_cast<Real64>(
				                                      params[0]) *
				                              line_spacing;
			} else // == 0 => Set page length in inches
			{
				needed_param = 1;
				num_param    = 0;
				esc_cmd      = 0x100;
				return true;
			}
			break;

		case 0x45: // Select bold font (ESC E)
			style.bold = 1;
			UpdateFont();
			break;

		case 0x46: // Cancel bold font (ESC F)
			style.bold = 0;
			UpdateFont();
			break;

		case 0x47: // Select dobule-strike printing (ESC G)
			style.doublestrike = 1;
			break;

		case 0x48: // Cancel double-strike printing (ESC H)
			style.doublestrike = 0;
			break;

		case 0x4a: // Advance print position vertically (ESC J n)
			cur_y += static_cast<Real64>(
			        static_cast<Real64>(params[0]) / 180);
			if (cur_y > bottom_margin) {
				NewPage(true, false);
			}
			break;

		case 0x4b: // Select 60-dpi graphics (ESC K)
			SetupBitImage(densk, static_cast<uint16_t>(Param16(0)));
			break;

		case 0x4c: // Select 120-dpi graphics (ESC L)
			SetupBitImage(densl, static_cast<uint16_t>(Param16(0)));
			break;

		case 0x4d: // Select 10.5-point, 12-cpi (ESC M)
			cpi        = 12;
			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		case 0x4e: // Set bottom margin (ESC N)
			top_margin = 0.0;
			bottom_margin = static_cast<Real64>(params[0]) * line_spacing;
			break;

		case 0x4f: // Cancel bottom (and top) margin
			top_margin    = 0.0;
			bottom_margin = page_height;
			break;

		case 0x50: // Select 10.5-point, 10-cpi (ESC P)
			cpi        = 10;
			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		case 0x51: // Set right margin
			right_margin = static_cast<Real64>(params[0] - 1.0) /
			               static_cast<Real64>(cpi);
			break;

		case 0x52: // Select an international character set (ESC R)
			if (params[0] <= 13 || params[0] == 64) {
				if (params[0] == 64) {
					params[0] = 14;
				}

				cur_map[0x23] = int_char_sets[params[0]][0];
				cur_map[0x24] = int_char_sets[params[0]][1];
				cur_map[0x40] = int_char_sets[params[0]][2];
				cur_map[0x5b] = int_char_sets[params[0]][3];
				cur_map[0x5c] = int_char_sets[params[0]][4];
				cur_map[0x5d] = int_char_sets[params[0]][5];
				cur_map[0x5e] = int_char_sets[params[0]][6];
				cur_map[0x60] = int_char_sets[params[0]][7];
				cur_map[0x7b] = int_char_sets[params[0]][8];
				cur_map[0x7c] = int_char_sets[params[0]][9];
				cur_map[0x7d] = int_char_sets[params[0]][10];
				cur_map[0x7e] = int_char_sets[params[0]][11];
			}
			break;

		case 0x53: // Select superscript/subscript printing (ESC S)
			if (params[0] == 0 || params[0] == 48) {
				style.subscript = 1;
			}
			if (params[0] == 1 || params[1] == 49) {
				style.superscript = 1;
			}
			UpdateFont();
			break;

		case 0x54: // Cancel superscript/subscript printing (ESC T)
			style.superscript = 0;
			style.subscript   = 0;
			UpdateFont();
			break;

		case 0x55: // Turn unidirectional mode on/off (ESC U)
			// We don't have a print head, so just ignore this
			break;

		case 0x57: // Turn double-width printing on/off (ESC W)
			if (!multipoint) {
				hmi = -1;
				if (params[0] == 0 || params[0] == 48) {
					style.doublewidth = 0;
				}
				if (params[0] == 1 || params[0] == 49) {
					style.doublewidth = 1;
				}
				UpdateFont();
			}
			break;

		case 0x58: // Select font by pitch and point (ESC X)
			multipoint = true;
			// Copy currently non-multipoint CPI if no value was set
			// so far
			if (multi_cpi == 0) {
				multi_cpi = cpi;
			}
			if (params[0] > 0) // Set CPI
			{
				if (params[0] == 1) { // Proportional spacing
					style.prop = 1;
				} else if (params[0] >= 5) {
					multi_cpi = static_cast<Real64>(360) /
					            static_cast<Real64>(params[0]);
				}
			}
			if (multi_point_size == 0) {
				multi_point_size = static_cast<Real64>(10.5);
			}
			if (Param16(1) > 0) { // Set points
				multi_point_size = (static_cast<Real64>(Param16(1))) /
				                   2;
			}
			UpdateFont();
			break;

		case 0x59: // Select 120-dpi, double-speed graphics (ESC Y)
			SetupBitImage(densy, static_cast<uint16_t>(Param16(0)));
			break;

		case 0x5a: // Select 240-dpi graphics (ESC Z)
			SetupBitImage(densz, static_cast<uint16_t>(Param16(0)));
			break;

		case 0x5c: // Set relative horizontal print position (ESC \)
		{
			const int16_t toMove = static_cast<int16_t>(Param16(0));
			Real64 unitSize      = defined_unit;
			if (unitSize < 0) {
				unitSize = static_cast<Real64>(
				        print_quality == PrintQuality::Draft ? 120.0
				                                       : 180.0);
			}
			cur_x += static_cast<Real64>(
			        static_cast<Real64>(toMove) / unitSize);
		} break;

		case 0x61: // Select justification (ESC a)
			// Ignore
			break;

		case 0x63: // Set horizontal motion index (HMI) (ESC c)
			hmi = static_cast<Real64>(Param16(0)) /
			      static_cast<Real64>(360.0);
			extra_intra_space = 0.0;
			break;

		case 0x67: // Select 10.5-point, 15-cpi (ESC g)
			cpi        = 15;
			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		case 0x846: // Select forward feed mode (FS F) - set reverse not
		            // implemented yet
			if (line_spacing < 0) {
				line_spacing *= -1;
			}
			break;

		case 0x6a: // Reverse paper feed (ESC j)
		{
			Real64 reverse = static_cast<Real64>(Param16(0)) /
			                 static_cast<Real64>(216.0);
			reverse = cur_y - reverse;
			if (reverse < left_margin) {
				cur_y = left_margin;
			} else {
				cur_y = reverse;
			}
			break;
		}
		case 0x6b: // Select typeface (ESC k)
			if (params[0] <= 11 || params[0] == 30 || params[0] == 31) {
				lq_typeface = static_cast<Typeface>(params[0]);
			}
			UpdateFont();
			break;

		case 0x6c: // Set left margin (ESC l)
			left_margin = static_cast<Real64>(params[0] - 1.0) /
			              static_cast<Real64>(cpi);
			if (cur_x < left_margin) {
				cur_x = left_margin;
			}
			break;

		case 0x70: // Turn proportional mode on/off (ESC p)
			if (params[0] == 0 || params[0] == 48) {
				style.prop = 0;
			}
			if (params[0] == 1 || params[0] == 49) {
				style.prop = 1;
				print_quality = PrintQuality::Lq;
			}
			multipoint = false;
			hmi        = -1;
			UpdateFont();
			break;

		case 0x72: // Select printing color (ESC r)

			if (params[0] == 0 || params[0] > 6) {
				color = ColorBlack;
			} else {
				color = static_cast<uint8_t>(params[0] << 5);
			}
			break;

		case 0x73: // Select low-speed mode (ESC s)
			// Ignore
			break;

		case 0x74:  // Select character table (ESC t)
		case 0x849: // Select character table (FS I)
			if (params[0] < 4) {
				cur_char_table = params[0];
			}
			if (params[0] >= 48 && params[0] <= 51) {
				cur_char_table = params[0] - 48;
			}
			SelectCodepage(char_tables[cur_char_table]);
			UpdateFont();
			break;

		case 0x77: // Turn double-height printing on/off (ESC w)
			if (!multipoint) {
				if (params[0] == 0 || params[0] == 48) {
					style.doubleheight = 0;
				}
				if (params[0] == 1 || params[0] == 49) {
					style.doubleheight = 1;
				}
				UpdateFont();
			}
			break;

		case 0x78: // Select LQ or draft (ESC x)
			if (params[0] == 0 || params[0] == 48) {
				print_quality = PrintQuality::Draft;
				style.condensed = 1;
			}
			if (params[0] == 1 || params[0] == 49) {
				print_quality = PrintQuality::Lq;
				style.condensed = 0;
			}
			hmi = -1;
			UpdateFont();
			break;

		case 0x100: // Set page length in inches (ESC C NUL)
			page_height   = static_cast<Real64>(params[0]);
			bottom_margin = page_height;
			top_margin    = 0.0;
			break;

		case 0x101: // Skip unsupported ESC ( command
			needed_param = static_cast<uint8_t>(Param16(0));
			num_param    = 0;
			break;

		case 0x274: // Assign character table (ESC (t)
			// codepages has 15 entries (indices 0..14). The
			// upstream bounds check '< 16' was off by one and would
			// index out of bounds when params[3] == 15.
			if (params[2] < 4 && params[3] < 15) {
				char_tables[params[2]] = codepages[params[3]];
				// LOG_MSG("curr table: %d, p2: %d, p3:
				// %d",cur_char_table,params[2],params[3]);
				if (params[2] == cur_char_table) {
					SelectCodepage(char_tables[cur_char_table]);
				}
			}
			break;

		case 0x22d: // Select line/score (ESC (-)
			style.underline     = 0;
		style.strikethrough = 0;
		style.overscore     = 0;
			score = static_cast<ScoreType>(params[4]);
			if (score != ScoreType::None) {
				if (params[3] == 1) {
					style.underline = 1;
				}
				if (params[3] == 2) {
					style.strikethrough = 1;
				}
				if (params[3] == 3) {
					style.overscore = 1;
				}
			}
			UpdateFont();
			break;

		case 0x242: // Bar code setup and print (ESC (B)
			LOG_ERR("PRINTER: Barcode printing not supported");
			// Find out how many bytes to skip
			needed_param = static_cast<uint8_t>(Param16(0));
			num_param    = 0;
			break;

		case 0x243: // Set page length in defined unit (ESC (C)
			if (params[0] != 0 && defined_unit > 0) {
				page_height = bottom_margin = (static_cast<Real64>(
				                                      Param16(2))) *
				                              defined_unit;
				top_margin = 0.0;
			}
			break;

		case 0x255: // Set unit (ESC (U)
			defined_unit = static_cast<Real64>(params[2]) /
			               static_cast<Real64>(3600);
			break;

		case 0x256: // Set absolute vertical print position (ESC (V)
		{
			Real64 unitSize = defined_unit;
			if (unitSize < 0) {
				unitSize = static_cast<Real64>(360.0);
			}
			const Real64 newPos = top_margin +
			                      ((static_cast<Real64>(Param16(2))) *
			                       unitSize);
			if (newPos > bottom_margin) {
				NewPage(true, false);
			} else {
				cur_y = newPos;
			}
		} break;

		case 0x25e: // Print data as characters (ESC (^)
			num_print_as_char = static_cast<uint16_t>(Param16(0));
			break;

		case 0x263: // Set page format (ESC (c)
			if (defined_unit > 0) {
				Real64 newTop, newBottom;
				newTop = (static_cast<Real64>(Param16(2))) *
				         defined_unit;
				newBottom = (static_cast<Real64>(Param16(4))) *
				            defined_unit;
				if (newTop >= newBottom) {
					break;
				}
				if (newTop < page_height) {
					top_margin = newTop;
				}
				if (newBottom < page_height) {
					bottom_margin = newBottom;
				}
				if (top_margin > cur_y) {
					cur_y = top_margin;
				}
				// LOG_MSG("du %d, p1 %d, p2 %d, newtop %f,
				// newbott %f, nt %f, nb %f, ph %f",
				//	static_cast<uint64_t>(defined_unit),Param16(2),Param16(4),top_margin,bottom_margin,
				//	newTop,newBottom,page_height);
			}
			break;

		case 0x276: // Set relative vertical print position (ESC (v)
		{
			Real64 unitSize = defined_unit;
			if (unitSize < 0) {
				unitSize = static_cast<Real64>(360.0);
			}
			const Real64 newPos = cur_y +
			                      (static_cast<Real64>(static_cast<int16_t>(
			                               Param16(2))) *
			                       unitSize);
			if (newPos > top_margin) {
				if (newPos > bottom_margin) {
					NewPage(true, false);
				} else {
					cur_y = newPos;
				}
			}
		} break;

		default:
			if (esc_cmd < 0x100) {
				// LOG(LOG_MISC,LOG_WARN)
				LOG_MSG("PRINTER: Skipped unsupported command ESC %c (%02X)",
				        esc_cmd,
				        esc_cmd);
			} else {
				// LOG(LOG_MISC,LOG_WARN)
				LOG_MSG("PRINTER: Skipped unsupported command ESC ( %c (%02X)",
				        esc_cmd - 0x200,
				        esc_cmd - 0x200);
			}
		}

		esc_cmd = 0;
		return true;
	}

	switch (ch) {
	case 0x00: // NUL is ignored by the printer
		return true;
	case 0x07: // Beeper (BEL)
		// BEEEP!
		return true;
	case 0x08: // Backspace (BS)
	{
		Real64 newX = cur_x - (1 / static_cast<Real64>(act_cpi));
		if (hmi > 0) {
			newX = cur_x - hmi;
		}
		if (newX >= left_margin) {
			cur_x = newX;
		}
	}
		return true;
	case 0x09: // Tab horizontally (HT)
	{
		// Find tab right to current pos
		Real64 moveTo = -1;
		for (uint8_t i = 0; i < num_horiz_tabs; i++) {
			if (horiz_tabs[i] > cur_x) {
				moveTo = horiz_tabs[i];
			}
		}
		// Nothing found => Ignore
		if (moveTo > 0 && moveTo < right_margin) {
			cur_x = moveTo;
		}
	}
		return true;
	case 0x0b:                        // Tab vertically (VT)
		if (num_vert_tabs == 0) { // All tabs cancelled => Act like CR
			cur_x = left_margin;
		} else if (num_vert_tabs == 255) // No tabs set since reset =>
		                                 // Act like LF
		{
			cur_x = left_margin;
			cur_y += line_spacing;
			if (cur_y > bottom_margin) {
				NewPage(true, false);
			}
		} else {
			// Find tab below current pos
			Real64 moveTo = -1;
			for (uint8_t i = 0; i < num_vert_tabs; i++) {
				if (vert_tabs[i] > cur_y) {
					moveTo = vert_tabs[i];
				}
			}

			// Nothing found => Act like FF
			if (moveTo > bottom_margin || moveTo < 0) {
				NewPage(true, false);
			} else {
				cur_y = moveTo;
			}
		}
		if (style.doublewidth_oneline) {
			style.doublewidth_oneline = 0;
			UpdateFont();
		}
		return true;
	case 0x0c: // Form feed (FF)
		if (style.doublewidth_oneline) {
			style.doublewidth_oneline = 0;
			UpdateFont();
		}
		NewPage(true, true);
		return true;
	case 0x0d: // Carriage Return (CR)
		cur_x = left_margin;
		if (!auto_feed) {
			return true;
		}
		[[fallthrough]];
	case 0x0a: // Line feed
		if (style.doublewidth_oneline) {
			style.doublewidth_oneline = 0;
			UpdateFont();
		}
		cur_x = left_margin;
		cur_y += line_spacing;
		if (cur_y > bottom_margin) {
			NewPage(true, false);
		}
		return true;
	case 0x0e: // Select Real64-width printing (one line) (SO)
		if (!multipoint) {
			hmi = -1;
			style.doublewidth_oneline = 1;
			UpdateFont();
		}
		return true;
	case 0x0f: // Select condensed printing (SI)
		if (!multipoint && (cpi != 15.0)) {
			hmi = -1;
			style.condensed = 1;
			UpdateFont();
		}
		return true;
	case 0x11: // Select printer (DC1)
		// Ignore
		return true;
	case 0x12: // Cancel condensed printing (DC2)
		hmi = -1;
		style.condensed = 0;
		UpdateFont();
		return true;
	case 0x13: // Deselect printer (DC3)
		// Ignore
		return true;
	case 0x14: // Cancel double-width printing (one line) (DC4)
		hmi = -1;
		style.doublewidth_oneline = 0;
		UpdateFont();
		return true;
	case 0x18: // Cancel line (CAN)
		return true;
	case 0x1b: // ESC
		esc_seen = true;
		return true;
	case 0x1c: // FS (IBM commands)
		fs_seen = true;
		return true;
	default: return false;
	}

	return false;
}

static void PRINTER_EventHandler([[maybe_unused]] uint32_t param);

void Printer::NewPage(const bool save, const bool reset_x)
{
	PIC_RemoveEvents(PRINTER_EventHandler);
	if (printer_timeout) {
		timeout_dirty = false;
	}

	if (save) {
		OutputPage();
	}

	if (reset_x) {
		cur_x = left_margin;
	}
	cur_y = top_margin;

	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = page->w;
	rect.h = page->h;
	SDL_FillRect(page, &rect, SDL_MapRGB(page->format, 255, 255, 255));

	/*for(int i = 0; i < 256; i++)
	{
	*(static_cast<uint8_t*>(page->pixels)+i)=i;
	}*/
}

void Printer::PrintChar(uint8_t ch)
{
	char_read = true;
	if (page == nullptr) {
		return;
	}

	// Don't think that DOS programs uses this but well: Apply MSB if desired
	if (msb != 255) {
		if (msb == 0) {
			ch &= 0x7F;
		}
		if (msb == 1) {
			ch |= 0x80;
		}
	}

	// Are we currently printing a bit graphic?
	if (bit_graph.rem_bytes > 0) {
		PrintBitGraph(ch);
		return;
	}

	// Print everything?
	if (num_print_as_char > 0) {
		num_print_as_char--;
	} else if (ProcessCommandChar(ch)) {
		return;
	}

	// Do not print if no font is available
	if (!cur_font) {
		return;
	}

	if (ch == 0x1) {
		ch = 0x20;
	}

	// Find the glyph for the char to render
	FT_UInt index = FT_Get_Char_Index(cur_font, cur_map[ch]);

	// Load the glyph
	FT_Load_Glyph(cur_font, index, FT_LOAD_DEFAULT);

	// Render a high-quality bitmap
	FT_Render_Glyph(cur_font->glyph, FT_RENDER_MODE_NORMAL);

	const auto penX = static_cast<uint16_t>(PixX() + cur_font->glyph->bitmap_left);
	uint16_t penY = static_cast<uint16_t>(PixY() - cur_font->glyph->bitmap_top +
	                                      cur_font->size->metrics.ascender / 64);

	if (style.subscript) {
		penY = static_cast<uint16_t>(penY + cur_font->glyph->bitmap.rows / 2);
	}

	// Copy bitmap into page
	SDL_LockSurface(page);

	BlitGlyph(cur_font->glyph->bitmap, penX, penY, false);
	BlitGlyph(cur_font->glyph->bitmap, penX + 1, penY, true);

	// Doublestrike => Print the glyph a second time one pixel below
	if (style.doublestrike) {
		BlitGlyph(cur_font->glyph->bitmap, penX, penY + 1, true);
		BlitGlyph(cur_font->glyph->bitmap, penX + 1, penY + 1, true);
	}

	// Bold => Print the glyph a second time one pixel to the right
	// or be a bit more bold...
	if (style.bold) {
		BlitGlyph(cur_font->glyph->bitmap, penX + 1, penY, true);
		BlitGlyph(cur_font->glyph->bitmap, penX + 2, penY, true);
		BlitGlyph(cur_font->glyph->bitmap, penX + 3, penY, true);
	}
	SDL_UnlockSurface(page);

	// For line printing
	const uint16_t lineStart = static_cast<uint16_t>(PixX());

	// advance the cursor to the right
	Real64 x_advance;
	if (style.prop) {
		x_advance = static_cast<Real64>(
		        static_cast<Real64>(cur_font->glyph->advance.x) /
		        static_cast<Real64>(dpi * 64));
	} else {
		if (hmi < 0) {
			x_advance = 1 / static_cast<Real64>(act_cpi);
		} else {
			x_advance = hmi;
		}
	}
	x_advance += extra_intra_space;
	cur_x += x_advance;

	// Draw lines if desired
	if ((score != ScoreType::None) &&
	    (style.underline || style.strikethrough || style.overscore)) {
		// Find out where to put the line
		uint16_t lineY      = static_cast<uint16_t>(PixY());
		const double height = static_cast<double>(
		        cur_font->size->metrics.height >> 6); // TODO height is
		                                              // fixed point
		                                              // madness...

		if (style.underline) {
			lineY = static_cast<uint16_t>(
			        PixY() + static_cast<uint16_t>(height * 0.9));
		} else if (style.strikethrough) {
			lineY = static_cast<uint16_t>(
			        PixY() + static_cast<uint16_t>(height * 0.45));
		} else if (style.overscore) {
			lineY = static_cast<uint16_t>(
			        PixY() - (((score == ScoreType::Double) ||
			                   (score == ScoreType::DoubleBroken))
			                          ? 5
			                          : 0));
		}

		DrawLine(lineStart,
		         PixX(),
		         lineY,
		         score == ScoreType::SingleBroken || score == ScoreType::DoubleBroken);

		// draw second line if needed
		if ((score == ScoreType::Double) || (score == ScoreType::DoubleBroken)) {
			// score is DOUBLE or DOUBLEBROKEN here; the upstream
			// expression also tested ScoreType::SingleBroken which is
			// unreachable in this branch.
			DrawLine(lineStart, PixX(), lineY + 5, score == ScoreType::DoubleBroken);
		}
	}
	// If the next character would go beyond the right margin, line-wrap.
	if ((cur_x + x_advance) > right_margin) {
		cur_x = left_margin;
		cur_y += line_spacing;
		if (cur_y > bottom_margin) {
			NewPage(true, false);
		}
	}
}

// Printer::BlitGlyph and Printer::DrawLine live in printer_glyph.cpp.

void Printer::SetAutofeed(const bool feed)
{
	auto_feed = feed;
}

bool Printer::GetAutofeed()
{
	return auto_feed;
}

bool Printer::IsBusy()
{
	// We're never busy
	return false;
}

bool Printer::Ack()
{
	// Acknowledge last char read
	if (char_read) {
		char_read = false;
		return true;
	}
	return false;
}

void Printer::SetupBitImage(const uint8_t density, const uint16_t num_cols)
{
	switch (density) {
	case 0:
		bit_graph.horiz_dens   = 60;
		bit_graph.vert_dens    = 60;
		bit_graph.adjacent     = true;
		bit_graph.bytes_column = 1;
		break;

	case 1:
		bit_graph.horiz_dens   = 120;
		bit_graph.vert_dens    = 60;
		bit_graph.adjacent     = true;
		bit_graph.bytes_column = 1;
		break;

	case 2:
		bit_graph.horiz_dens   = 120;
		bit_graph.vert_dens    = 60;
		bit_graph.adjacent     = false;
		bit_graph.bytes_column = 1;
		break;

	case 3:
		bit_graph.horiz_dens   = 60;
		bit_graph.vert_dens    = 240;
		bit_graph.adjacent     = false;
		bit_graph.bytes_column = 1;
		break;

	case 4:
		bit_graph.horiz_dens   = 80;
		bit_graph.vert_dens    = 60;
		bit_graph.adjacent     = true;
		bit_graph.bytes_column = 1;
		break;

	case 6:
		bit_graph.horiz_dens   = 90;
		bit_graph.vert_dens    = 60;
		bit_graph.adjacent     = true;
		bit_graph.bytes_column = 1;
		break;

	case 32:
		bit_graph.horiz_dens   = 60;
		bit_graph.vert_dens    = 180;
		bit_graph.adjacent     = true;
		bit_graph.bytes_column = 3;
		break;

	case 33:
		bit_graph.horiz_dens   = 120;
		bit_graph.vert_dens    = 180;
		bit_graph.adjacent     = true;
		bit_graph.bytes_column = 3;
		break;

	case 38:
		bit_graph.horiz_dens   = 90;
		bit_graph.vert_dens    = 180;
		bit_graph.adjacent     = true;
		bit_graph.bytes_column = 3;
		break;

	case 39:
		bit_graph.horiz_dens   = 180;
		bit_graph.vert_dens    = 180;
		bit_graph.adjacent     = true;
		bit_graph.bytes_column = 3;
		break;

	case 40:
		bit_graph.horiz_dens   = 360;
		bit_graph.vert_dens    = 180;
		bit_graph.adjacent     = false;
		bit_graph.bytes_column = 3;
		break;

	case 71:
		bit_graph.horiz_dens   = 180;
		bit_graph.vert_dens    = 360;
		bit_graph.adjacent     = true;
		bit_graph.bytes_column = 6;
		break;

	case 72:
		bit_graph.horiz_dens   = 360;
		bit_graph.vert_dens    = 360;
		bit_graph.adjacent     = false;
		bit_graph.bytes_column = 6;
		break;

	case 73:
		bit_graph.horiz_dens   = 360;
		bit_graph.vert_dens    = 360;
		bit_graph.adjacent     = true;
		bit_graph.bytes_column = 6;
		break;

	default: LOG_ERR("PRINTER: Unsupported bit image density %i", density);
	}

	bit_graph.rem_bytes         = num_cols * bit_graph.bytes_column;
	bit_graph.read_bytes_column = 0;
}

void Printer::PrintBitGraph(const uint8_t ch)
{
	bit_graph.column[bit_graph.read_bytes_column++] = ch;
	bit_graph.rem_bytes--;

	// Only print after reading a full column
	if (bit_graph.read_bytes_column < bit_graph.bytes_column) {
		return;
	}

	const Real64 oldY = cur_y;

	SDL_LockSurface(page);

	// When page dpi is greater than graphics dpi, the drawn pixels get
	// "bigger"
	uint64_t pixsizeX = 1;
	uint64_t pixsizeY = 1;
	if (bit_graph.adjacent) {
		pixsizeX = dpi / bit_graph.horiz_dens > 0 ? dpi / bit_graph.horiz_dens
		                                          : 1;
		pixsizeY = dpi / bit_graph.vert_dens > 0 ? dpi / bit_graph.vert_dens
		                                         : 1;
	}
	// TODO figure this out for 360dpi mode in windows

	//	uint64_t pixsizeX = dpi/bit_graph.horiz_dens > 0?
	// dpi/bit_graph.horiz_dens : 1; 	uint64_t pixsizeY =
	// dpi/bit_graph.vert_dens > 0? dpi/bit_graph.vert_dens : 1;

	for (uint64_t i = 0; i < bit_graph.bytes_column; i++) // for each byte
	{
		for (uint64_t j = 128; j != 0; j >>= 1) { // for each bit
			if (bit_graph.column[i] & j) {
				for (uint64_t xx = 0; xx < pixsizeX; xx++) {
					for (uint64_t yy = 0; yy < pixsizeY; yy++) {
						if ((static_cast<int>(PixX() + xx) <
						     page->w) &&
						    (static_cast<int>(PixY() + yy) <
						     page->h)) {
							*(static_cast<uint8_t*>(
							          page->pixels) +
							  (PixX() + xx) +
							  (PixY() + yy) * page->pitch) |=
							        (color | 0x1F);
						}
					}
				}
			} // else white pixel

			cur_y += static_cast<Real64>(1) /
			         static_cast<Real64>(bit_graph.vert_dens); // TODO
			                                                   // line
			                                                   // wrap?
		}
	}

	SDL_UnlockSurface(page);

	cur_y = oldY;

	bit_graph.read_bytes_column = 0;

	// Advance to the left
	cur_x += static_cast<Real64>(1) / static_cast<Real64>(bit_graph.horiz_dens);
}

void Printer::FormFeed()
{
	// Don't output blank pages
	NewPage(!IsBlank(), true);

	FinishMultipage();
}

// Cap on how many existing <prefix><N><ext> files we'll skip past before
// giving up. Sized to match the largest sensible print job; well beyond
// anything a real DOS application will produce in one session.
static constexpr int FindNextNameAttempts = 10000;

// Defined here, declared in printer_internal.h for use from the
// output backends in printer_png.cpp / printer_ps.cpp.
std::optional<std_fs::path> find_next_name(const std::string& prefix,
                                           const std::string& ext)
{
	const std_fs::path docdir = document_path.empty() ? std_fs::path{"."}
	                                                  : document_path;
	for (int i = 1; i <= FindNextNameAttempts; ++i) {
		std_fs::path candidate = docdir /
		                         (prefix + std::to_string(i) + ext);
		std::error_code ec;
		if (!std_fs::exists(candidate, ec)) {
			return candidate;
		}
	}
	LOG_WARNING(
	        "PRINTER: docpath already contains %d %s files matching "
	        "'%s<N>%s'; output disabled",
	        FindNextNameAttempts,
	        prefix.c_str(),
	        prefix.c_str(),
	        ext.c_str());
	return std::nullopt;
}

void Printer::OutputPage()
{
	if (iequals(output, "printer")) {
		// Win32 host-printer pass-through removed (out of scope).
		LOG_MSG("PRINTER: Direct printing not supported");
	}
#ifdef C_LIBPNG
	else if (iequals(output, "png")) {
		if (const auto out_path = find_next_name("page", ".png")) {
			write_png_page(page, *out_path);
		}
	}
#endif
	else if (iequals(output, "ps")) {
		OutputPagePostScript();
	} else {
		const auto out_path = find_next_name("page", ".bmp");
		if (out_path) {
			SDL_SaveBMP(page, out_path->string().c_str());
		}
	}
}

// Printer::FprintAscii85 lives in printer_ps.cpp.

void Printer::FinishMultipage()
{
	if (!output_handle) {
		return;
	}
	if (iequals(output, "ps")) {
		fprintf(output_handle.get(), "%%%%Pages: %i\n", multipage_counter);
		fprintf(output_handle.get(), "%%%%EOF\n");
	} else if (iequals(output, "printer")) {
		// Win32 host-printer pass-through removed (out of scope).
	}
	// FILE_unique_ptr destructor fcloses on reset.
	output_handle.reset();
}

bool Printer::IsBlank()
{
	bool blank = true;

	SDL_LockSurface(page);

	for (uint16_t y = 0; y < page->h; y++) {
		for (uint16_t x = 0; x < page->w; x++) {
			if (*(static_cast<uint8_t*>(page->pixels) + x +
			      (y * page->pitch)) != 0) {
				blank = false;
			}
		}
	}

	SDL_UnlockSurface(page);
	return blank;
}

uint8_t Printer::GetPixel(const uint32_t num)
{
	// Respect the pitch
	return *(static_cast<uint8_t*>(page->pixels) + (num % page->w) +
	         ((num / page->w) * page->pitch));
}

// LPT register bit positions (see src/hardware/lpt.h for the wider
// staging definitions; we duplicate the few we use here as named
// constants rather than pulling in the unions).
namespace {

// Control register (write-only, base + 2).
constexpr uint8_t CtrlStrobe     = 0x01;
constexpr uint8_t CtrlAutoLf     = 0x02;
constexpr uint8_t CtrlInitialise = 0x04;
constexpr uint8_t CtrlReservedHi = 0xe0; // unused bits, read back as 1
// Mask used when echoing the control register back in
// PRINTER_ReadControl: keep everything except auto_lf, which the
// printer reports based on its own state.
constexpr uint8_t CtrlReadMask = 0xfd;

// Status register (read-only, base + 1).
constexpr uint8_t StatusReservedLow = 0x1f; // reserved-low + select_in
constexpr uint8_t StatusAckHigh     = 0x40;
constexpr uint8_t StatusBusyHigh    = 0x80;

// Status value when no Printer exists yet. ERROR/ACK/BUSY high
// (their inverted polarity means "no error, no ack, not busy"),
// select_in high, paper_out low, irq low.
constexpr uint8_t StatusNoPrinter = 0xdf;

// Latched copies of the LPT port registers that printer.cpp drives.
struct LptRegisters {
	uint8_t data    = 0;
	uint8_t control = CtrlInitialise;
};
LptRegisters lpt{};

} // namespace

uint64_t PRINTER_ReadData([[maybe_unused]] const uint64_t port,
                          [[maybe_unused]] const uint64_t iolen)
{
	return lpt.data;
}

void PRINTER_WriteData([[maybe_unused]] const uint64_t port, const uint64_t val,
                       [[maybe_unused]] const uint64_t iolen)
{
	lpt.data = static_cast<uint8_t>(val);
}

uint64_t PRINTER_ReadStatus([[maybe_unused]] const uint64_t port,
                            [[maybe_unused]] const uint64_t iolen)
{
	// Don't create a Printer unless the program really wants to print.
	if (!default_printer) {
		return StatusNoPrinter;
	}

	// Printer is always online and never reports an error.
	uint8_t status = StatusReservedLow;
	if (!default_printer->IsBusy()) {
		status |= StatusBusyHigh;
	}
	if (!default_printer->Ack()) {
		status |= StatusAckHigh;
	}
	return status;
}

static void trigger_form_feed(const bool pressed)
{
	if (pressed) {
		if (default_printer) {
			PIC_RemoveEvents(PRINTER_EventHandler);
			if (printer_timeout) {
				timeout_dirty = false;
			}

			default_printer->FormFeed();
		}
	}
}

static void PRINTER_EventHandler([[maybe_unused]] const uint32_t param)
{
	// LOG_MSG("printerevent");
	if (timeout_dirty) { // add another
		PIC_AddEvent(PRINTER_EventHandler,
		             static_cast<float>(printer_timeout),
		             0);
		// LOG_MSG("timeout renew");
		timeout_dirty = false;
	} else {
		timeout_dirty = false;
		trigger_form_feed(true);
	}
}

void PRINTER_WriteControl([[maybe_unused]] const uint64_t port, const uint64_t val,
                          [[maybe_unused]] const uint64_t iolen)
{
	// Rising edge on the INITIALISE bit triggers a hard reset.
	if ((val & CtrlInitialise) && default_printer &&
	    !(lpt.control & CtrlInitialise)) {
		default_printer->ResetPrinterHard();
	}

	// Data is strobed to the printer on the falling edge of the STROBE bit.
	if (!(val & CtrlStrobe) && (lpt.control & CtrlStrobe)) {
		if (!default_printer) {
			default_printer = std::make_unique<Printer>(
			        conf_dpi, conf_width, conf_height,
			        conf_output_device, conf_multipage_output);
		}
		default_printer->PrintChar(lpt.data);
		if (!timeout_dirty) {
			PIC_AddEvent(PRINTER_EventHandler,
			             static_cast<float>(printer_timeout),
			             0);
			timeout_dirty = true;
		}
	}

	lpt.control = static_cast<uint8_t>(val);
	if (default_printer) {
		default_printer->SetAutofeed((val & CtrlAutoLf) != 0);
	}
}

uint64_t PRINTER_ReadControl([[maybe_unused]] const uint64_t port,
                             [[maybe_unused]] const uint64_t iolen)
{
	// Don't create a Printer unless the program really wants to print.
	if (!default_printer) {
		return CtrlReservedHi | lpt.control;
	}
	const uint8_t auto_lf = default_printer->GetAutofeed() ? CtrlAutoLf : 0;
	return CtrlReservedHi | auto_lf | (lpt.control & CtrlReadMask);
}

// PRINTER_Shutdown used to be registered via Section_prop::AddDestroyFunction
// in the upstream PRINTER_Init. dosbox-staging drives the printer lifecycle
// from printer_glue.cpp via PRINTER_Reset, so the function is no longer
// reachable and has been removed.

// Config-reading, IO-handler installation, and mapper key binding live in
// printer_glue.cpp. This file exposes the C-style hooks that printer_glue
// drives:
//   PRINTER_Configure   -- writes the static config values (dpi, paper
//                          size, output format, etc.) that the lazy
//                          Printer constructor reads on first use.
//   PRINTER_FormFeed    -- public wrapper around the static
//                          trigger_form_feed used by the mapper
//                          handler in printer_glue.
//   PRINTER_Reset       -- destroys the Printer singleton (flushes any
//                          open multipage doc) so Destroy/Init cycles
//                          work cleanly.
//
// Lifecycle state (active / inactive) lives entirely in printer_glue's
// PrinterState; this file deliberately has no equivalent flag.

void PRINTER_Configure(const uint16_t dpi, const uint16_t width, const uint16_t height,
                       const char* docpath, const char* output_format,
                       const bool multipage, const int timeout_ms)
{
	conf_dpi      = dpi;
	conf_width    = width;
	conf_height   = height;
	document_path = docpath ? docpath : "";
	conf_output_device    = output_format ? output_format : "";
	conf_multipage_output = multipage;
	printer_timeout       = timeout_ms;
	timeout_dirty         = (printer_timeout == 0);
}

void PRINTER_FormFeed(const bool pressed)
{
	trigger_form_feed(pressed);
}

void PRINTER_Reset()
{
	// Cancel pending timeout events first so PRINTER_EventHandler can't
	// fire on a half-destroyed printer.
	PIC_RemoveEvents(PRINTER_EventHandler);
	timeout_dirty = false;
	default_printer.reset();
	// The IO handlers in printer_glue stay installed across Reset/Init
	// cycles; clear the LPT register state so a stale strobe sequence
	// from before a previous Reset can't trigger lazy construction with
	// stale data.
	lpt.data    = 0;
	lpt.control = CtrlInitialise;
}

#endif
