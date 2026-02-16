// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/dos_system.h"

#include <cstring>

#include "dosbox.h"
#include "cpu/callback.h"
#include "cpu/registers.h"
#include "dev_con.h"
#include "dos.h"
#include "dos/drives.h"
#include "hardware/memory.h"
#include "ints/bios.h"
#include "misc/support.h"

DOS_Device * Devices[DOS_DEVICES];

struct ExtDeviceData {
	uint16_t attribute;
	uint16_t segment;
	uint16_t strategy;
	uint16_t interrupt;
};

class DOS_ExtDevice : public DOS_Device {
public:
	DOS_ExtDevice(const char *name, uint16_t seg, uint16_t off)
	{
		SetName(name);
		ext.attribute = real_readw(seg, off + 4);
		ext.segment = seg;
		ext.strategy = real_readw(seg, off + 6);
		ext.interrupt = real_readw(seg, off + 8);
	}
	bool Read(uint8_t* data, uint16_t* size) override;
	bool Write(uint8_t* data, uint16_t* size) override;
	bool Seek(uint32_t* pos, uint32_t type) override;
	void Close() override;
	uint16_t GetInformation() override;
	bool ReadFromControlChannel(PhysPt bufptr, uint16_t size,
	                            uint16_t* retcode) override;
	bool WriteToControlChannel(PhysPt bufptr, uint16_t size,
	                           uint16_t* retcode) override;
	uint8_t GetStatus(bool input_flag) override;
	bool CheckSameDevice(uint16_t seg, uint16_t s_off, uint16_t i_off);

private:
	struct ExtDeviceData ext = {};

	uint16_t CallDeviceFunction(uint8_t command, uint8_t length, PhysPt bufptr, uint16_t size);
};

bool DOS_ExtDevice::CheckSameDevice(uint16_t seg, uint16_t s_off, uint16_t i_off)
{
	if (seg == ext.segment && s_off == ext.strategy && i_off == ext.interrupt) {
		return true;
	}
	return false;
}

uint16_t DOS_ExtDevice::CallDeviceFunction(uint8_t command, uint8_t length, PhysPt bufptr, uint16_t size)
{
	uint16_t oldbx = reg_bx;
	uint16_t oldes = SegValue(es);

	real_writeb(dos.dcp, 0, length);
	real_writeb(dos.dcp, 1, 0);
	real_writeb(dos.dcp, 2, command);
	real_writew(dos.dcp, 3, 0);
	real_writed(dos.dcp, 5, 0);
	real_writed(dos.dcp, 9, 0);
	real_writeb(dos.dcp, 13, 0);
	real_writew(dos.dcp, 14, (uint16_t)(bufptr & 0x000f));
	real_writew(dos.dcp, 16, (uint16_t)(bufptr >> 4));
	real_writew(dos.dcp, 18, size);

	reg_bx = 0;
	SegSet16(es, dos.dcp);
	CALLBACK_RunRealFar(ext.segment, ext.strategy);
	CALLBACK_RunRealFar(ext.segment, ext.interrupt);
	reg_bx = oldbx;
	SegSet16(es, oldes);

	return real_readw(dos.dcp, 3);
}

bool DOS_ExtDevice::ReadFromControlChannel(PhysPt bufptr, uint16_t size, uint16_t *retcode)
{
	if (ext.attribute & 0x4000) {
		// IOCTL INPUT
		if ((CallDeviceFunction(3, 26, bufptr, size) & 0x8000) == 0) {
			*retcode = real_readw(dos.dcp, 18);
			return true;
		}
	}
	return false;
}

bool DOS_ExtDevice::WriteToControlChannel(PhysPt bufptr, uint16_t size, uint16_t *retcode)
{
	if (ext.attribute & 0x4000) {
		// IOCTL OUTPUT
		if ((CallDeviceFunction(12, 26, bufptr, size) & 0x8000) == 0) {
			*retcode = real_readw(dos.dcp, 18);
			return true;
		}
	}
	return false;
}

bool DOS_ExtDevice::Read(uint8_t *data, uint16_t *size)
{
	PhysPt bufptr = (dos.dcp << 4) | 32;
	for (uint16_t no = 0; no < *size; no++) {
		// INPUT
		if (CallDeviceFunction(4, 26, bufptr, 1) & 0x8000) {
			return false;
		} else {
			if (real_readw(dos.dcp, 18) != 1) {
				return false;
			}
			*data++ = mem_readb(bufptr);
		}
	}
	return true;
}

bool DOS_ExtDevice::Write(uint8_t *data, uint16_t *size)
{
	PhysPt bufptr = (dos.dcp << 4) | 32;
	for (uint16_t no = 0; no < *size; no++) {
		mem_writeb(bufptr, *data);
		// OUTPUT
		if (CallDeviceFunction(8, 26, bufptr, 1) & 0x8000) {
			return false;
		} else {
			if (real_readw(dos.dcp, 18) != 1) {
				return false;
			}
		}
		data++;
	}
	return true;
}

void DOS_ExtDevice::Close()
{
}

bool DOS_ExtDevice::Seek([[maybe_unused]] uint32_t *pos, [[maybe_unused]] uint32_t type)
{
	return true;
}

uint16_t DOS_ExtDevice::GetInformation()
{
	// bit9=1 .. ExtDevice
	return (ext.attribute & 0xc07f) | 0x0080 | EXT_DEVICE_BIT;
}

uint8_t DOS_ExtDevice::GetStatus(bool input_flag)
{
	uint16_t status;
	if (input_flag) {
		// NON-DESTRUCTIVE INPUT NO WAIT
		status = CallDeviceFunction(5, 14, 0, 0);
	} else {
		// OUTPUT STATUS
		status = CallDeviceFunction(10, 13, 0, 0);
	}
	// check NO ERROR & BUSY
	if ((status & 0x8200) == 0) {
		return 0xff;
	}
	return 0x00;
}

// Walk the DOS devices and return the real pointer to the device matching the
// given name, provided it's not an existing driver (if indicated).
RealPt DOS_CheckExtDevice(const std::string_view name, const bool skip_existing_drivers)
{
	// Helper lambda's to check various device properties
	//
	auto is_a_driver = [](const RealPt rp) {
		constexpr uint16_t driver_flag = (1 << 15);
		return DOS_DeviceHasAttributes(rp, driver_flag);
	};

	auto is_con_or_nul = [](const RealPt rp) {
		constexpr uint16_t con_strategy  = 0;
		constexpr uint16_t con_interrupt = 0;

		constexpr uint16_t nul_strategy  = 0xffff;
		constexpr uint16_t nul_interrupt = 0xffff;

		const auto strategy  = DOS_GetDeviceStrategy(rp);
		const auto interrupt = DOS_GetDeviceInterrupt(rp);

		return (strategy == con_strategy && interrupt == con_interrupt) ||
		       (strategy == nul_strategy && interrupt == nul_interrupt);
	};

	auto is_existing_driver = [](const RealPt rp) {
		for (const auto& dev : Devices) {
			if (!dev || (dev->GetInformation() & EXT_DEVICE_BIT) == 0) {
				continue;
			}
			const auto ext_dev = dynamic_cast<DOS_ExtDevice*>(dev);
			if (ext_dev &&
			    ext_dev->CheckSameDevice(RealSegment(rp),
			                             DOS_GetDeviceStrategy(rp),
			                             DOS_GetDeviceInterrupt(rp))) {
				return true;
			}
		}
		return false;
	};

	// start walking the device chain of real pointers
	auto rp = dos_infoblock.GetDeviceChain();

	while (!DOS_IsEndPointer(rp)) {
		if (!is_a_driver(rp) || !DOS_DeviceHasName(rp, name)) {
			rp = DOS_GetNextDevice(rp);
			continue;
		}
		if (is_con_or_nul(rp)) {
			return 0;
		}
		if (skip_existing_drivers && is_existing_driver(rp)) {
			return 0;
		}
		// The device at the real pointer is a driver, has a name
		// matching the given name, is neither the CON nor NUL devices,
		// and (if requested) is not an existing device driver.
		return rp;
	}
	return 0;
}

static void DOS_CheckOpenExtDevice(const char *name)
{
	uint32_t addr;

	if ((addr = DOS_CheckExtDevice(name, true)) != 0) {
		auto device = new DOS_ExtDevice(name, addr >> 16, addr & 0xffff);
		DOS_AddDevice(device);
	}
}

class device_NUL : public DOS_Device {
public:
	device_NUL()
	{
		SetName("NUL");
	}

	bool Read(uint8_t* /*data*/, uint16_t* size) override
	{
		*size = 0; // Return success and no data read.
		LOG(LOG_IOCTL, LOG_NORMAL)("%s:READ", GetName());
		return true;
	}
	bool Write(uint8_t* /*data*/, uint16_t* /*size*/) override
	{
		LOG(LOG_IOCTL, LOG_NORMAL)("%s:WRITE", GetName());
		return true;
	}
	bool Seek(uint32_t* /*pos*/, uint32_t /*type*/) override
	{
		LOG(LOG_IOCTL, LOG_NORMAL)("%s:SEEK", GetName());
		return true;
	}
	void Close() override
	{
	}
	uint16_t GetInformation() override
	{
		return 0x8084;
	}
	bool ReadFromControlChannel(PhysPt /*bufptr*/, uint16_t /*size*/,
	                            uint16_t* /*retcode*/) override
	{
		return false;
	}
	bool WriteToControlChannel(PhysPt /*bufptr*/, uint16_t /*size*/,
	                           uint16_t* /*retcode*/) override
	{
		return false;
	}
};

class device_LPT1 final : public device_NUL {
public:
	device_LPT1()
	{
		SetName("LPT1");
	}
	uint16_t GetInformation() override
	{
		return 0x80A0;
	}
	bool Read(uint8_t* /*data*/, uint16_t* /*size*/) override
	{
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
};

bool DOS_Device::Read(uint8_t * data,uint16_t * size) {
	return Devices[devnum]->Read(data,size);
}

bool DOS_Device::Write(uint8_t * data,uint16_t * size) {
	return Devices[devnum]->Write(data,size);
}

bool DOS_Device::Seek(uint32_t * pos,uint32_t type) {
	return Devices[devnum]->Seek(pos,type);
}

void DOS_Device::Close() {
	Devices[devnum]->Close();
}

uint16_t DOS_Device::GetInformation() { 
	return Devices[devnum]->GetInformation();
}

bool DOS_Device::ReadFromControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { 
	return Devices[devnum]->ReadFromControlChannel(bufptr,size,retcode);
}

bool DOS_Device::WriteToControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { 
	return Devices[devnum]->WriteToControlChannel(bufptr,size,retcode);
}

uint8_t DOS_Device::GetStatus(bool input_flag)
{
	uint16_t info = Devices[devnum]->GetInformation();
	if (info & EXT_DEVICE_BIT) {
		return Devices[devnum]->GetStatus(input_flag);
	}
	return (info & 0x40) ? 0x00 : 0xff;
}

DOS_File &DOS_File::operator=(const DOS_File &orig)
{
	flags = orig.flags;
	time=orig.time;
	date=orig.date;
	attr=orig.attr;
	refCtr=orig.refCtr;
	flush_time_on_close = orig.flush_time_on_close;
	hdrive=orig.hdrive;
	name = orig.name;
	return *this;
}

uint8_t DOS_FindDevice(const char* name)
{
	/* should only check for the names before the dot and spacepadded */
	char fullname[DOS_PATHLENGTH];uint8_t drive;
//	if(!name || !(*name)) return DOS_DEVICES; //important, but makename does it
	if (!DOS_MakeName(name,fullname,&drive)) return DOS_DEVICES;

	char* name_part = strrchr(fullname,'\\');
	if(name_part) {
		*name_part++ = 0;
		// Check validity of leading directory.
		if(!Drives.at(drive)->TestDir(fullname)) {
			return DOS_DEVICES;
		}
	} else
		name_part = fullname;

	char* dot = strrchr(name_part,'.');
	if(dot) *dot = 0; //no ext checking

	DOS_CheckOpenExtDevice(name_part);
	for (int index = DOS_DEVICES - 1; index >= 0; index--) {
		if (Devices[index]) {
			if (Devices[index]->GetInformation() & EXT_DEVICE_BIT) {
				if (wild_file_cmp(name_part,
				                  Devices[index]->name.c_str())) {
					if (DOS_CheckExtDevice(name_part, false) != 0) {
						return index;
					} else {
						delete Devices[index];
						Devices[index] = nullptr;
						break;
					}
				}
			} else {
				break;
			}
		}
	}

	// AUX is alias for COM1 and PRN for LPT1
	// A bit of a hack. (but less then before).
	// no need for casecmp as makename returns uppercase
	static char com[] = "COM1";
	static char lpt[] = "LPT1";
	if (strcmp(name_part, "AUX") == 0)
		name_part = com;
	if (strcmp(name_part, "PRN") == 0)
		name_part = lpt;

	/* loop through devices */
	for(uint8_t index = 0;index < DOS_DEVICES;index++) {
		if (Devices[index]) {
			if (wild_file_cmp(name_part, Devices[index]->GetName())) {
				return index;
			}
		}
	}
	return DOS_DEVICES;
}

void DOS_AddDevice(DOS_Device * adddev) {
//Caller creates the device. We store a pointer to it
//TODO Give the Device a real handler in low memory that responds to calls
	for(Bitu i = 0; i < DOS_DEVICES;i++) {
		if(!Devices[i]){
			Devices[i] = adddev;
			Devices[i]->SetDeviceNumber(i);
			return;
		}
	}
	E_Exit("DOS:Too many devices added");
}

void DOS_DelDevice(DOS_Device * dev) {
// We will destroy the device if we find it in our list.
// TODO:The file table is not checked to see the device is opened somewhere!
	for (Bitu i = 0; i <DOS_DEVICES;i++) {
		if (Devices[i] && !strcasecmp(Devices[i]->GetName(), dev->GetName())) {
			delete Devices[i];
			Devices[i] = nullptr;
			return;
		}
	}
}

// Structure of internal DOS tables, Device Driver
//
// Property   Offset  Type      Description
// ~~~~~~~~   ~~~~~~  ~~~~~     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// next_rpt   00h     DWord --> Next driver in chain (x:FFFF means end)
// attributes 04h     Word      Device attributes.
// strategy   06h     Word      Device strategy routine offset.
// interrupt  08h     Word      Device interrupt routine offset.
// name       0Ah     8 Bytes   Device name padded with spaces.
//
// Ref: https://www.infradead.org/devload/DOSTables.html, Appendix 2
//
namespace DeviceDriverInfo {
constexpr uint8_t next_rpt_offset = 0x00;   // DWORD, points to next driver in
                                            // chain (x:FFFF means end)
constexpr uint8_t attributes_offset = 0x04; // WORD, device attributes offset
constexpr uint8_t strategy_offset   = 0x06; // WORD, device strategy routine
                                            // offset.
constexpr uint8_t interrupt_offset = 0x08;  // WORD, device interrupt
                                            // routine offset.
constexpr uint8_t name_offset = 0x0a;       // 8 bytes, device name padded with
                                            // spaces.
constexpr size_t name_length = 8;
} // namespace DeviceDriverInfo

// Special pointer value indicating the end of the DOS device linked list
bool DOS_IsEndPointer(const RealPt rp)
{
	constexpr uint16_t last_offset_marker = 0xffff;
	return RealOffset(rp) == last_offset_marker;
}

// From the given real pointer, get the next device driver's real pointer
RealPt DOS_GetNextDevice(const RealPt rp)
{
	return real_readd(RealSegment(rp),
	                  RealOffset(rp) + DeviceDriverInfo::next_rpt_offset);
}

// Get the tail real pointer from the DOS device driver linked list
RealPt DOS_GetLastDevice()
{
	RealPt current = dos_infoblock.GetDeviceChain();
	RealPt next = current;

	while (!DOS_IsEndPointer(next)) {
		current = next;
		next = DOS_GetNextDevice(current);
	}

	return current;
}

// Append the device at the given address to the end of the DOS device linked list
void DOS_AppendDevice(const uint16_t segment, const uint16_t offset)
{
	const auto new_rp  = RealMake(segment, offset);
	const auto tail_rp = DOS_GetLastDevice();
	real_writed(RealSegment(tail_rp), RealOffset(tail_rp), new_rp);
}

bool DOS_DeviceHasAttributes(const RealPt rp, const uint16_t req_attributes)
{
	const auto attributes = real_readw(
	        RealSegment(rp), RealOffset(rp) + DeviceDriverInfo::attributes_offset);

	return (attributes & req_attributes) == req_attributes;
}

uint16_t DOS_GetDeviceStrategy(const RealPt rp)
{
	return real_readw(RealSegment(rp),
	                  RealOffset(rp) + DeviceDriverInfo::strategy_offset);
}

uint16_t DOS_GetDeviceInterrupt(const RealPt rp)
{
	return real_readw(RealSegment(rp),
	                  RealOffset(rp) + DeviceDriverInfo::interrupt_offset);
}

bool DOS_DeviceHasName(const RealPt rp, const std::string_view req_name)
{
	const auto segment    = RealSegment(rp);
	const auto offset     = RealOffset(rp) + DeviceDriverInfo::name_offset;

	std::string device_name = {};
	for (size_t i = 0; i < DeviceDriverInfo::name_length; ++i) {
		const auto c = static_cast<char>(real_readb(segment, check_cast<uint16_t>(offset + i)));

		// Device name should be padded with spaces if it is less than name length (8 characters)
		// Also stop reading upon encountering a null termination or control codes to be safe
		if (c <= 0x20) {
			break;
		}

		device_name.push_back(c);
	}

	return device_name == req_name;
}

void DOS_SetupDevices() {
	DOS_Device * newdev;
	newdev=new device_CON();
	DOS_AddDevice(newdev);
	DOS_Device * newdev2;
	newdev2=new device_NUL();
	DOS_AddDevice(newdev2);
	DOS_Device * newdev3;
	newdev3=new device_LPT1();
	DOS_AddDevice(newdev3);
}

void DOS_ShutDownDevices()
{
	for (auto &d : Devices) {
		delete d;
		d = nullptr;
	}
}
