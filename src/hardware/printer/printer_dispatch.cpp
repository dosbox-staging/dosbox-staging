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
		// Undocumented
		case 0x02:
		// (ESC LF) — Reverse line feed
		case 0x0a:
		// (ESC FF) — Return to top of current page
		case 0x0c:
		// (ESC SO) — Select double-width printing (one line)
		case 0x0e:
		// (ESC SI) — Select condensed printing
		case 0x0f:
		// (ESC #) — Cancel MSB control
		case 0x23:
		// (ESC 0) — Select 1/8-inch line spacing
		case 0x30:
		// (ESC 1) — Select 7/60-inch line spacing
		case 0x31:
		// (ESC 2) — Select 1/6-inch line spacing
		case 0x32:
		// (ESC 4) — Select italic font
		case 0x34:
		// (ESC 5) — Cancel italic font
		case 0x35:
		// (ESC 6) — Enable printing of upper control codes
		case 0x36:
		// (ESC 7) — Enable upper control codes
		case 0x37:
		// (ESC 8) — Disable paper-out detector
		case 0x38:
		// (ESC 9) — Enable paper-out detector
		case 0x39:
		// (ESC <) — Unidirectional mode (one line)
		case 0x3c:
		// (ESC =) — Set MSB to 0
		case 0x3d:
		// (ESC >) — Set MSB to 1
		case 0x3e:
		// (ESC @) — Initialize printer
		case 0x40:
		// (ESC E) — Select bold font
		case 0x45:
		// (ESC F) — Cancel bold font
		case 0x46:
		// (ESC G) — Select double-strike printing
		case 0x47:
		// (ESC H) — Cancel double-strike printing
		case 0x48:
		// (ESC M) — Select 10.5-point, 12-cpi
		case 0x4d:
		// (ESC O) — Cancel bottom margin [conflict]
		case 0x4f:
		// (ESC P) — Select 10.5-point, 10-cpi
		case 0x50:
		// (ESC T) — Cancel superscript/subscript printing
		case 0x54:
		// (ESC ^) — Enable printing of all character codes on next
		// character
		case 0x5e:
		// (ESC g) — Select 10.5-point, 15-cpi
		case 0x67:

		// (FS 4)	(= ESC 4) — Select italic font
		case 0x834:
		// (FS 5)	(= ESC 5) — Cancel italic font
		case 0x835:
		// (FS F) — Select forward feed mode
		case 0x846:
		// (FS R) — Select reverse feed mode
		case 0x852: needed_param = 0; break;

		// (ESC EM) — Control paper loading/ejecting
		case 0x19:
		// (ESC SP) — Set intercharacter space
		case 0x20:
		// (ESC !) — Master select
		case 0x21:
		// (ESC +) — Set n/360-inch line spacing
		case 0x2b:
		// (ESC -) — Turn underline on/off
		case 0x2d:
		// (ESC /) — Select vertical tab channel
		case 0x2f:
		// (ESC 3) — Set n/180-inch line spacing
		case 0x33:
		// (ESC A) — Set n/60-inch line spacing
		case 0x41:
		// (ESC C) — Set page length in lines
		case 0x43:
		// (ESC I) — Select character type and print pitch
		case 0x49:
		// (ESC J) — Advance print position vertically
		case 0x4a:
		// (ESC N) — Set bottom margin
		case 0x4e:
		// (ESC Q) — Set right margin
		case 0x51:
		// (ESC R) — Select an international character set
		case 0x52:
		// (ESC S) — Select superscript/subscript printing
		case 0x53:
		// (ESC U) — Turn unidirectional mode on/off
		case 0x55:
		// case 0x56: // Repeat data
		// (ESC V)
		// (ESC W) — Turn double-width printing on/off
		case 0x57:
		// (ESC a) — Select justification
		case 0x61:
		// (ESC f) — Absolute horizontal tab in columns [conflict]
		case 0x66:
		// (ESC h) — Select double or quadruple size
		case 0x68:
		// (ESC i) — Immediate print
		case 0x69:
		// (ESC j) — Reverse paper feed
		case 0x6a:
		// (ESC k) — Select typeface
		case 0x6b:
		// (ESC 1) — Set left margin
		case 0x6c:
		// (ESC p) — Turn proportional mode on/off
		case 0x70:
		// (ESC r) — Select printing color
		case 0x72:
		// (ESC s) — Low-speed mode on/off
		case 0x73:
		// (ESC t) — Select character table
		case 0x74:
		// (ESC w) — Turn double-height printing on/off
		case 0x77:
		// (ESC x) — Select LQ or draft
		case 0x78:
		// (ESC ~) — Select/Deselect slash zero
		case 0x7e:

		// (FS 2)	(= ESC 2) — Select 1/6-inch line spacing
		case 0x832:
		// (FS 3)	(= ESC +) — Set n/360-inch line spacing
		case 0x833:
		// (FS A)	(= ESC A) — Set n/60-inch line spacing
		case 0x841:
		// (FS C)	(= ESC k) — Select LQ type style
		case 0x843:
		// (FS E) — Select character width
		case 0x845:
		// (FS I)	(= ESC t) — Select character table
		case 0x849:
		// (FS S) — Select High Speed/High Density elite pitch
		case 0x853:
		// (FS V)	(= ESC w) — Turn double-height printing on/off
		case 0x856: needed_param = 1; break;

		// (ESC $) — Set absolute horizontal print position
		case 0x24:
		// (ESC ?) — Reassign bit-image mode
		case 0x3f:
		// (ESC K) — Select 60-dpi graphics
		case 0x4b:
		// (ESC L) — Select 120-dpi graphics
		case 0x4c:
		// (ESC Y) — Select 120-dpi, double-speed graphics
		case 0x59:
		// (ESC Z) — Select 240-dpi graphics
		case 0x5a:
		// (ESC \) — Set relative horizontal print position
		case 0x5c:
		// (ESC c) — Set horizontal motion index (HMI)	[conflict]
		case 0x63:
		// (ESC e) — Set vertical tab stops every n lines
		case 0x65:
		// (FS Z) — Print 24-bit hex-density graphics
		case 0x85a: needed_param = 2; break;

		// (ESC *) — Select bit image
		case 0x2a:
		// (ESC X) — Select font by pitch and point [conflict]
		case 0x58: needed_param = 3; break;

		// Select character height, width, line spacing
		case 0x5b: needed_param = 7; break;

		// (ESC b) — Set vertical tabs in VFU channels
		case 0x62:
		// (ESC B) — Set vertical tabs
		case 0x42: num_vert_tabs = 0; return true;

		// (ESC D) — Set horizontal tabs
		case 0x44: num_horiz_tabs = 0; return true;

		// (ESC %) — Select user-defined set
		case 0x25:
		// (ESC &) — Define user-defined characters
		case 0x26:
		// (ESC :) — Copy ROM to RAM
		case 0x3a:
			LOG_ERR("PRINTER: User-defined characters not supported");
			return true;

		// Two bytes sequence
		case 0x28: return true;

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
		// Bar code setup and print (ESC (B)
		case 0x242:
		// Print data as characters (ESC (^)
		case 0x25e: needed_param = 2; break;

		// Set unit (ESC (U)
		case 0x255: needed_param = 3; break;

		// Set page length in defined unit (ESC (C)
		case 0x243:
		// Set absolute vertical print position (ESC (V)
		case 0x256:
		// Set relative vertical print position (ESC (v)
		case 0x276: needed_param = 4; break;

		// Assign character table (ESC (t)
		case 0x274:
		// Select line/score (ESC (-)
		case 0x22d: needed_param = 5; break;

		// Set page format (ESC (c)
		case 0x263: needed_param = 6; break;

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
		// Undocumented
		case 0x02:
			// Ignore
			break;

		// Select double-width printing (one line) (ESC SO)
		case 0x0e:
			if (!multipoint) {
				hmi                       = -1;
				style.doublewidth_oneline = 1;
				UpdateFont();
			}
			break;

		// Select condensed printing (ESC SI)
		case 0x0f:
			if (!multipoint && (cpi != 15.0)) {
				hmi             = -1;
				style.condensed = 1;
				UpdateFont();
			}
			break;

		// Control paper loading/ejecting (ESC EM)
		case 0x19:
			// We are not really loading paper, so most commands can
			// be ignored
			if (params[0] == 'R') {
				// TODO confirm reset_x semantics here.
				NewPage(true, false);
			}
			break;

		// Set intercharacter space (ESC SP)
		case 0x20:
			if (!multipoint) {
				extra_intra_space = static_cast<Real64>(params[0]) /
				                    (print_quality == PrintQuality::Draft
				                             ? 120
				                             : 180);
				hmi = -1;
				UpdateFont();
			}
			break;

		// Master select (ESC !)
		case 0x21:
			cpi = params[0] & 0x01 ? 12 : 10;

			// Reset the first seven style bits (prop..superscript)
			// and rebuild them from the master-select byte below.
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
				score           = ScoreType::Single;
				style.underline = 1;
			}

			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		// Cancel MSB control (ESC #)
		case 0x23: msb = 255; break;

		// Set absolute horizontal print position (ESC $)
		case 0x24: {
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

		// Print 24-bit hex-density graphics (FS Z)
		case 0x85a:
			SetupBitImage(40, static_cast<uint16_t>(Param16(0)));
			break;

		// Select bit image (ESC *)
		case 0x2a:
			SetupBitImage(params[0], static_cast<uint16_t>(Param16(1)));
			break;

		// Set n/360-inch line spacing (ESC +)
		case 0x2b:
		// Set n/360-inch line spacing (FS 3)
		case 0x833:
			line_spacing = static_cast<Real64>(params[0]) / 360;
			break;

		// Turn underline on/off (ESC -)
		case 0x2d:
			if (params[0] == 0 || params[0] == 48) {
				style.underline = 0;
			}
			if (params[0] == 1 || params[0] == 49) {
				style.underline = 1;
				score           = ScoreType::Single;
			}
			UpdateFont();
			break;

		// Select vertical tab channel (ESC /)
		case 0x2f:
			// Ignore
			break;

		// Select 1/8-inch line spacing (ESC 0)
		case 0x30: line_spacing = static_cast<Real64>(1) / 8; break;

		// Select 1/6-inch line spacing (ESC 2)
		case 0x32: line_spacing = static_cast<Real64>(1) / 6; break;

		// Set n/180-inch line spacing (ESC 3)
		case 0x33:
			line_spacing = static_cast<Real64>(params[0]) / 180;
			break;

		// Select italic font (ESC 4)
		case 0x34:
			style.italics = 1;
			UpdateFont();
			break;

		// Cancel italic font (ESC 5)
		case 0x35:
			style.italics = 0;
			UpdateFont();
			break;

		// Enable printing of upper control codes (ESC 6)
		case 0x36: print_upper_contr = true; break;

		// Enable upper control codes (ESC 7)
		case 0x37: print_upper_contr = false; break;

		// Unidirectional mode (one line) (ESC <)
		case 0x3c:
			// We don't have a print head, so just ignore this
			break;

		// Set MSB to 0 (ESC =)
		case 0x3d: msb = 0; break;

		// Set MSB to 1 (ESC >)
		case 0x3e: msb = 1; break;

		// Reassign bit-image mode (ESC ?)
		case 0x3f:
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

		// Initialize printer (ESC @)
		case 0x40: ResetPrinter(); break;

		// Set n/60-inch line spacing
		case 0x41:
		case 0x841:
			line_spacing = static_cast<Real64>(params[0]) / 60;
			break;

		// Set page length in lines (ESC C)
		case 0x43:
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

		// Select bold font (ESC E)
		case 0x45:
			style.bold = 1;
			UpdateFont();
			break;

		// Cancel bold font (ESC F)
		case 0x46:
			style.bold = 0;
			UpdateFont();
			break;

		// Select dobule-strike printing (ESC G)
		case 0x47: style.doublestrike = 1; break;

		// Cancel double-strike printing (ESC H)
		case 0x48: style.doublestrike = 0; break;

		// Advance print position vertically (ESC J n)
		case 0x4a:
			cur_y += static_cast<Real64>(
			        static_cast<Real64>(params[0]) / 180);
			if (cur_y > bottom_margin) {
				NewPage(true, false);
			}
			break;

		// Select 60-dpi graphics (ESC K)
		case 0x4b:
			SetupBitImage(densk, static_cast<uint16_t>(Param16(0)));
			break;

		// Select 120-dpi graphics (ESC L)
		case 0x4c:
			SetupBitImage(densl, static_cast<uint16_t>(Param16(0)));
			break;

		// Select 10.5-point, 12-cpi (ESC M)
		case 0x4d:
			cpi        = 12;
			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		// Set bottom margin (ESC N)
		case 0x4e:
			top_margin = 0.0;
			bottom_margin = static_cast<Real64>(params[0]) * line_spacing;
			break;

		// Cancel bottom (and top) margin
		case 0x4f:
			top_margin    = 0.0;
			bottom_margin = page_height;
			break;

		// Select 10.5-point, 10-cpi (ESC P)
		case 0x50:
			cpi        = 10;
			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		// Set right margin
		case 0x51:
			right_margin = static_cast<Real64>(params[0] - 1.0) /
			               static_cast<Real64>(cpi);
			break;

		// Select an international character set (ESC R)
		case 0x52:
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

		// Select superscript/subscript printing (ESC S)
		case 0x53:
			if (params[0] == 0 || params[0] == 48) {
				style.subscript = 1;
			}
			if (params[0] == 1 || params[1] == 49) {
				style.superscript = 1;
			}
			UpdateFont();
			break;

		// Cancel superscript/subscript printing (ESC T)
		case 0x54:
			style.superscript = 0;
			style.subscript   = 0;
			UpdateFont();
			break;

		// Turn unidirectional mode on/off (ESC U)
		case 0x55:
			// We don't have a print head, so just ignore this
			break;

		// Turn double-width printing on/off (ESC W)
		case 0x57:
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

		// Select font by pitch and point (ESC X)
		case 0x58:
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

		// Select 120-dpi, double-speed graphics (ESC Y)
		case 0x59:
			SetupBitImage(densy, static_cast<uint16_t>(Param16(0)));
			break;

		// Select 240-dpi graphics (ESC Z)
		case 0x5a:
			SetupBitImage(densz, static_cast<uint16_t>(Param16(0)));
			break;

		// Set relative horizontal print position (ESC \)
		case 0x5c: {
			const int16_t toMove = static_cast<int16_t>(Param16(0));
			Real64 unitSize      = defined_unit;
			if (unitSize < 0) {
				unitSize = static_cast<Real64>(
				        print_quality == PrintQuality::Draft
				                ? 120.0
				                : 180.0);
			}
			cur_x += static_cast<Real64>(
			        static_cast<Real64>(toMove) / unitSize);
		} break;

		// Select justification (ESC a)
		case 0x61:
			// Ignore
			break;

		// Set horizontal motion index (HMI) (ESC c)
		case 0x63:
			hmi = static_cast<Real64>(Param16(0)) /
			      static_cast<Real64>(360.0);
			extra_intra_space = 0.0;
			break;

		// Select 10.5-point, 15-cpi (ESC g)
		case 0x67:
			cpi        = 15;
			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		// implemented yet — Select forward feed mode (FS F) - set
		// reverse not
		case 0x846:
			if (line_spacing < 0) {
				line_spacing *= -1;
			}
			break;

		// Reverse paper feed (ESC j)
		case 0x6a: {
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
		// Select typeface (ESC k)
		case 0x6b:
			if (params[0] <= 11 || params[0] == 30 || params[0] == 31) {
				lq_typeface = static_cast<Typeface>(params[0]);
			}
			UpdateFont();
			break;

		// Set left margin (ESC l)
		case 0x6c:
			left_margin = static_cast<Real64>(params[0] - 1.0) /
			              static_cast<Real64>(cpi);
			if (cur_x < left_margin) {
				cur_x = left_margin;
			}
			break;

		// Turn proportional mode on/off (ESC p)
		case 0x70:
			if (params[0] == 0 || params[0] == 48) {
				style.prop = 0;
			}
			if (params[0] == 1 || params[0] == 49) {
				style.prop    = 1;
				print_quality = PrintQuality::Lq;
			}
			multipoint = false;
			hmi        = -1;
			UpdateFont();
			break;

		// Select printing color (ESC r)
		case 0x72:

			if (params[0] == 0 || params[0] > 6) {
				color = ColorBlack;
			} else {
				color = static_cast<uint8_t>(params[0] << 5);
			}
			break;

		// Select low-speed mode (ESC s)
		case 0x73:
			// Ignore
			break;

		// Select character table (ESC t)
		case 0x74:
		// Select character table (FS I)
		case 0x849:
			if (params[0] < 4) {
				cur_char_table = params[0];
			}
			if (params[0] >= 48 && params[0] <= 51) {
				cur_char_table = params[0] - 48;
			}
			SelectCodepage(char_tables[cur_char_table]);
			UpdateFont();
			break;

		// Turn double-height printing on/off (ESC w)
		case 0x77:
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

		// Select LQ or draft (ESC x)
		case 0x78:
			if (params[0] == 0 || params[0] == 48) {
				print_quality   = PrintQuality::Draft;
				style.condensed = 1;
			}
			if (params[0] == 1 || params[0] == 49) {
				print_quality   = PrintQuality::Lq;
				style.condensed = 0;
			}
			hmi = -1;
			UpdateFont();
			break;

		// Set page length in inches (ESC C NUL)
		case 0x100:
			page_height   = static_cast<Real64>(params[0]);
			bottom_margin = page_height;
			top_margin    = 0.0;
			break;

		// Skip unsupported ESC ( command
		case 0x101:
			needed_param = static_cast<uint8_t>(Param16(0));
			num_param    = 0;
			break;

		// Assign character table (ESC (t)
		case 0x274:
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

		// Select line/score (ESC (-)
		case 0x22d:
			style.underline     = 0;
			style.strikethrough = 0;
			style.overscore     = 0;
			score               = static_cast<ScoreType>(params[4]);
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

		// Bar code setup and print (ESC (B)
		case 0x242:
			LOG_ERR("PRINTER: Barcode printing not supported");
			// Find out how many bytes to skip
			needed_param = static_cast<uint8_t>(Param16(0));
			num_param    = 0;
			break;

		// Set page length in defined unit (ESC (C)
		case 0x243:
			if (params[0] != 0 && defined_unit > 0) {
				page_height = bottom_margin = (static_cast<Real64>(
				                                      Param16(2))) *
				                              defined_unit;
				top_margin = 0.0;
			}
			break;

		// Set unit (ESC (U)
		case 0x255:
			defined_unit = static_cast<Real64>(params[2]) /
			               static_cast<Real64>(3600);
			break;

		// Set absolute vertical print position (ESC (V)
		case 0x256: {
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

		// Print data as characters (ESC (^)
		case 0x25e:
			num_print_as_char = static_cast<uint16_t>(Param16(0));
			break;

		// Set page format (ESC (c)
		case 0x263:
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

		// Set relative vertical print position (ESC (v)
		case 0x276: {
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
	// NUL is ignored by the printer
	case 0x00: return true;

	// Beeper (BEL)
	case 0x07:
		// BEEEP!
		return true;

	// Backspace (BS)
	case 0x08: {
		Real64 newX = cur_x - (1 / static_cast<Real64>(act_cpi));
		if (hmi > 0) {
			newX = cur_x - hmi;
		}
		if (newX >= left_margin) {
			cur_x = newX;
		}
	}
		return true;

	// Tab horizontally (HT)
	case 0x09: {
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

	// Tab vertically (VT)
	case 0x0b:
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

	// Form feed (FF)
	case 0x0c:
		if (style.doublewidth_oneline) {
			style.doublewidth_oneline = 0;
			UpdateFont();
		}
		NewPage(true, true);
		return true;

	// Carriage Return (CR)
	case 0x0d:
		cur_x = left_margin;
		if (!auto_feed) {
			return true;
		}
		[[fallthrough]];
	// Line feed
	case 0x0a:
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

	// Select Real64-width printing (one line) (SO)
	case 0x0e:
		if (!multipoint) {
			hmi                       = -1;
			style.doublewidth_oneline = 1;
			UpdateFont();
		}
		return true;

	// Select condensed printing (SI)
	case 0x0f:
		if (!multipoint && (cpi != 15.0)) {
			hmi             = -1;
			style.condensed = 1;
			UpdateFont();
		}
		return true;

	// Select printer (DC1)
	case 0x11:
		// Ignore
		return true;

	// Cancel condensed printing (DC2)
	case 0x12:
		hmi             = -1;
		style.condensed = 0;
		UpdateFont();
		return true;

	// Deselect printer (DC3)
	case 0x13:
		// Ignore
		return true;

	// Cancel double-width printing (one line) (DC4)
	case 0x14:
		hmi                       = -1;
		style.doublewidth_oneline = 0;
		UpdateFont();
		return true;

	// Cancel line (CAN)
	case 0x18: return true;

	// ESC
	case 0x1b: esc_seen = true; return true;

	// FS (IBM commands)
	case 0x1c: fs_seen = true; return true;

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
			}

			// TODO line-wrap when cur_y goes past bottom margin.
			cur_y += static_cast<Real64>(1) /
			         static_cast<Real64>(bit_graph.vert_dens);
		}
	}

	SDL_UnlockSurface(page);

	cur_y = oldY;

	bit_graph.read_bytes_column = 0;

	// Advance to the left
	cur_x += static_cast<Real64>(1) / static_cast<Real64>(bit_graph.horiz_dens);
}

#endif // C_PRINTER
