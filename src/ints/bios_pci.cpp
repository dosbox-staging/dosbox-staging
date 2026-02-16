// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ints/bios.h"

#include "cpu/callback.h"
#include "utils/checks.h"
#include "dosbox.h"
#include "hardware/port.h"
#include "hardware/pci_bus.h"
#include "cpu/registers.h"
#include "misc/support.h"

CHECK_NARROWING();

// Reference:
// - PCI BIOS Specification, revision 2.1

// TODO (not sure if needed by anything):
// - interrupt routing is not implemented
// - special cycles are not implemented
// - BIOS32 interface is not implemented
// - 0x000ffe6e entry point is not implemented

constexpr uint16_t max_device_index = 0x100;
constexpr uint32_t enable_bit       = static_cast<uint32_t>(1 << 31);

enum class PciReturnCode : uint8_t {
	Successful        = 0x00,
	FuncNotSupported  = 0x81,
	BadVendorId       = 0x83,
	DeviceNotFound    = 0x86,
	BadRegisterNumber = 0x87,
	SetFailed         = 0x88,
	BufferTooSmall    = 0x89,
};

static void warn_unknown_function(const uint8_t function)
{
	static bool already_warned[UINT8_MAX + 1];
	if (!already_warned[function]) {
		LOG_WARNING("INT1A: Unknown PCI function 0xb1%02x", function);
		already_warned[function] = true;
	}
}

static void warn_no_pci_present()
{
	static bool already_warned = false;
	if (!already_warned) {
		LOG_WARNING("INT1A: PCI function called despite no PCI present");
		already_warned = true;
	}
}

void INT1AB1_Handler()
{
	auto set_return_code = [](const PciReturnCode code) {
		reg_ah = enum_val(code);
		CALLBACK_SCF(code != PciReturnCode::Successful);
	};

	auto select_read_write_address = []() {
		const auto address = enable_bit |
	                             static_cast<uint32_t>(reg_bx << 8) |
	                             static_cast<uint32_t>(reg_di & 0xfc);
		IO_WriteD(port_num_pci_config_address,
		          static_cast<uint32_t>(address));
	};

	if (!PCI_IsInitialized()) {
		// No PCI subsystem

		if (reg_al != 0x01) {
			warn_no_pci_present();
		}

		set_return_code(PciReturnCode::FuncNotSupported);
		return;
	}

	switch (reg_al) {
	case 0x01: // PCI BIOS Present
		reg_bx  = 0x0210;     // version 2.10
		reg_cx  = 0x0000;     // only one PCI bus
		reg_edx = 0x20494350; // "PCI "
		reg_edi = PCI_GetPModeInterface();
		// AL informs which mechanisms are supported:
		// bit 0: configuration mechanism #1
		// bit 1: configuration mechanism #2
		// (either bit 1 or 2 needs to be set)
		// bit 4: special cycle generation mechanism #1
		// bit 5: special cycle generation mechanism #2
		// (either bit 4 or 5 can be set, must match the supported
		// configuration mechanism)
		reg_al = 0x01;
		set_return_code(PciReturnCode::Successful);
		return;
	case 0x02: // Find PCI Device
	{
		// Check if vendor ID is valid
		if (reg_dx == 0xffff) {
			set_return_code(PciReturnCode::BadVendorId);
			return;
		}

		const auto tag_value = (reg_cx << 16) | reg_dx;
		const auto device_tag = static_cast<uint32_t>(tag_value);

		// Try to find requested device
		uint32_t device_number = 0;
		for (uint16_t i = 0; i <= max_device_index; ++i) {
			// Query unique device/subdevice entries
			const auto i_shifted = static_cast<uint32_t>(i << 8);
			IO_WriteD(port_num_pci_config_address,
			          static_cast<uint32_t>(enable_bit | i_shifted));
			if (IO_ReadD(0xcfc) != device_tag) {
				continue; // Tag does not match
			}

			if (device_number == reg_si) {
				// Found!
				reg_bl = static_cast<uint8_t>(i & 0xff);
				reg_bh = 0x00; // bus 0
				set_return_code(PciReturnCode::Successful);
				return;
			}

			// Matches, but is not the SIth device
			++device_number;
		}

		// Device not found
		set_return_code(PciReturnCode::DeviceNotFound);
		return;
	}
	case 0x03: { // Find PCI Class Code
		const auto tag_value = reg_ecx & 0xffffff;
		const auto class_tag = static_cast<uint32_t>(tag_value);

		// Try to find requested device
		uint32_t device_number = 0;
		for (uint16_t i = 0; i <= max_device_index; ++i) {
			// Query unique device/subdevice entries
			const auto i_shifted = static_cast<uint32_t>(i << 8);
			IO_WriteD(port_num_pci_config_address,
			          static_cast<uint32_t>(enable_bit | i_shifted));
			if (IO_ReadD(port_num_pci_config_data) == UINT32_MAX) {
				continue;
			}
			IO_WriteD(port_num_pci_config_address,
			          static_cast<uint32_t>(enable_bit | i_shifted | 0x08));
			if ((IO_ReadD(port_num_pci_config_data) >> 8) != class_tag) {
				continue; // Tag does not match
			}

			if (device_number == reg_si) {
				// Found!
				reg_bl = static_cast<uint8_t>(i & 0xff);
				reg_bh = 0x00; // bus 0
				set_return_code(PciReturnCode::Successful);
				return;
			}

			// Matches, but is not the SIth device
			++device_number;
		}

		// Device not found
		set_return_code(PciReturnCode::DeviceNotFound);
		return;
	}
	case 0x08: // Read Configuration Byte
	{
		select_read_write_address();
		const auto port = port_num_pci_config_data + (reg_di & 3);
		reg_cl = IO_ReadB(static_cast<io_port_t>(port));
		set_return_code(PciReturnCode::Successful);
		return;
	}
	case 0x09: // Read Configuration Word
	{
		if ((reg_di % 2) != 0) {
			// Not a multiple of 2
			set_return_code(PciReturnCode::BadRegisterNumber);
			return;
		}
		select_read_write_address();
		const auto port = port_num_pci_config_data + (reg_di & 2);
		reg_cx = IO_ReadW(static_cast<io_port_t>(port));
		set_return_code(PciReturnCode::Successful);
		return;
	}
	case 0x0a: // Read Configuration Dword
	{
		if ((reg_di % 4) != 0) {
			// Not a multiple of 4
			set_return_code(PciReturnCode::BadRegisterNumber);
			return;
		}
		select_read_write_address();
		const auto port = port_num_pci_config_data + (reg_di & 3);
		reg_ecx = IO_ReadD(static_cast<io_port_t>(port));
		set_return_code(PciReturnCode::Successful);
		return;
	}
	case 0x0b: // Write Configuration Byte
	{
		select_read_write_address();
		const auto port = port_num_pci_config_data + (reg_di & 3);
		IO_WriteB(static_cast<io_port_t>(port), reg_cl);
		set_return_code(PciReturnCode::Successful);
		return;
	}
	case 0x0c: // Write Configuration Word
	{
		if ((reg_di % 2) != 0) {
			// Not a multiple of 2
			set_return_code(PciReturnCode::BadRegisterNumber);
			return;
		}
		select_read_write_address();
		const auto port = port_num_pci_config_data + (reg_di & 2);
		IO_WriteW(static_cast<io_port_t>(port), reg_cx);
		set_return_code(PciReturnCode::Successful);
		return;
	}
	case 0x0d: // Write Configuration Dword
	{
		if ((reg_di % 4) != 0) {
			// Not a multiple of 4
			set_return_code(PciReturnCode::BadRegisterNumber);
			return;
		}
		select_read_write_address();
		const auto port = port_num_pci_config_data + (reg_di & 3);
		IO_WriteD(static_cast<io_port_t>(port), reg_ecx);
		set_return_code(PciReturnCode::Successful);
		return;
	}
	case 0x06: // Generate Special Cycle
	case 0x0e: // Get PCI Interrupt Routing Options
	case 0x0f: // Set PCI Hardware Interrupt
		set_return_code(PciReturnCode::FuncNotSupported);
		return;
	default:
		warn_unknown_function(reg_al);
		set_return_code(PciReturnCode::FuncNotSupported);
	}
}
