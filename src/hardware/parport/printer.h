// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

// printer.h -- header for the CPrinter class that renders an ESC/P 2
// byte stream into a per-page bitmap and writes the bitmap to disk as
// a PNG. Targets the Epson 24-pin LQ family.
//
// Primary reference (cited as "escp2ref.pdf <section>" throughout the
// printer sources):
//
//     Epson America, "EPSON ESC/P Reference Manual", revised April
//     1997. https://files.support.epson.com/pdf/general/escp2ref.pdf

#include "dosbox_config.h"

#if !defined __PRINTER_H
#define __PRINTER_H

#include "misc/types.h"

#include <png.h>

#include "SDL.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#define Z_BEST_COMPRESSION 9
#define Z_DEFAULT_STRATEGY 0

#define STYLE_PROP 0x01
#define STYLE_CONDENSED 0x02
#define STYLE_BOLD 0x04
#define STYLE_DOUBLESTRIKE 0x08
#define STYLE_DOUBLEWIDTH 0x10
#define STYLE_ITALICS 0x20
#define STYLE_UNDERLINE 0x40
#define STYLE_SUPERSCRIPT 0x80
#define STYLE_SUBSCRIPT 0x100
#define STYLE_STRIKETHROUGH 0x200
#define STYLE_OVERSCORE 0x400
#define STYLE_DOUBLEWIDTHONELINE 0x800
#define STYLE_DOUBLEHEIGHT 0x1000

#define SCORE_NONE 0x00
#define SCORE_SINGLE 0x01
#define SCORE_DOUBLE 0x02
#define SCORE_SINGLEBROKEN 0x05
#define SCORE_DOUBLEBROKEN 0x06

#define QUALITY_DRAFT 0x01
#define QUALITY_LQ 0x02

#define COLOR_BLACK 7<<5

enum Typeface
{
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
	         bool multipageOutput);
	virtual ~CPrinter();

	// Process one character sent to virtual printer
	void printChar(uint8_t ch);

	// Hard Reset (like switching printer off and on)
	void resetPrinterHard();

	// Set Autofeed value
	void setAutofeed(bool feed);

	// Get Autofeed value
	bool getAutofeed();

	// True if printer is unable to process more data right now (do not use printChar)
	bool isBusy();

	// True if the last sent character was received
	bool ack();

	// Manual formfeed
	void formFeed();

	// Returns true if the current page is blank
	bool isBlank();

private:

	// used to fill the color "sub-pallettes"
	void FillPalette(uint8_t redmax, uint8_t greenmax, uint8_t bluemax,
	                 uint8_t colorID, SDL_Palette* pal);

	// Checks if given char belongs to a command and process it. If false,
	// the character should be printed
	bool processCommandChar(uint8_t ch);

	// Resets the printer to the factory settings
	void resetPrinter();

	// Reload font. Must be called after changing dpi, style or cpi
	void updateFont();

	// Clears page. If save is true, saves the current page to a bitmap
	void newPage(bool save, bool resetx);

	// Blits the given glyph on the page surface. If add is true, the values of bitmap are
	// added to the values of the pixels in the page
	void blitGlyph(FT_Bitmap bitmap, uint16_t destx, uint16_t desty, bool add);

	// Draws an anti-aliased line from (fromx, y) to (tox, y). If broken is true, gaps are included
	void drawLine(uint64_t fromx, uint64_t tox, uint64_t y, bool broken);

	// Setup the bitGraph structure
	void setupBitImage(uint8_t dens, uint16_t numCols);

	// Process a character that is part of bit image. Must be called iff bitGraph.remBytes > 0.
	void printBitGraph(uint8_t ch);

	// Copies the codepage mapping from the constant array to CurMap
	void selectCodepage(uint16_t cp);

	// Output current page
	void outputPage();

	// Prints out a byte using ASCII85 encoding (only outputs something every four bytes). When b>255, closes the ASCII85 string
	void fprintASCII85(FILE* f, uint16_t b);

	// Closes a multipage document
	void finishMultipage();

	// Returns value of the num-th pixel (couting left-right, top-down) in a safe way
	uint8_t getPixel(uint32_t num);

	FT_Library FTlib = nullptr; // FreeType2 library used to render the
	                            // characters

	SDL_Surface* page = nullptr; // Surface representing the current page
	FT_Face curFont = nullptr; // The font currently used to render characters
	uint8_t color = 0;

	Real64 curX = 0.0, curY = 0.0; // Position of the print head (in inch)

	uint16_t dpi    = 0;  // dpi of the page
	uint16_t ESCCmd = 0;  // ESC-command that is currently processed
	bool ESCSeen = false; // True if last read character was an ESC (0x1B)
	bool FSSeen  = false; // True if last read character was an FS (0x1C)
	                      // (IBM commands)

	uint8_t numParam = 0, neededParam = 0; // Numbers of parameters already
	                                       // read/needed to process command

	uint8_t params[20] = {};        // Buffer for the read params
	uint16_t style     = 0;         // Style of font (see STYLE_* constants)
	Real64 cpi = 0.0, actcpi = 0.0; // CPI value set by program and the
	                                // actual one (taking in account font
	                                // types)
	uint8_t score = 0; // Score for lines (see SCORE_* constants)

	Real64 topMargin = 0.0, bottomMargin = 0.0, rightMargin = 0.0,
	       leftMargin = 0.0; // Margins of the page (in inch)
	Real64 pageWidth = 0.0, pageHeight = 0.0; // Size of page (in inch)
	Real64 defaultPageWidth = 0.0, defaultPageHeight = 0.0; // Default size
	                                                        // of page (in
	                                                        // inch)
	Real64 lineSpacing = 0.0; // Size of one line (in inch)

	Real64 horiztabs[32] = {}; // Stores the set horizontal tabs (in inch)
	uint8_t numHorizTabs = 0;  // Number of configured tabs

	Real64 verttabs[16] = {}; // Stores the set vertical tabs (in inch)
	uint8_t numVertTabs = 0;  // Number of configured tabs

	uint8_t curCharTable = 0; // Currently used char table und charset
	uint8_t printQuality = 0; // Print quality (see QUALITY_* constants)

	Typeface LQtypeFace = roman; // Typeface used in LQ printing mode

	Real64 extraIntraSpace = 0.0; // Extra space between two characters (set
	                              // by program, in inch)

	bool charRead = false; // True if a character was read since the printer
	                       // was last initialized
	bool autoFeed = false; // True if a LF should automatically added after
	                       // a CR
	bool printUpperContr = false; // True if the upper command characters
	                              // should be printed

	struct bitGraphicParams				// Holds information about printing bit images
	{
		uint16_t horizDens = 0, vertDens = 0; // Density of image to
		                                      // print (in dpi)
		bool adjacent       = false; // Print adjacent pixels? (ignored)
		uint8_t bytesColumn = 0;     // Bytes per column
		uint16_t remBytes = 0; // Bytes left to read before image is done
		uint8_t column[6] = {}; // Bytes of the current and last column
		uint8_t readBytesColumn = 0; // Bytes read so far for the
		                             // current column
	} bitGraph{};

	uint8_t densk = 0, densl = 0, densy = 0, densz = 0; // Image density
	                                                    // modes used in ESC
	                                                    // K/L/Y/Z commands

	uint16_t curMap[256]   = {}; // Currently used ASCII => Unicode mapping
	uint16_t charTables[4] = {}; // Charactertables

	Real64 definedUnit = -1.0; // Unit used by some ESC/P2 commands
	                           // (negative => use default)

	bool multipoint       = false; // If multipoint mode is enabled
	Real64 multiPointSize = 0.0;   // Point size of font in multipoint mode
	Real64 multicpi       = 0.0;   // CPI used in multipoint mode

	Real64 hmi = -1.0; // Horizontal motion index (in inch; overrides CPI
	                   // settings)

	uint8_t msb             = 0; // MSB mode
	uint16_t numPrintAsChar = 0; // Number of bytes to print as characters
	                             // (even when normally control codes)

	char* output       = nullptr;  // Output method selected by user
	void* outputHandle = nullptr;  // If not null, additional pages will be
	                               // appended to the given handle
	bool multipageOutput = false;  // If true, all pages are combined to one
	                               // file/print job etc. until the "eject
	                               // page" button is pressed
	uint16_t multiPageCounter = 0; // Current page (when printing multipages)

	uint8_t ASCII85Buffer[4] = {}; // Buffer used in ASCII85 encoding
	uint8_t ASCII85BufferPos = 0;  // Position in ASCII85 encode buffer
	uint8_t ASCII85CurCol = 0; // Columns printed so far in the current lines
};

#endif
