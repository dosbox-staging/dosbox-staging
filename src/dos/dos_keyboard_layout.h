/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
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

#ifndef DOSBOX_DOS_KEYBOARD_LAYOUT_H
#define DOSBOX_DOS_KEYBOARD_LAYOUT_H

#include "dosbox.h"

enum class KeyboardLayoutResult {
	OK,
	// Could not open the keyboard SYS file
	LayoutFileNotFound,
	// Wrong file format
	InvalidLayoutFile,
	// Could not open the CPI file
	CpiFileNotFound,
	// Wrong CPP file format
	InvalidCpiFile,
	// Unsupported CPI file format (DR-DOS)
	UnsupportedCpiFileDrDos,
	// Keyboard layout not known
	LayoutNotKnown,
	// Keyboard layout does not support code page
	NoLayoutForCodePage,
	// Code page not known
	NoBundledCpiFileForCodePage,
	// No code page in user-supplied CPI file
	NoCodePageInCpiFile,
	// Pre-EGA machines can't change the code page
	IncompatibleMachine,
};

enum class CodePageFontOrigin {
	Unknown,
	// From graphics adapter ROM
	Rom,
	// From one of the bundled CPI files
	Bundled,
	// From user provided CPI file
	Custom,
};

// Tries to load a keyboard layout and a code page.
// If 'code_page' is 0, tries to load the default one for the given layout.
// If 'cpi_file' is empty, tries  to use one of the bundled ones.
// Uses 'code_page' to return code page it tried to load.
KeyboardLayoutResult DOS_LoadKeyboardLayout(const std::string& keyboard_layout,
                                            uint16_t& code_page,
                                            const std::string& cpi_file = {},
                                            const bool prefer_rom_font = false);

std::string DOS_GetLoadedLayout();
CodePageFontOrigin DOS_GetCodePageFontOrigin();

#endif
