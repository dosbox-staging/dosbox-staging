// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

// ESC/P2 command dispatch — Printer::ProcessCommandChar plus the
// bit-image helpers (SetupBitImage, PrintBitGraph) that ESC commands
// in here drive.

#include "printer.h"

#if C_PRINTER

#include "misc/logging.h"
#include "utils/checks.h"

#include "printer_charmaps.h"

CHECK_NARROWING();

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

#endif // C_PRINTER
