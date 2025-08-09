// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "vmware.h"

#include "util/checks.h"
#include "dosbox.h"
#include "inout.h"
#include "mouse.h"
#include "cpu/registers.h"
#include "config/setup.h"
#include "util/string_utils.h"
#include "misc/support.h"

#include <set>

CHECK_NARROWING();

// Reference:
// - https://wiki.osdev.org/VMware_tools
// Drivers:
// - https://git.javispedro.com/cgit/vbados.git
// - https://github.com/NattyNarwhal/vmwmouse (warning: release 0.1 is unstable)

static bool is_interface_enabled = false;
static bool has_feature_mouse    = false;

static std::string program_segment_name = {};

// ***************************************************************************
// Various common constants and type definitions
// ***************************************************************************

static std::set<std::string> SegmentBlackList = {
	// JEMM memory manager assumes certain memory layout if it detects the
	// VMware interface; this leads to incorrect JEMM behavior
	"JEMM386",
	"JEMMEX"
};

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
	// This command is a common way to detect VMware - since we only provide
	// a mouse support, hide the interface from software which is known to
	// misbehave with our limited implementation
	if (!SegmentBlackList.contains(program_segment_name)) {
		reg_eax = 0; // protocol version
		reg_ebx = VMWARE_MAGIC;
	}
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
// External notifications
// ***************************************************************************

void VMWARE_NotifyBooting()
{
	program_segment_name = {};
}

void VMWARE_NotifyProgramName(const std::string& segment_name)
{
	program_segment_name = segment_name;
	upcase(program_segment_name);
}

// ***************************************************************************
// Lifecycle
// ***************************************************************************

void VMWARE_Destroy(Section*)
{
	if (is_interface_enabled) {
		IO_FreeReadHandler(port_num_virtualbox, io_width_t::dword);
		is_interface_enabled = false;
	}
}

void VMWARE_Init(Section* sec)
{
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
		sec->AddDestroyFunction(&VMWARE_Destroy, false);
		IO_RegisterReadHandler(port_num_vmware,
		                       port_read_vmware,
		                       io_width_t::dword);
	}
}
