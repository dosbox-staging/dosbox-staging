// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox_config.h"

#if C_PRINTER

#if !defined __PRINTER_H
#define __PRINTER_H

#include "misc/types.h"

#ifdef C_LIBPNG
#include <png.h>
#endif

#include "SDL.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#define Z_BEST_COMPRESSION 9
#define Z_DEFAULT_STRATEGY 0

#define STYLE_PROP               0x01
#define STYLE_CONDENSED          0x02
#define STYLE_BOLD               0x04
#define STYLE_DOUBLESTRIKE       0x08
#define STYLE_DOUBLEWIDTH        0x10
#define STYLE_ITALICS            0x20
#define STYLE_UNDERLINE          0x40
#define STYLE_SUPERSCRIPT        0x80
#define STYLE_SUBSCRIPT          0x100
#define STYLE_STRIKETHROUGH      0x200
#define STYLE_OVERSCORE          0x400
#define STYLE_DOUBLEWIDTHONELINE 0x800
#define STYLE_DOUBLEHEIGHT       0x1000

#define SCORE_NONE         0x00
#define SCORE_SINGLE       0x01
#define SCORE_DOUBLE       0x02
#define SCORE_SINGLEBROKEN 0x05
#define SCORE_DOUBLEBROKEN 0x06

#define QUALITY_DRAFT 0x01
#define QUALITY_LQ    0x02

#define COLOR_BLACK 7 << 5

enum Typeface {
	roman = 0,
	sansserif,
	courier,
	prestige,
	script,
	ocrb,
	ocra,
	orator,
	orators,
	scriptc,
	romant,
	sansserifh,
	svbusaba = 30,
	svjittra = 31
};

class CPrinter {
public:
	CPrinter(uint16_t dpi, uint16_t width, uint16_t height, char* output,
	         bool multipage_output);
	virtual ~CPrinter();

	// Owns FreeType/SDL resources and a singleton CPrinter* — don't copy.
	CPrinter(const CPrinter&)            = delete;
	CPrinter& operator=(const CPrinter&) = delete;

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
	// used to fill the color "sub-pallettes"
	void FillPalette(uint8_t redmax, uint8_t greenmax, uint8_t bluemax,
	                 uint8_t colorID, SDL_Palette* pal);

	// Checks if given char belongs to a command and process it. If false,
	// the character should be printed
	bool ProcessCommandChar(uint8_t ch);

	// Resets the printer to the factory settings
	void ResetPrinter();

	// Reload font. Must be called after changing dpi, style or cpi
	void UpdateFont();

	// Clears page. If save is true, saves the current page to a bitmap
	void NewPage(bool save, bool resetx);

	// Blits the given glyph on the page surface. If add is true, the values
	// of bitmap are added to the values of the pixels in the page
	void BlitGlyph(FT_Bitmap bitmap, uint16_t destx, uint16_t desty, bool add);

	// Draws an anti-aliased line from (fromx, y) to (tox, y). If broken is
	// true, gaps are included
	void DrawLine(uint64_t fromx, uint64_t tox, uint64_t y, bool broken);

	// Setup the bit_graph structure
	void SetupBitImage(uint8_t dens, uint16_t numCols);

	// Process a character that is part of bit image. Must be called iff
	// bit_graph.rem_bytes > 0.
	void PrintBitGraph(uint8_t ch);

	// Copies the codepage mapping from the constant array to CurMap
	void SelectCodepage(uint16_t cp);

	// Output current page
	void OutputPage();

	// Prints out a byte using ASCII85 encoding (only outputs something
	// every four bytes). When b>255, closes the ASCII85 string
	void FprintAscii85(FILE* f, uint16_t b);

	// Closes a multipage document
	void FinishMultipage();

	// Returns value of the num-th pixel (couting left-right, top-down) in a
	// safe way
	uint8_t GetPixel(uint32_t num);

	FT_Library ft_lib = nullptr; // FreeType2 library used to render the
	                             // characters

	SDL_Surface* page = nullptr; // Surface representing the current page
	FT_Face cur_font = nullptr; // The font currently used to render characters
	uint8_t color = 0;

	Real64 cur_x = 0.0, cur_y = 0.0; // Position of the print head (in inch)

	uint16_t dpi     = 0;  // dpi of the page
	uint16_t esc_cmd = 0;  // ESC-command that is currently processed
	bool esc_seen = false; // True if last read character was an ESC (0x1B)
	bool fs_seen  = false; // True if last read character was an FS (0x1C)
	                       // (IBM commands)

	uint8_t num_param = 0, needed_param = 0; // Numbers of parameters already
	                                         // read/needed to process command

	uint8_t params[20] = {}; // Buffer for the read params
	uint16_t style     = 0;  // Style of font (see STYLE_* constants)
	Real64 cpi = 0.0, act_cpi = 0.0; // CPI value set by program and the actual
	                                 // one (taking in account font types)
	uint8_t score = 0; // Score for lines (see SCORE_* constants)

	Real64 top_margin = 0.0, bottom_margin = 0.0, right_margin = 0.0,
	       left_margin = 0.0; // Margins of the page (in inch)
	Real64 page_width = 0.0, page_height = 0.0; // Size of page (in inch)
	Real64 default_page_width = 0.0, default_page_height = 0.0; // Default
	                                                            // size of
	                                                            // page (in
	                                                            // inch)
	Real64 line_spacing = 0.0; // Size of one line (in inch)

	Real64 horiz_tabs[32]  = {}; // Stores the set horizontal tabs (in inch)
	uint8_t num_horiz_tabs = 0;  // Number of configured tabs

	Real64 vert_tabs[16]  = {}; // Stores the set vertical tabs (in inch)
	uint8_t num_vert_tabs = 0;  // Number of configured tabs

	uint8_t cur_char_table = 0; // Currently used char table und charset
	uint8_t print_quality  = 0; // Print quality (see QUALITY_* constants)

	Typeface lq_typeface = roman; // Typeface used in LQ printing mode

	Real64 extra_intra_space = 0.0; // Extra space between two characters
	                                // (set by program, in inch)

	bool char_read = false; // True if a character was read since the
	                        // printer was last initialized
	bool auto_feed = false; // True if a LF should automatically added after
	                        // a CR
	bool print_upper_contr = false; // True if the upper command characters
	                                // should be printed

	struct BitGraphicParams // Holds information about printing bit images
	{
		uint16_t horiz_dens = 0, vert_dens = 0; // Density of image to
		                                        // print (in dpi)
		bool adjacent = false;    // Print adjacent pixels? (ignored)
		uint8_t bytes_column = 0; // Bytes per column
		uint16_t rem_bytes = 0; // Bytes left to read before image is done
		uint8_t column[6] = {}; // Bytes of the current and last column
		uint8_t read_bytes_column = 0; // Bytes read so far for the
		                               // current column
	} bit_graph{};

	uint8_t densk = 0, densl = 0, densy = 0, densz = 0; // Image density
	                                                    // modes used in ESC
	                                                    // K/L/Y/Z commands

	uint16_t cur_map[256]   = {}; // Currently used ASCII => Unicode mapping
	uint16_t char_tables[4] = {}; // Charactertables

	Real64 defined_unit = -1.0; // Unit used by some ESC/P2 commands
	                            // (negative => use default)

	bool multipoint         = false; // If multipoint mode is enabled
	Real64 multi_point_size = 0.0; // Point size of font in multipoint mode
	Real64 multi_cpi        = 0.0; // CPI used in multipoint mode

	Real64 hmi = -1.0; // Horizontal motion index (in inch; overrides CPI
	                   // settings)

	uint8_t msb = 0;                // MSB mode
	uint16_t num_print_as_char = 0; // Number of bytes to print as characters
	                                // (even when normally control codes)

	char* output         = nullptr; // Output method selected by user
	FILE* output_handle  = nullptr; // If not null, additional pages will be
	                                // appended to the given handle
	bool multipage_output = false; // If true, all pages are combined to one
	                               // file/print job etc. until the "eject
	                               // page" button is pressed
	uint16_t multipage_counter = 0; // Current page (when printing multipages)

	uint8_t ascii85_buffer[4]  = {}; // Buffer used in ASCII85 encoding
	uint8_t ascii85_buffer_pos = 0;  // Position in ASCII85 encode buffer
	uint8_t ascii85_cur_col = 0; // Columns printed so far in the current lines
};

#endif

#endif
