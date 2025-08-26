// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos_keyboard_layout.h"

#ifndef DOSBOX_DOS_CODE_PAGE_H
#define DOSBOX_DOS_CODE_PAGE_H

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

#endif // DOSBOX_DOS_CODE_PAGE_H
