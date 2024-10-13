/*
 *  Copyright (C) 2022-2024  The DOSBox Staging Team
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

#include "dosbox.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "checks.h"
#include "dos_inc.h"
#include "unicode.h"

CHECK_NARROWING();

// ***************************************************************************
// Hardcoded data
// ***************************************************************************

// Note: Most of the Unicode engine data is stored in external files, loaded and
// parsed during runtime

using box_drawing_set_t = std::array<uint16_t, 40>;

// List of box drawing characters, ordered as in code page 437
static constexpr box_drawing_set_t BoxDrawingSetRegular = {
        0x2502, // 0xb3 - '│', BOX DRAWINGS LIGHT VERTICAL
        0x2524, // 0xb4 - '┤', BOX DRAWINGS LIGHT VERTICAL AND LEFT
        0x2561, // 0xb5 - '╡', BOX DRAWINGS VERTICAL SINGLE AND LEFT DOUBLE
        0x2562, // 0xb6 - '╢', BOX DRAWINGS VERTICAL DOUBLE AND LEFT SINGLE
        0x2556, // 0xb7 - '╖', BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE
        0x2555, // 0xb8 - '╕', BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE
        0x2563, // 0xb9 - '╣', BOX DRAWINGS DOUBLE VERTICAL AND LEFT
        0x2551, // 0xba - '║', BOX DRAWINGS DOUBLE VERTICAL
        0x2557, // 0xbb - '╗', BOX DRAWINGS DOUBLE DOWN AND LEFT
        0x255d, // 0xbc - '╝', BOX DRAWINGS DOUBLE UP AND LEFT
        0x255c, // 0xbd - '╜', BOX DRAWINGS UP DOUBLE AND LEFT SINGLE
        0x255b, // 0xbe - '╛', BOX DRAWINGS UP SINGLE AND LEFT DOUBLE
        0x2510, // 0xbf - '┐', BOX DRAWINGS LIGHT DOWN AND LEFT
        0x2514, // 0xc0 - '└', BOX DRAWINGS LIGHT UP AND RIGHT
        0x2534, // 0xc1 - '┴', BOX DRAWINGS LIGHT UP AND HORIZONTAL
        0x252c, // 0xc2 - '┬', BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
        0x251c, // 0xc3 - '├', BOX DRAWINGS LIGHT VERTICAL AND RIGHT
        0x2500, // 0xc4 - '─', BOX DRAWINGS LIGHT HORIZONTAL
        0x253c, // 0xc5 - '┼', BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
        0x255e, // 0xc6 - '╞', BOX DRAWINGS VERTICAL SINGLE AND RIGHT DOUBLE
        0x255f, // 0xc7 - '╟', BOX DRAWINGS VERTICAL DOUBLE AND RIGHT SINGLE
        0x255a, // 0xc8 - '╚', BOX DRAWINGS DOUBLE UP AND RIGHT
        0x2554, // 0xc9 - '╔', BOX DRAWINGS DOUBLE DOWN AND RIGHT
        0x2569, // 0xca - '╩', BOX DRAWINGS DOUBLE UP AND HORIZONTAL
        0x2566, // 0xcb - '╦', BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
        0x2560, // 0xcc - '╠', BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
        0x2550, // 0xcd - '═', BOX DRAWINGS DOUBLE HORIZONTAL
        0x256c, // 0xce - '╬', BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
        0x2567, // 0xcf - '╧', BOX DRAWINGS UP SINGLE AND HORIZONTAL DOUBLE
        0x2568, // 0xd0 - '╨', BOX DRAWINGS UP DOUBLE AND HORIZONTAL SINGLE
        0x2564, // 0xd1 - '╤', BOX DRAWINGS DOWN SINGLE AND HORIZONTAL DOUBLE
        0x2565, // 0xd2 - '╥', BOX DRAWINGS DOWN DOUBLE AND HORIZONTAL SINGLE
        0x2559, // 0xd3 - '╙', BOX DRAWINGS UP DOUBLE AND RIGHT SINGLE
        0x2558, // 0xd4 - '╘', BOX DRAWINGS UP SINGLE AND RIGHT DOUBLE
        0x2552, // 0xd5 - '╒', BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE
        0x2553, // 0xd6 - '╓', BOX DRAWINGS DOWN DOUBLE AND RIGHT SINGLE
        0x256b, // 0xd7 - '╫', BOX DRAWINGS VERTICAL DOUBLE AND HORIZONTAL SINGLE
        0x256a, // 0xd8 - '╪', BOX DRAWINGS VERTICAL SINGLE AND HORIZONTAL DOUBLE
        0x2518, // 0xd9 - '┘', BOX DRAWINGS LIGHT UP AND LEFT
        0x250c, // 0xda - '┌', BOX DRAWINGS LIGHT DOWN AND RIGHT
};

// Fallback list of box drawing characters, ordered as above;
// effectively turns all double lines into light lines
static constexpr box_drawing_set_t BoxDrawingSetLight = {
        0x2502, // 0xb3 - '│'
        0x2524, // 0xb4 - '┤'
        0x2524, // 0xb5 - '┤' (instead of '╡')
        0x2524, // 0xb6 - '┤' (instead of '╢')
        0x2510, // 0xb7 - '┐' (instead of '╖')
        0x2510, // 0xb8 - '┐' (instead of '╕')
        0x2524, // 0xb9 - '┤' (instead of '╣')
        0x2502, // 0xba - '│' (instead of '║')
        0x2510, // 0xbb - '┐' (instead of '╗')
        0x2518, // 0xbc - '┘' (instead of '╝')
        0x2518, // 0xbd - '┘' (instead of '╜')
        0x2518, // 0xbe - '┘' (instead of '╛')
        0x2510, // 0xbf - '┐'
        0x2514, // 0xc0 - '└'
        0x2534, // 0xc1 - '┴'
        0x252c, // 0xc2 - '┬'
        0x251c, // 0xc3 - '├'
        0x2500, // 0xc4 - '─'
        0x253c, // 0xc5 - '┼'
        0x251c, // 0xc6 - '├' (instead of '╞')
        0x251c, // 0xc7 - '├' (instead of '╟')
        0x2514, // 0xc8 - '└' (instead of '╚')
        0x250c, // 0xc9 - '┌' (instead of '╔')
        0x2534, // 0xca - '┴' (instead of '╩')
        0x252c, // 0xcb - '┬' (instead of '╦')
        0x251c, // 0xcc - '├' (instead of '╠')
        0x2500, // 0xcd - '─' (instead of '═')
        0x253c, // 0xce - '┼' (instead of '╬')
        0x2534, // 0xcf - '┴' (instead of '╧')
        0x2534, // 0xd0 - '┴' (instead of '╨')
        0x252c, // 0xd1 - '┬' (instead of '╤')
        0x252c, // 0xd2 - '┬' (instead of '╥')
        0x2514, // 0xd3 - '└' (instead of '╙')
        0x2514, // 0xd4 - '└' (instead of '╘')
        0x250c, // 0xd5 - '┌' (instead of '╒')
        0x250c, // 0xd6 - '┌' (instead of '╓')
        0x253c, // 0xd7 - '┼' (instead of '╫')
        0x253c, // 0xd8 - '┼' (instead of '╪')
        0x2518, // 0xd9 - '┘'
        0x250c, // 0xda - '┌'
};

// Additional box drawing fallback data - each group of aliases is applied as
// a whole, even if the code page actually have some characters

using alias_t                         = std::pair<uint16_t, uint16_t>;
using alias_groups_t                  = std::vector<std::vector<alias_t>>;

const alias_groups_t box_alias_groups = {
	// Note: If you compare horizontal and vertical, the groups might seem
	// 'asymetric' to you - this is not a mistake! This is due to the fact
	// that characters are not square, and some replacements are more
	// visible than others.
        {
		{0x252c, 0x2500}, // change '┬' to '─'
		{0x2534, 0x2500}, // change '┴' to '─'
		{0x2564, 0x2550}, // change '╤' to '═'
		{0x256a, 0x2550}, // change '╪' to '═'
		{0x2567, 0x2550}, // change '╧' to '═'
	},
	{
		{0x2565, 0x2500}, // change '╥' to '─'
		{0x256b, 0x2551}, // change '╫' to '║'
		{0x2568, 0x2500}, // change '╨' to '─'
	},
	{
		{0x2566, 0x2550}, // change '╦' to '═'
		{0x2569, 0x2550}, // change '╩' to '═'
	},
	{
		{0x2524, 0x2502}, // change '┤' to '│'
		{0x251c, 0x2502}, // change '├' to '│'
	},
	{
		{0x2561, 0x2502}, // change '╡' to '│'
		{0x255e, 0x2502}, // change '╞' to '│'
	},
	{
		{0x2562, 0x2551}, // change '╢' to '║'
		{0x255f, 0x2551}, // change '╟' to '║'
	},
	{
		{0x2563, 0x2551}, // change '╣' to '║'
		{0x2560, 0x2551}, // change '╠' to '║'
	},
	{
		{0x2556, 0x2557}, // change '╖' to '╗'
		{0x2555, 0x2557}, // change '╕' to '╗'
		{0x255c, 0x255d}, // change '╜' to '╝'
		{0x255b, 0x255d}, // change '╛' to '╝'
		{0x2559, 0x255a}, // change '╙' to '╚'
		{0x2558, 0x255a}, // change '╘' to '╚'
		{0x2552, 0x2554}, // change '╒' to '╔'
		{0x2553, 0x2554}, // change '╓' to '╔'
	},
	{
		{0x253c, 0x2502}, // change '┼' to '│'
	},
	{
		{0x256c, 0x2551}, // change '╬' to '║'
	},
};

// ***************************************************************************
// Unicode engine
// ***************************************************************************

using wide_string = std::vector<uint16_t>;

// Class representing a grapheme - one main code point + optionally a number
// os code points represening combining marks
class Grapheme final {
public:
	Grapheme() = default;
	Grapheme(const uint16_t code_point);

	[[nodiscard]] bool IsEmpty() const;
	[[nodiscard]] bool IsValid() const;
	[[nodiscard]] bool HasMark() const;
	[[nodiscard]] uint16_t GetCodePoint() const;
	void PushInto(wide_string& str_out) const;

	void Invalidate();
	void AddMark(const uint16_t code_point);
	void CopyMarksFrom(const Grapheme& other);
	void StripMarks();
	void Decompose();

	bool operator==(const Grapheme& other) const;
	bool operator<(const Grapheme& other) const;

private:
	// Unicode code point
	uint16_t code_point = static_cast<uint16_t>(' ');
	// Combining marks
	wide_string marks        = {};
	wide_string marks_sorted = {};

	bool is_empty = true;
	bool is_valid = true;
};

// Unicode to DOS code page mapping
using map_grapheme_to_dos_t = std::map<Grapheme, uint8_t>;
// DOS code page to Unicode mapping
using map_dos_to_grapheme_t = std::map<uint8_t, Grapheme>;

// Mapping between Unicode box character code point and fallback code point
// supported by given code page
using map_box_code_points_t = std::map<uint16_t, uint16_t>;

// Mapping between lowercase and uppercase code points
using map_code_point_case_t    = std::map<uint16_t, uint16_t>;
using map_dos_character_case_t = std::vector<uint8_t>;

// Rules how to decompose code points to split-out all the combining marks
using decomposition_rules_t = std::map<uint16_t, Grapheme>;

using config_duplicates_t = std::map<uint16_t, uint16_t>;
using config_aliases_t    = std::vector<alias_t>;

struct config_mapping_entry_t {
	bool valid                    = false;
	map_dos_to_grapheme_t mapping = {};
	uint16_t extends_code_page    = 0;
	std::string extends_dir       = "";
	std::string extends_file      = "";
};

using config_mappings_t = std::map<uint16_t, config_mapping_entry_t>;

static const std::string file_name_main          = "MAIN.TXT";
static const std::string file_name_ascii         = "ASCII.TXT";
static const std::string file_name_case          = "CAPITAL_SMALL.TXT";
static const std::string file_name_decomposition = "DECOMPOSITION.TXT";
static const std::string dir_name_mapping        = "mapping";

// Thresholds for UTF-8 decoding/encoding
constexpr uint8_t DecodeThresholdNonAscii = 0b1'000'0000;
constexpr uint8_t DecodeThreshold2Bytes   = 0b1'100'0000;
constexpr uint8_t DecodeThreshold3Bytes   = 0b1'110'0000;
constexpr uint8_t DecodeThreshold4Bytes   = 0b1'111'0000;
constexpr uint8_t DecodeThreshold5Bytes   = 0b1'111'1000;
constexpr uint8_t DecodeThreshold6Bytes   = 0b1'111'1100;
constexpr uint16_t EncodeThreshold2Bytes  = 0x0080;
constexpr uint16_t EncodeThreshold3Bytes  = 0x0800;

// Use the character below if there is no sane way to handle the Unicode glyph
constexpr uint8_t UnknownCharacter = 0x3f; // '?'

// End of file marking, use in some files from unicode.org
constexpr uint8_t EndOfFileMarking = 0x1a;

// Main information about how to create Unicode mappings for given DOS code page
static config_mappings_t config_mappings = {};

// Unicode -> Unicode fallback mapping (alias),
// use before fallback to 7-bit ASCII
static config_aliases_t config_aliases = {};
// Information about code pages which are exact duplicates
static config_duplicates_t config_duplicates = {};

// Unicode -> 7-bit ASCII mapping, use as a last resort mapping
static map_grapheme_to_dos_t mapping_ascii = {};

// Mappings between lowercase and uppercase characters
static map_code_point_case_t uppercase = {};
static map_code_point_case_t lowercase = {};

// Unicode 'KD' decomposition rules
static decomposition_rules_t decomposition_rules = {};

// Set of per code page mappings

struct code_page_maps_t {
	// DOS character to Unicode grapheme (normalized/decomposed) maps
	map_grapheme_to_dos_t dos_to_grapheme_normalized = {};
	map_grapheme_to_dos_t dos_to_grapheme_decomposed = {};
	// Additional fallback mappings, for handling certain graphemes
	// not existing in current code page
	map_grapheme_to_dos_t aliases_normalized = {};
	map_grapheme_to_dos_t aliases_decomposed = {};
	// Reverse mapping, Unicode grapheme to DOS character
	map_dos_to_grapheme_t grapheme_to_dos = {};
	// Mapping for box-optimized fallback mode
	map_box_code_points_t box_code_points = {};
	// Mapping to change DOS character casing
	map_dos_character_case_t uppercase = {};
	map_dos_character_case_t lowercase = {};
};

static std::map<uint16_t, code_page_maps_t> per_code_page_mappings = {};

// ***************************************************************************
// Grapheme type implementation
// ***************************************************************************

static bool is_combining_mark(const uint32_t code_point)
{
	static constexpr std::pair<uint16_t, uint16_t> Ranges[] = {
		// clang-format off
		{0x0300, 0x036f}, // Combining Diacritical Marks
		{0x0653, 0x065f}, // Arabic Combining Marks
		// Note: Arabic Combining Marks start from 0x064b, but some are
		// present as standalone characters in arabic code pages. To
		// allow this, we do not recognize them as combining marks!
		// Similarly for Spacing Modifier Letters.
		{0x02b9, 0x02bf}, // Spacing Modifier Letters
		{0x1ab0, 0x1aff}, // Combining Diacritical Marks Extended
		{0x1dc0, 0x1dff}, // Combining Diacritical Marks Supplement
		{0x20d0, 0x20ff}, // Combining Diacritical Marks for Symbols
		{0xfe20, 0xfe2f}, // Combining Half Marks
		// clang-format on
	};

	auto in_range = [code_point](const auto& range) {
		return code_point >= range.first && code_point <= range.second;
	};
	return std::any_of(std::begin(Ranges), std::end(Ranges), in_range);
}

Grapheme::Grapheme(const uint16_t code_point)
        : code_point(code_point),
          is_empty(false)
{
	// It is not valid to have a combining mark
	// as a main code point of the grapheme

	if (is_combining_mark(code_point)) {
		Invalidate();
	}
}

bool Grapheme::IsEmpty() const
{
	return is_empty;
}

bool Grapheme::IsValid() const
{
	return is_valid;
}

bool Grapheme::HasMark() const
{
	return !marks.empty();
}

uint16_t Grapheme::GetCodePoint() const
{
	return code_point;
}

void Grapheme::PushInto(wide_string& str_out) const
{
	if (is_empty || !is_valid) {
		return;
	}

	str_out.push_back(code_point);
	for (const auto mark : marks) {
		str_out.push_back(mark);
	}
}

void Grapheme::Invalidate()
{
	is_empty = false;
	is_valid = false;

	code_point = UnknownCharacter;
	marks.clear();
	marks_sorted.clear();
}

void Grapheme::AddMark(const uint16_t in_code_point)
{
	if (!is_valid) {
		// Can't add combining mark to invalid grapheme
		return;
	}

	if (!is_combining_mark(in_code_point) || is_empty) {
		// Not a combining mark or empty grapheme
		Invalidate();
		return;
	}

	if (std::find(marks.cbegin(), marks.cend(), in_code_point) != marks.cend()) {
		// Combining mark already present
		return;
	}

	marks.push_back(in_code_point);
	marks_sorted.push_back(in_code_point);
	std::sort(marks_sorted.begin(), marks_sorted.end());
}

void Grapheme::CopyMarksFrom(const Grapheme& other)
{
	// Can't add combining mark to/from invalid grapheme
	if (is_valid && other.is_valid) {
		marks        = other.marks;
		marks_sorted = other.marks_sorted;
	}
}

void Grapheme::StripMarks()
{
	marks.clear();
	marks_sorted.clear();
}

void Grapheme::Decompose()
{
	if (!is_valid || is_empty) {
		// Can't decompose invalid or empty grapheme
		return;
	}

	while (decomposition_rules.count(code_point) > 0) {
		const auto& rule = decomposition_rules.at(code_point);
		code_point       = rule.code_point;
		for (const auto mark : rule.marks) {
			AddMark(mark);
		}
	}
}

bool Grapheme::operator==(const Grapheme& other) const
{
	return (is_empty == other.is_empty) && (is_valid == other.is_valid) &&
	       (code_point == other.code_point) &&
	       (marks_sorted == other.marks_sorted);
}

bool Grapheme::operator<(const Grapheme& other) const
{
	if (code_point < other.code_point) {
		return true;
	}
	if (code_point > other.code_point) {
		return false;
	}

	if (marks_sorted.empty() && other.marks_sorted.empty()) {
		return false;
	}

	if (marks_sorted.size() < other.marks_sorted.size()) {
		return true;
	}
	if (marks_sorted.size() > other.marks_sorted.size()) {
		return false;
	}

	for (size_t idx = 0; idx < marks_sorted.size(); ++idx) {
		if (marks_sorted[idx] < other.marks_sorted[idx]) {
			return true;
		}
		if (marks_sorted[idx] > other.marks_sorted[idx]) {
			return false;
		}
	}

	assert(is_empty == other.is_empty);
	assert(is_valid == other.is_valid);

	return false;
}

// ***************************************************************************
// Helpers for control codes handing
// ***************************************************************************

// Constants to detect whether DOS character is a control code
constexpr uint8_t ControlCodeDelete    = 0x7f;
constexpr uint8_t ControlCodeThreshold = 0x20;

// Unicode code points for screen codes from 0x00 to 0x1f
// see: https://en.wikipedia.org/wiki/Code_page_437
constexpr std::array<uint16_t, 0x20> ScreenCodesWide = {
	// clang-format off
	0x0020, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, // 00-07
	0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c, // 08-13
	0x25ba, 0x25c4, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8, // 10-17
	0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc  // 18-1f
	// clang-format on
};

constexpr uint16_t ScreenCodeWide7f = 0x2302; // 'del' character

static bool is_control_code(const uint16_t value)
{
	return (value < ControlCodeThreshold) || (value == ControlCodeDelete);
}

static std::optional<uint16_t> screen_code_to_wide(const uint8_t byte,
                                                   const DosStringConvertMode convert_mode)
{
	if (convert_mode == DosStringConvertMode::WithControlCodes ||
	    convert_mode == DosStringConvertMode::NoSpecialCharacters) {
		return {};
	}

	assert(convert_mode == DosStringConvertMode::ScreenCodesOnly);

	if (byte == ControlCodeDelete) {
		return ScreenCodeWide7f;
	} else if (byte < ControlCodeThreshold) {
		return ScreenCodesWide[byte];
	}

	return {};
}

static std::optional<uint8_t> grapheme_to_control_code(const Grapheme& grapheme,
                                                       const DosStringConvertMode convert_mode)
{
	if (convert_mode == DosStringConvertMode::ScreenCodesOnly ||
	    convert_mode == DosStringConvertMode::NoSpecialCharacters) {
		return {};
	}

	assert(convert_mode == DosStringConvertMode::WithControlCodes);

	if (grapheme.HasMark()) {
		return {};
	}

	const auto code_point = grapheme.GetCodePoint();
	if (!is_control_code(code_point)) {
		return {};
	}

	return check_cast<uint8_t>(code_point);
}

static std::optional<uint8_t> grapheme_to_screen_code(const Grapheme& grapheme,
                                                      const DosStringConvertMode convert_mode)
{
	if (convert_mode == DosStringConvertMode::WithControlCodes ||
	    convert_mode == DosStringConvertMode::NoSpecialCharacters) {
		return {};
	}

	assert(convert_mode == DosStringConvertMode::ScreenCodesOnly);

	if (grapheme.HasMark()) {
		return {};
	}

	constexpr auto Begin = ScreenCodesWide.begin();
	constexpr auto End   = ScreenCodesWide.end();

	const auto iter = std::find(Begin, End, grapheme.GetCodePoint());
	if (iter == End) {
		return {};
	}

	return check_cast<uint8_t>(std::distance(Begin, iter));
}

// ***************************************************************************
// Conversion routines
// ***************************************************************************

static wide_string utf8_to_wide(const std::string& str)
{
	// Convert UTF-8 string to a sequence of decoded integers

	// For UTF-8 encoding explanation see here:
	// -
	// https://www.codeproject.com/Articles/38242/Reading-UTF-8-with-C-streams
	// - https://en.wikipedia.org/wiki/UTF-8#Encoding

	bool already_warned = false;
	auto warn_decode_problem = [&already_warned](const size_t position) {
		if (already_warned) {
			return;

		}
		LOG_WARNING("UNICODE: Problem decoding UTF8 string, position %u",
		            static_cast<unsigned int>(position));
		already_warned = true;
	};

	wide_string str_out = {};
	str_out.reserve(str.size());

	for (size_t i = 0; i < str.size(); ++i) {
		const size_t remaining = str.size() - i - 1;
		const uint8_t byte_1   = static_cast<uint8_t>(str[i]);
		const uint8_t byte_2   = (remaining >= 1)
		                               ? static_cast<uint8_t>(str[i + 1]) : 0;
		const uint8_t byte_3   = (remaining >= 2)
		                               ? static_cast<uint8_t>(str[i + 2]) : 0;

		auto advance = [&](const size_t bytes) {
			auto counter = std::min(remaining, bytes);
			while (counter--) {
				const auto byte_next = static_cast<uint8_t>(str[i + 1]);
				if (byte_next < DecodeThresholdNonAscii ||
				    byte_next >= DecodeThreshold2Bytes) {
					break;
				}
				++i;
			}

			// advance without decoding
			warn_decode_problem(i);
		};

		// Retrieve code point
		uint32_t code_point = UnknownCharacter;

		// Support code point needing up to 3 bytes to encode; this
		// includes Latin, Greek, Cyrillic, Hebrew, Arabic, VGA charset
		// symbols, etc. More bytes are needed mainly for historic
		// scripts, emoji, etc.

		if (byte_1 >= DecodeThreshold6Bytes) {
			// 6-byte code point (>= 31 bits), no support
			advance(5);
		} else if (byte_1 >= DecodeThreshold5Bytes) {
			// 5-byte code point (>= 26 bits), no support
			advance(4);
		} else if (byte_1 >= DecodeThreshold4Bytes) {
			// 4-byte code point (>= 21 bits), no support
			advance(3);
		} else if (byte_1 >= DecodeThreshold3Bytes) {
			// 3-byte code point - decode 1st byte
			code_point = static_cast<uint8_t>(byte_1 -
			                                  DecodeThreshold3Bytes);
			// Decode 2nd byte
			code_point = code_point << 6;
			if (byte_2 >= DecodeThresholdNonAscii &&
			    byte_2 < DecodeThreshold2Bytes) {
				++i;
				code_point = code_point + byte_2 -
				             DecodeThresholdNonAscii;
			} else {
				// code point encoding too short
				warn_decode_problem(i);
			}
			// Decode 3rd byte
			code_point = code_point << 6;
			if (byte_2 >= DecodeThresholdNonAscii &&
			    byte_2 < DecodeThreshold2Bytes &&
			    byte_3 >= DecodeThresholdNonAscii &&
			    byte_3 < DecodeThreshold2Bytes) {
				++i;
				code_point = code_point + byte_3 -
				             DecodeThresholdNonAscii;
			} else {
				// code point encoding too short
				warn_decode_problem(i);
			}
		} else if (byte_1 >= DecodeThreshold2Bytes) {
			// 2-byte code point - decode 1st byte
			code_point = static_cast<uint8_t>(byte_1 -
			                                  DecodeThreshold2Bytes);
			// Decode 2nd byte
			code_point = code_point << 6;
			if (byte_2 >= DecodeThresholdNonAscii &&
			    byte_2 < DecodeThreshold2Bytes) {
				++i;
				code_point = code_point + byte_2 -
				             DecodeThresholdNonAscii;
			} else {
				// code point encoding too short
				warn_decode_problem(i);
			}
		} else if (byte_1 < DecodeThresholdNonAscii) {
			// 1-byte code point, ASCII compatible
			code_point = byte_1;
		} else {
			warn_decode_problem(i); // not UTF8 encoding
		}

		str_out.push_back(static_cast<uint16_t>(code_point));
	}

	return str_out;
}

static std::string wide_to_utf8(const wide_string& str)
{
	std::string str_out = {};
	str_out.reserve(str.size() * 2);

	auto push = [&](const int value) {
		const auto byte = static_cast<uint8_t>(value);
		str_out.push_back(static_cast<char>(byte));
	};

	for (const auto code_point : str) {
		if (code_point < EncodeThreshold2Bytes) {
			// Encode using 1 byte
			push(code_point);
		} else if (code_point < EncodeThreshold3Bytes) {
			// Encode using 2 bytes
			const auto to_byte_1 = code_point >> 6;
			const auto to_byte_2 = 0b0'011'1111 & code_point;
			push(to_byte_1 | 0b1'100'0000);
			push(to_byte_2 | 0b1'000'0000);
		} else {
			// Encode using 3 bytes
			const auto to_byte_1 = code_point >> 12;
			const auto to_byte_2 = 0b0'011'1111 & (code_point >> 6);
			const auto to_byte_3 = 0b0'011'1111 & code_point;
			push(to_byte_1 | 0b1'110'0000);
			push(to_byte_2 | 0b1'000'0000);
			push(to_byte_3 | 0b1'000'0000);
		}
	}

	str_out.shrink_to_fit();
	return str_out;
}

static void warn_code_point(const uint16_t code_point)
{
	static std::set<uint16_t> already_warned;
	if (already_warned.count(code_point) > 0) {
		return;
	}
	already_warned.insert(code_point);
	LOG_WARNING("UNICODE: No fallback mapping for code point 0x%04x", code_point);
}

static void warn_code_page(const uint16_t code_page)
{
	static std::set<uint16_t> already_warned;
	if (already_warned.count(code_page) > 0) {
		return;
	}
	already_warned.insert(code_page);
	LOG_WARNING("UNICODE: Requested unknown code page %d", code_page);
}

static std::string wide_to_dos(const wide_string& str,
                               const DosStringConvertMode convert_mode,
                               const UnicodeFallback fallback,
                               const uint16_t code_page)
{
	std::string str_out = {};
	str_out.reserve(str.size());

	const map_grapheme_to_dos_t* mapping_normalized = nullptr;
	const map_grapheme_to_dos_t* mapping_decomposed = nullptr;
	const map_grapheme_to_dos_t* aliases_normalized = nullptr;
	const map_grapheme_to_dos_t* aliases_decomposed = nullptr;

	const map_box_code_points_t* box_code_points = nullptr;

	// Try to find UTF8 -> code page mapping
	if (code_page != 0) {
		const auto it = per_code_page_mappings.find(code_page);

		if (it != per_code_page_mappings.end()) {
			const auto& mappings = it->second;
			mapping_normalized = &mappings.dos_to_grapheme_normalized;
			mapping_decomposed = &mappings.dos_to_grapheme_decomposed;
			aliases_normalized = &mappings.aliases_normalized;
			aliases_decomposed = &mappings.aliases_decomposed;
			box_code_points    = &mappings.box_code_points;
		} else {
			warn_code_page(code_page);
		}
	}

	// Handle code points which are 7-bit ASCII characters
	auto push_7bit = [&str_out, &convert_mode](const Grapheme& grapheme) {
		if (grapheme.HasMark()) {
			return false; // not a 7-bit ASCII character
		}

		const auto code_point = grapheme.GetCodePoint();
		if (code_point >= DecodeThresholdNonAscii) {
			return false; // not a 7-bit ASCII character
		}

		if (!is_control_code(code_point)) {
			str_out.push_back(static_cast<char>(code_point));
			return true;
		}

		const auto control_code = grapheme_to_control_code(grapheme,
		                                                   convert_mode);
		if (control_code) {
			str_out.push_back(static_cast<char>(*control_code));
			return true;
		}

		return false;
	};

	// Handle box drawing characters, if needed use specialized fallback
	// strategy to guarantee table consistency
	auto push_box_drawing = [&str_out](const map_grapheme_to_dos_t* mapping,
	                                   const map_box_code_points_t* mapping_box,
	                                   const Grapheme& grapheme) {
		if (!mapping || !mapping_box) {
			return false;
		}

		if (grapheme.HasMark()) {
			return false; // not a box drawing character
		}

		const auto code_point = grapheme.GetCodePoint();
		if (mapping_box->count(code_point) == 0) {
			return false; // not a box drawing character
		}

		const auto alias_code_point = mapping_box->at(code_point);
		if (alias_code_point >= DecodeThresholdNonAscii) {
			str_out.push_back(static_cast<char>(mapping->at(alias_code_point)));
		} else {
			str_out.push_back(static_cast<char>(alias_code_point));
		}
		return true;
	};

	// Handle code points belonging to selected code page
	auto push_code_page = [&str_out, &convert_mode](const map_grapheme_to_dos_t* mapping,
	                                                const Grapheme& grapheme) {
		if (!mapping) {
			return false;
		}

		// Check if code page has a characters matching the grapheme
		const auto it = mapping->find(grapheme);
		if (it != mapping->end()) {
			str_out.push_back(static_cast<char>(it->second));
			return true;
		}

		// Check if the grapheme represent a screen characters which has
		// the same code as an ASCII control character
		const auto screen_code = grapheme_to_screen_code(grapheme,
		                                                 convert_mode);
		if (screen_code) {
			str_out.push_back(static_cast<char>(*screen_code));
			return true;
		}

		return false;
	};

	// Handle code points which can only be mapped to ASCII
	// using a fallback Unicode mapping table
	auto push_fallback = [&str_out](const Grapheme& grapheme) {
		if (grapheme.HasMark()) {
			return false;
		}

		const auto it = mapping_ascii.find(grapheme.GetCodePoint());
		if (it == mapping_ascii.end()) {
			return false;
		}

		str_out.push_back(static_cast<char>(it->second));
		return true;
	};

	// Handle unknown code points
	auto push_unknown = [&](const uint16_t code_point) {
		if (fallback == UnicodeFallback::EmptyString) {
			return false;
		}

		str_out.push_back(static_cast<char>(UnknownCharacter));
		warn_code_point(code_point);
		return true;
	};

	// Helper for handling normalized graphemes
	auto push_normalized = [&](const Grapheme& grapheme) {
		switch (fallback) {
		case UnicodeFallback::EmptyString:
			return push_7bit(grapheme) ||
			       push_code_page(mapping_normalized, grapheme);
		case UnicodeFallback::Simple:
			return push_7bit(grapheme) ||
			       push_code_page(mapping_normalized, grapheme) ||
			       push_code_page(aliases_normalized, grapheme) ||
			       push_fallback(grapheme);
		case UnicodeFallback::Box:
			return push_7bit(grapheme) ||
			       push_box_drawing(mapping_normalized,
			                        box_code_points,
			                        grapheme) ||
			       push_code_page(mapping_normalized, grapheme) ||
			       push_code_page(aliases_normalized, grapheme) ||
			       push_fallback(grapheme);
		default: assert(false); return false;
		}
	};

	// Helper for handling non-normalized graphemes
	auto push_decomposed = [&](const Grapheme& grapheme) {
		Grapheme decomposed = grapheme;
		decomposed.Decompose();

		switch (fallback) {
		case UnicodeFallback::EmptyString:
			return push_code_page(mapping_decomposed, decomposed);
		case UnicodeFallback::Simple:
		case UnicodeFallback::Box:
			return push_code_page(mapping_decomposed, decomposed) ||
			       push_code_page(aliases_decomposed, decomposed);
		default: assert(false); return false;
		}
	};

	for (size_t i = 0; i < str.size(); ++i) {
		Grapheme grapheme(str[i]);
		while (i + 1 < str.size() && is_combining_mark(str[i + 1])) {
			++i;
			grapheme.AddMark(str[i]);
		}

		// Try to push matching character
		if (push_normalized(grapheme) || push_decomposed(grapheme)) {
			continue;
		}

		// Last, desperate attempt: decompose and strip marks
		const auto original_code_point = grapheme.GetCodePoint();
		grapheme.Decompose();
		if (grapheme.HasMark()) {
			grapheme.StripMarks();
			if (push_normalized(grapheme)) {
				continue;
			}
		}

		// We are unable to match this grapheme at all
		if (!push_unknown(original_code_point)) {
			return {};
		}
	}

	str_out.shrink_to_fit();
	return str_out;
}

static wide_string dos_to_wide(const std::string& str,
                               const DosStringConvertMode convert_mode,
                               const uint16_t code_page)
{
	wide_string str_out = {};
	str_out.reserve(str.size());

	for (const auto character : str) {
		const auto byte = static_cast<uint8_t>(character);
		if (byte >= DecodeThresholdNonAscii) {
			// Take from code page mapping
			auto& mappings = per_code_page_mappings[code_page];

			if ((per_code_page_mappings.count(code_page) == 0) ||
			    (mappings.grapheme_to_dos.count(byte) == 0)) {
				str_out.push_back(UnknownCharacter);
			} else {
				mappings.grapheme_to_dos[byte].PushInto(str_out);
			}
		} else if (is_control_code(byte)) {
			const auto wide = screen_code_to_wide(byte, convert_mode);
			if (wide) {
				str_out.push_back(*wide);
			} else {
				str_out.push_back(UnknownCharacter);
			}
		} else {
			str_out.push_back(byte);
		}
	}

	return str_out;
}

// ***************************************************************************
// Read resources from files
// ***************************************************************************

static bool prepare_code_page(const uint16_t code_page);

template <typename T1, typename T2>
bool add_if_not_mapped(std::map<T1, T2>& mapping, T1 first, T2 second)
{
	[[maybe_unused]] const auto& [item, was_added] =
		mapping.try_emplace(first, second);

	return was_added;
}

static std::ifstream open_mapping_file(const std_fs::path& path_root,
                                       const std::string& file_name)
{
	const std_fs::path file_path = path_root / file_name;
	std::ifstream in_file(file_path.string());
	if (!in_file) {
		LOG_ERR("UNICODE: Could not open mapping file %s",
		        file_name.c_str());
	}

	return in_file;
}

static bool get_line(std::ifstream& in_file, std::string& line_str, size_t& line_num)
{
	line_str.clear();

	while (line_str.empty()) {
		if (!std::getline(in_file, line_str)) {
			return false;
		}
		if (line_str.length() >= 1 && line_str[0] == EndOfFileMarking) {
			return false; // end of definitions
		}
		++line_num;
	}

	return true;
}

static bool get_tokens(const std::string& line, std::vector<std::string>& tokens)
{
	// Split the line into tokens, strip away comments

	bool token_started = false;
	size_t start_pos   = 0;

	tokens.clear();
	for (size_t i = 0; i < line.size(); ++i) {
		if (line[i] == '#') {
			break; // comment started
		}

		const bool is_space = (line[i] == ' ') || (line[i] == '\t') ||
		                      (line[i] == '\r') || (line[i] == '\n');

		if (!token_started && !is_space) {
			// We are at first character of the token
			token_started = true;
			start_pos     = i;
			continue;
		}

		if (token_started && is_space) {
			// We are at the first character after the token
			token_started = false;
			tokens.emplace_back(line.substr(start_pos, i - start_pos));
			continue;
		}
	}

	if (token_started) {
		// We have a token which ends with the end of the line
		tokens.emplace_back(line.substr(start_pos));
	}

	return !tokens.empty();
}

static bool get_hex_8bit(const std::string& token, uint8_t& out)
{
	if (token.size() != 4 || token[0] != '0' || token[1] != 'x' ||
	    !isxdigit(token[2]) || !isxdigit(token[3])) {
		return false;
	}

	out = static_cast<uint8_t>(strtoul(token.c_str() + 2, nullptr, 16));
	return true;
}

static bool get_hex_16bit(const std::string& token, uint16_t& out)
{
	if (token.size() != 6 || token[0] != '0' || token[1] != 'x' ||
	    !isxdigit(token[2]) || !isxdigit(token[3]) || !isxdigit(token[4]) ||
	    !isxdigit(token[5])) {
		return false;
	}

	out = static_cast<uint16_t>(strtoul(token.c_str() + 2, nullptr, 16));
	return true;
}

static bool get_ascii(const std::string& token, uint8_t& out)
{
	if (token.length() == 1) {
		out = static_cast<uint8_t>(token[0]);
	} else if (token == "SPC") {
		out = ' ';
	} else if (token == "HSH") {
		out = '#';
	} else if (token == "NNN") {
		out = UnknownCharacter;
	} else {
		return false;
	}

	return true;
}

static bool get_code_page(const std::string& token, uint16_t& code_page)
{
	if (token.empty() || token.length() > 5) {
		return false;
	}

	for (const auto character : token) {
		if (character < '0' || character > '9') {
			return false;
		}
	}

	const auto tmp = std::atoi(token.c_str());
	if (tmp < 1 || tmp > UINT16_MAX) {
		return false;
	}

	code_page = static_cast<uint16_t>(tmp);
	return true;
}

static bool get_grapheme(const std::vector<std::string>& tokens, Grapheme& grapheme)
{
	uint16_t code_point = 0;
	if (tokens.size() < 2 || !get_hex_16bit(tokens[1], code_point)) {
		return false;
	}

	Grapheme new_grapheme(code_point);

	if (tokens.size() >= 3) {
		if (!get_hex_16bit(tokens[2], code_point)) {
			return false;
		}
		new_grapheme.AddMark(code_point);
	}

	if (tokens.size() >= 4) {
		if (!get_hex_16bit(tokens[3], code_point)) {
			return false;
		}
		new_grapheme.AddMark(code_point);
	}

	grapheme = new_grapheme;
	return true;
}

static void error_parsing(const std::string& file_name, const size_t line_num,
                          const std::string& details = "")
{
	if (details.empty()) {
		LOG_ERR("UNICODE: Error parsing mapping file %s, line %d",
		        file_name.c_str(),
		        static_cast<int>(line_num));
	} else {
		LOG_ERR("UNICODE: Error parsing mapping file %s, line %d: %s",
		        file_name.c_str(),
		        static_cast<int>(line_num),
		        details.c_str());
	}
}

static void error_code_point_found_twice(const uint16_t code_point,
                                         const std::string& file_name,
                                         const size_t line_num)
{
	std::stringstream details;
	details << std::string("code point U+") << std::hex << code_point
	        << " found twice";

	error_parsing(file_name, line_num, details.str());
}

static void error_not_combining_mark(const size_t position,
                                     const std::string& file_name,
                                     const size_t line_num)
{
	const auto details = std::string("token #") + std::to_string(position) +
	                     " is not a supported combining mark";
	error_parsing(file_name, line_num, details);
}

static void error_code_page_invalid(const std::string& file_name, const size_t line_num)
{
	error_parsing(file_name, line_num, "invalid code page number");
}

static void error_code_page_defined(const std::string& file_name, const size_t line_num)
{
	error_parsing(file_name, line_num, "code page already defined");
}

static void error_code_page_none(const std::string& file_name, const size_t line_num)
{
	error_parsing(file_name, line_num, "not currently defining a code page");
}

static bool check_import_status(const std::ifstream& in_file,
                                const std::string& file_name, const bool empty)
{
	if (in_file.fail() && !in_file.eof()) {
		LOG_ERR("UNICODE: Error reading mapping file %s", file_name.c_str());
		return false;
	}

	if (empty) {
		LOG_ERR("UNICODE: Mapping file %s has no entries",
		        file_name.c_str());
		return false;
	}

	return true;
}

static bool check_grapheme_valid(const Grapheme& grapheme,
                                 const std::string& file_name, const size_t line_num)
{
	if (grapheme.IsValid()) {
		return true;
	}

	LOG_ERR("UNICODE: Error, invalid grapheme defined in file %s, line %d",
	        file_name.c_str(),
	        static_cast<int>(line_num));
	return false;
}

static bool import_mapping_code_page(const std_fs::path& path_root,
                                     const std::string& file_name,
                                     map_dos_to_grapheme_t& mapping)
{
	// Import code page character -> UTF-8 mapping from external file

	assert(mapping.empty());

	// Open the file
	auto in_file = open_mapping_file(path_root, file_name);
	if (!in_file) {
		LOG_ERR("UNICODE: Error opening mapping file %s", file_name.c_str());
		return false;
	}

	// Read and parse
	std::string line_str = " ";
	size_t line_num      = 0;

	map_dos_to_grapheme_t new_mapping = {};

	while (get_line(in_file, line_str, line_num)) {
		std::vector<std::string> tokens;
		if (!get_tokens(line_str, tokens)) {
			continue; // empty line
		}

		uint8_t character_code = 0;
		if (!get_hex_8bit(tokens[0], character_code)) {
			error_parsing(file_name, line_num);
			return false;
		}

		if (tokens.size() == 1) {
			// Handle undefined character entry, ignore 7-bit ASCII
			// codes
			if (character_code >= DecodeThresholdNonAscii) {
				Grapheme grapheme;
				add_if_not_mapped(new_mapping, character_code, grapheme);
			}

		} else if (tokens.size() <= 4) {
			// Handle mapping entry, ignore 7-bit ASCII codes
			if (character_code >= DecodeThresholdNonAscii) {
				Grapheme grapheme;
				if (!get_grapheme(tokens, grapheme)) {
					error_parsing(file_name, line_num);
					return false;
				}

				// Invalid grapheme that is not added
				// (overridden) is OK here; at least CP 1258
				// definition from Unicode.org contains mapping
				// of code page characters to combining marks,
				// which is fine for converting texts, but a
				// no-no for DOS emulation (where the number of
				// output characters has to match the number of
				// input characters). For such code page
				// definitions, just override problematic mappings
				// in the main mapping configuration file.
				if (add_if_not_mapped(new_mapping, character_code, grapheme) &&
				    !check_grapheme_valid(grapheme, file_name, line_num)) {
					return false;
				}
			}
		} else {
			error_parsing(file_name, line_num);
			return false;
		}
	}

	if (!check_import_status(in_file, file_name, new_mapping.empty())) {
		return false;
	}

	// Reading/parsing succeeded - use all the data read from file
	mapping = new_mapping;
	return true;
}


static void import_config_main(const std_fs::path& path_root)
{
	// Import main configuration file, telling how to construct UTF-8
	// mappings for each and every supported code page

	assert(config_mappings.empty());
	assert(config_duplicates.empty());
	assert(config_aliases.empty());

	// Open the file
	const auto &file_name = file_name_main;
	auto in_file = open_mapping_file(path_root, file_name);
	if (!in_file) {
		return;
	}

	// Read and parse
	bool file_empty      = true;
	std::string line_str = " ";
	size_t line_num      = 0;

	uint16_t curent_code_page = 0;

	config_mappings_t new_config_mappings     = {};
	config_duplicates_t new_config_duplicates = {};
	config_aliases_t new_config_aliases       = {};

	while (get_line(in_file, line_str, line_num)) {
		std::vector<std::string> tokens;
		if (!get_tokens(line_str, tokens))
			continue; // empty line
		uint8_t character_code = 0;

		if (tokens[0] == "ALIAS") {
			if ((tokens.size() != 3 && tokens.size() != 4) ||
			    (tokens.size() == 4 && tokens[3] != "BIDIRECTIONAL")) {
				error_parsing(file_name, line_num);
				return;
			}

			uint16_t code_point_1 = 0;
			uint16_t code_point_2 = 0;
			if (!get_hex_16bit(tokens[1], code_point_1) ||
			    !get_hex_16bit(tokens[2], code_point_2)) {
				error_parsing(file_name, line_num);
				return;
			}

			new_config_aliases.emplace_back(
			        std::make_pair(code_point_1, code_point_2));

			if (tokens.size() == 4) // check if bidirectional
				new_config_aliases.emplace_back(
				        std::make_pair(code_point_2, code_point_1));

			curent_code_page = 0;

		} else if (tokens[0] == "CODEPAGE") {
			auto check_no_code_page = [&](const uint16_t code_page) {
				if ((new_config_mappings.find(code_page) != new_config_mappings.end() && new_config_mappings[code_page].valid) ||
				    new_config_duplicates.find(code_page) != new_config_duplicates.end()) {
					error_code_page_defined(file_name, line_num);
					return false;
				}
				return true;
			};

			if (tokens.size() == 4 && tokens[2] == "DUPLICATES") {
				uint16_t code_page_1 = 0;
				uint16_t code_page_2 = 0;
				if (!get_code_page(tokens[1], code_page_1) ||
				    !get_code_page(tokens[3], code_page_2)) {
					error_code_page_invalid(file_name, line_num);
					return;
				}

				// Make sure code page definition does not exist yet
				if (!check_no_code_page(code_page_1)) {
					return;
				}

				new_config_duplicates[code_page_1] = code_page_2;
				curent_code_page = 0;

			} else {
				uint16_t code_page = 0;
				if (tokens.size() != 2 ||
				    !get_code_page(tokens[1], code_page)) {
					error_code_page_invalid(file_name, line_num);
					return;
				}

				// Make sure code page definition does not exist yet
				if (!check_no_code_page(code_page)) {
					return;
				}

				new_config_mappings[code_page].valid = true;
				curent_code_page = code_page;
			}

		} else if (tokens[0] == "EXTENDS") {
			if (!curent_code_page) {
				error_code_page_none(file_name, line_num);
				return;
			}

			if (tokens.size() == 3 && tokens[1] == "CODEPAGE") {
				uint16_t code_page = 0;
				if (!get_code_page(tokens[2], code_page)) {
					error_code_page_invalid(file_name, line_num);
					return;
				}

				new_config_mappings[curent_code_page].extends_code_page = code_page;
			} else if (tokens.size() == 4 && tokens[1] == "FILE") {
				new_config_mappings[curent_code_page].extends_dir =
				        tokens[2];
				new_config_mappings[curent_code_page].extends_file =
				        tokens[3];
				// some meaningful mapping provided
				file_empty = false;
			} else {
				error_parsing(file_name, line_num);
				return;
			}

			curent_code_page = 0;

		} else if (get_hex_8bit(tokens[0], character_code)) {
			if (!curent_code_page) {
				error_code_page_none(file_name, line_num);
				return;
			}

			auto& new_mapping =
			        new_config_mappings[curent_code_page].mapping;

			if (tokens.size() == 1) {
				// Handle undefined character entry
				if (character_code >= DecodeThresholdNonAscii) {
					// ignore 7-bit ASCII codes
					Grapheme grapheme; // placeholder
					add_if_not_mapped(new_mapping,
					                  character_code,
					                  grapheme);
					file_empty = false; // some meaningful
					                    // mapping provided
				}

			} else if (tokens.size() <= 4) {
				// Handle mapping entry
				if (character_code >= DecodeThresholdNonAscii) {
					// ignore 7-bit ASCII codes
					Grapheme grapheme; // placeholder
					if (!get_grapheme(tokens, grapheme)) {
						error_parsing(file_name, line_num);
						return;
					}

					if (!check_grapheme_valid(grapheme,
					                          file_name,
					                          line_num))
						return;

					add_if_not_mapped(new_mapping,
					                  character_code,
					                  grapheme);
					// some meaningful mapping provided
					file_empty = false;
				}

			} else {
				error_parsing(file_name, line_num);
				return;
			}

		} else {
			error_parsing(file_name, line_num);
			return;
		}
	}

	if (!check_import_status(in_file, file_name, file_empty)) {
		return;
	}

	// Reading/parsing succeeded - use all the data read from file
	config_mappings   = new_config_mappings;
	config_duplicates = new_config_duplicates;
	config_aliases    = new_config_aliases;
}

static void import_decomposition(const std_fs::path& path_root)
{
	// Import Unicode decomposition rules; will be used to handle
	// non-normalized Unicode input

	assert(decomposition_rules.empty());

	// Open the file
	const auto& file_name = file_name_decomposition;
	auto in_file = open_mapping_file(path_root, file_name);
	if (!in_file) {
		return;
	}

	// Read and parse
	std::string line_str = "";
	size_t line_num      = 0;

	decomposition_rules_t new_rules = {};

	while (get_line(in_file, line_str, line_num)) {
		std::vector<std::string> tokens;
		if (!get_tokens(line_str, tokens)) {
			continue; // empty line
		}

		uint16_t code_point_1 = 0;
		uint16_t code_point_2 = 0;

		if (tokens.size() < 3 || !get_hex_16bit(tokens[0], code_point_1) ||
		    !get_hex_16bit(tokens[1], code_point_2)) {
			error_parsing(file_name, line_num);
			return;
		}

		new_rules[code_point_1] = code_point_2;
		for (size_t idx = 2; idx < tokens.size(); ++idx) {
			if (!get_hex_16bit(tokens[idx], code_point_2)) {
					error_parsing(file_name, line_num);
					return;
			}
			if (!is_combining_mark(code_point_2)) {
					error_not_combining_mark(idx + 1,
						                 file_name,
						                 line_num);
					return;
			}
			new_rules[code_point_1].AddMark(code_point_2);
		}
	}

	if (!check_import_status(in_file, file_name, new_rules.empty())) {
		return;
	}

	// Reading/parsing succeeded - use the rules
	decomposition_rules = new_rules;
}

static void import_mapping_ascii(const std_fs::path& path_root)
{
	// Import fallback mapping, from Unicode to 7-bit ASCII;
	// this mapping will only be used if everything else fails

	assert(mapping_ascii.empty());

	// Open the file
	const auto& file_name = file_name_ascii;
	auto in_file = open_mapping_file(path_root, file_name);
	if (!in_file) {
		return;
	}

	// Read and parse
	std::string line_str = "";
	size_t line_num      = 0;

	map_grapheme_to_dos_t new_mapping_ascii = {};

	while (get_line(in_file, line_str, line_num)) {
		std::vector<std::string> tokens;
		if (!get_tokens(line_str, tokens)) {
			continue; // empty line
		}

		uint16_t code_point = 0;
		uint8_t character   = 0;

		if (tokens.size() != 2 || !get_hex_16bit(tokens[0], code_point) ||
		    !get_ascii(tokens[1], character)) {
			error_parsing(file_name, line_num);
			return;
		}

		new_mapping_ascii[code_point] = character;
	}

	if (!check_import_status(in_file, file_name, new_mapping_ascii.empty())) {
		return;
	}

	// Reading/parsing succeeded - use the mapping
	mapping_ascii = new_mapping_ascii;
}

static void import_mapping_case(const std_fs::path& path_root)
{
	// Import lowercase <-> uppercase mapping data

	assert(uppercase.empty());
	assert(lowercase.empty());

	// Open the file
	const auto& file_name = file_name_case;
	auto in_file = open_mapping_file(path_root, file_name);
	if (!in_file) {
		return;
	}

	// Read and parse
	std::string line_str = "";
	size_t line_num      = 0;

	// All the encountered uppercase/lowercase code points
	std::set<uint16_t> all_uppercase = {};
	std::set<uint16_t> all_lowercase = {};

	map_code_point_case_t new_uppercase = {};
	map_code_point_case_t new_lowercase = {};

	while (get_line(in_file, line_str, line_num)) {
		std::vector<std::string> tokens;
		if (!get_tokens(line_str, tokens)) {
			continue; // empty line
		}

		if (tokens.size() != 2) {
			error_parsing(file_name, line_num);
			return;
		}

		uint16_t code_point_upper = 0;
		uint16_t code_point_lower = 0;

		// Skip code points which do not have corresponding upper/lower
		// case characters - they would be needed if we were providing
		// functions like is_upper_case(...)

		if (tokens[0] == "NNN") {
			// Retrieve code point
			if (!get_hex_16bit(tokens[1], code_point_lower)) {
					error_parsing(file_name, line_num);
					return;
			}

			// Make sure there are no repetitions
			if ((all_lowercase.count(code_point_lower) > 0) ||
			    (all_uppercase.count(code_point_lower) > 0)) {
				        error_code_point_found_twice(code_point_lower,
				                                     file_name,
				                                     line_num);
				        return;
			}

			// Store the code point for further repetition checks
			all_lowercase.insert(code_point_lower);
			continue;
		}

		if (tokens[1] == "NNN") {
			// Retrieve code point
			if (!get_hex_16bit(tokens[0], code_point_upper)) {
					error_parsing(file_name, line_num);
					return;
			}

			// Make sure there are no repetitions
			if ((all_lowercase.count(code_point_upper) > 0) ||
			    (all_uppercase.count(code_point_upper) > 0)) {
				        error_code_point_found_twice(code_point_upper,
				                                     file_name,
				                                     line_num);
				        return;
			}

			// Store the code point for further repetition checks
			all_uppercase.insert(code_point_upper);
			continue;
		}

		// Retrieve code points
		if (!get_hex_16bit(tokens[0], code_point_upper) ||
		    !get_hex_16bit(tokens[1], code_point_lower)) {
			error_parsing(file_name, line_num);
			return;
		}

		// Make sure there are no repetitions
		if ((all_lowercase.count(code_point_lower) > 0) ||
		    (all_uppercase.count(code_point_lower) > 0)) {
			error_code_point_found_twice(code_point_lower,
			                             file_name,
			                             line_num);
			return;
		}
		if ((all_lowercase.count(code_point_upper) > 0) ||
		    (all_uppercase.count(code_point_upper) > 0)) {
			error_code_point_found_twice(code_point_upper,
			                             file_name,
			                             line_num);
			return;
		}

		// Store the code points for further repetition checks
		all_lowercase.insert(code_point_lower);
		all_uppercase.insert(code_point_upper);

		// Add code points to case maps
		new_uppercase[code_point_lower] = code_point_upper;
		new_lowercase[code_point_upper] = code_point_lower;
	}

	if (!check_import_status(in_file, file_name, new_uppercase.empty())) {
		return;
	}

	// Reading/parsing succeeded - use the mapping
	uppercase = new_uppercase;
	lowercase = new_lowercase;
}

static uint16_t deduplicate_code_page(const uint16_t code_page)
{
	const auto it = config_duplicates.find(code_page);

	if (it == config_duplicates.end()) {
		return code_page;
	}

	return it->second;
}

static void construct_decomposed(const map_grapheme_to_dos_t& normalized,
                                 map_grapheme_to_dos_t& decomposed)
{
	decomposed.clear();
	for (const auto& [grapheme, character_code] : normalized) {
		auto tmp = grapheme;
		tmp.Decompose();
		if (grapheme == tmp) {
			continue;
		}

		decomposed[tmp] = character_code;
	}
}

static void construct_box_fallback(const map_grapheme_to_dos_t& code_page_mapping,
                                   map_box_code_points_t& out_box_code_points)
{
	auto is_adaptation_needed = [&]() {
		// Check if current list of box drawing characters needs
		// to be adapted due to missing characters in the given
		// code page
		for (const auto& [from, target] : out_box_code_points) {
			if (code_page_mapping.count(target) == 0) {
				return true;
			}
		}
		return false;
	};

	auto is_alias_group_suitable = [&](const std::vector<alias_t>& alias_group) {
		// Check if the given alias group is:
		// - applicable = the code page contains all the characters
		//   corresponding to target code points
		// - profitable = applying the alias will reduce the number
		//   of code points without matching characters in the given
		//   code page

		bool is_group_profitable = false;
		for (const auto& [from, target] : out_box_code_points) {
			for (const auto& alias : alias_group) {
				if (from != alias.first) {
					continue;
				}

				if (code_page_mapping.count(alias.second) == 0) {
					// We cannot apply this alias group
					return false;
				}

				if (code_page_mapping.count(target) == 0) {
					is_group_profitable = true;
				}
			}
		}

		return is_group_profitable;
	};

	auto try_set = [&](const box_drawing_set_t& drawing_set) {
		assert(BoxDrawingSetRegular.size() == drawing_set.size());

		// Create initial mapping
		out_box_code_points.clear();
		for (size_t idx = 0; idx < drawing_set.size(); ++idx) {
			const auto code_point_1 = BoxDrawingSetRegular[idx];
			const auto code_point_2 = drawing_set[idx];
			out_box_code_points[code_point_1] = code_point_2;
		}

		// If necessary, adapt mapping using alias groups
		for (const auto& alias_group : box_alias_groups) {
			if (!is_adaptation_needed()) {
				// No adaptation needed - our job is done here
				return true;
			}

			if (!is_alias_group_suitable(alias_group)) {
				continue;
			}

			// Apply the aliases from the current group
			for (const auto& [from, target] : out_box_code_points) {
				for (const auto& alias : alias_group) {
					if (from != alias.first) {
						continue;
					}

					out_box_code_points[from] = alias.second;
				}
			}
		}

		return !is_adaptation_needed();
	};

	// Try to adjust regular (full) drawing character set for the code page,
	// if failed try the reduced set without double lines
	if (try_set(BoxDrawingSetRegular) || try_set(BoxDrawingSetLight)) {
		return;
	}

	// Last resort fallback - use 7-bit ASCII fallback for everything
	out_box_code_points.clear();
	for (const auto& code_point : BoxDrawingSetRegular) {
		if (mapping_ascii.count(code_point) > 0) {
			out_box_code_points[code_point] = mapping_ascii.at(code_point);
		} else {
			out_box_code_points[code_point] = UnknownCharacter;
		}
	}
}

static void construct_case_mapping(const map_code_point_case_t& map_case_global,
		                   const map_grapheme_to_dos_t& code_page_mapping,
		                   map_dos_character_case_t& out_map_case)
{
	out_map_case.resize(UINT8_MAX + 1);
	for (uint16_t idx = 0; idx < UINT8_MAX + 1; ++idx) {
		if (idx < DecodeThresholdNonAscii &&
		    (map_case_global.count(idx) > 0)) {
			out_map_case[idx] = static_cast<uint8_t>(
			        map_case_global.at(idx));
		} else {
			out_map_case[idx] = static_cast<uint8_t>(idx);
		}
	}

	for (const auto& [grapheme, character_code] : code_page_mapping) {
		if (map_case_global.count(grapheme.GetCodePoint()) == 0) {
			// No information how to switch case for this code point
			continue;
		}

		Grapheme grapheme_switched = map_case_global.at(
			grapheme.GetCodePoint());
		grapheme_switched.CopyMarksFrom(grapheme);

		if (code_page_mapping.count(grapheme_switched) == 0) {
			// Code page does not contain switched case character
			continue;
		}

		out_map_case[character_code] = code_page_mapping.at(grapheme_switched);
	}
}

static bool construct_mapping(const uint16_t code_page)
{
	// Prevent processing if previous attempt failed;
	// also protect against circular dependencies

	static std::set<uint16_t> already_tried;
	if (already_tried.count(code_page) > 0) {
		return false;
	}
	already_tried.insert(code_page);

	assert(per_code_page_mappings.count(code_page) == 0);

	// First apply mapping found in main config file

	const auto& config_mapping      = config_mappings[code_page];
	map_grapheme_to_dos_t new_mapping         = {};
	map_dos_to_grapheme_t new_mapping_reverse = {};

	auto add_to_mappings = [&](const uint8_t first, const Grapheme& second) {
		if (first < DecodeThresholdNonAscii) {
			return;
		}
		if (!add_if_not_mapped(new_mapping_reverse, first, second)) {
			return;
		}
		if (second.IsEmpty() || !second.IsValid()) {
			return;
		}
		if (add_if_not_mapped(new_mapping, second, first)) {
			return;
		}

		LOG_WARNING("UNICODE: Mapping for code page %d uses a code point twice; character 0x%02x",
		            code_page,
		            first);
	};

	for (const auto& entry : config_mapping.mapping) {
		add_to_mappings(entry.first, entry.second);
	}

	// If code page is expansion of other code page, copy remaining entries

	if (config_mapping.extends_code_page) {
		const uint16_t dependency = deduplicate_code_page(config_mapping.extends_code_page);
		if (!prepare_code_page(dependency)) {
			LOG_ERR("UNICODE: Code page %d mapping requires code page %d mapping",
			        code_page,
			        dependency);
			return false;
		}

		for (const auto& entry :
		     per_code_page_mappings[dependency].dos_to_grapheme_normalized) {
			add_to_mappings(entry.second, entry.first);
		}
	}

	// If code page uses external mapping file, load appropriate entries

	if (!config_mapping.extends_file.empty()) {
		map_dos_to_grapheme_t mapping_file;

		if (!import_mapping_code_page(get_resource_path(config_mapping.extends_dir),
		                              config_mapping.extends_file,
		                              mapping_file)) {
			return false;
		}

		for (const auto& entry : mapping_file) {
			add_to_mappings(entry.first, entry.second);
		}
	}

	auto& mappings = per_code_page_mappings[code_page];

	mappings.dos_to_grapheme_normalized = std::move(new_mapping);
	mappings.grapheme_to_dos = std::move(new_mapping_reverse);

	// Construct decomposed mapping
	construct_decomposed(mappings.dos_to_grapheme_normalized,
	                     mappings.dos_to_grapheme_decomposed);

	// Construct mapping for box-optimized fallback mode
	construct_box_fallback(mappings.dos_to_grapheme_normalized,
	                       mappings.box_code_points);

	// Construct upper/lower case mappings
	construct_case_mapping(uppercase, mappings.dos_to_grapheme_normalized,
	                       mappings.uppercase);
	construct_case_mapping(lowercase, mappings.dos_to_grapheme_normalized,
	                       mappings.lowercase);

	return true;
}

static void construct_aliases(const uint16_t code_page)
{
	auto& mappings = per_code_page_mappings[code_page];

	assert(mappings.aliases_normalized.empty());
	assert(mappings.aliases_decomposed.empty());

	const auto& mapping_normalized = mappings.dos_to_grapheme_normalized;
	auto& aliases_normalized = mappings.aliases_normalized;

	auto add_alias = [&](const alias_t& alias) {
		const auto found_it = mapping_normalized.find(alias.second);
		if ((mapping_normalized.count(alias.first) == 0) &&
		    (aliases_normalized.count(alias.first) == 0) &&
		    (found_it != mapping_normalized.end())) {
			aliases_normalized[alias.first] = found_it->second;
		}
	};

	// Construct aliases based on the configuration
	std::for_each(config_aliases.begin(), config_aliases.end(), add_alias);

	// Add box drawing aliases
	for (const auto& alias_group : box_alias_groups) {
		std::for_each(alias_group.begin(), alias_group.end(), add_alias);
	}

	// Construct decomposed aliases
	construct_decomposed(mappings.aliases_normalized,
	                     mappings.aliases_decomposed);
}

static bool prepare_code_page(const uint16_t code_page)
{
	if (per_code_page_mappings.count(code_page) > 0) {
		return true; // code page already prepared
	}

	if ((config_mappings.count(code_page) == 0) || !construct_mapping(code_page)) {
		// Unsupported code page or error
		per_code_page_mappings.erase(code_page);
		return false;
	}

	construct_aliases(code_page);
	return true;
}

static void load_config_if_needed()
{
	// If this is the first time we are requested to prepare the code page,
	// load the top-level configuration and fallback 7-bit ASCII mapping

	static bool config_loaded = false;
	if (!config_loaded) {
		const auto path_root = get_resource_path(dir_name_mapping);
		import_decomposition(path_root);
		import_mapping_ascii(path_root);
		import_mapping_case(path_root);
		import_config_main(path_root);
		config_loaded = true;
	}
}

static uint16_t get_custom_code_page(const uint16_t in_code_page)
{
	load_config_if_needed();

	if (in_code_page == 0) {
		return 0;
	}

	const uint16_t code_page = deduplicate_code_page(in_code_page);
	if (!prepare_code_page(code_page)) {
		return 0;
	}

	return code_page;
}

// ***************************************************************************
// External interface
// ***************************************************************************

uint16_t get_utf8_code_page()
{
	load_config_if_needed();

	constexpr uint16_t RomCodePage = 437; // United States

	// Below EGA it wasn't possible to change the character set
	const uint16_t code_page = IS_EGAVGA_ARCH
	                                 ? deduplicate_code_page(dos.loaded_codepage)
	                                 : RomCodePage;

	// For unsupported code pages revert to default one
	if (prepare_code_page(code_page)) {
		return code_page;
	}

	return 0;
}

static std::string utf8_to_dos_common(const std::string& str,
                                      const DosStringConvertMode convert_mode,
                                      const UnicodeFallback fallback,
                                      const uint16_t code_page)
{
	load_config_if_needed();

	const auto tmp = utf8_to_wide(str);
	return wide_to_dos(tmp, convert_mode, fallback, code_page);
}

std::string utf8_to_dos(const std::string& str,
                        const DosStringConvertMode convert_mode,
                        const UnicodeFallback fallback)
{
	return utf8_to_dos_common(str,
	                          convert_mode,
	                          fallback,
	                          get_utf8_code_page());
}

std::string utf8_to_dos(const std::string& str,
                        const DosStringConvertMode convert_mode,
                        const UnicodeFallback fallback,
                        const uint16_t code_page)
{
	return utf8_to_dos_common(str,
	                          convert_mode,
	                          fallback,
	                          get_custom_code_page(code_page));
}

static std::string dos_to_utf8_common(const std::string& str,
                                      const DosStringConvertMode convert_mode,
                                      const uint16_t code_page)
{
	load_config_if_needed();

	const auto tmp = dos_to_wide(str, convert_mode, code_page);
	return wide_to_utf8(tmp);
}

std::string dos_to_utf8(const std::string& str,
                        const DosStringConvertMode convert_mode)
{
	return dos_to_utf8_common(str, convert_mode, get_utf8_code_page());
}

std::string dos_to_utf8(const std::string& str,
                        const DosStringConvertMode convert_mode,
                        const uint16_t code_page)
{
	return dos_to_utf8_common(str, convert_mode, get_custom_code_page(code_page));
}

static std::string lowercase_dos_common(const std::string& str,
                                        const uint16_t code_page)
{
	load_config_if_needed();

	assert(per_code_page_mappings.count(code_page) > 0);
	const auto& mapping = per_code_page_mappings[code_page].lowercase;
	assert(mapping.size() == UINT8_MAX);

	std::string str_out = str;
	for (auto& entry : str_out) {
		const auto idx = static_cast<uint8_t>(entry);
		entry = static_cast<char>(mapping[idx]);
	}

	return str_out;
}

std::string lowercase_dos(const std::string& str)
{
	return lowercase_dos_common(str, get_utf8_code_page());
}

std::string lowercase_dos(const std::string& str, const uint16_t code_page)
{
	return lowercase_dos_common(str, get_custom_code_page(code_page));
}

static std::string uppercase_dos_common(const std::string& str,
                                        const uint16_t code_page)
{
	load_config_if_needed();

	assert(per_code_page_mappings.count(code_page) > 0);
	const auto& mapping = per_code_page_mappings[code_page].uppercase;
	assert(mapping.size() == UINT8_MAX);

	std::string str_out = str;
	for (auto& entry : str_out) {
		const auto idx = static_cast<uint8_t>(entry);
		entry          = static_cast<char>(mapping[idx]);
	}

	return str_out;
}

std::string uppercase_dos(const std::string& str)
{
	return uppercase_dos_common(str, get_utf8_code_page());
}

std::string uppercase_dos(const std::string& str, const uint16_t code_page)
{
	return uppercase_dos_common(str, get_custom_code_page(code_page));
}
