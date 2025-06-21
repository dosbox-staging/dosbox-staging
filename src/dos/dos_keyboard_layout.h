// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

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
