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

#include "../ints/int10.h"

#define VOODOO_INITIAL_LFB	0xd0000000

void VOODOO_PCI_InitEnable(Bitu val);
void VOODOO_PCI_Enable(bool enable);
void VOODOO_PCI_SetLFB(uint32_t lfbaddr);

class PCI_VGADevice:public PCI_Device {
private:
	static const uint16_t vendor=0x5333;		// S3
	static const uint16_t device=0x8811;		// trio64
//	static const uint16_t device=0x8810;		// trio32
public:
	PCI_VGADevice():PCI_Device(vendor,device) {
	}

	static uint16_t VendorID(void) { return vendor; }
	static uint16_t DeviceID(void) { return device; }

	Bits ParseReadRegister(uint8_t regnum) {
		return regnum;
	}

	bool OverrideReadRegister(uint8_t, uint8_t*, uint8_t*) {
		return false;
	}

	Bits ParseWriteRegister(uint8_t regnum, uint8_t value) {
		if ((regnum>=0x18) && (regnum<0x28)) return -1;	// base addresses are read-only
		if ((regnum>=0x30) && (regnum<0x34)) return -1;	// expansion rom addresses are read-only
		switch (regnum) {
			case 0x10:
				return (pci_cfg_data[this->PCIId()][this->PCISubfunction()][0x10]&0x0f);
			case 0x11:
				return 0x00;
			case 0x12:
//				return (value&0xc0);	// -> 4mb addressable
				return (value&0x00);	// -> 16mb addressable
			case 0x13:
				return value;
			case 0x14:
				return (pci_cfg_data[this->PCIId()][this->PCISubfunction()][0x10]&0x0f);
			case 0x15:
				return 0x00;
			case 0x16:
				return value;	// -> 64kb addressable
			case 0x17:
				return value;
			default:
				break;
		}
		return value;
	}

	bool InitializeRegisters(uint8_t registers[256]) {
		// init (S3 graphics card)
//		registers[0x08] = 0x44;	// revision ID (s3 trio64v+)
		registers[0x08] = 0x00;	// revision ID
		registers[0x09] = 0x00;	// interface
		registers[0x0a] = 0x00;	// subclass type (vga compatible)
//		registers[0x0a] = 0x01;	// subclass type (xga device)
		registers[0x0b] = 0x03;	// class type (display controller)
		registers[0x0c] = 0x00;	// cache line size
		registers[0x0d] = 0x00;	// latency timer
		registers[0x0e] = 0x00;	// header type (other)

		// reset
		registers[0x04] = 0x23;	// command register (vga palette snoop, ports enabled, memory space enabled)
		registers[0x05] = 0x00;
		registers[0x06] = 0x80;	// status register (medium timing, fast back-to-back)
		registers[0x07] = 0x02;

//		registers[0x3c] = 0x0b;	// irq line
//		registers[0x3d] = 0x01;	// irq pin

//		uint32_t gfx_address_space=(((uint32_t)S3_LFB_BASE)&0xfffffff0) | 0x08;	// memory space, within first 4GB, prefetchable
		uint32_t gfx_address_space=(((uint32_t)S3_LFB_BASE)&0xfffffff0);	// memory space, within first 4GB
		registers[0x10] = (uint8_t)(gfx_address_space&0xff);		// base addres 0
		registers[0x11] = (uint8_t)((gfx_address_space>>8)&0xff);
		registers[0x12] = (uint8_t)((gfx_address_space>>16)&0xff);
		registers[0x13] = (uint8_t)((gfx_address_space>>24)&0xff);

		uint32_t gfx_address_space_mmio=(((uint32_t)S3_LFB_BASE+0x1000000)&0xfffffff0);	// memory space, within first 4GB
		registers[0x14] = (uint8_t)(gfx_address_space_mmio&0xff);		// base addres 0
		registers[0x15] = (uint8_t)((gfx_address_space_mmio>>8)&0xff);
		registers[0x16] = (uint8_t)((gfx_address_space_mmio>>16)&0xff);
		registers[0x17] = (uint8_t)((gfx_address_space_mmio>>24)&0xff);

		return true;
	}
};


class PCI_SSTDevice:public PCI_Device {
private:
	static const uint16_t vendor=0x121a;	// 3dfx
	uint16_t oscillator_ctr;
	uint16_t pci_ctr;
public:
	PCI_SSTDevice(Bitu type):PCI_Device(vendor,(type==2)?0x0002:0x0001) {
		oscillator_ctr=0;
		pci_ctr=0;
	}

	static uint16_t VendorID(void) { return vendor; }

	Bits ParseReadRegister(uint8_t regnum) {
//		LOG_MSG("SST ParseReadRegister %x",regnum);
		switch (regnum) {
			case 0x4c:
			case 0x4d:
			case 0x4e:
			case 0x4f:
				LOG_MSG("SST ParseReadRegister STATUS %x",regnum);
				break;
			case 0x54:
			case 0x55:
			case 0x56:
			case 0x57:
				if (DeviceID() >= 2) return -1;
				break;
			default:
				break;
		}
		return regnum;
	}

	bool OverrideReadRegister(uint8_t regnum, uint8_t* rval, uint8_t* rval_mask) {
		switch (regnum) {
			case 0x54:
				if (DeviceID() >= 2) {
					oscillator_ctr++;
					pci_ctr--;
					*rval=(oscillator_ctr | ((pci_ctr<<16) & 0x0fff0000)) & 0xff;
					*rval_mask=0xff;
					return true;
				}
				break;
			case 0x55:
				if (DeviceID() >= 2) {
					*rval=((oscillator_ctr | ((pci_ctr<<16) & 0x0fff0000)) >> 8) & 0xff;
					*rval_mask=0xff;
					return true;
				}
				break;
			case 0x56:
				if (DeviceID() >= 2) {
					*rval=((oscillator_ctr | ((pci_ctr<<16) & 0x0fff0000)) >> 16) & 0xff;
					*rval_mask=0xff;
					return true;
				}
				break;
			case 0x57:
				if (DeviceID() >= 2) {
					*rval=((oscillator_ctr | ((pci_ctr<<16) & 0x0fff0000)) >> 24) & 0xff;
					*rval_mask=0x0f;
					return true;
				}
				break;
			default:
				break;
		}
		return false;
	}

	Bits ParseWriteRegister(uint8_t regnum, uint8_t value) {
//		LOG_MSG("SST ParseWriteRegister %x:=%x",regnum,value);
		if ((regnum>=0x14) && (regnum<0x28)) return -1;	// base addresses are read-only
		if ((regnum>=0x30) && (regnum<0x34)) return -1;	// expansion rom addresses are read-only
		switch (regnum) {
			case 0x10:
				return (pci_cfg_data[this->PCIId()][this->PCISubfunction()][0x10]&0x0f);
			case 0x11:
				return 0x00;
			case 0x12:
				return (value&0x00);	// -> 16mb addressable (whyever)
			case 0x13:
				VOODOO_PCI_SetLFB(value<<24);
				return value;
			case 0x40:
				VOODOO_PCI_InitEnable(value&7);
				break;
			case 0x41:
			case 0x42:
			case 0x43:
				return -1;
			case 0xc0:
				VOODOO_PCI_Enable(true);
				return -1;
			case 0xe0:
				VOODOO_PCI_Enable(false);
				return -1;
			default:
				break;
		}
		return value;
	}

	bool InitializeRegisters(uint8_t registers[256]) {
		// init (3dfx voodoo)
		registers[0x08] = 0x02;	// revision
		registers[0x09] = 0x00;	// interface
//		registers[0x0a] = 0x00;	// subclass code
		registers[0x0a] = 0x00;	// subclass code (video/graphics controller)
//		registers[0x0b] = 0x00;	// class code (generic)
		registers[0x0b] = 0x04;	// class code (multimedia device)
		registers[0x0e] = 0x00;	// header type (other)

		// reset
		registers[0x04] = 0x02;	// command register (memory space enabled)
		registers[0x05] = 0x00;
		registers[0x06] = 0x80;	// status register (fast back-to-back)
		registers[0x07] = 0x00;

		registers[0x3c] = 0xff;	// no irq

		// memBaseAddr: size is 16MB
		uint32_t address_space=(((uint32_t)VOODOO_INITIAL_LFB)&0xfffffff0) | 0x08;	// memory space, within first 4GB, prefetchable
		registers[0x10] = (uint8_t)(address_space&0xff);		// base addres 0
		registers[0x11] = (uint8_t)((address_space>>8)&0xff);
		registers[0x12] = (uint8_t)((address_space>>16)&0xff);
		registers[0x13] = (uint8_t)((address_space>>24)&0xff);

		if (DeviceID() >= 2) {
			registers[0x40] = 0x00;
			registers[0x41] = 0x40;	// voodoo2 revision ID (rev4)
			registers[0x42] = 0x01;
			registers[0x43] = 0x00;
		}

		return true;
	}
};
