/*
 *  Copyright (C) 2002-2003  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dosbox.h"
#include "timer.h"
#include "pic.h"
#include "inout.h"
#include "config.h"

static struct {

	Bit8u regs[0x40];
	bool nmi;
	Bit8u reg;
	struct {
		bool enabled;
		Bit8u div;
		Bitu micro;
	} timer;
	Bit8u status_c;
	bool ack;
	bool update_ended;

} cmos;

static void cmos_timerevent(void) {
	PIC_ActivateIRQ(8); 
	if (cmos.ack) {
		PIC_AddEvent(cmos_timerevent,cmos.timer.micro);
		cmos.status_c=0x20;
		cmos.ack=false;
	}
}

static void cmos_checktimer(void) {
	PIC_RemoveEvents(cmos_timerevent);	
	if (!cmos.timer.div && !cmos.timer.enabled) return;
	if (cmos.timer.div<=2) cmos.timer.div+=7;
	cmos.timer.micro=(Bitu) (10000000.0/(32768.0 / (1 << (cmos.timer.div - 1))));
	LOG(LOG_PIT,"RTC Timer at %f hz",1000000.0/cmos.timer.micro);
	PIC_AddEvent(cmos_timerevent,cmos.timer.micro);
}

void cmos_selreg(Bit32u port,Bit8u val) {
	cmos.reg=val & 0x3f;
	cmos.nmi=(val & 0x80)>0;
}

static void cmos_writereg(Bit32u port,Bit8u val) {
	switch (cmos.reg) {
	case 0x00:		/* Seconds */
	case 0x01:		/* Seconds Alarm */
	case 0x02:		/* Minutes */
	case 0x03:		/* Minutes Alarm */
	case 0x04:		/* Hours */
	case 0x05:		/* Hours Alarm */
	case 0x06:		/* Day of week */
	case 0x07:		/* Date of month */
	case 0x08:		/* Month */
	case 0x09:		/* Year */
		cmos.regs[cmos.reg]=val;
		break;
	case 0x0a:		/* Status reg A */
		cmos.regs[cmos.reg]=val & 0x7f;
		if (val & 0x70!=0x20) LOG(LOG_ERROR|LOG_BIOS,"CMOS Illegal 22 stage divider value");
		cmos.timer.div=(val & 0xf);
		cmos_checktimer();
		break;
	case 0x0b:		/* Status reg B */
		cmos.regs[cmos.reg]=val & 0x7f;
		cmos.timer.enabled=(val & 0x40)>0;
		if (val&0x10) LOG(LOG_ERROR|LOG_BIOS,"CMOS:Updated ended interrupt not supported yet");
		cmos_checktimer();
		break;
	default:
		cmos.regs[cmos.reg]=val & 0x7f;
		LOG(LOG_ERROR|LOG_BIOS,"CMOS:Unhandled register %x",cmos.reg);
	}
}

static Bit8u cmos_readreg(Bit32u port) {
	if (cmos.reg>0x3f) {
		LOG(LOG_ERROR|LOG_BIOS,"CMOS:Read from illegal register %x",cmos.reg);
		return 0xff;
	}
	switch (cmos.reg) {
	case 0x0c:
		if (cmos.ack) return 0;
		else {
			cmos.ack=true;
			return 0x80|cmos.status_c;
		}
	default:
		return cmos.regs[cmos.reg];
	}
}

void CMOS_Init(Section* sec) {
	IO_RegisterWriteHandler(0x70,cmos_selreg,"CMOS");
	IO_RegisterWriteHandler(0x71,cmos_writereg,"CMOS");
	IO_RegisterReadHandler(0x71,cmos_readreg,"CMOS");
	cmos.timer.enabled=false;
}

