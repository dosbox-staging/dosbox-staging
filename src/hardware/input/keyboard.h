// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_KEYBOARD_H
#define DOSBOX_KEYBOARD_H

#include <cstdint>
#include <vector>

#include "config/setup.h"

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

	// If you intend to add multimedia keyboard scancodes, please check the
	// 'README.md' from the implementation directory for the list of known
	// scancodes.

	KBD_LAST,

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

void KEYBOARD_Init(Section* sec);

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
