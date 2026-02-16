// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2022 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "keyboard.h"

#include "private/intel8042.h"
#include "private/intel8255.h"

#include <array>

#include "config/config.h"
#include "cpu/cpu.h"
#include "dosbox.h"
#include "hardware/pic.h"
#include "hardware/timer.h"
#include "misc/support.h"
#include "utils/bitops.h"
#include "utils/checks.h"

CHECK_NARROWING();

// Emulates the PS/2 keybaord, as seen by the Intel 8042 microcontroller.

// Reference:
// - https://wiki.osdev.org/PS/2_Keyboard
// - https://stanislavs.org/helppc/keyboard_commands.html
// - https://kbd-project.org/docs/scancodes/scancodes.html
// - https://homepages.cwi.nl/~aeb/linux/kbd/scancodes.html
// - http://www-ug.eecg.toronto.edu/msl/nios_devices/datasheets/PS2%20Keyboard%20Protocol.htm

#ifdef ENABLE_SCANCODE_SET_3
#	undef HAS_SCANCODE_SET_2_OR_3
#	define HAS_SCANCODE_SET_2_OR_3
#endif // ENABLE_SCANCODE_SET_3
#ifdef ENABLE_SCANCODE_SET_2
#	undef HAS_SCANCODE_SET_2_OR_3
#	define HAS_SCANCODE_SET_2_OR_3
#endif // ENABLE_SCANCODE_SET_2

static constexpr uint8_t buffer_size = 8; // in scancodes
static constexpr size_t max_num_scancodes = UINT8_MAX + 1;

enum CodeSet : uint8_t {
	CodeSet1 = 0x01,
	CodeSet2 = 0x02,
	CodeSet3 = 0x03,
};

enum class KbdCommand : uint8_t { // PS/2 keyboard port commands
	None                 = 0x00,
	SetLeds              = 0xed,
	Echo                 = 0xee,
	CodeSet              = 0xf0,
	Identify             = 0xf2,
	SetTypeRate          = 0xf3,
	ClearEnable          = 0xf4,
	DefaultDisable       = 0xf5,
	ResetEnable          = 0xf6,
	Set3AllTypematic     = 0xf7,
	Set3AllMakeBreak     = 0xf8,
	Set3AllMakeOnly      = 0xf9,
	Set3AllTypeMakeBreak = 0xfa,
	Set3KeyTypematic     = 0xfb,
	Set3KeyMakeBreak     = 0xfc,
	Set3KeyMakeOnly      = 0xfd,
	Resend               = 0xfe,
	Reset                = 0xff,
};

// Internal keyboard scancode buffer

std::vector<uint8_t> buffer[buffer_size] = {};

static bool buffer_overflowed  = false;
static size_t buffer_start_idx = 0;
static size_t buffer_num_used  = 0;

// Key repetition mechanism data

static struct {
	KBD_KEYS key   = KBD_NONE; // key which went typematic
	uint16_t wait  = 0;
	uint16_t pause = 0;
	uint16_t rate  = 0;

} repeat;

struct Set3CodeInfoEntry {
	bool is_enabled_typematic = true;
	bool is_enabled_make      = true;
	bool is_enabled_break     = true;
};

static std::array<Set3CodeInfoEntry, max_num_scancodes> set3_code_info = {};

// State of keyboard LEDs, as requested via keyboard controller
static uint8_t led_state;
// If true, all LEDs are on due to keyboard reset
static bool leds_all_on = false;
// If false, keyboard does not push keycodes to the controller
static bool is_scanning = true;

static uint8_t code_set = CodeSet1;

// Command currently being executed, waiting for parameter
static KbdCommand current_command = KbdCommand::None;

// If enabled, all keyboard events are dropped until secure mode is enabled
static bool should_wait_for_secure_mode = false;

// ***************************************************************************
// Helper routines to log various warnings
// ***************************************************************************

static void warn_resend()
{
	static bool already_warned = false;
	if (!already_warned) {
		LOG_WARNING("KEYBOARD: Resend command not implemented");
		already_warned = true;
	}
}

static void warn_unknown_command(const KbdCommand command)
{
	static bool already_warned[max_num_scancodes];
	const auto code = static_cast<uint8_t>(command);
	if (!already_warned[code]) {
		LOG_WARNING("KEYBOARD: Unknown command 0x%02x", code);
		already_warned[code] = true;
	}
}

static void warn_unknown_scancode_set()
{
	static bool already_warned = false;
	if (!already_warned) {
		LOG_WARNING("KEYBOARD: Guest requested unknown scancode set");
		already_warned = true;
	}
}

static void warn_waiting_for_secure_mode()
{
	static bool already_warned = false;
	if (!already_warned) {
		LOG_WARNING("KEYBOARD: Input ignored until secure mode is set");
		already_warned = true;
	}
}

// ***************************************************************************
// keyboard buffer support
// ***************************************************************************

static void maybe_transfer_buffer()
{
	if (!buffer_num_used || !I8042_IsReadyForKbdFrame()) {
		return;
	}

	I8042_AddKbdFrame(buffer[buffer_start_idx]);

	--buffer_num_used;
	buffer_start_idx = (buffer_start_idx + 1) % buffer_size;
}

static void buffer_add(const std::vector<uint8_t>& scan_code)
{
	// Ignore unsupported keys, drop everything if buffer overflowed
	if (scan_code.empty() || buffer_overflowed) {
		return;
	}

	// If buffer got overflowed, drop everything until
	// the controllers queue gets free for the keyboard
	if (buffer_num_used == buffer_size) {
		buffer_num_used   = 0;
		buffer_overflowed = true;
		return;
	}

	// We can safely add a scancode to the buffer
	const size_t idx = (buffer_start_idx + buffer_num_used++) % buffer_size;
	buffer[idx] = scan_code;

	// If possible, transfer the scancode to keyboard controller
	maybe_transfer_buffer();
}

// ***************************************************************************
// Key repetition
// ***************************************************************************

static void typematic_update(const KBD_KEYS key_type, const bool is_pressed)
{
	if (key_type == KBD_pause || key_type == KBD_printscreen) {
		// Key is excluded from being repeated
	} else if (is_pressed) {
		if (repeat.key == key_type) {
			repeat.wait = repeat.rate;
		} else {
			repeat.wait = repeat.pause;
		}
		repeat.key = key_type;
	} else if (repeat.key == key_type) {
		// Currently repeated key being released
		repeat.key  = KBD_NONE;
		repeat.wait = 0;
	}
}

#ifdef ENABLE_SCANCODE_SET_3
static void typematic_update_set3(const KBD_KEYS key_type,
                                  const std::vector<uint8_t>& scan_code,
                                  const bool is_pressed)
{
	// Ignore keys not supported in set 3
	if (scan_code.empty()) {
		return;
	}

	// Sanity check, for debug builds only
	if (is_pressed) {
		assert(scan_code.size() == 1);
	} else {
		assert(scan_code.size() == 2);
		assert(scan_code[0] == 0xf0);
	}

	// Ignore keys for which typematic behavior was disabled
	if (!set3_code_info[scan_code.back()].is_enabled_typematic) {
		return;
	}

	// For all the other keys, follow usual behavior
	typematic_update(key_type, is_pressed);
}
#endif // ENABLE_SCANCODE_SET_3

static void typematic_tick()
{
	// Update countdown, check if we should try to add key press
	if (repeat.wait) {
		if (--repeat.wait) {
			return;
		}
	}

	// No typematic key = nothing to do
	if (!repeat.key) {
		return;
	}

	// Check if buffers are free
	if (buffer_num_used || !I8042_IsReadyForKbdFrame()) {
		repeat.wait = 1;
		return;
	}

	// Simulate key press
	constexpr bool is_pressed = true;
	KEYBOARD_AddKey(repeat.key, is_pressed);
}

// ***************************************************************************
// Keyboard microcontroller high-level emulation
// ***************************************************************************

static void maybe_notify_led_state()
{
	// TODO:
	// - add LED support to BIOS, currently it does not set them
	// - consider displaying LEDs on screen

	static uint8_t last_reported = 0x00;
	static bool first_time       = true;

	const uint8_t current_state = KEYBOARD_GetLedState();
	if (first_time || current_state != last_reported) {
#if 0
		LOG_INFO("KEYBOARD:  [%c] SCROLL_LOCK  [%c] NUM_LOCK  [%c] CAPS_LOCK",
		          bit::is(current_state, b0) ? '*' : ' ',
		          bit::is(current_state, b1) ? '*' : ' ',
		          bit::is(current_state, b2) ? '*' : ' ');
#endif
		last_reported = current_state;
	}
}

static void leds_all_on_expire_handler(uint32_t /*val*/)
{
	leds_all_on = false;
	maybe_notify_led_state();
}

static void clear_buffer()
{
	buffer_start_idx  = 0;
	buffer_num_used   = 0;
	buffer_overflowed = false;

	repeat.key  = KBD_NONE;
	repeat.wait = 0;
}

bool scancode_set(const uint8_t requested_set)
{
#ifndef ENABLE_SCANCODE_SET_2
	if (requested_set == CodeSet2) {
		return false;
	}
#endif // no ENABLE_SCANCODE_SET_2

#ifndef ENABLE_SCANCODE_SET_3
	if (requested_set == CodeSet3) {
		return false;
	}
#endif // no ENABLE_SCANCODE_SET_3

	if (requested_set < CodeSet1 || requested_set > CodeSet3) {
		warn_unknown_scancode_set();
		return false;
	}

#ifdef HAS_SCANCODE_SET_2_OR_3
	const auto old_set = code_set;
#endif // HAS_SCANCODE_SET_2_OR_3
	code_set = requested_set;

#ifdef HAS_SCANCODE_SET_2_OR_3
	static bool first_time = true;
	if (first_time || (code_set != old_set)) {
		LOG_INFO("KEYBOARD: Using scancode set #%d", code_set);
	}
#endif // HAS_SCANCODE_SET_2_OR_3

	clear_buffer();
	return true;
}

void set_type_rate(const uint8_t byte)
{
	// clang-format off
	static const uint16_t pause_table[]  = {
		250, 500, 750, 1000
	};
	static const uint16_t rate_table[] = {
		 33,  37,  42,  46,  50,  54,  58,  63,
		 67,  75,  83,  92, 100, 109, 118, 125,
		133, 149, 167, 182, 200, 217, 233, 250,
		270, 303, 333, 370, 400, 435, 476, 500
	};
	// clang-format on

	const auto pause_idx = (byte & 0b0110'0000) >> 5;
	const auto rate_idx  = (byte & 0b0001'1111);

	repeat.pause = pause_table[pause_idx];
	repeat.rate  = rate_table[rate_idx];
}

static void set_defaults()
{
	repeat.key   = KBD_NONE;
	repeat.pause = 500;
	repeat.rate  = 33;
	repeat.wait  = 0;

	for (auto& entry : set3_code_info) {
		entry.is_enabled_make      = true;
		entry.is_enabled_break     = true;
		entry.is_enabled_typematic = true;
	}

#ifdef ENABLE_SCANCODE_SET_2
	scancode_set(CodeSet2);
#else
	scancode_set(CodeSet1);
#endif
}

static void keyboard_reset(const bool is_startup = false)
{
	set_defaults();
	clear_buffer();

	is_scanning = true;

	// Flash all the LEDs
	PIC_RemoveEvents(leds_all_on_expire_handler);
	led_state   = 0;
	leds_all_on = !is_startup;
	if (leds_all_on) {
		// To commemorate how evil the whole keyboard
		// subsystem is, let's set blink expiration
		// time to 666 milliseconds
		constexpr double expire_time_ms = 666.0;
		PIC_AddEvent(leds_all_on_expire_handler, expire_time_ms);
	}
	maybe_notify_led_state();
}

static void execute_command(const KbdCommand command)
{
	// LOG_INFO("KEYBOARD: Command 0x%02x", static_cast<int>(command));

	switch (command) {
	//
	// Commands requiring a parameter
	//
	case KbdCommand::SetLeds:          // 0xed
	case KbdCommand::SetTypeRate:      // 0xf3
		I8042_AddKbdByte(0xfa); // acknowledge
		current_command = command;
		break;
	case KbdCommand::CodeSet:          // 0xf0
	case KbdCommand::Set3KeyTypematic: // 0xfb
	case KbdCommand::Set3KeyMakeBreak: // 0xfc
	case KbdCommand::Set3KeyMakeOnly:  // 0xfd
		I8042_AddKbdByte(0xfa); // acknowledge
		clear_buffer();
		current_command = command;
		break;
	//
	// No-parameter commands
	//
	case KbdCommand::Echo: // 0xee
		// Diagnostic echo, responds without acknowledge
		I8042_AddKbdByte(0xee);
		break;
	case KbdCommand::Identify: // 0xf2
		// Returns keyboard ID
		// - 0xab, 0x83: typical for multifunction PS/2 keyboards
		// - 0xab, 0x84: many short, space saver keyboards
		// - 0xab, 0x86: many 122-key keyboards
		I8042_AddKbdByte(0xfa); // acknowledge
		I8042_AddKbdByte(0xab);
		I8042_AddKbdByte(0x83);
		break;
	case KbdCommand::ClearEnable: // 0xf4
		// Clear internal buffer, enable scanning
		I8042_AddKbdByte(0xfa); // acknowledge
		clear_buffer();
		is_scanning = true;
		break;
	case KbdCommand::DefaultDisable: // 0xf5
		// Restore defaults, disable scanning
		I8042_AddKbdByte(0xfa); // acknowledge
		clear_buffer();
		set_defaults();
		is_scanning = false;
		break;
	case KbdCommand::ResetEnable: // 0xf6
		// Restore defaults, enable scanning
		I8042_AddKbdByte(0xfa); // acknowledge
		clear_buffer();
		set_defaults();
		is_scanning = true;
		break;
	case KbdCommand::Set3AllTypematic: // 0xf7
		// Set scanning type for all the keys,
		// relevant for scancode set 3 only
		I8042_AddKbdByte(0xfa); // acknowledge
		clear_buffer();
		for (auto& entry : set3_code_info) {
			entry.is_enabled_typematic = true;
			entry.is_enabled_make      = false;
			entry.is_enabled_break     = false;
		}
		break;
	case KbdCommand::Set3AllMakeBreak: // 0xf8
		// Set scanning type for all the keys,
		// relevant for scancode set 3 only
		I8042_AddKbdByte(0xfa); // acknowledge
		clear_buffer();
		for (auto& entry : set3_code_info) {
			entry.is_enabled_typematic = false;
			entry.is_enabled_make      = true;
			entry.is_enabled_break     = true;
		}
		break;
	case KbdCommand::Set3AllMakeOnly: // 0xf9
		// Set scanning type for all the keys,
		// relevant for scancode set 3 only
		I8042_AddKbdByte(0xfa); // acknowledge
		clear_buffer();
		for (auto& entry : set3_code_info) {
			entry.is_enabled_typematic = false;
			entry.is_enabled_make      = true;
			entry.is_enabled_break     = false;
		}
		break;
	case KbdCommand::Set3AllTypeMakeBreak: // 0xfa
		// Set scanning type for all the keys,
		// relevant for scancode set 3 only
		I8042_AddKbdByte(0xfa); // acknowledge
		clear_buffer();
		for (auto& entry : set3_code_info) {
			entry.is_enabled_typematic = true;
			entry.is_enabled_make      = true;
			entry.is_enabled_break     = true;
		}
		break;
	case KbdCommand::Resend: // 0xfe
		// Resend byte, should normally be used on transmission
		// errors - not implemented, as the emulation can
		// also send whole multi-byte scancode at once
		warn_resend();
		// We have to respond, or else the 'In Extremis' game intro
		// (sends 0xfe and 0xaa commands) hangs with a black screen
		I8042_AddKbdByte(0xfa); // acknowledge
		break;
	case KbdCommand::Reset: // 0xff
		// Full keyboard reset and self test
		// 0xaa: passed; 0xfc/0xfd: failed
		I8042_AddKbdByte(0xfa); // acknowledge
		keyboard_reset();
		I8042_AddKbdByte(0xaa);
		break;
	//
	// Unknown commands
	//
	default:
		warn_unknown_command(command);
		I8042_AddKbdByte(0xfe); // resend
		break;
	}
}

static void execute_command(const KbdCommand command, const uint8_t param)
{
	// LOG_INFO("KEYBOARD: Command 0x%02x, parameter 0x%02x",
	//          static_cast<int>(command), param);

	switch (command) {
	case KbdCommand::SetLeds: // 0xed
		// Set keyboard LEDs according to bitfielld
		I8042_AddKbdByte(0xfa); // acknowledge
		led_state = param;
		maybe_notify_led_state();
		break;
	case KbdCommand::CodeSet: // 0xf0
		// Query or change the scancode set
		if (param != 0) {
			// Query current scancode set
			if (scancode_set(param)) {
				I8042_AddKbdByte(0xfa); // acknowledge
			} else {
				current_command = command;
				I8042_AddKbdByte(0xfe); // resend
			}
		} else {
			I8042_AddKbdByte(0xfa); // acknowledge
			switch (code_set) {
			case CodeSet1:
			case CodeSet2:
			case CodeSet3: I8042_AddKbdByte(code_set); break;
			default:
				assert(false);
				I8042_AddKbdByte(CodeSet1);
				break;
			}
		}
		break;
	case KbdCommand::SetTypeRate: // 0xf3
		// Sets typematic rate/delay
		I8042_AddKbdByte(0xfa); // acknowledge
		set_type_rate(param);
		break;
	case KbdCommand::Set3KeyTypematic: // 0xfb
		// Set scanning type for the given key,
		// relevant for scancode set 3 only
		I8042_AddKbdByte(0xfa); // acknowledge
		clear_buffer();
		set3_code_info[param].is_enabled_typematic = true;
		set3_code_info[param].is_enabled_make      = false;
		set3_code_info[param].is_enabled_break     = false;
		break;
	case KbdCommand::Set3KeyMakeBreak: // 0xfc
		// Set scanning type for the given key,
		// relevant for scancode set 3 only
		I8042_AddKbdByte(0xfa); // acknowledge
		clear_buffer();
		set3_code_info[param].is_enabled_typematic = false;
		set3_code_info[param].is_enabled_make      = true;
		set3_code_info[param].is_enabled_break     = true;
		break;
	case KbdCommand::Set3KeyMakeOnly: // 0xfd
		// Set scanning type for the given key,
		// relevant for scancode set 3 only
		I8042_AddKbdByte(0xfa); // acknowledge
		clear_buffer();
		set3_code_info[param].is_enabled_typematic = false;
		set3_code_info[param].is_enabled_make      = true;
		set3_code_info[param].is_enabled_break     = false;
		break;
	default:
		// If we are here, than either this function
		// was wrongly called or it is incomplete
		assert(false);
		break;
	};
}

// ***************************************************************************
// External interfaces
// ***************************************************************************

void KEYBOARD_WaitForSecureMode()
{
	// This should never be undone!
	should_wait_for_secure_mode = true;
}

void KEYBOARD_PortWrite(const uint8_t byte)
{
	using namespace bit::literals;

	// Highest bit set usuaally means a command
	const bool is_command = bit::is(byte, b7) &&
	                        current_command != KbdCommand::Set3KeyTypematic &&
	                        current_command != KbdCommand::Set3KeyMakeBreak &&
	                        current_command != KbdCommand::Set3KeyMakeOnly;

	if (is_command) {
		// Terminate previous command
		current_command = KbdCommand::None;
	}

	const auto command = current_command;
	if (command != KbdCommand::None) {
		// Continue execution of previous command
		current_command = KbdCommand::None;
		execute_command(command, byte);
	} else if (is_command) {
		execute_command(static_cast<KbdCommand>(byte));
	}
}

void KEYBOARD_NotifyReadyForFrame()
{
	// Since the guest software seems to be reacting on keys again,
	// clear the buffer overflow flag, do not ignore keys any more
	buffer_overflowed = false;

	maybe_transfer_buffer();
}

void KEYBOARD_AddKey(const KBD_KEYS key_type, const bool is_pressed)
{
	if (should_wait_for_secure_mode && !control->SecureMode()) {
		warn_waiting_for_secure_mode();
		return;
	}

	if (!is_scanning) {
		return;
	}

	std::vector<uint8_t> scan_code = {};

	switch (code_set) {
	case CodeSet1:
		scan_code = KEYBOARD_GetScanCode1(key_type, is_pressed);
		typematic_update(key_type, is_pressed);
		break;
#ifdef ENABLE_SCANCODE_SET_2
	case CodeSet2:
		scan_code = KEYBOARD_GetScanCode2(key_type, is_pressed);
		typematic_update(key_type, is_pressed);
		break;
#endif // ENABLE_SCANCODE_SET_2
#ifdef ENABLE_SCANCODE_SET_3
	case CodeSet3:
		scan_code = KEYBOARD_GetScanCode3(key_type, is_pressed);
		typematic_update_set3(key_type, scan_code, is_pressed);
		break;
#endif // ENABLE_SCANCODE_SET_3
	default: assert(false); break;
	}

	buffer_add(scan_code);
}

uint8_t KEYBOARD_GetLedState()
{
	// We support only 3 leds
	return (leds_all_on ? 0xff : led_state) & 0b0000'0111;
}

void KEYBOARD_ClrBuffer()
{
	// Sometimes the GUI part wants us to clear the buffer. Original code
	// was clearing the controller buffer, but this is a REALLY dangerous
	// operation, because it might:
	// - clear the result of keyboard / device command, which might
	//   confuse the guest side software
	// - clear part of the scancode from the buffer, while the other
	//   part was already fetched by the guest software
	// - wipe the information about mouse button release
	// This might lead to occasional misbehaviour, timing dependent,
	// possibly reproducible on some hosts and on on other.
	// Moreover, Windows 3.11 for Workgroups does not like unnecessary
	// keyboard IRQs - so once we fired an IRQ for the scancode package,
	// it's too late to withdraw it!

	// We have to limit clearing to keyboard internal buffer, this is safe
	clear_buffer();
}

// ***************************************************************************
// Initialization
// ***************************************************************************

void KEYBOARD_Init()
{
	I8042_Init();
	I8255_Init();
	TIMER_AddTickHandler(&typematic_tick);

	constexpr bool is_startup = true;
	keyboard_reset(is_startup);
	scancode_set(CodeSet1);
}
