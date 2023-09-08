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
#include "pci_bus.h"
#include "setup.h"
#include "support.h"

#include <map>
#include <set>

CHECK_NARROWING();

// References:
// - https://wiki.osdev.org/VirtualBox_Guest_Additions
// Drivers:
// - https://git.javispedro.com/cgit/vbados.git
// - https://git.javispedro.com/cgit/vbmouse.git

// ----- IMPORTANT NOTE -----
// This currently works with DOS VirtualBox driver (from VBADOS), but not with
// Windows 3.1x VBMOUSE driver for VirtualBox. With the Windows 3.1x driver
// the pointer we are getting in 'port_write_virtualbox' is wrong; mosy likely
// we need to implement the VDS (Virtual DMA Services), as described in Q93469.
// You can read the article for example here:
// - https://jeffpar.github.io/kbarchive/kb/093/Q93469/
// Thus for now the mouse support is disabled. It can be enabled in 'mouse.h' by
// defining EXPERIMENTAL_VIRTUALBOX_MOUSE.

// Static check if port number is valid
static_assert((port_num_virtualbox & 0xfffc) == port_num_virtualbox);

// ***************************************************************************
// Various common type definitions
// ***************************************************************************

struct MouseFeatures {
	uint32_t _data = 0;

	static constexpr uint32_t mask_guest_can_absolute              = 1;
	static constexpr uint32_t mask_host_wants_absolute             = 1 << 1;
	static constexpr uint32_t mask_guest_needs_host_cursor         = 1 << 2;
	static constexpr uint32_t mask_host_cannot_hwpointer           = 1 << 3;
	static constexpr uint32_t mask_new_protocol                    = 1 << 4;
	static constexpr uint32_t mask_host_rechecks_needs_host_cursor = 1 << 5;
	static constexpr uint32_t mask_host_has_abs_dev                = 1 << 6;
	static constexpr uint32_t mask_guest_uses_full_state_protocol  = 1 << 7;
	static constexpr uint32_t mask_host_uses_full_state_protocol   = 1 << 8;

	bool Get(const uint32_t mask) const
	{
		return _data & mask;
	}

	void Set(const uint32_t mask, const bool state)
	{
		if (state) {
			_data |= mask;
		} else {
			_data &= ~mask;
		}
	}

	void Copy(const MouseFeatures& other, const uint32_t mask)
	{
		Set(mask, other.Get(mask));
	}

	void SetInitialValue()
	{
		Set(mask_host_wants_absolute, true);
		Set(mask_host_cannot_hwpointer, true);
		Set(mask_host_rechecks_needs_host_cursor, false);
		Set(mask_host_has_abs_dev, true);
		Set(mask_host_uses_full_state_protocol, false);
	}

	void CombineGuestValue(const MouseFeatures& guest_features)
	{
		Copy(guest_features, mask_guest_can_absolute);
		Copy(guest_features, mask_guest_needs_host_cursor);
		Copy(guest_features, mask_guest_uses_full_state_protocol);
		Copy(guest_features, mask_new_protocol);
	}
};

struct MousePointerFlags {
	uint32_t _data = 0;

	static constexpr uint32_t mask_pointer_visible = 1;
	static constexpr uint32_t mask_pointer_alpha   = 1 << 1;
	static constexpr uint32_t mask_pointer_shape   = 1 << 2;

	bool Get(const uint32_t mask) const
	{
		return _data & mask;
	}
};

// ***************************************************************************
// Request Header constants and structures
// ***************************************************************************

enum class RequestType : uint32_t {
	InvalidRequest  = 0,
	GetMouseStatus  = 1,
	SetMouseStatus  = 2,
	SetPointerShape = 3,
	ReportGuestInfo = 50,
};

constexpr uint32_t ver_1_01 = (1 << 16) + 1;
constexpr uint32_t ver_1_04 = (1 << 16) + 4;

constexpr uint8_t header_size = 24;
struct RequestHeader {
	RequestHeader(const PhysPt pointer);
	RequestHeader() = delete;

	bool IsValid() const;
	bool CheckStructSize(const uint32_t needed_sise) const;

	uint32_t struct_size     = 0;
	uint32_t struct_version  = 0;
	RequestType request_type = RequestType::InvalidRequest;
	PhysPt return_code_pt    = 0;

	// These values are not used by DOSBox:
	// - uint32_t reserved
	// - uint32_t requestor
};

bool RequestHeader::IsValid() const
{
	return struct_size >= header_size;
}

RequestHeader::RequestHeader(const PhysPt pointer)
{
	struct_size    = mem_readd(pointer);
	struct_version = mem_readd(pointer + 4);
	request_type   = static_cast<RequestType>(mem_readd(pointer + 8));
	return_code_pt = pointer + 12;
}

bool RequestHeader::CheckStructSize(const uint32_t needed_size) const
{
	assert(struct_size > header_size);

	const uint32_t available = struct_size - header_size;
	if (needed_size > available) {
		LOG_WARNING("VIRTUALBOX: request #%d - structure v%d.%02d too short, %d instead of at least %d",
		            enum_val(request_type),
		            struct_version >> 16,
		            struct_version & 0xffff,
		            available,
		            needed_size);
		return false;
	}
	return true;
}

// ***************************************************************************
// Request structures
// ***************************************************************************

struct VirtualBox_GuestInfo_1_01 {
	uint32_t interface_version = 0;

	// These values are not used by DOSBox:
	// - uint32_t os_type

	VirtualBox_GuestInfo_1_01() = delete;
	VirtualBox_GuestInfo_1_01(const PhysPt pointer)
	{
		interface_version = mem_readd(pointer);
	}

	static uint32_t GetSize()
	{
		return 8;
	}
};

struct VirtualBox_MouseStatus_1_01 {
	MouseFeatures features = {};

	int32_t pointer_x_pos = 0;
	int32_t pointer_y_pos = 0;

	VirtualBox_MouseStatus_1_01() = delete;
	VirtualBox_MouseStatus_1_01(const PhysPt pointer)
	{
		features._data = mem_readd(pointer);
		pointer_x_pos  = static_cast<int32_t>(mem_readd(pointer + 4));
		pointer_y_pos  = static_cast<int32_t>(mem_readd(pointer + 8));
	}

	static uint32_t GetSize()
	{
		return 12;
	}
};

struct VirtualBox_MousePointer_1_01 {
	MousePointerFlags flags = {};

	uint32_t x_hot_spot    = 0;
	uint32_t y_hot_spot    = 0;
	uint32_t poiner_width  = 0;
	uint32_t poiner_height = 0;

	// These values are not used by DOSBox (optional):
	// - uint8_t pointer_data[4]

	VirtualBox_MousePointer_1_01() = delete;
	VirtualBox_MousePointer_1_01(const PhysPt pointer)
	{
		flags._data   = mem_readd(pointer);
		x_hot_spot    = mem_readd(pointer + 4);
		y_hot_spot    = mem_readd(pointer + 8);
		poiner_width  = mem_readd(pointer + 12);
		poiner_height = mem_readd(pointer + 16);
	}

	static uint32_t GetSize()
	{
		return 20;
	}
};

// ***************************************************************************
// Helper code to print out warnings
// ***************************************************************************

static void warn_unsupported_request(const RequestType request_type)
{
	static std::set<RequestType> already_warned = {};
	if (already_warned.find(request_type) != already_warned.end()) {
		LOG_WARNING("VIRTUALBOX: unimplemented request #%d",
		            enum_val(request_type));
		already_warned.insert(request_type);
	}
}

static void warn_unsupported_struct_version(const RequestHeader& header)
{
	static std::map<RequestType, std::set<uint32_t>> already_warned = {};
	auto& already_warned_set = already_warned[header.request_type];
	if (already_warned_set.find(header.struct_version) !=
	    already_warned_set.end()) {
		LOG_WARNING("VIRTUALBOX: unimplemented request #%d structure v%d.%02d",
		            enum_val(header.request_type),
		            header.struct_version >> 16,
		            header.struct_version && 0xffff);
		already_warned_set.insert(header.struct_version);
	}
}

static void warn_mouse_alpha_shape()
{
	static bool already_warned = false;
	if (!already_warned) {
		LOG_WARNING("VIRTUALBOX: mouse cursor alpha and custom shape not implemented");
		already_warned = true;
	}
}

static void warn_mouse_host_cursor()
{
	static bool already_warned = false;
	if (!already_warned) {
		LOG_WARNING("VIRTUALBOX: host mouse cursor not implemented");
		already_warned = true;
	}
}

static void warn_mouse_new_protocol()
{
	static bool already_warned = false;
	if (!already_warned) {
		LOG_WARNING("VIRTUALBOX: new mouse protocol not implemented");
		already_warned = true;
	}
}

// ***************************************************************************
// PCI card
// ***************************************************************************

struct PCI_VirtualBoxDevice : public PCI_Device {
	enum : uint16_t {
		vendor = 0x80ee,
		device = 0xcafe,
	};

	PCI_VirtualBoxDevice() : PCI_Device(vendor, device) {}

	bool InitializeRegisters(uint8_t registers[256]) override;
	Bits ParseReadRegister(uint8_t regnum) override;
	bool OverrideReadRegister(uint8_t regnum, uint8_t* rval,
	                          uint8_t* rval_mask) override;
	Bits ParseWriteRegister(uint8_t regnum, uint8_t value) override;
};

bool PCI_VirtualBoxDevice::InitializeRegisters(uint8_t registers[256])
{
	registers[0x04] = 0x01; // command register
	registers[0x05] = 0x00;
	registers[0x06] = 0x00; // status register
	registers[0x07] = 0x00;

	registers[0x08] = 0x00; // card revision
	registers[0x09] = 0x00; // programming interface
	registers[0x0a] = 0x00; // subclass code
	registers[0x0b] = 0x00; // class code
	registers[0x0c] = 0x00; // cache line size
	registers[0x0d] = 0x00; // latency timer
	registers[0x0e] = 0x00; // header type (other)

	registers[0x3c] = 0xff; // no IRQ

	constexpr auto port_num = static_cast<uint16_t>(port_num_virtualbox);
	// BAR 0
	registers[0x10] = static_cast<uint8_t>((port_num & 0xfc) + 1);
	registers[0x11] = static_cast<uint8_t>((port_num >> 8) & 0xff);
	registers[0x12] = 0;
	registers[0x13] = 0;

	return true;
}

Bits PCI_VirtualBoxDevice::ParseReadRegister(uint8_t regnum)
{
	return regnum;
}

bool PCI_VirtualBoxDevice::OverrideReadRegister([[maybe_unused]] uint8_t regnum,
                                                [[maybe_unused]] uint8_t* rval,
                                                [[maybe_unused]] uint8_t* rval_mask)
{
	return false;
}

Bits PCI_VirtualBoxDevice::ParseWriteRegister([[maybe_unused]] uint8_t regnum,
                                              [[maybe_unused]] uint8_t value)
{
	return -1;
}

// ***************************************************************************
// Mouse Server, which maintains state and handles I/O port writes
// ***************************************************************************

class MouseServer {
public:
	MouseServer();
	~MouseServer();

private:
	void WriteToPort(io_port_t, io_val_t value, io_width_t width);

	void HandleGetMouseStatus(const RequestHeader& header,
	                          const PhysPt struct_pointer);
	void HandleSetMouseStatus(const RequestHeader& header,
	                          const PhysPt struct_pointer);

	void HandleSetPointerShape(const RequestHeader& header,
	                           const PhysPt struct_pointer);

	static void HandleReportGuestInfo(const RequestHeader& header,
	                                  const PhysPt struct_pointer);

	MouseFeatures mouse_features          = {};
	MousePointerFlags mouse_pointer_flags = {};
};

MouseServer::MouseServer()
{
	mouse_features.SetInitialValue();

	// TODO: implement more features:
	// - shared directories for VBSF.EXE driver by Javis Pedro
	// - possibly display for the OS/2 Museum work-in-progres drivers
	//   https://www.os2museum.com/wp/antique-display-driving/
	// - (very far future) possibly Windows 9x 3D acceleration using
	//   project like SoftGPU (or whatever will be available):
	//   https://github.com/JHRobotics/softgpu

	PCI_AddDevice(new PCI_VirtualBoxDevice());

	using namespace std::placeholders;
	const auto write_to = std::bind(&MouseServer::WriteToPort, this, _1, _2, _3);
	IO_RegisterWriteHandler(port_num_virtualbox, write_to, io_width_t::dword);
}

MouseServer::~MouseServer()
{
	MOUSEVMM_Deactivate(MouseVmmProtocol::VirtualBox);
	IO_FreeWriteHandler(port_num_virtualbox, io_width_t::dword);
	PCI_RemoveDevice(PCI_VirtualBoxDevice::vendor, PCI_VirtualBoxDevice::device);
}

template <typename T>
static constexpr bool check_size(const RequestHeader& header)
{
	return header.CheckStructSize(T::GetSize());
}

static void report_success(PhysPt return_code_pt)
{
	mem_writed(return_code_pt, 0);
}

void MouseServer::HandleGetMouseStatus(const RequestHeader& header,
                                       const PhysPt struct_pointer)
{
	switch (header.struct_version) {
	case ver_1_01: {
		if (!check_size<VirtualBox_MouseStatus_1_01>(header)) {
			break;
		}

		MouseVirtualBoxPointerStatus status = {};
		MOUSEVMM_GetPointerStatus(status);

		mem_writed(struct_pointer, mouse_features._data);
		mem_writed(struct_pointer + 4, status.absolute_x);
		mem_writed(struct_pointer + 8, status.absolute_y);

		report_success(header.return_code_pt);
		break;
	}
	default: warn_unsupported_struct_version(header); break;
	}
}

void MouseServer::HandleSetMouseStatus(const RequestHeader& header,
                                       const PhysPt struct_pointer)
{
	switch (header.struct_version) {
	case ver_1_01: {
		if (!check_size<VirtualBox_MouseStatus_1_01>(header)) {
			break;
		}

		const VirtualBox_MouseStatus_1_01 payload(struct_pointer);
		const auto& requested = payload.features;
		if (requested.Get(requested.mask_guest_needs_host_cursor)) {
			warn_mouse_host_cursor();
		}
		if (requested.Get(requested.mask_new_protocol)) {
			warn_mouse_new_protocol();
		}

		mouse_features.CombineGuestValue(payload.features);
		const auto& features = mouse_features;

		const bool guest_can_absolute = features.Get(
		        features.mask_guest_can_absolute);

		if (guest_can_absolute) {
			MOUSEVMM_Activate(MouseVmmProtocol::VirtualBox);
		} else {
			MOUSEVMM_Deactivate(MouseVmmProtocol::VirtualBox);
		}

		report_success(header.return_code_pt);
		break;
	}
	default: warn_unsupported_struct_version(header); break;
	}
}

void MouseServer::HandleSetPointerShape(const RequestHeader& header,
                                        const PhysPt struct_pointer)
{
	switch (header.struct_version) {
	case ver_1_01: {
		if (!check_size<VirtualBox_MousePointer_1_01>(header)) {
			break;
		}

		const VirtualBox_MousePointer_1_01 payload(struct_pointer);

		mouse_pointer_flags = payload.flags;
		const auto& flags   = mouse_pointer_flags;

		const bool pointer_visible = flags.Get(
		        payload.flags.mask_pointer_visible);
		const bool pointer_alpha = flags.Get(payload.flags.mask_pointer_alpha);
		const bool pointer_shape = flags.Get(payload.flags.mask_pointer_shape);

		if (pointer_visible && (pointer_alpha || pointer_shape)) {
			warn_mouse_alpha_shape();
		}

		MOUSEVMM_SetPointerVisible_VirtualBox(pointer_visible);

		report_success(header.return_code_pt);
		break;
	}
	default: warn_unsupported_struct_version(header); break;
	}
}

void VIRTUALBOX_Configure(const ModuleLifecycle, Section* = nullptr);

void MouseServer::HandleReportGuestInfo(const RequestHeader& header,
                                        const PhysPt struct_pointer)
{
	switch (header.struct_version) {
	case ver_1_01: {
		if (!check_size<VirtualBox_GuestInfo_1_01>(header)) {
			break;
		}

		const VirtualBox_GuestInfo_1_01 payload(struct_pointer);

		if (payload.interface_version != ver_1_04) {
			LOG_WARNING("VIRTUALBOX: unimplemented protocol v%d.%02d",
			            payload.interface_version >> 16,
			            payload.interface_version && 0xffff);

			VIRTUALBOX_Configure(ModuleLifecycle::Destroy);
			break;
		}

		MOUSEVMM_Activate(MouseVmmProtocol::VirtualBox);
		VIRTUALBOX_Configure(ModuleLifecycle::Create);

		report_success(header.return_code_pt);
		break;
	}
	default: warn_unsupported_struct_version(header); break;
	}
}

void MouseServer::WriteToPort(io_port_t, io_val_t value, io_width_t width)
{
	if (width != io_width_t::dword) {
		return; // not a VirtualBox protocol
	}

	const PhysPt header_pointer = value;
	const PhysPt struct_pointer = value + header_size;
	RequestHeader header(header_pointer);

	if (header.request_type != RequestType::ReportGuestInfo) {
		return; // not a proper VirtualBox client
	}

	if (!header.IsValid()) {
		LOG_WARNING("VIRTUALBOX: invalid request header");
		return;
	}

	switch (header.request_type) {
	case RequestType::GetMouseStatus:
		HandleGetMouseStatus(header, struct_pointer);
		break;
	case RequestType::SetMouseStatus:
		HandleSetMouseStatus(header, struct_pointer);
		break;
	case RequestType::SetPointerShape:
		HandleSetPointerShape(header, struct_pointer);
		break;
	case RequestType::ReportGuestInfo:
		HandleReportGuestInfo(header, struct_pointer);
		break;
	default: warn_unsupported_request(header.request_type); break;
	}
}

// ***************************************************************************
// Lifecycle
// ***************************************************************************

void VIRTUALBOX_Configure(const ModuleLifecycle lifecycle, Section*)
{
	static std::unique_ptr<MouseServer> state_instance = {};

	switch (lifecycle) {
	case ModuleLifecycle::Reconfigure:
		state_instance.reset();
		[[fallthrough]];

	case ModuleLifecycle::Create:
		if (MOUSEVMM_IsSupported(MouseVmmProtocol::VirtualBox) &&
		    !state_instance) {
			state_instance = std::make_unique<MouseServer>();
		}
		break;

	case ModuleLifecycle::Destroy:
		state_instance.reset();
		break;
	}
}

void VIRTUALBOX_NotifyBooting()
{
	VIRTUALBOX_Configure(ModuleLifecycle::Destroy);
}
void VIRTUALBOX_Init(Section* section)
{
	VIRTUALBOX_Configure(ModuleLifecycle::Create, section);
}
