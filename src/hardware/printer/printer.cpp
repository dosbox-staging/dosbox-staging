// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "printer.h"

#include "private/printer.h"

#include "capture/capture.h"
#include "capture/image/png_writer.h"
#include "config/config.h"
#include "config/setup.h"
#include "gui/mapper.h"
#include "hardware/parallelport/lpt.h"
#include "hardware/port.h"
#include "misc/cross.h"
#include "misc/logging.h"
#include "misc/notifications.h"
#include "misc/support.h"
#include "private/printer_charmaps.h"
#include <array>
#include <cstdlib>
#include <math.h>
#include <memory>
#include <utility>
#include <vector>

#include <zlib.h>

#include "misc/std_filesystem.h"
#include "private/postscript_passthrough.h"
#include "private/raw_passthrough.h"
#include "utils/string_utils.h"

// PIC_AddEvent / PIC_RemoveEvents drive the form-feed timeout.
#include "hardware/pic.h"
#include "utils/checks.h"

CHECK_NARROWING();

using VirtualPrinter::Printer;
using VirtualPrinter::PrinterModel;

static void printer_event_handler([[maybe_unused]] uint32_t param);

static std::unique_ptr<Printer> default_printer = nullptr;
static std::unique_ptr<VirtualPrinter::PostScriptPassthrough> postscript_sink = nullptr;
static std::unique_ptr<VirtualPrinter::RawPassthrough> raw_sink = nullptr;

static PrinterModel conf_model    = PrinterModel::None;
static int conf_dpi               = 360;
static double conf_page_width_in  = 8.27;  // A4
static double conf_page_height_in = 11.69; // A4
static int conf_pins              = 24;    // 9 = FX/LX, 24 = LQ
static int printer_timeout        = 500;

namespace VirtualPrinter {

namespace {

// Default power-on / ESC @ font and layout settings (escp2ref.pdf C-69
// "Initialize Printer").
constexpr double DefaultCpi           = 10.0;
constexpr double DefaultLineSpacingIn = 1.0 / 6.0;
constexpr int DefaultTabIntervalChars = 8;

// ESC = / ESC > target bit 7 of every byte that follows; ESC # cancels
// the override. Modelled as a bit_view so the cast direction and bit
// position read directly from the field name rather than from 0x80.
union ByteWithMsbBit {
	uint8_t data = 0;
	bit_view<7, 1> msb;
};

// Unicode range covering CP437 box-drawing glyphs (U+2500..U+257F) and
// block elements (U+2580..U+259F). Both are designed to tile across
// cell boundaries; we render them from a monospace fallback to keep
// the grid aligned regardless of the active typeface.
constexpr uint16_t BoxAndBlockGlyphsFirst = 0x2500;
constexpr uint16_t BoxAndBlockGlyphsLast  = 0x259F;

} // namespace

// Printer::FillPalette lives in printer_glyph.cpp.

Printer::Printer(const int dpi, const double page_width_in,
                 const double page_height_in, const int pins)
{
	if (FT_Init_FreeType(&ft_lib)) {
		LOG_ERR("PRINTER: Unable to init Freetype2. Printing disabled");
		return;
	}

	this->dpi  = dpi;
	this->pins = pins;

	default_page_width  = page_width_in;
	default_page_height = page_height_in;

	page.width  = static_cast<int>(default_page_width * dpi);
	page.height = static_cast<int>(default_page_height * dpi);
	page.pitch  = page.width;
	page.pixels.assign(static_cast<size_t>(page.pitch) *
	                           static_cast<size_t>(page.height),
	                   0);

	// The 8-bit palette is sliced into 8 sub-palettes of 32 entries
	// each. The high 3 bits of a pixel select the sub-palette (the
	// 'colour ID'), the low 5 bits select intensity within that
	// sub-palette (0 = white, 31 = saturated).
	//
	// FillPalette's first three args are CMY-dye-coverage maxima
	// (subtractive), not RGB light. Each sub-palette's full-coverage
	// light output is (1 - r_max, 1 - g_max, 1 - b_max), so the
	// indices below double as ESC r colour IDs (escp2ref.pdf C-193) AND as
	// OR-overprint bit patterns (M=001, C=010, Y=100):
	//   1 = Magenta            (M)
	//   2 = Cyan               (C)
	//   3 = Violet / Blue      (M|C)
	//   4 = Yellow             (Y)
	//   5 = Red                (M|Y)
	//   6 = Green              (C|Y)
	//   7 = Black              (M|C|Y) -- also BlackColorId
	//
	// Sub-palette 0 is the 'all white' page background.
	for (int i = 0; i < 32; ++i) {
		page.palette[i] = Rgb888{255, 255, 255};
	}
	FillPalette(0, 255, 0, 1);
	FillPalette(255, 0, 0, 2);
	FillPalette(255, 255, 0, 3);
	FillPalette(0, 0, 255, 4);
	FillPalette(0, 255, 255, 5);
	FillPalette(255, 0, 255, 6);
	FillPalette(255, 255, 255, 7);

	color.color_id = BlackColorId;

	cur_font  = nullptr;
	char_read = false;
	auto_feed = false;

	ResetPrinter();

	LOG_MSG("PRINTER: Print job started");
}

void Printer::ResetPrinterHard()
{
	char_read = false;
	ResetPrinter();
}

void Printer::ResetPrinter()
{
	color.color_id = BlackColorId;
	esc_seen       = false;
	fs_seen        = false;
	esc_cmd   = 0;
	num_param = needed_param = 0;

	// Default printable-area margins of 0.25 inch on all sides
	// approximate the non-printable area of real Epson dot-matrix
	// printers (see escp2ref.pdf Feature Summary per-model "Nonprintable
	// area" lines, e.g. F-33 for LQ-850).
	constexpr double DefaultMarginIn = 0.25;

	top_margin    = DefaultMarginIn;
	left_margin   = DefaultMarginIn;
	page_width    = default_page_width;
	page_height   = default_page_height;
	right_margin  = default_page_width - DefaultMarginIn;
	bottom_margin = default_page_height - DefaultMarginIn;

	cur_x                = left_margin;
	cur_y                = top_margin;
	line_spacing         = DefaultLineSpacingIn;
	cpi                  = DefaultCpi;
	cur_char_table       = 1;
	style.data           = 0;
	extra_intra_space    = 0.0;
	print_upper_contr    = true;
	bit_graph.bytes_left = 0;

	densk = 0;
	densl = 1;
	densy = 2;
	densz = 3;

	// Char table slot 0 is reserved for italics; slots 1..3 default to CP437.
	char_tables[0] = 0;
	char_tables[1] = char_tables[2] = char_tables[3] = 437;

	defined_unit      = -1;
	multipoint        = false;
	multi_point_size  = 0.0;
	multi_cpi         = 0.0;
	hmi               = -1.0;
	msb               = MsbModeDisabled;
	num_print_as_char = 0;
	lq_typeface       = Typeface::Roman;

	SelectCodepage(char_tables[cur_char_table]);

	UpdateFont();

	NewPage(false, true);

	// Default tabs -- one every 8 characters, measured from the left
	// margin (not from the page origin). Matches the Epson ESC/P spec
	// (ESC D description: "absolute position from the left-margin
	// position").
	const int default_num_horiz_tabs = static_cast<int>(horiz_tabs.size());
	for (int i = 0; i < default_num_horiz_tabs; ++i) {
		horiz_tabs[i] = left_margin +
		                static_cast<double>((i + 1) * DefaultTabIntervalChars) /
		                        static_cast<double>(cpi);
	}
	num_horiz_tabs = default_num_horiz_tabs;

	num_vert_tabs = VerticalTabsNotConfigured;
}

Printer::~Printer(void)
{
	if (ft_lib != nullptr) {
		FT_Done_FreeType(ft_lib);
		ft_lib = nullptr;
	}
}

void Printer::SelectCodepage(const uint16_t codepage)
{
	const uint16_t* map_to_use = nullptr;

	int i = 0;
	while (charmap[i].codepage != 0) {
		if (charmap[i].codepage == codepage) {
			map_to_use = charmap[i].map;
			break;
		}
		++i;
	}
	if (map_to_use == nullptr) {
		LOG_WARNING("PRINTER: Unsupported codepage %i. Using CP437 instead.",
		            codepage);
		SelectCodepage(437);
		return;
	}

	for (int j = 0; j < 256; ++j) {
		cur_map[j] = map_to_use[j];
	}
}

void Printer::NewPage(const bool save, const bool reset_x)
{
	PIC_RemoveEvents(printer_event_handler);

	if (save) {
		OutputPage();
	}

	if (reset_x) {
		cur_x = left_margin;
	}
	cur_y = top_margin;

	// Palette index 0 lives in sub-palette 0 (all-white), so a zeroed
	// pixel buffer is a blank white page.
	std::fill(page.pixels.begin(), page.pixels.end(), 0);
}

void Printer::PrintChar(uint8_t ch)
{
	char_read = true;
	if (page.pixels.empty()) {
		return;
	}

	// Don't think that DOS programs uses this but well: Apply MSB if
	// desired. Both active modes (MsbModeForceLow = 0, MsbModeForceHigh
	// = 1) write the mode value straight into bit 7.
	if (msb != MsbModeDisabled) {
		ByteWithMsbBit byte{};
		byte.data = ch;
		byte.msb  = msb;
		ch        = byte.data;
	}

	// Are we currently printing a bit graphic?
	if (bit_graph.bytes_left > 0) {
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

	// Box-drawing chars (U+2500..U+257F) and block elements
	// (U+2580..U+259F) are designed to tile across cell boundaries. In
	// fixed-pitch mode we render them from the mono fallback face
	// (independent of the active typeface), matching real Epson which
	// shipped a typeface-agnostic box-drawing bitmap set. The face is
	// then temporarily stretched in both axes so its em exactly fills
	// the 1/cpi horizontal cell and the line_spacing vertical cell;
	// the bitmap overhang baked into each glyph bridges the seam.
	const auto unicode = cur_map[ch];

	const bool is_box_char = unicode >= BoxAndBlockGlyphsFirst &&
	                         unicode <= BoxAndBlockGlyphsLast;

	FT_Face render_face = (is_box_char && !style.prop && mono_box_font != nullptr)
	                            ? mono_box_font
	                            : cur_font;

	const bool apply_box_fill = is_box_char && box_fill_horiz_points > 0.0 &&
	                            line_spacing > 0.0 && natural_em_height_px > 0;

	int box_fill_em_h_px   = 0;
	int box_fill_em_asc_px = 0;

	if (apply_box_fill) {
		const auto cell_h_px = static_cast<int>(line_spacing * dpi);

		const auto box_fill_vert_points = cur_vert_points *
		                                  BoxFillOvershootVertical *
		                                  static_cast<double>(cell_h_px) /
		                                  static_cast<double>(
		                                          natural_em_height_px);

		FT_Set_Char_Size(render_face,
		                 static_cast<FT_F26Dot6>(box_fill_horiz_points *
		                                         Ft26Dot6Unit),
		                 static_cast<FT_F26Dot6>(box_fill_vert_points *
		                                         Ft26Dot6Unit),
		                 dpi,
		                 dpi);

		box_fill_em_h_px = ft26_6_to_pixels(render_face->size->metrics.height);

		box_fill_em_asc_px = ft26_6_to_pixels(
		        render_face->size->metrics.ascender);
	}

	// Find the glyph for the char to render
	FT_UInt index = FT_Get_Char_Index(render_face, unicode);

	// Load the glyph
	FT_Load_Glyph(render_face, index, FT_LOAD_DEFAULT);

	// Render a high-quality bitmap
	FT_Render_Glyph(render_face->glyph, FT_RENDER_MODE_NORMAL);

	if (apply_box_fill) {
		FT_Set_Char_Size(render_face,
		                 static_cast<FT_F26Dot6>(cur_horiz_points * Ft26Dot6Unit),
		                 static_cast<FT_F26Dot6>(cur_vert_points * Ft26Dot6Unit),
		                 dpi,
		                 dpi);
	}

	// Fixed-pitch centring. A proportional font (Roman, Sans Serif)
	// forced into a 1/actual_cpi inch cell would otherwise sit
	// left-aligned with a visible gap on the right of every char.
	// Shift narrow glyphs to the middle of the cell to even that out.
	//
	// Skip this for native monospace fonts. Their bitmap_left already
	// positions glyphs correctly within the advance cell, and shifting
	// would misalign the CP437 box-drawing characters, where `│`/`─`/`┼`
	// would land at different X positions and break grids.
	int centre_offset = 0;

	if (!style.prop && actual_cpi > 0.0 && !FT_IS_FIXED_WIDTH(render_face)) {
		const auto cell_px = static_cast<int>(dpi / actual_cpi);
		const auto glyph_w = static_cast<int>(render_face->glyph->bitmap.width);

		if (cell_px > glyph_w) {
			centre_offset = static_cast<int>((cell_px - glyph_w) / 2);
		}
	}

	// For box-fill chars, centre the stretched em on the cell using a
	// constant offset so every box-drawing glyph gets the same
	// horizontal positioning -- features anchored to em-centre
	// (verticals, line crossings) all land at cell-centre across the
	// grid. Centring by the glyph's own bitmap.width would shift each
	// glyph differently and break grid alignment.
	int signed_pen_x = 0;

	if (apply_box_fill && actual_cpi > 0.0) {
		const auto cell_px = static_cast<int>(dpi / actual_cpi);

		signed_pen_x = static_cast<int>(PixX()) +
		               (cell_px - box_fill_em_px) / 2 +
		               render_face->glyph->bitmap_left;
	} else {
		signed_pen_x = static_cast<int>(PixX()) +
		               render_face->glyph->bitmap_left + centre_offset;
	}

	const int pen_x = signed_pen_x < 0 ? 0 : signed_pen_x;

	// Anchor every glyph to the per-line baseline captured in
	// UpdateFont() so that double-height chars extend upward from the
	// shared baseline and normal-size chars on the same line stay on
	// the baseline. Box-fill chars override this with em-centring on
	// the line cell.
	int signed_pen_y = 0;

	if (apply_box_fill) {
		const auto cell_h_px = static_cast<int>(line_spacing * dpi);

		const auto em_top_y = static_cast<int>(PixY()) +
		                      (cell_h_px - box_fill_em_h_px) / 2;

		signed_pen_y = em_top_y + box_fill_em_asc_px -
		               render_face->glyph->bitmap_top;
	} else {
		signed_pen_y = static_cast<int>(PixY()) + line_baseline_anchor_px -
		               render_face->glyph->bitmap_top;
	}
	int pen_y = signed_pen_y < 0 ? 0 : signed_pen_y;

	// Sub- and superscript vertical shift. The spec (escp2ref.pdf
	// C-129) only describes the *direction* ("lower part" vs "upper
	// part" of normal character space) and leaves the exact offset to
	// the printer. We use rise = point_size / 3 (in PDF baseline units,
	// converted to pixels in UpdateFont).
	if (style.subscript) {
		pen_y += subscript_shift_px;
	}
	if (style.superscript) {
		pen_y -= superscript_shift_px;
	}

	// Copy bitmap into page
	BlitGlyph(render_face->glyph->bitmap, pen_x, pen_y, false);
	BlitGlyph(render_face->glyph->bitmap, pen_x + 1, pen_y, true);

	// Doublestrike => Print the glyph a second time one pixel below
	if (style.doublestrike) {
		BlitGlyph(render_face->glyph->bitmap, pen_x, pen_y + 1, true);
		BlitGlyph(render_face->glyph->bitmap, pen_x + 1, pen_y + 1, true);
	}

	// Bold => Print the glyph a second time one pixel to the right
	// or be a bit more bold...
	if (style.bold) {
		BlitGlyph(render_face->glyph->bitmap, pen_x + 1, pen_y, true);
		BlitGlyph(render_face->glyph->bitmap, pen_x + 2, pen_y, true);
		BlitGlyph(render_face->glyph->bitmap, pen_x + 3, pen_y, true);
	}

	// For line printing
	const auto line_start = PixX();

	// advance the cursor to the right
	double x_advance;

	if (style.prop) {
		// advance.x is in 26.6 pixel units; divide by dpi *
		// Ft26Dot6Unit in a single step to keep sub-pixel precision
		// before converting to inches.
		x_advance = static_cast<double>(render_face->glyph->advance.x) /
		            static_cast<double>(dpi * Ft26Dot6Unit);
	} else {
		if (hmi < 0) {
			x_advance = 1 / static_cast<double>(actual_cpi);
		} else {
			x_advance = hmi;
		}
	}

	x_advance += extra_intra_space;
	cur_x += x_advance;

	// Draw lines if desired
	if ((score != ScoreType::None) &&
	    (style.underline || style.strikethrough || style.overscore)) {

		// Find out where to put the line.
		int line_y = PixY();

		const auto height = ft26_6_to_pixels(cur_font->size->metrics.height);

		if (style.underline) {
			line_y = PixY() + static_cast<int>(height * 0.9);

		} else if (style.strikethrough) {
			line_y = PixY() + static_cast<int>(height * 0.45);

		} else if (style.overscore) {
			line_y = PixY() - (((score == ScoreType::Double) ||
			                    (score == ScoreType::DoubleBroken))
			                           ? 5
			                           : 0);
		}

		DrawLine(line_start,
		         PixX(),
		         line_y,
		         (score == ScoreType::SingleBroken) ||
		                 (score == ScoreType::DoubleBroken));

		// draw second line if needed
		if ((score == ScoreType::Double) ||
		    (score == ScoreType::DoubleBroken)) {

			// score is DOUBLE or DOUBLEBROKEN here; the upstream
			// expression also tested ScoreType::SingleBroken which
			// is unreachable in this branch.
			DrawLine(line_start,
			         PixX(),
			         line_y + 5,
			         (score == ScoreType::DoubleBroken));
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

void Printer::FormFeed()
{
	// Don't output blank pages
	NewPage(!IsBlank(), true);
}

void Printer::OutputPage()
{
	FILE_unique_ptr fp{CAPTURE_CreateFile(CaptureType::PrinterPng)};
	if (!fp) {
		return;
	}

	PngWriter w;

	// Pages favour file size over encode speed; level 9 is the long-
	// standing dot-matrix output setting.
	w.SetCompressionLevel(Z_BEST_COMPRESSION);

	if (!w.InitIndexed8(fp.get(), page.width, page.height, page.palette)) {
		return;
	}

	for (int y = 0; y < page.height; ++y) {
		w.WriteRow(page.pixels.begin() + y * page.pitch);
	}
}

bool Printer::IsBlank()
{
	for (uint8_t b : page.pixels) {
		if (b != 0) {
			return false;
		}
	}
	return true;
}

uint8_t Printer::GetPixel(const int num)
{
	return page.pixels[(num % page.width) + ((num / page.width) * page.pitch)];
}

} // namespace VirtualPrinter

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

io_val_t PRINTER_ReadData([[maybe_unused]] io_port_t port,
                          [[maybe_unused]] io_width_t width)
{
	return lpt.data;
}

void PRINTER_WriteData([[maybe_unused]] io_port_t port, io_val_t val,
                       [[maybe_unused]] io_width_t width)
{
	lpt.data = static_cast<uint8_t>(val);
}

io_val_t PRINTER_ReadStatus([[maybe_unused]] io_port_t port,
                            [[maybe_unused]] io_width_t width)
{
	// Don't create a Printer unless the program really wants to print.
	if (!default_printer) {
		return StatusNoPrinter;
	}

	// Printer is always online and never reports an error.
	uint8_t status  = StatusReservedLow;
	const bool busy = default_printer->IsBusy();
	if (!busy) {
		status |= StatusBusyHigh;
	}

	if (!default_printer->Ack()) {
		status |= StatusAckHigh;
	}

	return status;
}

enum class FormFeedSource {
	UserEjectKey,
	IdleTimeout,
};

static void trigger_form_feed(const FormFeedSource source)
{
	PIC_RemoveEvents(printer_event_handler);

	const char* source_str = (source == FormFeedSource::UserEjectKey)
	                               ? "user eject key"
	                               : "idle timeout";

	if (default_printer) {
		LOG_MSG("PRINTER: Form feed (%s)", source_str);
		default_printer->FormFeed();
	}
	// PostScript is self-describing — the driver's own bytes delimit
	// pages and jobs. Form-feed events (idle timeout / Ctrl+F2) do not
	// close the file; only the PS end-of-job marker `^D` (handled inside
	// the sink) does.
	if (raw_sink) {
		LOG_MSG("PRINTER: Closing raw passthrough job (%s)", source_str);
		raw_sink->Close();
	}
}

static void printer_event_handler([[maybe_unused]] const uint32_t param)
{
	trigger_form_feed(FormFeedSource::IdleTimeout);
}

void PRINTER_WriteControl([[maybe_unused]] io_port_t port, io_val_t val,
                          [[maybe_unused]] io_width_t width)
{
	// Rising edge on the INITIALISE bit triggers a hard reset.
	if ((val & CtrlInitialise) && default_printer &&
	    !(lpt.control & CtrlInitialise)) {
		default_printer->ResetPrinterHard();
	}

	// Data is strobed to the printer on the falling edge of the STROBE bit.
	if (!(val & CtrlStrobe) && (lpt.control & CtrlStrobe)) {
		if (conf_model == PrinterModel::EpsonDotMatrix) {

			if (!default_printer) {
				default_printer = std::make_unique<Printer>(
				        conf_dpi,
				        conf_page_width_in,
				        conf_page_height_in,
				        conf_pins);
			}
			if (default_printer) {
				default_printer->PrintChar(lpt.data);
			}

		} else if (conf_model == PrinterModel::PostScript) {
			if (!postscript_sink) {
				postscript_sink =
				        std::make_unique<VirtualPrinter::PostScriptPassthrough>();
			}
			postscript_sink->Write(lpt.data);

		} else if (conf_model == PrinterModel::RawPassthrough) {
			if (!raw_sink) {
				raw_sink = std::make_unique<VirtualPrinter::RawPassthrough>();
			}
			raw_sink->Write(lpt.data);
		}
		// PostScript is self-regulating — the inactivity timeout
		// does not apply. The other models still need it for their
		// auto-eject behaviour.
		if (printer_timeout && conf_model != PrinterModel::PostScript) {
			PIC_RemoveEvents(printer_event_handler);
			PIC_AddEvent(printer_event_handler,
			             static_cast<float>(printer_timeout),
			             0);
		}
	}

	lpt.control = static_cast<uint8_t>(val);
	if (default_printer) {
		default_printer->SetAutofeed((val & CtrlAutoLf) != 0);
	}
}

io_val_t PRINTER_ReadControl([[maybe_unused]] io_port_t port,
                             [[maybe_unused]] io_width_t width)
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
// via PRINTER_Reset, so the function is no longer reachable and has been
// removed.

// C-style hooks driven by the config/init code further down in this file:
//   PRINTER_Configure   -- writes the static config values (dpi, paper
//                          size, output format, etc.) that the lazy
//                          Printer constructor reads on first use.
//   PRINTER_FormFeed    -- public wrapper around the static
//                          trigger_form_feed used by the mapper
//                          handler.
//   PRINTER_Reset       -- destroys the Printer singleton (flushes any
//                          open multipage doc) so Destroy/Init cycles
//                          work cleanly.
//
// Lifecycle state (active / inactive) lives in the PrinterState struct
// further down; the code above deliberately has no equivalent flag.

void PRINTER_Configure(const PrinterModelKind model, const int dpi,
                       const double page_width_in, const double page_height_in,
                       const int timeout_ms)
{
	switch (model) {
	case PrinterModelKind::None:
		conf_model = PrinterModel::None;
		break;

	case PrinterModelKind::EpsonDotMatrix9Pin:
		conf_model = PrinterModel::EpsonDotMatrix;
		conf_pins  = 9;
		break;

	case PrinterModelKind::EpsonDotMatrix24Pin:
		conf_model = PrinterModel::EpsonDotMatrix;
		conf_pins  = 24;
		break;

	case PrinterModelKind::PostScript:
		conf_model = PrinterModel::PostScript;
		break;

	case PrinterModelKind::Passthrough:
		conf_model = PrinterModel::RawPassthrough;
		break;
	}

	conf_dpi            = dpi;
	conf_page_width_in  = page_width_in;
	conf_page_height_in = page_height_in;
	printer_timeout     = timeout_ms;
}

void PRINTER_FormFeed(const bool pressed)
{
	if (pressed) {
		trigger_form_feed(FormFeedSource::UserEjectKey);
	}
}

void PRINTER_Reset()
{
	// Cancel pending timeout events first so printer_event_handler can't
	// fire on a half-destroyed printer.
	PIC_RemoveEvents(printer_event_handler);

	default_printer.reset();
	postscript_sink.reset();
	raw_sink.reset();

	// The IO handlers below stay installed across Reset/Init cycles;
	// clear the LPT register state so a stale strobe sequence from
	// before a previous Reset can't trigger lazy construction with
	// stale data.
	lpt.data    = 0;
	lpt.control = CtrlInitialise;
}

namespace {

struct PrinterState {
	std::unique_ptr<IO_WriteHandleObject> data_write;
	std::unique_ptr<IO_ReadHandleObject> data_read;
	std::unique_ptr<IO_ReadHandleObject> status_read;
	std::unique_ptr<IO_WriteHandleObject> control_write;
	std::unique_ptr<IO_ReadHandleObject> control_read;

	bool active = false;
};

// Recognised page-size presets in inches. The "common DOS-era sizes
// users actually saw" list — A3/Tabloid/Fanfold are wide-carriage
// (LQ-1170, FX-100, FX-1050, etc.); the others fit any model.
struct PageSizePreset {
	std::string_view name;
	double width_in;
	double height_in;
};

constexpr PageSizePreset PageSizePresets[] = {
        {     "a3", 29.7 / 2.54, 42.0 / 2.54},
        {     "a4", 21.0 / 2.54, 29.7 / 2.54},
        {     "a5", 14.8 / 2.54, 21.0 / 2.54},
        { "letter",         8.5,        11.0},
        {  "legal",         8.5,        14.0},
        {"tabloid",        11.0,        17.0},
        {"fanfold",      14.875,        11.0},
};

// Parse a printer_page_size string. Accepts case-insensitive preset
// names (a3, a4, a5, letter, legal, tabloid, fanfold) or explicit
// dimensions in the form "<W>x<H>cm" or "<W>x<H>inch" / "<W>x<H>in".
// An optional trailing " landscape" / " portrait" word swaps the
// dimensions to satisfy the requested orientation. With no suffix
// the dimensions are used as given.
// Returns true on success, with width_in / height_in set to inches.
bool parse_page_size(const std::string& input, double& width_in, double& height_in)
{
	std::string lower = input;
	lowcase(lower);

	auto tokens = split(lower);
	if (tokens.empty()) {
		return false;
	}

	enum class Orient { AsIs, Landscape, Portrait };
	auto orient = Orient::AsIs;

	if (tokens.back() == "landscape") {
		orient = Orient::Landscape;
		tokens.pop_back();

	} else if (tokens.back() == "portrait") {
		orient = Orient::Portrait;
		tokens.pop_back();
	}

	if (tokens.empty()) {
		return false;
	}

	// Re-join the (possibly multi-token, whitespace-tolerant) size part
	// into a single string for preset / dimension matching.
	std::string size_str;
	for (const auto& t : tokens) {
		size_str += t;
	}

	bool parsed = false;
	for (const auto& preset : PageSizePresets) {
		if (size_str == preset.name) {
			width_in  = preset.width_in;
			height_in = preset.height_in;
			parsed    = true;
			break;
		}
	}

	if (!parsed) {
		// Explicit dimensions: "<W>x<H><unit>" where unit is cm, inch
		// or in. Strip the unit from the end first.
		double mult = 0.0;
		if (size_str.ends_with("cm")) {
			size_str.resize(size_str.size() - 2);
			mult = 1.0 / 2.54;

		} else if (size_str.ends_with("inch")) {
			size_str.resize(size_str.size() - 4);
			mult = 1.0;

		} else if (size_str.ends_with("in")) {
			size_str.resize(size_str.size() - 2);
			mult = 1.0;

		} else {
			return false;
		}

		// Split on 'x' to get width and height.
		const auto parts = split(size_str, "x");
		if (parts.size() != 2) {
			return false;
		}

		const auto w = parse_float(parts[0]);
		const auto h = parse_float(parts[1]);
		if (!w || !h) {
			return false;
		}

		width_in  = *w * mult;
		height_in = *h * mult;
	}

	// 1 inch lower bound (anything smaller is nonsense in practice);
	// 50 inch upper bound (well past any real paper).
	constexpr double MinDimIn = 1.0;
	constexpr double MaxDimIn = 50.0;

	if (width_in < MinDimIn || height_in < MinDimIn ||
	    width_in > MaxDimIn || height_in > MaxDimIn) {
		return false;
	}

	if (orient == Orient::Landscape && width_in < height_in) {
		std::swap(width_in, height_in);

	} else if (orient == Orient::Portrait && height_in < width_in) {
		std::swap(width_in, height_in);
	}
	return true;
}

PrinterModelKind parse_printer_model(const std::string& s)
{
	if (s == "epson-9pin") {
		return PrinterModelKind::EpsonDotMatrix9Pin;
	}
	if (s == "epson-24pin") {
		return PrinterModelKind::EpsonDotMatrix24Pin;
	}
	if (s == "postscript") {
		return PrinterModelKind::PostScript;
	}
	if (s == "passthrough") {
		return PrinterModelKind::Passthrough;
	}
	return PrinterModelKind::None;
}

PrinterState state{};

io_port_t lpt_port_for_name(const std::string& name)
{
	if (name == "lpt2") {
		return Lpt2Port;
	}
	if (name == "lpt3") {
		return Lpt3Port;
	}
	return Lpt1Port;
}

void install_io_handlers(const io_port_t lpt_port)
{
	const auto data_port    = lpt_port;
	const auto status_port  = static_cast<io_port_t>(lpt_port + 1u);
	const auto control_port = static_cast<io_port_t>(lpt_port + 2u);

	state.data_write    = std::make_unique<IO_WriteHandleObject>();
	state.data_read     = std::make_unique<IO_ReadHandleObject>();
	state.status_read   = std::make_unique<IO_ReadHandleObject>();
	state.control_write = std::make_unique<IO_WriteHandleObject>();
	state.control_read  = std::make_unique<IO_ReadHandleObject>();

	state.data_write->Install(data_port, PRINTER_WriteData, io_width_t::byte);
	state.data_read->Install(data_port, PRINTER_ReadData, io_width_t::byte);
	state.status_read->Install(status_port, PRINTER_ReadStatus, io_width_t::byte);
	state.control_write->Install(control_port, PRINTER_WriteControl, io_width_t::byte);
	state.control_read->Install(control_port, PRINTER_ReadControl, io_width_t::byte);
}

void uninstall_io_handlers()
{
	state.data_write.reset();
	state.data_read.reset();
	state.status_read.reset();
	state.control_write.reset();
	state.control_read.reset();
}

void formfeed_handler(bool pressed)
{
	PRINTER_FormFeed(pressed);
}

bool mapper_handler_registered = false;

} // namespace

void PRINTER_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);
	auto section = conf->AddSection("printer");
	auto& s      = *static_cast<SectionProp*>(section);

	using enum Property::Changeable::Value;

	// Any printer-section property change at the DOS prompt
	// re-initialises the whole printer subsystem. The user is
	// expected not to reconfigure mid-print (same contract as
	// turning off a real printer mid-job: the in-flight page
	// is lost).
	s.AddUpdateHandler([](SectionProp& /*sec*/, const std::string& prop_name) {
		LOG_MSG("PRINTER: Configuration changed (%s), "
		        "re-initialising",
		        prop_name.c_str());
		PRINTER_Destroy();
		PRINTER_Init();
	});

	auto pstring = s.AddString("printer_model", WhenIdle, "none");
	pstring->SetHelp(
	        "Type of virtual printer to emulate ('none' by default).\n"
	        "\n"
	        "  none:         No printer emulation (default).\n"
	        "\n"
	        "  epson-9pin:   9-pin Epson FX/LX-series printer. Use for older DOS programs\n"
	        "                that target an FX-80 / LX-series driver. Line spacing will look\n"
	        "                wrong if you use the 'epson-24pin' model with these drivers, and\n"
	        "                you might get vertically stretched graphics as well. Writes one\n"
	        "                PNG image per page in the capture directory (see 'capture') on\n"
	        "                the form feed command (^L byte), after 'printer_timeout'\n"
	        "                milliseconds of inactivity, or by pressing the 'ejectpage'\n"
	        "                hotkey.\n"
	        "\n"
	        "  epson-24pin:  24-pin Epson LQ-series dot-matrix printer (ESC/P and ESC/P 2\n"
	        "                dialects). Best fit for the Epson LQ-870 / LQ-850 / LQ-550\n"
	        "                drivers. See 'epson-9pin' for the output format.\n"
	        "\n"
	        "  postscript:   Saves the raw PostScript data sent to the printer to a file. Use\n"
	        "                this when your program targets a PostScript printer (e.g., Apple\n"
	        "                LaserWriter). Writes a single PostScript file with .ps extension\n"
	        "                to the capture directory (see 'capture'). The output file is \n"
	        "                closed on the end-of-job marker (^D byte); 'printer_timeout' and\n"
	        "                the 'ejectpage' hotkey are ignored.\n"
	        "\n"
	        "  passthrough:  Raw byte-stream capture; saves the raw data sent to the printer\n"
	        "                to a .prn file. Use this for printer languages other than\n"
	        "                PostScript or ESC/P (e.g. HP PCL, HP-GL, Canon BJL, etc.), or\n"
	        "                when you want a raw capture for offline processing. One file is\n"
	        "                created per print session; the session ends after\n"
	        "                'printer_timeout' milliseconds of inactivity or by pressing the\n"
	        "                'ejectpage' hotkey.");
	pstring->SetValues(
	        {"none", "epson-9pin", "epson-24pin", "postscript", "passthrough"});

	pstring = s.AddString("printer_port", WhenIdle, "lpt1");
	pstring->SetHelp(
	        "Parallel port the virtual printer is attached to ('lpt1' by default).\n"
	        "Most DOS programs target LPT1. Use 'lpt2' or 'lpt3' if you also need an LPT DAC\n"
	        "(see 'lpt_dac')."
	        "an LPT DAC (Disney / Covox / Stereo-on-1) on LPT1.");
	pstring->SetValues({"lpt1", "lpt2", "lpt3"});

	auto pint = s.AddInt("printer_dpi", WhenIdle, 360);
	pint->SetHelp(
	        "Output resolution of the virtual printer in dots per inch (360 by default).\n"
	        "Valid range is 60-1200. Higher values produce sharper output and larger PNG\n"
	        "files.\n"
	        "\n"
	        "Note:  Only applies to the 'epson-24pin' printer model to set the size of the\n"
	        "       PNG output image.");

	pstring = s.AddString("printer_page_size", WhenIdle, "a4");
	pstring->SetHelp(
	        "Output page size of the virtual printer ('a4' by default). Accepts a named\n"
	        "preset or explicit dimensions with an optional orientation suffix. Append\n"
	        "'landscape' or 'portrait' to specify the orientation (e.g. 'a4 landscape'),\n"
	        "defaults to 'portrait' if unspecified. Presets:\n"
	        "\n"
	        "  a3:       Wide-carriage Epson only (29.7x42 cm)\n"
	        "  a4:       The European default (21x29.7 cm)\n"
	        "  a5:       Half an A4 page (14.8x21 cm)\n"
	        "\n"
	        "  letter:   US default (8.5x11 inch)\n"
	        "  legal:    Long US (8.5x14 inch)\n"
	        "  tabloid:  Wide US (11x17 inch)\n"
	        "\n"
	        "  fanfold:  Tractor-feed continuous paper, the classic DOS-era format\n"
	        "            (14.875x11 inch)\n"
	        "\n"
	        "  WxHcm:    Exact dimensions in centimeters (e.g., 29.7x42cm)\n"
	        "\n"
	        "  WxHin:    Exact dimensions in inches (e.g., 8.5x14inch)\n"
	        "  WxHinch:\n"
	        "\n"
	        "Note:  Only applies to the 'epson-24pin' printer model to set the size of the\n"
	        "       PNG output image.");

	pint = s.AddInt("printer_timeout", WhenIdle, 10000);
	pint->SetHelp(
	        "Inactivity timeout in milliseconds after which an unfinished\n"
	        "page is auto-ejected. Default 10000ms (10 seconds). DOS print\n"
	        "drivers commonly pause for several seconds mid-job while\n"
	        "preparing the next batch of bytes; too short a timeout splits\n"
	        "one logical page across multiple output files. Raise this\n"
	        "further if you still see split pages, or set to 0 to disable\n"
	        "auto-eject entirely. You can also eject manually via the\n"
	        "'ejectpage' mapper action (default Ctrl+F2, fully remappable).\n"
	        "\n"
	        "Ignored when printer_model = postscript -- the PostScript\n"
	        "output file closes only on the driver's end-of-job marker.");
}

void PRINTER_Init()
{
	if (state.active) {
		return;
	}

	const auto section_base = get_section("printer");
	if (!section_base) {
		return;
	}
	auto& section = *section_base;

	const auto model_str = section.GetString("printer_model");
	const auto model     = parse_printer_model(model_str);
	if (model == PrinterModelKind::None) {
		return;
	}

	const auto port_name = section.GetString("printer_port");
	const auto lpt_port  = lpt_port_for_name(port_name);

	if (IO_HasWriteHandler(lpt_port)) {
		LOG_WARNING(
		        "PRINTER: Port %s (%03xh) is already in use by another device;"
		        " printer disabled. Set 'printer_port' to a free LPT port.",
		        port_name.c_str(),
		        lpt_port);
		return;
	}

	constexpr int MinDpi     = 60;
	constexpr int MaxDpi     = 1200;
	constexpr int DefaultDpi = 360;

	auto dpi = section.GetInt("printer_dpi");
	if (dpi < MinDpi || dpi > MaxDpi) {
		const auto bad = std::to_string(dpi);

		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "PRINTER",
		                      "PROGRAM_CONFIG_INVALID_SETTING",
		                      "printer_dpi",
		                      bad.c_str(),
		                      "360");
		dpi = DefaultDpi;
	}

	const auto dpi_u16  = static_cast<uint16_t>(dpi);
	const auto size_str = section.GetString("printer_page_size");
	const auto timeout  = section.GetInt("printer_timeout");

	double page_w_in = 0.0;
	double page_h_in = 0.0;
	if (!parse_page_size(size_str, page_w_in, page_h_in)) {
	
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "PRINTER",
		                      "PROGRAM_CONFIG_INVALID_SETTING",
		                      "printer_page_size",
		                      size_str.c_str(),
		                      "a4");

		parse_page_size("a4", page_w_in, page_h_in);
	}

	PRINTER_Configure(model, dpi, page_w_in, page_h_in, timeout);

	install_io_handlers(lpt_port);

	if (!mapper_handler_registered) {
		MAPPER_AddHandler(formfeed_handler,
		                  SDL_SCANCODE_F2,
		                  PRIMARY_MOD,
		                  "ejectpage",
		                  "Eject page");

		mapper_handler_registered = true;
	}

	state.active = true;

	const char* model_name = "";
	switch (model) {
	case PrinterModelKind::EpsonDotMatrix9Pin:
		model_name = "Epson dot-matrix (9-pin)";
		break;

	case PrinterModelKind::EpsonDotMatrix24Pin:
		model_name = "Epson dot-matrix (24-pin)";
		break;

	case PrinterModelKind::PostScript:
		model_name = "PostScript passthrough";
		break;

	case PrinterModelKind::Passthrough:
		model_name = "Raw passthrough";
		break;

	case PrinterModelKind::None: break;
	}

	const bool is_dot_matrix = (model == PrinterModelKind::EpsonDotMatrix9Pin ||
	                            model == PrinterModelKind::EpsonDotMatrix24Pin);

	if (is_dot_matrix) {
		LOG_MSG("PRINTER: Initialised %s on %s (%03xh), %dx%d page at %d dpi",
		        model_name,
		        port_name.c_str(),
		        lpt_port,
		        static_cast<int>(page_w_in * 100 + 0.5),
		        static_cast<int>(page_h_in * 100 + 0.5),
		        dpi);
	} else {
		LOG_MSG("PRINTER: Initialised %s on %s (%03xh)",
		        model_name,
		        port_name.c_str(),
		        lpt_port);
	}
}

void PRINTER_Destroy()
{
	if (!state.active) {
		return;
	}

	PRINTER_Reset();

	uninstall_io_handlers();
	state.active = false;

	// Note: staging's mapper API has no MAPPER_RemoveHandler counterpart,
	// so the Ctrl+F2 binding stays registered; subsequent Init calls
	// reuse it.
}
