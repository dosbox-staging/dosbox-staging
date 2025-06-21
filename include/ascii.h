// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_ASCII_H
#define DOSBOX_ASCII_H

#include <cstdint>

namespace Ascii {

enum ControlCode : uint8_t {
	// Control characters
	Null           = 0x00,
	CtrlC          = 0x03,
	Backspace      = 0x08,
	LineFeed       = 0x0a,
	FormFeed       = 0x0c,
	CarriageReturn = 0x0d,
	Escape         = 0x1b,
	Delete         = 0x7f,
	Extended       = 0xe0,
};

}

#endif
