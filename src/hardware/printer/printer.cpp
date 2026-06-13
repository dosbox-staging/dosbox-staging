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
#include <array>
#include <math.h>
#include <memory>
#include <optional>
#include <vector>

#include "misc/std_filesystem.h"
#include "printer_internal.h"
#include "printer_png.h"
#include "utils/string_utils.h"

// PIC_AddEvent / PIC_RemoveEvents drive the form-feed timeout.
#include "hardware/pic.h"
#include "utils/checks.h"

CHECK_NARROWING();

using VirtualPrinter::Printer;
using VirtualPrinter::PrinterModel;

static void printer_event_handler([[maybe_unused]] uint32_t param);

static std::unique_ptr<Printer> default_printer = nullptr;

static PrinterModel conf_model    = PrinterModel::None;
static uint16_t conf_dpi          = 360;
static double conf_page_width_in  = 8.27;  // A4
static double conf_page_height_in = 11.69; // A4
static int conf_pins              = 24;    // 9 = FX/LX, 24 = LQ
static uint64_t printer_timeout   = 500;
static bool timeout_dirty         = false;
static std_fs::path document_path;

namespace VirtualPrinter {

// Printer::FillPalette lives in printer_glyph.cpp.

Printer::Printer(uint16_t dpi, const double page_width_in,
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
	// Sub-palette indices are chosen so that overprinting two
	// colours ORs the bits of their IDs to produce a third
	// reasonable colour, mirroring how real ribbon overprinting
	// works (e.g. magenta=001 OR yellow=100 -> red=101).
	//
	// Sub-palette 0 is the 'all white' page background and gets
	// hand-initialised below; the other seven are derived from
	// (red_max, green_max, blue_max) tuples by FillPalette.
	for (uint64_t i = 0; i < 32; i++) {
		auto& c = page.palette[i];
		c.r = c.g = c.b = 255;
	}
	FillPalette(0, 0, 0, 1);
	FillPalette(0, 255, 0, 1);
	FillPalette(255, 0, 0, 2);
	FillPalette(255, 255, 0, 3);
	FillPalette(0, 0, 255, 4);
	FillPalette(0, 255, 255, 5);
	FillPalette(255, 0, 255, 6);
	FillPalette(255, 255, 255, 7);

	color = ColorBlack;

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
	color     = ColorBlack;
	esc_seen  = false;
	fs_seen   = false;
	esc_cmd   = 0;
	num_param = needed_param = 0;

	// Default printable-area margins of 0.25 inch on all sides
	// approximate the non-printable area of real Epson dot-matrix
	// printers (see escp2ref.pdf Feature Summary per-model "Nonprintable
	// area" lines, e.g. F-33 for LQ-850).
	constexpr double DefaultMarginIn = 0.25;
	top_margin                       = DefaultMarginIn;
	left_margin                      = DefaultMarginIn;
	page_width                       = default_page_width;
	page_height                      = default_page_height;
	right_margin                     = default_page_width - DefaultMarginIn;
	bottom_margin       = default_page_height - DefaultMarginIn;
	cur_x               = left_margin;
	cur_y               = top_margin;
	line_spacing        = static_cast<double>(1) / 6;
	cpi                 = 10.0;
	cur_char_table      = 1;
	style.data          = 0;
	extra_intra_space   = 0.0;
	print_upper_contr   = true;
	bit_graph.rem_bytes = 0;
	densk               = 0;
	densl               = 1;
	densy               = 2;
	densz               = 3;
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
	lq_typeface                                      = Typeface::Roman;

	SelectCodepage(char_tables[cur_char_table]);

	UpdateFont();

	NewPage(false, true);

	// Default tabs -- one every 8 characters, measured from the left
	// margin (not from the page origin). Matches the Epson ESC/P spec
	// (ESC D description: "absolute position from the left-margin
	// position").
	for (uint64_t i = 0; i < 32; i++) {
		horiz_tabs[i] = left_margin +
		                static_cast<double>((i + 1) * 8) /
		                        static_cast<double>(cpi);
	}
	num_horiz_tabs = 32;

	num_vert_tabs = 255;
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
	 // Italics, use cp437
	 case 0:
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

// Printer::UpdateFont lives in printer_glyph.cpp.

// Printer::ProcessCommandChar lives in printer_dispatch.cpp.

void Printer::NewPage(const bool save, const bool reset_x)
{
	PIC_RemoveEvents(printer_event_handler);
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

	// Box-drawing chars (U+2500..U+257F) and block elements
	// (U+2580..U+259F) are designed to tile across cell boundaries. In
	// fixed-pitch mode we render them from the mono fallback face
	// (independent of the active typeface), matching real Epson which
	// shipped a typeface-agnostic box-drawing bitmap set. The face is
	// then temporarily stretched in both axes so its em exactly fills
	// the 1/cpi horizontal cell and the line_spacing vertical cell;
	// the bitmap overhang baked into each glyph bridges the seam.
	const auto unicode = cur_map[ch];
	const bool is_box_char = unicode >= 0x2500 && unicode <= 0x259F;
	FT_Face render_face = (is_box_char && !style.prop && mono_box_font != nullptr)
	                              ? mono_box_font
	                              : cur_font;
	const bool apply_box_fill = is_box_char &&
	                            box_fill_horiz_points > 0.0 &&
	                            line_spacing > 0.0 &&
	                            natural_em_height_px > 0;

	int box_fill_em_h_px   = 0;
	int box_fill_em_asc_px = 0;
	if (apply_box_fill) {
		const auto cell_h_px = static_cast<int>(line_spacing * dpi);
		const auto box_fill_vert_points = cur_vert_points * BoxFillOvershootVertical *
		                                  static_cast<double>(cell_h_px) /
		                                  static_cast<double>(natural_em_height_px);
		FT_Set_Char_Size(render_face,
		                 static_cast<FT_F26Dot6>(box_fill_horiz_points *
		                                         Ft26Dot6Unit),
		                 static_cast<FT_F26Dot6>(box_fill_vert_points *
		                                         Ft26Dot6Unit),
		                 dpi,
		                 dpi);
		box_fill_em_h_px   = ft26_6_to_pixels(render_face->size->metrics.height);
		box_fill_em_asc_px = ft26_6_to_pixels(render_face->size->metrics.ascender);
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
	// forced into a 1/act_cpi inch cell would otherwise sit
	// left-aligned with a visible gap on the right of every char.
	// Shift narrow glyphs to the middle of the cell to even that out.
	//
	// Skip this for native monospace fonts. Their bitmap_left already
	// positions glyphs correctly within the advance cell, and shifting would
	// misalign the CP437 box-drawing characters, where `│`/`─`/`┼` would land
	// at different X positions and break grids.
	uint16_t centre_offset = 0;

	if (!style.prop && act_cpi > 0.0 && !FT_IS_FIXED_WIDTH(render_face)) {
		const auto cell_px = static_cast<int>(dpi / act_cpi);
		const auto glyph_w = static_cast<int>(render_face->glyph->bitmap.width);

		if (cell_px > glyph_w) {
			centre_offset = static_cast<uint16_t>((cell_px - glyph_w) / 2);
		}
	}

	// For box-fill chars, centre the stretched em on the cell using a
	// constant offset so every box-drawing glyph gets the same
	// horizontal positioning -- features anchored to em-centre
	// (verticals, line crossings) all land at cell-centre across the
	// grid. Centring by the glyph's own bitmap.width would shift each
	// glyph differently and break grid alignment.
	int signed_pen_x = 0;

	if (apply_box_fill && act_cpi > 0.0) {
		const auto cell_px = static_cast<int>(dpi / act_cpi);

		signed_pen_x = static_cast<int>(PixX()) +
		              (cell_px - box_fill_em_px) / 2 +
		              render_face->glyph->bitmap_left;
	} else {
		signed_pen_x = static_cast<int>(PixX()) +
		              render_face->glyph->bitmap_left + centre_offset;
	}

	const uint16_t pen_x = static_cast<uint16_t>(signed_pen_x < 0 ? 0 : signed_pen_x);

	// Anchor every glyph to the per-line baseline captured in
	// UpdateFont() so that double-height chars extend upward from the
	// shared baseline and normal-size chars on the same line stay on
	// the baseline. Box-fill chars override this with em-centring on
	// the line cell.
	int signed_pen_y = 0;

	if (apply_box_fill) {
		const auto cell_h_px = static_cast<int>(line_spacing * dpi);

		const auto em_top_y  = static_cast<int>(PixY()) +
		                      (cell_h_px - box_fill_em_h_px) / 2;

		signed_pen_y = em_top_y + box_fill_em_asc_px -
		              render_face->glyph->bitmap_top;
	} else {
		signed_pen_y = static_cast<int>(PixY()) + line_baseline_anchor_px -
		              render_face->glyph->bitmap_top;
	}
	uint16_t pen_y = static_cast<uint16_t>(signed_pen_y < 0 ? 0 : signed_pen_y);

	// Sub- and superscript vertical shift. The spec (escp2ref.pdf
	// C-129) only describes the *direction* ("lower part" vs "upper
	// part" of normal character space) and leaves the exact offset to
	// the printer. We use rise = point_size / 3 (in PDF baseline units,
	// converted to pixels in UpdateFont).
	if (style.subscript) {
		pen_y = static_cast<uint16_t>(pen_y + subscript_shift_px);
	}
	if (style.superscript) {
		pen_y = static_cast<uint16_t>(pen_y - superscript_shift_px);
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
	const uint16_t lineStart = static_cast<uint16_t>(PixX());

	// advance the cursor to the right
	double x_advance;
	if (style.prop) {
		// advance.x is in 26.6 pixel units; divide by dpi * Ft26Dot6Unit
		// in a single step to keep sub-pixel precision before
		// converting to inches.
		x_advance = static_cast<double>(render_face->glyph->advance.x) /
		            static_cast<double>(dpi * Ft26Dot6Unit);
	} else {
		if (hmi < 0) {
			x_advance = 1 / static_cast<double>(act_cpi);
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
		// TODO height is in fixed-point format from FreeType.
		uint16_t lineY      = static_cast<uint16_t>(PixY());
		const double height = static_cast<double>(
		        cur_font->size->metrics.height >> 6);

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
		         score == ScoreType::SingleBroken ||
		                 score == ScoreType::DoubleBroken);

		// draw second line if needed
		if ((score == ScoreType::Double) ||
		    (score == ScoreType::DoubleBroken)) {
			// score is DOUBLE or DOUBLEBROKEN here; the upstream
			// expression also tested ScoreType::SingleBroken which
			// is unreachable in this branch.
			DrawLine(lineStart,
			         PixX(),
			         lineY + 5,
			         score == ScoreType::DoubleBroken);
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

// Printer::SetupBitImage lives in printer_dispatch.cpp.

// Printer::PrintBitGraph lives in printer_dispatch.cpp.

void Printer::FormFeed()
{
	// Don't output blank pages
	NewPage(!IsBlank(), true);
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
		std_fs::path candidate = docdir / (prefix + std::to_string(i) + ext);
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
	if (const auto out_path = find_next_name("page", ".png")) {
		if (write_png_page(page, *out_path)) {
			LOG_MSG("PRINTER: Wrote page to %s",
			        out_path->string().c_str());
		}
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

uint8_t Printer::GetPixel(const uint32_t num)
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
			PIC_RemoveEvents(printer_event_handler);
			if (printer_timeout) {
				timeout_dirty = false;
			}

			default_printer->FormFeed();
		}
	}
}

static void printer_event_handler([[maybe_unused]] const uint32_t param)
{
	if (timeout_dirty) {
		// More data arrived since this event was queued — push the
		// timeout further out and re-fire later.
		PIC_AddEvent(printer_event_handler,
		             static_cast<float>(printer_timeout),
		             0);
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
		if (!default_printer && conf_model == PrinterModel::EpsonDotMatrix) {
			default_printer = std::make_unique<Printer>(conf_dpi,
			                                            conf_page_width_in,
			                                            conf_page_height_in,
			                                            conf_pins);
		}
		if (default_printer) {
			default_printer->PrintChar(lpt.data);
		}
		if (!timeout_dirty) {
			PIC_AddEvent(printer_event_handler,
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

void PRINTER_Configure(const PrinterModelKind model, const uint16_t dpi,
                       const double page_width_in, const double page_height_in,
                       const std::string& output_dir, const int timeout_ms)
{
	switch (model) {
	case PrinterModelKind::None: conf_model = PrinterModel::None; break;
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
	}

	conf_dpi            = dpi;
	conf_page_width_in  = page_width_in;
	conf_page_height_in = page_height_in;

	document_path   = output_dir.empty() ? std_fs::path{"."}
	                                     : std_fs::path{output_dir};
	printer_timeout = static_cast<uint64_t>(timeout_ms);
	timeout_dirty   = (printer_timeout == 0);

	// Auto-create the output directory if it doesn't exist (existing
	// behaviour was a confusing per-file error on first page write).
	if (!document_path.empty()) {
		std::error_code ec;
		if (!std_fs::exists(document_path, ec)) {
			if (std_fs::create_directories(document_path, ec)) {
				LOG_MSG("PRINTER: Created output directory %s",
				        document_path.string().c_str());
			} else if (ec) {
				LOG_ERR("PRINTER: Failed to create output directory %s: %s",
				        document_path.string().c_str(),
				        ec.message().c_str());
			}
		}
	}
}

void PRINTER_FormFeed(const bool pressed)
{
	trigger_form_feed(pressed);
}

void PRINTER_Reset()
{
	// Cancel pending timeout events first so printer_event_handler can't
	// fire on a half-destroyed printer.
	PIC_RemoveEvents(printer_event_handler);
	timeout_dirty = false;
	default_printer.reset();
	// The IO handlers in printer_glue stay installed across Reset/Init
	// cycles; clear the LPT register state so a stale strobe sequence
	// from before a previous Reset can't trigger lazy construction with
	// stale data.
	lpt.data    = 0;
	lpt.control = CtrlInitialise;
}
