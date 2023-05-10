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

#include "tui_types.h"

#include "checks.h"
#include "logging.h"
#include "math_utils.h"
#include "string_utils.h"

#include <algorithm>
#include <set>

CHECK_NARROWING();


TuiCoordinates::TuiCoordinates(const int x, const int y) :
	x(clamp_to_uint8(x)),
	y(clamp_to_uint8(y))
{
}

bool TuiCoordinates::operator==(const TuiCoordinates &other) const
{
	return (x == other.x) && (y == other.y);
}

bool TuiCoordinates::operator!=(const TuiCoordinates &other) const
{
	return (x != other.x) || (y != other.y);
}

TuiCoordinates& TuiCoordinates::operator+=(const TuiCoordinates &other)
{
	x = clamp_to_uint8(x + other.x);
	y = clamp_to_uint8(y + other.y);
	return *this;
}

TuiCoordinates& TuiCoordinates::operator-=(const TuiCoordinates &other)
{
	x = clamp_to_uint8(x - other.x);
	y = clamp_to_uint8(y - other.y);
	return *this;
}

TuiCoordinates TuiCoordinates::operator+(const TuiCoordinates &other) const
{
	auto result = *this;
	result += other;
	return result;
}

TuiCoordinates TuiCoordinates::operator-(const TuiCoordinates &other) const
{
	auto result = *this;
	result -= other;
	return result;
}

bool TuiCell::operator==(const TuiCell &other) const
{
	return (screen_code == other.screen_code) &&
               (attributes == other.attributes);
}

bool TuiCell::operator!=(const TuiCell &other) const
{
	return (screen_code != other.screen_code) ||
	       (attributes != other.attributes);
}

uint8_t operator+(const TuiColor arg1, const TuiBgColor arg2)
{
	const auto result = static_cast<uint8_t>(arg1) +
	                    static_cast<uint8_t>(arg2);
	return clamp_to_uint8(result);
}


TuiKeyboardStatus::TuiKeyboardStatus()
{
}

TuiKeyboardStatus::TuiKeyboardStatus(const uint8_t data) :
	_data(data)
{
}

TuiKeyboardStatus::TuiKeyboardStatus(const TuiKeyboardStatus &other) :
	_data(other._data)
{
}

TuiKeyboardStatus& TuiKeyboardStatus::operator=(const uint8_t data)
{
	_data = data;
	return *this;
}

TuiKeyboardStatus& TuiKeyboardStatus::operator=(const TuiKeyboardStatus& other)
{
	_data = other._data;
	return *this;
}

bool TuiKeyboardStatus::operator==(const TuiKeyboardStatus &other) const
{
	return (_data == other._data);
}

bool TuiKeyboardStatus::operator!=(const TuiKeyboardStatus &other) const
{
	return (_data != other._data);
}

bool TuiKeyboardStatus::IsCapsLockActive() const
{
	return is_caps_lock_active;
}

bool TuiKeyboardStatus::IsNumLockActive() const
{
	return is_num_lock_active;
}

bool TuiKeyboardStatus::IsScrollLockActive() const
{
	return is_scroll_lock_active;
}

bool TuiKeyboardStatus::IsShiftPressed() const
{
	return is_left_shift_pressed || is_right_shift_pressed;
}

bool TuiKeyboardStatus::IsControlPressed() const
{
	return is_control_pressed;
}

bool TuiKeyboardStatus::IsAltPressed() const
{
	return is_alt_pressed;
}

TuiScanCode::TuiScanCode(const uint8_t bios_code,
                         const uint8_t ascii_code,
                         const TuiKeyboardStatus keyboard_status) :
	bios_code(bios_code),
	ascii_code(ascii_code),
	keyboard_status(keyboard_status)
{
	control_key  = DetermineIfControlKey();
	hot_key      = DetermineIfHotKey();
	is_printable = DetermineIfPrintable(); // has to be the last one!

	/* For testing purposes only
	LOG_INFO("TUI: key 0x%04x, printable: %s, %s%s%s",
		(bios_code << 8) + ascii_code,
		is_printable ? "Y" : "N",
		HasShift()   ? "SHIFT"   : "",
		HasControl() ? "CONTROL" : "",
		HasAlt()     ? "ALT"     : "");
	*/
}

TuiControlKey TuiScanCode::DetermineIfControlKey() const
{
	if (IsPrintable() || IsControlKey() || IsHotKey()) {
		return GetControlKey();
	}

	const auto full_code = (bios_code << 8) + ascii_code;

	if (ShouldDecodeWithoutModifiers()) {
		switch (full_code) {
		// Main block
		case 0x1c0d: return TuiControlKey::Enter;
		case 0x0f09: return TuiControlKey::Tabulation;
		case 0x0e08: return TuiControlKey::Backspace;
		case 0x011b: return TuiControlKey::Escape;
		case 0x48e0: return TuiControlKey::CursorUp;
		case 0x50e0: return TuiControlKey::CursorDown;
		case 0x4be0: return TuiControlKey::CursorLeft;
		case 0x4de0: return TuiControlKey::CursorRight;
		case 0x52e0: return TuiControlKey::Insert;
		case 0x53e0: return TuiControlKey::Delete;
		case 0x47e0: return TuiControlKey::Home;
		case 0x4fe0: return TuiControlKey::End;
		case 0x49e0: return TuiControlKey::PageUp;
		case 0x51e0: return TuiControlKey::PageDown;
		case 0x7200: return TuiControlKey::PrintScreen;
		// Keypad
		case 0xe00d: return TuiControlKey::Enter;
		case 0x4800: return TuiControlKey::CursorUp;
		case 0x5000: return TuiControlKey::CursorDown;
		case 0x4b00: return TuiControlKey::CursorLeft;
		case 0x4d00: return TuiControlKey::CursorRight;
		case 0x5200: return TuiControlKey::Insert;
		case 0x5300: return TuiControlKey::Delete;
		case 0x4700: return TuiControlKey::Home;
		case 0x4f00: return TuiControlKey::End;
		case 0x4900: return TuiControlKey::PageUp;
		case 0x5100: return TuiControlKey::PageDown;
		default: break; // Not a supported control key
		}
	} else if (ShouldDecodeWithShift()) {
		switch (full_code) {
		// Main block
		case 0x1c0d: return TuiControlKey::ShiftEnter;
		case 0x0f00: return TuiControlKey::ShiftTabulation;
		case 0x0e08: return TuiControlKey::ShiftBackspace;
		case 0x011b: return TuiControlKey::ShiftEscape;
		case 0x48e0: return TuiControlKey::ShiftCursorUp;
		case 0x50e0: return TuiControlKey::ShiftCursorDown;
		case 0x4be0: return TuiControlKey::ShiftCursorLeft;
		case 0x4de0: return TuiControlKey::ShiftCursorRight;
		case 0x52e0: return TuiControlKey::ShiftInsert;
		case 0x53e0: return TuiControlKey::ShiftDelete;
		case 0x47e0: return TuiControlKey::ShiftHome;
		case 0x4fe0: return TuiControlKey::ShiftEnd;
		case 0x49e0: return TuiControlKey::ShiftPageUp;
		case 0x51e0: return TuiControlKey::ShiftPageDown;
		// Keypad
		case 0xe00d: return TuiControlKey::ShiftEnter;
		case 0x4838: return TuiControlKey::ShiftCursorUp;
		case 0x5032: return TuiControlKey::ShiftCursorDown;
		case 0x4b34: return TuiControlKey::ShiftCursorLeft;
		case 0x4d36: return TuiControlKey::ShiftCursorRight;
		case 0x5230: return TuiControlKey::ShiftInsert;
		case 0x532e: return TuiControlKey::ShiftDelete;
		case 0x4737: return TuiControlKey::ShiftHome;
		case 0x4f31: return TuiControlKey::ShiftEnd;
		case 0x4939: return TuiControlKey::ShiftPageUp;
		case 0x5133: return TuiControlKey::ShiftPageDown;
		default: break; // Not a supported control key
		} 
	} if (ShouldDecodeWithControl()) {
		switch (full_code) {
		// Main block
		case 0x1c0a: return TuiControlKey::ControlEnter;
		case 0x9400: return TuiControlKey::ControlTabulation;
		case 0x0e7f: return TuiControlKey::ControlBackspace;
		case 0x8de0: return TuiControlKey::ControlCursorUp;
		case 0x91e0: return TuiControlKey::ControlCursorDown;
		case 0x73e0: return TuiControlKey::ControlCursorLeft;
		case 0x74e0: return TuiControlKey::ControlCursorRight;
		case 0x92e0: return TuiControlKey::ControlInsert;
		case 0x93e0: return TuiControlKey::ControlDelete;
		case 0x77e0: return TuiControlKey::ControlHome;
		case 0x75e0: return TuiControlKey::ControlEnd;
		case 0x84e0: return TuiControlKey::ControlPageUp;
		case 0x76e0: return TuiControlKey::ControlPageDown;
		// Keypad
		case 0xe00a: return TuiControlKey::ControlEnter;
		case 0x8d00: return TuiControlKey::ControlCursorUp;
		case 0x9100: return TuiControlKey::ControlCursorDown;
		case 0x7300: return TuiControlKey::ControlCursorLeft;
		case 0x7400: return TuiControlKey::ControlCursorRight;
		case 0x9200: return TuiControlKey::ControlInsert;
		case 0x9300: return TuiControlKey::ControlDelete;
		case 0x7700: return TuiControlKey::ControlHome;
		case 0x7500: return TuiControlKey::ControlEnd;
		case 0x8400: return TuiControlKey::ControlPageUp;
		case 0x7600: return TuiControlKey::ControlPageDown;
		default: break; // Not a supported control key
		}
	} if (ShouldDecodeWithAlt()) {
		switch (full_code) {
		// Main block
		case 0xa600: return TuiControlKey::AltEnter;
		case 0xa500: return TuiControlKey::AltTabulation;
		case 0x0e00: return TuiControlKey::AltBackspace;
		case 0x0100: return TuiControlKey::AltEscape;
		case 0x9800: return TuiControlKey::AltCursorUp;
		case 0xa000: return TuiControlKey::AltCursorDown;
		case 0x9b00: return TuiControlKey::AltCursorLeft;
		case 0x9d00: return TuiControlKey::AltCursorRight;
		case 0xa200: return TuiControlKey::AltInsert;
		case 0xa300: return TuiControlKey::AltDelete;
		case 0x9700: return TuiControlKey::AltHome;
		case 0x9f00: return TuiControlKey::AltEnd;
		case 0x9900: return TuiControlKey::AltPageUp;
		case 0xa100: return TuiControlKey::AltPageDown;
		// Keypad is not reported by BIOS if 'Alt' key is pressed
		default: break; // Not a supported control key
		}
	} else if (ShouldDecodeWithShiftControl()) {
		switch (full_code) {
		// Main block
		case 0x8de0: return TuiControlKey::ShiftControlCursorUp;
		case 0x91e0: return TuiControlKey::ShiftControlCursorDown;
		case 0x73e0: return TuiControlKey::ShiftControlCursorLeft;
		case 0x74e0: return TuiControlKey::ShiftControlCursorRight;
		case 0x92e0: return TuiControlKey::ShiftControlInsert;
		case 0x93e0: return TuiControlKey::ShiftControlDelete;
		case 0x77e0: return TuiControlKey::ShiftControlHome;
		case 0x75e0: return TuiControlKey::ShiftControlEnd;
		case 0x84e0: return TuiControlKey::ShiftControlPageUp;
		case 0x76e0: return TuiControlKey::ShiftControlPageDown;
		// Keypad
		case 0x8d00: return TuiControlKey::ShiftControlCursorUp;
		case 0x9100: return TuiControlKey::ShiftControlCursorDown;
		case 0x7300: return TuiControlKey::ShiftControlCursorLeft;
		case 0x7400: return TuiControlKey::ShiftControlCursorRight;
		case 0x9200: return TuiControlKey::ShiftControlInsert;
		case 0x9300: return TuiControlKey::ShiftControlDelete;
		case 0x7700: return TuiControlKey::ShiftControlHome;
		case 0x7500: return TuiControlKey::ShiftControlEnd;
		case 0x8400: return TuiControlKey::ShiftControlPageUp;
		case 0x7600: return TuiControlKey::ShiftControlPageDown;
		default: break; // Not a supported control key
		}
	}

	return TuiControlKey::None;
}

TuiHotKey TuiScanCode::DetermineIfHotKey() const
{
	if (IsPrintable() || IsControlKey() || IsHotKey()) {
		return GetHotKey();
	}

	const auto full_code = (bios_code << 8) + ascii_code;
	if (ShouldDecodeWithoutModifiers()) {
		switch (full_code) {
		// Function keys
		case 0x3b00: return TuiHotKey::F1;
		case 0x3c00: return TuiHotKey::F2;
		case 0x3d00: return TuiHotKey::F3;
		case 0x3e00: return TuiHotKey::F4;
		case 0x3f00: return TuiHotKey::F5;
		case 0x4000: return TuiHotKey::F6;
		case 0x4100: return TuiHotKey::F7;
		case 0x4200: return TuiHotKey::F8;
		case 0x4300: return TuiHotKey::F9;
		case 0x4400: return TuiHotKey::F10;
		case 0x8500: return TuiHotKey::F11;
		case 0x8600: return TuiHotKey::F12;
		default: break; // Not a supported hotkey
		}
	} else if (ShouldDecodeWithShift()) {
		switch (full_code) {
		// Function keys
		case 0x5400: return TuiHotKey::Shift_F1;
		case 0x5500: return TuiHotKey::Shift_F2;
		case 0x5600: return TuiHotKey::Shift_F3;
		case 0x5700: return TuiHotKey::Shift_F4;
		case 0x5800: return TuiHotKey::Shift_F5;
		case 0x5900: return TuiHotKey::Shift_F6;
		case 0x5a00: return TuiHotKey::Shift_F7;
		case 0x5b00: return TuiHotKey::Shift_F8;
		case 0x5c00: return TuiHotKey::Shift_F9;
		case 0x5d00: return TuiHotKey::Shift_F10;
		case 0x8700: return TuiHotKey::Shift_F11;
		case 0x8800: return TuiHotKey::Shift_F12;
		default: break; // Not a supported hotkey
		}
	} else if (ShouldDecodeWithControl()) {
		switch (full_code) {
		// Letters
		case 0x1e01: return TuiHotKey::Control_A;
		case 0x3002: return TuiHotKey::Control_B;
		case 0x2e03: return TuiHotKey::Control_C;
		case 0x2004: return TuiHotKey::Control_D;
		case 0x1205: return TuiHotKey::Control_E;
		case 0x2106: return TuiHotKey::Control_F;
		case 0x2207: return TuiHotKey::Control_G;
		case 0x2308: return TuiHotKey::Control_H;
		case 0x1709: return TuiHotKey::Control_I;
		case 0x240a: return TuiHotKey::Control_J;
		case 0x250b: return TuiHotKey::Control_K;
		case 0x260c: return TuiHotKey::Control_L;
		case 0x320d: return TuiHotKey::Control_M;
		case 0x310e: return TuiHotKey::Control_N;
		case 0x180f: return TuiHotKey::Control_O;
		case 0x1910: return TuiHotKey::Control_P;
		case 0x1011: return TuiHotKey::Control_Q;
		case 0x1312: return TuiHotKey::Control_R;
		case 0x1f13: return TuiHotKey::Control_S;
		case 0x1414: return TuiHotKey::Control_T;
		case 0x1615: return TuiHotKey::Control_U;
		case 0x2f16: return TuiHotKey::Control_V;
		case 0x1117: return TuiHotKey::Control_W;
		case 0x2d18: return TuiHotKey::Control_X;
		case 0x1519: return TuiHotKey::Control_Y;
		case 0x2c1a: return TuiHotKey::Control_Z;
		// Function keys
		case 0x5e00: return TuiHotKey::Control_F1;
		case 0x5f00: return TuiHotKey::Control_F2;
		case 0x6000: return TuiHotKey::Control_F3;
		case 0x6100: return TuiHotKey::Control_F4;
		case 0x6200: return TuiHotKey::Control_F5;
		case 0x6300: return TuiHotKey::Control_F6;
		case 0x6400: return TuiHotKey::Control_F7;
		case 0x6500: return TuiHotKey::Control_F8;
		case 0x6600: return TuiHotKey::Control_F9;
		case 0x6700: return TuiHotKey::Control_F10;
		case 0x8900: return TuiHotKey::Control_F11;
		case 0x8a00: return TuiHotKey::Control_F12;
		// Symbols
		case 0x1a1b: return TuiHotKey::Control_LeftBracket;
		case 0x1b1d: return TuiHotKey::Control_RightBracket;
		default: break; // Not a supported hotkey
		}
	} else if (ShouldDecodeWithAlt()) {
		switch (full_code) {
		// Letters
		case 0x1e00: return TuiHotKey::Alt_A;
		case 0x3000: return TuiHotKey::Alt_B;
		case 0x2e00: return TuiHotKey::Alt_C;
		case 0x2000: return TuiHotKey::Alt_D;
		case 0x1200: return TuiHotKey::Alt_E;
		case 0x2100: return TuiHotKey::Alt_F;
		case 0x2200: return TuiHotKey::Alt_G;
		case 0x2300: return TuiHotKey::Alt_H;
		case 0x1700: return TuiHotKey::Alt_I;
		case 0x2400: return TuiHotKey::Alt_J;
		case 0x2500: return TuiHotKey::Alt_K;
		case 0x2600: return TuiHotKey::Alt_L;
		case 0x3200: return TuiHotKey::Alt_M;
		case 0x3100: return TuiHotKey::Alt_N;
		case 0x1800: return TuiHotKey::Alt_O;
		case 0x1900: return TuiHotKey::Alt_P;
		case 0x1000: return TuiHotKey::Alt_Q;
		case 0x1300: return TuiHotKey::Alt_R;
		case 0x1f00: return TuiHotKey::Alt_S;
		case 0x1400: return TuiHotKey::Alt_T;
		case 0x1600: return TuiHotKey::Alt_U;
		case 0x2f00: return TuiHotKey::Alt_V;
		case 0x1100: return TuiHotKey::Alt_W;
		case 0x2d00: return TuiHotKey::Alt_X;
		case 0x1500: return TuiHotKey::Alt_Y;
		case 0x2c00: return TuiHotKey::Alt_Z;
		// Digits
		case 0x8100: return TuiHotKey::Alt_0;
		case 0x7800: return TuiHotKey::Alt_1;
		case 0x7900: return TuiHotKey::Alt_2;
		case 0x7a00: return TuiHotKey::Alt_3;
		case 0x7b00: return TuiHotKey::Alt_4;
		case 0x7c00: return TuiHotKey::Alt_5;
		case 0x7d00: return TuiHotKey::Alt_6;
		case 0x7e00: return TuiHotKey::Alt_7;
		case 0x7f00: return TuiHotKey::Alt_8;
		case 0x8000: return TuiHotKey::Alt_9;
		// Function keys
		case 0x6800: return TuiHotKey::Alt_F1;
		case 0x6900: return TuiHotKey::Alt_F2;
		case 0x6a00: return TuiHotKey::Alt_F3;
		case 0x6b00: return TuiHotKey::Alt_F4;
		case 0x6c00: return TuiHotKey::Alt_F5;
		case 0x6d00: return TuiHotKey::Alt_F6;
		case 0x6e00: return TuiHotKey::Alt_F7;
		case 0x6f00: return TuiHotKey::Alt_F8;
		case 0x7000: return TuiHotKey::Alt_F9;
		case 0x7100: return TuiHotKey::Alt_F10;
		case 0x8b00: return TuiHotKey::Alt_F11;
		case 0x8c00: return TuiHotKey::Alt_F12;
		// Symbols
		case 0x8300: return TuiHotKey::Alt_Plus;  // main block
		case 0x8200: return TuiHotKey::Alt_Minus; // main block
		case 0x4e00: return TuiHotKey::Alt_Plus;  // keypad
		case 0x4a00: return TuiHotKey::Alt_Minus; // keypad
		case 0x1a00: return TuiHotKey::Alt_LeftBracket;
		case 0x1b00: return TuiHotKey::Alt_RightBracket;
		default: break; // Not a supported hotkey
		}
	} else if (ShouldDecodeWithShiftControl()) {
		switch (full_code) {
		case 0x8300: return TuiHotKey::Alt_Plus;  // main block
		case 0x8200: return TuiHotKey::Alt_Minus; // main block
		default: break; // Not a supported hotkey
		}
	}

	return TuiHotKey::None;
}

bool TuiScanCode::DetermineIfPrintable() const
{
	if (IsPrintable() || IsControlKey() || IsHotKey()) {
		return IsPrintable();
	}

	// Check whether this represents a printable character scan code,
	// based on PC Sourcebook, 2nd edition, table 7.014

	static constexpr std::pair<uint8_t, uint8_t> printable_ranges[] = {
		{0x02, 0x0d},
		{0x10, 0x1b},
		{0x1e, 0x29},
		{0x2b, 0x35},
		{0x39, 0x39},
	};

	auto is_printable_bios_code = [this]() {
		auto range_check = [this](const auto& range) {
			return bios_code >= range.first &&
		               bios_code <= range.second;
		};
		return std::any_of(std::begin(printable_ranges),
		                   std::end(printable_ranges),
		                   range_check);
	};

	auto is_printable_ascii_code = [this]() {
		const auto code = static_cast<char>(ascii_code);
		return is_extended_printable_ascii(code);
	};

	static const std::set<uint16_t> keypad_full_codes = {
		// clang-format off
		0x5230, 0x4f31, 0x5032, 0x5133, 0x4b34, // 0-4
		0x4c35, 0x4d36, 0x4737, 0x4838, 0x4939, // 5-9
		0x4e2b, 0x4a2d, 0x372a, 0xe02f, 0x532e, // +-*/.
		// clang-format on
	};

	auto is_printable_keypad_code = [this]() {
		// XXX check NumLock status
		const auto full_code = (bios_code << 8) + ascii_code;
		return keypad_full_codes.count(static_cast<uint16_t>(full_code)) != 0;
	};

	return (is_printable_ascii_code() && is_printable_bios_code()) ||
	       is_printable_keypad_code();
}

bool TuiScanCode::ShouldDecodeWithShift() const
{
	return HasShift() && !HasControl() && !HasAlt();
}

bool TuiScanCode::ShouldDecodeWithControl() const
{
	return !HasShift() && HasControl() && !HasAlt();
}

bool TuiScanCode::ShouldDecodeWithAlt() const
{
	return !HasShift() && !HasControl() && HasAlt();
}

bool TuiScanCode::ShouldDecodeWithShiftControl() const
{
	return HasShift() && HasControl() && !HasAlt();
}

bool TuiScanCode::ShouldDecodeWithoutModifiers() const
{
	return !HasShift() && !HasControl() && !HasAlt();
}

char TuiScanCode::GetPrintableChar() const
{
	if (!is_printable) {
		return 0;
	}
	return static_cast<char>(ascii_code);
}

uint8_t TuiScanCode::GetPrintableCode() const
{
	if (!is_printable) {
		return 0;
	}
	return ascii_code;
}
