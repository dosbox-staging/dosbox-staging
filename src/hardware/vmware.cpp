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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "checks.h"
#include "dosbox.h"
#include "inout.h"
#include "mouse.h"
#include "regs.h"
#include "setup.h"
#include "support.h"

CHECK_NARROWING();

// Reference:
// - https://wiki.osdev.org/VMware_tools
// Drivers:
// - https://git.javispedro.com/cgit/vbados.git
// - https://github.com/NattyNarwhal/vmwmouse (warning: release 0.1 is unstable)

static bool is_interface_enabled = false;
static bool has_feature_mouse    = false;

// ***************************************************************************
// Various common constants and type definitions
// ***************************************************************************

// Magic number for all VMware calls
static constexpr uint32_t VMWARE_MAGIC = 0x564d5868u;

enum class VmWareCommand : uint16_t {
	GetVersion        = 10,
	AbsPointerData    = 39,
	AbsPointerStatus  = 40,
	AbsPointerCommand = 41,
};

enum class VmWarePointer : uint32_t {
	Enable   = 0x45414552,
	Relative = 0xf5,
	Absolute = 0x53424152,
};

// ***************************************************************************
// Request handling
// ***************************************************************************

static void command_get_version()
{
	reg_eax = 0; // protocol version
	reg_ebx = VMWARE_MAGIC;
}

static void command_abs_pointer_data()
{
	MouseVmWarePointerStatus status = {};
	MOUSEVMM_GetPointerStatus(status);

	reg_eax = status.buttons;
	reg_ebx = status.absolute_x;
	reg_ecx = status.absolute_y;
	reg_edx = status.wheel_counter;
}

static void command_abs_pointer_status()
{
	constexpr uint32_t ABS_UPDATED     = 4;
	constexpr uint32_t ABS_NOT_UPDATED = 0;

	reg_eax = MOUSEVMM_CheckIfUpdated_VmWare() ? ABS_UPDATED : ABS_NOT_UPDATED;
}

static void command_abs_pointer()
{
	switch (static_cast<VmWarePointer>(reg_ebx)) {
	case VmWarePointer::Enable: break; // can be safely ignored
	case VmWarePointer::Relative:
		MOUSEVMM_Deactivate(MouseVmmProtocol::VmWare);
		break;
	case VmWarePointer::Absolute:
		MOUSEVMM_Activate(MouseVmmProtocol::VmWare);
		break;
	default:
		LOG_WARNING("VMWARE: unimplemented mouse subcommand 0x%08x", reg_ebx);
		break;
	}
}

// ***************************************************************************
// I/O port
// ***************************************************************************

static uint32_t port_read_vmware(const io_port_t, const io_width_t)
{
	if (reg_eax != VMWARE_MAGIC) {
		return 0;
	}

	switch (static_cast<VmWareCommand>(reg_cx)) {
	case VmWareCommand::GetVersion: command_get_version(); break;
	case VmWareCommand::AbsPointerData: command_abs_pointer_data(); break;
	case VmWareCommand::AbsPointerStatus: command_abs_pointer_status(); break;
	case VmWareCommand::AbsPointerCommand: command_abs_pointer(); break;
	default:
		LOG_WARNING("VMWARE: unimplemented command 0x%08x", reg_ecx);
		break;
	}

	return reg_eax;
}

// ***************************************************************************
// Lifecycle
// ***************************************************************************

void VMWARE_Configure(const ModuleLifecycle lifecycle, Section*)
{
	switch (lifecycle) {
	case ModuleLifecycle::Create:
		has_feature_mouse = MOUSEVMM_IsSupported(MouseVmmProtocol::VmWare);

		// TODO: implement more features:
		// - shared directories, for VMSMount tool:
		//   https://github.com/eduardocasino/vmsmount
		// - everything supported by the official Windows 9x VMware Tools
		// - (very far future) possibly Windows 9x 3D acceleration using
		//   project like SoftGPU (or whatever will be available):
		//   https://github.com/JHRobotics/softgpu

		is_interface_enabled = has_feature_mouse;
		if (is_interface_enabled) {
			IO_RegisterReadHandler(port_num_vmware,
			                       port_read_vmware,
			                       io_width_t::dword);
		}
		break;

	// This module doesn't support reconfiguration at runtime
	case ModuleLifecycle::Reconfigure:
		break;

	case ModuleLifecycle::Destroy:
		if (is_interface_enabled) {
			IO_FreeReadHandler(port_num_virtualbox, io_width_t::dword);
			is_interface_enabled = false;
		}
		break;
	}
}

void VMWARE_Destroy(Section* section)
{
	VMWARE_Configure(ModuleLifecycle::Destroy, section);
}

void VMWARE_Init(Section* section)
{
	VMWARE_Configure(ModuleLifecycle::Create, section);
	section->AddDestroyFunction(&VMWARE_Destroy);
}
