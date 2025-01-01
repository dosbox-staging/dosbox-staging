/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2025-2025  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dos_code_page.h"

#include "../ints/int10.h"
#include "bios.h"
#include "checks.h"
#include "dos_locale.h"
#include "support.h"
#include "mem.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

CHECK_NARROWING();

// References:
// - https://www.seasip.info/DOS/CPI/cpi.html
// - http://kbd-project.org/docs/font-formats/font-formats-3.html

// ***************************************************************************
// Storage data types
// ***************************************************************************

constexpr size_t NumCharacters = 256;

using ScreenFont_8x8  = std::array<uint8_t, NumCharacters * 8>;
using ScreenFont_8x14 = std::array<uint8_t, NumCharacters * 14>;
using ScreenFont_8x16 = std::array<uint8_t, NumCharacters * 16>;

struct ScreenFont {
	ScreenFont_8x16 font_8x16 = {};
	ScreenFont_8x14 font_8x14 = {};
	ScreenFont_8x8  font_8x8  = {};

	bool operator==(const ScreenFont& other) const {
		return font_8x16 == other.font_8x16 &&
		       font_8x14 == other.font_8x14 &&
		       font_8x8  == other.font_8x8;
	}
};

using ScreenFonts = std::unordered_map<uint16_t, ScreenFont>;

// ***************************************************************************
// Screen font storage
// ***************************************************************************

static ScreenFonts screen_font_storage = {};

// ***************************************************************************
// CPI file structure reader
// ***************************************************************************

struct FontFileHeader
{
	size_t struct_position = 0;

	uint8_t     id_byte      = 0;
	std::string id_string    = {};
	uint16_t    num_pointers = 0;
	uint8_t     pointer_type = 0;
	uint32_t    fih_offset   = 0;
};

struct FontFileHeader_DrDos
{
	size_t struct_position = 0;

	uint8_t fonts_per_codepage       = 0;
	std::vector<uint8_t>  cell_size  = {};
	std::vector<uint32_t> dfd_offset = {};
};

struct FontInfoHeader
{
	size_t struct_position = 0;

	uint16_t num_code_pages = 0;
};

struct CodePageEntryHeader
{
	size_t struct_position = 0;

	uint16_t cpeh_size        = 0;
	uint32_t next_cpeh_offset = 0;
	uint16_t device_type      = 0;
	// "EGA " or "LCD " - screen
	std::string device_name   = {};
	uint16_t code_page        = 0;
	uint32_t cpih_offset      = 0;
};

struct CodePageInfoHeader
{
	size_t struct_position = 0;

	uint16_t version   = 0;
	uint16_t num_fonts = 0;
	uint16_t size      = 0;
};

struct ScreenFontHeader
{
	size_t struct_position = 0;

	uint8_t height    = 0;
	uint8_t width     = 0;
	uint8_t y_aspect  = 0;
	uint8_t x_aspect  = 0;
	uint16_t num_chars = 0;
};

#if 0
struct PrinterFontHeader
{
	size_t struct_position = 0;

	// 1 - character set has to be uploaded to the printer
	// 2 - the printer already have the character set, we need to select it
	uint16_t printer_type  = 0;

	// Number of bytes in escape sequences that follow; for printer type 1
	// there are two escape sequences (first to select built-in code page,
	// second to download and select the new one), for printer type 2
	// there is only one.
	// Escape sequences are stored in Pascal string format (they start with
	// sequence length).
	// Reference:
	// - https://www.seasip.info/DOS/CPI/cpi.html
	uint16_t escape_length = 0;
};
#endif

struct CharacterIndexTable_DrDos
{
	size_t struct_position = 0;

	std::array<uint16_t, 256> font_index = { 0 };
};

class CpiReader
{
public:
	CpiReader(const std::vector<uint8_t>& content) : content(content) {}

	size_t get_position() const;
	void set_position(const size_t new_position);
	void skip_bytes(const size_t offset);

	bool is_cpx_file() const;

	std::vector<uint8_t> read_blob(const size_t length);

	std::optional<FontFileHeader>            read_font_file_header();
	std::optional<FontFileHeader_DrDos>      read_font_file_header_drdos();
	std::optional<FontInfoHeader>            read_font_info_header();
	std::optional<CodePageEntryHeader>       read_code_page_entry_header();
	std::optional<CodePageInfoHeader>        read_code_page_info_header();
	std::optional<ScreenFontHeader>          read_screen_font_header();
	std::optional<CharacterIndexTable_DrDos> read_character_index_table_drdos();

private:
	// Just like with C++ standard library, EOF mark has to be read first
	bool is_eof() const;

	uint8_t     read_byte();
	uint16_t    read_short();
	uint32_t    read_long();
	std::string read_string(const size_t length);

	const std::vector<uint8_t>& content;
	size_t position = 0;
};

bool CpiReader::is_eof() const
{
	return position >= content.size();
}

uint8_t CpiReader::read_byte()
{
	const auto idx = position;

	position += 1;
	if (is_eof()) {
		return 0;
	}

	return content[idx];
}

uint16_t CpiReader::read_short()
{
	const auto idx = position;

	position += 2;
	if (is_eof()) {
		return 0;
	}

	const auto result = content[idx] + (content[idx + 1] << 8);
	return static_cast<uint16_t>(result);
}

uint32_t CpiReader::read_long()
{
	const auto idx = position;

	position += 4;
	if (is_eof()) {
		return 0;
	}

	const auto result = content[idx] +
                            (content[idx + 1] << 8)  +
                            (content[idx + 2] << 16) +
                            (content[idx + 3] << 24);
	return static_cast<uint32_t>(result);
}

std::string CpiReader::read_string(const size_t length)
{
	const auto idx = position;

	position += length;
	if (is_eof()) {
		return {};
	}

	return std::string(content.begin() + idx, content.begin() + idx + length);
}

size_t CpiReader::get_position() const
{
	return position;
}

void CpiReader::set_position(const size_t new_position)
{
	position = new_position;
}

void CpiReader::skip_bytes(const size_t offset)
{
	position += offset;	
}

bool CpiReader::is_cpx_file() const
{
	const std::string UpxSignature      = "UPX!";
	constexpr size_t  MaxSignatureIndex = 0x40;
	constexpr size_t  MaxFileSize       = UINT16_MAX;

	if (content.size() > MaxFileSize) {
		return false;
	}

	const auto iter = std::search(content.begin(),
	                              content.end(),
	                              UpxSignature.begin(),
	                              UpxSignature.end());

	return iter <= content.begin() + MaxSignatureIndex;
}

std::vector<uint8_t> CpiReader::read_blob(const size_t length)
{
	const auto idx = position;

	position += length;
	if (is_eof()) {
		return {};
	}

	return std::vector<uint8_t>(content.begin() + idx, content.begin() + idx + length);
}

std::optional<FontFileHeader> CpiReader::read_font_file_header()
{
	FontFileHeader result = {};

	result.struct_position = position;
	result.id_byte         = read_byte();
	result.id_string       = read_string(7);
	skip_bytes(8);
	result.num_pointers    = read_short();
	result.pointer_type    = read_byte();
	result.fih_offset      = read_long();

	if (is_eof()) {
		return {};
	}
	return result;
}

std::optional<FontFileHeader_DrDos> CpiReader::read_font_file_header_drdos()
{
	FontFileHeader_DrDos result = {};

	result.struct_position    = position;
	result.fonts_per_codepage = read_byte();

	for (size_t idx = 0; idx < result.fonts_per_codepage; ++idx) {
		result.cell_size.push_back(read_byte());
	}

	for (size_t idx = 0; idx < result.fonts_per_codepage; ++idx) {
		result.dfd_offset.push_back(read_long());
	}

	if (is_eof()) {
		return {};
	}
	return result;
}

std::optional<FontInfoHeader> CpiReader::read_font_info_header()
{
	FontInfoHeader result = {};

	result.struct_position = position;
	result.num_code_pages  = read_short();

	if (is_eof()) {
		return {};
	}
	return result;
}

std::optional<CodePageEntryHeader> CpiReader::read_code_page_entry_header()
{
	CodePageEntryHeader result = {};

	result.struct_position  = position;
	result.cpeh_size        = read_short();
	result.next_cpeh_offset = read_long();
	result.device_type      = read_short();
	result.device_name      = read_string(8);
	result.code_page        = read_short();
	skip_bytes(6);
	if (result.cpeh_size == 26) {
		result.cpih_offset = read_short();
	} else {
		result.cpih_offset = read_long();	
	}

	if (is_eof()) {
		return {};
	}
	return result;
}

std::optional<CodePageInfoHeader> CpiReader::read_code_page_info_header()
{
	CodePageInfoHeader result = {};

	result.struct_position = position;
	result.version         = read_short();
	result.num_fonts       = read_short();
	result.size            = read_short();

	if (is_eof()) {
		return {};
	}
	return result;
}

std::optional<ScreenFontHeader> CpiReader::read_screen_font_header()
{
	ScreenFontHeader result = {};

	result.struct_position = position;
	result.height          = read_byte();
	result.width           = read_byte();
	result.y_aspect        = read_byte();
	result.x_aspect        = read_byte();
	result.num_chars       = read_short();

	if (is_eof()) {
		return {};
	}
	return result;
}

std::optional<CharacterIndexTable_DrDos> CpiReader::read_character_index_table_drdos()
{
	CharacterIndexTable_DrDos result = {};

	result.struct_position = position;
	for (size_t idx = 0; idx < result.font_index.size(); ++idx) {
		result.font_index[idx] = read_short();

	}

	if (is_eof()) {
		return {};
	}
	return result;
}

// ***************************************************************************
// Extracting screen fonts out of the CPI files
// ***************************************************************************

using ScreenFontsResult = std::pair<KeyboardLayoutResult, ScreenFonts>;

static bool is_for_ega(const CodePageEntryHeader& entry_header)
{
	return (entry_header.device_type == 1) &&
	       (entry_header.device_name == "EGA     ");
}

static bool is_for_lcd(const CodePageEntryHeader& entry_header)
{
	return (entry_header.device_type == 1) &&
	       (entry_header.device_name == "LCD     ");
}

static bool is_for_printer(const CodePageEntryHeader& entry_header)
{
	if (entry_header.device_type == 2) {
		return true;
	}

	// It is reported that some printer CPI files bundled with early
	// DR-DOS release had an incorrect device type '1' - check for these

	const std::set<std::string> KnownPrinters = {
		"4201    ",
		"4208    ",
		"5202    ",
		"1050    ",
		"EPS     ",
		"PPDS    "
	};

	return (entry_header.device_type == 1) &&
	       KnownPrinters.contains(entry_header.device_name);
}

static bool is_for_display(const CodePageEntryHeader& entry_header)
{
	return (entry_header.device_type == 1) && !is_for_printer(entry_header);
}

static bool is_code_page_matching(const CodePageEntryHeader& entry_header,
                                  const uint16_t code_page_filter)
{
	return (code_page_filter == 0) ||
	       (entry_header.code_page == code_page_filter);
}

static std::optional<ScreenFont> get_screen_font_microsoft(CpiReader& reader,
                                                           const size_t font_position)
{
	const auto stored_position = reader.get_position();
	reader.set_position(font_position);

	const auto code_page_header = reader.read_code_page_info_header();
	if (!code_page_header) {
		reader.set_position(stored_position);
		return {}; // XXX warning log
	}

	if (code_page_header->version != 1) {
		// NOTE: There are reports of some early Toshiba LCD fonts with
		// a value 0, but we are not using LCD fonts at the moment
		reader.set_position(stored_position);
		return {}; // XXX warning log		
	}

	// For printer fonts, the PrinterFontHeader structure starts here

	if (code_page_header->num_fonts < 3) {
		reader.set_position(stored_position);
		return {}; // XXX warning log
	}

	// Read all the font sizes we need
	ScreenFont result = {};
	
	bool has_font_8x8  = false;
	bool has_font_8x14 = false;
	bool has_font_8x16 = false;
	// Other format spotted in the wild is 8x19 (in AST Research CPI files),
	// probably for displaying a 80x25 text on a 640x480 screen
	

	const auto start_position = reader.get_position();
	for (size_t idx = 0; idx < code_page_header->num_fonts; ++idx) {
		const auto header = reader.read_screen_font_header();
		if (!header) {
			// XXX warning
			break;
		}

		bool skip_font = (header->width != 8) ||
		                 (header->num_chars != 256) ||
		                 (header->x_aspect != 0) ||
		                 (header->y_aspect != 0);

		if ((has_font_8x8  && header->height == 8)  ||
		    (has_font_8x14 && header->height == 14) ||
		    (has_font_8x16 && header->height == 16)) {
		    	if (!skip_font) {
			    	// XXX warning log
				skip_font = true;
			}
		}

		// Calculate font length in bytes
		const size_t length = header->num_chars * header->height *
		                      ((header->width + 7) / 8);

		if (skip_font) {
			reader.skip_bytes(length);
		} else {
			const auto font = reader.read_blob(length);
			if (font.size() != length) {
				// XXX warning
			} else if (header->height == 8) {
				std::copy_n(font.begin(),
				            length,
				            result.font_8x8.begin());
				has_font_8x8 = true;
			} else if (header->height == 14) {
				std::copy_n(font.begin(),
				            length,
				            result.font_8x14.begin());
				has_font_8x14 = true;
			} else if (header->height == 16) {
				std::copy_n(font.begin(),
				            length,
				            result.font_8x16.begin());
				has_font_8x16 = true;
			} else {
				assert(false);
			}
		}
	}
	const auto end_position = reader.get_position();

	if (end_position - start_position != code_page_header->size) {
		// XXX warning
	}

	reader.set_position(stored_position);
	if (has_font_8x8 && has_font_8x14 && has_font_8x16) {
		return result;
	} else {
		// XXX warning
		return {};
	}
}

static ScreenFont get_screen_font_digital_research(CpiReader& reader,
                                                   const size_t font_position)
{
	// XXX
	return {};
}

static ScreenFontsResult get_fonts_from_cpi_msdos(CpiReader& reader,
                                                  const uint32_t fih_offset,
                                                  const uint16_t code_page_filter)
{
	ScreenFontsResult result = {};
	result.first = KeyboardLayoutResult::InvalidCpiFile;

	// XXX detect segment/offset values

	// CPI files might contain a copyright info after the header
	if (fih_offset < reader.get_position()) {
		return result;
	}
	reader.set_position(fih_offset);

	// Read the FontInfoHeader structure
	const auto font_info_header = reader.read_font_info_header();
	if (!font_info_header || font_info_header->num_code_pages == 0) { // XXX how about empty file error?
		return result;
	}
	const auto num_code_pages = font_info_header->num_code_pages;

	// Go through all the code pages in the file
	for (uint16_t idx = 0; idx < num_code_pages; ++idx) {
		// Read the CodePageEntryHeader structure
		const auto entry_header = reader.read_code_page_entry_header();
		if (!entry_header) {
			break; // XXX warning log
		}

		// XXX store information from is_for_lcd, is_for_display, is_for_printer - for logging 
		if (is_for_ega(*entry_header) &&
		    is_code_page_matching(*entry_header, code_page_filter)) {
			const size_t font_position = entry_header->cpih_offset;
			const auto font = get_screen_font_microsoft(reader, font_position);
			if (font) {
				result.second[entry_header->code_page] = *font;
			}
		}

		// Seek to the next CodePageEntryHeader
		const size_t next_position = entry_header->next_cpeh_offset;
		if (next_position < reader.get_position()) {
			// Going backwards is not allowed, terminate
			break; // XXX warning log
		}
		reader.set_position(next_position);
	}

	result.first = KeyboardLayoutResult::OK;
	return result;
}

// XXX consider unification with 'get_fonts_from_cpi_msdos'
static ScreenFontsResult get_fonts_from_cpi_winnt(CpiReader& reader,
                                                  const uint32_t fih_offset,
                                                  const uint16_t code_page_filter)
{
	ScreenFontsResult result = {};
	result.first = KeyboardLayoutResult::InvalidCpiFile;

	// CPI files might contain a copyright info after the header
	if (fih_offset < reader.get_position()) {
		return result;
	}
	reader.set_position(fih_offset);

	// Read the FontInfoHeader structure
	const auto font_info_header = reader.read_font_info_header();
	if (!font_info_header || font_info_header->num_code_pages == 0) { // XXX how about empty file error?
		return result;
	}
	const auto num_code_pages = font_info_header->num_code_pages;

	// Go through all the code pages in the file
	for (uint16_t idx = 0; idx < num_code_pages; ++idx) {
		// Read the CodePageEntryHeader structure
		const auto entry_header = reader.read_code_page_entry_header();
		if (!entry_header) {
			break; // XXX warning log
		}

		// XXX store information from is_for_lcd, is_for_display, is_for_printer - for logging 
		if (is_for_ega(*entry_header) &&
		    is_code_page_matching(*entry_header, code_page_filter)) {
			const size_t font_position = entry_header->struct_position +
		                                     entry_header->cpih_offset;
			const auto font = get_screen_font_microsoft(reader, font_position);
			if (font) {
				result.second[entry_header->code_page] = *font;
			}
		}

		// Seek to the next CodePageEntryHeader
		const size_t next_position = entry_header->struct_position +
		                             entry_header->next_cpeh_offset;
		if (next_position < reader.get_position()) {
			// Going backwards is not allowed, terminate
			break; // XXX warning log
		}
		reader.set_position(next_position);
	}

	result.first = KeyboardLayoutResult::OK;
	return result;
}

static ScreenFontsResult get_fonts_from_cpi_drdos(CpiReader& reader,
                                                  const uint32_t fih_offset,
                                                  const uint16_t code_page_filter)
{
	ScreenFontsResult result = {};
	result.first = KeyboardLayoutResult::InvalidCpiFile;

	const auto header = reader.read_font_file_header_drdos();
	if (!header || header->fonts_per_codepage == 0) { // XXX how about empty file error?
		return result;
	}

	// ViewMAX display drivers and DR-DOS MODE command both assume that the
	// FontInfoHeader immediately follows the DR-DOS extended header - see:
	// - https://www.seasip.info/DOS/CPI/cpi.html
	if (fih_offset != reader.get_position()) {
		return result;
	}

	// Read the FontInfoHeader structure
	const auto font_info_header = reader.read_font_info_header();
	if (!font_info_header || font_info_header->num_code_pages == 0) { // XXX how about empty file error?
		return result;
	}
	const auto num_code_pages = font_info_header->num_code_pages;

	// Go through all the code pages in the file
	for (uint16_t idx = 0; idx < num_code_pages; ++idx) {
		// Read the CodePageEntryHeader structure
		const auto entry_header = reader.read_code_page_entry_header();
		if (!entry_header) {
			break; // XXX warning log
		}

		// XXX store information from is_for_lcd, is_for_display, is_for_printer - for logging
		if (is_for_ega(*entry_header) &&
		    is_code_page_matching(*entry_header, code_page_filter)) {
			// XXX entry_header->cpih_offset

			// XXX	
		}

		// Seek to the next CodePageEntryHeader
		const size_t next_position = entry_header->next_cpeh_offset;
		if (next_position < reader.get_position()) {
			// Going backwards is not allowed, terminate
			break; // XXX warning log
		}
		reader.set_position(next_position);
	}

	result.first = KeyboardLayoutResult::OK;
	return result;
}

static ScreenFontsResult get_fonts_from_cpi(const std::vector<uint8_t>& content,
                                            const uint16_t code_page_filter = 0)
{
	ScreenFontsResult result = {};
	result.first = KeyboardLayoutResult::InvalidCpiFile;

	// Fetch the header
	CpiReader reader(content);
	const auto header = reader.read_font_file_header();
	if (!header) {
		return result;
	}

	// Check if header is valid
	bool is_header_valid = true;
	if (header->num_pointers != 1) {
		// All known CPI files have just 1 pointer; it is not clear how
		// the header should look like if there is more than 1 (do they
		// have the same pointer type?)
		is_header_valid = false;
	}
	if (header->pointer_type != 1) {
		// All known CPI files have pointer type 1; it is unknown what
		// the other values mean
		is_header_valid = false;
	}

	// Determine file format - read the first 8 bytes of the FontFileHeader
	if (is_header_valid) {
		if (header->id_byte == 0xff && header->id_string == "FONT   ") {
			return get_fonts_from_cpi_msdos(reader, header->fih_offset, code_page_filter);
		}
		if (header->id_byte == 0xff && header->id_string == "FONT.NT") {
			return get_fonts_from_cpi_winnt(reader, header->fih_offset, code_page_filter);
		}
		if (header->id_byte == 0x7f && header->id_string == "DRFONT ") {
			return get_fonts_from_cpi_drdos(reader, header->fih_offset, code_page_filter);
		}
	}

	// Not a supported file format; check for FreeDOS CPX file
	if (reader.is_cpx_file()) {
		result.first = KeyboardLayoutResult::UnsupportedCpiFileFreeDOS;
	}

	return result;
}

// ***************************************************************************
// Resource management
// ***************************************************************************

// XXX struct, different constructors, same for ScreenFontsResult
using ScreenFontResult = std::pair<KeyboardLayoutResult, ScreenFont>;

static ScreenFontResult get_custom_font(const uint16_t code_page,
                                        const std::string& file_name)
{
	static uint32_t MaxFileSize = 500 * 1024;

	ScreenFontResult result = {};

	uint16_t handle = {};
	if (!DOS_OpenFile(file_name.c_str(), OPEN_READ, &handle)) {
		result.first = KeyboardLayoutResult::CpiFileNotFound;
		return result;
	}

	uint32_t position = {};
	if (!DOS_SeekFile(handle, &position, DOS_SEEK_END))
	{
		// XXX error reading file
		DOS_CloseFile(handle);
		result.first = KeyboardLayoutResult::CpiFileNotFound;
		return result;
	}
	const auto file_size = position;

	if (file_size > MaxFileSize) {
		// XXX file too large
		DOS_CloseFile(handle);
		result.first = KeyboardLayoutResult::CpiFileNotFound;
		return result;	
	}

	position = 0;
	if (!DOS_SeekFile(handle, &position, DOS_SEEK_SET))
	{
		// XXX error reading file
		DOS_CloseFile(handle);
		result.first = KeyboardLayoutResult::CpiFileNotFound;
		return result;
	}

	std::vector<uint8_t> content(file_size);
	uint32_t bytes_already_read = 0;
	while (bytes_already_read != file_size) {
		const uint16_t chunk_size = std::min(UINT16_MAX, static_cast<int>(file_size - bytes_already_read));
		uint16_t bytes_to_read = chunk_size;
		if (!DOS_ReadFile(handle, content.data() + bytes_already_read, &bytes_to_read) ||
		    bytes_to_read != chunk_size)
		{
			// XXX error reading file
			DOS_CloseFile(handle);
			result.first = KeyboardLayoutResult::CpiFileNotFound;
			return result;
		}
		bytes_already_read += chunk_size;
	}

	DOS_CloseFile(handle);

	// XXX pass the name, for logging
	const auto result_get = get_fonts_from_cpi(content, code_page);
	if (result_get.first != KeyboardLayoutResult::OK) {
		// XXX warning log
		result.first = result_get.first;
		return result;
	}

	if (!result_get.second.contains(code_page)) {
		result.first = KeyboardLayoutResult::NoCodePageInCpiFile;
		return result;	
	}

	result.first  = KeyboardLayoutResult::OK;
	result.second = result_get.second.at(code_page);

	return result;
}


static void add_fonts_to_resources(const ScreenFonts &screen_fonts,
                                   const std::string& file_name)
{
	for (const auto& entry : screen_fonts) {
		const auto code_page = entry.first;		
		if (screen_font_storage.contains(code_page)) {
			continue;
		}

		// XXX investigate this
		// There are at least two slighlty different (but visually
		// almost identical) code page 852 fonts in the FreeDOS bundle;
		// make sure we always use the same variant
		if (DOS_GetBundledCpiFileName(code_page) != file_name) {
			continue;
		}

		screen_font_storage[code_page] = entry.second;
	}
}

static void maybe_read_bundled_font(const uint16_t code_page)
{
	static const std::string ResourceDir = "freedos-cpi"; // XXX move such constants to the top of the file

	if (screen_font_storage.contains(code_page)) {
		return;
	}

	// Check if we have a bundled CPI file for the code page
	const auto file_name = DOS_GetBundledCpiFileName(code_page);
	if (file_name.empty()) {
		return;
	}

	// Do not attempt to load the same bundled CPI file twice
	static std::set<std::string> already_read = {};
	if (already_read.contains(file_name)) {
		return;
	}
	already_read.insert(file_name);

	// Open the file
	const auto resource_path = get_resource_path(ResourceDir, file_name);
	std::ifstream file_stream = {};
	file_stream.open(resource_path, std::ios::binary);
	// XXX check if succeed

	// Check file size
	file_stream.seekg(0, std::ios::end);
	const auto file_size = file_stream.tellg();
	file_stream.seekg(0, std::ios::beg);

	// Read the file
	std::vector<uint8_t> content(file_size);
	// XXX better casting
	file_stream.read(reinterpret_cast<char *>(content.data()), file_size);
	// XXX check for errors
	file_stream.close();

	// XXX pass the name, for logging
	const auto result = get_fonts_from_cpi(content);
	if (result.first != KeyboardLayoutResult::OK) {
		// XXX warning log
		return;
	}

	add_fonts_to_resources(result.second, file_name);
}

// ***************************************************************************
// Functions to set/reset screen font
// ***************************************************************************

static void set_screen_font(const ScreenFont &screen_font)
{
	const auto& font_8x16 = screen_font.font_8x16;
	const auto& font_8x14 = screen_font.font_8x14;
	const auto& font_8x8  = screen_font.font_8x8;

	// Set the 8x16 font
	const auto memory_8x16 = RealToPhysical(int10.rom.font_16);
	for (uint16_t idx = 0; idx < font_8x16.size(); ++idx) {
		phys_writeb(memory_8x16 + idx, font_8x16[idx]);
	}
	// Clear pointer to the alternate font to prevent swiching
	phys_writeb(RealToPhysical(int10.rom.font_16_alternate), 0);

	// Set the 8x14 font
	const auto memory_8x14 = RealToPhysical(int10.rom.font_14);
	for (uint16_t idx = 0; idx < font_8x14.size(); ++idx) {
		phys_writeb(memory_8x14 + idx, font_8x14[idx]);
	}
	// Clear pointer to the alternate font to prevent swiching
	phys_writeb(RealToPhysical(int10.rom.font_14_alternate), 0);

	// Set the 8x8 font
	const uint16_t threshold = font_8x8.size() / 2;
	const auto memory_8x8_first  = RealToPhysical(int10.rom.font_8_first);
	const auto memory_8x8_second = RealToPhysical(int10.rom.font_8_second) - threshold;
	for (uint16_t idx = 0; idx < threshold; ++idx) {
		phys_writeb(memory_8x8_first + idx, font_8x8[idx]);
	}
	for (uint16_t idx = threshold; idx < font_8x8.size(); ++idx) {
		phys_writeb(memory_8x8_second + idx, font_8x8[idx]);
	}

	if (CurMode->type == M_TEXT) {
		INT10_ReloadFont();
	}
	INT10_SetupRomMemoryChecksum();
}

static void reset_screen_font()
{
	INT10_ReloadRomFonts();
	if (CurMode->type == M_TEXT) {
		INT10_ReloadFont();
	}
}

// ***************************************************************************
// External interface
// ***************************************************************************

bool DOS_CanLoadScreenFonts()
{
	return machine == MCH_EGA || machine == MCH_VGA;
}

KeyboardLayoutResult DOS_LoadScreenFont(const uint16_t code_page,
                                        const std::string& file_name)
{
	if (!DOS_CanLoadScreenFonts()) {
		return KeyboardLayoutResult::IncompatibleMachine;
	}

	// XXX check for code page 0

	if (!file_name.empty()) {
		// Get full file name, for the log
	   	uint8_t drive;
	   	char full_name_dir[DOS_PATHLENGTH];
		if (!DOS_MakeName(file_name.c_str(), full_name_dir, &drive)) {
			return KeyboardLayoutResult::CpiFileNotFound;
		}

		const auto result = get_custom_font(code_page, file_name);
		if (result.first != KeyboardLayoutResult::OK) {
			return result.first;
		}

		set_screen_font(result.second);
		dos.loaded_codepage    = code_page;
		dos.screen_font_origin = ScreenFontOrigin::Custom;

		LOG_MSG("LOCALE: Loaded code page %d from '%c:\\%s' file",
		        code_page, 'A' + drive, full_name_dir);

		return KeyboardLayoutResult::OK;
	}

	// XXX search for duplicate code pages
	// XXX do not put duplicate code pages into the internal database

	maybe_read_bundled_font(code_page);
	if (!screen_font_storage.contains(code_page)) {
		return KeyboardLayoutResult::NoBundledCpiFileForCodePage;
	}

	set_screen_font(screen_font_storage.at(code_page));
	dos.loaded_codepage    = code_page;
	dos.screen_font_origin = ScreenFontOrigin::Bundled;

	LOG_MSG("LOCALE: Loaded code page %d - '%s'",
		code_page,
		DOS_GetCodePageDescriptionForLog(code_page).c_str());

	return KeyboardLayoutResult::OK;
}

void DOS_SetRomScreenFont()
{
	reset_screen_font();

	dos.loaded_codepage    = DefaultCodePage;
	dos.screen_font_origin = ScreenFontOrigin::Rom;

	LOG_MSG("LOCALE: Loaded code page %d (ROM font)",
	        dos.loaded_codepage);
}
