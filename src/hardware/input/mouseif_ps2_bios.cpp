/*
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
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

#include "mouse.h"
#include "mouse_config.h"
#include "mouse_interfaces.h"

#include <algorithm>
#include <cmath>

#include "bios.h"
#include "bitops.h"
#include "callback.h"
#include "checks.h"
#include "cpu.h"
#include "intel8042.h"
#include "math_utils.h"
#include "pic.h"
#include "regs.h"

#include "../../ints/int10.h"

using namespace bit::literals;

CHECK_NARROWING();

// Here the BIOS abstraction layer for the PS/2 AUX port mouse is implemented.
// PS/2 direct hardware access is not supported yet.

// Reference:
// - https://www.digchip.com/datasheets/parts/datasheet/196/HT82M30A-pdf.php
// - https://isdaman.com/alsos/hardware/mouse/ps2interface.htm
// - https://wiki.osdev.org/Mouse_Input

static const std::vector<uint8_t> list_rates_hz = {
        10, 20, 40, 60, 80, 100, 200 // PS/2 mouse sampling rates
};

static const std::vector<uint8_t> list_resolutions = {
        1, 2, 4, 8 // PS/2 mouse resolution values
};

static MouseButtonsAll buttons;     // currently visible button state
static MouseButtonsAll buttons_all; // state of all 5 buttons as on the host side
static MouseButtons12S buttons_12S; // buttons with 3/4/5 quished together

static float delta_x = 0.0f; // accumulated mouse movement since last reported
static float delta_y = 0.0f;
static int8_t counter_w = 0; // mouse wheel counter

static MouseModelPS2 protocol = MouseModelPS2::Standard;
static uint8_t unlock_idx_im = 0; // sequence index for unlocking extended protocol
static uint8_t unlock_idx_xp = 0;

static uint8_t packet[4] = {0}; // packet to be transferred via BIOS interface

static uint8_t rate_hz = 0;     // maximum rate at which the mouse state is updated
static bool scaling_21 = false; // NOTE: scaling only works for stream mode,
                                // not when reading data manually!
                                // https://www3.tuhh.de/osg/Lehre/SS21/V_BSB/doc/ps2mouse.html

static uint8_t counts_mm = 0;    // counts per mm
static float counts_rate = 0.0f; // 1.0 is 4 counts per mm

// ***************************************************************************
// PS/2 hardware mouse implementation
// ***************************************************************************

void MOUSEPS2_UpdateButtonSquish()
{
	// - if VMware compatible driver is enabled, never try to report
	//   mouse buttons 4 and 5, that would be asking for trouble
	// - for PS/2 modes other than IntelliMouse Explorer there is
	//   no standard way to report buttons 4 and 5

	const bool squish = mouse_shared.active_vmm ||
	                    (protocol != MouseModelPS2::Explorer);
	buttons.data = squish ? buttons_12S.data : buttons_all.data;
}

static void terminate_unlock_sequence()
{
	unlock_idx_im = 0;
	unlock_idx_xp = 0;
}

static void set_protocol(const MouseModelPS2 new_protocol)
{
	terminate_unlock_sequence();

	static bool first_time = true;
	if (first_time || protocol != new_protocol) {
		first_time                = false;
		protocol                  = new_protocol;
		const char *protocol_name = nullptr;
		switch (protocol) {
		case MouseModelPS2::Standard:
			protocol_name = "Standard, 3 buttons";
			break;
		case MouseModelPS2::IntelliMouse:
			protocol_name = "IntelliMouse, wheel, 3 buttons";
			break;
		case MouseModelPS2::Explorer:
			protocol_name = "IntelliMouse Explorer, wheel, 5 buttons";
			break;
		default: break;
		}

		LOG_MSG("MOUSE (PS/2): %s", protocol_name);

		packet[0] = 0;
		packet[1] = 0;
		packet[2] = 0;
		packet[3] = 0;

		MOUSEPS2_UpdateButtonSquish();
	}
}

static uint8_t get_reset_wheel_4bit()
{
	const int8_t tmp = std::clamp(counter_w,
	                              static_cast<int8_t>(-0x08),
	                              static_cast<int8_t>(0x07));
	counter_w = 0; // reading always clears the counter

	// 0x0f for -1, 0x0e for -2, etc.
	return static_cast<uint8_t>((tmp >= 0) ? tmp : 0x10 + tmp);
}

static uint8_t get_reset_wheel_8bit()
{
	const auto tmp = counter_w;
	counter_w = 0; // reading always clears thecounter

	// 0xff for -1, 0xfe for -2, etc.
	return static_cast<uint8_t>((tmp >= 0) ? tmp : 0x100 + tmp);
}

static int16_t get_scaled_movement(const int16_t d)
{
	if (!scaling_21)
		return d;

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
}

static void reset_counters()
{
	delta_x   = 0.0f;
	delta_y   = 0.0f;
	counter_w = 0;
}

void MOUSEPS2_UpdatePacket()
{
	union {
		uint8_t data = 0x08;

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

	auto dx = static_cast<int16_t>(std::round(delta_x));
	auto dy = static_cast<int16_t>(std::round(delta_y));

	delta_x -= dx;
	delta_y -= dy;

	dx = get_scaled_movement(dx);
	dy = get_scaled_movement(static_cast<int16_t>(-dy));

	if (protocol == MouseModelPS2::Explorer) {
		// There is no overflow for 5-button mouse protocol, see
		// HT82M30A datasheet
		dx = std::clamp(dx,
		                static_cast<int16_t>(-UINT8_MAX),
		                static_cast<int16_t>(UINT8_MAX));
		dy = std::clamp(dy,
		                static_cast<int16_t>(-UINT8_MAX),
		                static_cast<int16_t>(UINT8_MAX));
	} else {
		if ((dx > UINT8_MAX) || (dx < -UINT8_MAX))
			mdat.overflow_x = 1;
		if ((dy > UINT8_MAX) || (dy < -UINT8_MAX))
			mdat.overflow_y = 1;
	}

	dx = static_cast<int16_t>(dx % (UINT8_MAX + 1));
	if (dx < 0) {
		dx          = static_cast<int16_t>(dx + UINT8_MAX + 1);
		mdat.sign_x = 1;
	}

	dy = static_cast<int16_t>(dy % (UINT8_MAX + 1));
	if (dy < 0) {
		dy          = static_cast<int16_t>(dy + UINT8_MAX + 1);
		mdat.sign_y = 1;
	}

	packet[0] = mdat.data;
	packet[1] = static_cast<uint8_t>(dx);
	packet[2] = static_cast<uint8_t>(dy);

	if (protocol == MouseModelPS2::IntelliMouse)
		packet[3] = get_reset_wheel_8bit();
	else if (protocol == MouseModelPS2::Explorer) {
		packet[3] = get_reset_wheel_4bit();
		if (buttons.extra_1)
			bit::set(packet[3], b4);
		if (buttons.extra_2)
			bit::set(packet[3], b5);
	} else
		packet[3] = 0;
}

static void cmd_set_resolution(const uint8_t new_counts_mm)
{
	terminate_unlock_sequence();

	if (new_counts_mm != 1 && new_counts_mm != 2 && new_counts_mm != 4 &&
	    new_counts_mm != 8)
		// Invalid parameter, set default
		counts_mm = 4;
	else
		counts_mm = new_counts_mm;

	counts_rate = counts_mm / 4.0f;
}

static void cmd_set_sample_rate(const uint8_t new_rate_hz)
{
	reset_counters();

	if (!std::binary_search(list_rates_hz.begin(), list_rates_hz.end(), new_rate_hz)) {
		// Invalid parameter, set default
		terminate_unlock_sequence();
		rate_hz = 100;
	} else
		rate_hz = new_rate_hz;

	// Update event queue settings and interface information
	MouseInterface::GetPS2()->NotifyInterfaceRate(rate_hz);

	// Handle extended mouse protocol unlock sequences
	auto process_unlock = [](const std::vector<uint8_t> &sequence,
	                         uint8_t &idx,
	                         const MouseModelPS2 potential_protocol) {
		if (sequence[idx] != rate_hz)
			idx = 0;
		else if (sequence.size() == ++idx) {
			set_protocol(potential_protocol);
		}
	};

	static const std::vector<uint8_t> unlock_sequence_im = {200, 100, 80};
	static const std::vector<uint8_t> unlock_sequence_xp = {200, 200, 80};

	if (mouse_config.model_ps2 == MouseModelPS2::IntelliMouse)
		process_unlock(unlock_sequence_im,
		               unlock_idx_im,
		               MouseModelPS2::IntelliMouse);
	else if (mouse_config.model_ps2 == MouseModelPS2::Explorer) {
		process_unlock(unlock_sequence_im,
		               unlock_idx_im,
		               MouseModelPS2::IntelliMouse);
		process_unlock(unlock_sequence_xp, unlock_idx_xp, MouseModelPS2::Explorer);
	}
}

static void cmd_set_defaults()
{
	cmd_set_resolution(4);
	cmd_set_sample_rate(100);

	MOUSEPS2_UpdateButtonSquish();
}

static void cmd_reset()
{
	cmd_set_defaults();
	set_protocol(MouseModelPS2::Standard);
	reset_counters();
}

static void cmd_set_scaling_21(const bool enable)
{
	terminate_unlock_sequence();

	scaling_21 = enable;
}

bool MOUSEPS2_PortWrite([[maybe_unused]] const uint8_t byte)
{
	return false; // TODO: implement
}

void MOUSEPS2_NotifyReadyForFrame()
{
	// TODO: implement
}

bool MOUSEPS2_NotifyMoved(const float x_rel, const float y_rel)
{
	delta_x = MOUSE_ClampRelativeMovement(delta_x + x_rel);
	delta_y = MOUSE_ClampRelativeMovement(delta_y + y_rel);

	// Threshold the accumulated movement needs to cross
	// to be considered significant enough for new event
	constexpr float threshold = 0.5f;

	return (std::fabs(delta_x) >= threshold) ||
	       (std::fabs(delta_y) >= threshold);
}

bool MOUSEPS2_NotifyButton(const MouseButtons12S new_buttons_12S,
                           const MouseButtonsAll new_buttons_all)
{
	const auto buttons_old = buttons;

	buttons_12S = new_buttons_12S;
	buttons_all = new_buttons_all;
	MOUSEPS2_UpdateButtonSquish();

	return (buttons_old.data != buttons.data);
}

bool MOUSEPS2_NotifyWheel(const int16_t w_rel)
{
	if (protocol != MouseModelPS2::IntelliMouse &&
	    protocol != MouseModelPS2::Explorer)
		return false;

	auto old_counter_w = counter_w;
	counter_w = clamp_to_int8(static_cast<int32_t>(counter_w + w_rel));

	return (old_counter_w != counter_w);
}

// ***************************************************************************
// BIOS interface implementation
// ***************************************************************************

// TODO: Once the the physical PS/2 mouse is implemented, BIOS has to be changed
// to interact with I/O ports, not to call PS/2 hardware implementation routines
// directly (no Cmd* calls should be present in BIOS) - otherwise the
// complicated Windows 3.x mouse/keyboard support will get confused. See:
// https://www.os2museum.com/wp/jumpy-ps2-mouse-in-enhanced-mode-windows-3-x/
// Other solution might be to put interrupt lines low ion BIOS implementation,
// like this is done in DOSBox X.

static bool packet_4bytes = false;

static bool callback_init    = false;
static uint16_t callback_seg = 0;
static uint16_t callback_ofs = 0;
static RealPt ps2_callback   = 0;

void MOUSEBIOS_Reset()
{
	cmd_reset();
	PIC_SetIRQMask(mouse_predefined.IRQ_PS2, false); // lower IRQ line
	MOUSEVMM_Deactivate(); // VBADOS seems to expect this
}

void MOUSEBIOS_SetCallback(const uint16_t pseg, const uint16_t pofs)
{
	if ((pseg == 0) && (pofs == 0)) {
		callback_init = false;
	} else {
		callback_init = true;
		callback_seg  = pseg;
		callback_ofs  = pofs;
	}
}

bool MOUSEBIOS_SetPacketSize(const uint8_t packet_size)
{
	if (packet_size == 3)
		packet_4bytes = false;
	else if (packet_size == 4)
		packet_4bytes = true;
	else
		return false; // unsupported packet size

	return true;
}

bool MOUSEBIOS_SetSampleRate(const uint8_t rate_id)
{
	if (rate_id >= list_rates_hz.size())
		return false;

	cmd_set_sample_rate(list_rates_hz[rate_id]);
	return true;
}

bool MOUSEBIOS_SetResolution(const uint8_t res_id)
{
	if (res_id >= list_resolutions.size())
		return false;

	cmd_set_sample_rate(list_resolutions[res_id]);
	return true;
}

void MOUSEBIOS_SetScaling21(const bool enable)
{
	cmd_set_scaling_21(enable);
}

bool MOUSEBIOS_Enable()
{
	mouse_shared.active_bios = callback_init;
	MOUSE_UpdateGFX();
	return callback_init;
}

bool MOUSEBIOS_Disable()
{
	mouse_shared.active_bios = false;
	MOUSE_UpdateGFX();
	return true;
}

uint8_t MOUSEBIOS_GetResolution()
{
	return counts_mm;
}

uint8_t MOUSEBIOS_GetSampleRate()
{
	return rate_hz;
}

uint8_t MOUSEBIOS_GetStatus()
{
	union {
		uint8_t data = 0;

		bit_view<0, 1> left;
		bit_view<1, 1> right;
		bit_view<2, 1> middle;
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
	ret.reporting  = 1;

	return ret.data;
}

uint8_t MOUSEBIOS_GetProtocol()
{
	return static_cast<uint8_t>(protocol);
}

static Bitu callback_ret()
{
	CPU_Pop16();
	CPU_Pop16();
	CPU_Pop16();
	CPU_Pop16(); // remove 4 words
	return CBRET_NONE;
}

Bitu MOUSEBIOS_DoCallback()
{
	if (!packet_4bytes) {
		CPU_Push16(packet[0]);
		CPU_Push16(packet[1]);
		CPU_Push16(packet[2]);
	} else {
		CPU_Push16(static_cast<uint16_t>((packet[0] + packet[1] * 0x100)));
		CPU_Push16(packet[2]);
		CPU_Push16(packet[3]);
	}
	CPU_Push16(0u);

	CPU_Push16(RealSeg(ps2_callback));
	CPU_Push16(RealOff(ps2_callback));
	SegSet16(cs, callback_seg);
	reg_ip = callback_ofs;

	return CBRET_NONE;
}

void MOUSEPS2_Init()
{
	// Callback for ps2 user callback handling
	const auto call_ps2 = CALLBACK_Allocate();
	CALLBACK_Setup(call_ps2, &callback_ret, CB_RETF, "ps2 bios callback");
	ps2_callback = CALLBACK_RealPointer(call_ps2);

	MOUSEBIOS_Reset();
}
