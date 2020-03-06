/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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

//#define PCI_FUNCTIONALITY_ENABLED 0

#if defined PCI_FUNCTIONALITY_ENABLED

#define PCI_MAX_PCIDEVICES		10
#define PCI_MAX_PCIFUNCTIONS	8


class PCI_Device {
private:
	Bits pci_id, pci_subfunction;
	Bit16u vendor_id, device_id;

	// subdevices declarations, they will respond to pci functions 1 to 7
	// (main device is attached to function 0)
	Bitu num_subdevices;
	PCI_Device* subdevices[PCI_MAX_PCIFUNCTIONS-1];

public:
	PCI_Device(Bit16u vendor, Bit16u device);

	Bits PCIId(void) {
		return pci_id;
	}
	Bits PCISubfunction(void) {
		return pci_subfunction;
	}
	Bit16u VendorID(void) {
		return vendor_id;
	}
	Bit16u DeviceID(void) {
		return device_id;
	}

	void SetPCIId(Bitu number, Bits subfct);

	bool AddSubdevice(PCI_Device* dev);
	void RemoveSubdevice(Bits subfct);

	PCI_Device* GetSubdevice(Bits subfct);

	Bit16u NumSubdevices(void) {
		if (num_subdevices>PCI_MAX_PCIFUNCTIONS-1) return (Bit16u)(PCI_MAX_PCIFUNCTIONS-1);
		return (Bit16u)num_subdevices;
	}

	Bits GetNextSubdeviceNumber(void) {
		if (num_subdevices>=PCI_MAX_PCIFUNCTIONS-1) return -1;
		return (Bits)num_subdevices+1;
	}

	virtual Bits ParseReadRegister(Bit8u regnum)=0;
	virtual bool OverrideReadRegister(Bit8u regnum, Bit8u* rval, Bit8u* rval_mask)=0;
	virtual Bits ParseWriteRegister(Bit8u regnum,Bit8u value)=0;
	virtual bool InitializeRegisters(Bit8u registers[256])=0;

};

bool PCI_IsInitialized();

RealPt PCI_GetPModeInterface(void);

#endif

#endif
