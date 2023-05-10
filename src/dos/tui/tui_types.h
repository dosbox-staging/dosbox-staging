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

#ifndef DOSBOX_TUI_TYPES_H
#define DOSBOX_TUI_TYPES_H

#include "bit_view.h"

#include <cstdint>
#include <utility>


struct TuiCoordinates {
	TuiCoordinates() {}
	TuiCoordinates(const int x, const int y);

	bool operator==(const TuiCoordinates &other) const;
	bool operator!=(const TuiCoordinates &other) const;

	TuiCoordinates& operator+=(const TuiCoordinates &other);
	TuiCoordinates& operator-=(const TuiCoordinates &other);

	TuiCoordinates operator+(const TuiCoordinates &other) const;
	TuiCoordinates operator-(const TuiCoordinates &other) const;

	uint8_t x = 0;
	uint8_t y = 0;
};

struct TuiCell {
	bool operator==(const TuiCell &other) const;
	bool operator!=(const TuiCell &other) const;

	uint8_t screen_code = 0;
	uint8_t attributes  = 0;
};

enum class TuiColor : uint8_t {
	Black        = 0x00,
	Blue         = 0x01,
	Green        = 0x02,
	Cyan         = 0x03,
	Red          = 0x04,
	Magenta      = 0x05,
	Brown        = 0x06,
	White        = 0x07,
	Gray         = 0x08,
	LightBlue    = 0x09,
	LightGreen   = 0x0a,
	LightCyan    = 0x0b,
	LightRed     = 0x0c,
	LightMagenta = 0x0d,
	Yellow       = 0x0e,
	BrightWhite  = 0x0f,
};

enum class TuiBgColor : uint8_t {
	Black        = 0x00,
	Blue         = 0x10,
	Green        = 0x20,
	Cyan         = 0x30,
	Red          = 0x40,
	Magenta      = 0x50,
	Brown        = 0x60,
	White        = 0x70,
};

uint8_t operator+(const TuiColor arg1, const TuiBgColor arg2);

enum class TuiHotKey : uint8_t {
	None,
	// clang-format off
	Control_A, Control_B, Control_C, Control_D, Control_E, Control_F,
	Control_G, Control_H, Control_I, Control_J, Control_K, Control_L,
	Control_M, Control_N, Control_O, Control_P, Control_Q, Control_R,
	Control_S, Control_T, Control_U, Control_V, Control_W, Control_X,
	Control_Y, Control_Z,

	Alt_A, Alt_B, Alt_C, Alt_D, Alt_E, Alt_F, Alt_G, Alt_H, Alt_I, Alt_J,
	Alt_K, Alt_L, Alt_M, Alt_N, Alt_O, Alt_P, Alt_Q, Alt_R, Alt_S, Alt_T,
	Alt_U, Alt_V, Alt_W, Alt_X, Alt_Y, Alt_Z,

	Alt_0, Alt_1, Alt_2, Alt_3, Alt_4, Alt_5, Alt_6, Alt_7, Alt_8, Alt_9,

	F1,  Shift_F1,  Control_F1,  Alt_F1,
	F2,  Shift_F2,  Control_F2,  Alt_F2,
	F3,  Shift_F3,  Control_F3,  Alt_F3,
	F4,  Shift_F4,  Control_F4,  Alt_F4,
	F5,  Shift_F5,  Control_F5,  Alt_F5,
	F6,  Shift_F6,  Control_F6,  Alt_F6,
	F7,  Shift_F7,  Control_F7,  Alt_F7,
	F8,  Shift_F8,  Control_F8,  Alt_F8,
	F9,  Shift_F9,  Control_F9,  Alt_F9,
	F10, Shift_F10, Control_F10, Alt_F10,
	F11, Shift_F11, Control_F11, Alt_F11,
	F12, Shift_F12, Control_F12, Alt_F12,

	Alt_Plus, Alt_Minus,
	
	Control_LeftBracket, Control_RightBracket,
	Alt_LeftBracket, Alt_RightBracket,
	// clang-format on
};

enum class TuiControlKey : uint8_t {
	None,
	// clang-format off
	Enter,       ShiftEnter,       ControlEnter,       AltEnter,
	Tabulation,  ShiftTabulation,  ControlTabulation,  AltTabulation,
	Backspace,   ShiftBackspace,   ControlBackspace,   AltBackspace,
	Escape,      ShiftEscape,      /* XXX */           AltEscape,

	CursorUp,    ShiftCursorUp,    ControlCursorUp,    AltCursorUp,
	CursorDown,  ShiftCursorDown,  ControlCursorDown,  AltCursorDown,
	CursorLeft,  ShiftCursorLeft,  ControlCursorLeft,  AltCursorLeft,
	CursorRight, ShiftCursorRight, ControlCursorRight, AltCursorRight,

	Insert,      ShiftInsert,      ControlInsert,      AltInsert,
	Delete,      ShiftDelete,      ControlDelete,      AltDelete,
	Home,        ShiftHome,        ControlHome,        AltHome,
	End,         ShiftEnd,         ControlEnd,         AltEnd,
	PageUp,      ShiftPageUp,      ControlPageUp,      AltPageUp,
	PageDown,    ShiftPageDown,    ControlPageDown,    AltPageDown,

	ShiftControlCursorUp,   ShiftControlCursorDown,
	ShiftControlCursorLeft, ShiftControlCursorRight,
	ShiftControlInsert,     ShiftControlDelete,
	ShiftControlHome,       ShiftControlEnd,
	ShiftControlPageUp,     ShiftControlPageDown,

	PrintScreen, /* XXX */         /* XXX */           /* XXX */
	// clang-format on
};

class TuiKeyboardStatus final {
public:
	TuiKeyboardStatus();
	TuiKeyboardStatus(const uint8_t data);
	TuiKeyboardStatus(const TuiKeyboardStatus &other);

	TuiKeyboardStatus& operator=(const uint8_t data);
	TuiKeyboardStatus& operator=(const TuiKeyboardStatus& other);

	bool operator==(const TuiKeyboardStatus &other) const;
	bool operator!=(const TuiKeyboardStatus &other) const;

	bool IsCapsLockActive() const;
	bool IsNumLockActive() const;
	bool IsScrollLockActive() const;

	bool IsShiftPressed() const;
	bool IsControlPressed() const;
	bool IsAltPressed() const;

private:
	union {
		uint8_t _data = 0;

		bit_view<0, 1> is_right_shift_pressed;
		bit_view<1, 1> is_left_shift_pressed;
		bit_view<2, 1> is_control_pressed;
		bit_view<3, 1> is_alt_pressed;
		bit_view<4, 1> is_scroll_lock_active;
		bit_view<5, 1> is_num_lock_active;
		bit_view<6, 1> is_caps_lock_active;
		bit_view<7, 1> is_insert_active;
	};
};

class TuiScanCode final
{
public:
	TuiScanCode(const uint8_t bios_code,
	            const uint8_t ascii_code,
	            const TuiKeyboardStatus keyboard_status);

	bool IsControlKey() const { return control_key != TuiControlKey::None; }
	TuiControlKey GetControlKey() const { return control_key; }
	
	bool IsHotKey() const { return hot_key != TuiHotKey::None; }
	TuiHotKey GetHotKey() const { return hot_key; }

	bool IsPrintable() const { return is_printable; }
	char GetPrintableChar() const;
	uint8_t GetPrintableCode() const;

	bool HasShift() const { return keyboard_status.IsShiftPressed(); }
	bool HasControl() const { return keyboard_status.IsControlPressed(); }
	bool HasAlt() const { return keyboard_status.IsAltPressed(); }

private:
	bool DetermineIfPrintable() const;
	TuiControlKey DetermineIfControlKey() const;
	TuiHotKey DetermineIfHotKey() const;

	bool ShouldDecodeWithShift() const;
	bool ShouldDecodeWithControl() const;
	bool ShouldDecodeWithAlt() const;
	bool ShouldDecodeWithShiftControl() const;
	bool ShouldDecodeWithoutModifiers() const;

	uint8_t bios_code  = 0;
	uint8_t ascii_code = 0;

	TuiKeyboardStatus keyboard_status = {};

	TuiControlKey control_key = TuiControlKey::None;
	TuiHotKey hot_key = TuiHotKey::None;

	bool is_printable = false;
};

enum class TuiCursor : uint8_t {
	Hidden,
	Normal,
	Block
};


#endif // DOSBOX_TUI_TYPES_H
