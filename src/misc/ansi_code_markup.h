// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_ANSI_CODE_MARKUP_H
#define DOSBOX_ANSI_CODE_MARKUP_H

#include <string>

// Convert marked up strings to strings with ANSI codes
std::string convert_ansi_markup(const char* str);
std::string convert_ansi_markup(const std::string& str);

std::string strip_ansi_markup(const std::string& str);

// Pre-defined markups to help creating strings
namespace Ansi {
	extern const std::string Reset;

	// Low intensity text colours
	extern const std::string ColorBlack;
	extern const std::string ColorBlue;
	extern const std::string ColorGreen;
	extern const std::string ColorCyan;
	extern const std::string ColorRed;
	extern const std::string ColorMagenta;
	extern const std::string ColorBrown;
	extern const std::string ColorLightGray;

	// High intensity text colours
	extern const std::string ColorDarkGray;
	extern const std::string ColorLightBlue;
	extern const std::string ColorLightGreen;
	extern const std::string ColorLightCyan;
	extern const std::string ColorLightRed;
	extern const std::string ColorLightMagenta;
	extern const std::string ColorYellow;
	extern const std::string ColorWhite;

	// Definitions to help keeping the command output style consistent
	extern const std::string& HighlightHeader;
	extern const std::string& HighlightSelection;

	// TODO: Rework the whole code to use definitions from this header
}

#endif // DOSBOX_ANSI_CODE_MARKUP_H
