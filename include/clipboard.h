/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

#ifndef DOSBOX_CLIPBOARD_H
#define DOSBOX_CLIPBOARD_H

#include <cstdint>
#include <string>
#include <vector>

// Text clipboard support

bool clipboard_has_text();

void clipboard_copy_text_dos(const std::string &content);
void clipboard_copy_text_dos(const std::string &content,
                             const uint16_t code_page);
void clipboard_copy_text_utf8(const std::string &content);

std::string clipboard_paste_text_dos();
std::string clipboard_paste_text_dos(const uint16_t code_page);
std::string clipboard_paste_text_utf8();

// Binary clipboard support

bool clipboard_has_binary();
void clipboard_copy_binary(const std::vector<uint8_t> &content);
std::vector<uint8_t> clipboard_paste_binary();

#endif // DOSBOX_CLIPBOARD_H
