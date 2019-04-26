/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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

#include <string.h>

// from http://www.calel.org/pci-devices/xorg-device-list.html#S3
// 86c764_0 [Trio 32 vers 0] 5333:8810
// 86c764/765 [Trio32/64/64V+] 5333:8811
// 86c764_3 [Trio 32/64 vers 3] 5333:8813
class PCI_S3Trio3264: public PCI_Device {
private:
	static const Bit16u vendor=0x5333;
	static const Bit16u device=0x8813;
public:
	PCI_S3Trio3264(): PCI_Device(vendor,device) {};

	virtual bool InitializeRegisters(Bit8u registers[256]) {
		memset(registers, 0, 256);
	}

	virtual Bits ParseReadRegister(Bit8u regnum) {
		return regnum;
	}

	virtual Bits ParseWriteRegister(Bit8u regnum,Bit8u value) {
		return value;
	}

	virtual bool OverrideReadRegister(Bit8u regnum, Bit8u* rval, Bit8u* rval_mask) {
		return false;
	}
};
