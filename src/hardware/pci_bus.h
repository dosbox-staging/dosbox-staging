// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PCI_H
#define DOSBOX_PCI_H

#include "dosbox.h"

#include "config/setup.h"
#include "hardware/memory.h"

// Start of PCI address space
constexpr uint32_t PciMemoryBase = 0xc000'0000; // 3072 MB max

// Graphics card
constexpr uint32_t PciGfxLfbBase  = 0xc000'0000;
constexpr uint32_t PciGfxLfbLimit = 0xc100'0000; // 16 MB max
constexpr uint32_t PciGfxMmioBase = 0xc100'0000;

// 3dfx Voodoo 3D accelerator
constexpr uint32_t PciVoodooLfbBase  = 0xd000'0000;
constexpr uint32_t PciVoodooLfbLimit = 0xd100'0000; // 16 MB max

#define PCI_MAX_PCIDEVICES   10
#define PCI_MAX_PCIFUNCTIONS 8

class PCI_Device {
protected:
	PCI_Device(const PCI_Device&)            = delete;
	PCI_Device& operator=(const PCI_Device&) = delete;

	uint16_t device_id   = 0;
	uint16_t vendor_id   = 0;
	Bits pci_id          = 0;
	Bits pci_subfunction = 0;

	// subdevices declarations, they will respond to pci functions 1 to 7
	// (main device is attached to function 0)
	Bitu num_subdevices = 0;
	PCI_Device* subdevices[PCI_MAX_PCIFUNCTIONS - 1];

public:
	PCI_Device(uint16_t vendor, uint16_t device);

	virtual ~PCI_Device() = default;

	Bits PCIId() const
	{
		return pci_id;
	}

	Bits PCISubfunction() const
	{
		return pci_subfunction;
	}

	uint16_t VendorID() const
	{
		return vendor_id;
	}

	uint16_t DeviceID() const
	{
		return device_id;
	}

	void SetPCIId(Bits number, Bits subfct);

	bool AddSubdevice(PCI_Device* dev);

	void RemoveSubdevice(Bits sub_fct);

	PCI_Device* GetSubdevice(Bits sub_fct);

	uint16_t NumSubdevices() const
	{
		if (num_subdevices > PCI_MAX_PCIFUNCTIONS - 1) {
			return static_cast<uint16_t>(PCI_MAX_PCIFUNCTIONS - 1);
		}
		return static_cast<uint16_t>(num_subdevices);
	}

	Bits GetNextSubdeviceNumber() const
	{
		if (num_subdevices >= PCI_MAX_PCIFUNCTIONS - 1) {
			return -1;
		}
		return static_cast<Bits>(num_subdevices + 1);
	}

	virtual bool InitializeRegisters(uint8_t registers[256]) = 0;

	virtual Bits ParseReadRegister(uint8_t regnum) = 0;

	virtual bool OverrideReadRegister(uint8_t regnum, uint8_t* rval,
	                                  uint8_t* rval_mask) = 0;

	virtual Bits ParseWriteRegister(uint8_t regnum, uint8_t value) = 0;
};

void PCI_Init(Section* sec);
bool PCI_IsInitialized();

RealPt PCI_GetPModeInterface();

void PCI_AddDevice(PCI_Device* dev);
void PCI_RemoveDevice(uint16_t vendor_id, uint16_t device_id);

uint8_t PCI_GetCFGData(Bits pci_id, Bits pci_subfunction, uint8_t regnum);

#endif
