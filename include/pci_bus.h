/*
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

#define PCI_FUNCTIONALITY_ENABLED 1

#if defined PCI_FUNCTIONALITY_ENABLED

#define PCI_MAX_PCIDEVICES		10
#define PCI_MAX_PCIFUNCTIONS	8


class PCI_Device {
private:
	Bits pci_id, pci_subfunction;
	uint16_t vendor_id, device_id;

	// subdevices declarations, they will respond to pci functions 1 to 7
	// (main device is attached to function 0)
	Bitu num_subdevices;
	PCI_Device* subdevices[PCI_MAX_PCIFUNCTIONS-1];

public:
	PCI_Device(uint16_t vendor, uint16_t device);

	Bits PCIId(void) {
		return pci_id;
	}
	Bits PCISubfunction(void) {
		return pci_subfunction;
	}
	uint16_t VendorID(void) {
		return vendor_id;
	}
	uint16_t DeviceID(void) {
		return device_id;
	}

	void SetPCIId(Bitu number, Bits subfct);

	bool AddSubdevice(PCI_Device* dev);
	void RemoveSubdevice(Bits subfct);

	PCI_Device* GetSubdevice(Bits subfct);

	uint16_t NumSubdevices(void) {
		if (num_subdevices>PCI_MAX_PCIFUNCTIONS-1) return (uint16_t)(PCI_MAX_PCIFUNCTIONS-1);
		return (uint16_t)num_subdevices;
	}

	Bits GetNextSubdeviceNumber(void) {
		if (num_subdevices>=PCI_MAX_PCIFUNCTIONS-1) return -1;
		return (Bits)num_subdevices+1;
	}

	virtual Bits ParseReadRegister(uint8_t regnum)=0;
	virtual bool OverrideReadRegister(uint8_t regnum, uint8_t* rval, uint8_t* rval_mask)=0;
	virtual Bits ParseWriteRegister(uint8_t regnum,uint8_t value)=0;
	virtual bool InitializeRegisters(uint8_t registers[256])=0;
	virtual ~PCI_Device() = 0;

};

bool PCI_IsInitialized();

RealPt PCI_GetPModeInterface(void);

#endif

void PCI_AddSST_Device(Bitu type);
void PCI_RemoveSST_Device(void);

#endif
