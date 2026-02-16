// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include <cmath>
#include <ctime>
#include <memory>

#include "hardware/memory.h"
#include "hardware/pic.h"
#include "hardware/port.h"
#include "hardware/timer.h"
#include "ints/bios_disk.h"
#include "misc/cross.h"

static struct {
	uint8_t regs[0x40];
	bool nmi;
	bool bcd;
	uint8_t reg;
	struct {
		bool enabled;
		uint8_t div;
		double delay;
		bool acknowledged;
	} timer;
	struct {
		double timer;
		double ended;
		double alarm;
	} last;
	bool update_ended;
} cmos;

static void cmos_timerevent(uint32_t /*val*/)
{
	if (cmos.timer.acknowledged) {
		cmos.timer.acknowledged = false;
		PIC_ActivateIRQ(8);
	}
	if (cmos.timer.enabled) {
		PIC_AddEvent(cmos_timerevent,cmos.timer.delay);
		cmos.regs[0xc] = 0xC0;//Contraption Zack (music)
	}
}

static void cmos_checktimer(void) {
	PIC_RemoveEvents(cmos_timerevent);
	if (cmos.timer.div<=2) cmos.timer.div+=7;
	cmos.timer.delay = (1000.0 / (32768.0 / (1 << (cmos.timer.div - 1))));
	if (!cmos.timer.div || !cmos.timer.enabled) return;
	LOG(LOG_PIT, LOG_NORMAL)("RTC Timer at %.2f hz", 1000.0 / static_cast<double>(cmos.timer.delay));
	//	PIC_AddEvent(cmos_timerevent,cmos.timer.delay);
	/* A rtc is always running */
	const auto remd = fmod(PIC_FullIndex(), cmos.timer.delay);
	// Should be more like a real pc. Check
	PIC_AddEvent(cmos_timerevent, cmos.timer.delay - remd);
	// Status reg A reading with this (and with other delays actually)
}

void cmos_selreg(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	cmos.reg = val & 0x3f;
	cmos.nmi = (val & 0x80) > 0;
}

static void cmos_writereg(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	switch (cmos.reg) {
	case 0x00:		/* Seconds */
	case 0x02:		/* Minutes */
	case 0x04:		/* Hours */
	case 0x06:		/* Day of week */
	case 0x07:		/* Date of month */
	case 0x08:		/* Month */
	case 0x09:		/* Year */
	case 0x32:              /* Century */
		/* Ignore writes to change alarm */
		break;
	case 0x01:		/* Seconds Alarm */
	case 0x03:		/* Minutes Alarm */
	case 0x05:		/* Hours Alarm */
		LOG(LOG_BIOS,LOG_NORMAL)("CMOS:Trying to set alarm");
		cmos.regs[cmos.reg]=val;
		break;
	case 0x0a:		/* Status reg A */
		cmos.regs[cmos.reg]=val & 0x7f;
		if ((val & 0x70)!=0x20) LOG(LOG_BIOS,LOG_ERROR)("CMOS Illegal 22 stage divider value");
		cmos.timer.div=(val & 0xf);
		cmos_checktimer();
		break;
	case 0x0b:		/* Status reg B */
		cmos.bcd=!(val & 0x4);
		cmos.regs[cmos.reg]=val & 0x7f;
		cmos.timer.enabled=(val & 0x40)>0;
		if (val&0x10) LOG(LOG_BIOS,LOG_ERROR)("CMOS:Updated ended interrupt not supported yet");
		cmos_checktimer();
		break;
	case 0x0d:/* Status reg D */
		cmos.regs[cmos.reg]=val & 0x80;	/*Bit 7=1:RTC Pown on*/
		break;
	case 0x0f:		/* Shutdown status byte */
		cmos.regs[cmos.reg]=val & 0x7f;
		break;
	default:
		cmos.regs[cmos.reg]=val & 0x7f;
		LOG(LOG_BIOS,LOG_ERROR)("CMOS:WRite to unhandled register %x",cmos.reg);
	}
}

#define MAKE_RETURN(_VAL) (cmos.bcd ? ((((_VAL) / 10) << 4) | ((_VAL) % 10)) : (_VAL));

static uint8_t cmos_readreg(io_port_t, io_width_t)
{
	if (cmos.reg > 0x3f) {
		LOG(LOG_BIOS, LOG_ERROR)("CMOS:Read from illegal register %x", cmos.reg);
		return 0xff;
	}
	Bitu drive_a, drive_b;
	uint8_t hdparm;

	const time_t curtime = time(nullptr);
	struct tm datetime;
	cross::localtime_r(&curtime, &datetime);

	switch (cmos.reg) {
	case 0x00:		/* Seconds */
		return MAKE_RETURN(datetime.tm_sec);
	case 0x02:		/* Minutes */
		return MAKE_RETURN(datetime.tm_min);
	case 0x04:		/* Hours */
		return MAKE_RETURN(datetime.tm_hour);
	case 0x06:		/* Day of week */
		return MAKE_RETURN(datetime.tm_wday + 1);
	case 0x07:		/* Date of month */
		return MAKE_RETURN(datetime.tm_mday);
	case 0x08:		/* Month */
		return MAKE_RETURN(datetime.tm_mon + 1);
	case 0x09:		/* Year */
		return MAKE_RETURN(datetime.tm_year % 100);
	case 0x32:		/* Century */
		return MAKE_RETURN(datetime.tm_year / 100 + 19);
	case 0x01:		/* Seconds Alarm */
	case 0x03:		/* Minutes Alarm */
	case 0x05:		/* Hours Alarm */
		return cmos.regs[cmos.reg];
	case 0x0a:		/* Status register A */
		if (PIC_TickIndex() < 0.002f) {
			return (cmos.regs[0x0a]&0x7f) | 0x80;
		} else {
			return (cmos.regs[0x0a]&0x7f);
		}
	case 0x0c:		/* Status register C */
		cmos.timer.acknowledged=true;
		if (cmos.timer.enabled) {
			/* In periodic interrupt mode only care for those flags */
			uint8_t val=cmos.regs[0xc];
			cmos.regs[0xc]=0;
			return val;
		} else {
			/* Give correct values at certain times */
			uint8_t val=0;
			const auto index = PIC_FullIndex();
			if (index>=(cmos.last.timer+cmos.timer.delay)) {
				cmos.last.timer=index;
				val|=0x40;
			} 
			if (index>=(cmos.last.ended+1000)) {
				cmos.last.ended=index;
				val|=0x10;
			} 
			return val;
		}
	case 0x10:		/* Floppy size */
		drive_a = 0;
		drive_b = 0;
		if (imageDiskList[0]) {
			drive_a = imageDiskList[0]->GetBiosType();
		}
		if (imageDiskList[1]) {
			drive_b = imageDiskList[1]->GetBiosType();
		}
		return ((drive_a << 4) | drive_b);
	/* First harddrive info */
	case 0x12:
		hdparm = 0;
		if (imageDiskList[2]) {
			hdparm |= 0xf;
		}
		if (imageDiskList[3]) {
			hdparm |= 0xf0;
		}
		return hdparm;
	case 0x19:
		if (imageDiskList[2]) {
			return 47; /* User defined type */
		}
		return 0;
	case 0x1b:
		if (imageDiskList[2]) {
			return (imageDiskList[2]->cylinders & 0xff);
		}
		return 0;
	case 0x1c:
		if (imageDiskList[2]) {
			return ((imageDiskList[2]->cylinders & 0xff00) >> 8);
		}
		return 0;
	case 0x1d:
		if (imageDiskList[2]) {
			return (imageDiskList[2]->heads);
		}
		return 0;
	case 0x1e:
		if (imageDiskList[2]) {
			return 0xff;
		}
		return 0;
	case 0x1f:
		if (imageDiskList[2]) {
			return 0xff;
		}
		return 0;
	case 0x20:
		if (imageDiskList[2]) {
			return (0xc0 | (((imageDiskList[2]->heads) > 8) << 3));
		}
		return 0;
	case 0x21:
		if (imageDiskList[2]) {
			return (imageDiskList[2]->cylinders & 0xff);
		}
		return 0;
	case 0x22:
		if (imageDiskList[2]) {
			return ((imageDiskList[2]->cylinders & 0xff00) >> 8);
		}
		return 0;
	case 0x23:
		if (imageDiskList[2]) {
			return (imageDiskList[2]->sectors);
		}
		return 0;
	/* Second harddrive info */
	case 0x1a:
		if (imageDiskList[3]) {
			return 47; /* User defined type */
		}
		return 0;
	case 0x24:
		if (imageDiskList[3]) {
			return (imageDiskList[3]->cylinders & 0xff);
		}
		return 0;
	case 0x25:
		if (imageDiskList[3]) {
			return ((imageDiskList[3]->cylinders & 0xff00) >> 8);
		}
		return 0;
	case 0x26:
		if (imageDiskList[3]) {
			return (imageDiskList[3]->heads);
		}
		return 0;
	case 0x27:
		if (imageDiskList[3]) {
			return 0xff;
		}
		return 0;
	case 0x28:
		if (imageDiskList[3]) {
			return 0xff;
		}
		return 0;
	case 0x29:
		if (imageDiskList[3]) {
			return (0xc0 | (((imageDiskList[3]->heads) > 8) << 3));
		}
		return 0;
	case 0x2a:
		if (imageDiskList[3]) {
			return (imageDiskList[3]->cylinders & 0xff);
		}
		return 0;
	case 0x2b:
		if (imageDiskList[3]) {
			return ((imageDiskList[3]->cylinders & 0xff00) >> 8);
		}
		return 0;
	case 0x2c:
		if (imageDiskList[3]) {
			return (imageDiskList[3]->sectors);
		}
		return 0;
	case 0x39:
		return 0;
	case 0x3a:
		return 0;

	case 0x0b:		/* Status register B */
	case 0x0d:		/* Status register D */
	case 0x0f:		/* Shutdown status byte */
	case 0x14:		/* Equipment */
	case 0x15:		/* Base Memory KB Low Byte */
	case 0x16:		/* Base Memory KB High Byte */
	case 0x17:		/* Extended memory in KB Low Byte */
	case 0x18:		/* Extended memory in KB High Byte */
	case 0x30:		/* Extended memory in KB Low Byte */
	case 0x31:		/* Extended memory in KB High Byte */
//		LOG(LOG_BIOS,LOG_NORMAL)("CMOS:Read from reg %X : %04X",cmos.reg,cmos.regs[cmos.reg]);
		return cmos.regs[cmos.reg];
	default:
		LOG(LOG_BIOS,LOG_NORMAL)("CMOS:Read from reg %X",cmos.reg);
		return cmos.regs[cmos.reg];
	}
}

void CMOS_SetRegister(Bitu regNr, uint8_t val) {
	cmos.regs[regNr] = val;
}

class CMOS {
private:
	IO_ReadHandleObject ReadHandler[2];
	IO_WriteHandleObject WriteHandler[2];
public:
	CMOS()
	{
		constexpr io_port_t port_0x70 = 0x70;
		constexpr io_port_t port_0x71 = 0x71;

		WriteHandler[0].Install(port_0x70, cmos_selreg, io_width_t::byte);
		WriteHandler[1].Install(port_0x71, cmos_writereg, io_width_t::byte);
		ReadHandler[0].Install(port_0x71, cmos_readreg, io_width_t::byte);
		cmos.timer.enabled = false;
		cmos.timer.acknowledged = true;
		cmos.reg = 0xa;
		cmos_writereg(port_0x71, 0x26, io_width_t::byte);
		cmos.reg = 0xb;
		cmos_writereg(port_0x71, 0x2, io_width_t::byte); // Struct tm
		                                                 // *loctime is
		                                                 // of 24 hour
		                                                 // format,
		cmos.reg = 0xd;
		cmos_writereg(port_0x71, 0x80, io_width_t::byte); /* RTC power
		                                                     on */
		// Equipment is updated from bios.cpp and bios_disk.cpp
		/* Fill in base memory size, it is 640K always */
		cmos.regs[0x15]=(uint8_t)0x80;
		cmos.regs[0x16]=(uint8_t)0x02;
		/* Fill in extended memory size */
		Bitu exsize=(MEM_TotalPages()*4)-1024;
		cmos.regs[0x17]=(uint8_t)exsize;
		cmos.regs[0x18]=(uint8_t)(exsize >> 8);
		cmos.regs[0x30]=(uint8_t)exsize;
		cmos.regs[0x31]=(uint8_t)(exsize >> 8);
	}
};

static std::unique_ptr<CMOS> cmos_module = {};

void CMOS_Init()
{
	cmos_module = std::make_unique<CMOS>();
}

void CMOS_Destroy()
{
	cmos_module = {};
}
