// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos_code_page.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

#include "shell/autoexec.h"
#include "ints/bios.h"
#include "utils/checks.h"
#include "dos_locale.h"
#include "ints/int10.h"
#include "utils/math_utils.h"
#include "hardware/memory.h"
#include "utils/string_utils.h"
#include "misc/support.h"
#include "misc/unicode.h"

CHECK_NARROWING();

// References:
// - https://www.seasip.info/DOS/CPI/cpi.html
// - http://kbd-project.org/docs/font-formats/font-formats-3.html

extern void DOS_UpdateCurrentProgramName();

static void notify_code_page_changed()
{
	// Recreate various information to match new code page
	MSG_NotifyNewCodePage();
	DOS_UpdateCurrentProgramName();
	DOS_RepopulateCountryInfo();
	AUTOEXEC_RefreshFile();
}

// ***************************************************************************
// Constants
// ***************************************************************************

static const std::string ResourceDir = "freedos-cpi";

enum class FileFormat {
	// Not yet detected or unrecognized
	Unknown,
	// Used by MS-DOS and most clones
	MsDos,
	// Used by Windows NT, slight variation of MS-DOS format
	WinNt,
	// Used by DR-DOS, encodes screen fonts in much more efficient way
	DrDos
};

// Supported file size limit, just for some extra safety.
// - Almost all the CPI files in the wild are below 64 KB size (MS-DOS does not
//   support anything bigger)
// - 'DISPLAY.CPI' file from PTS-DOS 2000 is below 88 KB
// - 'EGA.CPI' file from Windows 2000 (English) is slightly below 128 KB
constexpr uint32_t MaxFileSizeBytes = 1024 * 1024;


namespace FileId {
	constexpr uint8_t MsDosWinNt = 0xff;
	constexpr uint8_t DrDos      = 0x7f;
}

namespace Signature {
	static const std::string MsDos = "FONT   ";
	static const std::string WinNt = "FONT.NT";
	static const std::string DrDos = "DRFONT ";

	static const std::string Upx = "UPX!";
}

namespace DeviceName {
	static const std::string Ega = "EGA";
	static const std::string Lcd = "LCD";

	static const std::set<std::string> KnownPrinters =
	        {"4201", "4208", "5202", "1050", "EPS", "PPDS"};
}

// Some CPI files (like 'DISPLAY.CPI' from PTS-DOS 2000 or '4208.CPI' from
// MS-DOS 6.22) contain obviously dummy size or offset values in some places;
// ignore them and don't spam the log output with warnings.
static const std::set<uint16_t> DummySizeValues   = {0, 1, UINT16_MAX};
static const std::set<uint32_t> DummyOffsetValues = {0, 1, UINT32_MAX};

// ***************************************************************************
// Storage data types
// ***************************************************************************

// Number of characters we need; the real font in the CPI file can contain a
// different number of characters.
constexpr uint16_t NumCharactersInFont = 256;

struct ScreenFont {
	static constexpr uint32_t FullSize_8x16 = NumCharactersInFont * 16;
	static constexpr uint32_t FullSize_8x14 = NumCharactersInFont * 14;
	static constexpr uint32_t FullSize_8x8  = NumCharactersInFont * 8;

	std::vector<uint8_t> font_8x16 = {};
	std::vector<uint8_t> font_8x14 = {};
	std::vector<uint8_t> font_8x8  = {};
};

using ScreenFonts = std::unordered_map<uint16_t, ScreenFont>;

// ***************************************************************************
// Persistent data storage
// ***************************************************************************

static ScreenFonts ega_font_storage = {};

// ***************************************************************************
// CPI file structure reader
// ***************************************************************************

struct FontFileHeader {
	static constexpr auto StructName = "FontFileHeader";

	uint32_t struct_offset = 0;

	// File format identification data
	uint8_t id_byte       = 0;
	std::string id_string = {};
	// Number of pointers (offsets) in the header
	uint16_t num_pointers = 0;
	// Type of pointer (offset) in the header
	uint8_t pointer_type = 0;
	// Offset to the FontInfoHeader structure
	uint32_t fih_offset = 0;
};

// DR-DOS format specific structure
struct FontFileExtendedHeader {
	static constexpr auto StructName = "FontFileExtendedHeader";

	uint32_t struct_offset = 0;

	// This structure is specific to DR-DOS file format

	// Number of fonts (different sizes) for each code page
	uint8_t fonts_per_codepage = 0;
	// Size of character in bytes, separate value for each font size
	std::vector<uint8_t> cell_size = {};
	// Offset to the first byte of character bitmaps for each font size
	std::vector<uint32_t> dfd_offset = {};
};

struct FontInfoHeader {
	static constexpr auto StructName = "FontInfoHeader";

	uint32_t struct_offset = 0;

	// Number of code pages in the file
	uint16_t num_code_pages = 0;
};

struct CodePageEntryHeader {
	static constexpr auto StructName = "CodePageEntryHeader";

	uint32_t struct_offset = 0;

	// Size of 'CodePageEntryHeader' structure in bytes
	uint16_t cpeh_size = 0;
	// Offset to the next 'CodePageEntryHeader' structure
	uint32_t next_cpeh_offset = 0;
	// Type and name of device the code page is indended for
	uint16_t device_type    = 0;
	std::string device_name = {};
	// Code page identifier
	uint16_t code_page = 0;
	// Offset to 'CodePageInfoHeader' structure
	uint32_t cpih_offset = 0;
};

struct CodePageInfoHeader {
	static constexpr auto StructName = "CodePageInfoHeader";

	uint32_t struct_offset = 0;

	// Font format version
	uint16_t version = 0;
	// Number of 'ScreenFontHeader' records which follow
	uint16_t num_fonts = 0;
	// Size of the font data
	uint16_t size = 0;
};

struct ScreenFontHeader {
	static constexpr auto StructName = "ScreenFontHeader";

	uint32_t struct_offset = 0;

	// Character dimension in pixels
	uint8_t height = 0;
	uint8_t width  = 0;
	// Aspect ratio (details unknown)
	uint8_t y_aspect = 0;
	uint8_t x_aspect = 0;
	// Number of characters for the font size
	uint16_t num_chars = 0;
};

#if 0
// Structure needed to decode printer fonts.
// Just for the documentation, unused for now.

struct PrinterFontHeader
{
	// 1 - character set has to be uploaded to the printer
	// 2 - the printer already have the character set
	uint16_t printer_type  = 0;

	// Number of bytes in escape sequences that follow; for printer type 1
	// there are two escape sequences (first to select built-in code page,
	// second to select the downloaded one), for printer type 2 there is
	// only one.
	// Escape sequences are stored in Pascal string format (first byte is
	// a string length).
	// Any remaining bytes up to size given in the CodePageInfoHeader are
	// a font definition which has to be sent to the printer.
	// Reference:
	// - https://www.seasip.info/DOS/CPI/cpi.html
	uint16_t escape_length = 0;

	// There are no reserved bytes in the structure
};
#endif

// DR-DOS format specific structure
struct CharacterIndexTable {
	static constexpr auto StructName = "CharacterIndexTable";

	uint32_t struct_offset = 0;

	// This structure is specific to DR-DOS file format

	// Index of bitmap, whe first one is pointed by 'dfd_offset'
	std::vector<uint16_t> font_index = {};
};

class CpiReader {
protected:
	CpiReader(const std::vector<uint8_t>& content) : content(content) {}
	virtual ~CpiReader() = default;

	size_t GetContentSize() const;
	uint32_t GetCurrentOffset() const;
	void SetCurrentOffset(const uint32_t new_offset);
	void SkipBytes(const uint32_t num_bytes);

	bool HasUpxSignature() const;

	bool IsCopyrightString() const;

	std::vector<uint8_t> ReadBlob(const uint32_t num_bytes);

	std::optional<FontFileHeader> ReadFontFileHeader();
	std::optional<FontFileExtendedHeader> ReadFontFileExtendedHeader();
	std::optional<FontInfoHeader> ReadFontInfoHeader();
	std::optional<CodePageEntryHeader> ReadCodePageEntryHeader();
	std::optional<CodePageInfoHeader> ReadCodePageInfoHeader();
	std::optional<ScreenFontHeader> ReadScreenFontHeader();
	std::optional<CharacterIndexTable> ReadCharacterIndexTable(const uint32_t num_chars);

	// Warnings support

	virtual void Warn(const uint32_t struct_offset,
	                  const std::string& struct_name,
	                  const std::string& message) = 0;

	virtual void Warn(const uint32_t offset_start,
	                  const uint32_t offset_end,
	                  const std::string& message) = 0;

	void WarnDataOutOfBounds(const uint32_t start_offset,
	                         const std::string& struct_name = {});
private:
	bool IsOffsetBeyondContent() const;

	uint8_t     ReadByte();
	uint16_t    ReadShort();
	uint32_t    ReadLong();
	std::string ReadString(const uint32_t length);

	const std::vector<uint8_t>& content;
	uint32_t offset = 0;
};

bool CpiReader::IsOffsetBeyondContent() const
{
	return offset > content.size();
}

uint8_t CpiReader::ReadByte()
{
	const auto idx = offset;

	offset += 1;
	if (IsOffsetBeyondContent()) {
		return 0;
	}

	return content[idx];
}

uint16_t CpiReader::ReadShort()
{
	const auto idx = offset;

	offset += 2;
	if (IsOffsetBeyondContent()) {
		return 0;
	}

	const auto result = content[idx] + (content[idx + 1] << 8);
	return static_cast<uint16_t>(result);
}

uint32_t CpiReader::ReadLong()
{
	const auto idx = offset;

	offset += 4;
	if (IsOffsetBeyondContent()) {
		return 0;
	}

	const auto result = content[idx] + (content[idx + 1] << 8) +
	                    (content[idx + 2] << 16) + (content[idx + 3] << 24);
	return static_cast<uint32_t>(result);
}

std::string CpiReader::ReadString(const uint32_t length)
{
	const auto idx = offset;

	offset += length;
	if (IsOffsetBeyondContent()) {
		return {};
	}

	return std::string(content.begin() + idx, content.begin() + idx + length);
}

size_t CpiReader::GetContentSize() const
{
	return content.size();
}

uint32_t CpiReader::GetCurrentOffset() const
{
	return offset;
}

void CpiReader::SetCurrentOffset(const uint32_t new_offset)
{
	offset = new_offset;
}

void CpiReader::SkipBytes(const uint32_t num_bytes)
{
	offset += num_bytes;
}

bool CpiReader::HasUpxSignature() const
{
	constexpr size_t MaxSignatureIndex = 0x40;

	const auto search_limit = std::min(MaxSignatureIndex, content.size());
	return content.end() != std::search(content.begin(),
	                                    content.begin() + search_limit,
	                                    Signature::Upx.begin(),
	                                    Signature::Upx.end());
}

bool CpiReader::IsCopyrightString() const
{
	if (offset > content.size()) {
		return false;
	}

	// Some CPI files do not have a proper number of code pages in their
	// 'FontInfoHeader' structure, and their last offset in the chain of
	// 'CodePageEntryHeader' structures points to the copyright message.
	// We need to detect this to avoid misinterpreting the file content.

	// According to MS-DOS Programmer's Reference (both for MS-DOS 5 and 6)
	// the maximum length of the copyright message is 0x150 bytes.
	constexpr size_t MaxCopyrightLength = 0x150;

	const auto remaining = content.size() - offset;
	if (remaining > MaxCopyrightLength) {
		return false;
	}

	// If we are that close to the end of the content, it is very unlikely
	// the data represent useful 'CodePageEntryHeader'. To be sure, check
	// the next two bytes, which should represent the structure size.
	if (remaining < 2) {
		return true;
	}
	const size_t value = content[offset] + (content[offset + 1] << 8);
	if (value > remaining) {
		return true;
	}

	return false;
}

std::vector<uint8_t> CpiReader::ReadBlob(const uint32_t num_bytes)
{
	const auto idx = offset;

	offset += num_bytes;
	if (IsOffsetBeyondContent()) {
		WarnDataOutOfBounds(idx);
		return {};
	}

	return std::vector<uint8_t>(content.begin() + idx,
	                            content.begin() + idx + num_bytes);
}

std::optional<FontFileHeader> CpiReader::ReadFontFileHeader()
{
	FontFileHeader result = {};

	result.struct_offset = offset;

	result.id_byte   = ReadByte();
	result.id_string = ReadString(7);
	SkipBytes(8);
	result.num_pointers = ReadShort();
	result.pointer_type = ReadByte();
	result.fih_offset   = ReadLong();

	if (IsOffsetBeyondContent()) {
		WarnDataOutOfBounds(result.struct_offset, result.StructName);
		return {};
	}
	return result;
}

std::optional<FontFileExtendedHeader> CpiReader::ReadFontFileExtendedHeader()
{
	FontFileExtendedHeader result = {};

	result.struct_offset = offset;

	result.fonts_per_codepage = ReadByte();

	for (size_t idx = 0; idx < result.fonts_per_codepage; ++idx) {
		result.cell_size.push_back(ReadByte());
	}

	for (size_t idx = 0; idx < result.fonts_per_codepage; ++idx) {
		result.dfd_offset.push_back(ReadLong());
	}

	if (IsOffsetBeyondContent()) {
		WarnDataOutOfBounds(result.struct_offset, result.StructName);
		return {};
	}
	return result;
}

std::optional<FontInfoHeader> CpiReader::ReadFontInfoHeader()
{
	FontInfoHeader result = {};

	result.struct_offset = offset;

	result.num_code_pages = ReadShort();

	if (IsOffsetBeyondContent()) {
		WarnDataOutOfBounds(result.struct_offset, result.StructName);
		return {};
	}
	return result;
}

std::optional<CodePageEntryHeader> CpiReader::ReadCodePageEntryHeader()
{
	CodePageEntryHeader result = {};

	result.struct_offset = offset;

	result.cpeh_size        = ReadShort();
	result.next_cpeh_offset = ReadLong();
	result.device_type      = ReadShort();
	result.device_name      = ReadString(8);
	result.code_page        = ReadShort();
	SkipBytes(6);

	// Strip leading device name spaces
	while (result.device_name.ends_with(" ")) {
		result.device_name.pop_back();
	}

	// According to https://www.seasip.info/DOS/CPI/cpi.html:
	// - the structure is normally 28 bytes
	// - many tools simply ignore the size
	// - sometimes (in old files) the structure is 26 bytes due to offset
	//   being stored on 2 bytes
	constexpr uint16_t SizeRegular = 28;
	constexpr uint16_t SizeShort   = 26;	

	const auto size = result.cpeh_size;
	if (size == SizeShort) {
		result.cpih_offset = ReadShort();
	} else {
		if (size != SizeRegular && !DummySizeValues.contains(size)) {
			Warn(result.struct_offset, result.StructName,
			     format_str("invalid size %d", size));
		}
		result.cpih_offset = ReadLong();
	}

	if (IsOffsetBeyondContent()) {
		WarnDataOutOfBounds(result.struct_offset, result.StructName);
		return {};
	}
	return result;
}

std::optional<CodePageInfoHeader> CpiReader::ReadCodePageInfoHeader()
{
	CodePageInfoHeader result = {};

	result.struct_offset = offset;

	result.version   = ReadShort();
	result.num_fonts = ReadShort();
	result.size      = ReadShort();

	if (IsOffsetBeyondContent()) {
		WarnDataOutOfBounds(result.struct_offset, result.StructName);
		return {};
	}
	return result;
}

std::optional<ScreenFontHeader> CpiReader::ReadScreenFontHeader()
{
	ScreenFontHeader result = {};

	result.struct_offset = offset;

	result.height    = ReadByte();
	result.width     = ReadByte();
	result.y_aspect  = ReadByte();
	result.x_aspect  = ReadByte();
	result.num_chars = ReadShort();

	if (IsOffsetBeyondContent()) {
		WarnDataOutOfBounds(result.struct_offset, result.StructName);
		return {};
	}
	return result;
}

std::optional<CharacterIndexTable> CpiReader::ReadCharacterIndexTable(const uint32_t num_chars)
{
	CharacterIndexTable result = {};

	result.struct_offset = offset;

	for (uint32_t idx = 0; idx < num_chars; ++idx) {
		result.font_index.push_back(ReadShort());
	}

	if (IsOffsetBeyondContent()) {
		WarnDataOutOfBounds(result.struct_offset, result.StructName);
		return {};
	}
	return result;
}

// ***************************************************************************
// CPI file reader - warnings support
// ***************************************************************************

void CpiReader::WarnDataOutOfBounds(const uint32_t start_offset,
                                    const std::string& struct_name)
{
	std::string message = {};
	if (struct_name.empty()) {
		message = "data out of address bounds";
	} else {
		message = format_str("structure '%s' out of address bounds",
		                     struct_name.c_str());
	}

	Warn(start_offset, offset, message);
}

// ***************************************************************************
// CPI file parser - declaration
// ***************************************************************************

struct ParserResult {
	// A general return code; 'false' if the file can't be parsed at all
	bool status_ok = true;

	// If the reason of parsing failure was an unsupported UPX compression
	bool unsupported_cpx_file = false;

	bool found_printer_fonts = false;
	bool found_screen_fonts  = false;

	// Fonts found but not necessarily extracted
	std::set<uint16_t> found_ega_fonts   = {};
	std::set<uint16_t> found_lcd_fonts   = {};
	std::set<uint16_t> found_other_fonts = {};

	// These are fonts which were extracted
	ScreenFonts extracted_ega_fonts   = {};
	ScreenFonts extracted_lcd_fonts   = {};
	ScreenFonts extracted_other_fonts = {};

	ParserResult(const bool status_ok) : status_ok(status_ok) {}
	ParserResult() = default;
};

class CpiParser : public CpiReader {
public:
	CpiParser(const std::vector<uint8_t>& content, const std::string& name_for_log)
	        : CpiReader(content),
	          name_for_log(name_for_log)
	{}

	ParserResult GetFonts(const uint16_t code_page_filter = 0);

private:
	const std::string name_for_log = {};

	FileFormat file_format = FileFormat::Unknown;

	// Offsets needed by the screen font format 2
	struct FontOffsets {
		std::optional<uint32_t> start_8x16 = {};
		std::optional<uint32_t> start_8x14 = {};
		std::optional<uint32_t> start_8x8  = {};
	};

	FontOffsets font_offsets = {};

	struct Found {
		std::set<std::string> device_names                    = {};
		std::set<std::pair<std::string, uint16_t>> code_pages = {};
	};

	Found found = {};

	// Initialization, file format detection

	void InitParser();
	void DetectFileFormat(const uint8_t id_byte, const std::string& id_string);

	// Common/generic helper functions

	bool IsFormatMsDos() const
	{
		return file_format == FileFormat::MsDos;
	}
	bool IsFormatWinNt() const
	{
		return file_format == FileFormat::WinNt;
	}
	bool IsFormatDrDos() const
	{
		return file_format == FileFormat::DrDos;
	}

	static uint32_t ConvertOffset(const uint32_t value);

	static bool IsCodePageMatching(const CodePageEntryHeader& header,
	                               const uint16_t code_page_filter);

	static bool IsDeviceNameValid(const CodePageEntryHeader& header);

	static bool IsEgaCodePage(const CodePageEntryHeader& header);
	static bool IsLcdCodePage(const CodePageEntryHeader& header);

	static bool IsPrinterCodePage(const CodePageEntryHeader& header);
	static bool IsScreenCodePage(const CodePageEntryHeader& header);

	void StoreFontType(const CodePageEntryHeader& header,
	                   ParserResult& result);

	void ExtractAndStoreFont(const CodePageEntryHeader& header,
	                         const uint32_t font_offset,
	                         ParserResult& result);

	uint32_t AdaptOffset(const uint32_t value, bool& is_adaptation_needed);

	// Concrete screen font extraction

	std::optional<ScreenFont> GetScreenFontFormat1(const uint16_t num_fonts,
	                                               const uint16_t total_size);
	std::optional<ScreenFont> GetScreenFontFormat2(const uint16_t num_fonts,
	                                               const uint16_t total_size);

	std::optional<ScreenFont> GetScreenFont(const uint32_t font_offset);

	// Top-level file structure parsers

	bool ShouldSkipCodePage(const CodePageEntryHeader& header);

	ParserResult GetFontsCommonPart(const uint16_t code_page_filter);
	ParserResult GetFontsMsDos(const uint32_t fih_offset,
	                           const uint16_t code_page_filter);
	ParserResult GetFontsDrDos(const uint32_t fih_offset,
	                           const uint16_t code_page_filter);

	// Warnings support

	struct Warnings {
		bool header_printed = false;

		bool multiple_devices_in_same_file = false;
		bool screen_printer_in_same_file   = false;
	};

	Warnings already_warned = {};

	void MaybeLogWarningHeader();

	void Warn(const std::string& message);

	void Warn(const uint32_t struct_offset,
	          const std::string& message);

	void Warn(const uint32_t struct_offset,
	          const std::string& struct_name,
	          const std::string& message) override;

	void Warn(const uint32_t offset_start,
	          const uint32_t offset_end,
	          const std::string& message) override;

	void WarnOnce(bool& already_warned, const std::string& message);
};

// ***************************************************************************
// CPI file parser - warnings support
// ***************************************************************************

void CpiParser::MaybeLogWarningHeader()
{
	if (already_warned.header_printed) {
		return;
	}

	LOG_WARNING("LOCALE: The following problems were found in %s:",
	            name_for_log.c_str());
	already_warned.header_printed = true;
}

void CpiParser::Warn(const std::string& message)
{
	MaybeLogWarningHeader();
	LOG_WARNING("LOCALE: - %s", message.c_str());
}

void CpiParser::Warn(const uint32_t struct_offset,
                     const std::string& message)
{
	MaybeLogWarningHeader();
	LOG_WARNING("LOCALE: - [0x%08x]: %s", struct_offset, message.c_str());
}

void CpiParser::Warn(const uint32_t struct_offset,
                     const std::string& struct_name,
                     const std::string& message)
{
	MaybeLogWarningHeader();
	LOG_WARNING("LOCALE: - [0x%08x] '%s': %s",
	            struct_offset,
	            struct_name.c_str(),
	            message.c_str());
}

void CpiParser::Warn(const uint32_t start_offset,
                     const uint32_t end_offset,
                     const std::string& message)
{
	MaybeLogWarningHeader();
	LOG_WARNING("LOCALE: - [0x%08x-0x%08x] %s",
	            start_offset,
	            end_offset,
	            message.c_str());
}

void CpiParser::WarnOnce(bool& already_warned, const std::string& message)
{
	if (already_warned) {
		return;
	}

	Warn(message);
	already_warned = true;
}

// ***************************************************************************
// CPI file parser - common/generic helper functions
// ***************************************************************************

uint32_t CpiParser::ConvertOffset(const uint32_t value)
{
	const auto segment = clamp_to_uint16(value / (UINT16_MAX + 1));
	const auto offset  = clamp_to_uint16(value % (UINT16_MAX + 1));

	return PhysicalMake(segment, offset);
}

bool CpiParser::IsCodePageMatching(const CodePageEntryHeader& header,
                                   const uint16_t code_page_filter)
{
	return (code_page_filter == 0) || (header.code_page == code_page_filter);
}

bool CpiParser::IsDeviceNameValid(const CodePageEntryHeader& header)
{
	const auto& device_name = header.device_name;
	if (device_name.empty()) {
		return false;
	}

	for (const auto character : device_name) {
		if (!is_printable_ascii(character)) {
			return false;
		}
	}

	return true;
}

bool CpiParser::IsEgaCodePage(const CodePageEntryHeader& header)
{
	return (header.device_type == 1) &&
	       (header.device_name == DeviceName::Ega);
}

bool CpiParser::IsLcdCodePage(const CodePageEntryHeader& header)
{
	return (header.device_type == 1) &&
	       (header.device_name == DeviceName::Lcd);
}

bool CpiParser::IsPrinterCodePage(const CodePageEntryHeader& header)
{
	if (header.device_type == 2) {
		return true;
	}

	// It is reported that some printer CPI files bundled with early
	// DR-DOS release had an incorrect device type '1' - check for these
	return (header.device_type == 1) &&
	       DeviceName::KnownPrinters.contains(header.device_name);
}

bool CpiParser::IsScreenCodePage(const CodePageEntryHeader& header)
{
	return (header.device_type == 1) && !IsPrinterCodePage(header);
}

void CpiParser::StoreFontType(const CodePageEntryHeader& header, ParserResult& result)
{
	const bool is_ega = IsEgaCodePage(header);
	const bool is_lcd = IsLcdCodePage(header);

	const bool is_screen  = IsScreenCodePage(header);
	const bool is_printer = IsPrinterCodePage(header);

	LOG_DEBUG("LOCALE: - device '%s', code page %d",
	          header.device_name.c_str(),
	          header.code_page);

	found.device_names.insert(header.device_name);
	if (found.device_names.size() > 1) {
		WarnOnce(already_warned.multiple_devices_in_same_file,
		         "found fonts for multiple devices in the same file");
	}

	if (found.code_pages.contains({header.device_name, header.code_page})) {
		Warn(header.struct_offset, header.StructName,
		     format_str("duplicated device '%s' font for code page %d",
	                        header.device_name.c_str(),
	                        header.code_page));
	} else {
		found.code_pages.emplace(header.device_name, header.code_page);
	}

	if (is_screen) {
		result.found_screen_fonts = true;
		if (is_ega) {
			result.found_ega_fonts.insert(header.code_page);
		} else if (is_lcd) {
			result.found_lcd_fonts.insert(header.code_page);
			if (IsFormatWinNt()) {
				Warn(header.struct_offset, header.StructName,
				     "non-EGA screen font, but the file format is 'Windows NT'");
			}
		} else {
			result.found_other_fonts.insert(header.code_page);
		}
	} else if (is_printer) {
		result.found_printer_fonts = true;
		if (!IsFormatMsDos()) {
			Warn(header.struct_offset, header.StructName,
			     "printer font, but the file format is not 'MS-DOS'");
		}
	} else {
		Warn(header.struct_offset, header.StructName,
		     "unknown device type");
	}

	if (result.found_screen_fonts && result.found_printer_fonts) {
		WarnOnce(already_warned.screen_printer_in_same_file,
		         "found both screen and printer fonts in the same file");
	}
}

// ***************************************************************************
// CPI file parser - concrete screen font extraction
// ***************************************************************************

// Screen font format normally used in MS-DOS or Windows NT files
std::optional<ScreenFont> CpiParser::GetScreenFontFormat1(const uint16_t num_fonts,
                                                          const uint16_t total_size)
{
	// Read all the font sizes we need
	ScreenFont result = {};

	const auto start_offset = GetCurrentOffset();
	for (size_t idx = 0; idx < num_fonts; ++idx) {
		const auto header = ReadScreenFontHeader();
		if (!header) {
			break;
		}

		if (!header->num_chars) {
			Warn(header->struct_offset, header->StructName,
			     "screen font size does no contain any charcters");
			continue;
		}

		bool should_skip = false;
		if (header->x_aspect != 0 || header->y_aspect != 0) {
			// All the known CPI files have offsets 0.
			// It is not known how to handle other values.
			Warn(header->struct_offset, header->StructName,
			     "screen font has a non-zero aspect value");
			should_skip = true;
		}

		// We are only interested in fonts of certain sizes
		const auto width  = header->width;
		const auto height = header->height;
		if (width != 8) {
			should_skip = true;
		}
		if (height != 16 && height != 14 && height != 8) {
			should_skip = true;
		}

		if ((height == 16 && !result.font_8x16.empty()) ||
		    (height == 14 && !result.font_8x14.empty()) ||
		    (height == 8 && !result.font_8x8.empty())) {
			Warn(header->struct_offset, header->StructName,
			     format_str("screen font size 8x%d found more than once",
			                height));
			should_skip = true;
		}

		// Other format spotted in the wild is 8x19 (in the AST Research
		// CPI files), probably for displaying a 80x25 text on a 640x480
		// resolution screen

		// Calculate font length in bytes
		const uint32_t length = header->num_chars * header->height *
		                        ((header->width + 7) / 8);

		if (should_skip) {
			SkipBytes(length);
			continue;
		}

		const auto font = ReadBlob(length);
		if (font.size() != length) {
			assert(font.empty());
			break;
		} else if (height == 16) {
			result.font_8x16.resize(length);
			std::copy_n(font.begin(), length, result.font_8x16.begin());
			result.font_8x16.resize(
			        std::min(length, ScreenFont::FullSize_8x16));
		} else if (height == 14) {
			result.font_8x14.resize(length);
			std::copy_n(font.begin(), length, result.font_8x14.begin());
			result.font_8x14.resize(
			        std::min(length, ScreenFont::FullSize_8x14));
		} else if (height == 8) {
			result.font_8x8.resize(length);
			std::copy_n(font.begin(), length, result.font_8x8.begin());
			result.font_8x8.resize(
			        std::min(length, ScreenFont::FullSize_8x8));
		}
	}

	// Check if font size was exactly as stated in the top-level structure
	const auto end_offset = GetCurrentOffset();
	if (end_offset - start_offset != total_size &&
	    !DummySizeValues.contains(total_size)) {
		Warn(start_offset,
		     format_str("expected screen font data size %d, real size %d",
		                total_size, end_offset - start_offset));

	}

	if (result.font_8x16.empty() && result.font_8x14.empty() &&
	    result.font_8x8.empty()) {
		Warn(start_offset,
		     "screen font does not contain any of the 8x16, 8x14 and 8x8 sizes");
	}

	return result;
}

// Screen font format used in DR-DOS files
std::optional<ScreenFont> CpiParser::GetScreenFontFormat2(const uint16_t num_fonts,
                                                          const uint16_t total_size)
{
	uint16_t num_chars_8x16 = 0;
	uint16_t num_chars_8x14 = 0;
	uint16_t num_chars_8x8  = 0;

	const auto start_offset = GetCurrentOffset();
	for (size_t idx = 0; idx < num_fonts; ++idx) {
		const auto header = ReadScreenFontHeader();
		if (!header) {
			return {};
		}

		if (header->x_aspect != 0 || header->y_aspect != 0) {
			// All the known CPI files have offsets 0.
			// It is not known how to handle other values.
			Warn(header->struct_offset, header->StructName,
			     "screen font has a non-zero aspect value");
			continue;
		}

		const auto width  = header->width;
		const auto height = header->height;

		if (width != 8) {
			if (width > 8) {
				Warn(header->struct_offset, header->StructName,
				     format_str("screen font width %d is invalid", width));
			}
			continue;
		}
		if (height != 16 && height != 14 && height != 8) {
			// To prevent reporting problems regarding font sizes
			// we are not interested in anyway
			continue;
		}

		const auto num_chars = std::min(header->num_chars,
		                                NumCharactersInFont);

		switch (height) {
		case 16: num_chars_8x16 = num_chars; break;
		case 14: num_chars_8x14 = num_chars; break;
		case 8: num_chars_8x8 = num_chars; break;
		}
	}

	// Check if font size was exactly as stated in the top-level structure
	const auto end_offset = GetCurrentOffset();
	if (end_offset - start_offset != total_size &&
	    !DummySizeValues.contains(total_size)) {
		Warn(start_offset,
		     format_str("expected screen font data size %d, real size %d",
		                total_size, end_offset - start_offset));
	}

	const auto num_chars_max = std::max(
	        {num_chars_8x16, num_chars_8x14, num_chars_8x8});

	const auto index_table = ReadCharacterIndexTable(num_chars_max);
	if (!index_table) {
		return {};
	}

	const auto& font_index = index_table->font_index;

	const auto& offset_8x16 = font_offsets.start_8x16;
	const auto& offset_8x14 = font_offsets.start_8x14;
	const auto& offset_8x8  = font_offsets.start_8x8;

	ScreenFont result = {};

	auto create_font = [&](const uint32_t offset,
	                       const uint16_t num_chars,
	                       const uint8_t height) -> std::vector<uint8_t> {
		if (num_chars == 0) {
			return {};
		}

		std::vector<uint8_t> font(num_chars * height);
		for (uint16_t idx = 0; idx < num_chars; ++idx) {
			SetCurrentOffset(offset + font_index[idx] * height);
			const auto character = ReadBlob(height);
			if (character.empty()) {
				return {};
			}
			std::copy_n(character.begin(),
			            height,
			            font.begin() + idx * height);
		}
		return font;
	};

	if (offset_8x16 && num_chars_8x16 != 0) {
		result.font_8x16 = create_font(*offset_8x16, num_chars_8x16, 16);
	}
	if (offset_8x14 && num_chars_8x14 != 0) {
		result.font_8x14 = create_font(*offset_8x14, num_chars_8x14, 14);
	}
	if (offset_8x8 && num_chars_8x8 != 0) {
		result.font_8x8 = create_font(*offset_8x8, num_chars_8x8, 8);
	}

	if (result.font_8x16.empty() && result.font_8x14.empty() &&
	    result.font_8x8.empty()) {
		Warn(start_offset,
		     "screen font does not contain any of the 8x16, 8x14 and 8x8 sizes");
	}

	return result;
}

// ***************************************************************************
// CPI file parser - top-level file structure parsers
// ***************************************************************************

std::optional<ScreenFont> CpiParser::GetScreenFont(const uint32_t font_offset)
{
	const auto stored_offset = GetCurrentOffset();
	SetCurrentOffset(font_offset);

	const auto header = ReadCodePageInfoHeader();
	if (!header) {
		SetCurrentOffset(stored_offset);
		return {};
	}

	// For printer fonts, the PrinterFontHeader structure starts here;
	// num_fonts should be 1 for printers, but apparently some early DR-DOS
	// printer CPI files have it set to 2 (wrongly)

	if (header->num_fonts == 0) {
		Warn(header->struct_offset, header->StructName,
		     "screen font does not define any font size");
		SetCurrentOffset(stored_offset);
		return {};
	}

	std::optional<ScreenFont> result = {};
	switch (header->version) {
	case 0:
		// It is reported that early LCD CPI files from Toshiba set this
		// to 0 (improperly) instead of 1 (which would be expected)
		if (IsFormatMsDos()) {
			result = GetScreenFontFormat1(header->num_fonts,
			                              header->size);
		} else {
			Warn(header->struct_offset, header->StructName,
		             format_str("invalid screen font format 0"));
		}
		break;
	case 1:
		result = GetScreenFontFormat1(header->num_fonts, header->size);
		break;
	case 2:
		if (IsFormatDrDos()) {
			// Format needs fonts offsets, which are only provided
			// in the DR-DOS format specific extended header
			result = GetScreenFontFormat2(header->num_fonts,
			                              header->size);
			break;
		} else {
			Warn(header->struct_offset, header->StructName,
		             "screen font format 2 is only valid in 'DR-DOS' format files");
			break;
		}
	default:
		Warn(header->struct_offset, header->StructName,
		     format_str("unknown screen font format %d", header->version));
		break;
	}

	SetCurrentOffset(stored_offset);
	return result;
}

void CpiParser::ExtractAndStoreFont(const CodePageEntryHeader& header,
                                    const uint32_t font_offset,
	                            ParserResult& result)
{
	if (!IsScreenCodePage(header)) {
		// For now only screen fonts are supported
		return;
	}

	const auto font = GetScreenFont(font_offset);
	if (!font) {
		return;
	}

	if (font->font_8x16.empty() &&
	    font->font_8x14.empty() &&
	    font->font_8x8.empty()) {
		// Screen font is useless to if it does not contain at least one
		// resolution we are interested in
		return;
	}

	const bool is_ega = IsEgaCodePage(header);
	const bool is_lcd = IsLcdCodePage(header);

	auto& result_ega   = result.extracted_ega_fonts;
	auto& result_lcd   = result.extracted_lcd_fonts;
	auto& result_other = result.extracted_other_fonts;

	if (is_ega && !result_ega.contains(header.code_page)) {
		result_ega[header.code_page] = *font;
	}
	if (is_lcd && !result_lcd.contains(header.code_page)) {
		result_lcd[header.code_page] = *font;
	}
	if (!is_ega && !is_lcd && !result_other.contains(header.code_page)) {
		result_other[header.code_page] = *font;
	}
}

uint32_t CpiParser::AdaptOffset(const uint32_t value, bool& is_adaptation_needed)
{
	// Some CPI files require offset value conversion (example: 'EGA.ICE'
	// from MS-DOS 6.0 require all the offsets converted, but it is said
	// some CPI files require offset conversion only for pointing above the
	// 64 KB). Detect when the conversion is needed.

	if (IsFormatMsDos() && GetContentSize() > UINT16_MAX &&
	    value > GetContentSize() && value != UINT32_MAX) {
		is_adaptation_needed = true;
	}

	if (is_adaptation_needed) {
		return ConvertOffset(value);
	} else {
		return value;
	}
}

bool CpiParser::ShouldSkipCodePage(const CodePageEntryHeader& header)
{
	bool should_skip = false;

	// Check if code page is valid
	if (header.code_page == 0) {
		Warn(header.struct_offset,
		     header.StructName,
		     format_str("invalid code page %d", header.code_page));
		should_skip = true;
	}

	// Check if device name is valid
	if (!IsDeviceNameValid(header)) {
		Warn(header.struct_offset, header.StructName, "invalid device name");
		should_skip = true;
	}

	return should_skip;
}

ParserResult CpiParser::GetFontsCommonPart(const uint16_t code_page_filter)
{
	// Helper code for offset format conmversion
	bool cpih_offset_adaptation_needed = false;
	bool cpeh_offset_adaptation_needed = false;

	auto adapt_cpih = [&](const uint32_t value) {
		return AdaptOffset(value, cpih_offset_adaptation_needed);
	};

	auto adapt_cpeh = [&](const uint32_t value) {
		return AdaptOffset(value, cpeh_offset_adaptation_needed);
	};

	// Read the FontInfoHeader structure
	const auto font_info_header = ReadFontInfoHeader();
	if (!font_info_header) {
		return false;
	}
	if (font_info_header->num_code_pages == 0) {
		Warn(font_info_header->struct_offset, font_info_header->StructName,
		     "no code pages in the file");
		return false;
	}

	ParserResult result = {};

	// NOTE: Some files contain a value here which is too high
	const auto num_code_pages = font_info_header->num_code_pages;

	LOG_DEBUG("LOCALE: - '%s' reports %d code pages",
	          font_info_header->StructName,
	          num_code_pages);

	// Go through all the code pages in the file
	uint16_t idx = 0;
	while (true) {
		// Some unofficial files have too high number of code pages in
		// the 'FontInfoHeader' and their last offset points to the
		// copyright message - detect this to prevent file content
		// misinterpretation.
		if (IsCopyrightString()) {
			break;
		}

		// Read the CodePageEntryHeader structure
		const auto header = ReadCodePageEntryHeader();
		if (!header) {
			break;
		}

		bool should_skip = ShouldSkipCodePage(*header);

		const auto cpih_offset      = adapt_cpih(header->cpih_offset);
		const auto next_cpeh_offset = adapt_cpeh(header->next_cpeh_offset);

		const uint32_t base_offset = IsFormatWinNt() ? header->struct_offset
		                                             : 0;
		const uint32_t font_offset = base_offset + cpih_offset;
		if (font_offset < GetCurrentOffset()) {
			// Going backwards is not allowed, skip this font
			Warn(header->struct_offset, header->StructName,
			     "font offset pointing backwards");
			should_skip = true;
		}

		if (!should_skip) {
			StoreFontType(*header, result);
			if (IsCodePageMatching(*header, code_page_filter)) {
				ExtractAndStoreFont(*header, font_offset, result);

			}
		}

		// If it was the last structure - terminate
		if (idx + 1 == num_code_pages) {
			break;
		}

		if (!IsFormatWinNt()) {
			// Some files (mostly unofficial ones, but also the
			// '4208.CPI' from MS-DOS 6.22) have too high number of
			// code pages in the 'FontInfoHeader', but they mark the
			// end of usable content with a special offset value.
			if (next_cpeh_offset == GetContentSize() ||
			    DummyOffsetValues.contains(next_cpeh_offset)) {
				break;
			}
		}

		// If not the last code page, seek to the next one
		const uint32_t next_offset = base_offset + next_cpeh_offset;
		if (next_offset < GetCurrentOffset()) {
			// Going backwards is not allowed; terminate to prevent
			// a possible infinite loop
			Warn(header->struct_offset, header->StructName,
			     "next entry offset pointing backwards");
			// At least one CPI file ('4208.CPI' from MS-DOS 6.22)
			// is known to fail this check just before the last
			// font (the offset is 0).
			// Since this is a printer font (not useful right now),
			// there was no in-depth investigation.
			break;
		}
		SetCurrentOffset(next_offset);
		++idx;
	}

	return result;
}

ParserResult CpiParser::GetFontsMsDos(const uint32_t fih_offset,
                                      const uint16_t code_page_filter)
{
	if (IsFormatMsDos()) {
		LOG_DEBUG("LOCALE: - file format is 'MS-DOS'");
	} else if (IsFormatWinNt()) {
		LOG_DEBUG("LOCALE: - file format is 'Windows NT'");
	} else {
		assert(false);
	}

	// CPI files might contain a copyright info after the header
	if (fih_offset < GetCurrentOffset()) {
		Warn(0, FontFileHeader::StructName,
		     "font info offset pointing backwards");
		return false;
	}
	SetCurrentOffset(fih_offset);

	return GetFontsCommonPart(code_page_filter);
}

ParserResult CpiParser::GetFontsDrDos(const uint32_t fih_offset,
                                      const uint16_t code_page_filter)
{
	LOG_DEBUG("LOCALE: - file format is 'DR-DOS'");

	const auto header = ReadFontFileExtendedHeader();
	if (!header) {
		return false;
	}

	// ViewMAX display drivers and DR-DOS MODE command both assume that the
	// 'FontInfoHeader' immediately follows the DR-DOS extended header; see:
	// - https://www.seasip.info/DOS/CPI/cpi.html
	if (fih_offset != GetCurrentOffset()) {
		if (fih_offset < GetCurrentOffset()) {
			Warn(0, FontFileHeader::StructName,
			     "font info offset pointing backwards");
			return false;
		}
		Warn(header->struct_offset, header->StructName,
		     format_str("did not come directly after '%s'",
		                FontFileHeader::StructName));
		SetCurrentOffset(fih_offset);
	}

	// Retrieve offsets for the font sizes we need
	for (uint16_t idx = 0; idx < header->fonts_per_codepage; ++idx) {
		auto& offset_8x16 = font_offsets.start_8x16;
		auto& offset_8x14 = font_offsets.start_8x14;
		auto& offset_8x8  = font_offsets.start_8x8;

		switch (header->cell_size[idx]) {
		case 16:
			if (offset_8x16) {
				Warn(header->struct_offset, header->StructName,
				     "multiple offsets found for screen font 8x16");
			} else {
				offset_8x16 = header->dfd_offset[idx];
			}
			break;
		case 14:
			if (offset_8x14) {
				Warn(header->struct_offset, header->StructName,
				     "multiple offsets found for screen font 8x14");
			} else {
				offset_8x14 = header->dfd_offset[idx];
			}
			break;
		case 8:
			if (offset_8x8) {
				Warn(header->struct_offset, header->StructName,
				     "multiple offsets found for screen font 8x8");
			} else {
				offset_8x8 = header->dfd_offset[idx];
			}
			break;
		}
	}

	return GetFontsCommonPart(code_page_filter);
}

// ***************************************************************************
// CPI file parser - entry point, initialization, file format detection
// ***************************************************************************

void CpiParser::InitParser()
{
	file_format = FileFormat::Unknown;

	font_offsets   = FontOffsets();
	found          = Found();
	already_warned = Warnings();
}

void CpiParser::DetectFileFormat(const uint8_t id_byte, const std::string& id_string)
{
	if (id_byte == FileId::MsDosWinNt) {
		if (id_string == Signature::MsDos) {
			file_format = FileFormat::MsDos;
			return;
		} else if (id_string == Signature::WinNt) {
			file_format = FileFormat::WinNt;
			return;
		}
	} else if (id_byte == FileId::DrDos && id_string == Signature::DrDos) {
		file_format = FileFormat::DrDos;
		return;
	}

	file_format = FileFormat::Unknown;
}

ParserResult CpiParser::GetFonts(const uint16_t code_page_filter)
{
	if (code_page_filter) {
		LOG_DEBUG("LOCALE: Looking for code page %d n %s",
		          code_page_filter, name_for_log.c_str());
	} else {
		LOG_DEBUG("LOCALE: Looking for code pages in %s",
		          name_for_log.c_str());
	}

	InitParser();

	const auto header = ReadFontFileHeader();
	if (!header) {
		return false;
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
		DetectFileFormat(header->id_byte, header->id_string);

		const auto fih_offset = header->fih_offset;
		switch (file_format) {
		case FileFormat::MsDos:
			// Having 64 KB of garbage data before the first
			// 'FontInfoHeader' is unrealistic; most likely this
			// particular CPI file requires an offset conversion.
			if (fih_offset > UINT16_MAX) {
				return GetFontsMsDos(ConvertOffset(fih_offset),
				                     code_page_filter);
			}
			return GetFontsMsDos(fih_offset, code_page_filter);
		case FileFormat::WinNt:
			return GetFontsMsDos(fih_offset, code_page_filter);
		case FileFormat::DrDos:
			return GetFontsDrDos(fih_offset, code_page_filter);
		default: break;
		}
	}

	// Not a valid/supported file format; check for FreeDOS CPX file
	ParserResult result(false);

	if (HasUpxSignature()) {
		result.unsupported_cpx_file = true;
	} else if (!is_header_valid && file_format != FileFormat::Unknown) {
		Warn(header->struct_offset, header->StructName,
	             "invalid file pointer");
	}

	return result;
}

// ***************************************************************************
// Resource management
// ***************************************************************************

struct GetCustomFontResult {
	KeyboardLayoutResult result = KeyboardLayoutResult::OK;

	ScreenFont screen_font = {};

	GetCustomFontResult(const KeyboardLayoutResult result) : result(result)
	{}
	GetCustomFontResult(const ScreenFont& screen_font)
	        : screen_font(screen_font)
	{}
};

static bool check_screen_font_complete(const ScreenFont& font,
                                       const uint16_t code_page,
                                       const std::string& name_for_log)
{
	bool status = true;
	std::string log_string = {};

	if (font.font_8x16.empty()) {
		log_string += ", no 8x16 font";
		status = false;
	} else if (font.font_8x16.size() != ScreenFont::FullSize_8x16) {
		log_string += ", incomplete 8x16 font";
		status = false;
	}

	if (font.font_8x14.empty()) {
		log_string += ", no 8x14 font";
		status = false;
	} else if (font.font_8x14.size() != ScreenFont::FullSize_8x14) {
		log_string += ", incomplete 8x14 font";
		status = false;
	}

	if (font.font_8x8.empty()) {
		log_string += ", no 8x8 font";
		status = false;
	} else if (font.font_8x8.size() != ScreenFont::FullSize_8x8) {
		log_string += ", incomplete 8x8 font";
		status = false;
	}

	if (status) {
		return true;
	}

	// TODO: This information (translated) should also be printed out in the
	// KEYB command output (we need the log in case the font is botched and
	// the rendered screen is unreadable)
	LOG_WARNING("LOCALE: Incomplete code page %d in %s: %s",
	            code_page, name_for_log.c_str(), log_string.substr(2).c_str());
	return false;
}

struct ReadDosCpiFileResult
{
	KeyboardLayoutResult result = KeyboardLayoutResult::OK;

	std::vector<uint8_t> content = {};

	ReadDosCpiFileResult(const KeyboardLayoutResult result) : result(result)
	{}
	ReadDosCpiFileResult(const std::vector<uint8_t>& content) : content(content)
	{}
};

static ReadDosCpiFileResult read_dos_cpi_file(const std::string& file_name)
{
	uint16_t handle = {};
	if (!DOS_OpenFile(file_name.c_str(), OPEN_READ, &handle)) {
		return KeyboardLayoutResult::CpiFileNotFound;
	}

	uint32_t position = {};
	if (!DOS_SeekFile(handle, &position, DOS_SEEK_END)) {
		DOS_CloseFile(handle);
		return KeyboardLayoutResult::CpiReadError;
	}
	const auto file_size = position;

	if (file_size > MaxFileSizeBytes) {
		DOS_CloseFile(handle);
		return KeyboardLayoutResult::CpiFileTooLarge;
	}

	position = 0;
	if (!DOS_SeekFile(handle, &position, DOS_SEEK_SET)) {
		DOS_CloseFile(handle);
		return KeyboardLayoutResult::CpiReadError;
	}

	std::vector<uint8_t> content(file_size);
	uint32_t bytes_already_read = 0;
	while (bytes_already_read != file_size) {
		const auto chunk_size = clamp_to_uint16(file_size - bytes_already_read);
		uint16_t bytes_read = chunk_size;
		if (!DOS_ReadFile(handle, content.data() + bytes_already_read, &bytes_read) ||
		    bytes_read != chunk_size) {
			DOS_CloseFile(handle);
			return KeyboardLayoutResult::CpiReadError;
		}
		bytes_already_read += chunk_size;
	}

	DOS_CloseFile(handle);

	return content;
}

static GetCustomFontResult get_custom_font(const uint16_t code_page,
                                           const std::string& file_name)
{
	const auto content_result = read_dos_cpi_file(file_name);
	if (content_result.result != KeyboardLayoutResult::OK) {
		return content_result.result;
	}

	const auto name_for_log = format_str("the '%s' file", file_name.c_str());
	CpiParser parser(content_result.content, name_for_log);
	const auto parser_result = parser.GetFonts(code_page);

	if (parser_result.unsupported_cpx_file) {
		return KeyboardLayoutResult::UnsupportedCpxFile;
	} else if (!parser_result.status_ok) {
		return KeyboardLayoutResult::InvalidCpiFile;
	}

	// Return the font
	const auto& extracted_ega_fonts = parser_result.extracted_ega_fonts;
	if (extracted_ega_fonts.contains(code_page)) {
		const auto& font = extracted_ega_fonts.at(code_page);
		check_screen_font_complete(font, code_page, name_for_log);
		return font;
	}
	const auto& extracted_lcd_fonts = parser_result.extracted_lcd_fonts;
	if (extracted_lcd_fonts.contains(code_page)) {
		const auto& font = extracted_lcd_fonts.at(code_page);
		check_screen_font_complete(font, code_page, name_for_log);
		return font;
	}
	const auto& extracted_other_fonts = parser_result.extracted_other_fonts;
	if (extracted_other_fonts.contains(code_page)) {
		const auto& font = extracted_other_fonts.at(code_page);
		check_screen_font_complete(font, code_page, name_for_log);
		return font;
	}

	// Let the user know if screen font was found but could not be used
	if (parser_result.found_ega_fonts.contains(code_page) ||
	    parser_result.found_lcd_fonts.contains(code_page) ||
	    parser_result.found_other_fonts.contains(code_page)) {
		return KeyboardLayoutResult::ScreenFontUnusable;
	}

	// Let the user know if he used a printer CPI file
	if (parser_result.found_printer_fonts && !parser_result.found_screen_fonts) {
		return KeyboardLayoutResult::PrinterCpiFile;
	}

	// In all the other case - tell the user the screen font was not found
	// TODO: List the screen code pages in the KEYB command output
	auto all_found = parser_result.found_other_fonts;
	std::merge(parser_result.found_ega_fonts.begin(),
	           parser_result.found_ega_fonts.end(),
	           parser_result.found_lcd_fonts.begin(),
	           parser_result.found_lcd_fonts.end(),
	           inserter(all_found, all_found.begin()));

	std::string code_pages_str = {};
	for (const auto item : all_found) {
		if (!code_pages_str.empty()) {
			code_pages_str += ", ";
		}
		code_pages_str += std::to_string(item);
	}
	LOG_WARNING("LOCALE: The file '%s' does not contain code page %d; it contains %s",
	            file_name.c_str(),
	            code_page,
	            code_pages_str.c_str());

	return KeyboardLayoutResult::NoCodePageInCpiFile;
}

static void add_ega_fonts_to_storage(const ScreenFonts& ega_fonts,
                                     const std::string& file_name,
                                     const std::string& name_for_log)
{
	for (const auto& entry : ega_fonts) {
		const auto code_page = entry.first;
		const auto& font     = entry.second;

		if (ega_font_storage.contains(code_page)) {
			continue;
		}

		// The bundled FreeDOS CPI file set contains two versions
		// of the code page 852 (and this is intentional) - this ensures
		// we always use the code page from the same file
		if (DOS_GetBundledCpiFileName(code_page) != file_name) {
			continue;
		}

		if (!check_screen_font_complete(font, code_page, name_for_log)) {
			continue;
		}

		ega_font_storage[code_page] = font;
	}
}

static void maybe_read_bundled_font(const uint16_t code_page)
{
	if (ega_font_storage.contains(code_page)) {
		return;
	}

	// Check if we have a bundled CPI file for the code page
	const auto file_name = DOS_GetBundledCpiFileName(code_page);
	if (file_name.empty()) {
		return;
	}
	const auto name_for_log = format_str("the bundled '%s' file",
	                                     file_name.c_str());

	// Do not attempt to load the same bundled CPI file twice
	static std::set<std::string> already_read = {};
	if (already_read.contains(file_name)) {
		return;
	}
	already_read.insert(file_name);

	// Open the file
	const auto resource_path  = get_resource_path(ResourceDir, file_name);
	std::ifstream file_stream = {};
	file_stream.open(resource_path, std::ios::binary);
	if (file_stream.bad()) {
		LOG_ERR("LOCALE: Could not open %s", name_for_log.c_str());
		return;
	}

	// Check file size
	file_stream.seekg(0, std::ios::end);
	const auto file_size = static_cast<size_t>(file_stream.tellg());
	file_stream.seekg(0, std::ios::beg);
	if (file_size > MaxFileSizeBytes) {
		LOG_ERR("LOCALE: Bundled '%s' file too large", file_name.c_str());
		return;
	}

	// Read the file
	std::vector<uint8_t> content(file_size);
	file_stream.read(reinterpret_cast<char*>(content.data()), file_size);
	if (file_stream.bad()) {
		LOG_ERR("LOCALE: Error reading %s", name_for_log.c_str());
		return;
	}
	file_stream.close();

	CpiParser parser(content, name_for_log);
	const auto parser_output = parser.GetFonts();

	if (!parser_output.status_ok) {
		LOG_ERR("LOCALE: Could not parse %s", name_for_log.c_str());
		return;
	}

	add_ega_fonts_to_storage(parser_output.extracted_ega_fonts,
	                         file_name,
	                         name_for_log);
}

// ***************************************************************************
// Support for font patching and code page duplicates
// ***************************************************************************

static std::optional<uint16_t> find_ega_font_storage_index(const uint16_t code_page)
{
	if (ega_font_storage.contains(code_page)) {
		return code_page;
	}

	// If we don't have the specific font in storage, search it for known
	// code page duplicates
	for (const auto& entry : ega_font_storage) {
		if (is_code_page_equal(entry.first, code_page)) {
			return entry.first;
		}
	}

	return {};
}

static void patch_font_dotted_i(ScreenFont& font)
{
	// Incomplete fonts should not be allowed in storage
	assert(font.font_8x16.size() == ScreenFont::FullSize_8x16);
	assert(font.font_8x14.size() == ScreenFont::FullSize_8x14);
	assert(font.font_8x8.size()  == ScreenFont::FullSize_8x8);

	// Codes of characters to swap
	constexpr uint8_t CodePoint1 = 0x49;
	constexpr uint8_t CodePoint2 = 0xf2;

	// Lambda to patch individual font size variant
	auto swap_code_points = [&](std::vector<uint8_t>& font_data,
	                            const uint8_t height) {
		for (auto idx = 0; idx < height; ++idx) {
			std::swap(font_data[CodePoint1 * height + idx],
			          font_data[CodePoint2 * height + idx]);
		}
	};

	swap_code_points(font.font_8x16, 16);
	swap_code_points(font.font_8x14, 14);
	swap_code_points(font.font_8x8,  8);
}

static void patch_font_low_codes(ScreenFont& font, const uint16_t code_page)
{
	// Incomplete fonts should not be allowed in storage
	assert(font.font_8x16.size() == ScreenFont::FullSize_8x16);
	assert(font.font_8x14.size() == ScreenFont::FullSize_8x14);
	assert(font.font_8x8.size()  == ScreenFont::FullSize_8x8);

	constexpr uint8_t InitialCharactersToReplace = 32;

	// Source code page for the characters to replace; this one is present
	// in the same CPI file as many code pages we need to patch
	constexpr uint16_t SourceCodePage = 1280;

	// Ensure we have the source data loaded
	maybe_read_bundled_font(SourceCodePage);
	if (!ega_font_storage.contains(SourceCodePage)) {
		LOG_ERR("LOCALE: Cound not get data to patch the code page %d screen font",
		        code_page);
		return;
	}
	const auto& source = ega_font_storage.at(SourceCodePage);

	// Incomplete fonts should not be allowed in storage
	assert(source.font_8x16.size() == ScreenFont::FullSize_8x16);
	assert(source.font_8x14.size() == ScreenFont::FullSize_8x14);
	assert(source.font_8x8.size()  == ScreenFont::FullSize_8x8);

	// Lambda to patch individual font size variant
	auto replace_characters = [&](std::vector<uint8_t>& font_data,
	                              const std::vector<uint8_t>& source_data,
	                              const uint8_t height) {
		std::copy_n(source_data.begin(),
		            InitialCharactersToReplace * height,
		            font_data.begin());
	};

	replace_characters(font.font_8x16, source.font_8x16, 16);
	replace_characters(font.font_8x14, source.font_8x14, 14);
	replace_characters(font.font_8x8,  source.font_8x8,  8);
}

const std::optional<ScreenFont> get_patched_screen_font(const uint16_t code_page)
{
	const auto storage_index = find_ega_font_storage_index(code_page);
	if (!storage_index || !ega_font_storage.contains(*storage_index)) {
		return {};
	}

	auto font = ega_font_storage.at(*storage_index);

	if (LocaleData::NeedsPatchDottedI.contains(code_page)) {
		patch_font_dotted_i(font);
	}
	if (LocaleData::NeedsPatchLowCodes.contains(code_page)) {
		patch_font_low_codes(font, code_page);
	}

	return font;
}

// ***************************************************************************
// Functions to set/reset screen font
// ***************************************************************************

static void set_screen_font(const ScreenFont& screen_font,
                            const ScreenFont& fallback_font = {})
{
	auto set_font_8x16 = [](const ScreenFont& screen_font) {
		auto font = screen_font.font_8x16;
		font.resize(ScreenFont::FullSize_8x16, 0);
		const auto memory = RealToPhysical(int10.rom.font_16);
		for (uint16_t idx = 0; idx < font.size(); ++idx) {
			phys_writeb(memory + idx, font.at(idx));
		}
	};

	auto set_font_8x14 = [](const ScreenFont& screen_font) {
		auto font = screen_font.font_8x14;
		font.resize(ScreenFont::FullSize_8x14, 0);
		const auto memory = RealToPhysical(int10.rom.font_14);
		for (uint16_t idx = 0; idx < font.size(); ++idx) {
			phys_writeb(memory + idx, font.at(idx));
		}
	};

	auto set_font_8x8 = [](const ScreenFont& screen_font) {
		auto font = screen_font.font_8x8;
		font.resize(ScreenFont::FullSize_8x8, 0);
		const auto middle   = static_cast<uint16_t>(font.size() / 2);
		const auto memory_1 = RealToPhysical(int10.rom.font_8_first);
		const auto memory_2 = RealToPhysical(int10.rom.font_8_second);
		for (uint16_t idx = 0; idx < middle; ++idx) {
			phys_writeb(memory_1 + idx, font.at(idx));
		}
		for (uint16_t idx = middle; idx < font.size(); ++idx) {
			phys_writeb(memory_2 - middle + idx, font.at(idx));
		}
	};

	// Start from setting back the default font
	INT10_ReloadRomFonts();

	// Set the 8x16 font
	if (!screen_font.font_8x16.empty()) {
		set_font_8x16(screen_font);
	} else if (!fallback_font.font_8x16.empty()) {
		set_font_8x16(fallback_font);
	}
	// Clear pointer to the alternate font to prevent swiching
	phys_writeb(RealToPhysical(int10.rom.font_16_alternate), 0);

	// Set the 8x14 font
	if (!screen_font.font_8x14.empty()) {
		set_font_8x14(screen_font);
	} else if (!fallback_font.font_8x14.empty()) {
		set_font_8x14(fallback_font);
	}
	// Clear pointer to the alternate font to prevent swiching
	phys_writeb(RealToPhysical(int10.rom.font_14_alternate), 0);

	// Set the 8x8 font
	if (!screen_font.font_8x8.empty()) {
		set_font_8x8(screen_font);
	} else if (!fallback_font.font_8x8.empty()) {
		set_font_8x8(fallback_font);
	}
	// There is no alternate variant for the 8x8 font

	if (CurMode->type == M_TEXT) {
		INT10_ReloadFont();
	}
	INT10_SetupRomMemoryChecksum();
}

static KeyboardLayoutResult load_custom_screen_font(const uint16_t code_page,
                                                    const std::string& file_name)
{
	const auto canonical_file_name = DOS_Canonicalize(file_name.c_str());
	if (canonical_file_name.empty()) {
		return KeyboardLayoutResult::CpiFileNotFound;
	}

	const auto result = get_custom_font(code_page, canonical_file_name);
	if (result.result != KeyboardLayoutResult::OK) {
		return result.result;
	}

	maybe_read_bundled_font(code_page);
	const auto fallback_font = get_patched_screen_font(code_page);
	if (fallback_font) {
		// Use the bundled font as a fallback - in case the one from the
		// user's CPI file does not provide all the resolutions we need
		set_screen_font(result.screen_font, *fallback_font);
	} else {
		set_screen_font(result.screen_font);
	}
	dos.loaded_codepage       = code_page;
	dos.screen_font_type      = ScreenFontType::Custom;
	dos.screen_font_file_name = canonical_file_name;

	LOG_MSG("LOCALE: Loaded code page %d from '%s' file",
	        code_page,
	        canonical_file_name.c_str());

	notify_code_page_changed();
	return KeyboardLayoutResult::OK;
}

static KeyboardLayoutResult load_bundled_screen_font(const uint16_t code_page)
{
	if (dos.loaded_codepage == code_page &&
	    dos.screen_font_type == ScreenFontType::Bundled) {
		// Already loaded - skip loading and notifying other subsystems
		return KeyboardLayoutResult::OK;
	}

	maybe_read_bundled_font(code_page);
	const auto patched_font = get_patched_screen_font(code_page);
	if (!patched_font) {
		return KeyboardLayoutResult::NoBundledCpiFileForCodePage;
	}

	set_screen_font(*patched_font);
	dos.loaded_codepage       = code_page;
	dos.screen_font_type      = ScreenFontType::Bundled;
	dos.screen_font_file_name = {};

	LOG_MSG("LOCALE: Loaded code page %d - '%s'",
	        code_page,
	        DOS_GetEnglishCodePageDescription(code_page).c_str());

	notify_code_page_changed();
	return KeyboardLayoutResult::OK;
}

static void load_default_screen_font()
{
	if (dos.loaded_codepage  == DefaultCodePage &&
	    dos.screen_font_type == ScreenFontType::Rom) {
		// Already loaded - skip loading and notifying other subsystems
		return;
	}

	INT10_ReloadRomFonts();
	if (CurMode->type == M_TEXT) {
		INT10_ReloadFont();
	}

	dos.loaded_codepage       = DefaultCodePage;
	dos.screen_font_type      = ScreenFontType::Rom;
	dos.screen_font_file_name = {};

	LOG_MSG("LOCALE: Loaded code page %d (ROM font)", dos.loaded_codepage);

	notify_code_page_changed();
}

// ***************************************************************************
// External interface
// ***************************************************************************

bool DOS_CanLoadScreenFonts()
{
	return is_machine_ega_or_better();
}

KeyboardLayoutResult DOS_LoadScreenFont(const uint16_t code_page,
                                        const std::string& file_name)
{
	if (!DOS_CanLoadScreenFonts()) {
		return KeyboardLayoutResult::IncompatibleMachine;
	}

	if (code_page == 0) {
		assert(false);
		return KeyboardLayoutResult::NoLayoutForCodePage;
	}

	// Load screen font from the custom CPI file

	if (!file_name.empty()) {
		return load_custom_screen_font(code_page, file_name);
	} else {
		return load_bundled_screen_font(code_page);
	}
}

void DOS_SetRomScreenFont()
{
	load_default_screen_font();
}
