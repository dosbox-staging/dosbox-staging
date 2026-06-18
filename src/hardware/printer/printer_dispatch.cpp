// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

// ESC/P2 command dispatch — Printer::ProcessCommandChar plus the
// bit-image helpers (SetupBitImage, PrintBitGraph) that ESC commands
// in here drive.

#include "printer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>

#include "misc/logging.h"
#include "utils/checks.h"

#include "printer_charmaps.h"

CHECK_NARROWING();

namespace VirtualPrinter {

// ESC/P2 opcode constants.
// The high byte distinguishes the escape-sequence family:
//   0x000 - 0x0ff : single-byte ESC commands ('ESC <ch>')
//   0x100 - 0x1ff : internal sentinels used by the param-collection state
//                   machine (no real ESC byte; see ProcessData)
//   0x200 - 0x2ff : two-byte 'ESC ( <ch>' commands
namespace {

namespace Esc {
constexpr auto Undoc02                                          = 0x02;
constexpr auto ReverseLineFeed                                  = 0x0a;
constexpr auto ReturnToTopOfCurrentPage                         = 0x0c;
constexpr auto SelectDoubleWidthPrinting                        = 0x0e;
constexpr auto SelectCondensedPrinting                          = 0x0f;
constexpr auto ControlPaperLoadingEjecting                      = 0x19;
constexpr auto SetIntercharacterSpace                           = 0x20;
constexpr auto MasterSelect                                     = 0x21;
constexpr auto CancelMsbControl                                 = 0x23;
constexpr auto SetAbsoluteHorizontalPrintPosition               = 0x24;
constexpr auto SelectUserDefinedSet                             = 0x25;
constexpr auto DefineUserDefinedCharacters                      = 0x26;
constexpr auto TwoBytesSequence                                 = 0x28;
constexpr auto SelectBitImage                                   = 0x2a;
constexpr auto SetN360InchLineSpacing                           = 0x2b;
constexpr auto TurnUnderlineOnOff                               = 0x2d;
constexpr auto SelectVerticalTabChannel                         = 0x2f;
constexpr auto Select18InchLineSpacing                          = 0x30;
constexpr auto Select760InchLineSpacing                         = 0x31;
constexpr auto Select16InchLineSpacing                          = 0x32;
constexpr auto SetN180InchLineSpacing                           = 0x33;
constexpr auto SelectItalicFont                                 = 0x34;
constexpr auto CancelItalicFont                                 = 0x35;
constexpr auto EnablePrintingOfUpperControlCodes                = 0x36;
constexpr auto EnableUpperControlCodes                          = 0x37;
constexpr auto DisablePaperOutDetector                          = 0x38;
constexpr auto EnablePaperOutDetector                           = 0x39;
constexpr auto CopyRomToRam                                     = 0x3a;
constexpr auto UnidirectionalMode                               = 0x3c;
constexpr auto SetMsbTo0                                        = 0x3d;
constexpr auto SetMsbTo1                                        = 0x3e;
constexpr auto ReassignBitImageMode                             = 0x3f;
constexpr auto InitializePrinter                                = 0x40;
constexpr auto SetN60InchLineSpacing                            = 0x41;
constexpr auto SetVerticalTabs                                  = 0x42;
constexpr auto SetPageLengthInLines                             = 0x43;
constexpr auto SetHorizontalTabs                                = 0x44;
constexpr auto SelectBoldFont                                   = 0x45;
constexpr auto CancelBoldFont                                   = 0x46;
constexpr auto SelectDoubleStrikePrinting                       = 0x47;
constexpr auto CancelDoubleStrikePrinting                       = 0x48;
constexpr auto SelectCharacterTypePrintPitch                    = 0x49;
constexpr auto AdvancePrintPositionVertically                   = 0x4a;
constexpr auto Select60DpiGraphics                              = 0x4b;
constexpr auto Select120DpiGraphics                             = 0x4c;
constexpr auto Select105Point12Cpi                              = 0x4d;
constexpr auto SetBottomMargin                                  = 0x4e;
constexpr auto CancelBottomMargin                               = 0x4f;
constexpr auto Select105Point10Cpi                              = 0x50;
constexpr auto SetRightMargin                                   = 0x51;
constexpr auto SelectInternationalCharacterSet                  = 0x52;
constexpr auto SelectSuperscriptSubscriptPrinting               = 0x53;
constexpr auto CancelSuperscriptSubscriptPrinting               = 0x54;
constexpr auto TurnUnidirectionalModeOnOff                      = 0x55;
constexpr auto TurnDoubleWidthPrintingOnOff                     = 0x57;
constexpr auto SelectFontPitchPoint                             = 0x58;
constexpr auto Select120DpiDoubleSpeedGraphics                  = 0x59;
constexpr auto Select240DpiGraphics                             = 0x5a;
constexpr auto SelectCharacterHeightWidthLineSpacing            = 0x5b;
constexpr auto SetRelativeHorizontalPrintPosition               = 0x5c;
constexpr auto Select60Or120Dpi9PinGraphics = 0x5e;
constexpr auto SelectJustification                              = 0x61;
constexpr auto SetVerticalTabsInVfuChannels                     = 0x62;
constexpr auto SetHorizontalMotionIndex                         = 0x63;
constexpr auto SetVerticalTabStopsEveryNLines                   = 0x65;
constexpr auto HorizontalVerticalSkip                   = 0x66;
constexpr auto Select105Point15Cpi                              = 0x67;
constexpr auto SelectDoubleQuadrupleSize                        = 0x68;
constexpr auto ImmediatePrint                                   = 0x69;
constexpr auto ReversePaperFeed                                 = 0x6a;
constexpr auto SelectTypeface                                   = 0x6b;
constexpr auto SetLeftMargin                                    = 0x6c;
constexpr auto TurnProportionalModeOnOff                        = 0x70;
constexpr auto SelectPrintingColor                              = 0x72;
constexpr auto LowSpeedModeOnOff                                = 0x73;
constexpr auto SelectCharacterTable                             = 0x74;
constexpr auto TurnDoubleHeightPrintingOnOff                    = 0x77;
constexpr auto SelectLqDraft                                    = 0x78;
constexpr auto SelectDeselectSlashZero                          = 0x7e;
constexpr auto SetPageLengthInInches                            = 0x100;
constexpr auto SkipUnsupportedEscCommand                        = 0x101;
constexpr auto ParenSelectLineScore                             = 0x22d;
constexpr auto ParenBarCodeSetupPrint                           = 0x242;
constexpr auto ParenSetPageLengthInDefinedUnit                  = 0x243;
constexpr auto ParenSetUnit                                     = 0x255;
constexpr auto ParenSetAbsoluteVerticalPrintPosition            = 0x256;
constexpr auto ParenPrintDataAsCharacters                       = 0x25e;
constexpr auto ParenSetPageFormat                               = 0x263;
constexpr auto ParenAssignCharacterTable                        = 0x274;
constexpr auto ParenSetRelativeVerticalPrintPosition            = 0x276;
} // namespace Esc

// ESC/P vertical motion divisors. Vertical positions and line spacings
// arrive as integer parameter bytes that we divide by these constants
// to get inches. 9-pin printers use a finer base unit than 24/48-pin
// ones for the same commands (escp2ref.pdf C-37, R-62).
constexpr double FineVerticalDivisor24Pin = 180.0;
constexpr double FineVerticalDivisor9Pin  = 216.0;

// Coarser line-spacing divisors for ESC A / FS A (escp2ref.pdf C-39).
constexpr double CoarseVerticalDivisor24Pin = 60.0;
constexpr double CoarseVerticalDivisor9Pin  = 72.0;

// ESCP2 'defined unit' divisor used by ESC ( U, ESC ( C, and the
// ESC + / ESC ( V family when a custom unit hasn't been selected.
constexpr double DefinedUnitDivisor = 360.0;

// Per-opcode debounce interval for unsupported-command warnings.
// Many DOS apps emit the same unsupported opcode repeatedly; muting
// outright (once-per-session) hides bursts that come back later,
// logging every invocation drowns the log.
constexpr std::chrono::seconds UnsupportedOpcodeLogInterval{3};

} // namespace

bool Printer::ProcessCommandChar(const uint8_t ch)
{
	if (fs_seen) {
		fs_seen = false;

		using Clock = std::chrono::steady_clock;
		static std::unordered_map<uint8_t, Clock::time_point> last_warned;

		const auto now = Clock::now();
		auto it        = last_warned.find(ch);

		if (it == last_warned.end() ||
		    now - it->second >= UnsupportedOpcodeLogInterval) {
			LOG_WARNING(
			        "PRINTER: FS %c (%02Xh) not supported "
			        "(IBM PPDS mode is not emulated); output "
			        "may be incorrect",
			        ch,
			        ch);

			last_warned[ch] = now;
		}
		return true;
	}

	if (esc_seen) {
		esc_cmd   = ch;
		esc_seen  = false;
		num_param = 0;

		switch (esc_cmd) {
		// Undocumented
		case Esc::Undoc02: // 0x02
		// (ESC LF) — Reverse line feed
		case Esc::ReverseLineFeed: // 0x0a
		// (ESC FF) — Return to top of current page
		case Esc::ReturnToTopOfCurrentPage: // 0x0c
		// (ESC SO) — Select double-width printing (one line)
		case Esc::SelectDoubleWidthPrinting: // 0x0e
		// (ESC SI) — Select condensed printing
		case Esc::SelectCondensedPrinting: // 0x0f
		// (ESC #) — Cancel MSB control
		case Esc::CancelMsbControl: // 0x23
		// (ESC 0) — Select 1/8-inch line spacing
		case Esc::Select18InchLineSpacing: // 0x30
		// (ESC 1) — Select 7/60-inch line spacing
		case Esc::Select760InchLineSpacing: // 0x31
		// (ESC 2) — Select 1/6-inch line spacing
		case Esc::Select16InchLineSpacing: // 0x32
		// (ESC 4) — Select italic font
		case Esc::SelectItalicFont: // 0x34
		// (ESC 5) — Cancel italic font
		case Esc::CancelItalicFont: // 0x35
		// (ESC 6) — Enable printing of upper control codes
		case Esc::EnablePrintingOfUpperControlCodes: // 0x36
		// (ESC 7) — Enable upper control codes
		case Esc::EnableUpperControlCodes: // 0x37
		// (ESC 8) — Disable paper-out detector
		case Esc::DisablePaperOutDetector: // 0x38
		// (ESC 9) — Enable paper-out detector
		case Esc::EnablePaperOutDetector: // 0x39
		// (ESC <) — Unidirectional mode (one line)
		case Esc::UnidirectionalMode: // 0x3c
		// (ESC =) — Set MSB to 0
		case Esc::SetMsbTo0: // 0x3d
		// (ESC >) — Set MSB to 1
		case Esc::SetMsbTo1: // 0x3e
		// (ESC @) — Initialize printer
		case Esc::InitializePrinter: // 0x40
		// (ESC E) — Select bold font
		case Esc::SelectBoldFont: // 0x45
		// (ESC F) — Cancel bold font
		case Esc::CancelBoldFont: // 0x46
		// (ESC G) — Select double-strike printing
		case Esc::SelectDoubleStrikePrinting: // 0x47
		// (ESC H) — Cancel double-strike printing
		case Esc::CancelDoubleStrikePrinting: // 0x48
		// (ESC M) — Select 10.5-point, 12-cpi
		case Esc::Select105Point12Cpi: // 0x4d
		// (ESC O) — Cancel bottom margin [conflict]
		case Esc::CancelBottomMargin: // 0x4f
		// (ESC P) — Select 10.5-point, 10-cpi
		case Esc::Select105Point10Cpi: // 0x50
		// (ESC T) — Cancel superscript/subscript printing
		case Esc::CancelSuperscriptSubscriptPrinting: // 0x54
		// (ESC g) — Select 10.5-point, 15-cpi
		case Esc::Select105Point15Cpi: // 0x67
			needed_param = 0;
			break;

		// (ESC EM) — Control paper loading/ejecting
		case Esc::ControlPaperLoadingEjecting: // 0x19
		// (ESC SP) — Set intercharacter space
		case Esc::SetIntercharacterSpace: // 0x20
		// (ESC !) — Master select
		case Esc::MasterSelect: // 0x21
		// (ESC +) — Set n/360-inch line spacing
		case Esc::SetN360InchLineSpacing: // 0x2b
		// (ESC -) — Turn underline on/off
		case Esc::TurnUnderlineOnOff: // 0x2d
		// (ESC /) — Select vertical tab channel
		case Esc::SelectVerticalTabChannel: // 0x2f
		// (ESC 3) — Set n/180-inch line spacing
		case Esc::SetN180InchLineSpacing: // 0x33
		// (ESC A) — Set n/60-inch line spacing
		case Esc::SetN60InchLineSpacing: // 0x41
		// (ESC C) — Set page length in lines
		case Esc::SetPageLengthInLines: // 0x43
		// (ESC I) — Select character type and print pitch
		case Esc::SelectCharacterTypePrintPitch: // 0x49
		// (ESC J) — Advance print position vertically
		case Esc::AdvancePrintPositionVertically: // 0x4a
		// (ESC N) — Set bottom margin
		case Esc::SetBottomMargin: // 0x4e
		// (ESC Q) — Set right margin
		case Esc::SetRightMargin: // 0x51
		// (ESC R) — Select an international character set
		case Esc::SelectInternationalCharacterSet: // 0x52
		// (ESC S) — Select superscript/subscript printing
		case Esc::SelectSuperscriptSubscriptPrinting: // 0x53
		// (ESC U) — Turn unidirectional mode on/off
		case Esc::TurnUnidirectionalModeOnOff: // 0x55
		// case 0x56: // Repeat data
		// (ESC V)
		// (ESC W) — Turn double-width printing on/off
		case Esc::TurnDoubleWidthPrintingOnOff: // 0x57
		// (ESC a) — Select justification
		case Esc::SelectJustification: // 0x61
		// (ESC h) — Select double or quadruple size
		case Esc::SelectDoubleQuadrupleSize: // 0x68
		// (ESC i) — Immediate print
		case Esc::ImmediatePrint: // 0x69
		// (ESC j) — Reverse paper feed
		case Esc::ReversePaperFeed: // 0x6a
		// (ESC k) — Select typeface
		case Esc::SelectTypeface: // 0x6b
		// (ESC 1) — Set left margin
		case Esc::SetLeftMargin: // 0x6c
		// (ESC p) — Turn proportional mode on/off
		case Esc::TurnProportionalModeOnOff: // 0x70
		// (ESC r) — Select printing color
		case Esc::SelectPrintingColor: // 0x72
		// (ESC s) — Low-speed mode on/off
		case Esc::LowSpeedModeOnOff: // 0x73
		// (ESC t) — Select character table
		case Esc::SelectCharacterTable: // 0x74
		// (ESC w) — Turn double-height printing on/off
		case Esc::TurnDoubleHeightPrintingOnOff: // 0x77
		// (ESC x) — Select LQ or draft
		case Esc::SelectLqDraft: // 0x78
		// (ESC ~) — Select/Deselect slash zero
		case Esc::SelectDeselectSlashZero: // 0x7e
			needed_param = 1;
			break;

		// (ESC $) — Set absolute horizontal print position
		case Esc::SetAbsoluteHorizontalPrintPosition: // 0x24
		// (ESC ?) — Reassign bit-image mode
		case Esc::ReassignBitImageMode: // 0x3f
		// (ESC K) — Select 60-dpi graphics
		case Esc::Select60DpiGraphics: // 0x4b
		// (ESC L) — Select 120-dpi graphics
		case Esc::Select120DpiGraphics: // 0x4c
		// (ESC Y) — Select 120-dpi, double-speed graphics
		case Esc::Select120DpiDoubleSpeedGraphics: // 0x59
		// (ESC Z) — Select 240-dpi graphics
		case Esc::Select240DpiGraphics: // 0x5a
		// (ESC \) — Set relative horizontal print position
		case Esc::SetRelativeHorizontalPrintPosition: // 0x5c
		// (ESC c) — Set horizontal motion index (HMI)	[conflict]
		case Esc::SetHorizontalMotionIndex: // 0x63
		// (ESC e) — Set vertical tab stops every n lines
		case Esc::SetVerticalTabStopsEveryNLines: // 0x65
		// (ESC f) — Horizontal/vertical skip (spec C-47, m + n)
		case Esc::HorizontalVerticalSkip: // 0x66
			needed_param = 2;
			break;

		// (ESC *) — Select bit image
		case Esc::SelectBitImage: // 0x2a
		// (ESC X) — Select font by pitch and point [conflict]
		case Esc::SelectFontPitchPoint: // 0x58
		// (ESC ^) — Select 60/120-dpi 9-pin graphics (spec C-191).
		// Header is m nL nH; the dispatcher must then consume
		// 2*(nH*256+nL) data bytes — see the late dispatch handler.
		case Esc::Select60Or120Dpi9PinGraphics: // 0x5e
			needed_param = 3;
			break;

		// Select character height, width, line spacing
		case Esc::SelectCharacterHeightWidthLineSpacing:
			needed_param = 7;
			break; // 0x5b

		// (ESC b) — Set vertical tabs in VFU channels
		case Esc::SetVerticalTabsInVfuChannels: // 0x62
		// (ESC B) — Set vertical tabs
		case Esc::SetVerticalTabs:
			num_vert_tabs = 0;
			return true; // 0x42

		// (ESC D) — Set horizontal tabs
		case Esc::SetHorizontalTabs:
			num_horiz_tabs = 0;
			return true; // 0x44

		// (ESC %) — Select user-defined set
		case Esc::SelectUserDefinedSet: // 0x25
		// (ESC &) — Define user-defined characters
		case Esc::DefineUserDefinedCharacters: // 0x26
			LOG_ERR("PRINTER: User-defined characters not supported");
			return true;

		// (ESC :) — Copy ROM to RAM (ESC : NUL n m, 3 param bytes per
		// spec C-89). Not implemented; we consume the params so the
		// rest of the stream isn't misinterpreted as text. The late
		// dispatch default emits the "recognised but not implemented"
		// warning once per debounce interval.
		case Esc::CopyRomToRam: // 0x3a
			needed_param = 3;
			break;

		// Two bytes sequence
		case Esc::TwoBytesSequence: return true; // 0x28

		default:
			LOG_MSG("PRINTER: Unknown command ESC (%02Xh) %c , unable to skip parameters.",
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
		case Esc::ParenBarCodeSetupPrint: // 0x242
		// Print data as characters (ESC (^)
		case Esc::ParenPrintDataAsCharacters:
			needed_param = 2;
			break; // 0x25e

		// Set unit (ESC (U)
		case Esc::ParenSetUnit:
			needed_param = 3;
			break; // 0x255

		// Set page length in defined unit (ESC (C)
		case Esc::ParenSetPageLengthInDefinedUnit: // 0x243
		// Set absolute vertical print position (ESC (V)
		case Esc::ParenSetAbsoluteVerticalPrintPosition: // 0x256
		// Set relative vertical print position (ESC (v)
		case Esc::ParenSetRelativeVerticalPrintPosition:
			needed_param = 4;
			break; // 0x276

		// Assign character table (ESC (t)
		case Esc::ParenAssignCharacterTable: // 0x274
		// Select line/score (ESC (-)
		case Esc::ParenSelectLineScore:
			needed_param = 5;
			break; // 0x22d

		// Set page format (ESC (c)
		case Esc::ParenSetPageFormat: needed_param = 6; break; // 0x263

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
		                        static_cast<double>(ch) * line_spacing)) { // Done
			esc_cmd = 0;
		} else if (num_vert_tabs < 16) {
			vert_tabs[num_vert_tabs++] = static_cast<double>(ch) *
			                             line_spacing;
		}
	}

	// Collect horizontal tabs
	if (esc_cmd == 0x44) {
		if (ch == 0 || (num_horiz_tabs > 0 &&
		                horiz_tabs[num_horiz_tabs - 1] >
		                        static_cast<double>(ch) *
		                                (1 / static_cast<double>(cpi)))) { // Done
			esc_cmd = 0;
		} else if (num_horiz_tabs < 32) {
			horiz_tabs[num_horiz_tabs++] = static_cast<double>(ch) *
			                               (1 / static_cast<double>(cpi));
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
		case Esc::Undoc02: // 0x02
			// Ignore
			break;

		// Select double-width printing (one line) (ESC SO)
		case Esc::SelectDoubleWidthPrinting: // 0x0e
			if (!multipoint) {
				hmi                       = -1;
				style.doublewidth_oneline = 1;
				UpdateFont();
			}
			break;

		// Select condensed printing (ESC SI)
		case Esc::SelectCondensedPrinting: // 0x0f
			if (!multipoint && (cpi != 15.0)) {
				hmi             = -1;
				style.condensed = 1;
				UpdateFont();
			}
			break;

		// Control paper loading/ejecting (ESC EM)
		case Esc::ControlPaperLoadingEjecting: // 0x19
			// We are not really loading paper, so most commands can
			// be ignored
			if (params[0] == 'R') {
				NewPage(true, false);
			}
			break;

		// Set intercharacter space (ESC SP)
		case Esc::SetIntercharacterSpace: // 0x20
			if (!multipoint) {
				extra_intra_space = static_cast<double>(params[0]) /
				                    (print_quality == PrintQuality::Draft
				                             ? 120
				                             : 180);
				hmi = -1;
				UpdateFont();
			}
			break;

		// Master select (ESC !)
		case Esc::MasterSelect: // 0x21
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
		case Esc::CancelMsbControl:
			msb = 255;
			break; // 0x23

		// Set absolute horizontal print position (ESC $).
		// Spec C-31: position = param × (defined unit) + left_margin.
		// defined_unit (set by ESC ( U) is in inches/unit; the default
		// when no custom unit is selected is 1/60 inch.
		case Esc::SetAbsoluteHorizontalPrintPosition: { // 0x24
			constexpr double DefaultUnitInches = 1.0 / 60.0;
			const double unit_inches = (defined_unit > 0)
			                                 ? defined_unit
			                                 : DefaultUnitInches;
			const double newX = left_margin +
			                    static_cast<double>(Param16(0)) *
			                            unit_inches;
			if (newX <= right_margin) {
				cur_x = newX;
			}
		} break;

		// Select bit image (ESC *)
		case Esc::SelectBitImage: // 0x2a
			SetupBitImage(params[0], static_cast<uint16_t>(Param16(1)));
			break;

		// Select 60/120-dpi, 9-pin graphics (ESC ^). Header is
		// m nL nH; (nH*256 + nL) is the column count, with 2 data
		// bytes per column. Spec C-191 says this is a 9-pin
		// nonrecommended command and recommends ESC * instead. We
		// don't render the dots; the bytes are pulled out of the
		// stream via the bit_graph discard flag so the rest of the
		// stream isn't misinterpreted as text.
		case Esc::Select60Or120Dpi9PinGraphics: // 0x5e
			bit_graph.bytes_left = static_cast<uint16_t>(
			        2 * Param16(1));
			bit_graph.bytes_column      = 2;
			bit_graph.read_bytes_column = 0;
			bit_graph.col_index         = 0;
			bit_graph.discard_data      = true;
			break;

		// Set n/360-inch line spacing (ESC +) -- 24/48-pin only
		// per escp2ref.pdf C-29 ("Not available on 9-pin printers").
		// On a 9-pin printer model, ignore.
		case Esc::SetN360InchLineSpacing: // 0x2b
			if (pins != 9) {
				line_spacing = static_cast<double>(params[0]) /
				               DefinedUnitDivisor;
			}
			break;

		// Turn underline on/off (ESC -)
		case Esc::TurnUnderlineOnOff: // 0x2d
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
		case Esc::SelectVerticalTabChannel: // 0x2f
			// Ignore
			break;

		// Select 1/8-inch line spacing (ESC 0)
		case Esc::Select18InchLineSpacing:
			line_spacing = static_cast<double>(1) / 8;
			break; // 0x30

		// Select 1/6-inch line spacing (ESC 2)
		case Esc::Select16InchLineSpacing:
			line_spacing = static_cast<double>(1) / 6;
			break; // 0x32

		// Set line spacing (ESC 3) -- n/180 for 24/48-pin, n/216 for
		// 9-pin (escp2ref.pdf C-37).
		case Esc::SetN180InchLineSpacing: // 0x33
			line_spacing = static_cast<double>(params[0]) /
			               (pins == 9 ? FineVerticalDivisor9Pin
			                          : FineVerticalDivisor24Pin);
			break;

		// Select italic font (ESC 4)
		case Esc::SelectItalicFont: // 0x34
			style.italics = 1;
			UpdateFont();
			break;

		// Cancel italic font (ESC 5)
		case Esc::CancelItalicFont: // 0x35
			style.italics = 0;
			UpdateFont();
			break;

		// Enable printing of upper control codes (ESC 6)
		case Esc::EnablePrintingOfUpperControlCodes:
			print_upper_contr = true;
			break; // 0x36

		// Enable upper control codes (ESC 7)
		case Esc::EnableUpperControlCodes:
			print_upper_contr = false;
			break; // 0x37

		// Unidirectional mode (one line) (ESC <)
		case Esc::UnidirectionalMode: // 0x3c
			// We don't have a print head, so just ignore this
			break;

		// Set MSB to 0 (ESC =)
		case Esc::SetMsbTo0:
			msb = 0;
			break; // 0x3d

		// Set MSB to 1 (ESC >)
		case Esc::SetMsbTo1:
			msb = 1;
			break; // 0x3e

		// Reassign bit-image mode (ESC ?)
		case Esc::ReassignBitImageMode: // 0x3f
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

		// Initialize printer (ESC @) -- soft reset. Save any in-progress
		// page before clobbering state; DOS drivers typically end a job
		// with ESC @ to clean up, and the last content fragment lives
		// in the page buffer until something flushes it.
		case Esc::InitializePrinter:
			FormFeed();
			ResetPrinter();
			break; // 0x40

		// Set line spacing (ESC A) -- n/60 for 24/48-pin,
		// n/72 for 9-pin (escp2ref.pdf C-39).
		case Esc::SetN60InchLineSpacing: // 0x41
			line_spacing = static_cast<double>(params[0]) /
			               (pins == 9 ? CoarseVerticalDivisor9Pin
			                          : CoarseVerticalDivisor24Pin);
			break;

		// Set page length in lines (ESC C). Spec C-13: setting the
		// page length cancels both the top and the bottom margin
		// (the 9-pin variant at C-14 only cancels the bottom, but we
		// don't have a model-specific code path here and resetting
		// top_margin on a 9-pin printer is harmless).
		case Esc::SetPageLengthInLines: // 0x43
			if (params[0] != 0) {
				page_height = bottom_margin = static_cast<double>(
				                                      params[0]) *
				                              line_spacing;
				top_margin = 0.0;
			} else // == 0 => Set page length in inches
			{
				needed_param = 1;
				num_param    = 0;
				esc_cmd      = 0x100;
				return true;
			}
			break;

		// Select bold font (ESC E)
		case Esc::SelectBoldFont: // 0x45
			style.bold = 1;
			UpdateFont();
			break;

		// Cancel bold font (ESC F)
		case Esc::CancelBoldFont: // 0x46
			style.bold = 0;
			UpdateFont();
			break;

		// Select dobule-strike printing (ESC G)
		case Esc::SelectDoubleStrikePrinting:
			style.doublestrike = 1;
			break; // 0x47

		// Cancel double-strike printing (ESC H)
		case Esc::CancelDoubleStrikePrinting:
			style.doublestrike = 0;
			break; // 0x48

		// Advance print position vertically (ESC J n) -- n/180 for
		// 24/48-pin, n/216 for 9-pin (escp2ref.pdf R-62).
		case Esc::AdvancePrintPositionVertically: // 0x4a
			cur_y += static_cast<double>(params[0]) /
			         (pins == 9 ? FineVerticalDivisor9Pin
			                    : FineVerticalDivisor24Pin);
			if (cur_y > bottom_margin) {
				NewPage(true, false);
			}
			break;

		// Select 60-dpi graphics (ESC K)
		case Esc::Select60DpiGraphics: // 0x4b
			SetupBitImage(densk, static_cast<uint16_t>(Param16(0)));
			break;

		// Select 120-dpi graphics (ESC L)
		case Esc::Select120DpiGraphics: // 0x4c
			SetupBitImage(densl, static_cast<uint16_t>(Param16(0)));
			break;

		// Select 10.5-point, 12-cpi (ESC M)
		case Esc::Select105Point12Cpi: // 0x4d
			cpi        = 12;
			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		// Set bottom margin (ESC N)
		case Esc::SetBottomMargin: // 0x4e
			top_margin = 0.0;
			bottom_margin = static_cast<double>(params[0]) * line_spacing;
			break;

		// Cancel bottom (and top) margin
		case Esc::CancelBottomMargin: // 0x4f
			top_margin    = 0.0;
			bottom_margin = page_height;
			break;

		// Select 10.5-point, 10-cpi (ESC P)
		case Esc::Select105Point10Cpi: // 0x50
			cpi        = 10;
			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		// Set right margin (ESC Q). Spec C-21: right margin sits at
		// the right edge of column n, so position = n / cpi (no off-
		// by-one — contrast ESC l which puts the *left* edge of
		// column n at the margin and so subtracts one).
		case Esc::SetRightMargin: // 0x51
			right_margin = static_cast<double>(params[0]) /
			               static_cast<double>(cpi);
			break;

		// Select an international character set (ESC R)
		case Esc::SelectInternationalCharacterSet: // 0x52
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

		// Select superscript/subscript printing (ESC S). The single
		// parameter is 0 / '0' (subscript) or 1 / '1' (superscript).
		// Earlier code read params[1] for the '1' check, which is
		// out of bounds for a one-param command.
		// Select superscript / subscript printing (ESC S n) -- per
		// escp2ref.pdf C-129: n = 0 or 48 selects the *upper* part
		// of the character space (superscript); n = 1 or 49 selects
		// the *lower* part (subscript).
		case Esc::SelectSuperscriptSubscriptPrinting: // 0x53
			if (params[0] == 0 || params[0] == 48) {
				style.superscript = 1;
			}
			if (params[0] == 1 || params[0] == 49) {
				style.subscript = 1;
			}
			UpdateFont();
			break;

		// Cancel superscript/subscript printing (ESC T)
		case Esc::CancelSuperscriptSubscriptPrinting: // 0x54
			style.superscript = 0;
			style.subscript   = 0;
			UpdateFont();
			break;

		// Turn unidirectional mode on/off (ESC U)
		case Esc::TurnUnidirectionalModeOnOff: // 0x55
			// We don't have a print head, so just ignore this
			break;

		// Turn double-width printing on/off (ESC W)
		case Esc::TurnDoubleWidthPrintingOnOff: // 0x57
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
		case Esc::SelectFontPitchPoint: // 0x58
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
					multi_cpi = static_cast<double>(360) /
					            static_cast<double>(params[0]);
				}
			}
			if (multi_point_size == 0) {
				multi_point_size = static_cast<double>(10.5);
			}
			if (Param16(1) > 0) { // Set points
				multi_point_size = (static_cast<double>(Param16(1))) /
				                   2;
			}
			UpdateFont();
			break;

		// Select 120-dpi, double-speed graphics (ESC Y)
		case Esc::Select120DpiDoubleSpeedGraphics: // 0x59
			SetupBitImage(densy, static_cast<uint16_t>(Param16(0)));
			break;

		// Select 240-dpi graphics (ESC Z)
		case Esc::Select240DpiGraphics: // 0x5a
			SetupBitImage(densz, static_cast<uint16_t>(Param16(0)));
			break;

		// Set relative horizontal print position (ESC \).
		// Spec C-33: cur_x += param × (defined unit). defined_unit
		// (set by ESC ( U) is in inches/unit; the default when no
		// custom unit is selected is 1/120 inch in draft mode and
		// 1/180 inch in LQ mode.
		case Esc::SetRelativeHorizontalPrintPosition: { // 0x5c
			constexpr double DefaultDraftUnitInches = 1.0 / 120.0;
			constexpr double DefaultLqUnitInches    = 1.0 / 180.0;

			const int16_t toMove     = static_cast<int16_t>(Param16(0));
			const double unit_inches = (defined_unit > 0)
			                                 ? defined_unit
			                                 : ((print_quality ==
			                                     PrintQuality::Draft)
			                                            ? DefaultDraftUnitInches
			                                            : DefaultLqUnitInches);

			cur_x += static_cast<double>(toMove) * unit_inches;
		} break;

		// Select justification (ESC a)
		case Esc::SelectJustification: // 0x61
			// Ignore
			break;

		// Set horizontal motion index (HMI) (ESC c)
		case Esc::SetHorizontalMotionIndex: // 0x63
			hmi = static_cast<double>(Param16(0)) / DefinedUnitDivisor;
			extra_intra_space = 0.0;
			break;

		// Select 10.5-point, 15-cpi (ESC g)
		case Esc::Select105Point15Cpi: // 0x67
			cpi        = 15;
			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		// Reverse paper feed (ESC j) -- 9-pin only per escp2ref.pdf
		// C-213, single-byte parameter, n/216 inch reverse. Featured
		// only on EX-800, EX-1000, FX-80/85/100/185/286, JX-80.
		// Old FX-* DOS app drivers may still emit it. The clamp is
		// against top_margin (a vertical value) — the previous code
		// compared against left_margin, which is a horizontal one.
		case Esc::ReversePaperFeed: { // 0x6a
			const double reverse = static_cast<double>(params[0]) /
			                       FineVerticalDivisor9Pin;
			const double new_y = cur_y - reverse;
			cur_y = (new_y < top_margin) ? top_margin : new_y;
			break;
		}

		// Select typeface (ESC k)
		case Esc::SelectTypeface: // 0x6b
			if (params[0] <= 11 || params[0] == 30 || params[0] == 31) {
				lq_typeface = static_cast<Typeface>(params[0]);
			}
			UpdateFont();
			break;

		// Set left margin (ESC l). escp2ref.pdf C-23 places the left
		// edge of column n at the margin, so position = (n - 1) / cpi.
		// Clamp to 0 because real ESC/P drivers use n = 0 as a "no
		// margin" / reset shorthand, and the (n - 1) subtraction would
		// otherwise underflow to a negative position.
		case Esc::SetLeftMargin: // 0x6c
			left_margin = static_cast<double>(params[0] - 1.0) /
			              static_cast<double>(cpi);

			if (left_margin < 0.0) {
				left_margin = 0.0;
			}
			if (cur_x < left_margin) {
				cur_x = left_margin;
			}
			break;

		// Turn proportional mode on/off (ESC p)
		case Esc::TurnProportionalModeOnOff: // 0x70
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
		case Esc::SelectPrintingColor: // 0x72

			if (params[0] == 0 || params[0] > 6) {
				color = ColorBlack;
			} else {
				PagePixel pixel{};
				pixel.color_id = params[0];
				color          = pixel.data;
			}
			break;

		// Select low-speed mode (ESC s)
		case Esc::LowSpeedModeOnOff: // 0x73
			// Ignore
			break;

		// Select character table (ESC t)
		case Esc::SelectCharacterTable: // 0x74
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
		case Esc::TurnDoubleHeightPrintingOnOff: // 0x77
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

		// Select LQ or draft (ESC x). Spec C-93: only changes the
		// print quality; condensed is a separate style controlled
		// by SI / ESC SI / DC2 / ESC ! and must not be touched here.
		case Esc::SelectLqDraft: // 0x78
			if (params[0] == 0 || params[0] == 48) {
				print_quality = PrintQuality::Draft;
			}
			if (params[0] == 1 || params[0] == 49) {
				print_quality = PrintQuality::Lq;
			}
			hmi = -1;
			UpdateFont();
			break;

		// Set page length in inches (ESC C NUL)
		case Esc::SetPageLengthInInches: // 0x100
			page_height   = static_cast<double>(params[0]);
			bottom_margin = page_height;
			top_margin    = 0.0;
			break;

		// Skip unsupported ESC ( command
		case Esc::SkipUnsupportedEscCommand: // 0x101
			needed_param = static_cast<uint8_t>(Param16(0));
			num_param    = 0;
			break;

		// Assign character table (ESC (t)
		case Esc::ParenAssignCharacterTable: // 0x274
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
		case Esc::ParenSelectLineScore: // 0x22d
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
		case Esc::ParenBarCodeSetupPrint: // 0x242
			LOG_ERR("PRINTER: Barcode printing not supported");
			// Find out how many bytes to skip
			needed_param = static_cast<uint8_t>(Param16(0));
			num_param    = 0;
			break;

		// Set page length in defined unit (ESC (C)
		case Esc::ParenSetPageLengthInDefinedUnit: // 0x243
			if (params[0] != 0 && defined_unit > 0) {
				page_height = bottom_margin = (static_cast<double>(
				                                      Param16(2))) *
				                              defined_unit;
				top_margin = 0.0;
			}
			break;

		// Set unit (ESC (U)
		case Esc::ParenSetUnit: // 0x255
			defined_unit = static_cast<double>(params[2]) /
			               static_cast<double>(3600);
			break;

		// Set absolute vertical print position (ESC (V)
		case Esc::ParenSetAbsoluteVerticalPrintPosition: { // 0x256
			double unit_size = defined_unit;
			if (unit_size < 0) {
				unit_size = 1.0 / DefinedUnitDivisor;
			}
			const double new_pos = top_margin +
			                       (static_cast<double>(Param16(2)) *
			                        unit_size);
			if (new_pos > bottom_margin) {
				NewPage(true, false);
			} else {
				cur_y = new_pos;
			}
		} break;

		// Print data as characters (ESC (^)
		case Esc::ParenPrintDataAsCharacters: // 0x25e
			num_print_as_char = static_cast<uint16_t>(Param16(0));
			break;

		// Set page format (ESC (c)
		case Esc::ParenSetPageFormat: // 0x263
			if (defined_unit > 0) {
				const auto new_top = (static_cast<double>(Param16(2))) *
				                     defined_unit;

				const auto new_bottom = (static_cast<double>(
				                                Param16(4))) *
				                        defined_unit;

				if (new_top >= new_bottom) {
					break;
				}
				if (new_top < page_height) {
					top_margin = new_top;
				}
				if (new_bottom < page_height) {
					bottom_margin = new_bottom;
				}
				if (top_margin > cur_y) {
					cur_y = top_margin;
				}
				// LOG_MSG("du %d, p1 %d, p2 %d, new_top %f,
				// newbott %f, nt %f, nb %f, ph %f",
				//	static_cast<uint64_t>(defined_unit),Param16(2),Param16(4),top_margin,bottom_margin,
				//	new_top,new_bottom,page_height);
			}
			break;

		// Set relative vertical print position (ESC (v)
		case Esc::ParenSetRelativeVerticalPrintPosition: { // 0x276
			double unit_size = defined_unit;
			if (unit_size < 0) {
				unit_size = 1.0 / DefinedUnitDivisor;
			}
			const double new_pos = cur_y +
			                       (static_cast<double>(static_cast<int16_t>(
			                                Param16(2))) *
			                        unit_size);
			if (new_pos > top_margin) {
				if (new_pos > bottom_margin) {
					NewPage(true, false);
				} else {
					cur_y = new_pos;
				}
			}
		} break;

		default: {
			using Clock = std::chrono::steady_clock;
			static std::unordered_map<uint16_t, Clock::time_point> last_warned;

			const auto now = Clock::now();
			auto it        = last_warned.find(esc_cmd);

			if (it == last_warned.end() ||
			    now - it->second >= UnsupportedOpcodeLogInterval) {
				if (esc_cmd < 0x100) {
					LOG_WARNING(
					        "PRINTER: ESC %c (%02Xh) recognised but not "
					        "implemented; output may be incorrect",
					        esc_cmd,
					        esc_cmd);
				} else {
					LOG_WARNING(
					        "PRINTER: ESC ( %c (%02Xh) recognised but not "
					        "implemented; output may be incorrect",
					        esc_cmd - 0x200,
					        esc_cmd - 0x200);
				}

				last_warned[esc_cmd] = now;
			}
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
		double newX = cur_x - (1 / static_cast<double>(actual_cpi));

		if (hmi > 0) {
			newX = cur_x - hmi;
		}
		if (newX >= left_margin) {
			cur_x = newX;
		}
	}
		return true;

	// Tab horizontally (HT) -- jump to the *first* tab stop strictly
	// to the right of the current x position. (The previous loop kept
	// overwriting `move_to` with later matches and ended at the last
	// tab right of cur_x, which then usually fell past right_margin
	// and the tab was silently ignored.)
	case 0x09: {
		double move_to = -1;
		for (uint8_t i = 0; i < num_horiz_tabs; i++) {
			if (horiz_tabs[i] > cur_x) {
				move_to = horiz_tabs[i];
				break;
			}
		}
		if (move_to > 0 && move_to < right_margin) {
			cur_x = move_to;
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
			double move_to = -1;
			for (uint8_t i = 0; i < num_vert_tabs; i++) {
				if (vert_tabs[i] > cur_y) {
					move_to = vert_tabs[i];
				}
			}

			// Nothing found => Act like FF
			if (move_to > bottom_margin || move_to < 0) {
				NewPage(true, false);
			} else {
				cur_y = move_to;
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

	// Select double-width printing (one line) (SO)
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

	// FS (0x1C). Real Epson printers run in either ESC/P mode or IBM
	// ProPrinter emulation mode (DIP-switch selectable, mutually
	// exclusive). FS is only meaningful in IBM mode, which we don't
	// emulate. Discard the FS byte and the opcode byte that follows;
	// the latter is logged once per opcode (see top of this function).
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
		bit_graph.vert_dens    = (pins == 9) ? 72 : 60;
		bit_graph.adjacent     = true;
		bit_graph.bytes_column = 1;
		break;

	case 1:
		bit_graph.horiz_dens   = 120;
		bit_graph.vert_dens    = (pins == 9) ? 72 : 60;
		bit_graph.adjacent     = true;
		bit_graph.bytes_column = 1;
		break;

	case 2:
		bit_graph.horiz_dens   = 120;
		bit_graph.vert_dens    = (pins == 9) ? 72 : 60;
		bit_graph.adjacent     = false;
		bit_graph.bytes_column = 1;
		break;

	case 3:
		bit_graph.horiz_dens   = 240;
		bit_graph.vert_dens    = (pins == 9) ? 72 : 60;
		bit_graph.adjacent     = false;
		bit_graph.bytes_column = 1;
		break;

	case 4:
		bit_graph.horiz_dens   = 80;
		bit_graph.vert_dens    = (pins == 9) ? 72 : 60;
		bit_graph.adjacent     = true;
		bit_graph.bytes_column = 1;
		break;

	case 6:
		bit_graph.horiz_dens   = 90;
		bit_graph.vert_dens    = (pins == 9) ? 72 : 60;
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

	// Densities 71/72/73 are 48-pin modes (6 bytes per column).
	// Only printer in the ESC/P 2 reference (Dec 1997) that uses
	// these is the TLQ-4800 (F-75) - the world's first 48-pin
	// impact printer. Niche, but in scope: real driver exists.
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

	// modes (escp2ref.pdf C-177). The higher densities (>= 32) are
	// 24/48-pin only and aren't reachable from a 9-pin driver anyway.
	if (pins == 9 && density < 32) {
		bit_graph.vert_dens = 72;
	}

	bit_graph.bytes_left        = num_cols * bit_graph.bytes_column;
	bit_graph.read_bytes_column = 0;
	bit_graph.col_index         = 0;
	bit_graph.base_x            = cur_x;
	bit_graph.discard_data      = false;
}

// Bit-image rendering.
//
// The DOS app sends columns of bytes; each byte is 8 vertically-stacked
// dots; columns advance horizontally by 1/horiz_dens inch. We accept
// one byte per Write() call until a full column has arrived, then blit
// every set bit in that column onto the page bitmap.
//
// Dot placement uses linear coverage anti-aliasing rather than naive
// nearest-neighbour stamping. At any common combination of graphics
// density and page DPI the dot pitch in inches doesn't align with the
// page-pixel grid (a 60-dpi dot on a 360-dpi page is 6 pixels wide; a
// 72-dpi dot on a 233-dpi page lands on fractional pixel boundaries),
// so naive stamping produces visible staircase edges. Coverage AA
// spreads each dot's "ink" over the pixels it overlaps in proportion
// to the area of overlap.
//
// Overlapping dots accumulate intensity (capped at MaxIntensity)
// rather than replacing each other. This matches the dot-fattening of
// a real Epson head — overprinted dots are darker than single passes.
//
// Column and dot positions are derived from indices into the run (see
// SetupBitImage / bit_graph.col_index) rather than accumulating cur_x
// / cur_y one step at a time, to avoid floating-point drift across
// long bit-image runs.
void Printer::PrintBitGraph(const uint8_t ch)
{
	bit_graph.column[bit_graph.read_bytes_column++] = ch;
	bit_graph.bytes_left--;

	// ESC ^ (9-pin graphics) consumes bytes without rendering. Reset
	// the column accumulator each full column so the run ends cleanly
	// once bytes_left drains.
	if (bit_graph.discard_data) {
		if (bit_graph.read_bytes_column >= bit_graph.bytes_column) {
			bit_graph.read_bytes_column = 0;
		}
		if (bit_graph.bytes_left == 0) {
			bit_graph.discard_data = false;
		}
		return;
	}

	// Only print after reading a full column
	if (bit_graph.read_bytes_column < bit_graph.bytes_column) {
		return;
	}

	const auto inv_h = 1.0 / static_cast<double>(bit_graph.horiz_dens);
	const auto inv_v = 1.0 / static_cast<double>(bit_graph.vert_dens);

	const auto dot_w_px = dpi * inv_h;
	const auto dot_h_px = dpi * inv_v;

	const auto left_px = (bit_graph.base_x + bit_graph.col_index * inv_h) * dpi;
	const auto right_px = left_px + dot_w_px;
	const auto old_y_px = cur_y * dpi;

	int dot_index = 0;
	for (auto i = 0; i < bit_graph.bytes_column; ++i) {
		for (auto j = 128; j != 0; j >>= 1) {
			if (bit_graph.column[i] & j) {
				const auto top_px = old_y_px + dot_index * dot_h_px;
				const auto bottom_px = top_px + dot_h_px;

				BlitAntialiasedDot(left_px, right_px, top_px, bottom_px);
			}
			++dot_index;
		}
	}

	bit_graph.read_bytes_column = 0;
	++bit_graph.col_index;

	cur_x = bit_graph.base_x + bit_graph.col_index * inv_h;
}

void Printer::BlitAntialiasedDot(const double left_px, const double right_px,
                                 const double top_px, const double bottom_px)
{
	// Clip the dot's bounding box to the page pixel grid.
	const auto x0 = std::max(0, static_cast<int>(std::floor(left_px)));
	const auto x1 = std::min(page.width, static_cast<int>(std::ceil(right_px)));

	const auto y0 = std::max(0, static_cast<int>(std::floor(top_px)));
	const auto y1 = std::min(page.height,
	                         static_cast<int>(std::ceil(bottom_px)));

	for (int py = y0; py < y1; ++py) {
		// Vertical overlap of the dot with this pixel row, in [0, 1].
		const auto dy = std::min(static_cast<double>(py + 1), bottom_px) -
		                std::max(static_cast<double>(py), top_px);

		if (dy <= 0.0) {
			continue;
		}

		for (int px = x0; px < x1; ++px) {
			// Horizontal overlap with this pixel column.
			const auto dx = std::min(static_cast<double>(px + 1),
			                         right_px) -
			                std::max(static_cast<double>(px), left_px);

			if (dx <= 0.0) {
				continue;
			}

			// dx * dy is the area of the dot-pixel overlap in
			// [0, 1]. Maps directly to the fraction of
			// MaxIntensity to add to the pixel. Round to nearest.
			const auto coverage = dx * dy;
			const auto add      = static_cast<int>(
                                coverage * MaxIntensity + 0.5);

			auto& pixel = *reinterpret_cast<PagePixel*>(
			        &page.pixels[px + py * page.pitch]);

			const int new_intensity = std::min(MaxIntensity,
			                                   pixel.intensity + add);

			// Stamp the head's colour-ID over whatever was
			// there. Overprint semantics: a coloured dot landing
			// on a previously-coloured pixel replaces the
			// colour-ID but adds to the intensity.
			pixel.intensity = static_cast<uint8_t>(new_intensity);
			pixel.color_id  = static_cast<uint8_t>(pixel.color_id |
                                                              (color >> 5));
		}
	}
}

} // namespace VirtualPrinter
