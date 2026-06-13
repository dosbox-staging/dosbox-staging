// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

// ESC/P2 command dispatch — Printer::ProcessCommandChar plus the
// bit-image helpers (SetupBitImage, PrintBitGraph) that ESC commands
// in here drive.

#include "printer.h"

#include <chrono>
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
//   0x800 - 0x8ff : 'FS <ch>' commands (some are aliases of ESC variants)
namespace {

constexpr auto EscUndoc02                                          = 0x02;
constexpr auto EscReverseLineFeed                                  = 0x0a;
constexpr auto EscReturnToTopOfCurrentPage                         = 0x0c;
constexpr auto EscSelectDoubleWidthPrinting                        = 0x0e;
constexpr auto EscSelectCondensedPrinting                          = 0x0f;
constexpr auto EscControlPaperLoadingEjecting                      = 0x19;
constexpr auto EscSetIntercharacterSpace                           = 0x20;
constexpr auto EscMasterSelect                                     = 0x21;
constexpr auto EscCancelMsbControl                                 = 0x23;
constexpr auto EscSetAbsoluteHorizontalPrintPosition               = 0x24;
constexpr auto EscSelectUserDefinedSet                             = 0x25;
constexpr auto EscDefineUserDefinedCharacters                      = 0x26;
constexpr auto EscTwoBytesSequence                                 = 0x28;
constexpr auto EscSelectBitImage                                   = 0x2a;
constexpr auto EscSetN360InchLineSpacing                           = 0x2b;
constexpr auto EscTurnUnderlineOnOff                               = 0x2d;
constexpr auto EscSelectVerticalTabChannel                         = 0x2f;
constexpr auto EscSelect18InchLineSpacing                          = 0x30;
constexpr auto EscSelect760InchLineSpacing                         = 0x31;
constexpr auto EscSelect16InchLineSpacing                          = 0x32;
constexpr auto EscSetN180InchLineSpacing                           = 0x33;
constexpr auto EscSelectItalicFont                                 = 0x34;
constexpr auto EscCancelItalicFont                                 = 0x35;
constexpr auto EscEnablePrintingOfUpperControlCodes                = 0x36;
constexpr auto EscEnableUpperControlCodes                          = 0x37;
constexpr auto EscDisablePaperOutDetector                          = 0x38;
constexpr auto EscEnablePaperOutDetector                           = 0x39;
constexpr auto EscCopyRomToRam                                     = 0x3a;
constexpr auto EscUnidirectionalMode                               = 0x3c;
constexpr auto EscSetMsbTo0                                        = 0x3d;
constexpr auto EscSetMsbTo1                                        = 0x3e;
constexpr auto EscReassignBitImageMode                             = 0x3f;
constexpr auto EscInitializePrinter                                = 0x40;
constexpr auto EscSetN60InchLineSpacing                            = 0x41;
constexpr auto EscSetVerticalTabs                                  = 0x42;
constexpr auto EscSetPageLengthInLines                             = 0x43;
constexpr auto EscSetHorizontalTabs                                = 0x44;
constexpr auto EscSelectBoldFont                                   = 0x45;
constexpr auto EscCancelBoldFont                                   = 0x46;
constexpr auto EscSelectDoubleStrikePrinting                       = 0x47;
constexpr auto EscCancelDoubleStrikePrinting                       = 0x48;
constexpr auto EscSelectCharacterTypePrintPitch                    = 0x49;
constexpr auto EscAdvancePrintPositionVertically                   = 0x4a;
constexpr auto EscSelect60DpiGraphics                              = 0x4b;
constexpr auto EscSelect120DpiGraphics                             = 0x4c;
constexpr auto EscSelect105Point12Cpi                              = 0x4d;
constexpr auto EscSetBottomMargin                                  = 0x4e;
constexpr auto EscCancelBottomMargin                               = 0x4f;
constexpr auto EscSelect105Point10Cpi                              = 0x50;
constexpr auto EscSetRightMargin                                   = 0x51;
constexpr auto EscSelectInternationalCharacterSet                  = 0x52;
constexpr auto EscSelectSuperscriptSubscriptPrinting               = 0x53;
constexpr auto EscCancelSuperscriptSubscriptPrinting               = 0x54;
constexpr auto EscTurnUnidirectionalModeOnOff                      = 0x55;
constexpr auto EscTurnDoubleWidthPrintingOnOff                     = 0x57;
constexpr auto EscSelectFontPitchPoint                             = 0x58;
constexpr auto EscSelect120DpiDoubleSpeedGraphics                  = 0x59;
constexpr auto EscSelect240DpiGraphics                             = 0x5a;
constexpr auto EscSelectCharacterHeightWidthLineSpacing            = 0x5b;
constexpr auto EscSetRelativeHorizontalPrintPosition               = 0x5c;
constexpr auto EscEnablePrintingOfAllCharacterCodesOnNextCharacter = 0x5e;
constexpr auto EscSelectJustification                              = 0x61;
constexpr auto EscSetVerticalTabsInVfuChannels                     = 0x62;
constexpr auto EscSetHorizontalMotionIndex                         = 0x63;
constexpr auto EscSetVerticalTabStopsEveryNLines                   = 0x65;
constexpr auto EscAbsoluteHorizontalTabInColumns                   = 0x66;
constexpr auto EscSelect105Point15Cpi                              = 0x67;
constexpr auto EscSelectDoubleQuadrupleSize                        = 0x68;
constexpr auto EscImmediatePrint                                   = 0x69;
constexpr auto EscReversePaperFeed                                 = 0x6a;
constexpr auto EscSelectTypeface                                   = 0x6b;
constexpr auto EscSetLeftMargin                                    = 0x6c;
constexpr auto EscTurnProportionalModeOnOff                        = 0x70;
constexpr auto EscSelectPrintingColor                              = 0x72;
constexpr auto EscLowSpeedModeOnOff                                = 0x73;
constexpr auto EscSelectCharacterTable                             = 0x74;
constexpr auto EscTurnDoubleHeightPrintingOnOff                    = 0x77;
constexpr auto EscSelectLqDraft                                    = 0x78;
constexpr auto EscSelectDeselectSlashZero                          = 0x7e;
constexpr auto EscSetPageLengthInInches                            = 0x100;
constexpr auto EscSkipUnsupportedEscCommand                        = 0x101;
constexpr auto EscParenSelectLineScore                             = 0x22d;
constexpr auto EscParenBarCodeSetupPrint                           = 0x242;
constexpr auto EscParenSetPageLengthInDefinedUnit                  = 0x243;
constexpr auto EscParenSetUnit                                     = 0x255;
constexpr auto EscParenSetAbsoluteVerticalPrintPosition            = 0x256;
constexpr auto EscParenPrintDataAsCharacters                       = 0x25e;
constexpr auto EscParenSetPageFormat                               = 0x263;
constexpr auto EscParenAssignCharacterTable                        = 0x274;
constexpr auto EscParenSetRelativeVerticalPrintPosition            = 0x276;
constexpr auto FsSelect16InchLineSpacing                           = 0x832;
constexpr auto FsSetN360InchLineSpacing                            = 0x833;
constexpr auto FsSelectItalicFont                                  = 0x834;
constexpr auto FsCancelItalicFont                                  = 0x835;
constexpr auto FsSetN60InchLineSpacing                             = 0x841;
constexpr auto FsSelectLqTypeStyle                                 = 0x843;
constexpr auto FsSelectCharacterWidth                              = 0x845;
constexpr auto FsSelectForwardFeedMode                             = 0x846;
constexpr auto FsSelectCharacterTable                              = 0x849;
constexpr auto FsSelectReverseFeedMode                             = 0x852;
constexpr auto FsSelectHighSpeedHighDensityElitePitch              = 0x853;
constexpr auto FsTurnDoubleHeightPrintingOnOff                     = 0x856;
constexpr auto FsPrint24BitHexDensityGraphics                      = 0x85a;

// Per-opcode debounce interval for unsupported-command warnings.
// Many DOS apps emit the same unsupported opcode repeatedly; muting
// outright (once-per-session) hides bursts that come back later,
// logging every invocation drowns the log.
constexpr std::chrono::seconds UnsupportedOpcodeLogInterval{3};

} // namespace

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
		case EscUndoc02: // 0x02
		// (ESC LF) — Reverse line feed
		case EscReverseLineFeed: // 0x0a
		// (ESC FF) — Return to top of current page
		case EscReturnToTopOfCurrentPage: // 0x0c
		// (ESC SO) — Select double-width printing (one line)
		case EscSelectDoubleWidthPrinting: // 0x0e
		// (ESC SI) — Select condensed printing
		case EscSelectCondensedPrinting: // 0x0f
		// (ESC #) — Cancel MSB control
		case EscCancelMsbControl: // 0x23
		// (ESC 0) — Select 1/8-inch line spacing
		case EscSelect18InchLineSpacing: // 0x30
		// (ESC 1) — Select 7/60-inch line spacing
		case EscSelect760InchLineSpacing: // 0x31
		// (ESC 2) — Select 1/6-inch line spacing
		case EscSelect16InchLineSpacing: // 0x32
		// (ESC 4) — Select italic font
		case EscSelectItalicFont: // 0x34
		// (ESC 5) — Cancel italic font
		case EscCancelItalicFont: // 0x35
		// (ESC 6) — Enable printing of upper control codes
		case EscEnablePrintingOfUpperControlCodes: // 0x36
		// (ESC 7) — Enable upper control codes
		case EscEnableUpperControlCodes: // 0x37
		// (ESC 8) — Disable paper-out detector
		case EscDisablePaperOutDetector: // 0x38
		// (ESC 9) — Enable paper-out detector
		case EscEnablePaperOutDetector: // 0x39
		// (ESC <) — Unidirectional mode (one line)
		case EscUnidirectionalMode: // 0x3c
		// (ESC =) — Set MSB to 0
		case EscSetMsbTo0: // 0x3d
		// (ESC >) — Set MSB to 1
		case EscSetMsbTo1: // 0x3e
		// (ESC @) — Initialize printer
		case EscInitializePrinter: // 0x40
		// (ESC E) — Select bold font
		case EscSelectBoldFont: // 0x45
		// (ESC F) — Cancel bold font
		case EscCancelBoldFont: // 0x46
		// (ESC G) — Select double-strike printing
		case EscSelectDoubleStrikePrinting: // 0x47
		// (ESC H) — Cancel double-strike printing
		case EscCancelDoubleStrikePrinting: // 0x48
		// (ESC M) — Select 10.5-point, 12-cpi
		case EscSelect105Point12Cpi: // 0x4d
		// (ESC O) — Cancel bottom margin [conflict]
		case EscCancelBottomMargin: // 0x4f
		// (ESC P) — Select 10.5-point, 10-cpi
		case EscSelect105Point10Cpi: // 0x50
		// (ESC T) — Cancel superscript/subscript printing
		case EscCancelSuperscriptSubscriptPrinting: // 0x54
		// (ESC ^) — Enable printing of all character codes on next
		// character
		case EscEnablePrintingOfAllCharacterCodesOnNextCharacter: // 0x5e
		// (ESC g) — Select 10.5-point, 15-cpi
		case EscSelect105Point15Cpi: // 0x67

		// (FS 4)	(= ESC 4) — Select italic font
		case FsSelectItalicFont: // 0x834
		// (FS 5)	(= ESC 5) — Cancel italic font
		case FsCancelItalicFont: // 0x835
		// (FS F) — Select forward feed mode
		case FsSelectForwardFeedMode: // 0x846
		// (FS R) — Select reverse feed mode
		case FsSelectReverseFeedMode:
			needed_param = 0;
			break; // 0x852

		// (ESC EM) — Control paper loading/ejecting
		case EscControlPaperLoadingEjecting: // 0x19
		// (ESC SP) — Set intercharacter space
		case EscSetIntercharacterSpace: // 0x20
		// (ESC !) — Master select
		case EscMasterSelect: // 0x21
		// (ESC +) — Set n/360-inch line spacing
		case EscSetN360InchLineSpacing: // 0x2b
		// (ESC -) — Turn underline on/off
		case EscTurnUnderlineOnOff: // 0x2d
		// (ESC /) — Select vertical tab channel
		case EscSelectVerticalTabChannel: // 0x2f
		// (ESC 3) — Set n/180-inch line spacing
		case EscSetN180InchLineSpacing: // 0x33
		// (ESC A) — Set n/60-inch line spacing
		case EscSetN60InchLineSpacing: // 0x41
		// (ESC C) — Set page length in lines
		case EscSetPageLengthInLines: // 0x43
		// (ESC I) — Select character type and print pitch
		case EscSelectCharacterTypePrintPitch: // 0x49
		// (ESC J) — Advance print position vertically
		case EscAdvancePrintPositionVertically: // 0x4a
		// (ESC N) — Set bottom margin
		case EscSetBottomMargin: // 0x4e
		// (ESC Q) — Set right margin
		case EscSetRightMargin: // 0x51
		// (ESC R) — Select an international character set
		case EscSelectInternationalCharacterSet: // 0x52
		// (ESC S) — Select superscript/subscript printing
		case EscSelectSuperscriptSubscriptPrinting: // 0x53
		// (ESC U) — Turn unidirectional mode on/off
		case EscTurnUnidirectionalModeOnOff: // 0x55
		// case 0x56: // Repeat data
		// (ESC V)
		// (ESC W) — Turn double-width printing on/off
		case EscTurnDoubleWidthPrintingOnOff: // 0x57
		// (ESC a) — Select justification
		case EscSelectJustification: // 0x61
		// (ESC f) — Absolute horizontal tab in columns [conflict]
		case EscAbsoluteHorizontalTabInColumns: // 0x66
		// (ESC h) — Select double or quadruple size
		case EscSelectDoubleQuadrupleSize: // 0x68
		// (ESC i) — Immediate print
		case EscImmediatePrint: // 0x69
		// (ESC j) — Reverse paper feed
		case EscReversePaperFeed: // 0x6a
		// (ESC k) — Select typeface
		case EscSelectTypeface: // 0x6b
		// (ESC 1) — Set left margin
		case EscSetLeftMargin: // 0x6c
		// (ESC p) — Turn proportional mode on/off
		case EscTurnProportionalModeOnOff: // 0x70
		// (ESC r) — Select printing color
		case EscSelectPrintingColor: // 0x72
		// (ESC s) — Low-speed mode on/off
		case EscLowSpeedModeOnOff: // 0x73
		// (ESC t) — Select character table
		case EscSelectCharacterTable: // 0x74
		// (ESC w) — Turn double-height printing on/off
		case EscTurnDoubleHeightPrintingOnOff: // 0x77
		// (ESC x) — Select LQ or draft
		case EscSelectLqDraft: // 0x78
		// (ESC ~) — Select/Deselect slash zero
		case EscSelectDeselectSlashZero: // 0x7e

		// (FS 2)	(= ESC 2) — Select 1/6-inch line spacing
		case FsSelect16InchLineSpacing: // 0x832
		// (FS 3)	(= ESC +) — Set n/360-inch line spacing
		case FsSetN360InchLineSpacing: // 0x833
		// (FS A)	(= ESC A) — Set n/60-inch line spacing
		case FsSetN60InchLineSpacing: // 0x841
		// (FS C)	(= ESC k) — Select LQ type style
		case FsSelectLqTypeStyle: // 0x843
		// (FS E) — Select character width
		case FsSelectCharacterWidth: // 0x845
		// (FS I)	(= ESC t) — Select character table
		case FsSelectCharacterTable: // 0x849
		// (FS S) — Select High Speed/High Density elite pitch
		case FsSelectHighSpeedHighDensityElitePitch: // 0x853
		// (FS V)	(= ESC w) — Turn double-height printing on/off
		case FsTurnDoubleHeightPrintingOnOff:
			needed_param = 1;
			break; // 0x856

		// (ESC $) — Set absolute horizontal print position
		case EscSetAbsoluteHorizontalPrintPosition: // 0x24
		// (ESC ?) — Reassign bit-image mode
		case EscReassignBitImageMode: // 0x3f
		// (ESC K) — Select 60-dpi graphics
		case EscSelect60DpiGraphics: // 0x4b
		// (ESC L) — Select 120-dpi graphics
		case EscSelect120DpiGraphics: // 0x4c
		// (ESC Y) — Select 120-dpi, double-speed graphics
		case EscSelect120DpiDoubleSpeedGraphics: // 0x59
		// (ESC Z) — Select 240-dpi graphics
		case EscSelect240DpiGraphics: // 0x5a
		// (ESC \) — Set relative horizontal print position
		case EscSetRelativeHorizontalPrintPosition: // 0x5c
		// (ESC c) — Set horizontal motion index (HMI)	[conflict]
		case EscSetHorizontalMotionIndex: // 0x63
		// (ESC e) — Set vertical tab stops every n lines
		case EscSetVerticalTabStopsEveryNLines: // 0x65
		// (FS Z) — Print 24-bit hex-density graphics
		case FsPrint24BitHexDensityGraphics:
			needed_param = 2;
			break; // 0x85a

		// (ESC *) — Select bit image
		case EscSelectBitImage: // 0x2a
		// (ESC X) — Select font by pitch and point [conflict]
		case EscSelectFontPitchPoint:
			needed_param = 3;
			break; // 0x58

		// Select character height, width, line spacing
		case EscSelectCharacterHeightWidthLineSpacing:
			needed_param = 7;
			break; // 0x5b

		// (ESC b) — Set vertical tabs in VFU channels
		case EscSetVerticalTabsInVfuChannels: // 0x62
		// (ESC B) — Set vertical tabs
		case EscSetVerticalTabs:
			num_vert_tabs = 0;
			return true; // 0x42

		// (ESC D) — Set horizontal tabs
		case EscSetHorizontalTabs:
			num_horiz_tabs = 0;
			return true; // 0x44

		// (ESC %) — Select user-defined set
		case EscSelectUserDefinedSet: // 0x25
		// (ESC &) — Define user-defined characters
		case EscDefineUserDefinedCharacters: // 0x26
		// (ESC :) — Copy ROM to RAM
		case EscCopyRomToRam: // 0x3a
			LOG_ERR("PRINTER: User-defined characters not supported");
			return true;

		// Two bytes sequence
		case EscTwoBytesSequence: return true; // 0x28

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
		case EscParenBarCodeSetupPrint: // 0x242
		// Print data as characters (ESC (^)
		case EscParenPrintDataAsCharacters:
			needed_param = 2;
			break; // 0x25e

		// Set unit (ESC (U)
		case EscParenSetUnit:
			needed_param = 3;
			break; // 0x255

		// Set page length in defined unit (ESC (C)
		case EscParenSetPageLengthInDefinedUnit: // 0x243
		// Set absolute vertical print position (ESC (V)
		case EscParenSetAbsoluteVerticalPrintPosition: // 0x256
		// Set relative vertical print position (ESC (v)
		case EscParenSetRelativeVerticalPrintPosition:
			needed_param = 4;
			break; // 0x276

		// Assign character table (ESC (t)
		case EscParenAssignCharacterTable: // 0x274
		// Select line/score (ESC (-)
		case EscParenSelectLineScore:
			needed_param = 5;
			break; // 0x22d

		// Set page format (ESC (c)
		case EscParenSetPageFormat: needed_param = 6; break; // 0x263

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
		case EscUndoc02: // 0x02
			// Ignore
			break;

		// Select double-width printing (one line) (ESC SO)
		case EscSelectDoubleWidthPrinting: // 0x0e
			if (!multipoint) {
				hmi                       = -1;
				style.doublewidth_oneline = 1;
				UpdateFont();
			}
			break;

		// Select condensed printing (ESC SI)
		case EscSelectCondensedPrinting: // 0x0f
			if (!multipoint && (cpi != 15.0)) {
				hmi             = -1;
				style.condensed = 1;
				UpdateFont();
			}
			break;

		// Control paper loading/ejecting (ESC EM)
		case EscControlPaperLoadingEjecting: // 0x19
			// We are not really loading paper, so most commands can
			// be ignored
			if (params[0] == 'R') {
				NewPage(true, false);
			}
			break;

		// Set intercharacter space (ESC SP)
		case EscSetIntercharacterSpace: // 0x20
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
		case EscMasterSelect: // 0x21
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
		case EscCancelMsbControl:
			msb = 255;
			break; // 0x23

		// Set absolute horizontal print position (ESC $)
		case EscSetAbsoluteHorizontalPrintPosition: { // 0x24
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
		case FsPrint24BitHexDensityGraphics: // 0x85a
			SetupBitImage(40, static_cast<uint16_t>(Param16(0)));
			break;

		// Select bit image (ESC *)
		case EscSelectBitImage: // 0x2a
			SetupBitImage(params[0], static_cast<uint16_t>(Param16(1)));
			break;

		// Set n/360-inch line spacing (ESC + / FS 3) -- 24/48-pin only
		// per escp2ref.pdf C-29 ("Not available on 9-pin printers").
		// On a 9-pin printer model, ignore.
		case EscSetN360InchLineSpacing: // 0x2b
		case FsSetN360InchLineSpacing:  // 0x833
			if (pins != 9) {
				line_spacing = static_cast<Real64>(params[0]) / 360.0;
			}
			break;

		// Turn underline on/off (ESC -)
		case EscTurnUnderlineOnOff: // 0x2d
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
		case EscSelectVerticalTabChannel: // 0x2f
			// Ignore
			break;

		// Select 1/8-inch line spacing (ESC 0)
		case EscSelect18InchLineSpacing:
			line_spacing = static_cast<Real64>(1) / 8;
			break; // 0x30

		// Select 1/6-inch line spacing (ESC 2)
		case EscSelect16InchLineSpacing:
			line_spacing = static_cast<Real64>(1) / 6;
			break; // 0x32

		// Set line spacing (ESC 3) -- n/180 for 24/48-pin, n/216 for
		// 9-pin (escp2ref.pdf C-37).
		case EscSetN180InchLineSpacing: // 0x33
			line_spacing = static_cast<Real64>(params[0]) /
			               (pins == 9 ? 216.0 : 180.0);
			break;

		// Select italic font (ESC 4)
		case EscSelectItalicFont: // 0x34
			style.italics = 1;
			UpdateFont();
			break;

		// Cancel italic font (ESC 5)
		case EscCancelItalicFont: // 0x35
			style.italics = 0;
			UpdateFont();
			break;

		// Enable printing of upper control codes (ESC 6)
		case EscEnablePrintingOfUpperControlCodes:
			print_upper_contr = true;
			break; // 0x36

		// Enable upper control codes (ESC 7)
		case EscEnableUpperControlCodes:
			print_upper_contr = false;
			break; // 0x37

		// Unidirectional mode (one line) (ESC <)
		case EscUnidirectionalMode: // 0x3c
			// We don't have a print head, so just ignore this
			break;

		// Set MSB to 0 (ESC =)
		case EscSetMsbTo0:
			msb = 0;
			break; // 0x3d

		// Set MSB to 1 (ESC >)
		case EscSetMsbTo1:
			msb = 1;
			break; // 0x3e

		// Reassign bit-image mode (ESC ?)
		case EscReassignBitImageMode: // 0x3f
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
		case EscInitializePrinter:
			FormFeed();
			ResetPrinter();
			break; // 0x40

		// Set line spacing (ESC A / FS A) -- n/60 for 24/48-pin,
		// n/72 for 9-pin (escp2ref.pdf C-39).
		case EscSetN60InchLineSpacing: // 0x41
		case FsSetN60InchLineSpacing:  // 0x841
			line_spacing = static_cast<Real64>(params[0]) /
			               (pins == 9 ? 72.0 : 60.0);
			break;

		// Set page length in lines (ESC C)
		case EscSetPageLengthInLines: // 0x43
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
		case EscSelectBoldFont: // 0x45
			style.bold = 1;
			UpdateFont();
			break;

		// Cancel bold font (ESC F)
		case EscCancelBoldFont: // 0x46
			style.bold = 0;
			UpdateFont();
			break;

		// Select dobule-strike printing (ESC G)
		case EscSelectDoubleStrikePrinting:
			style.doublestrike = 1;
			break; // 0x47

		// Cancel double-strike printing (ESC H)
		case EscCancelDoubleStrikePrinting:
			style.doublestrike = 0;
			break; // 0x48

		// Advance print position vertically (ESC J n) -- n/180 for
		// 24/48-pin, n/216 for 9-pin (escp2ref.pdf R-62).
		case EscAdvancePrintPositionVertically: // 0x4a
			cur_y += static_cast<Real64>(params[0]) /
			         (pins == 9 ? 216.0 : 180.0);
			if (cur_y > bottom_margin) {
				NewPage(true, false);
			}
			break;

		// Select 60-dpi graphics (ESC K)
		case EscSelect60DpiGraphics: // 0x4b
			SetupBitImage(densk, static_cast<uint16_t>(Param16(0)));
			break;

		// Select 120-dpi graphics (ESC L)
		case EscSelect120DpiGraphics: // 0x4c
			SetupBitImage(densl, static_cast<uint16_t>(Param16(0)));
			break;

		// Select 10.5-point, 12-cpi (ESC M)
		case EscSelect105Point12Cpi: // 0x4d
			cpi        = 12;
			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		// Set bottom margin (ESC N)
		case EscSetBottomMargin: // 0x4e
			top_margin = 0.0;
			bottom_margin = static_cast<Real64>(params[0]) * line_spacing;
			break;

		// Cancel bottom (and top) margin
		case EscCancelBottomMargin: // 0x4f
			top_margin    = 0.0;
			bottom_margin = page_height;
			break;

		// Select 10.5-point, 10-cpi (ESC P)
		case EscSelect105Point10Cpi: // 0x50
			cpi        = 10;
			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		// Set right margin
		case EscSetRightMargin: // 0x51
			right_margin = static_cast<Real64>(params[0] - 1.0) /
			               static_cast<Real64>(cpi);
			break;

		// Select an international character set (ESC R)
		case EscSelectInternationalCharacterSet: // 0x52
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
		case EscSelectSuperscriptSubscriptPrinting: // 0x53
			if (params[0] == 0 || params[0] == 48) {
				style.superscript = 1;
			}
			if (params[0] == 1 || params[0] == 49) {
				style.subscript = 1;
			}
			UpdateFont();
			break;

		// Cancel superscript/subscript printing (ESC T)
		case EscCancelSuperscriptSubscriptPrinting: // 0x54
			style.superscript = 0;
			style.subscript   = 0;
			UpdateFont();
			break;

		// Turn unidirectional mode on/off (ESC U)
		case EscTurnUnidirectionalModeOnOff: // 0x55
			// We don't have a print head, so just ignore this
			break;

		// Turn double-width printing on/off (ESC W)
		case EscTurnDoubleWidthPrintingOnOff: // 0x57
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
		case EscSelectFontPitchPoint: // 0x58
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
		case EscSelect120DpiDoubleSpeedGraphics: // 0x59
			SetupBitImage(densy, static_cast<uint16_t>(Param16(0)));
			break;

		// Select 240-dpi graphics (ESC Z)
		case EscSelect240DpiGraphics: // 0x5a
			SetupBitImage(densz, static_cast<uint16_t>(Param16(0)));
			break;

		// Set relative horizontal print position (ESC \)
		case EscSetRelativeHorizontalPrintPosition: { // 0x5c
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
		case EscSelectJustification: // 0x61
			// Ignore
			break;

		// Set horizontal motion index (HMI) (ESC c)
		case EscSetHorizontalMotionIndex: // 0x63
			hmi = static_cast<Real64>(Param16(0)) /
			      static_cast<Real64>(360.0);
			extra_intra_space = 0.0;
			break;

		// Select 10.5-point, 15-cpi (ESC g)
		case EscSelect105Point15Cpi: // 0x67
			cpi        = 15;
			hmi        = -1;
			multipoint = false;
			UpdateFont();
			break;

		// implemented yet — Select forward feed mode (FS F) - set
		// reverse not
		case FsSelectForwardFeedMode: // 0x846
			if (line_spacing < 0) {
				line_spacing *= -1;
			}
			break;

		// Reverse paper feed (ESC j). The parameter is a single byte
		// (range 0-255) per the ESC/P 2 reference (C-213). The handler
		// originally read Param16 (two bytes), which made the reverse
		// amount depend on whatever was left in params[1] from the
		// previous command.
		case EscReversePaperFeed: { // 0x6a
			Real64 reverse = static_cast<Real64>(params[0]) /
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
		case EscSelectTypeface: // 0x6b
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
		case EscSetLeftMargin: // 0x6c
			left_margin = static_cast<Real64>(params[0] - 1.0) /
			              static_cast<Real64>(cpi);

			if (left_margin < 0.0) {
				left_margin = 0.0;
			}
			if (cur_x < left_margin) {
				cur_x = left_margin;
			}
			break;

		// Turn proportional mode on/off (ESC p)
		case EscTurnProportionalModeOnOff: // 0x70
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
		case EscSelectPrintingColor: // 0x72

			if (params[0] == 0 || params[0] > 6) {
				color = ColorBlack;
			} else {
				color = static_cast<uint8_t>(params[0] << 5);
			}
			break;

		// Select low-speed mode (ESC s)
		case EscLowSpeedModeOnOff: // 0x73
			// Ignore
			break;

		// Select character table (ESC t)
		case EscSelectCharacterTable: // 0x74
		// Select character table (FS I)
		case FsSelectCharacterTable: // 0x849
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
		case EscTurnDoubleHeightPrintingOnOff: // 0x77
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
		case EscSelectLqDraft: // 0x78
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
		case EscSetPageLengthInInches: // 0x100
			page_height   = static_cast<Real64>(params[0]);
			bottom_margin = page_height;
			top_margin    = 0.0;
			break;

		// Skip unsupported ESC ( command
		case EscSkipUnsupportedEscCommand: // 0x101
			needed_param = static_cast<uint8_t>(Param16(0));
			num_param    = 0;
			break;

		// Assign character table (ESC (t)
		case EscParenAssignCharacterTable: // 0x274
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
		case EscParenSelectLineScore: // 0x22d
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
		case EscParenBarCodeSetupPrint: // 0x242
			LOG_ERR("PRINTER: Barcode printing not supported");
			// Find out how many bytes to skip
			needed_param = static_cast<uint8_t>(Param16(0));
			num_param    = 0;
			break;

		// Set page length in defined unit (ESC (C)
		case EscParenSetPageLengthInDefinedUnit: // 0x243
			if (params[0] != 0 && defined_unit > 0) {
				page_height = bottom_margin = (static_cast<Real64>(
				                                      Param16(2))) *
				                              defined_unit;
				top_margin = 0.0;
			}
			break;

		// Set unit (ESC (U)
		case EscParenSetUnit: // 0x255
			defined_unit = static_cast<Real64>(params[2]) /
			               static_cast<Real64>(3600);
			break;

		// Set absolute vertical print position (ESC (V)
		case EscParenSetAbsoluteVerticalPrintPosition: { // 0x256
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
		case EscParenPrintDataAsCharacters: // 0x25e
			num_print_as_char = static_cast<uint16_t>(Param16(0));
			break;

		// Set page format (ESC (c)
		case EscParenSetPageFormat: // 0x263
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
		case EscParenSetRelativeVerticalPrintPosition: { // 0x276
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
		bit_graph.horiz_dens   = 240;
		bit_graph.vert_dens    = 60;
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

	// 9-pin printers use 1/72-inch vertical pitch for the low-density
	// modes (dot density < 32); higher densities are 24/48-pin only and
	// aren't reachable from a 9-pin driver anyway.
	if (pins == 9 && density < 32) {
		bit_graph.vert_dens = 72;
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

	// When page dpi is greater than graphics dpi, each dot of the
	// graphics stream becomes a rectangle of (dpi/h_dens) x (dpi/v_dens)
	// page pixels. Scaling is unconditional — the `adjacent` flag is a
	// print-head physical property unrelated to pixel size (ESC/P 2
	// reference C-177).
	const uint64_t pixsizeX = dpi / bit_graph.horiz_dens > 0
	                                ? dpi / bit_graph.horiz_dens
	                                : 1;
	const uint64_t pixsizeY = dpi / bit_graph.vert_dens > 0
	                                ? dpi / bit_graph.vert_dens
	                                : 1;

	for (uint64_t i = 0; i < bit_graph.bytes_column; i++) // for each byte
	{
		for (uint64_t j = 128; j != 0; j >>= 1) { // for each bit
			if (bit_graph.column[i] & j) {
				for (uint64_t xx = 0; xx < pixsizeX; xx++) {
					for (uint64_t yy = 0; yy < pixsizeY; yy++) {
						if ((static_cast<int>(PixX() + xx) <
						     page.width) &&
						    (static_cast<int>(PixY() + yy) <
						     page.height)) {
							page.pixels[(PixX() + xx) +
							            (PixY() + yy) *
							                    page.pitch] |=
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

	cur_y = oldY;

	bit_graph.read_bytes_column = 0;

	// Advance to the left
	cur_x += static_cast<Real64>(1) / static_cast<Real64>(bit_graph.horiz_dens);
}

} // namespace VirtualPrinter
