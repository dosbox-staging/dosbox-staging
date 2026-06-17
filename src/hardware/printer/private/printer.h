// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRIVATE_PRINTER_H
#define DOSBOX_PRIVATE_PRINTER_H

#include "dosbox_config.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "misc/support.h"
#include "misc/types.h"
#include "utils/bit_view.h"
#include "utils/rgb888.h"

#include <png.h>

#include <ft2build.h>
#include FT_FREETYPE_H

// Which printer the user picked. Mirrors VirtualPrinter::PrinterModel but
// kept in plain enum form so the C-style cross-module API doesn't depend
// on the dot-matrix internals below.
//
// EpsonDotMatrix24Pin is the 24-pin LQ family (default, ESC/P + ESC/P 2).
// EpsonDotMatrix9Pin is the 9-pin FX/LX family (older ESC/P only -- has
// different line-spacing divisors and lacks several ESC/P 2 commands;
// see Printer::pins branches in printer_dispatch.cpp).
enum class PrinterModelKind {
	None,
	EpsonDotMatrix9Pin,
	EpsonDotMatrix24Pin,
	PostScript,
	Passthrough,
};

uint64_t PRINTER_ReadData(const uint64_t port, const uint64_t iolen);
void PRINTER_WriteData(const uint64_t port, const uint64_t val, const uint64_t iolen);

uint64_t PRINTER_ReadStatus(const uint64_t port, const uint64_t iolen);

void PRINTER_WriteControl(const uint64_t port, const uint64_t val,
                          const uint64_t iolen);
uint64_t PRINTER_ReadControl(const uint64_t port, const uint64_t iolen);

// Set the printer config values that are read at lazy printer
// construction time inside PRINTER_WriteControl. page_width_in /
// page_height_in are inches (double). Dot-matrix-only settings are
// ignored when model is PostScript.
void PRINTER_Configure(const PrinterModelKind model, const int dpi,
                       const double page_width_in, const double page_height_in,
                       const int timeout_ms);

// Trigger a form-feed (eject the current page). Called from the mapper
// (Ctrl+F2 by default) and from the inactivity-timeout PIC event.
void PRINTER_FormFeed(bool pressed);

// Release the lazily-created printer instance.
void PRINTER_Reset();

namespace VirtualPrinter {

// Page bitmap. 8-bit palette-indexed, contiguous storage (no row
// padding), one entry per pixel.
struct PageBitmap {
	std::vector<uint8_t> pixels     = {};
	std::array<Rgb888, 256> palette = {};

	int width  = 0;
	int height = 0;

	// Bytes per row. Equals width for our contiguous buffer.
	int pitch = 0;

	uint8_t& at(const int x, const int y)
	{
		return pixels[x + y * pitch];
	}

	uint8_t at(const int x, const int y) const
	{
		return pixels[x + y * pitch];
	}
};

// FreeType uses signed 26.6 fixed-point for sub-pixel metrics (size
// metrics, glyph advances, character sizes passed to FT_Set_Char_Size).
// Multiply a whole-pixel/point value by Ft26Dot6Unit to convert it into
// 26.6 form, divide by Ft26Dot6Unit to convert back. Use points_to_26_6
// / ft26_6_to_pixels at the call site to keep the conversion readable.
constexpr int Ft26Dot6Unit = 64;

// Truncates the fractional part of `pts` before converting -- matches
// the historical behaviour of `static_cast<uint16_t>(pts) * 64` that the
// printer code has used since DOSBox 0.74. Callers that need sub-point
// precision should multiply explicitly.
static constexpr FT_F26Dot6 points_to_26_6(const double pts)
{
	return static_cast<FT_F26Dot6>(static_cast<int>(pts)) * Ft26Dot6Unit;
}

static constexpr int ft26_6_to_pixels(const FT_Pos value)
{
	return static_cast<int>(value / Ft26Dot6Unit);
}

// CP437 box-drawing and block-element glyphs are scaled so their em
// exactly fills the cell (1/cpi inch horizontally, line_spacing inch
// vertically) in both axes. At 1.0 there's no overshoot beyond the
// cell -- adjacent glyphs touch via the bitmap overhang the font
// designer baked into the box-drawing chars (a few px past the em on
// each side, so they can connect in a terminal with zero kerning).
// The guard in UpdateFont skips the stretch when the font's natural
// advance is already >= cell, so this is a no-op for fonts whose
// design size matches the configured cpi.
constexpr double BoxFillOvershootHorizontal = 1.00;
constexpr double BoxFillOvershootVertical   = 1.00;

// Currently-active text style flags. Modelled on LptStatusRegister in
// src/hardware/parallelport/lpt.h: writes go through the named bit_view
// members, reads through either the members or the 'data' field for masking.
union PrinterStyle {
	uint16_t data = 0;
	bit_view<0, 1> prop;
	bit_view<1, 1> condensed;
	bit_view<2, 1> bold;
	bit_view<3, 1> doublestrike;
	bit_view<4, 1> doublewidth;
	bit_view<5, 1> italics;
	bit_view<6, 1> underline;
	bit_view<7, 1> superscript;
	bit_view<8, 1> subscript;
	bit_view<9, 1> strikethrough;
	bit_view<10, 1> overscore;
	bit_view<11, 1> doublewidth_oneline;
	bit_view<12, 1> doubleheight;
};

enum class ScoreType : uint8_t {
	None         = 0x00,
	Single       = 0x01,
	Double       = 0x02,
	SingleBroken = 0x05,
	DoubleBroken = 0x06,
};

enum class PrintQuality : uint8_t {
	// Power-on default. Not yet selected by the application via ESC x.
	// Treated as 'not Draft' by code that checks for the draft mode
	// specifically, matching the upstream behaviour where this byte
	// was indeterminate / typically zero before first use.
	None  = 0x00,
	Draft = 0x01,
	Lq    = 0x02,
};

// Top-level dispatch: what kind of printer the user picked.
enum class PrinterModel {
	None,
	EpsonDotMatrix,
	PostScript,
	RawPassthrough,
};

// Page bitmap pixel encoding. Each byte packs a 5-bit intensity in
// the bottom bits (0 = white background, 31 = full saturation) and a
// 3-bit colour ID in the top bits. Writes go through the named
// bit_view members, reads through either the members or the 'data'
// field for masking. Modelled on PrinterStyle above.
union PagePixel {
	uint8_t data = 0;
	bit_view<0, 5> intensity;
	bit_view<5, 3> color_id;
};

constexpr int MaxIntensity = 31;

// Palette is split into 8 sub-palettes of 32 colours each, indexed by
// the 3-bit colour ID. Black is sub-palette 7.
constexpr uint8_t ColorBlack = 7 << 5;

enum class Typeface {
	Roman = 0,
	SansSerif,
	Courier,
	Prestige,
	Script,
	OcrB,
	OcrA,
	Orator,
	OratorS,
	ScriptC,
	RomanT,
	SansSerifH,
	Svbusaba = 30,
	Svjittra = 31,
};

class Printer {
public:
	// page_width_in and page_height_in are inches (double). pins selects
	// the printer head pin count (9 = FX/LX series, 24 = LQ series).
	// 9-pin printers use different line-spacing divisors and lack
	// several ESC/P 2 commands; the dispatch code branches on `pins`.
	Printer(const int dpi, const double page_width_in,
	        const double page_height_in, const int pins = 24);

	virtual ~Printer();

	// Owns FreeType/SDL resources and a singleton Printer* — don't copy.
	Printer(const Printer&)            = delete;
	Printer& operator=(const Printer&) = delete;

	// Process one character sent to virtual printer
	void PrintChar(uint8_t ch);

	// Hard Reset (like switching printer off and on)
	void ResetPrinterHard();

	// Set Autofeed value
	void SetAutofeed(bool feed);

	// Get Autofeed value
	bool GetAutofeed();

	// True if printer is unable to process more data right now (do not use
	// PrintChar)
	bool IsBusy();

	// True if the last sent character was received
	bool Ack();

	// Manual formfeed
	void FormFeed();

	// Returns true if the current page is blank
	bool IsBlank();

private:
	// Fill one of the eight 32-entry colour sub-palettes in 'page'.
	void FillPalette(const uint8_t red_max, const uint8_t green_max,
	                 const uint8_t blue_max, const uint8_t color_id);

	// Checks if given char belongs to a command and process it. If false,
	// the character should be printed
	bool ProcessCommandChar(const uint8_t ch);

	// Resets the printer to the factory settings
	void ResetPrinter();

	// Reload font. Must be called after changing dpi, style or cpi
	void UpdateFont();

	// Clears page. If save is true, saves the current page to a bitmap
	void NewPage(const bool save, const bool reset_x);

	// Blits the given glyph on the page surface. If add is true, the values
	// of bitmap are added to the values of the pixels in the page
	void BlitGlyph(FT_Bitmap bitmap, const int dest_x, const int dest_y,
	               const bool add);

	// Draws an anti-aliased line from (fromx, y) to (tox, y). If broken is
	// true, gaps are included
	void DrawLine(const int from_x, const int to_x, const int y,
	              const bool broken);

	// Setup the bit_graph structure
	void SetupBitImage(const uint8_t density, const uint16_t num_cols);

	// Process a character that is part of bit image. Must be called iff
	// bit_graph.bytes_left > 0.
	void PrintBitGraph(const uint8_t ch);

	// Blit a single bit-image dot into the page bitmap using linear
	// coverage anti-aliasing. The dot is a 1/horiz_dens by 1/vert_dens
	// inch rectangle whose corners are passed in page-pixel
	// coordinates (may be fractional). For each page pixel the dot's
	// bounding box touches, dx*dy is the area of overlap in [0, 1],
	// scaled to MaxIntensity and added to the pixel's intensity field
	// (capped at MaxIntensity). The pixel's colour-ID bits are set to
	// the head's current colour. This preserves overprint semantics:
	// overlapping dots accumulate intensity rather than replacing it.
	void BlitAntialiasedDot(const double left_px, const double right_px,
	                        const double top_px, const double bottom_px);

	// Copies the codepage mapping from the constant array to CurMap
	void SelectCodepage(const uint16_t codepage);

	// Output current page as a PNG file.
	void OutputPage();

	// Decode a little-endian 16-bit ESC/P2 parameter starting at params[i].
	uint16_t Param16(const int i) const
	{
		return static_cast<uint16_t>(params[i + 1] * 256 + params[i]);
	}

	// Current head position expressed as a pixel coordinate at the
	// configured DPI.
	int PixX() const
	{
		return static_cast<int>(floor(cur_x * dpi + 0.5));
	}

	int PixY() const
	{
		return static_cast<int>(floor(cur_y * dpi + 0.5));
	}

	// Returns value of the num-th pixel (couting left-right, top-down) in a
	// safe way
	uint8_t GetPixel(const int num);

	// FreeType2 library used to render the characters.
	FT_Library ft_lib = nullptr;

	// Off-screen bitmap of the current page.
	PageBitmap page = {};

	// The font currently used to render characters.
	FT_Face cur_font = nullptr;

	// Monospace fallback (courier.ttf), loaded in parallel with
	// cur_font. PrintChar swaps to this for CP437 box-drawing and
	// block-element glyphs when in fixed-pitch mode (style.prop = 0)
	// so the box chars from a proportional typeface line up on the
	// grid -- proportional fonts have variable advance widths even for
	// box-drawing chars. Mirrors escapy's approach.
	FT_Face mono_box_font = nullptr;

	uint8_t color = 0;

	// Position of the print head (in inch).
	double cur_x = 0.0;
	double cur_y = 0.0;

	// Page resolution in dots per inch.
	int dpi = 0;

	// ESC command currently being processed.
	uint16_t esc_cmd = 0;

	// True if last read character was an ESC (0x1B).
	bool esc_seen = false;

	// True if last read character was an FS (0x1C). We don't emulate
	// IBM PPDS mode, so the next byte is logged and discarded.
	bool fs_seen = false;

	// Numbers of parameters already read / needed to process command.
	uint8_t num_param    = 0;
	uint8_t needed_param = 0;

	// Buffer for the read parameters.
	std::array<uint8_t, 20> params = {};

	// Bitmask of currently active text styles.
	PrinterStyle style = {};

	// CPI value set by program and the actual one (taking font types into
	// account).
	double cpi        = 0.0;
	double actual_cpi = 0.0;

	// Score (underline / strikethrough / overscore) style for lines.
	ScoreType score = ScoreType::None;

	// Margins of the page (in inch).
	double top_margin    = 0.0;
	double bottom_margin = 0.0;
	double right_margin  = 0.0;
	double left_margin   = 0.0;

	// Size of the current page (in inch).
	double page_width  = 0.0;
	double page_height = 0.0;

	// Default size of the page (in inch).
	double default_page_width  = 0.0;
	double default_page_height = 0.0;

	// Printer head pin count: 9 (FX/LX) or 24 (LQ). 48-pin printing
	// happens through ESC * densities 71/72/73 on a 24-pin printer
	// driver -- it's a data-stream property, not a model property.
	int pins = 24;

	// Per-line baseline anchor in pixels relative to PixY(). Captured
	// in UpdateFont() at the natural (cpi-derived, no double-height,
	// no sub/super) point size so that every glyph rendered on the line
	// shares the same baseline regardless of which size each char ends
	// up at.
	int line_baseline_anchor_px = 0;

	// Current font horizontal/vertical point sizes as last passed to
	// FT_Set_Char_Size in UpdateFont(). PrintChar uses these to restore
	// the font size after temporarily applying the box-fill stretch.
	double cur_horiz_points = 0.0;
	double cur_vert_points  = 0.0;

	// Horizontal point size used to render CP437 box-drawing and
	// block-element chars (U+2500..U+259F). Set in UpdateFont(); zero
	// when the natural advance already fills the cell or for
	// proportional fonts. PrintChar computes the matching vertical
	// stretch lazily because it depends on the current line_spacing.
	double box_fill_horiz_points = 0.0;

	// Em width in pixels at box_fill_horiz_points. Used in PrintChar
	// for em-centring on the cell so features anchored to em-centre
	// (verticals, line crossings) line up across the grid.
	int box_fill_em_px = 0;

	// Natural em-height at cur_vert_points (read in UpdateFont). Used
	// in PrintChar to compute the lazy vertical box-fill stretch.
	int natural_em_height_px = 0;

	// Subscript / superscript vertical pixel shift. Equals
	// rise = point_size / 3 (PDF baseline units, converted to pixels).
	// With the per-line baseline anchor above, the shift is just the
	// rise -- no per-font delta correction is needed because the
	// baseline stays constant across font-size changes within the line.
	int subscript_shift_px   = 0;
	int superscript_shift_px = 0;

	// Size of one line (in inch).
	double line_spacing = 0.0;

	// Currently configured horizontal tabs (in inch).
	std::array<double, 32> horiz_tabs = {};
	uint8_t num_horiz_tabs            = 0;

	// Currently configured vertical tabs (in inch).
	std::array<double, 16> vert_tabs = {};
	uint8_t num_vert_tabs            = 0;

	// Currently selected character table / charset.
	uint8_t cur_char_table = 0;

	// Print quality.
	PrintQuality print_quality = PrintQuality::None;

	// Typeface used in LQ printing mode.
	Typeface lq_typeface = Typeface::Roman;

	// Extra space between two characters (set by program, in inch).
	double extra_intra_space = 0.0;

	// True if a character was read since the printer was last initialised.
	bool char_read = false;

	// True if a LF should automatically be added after a CR.
	bool auto_feed = false;

	// True if the upper-half control characters should be printed.
	bool print_upper_contr = false;

	// Bit-image printing state.
	struct BitGraphicParams {
		// Density of image to print (in dpi).
		uint16_t horiz_dens = 0;
		uint16_t vert_dens  = 0;

		// Whether to print adjacent pixels (ignored).
		bool adjacent = false;

		// Bytes per column.
		uint8_t bytes_column = 0;

		// Bytes left to read before image is done.
		uint16_t bytes_left = 0;

		// Bytes of the current and last column.
		std::array<uint8_t, 6> column = {};

		// Bytes read so far for the current column.
		uint8_t read_bytes_column = 0;

		// Column index and cur_x at the start of the current ESC *
		// run. PrintBitGraph derives each column's position as
		// `base_x + col_index/horiz_dens` rather than accumulating
		// cur_x by 1/horiz_dens per call.
		uint16_t col_index = 0;
		double base_x      = 0.0;

		// True while consuming bit-image bytes that should not be
		// rendered. Used by ESC ^ (9-pin graphics) so the data is
		// pulled out of the stream without ink reaching the page.
		bool discard_data = false;
	} bit_graph{};

	// Image density modes used by the ESC K / L / Y / Z commands.
	uint8_t densk = 0;
	uint8_t densl = 0;
	uint8_t densy = 0;
	uint8_t densz = 0;

	// Currently used ASCII => Unicode mapping.
	std::array<uint16_t, 256> cur_map = {};

	// Character tables (codepage slots 0..3, see ESC ( t).
	std::array<uint16_t, 4> char_tables = {};

	// Unit used by some ESC/P2 commands (negative => use default).
	double defined_unit = -1.0;

	// True if multipoint (scalable) mode is enabled.
	bool multipoint = false;

	// Point size of font in multipoint mode.
	double multi_point_size = 0.0;

	// CPI used in multipoint mode.
	double multi_cpi = 0.0;

	// Horizontal motion index (in inch; overrides CPI settings).
	double hmi = -1.0;

	// MSB mode.
	uint8_t msb = 0;

	// Number of bytes to print as characters (even when normally control
	// codes).
	uint16_t num_print_as_char = 0;
};

} // namespace VirtualPrinter

#endif // DOSBOX_PRIVATE_PRINTER_H
