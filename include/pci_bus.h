/*
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_PCI_H
#define DOSBOX_PCI_H

#include "dosbox.h"

#include "mem.h"

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

bool PCI_IsInitialized();

RealPt PCI_GetPModeInterface();

void PCI_AddDevice(PCI_Device* dev);

uint8_t PCI_GetCFGData(Bits pci_id, Bits pci_subfunction, uint8_t regnum);

#endif
