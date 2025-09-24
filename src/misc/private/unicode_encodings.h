// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_UNICODE_ENCODINGS_H
#define DOSBOX_UNICODE_ENCODINGS_H

#include <cstdint>
#include <string>

// Use the character below if there is no sane way to handle the character
constexpr uint8_t UnknownCharacter = 0x3f; // '?'

std::string wide_to_utf8(const std::u32string& str);
std::u32string utf8_to_wide(const std::string& str);

std::u32string utf16_to_wide(const std::u16string& str);
std::u16string wide_to_utf16(const std::u32string& str);

std::u32string ucs2_to_wide(const std::u16string& str);
std::u16string wide_to_ucs2(const std::u32string& str);

#endif // DOSBOX_UNICODE_ENCODINGS_H
