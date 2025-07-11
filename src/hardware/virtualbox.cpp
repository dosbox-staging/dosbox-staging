// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "virtualbox.h"

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

// Static check if port number is valid
static_assert((port_num_virtualbox & 0xfffc) == port_num_virtualbox);

static bool is_interface_enabled = false;
static bool has_feature_mouse    = false;

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
// Server state
// ***************************************************************************

static struct {
	bool is_client_connected = false;

	MouseFeatures mouse_features          = {};
	MousePointerFlags mouse_pointer_flags = {};

} state;

// ***************************************************************************
// Request Header constants and structures
// ***************************************************************************

enum class VBoxRequestType : uint32_t {
	InvalidRequest  = 0,
	GetMouseStatus  = 1,
	SetMouseStatus  = 2,
	SetPointerShape = 3,
	ReportGuestInfo = 50,
};

enum class VBoxReturnCode : uint32_t {
	ErrorNotImplemented = (UINT32_MAX + 1) - 12,
	ErrorNotSupported   = (UINT32_MAX + 1) - 37,
};

constexpr uint32_t ver_1_01 = (1 << 16) + 1;
constexpr uint32_t ver_1_04 = (1 << 16) + 4;

constexpr uint8_t header_size = 24;
struct RequestHeader {
	RequestHeader(const PhysPt pointer);
	RequestHeader() = delete;

	bool IsValid() const;
	bool CheckStructSize(const uint32_t needed_sise) const;

	uint32_t struct_size    = 0;
	uint32_t struct_version = 0;
	VBoxRequestType request_type = VBoxRequestType::InvalidRequest;
	PhysPt return_code_pt   = 0;

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
	struct_size    = phys_readd(pointer);
	struct_version = phys_readd(pointer + 4);
	request_type   = static_cast<VBoxRequestType>(phys_readd(pointer + 8));
	return_code_pt = pointer + 12;
}

bool RequestHeader::CheckStructSize(const uint32_t needed_size) const
{
	assert(struct_size > header_size);

	const auto available = static_cast<uint32_t>(struct_size - header_size);
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
		interface_version = phys_readd(pointer);
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
		features._data = phys_readd(pointer);
		pointer_x_pos  = static_cast<int32_t>(phys_readd(pointer + 4));
		pointer_y_pos  = static_cast<int32_t>(phys_readd(pointer + 8));
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
		flags._data   = phys_readd(pointer);
		x_hot_spot    = phys_readd(pointer + 4);
		y_hot_spot    = phys_readd(pointer + 8);
		poiner_width  = phys_readd(pointer + 12);
		poiner_height = phys_readd(pointer + 16);
	}

	static uint32_t GetSize()
	{
		return 20;
	}
};

// ***************************************************************************
// Helper code to print out warnings
// ***************************************************************************

static void warn_unsupported_request(const VBoxRequestType request_type)
{
	static std::set<VBoxRequestType> already_warned = {};
	if (already_warned.contains(request_type)) {
		LOG_WARNING("VIRTUALBOX: unimplemented request #%d",
		            enum_val(request_type));
		already_warned.insert(request_type);
	}
}

static void warn_unsupported_struct_version(const RequestHeader& header)
{
	static std::map<VBoxRequestType, std::set<uint32_t>> already_warned = {};
	auto& already_warned_set = already_warned[header.request_type];
	if (already_warned_set.contains(header.struct_version)) {
		LOG_WARNING("VIRTUALBOX: unimplemented request #%d structure v%d.%02d",
		            enum_val(header.request_type),
		            header.struct_version >> 16,
		            header.struct_version & 0xffff);
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
// Request decoding & handling
// ***************************************************************************

static void client_connect()
{
	state.is_client_connected = true;
}

static void client_disconnect()
{
	if (!state.is_client_connected) {
		return;
	}

	if (has_feature_mouse) {
		MOUSEVMM_Deactivate(MouseVmmProtocol::VirtualBox);
	}

	state.is_client_connected = false;
}

static void report_success(PhysPt return_code_pt)
{
	phys_writed(return_code_pt, 0);
}

static void report_failure(PhysPt return_code_pt, const VBoxReturnCode fail_code)
{
	phys_writed(return_code_pt, static_cast<uint32_t>(fail_code));
}

template <typename T>
static bool check_size(const RequestHeader& header)
{
	return header.CheckStructSize(T::GetSize());
}

static void handle_error_unsupported_request(const RequestHeader& header)
{
	report_failure(header.return_code_pt,
	               VBoxReturnCode::ErrorNotImplemented);
	warn_unsupported_request(header.request_type);
}

static void handle_error_unsupported_struct_version(const RequestHeader& header)
{
	report_failure(header.return_code_pt,
	               VBoxReturnCode::ErrorNotSupported);
	warn_unsupported_struct_version(header);
}

static void handle_get_mouse_status(const RequestHeader& header,
                                    const PhysPt struct_pointer)
{
	if (!has_feature_mouse) {
		report_failure(header.return_code_pt,
			       VBoxReturnCode::ErrorNotSupported);
		return;
	}

	switch (header.struct_version) {
	case ver_1_01: {
		if (!check_size<VirtualBox_MouseStatus_1_01>(header)) {
			break;
		}

		MouseVirtualBoxPointerStatus status = {};
		MOUSEVMM_GetPointerStatus(status);
		phys_writed(struct_pointer, state.mouse_features._data);
		phys_writed(struct_pointer + 4, status.absolute_x);
		phys_writed(struct_pointer + 8, status.absolute_y);

		report_success(header.return_code_pt);
		break;
	}
	default: handle_error_unsupported_struct_version(header); break;
	}
}

static void handle_set_mouse_status(const RequestHeader& header,
                                    const PhysPt struct_pointer)
{
	if (!has_feature_mouse) {
		report_failure(header.return_code_pt,
			       VBoxReturnCode::ErrorNotSupported);
		return;
	}

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
		state.mouse_features.CombineGuestValue(payload.features);
		const auto& features = state.mouse_features;

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
	default: handle_error_unsupported_struct_version(header); break;
	}
}

static void handle_set_pointer_shape(const RequestHeader& header,
                                     const PhysPt struct_pointer)
{
	if (!has_feature_mouse) {
		report_failure(header.return_code_pt,
		               VBoxReturnCode::ErrorNotSupported);
		return;
	}

	switch (header.struct_version) {
	case ver_1_01: {
		if (!check_size<VirtualBox_MousePointer_1_01>(header)) {
			break;
		}

		const VirtualBox_MousePointer_1_01 payload(struct_pointer);
		state.mouse_pointer_flags = payload.flags;
		const auto& flags         = state.mouse_pointer_flags;

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
	default: handle_error_unsupported_struct_version(header); break;
	}
}

static void handle_report_guest_info(const RequestHeader& header,
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
			            payload.interface_version & 0xffff);
			client_disconnect();
			break;
		}

		client_connect();
		report_success(header.return_code_pt);
		break;
	}
	default: handle_error_unsupported_struct_version(header); break;
	}
}

// ***************************************************************************
// I/O port
// ***************************************************************************

static void port_write_virtualbox(io_port_t, io_val_t value, io_width_t width)
{
	if (width != io_width_t::dword) {
		return; // not a VirtualBox protocol
	}

	const PhysPt header_pointer = value;
	const PhysPt struct_pointer = value + header_size;
	RequestHeader header(header_pointer);

	if (!state.is_client_connected &&
	    header.request_type != VBoxRequestType::ReportGuestInfo) {
		return; // not a proper VirtualBox client
	}

	if (!header.IsValid()) {
		LOG_WARNING("VIRTUALBOX: invalid request header");
		return;
	}

	switch (header.request_type) {
	case VBoxRequestType::GetMouseStatus:
		handle_get_mouse_status(header, struct_pointer);
		break;
	case VBoxRequestType::SetMouseStatus:
		handle_set_mouse_status(header, struct_pointer);
		break;
	case VBoxRequestType::SetPointerShape:
		handle_set_pointer_shape(header, struct_pointer);
		break;
	case VBoxRequestType::ReportGuestInfo:
		handle_report_guest_info(header, struct_pointer);
		break;
	default: handle_error_unsupported_request(header); break;
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
// External notifications
// ***************************************************************************

void VIRTUALBOX_NotifyBooting()
{
	client_disconnect();
}

// ***************************************************************************
// Lifecycle
// ***************************************************************************

void VIRTUALBOX_Destroy(Section*)
{
	if (is_interface_enabled) {
		client_disconnect();
		PCI_RemoveDevice(PCI_VirtualBoxDevice::vendor,
		                 PCI_VirtualBoxDevice::device);
		IO_FreeWriteHandler(port_num_virtualbox, io_width_t::dword);
		is_interface_enabled = false;
	}
}

void VIRTUALBOX_Init(Section* sec)
{
	has_feature_mouse = MOUSEVMM_IsSupported(MouseVmmProtocol::VirtualBox);
	if (has_feature_mouse) {
		state.mouse_features.SetInitialValue();
	}

	// TODO: implement more features:
	// - shared directories for VBSF.EXE driver by Javis Pedro
	// - possibly display for the OS/2 Museum work-in-progres drivers
	//   https://www.os2museum.com/wp/antique-display-driving/
	// - (very far future) possibly Windows 9x 3D acceleration using
	//   project like SoftGPU (or whatever will be available):
	//   https://github.com/JHRobotics/softgpu

	is_interface_enabled = has_feature_mouse;
	if (is_interface_enabled) {
		sec->AddDestroyFunction(&VIRTUALBOX_Destroy, false);
		PCI_AddDevice(new PCI_VirtualBoxDevice());
		IO_RegisterWriteHandler(port_num_virtualbox,
		                        port_write_virtualbox,
		                        io_width_t::dword);
	}
}
