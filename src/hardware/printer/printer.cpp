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

// PIC_AddEvent / PIC_RemoveEvents drive the form-feed timeout.
#include "hardware/pic.h"
#include "utils/checks.h"

CHECK_NARROWING();

using VirtualPrinter::Printer;

static void PRINTER_EventHandler([[maybe_unused]] uint32_t param);

static std::unique_ptr<Printer> default_printer = nullptr;

static uint16_t conf_dpi, conf_width, conf_height;
static uint64_t printer_timeout;
static bool timeout_dirty;
static std_fs::path document_path;
// static const char* font_path;
static std::string conf_output_device;
static bool conf_multipage_output;

namespace VirtualPrinter {

// Printer::FillPalette lives in printer_glyph.cpp.

Printer::Printer(uint16_t dpi, const uint16_t width, const uint16_t height,
                 const std::string& output, bool multipage_output)
{
	if (FT_Init_FreeType(&ft_lib)) {
		LOG_ERR("PRINTER: Unable to init Freetype2. Printing disabled");
		return;
	}

	this->dpi              = dpi;
	this->output           = output;
	this->multipage_output = multipage_output;

	default_page_width = static_cast<Real64>(width) /
	                     static_cast<Real64>(10);
	default_page_height = static_cast<Real64>(height) /
	                      static_cast<Real64>(10);

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
	output_handle.reset();

	ResetPrinter();

	LOG_MSG("PRINTER: Enabled");
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

// Printer::ProcessCommandChar lives in printer_dispatch.cpp.

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
	for (uint8_t b : page.pixels) {
		if (b != 0) {
			return false;
		}
	}
	return true;
}

uint8_t Printer::GetPixel(const uint32_t num)
{
	return page.pixels[(num % page.width) +
	                   ((num / page.width) * page.pitch)];
}

} // namespace VirtualPrinter

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
			default_printer = std::make_unique<Printer>(conf_dpi,
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
	conf_dpi              = dpi;
	conf_width            = width;
	conf_height           = height;
	document_path         = docpath ? docpath : "";
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
