// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "printer.h"

#include "config/setup.h"
#include "gui/mapper.h"
#include "misc/cross.h"
#include "misc/support.h"
#include "printer_charmaps.h"
#include "printer_if.h"
#include <math.h>
#include <vector>

#include "hardware/pic.h" // for timeout
#include "utils/checks.h"

CHECK_NARROWING();

extern void GFX_CaptureMouse(void);
extern bool mouselocked;

static CPrinter* default_printer = nullptr;

#define PARAM16(I) (params[I + 1] * 256 + params[I])
#define PIXX       (static_cast<uint64_t>(floor(cur_x * dpi + 0.5)))
#define PIXY       (static_cast<uint64_t>(floor(cur_y * dpi + 0.5)))

static uint16_t conf_dpi, conf_width, conf_height;
static uint64_t printer_timeout;
static bool timeout_dirty;
static const char* document_path;
// static const char* font_path;
static char conf_output_device[50];
static bool conf_multipage_output;

void CPrinter::FillPalette(const uint8_t red_max, const uint8_t green_max,
                           const uint8_t blue_max, uint8_t color_id, SDL_Palette* palette)
{
	const float red   = red_max / 30.9f;
	const float green = green_max / 30.9f;
	const float blue  = blue_max / 30.9f;

	const uint8_t color_mask = color_id <<= 5;

	for (int i = 0; i < 32; i++) {
		palette->colors[i + color_mask].r = static_cast<Uint8>(
		        255 - (red * static_cast<float>(i)));
		palette->colors[i + color_mask].g = static_cast<Uint8>(
		        255 - (green * static_cast<float>(i)));
		palette->colors[i + color_mask].b = static_cast<Uint8>(
		        255 - (blue * static_cast<float>(i)));
	}
}

CPrinter::CPrinter(uint16_t dpi, const uint16_t width, const uint16_t height,
                   char* output, bool multipage_output)
{
	if (FT_Init_FreeType(&ft_lib)) {
		LOG(LOG_MISC, LOG_ERROR)(
		        "PRINTER: Unable to init Freetype2. Printing disabled");
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

		color = COLOR_BLACK;

		cur_font      = nullptr;
		char_read     = false;
		auto_feed     = false;
		output_handle = nullptr;

		ResetPrinter();

		// Win32 host-printer pass-through removed (out of scope).
		LOG(LOG_MISC, LOG_NORMAL)("PRINTER: Enabled");
	}
}

void CPrinter::ResetPrinterHard()
{
	char_read = false;
	ResetPrinter();
}

void CPrinter::ResetPrinter()
{
	color = COLOR_BLACK;
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
	style                       = 0;
	extra_intra_space           = 0.0;
	print_upper_contr           = true;
	bit_graph.rem_bytes         = 0;
	densk                       = 0;
	densl                       = 1;
	densy                       = 2;
	densz                       = 3;
	char_tables[0]              = 0; // Italics
	char_tables[1] = char_tables[2] = char_tables[3] = 437;
	defined_unit                                     = -1;
	multipoint                                       = false;
	multi_point_size                                 = 0.0;
	multi_cpi                                        = 0.0;
	hmi                                              = -1.0;
	msb                                              = 255;
	num_print_as_char                                = 0;
	lq_typeface                                      = courier;

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

CPrinter::~CPrinter(void)
{
	FinishMultipage();
	if (page != nullptr) {
		SDL_FreeSurface(page);
		page = nullptr;
		FT_Done_FreeType(ft_lib);
	}
}

void CPrinter::SelectCodepage(const uint16_t codepage)
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
		LOG(LOG_MISC,
		    LOG_WARN)("Unsupported codepage %i. Using CP437 instead.", codepage);
		SelectCodepage(437);
		return;
	} /*
	 switch(cp)
	 {
	 case 0: // Italics, use cp437
	 case 437:
	         map_to_use = (uint16_t*)&cp437_map;
	         break;
	 case 737:
	         map_to_use = (uint16_t*)&cp737_map;
	         break;
	 case 775:
	         map_to_use = (uint16_t*)&cp775_map;
	         break;
	 case 850:
	         map_to_use = (uint16_t*)&cp850_map;
	         break;
	 case 852:
	         map_to_use = (uint16_t*)&cp852_map;
	         break;
	 case 855:
	         map_to_use = (uint16_t*)&cp855_map;
	         break;
	 case 857:
	         map_to_use = (uint16_t*)&cp857_map;
	         break;
	 case 860:
	         map_to_use = (uint16_t*)&cp860_map;
	         break;
	 case 861:
	         map_to_use = (uint16_t*)&cp861_map;
	         break;
	 case 863:
	         map_to_use = (uint16_t*)&cp863_map;
	         break;
	 case 864:
	         map_to_use = (uint16_t*)&cp864_map;
	         break;
	 case 865:
	         map_to_use = (uint16_t*)&cp865_map;
	         break;
	 case 866:
	         map_to_use = (uint16_t*)&cp866_map;
	         break;
	 default:
	         LOG(LOG_MISC,LOG_WARN)("Unsupported codepage %i. Using CP437
	 instead.", cp); map_to_use = (uint16_t*)&cp437_map;
	 }*/

	for (int i = 0; i < 256; i++) {
		cur_map[i] = map_to_use[i];
	}
}

void CPrinter::UpdateFont()
{
	//	char buffer[1000];

	if (cur_font != nullptr) {
		FT_Done_Face(cur_font);
	}

	// Map the Epson typeface enum to a font filename. We ship
	// Liberation Serif / Sans / Mono renamed to these slot names
	// in resources/fonts/. Users can override by dropping their
	// own TTF with the same filename into <config-dir>/fonts/.
	const char* font_filename = "roman.ttf";

	switch (lq_typeface) {
	case roman: font_filename = "roman.ttf"; break;
	case sansserif: font_filename = "sansserif.ttf"; break;
	case courier: font_filename = "courier.ttf"; break;
	case script: font_filename = "script.ttf"; break;
	case ocra:
	case ocrb: font_filename = "ocra.ttf"; break;
	default: font_filename = "roman.ttf"; break;
	}

	// get_resource_path walks the standard hierarchy (bundled
	// resources first, the user's config dir last as an override).
	const auto font_path = get_resource_path("fonts", font_filename);

	if (font_path.empty() ||
	    FT_New_Face(ft_lib, font_path.string().c_str(), 0, &cur_font)) {
		LOG_MSG("Unable to load font %s", font_filename);
		cur_font = nullptr;
	}

	Real64 horizPoints = 10.5;
	Real64 vertPoints  = 10.5;

	if (!multipoint) {
		act_cpi = cpi;
		/*
		switch(style & (STYLE_CONDENSED|STYLE_PROP)) {
		        case STYLE_CONDENSED: // only condensed
		                if (cpi == 10.0) {
		                        act_cpi = 17.14;
		                        horizPoints *= 10.0/17.14;
		                } else if(cpi == 12.0) {
		                        act_cpi = 20.0;
		                        horizPoints *= 10.0/20.0;
		                        vertPoints *= 10.0/12.0;
		                } else {
		                        // ignored
		                }
		                break;
		        case STYLE_PROP|STYLE_CONDENSED:
		                horizPoints /= 2.0;
		                break;
		        case 0: // neither
		        case STYLE_PROP: // only proportional
		                horizPoints *= 10.0/cpi;
		                vertPoints *= 10.0/cpi;
		                break;
		}
	*/
		if (!(style & STYLE_CONDENSED)) {
			horizPoints *= 10.0 / cpi;
			vertPoints *= 10.0 / cpi;
		}

		if (!(style & STYLE_PROP)) {
			if ((cpi == 10.0) && (style & STYLE_CONDENSED)) {
				act_cpi = 17.14;
				horizPoints *= 10.0 / 17.14;
			}
			if ((cpi == 12.0) && (style & STYLE_CONDENSED)) {
				act_cpi = 20.0;
				horizPoints *= 10.0 / 20.0;
				vertPoints *= 10.0 / 12.0;
			}
		} else if (style & STYLE_CONDENSED) {
			horizPoints /= 2.0;
		}

		if ((style & STYLE_DOUBLEWIDTH) ||
		    (style & STYLE_DOUBLEWIDTHONELINE)) {
			act_cpi /= 2.0;
			horizPoints *= 2.0;
		}

		if (style & STYLE_DOUBLEHEIGHT) {
			vertPoints *= 2.0;
		}
	} else { // multipoint true
		act_cpi     = multi_cpi;
		horizPoints = vertPoints = multi_point_size;
	}

	if ((style & STYLE_SUPERSCRIPT) || (style & STYLE_SUBSCRIPT)) {
		horizPoints *= 2.0 / 3.0;
		vertPoints *= 2.0 / 3.0;
		act_cpi /= 2.0 / 3.0;
	}

	FT_Set_Char_Size(cur_font,
	                 static_cast<uint16_t>(horizPoints) * 64,
	                 static_cast<uint16_t>(vertPoints) * 64,
	                 dpi,
	                 dpi);

	if (style & STYLE_ITALICS || char_tables[cur_char_table] == 0) {
		FT_Matrix matrix;
		matrix.xx = 0x10000L;
		matrix.xy = (FT_Fixed)(0.20 * 0x10000L);
		matrix.yx = 0;
		matrix.yy = 0x10000L;
		FT_Set_Transform(cur_font, &matrix, 0);
	}
}

bool CPrinter::ProcessCommandChar(const uint8_t ch)
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
			LOG(LOG_MISC,
			    LOG_ERROR)("User-defined characters not supported!");
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
				style |= STYLE_DOUBLEWIDTHONELINE;
				UpdateFont();
			}
			break;
		case 0x0f: // Select condensed printing (ESC SI)
			if (!multipoint && (cpi != 15.0)) {
				hmi = -1;
				style |= STYLE_CONDENSED;
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
				                    (print_quality == QUALITY_DRAFT
				                             ? 120
				                             : 180);
				hmi = -1;
				UpdateFont();
			}
			break;
		case 0x21: // Master select (ESC !)
			cpi = params[0] & 0x01 ? 12 : 10;

			// Reset first seven bits
			style &= 0xFF80;
			if (params[0] & 0x02) {
				style |= STYLE_PROP;
			}
			if (params[0] & 0x04) {
				style |= STYLE_CONDENSED;
			}
			if (params[0] & 0x08) {
				style |= STYLE_BOLD;
			}
			if (params[0] & 0x10) {
				style |= STYLE_DOUBLESTRIKE;
			}
			if (params[0] & 0x20) {
				style |= STYLE_DOUBLEWIDTH;
			}
			if (params[0] & 0x40) {
				style |= STYLE_ITALICS;
			}
			if (params[0] & 0x80) {
				score = SCORE_SINGLE;
				style |= STYLE_UNDERLINE;
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
			                    (static_cast<Real64>(PARAM16(0)) /
			                     unitSize);
			if (newX <= right_margin) {
				cur_x = newX;
			}
		} break;
		case 0x85a: // Print 24-bit hex-density graphics (FS Z)
			SetupBitImage(40, static_cast<uint16_t>(PARAM16(0)));
			break;
		case 0x2a: // Select bit image (ESC *)
			SetupBitImage(params[0], static_cast<uint16_t>(PARAM16(1)));
			break;
		case 0x2b:  // Set n/360-inch line spacing (ESC +)
		case 0x833: // Set n/360-inch line spacing (FS 3)
			line_spacing = static_cast<Real64>(params[0]) / 360;
			break;
		case 0x2d: // Turn underline on/off (ESC -)
			if (params[0] == 0 || params[0] == 48) {
				style &= ~STYLE_UNDERLINE;
			}
			if (params[0] == 1 || params[0] == 49) {
				style |= STYLE_UNDERLINE;
				score = SCORE_SINGLE;
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
			style |= STYLE_ITALICS;
			UpdateFont();
			break;
		case 0x35: // Cancel italic font (ESC 5)
			style &= ~STYLE_ITALICS;
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
			style |= STYLE_BOLD;
			UpdateFont();
			break;
		case 0x46: // Cancel bold font (ESC F)
			style &= ~STYLE_BOLD;
			UpdateFont();
			break;
		case 0x47: // Select dobule-strike printing (ESC G)
			style |= STYLE_DOUBLESTRIKE;
			break;
		case 0x48: // Cancel double-strike printing (ESC H)
			style &= ~STYLE_DOUBLESTRIKE;
			break;
		case 0x4a: // Advance print position vertically (ESC J n)
			cur_y += static_cast<Real64>(
			        static_cast<Real64>(params[0]) / 180);
			if (cur_y > bottom_margin) {
				NewPage(true, false);
			}
			break;
		case 0x4b: // Select 60-dpi graphics (ESC K)
			SetupBitImage(densk, static_cast<uint16_t>(PARAM16(0)));
			break;
		case 0x4c: // Select 120-dpi graphics (ESC L)
			SetupBitImage(densl, static_cast<uint16_t>(PARAM16(0)));
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
				style |= STYLE_SUBSCRIPT;
			}
			if (params[0] == 1 || params[1] == 49) {
				style |= STYLE_SUPERSCRIPT;
			}
			UpdateFont();
			break;
		case 0x54: // Cancel superscript/subscript printing (ESC T)
			style &= 0xFFFF - STYLE_SUPERSCRIPT - STYLE_SUBSCRIPT;
			UpdateFont();
			break;
		case 0x55: // Turn unidirectional mode on/off (ESC U)
			// We don't have a print head, so just ignore this
			break;
		case 0x57: // Turn double-width printing on/off (ESC W)
			if (!multipoint) {
				hmi = -1;
				if (params[0] == 0 || params[0] == 48) {
					style &= ~STYLE_DOUBLEWIDTH;
				}
				if (params[0] == 1 || params[0] == 49) {
					style |= STYLE_DOUBLEWIDTH;
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
					style |= STYLE_PROP;
				} else if (params[0] >= 5) {
					multi_cpi = static_cast<Real64>(360) /
					            static_cast<Real64>(params[0]);
				}
			}
			if (multi_point_size == 0) {
				multi_point_size = static_cast<Real64>(10.5);
			}
			if (PARAM16(1) > 0) { // Set points
				multi_point_size = (static_cast<Real64>(PARAM16(1))) /
				                   2;
			}
			UpdateFont();
			break;
		case 0x59: // Select 120-dpi, double-speed graphics (ESC Y)
			SetupBitImage(densy, static_cast<uint16_t>(PARAM16(0)));
			break;
		case 0x5a: // Select 240-dpi graphics (ESC Z)
			SetupBitImage(densz, static_cast<uint16_t>(PARAM16(0)));
			break;
		case 0x5c: // Set relative horizontal print position (ESC \)
		{
			const int16_t toMove = static_cast<int16_t>(PARAM16(0));
			Real64 unitSize      = defined_unit;
			if (unitSize < 0) {
				unitSize = static_cast<Real64>(
				        print_quality == QUALITY_DRAFT ? 120.0
				                                       : 180.0);
			}
			cur_x += static_cast<Real64>(
			        static_cast<Real64>(toMove) / unitSize);
		} break;
		case 0x61: // Select justification (ESC a)
			// Ignore
			break;
		case 0x63: // Set horizontal motion index (HMI) (ESC c)
			hmi = static_cast<Real64>(PARAM16(0)) /
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
			Real64 reverse = static_cast<Real64>(PARAM16(0)) /
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
				lq_typeface = (Typeface)params[0];
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
				style &= (0xffff - STYLE_PROP);
			}
			if (params[0] == 1 || params[0] == 49) {
				style |= STYLE_PROP;
				print_quality = QUALITY_LQ;
			}
			multipoint = false;
			hmi        = -1;
			UpdateFont();
			break;
		case 0x72: // Select printing color (ESC r)

			if (params[0] == 0 || params[0] > 6) {
				color = COLOR_BLACK;
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
					style &= ~STYLE_DOUBLEHEIGHT;
				}
				if (params[0] == 1 || params[0] == 49) {
					style |= STYLE_DOUBLEHEIGHT;
				}
				UpdateFont();
			}
			break;
		case 0x78: // Select LQ or draft (ESC x)
			if (params[0] == 0 || params[0] == 48) {
				print_quality = QUALITY_DRAFT;
				style |= STYLE_CONDENSED;
			}
			if (params[0] == 1 || params[0] == 49) {
				print_quality = QUALITY_LQ;
				style &= ~STYLE_CONDENSED;
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
			needed_param = static_cast<uint8_t>(PARAM16(0));
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
			style &= ~(STYLE_UNDERLINE | STYLE_STRIKETHROUGH |
			           STYLE_OVERSCORE);
			score = params[4];
			if (score) {
				if (params[3] == 1) {
					style |= STYLE_UNDERLINE;
				}
				if (params[3] == 2) {
					style |= STYLE_STRIKETHROUGH;
				}
				if (params[3] == 3) {
					style |= STYLE_OVERSCORE;
				}
			}
			UpdateFont();
			break;
		case 0x242: // Bar code setup and print (ESC (B)
			LOG(LOG_MISC,
			    LOG_ERROR)("PRINTER: Bardcode printing not supported");
			// Find out how many bytes to skip
			needed_param = static_cast<uint8_t>(PARAM16(0));
			num_param    = 0;
			break;
		case 0x243: // Set page length in defined unit (ESC (C)
			if (params[0] != 0 && defined_unit > 0) {
				page_height = bottom_margin = (static_cast<Real64>(
				                                      PARAM16(2))) *
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
			                      ((static_cast<Real64>(PARAM16(2))) *
			                       unitSize);
			if (newPos > bottom_margin) {
				NewPage(true, false);
			} else {
				cur_y = newPos;
			}
		} break;
		case 0x25e: // Print data as characters (ESC (^)
			num_print_as_char = static_cast<uint16_t>(PARAM16(0));
			break;
		case 0x263: // Set page format (ESC (c)
			if (defined_unit > 0) {
				Real64 newTop, newBottom;
				newTop = (static_cast<Real64>(PARAM16(2))) *
				         defined_unit;
				newBottom = (static_cast<Real64>(PARAM16(4))) *
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
				//	static_cast<uint64_t>(defined_unit),PARAM16(2),PARAM16(4),top_margin,bottom_margin,
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
			                               PARAM16(2))) *
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
		if (style & STYLE_DOUBLEWIDTHONELINE) {
			style &= 0xFFFF - STYLE_DOUBLEWIDTHONELINE;
			UpdateFont();
		}
		return true;
	case 0x0c: // Form feed (FF)
		if (style & STYLE_DOUBLEWIDTHONELINE) {
			style &= 0xFFFF - STYLE_DOUBLEWIDTHONELINE;
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
		if (style & STYLE_DOUBLEWIDTHONELINE) {
			style &= 0xFFFF - STYLE_DOUBLEWIDTHONELINE;
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
			style |= STYLE_DOUBLEWIDTHONELINE;
			UpdateFont();
		}
		return true;
	case 0x0f: // Select condensed printing (SI)
		if (!multipoint && (cpi != 15.0)) {
			hmi = -1;
			style |= STYLE_CONDENSED;
			UpdateFont();
		}
		return true;
	case 0x11: // Select printer (DC1)
		// Ignore
		return true;
	case 0x12: // Cancel condensed printing (DC2)
		hmi = -1;
		style &= ~STYLE_CONDENSED;
		UpdateFont();
		return true;
	case 0x13: // Deselect printer (DC3)
		// Ignore
		return true;
	case 0x14: // Cancel double-width printing (one line) (DC4)
		hmi = -1;
		style &= ~STYLE_DOUBLEWIDTHONELINE;
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

void CPrinter::NewPage(const bool save, const bool reset_x)
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

void CPrinter::PrintChar(uint8_t ch)
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

	const auto penX = static_cast<uint16_t>(PIXX + cur_font->glyph->bitmap_left);
	uint16_t penY = static_cast<uint16_t>(PIXY - cur_font->glyph->bitmap_top +
	                                      cur_font->size->metrics.ascender / 64);

	if (style & STYLE_SUBSCRIPT) {
		penY = static_cast<uint16_t>(penY + cur_font->glyph->bitmap.rows / 2);
	}

	// Copy bitmap into page
	SDL_LockSurface(page);

	BlitGlyph(cur_font->glyph->bitmap, penX, penY, false);
	BlitGlyph(cur_font->glyph->bitmap, penX + 1, penY, true);

	// Doublestrike => Print the glyph a second time one pixel below
	if (style & STYLE_DOUBLESTRIKE) {
		BlitGlyph(cur_font->glyph->bitmap, penX, penY + 1, true);
		BlitGlyph(cur_font->glyph->bitmap, penX + 1, penY + 1, true);
	}

	// Bold => Print the glyph a second time one pixel to the right
	// or be a bit more bold...
	if (style & STYLE_BOLD) {
		BlitGlyph(cur_font->glyph->bitmap, penX + 1, penY, true);
		BlitGlyph(cur_font->glyph->bitmap, penX + 2, penY, true);
		BlitGlyph(cur_font->glyph->bitmap, penX + 3, penY, true);
	}
	SDL_UnlockSurface(page);

	// For line printing
	const uint16_t lineStart = static_cast<uint16_t>(PIXX);

	// advance the cursor to the right
	Real64 x_advance;
	if (style & STYLE_PROP) {
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
	if ((score != SCORE_NONE) &&
	    (style & (STYLE_UNDERLINE | STYLE_STRIKETHROUGH | STYLE_OVERSCORE))) {
		// Find out where to put the line
		uint16_t lineY      = static_cast<uint16_t>(PIXY);
		const double height = static_cast<double>(
		        cur_font->size->metrics.height >> 6); // TODO height is
		                                              // fixed point
		                                              // madness...

		if (style & STYLE_UNDERLINE) {
			lineY = static_cast<uint16_t>(
			        PIXY + static_cast<uint16_t>(height * 0.9));
		} else if (style & STYLE_STRIKETHROUGH) {
			lineY = static_cast<uint16_t>(
			        PIXY + static_cast<uint16_t>(height * 0.45));
		} else if (style & STYLE_OVERSCORE) {
			lineY = static_cast<uint16_t>(
			        PIXY - (((score == SCORE_DOUBLE) ||
			                 (score == SCORE_DOUBLEBROKEN))
			                        ? 5
			                        : 0));
		}

		DrawLine(lineStart,
		         PIXX,
		         lineY,
		         score == SCORE_SINGLEBROKEN || score == SCORE_DOUBLEBROKEN);

		// draw second line if needed
		if ((score == SCORE_DOUBLE) || (score == SCORE_DOUBLEBROKEN)) {
			// score is DOUBLE or DOUBLEBROKEN here; the upstream
			// expression also tested SCORE_SINGLEBROKEN which is
			// unreachable in this branch.
			DrawLine(lineStart, PIXX, lineY + 5, score == SCORE_DOUBLEBROKEN);
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

void CPrinter::BlitGlyph(const FT_Bitmap bitmap, const uint16_t destx,
                         const uint16_t desty, const bool add)
{
	for (uint64_t y = 0; y < bitmap.rows; y++) {
		for (uint64_t x = 0; x < bitmap.width; x++) {
			// Read pixel from glyph bitmap
			uint8_t source = *(bitmap.buffer + x + y * bitmap.pitch);

			// Ignore background and don't go over the border
			if (source > 0 && (static_cast<int>(destx + x) < page->w) &&
			    (static_cast<int>(desty + y) < page->h)) {
				uint8_t* target = static_cast<uint8_t*>(page->pixels) +
				                  (x + destx) +
				                  (y + desty) * page->pitch;
				source >>= 3;

				if (add) {
					if (((*target) & 0x1f) + source > 31) {
						*target |= (color | 0x1f);
					} else {
						*target += source;
						*target |= color;
					}
				} else {
					*target = source | color;
				}
			}
		}
	}
}

void CPrinter::DrawLine(const uint64_t fromx, const uint64_t tox,
                        const uint64_t y, const bool broken)
{
	SDL_LockSurface(page);

	const uint64_t breakmod = dpi / 15;
	const uint64_t gapstart = (breakmod * 4) / 5;

	// Draw anti-aliased line
	for (uint64_t x = fromx; x <= tox; x++) {
		// Skip parts if broken line or going over the border
		if ((!broken || (x % breakmod <= gapstart)) &&
		    (static_cast<int>(x) < page->w)) {
			if (y > 0 && static_cast<int>(y - 1) < page->h) {
				*(static_cast<uint8_t*>(page->pixels) + x +
				  (y - 1) * page->pitch) = 240;
			}
			if (static_cast<int>(y) < page->h) {
				*(static_cast<uint8_t*>(page->pixels) + x +
				  y * page->pitch) = !broken ? 255 : 240;
			}
			if (static_cast<int>(y + 1) < page->h) {
				*(static_cast<uint8_t*>(page->pixels) + x +
				  (y + 1) * page->pitch) = 240;
			}
		}
	}
	SDL_UnlockSurface(page);
}

void CPrinter::SetAutofeed(const bool feed)
{
	auto_feed = feed;
}

bool CPrinter::GetAutofeed()
{
	return auto_feed;
}

bool CPrinter::IsBusy()
{
	// We're never busy
	return false;
}

bool CPrinter::Ack()
{
	// Acknowledge last char read
	if (char_read) {
		char_read = false;
		return true;
	}
	return false;
}

void CPrinter::SetupBitImage(const uint8_t density, const uint16_t num_cols)
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
	default:
		LOG(LOG_MISC,
		    LOG_ERROR)("PRINTER: Unsupported bit image density %i", density);
	}

	bit_graph.rem_bytes         = num_cols * bit_graph.bytes_column;
	bit_graph.read_bytes_column = 0;
}

void CPrinter::PrintBitGraph(const uint8_t ch)
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
	//dpi/bit_graph.horiz_dens : 1; 	uint64_t pixsizeY =
	//dpi/bit_graph.vert_dens > 0? dpi/bit_graph.vert_dens : 1;

	for (uint64_t i = 0; i < bit_graph.bytes_column; i++) // for each byte
	{
		for (uint64_t j = 128; j != 0; j >>= 1) { // for each bit
			if (bit_graph.column[i] & j) {
				for (uint64_t xx = 0; xx < pixsizeX; xx++) {
					for (uint64_t yy = 0; yy < pixsizeY; yy++) {
						if ((static_cast<int>(PIXX + xx) <
						     page->w) &&
						    (static_cast<int>(PIXY + yy) <
						     page->h)) {
							*(static_cast<uint8_t*>(
							          page->pixels) +
							  (PIXX + xx) +
							  (PIXY + yy) * page->pitch) |=
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

void CPrinter::FormFeed()
{
	// Don't output blank pages
	NewPage(!IsBlank(), true);

	FinishMultipage();
}

// Cap on how many existing page<N>.<ext> files we'll skip past before
// giving up. Sized to match the largest sensible print job; well beyond
// anything a real DOS application will produce in one session.
static constexpr int FindNextNameAttempts = 10000;

// Size of the caller-owned fname buffer (CPrinter::OutputPage's stack
// array). Keep in sync with that declaration.
static constexpr size_t FnameBufSize = 200;

#ifdef WIN32
constexpr char PathSep = '\\';
#else
constexpr char PathSep = '/';
#endif

static void find_next_name(const char* front, const char* ext, char* fname)
{
	FILE* test = nullptr;
	for (int i = 1; i <= FindNextNameAttempts; ++i) {
		const int written = snprintf(fname,
		                             FnameBufSize,
		                             "%s%c%s%d%s",
		                             document_path,
		                             PathSep,
		                             front,
		                             i,
		                             ext);
		if (written < 0 || static_cast<size_t>(written) >= FnameBufSize) {
			LOG_WARNING(
			        "PRINTER: page filename for docpath '%s' "
			        "exceeds %zu chars; page output disabled",
			        document_path,
			        FnameBufSize);
			fname[0] = 0;
			return;
		}
		test = fopen(fname, "rb");
		if (test == nullptr) {
			return; // free slot
		}
		fclose(test);
	}
	LOG_WARNING(
	        "PRINTER: docpath already contains %d %s files matching "
	        "'%s<N>%s'; overwriting the last one",
	        FindNextNameAttempts,
	        front,
	        front,
	        ext);
	// fname holds the final attempt; let the caller overwrite it.
}

void CPrinter::OutputPage()
{
	char fname[200];

	if (strcasecmp(output, "printer") == 0) {
		// Win32 host-printer pass-through removed (out of scope).
		LOG_MSG("PRINTER: Direct printing not supported");
	}
	else if (strcasecmp(output, "png") == 0) {
		// Find a page that does not exists
		find_next_name("page", ".png", &fname[0]);

		png_structp png_ptr;
		png_infop info_ptr;
		png_color palette[256];
		uint64_t i;

		/* Open the actual file */
		FILE* fp = fopen(fname, "wb");
		if (!fp) {
			LOG(LOG_MISC,
			    LOG_ERROR)("PRINTER: Can't open file %s for printer output",
			               fname);
			return;
		}

		/* First try to alloacte the png structures */
		png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		                                  nullptr,
		                                  nullptr,
		                                  nullptr);
		if (!png_ptr) {
			fclose(fp);
			return;
		}
		info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr) {
			png_destroy_write_struct(&png_ptr,
			                         static_cast<png_infopp>(nullptr));
			fclose(fp);
			return;
		}

		/* Finalize the initing of png library */
		png_init_io(png_ptr, fp);
		png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);

		/* set other zlib parameters */
		png_set_compression_mem_level(png_ptr, 8);
		png_set_compression_strategy(png_ptr, Z_DEFAULT_STRATEGY);
		png_set_compression_window_bits(png_ptr, 15);
		png_set_compression_method(png_ptr, 8);
		png_set_compression_buffer_size(png_ptr, 8192);

		png_set_IHDR(png_ptr,
		             info_ptr,
		             page->w,
		             page->h,
		             8,
		             PNG_COLOR_TYPE_PALETTE,
		             PNG_INTERLACE_NONE,
		             PNG_COMPRESSION_TYPE_DEFAULT,
		             PNG_FILTER_TYPE_DEFAULT);
		for (i = 0; i < 256; i++) {
			palette[i].red   = page->format->palette->colors[i].r;
			palette[i].green = page->format->palette->colors[i].g;
			palette[i].blue  = page->format->palette->colors[i].b;
		}
		png_set_PLTE(png_ptr, info_ptr, palette, 256);

		SDL_LockSurface(page);

		// Array of scanline pointers, owned by std::vector so we don't
		// need to manually free it before returning.
		std::vector<png_bytep> row_pointers(page->h);
		for (i = 0; static_cast<int>(i) < page->h; i++) {
			row_pointers[i] = static_cast<uint8_t*>(page->pixels) +
			                  (i * page->pitch);
		}

		// tell the png library what to encode.
		png_set_rows(png_ptr, info_ptr, row_pointers.data());

		// Write image to file
		png_write_png(png_ptr, info_ptr, 0, nullptr);

		SDL_UnlockSurface(page);

		fclose(fp);
		png_destroy_write_struct(&png_ptr, &info_ptr);
	}
	else if (strcasecmp(output, "ps") == 0) {
		FILE* psfile = nullptr;

		// Continue postscript file?
		if (output_handle != nullptr) {
			psfile = output_handle;
		}

		// Create new file?
		if (psfile == nullptr) {
			if (!multipage_output) {
				find_next_name("page", ".ps", &fname[0]);
			} else {
				find_next_name("doc", ".ps", &fname[0]);
			}

			psfile = fopen(fname, "wb");
			if (!psfile) {
				LOG(LOG_MISC,
				    LOG_ERROR)("PRINTER: Can't open file %s for printer output",
				               fname);
				return;
			}

			// Print header
			fprintf(psfile, "%%!PS-Adobe-3.0\n");
			fprintf(psfile, "%%%%Pages: (atend)\n");
			fprintf(psfile,
			        "%%%%BoundingBox: 0 0 %i %i\n",
			        static_cast<uint16_t>(default_page_width * 72),
			        static_cast<uint16_t>(default_page_height * 72));
			fprintf(psfile, "%%%%Creator: DOSBOX Virtual Printer\n");
			fprintf(psfile, "%%%%DocumentData: Clean7Bit\n");
			fprintf(psfile, "%%%%LanguageLevel: 2\n");
			fprintf(psfile, "%%%%EndComments\n");
			multipage_counter = 1;
		}

		fprintf(psfile, "%%%%Page: %i %i\n", multipage_counter, multipage_counter);
		fprintf(psfile,
		        "%i %i scale\n",
		        static_cast<uint16_t>(default_page_width * 72),
		        static_cast<uint16_t>(default_page_height * 72));
		fprintf(psfile,
		        "%i %i 8 [%i 0 0 -%i 0 %i]\n",
		        page->w,
		        page->h,
		        page->w,
		        page->h,
		        page->h);
		fprintf(psfile, "currentfile\n");
		fprintf(psfile, "/ASCII85Decode filter\n");
		fprintf(psfile, "/RunLengthDecode filter\n");
		fprintf(psfile, "image\n");

		SDL_LockSurface(page);

		uint32_t pix          = 0;
		const uint32_t numpix = page->h * page->w;
		ascii85_buffer_pos = ascii85_cur_col = 0;

		while (pix < numpix) {
			// Compress data using RLE

			if ((pix < numpix - 2) &&
			    (GetPixel(pix) == GetPixel(pix + 1)) &&
			    (GetPixel(pix) == GetPixel(pix + 2))) {
				// Found three or more pixels with the same color
				uint8_t sameCount = 3;
				const uint8_t col = GetPixel(pix);
				while (sameCount < 128 && sameCount + pix < numpix &&
				       col == GetPixel(pix + sameCount)) {
					sameCount++;
				}

				FprintAscii85(psfile, 257 - sameCount);
				FprintAscii85(psfile, 255 - col);

				// Skip ahead
				pix += sameCount;
			} else {
				// Find end of heterogenous area
				uint8_t diffCount = 1;
				while (diffCount < 128 && diffCount + pix < numpix &&
				       ((diffCount + pix < numpix - 2) ||
				        (GetPixel(pix + diffCount) !=
				         GetPixel(pix + diffCount + 1)) ||
				        (GetPixel(pix + diffCount) !=
				         GetPixel(pix + diffCount + 2)))) {
					diffCount++;
				}

				FprintAscii85(psfile, diffCount - 1);
				for (uint8_t i = 0; i < diffCount; i++) {
					FprintAscii85(psfile, 255 - GetPixel(pix++));
				}
			}
		}

		// Write EOD for RLE and ASCII85
		FprintAscii85(psfile, 128);
		FprintAscii85(psfile, 256);

		SDL_UnlockSurface(page);

		fprintf(psfile, "showpage\n");

		if (multipage_output) {
			multipage_counter++;
			output_handle = psfile;
		} else {
			fprintf(psfile, "%%%%Pages: 1\n");
			fprintf(psfile, "%%%%EOF\n");
			fclose(psfile);
			output_handle = nullptr;
		}
	} else {
		// Find a page that does not exists
		find_next_name("page", ".bmp", &fname[0]);
		SDL_SaveBMP(page, fname);
	}
}

void CPrinter::FprintAscii85(FILE* file, uint16_t byte)
{
	if (byte != 256) {
		if (byte < 256) {
			ascii85_buffer[ascii85_buffer_pos++] = static_cast<uint8_t>(byte);
		}

		if (ascii85_buffer_pos == 4 || byte == 257) {
			uint32_t num = static_cast<uint32_t>(ascii85_buffer[0])
			                    << 24 |
			               static_cast<uint32_t>(ascii85_buffer[1])
			                       << 16 |
			               static_cast<uint32_t>(ascii85_buffer[2]) << 8 |
			               static_cast<uint32_t>(ascii85_buffer[3]);

			// Deal with special case
			if (num == 0 && byte != 257) {
				fprintf(file, "z");
				if (++ascii85_cur_col >= 79) {
					ascii85_cur_col = 0;
					fprintf(file, "\n");
				}
			} else {
				char buffer[5];
				for (int8_t i = 4; i >= 0; i--) {
					buffer[i] = static_cast<uint8_t>(
					        static_cast<uint32_t>(num) %
					        static_cast<uint32_t>(85));
					buffer[i] += 33;
					num /= static_cast<uint32_t>(85);
				}

				// Make sure a line never starts with a % (which
				// may be mistaken as start of a comment)
				if (ascii85_cur_col == 0 && buffer[0] == '%') {
					fprintf(file, " ");
				}

				for (int i = 0;
				     i < ((byte != 257) ? 5 : ascii85_buffer_pos + 1);
				     i++) {
					fprintf(file, "%c", buffer[i]);
					if (++ascii85_cur_col >= 79) {
						ascii85_cur_col = 0;
						fprintf(file, "\n");
					}
				}
			}

			ascii85_buffer_pos = 0;
		}

	} else // Close string
	{
		// Partial tupel if there are still bytes in the buffer
		if (ascii85_buffer_pos > 0) {
			for (uint8_t i = ascii85_buffer_pos; i < 4; i++) {
				ascii85_buffer[i] = 0;
			}

			FprintAscii85(file, 257);
		}

		fprintf(file, "~");
		fprintf(file, ">\n");
	}
}

void CPrinter::FinishMultipage()
{
	if (output_handle != nullptr) {
		if (strcasecmp(output, "ps") == 0) {
			FILE* psfile = output_handle;
			fprintf(psfile, "%%%%Pages: %i\n", multipage_counter);
			fprintf(psfile, "%%%%EOF\n");
			fclose(psfile);
		} else if (strcasecmp(output, "printer") == 0) {
			// Win32 host-printer pass-through removed (out of scope).
		}
		output_handle = nullptr;
	}
}

bool CPrinter::IsBlank()
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

uint8_t CPrinter::GetPixel(const uint32_t num)
{
	// Respect the pitch
	return *(static_cast<uint8_t*>(page->pixels) + (num % page->w) +
	         ((num / page->w) * page->pitch));
}

// LPT register bit positions (see src/hardware/parallelport/lpt.h for the wider
// staging definitions; we duplicate the few we use here as named
// constants rather than pulling in the unions).
namespace {

// Control register (write-only, base + 2).
constexpr uint8_t CtrlStrobe     = 0x01;
constexpr uint8_t CtrlAutoLf     = 0x02;
constexpr uint8_t CtrlInitialise = 0x04;
// unused bits, read back as 1
constexpr uint8_t CtrlReservedHi = 0xe0;

// Mask used when echoing the control register back in
// PRINTER_ReadControl: keep everything except auto_lf, which the
// printer reports based on its own state.
constexpr uint8_t CtrlReadMask = 0xfd;

// Status register (read-only, base + 1)
// reserved-low + select_in
constexpr uint8_t StatusReservedLow = 0x1f;
constexpr uint8_t StatusAckHigh     = 0x40;
constexpr uint8_t StatusBusyHigh    = 0x80;

// Status value when no CPrinter exists yet. ERROR/ACK/BUSY high
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
	// Don't create a CPrinter unless the program really wants to print.
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
			default_printer = new CPrinter(conf_dpi,
			                               conf_width,
			                               conf_height,
			                               conf_output_device,
			                               conf_multipage_output);
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
	// Don't create a CPrinter unless the program really wants to print.
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
//                          CPrinter constructor reads on first use.
//   PRINTER_FormFeed    -- public wrapper around the static
//                          trigger_form_feed used by the mapper
//                          handler in printer_glue.
//   PRINTER_Reset       -- destroys the CPrinter singleton (flushes any
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
	document_path = docpath;
	strncpy(&conf_output_device[0], output_format, sizeof(conf_output_device) - 1);
	conf_output_device[sizeof(conf_output_device) - 1] = 0;
	conf_multipage_output                              = multipage;
	printer_timeout                                    = timeout_ms;
	timeout_dirty = (printer_timeout == 0);
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
	if (default_printer) {
		delete default_printer;
		default_printer = nullptr;
	}
	// The IO handlers in printer_glue stay installed across Reset/Init
	// cycles; clear the LPT register state so a stale strobe sequence
	// from before a previous Reset can't trigger lazy construction with
	// stale data.
	lpt.data    = 0;
	lpt.control = CtrlInitialise;
}
