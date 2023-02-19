/*
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
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

#include "checks.h"
#include "inout.h"
#include "math_utils.h"
#include "pic.h"
#include "regs.h"

CHECK_NARROWING();

// VMware mouse interface passes both absolute mouse position and button
// state to the guest side driver, but still relies on PS/2 interface,
// which has to be used to listen for events

// Reference:
// - https://wiki.osdev.org/VMware_tools
// - https://wiki.osdev.org/VirtualBox_Guest_Additions (planned support)
// Drivers:
// - https://git.javispedro.com/cgit/vbados.git
// - https://github.com/NattyNarwhal/vmwmouse (warning: release 0.1 is unstable)
// - https://git.javispedro.com/cgit/vbmouse.git (planned support)

enum class VMwareCmd : uint16_t {
	GetVersion        = 10,
	AbsPointerData    = 39,
	AbsPointerStatus  = 40,
	AbsPointerCommand = 41,
};

enum class VMwareAbsPointer : uint32_t {
	Enable   = 0x45414552,
	Relative = 0xF5,
	Absolute = 0x53424152,
};

union VMwareButtons {
	uint8_t data = 0;
	bit_view<5, 1> left;
	bit_view<4, 1> right;
	bit_view<3, 1> middle;
};

static constexpr uint32_t VMWARE_MAGIC = 0x564D5868u;  // magic number for all VMware calls
static constexpr uint32_t ABS_UPDATED  = 4; // tells about new pointer position
static constexpr uint32_t ABS_NOT_UPDATED = 0;

static bool use_relative = true; // true = ignore absolute mouse position, use relative
static bool is_input_raw = true; // true = no host mouse acceleration pre-applied

static bool updated = false;       // true = mouse state update waits to be picked up
static VMwareButtons buttons;      // state of mouse buttons, in VMware format
static uint16_t scaled_x = 0x7fff; // absolute position scaled from 0 to 0xffff
static uint16_t scaled_y = 0x7fff; // 0x7fff is a center position
static int8_t counter_w  = 0;      // wheel movement counter

static float pos_x = 0.0f;
static float pos_y = 0.0f;

// Multiply scale by 0.02f to put acceleration_vmm in a reasonable
// range, similar to sensitivity_dos or sensitivity_vmm)
constexpr float acceleration_multiplier = 0.02f;
static MouseSpeedCalculator speed_xy(acceleration_multiplier *mouse_predefined.acceleration_vmm);

// ***************************************************************************
// VMware interface implementation
// ***************************************************************************

static void MOUSEVMM_Activate()
{
	if (!mouse_shared.active_vmm) {
		mouse_shared.active_vmm = true;
		LOG_MSG("MOUSE (PS/2): VMware protocol enabled");
		MOUSEPS2_UpdateButtonSquish();
		MOUSE_UpdateGFX();
		if (use_relative) {
			// If no seamless integration, prepare sane
			// cursor star position
			pos_x    = static_cast<float>(mouse_shared.resolution_x) / 2.0f;
			pos_y    = static_cast<float>(mouse_shared.resolution_y) / 2.0f;
			scaled_x = 0;
			scaled_y = 0;
			MOUSEPS2_NotifyMovedDummy();
		}
	}
	buttons.data = 0;
	counter_w    = 0;
}

void MOUSEVMM_Deactivate()
{
	if (mouse_shared.active_vmm) {
		mouse_shared.active_vmm = false;
		LOG_MSG("MOUSE (PS/2): VMware protocol disabled");
		MOUSEPS2_UpdateButtonSquish();
		MOUSE_UpdateGFX();
	}
	buttons.data = 0;
	counter_w    = 0;
}

void MOUSEVMM_NotifyInputType(const bool new_use_relative,
                              const bool new_is_input_raw)
{
	use_relative = new_use_relative;
	is_input_raw = new_is_input_raw;
}

static void cmd_get_version()
{
	reg_eax = 0; // protocol version
	reg_ebx = VMWARE_MAGIC;
}

static void cmd_abs_pointer_data()
{
	reg_eax = buttons.data;
	reg_ebx = scaled_x;
	reg_ecx = scaled_y;
	reg_edx = static_cast<uint32_t>((counter_w >= 0) ? counter_w
	                                                 : 0x100 + counter_w);

	counter_w = 0;
}

static void cmd_abs_pointer_status()
{
	reg_eax = updated ? ABS_UPDATED : ABS_NOT_UPDATED;
	updated = false;
}

static void cmd_abs_pointer_command()
{
	switch (static_cast<VMwareAbsPointer>(reg_ebx)) {
	case VMwareAbsPointer::Enable: break; // can be safely ignored
	case VMwareAbsPointer::Relative: MOUSEVMM_Deactivate(); break;
	case VMwareAbsPointer::Absolute: MOUSEVMM_Activate(); break;
	default:
		LOG_WARNING("MOUSE (PS/2): unimplemented VMware subcommand 0x%08x",
		            reg_ebx);
		break;
	}
}

static uint32_t port_read_vmware(const io_port_t, const io_width_t)
{
	if (reg_eax != VMWARE_MAGIC)
		return 0;

	switch (static_cast<VMwareCmd>(reg_cx)) {
	case VMwareCmd::GetVersion: cmd_get_version(); break;
	case VMwareCmd::AbsPointerData: cmd_abs_pointer_data(); break;
	case VMwareCmd::AbsPointerStatus: cmd_abs_pointer_status(); break;
	case VMwareCmd::AbsPointerCommand: cmd_abs_pointer_command(); break;
	default:
		LOG_WARNING("MOUSE (PS/2): unimplemented VMware command 0x%08x",
		            reg_ecx);
		break;
	}

	return reg_eax;
}

void MOUSEVMM_NotifyMoved(const float x_rel, const float y_rel,
                          const uint32_t x_abs, const uint32_t y_abs)
{
	if (!mouse_shared.active_vmm) {
		return;
	}

	speed_xy.Update(std::sqrt(x_rel * x_rel + y_rel * y_rel));

	const auto old_scaled_x = scaled_x;
	const auto old_scaled_y = scaled_y;

	auto calculate = [](float &position,
	                    const float relative,
	                    const uint32_t absolute,
	                    const uint32_t resolution) {
		assert(resolution > 1u);

		if (use_relative) {
			// Mouse is captured or mapped, there is no need for
			// pointer integration with host OS - we can use
			// relative movement with configured sensitivity and
			// (for raw mouse input) our built-in pointer
			// acceleration model

			if (is_input_raw) {
				const auto coeff = MOUSE_GetBallisticsCoeff(speed_xy.Get());
				position += MOUSE_ClampRelativeMovement(relative * coeff);
			} else
				position += MOUSE_ClampRelativeMovement(relative);
		} else
			// Cursor position controlled by the host OS
			position = static_cast<float>(absolute);

		position = std::clamp(position, 0.0f, static_cast<float>(resolution));

		const auto scale = static_cast<float>(UINT16_MAX) /
		                   static_cast<float>(resolution - 1);
		const auto tmp = std::min(static_cast<uint32_t>(UINT16_MAX),
		                          static_cast<uint32_t>(std::lround(position * scale)));

		return static_cast<uint16_t>(tmp);
	};

	scaled_x = calculate(pos_x, x_rel, x_abs, mouse_shared.resolution_x);
	scaled_y = calculate(pos_y, y_rel, y_abs, mouse_shared.resolution_y);

	// Filter out unneeded events (like sub-pixel mouse movements,
	// which won't change guest side mouse state)
	if (GCC_UNLIKELY(old_scaled_x == scaled_x && old_scaled_y == scaled_y)) {
		return;
	}

	updated = true;
	MOUSEPS2_NotifyMovedDummy();
}

void MOUSEVMM_NotifyButton(const MouseButtons12S buttons_12S)
{
	if (!mouse_shared.active_vmm) {
		return;
	}

	const auto old_buttons = buttons;
	buttons.data           = 0;

	// Direct assignment of .data is not possible, as bit layout is different
	buttons.left   = static_cast<bool>(buttons_12S.left);
	buttons.right  = static_cast<bool>(buttons_12S.right);
	buttons.middle = static_cast<bool>(buttons_12S.middle);

	if (GCC_UNLIKELY(old_buttons.data == buttons.data)) {
		return;
	}

	updated = true;
	MOUSEPS2_NotifyMovedDummy();
}

void MOUSEVMM_NotifyWheel(const int16_t w_rel)
{
	if (!mouse_shared.active_vmm) {
		return;
	}

	const auto old_counter_w = counter_w;
	counter_w = clamp_to_int8(static_cast<int32_t>(counter_w + w_rel));

	if (GCC_UNLIKELY(old_counter_w == counter_w)) {
		return;
	}

	updated = true;
	MOUSEPS2_NotifyMovedDummy();
}

void MOUSEVMM_NewScreenParams(const uint32_t x_abs, const uint32_t y_abs)
{
	MOUSEVMM_NotifyMoved(0.0f, 0.0f, x_abs, y_abs);
}

void MOUSEVMM_Init()
{
	IO_RegisterReadHandler(port_num_vmware,
	                       port_read_vmware,
	                       io_width_t::dword);
}
