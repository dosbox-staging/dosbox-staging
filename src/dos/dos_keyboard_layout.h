/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2025  The DOSBox Staging Team
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

	// Could not open the CPI file
	CpiFileNotFound,
	// I/O error while reading the file
	CpiReadError,
	// Wrong CPI file format
	InvalidCpiFile,
	// CPI file size over the internal limit
	CpiFileTooLarge,
	// Unsupported CPI file format (FreeDOS)
	UnsupportedCpxFile,
	// The CPI file is meant to be used with printers
	PrinterCpiFile,
	// Code page found, but the screen font is unusable
	ScreenFontUnusable,
	// Code page not known
	NoBundledCpiFileForCodePage,
	// No code page in user-supplied CPI file
	NoCodePageInCpiFile,
	// Pre-EGA machines can't change the code page
	IncompatibleMachine,

	// Could not open the keyboard SYS file
	LayoutFileNotFound,
	// Wrong file format
	InvalidLayoutFile,
	// Keyboard layout not known
	LayoutNotKnown,
	// Keyboard layout does not support code page
	NoLayoutForCodePage,
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

#endif
