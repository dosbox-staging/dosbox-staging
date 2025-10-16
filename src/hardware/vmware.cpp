// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "vmware.h"

#include <set>

#include "config/setup.h"
#include "cpu/registers.h"
#include "dosbox.h"
#include "hardware/input/mouse.h"
#include "hardware/port.h"
#include "misc/support.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

// Reference:
// - https://wiki.osdev.org/VMware_tools
// Drivers:
// - https://git.javispedro.com/cgit/vbados.git
// - https://github.com/NattyNarwhal/vmwmouse (warning: release 0.1 is unstable)
// - official Windows 9x VMware mouse driver

static bool is_interface_enabled = false;
static bool has_feature_mouse    = false;

// Whether Intel 8042 entry point API is currently enabled
static bool is_i8042_unlocked = false;

// Currently running program
static std::string program_segment_name = {};

// Queued data, waiting to be fectehd by the guest side driver
static std::vector<uint32_t> abs_pointer_queue = {};

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
static constexpr uint32_t VmWareMagic = 0x564d5868u;

// The exact meaning of the version ID below is unknown - so far we know that:
// - Linux kernel requires precisely this particular version ID, otherwise
//   it refuses to talk to the VmWare mouse interface.
// - The official VMware mouse driver for Windows 9x seems to be doing a version
//   ID validation, too - you can't just provide any random value. Details are
//   unknown, but this particular value works.
static constexpr uint32_t VmMouseVersionId = 0x3442554au;

enum class VmWareCommand : uint16_t {
	GetVersion         = 10,
	AbsPointerData     = 39,
	AbsPointerStatus   = 40,
	AbsPointerCommand  = 41,
	AbsPointerRestrict = 86,
};

enum class VmMouseCommand : uint32_t {
	Enable   = 0x45414552u,
	Disable  = 0xf5u,
	Absolute = 0x53424152u,
	Relative = 0x4c455252u,
};

// ***************************************************************************
// Mouse queue and commands
// ***************************************************************************

static uint32_t fetch_from_abs_pointer_queue()
{
	if (abs_pointer_queue.empty()) {
		return 0;
	}

	const auto result = abs_pointer_queue.back();
	abs_pointer_queue.pop_back();

	return result;
}

static void mouse_status_to_abs_pointer_queue()
{
	if (!MOUSEVMM_CheckIfUpdated_VmWare()) {
		return;
	}

	if (abs_pointer_queue.size() == 1) {
		// We have a status response waiting in the queue, do not
		// override it
		return;
	}

	MouseVmWarePointerStatus status = {};
	MOUSEVMM_GetPointerStatus(status);

	abs_pointer_queue.clear();
	abs_pointer_queue.push_back(status.wheel_counter);
	abs_pointer_queue.push_back(status.absolute_y);
	abs_pointer_queue.push_back(status.absolute_x);
	abs_pointer_queue.push_back(status.buttons);
}

static void execute_command(const VmMouseCommand command)
{
	abs_pointer_queue.clear();
	switch (command) {
	case VmMouseCommand::Enable:
		abs_pointer_queue = {VmMouseVersionId};
		break;
	case VmMouseCommand::Disable:
		MOUSEVMM_Deactivate(MouseVmmProtocol::VmWare);
		break;
	case VmMouseCommand::Absolute:
		MOUSEVMM_Activate(MouseVmmProtocol::VmWare);
		break;
	case VmMouseCommand::Relative:
		LOG_WARNING("VMWARE: Relative mouse packets not implemented");
		break;
	default:
		LOG_WARNING("VMWARE: Unimplemented mouse subcommand 0x%08x", reg_ebx);
		break;
	}
}

// ***************************************************************************
// Low bandwidth I/O port interface
// ***************************************************************************

static void command_get_version()
{
	// This command is a common way to detect VMware - since currently we
	// only implement  mouse support, hide the interface from software which
	// is known to misbehave with our limited implementation
	if (!SegmentBlackList.contains(program_segment_name)) {
		reg_eax = 0; // protocol version
		reg_ebx = VmWareMagic;
	}
}

static void command_abs_pointer_data()
{
	if (abs_pointer_queue.empty() || abs_pointer_queue.size() == 4) {
		reg_eax = fetch_from_abs_pointer_queue();
		reg_ebx = fetch_from_abs_pointer_queue();
		reg_ecx = fetch_from_abs_pointer_queue();
		reg_edx = fetch_from_abs_pointer_queue();
	} else {
		// Should not happen with a properly functioning guest driver
		LOG_WARNING("VMWARE: No valid mouse pointer status in the queue");
		abs_pointer_queue.clear();
		reg_eax = 0;
		reg_ebx = 0;
		reg_ecx = 0;
		reg_edx = 0;
	}
}

static void command_abs_pointer_status()
{
	mouse_status_to_abs_pointer_queue();
	reg_eax = check_cast<uint32_t>(abs_pointer_queue.size());
}

static void command_abs_pointer()
{
	const auto command = static_cast<VmMouseCommand>(reg_ebx);
	if (command == VmMouseCommand::Enable) {
		// For the standard VMware port interface we need regular PS/2
		// auxilary (mouse) interrupt handling
		MOUSEVMM_EnableImmediateInterrupts(false);
	}
	execute_command(command);
	if (abs_pointer_queue.size() == 1) {
		reg_eax = fetch_from_abs_pointer_queue();
	}
}

static uint32_t port_read_vmware(const io_port_t, const io_width_t)
{
	if (reg_eax != VmWareMagic) {
		return 0;
	}

	switch (static_cast<VmWareCommand>(reg_cx)) {
	case VmWareCommand::GetVersion:
		command_get_version();
		break;
	case VmWareCommand::AbsPointerData:
		command_abs_pointer_data();
		break;
	case VmWareCommand::AbsPointerStatus:
		command_abs_pointer_status();
		break;
	case VmWareCommand::AbsPointerCommand:
		command_abs_pointer();
		break;
	case VmWareCommand::AbsPointerRestrict:
		LOG_WARNING("VMWARE: Mouse pointer restrictions not implemented");
		break;
	default:
		LOG_WARNING("VMWARE: Unimplemented command 0x%08x", reg_ecx);
		break;
	}

	return reg_eax;
}

// ***************************************************************************
// Intel 8042 interface
// ***************************************************************************

bool VMWARE_I8042_ReadTakeover()
{
	return is_i8042_unlocked;
}

uint32_t VMWARE_I8042_ReadStatusRegister()
{
	// Port 0x64 read handler

	assert(is_i8042_unlocked);

	mouse_status_to_abs_pointer_queue();
	return check_cast<uint32_t>(abs_pointer_queue.size());
}

uint32_t VMWARE_I8042_ReadDataPort()
{
	// Port 0x60 read handler

	assert(is_i8042_unlocked);

	return fetch_from_abs_pointer_queue();
}

bool VMWARE_I8042_WriteCommandPort(const uint32_t value)
{
	// Port 0x64 write handler

	if (!has_feature_mouse) {
		return false;
	}

	const auto command = static_cast<VmMouseCommand>(value);

	if (!is_i8042_unlocked && command == VmMouseCommand::Enable) {
		is_i8042_unlocked = true;
		// For the Intel 8042 VMware port interface we need the PS/2
		// auxilary (mouse) interrupts to be triggered immediately,
		// without creating mouse data packets - these are not being
		// fetched by the official Windows 9x VMware mouse driver
		MOUSEVMM_EnableImmediateInterrupts(true);
	}

	const auto was_taken_over = is_i8042_unlocked;
	if (is_i8042_unlocked) {
		execute_command(command);
	}

	if (command == VmMouseCommand::Disable) {
		is_i8042_unlocked = false;
		MOUSEVMM_EnableImmediateInterrupts(false);
	}

	return was_taken_over;
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

void VMWARE_Init()
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
		IO_RegisterReadHandler(port_num_vmware,
		                       port_read_vmware,
		                       io_width_t::dword);
	}
}

void VMWARE_Destroy()
{
	if (is_interface_enabled) {
		IO_FreeReadHandler(port_num_virtualbox, io_width_t::dword);
		is_interface_enabled = false;
	}
}
