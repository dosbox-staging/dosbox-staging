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

#include "checks.h"
#include "dos_inc.h"
#include "codepages-utf8/cp.h"

CHECK_NARROWING();


static std::map<uint16_t, CodePageMapping> mappings;


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

static void utf8_to_wide(const std::string &str_in, std::vector<uint16_t> &str_out)
{
    // For UTF-8 encoding explanation see here:
    // - https://www.codeproject.com/Articles/38242/Reading-UTF-8-with-C-streams
    // - https://en.wikipedia.org/wiki/UTF-8#Encoding

    str_out.clear();
    for (size_t i = 0; i < str_in.size(); i++) {
        const size_t  remaining = str_in.size() - i - 1;
        const uint8_t byte_1 = char_to_uint8(str_in[i]);
        const uint8_t byte_2 = (remaining >= 1) ? char_to_uint8(str_in[i + 1]) : 0;
        const uint8_t byte_3 = (remaining >= 2) ? char_to_uint8(str_in[i + 2]) : 0;

        // Retrieve code point
        uint32_t code_point = UINT32_MAX; // dummy, to trigger unsupported
        if (byte_1 < 0x80) {

            // This is a 1-byte code point, ASCII compatible
            code_point = byte_1;

        } else if (GCC_UNLIKELY(byte_1 < 0xc0)) {

            // Not an UTF-8 code point, handle like code page 437
            const auto it = cp_437.find(byte_1);
            if (it != cp_437.end())
                code_point = it->second;

        } else if (GCC_UNLIKELY(byte_1 >= 0xfc)) {

            // 6-byte code point (>= 31 bits), no support
            i += 5;

        } else if (GCC_UNLIKELY(byte_1 >= 0xf8)) {

            // 5-byte code point (>= 26 bits), no support
            i += 4;

        } else if (GCC_UNLIKELY(byte_1 >= 0xf0)) {

            // 4-byte code point (>= 21 bits), no support
            i += 3;

        } else if (GCC_UNLIKELY(byte_1 >= 0xe0)) {

            // 3-byte code point
            i += 2;
            if (byte_2 >= 0x80 && byte_2 < 0xc0 &&
                byte_3 >= 0x80 && byte_3 < 0xc0) {
                code_point = static_cast<uint8_t>(byte_1 - 0xe0);
                code_point = code_point << 6;
                code_point = code_point + byte_2 - 0x80;
                code_point = code_point << 6;
                code_point = code_point + byte_3 - 0x80;
            }

        } else {

            // 2-byte codepoint
            i += 1;
            if (byte_2 >= 0x80 && byte_2 < 0xc0) {
                code_point = static_cast<uint8_t>(byte_1 - 0xc0);
                code_point = code_point << 6;
                code_point = code_point + byte_2 - 0x80;
            }
        }

        if (code_point <= UINT16_MAX)
            // Latin, Greek, Cyrillic, Hebrew, Arabic, etc.
            // VGA charset symbols are in this range, too
            str_out.push_back(static_cast<uint16_t>(code_point));
        else {
            // Chinese, Japanese, Korean, historic scripts, emoji, etc.
            // No support for these, useless for DOS emulation
            str_out.push_back(unknown_character);
        }
    }
}

static void warn_code_point(const uint16_t code_point)
{
    static std::set<uint16_t> already_warned;
    if (already_warned.count(code_point))
        return;
    already_warned.insert(code_point);
    LOG_WARNING("UTF8: No mapping in cp_ascii for code point 0x%04x", code_point);
}

static void warn_code_page(const uint16_t code_page)
{
    static std::set<uint16_t> already_warned;
    if (already_warned.count(code_page))
        return;
    already_warned.insert(code_page);
    LOG_WARNING("UTF8: Requested unknown code page %d", code_page);
}

static void wide_to_code_page(const std::vector<uint16_t> &str_in,
                              std::string &str_out, const uint16_t code_page)
{
    // try to find UTF8 -> code page mapping
    CodePageMapping *mapping = nullptr;
    if (code_page != 0) {
        const auto it = mappings.find(code_page);
        if (it != mappings.end())
            mapping = &it->second;
        else
            warn_code_page(code_page);
    }

    // Handle code points which are 7-bit ASCII characters
    auto push_7bit = [&str_out](const uint16_t code_point) {
        if (code_point >= 0x80)
            return false; // not a 7-bit ASCII character

        str_out.push_back(static_cast<char>(code_point));
        return true;
    };

    // Handle code points belonging to selected code page
    auto push_code_page = [&str_out, &mapping](const uint16_t code_point) {
        if (!mapping)
            return false;
        const auto it = mapping->find(code_point);
        if (it == mapping->end())
            return false;
        str_out.push_back(it->second);
        return true;
    };

    // Handle code points which can only be mapped to ASCII
    // using a fallback UTF8 mapping table
    auto push_ascii = [&str_out](const uint16_t code_point) {
        const auto it = cp_ascii.find(code_point);
        if (it == cp_ascii.end())
            return false;
        str_out.push_back(it->second);
        return true;
    };

    // Handle unknown code points
    auto push_unknown = [&str_out](const uint16_t code_point) {
        str_out.push_back(static_cast<char>(unknown_character));
        warn_code_point(code_point);
    };

    for (const auto code_point : str_in) {
        if (push_7bit(code_point) || push_code_page(code_point) ||
            push_ascii(code_point))
            continue;

        push_unknown(code_point);
    }
}

static void construct_mapping(CodePageMapping &mapping,
                              const std::vector<const CodePageImport *> &list)
{
    std::set<uint8_t> already_added;

    for (auto &cp_import : list)
        for (const auto entry : *cp_import) {
            if (already_added.count(entry.first))
                continue;
            already_added.insert(entry.first);
            mapping[entry.second] = uint8_to_char(entry.first);
        }
}

static void apply_patches(CodePageMapping &mapping)
{
    for (const auto &patch : cp_patches) {
        // Get UTF-8 code points from entry
        const auto &cp_1 = patch.first;
        const auto &cp_2 = patch.second;
        // Extend the mapping to possibly include more characters
        if (mapping.count(cp_1) && !mapping.count(cp_2))
            mapping[cp_2] = mapping[cp_1];
        else if (!mapping.count(cp_1) && mapping.count(cp_2))
            mapping[cp_1] = mapping[cp_2];
    }
}

static uint16_t deduplicate_code_page(const uint16_t code_page)
{
    const auto it = cp_duplicates.find(code_page);

    if (it == cp_duplicates.end())
        return code_page;
    else
        return it->second;
}

static bool prepare_code_page(const uint16_t code_page)
{
    if (mappings.count(code_page))
        return true; // code page already prepared

    const auto it = cp_receipts.find(code_page);
    if (it == cp_receipts.end())
        return false; // no information how to make the code page

    construct_mapping(mappings[code_page], it->second);

    // Note: if you wat to extend the UTF-8 support, to convert from guest
    // to host (native filesystem in UTF-8? TTF console?), this is the
    // proper place to construct a reverse map - patches below alter the
    // mappling, so that it is no longer one-to-one

    apply_patches(mappings[code_page]);
    return true;
}

uint16_t UTF8_GetCodePage()
{
    const uint16_t cp_default = 437; // United States
    if (!IS_EGAVGA_ARCH)
        // Below EGA it wasn't possible to change character set
        return cp_default;

    const uint16_t cp = deduplicate_code_page(dos.loaded_codepage);

    // For unsupported code pages, revert to default one
    if (prepare_code_page(cp))
        return cp;
    else
        return cp_default;
}

void UTF8_RenderForDos(const std::string &str_in, std::string &str_out,
                       const uint16_t code_page)
{
    const uint16_t cp = deduplicate_code_page(code_page);
    prepare_code_page(cp);

    std::vector<uint16_t> str_wide;
    utf8_to_wide(str_in, str_wide);

    str_out.clear();
    wide_to_code_page(str_wide, str_out, cp);
    str_out.shrink_to_fit();
}
