// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox_config.h"

#if C_PRINTER

#if !defined __PRINTER_H
#define __PRINTER_H

#include "misc/types.h"
#include "utils/bit_view.h"

#ifdef C_LIBPNG
#include <png.h>
#endif

#include "SDL.h"

#include <ft2build.h>
#include FT_FREETYPE_H

// zlib compression levels duplicated locally to avoid pulling in zlib.h.
constexpr int ZBestCompression = 9;
constexpr int ZDefaultStrategy = 0;

// Currently-active text style flags. Modelled on LptStatusRegister in
// src/hardware/lpt.h: writes go through the named bit_view members,
// reads through either the members or the 'data' field for masking.
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

#define SCORE_NONE         0x00
#define SCORE_SINGLE       0x01
#define SCORE_DOUBLE       0x02
#define SCORE_SINGLEBROKEN 0x05
#define SCORE_DOUBLEBROKEN 0x06

#define QUALITY_DRAFT 0x01
#define QUALITY_LQ    0x02

// Palette is split into 8 sub-palettes of 32 colours each, indexed by a
// 3-bit colour ID in the top 3 bits. Black is sub-palette 7.
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
	Printer(uint16_t dpi, uint16_t width, uint16_t height, char* output,
	        bool multipage_output);
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
	// Fill one of the eight 32-entry colour sub-palettes.
	void FillPalette(uint8_t red_max, uint8_t green_max, uint8_t blue_max,
	                 uint8_t color_id, SDL_Palette* palette);

	// Checks if given char belongs to a command and process it. If false,
	// the character should be printed
	bool ProcessCommandChar(uint8_t ch);

	// Resets the printer to the factory settings
	void ResetPrinter();

	// Reload font. Must be called after changing dpi, style or cpi
	void UpdateFont();

	// Clears page. If save is true, saves the current page to a bitmap
	void NewPage(bool save, bool reset_x);

	// Blits the given glyph on the page surface. If add is true, the values
	// of bitmap are added to the values of the pixels in the page
	void BlitGlyph(FT_Bitmap bitmap, uint16_t destx, uint16_t desty, bool add);

	// Draws an anti-aliased line from (fromx, y) to (tox, y). If broken is
	// true, gaps are included
	void DrawLine(uint64_t fromx, uint64_t tox, uint64_t y, bool broken);

	// Setup the bit_graph structure
	void SetupBitImage(uint8_t density, uint16_t num_cols);

	// Process a character that is part of bit image. Must be called iff
	// bit_graph.rem_bytes > 0.
	void PrintBitGraph(uint8_t ch);

	// Copies the codepage mapping from the constant array to CurMap
	void SelectCodepage(uint16_t codepage);

	// Output current page
	void OutputPage();

	// Decode a little-endian 16-bit ESC/P2 parameter starting at params[i].
	uint16_t Param16(int i) const
	{
		return static_cast<uint16_t>(params[i + 1] * 256 + params[i]);
	}

	// Current head position expressed as a pixel coordinate at the
	// configured DPI.
	uint64_t PixX() const
	{
		return static_cast<uint64_t>(floor(cur_x * dpi + 0.5));
	}
	uint64_t PixY() const
	{
		return static_cast<uint64_t>(floor(cur_y * dpi + 0.5));
	}

	// Prints out a byte using ASCII85 encoding (only outputs something
	// every four bytes). When b>255, closes the ASCII85 string
	void FprintAscii85(FILE* file, uint16_t byte);

	// Closes a multipage document
	void FinishMultipage();

	// Returns value of the num-th pixel (couting left-right, top-down) in a
	// safe way
	uint8_t GetPixel(uint32_t num);

	// FreeType2 library used to render the characters.
	FT_Library ft_lib = nullptr;

	// Surface representing the current page.
	SDL_Surface* page = nullptr;

	// The font currently used to render characters.
	FT_Face cur_font = nullptr;

	uint8_t color = 0;

	// Position of the print head (in inch).
	Real64 cur_x = 0.0, cur_y = 0.0;

	// Page resolution in dots per inch.
	uint16_t dpi = 0;

	// ESC command currently being processed.
	uint16_t esc_cmd = 0;

	// True if last read character was an ESC (0x1B).
	bool esc_seen = false;

	// True if last read character was an FS (0x1C) (IBM commands).
	bool fs_seen = false;

	// Numbers of parameters already read / needed to process command.
	uint8_t num_param = 0, needed_param = 0;

	// Buffer for the read parameters.
	uint8_t params[20] = {};

	// Bitmask of currently active text styles.
	PrinterStyle style = {};

	// CPI value set by program and the actual one (taking font types into
	// account).
	Real64 cpi = 0.0, act_cpi = 0.0;

	// Score for lines (see SCORE_* constants).
	uint8_t score = 0;

	// Margins of the page (in inch).
	Real64 top_margin = 0.0, bottom_margin = 0.0, right_margin = 0.0,
	       left_margin = 0.0;

	// Size of the current page (in inch).
	Real64 page_width = 0.0, page_height = 0.0;

	// Default size of the page (in inch).
	Real64 default_page_width = 0.0, default_page_height = 0.0;

	// Size of one line (in inch).
	Real64 line_spacing = 0.0;

	// Currently configured horizontal tabs (in inch).
	Real64 horiz_tabs[32]  = {};
	uint8_t num_horiz_tabs = 0;

	// Currently configured vertical tabs (in inch).
	Real64 vert_tabs[16]  = {};
	uint8_t num_vert_tabs = 0;

	// Currently selected character table / charset.
	uint8_t cur_char_table = 0;

	// Print quality (see QUALITY_* constants).
	uint8_t print_quality = 0;

	// Typeface used in LQ printing mode.
	Typeface lq_typeface = Typeface::Roman;

	// Extra space between two characters (set by program, in inch).
	Real64 extra_intra_space = 0.0;

	// True if a character was read since the printer was last initialised.
	bool char_read = false;

	// True if a LF should automatically be added after a CR.
	bool auto_feed = false;

	// True if the upper-half control characters should be printed.
	bool print_upper_contr = false;

	// Bit-image printing state.
	struct BitGraphicParams {
		// Density of image to print (in dpi).
		uint16_t horiz_dens = 0, vert_dens = 0;

		// Whether to print adjacent pixels (ignored).
		bool adjacent = false;

		// Bytes per column.
		uint8_t bytes_column = 0;

		// Bytes left to read before image is done.
		uint16_t rem_bytes = 0;

		// Bytes of the current and last column.
		uint8_t column[6] = {};

		// Bytes read so far for the current column.
		uint8_t read_bytes_column = 0;
	} bit_graph{};

	// Image density modes used by the ESC K / L / Y / Z commands.
	uint8_t densk = 0, densl = 0, densy = 0, densz = 0;

	// Currently used ASCII => Unicode mapping.
	uint16_t cur_map[256] = {};

	// Character tables (codepage slots 0..3, see ESC ( t).
	uint16_t char_tables[4] = {};

	// Unit used by some ESC/P2 commands (negative => use default).
	Real64 defined_unit = -1.0;

	// True if multipoint (scalable) mode is enabled.
	bool multipoint = false;

	// Point size of font in multipoint mode.
	Real64 multi_point_size = 0.0;

	// CPI used in multipoint mode.
	Real64 multi_cpi = 0.0;

	// Horizontal motion index (in inch; overrides CPI settings).
	Real64 hmi = -1.0;

	// MSB mode.
	uint8_t msb = 0;

	// Number of bytes to print as characters (even when normally control
	// codes).
	uint16_t num_print_as_char = 0;

	// Output method selected by the user.
	char* output = nullptr;

	// If not null, additional pages will be appended to the given handle.
	FILE* output_handle = nullptr;

	// If true, all pages of a print job are combined into one file until
	// the "eject page" key is pressed.
	bool multipage_output = false;

	// Current page index when printing a multipage document.
	uint16_t multipage_counter = 0;

	// State of the ASCII85 encoder used by PostScript output.
	uint8_t ascii85_buffer[4]  = {};
	uint8_t ascii85_buffer_pos = 0;
	uint8_t ascii85_cur_col    = 0;
};

#endif

#endif
