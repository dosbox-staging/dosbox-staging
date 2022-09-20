/*
 *  Copyright (C) 2022       The DOSBox Staging Team
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
#include <string>
#include <vector>

#include "checks.h"
#include "dos_inc.h"
#include "string_utils.h"

CHECK_NARROWING();


class Grapheme final {
public:
    Grapheme() = default;
    Grapheme(const uint16_t code_point);

    bool IsEmpty() const;
    bool IsValid() const;
    bool HasMark() const;
    uint16_t GetCodePoint() const;

    void Invalidate();
    void AddMark(const uint16_t code_point);
    void StripMarks();

    bool operator<(const Grapheme &other) const;

private:
    // Unicode code point
    uint16_t code_point = static_cast<uint16_t>(' ');
    // Combining marks, 0 = none, 1 = U+0300, 2 = U+0301, etc.
    uint16_t mark_1st = 0;
    uint16_t mark_2nd = 0;

    bool empty = true;
    bool valid = true;
};

using code_page_mapping_t = std::map<Grapheme, uint8_t>; // UTF-8 -> code page
using code_page_mapping_reverse_t = std::map<uint8_t, Grapheme>;

using config_duplicates_t = std::map<uint16_t, uint16_t>;
using config_aliases_t    = std::vector<std::pair<uint16_t, uint16_t>>;

struct ConfigMappingEntry {
    bool valid                          = false;
    code_page_mapping_reverse_t mapping = {};
    uint16_t extends_code_page          = 0;
    std::string extends_dir             = "";
    std::string extends_file            = "";
};

using config_mappings_t = std::map<uint16_t, ConfigMappingEntry>;

static const std::string file_name_main   = "MAIN.TXT";
static const std::string file_name_ascii  = "ASCII.TXT";
static const std::string dir_name_mapping = "mapping";

// Thresholds to tell how the code point is encoded
constexpr uint8_t threshold_non_ascii = 0b1'000'0000;
constexpr uint8_t threshold_encoded   = 0b1'100'0000;
constexpr uint8_t threshold_bytes_3   = 0b1'110'0000;
constexpr uint8_t threshold_bytes_4   = 0b1'111'0000;
constexpr uint8_t threshold_bytes_5   = 0b1'111'1000;
constexpr uint8_t threshold_bytes_6   = 0b1'111'1100;

// Use the character below if there is absolutely no sane way to handle UTF-8 glyph
constexpr uint8_t unknown_character = 0x3f; // '?'

// End of file marking, use in some files from unicode.org
constexpr uint8_t end_of_file_marking = 0x1a;

// Main information about how to create UTF-8 mappings for given DOS code page
static config_mappings_t config_mappings = {};
// UTF-8 -> UTF-8 fallback mapping (alias), use before fallback to 7-bit ASCII
static config_aliases_t config_aliases = {};
// Information about code pages which are exact duplicates
static config_duplicates_t config_duplicates = {};

// UTF-8 -> 7-bit ASCII mapping, use as a last resort mapping
static code_page_mapping_t mapping_ascii = {};

// Concrete UTF-8 -> codepage mappings
static std::map<uint16_t, code_page_mapping_t> mappings = {};
// Additional UTF-8 -> codepage mappings, to avoid unknown characters
static std::map<uint16_t, code_page_mapping_t> mappings_aliases = {};
// Reverse mappings, codepage -> UTF-8
static std::map<uint16_t, code_page_mapping_reverse_t> mappings_reverse = {};

// ***************************************************************************
// Grapheme type implementation
// ***************************************************************************

static bool is_combining_mark(const uint32_t code_point)
{
    static const std::vector<std::pair<uint16_t, uint16_t>> ranges = {
        { 0x0300, 0x036f }, // Combining Diacritical Marks
        { 0x1ab0, 0x1aff }, // Combining Diacritical Marks Extended
        { 0x1dc0, 0x1dff }, // Combining Diacritical Marks Supplement
        { 0x20d0, 0x20ff }, // Combining Diacritical Marks for Symbols
        { 0xfe20, 0xfe2f }, // Combining Half Marks
    };

    for (const auto &range : ranges)
        if (code_point >= range.first && code_point <= range.second)
            return true;

    return false;
}

Grapheme::Grapheme(const uint16_t code_point) :
    code_point(code_point),
    empty(false)
{
    // It is not valid to have a combining mark
    // as a main code point of the grapheme

    if (is_combining_mark(code_point))
        Invalidate();
}

bool Grapheme::IsEmpty() const
{
    return empty;
}

bool Grapheme::IsValid() const
{
    return valid;
}

bool Grapheme::HasMark() const
{
    return mark_1st;
}

uint16_t Grapheme::GetCodePoint() const
{
    return code_point;
}

void Grapheme::Invalidate()
{
    empty = false;
    valid = false;

    code_point = unknown_character;
    mark_1st   = 0;
    mark_2nd   = 0;
}

void Grapheme::AddMark(const uint16_t code_point)
{
    if (!valid)
        return; // can't add combining mark to invalid grapheme

    if (!is_combining_mark(code_point) || empty) {
        Invalidate(); // not a combining mark or empty grapheme
        return;
    }

    if (mark_1st && mark_2nd)
        return; // up to 2 combining marks supported, ignore the rest

    if (mark_1st)
        mark_2nd = code_point;
    else
        mark_1st = code_point;
}

void Grapheme::StripMarks()
{
    mark_1st = 0;
    mark_2nd = 0;
}

bool Grapheme::operator<(const Grapheme &other) const
{
    // TODO: after migrating to C++20 try to use a templated lambda here

    if (code_point < other.code_point)
        return true;
    else if (code_point > other.code_point)
        return false;

    if (mark_1st < other.mark_1st)
        return true;
    else if (mark_1st > other.mark_1st)
        return false;

    if (mark_2nd < other.mark_2nd)
        return true;
    else if (mark_2nd > other.mark_2nd)
        return false;

    if (!empty && other.empty)
        return true;
    else if (empty && !other.empty)
        return false;

    if (!valid && other.valid)
        return true;
    else if (valid && !other.valid)
        return false;

    // '*this == other' condition is, unfortunately, not recognized by older compilers
    assert(code_point == other.code_point &&
           mark_1st   == other.mark_1st &&
           mark_2nd   == other.mark_2nd &&
           empty      == other.empty &&
           valid      == other.valid);

    return false;
}

// ***************************************************************************
// Conversion routines
// ***************************************************************************

static uint8_t char_to_uint8(const char in_char)
{
    if (in_char >= 0)
        return static_cast<uint8_t>(in_char);
    else
        return static_cast<uint8_t>(UINT8_MAX + in_char + 1);
}

static char uint8_to_char(const uint8_t in_uint8)
{
    if (in_uint8 <= INT8_MAX)
        return static_cast<char>(in_uint8);
    else
        return static_cast<char>(in_uint8 - INT8_MIN + INT8_MIN);
}

static bool utf8_to_wide(const std::string &str_in, std::vector<uint16_t> &str_out)
{
    // Convert UTF-8 string to a sequence of decoded integers

    // For UTF-8 encoding explanation see here:
    // - https://www.codeproject.com/Articles/38242/Reading-UTF-8-with-C-streams
    // - https://en.wikipedia.org/wiki/UTF-8#Encoding

    bool status = true;

    str_out.clear();
    for (size_t i = 0; i < str_in.size(); ++i) {
        const size_t remaining = str_in.size() - i - 1;
        const uint8_t byte_1   = char_to_uint8(str_in[i]);
        const uint8_t byte_2   = (remaining >= 1) ? char_to_uint8(str_in[i + 1]) : 0;
        const uint8_t byte_3   = (remaining >= 2) ? char_to_uint8(str_in[i + 2]) : 0;

        auto advance = [&](const size_t bytes) {
            auto counter = std::min(remaining, bytes);
            while (counter--) {
                const auto byte_next = char_to_uint8(str_in[i + 1]);
                if (byte_next < threshold_non_ascii || byte_next >= threshold_encoded)
                    break;
                ++i;
            }

            status = false; // advancing without decoding
        };

        // Retrieve code point
        uint32_t code_point = unknown_character;

        // Support code point needing up to 3 bytes to encode; this includes
        // Latin, Greek, Cyrillic, Hebrew, Arabic, VGA charset symbols, etc.
        // More bytes are needed mainly for historic scripts, emoji, etc.

        if (GCC_UNLIKELY(byte_1 >= threshold_bytes_6))
            // 6-byte code point (>= 31 bits), no support
            advance(5);
        else if (GCC_UNLIKELY(byte_1 >= threshold_bytes_5))
            // 5-byte code point (>= 26 bits), no support
            advance(4);
        else if (GCC_UNLIKELY(byte_1 >= threshold_bytes_4))
            // 4-byte code point (>= 21 bits), no support
            advance(3);
        else if (GCC_UNLIKELY(byte_1 >= threshold_bytes_3)) {
            // 3-byte code point - decode 1st byte
            code_point = static_cast<uint8_t>(byte_1 - threshold_bytes_3);
            // Decode 2nd byte
            code_point = code_point << 6;
            if (byte_2 >= threshold_non_ascii && byte_2 < threshold_encoded) {
                ++i;
                code_point = code_point + byte_2 - threshold_non_ascii;
            } else
                status = false; // code point encoding too short
            // Decode 3rd byte
            code_point = code_point << 6;
            if (byte_2 >= threshold_non_ascii && byte_2 < threshold_encoded &&
                byte_3 >= threshold_non_ascii && byte_3 < threshold_encoded) {
                ++i;
                code_point = code_point + byte_3 - threshold_non_ascii;
            } else
                status = false; // code point encoding too short
        } else if (GCC_UNLIKELY(byte_1 >= threshold_encoded)) {
            // 2-byte code point - decode 1st byte
            code_point = static_cast<uint8_t>(byte_1 - threshold_encoded);
            // Decode 2nd byte
            code_point = code_point << 6;
            if (byte_2 >= threshold_non_ascii && byte_2 < threshold_encoded) {
                ++i;
                code_point = code_point + byte_2 - threshold_non_ascii;
            } else
                status = false; // code point encoding too short
        } else if (byte_1 < threshold_non_ascii)
            // 1-byte code point, ASCII compatible
            code_point = byte_1;
        else
            status = false; // not UTF8 encoding

        str_out.push_back(static_cast<uint16_t>(code_point));
    }

    return status;
}

static void warn_code_point(const uint16_t code_point)
{
    static std::set<uint16_t> already_warned;
    if (already_warned.count(code_point))
        return;
    already_warned.insert(code_point);
    LOG_WARNING("UTF8: No fallback mapping for code point 0x%04x", code_point);
}

static void warn_code_page(const uint16_t code_page)
{
    static std::set<uint16_t> already_warned;
    if (already_warned.count(code_page))
        return;
    already_warned.insert(code_page);
    LOG_WARNING("UTF8: Requested unknown code page %d", code_page);
}

static std::string wide_to_code_page(const std::vector<uint16_t> &str_in,
                                     const uint16_t code_page)
{
    std::string str_out = {};

    code_page_mapping_t *mapping         = nullptr;
    code_page_mapping_t *mapping_aliases = nullptr;

    // Try to find UTF8 -> code page mapping
    if (code_page != 0) {
        const auto it = mappings.find(code_page);
        if (it != mappings.end())
            mapping = &it->second;
        else
            warn_code_page(code_page);

        const auto it_alias = mappings_aliases.find(code_page);
        if (it_alias != mappings_aliases.end())
            mapping_aliases = &it_alias->second;
    }

    // Handle code points which are 7-bit ASCII characters
    auto push_7bit = [&str_out](const Grapheme &grapheme) {
        if (grapheme.HasMark())
            return false; // not a 7-bit ASCII character

        const auto code_point = grapheme.GetCodePoint();
        if (code_point >= threshold_non_ascii)
            return false; // not a 7-bit ASCII character

        str_out.push_back(static_cast<char>(code_point));
        return true;
    };

    // Handle code points belonging to selected code page
    auto push_code_page = [&str_out](const code_page_mapping_t *mapping,
                                     const Grapheme &grapheme) {
        if (!mapping)
            return false;
        const auto it = mapping->find(grapheme);
        if (it == mapping->end())
            return false;

        str_out.push_back(uint8_to_char(it->second));
        return true;
    };

    // Handle code points which can only be mapped to ASCII
    // using a fallback UTF8 mapping table
    auto push_fallback = [&str_out](const Grapheme &grapheme) {
        if (grapheme.HasMark())
            return false;

        const auto it = mapping_ascii.find(grapheme.GetCodePoint());
        if (it == mapping_ascii.end())
            return false;

        str_out.push_back(uint8_to_char(it->second));
        return true;
    };

    // Handle unknown code points
    auto push_unknown = [&str_out](const uint16_t code_point) {
        str_out.push_back(static_cast<char>(unknown_character));
        warn_code_point(code_point);
    };

    for (size_t i = 0; i < str_in.size(); ++i) {
        Grapheme grapheme(str_in[i]);
        while (i + 1 < str_in.size() && is_combining_mark(str_in[i + 1])) {
            ++i;
            grapheme.AddMark(str_in[i]);
        }

        auto push = [&](const Grapheme &grapheme) {
            return push_7bit(grapheme) ||
                   push_code_page(mapping, grapheme) ||
                   push_code_page(mapping_aliases, grapheme) ||
                   push_fallback(grapheme);
        };

        if (push(grapheme))
            continue;

        if (grapheme.HasMark()) {
            grapheme.StripMarks();
            if (push(grapheme))
                continue;
        }

        push_unknown(grapheme.GetCodePoint());
    }

    str_out.shrink_to_fit();
    return str_out;
}

// ***************************************************************************
// Read resources from files
// ***************************************************************************

static bool prepare_code_page(const uint16_t code_page);

template <typename T1, typename T2>
bool add_if_not_mapped(std::map<T1, T2> &mapping, T1 first, T2 second)
{
    [[maybe_unused]] const auto &[item, was_added] = 
        mapping.try_emplace(first, second);

    return was_added;
}

static std::ifstream open_mapping_file(const std_fs::path &path_root,
                                       const std::string &file_name)
{
    const std_fs::path file_path = path_root / file_name;
    std::ifstream in_file(file_path.string());
    if (!in_file)
        LOG_ERR("UTF8: Could not open mapping file %s", file_name.c_str());

    return in_file;
}

static bool get_line(std::ifstream &in_file, std::string &line_str, size_t &line_num)
{
    line_str.clear();

    while (line_str.empty()) {
        if (!std::getline(in_file, line_str))
            return false;
        if (line_str.length() >= 1 && line_str[0] == end_of_file_marking)
            return false; // end of definitions
        ++line_num;
    }

    return true;
}

static bool get_tokens(const std::string &line, std::vector<std::string> &tokens)
{
    // Split the line into tokens, strip away comments

    bool token_started = false;
    size_t start_pos   = 0;

    tokens.clear();
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '#')
            break; // comment started

        const bool is_space = (line[i] == ' ')  || (line[i] == '\t') ||
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

    if (token_started)
        // We have a token which ends with the end of the line
        tokens.emplace_back(line.substr(start_pos));

   return !tokens.empty();
}

static bool get_hex_8bit(const std::string &token, uint8_t &out)
{
    if (token.size() != 4 || token[0] != '0' || token[1] != 'x' ||
        !isxdigit(token[2]) || !isxdigit(token[3]))
        return false;

    out = static_cast<uint8_t>(strtoul(token.c_str() + 2, nullptr, 16));
    return true;
}

static bool get_hex_16bit(const std::string &token, uint16_t &out)
{
    if (token.size() != 6 || token[0] != '0' || token[1] != 'x' ||
        !isxdigit(token[2]) || !isxdigit(token[3]) ||
        !isxdigit(token[4]) || !isxdigit(token[5]))
        return false;

    out = static_cast<uint16_t>(strtoul(token.c_str() + 2, nullptr, 16));
    return true;
}

static bool get_ascii(const std::string &token, uint8_t &out)
{
    if (token.length() == 1) {
        out = char_to_uint8(token[0]);
    } else if (GCC_UNLIKELY(token == "SPC")) {
        out = ' ';
    } else if (GCC_UNLIKELY(token == "HSH")) {
        out = '#';
    } else if (token == "NNN") {
        out = unknown_character;
    } else
        return false;

    return true;
}

static bool get_code_page(const std::string &token, uint16_t &code_page)
{
    if (GCC_UNLIKELY(token.empty() || token.length() > 5))
        return false;

    for (size_t i = 0; i < token.size(); ++i)
        if (GCC_UNLIKELY(token[i] < '0' || token[i] > '9'))
            return false;

    const auto tmp = std::atoi(token.c_str());
    if (GCC_UNLIKELY(tmp < 1 || tmp > UINT16_MAX))
        return false;

    code_page = static_cast<uint16_t>(tmp);
    return true;
}

static bool get_grapheme(const std::vector<std::string> &tokens, Grapheme &grapheme)
{
    uint16_t code_point = 0;
    if (tokens.size() < 2 || !get_hex_16bit(tokens[1], code_point))
        return false;

    Grapheme new_grapheme(code_point);

    if (tokens.size() >= 3) {
        if (!get_hex_16bit(tokens[2], code_point))
            return false;
        new_grapheme.AddMark(code_point);
    }

    if (tokens.size() >= 4) {
        if (!get_hex_16bit(tokens[3], code_point))
            return false;
        new_grapheme.AddMark(code_point);
    }

    grapheme = new_grapheme;
    return true;
}

static void error_parsing(const std::string &file_name, const size_t line_num,
                          const std::string &details = "")
{
    if (details.empty())
        LOG_ERR("UTF8: Error parsing mapping file %s, line %d",
                file_name.c_str(), static_cast<int>(line_num));
    else
        LOG_ERR("UTF8: Error parsing mapping file %s, line %d: %s",
                file_name.c_str(), static_cast<int>(line_num), details.c_str());
}

static void error_code_page_invalid(const std::string &file_name, const size_t line_num)
{
    error_parsing(file_name, line_num, "invalid code page number");
}

static void error_code_page_defined(const std::string &file_name, const size_t line_num)
{
    error_parsing(file_name, line_num, "code page already defined");
}

static void error_code_page_none(const std::string &file_name, const size_t line_num)
{
    error_parsing(file_name, line_num, "not currently defining a code page");
}

static bool check_import_status(const std::ifstream &in_file,
                                const std::string &file_name, const bool empty)
{
    if (in_file.fail() && !in_file.eof()) {
        LOG_ERR("UTF8: Error reading mapping file %s", file_name.c_str());
        return false;
    }

    if (empty) {
        LOG_ERR("UTF8: Mapping file %s has no entries", file_name.c_str());
        return false;
    }

    return true;
}

static bool check_grapheme_valid(const Grapheme &grapheme,
                                 const std::string &file_name, const size_t line_num)
{
    if (grapheme.IsValid())
        return true;

    LOG_ERR("UTF8: Error, invalid grapheme defined in file %s, line %d",
            file_name.c_str(), static_cast<int>(line_num));
    return false;
}

static bool import_mapping_code_page(const std_fs::path &path_root,
                                     const std::string &file_name,
                                     code_page_mapping_reverse_t &mapping)
{
    // Import code page character -> UTF-8 mapping from external file

    // Open the file
    auto in_file = open_mapping_file(path_root, file_name);
    if (!in_file) {
        LOG_ERR("UTF8: Error opening mapping file %s", file_name.c_str());
        return false;
    }

    // Read and parse
    std::string line_str = " ";
    size_t line_num      = 0;

    code_page_mapping_reverse_t new_mapping;

    while (get_line(in_file, line_str, line_num)) {
        std::vector<std::string> tokens;
        if (!get_tokens(line_str, tokens))
            continue; // empty line

        uint8_t character_code = 0;
        if (!get_hex_8bit(tokens[0], character_code)) {
            error_parsing(file_name, line_num);
            return false;
        }

        if (GCC_UNLIKELY(tokens.size() == 1)) {
            // Handle undefined character entry, ignore 7-bit ASCII
            // codes
            if (character_code >= threshold_non_ascii) {
                Grapheme grapheme;
                add_if_not_mapped(new_mapping, character_code, grapheme);
            }

        } else if (tokens.size() <= 4) {
            // Handle mapping entry, ignore 7-bit ASCII codes
            if (character_code >= threshold_non_ascii) {
                Grapheme grapheme;
                if (!get_grapheme(tokens, grapheme)) {
                    error_parsing(file_name, line_num);
                    return false;
                }

                // Invalid grapheme that is not added (overridden) is OK here;
                // at least CP 1258 definition from Unicode.org contains mapping
                // of code page characters to combining marks, which is fine for
                // converting texts, but a no-no for DOS emulation (where the
                // number of output characters has to match the number of input
                // characters).
                // For such code page definitions, just override problematic
                // mappings in the main mapping configuration file.
                if (add_if_not_mapped(new_mapping, character_code, grapheme) &&
                    !check_grapheme_valid(grapheme, file_name, line_num))
                    return false;
            }
        } else {
            error_parsing(file_name, line_num);
            return false;
        }
    }

    if (!check_import_status(in_file, file_name, new_mapping.empty()))
        return false;

    // Reading/parsing succeeded - use all the data read from file
    mapping = new_mapping;
    return true;
}

static void import_config_main(const std_fs::path &path_root)
{
    // Import main configuration file, telling how to construct UTF-8
    // mappings for each and every supported code page

    // Open the file
    auto in_file = open_mapping_file(path_root, file_name_main);
    if (!in_file)
        return;

    // Read and parse
    bool file_empty      = true;
    std::string line_str = " ";
    size_t line_num      = 0;

    uint16_t curent_code_page = 0;

    config_mappings_t new_config_mappings;
    config_duplicates_t new_config_duplicates;
    config_aliases_t new_config_aliases;

    while (get_line(in_file, line_str, line_num)) {
        std::vector<std::string> tokens;
        if (!get_tokens(line_str, tokens))
            continue; // empty line
        uint8_t character_code = 0;

        if (GCC_UNLIKELY(tokens[0] == "ALIAS")) {
            if ((tokens.size() != 3 && tokens.size() != 4) ||
                (tokens.size() == 4 && tokens[3] != "BIDIRECTIONAL")) {
                error_parsing(file_name_main, line_num);
                return;
            }

            uint16_t code_point_1 = 0;
            uint16_t code_point_2 = 0;
            if (!get_hex_16bit(tokens[1], code_point_1) ||
                !get_hex_16bit(tokens[2], code_point_2)) {
                error_parsing(file_name_main, line_num);
                return;
            }

            new_config_aliases.emplace_back(std::make_pair(code_point_1, code_point_2));

            if (tokens.size() == 4) // check if bidirectional
                new_config_aliases.emplace_back(std::make_pair(code_point_2, code_point_1));

            curent_code_page = 0;

        } else if (GCC_UNLIKELY(tokens[0] == "CODEPAGE")) {

            auto check_no_code_page = [&](const uint16_t code_page) {
                if ((new_config_mappings.find(code_page) != new_config_mappings.end() &&
                       new_config_mappings[code_page].valid) ||
                    new_config_duplicates.find(code_page) != new_config_duplicates.end()) {
                    error_code_page_defined(file_name_main, line_num);
                    return false;
                }
                return true;
            };

            if (tokens.size() == 4 && tokens[2] == "DUPLICATES") {
                uint16_t code_page_1 = 0;
                uint16_t code_page_2 = 0;
                if (!get_code_page(tokens[1], code_page_1) ||
                    !get_code_page(tokens[3], code_page_2)) {
                    error_code_page_invalid(file_name_main, line_num);
                    return;
                }

                // Make sure code page definition does nto exist yet
                if (!check_no_code_page(code_page_1))
                    return;

                new_config_duplicates[code_page_1] = code_page_2;
                curent_code_page = 0;

            } else {
                uint16_t code_page = 0;
                if (tokens.size() != 2 || !get_code_page(tokens[1], code_page)) {
                    error_code_page_invalid(file_name_main, line_num);
                    return;
                }

                // Make sure code page definition does nto exist yet
                if (!check_no_code_page(code_page))
                    return;

                new_config_mappings[code_page].valid = true;
                curent_code_page = code_page;
            }

        } else if (GCC_UNLIKELY(tokens[0] == "EXTENDS")) {
            if (!curent_code_page) {
                error_code_page_none(file_name_main, line_num);
                return;
            }

            if (tokens.size() == 3 && tokens[1] == "CODEPAGE") {
                uint16_t code_page = 0;
                if (!get_code_page(tokens[2], code_page)) {
                    error_code_page_invalid(file_name_main, line_num);
                    return;
                }

                new_config_mappings[curent_code_page].extends_code_page = code_page;
            } else if (tokens.size() == 4 && tokens[1] == "FILE") {
                new_config_mappings[curent_code_page].extends_dir  = tokens[2];
                new_config_mappings[curent_code_page].extends_file = tokens[3];
                file_empty = false; // some meaningful mapping provided
            } else {
                error_parsing(file_name_main, line_num);
                return;
            }

            curent_code_page = 0;

        } else if (get_hex_8bit(tokens[0], character_code)) {
            if (!curent_code_page) {
                error_code_page_none(file_name_main, line_num);
               return;
            }

            auto &new_mapping = new_config_mappings[curent_code_page].mapping;

            if (tokens.size() == 1) {
                // Handle undefined character entry
                if (character_code >= threshold_non_ascii) { // ignore 7-bit ASCII codes
                    Grapheme grapheme;
                    add_if_not_mapped(new_mapping, character_code, grapheme);
                    file_empty = false; // some meaningful mapping provided
                }

            } else if (tokens.size() <= 4) {
                // Handle mapping entry
                if (character_code >= threshold_non_ascii) { // ignore 7-bit ASCII codes

                    Grapheme grapheme;
                    if (!get_grapheme(tokens, grapheme)) {
                        error_parsing(file_name_main, line_num);
                        return;
                    }

                    if (!check_grapheme_valid(grapheme, file_name_main, line_num))
                        return;

                    add_if_not_mapped(new_mapping, character_code, grapheme);
                    file_empty = false; // some meaningful mapping provided
                }

            } else {
                error_parsing(file_name_main, line_num);
                return;
            }

        } else {
            error_parsing(file_name_main, line_num);
            return;
        }
    }

    if (!check_import_status(in_file, file_name_main, file_empty))
        return;

    // Reading/parsing succeeded - use all the data read from file
    config_mappings   = new_config_mappings;
    config_duplicates = new_config_duplicates;
    config_aliases    = new_config_aliases;
}

static void import_mapping_ascii(const std_fs::path &path_root)
{
    // Import fallback mapping, from UTH-8 to 7-bit ASCII;
    // this mapping will only be used if everything else fails

    // Open the file
    auto in_file = open_mapping_file(path_root, file_name_ascii);
    if (!in_file)
        return;

    // Read and parse
    std::string line_str = "";
    size_t line_num      = 0;

    code_page_mapping_t new_mapping_ascii;

    while (get_line(in_file, line_str, line_num)) {
        std::vector<std::string> tokens;
        if (!get_tokens(line_str, tokens))
            continue; // empty line

        uint16_t code_point = 0;
        uint8_t character   = 0;

        if (tokens.size() != 2 ||
              !get_hex_16bit(tokens[0], code_point) ||
            !get_ascii(tokens[1], character)) {
            error_parsing(file_name_ascii, line_num);
            return;
        }

        new_mapping_ascii[code_point] = character;
    }

    if (!check_import_status(in_file, file_name_ascii, new_mapping_ascii.empty()))
         return;

    // Reading/parsing succeeded - use the mapping
    mapping_ascii = new_mapping_ascii;
}

static uint16_t deduplicate_code_page(const uint16_t code_page)
{
    const auto it = config_duplicates.find(code_page);

    if (it == config_duplicates.end())
        return code_page;
    else
        return it->second;
}

static bool construct_mapping(const uint16_t code_page)
{
    // Prevent processing if previous attempt failed;
    // also protect against circular dependencies

    static std::set<uint16_t> already_tried;
    if (already_tried.count(code_page))
        return false;
    already_tried.insert(code_page);

    assert(config_mappings.count(code_page));
    assert(!mappings.count(code_page));
    assert(!mappings_reverse.count(code_page));
    assert(!mappings.count(code_page));

    // First apply mapping found in main config file

    const auto &config_mapping      = config_mappings[code_page];
    code_page_mapping_t new_mapping = {};
    code_page_mapping_reverse_t new_mapping_reverse = {};

    auto add_to_mappings = [&](const uint8_t first, const Grapheme &second) {
        if (first < threshold_non_ascii)
            return;
        if (!add_if_not_mapped(new_mapping_reverse, first, second))
            return;
        if (second.IsEmpty() || !second.IsValid())
            return;
        if (add_if_not_mapped(new_mapping, second, first))
            return;

        LOG_WARNING("UTF8: mapping for code page %d uses a code point twice; character 0x%02x",
                    code_page, first);
    };

    for (const auto &entry : config_mapping.mapping)
         add_to_mappings(entry.first, entry.second);

    // If code page is expansion of other code page, copy remaining entries

    if (config_mapping.extends_code_page) {
        const uint16_t dependency = deduplicate_code_page(config_mapping.extends_code_page);
        if (!prepare_code_page(dependency)) {
            LOG_ERR("UTF8: Code page %d mapping requires code page %d mapping",
                    code_page, dependency);
            return false;
        }

        for (const auto &entry : mappings[dependency])
            add_to_mappings(entry.second, entry.first);
    }

    // If code page uses external mapping file, load appropriate entries

    if (!config_mapping.extends_file.empty()) {
        code_page_mapping_reverse_t mapping_file;

        if (!import_mapping_code_page(GetResourcePath(config_mapping.extends_dir),
                                      config_mapping.extends_file, mapping_file))
            return false;

        for (const auto &entry : mapping_file)
             add_to_mappings(entry.first, entry.second);
    }

    mappings[code_page]         = new_mapping;
    mappings_reverse[code_page] = new_mapping_reverse;
    return true;
}

static void construct_mapping_aliases(const uint16_t code_page)
{
    assert(!mappings_aliases.count(code_page));
    assert(mappings.count(code_page));

    const auto &mapping   = mappings[code_page];
    auto &mapping_aliases = mappings_aliases[code_page];

    for (const auto &alias : config_aliases)
        if (!mapping.count(alias.first) && mapping.count(alias.second) &&
            !mapping_aliases.count(alias.first))
            mapping_aliases[alias.first] = mapping.find(alias.second)->second;
}

static bool prepare_code_page(const uint16_t code_page)
{
    if (mappings.count(code_page))
        return true; // code page already prepared

    if (!config_mappings.count(code_page) || !construct_mapping(code_page)) {
        // Unsupported code page or error
        mappings.erase(code_page);
        mappings_reverse.erase(code_page);
        return false;
    }

    construct_mapping_aliases(code_page);
    return true;
}

static void load_config_if_needed()
{
    // If this is the first time we are requested to prepare the code page,
    // load the top-level configuration and fallback 7-bit ASCII mapping

    static bool config_loaded = false;
    if (!config_loaded) {
        const auto path_root = GetResourcePath(dir_name_mapping);
        import_mapping_ascii(path_root);
        import_config_main(path_root);
        config_loaded = true;
    }
}

// ***************************************************************************
// External interface
// ***************************************************************************

uint16_t UTF8_GetCodePage()
{
    const uint16_t cp_default = 437; // United States
    if (!IS_EGAVGA_ARCH)
        // Below EGA it wasn't possible to change character set
        return cp_default;

    load_config_if_needed();
    const uint16_t cp = deduplicate_code_page(dos.loaded_codepage);

    // For unsupported code pages, revert to default one
    if (prepare_code_page(cp))
        return cp;
    else
        return cp_default;
}

std::string UTF8_RenderForDos(const std::string &str_in,
                              const uint16_t code_page)
{
    load_config_if_needed();
    const uint16_t cp = deduplicate_code_page(code_page);
    prepare_code_page(cp);

    std::vector<uint16_t> str_wide;
    if (!utf8_to_wide(str_in, str_wide))
        LOG_WARNING("UTF8: Problem rendering string");

    return wide_to_code_page(str_wide, cp);
}


// If you need a routine to convert a screen code to UTF-8,
// here is how it should look like:

#if 0

// Note: this returns unencoded sequence of code points

void Grapheme::PushInto(std::vector<uint16_t> &str_out)
{
    str_out.push_back(code_point);

    if (!mark_1st)
        return;
    str_out.push_back(mark_1st);

    if (!mark_2nd)
        return;
    str_out.push_back(mark_2nd);
}

void UTF8_FromScreenCode(const uint8_t character_in, std::vector<uint16_t> &str_out,
                         const uint16_t code_page)
{
    str_out.clear();

    if (GCC_UNLIKELY(character_in >= threshold_non_ascii)) {

        // Character above 0x07f - take from code page mapping

        load_config_if_needed();
        const uint16_t cp = deduplicate_code_page(code_page);
        prepare_code_page(cp);

        if (!mappings_reverse.count(cp) ||
            !mappings_reverse[cp].count(character_in))
            str_out.push_back(' ');
        else
            (mappings_reverse[cp])[character_in].PushInto(str_out);

    } else {

        // Unicode code points for screen codes from 0x00 to 0x1f
        // see: https://en.wikipedia.org/wiki/Code_page_437
        constexpr uint16_t codes[0x20] = {
            0x0020, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, // 00-07
            0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c, // 08-0f
            0x25ba, 0x25c4, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8, // 10-17
            0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc, // 18-1f
        };

        if (GCC_UNLIKELY(character_in == 0x7f))
            str_out.push_back(0x2302);
        else if (character_in >= 0x20)
            str_out.push_back(character_in);
        else
            str_out.push_back(codes[character_in]);
    }

   str_out.shrink_to_fit();
}

#endif
