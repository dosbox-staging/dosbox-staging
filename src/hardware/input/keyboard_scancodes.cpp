// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2022 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "keyboard.h"

#include "dosbox.h"
#include "utils/checks.h"

CHECK_NARROWING();

// Reference:
// - http://www.quadibloc.com/comp/scan.htm
// - https://wiki.osdev.org/PS/2_Keyboard
// - https://stanislavs.org/helppc/make_codes.html
// - https://kbd-project.org/docs/scancodes/scancodes.html
// - https://homepages.cwi.nl/~aeb/linux/kbd/scancodes.html
// - https://deskthority.net/wiki/Scancode
// - 'Keyboard Scan Code Specification' by Microsoft
// Values for code set 3 taken mainly from:
// - http://www.computer-engineering.org/ps2keyboard/scancodes3.html,
//   access via web.archive.org

// Reserved codes:
//
// 0x00         key detection error or internal buffer overflow - for set 1
// 0xaa         keyboard self test passed
// 0xee         echo response
// 0xfa         acknowledge
// 0xfc, 0xfd   keyboard self test failed
// 0xfe         resend request
// 0xff         key detection error or internal buffer overflow - for sets 2 & 3

std::vector<uint8_t> KEYBOARD_GetScanCode1(const KBD_KEYS key_type,
                                           const bool is_pressed)
{
	uint8_t code = 0x00;
	bool extend  = false;

	switch (key_type) {
	// clang-format off

        case KBD_esc:            code = 0x01; break;
        case KBD_1:              code = 0x02; break;
        case KBD_2:              code = 0x03; break;
        case KBD_3:              code = 0x04; break;
        case KBD_4:              code = 0x05; break;
        case KBD_5:              code = 0x06; break;
        case KBD_6:              code = 0x07; break;
        case KBD_7:              code = 0x08; break;
        case KBD_8:              code = 0x09; break;
        case KBD_9:              code = 0x0a; break;
        case KBD_0:              code = 0x0b; break;

        case KBD_minus:          code = 0x0c; break;
        case KBD_equals:         code = 0x0d; break;
        case KBD_backspace:      code = 0x0e; break;
        case KBD_tab:            code = 0x0f; break;

        case KBD_q:              code = 0x10; break;
        case KBD_w:              code = 0x11; break;
        case KBD_e:              code = 0x12; break;
        case KBD_r:              code = 0x13; break;
        case KBD_t:              code = 0x14; break;
        case KBD_y:              code = 0x15; break;
        case KBD_u:              code = 0x16; break;
        case KBD_i:              code = 0x17; break;
        case KBD_o:              code = 0x18; break;
        case KBD_p:              code = 0x19; break;

        case KBD_leftbracket:    code = 0x1a; break;
        case KBD_rightbracket:   code = 0x1b; break;
        case KBD_enter:          code = 0x1c; break;
        case KBD_leftctrl:       code = 0x1d; break;

        case KBD_a:              code = 0x1e; break;
        case KBD_s:              code = 0x1f; break;
        case KBD_d:              code = 0x20; break;
        case KBD_f:              code = 0x21; break;
        case KBD_g:              code = 0x22; break;
        case KBD_h:              code = 0x23; break;
        case KBD_j:              code = 0x24; break;
        case KBD_k:              code = 0x25; break;
        case KBD_l:              code = 0x26; break;

        case KBD_semicolon:      code = 0x27; break;
        case KBD_quote:          code = 0x28; break;
        case KBD_grave:          code = 0x29; break;
        case KBD_leftshift:      code = 0x2a; break;
        case KBD_backslash:      code = 0x2b; break;

        case KBD_z:              code = 0x2c; break;
        case KBD_x:              code = 0x2d; break;
        case KBD_c:              code = 0x2e; break;
        case KBD_v:              code = 0x2f; break;
        case KBD_b:              code = 0x30; break;
        case KBD_n:              code = 0x31; break;
        case KBD_m:              code = 0x32; break;

        case KBD_comma:          code = 0x33; break;
        case KBD_period:         code = 0x34; break;
        case KBD_slash:          code = 0x35; break;
        case KBD_rightshift:     code = 0x36; break;
        case KBD_kpmultiply:     code = 0x37; break;
        case KBD_leftalt:        code = 0x38; break;
        case KBD_space:          code = 0x39; break;
        case KBD_capslock:       code = 0x3a; break;

        case KBD_f1:             code = 0x3b; break;
        case KBD_f2:             code = 0x3c; break;
        case KBD_f3:             code = 0x3d; break;
        case KBD_f4:             code = 0x3e; break;
        case KBD_f5:             code = 0x3f; break;
        case KBD_f6:             code = 0x40; break;
        case KBD_f7:             code = 0x41; break;
        case KBD_f8:             code = 0x42; break;
        case KBD_f9:             code = 0x43; break;
        case KBD_f10:            code = 0x44; break;

        case KBD_numlock:        code = 0x45; break;
        case KBD_scrolllock:     code = 0x46; break;

        case KBD_kp7:            code = 0x47; break;
        case KBD_kp8:            code = 0x48; break;
        case KBD_kp9:            code = 0x49; break;
        case KBD_kpminus:        code = 0x4a; break;
        case KBD_kp4:            code = 0x4b; break;
        case KBD_kp5:            code = 0x4c; break;
        case KBD_kp6:            code = 0x4d; break;
        case KBD_kpplus:         code = 0x4e; break;
        case KBD_kp1:            code = 0x4f; break;
        case KBD_kp2:            code = 0x50; break;
        case KBD_kp3:            code = 0x51; break;
        case KBD_kp0:            code = 0x52; break;
        case KBD_kpperiod:       code = 0x53; break;

        case KBD_oem102:         code = 0x56; break;
        case KBD_f11:            code = 0x57; break;
        case KBD_f12:            code = 0x58; break;

        case KBD_abnt1:          code = 0x73; break;

        // Extended keys

        case KBD_kpenter:        extend = true; code = 0x1c; break;
        case KBD_rightctrl:      extend = true; code = 0x1d; break;
        case KBD_kpdivide:       extend = true; code = 0x35; break;
        case KBD_rightalt:       extend = true; code = 0x38; break;
        case KBD_home:           extend = true; code = 0x47; break;
        case KBD_up:             extend = true; code = 0x48; break;
        case KBD_pageup:         extend = true; code = 0x49; break;
        case KBD_left:           extend = true; code = 0x4b; break;
        case KBD_right:          extend = true; code = 0x4d; break;
        case KBD_end:            extend = true; code = 0x4f; break;
        case KBD_down:           extend = true; code = 0x50; break;
        case KBD_pagedown:       extend = true; code = 0x51; break;
        case KBD_insert:         extend = true; code = 0x52; break;
        case KBD_delete:         extend = true; code = 0x53; break;
        case KBD_leftgui:        extend = true; code = 0x5b; break;
        case KBD_rightgui:       extend = true; code = 0x5c; break;

	// clang-format on

	case KBD_pause:
		if (is_pressed) {
			// Pause key gets released as soon as it is pressed
			return {
			        0xe1,
			        0x1d,
			        0x45,
			        0xe1,
			        0x1d | 0x80,
			        0x45 | 0x80,
			};
		}
		return {};

	case KBD_printscreen:
		return {
		        0xe0,
		        static_cast<uint8_t>(0x2a | (is_pressed ? 0 : 0x80)),
		        0xe0,
		        static_cast<uint8_t>(0x37 | (is_pressed ? 0 : 0x80)),
		};

	default: E_Exit("Missing key in codeset 1"); return {};
	}

	if (extend) {
		return {
		        0xe0,
		        static_cast<uint8_t>(code | (is_pressed ? 0 : 0x80)),
		};
	}

	return {
	        static_cast<uint8_t>(code | (is_pressed ? 0 : 0x80)),
	};
}

#ifdef ENABLE_SCANCODE_SET_2
std::vector<uint8_t> KEYBOARD_GetScanCode2(const KBD_KEYS key_type,
                                           const bool is_pressed)
{
	uint8_t code = 0x00;
	bool extend  = false;

	switch (key_type) {
	// clang-format off

        case KBD_f9:             code = 0x01; break;
        case KBD_f5:             code = 0x03; break;
        case KBD_f3:             code = 0x04; break;
        case KBD_f1:             code = 0x05; break;
        case KBD_f2:             code = 0x06; break;
        case KBD_f12:            code = 0x07; break;
        case KBD_f10:            code = 0x09; break;
        case KBD_f8:             code = 0x0a; break;
        case KBD_f6:             code = 0x0b; break;
        case KBD_f4:             code = 0x0c; break;
        case KBD_tab:            code = 0x0d; break;
        case KBD_grave:          code = 0x0e; break;
        case KBD_leftalt:        code = 0x11; break;
        case KBD_leftshift:      code = 0x12; break;
        case KBD_leftctrl:       code = 0x14; break;
        case KBD_q:              code = 0x15; break;
        case KBD_1:              code = 0x16; break;
        case KBD_z:              code = 0x1a; break;
        case KBD_s:              code = 0x1b; break;
        case KBD_a:              code = 0x1c; break;
        case KBD_w:              code = 0x1d; break;
        case KBD_2:              code = 0x1e; break;
        case KBD_c:              code = 0x21; break;
        case KBD_x:              code = 0x22; break;
        case KBD_d:              code = 0x23; break;
        case KBD_e:              code = 0x24; break;
        case KBD_4:              code = 0x25; break;
        case KBD_3:              code = 0x26; break;
        case KBD_space:          code = 0x29; break;
        case KBD_v:              code = 0x2a; break;
        case KBD_f:              code = 0x2b; break;
        case KBD_t:              code = 0x2c; break;
        case KBD_r:              code = 0x2d; break;
        case KBD_5:              code = 0x2e; break;
        case KBD_n:              code = 0x31; break;
        case KBD_b:              code = 0x32; break;
        case KBD_h:              code = 0x33; break;
        case KBD_g:              code = 0x34; break;
        case KBD_y:              code = 0x35; break;
        case KBD_6:              code = 0x36; break;
        case KBD_m:              code = 0x3a; break;
        case KBD_j:              code = 0x3b; break;
        case KBD_u:              code = 0x3c; break;
        case KBD_7:              code = 0x3d; break;
        case KBD_8:              code = 0x3e; break;
        case KBD_comma:          code = 0x41; break;
        case KBD_k:              code = 0x42; break;
        case KBD_i:              code = 0x43; break;
        case KBD_o:              code = 0x44; break;
        case KBD_0:              code = 0x45; break;
        case KBD_9:              code = 0x46; break;
        case KBD_period:         code = 0x49; break;
        case KBD_slash:          code = 0x4a; break;
        case KBD_l:              code = 0x4b; break;
        case KBD_semicolon:      code = 0x4c; break;
        case KBD_p:              code = 0x4d; break;
        case KBD_minus:          code = 0x4e; break;
        case KBD_abnt1:          code = 0x51; break;
        case KBD_quote:          code = 0x52; break;
        case KBD_leftbracket:    code = 0x54; break;
        case KBD_equals:         code = 0x55; break;
        case KBD_capslock:       code = 0x58; break;
        case KBD_rightshift:     code = 0x59; break;
        case KBD_enter:          code = 0x5a; break;
        case KBD_rightbracket:   code = 0x5b; break;
        case KBD_backslash:      code = 0x5d; break;    
        case KBD_oem102:         code = 0x61; break;
        case KBD_backspace:      code = 0x66; break;
        case KBD_kp1:            code = 0x69; break;
        case KBD_kp4:            code = 0x6b; break;
        case KBD_kp7:            code = 0x6c; break;
        case KBD_kp0:            code = 0x70; break;
        case KBD_kpperiod:       code = 0x71; break;
        case KBD_kp2:            code = 0x72; break;
        case KBD_kp5:            code = 0x73; break;
        case KBD_kp6:            code = 0x74; break;
        case KBD_kp8:            code = 0x75; break;
        case KBD_esc:            code = 0x76; break;
        case KBD_numlock:        code = 0x77; break;
        case KBD_f11:            code = 0x78; break;
        case KBD_kpplus:         code = 0x79; break;
        case KBD_kp3:            code = 0x7a; break;
        case KBD_kpminus:        code = 0x7b; break;
        case KBD_kpmultiply:     code = 0x7c; break;
        case KBD_kp9:            code = 0x7d; break;
        case KBD_scrolllock:     code = 0x7e; break;
        case KBD_f7:             code = 0x83; break;

        // Extended keys

        case KBD_rightalt:       extend = true; code = 0x11; break;
        case KBD_rightctrl:      extend = true; code = 0x14; break;
        case KBD_leftgui:        extend = true; code = 0x1f; break;
        case KBD_rightgui:       extend = true; code = 0x27; break;
        case KBD_kpdivide:       extend = true; code = 0x4a; break;
        case KBD_kpenter:        extend = true; code = 0x5a; break;
        case KBD_end:            extend = true; code = 0x69; break;
        case KBD_left:           extend = true; code = 0x6b; break;
        case KBD_home:           extend = true; code = 0x6c; break;
        case KBD_insert:         extend = true; code = 0x70; break;
        case KBD_delete:         extend = true; code = 0x71; break;
        case KBD_down:           extend = true; code = 0x72; break;
        case KBD_right:          extend = true; code = 0x74; break;
        case KBD_up:             extend = true; code = 0x75; break;
        case KBD_pagedown:       extend = true; code = 0x7a; break;
        case KBD_pageup:         extend = true; code = 0x7d; break;

	// clang-format on

	case KBD_printscreen:
		if (!is_pressed) {
			// Print Screen gets reported only when released
			return {
			        0xe0,
			        0xf0,
			        0x7c,
			        0xe0,
			        0xf0,
			        0x12,
			};
		}
		break;

	case KBD_pause:
		if (is_pressed) {
			// Pause key gets released as soon as it is pressed
			return {
			        0xe1,
			        0x14,
			        0x77,
			        0xe1,
			        0xf0,
			        0x14,
			        0xf0,
			};
		}
		break;

	default: E_Exit("Missing key in codeset 2"); return {};
	}

	if (is_pressed && !extend) {
		return {
		        code,
		};
	} else if (is_pressed && extend) {
		return {
		        0xe0,
		        code,
		};
	} else if (!is_pressed && !extend) {
		return {
		        0xf0,
		        code,
		};
	} else if (!is_pressed && extend) {
		return {
		        0xe0,
		        0xf0,
		        code,
		};
	}

	return {};
}
#endif // ENABLE_SCANCODE_SET_2

#ifdef ENABLE_SCANCODE_SET_3
std::vector<uint8_t> KEYBOARD_GetScanCode3(const KBD_KEYS key_type,
                                           const bool is_pressed)
{
	uint8_t code = 0x00;

	switch (key_type) {
	// clang-format off

        case KBD_f1:             code = 0x07; break;
        case KBD_esc:            code = 0x08; break;
        case KBD_tab:            code = 0x0d; break;
        case KBD_grave:          code = 0x0e; break;
        case KBD_f2:             code = 0x0f; break;
        case KBD_leftctrl:       code = 0x11; break;
        case KBD_leftshift:      code = 0x12; break;
        case KBD_oem102:         code = 0x13; break;
        case KBD_capslock:       code = 0x14; break;
        case KBD_q:              code = 0x15; break;
        case KBD_1:              code = 0x16; break;
        case KBD_f3:             code = 0x17; break;
        case KBD_leftalt:        code = 0x19; break;
        case KBD_z:              code = 0x1a; break;
        case KBD_s:              code = 0x1b; break;
        case KBD_a:              code = 0x1c; break;
        case KBD_w:              code = 0x1d; break;
        case KBD_2:              code = 0x1e; break;
        case KBD_f4:             code = 0x1f; break;
        case KBD_c:              code = 0x21; break;
        case KBD_x:              code = 0x22; break;
        case KBD_d:              code = 0x23; break;
        case KBD_e:              code = 0x24; break;
        case KBD_4:              code = 0x25; break;
        case KBD_3:              code = 0x26; break;
        case KBD_f5:             code = 0x27; break;
        case KBD_space:          code = 0x29; break;
        case KBD_v:              code = 0x2a; break;
        case KBD_f:              code = 0x2b; break;
        case KBD_t:              code = 0x2c; break;
        case KBD_r:              code = 0x2d; break;
        case KBD_5:              code = 0x2e; break;
        case KBD_f6:             code = 0x2f; break;
        case KBD_n:              code = 0x31; break;
        case KBD_b:              code = 0x32; break;
        case KBD_h:              code = 0x33; break;
        case KBD_g:              code = 0x34; break;
        case KBD_y:              code = 0x35; break;
        case KBD_6:              code = 0x36; break;
        case KBD_f7:             code = 0x37; break;
        case KBD_rightalt:       code = 0x39; break;
        case KBD_m:              code = 0x3a; break;
        case KBD_j:              code = 0x3b; break;
        case KBD_u:              code = 0x3c; break;
        case KBD_7:              code = 0x3d; break;
        case KBD_8:              code = 0x3e; break;
        case KBD_f8:             code = 0x3f; break;
        case KBD_comma:          code = 0x41; break;
        case KBD_k:              code = 0x42; break;
        case KBD_i:              code = 0x43; break;
        case KBD_o:              code = 0x44; break;
        case KBD_0:              code = 0x45; break;
        case KBD_9:              code = 0x46; break;
        case KBD_f9:             code = 0x47; break;
        case KBD_period:         code = 0x49; break;
        case KBD_slash:          code = 0x4a; break;
        case KBD_kpdivide:       code = 0x4a; break;
        case KBD_l:              code = 0x4b; break;
        case KBD_semicolon:      code = 0x4c; break;
        case KBD_p:              code = 0x4d; break;
        case KBD_minus:          code = 0x4e; break;
        case KBD_kpminus:        code = 0x4e; break;
        case KBD_f10:            code = 0x4f; break;
        case KBD_abnt1:          code = 0x51; break;
        case KBD_quote:          code = 0x52; break;
        case KBD_leftbracket:    code = 0x54; break;
        case KBD_equals:         code = 0x55; break;
        case KBD_f11:            code = 0x56; break;
        case KBD_printscreen:    code = 0x57; break;
        case KBD_rightctrl:      code = 0x58; break;
        case KBD_rightshift:     code = 0x59; break;
        case KBD_enter:          code = 0x5a; break;
        case KBD_rightbracket:   code = 0x5b; break;
        case KBD_backslash:      code = 0x5c; break;
        case KBD_f12:            code = 0x5e; break;
        case KBD_scrolllock:     code = 0x5f; break;
        case KBD_down:           code = 0x60; break;
        case KBD_left:           code = 0x61; break;
        case KBD_pause:          code = 0x62; break;
        case KBD_up:             code = 0x63; break;
        case KBD_delete:         code = 0x64; break;
        case KBD_end:            code = 0x65; break;
        case KBD_backspace:      code = 0x66; break;
        case KBD_insert:         code = 0x67; break;
        case KBD_kp1:            code = 0x69; break;
        case KBD_right:          code = 0x6a; break;
        case KBD_kp4:            code = 0x6b; break;
        case KBD_kp7:            code = 0x6c; break;
        case KBD_pagedown:       code = 0x6d; break;
        case KBD_home:           code = 0x6e; break;
        case KBD_pageup:         code = 0x6f; break;
        case KBD_kp0:            code = 0x70; break;
        case KBD_kpperiod:       code = 0x71; break;
        case KBD_kp2:            code = 0x72; break;
        case KBD_kp5:            code = 0x73; break;
        case KBD_kp6:            code = 0x74; break;
        case KBD_kp8:            code = 0x75; break;
        case KBD_numlock:        code = 0x76; break;
        case KBD_kpenter:        code = 0x79; break;
        case KBD_kp3:            code = 0x7a; break;
        case KBD_kpplus:         code = 0x7c; break;
        case KBD_kp9:            code = 0x7d; break;
        case KBD_kpmultiply:     code = 0x7e; break;
        case KBD_leftgui:        code = 0x8b; break;
        case KBD_rightgui:       code = 0x8c; break;

	// clang-format on

	default: E_Exit("Missing key in codeset 3"); return {};
	}

	if (is_pressed) {
		return {
		        code,
		};
	} else {
		return {
		        0xf0,
		        code,
		};
	}

	return {};
}
#endif // ENABLE_SCANCODE_SET_3
