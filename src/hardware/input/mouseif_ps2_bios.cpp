// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mouse.h"

#include "private/intel8042.h"
#include "private/mouse_config.h"
#include "private/mouse_interfaces.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "cpu/callback.h"
#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "hardware/pic.h"
#include "ints/bios.h"
#include "ints/int10.h"
#include "utils/bitops.h"
#include "utils/checks.h"
#include "utils/math_utils.h"

CHECK_NARROWING();

// This file implements both BIOS interface and PS/2 direct mouse access.

// Reference:
// - https://www.digchip.com/datasheets/parts/datasheet/196/HT82M30A-pdf.php
// - https://isdaman.com/alsos/hardware/mouse/ps2interface.htm
// - https://wiki.osdev.org/Mouse_Input
// - https://wiki.osdev.org/PS/2_Mouse
// - https://www.os2museum.com/wp/jumpy-ps2-mouse-in-enhanced-mode-windows-3-x/
// - http://hackipedia.org, 'Windows and the 5-Button Wheel Mouse'

// TODO: consider adding Typhoon mouse emulation:
// - unlock sequence (sample rates): 200, 100, 80, 60, 40, 20
// - mouse protocol ID: 8
// - frame: 6 bytes
//   - 3 bytes as usual
//   - byte 4: always 1
//   - byte 5: z-axis movement, one of: 0xff, 0x00, or 0x01
//   - byte 6: 0
// Reference: https://kbd-project.org/docs/scancodes/scancodes-13.html#mcf2
// Drawbacks:
// - probably Windows 9x only (couldn't find DOS driver)
// - protocol is bandwidth inefficient

// clang-format off

static const std::vector<uint8_t> list_rates_hz = {
        10, 20, 40, 60, 80, 100, 200 // PS/2 mouse sampling rates
};

static const std::vector<uint8_t> list_resolutions = {
        1, 2, 4, 8 // PS/2 mouse resolution values
};

// clang-format on

enum class AuxCommand : uint8_t { // PS/2 AUX (mouse) port commands
	None          = 0x00,
	SetScaling11  = 0xe6,
	SetScaling21  = 0xe7,
	SetResolution = 0xe8,
	GetStatus     = 0xe9,
	SetStreamMode = 0xea,
	PollFrame     = 0xeb,
	ResetWrapMode = 0xec,
	SetWrapMode   = 0xee,
	SetRemoteMode = 0xf0,
	GetDevId      = 0xf2,
	SetSampleRate = 0xf3,
	EnableDev     = 0xf4,
	DisableDev    = 0xf5,
	SetDefaults   = 0xf6,
	ResetDev      = 0xff,
};

// delay to enforce between callbacks, in milliseconds
static uint8_t delay_ms = 5;

// true = delay timer is in progress
static bool delay_running = false;
// true = delay timer expired, event can be sent immediately
static bool delay_expired = true;

static MouseButtonsAll buttons;     // currently visible button state
static MouseButtonsAll buttons_all; // state of all 5 buttons as on the host side
static MouseButtons12S buttons_12S; // buttons with 3/4/5 quished together

// Accumulated mouse movement, waiting to be reported
static float delta_x     = 0.0f;
static float delta_y     = 0.0f;
static float delta_wheel = 0.0f;

static MouseModelPs2 protocol = MouseModelPs2::Standard;
static uint8_t unlock_idx_im = 0; // sequence index for unlocking extended protocol
static uint8_t unlock_idx_xp = 0;

// frame to be sent via the i8042 controller
static std::vector<uint8_t> frame = {};
// if enough movement or other mouse state changes for a new frame
static bool has_data_for_frame = false;
// if virtual machine mouse interface needs us to issue dummy event
static bool vmm_needs_dummy_event = false;

static uint8_t rate_hz = 0; // maximum rate at which the mouse state is updated
static bool scaling_21 = false;

static uint8_t counts_mm = 0;    // counts per mm
static float counts_rate = 0.0f; // 1.0 is 4 counts per mm

static bool is_reporting = false;
static bool mode_remote  = false; // true = remote mode, false = stream mode
static bool mode_wrap    = false; // true = wrap mode

// Command currently being executed, waiting for parameter
static AuxCommand current_command = AuxCommand::None;

// ***************************************************************************
// Helper routines to log various warnings
// ***************************************************************************

static void warn_unknown_command(const AuxCommand command)
{
	static bool already_warned[UINT8_MAX + 1];
	const auto code = static_cast<uint8_t>(command);
	if (!already_warned[code]) {
		LOG_WARNING("MOUSE (PS/2): Unknown command 0x%02x", code);
		already_warned[code] = true;
	}
}

// ***************************************************************************
// PS/2 hardware mouse implementation
// ***************************************************************************

void MOUSEPS2_UpdateButtonSquish()
{
	// - if VMware or VirtualBox compatible driver is enabled, never try
	//   to report mouse buttons 4 and 5, that would be asking for trouble
	// - for PS/2 modes other than IntelliMouse Explorer there is
	//   no standard way to report buttons 4 and 5

	const bool squish = mouse_shared.active_vmm ||
	                    (protocol != MouseModelPs2::Explorer);
	buttons._data = squish ? buttons_12S._data : buttons_all._data;
}

static void terminate_unlock_sequence()
{
	unlock_idx_im = 0;
	unlock_idx_xp = 0;
}

static void maybe_log_mouse_protocol()
{
	using enum MouseModelPs2;

	static bool first_time = true;
	static MouseModelPs2 last_logged = {};

	if (!first_time && protocol == last_logged) {
		return;
	}

	std::string protocol_name = {};
	switch (protocol) {
	case Standard:
		protocol_name = "3 buttons";
		break;
	case IntelliMouse:
		protocol_name = "3 buttons + wheel (IntelliMouse)";
		break;
	case Explorer:
		protocol_name = "5 buttons + wheel (IntelliMouse Explorer)";
		break;
	case NoMouse:
		break;
	default:
		assertm(false, "unknown mouse model (PS/2)");
		break;
	}

	if (!protocol_name.empty()) {
		LOG_MSG("MOUSE (PS/2): Using a %s protocol, selected by guest software",
	        	protocol_name.c_str());
	}	

	first_time  = false;
	last_logged = protocol;
}

static void set_protocol(const MouseModelPs2 new_protocol, bool is_startup = false)
{
	terminate_unlock_sequence();

	if (is_startup || protocol != new_protocol) {
		protocol = new_protocol;

		if (!is_startup) {
			maybe_log_mouse_protocol();
		}

		frame.clear();
		MOUSEPS2_UpdateButtonSquish();
	}
}

static uint8_t get_reset_wheel_4bit()
{
	auto d = MOUSE_ConsumeInt8(delta_wheel);
	d = std::clamp(d, static_cast<int8_t>(-0x08), static_cast<int8_t>(0x07));

	// 0x0f for -1, 0x0e for -2, etc.
	return static_cast<uint8_t>((d >= 0) ? d : 0x10 + d);
}

static uint8_t get_reset_wheel_8bit()
{
	const auto d = MOUSE_ConsumeInt8(delta_wheel);

	// 0xff for -1, 0xfe for -2, etc.
	return static_cast<uint8_t>((d >= 0) ? d : 0x100 + d);
}

static int16_t get_scaled_movement(const int16_t d, const bool is_polling)
{
	// Scaling is not applied when polling, see:
	// see https://www3.tuhh.de/osg/Lehre/SS21/V_BSB/doc/ps2mouse.html

	if (!scaling_21 || is_polling) {
		return d;
	}

	// clang-format off
	switch (d) {
	case -5: return -9;
	case -4: return -6;
	case -3: return -3;
	case -2: [[fallthrough]];
	case -1: return -1;
	case 1:  [[fallthrough]];
	case 2:  return 1;
	case 3:  return 3;
	case 4:  return 6;
	case 5:  return 9;
	default: return static_cast<int16_t>(2 * d);
	}
	// clang-format on
}

static void reset_counters()
{
	delta_x     = 0.0f;
	delta_y     = 0.0f;
	delta_wheel = 0.0f;
}

static void build_protocol_frame(const bool is_polling = false)
{
	using namespace bit::literals;

	union {
		uint8_t _data = 0x08;

		bit_view<0, 1> left;
		bit_view<1, 1> right;
		bit_view<2, 1> middle;
		bit_view<4, 1> sign_x;
		bit_view<5, 1> sign_y;
		bit_view<6, 1> overflow_x;
		bit_view<7, 1> overflow_y;
	} mdat;

	mdat.left   = buttons.left;
	mdat.right  = buttons.right;
	mdat.middle = buttons.middle;

	auto dx = MOUSE_ConsumeInt16(delta_x);
	auto dy = MOUSE_ConsumeInt16(delta_y);

	dx = get_scaled_movement(dx, is_polling);
	dy = get_scaled_movement(static_cast<int16_t>(-dy), is_polling);

	if (protocol == MouseModelPs2::Explorer) {
		// There is no overflow for 5-button mouse protocol, see
		// HT82M30A datasheet
		dx = std::clamp(dx,
		                static_cast<int16_t>(-UINT8_MAX),
		                static_cast<int16_t>(UINT8_MAX));
		dy = std::clamp(dy,
		                static_cast<int16_t>(-UINT8_MAX),
		                static_cast<int16_t>(UINT8_MAX));
	} else {
		if ((dx > UINT8_MAX) || (dx < -UINT8_MAX)) {
			mdat.overflow_x = 1;
		}
		if ((dy > UINT8_MAX) || (dy < -UINT8_MAX)) {
			mdat.overflow_y = 1;
		}
	}

	dx = static_cast<int16_t>(dx % (UINT8_MAX + 1));
	if (dx < 0) {
		dx = static_cast<int16_t>(dx + UINT8_MAX + 1);
		mdat.sign_x = 1;
	}

	dy = static_cast<int16_t>(dy % (UINT8_MAX + 1));
	if (dy < 0) {
		dy = static_cast<int16_t>(dy + UINT8_MAX + 1);
		mdat.sign_y = 1;
	}

	frame.resize(3);
	frame[0] = mdat._data;
	frame[1] = static_cast<uint8_t>(dx);
	frame[2] = static_cast<uint8_t>(dy);

	if (protocol == MouseModelPs2::IntelliMouse) {
		frame.resize(4);
		frame[3] = get_reset_wheel_8bit();
	} else if (protocol == MouseModelPs2::Explorer) {
		frame.resize(4);
		frame[3] = get_reset_wheel_4bit();

		if (buttons.extra_1) {
			bit::set(frame[3], b4);
		}
		if (buttons.extra_2) {
			bit::set(frame[3], b5);
		}
	}

	// Protocol frame was build, no need for a second one
	has_data_for_frame = false;
}

static void maybe_transfer_frame(); // forward declaration

static void delay_handler(uint32_t /*val*/)
{
	delay_running = false;
	delay_expired = true;

	maybe_transfer_frame();
}

static void maybe_start_delay_timer(const uint8_t timer_delay_ms)
{
	if (delay_running) {
		return;
	}
	PIC_AddEvent(delay_handler, timer_delay_ms);
	delay_running = true;
	delay_expired = false;
}

static bool should_report()
{
	return !mode_wrap && !mode_remote && is_reporting;
}

static void maybe_transfer_frame()
{
	if (!delay_expired) {
		maybe_start_delay_timer(delay_ms);
		return;
	}

	if (!has_data_for_frame || !I8042_IsReadyForAuxFrame()) {
		return;
	}

	maybe_start_delay_timer(delay_ms);

	build_protocol_frame();
	if (should_report()) {
		I8042_AddAuxFrame(frame);
	}

	frame.clear();
}

static uint8_t get_status_byte()
{
	union {
		uint8_t _data = 0;

		bit_view<0, 1> right;
		bit_view<1, 1> middle;
		bit_view<2, 1> left;
		// bit 3 - reserved
		bit_view<4, 1> scaling_21;
		bit_view<5, 1> reporting;
		bit_view<6, 1> mode_remote;
		// bit 7 - reserved
	} ret;

	ret.left   = buttons.left;
	ret.right  = buttons.right;
	ret.middle = buttons.middle;

	ret.scaling_21 = scaling_21;
	ret.reporting  = is_reporting;

	return ret._data;
}

static void cmd_get_status()
{
	I8042_AddAuxByte(get_status_byte());
	I8042_AddAuxByte(counts_mm);
	I8042_AddAuxByte(rate_hz);
}

static void cmd_get_dev_id()
{
	I8042_AddAuxByte(static_cast<uint8_t>(protocol));
	reset_counters();
}

static void cmd_poll_frame()
{
	constexpr bool is_polling = true;
	build_protocol_frame(is_polling);
	I8042_AddAuxFrame(frame);
	frame.clear();
	// resetting counters not necessary; frame building process consumes
	// all the data
}

static void cmd_set_resolution(const uint8_t new_counts_mm)
{
	terminate_unlock_sequence();
	reset_counters();

	if (new_counts_mm != 1 && new_counts_mm != 2 &&
	    new_counts_mm != 4 && new_counts_mm != 8) {
		// Invalid parameter, set default
		counts_mm = 4;
	} else {
		counts_mm = new_counts_mm;
	}

	counts_rate = counts_mm / 4.0f;
}

static void cmd_set_sample_rate(const uint8_t new_rate_hz)
{
	reset_counters();

	if (!std::binary_search(list_rates_hz.begin(), list_rates_hz.end(), new_rate_hz)) {
		// Invalid parameter, set default
		terminate_unlock_sequence();
		rate_hz = 100;
	} else {
		rate_hz = new_rate_hz;
	}

	// Update event queue settings and interface information
	auto& interface = MouseInterface::GetInstance(MouseInterfaceId::PS2);
	interface.NotifyInterfaceRate(rate_hz);

	// Handle extended mouse protocol unlock sequences
	auto process_unlock = [](const std::vector<uint8_t>& sequence,
	                         uint8_t& idx,
	                         const MouseModelPs2 potential_protocol) {
		if (sequence[idx] != rate_hz) {
			idx = 0;
		} else if (sequence.size() == ++idx) {
			set_protocol(potential_protocol);
		}
	};

	static const std::vector<uint8_t> unlock_sequence_im = {200, 100, 80};
	static const std::vector<uint8_t> unlock_sequence_xp = {200, 200, 80};

	using enum MouseModelPs2;

	if (mouse_config.model_ps2 == IntelliMouse) {
		process_unlock(unlock_sequence_im, unlock_idx_im, IntelliMouse);
	} else if (mouse_config.model_ps2 == Explorer) {
		process_unlock(unlock_sequence_im, unlock_idx_im, IntelliMouse);
		process_unlock(unlock_sequence_xp, unlock_idx_xp, Explorer);
	}
}

static void cmd_set_scaling_21(const bool enable)
{
	terminate_unlock_sequence();
	scaling_21 = enable;
}

static void cmd_set_reporting(const bool enabled)
{
	terminate_unlock_sequence();
	reset_counters();
	is_reporting = enabled;
}

static void cmd_set_mode_remote(const bool enabled)
{
	terminate_unlock_sequence();
	reset_counters();
	mode_remote = enabled;
}

static void cmd_set_mode_wrap(const bool enabled)
{
	terminate_unlock_sequence();
	reset_counters();
	mode_wrap = enabled;
}

static void cmd_set_defaults()
{
	cmd_set_resolution(4);
	cmd_set_sample_rate(100);
	cmd_set_scaling_21(false);
	cmd_set_reporting(false);

	cmd_set_mode_remote(false);
	cmd_set_mode_wrap(false);

	reset_counters();
}

static void cmd_reset(bool is_startup = false)
{
	cmd_set_defaults();

	set_protocol(MouseModelPs2::Standard, is_startup);
	frame.clear();

	if (is_startup) {
		PIC_SetIRQMask(Mouse::IrqPs2, false);
	} else {
		I8042_AddAuxByte(0xaa); // self-test passed
		cmd_get_dev_id();
	}
}

static void execute_command(const AuxCommand command)
{
	// LOG_INFO("MOUSEPS2: Command 0x%02x", static_cast<int>(command));

	I8042_AddAuxByte(0xfa); // acknowledge

	switch (command) {
	//
	// Commands requiring a parameter
	//
	case AuxCommand::SetResolution: // 0xe8
	case AuxCommand::SetSampleRate: // 0xf3
		current_command = command;
		break;
	//
	// No-parameter commands
	//
	case AuxCommand::SetScaling11: // 0xe6
		// Set mouse movement scaling 1:1
		cmd_set_scaling_21(false);
		break;
	case AuxCommand::SetScaling21: // 0xe7
		// Set mouse movement scaling 2:1
		cmd_set_scaling_21(true);
		break;
	case AuxCommand::GetStatus: // 0xe9
		// Send a 3-byte status packet
		cmd_get_status();
		break;
	case AuxCommand::SetStreamMode: // 0xea
		// Set stream (non-remote) mode, reset movement counters
		cmd_set_mode_remote(false);
		break;
	case AuxCommand::PollFrame: // 0xeb
		// Set mouse data packet, reset movement counters afterwards
		cmd_poll_frame();
		break;
	case AuxCommand::ResetWrapMode: // 0xec
		// Reset wrap mode, reset movement counters
		cmd_set_mode_wrap(false);
		break;
	case AuxCommand::SetWrapMode: // 0xee
		// Set wrap mode, reset movement counters
		cmd_set_mode_wrap(true);
		break;
	case AuxCommand::SetRemoteMode: // 0xf0
		// Set remote (non-stream) mode, reset movement counters
		cmd_set_mode_remote(true);
		break;
	case AuxCommand::GetDevId: // 0xf2
		// Send current protocol ID, reset movement counters
		cmd_get_dev_id();
		break;
	case AuxCommand::EnableDev: // 0xf4
		// Enable reporting in stream mode, reset movement counters
		cmd_set_reporting(true);
		break;
	case AuxCommand::DisableDev: // 0xf5
		// Disable reporting in stream mode, reset movement counters
		cmd_set_reporting(false);
		break;
	case AuxCommand::SetDefaults: // 0xf6
		// Load defaults, reset movement counters, enter stream mode
		cmd_set_defaults();
		break;
	case AuxCommand::ResetDev: // 0xff
		// Enter reset mode
		cmd_reset();
		break;
	default: warn_unknown_command(command); break;
	}
}

static void execute_command(const AuxCommand command, const uint8_t param)
{
	// LOG_INFO("MOUSEPS2: Command 0x%02x, parameter 0x%02x",
	//          static_cast<int>(command), param);

	I8042_AddAuxByte(0xfa); // acknowledge

	switch (command) {
	case AuxCommand::SetResolution: // 0xe8
		// Set mouse resolution, reset movement counters
		cmd_set_resolution(param);
		break;
	case AuxCommand::SetSampleRate: // 0xf3
		// Set mouse resolution, reset movement counters
		// Magic sequences change mouse protocol
		cmd_set_sample_rate(param);
		break;
	default:
		// If we are here, than either this function
		// was wrongly called or it is incomplete
		assert(false);
		break;
	}
}

bool MOUSEPS2_PortWrite(const uint8_t byte)
{
	if (mouse_config.model_ps2 == MouseModelPs2::NoMouse) {
		return false; // no mouse emulated
	}

	maybe_log_mouse_protocol();

	if (byte != static_cast<uint8_t>(AuxCommand::ResetDev) && mode_wrap &&
	    byte != static_cast<uint8_t>(AuxCommand::ResetWrapMode)) {
		I8042_AddAuxByte(byte); // wrap mode, just send bytes back
		return true;
	}

	const auto command = current_command;
	if (command != AuxCommand::None) {
		// Continue execution of previous command
		current_command = AuxCommand::None;
		execute_command(command, byte);
	} else {
		execute_command(static_cast<AuxCommand>(byte));
	}

	return true;
}

void MOUSEPS2_NotifyReadyForFrame()
{
	maybe_transfer_frame();
}

void MOUSEPS2_NotifyMoved(const float x_rel, const float y_rel)
{
	delta_x = MOUSE_ClampRelativeMovement(delta_x + x_rel);
	delta_y = MOUSE_ClampRelativeMovement(delta_y + y_rel);

	has_data_for_frame |= MOUSE_HasAccumulatedInt(delta_x) ||
	                      MOUSE_HasAccumulatedInt(delta_y) ||
	                      vmm_needs_dummy_event;

	maybe_transfer_frame();
	vmm_needs_dummy_event = false;
}

void MOUSEPS2_NotifyButton(const MouseButtons12S new_buttons_12S,
                           const MouseButtonsAll new_buttons_all)
{
	const auto buttons_old = buttons;

	buttons_12S = new_buttons_12S;
	buttons_all = new_buttons_all;
	MOUSEPS2_UpdateButtonSquish();

	has_data_for_frame |= (buttons_old._data != buttons._data) ||
	                      vmm_needs_dummy_event;
	maybe_transfer_frame();
	vmm_needs_dummy_event = false;
}

void MOUSEPS2_NotifyWheel(const float w_rel)
{
	// Note: VMware mouse protocol can support wheel even if the emulated
	// PS/2 mouse does not have it - this works at least with VBADOS v0.67.
	// Thus, we can't skip the function entirely for basic PS/2 mouse.

	constexpr bool skip_delta_update = true;

	const auto old_counter = MOUSE_ConsumeInt8(delta_wheel, skip_delta_update);

	if (protocol == MouseModelPs2::IntelliMouse ||
	    protocol == MouseModelPs2::Explorer) {
		delta_wheel = MOUSE_ClampWheelMovement(delta_wheel + w_rel);
	}

	const auto new_counter = MOUSE_ConsumeInt8(delta_wheel, skip_delta_update);

	has_data_for_frame |= (old_counter != new_counter);
	has_data_for_frame |= vmm_needs_dummy_event;
	maybe_transfer_frame();
	vmm_needs_dummy_event = false;
}

void MOUSEPS2_SetDelay(const uint8_t new_delay_ms)
{
	delay_ms = new_delay_ms;
}

void MOUSEPS2_NotifyInterruptNeeded(const bool immediately)
{
	if (immediately) {
		I8042_TriggerAuxInterrupt();
	} else if (should_report()) {
		vmm_needs_dummy_event = true;
	}
}

// ***************************************************************************
// BIOS interface implementation
// ***************************************************************************

enum class BiosRetVal : uint8_t {
	Success         = 0x00,
	InvalidFunction = 0x01,
	InvalidInput    = 0x02,
	InterfaceError  = 0x03,
	NeedToResend    = 0x04,
	NoDeviceHandler = 0x05,
};

static bool bios_is_flushing   = false;
static uint8_t bios_frame_size = 3;

static bool callback_init    = false;
static uint16_t callback_seg = 0;
static uint16_t callback_ofs = 0;
static RealPt ps2_callback   = 0;

std::vector<uint8_t> bios_buffer = {};

static bool bios_delay_running = false;
static bool bios_delay_expired = true;

static void bios_delay_handler(uint32_t /*val*/)
{
	bios_delay_running = false;
	bios_delay_expired = true;

	PIC_ActivateIRQ(Mouse::IrqPs2);
}

static void bios_maybe_start_delay_timer()
{
	constexpr uint8_t timer_delay_ms = 1;

	if (bios_delay_running) {
		return;
	}

	PIC_AddEvent(bios_delay_handler, timer_delay_ms);
	bios_delay_running = true;
	bios_delay_expired = false;
}

static void bios_cancel_delay_timer()
{
	PIC_RemoveEvents(bios_delay_handler);
	bios_delay_running = false;
	bios_delay_expired = true;
}

static bool bios_enable()
{
	mouse_shared.active_bios = callback_init;
	bios_buffer.clear();
	MOUSE_UpdateGFX();
	cmd_set_reporting(true);
	return callback_init;
}

static bool bios_disable()
{
	bios_cancel_delay_timer();

	mouse_shared.active_bios = false;
	bios_buffer.clear();
	MOUSE_UpdateGFX();
	cmd_set_reporting(false);
	return true;
}

static bool bios_is_aux_byte_waiting()
{
	using namespace bit::literals;

	const auto byte = IO_ReadB(0x64);
	return bit::is(byte, b0) && bit::is(byte, b5);
}

static void bios_flush_aux()
{
	constexpr uint8_t max_wait_ms = 30;

	bios_is_flushing = true;
	bios_buffer.clear();

	bool has_more = true;
	while (has_more) {
		while (bios_is_aux_byte_waiting()) {
			IO_ReadB(port_num_i8042_data);
		}

		const auto start_ticks = PIC_Ticks;

		while (PIC_Ticks - start_ticks <= max_wait_ms) {
			if (CALLBACK_Idle()) {
				break;
			}

			has_more = bios_is_aux_byte_waiting();
			if (has_more) {
				break;
			}
		}
	}

	PIC_SetIRQMask(Mouse::IrqPs2, false);
	bios_is_flushing = false;
}

static void bios_clear_oldest_frame()
{
	bios_buffer = {bios_buffer.begin() + bios_frame_size, bios_buffer.end()};
}

static Bitu bios_ps2_callback_ret()
{
	CPU_Pop16();
	CPU_Pop16();
	CPU_Pop16();
	CPU_Pop16(); // remove 4 words
	return CBRET_NONE;
}

bool MOUSEBIOS_CheckCallback()
{
	if (bios_is_flushing) {
		return false;
	}

	// Least common multiple of supported framed sizes - to minimize chances
	// of guest driver going out-of-sync if we are forced to remove frame
	// from the buffer
	constexpr size_t max_buffer_size = 3 * 4;

	while (1) {
		if (!bios_is_aux_byte_waiting()) {
			// No more AUX data to read
			break;
		}

		const auto byte = IO_ReadB(port_num_i8042_data);
		if (mouse_shared.active_bios && callback_init) {
			bios_buffer.push_back(byte);
			// Do not allow too many old frames in the buffer
			if (bios_buffer.size() >= max_buffer_size) {
				bios_clear_oldest_frame();
			}
		}
	}

	// Check if we have enough data for a callback
	if (bios_buffer.size() < bios_frame_size) {
		return false;
	}

	return true;
}

static bool is_bios_frame_size_supported(const uint8_t frame_size)
{
	return (frame_size == 1) || // VBADOS (VBMOUSE.EXE) preferred size
	       (frame_size == 3) || // standard size, has to be supported
	       (frame_size == 4);   // CuteMouse 2.1 uses this for wheel mice
}

void MOUSEBIOS_DoCallback()
{
	assert(is_bios_frame_size_supported(bios_frame_size));
	assert(bios_buffer.size() >= bios_frame_size);

	switch (bios_frame_size) {
	case 1:
		CPU_Push16(bios_buffer[0]);
		CPU_Push16(0);
		CPU_Push16(0);
		break;
	case 3:
		CPU_Push16(bios_buffer[0]);
		CPU_Push16(bios_buffer[1]);
		CPU_Push16(bios_buffer[2]);
		break;
	case 4: {
		const auto word_0 = bios_buffer[0] + (bios_buffer[1] << 8);
		CPU_Push16(static_cast<uint16_t>(word_0));
		CPU_Push16(bios_buffer[2]);
		CPU_Push16(bios_buffer[3]);
		break;
	}
	default: assert(false); break;
	}
	CPU_Push16(0u);

	bios_clear_oldest_frame();

	CPU_Push16(RealSegment(ps2_callback));
	CPU_Push16(RealOffset(ps2_callback));
	SegSet16(cs, callback_seg);
	reg_ip = callback_ofs;
}

void MOUSEBIOS_FinalizeInterrupt()
{
	// It is possible that before our interrupt got handled, another full
	// packet arrived from the simulated PS/2 hardware
	if (MOUSEBIOS_CheckCallback()) {
		bios_maybe_start_delay_timer();
	} else {
		bios_cancel_delay_timer();
	}
}

void MOUSEBIOS_Subfunction_C2() // INT 15h, AH = 0xc2
{
	auto set_return_value = [](const BiosRetVal value) {
		CALLBACK_SCF(value != BiosRetVal::Success);
		reg_ah = static_cast<uint8_t>(value);
	};

	auto is_return_success = []() {
		return reg_ah == static_cast<uint8_t>(BiosRetVal::Success);
	};

	if (mouse_config.model_ps2 == MouseModelPs2::NoMouse) {
		set_return_value(BiosRetVal::InterfaceError);
		return;
	}

	maybe_log_mouse_protocol();

	switch (reg_al) {
	case 0x00: // enable/disable mouse
		if (reg_bh == 0) {
			// disable mouse
			bios_disable();
			set_return_value(BiosRetVal::Success);
		} else if (reg_bh == 0x01) {
			// enable mouse
			if (!bios_enable()) {
				set_return_value(BiosRetVal::NoDeviceHandler);
				break;
			}
			set_return_value(BiosRetVal::Success);
		} else {
			set_return_value(BiosRetVal::InvalidFunction);
		}
		break;
	case 0x01: // reset
		// VBADOS seems to expect VMware interface to get dectivated
		MOUSEVMM_DeactivateAll();
		cmd_reset();
		bios_disable();
		cmd_set_defaults();
		reg_bx = 0x00aa;
		set_return_value(BiosRetVal::Success);
		break;
	case 0x02: // set sampling rate
		if (reg_bh >= list_rates_hz.size()) {
			set_return_value(BiosRetVal::InvalidInput);
			break;
		}
		cmd_set_sample_rate(list_rates_hz[reg_bh]);
		set_return_value(BiosRetVal::Success);
		break;
	case 0x03: // set resolution
		if (reg_bh >= list_resolutions.size()) {
			set_return_value(BiosRetVal::InvalidInput);
			break;
		}
		cmd_set_resolution(list_resolutions[reg_bh]);
		set_return_value(BiosRetVal::Success);
		break;
	case 0x04: // get mouse type/protocol
		reg_bh = static_cast<uint8_t>(protocol);
		set_return_value(BiosRetVal::Success);
		break;
	case 0x05: // initialize
		if (is_bios_frame_size_supported(reg_bh)) {
			// NOTE: if you want to support more frame sizes, do not
			// forget to update 'max_buffer_size' constant in
			// 'MOUSEBIOS_CheckCallback' routine!
			bios_frame_size = reg_bh;
			bios_disable();
			cmd_set_defaults();
			set_return_value(BiosRetVal::Success);
		} else {
			set_return_value(BiosRetVal::InvalidInput);
		}
		break;
	case 0x06: // extended commands
		if (reg_bh == 0x00) {
			// get mouse status
			reg_bx = get_status_byte();
			reg_cx = counts_mm;
			reg_dx = rate_hz;
			set_return_value(BiosRetVal::Success);
		} else if (reg_bh == 0x01 || reg_bh == 0x02) {
			// set scaling 2:1
			cmd_set_scaling_21(reg_bh == 0x02);
			set_return_value(BiosRetVal::Success);
		} else {
			set_return_value(BiosRetVal::InvalidFunction);
		}
		break;
	case 0x07: // set callback
		if ((SegValue(es) == 0) && (reg_bx == 0)) {
			callback_init = false;
		} else {
			callback_init = true;
			callback_seg  = SegValue(es);
			callback_ofs  = reg_bx;
		}
		bios_buffer.clear();
		set_return_value(BiosRetVal::Success);
		break;
	default: set_return_value(BiosRetVal::InvalidFunction); break;
	}

	if (is_return_success()) {
		bios_flush_aux();
	}
}

void MOUSEPS2_Init()
{
	// Callback for ps2 user callback handling
	const auto call_ps2 = CALLBACK_Allocate();
	CALLBACK_Setup(call_ps2, &bios_ps2_callback_ret, CB_RETF, "ps2 bios mouse callback");
	ps2_callback = CALLBACK_RealPointer(call_ps2);

	constexpr bool is_startup = true;
	cmd_reset(is_startup);
}
