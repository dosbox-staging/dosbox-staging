/*
 *  Copyright (C) 2022-2025  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_KEYBOARD_H
#define DOSBOX_KEYBOARD_H

#include <cstdint>
#include <vector>

enum KBD_KEYS {
	// clang-format off
	KBD_NONE,

	KBD_1, KBD_2, KBD_3, KBD_4, KBD_5, KBD_6, KBD_7, KBD_8, KBD_9, KBD_0,
	KBD_q, KBD_w, KBD_e, KBD_r, KBD_t, KBD_y, KBD_u, KBD_i, KBD_o, KBD_p,
	KBD_a, KBD_s, KBD_d, KBD_f, KBD_g, KBD_h, KBD_j, KBD_k, KBD_l,
	KBD_z, KBD_x, KBD_c, KBD_v, KBD_b, KBD_n, KBD_m,

	KBD_f1,  KBD_f2,  KBD_f3,  KBD_f4,  KBD_f5,  KBD_f6,
	KBD_f7,  KBD_f8,  KBD_f9,  KBD_f10, KBD_f11, KBD_f12,

	KBD_esc, KBD_tab, KBD_backspace, KBD_enter, KBD_space,

	KBD_leftalt,   KBD_rightalt,
	KBD_leftctrl,  KBD_rightctrl,
	KBD_leftgui,   KBD_rightgui, // 'windows' keys
	KBD_leftshift, KBD_rightshift,

	KBD_capslock, KBD_scrolllock, KBD_numlock,

	KBD_grave, KBD_minus, KBD_equals, KBD_backslash,
	KBD_leftbracket, KBD_rightbracket,
	KBD_semicolon, KBD_quote,
	KBD_oem102, // usually between SHIFT and Z, has 2 or more symbols (|, \, <, >), depending on layout
	KBD_period, KBD_comma, KBD_slash, KBD_abnt1,

	KBD_printscreen, KBD_pause,

	KBD_insert, KBD_home, KBD_pageup,
	KBD_delete, KBD_end,  KBD_pagedown,
	
	KBD_left, KBD_up, KBD_down, KBD_right,

	KBD_kp1, KBD_kp2, KBD_kp3, KBD_kp4, KBD_kp5, KBD_kp6, KBD_kp7, KBD_kp8, KBD_kp9, KBD_kp0,
	KBD_kpdivide, KBD_kpmultiply, KBD_kpminus, KBD_kpplus,
	KBD_kpenter, KBD_kpperiod,

	KBD_LAST,

	// The keys below are not currently supported, thus they are placed
	// after KBD_LAST.
	// Note that our BIOS might be unable to handle the scancodes of these
	// keys correctly - this needs to be tested before the support is added.

	// Scancodes 1 and 2 for these keys were confirmed using the Microsoft
	// 'Digital Media Pro Keyboard':
	// - model 1031 (as printed on the box)
	// - model KC-0405 (as printed on the bottom side sticker)
	// - part number X809745-002 (as printed on the bottom side sticker)
	// If querried, the keyboard returns ID 0x41.

	KBD_guimenu,

	KBD_acpi_sleep, KBD_log_off,

	KBD_undo, KBD_redo, KBD_help,

	KBD_vol_mute, KBD_vol_up, KBD_vol_down,

	KBD_media_play, KBD_media_stop,
	KBD_media_prev, KBD_media_next,
	KBD_media_music, KBD_media_pictures,

	KBD_zoom_in, KBD_zoom_out,

	KBD_calculator, KBD_email, KBD_messenger, KBD_www_home,

	KBD_my_documents,

	KBD_new, KBD_open, KBD_close, KBD_save, KBD_print, KBD_spell,
	KBD_reply, KBD_forward, KBD_send,

	KBD_favorites, KBD_favorite1, KBD_favorite2,
	KBD_favorite3, KBD_favorite4, KBD_favorite5,

	// Scancodes below were taken from various sources - if possible, test
	// on real hardware before enabling them.

	KBD_acpi_power, KBD_acpi_wake,

	KBD_f13, KBD_f14, KBD_f15, KBD_f16, KBD_f17, KBD_f18,
	KBD_f19, KBD_f20, KBD_f21, KBD_f22, KBD_f23, KBD_f24,
	KBD_sys_req,

	KBD_intl1, KBD_intl2, KBD_intl4, KBD_intl5,
	KBD_katakana, KBD_fugirana, KBD_kanji, KBD_hiragana,

	KBD_cut, KBD_copy, KBD_paste,

	KBD_media_eject,
	KBD_media_select, // media_video according to some sources

	KBD_www_search, KBD_www_favorites, KBD_www_refresh,
	KBD_www_stop, KBD_www_forward, KBD_www_back,

	KBD_my_computer,

	// clang-format on
};

enum class ScanCode : uint8_t {
	None,
	AltEscape,
	AltSpace,
	ControlInsert = 0x04,
	ShiftInsert,
	ControlDelete,
	ShiftDelete,
	AltBackspace,
	AltShiftBackspace,
	ShiftTab = 0x0F,
	AltQ,
	AltW,
	AltE,
	AltR,
	AltT,
	AltY,
	AltU,
	AltI,
	AltO,
	AltP,
	AltOpenBracket,
	AltCloseBracket,
	AltA = 0x1E,
	AltS,
	AltD,
	AltF,
	AltG,
	AltH,
	AltJ,
	AltK,
	AltL,
	AltSemicolon,
	AltApostrophe,
	AltBacktick,
	AltBackslash = 0x2B,
	AltZ,
	AltX,
	AltC,
	AltV,
	AltB,
	AltN,
	AltM,
	AltComma,
	AltPeriod,
	AltSlash,
	AltNumpadAsterisk = 0x37,
	F1                = 0x3B,
	F2,
	F3,
	F4,
	F5,
	F6,
	F7,
	F8,
	F9,
	F10,
	Home = 0x47,
	Up,
	PageUp,
	Left = 0x4B,
	Center,
	Right,
	AltPlus,
	End,
	Down,
	PageDown,
	Insert,
	Delete,
	ShiftF1,
	ShiftF2,
	ShiftF3,
	ShiftF4,
	ShiftF5,
	ShiftF6,
	ShiftF7,
	ShiftF8,
	ShiftF9,
	ShiftF10,
	ControlF1,
	ControlF2,
	ControlF3,
	ControlF4,
	ControlF5,
	ControlF6,
	ControlF7,
	ControlF8,
	ControlF9,
	ControlF10,
	AltF1,
	AltF2,
	AltF3,
	AltF4,
	AltF5,
	AltF6,
	AltF7,
	AltF8,
	AltF9,
	AltF10,
	ControlPrintScreen,
	ControlLeft,
	ControlRight,
	ControlEnd,
	ControlPageDown,
	ControlHome,
	Alt1,
	Alt2,
	Alt3,
	Alt4,
	Alt5,
	Alt6,
	Alt7,
	Alt8,
	Alt9,
	Alt0,
	AltMinus,
	AltEquals,
	ControlPageUp,
	F11,
	F12,
	ShiftF11,
	ShiftF12,
	ControlF11,
	ControlF12,
	AltF11,
	AltF12,
	ControlUp,
	ControlMinus,
	ControlCenter,
	ControlPlus,
	ControlDown,
	ControlTab = 0x94,
	AltHome    = 0x97,
	AltUp,
	AltPageUp = 0x99,
	AltLeft   = 0x9B,
	AltRight  = 0x9D,
	AltEnd    = 0x9F,
	AltDown,
	AltPageDown,
	AltInsert,
	AltDelete,
	AltTab = 0xA5,
};

// After calling, it drops all the input until secure mode is enabled - safety
// measure to prevent malicious user from possibily interupting AUTOEXEC.BAT
// execution before it applies the secure mode
void KEYBOARD_WaitForSecureMode();

// Simulate key press or release
void KEYBOARD_AddKey(const KBD_KEYS key_type, const bool is_pressed);

// bit 0: scroll_lock, bit 1: num_lock, bit 2: caps_lock
// TODO: BIOS does not update LEDs as of yet
uint8_t KEYBOARD_GetLedState();

// Do not use KEYBOARD_ClrBuffer in new code, it can't clear everything!
void KEYBOARD_ClrBuffer();

// Keyboard scancode set 1 is required, always.
// Sets 2 and 3 are not tested yet.
// Set 3 was never widely adopted, several existing keyboards
// are said to have buggy implementations, and it seems it
// was never extended to cover the multimedia keys.

// #define ENABLE_SCANCODE_SET_2
// #define ENABLE_SCANCODE_SET_3

// Retrieve a scancode for the given key, for scancode set 1, 2, or 3
std::vector<uint8_t> KEYBOARD_GetScanCode1(const KBD_KEYS key_type,
                                           const bool is_pressed);
#ifdef ENABLE_SCANCODE_SET_2
std::vector<uint8_t> KEYBOARD_GetScanCode2(const KBD_KEYS key_type,
                                           const bool is_pressed);
#endif // ENABLE_SCANCODE_SET_2
#ifdef ENABLE_SCANCODE_SET_3
std::vector<uint8_t> KEYBOARD_GetScanCode3(const KBD_KEYS key_type,
                                           const bool is_pressed);
#endif // ENABLE_SCANCODE_SET_3

#endif // DOSBOX_KEYBOARD_H
