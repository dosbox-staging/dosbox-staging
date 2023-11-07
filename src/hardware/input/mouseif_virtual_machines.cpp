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
#include "math_utils.h"

CHECK_NARROWING();

// Virtual Machine Manager mouses interfaces passe absolute mouse position and
// (in case of VMware) button state to the guest side driver, but they still
// depend on PS/2 interface, which has to be used to listen for events

// Drivers:
// - https://git.javispedro.com/cgit/vbados.git
// - https://github.com/NattyNarwhal/vmwmouse (warning: release 0.1 is unstable)
// - https://git.javispedro.com/cgit/vbmouse.git

union VmWareButtons {
	uint8_t _data = 0;
	bit_view<5, 1> left;
	bit_view<4, 1> right;
	bit_view<3, 1> middle;
};

static struct {
	bool is_active = false;

	// true = guest driver wants host mouse pointer visible
	bool wants_pointer = false;

} virtualbox;

static struct {
	bool is_active = false;

	bool updated = false;       // true = state update waits to be picked up
	VmWareButtons buttons = {}; // state of mouse buttons, in VMware format
	int8_t counter_w      = 0;  // wheel movement counter
} vmware;

static bool use_relative = true; // true = ignore absolute mouse position, use relative
static bool is_input_raw = true; // true = no host mouse acceleration pre-applied

static float pos_x = 0.0f; // absolute mouse position in guest-side pixels
static float pos_y = 0.0f;

static uint16_t scaled_x = 0x7fff; // absolute position scaled from 0 to 0xffff
static uint16_t scaled_y = 0x7fff; // 0x7fff is a center position

// Multiply scale by 0.02f to put acceleration_vmm in a reasonable
// range, similar to sensitivity_dos or sensitivity_vmm)
constexpr float acceleration_multiplier = 0.02f;
static MouseSpeedCalculator speed_xy(acceleration_multiplier *mouse_predefined.acceleration_vmm);

// ***************************************************************************
// Internal helper routines
// ***************************************************************************

static void maybe_check_remove_mappings()
{
	if (!mouse_shared.vmm_wants_pointer) {
		return;
	}

	bool needs_warning = false;
	for (const auto& interface : mouse_interfaces) {
		if (interface->IsMapped()) {
			needs_warning = true;
			interface->ConfigUnMap();
		}
	}

	if (needs_warning) {
		LOG_WARNING("MOUSE (VMM): Mappings removed due to incompatible VirtualBox driver");
	}
}

// ***************************************************************************
// Requests from Virtual Machine Manager guest side drivers
// ***************************************************************************

bool MOUSEVMM_IsSupported(const MouseVmmProtocol protocol)
{
	if (mouse_config.model_ps2 == MouseModelPS2::NoMouse) {
		return false;
	}

	switch (protocol) {
	case MouseVmmProtocol::VmWare:
		return mouse_config.is_vmware_mouse_enabled;
	case MouseVmmProtocol::VirtualBox:
		return mouse_config.is_virtualbox_mouse_enabled;
	default: assert(false);
	}

	return false;
}

void MOUSEVMM_Activate(const MouseVmmProtocol protocol)
{
	bool is_activating = false;

	if (protocol == MouseVmmProtocol::VirtualBox && !virtualbox.is_active) {
		virtualbox.is_active = true;
		is_activating        = true;
		LOG_MSG("MOUSE (PS/2): VirtualBox protocol enabled");
		mouse_shared.vmm_wants_pointer = virtualbox.wants_pointer;
		maybe_check_remove_mappings();
	} else if (protocol == MouseVmmProtocol::VmWare && !vmware.is_active) {
		vmware.is_active = true;
		is_activating    = true;
		LOG_MSG("MOUSE (PS/2): VMware protocol enabled");
	}

	if (is_activating) {
		mouse_shared.active_vmm = true;
		MOUSEPS2_UpdateButtonSquish();
		MOUSE_UpdateGFX();
		if (use_relative) {
			// If no seamless integration was in effect,
			// prepare sane cursor start position
			pos_x = static_cast<float>(mouse_shared.resolution_x) / 2.0f;
			pos_y = static_cast<float>(mouse_shared.resolution_y) / 2.0f;
			scaled_x = 0;
			scaled_y = 0;
			MOUSEPS2_NotifyMovedDummy();
		}
	}

	if (protocol == MouseVmmProtocol::VmWare) {
		vmware.buttons._data = 0;
		vmware.counter_w     = 0;
	}
}

void MOUSEVMM_Deactivate(const MouseVmmProtocol protocol)
{
	bool is_deactivating  = false;
	const bool was_active = mouse_shared.active_vmm;

	if (protocol == MouseVmmProtocol::VirtualBox && virtualbox.is_active) {
		virtualbox.is_active = false;
		is_deactivating      = true;
		LOG_MSG("MOUSE (PS/2): VirtualBox protocol disabled");
		mouse_shared.vmm_wants_pointer = false;
	} else if (protocol == MouseVmmProtocol::VmWare && vmware.is_active) {
		vmware.is_active = false;
		is_deactivating  = true;
		LOG_MSG("MOUSE (PS/2): VMware protocol disabled");
	}

	if (is_deactivating && was_active) {
		mouse_shared.active_vmm = virtualbox.is_active || vmware.is_active;
		MOUSEPS2_UpdateButtonSquish();
		MOUSE_UpdateGFX();
	}

	if (protocol == MouseVmmProtocol::VmWare) {
		vmware.buttons._data = 0;
		vmware.counter_w     = 0;
	}
}

void MOUSEVMM_DeactivateAll()
{
	MOUSEVMM_Deactivate(MouseVmmProtocol::VirtualBox);
	MOUSEVMM_Deactivate(MouseVmmProtocol::VmWare);
}

// ***************************************************************************
// VirtualBox specific requests
// ***************************************************************************

void MOUSEVMM_GetPointerStatus(MouseVirtualBoxPointerStatus& status)
{
	status.absolute_x = scaled_x;
	status.absolute_y = scaled_y;
}

void MOUSEVMM_SetPointerVisible_VirtualBox(const bool is_visible)
{
	if (virtualbox.wants_pointer != is_visible) {
		virtualbox.wants_pointer = is_visible;
		if (virtualbox.is_active) {
			mouse_shared.vmm_wants_pointer = is_visible;
			maybe_check_remove_mappings();
			MOUSE_UpdateGFX();
		}
	}
}

// ***************************************************************************
// VMware specific requests
// ***************************************************************************

bool MOUSEVMM_CheckIfUpdated_VmWare()
{
	const bool result = vmware.updated;
	vmware.updated    = false;

	return result;
}

void MOUSEVMM_GetPointerStatus(MouseVmWarePointerStatus& status)
{
	status.absolute_x = scaled_x;
	status.absolute_y = scaled_y;

	status.buttons       = vmware.buttons._data;
	status.wheel_counter = static_cast<uint8_t>(vmware.counter_w);

	vmware.counter_w = 0;
}

// ***************************************************************************
// Notifications from mouse subsystem
// ***************************************************************************

void MOUSEVMM_NotifyInputType(const bool new_use_relative, const bool new_is_input_raw)
{
	use_relative = new_use_relative;
	is_input_raw = new_is_input_raw;
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

	vmware.updated = vmware.is_active;
	MOUSEPS2_NotifyMovedDummy();
}

void MOUSEVMM_NotifyButton(const MouseButtons12S buttons_12S)
{
	if (!vmware.is_active) { // only needed by VMware
		return;
	}

	const auto old_buttons = vmware.buttons;
	vmware.buttons._data   = 0;

	// Direct assignment of ._data is not possible, as bit layout is different
	vmware.buttons.left   = static_cast<bool>(buttons_12S.left);
	vmware.buttons.right  = static_cast<bool>(buttons_12S.right);
	vmware.buttons.middle = static_cast<bool>(buttons_12S.middle);

	if (GCC_UNLIKELY(old_buttons._data == vmware.buttons._data)) {
		return;
	}

	vmware.updated = vmware.is_active;
	MOUSEPS2_NotifyMovedDummy();
}

void MOUSEVMM_NotifyWheel(const int16_t w_rel)
{
	if (!vmware.is_active) { // only needed by VMware
		return;
	}

	const auto old_counter_w = vmware.counter_w;
	const auto new_counter_w = vmware.counter_w + w_rel;
	vmware.counter_w = clamp_to_int8(static_cast<int32_t>(new_counter_w));

	if (GCC_UNLIKELY(old_counter_w == vmware.counter_w)) {
		return;
	}

	vmware.updated = vmware.is_active;
	MOUSEPS2_NotifyMovedDummy();
}

void MOUSEVMM_NewScreenParams(const uint32_t x_abs, const uint32_t y_abs)
{
	MOUSEVMM_NotifyMoved(0.0f, 0.0f, x_abs, y_abs);
}
