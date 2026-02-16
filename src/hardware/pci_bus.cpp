// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "pci_bus.h"

#include <array>
#include <memory>

#include "config/setup.h"
#include "cpu/callback.h"
#include "cpu/registers.h"
#include "debugger/debugger.h"
#include "dosbox.h"
#include "hardware/memory.h"
#include "hardware/port.h"
#include "misc/support.h"

// Current PCI addressing
static uint32_t pci_caddress = 0;

// Number of registered PCI devices
static Bitu pci_devices_installed = 0;

// PCI configuration data
static uint8_t pci_cfg_data[PCI_MAX_PCIDEVICES][PCI_MAX_PCIFUNCTIONS][256];

static std::array<std::unique_ptr<PCI_Device>, PCI_MAX_PCIDEVICES> pci_devices;

// PCI address
// 31    - set for a PCI access
// 30-24 - 0
// 23-16 - bus number			(0x00ff0000)
// 15-11 - device number (slot)	(0x0000f800)
// 10- 8 - subfunction number	(0x00000700)
//  7- 2 - config register #	(0x000000fc)

static void write_pci_addr([[maybe_unused]] io_port_t port, io_val_t val, io_width_t)
{
	LOG(LOG_PCI, LOG_NORMAL)("PCI: Write PCI address :=%x", val);
	pci_caddress = val;
}

static void write_pci_register(PCI_Device* dev, uint8_t regnum, uint8_t value)
{
	// vendor/device/class IDs/header type/etc. are read-only
	if ((regnum < 0x04) || ((regnum >= 0x06) && (regnum < 0x0c)) ||
	    (regnum == 0x0e)) {
		return;
	}
	if (dev == nullptr) {
		return;
	}
	switch (pci_cfg_data[dev->PCIId()][dev->PCISubfunction()][0x0e] & 0x7f) {
		// header-type specific handling
	case 0x00:
		if ((regnum >= 0x28) && (regnum < 0x30)) {
			// subsystem information is read-only
			return;
		}
		break;
	case 0x01:
	case 0x02:
	default: break;
	}

	// Call device routine for special actions and the
	// possibility to discard/replace the value that is to be written.
	Bits parsed_register = dev->ParseWriteRegister(regnum, value);
	if (parsed_register >= 0) {
		pci_cfg_data[dev->PCIId()][dev->PCISubfunction()][regnum] =
		        (uint8_t)(parsed_register & 0xff);
	}
}

static void write_pci(const io_port_t port, const io_val_t value,
                      const io_width_t width)
{
	// write_pci() is only ever registered as an 8-bit handler, despite
	// appearing to handle up to 32-bit requests. Let's check that.
	const auto val = check_cast<uint8_t>(value);
	assert(width == io_width_t::byte);

	LOG(LOG_PCI, LOG_NORMAL)("PCI: Write to port: %x, value: %x (io_width=%d)",
	                         port,
	                         val,
	                         static_cast<int>(width));

	// Check for enabled/bus 0
	if ((pci_caddress & 0x80ff0000) == 0x80000000) {
		auto devnum = (uint8_t)((pci_caddress >> 11) & 0x1f);
		auto fctnum = (uint8_t)((pci_caddress >> 8) & 0x7);
		auto regnum = (uint8_t)((pci_caddress & 0xfc) + (port & 0x03));
		LOG(LOG_PCI, LOG_NORMAL)("PCI: Write to device %x register %x (function %x) (:=%x)",
		                         devnum,
		                         regnum,
		                         fctnum,
		                         val);

		if (devnum >= pci_devices_installed) {
			return;
		}
		PCI_Device* selected_device = pci_devices[devnum].get();
		if (selected_device == nullptr) {
			return;
		}
		if (fctnum > selected_device->NumSubdevices()) {
			return;
		}

		PCI_Device* dev = selected_device->GetSubdevice(fctnum);
		if (dev == nullptr) {
			return;
		}

		// Write 8-bit data to PCI device/configuration
		write_pci_register(dev, regnum, val);

		// (WORD and DWORD writes aren't performed because no port
		// registers these types)
	}
}

static uint32_t read_pci_addr([[maybe_unused]] io_port_t port, io_width_t)
{
	LOG(LOG_PCI, LOG_NORMAL)("PCI: Read PCI address -> %x", pci_caddress);
	return pci_caddress;
}

// Read single 8bit value from register file (special register treatment included)
static uint8_t read_pci_register(PCI_Device* dev, uint8_t regnum)
{
	switch (regnum) {
	case 0x00: return (uint8_t)(dev->VendorID() & 0xff);
	case 0x01: return (uint8_t)((dev->VendorID() >> 8) & 0xff);
	case 0x02: return (uint8_t)(dev->DeviceID() & 0xff);
	case 0x03: return (uint8_t)((dev->DeviceID() >> 8) & 0xff);

	case 0x0e:
		return (pci_cfg_data[dev->PCIId()][dev->PCISubfunction()][regnum] &
		        0x7f) |
		       ((dev->NumSubdevices() > 0) ? 0x80 : 0x00);
	default: break;
	}

	// Call device routine for special actions and possibility to
	// discard/remap register.
	Bits parsed_regnum = dev->ParseReadRegister(regnum);
	if ((parsed_regnum >= 0) && (parsed_regnum < 256)) {
		return pci_cfg_data[dev->PCIId()][dev->PCISubfunction()][parsed_regnum];
	}

	uint8_t newval, mask;
	if (dev->OverrideReadRegister(regnum, &newval, &mask)) {
		uint8_t oldval = pci_cfg_data[dev->PCIId()][dev->PCISubfunction()][regnum] &
		                 (~mask);
		return oldval | (newval & mask);
	}

	return 0xff;
}

static uint8_t read_pci(const io_port_t port, [[maybe_unused]] io_width_t width)
{
	// read_pci() is only ever registered as an 8-bit handler, despite
	// appearing to handle up to 32-bit requests. Let's check that.
	assert(width == io_width_t::byte);

	LOG(LOG_PCI, LOG_NORMAL)("PCI: Read PCI data -> %x", pci_caddress);

	if ((pci_caddress & 0x80ff0000) == 0x80000000) {
		auto devnum = (uint8_t)((pci_caddress >> 11) & 0x1f);
		auto fctnum = (uint8_t)((pci_caddress >> 8) & 0x7);
		auto regnum = (uint8_t)((pci_caddress & 0xfc) + (port & 0x03));

		if (devnum >= pci_devices_installed) {
			return 0xff;
		}

		LOG(LOG_PCI, LOG_NORMAL)("PCI: Read from device %x register %x (function %x); addr %x",
		                         devnum,
		                         regnum,
		                         fctnum,
		                         pci_caddress);

		PCI_Device* selected_device = pci_devices[devnum].get();

		if (selected_device == nullptr) {
			return 0xff;
		}
		if (fctnum > selected_device->NumSubdevices()) {
			return 0xff;
		}

		PCI_Device* dev = selected_device->GetSubdevice(fctnum);

		if (dev != nullptr) {
			return read_pci_register(dev, regnum);
		}
	}
	return 0xff;
}

static Bitu PCI_PM_Handler()
{
	LOG_MSG("PCI: PMode handler, function %x", reg_ax);
	return CBRET_NONE;
}

PCI_Device::PCI_Device(uint16_t vendor, uint16_t device)
{
	pci_id          = -1;
	pci_subfunction = -1;
	vendor_id       = vendor;
	device_id       = device;
	num_subdevices  = 0;
	for (Bitu dct = 0; dct < PCI_MAX_PCIFUNCTIONS - 1; dct++) {
		subdevices[dct] = nullptr;
	}
}

void PCI_Device::SetPCIId(const Bits number, const Bits sub_fct)
{
	if ((number >= 0) && (number < PCI_MAX_PCIDEVICES)) {
		pci_id = number;
		if ((sub_fct >= 0) && (sub_fct < PCI_MAX_PCIFUNCTIONS - 1)) {
			pci_subfunction = sub_fct;
		} else {
			pci_subfunction = -1;
		}
	}
}

bool PCI_Device::AddSubdevice(PCI_Device* dev)
{
	if (num_subdevices < PCI_MAX_PCIFUNCTIONS - 1) {
		if (subdevices[num_subdevices] != nullptr) {
			E_Exit("PCI: subdevice slot already in use!");
		}
		subdevices[num_subdevices] = dev;
		num_subdevices++;
		return true;
	}
	return false;
}

void PCI_Device::RemoveSubdevice(Bits subfct)
{
	if ((subfct > 0) && (subfct < PCI_MAX_PCIFUNCTIONS)) {
		if (subfct <= this->NumSubdevices()) {
			delete subdevices[subfct - 1];
			subdevices[subfct - 1] = nullptr;
			// should adjust things like num_subdevices as well...
		}
	}
}

PCI_Device* PCI_Device::GetSubdevice(const Bits sub_fct)
{
	if (sub_fct >= PCI_MAX_PCIFUNCTIONS) {
		return nullptr;
	}

	if (sub_fct == 0) {
		return this;
	}

	if (sub_fct > 0 && sub_fct <= this->NumSubdevices()) {
		return subdevices[sub_fct - 1];
	}

	return nullptr;
}

// Queued devices (PCI device registering requested before the PCI framework
// was initialized)
static const Bitu max_rqueued_devices = 16;

static Bitu num_rqueued_devices = 0;

static PCI_Device* rqueued_devices[max_rqueued_devices];

class PCI {
private:
	bool initialized = false;

	IO_WriteHandleObject PCI_WriteHandler[5];
	IO_ReadHandleObject PCI_ReadHandler[5];

	CALLBACK_HandlerObject callback_pci;

public:
	PhysPt GetPModeCallbackPointer()
	{
		return RealToPhysical(callback_pci.Get_RealPointer());
	}

	bool IsInitialized()
	{
		return initialized;
	}

	// Set up port handlers and configuration data
	void InitializePCI()
	{
		// Install PCI-addressing ports
		PCI_WriteHandler[0].Install(port_num_pci_config_address,
		                            write_pci_addr,
		                            io_width_t::dword);

		PCI_ReadHandler[0].Install(port_num_pci_config_address,
		                           read_pci_addr,
		                           io_width_t::dword);

		// Install PCI-register read/write handlers
		for (uint8_t ct = 0; ct < 4; ++ct) {
			PCI_WriteHandler[1 + ct].Install(port_num_pci_config_data + ct,
			                                 write_pci,
			                                 io_width_t::byte);

			PCI_ReadHandler[1 + ct].Install(port_num_pci_config_data + ct,
			                                read_pci,
			                                io_width_t::byte);
		}

		for (Bitu dev = 0; dev < PCI_MAX_PCIDEVICES; dev++) {
			for (Bitu fct = 0; fct < PCI_MAX_PCIFUNCTIONS - 1; fct++) {
				for (Bitu reg = 0; reg < 256; reg++) {
					pci_cfg_data[dev][fct][reg] = 0;
				}
			}
		}

		callback_pci.Install(&PCI_PM_Handler, CB_IRETD, "PCI PM");

		initialized = true;
	}

	// Register PCI device to bus and setup data
	Bits RegisterPCIDevice(PCI_Device* device, Bits slot = -1)
	{
		if (device == nullptr) {
			return -1;
		}

		if (slot >= 0) {
			// Specific slot specified, basic check for validity
			if (slot >= PCI_MAX_PCIDEVICES) {
				return -1;
			}
		} else {
			// Auto-add to new slot, check if one is still free
			if (pci_devices_installed >= PCI_MAX_PCIDEVICES) {
				return -1;
			}
		}

		if (!initialized) {
			InitializePCI();
		}

		if (slot < 0) {
			// Use next slot
			slot = pci_devices_installed;
		}

		// Main device unless specific already-occupied slot is requested
		Bits subfunction = 0;

		if (pci_devices[slot] != nullptr) {
			subfunction = pci_devices[slot]->GetNextSubdeviceNumber();
			if (subfunction < 0) {
				E_Exit("PCI: Too many PCI subdevices!");
			}
		}

		if (device->InitializeRegisters(pci_cfg_data[slot][subfunction])) {
			device->SetPCIId(slot, subfunction);
			if (pci_devices[slot] == nullptr) {
				pci_devices[slot] = std::unique_ptr<PCI_Device>(device);
				pci_devices_installed++;
			} else {
				pci_devices[slot]->AddSubdevice(device);
			}

			return slot;
		}

		return -1;
	}

	void Deinitialize()
	{
		initialized           = false;
		pci_devices_installed = 0;
		num_rqueued_devices   = 0;
		pci_caddress          = 0;

		for (Bitu dev = 0; dev < PCI_MAX_PCIDEVICES; dev++) {
			for (Bitu fct = 0; fct < PCI_MAX_PCIFUNCTIONS - 1; fct++) {
				for (Bitu reg = 0; reg < 256; reg++) {
					pci_cfg_data[dev][fct][reg] = 0;
				}
			}
		}

		// install PCI-addressing ports
		PCI_WriteHandler[0].Uninstall();
		PCI_ReadHandler[0].Uninstall();
		// install PCI-register read/write handlers
		for (Bitu ct = 0; ct < 4; ct++) {
			PCI_WriteHandler[1 + ct].Uninstall();
			PCI_ReadHandler[1 + ct].Uninstall();
		}

		callback_pci.Uninstall();
	}

	void RemoveDevice(uint16_t vendor_id, uint16_t device_id)
	{
		for (Bitu dct = 0; dct < pci_devices_installed; dct++) {
			if (pci_devices[dct] != nullptr) {
				if (pci_devices[dct]->NumSubdevices() > 0) {
					for (Bitu sct = 1; sct < PCI_MAX_PCIFUNCTIONS;
					     sct++) {
						PCI_Device* sdev =
						        pci_devices[dct]->GetSubdevice(
						                sct);
						if (sdev != nullptr) {
							if ((sdev->VendorID() ==
							     vendor_id) &&
							    (sdev->DeviceID() ==
							     device_id)) {
								pci_devices[dct]->RemoveSubdevice(
								        sct);
							}
						}
					}
				}

				if ((pci_devices[dct]->VendorID() == vendor_id) &&
				    (pci_devices[dct]->DeviceID() == device_id)) {
					pci_devices[dct].reset();
				}
			}
		}

		// Check if all devices have been removed
		bool any_device_left = false;
		for (Bitu dct = 0; dct < PCI_MAX_PCIDEVICES; dct++) {
			if (dct >= pci_devices_installed) {
				break;
			}
			if (pci_devices[dct] != nullptr) {
				any_device_left = true;
				break;
			}
		}
		if (!any_device_left) {
			Deinitialize();
		}

		Bitu last_active_device = PCI_MAX_PCIDEVICES;
		for (Bitu dct = 0; dct < PCI_MAX_PCIDEVICES; dct++) {
			if (pci_devices[dct] != nullptr) {
				last_active_device = dct;
			}
		}
		if (last_active_device < pci_devices_installed) {
			pci_devices_installed = last_active_device + 1;
		}
	}

	PCI() : callback_pci{}
	{
		pci_devices_installed = 0;

		for (Bitu devct = 0; devct < PCI_MAX_PCIDEVICES; devct++) {
			pci_devices[devct] = nullptr;
		}

		if (num_rqueued_devices > 0) {
			// Register all devices that have been added before the
			// PCI bus was instantiated.
			for (Bitu dct = 0; dct < num_rqueued_devices; dct++) {
				this->RegisterPCIDevice(rqueued_devices[dct]);
			}
			num_rqueued_devices = 0;
		}
	}

	~PCI()
	{
		initialized           = false;
		pci_devices_installed = 0;
		num_rqueued_devices   = 0;
	}
};

static std::unique_ptr<PCI> pci_interface = {};

PhysPt PCI_GetPModeInterface()
{
	if (pci_interface) {
		return pci_interface->GetPModeCallbackPointer();
	}
	return 0;
}

bool PCI_IsInitialized()
{
	if (pci_interface) {
		return pci_interface->IsInitialized();
	}
	return false;
}

void PCI_AddDevice(PCI_Device* dev)
{
	if (pci_interface != nullptr) {
		pci_interface->RegisterPCIDevice(dev);
	} else {
		if (num_rqueued_devices < max_rqueued_devices) {
			rqueued_devices[num_rqueued_devices++] = dev;
		}
	}
}

void PCI_RemoveDevice(const uint16_t vendor_id, const uint16_t device_id)
{
	if (pci_interface) {
		pci_interface->RemoveDevice(vendor_id, device_id);
	}
}

uint8_t PCI_GetCFGData(Bits pci_id, Bits pci_subfunction, uint8_t regnum)
{
	return pci_cfg_data[pci_id][pci_subfunction][regnum];
}

void PCI_Destroy()
{
	pci_interface = {};
}

void PCI_Init()
{
	pci_interface = std::make_unique<PCI>();
}

