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

#include "dos_keyboard_layout.h"

#ifndef DOSBOX_SCREEN_FONTS_H
#define DOSBOX_SCREEN_FONTS_H

// Returns 'true' if the emulated hardware allows to change the screen font
bool DOS_CanLoadScreenFonts();

// Loads and sets the screen font from the DOS file or (if no file name given)
// from one of the bundled CPI files.
// Only to be called from within the keyboard layout handling code!
KeyboardLayoutResult DOS_LoadScreenFont(const uint16_t code_page,
                                        const std::string& file_name = {});

// Restores the standard ROM screen font.
// Only to be called from within the keyboard layout handling code!
void DOS_SetRomScreenFont();

#endif // DOSBOX_SCREEN_FONTS_H
