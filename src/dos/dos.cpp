/*
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
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

#include "dos_inc.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <array>

#include "ascii.h"
#include "bios.h"
#include "callback.h"
#include "dos_locale.h"
#include "drives.h"
#include "mem.h"
#include "program_mount_common.h"
#include "regs.h"
#include "serialport.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"

#if defined(WIN32)
#include <winsock2.h> // for gethostname
#endif

DOS_Block dos;
DOS_InfoBlock dos_infoblock;
uint16_t countryNo = 0;
unsigned int result_errorcode = 0;

static bool is_guest_booted = false;

extern void DOS_ClearLaunchedProgramNames();

void DOS_NotifyBooting()
{
	is_guest_booted = true;
	DOS_ClearLaunchedProgramNames();
}

bool DOS_IsGuestOsBooted()
{
	return is_guest_booted;
}

#define DOS_COPYBUFSIZE 0x10000
uint8_t dos_copybuf[DOS_COPYBUFSIZE];

void DOS_SetError(uint16_t code) {
	dos.errorcode=code;
}

uint16_t DOS_GetBiosTimePacked()
{
	const auto ticks   = mem_readd(BIOS_TIMER);
	const auto seconds = (ticks * 10) / 182;
	const auto hour    = static_cast<uint16_t>(seconds / 3600);
	const auto min     = static_cast<uint16_t>((seconds % 3600) / 60);
	const auto sec     = static_cast<uint16_t>(seconds % 60);

	return DOS_PackTime(hour, min, sec);
}

uint16_t DOS_GetBiosDatePacked()
{
	return DOS_PackDate(dos.date.year, dos.date.month, dos.date.day);
}

static void DOS_AddDays(Bitu days)
{
	dos.date.day += days;
	uint8_t monthlimit = DOS_DATE_months[dos.date.month];

	if(dos.date.day > monthlimit) {
		if((dos.date.year %4 == 0) && (dos.date.month==2)) {
			// leap year
			if(dos.date.day > 29) {
				dos.date.month++;
				dos.date.day -= 29;
			}
		} else {
			//not leap year
			dos.date.month++;
			dos.date.day -= monthlimit;
		}
		if(dos.date.month > 12) {
			// year over
			dos.date.month = 1;
			dos.date.year++;
		}
	}
}

static uint16_t DOS_GetAmount(void) {
	uint16_t amount = reg_cx;
	if (amount > 0xfff1) {
		uint16_t overflow = (amount & 0xf) + (reg_dx & 0xf);
		if (overflow > 0x10) {
			amount -= (overflow & 0xf);
			LOG(LOG_DOSMISC,LOG_WARN)("DOS:0x%X:Amount reduced from %X to %X",reg_ah,reg_cx,amount);
		}
	}
	return amount;
}

#define DATA_TRANSFERS_TAKE_CYCLES 1
#ifdef DATA_TRANSFERS_TAKE_CYCLES

#ifndef DOSBOX_CPU_H
#include "cpu.h"
#endif
static inline void modify_cycles(Bits value) {
	if((4*value+5) < CPU_Cycles) {
		CPU_Cycles -= 4*value;
		CPU_IODelayRemoved += 4*value;
	} else {
		CPU_IODelayRemoved += CPU_Cycles/*-5*/; //don't want to mess with negative
		CPU_Cycles = 5;
	}
}
#else
static inline void modify_cycles(Bits /* value */) {
	return;
}
#endif
#define DOS_OVERHEAD 1
#ifdef DOS_OVERHEAD
#ifndef DOSBOX_CPU_H
#include "cpu.h"
#endif

static inline void overhead() {
	reg_ip += 2;
}
#else
static inline void overhead() {
	return;
}
#endif

#define DOSNAMEBUF 256
static Bitu DOS_21Handler(void) {
	if (((reg_ah != 0x50) && (reg_ah != 0x51) && (reg_ah != 0x62) && (reg_ah != 0x64)) && (reg_ah<0x6c)) {
		DOS_PSP psp(dos.psp());
		psp.SetStack(RealMake(SegValue(ss),reg_sp-18));
		/* Save registers */
		real_writew(SegValue(ss),reg_sp-18,reg_ax);
		real_writew(SegValue(ss),reg_sp-16,reg_bx);
		real_writew(SegValue(ss),reg_sp-14,reg_cx);
		real_writew(SegValue(ss),reg_sp-12,reg_dx);
		real_writew(SegValue(ss),reg_sp-10,reg_si);
		real_writew(SegValue(ss),reg_sp- 8,reg_di);
		real_writew(SegValue(ss),reg_sp- 6,reg_bp);
		real_writew(SegValue(ss),reg_sp- 4,SegValue(ds));
		real_writew(SegValue(ss),reg_sp- 2,SegValue(es));
	}

	char name1[DOSNAMEBUF+2+DOS_NAMELENGTH_ASCII];
	char name2[DOSNAMEBUF+2+DOS_NAMELENGTH_ASCII];
	
	static Bitu time_start = 0; //For emulating temporary time changes.

	switch (reg_ah) {
	case 0x00:		/* Terminate Program */
		DOS_Terminate(real_readw(SegValue(ss),reg_sp+2),false,0);
		break;
	case 0x01:		/* Read character from STDIN, with echo */
		{	
			uint8_t c;uint16_t n=1;
			dos.echo=true;
			DOS_ReadFile(STDIN,&c,&n);
			reg_al=c;
			dos.echo=false;
		}
		break;
	case 0x02:		/* Write character to STDOUT */
		{
			uint8_t c=reg_dl;uint16_t n=1;
			DOS_WriteFile(STDOUT,&c,&n);
			//Not in the official specs, but happens nonetheless. (last written character)
			reg_al=(c==9)?0x20:c; //strangely, tab conversion to spaces is reflected here
		}
		break;
	case 0x03:		/* Read character from STDAUX */
		{
			uint16_t port = real_readw(0x40,0);
			if(port!=0 && serialports[0]) {
				uint8_t status;
				// RTS/DTR on
				IO_WriteB(port+4,0x3);
				serialports[0]->Getchar(&reg_al, &status, true, 0xFFFFFFFF);
			}
		}
		break;
	case 0x04:		/* Write Character to STDAUX */
		{
			uint16_t port = real_readw(0x40,0);
			if(port!=0 && serialports[0]) {
				// RTS/DTR on
				IO_WriteB(port+4,0x3);
				serialports[0]->Putchar(reg_dl,true,true, 0xFFFFFFFF);
				// RTS off
				IO_WriteB(port+4,0x1);
			}
		}
		break;
	case 0x05:		/* Write Character to PRINTER */
		E_Exit("DOS:Unhandled call %02X",reg_ah);
		break;
	case 0x06:		/* Direct Console Output / Input */
		switch (reg_dl) {
		case 0xFF:	/* Input */
			{	
				//Simulate DOS overhead for timing sensitive games
				//MM1
				overhead();
				//TODO Make this better according to standards
				if (!DOS_GetSTDINStatus()) {
					reg_al=0;
					CALLBACK_SZF(true);
					break;
				}
				uint8_t c;uint16_t n=1;
				DOS_ReadFile(STDIN,&c,&n);
				reg_al=c;
				CALLBACK_SZF(false);
				break;
			}
		default:
			{
				uint8_t c = reg_dl;uint16_t n = 1;
				dos.direct_output=true;
				DOS_WriteFile(STDOUT,&c,&n);
				dos.direct_output=false;
				reg_al=c;
			}
			break;
		};
		break;
	case 0x07:              /* Character Input, without echo */
	case 0x08:		/* Direct Character Input, without echo (checks for breaks officially :)*/
		{
				uint8_t c;uint16_t n=1;
				DOS_ReadFile (STDIN,&c,&n);
				reg_al=c;
				break;
		};
	case 0x09:		/* Write string to STDOUT */
		{	
			uint8_t c;uint16_t n=1;
			PhysPt buf=SegPhys(ds)+reg_dx;
			while ((c=mem_readb(buf++))!='$') {
				DOS_WriteFile(STDOUT,&c,&n);
			}
			reg_al=c;
		}
		break;
	case 0x0a:		/* Buffered Input */
		{
			//TODO ADD Break checkin in STDIN but can't care that much for it
			PhysPt data=SegPhys(ds)+reg_dx;
			uint8_t free=mem_readb(data);
			uint8_t read=0;uint8_t c;uint16_t n=1;
			if (!free) break;
			free--;
			for(;;) {
				DOS_ReadFile(STDIN,&c,&n);
				// gracefully exit potentially endless loop
				if (shutdown_requested) {
					break;
				}
				if (n == 0)				// End of file
					E_Exit("DOS:0x0a:Redirected input reached EOF");
				if (c == 10)			// Line feed
					continue;
				if (c == 8) {			// Backspace
					if (read) {	//Something to backspace.
						// STDOUT treats backspace as non-destructive.
						         DOS_WriteFile(STDOUT,&c,&n);
						c = ' '; DOS_WriteFile(STDOUT,&c,&n);
						c = 8;   DOS_WriteFile(STDOUT,&c,&n);
						--read;
					}
					continue;
				}
				if (read == free && c != 13) {		// Keyboard buffer full
					uint8_t bell = 7;
					DOS_WriteFile(STDOUT, &bell, &n);
					continue;
				}
				DOS_WriteFile(STDOUT,&c,&n);
				mem_writeb(data+read+2,c);
				if (c==13) 
					break;
				read++;
			};
			mem_writeb(data+1,read);
			break;
		};
	case 0x0b:		/* Get STDIN Status */
		if (!DOS_GetSTDINStatus()) {reg_al=0x00;}
		else {reg_al=0xFF;}
		//Simulate some overhead for timing issues
		//Tankwar menu (needs maybe even more)
		overhead();
		break;
	case 0x0c:		/* Flush Buffer and read STDIN call */
		{
			/* flush buffer if STDIN is CON */
			uint8_t handle=RealHandle(STDIN);
			if (handle!=0xFF && Files[handle] && Files[handle]->IsName("CON")) {
				uint8_t c;uint16_t n;
				while (DOS_GetSTDINStatus()) {
					n=1;
					DOS_ReadFile(STDIN,&c,&n);

					// gracefully exit potentially endless loop
					if (shutdown_requested) {
						break;
					}
				}
			}
			switch (reg_al) {
			case 0x1:
			case 0x6:
			case 0x7:
			case 0x8:
			case 0xa:
				{ 
					uint8_t oldah=reg_ah;
					reg_ah=reg_al;
					DOS_21Handler();
					reg_ah=oldah;
				}
				break;
			default:
//				LOG_ERROR("DOS:0C:Illegal Flush STDIN Buffer call %d",reg_al);
				reg_al=0;
				break;
			}
		}
		break;
//TODO Find out the values for when reg_al!=0
//TODO Hope this doesn't do anything special
	case 0x0d:		/* Disk Reset */
//Sure let's reset a virtual disk
		break;	
	case 0x0e:		/* Select Default Drive */
		DOS_SetDefaultDrive(reg_dl);
		reg_al=DOS_DRIVES;
		break;
	case 0x0f:		/* Open File using FCB */
		if(DOS_FCBOpen(SegValue(ds),reg_dx)){
			reg_al=0;
		}else{
			reg_al=0xff;
		}
		LOG(LOG_FCB,LOG_NORMAL)("DOS:0x0f FCB-fileopen used, result:al=%d",reg_al);
		break;
	case 0x10:		/* Close File using FCB */
		if(DOS_FCBClose(SegValue(ds),reg_dx)){
			reg_al=0;
		}else{
			reg_al=0xff;
		}
		LOG(LOG_FCB,LOG_NORMAL)("DOS:0x10 FCB-fileclose used, result:al=%d",reg_al);
		break;
	case 0x11:		/* Find First Matching File using FCB */
		if(DOS_FCBFindFirst(SegValue(ds),reg_dx)) reg_al = 0x00;
		else reg_al = 0xFF;
		LOG(LOG_FCB,LOG_NORMAL)("DOS:0x11 FCB-FindFirst used, result:al=%d",reg_al);
		break;
	case 0x12:		/* Find Next Matching File using FCB */
		if(DOS_FCBFindNext(SegValue(ds),reg_dx)) reg_al = 0x00;
		else reg_al = 0xFF;
		LOG(LOG_FCB,LOG_NORMAL)("DOS:0x12 FCB-FindNext used, result:al=%d",reg_al);
		break;
	case 0x13:		/* Delete File using FCB */
		if (DOS_FCBDeleteFile(SegValue(ds),reg_dx)) reg_al = 0x00;
		else reg_al = 0xFF;
		LOG(LOG_FCB,LOG_NORMAL)("DOS:0x16 FCB-Delete used, result:al=%d",reg_al);
		break;
	case 0x14:		/* Sequential read from FCB */
		reg_al = DOS_FCBRead(SegValue(ds),reg_dx,0);
		LOG(LOG_FCB,LOG_NORMAL)("DOS:0x14 FCB-Read used, result:al=%d",reg_al);
		break;
	case 0x15:		/* Sequential write to FCB */
		reg_al=DOS_FCBWrite(SegValue(ds),reg_dx,0);
		LOG(LOG_FCB,LOG_NORMAL)("DOS:0x15 FCB-Write used, result:al=%d",reg_al);
		break;
	case 0x16:		/* Create or truncate file using FCB */
		if (DOS_FCBCreate(SegValue(ds),reg_dx)) reg_al = 0x00;
		else reg_al = 0xFF;
		LOG(LOG_FCB,LOG_NORMAL)("DOS:0x16 FCB-Create used, result:al=%d",reg_al);
		break;
	case 0x17:		/* Rename file using FCB */		
		if (DOS_FCBRenameFile(SegValue(ds),reg_dx)) reg_al = 0x00;
		else reg_al = 0xFF;
		break;
	case 0x1b:		/* Get allocation info for default drive */	
		if (!DOS_GetAllocationInfo(0,&reg_cx,&reg_al,&reg_dx)) reg_al=0xff;
		break;
	case 0x1c:		/* Get allocation info for specific drive */
		if (!DOS_GetAllocationInfo(reg_dl,&reg_cx,&reg_al,&reg_dx)) reg_al=0xff;
		break;
	case 0x21:		/* Read random record from FCB */
		{
			uint16_t toread=1;
			reg_al = DOS_FCBRandomRead(SegValue(ds),reg_dx,&toread,true);
		}
		LOG(LOG_FCB,LOG_NORMAL)("DOS:0x21 FCB-Random read used, result:al=%d",reg_al);
		break;
	case 0x22:		/* Write random record to FCB */
		{
			uint16_t towrite=1;
			reg_al=DOS_FCBRandomWrite(SegValue(ds),reg_dx,&towrite,true);
		}
		LOG(LOG_FCB,LOG_NORMAL)("DOS:0x22 FCB-Random write used, result:al=%d",reg_al);
		break;
	case 0x23:		/* Get file size for FCB */
		if (DOS_FCBGetFileSize(SegValue(ds),reg_dx)) reg_al = 0x00;
		else reg_al = 0xFF;
		break;
	case 0x24:		/* Set Random Record number for FCB */
		DOS_FCBSetRandomRecord(SegValue(ds),reg_dx);
		break;
	case 0x27:		/* Random block read from FCB */
		reg_al = DOS_FCBRandomRead(SegValue(ds),reg_dx,&reg_cx,false);
		LOG(LOG_FCB,LOG_NORMAL)("DOS:0x27 FCB-Random(block) read used, result:al=%d",reg_al);
		break;
	case 0x28:		/* Random Block write to FCB */
		reg_al=DOS_FCBRandomWrite(SegValue(ds),reg_dx,&reg_cx,false);
		LOG(LOG_FCB,LOG_NORMAL)("DOS:0x28 FCB-Random(block) write used, result:al=%d",reg_al);
		break;
	case 0x29:		/* Parse filename into FCB */
		{   
			uint8_t difference;
			char string[1024];
			MEM_StrCopy(SegPhys(ds)+reg_si,string,1023); // 1024 toasts the stack
			reg_al=FCB_Parsename(SegValue(es),reg_di,reg_al ,string, &difference);
			reg_si+=difference;
		}
		LOG(LOG_FCB,LOG_NORMAL)("DOS:29:FCB Parse Filename, result:al=%d",reg_al);
		break;
	case 0x19:		/* Get current default drive */
		reg_al=DOS_GetDefaultDrive();
		break;
	case 0x1a:		/* Set Disk Transfer Area Address */
		dos.dta(RealMakeSeg(ds,reg_dx));
		break;
	case 0x25:		/* Set Interrupt Vector */
		RealSetVec(reg_al,RealMakeSeg(ds,reg_dx));
		break;
	case 0x26:		/* Create new PSP */
		DOS_NewPSP(reg_dx,DOS_PSP(dos.psp()).GetSize());
		reg_al=0xf0;	/* al destroyed */		
		break;
	case 0x2a:		/* Get System Date */
		{
			reg_ax=0; // get time
			CALLBACK_RunRealInt(0x1a);
			if(reg_al) DOS_AddDays(reg_al);
			int a = (14 - dos.date.month)/12;
			int y = dos.date.year - a;
			int m = dos.date.month + 12*a - 2;
			reg_al=(dos.date.day+y+(y/4)-(y/100)+(y/400)+(31*m)/12) % 7;
			reg_cx=dos.date.year;
			reg_dh=dos.date.month;
			reg_dl=dos.date.day;
		}
		break;
	case 0x2b:		/* Set System Date */
		if (!is_date_valid(reg_cx, reg_dh, reg_dl)) {
			reg_al = 0xff;
			break;
		}
		dos.date.year=reg_cx;
		dos.date.month=reg_dh;
		dos.date.day=reg_dl;
		reg_al=0;
		break;
	case 0x2c: {	/* Get System Time */
		reg_ax=0; // get time
		CALLBACK_RunRealInt(0x1a);
		if(reg_al) DOS_AddDays(reg_al);
		reg_ah=0x2c;

		Bitu ticks=((Bitu)reg_cx<<16)|reg_dx;
		if(time_start<=ticks) ticks-=time_start;
		Bitu time=(Bitu)((100.0/((double)PIT_TICK_RATE/65536.0)) * (double)ticks);

		reg_dl=(uint8_t)((Bitu)time % 100); // 1/100 seconds
		time/=100;
		reg_dh=(uint8_t)((Bitu)time % 60); // seconds
		time/=60;
		reg_cl=(uint8_t)((Bitu)time % 60); // minutes
		time/=60;
		reg_ch=(uint8_t)((Bitu)time % 24); // hours

		//Simulate DOS overhead for timing-sensitive games
        //Robomaze 2
		overhead();
		break;
	}
	case 0x2d:		/* Set System Time */
		if (!is_time_valid(reg_ch, reg_cl, reg_dh) || reg_dl > 99)
			reg_al = 0xff;
		else { //Allow time to be set to zero. Restore the orginal time for all other parameters. (QuickBasic)
			if (reg_cx == 0 && reg_dx == 0) {time_start = mem_readd(BIOS_TIMER);LOG_MSG("Warning: game messes with DOS time!");}
			else time_start = 0;
			// Original IBM PC used ~1.19MHz crystal for timer,
			// because at 1.19MHz, 2^16 ticks is ~1 hour, making it
			// easy to count hours and days. More precisely:
			//
			// clock updates at 1193180/65536 ticks per second.
			// ticks per second ??? 18.2
			// ticks per hour   ??? 65543
			// ticks per day    ??? 1573040
			constexpr uint64_t ticks_per_day = 1573040;
			const auto seconds = reg_ch * 3600 + reg_cl * 60 + reg_dh;
			const auto ticks = ticks_per_day * seconds / (24 * 3600);
			mem_writed(BIOS_TIMER, check_cast<uint32_t>(ticks));
			reg_al = 0;
		}
		break;
	case 0x2e:		/* Set Verify flag */
		dos.verify=(reg_al==1);
		break;
	case 0x2f:		/* Get Disk Transfer Area */
		SegSet16(es,RealSegment(dos.dta()));
		reg_bx=RealOffset(dos.dta());
		break;
	case 0x30: /* Get DOS Version */
		{
			if (reg_al == 0) {
				reg_bh = 0xff; // 0xff for MS-DOS, 0x00 for PC-DOS
			}
			if (reg_al == 1) {
				reg_bh = 0x10; // DOS is in HMA
			}
			DOS_PSP psp(dos.psp());
			// Prior to MS-DOS 5.0 version number was hardcoded,
			// later DOS releases used values from PSP, for SETVER
			reg_al = psp.GetVersionMajor();
			reg_ah = psp.GetVersionMinor();
			// Serial number
			reg_bl = 0x00;
			reg_cx = 0x0000;
		}
		break;
	case 0x31: /* Terminate and stay resident */
		// Important: This service does not set the carry flag!
		DOS_ResizeMemory(dos.psp(),&reg_dx);
		DOS_Terminate(dos.psp(),true,reg_al);
		break;
	case 0x1f: /* Get drive parameter block for default drive */
	case 0x32: /* Get drive parameter block for specific drive */
		{	/* Officially a dpb should be returned as well. The disk detection part is implemented */
			uint8_t drive=reg_dl;
			if (!drive || reg_ah==0x1f) drive = DOS_GetDefaultDrive();
			else drive--;
			if (drive < DOS_DRIVES && Drives[drive] && !Drives[drive]->IsRemovable()) {
				reg_al = 0x00;
				SegSet16(ds,dos.tables.dpb);
				reg_bx = drive*9;
				LOG(LOG_DOSMISC,LOG_ERROR)("Get drive parameter block.");
			} else {
				reg_al=0xff;
			}
		}
		break;
	case 0x33: /* Extended Break Checking */
		switch (reg_al) {
			case 0: reg_dl = dos.breakcheck;       break; /* Get the breakcheck flag */
			case 1: dos.breakcheck = (reg_dl > 0); break; /* Set the breakcheck flag */
			case 2:
				{
					const bool old = dos.breakcheck;
					dos.breakcheck = (reg_dl > 0);
					reg_dl = old;
				}
				break;
			case 3: /* Get cpsw - fallthrough*/
			case 4: /* Set cpsw */
				LOG(LOG_DOSMISC,LOG_ERROR)("Someone playing with cpsw %x",reg_ax);
				break;
			case 5: reg_dl = 3; break; // TODO should be z  /* Always boot from c: :) */
		        case 6: // Get true version number, not affected by SETVER
				{
					DOS_PSP psp(dos.psp());
					reg_bl = dos.version.major;
					reg_bh = dos.version.minor;
					reg_dl = dos.version.revision;
					reg_dh = 0x10; // DOS in HMA
				}
				break;
		        default:
				LOG(LOG_DOSMISC, LOG_ERROR)("Weird 0x33 call %2X", reg_al);
				reg_al = 0xff;
				break;
		}
		break;
	case 0x34: /* Get INDos Flag */
		SegSet16(es, DOS_SDA_SEG);
		reg_bx = DOS_SDA_OFS + 0x01;
		break;
	case 0x35: /* Get interrupt vector */
		reg_bx = real_readw(0, ((uint16_t)reg_al) * 4);
		SegSet16(es, real_readw(0, ((uint16_t)reg_al) * 4 + 2));
		break;
	case 0x36: /* Get Free Disk Space */
		{
			uint16_t bytes    = 0;
			uint16_t clusters = 0;
			uint16_t free     = 0;
			uint8_t  sectors  = 0;
			if (DOS_GetFreeDiskSpace(reg_dl, &bytes, &sectors,
			                         &clusters, &free)) {
				reg_ax = sectors;
				reg_bx = free;
				reg_cx = bytes;
				reg_dx = clusters;
			} else {
				[[maybe_unused]] uint8_t drive = reg_dl;
				if (drive == 0) {
					drive = DOS_GetDefaultDrive();
				} else {
					--drive;
				}
				/*
				if (drive < 2) {
					// floppy drive, non-present drivesdisks issue floppy check through int24
					// (critical error handler); needed for Mixed up Mother Goose (hook)
					CALLBACK_RunRealInt(0x24);
				} */
				reg_ax = 0xffff; // invalid drive specified
			}
		}
		break;
	case 0x37:		/* Get/Set Switch char Get/Set Availdev thing */
//TODO	Give errors for these functions to see if anyone actually uses this shit-
		switch (reg_al) {
		case 0:
			 reg_al=0;reg_dl=0x2f;break;  /* always return '/' like dos 5.0+ */
		case 1:
			 reg_al=0;break;
		case 2:
			 reg_al=0;reg_dl=0x2f;break;
		case 3:
			 reg_al=0;break;
		};
		LOG(LOG_MISC,LOG_ERROR)("DOS:0x37:Call for not supported switchchar");
		break;
	case 0x38:
		if (reg_dx == 0xffff) { /* Set Country Code */
			// TODO: For unknown reason on modern DOSes (checked
			// MS-DOS 6.22, PC DOS 2000 and DR DOS 7.03) this only
			// works when setting the country to the one currently
			// set - unknown, why
			countryNo = (reg_al == 0xff) ? reg_bx : reg_al;
			if (DOS_SetCountry(countryNo)) {
				reg_ax = 0;
				CALLBACK_SCF(false);
			} else {
				reg_ax = 0x02; // invalid country
				CALLBACK_SCF(true);
			}
		} else { /* Get country specific information */
			PhysPt dest = SegPhys(ds) + reg_dx;
			MEM_BlockWrite(dest, dos.tables.country, 0x18);
			reg_bx = DOS_GetCountry();
			reg_ah = 0;
			reg_al = reg_bl;
			CALLBACK_SCF(false);
		}
		break;
	case 0x39:		/* MKDIR Create directory */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if (DOS_MakeDir(name1)) {
			reg_ax=0x05;	/* ax destroyed */
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x3a:		/* RMDIR Remove directory */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if  (DOS_RemoveDir(name1)) {
			reg_ax=0x05;	/* ax destroyed */
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
			LOG(LOG_MISC,LOG_NORMAL)("Remove dir failed on %s with error %X",name1,dos.errorcode);
		}
		break;
	case 0x3b:		/* CHDIR Set current directory */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if  (DOS_ChangeDir(name1)) {
			reg_ax=0x00;	/* ax destroyed */
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x3c:		/* CREATE Create of truncate file */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if (DOS_CreateFile(name1, reg_cl, &reg_ax)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x3d:		/* OPEN Open existing file */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if (DOS_OpenFile(name1,reg_al,&reg_ax)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x3e:		/* CLOSE Close file */
		if (DOS_CloseFile(reg_bx,false,&reg_al)) {
			/* al destroyed with pre-close refcount from sft */
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x3f:		/* READ Read from file or device */
		{ 
			uint16_t toread=DOS_GetAmount();
			dos.echo=true;
			if (DOS_ReadFile(reg_bx,dos_copybuf,&toread)) {
				MEM_BlockWrite(SegPhys(ds)+reg_dx,dos_copybuf,toread);
				reg_ax=toread;
				CALLBACK_SCF(false);
			} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
			}
			modify_cycles(reg_ax);
			dos.echo=false;
			break;
		}
	case 0x40:					/* WRITE Write to file or device */
		{
			uint16_t towrite=DOS_GetAmount();
			MEM_BlockRead(SegPhys(ds)+reg_dx,dos_copybuf,towrite);
			if (DOS_WriteFile(reg_bx,dos_copybuf,&towrite)) {
				reg_ax=towrite;
	   			CALLBACK_SCF(false);
			} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
			}
			modify_cycles(reg_ax);
			break;
		};
	case 0x41:					/* UNLINK Delete file */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if (DOS_UnlinkFile(name1)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x42:					/* LSEEK Set current file position */
		{
			uint32_t pos=(reg_cx<<16) + reg_dx;
			if (DOS_SeekFile(reg_bx,&pos,reg_al)) {
				reg_dx=(uint16_t)(pos >> 16);
				reg_ax=(uint16_t)(pos & 0xFFFF);
				CALLBACK_SCF(false);
			} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
			}
			break;
		}
	case 0x43:					/* Get/Set file attributes */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		switch (reg_al) {
		case 0x00:				/* Get */
			{
			        FatAttributeFlags attr_val = reg_cl;
			        if (DOS_GetFileAttr(name1, &attr_val)) {
				        reg_cx = attr_val._data;
				        reg_ax = attr_val._data; // Undocumented
				        CALLBACK_SCF(false);
			        } else {
				        CALLBACK_SCF(true);
					reg_ax=dos.errorcode;
			        }
			        break;
			};
		case 0x01:				/* Set */
			if (DOS_SetFileAttr(name1, reg_cl)) {
				reg_ax=0x202;	/* ax destroyed */
				CALLBACK_SCF(false);
			} else {
				CALLBACK_SCF(true);
				reg_ax=dos.errorcode;
			}
			break;
		default:
			LOG(LOG_MISC,LOG_ERROR)("DOS:0x43:Illegal subfunction %2X",reg_al);
			reg_ax=1;
			CALLBACK_SCF(true);
			break;
		}
		break;
	case 0x44:					/* IOCTL Functions */
		if (DOS_IOCTL()) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x45:					/* DUP Duplicate file handle */
		if (DOS_DuplicateEntry(reg_bx,&reg_ax)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x46:					/* DUP2,FORCEDUP Force duplicate file handle */
		if (DOS_ForceDuplicateEntry(reg_bx,reg_cx)) {
			reg_ax=reg_cx; //Not all sources agree on it.
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x47:					/* CWD Get current directory */
		if (DOS_GetCurrentDir(reg_dl,name1)) {
			MEM_BlockWrite(SegPhys(ds)+reg_si,name1,(Bitu)(safe_strlen(name1)+1));	
			reg_ax=0x0100;
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x48:					/* Allocate memory */
		{
			uint16_t size=reg_bx;uint16_t seg;
			if (DOS_AllocateMemory(&seg,&size)) {
				reg_ax=seg;
				CALLBACK_SCF(false);
			} else {
				reg_ax=dos.errorcode;
				reg_bx=size;
				CALLBACK_SCF(true);
			}
			break;
		}
	case 0x49:					/* Free memory */
		if (DOS_FreeMemory(SegValue(es))) {
			CALLBACK_SCF(false);
		} else {            
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x4a:					/* Resize memory block */
		{
			uint16_t size=reg_bx;
			if (DOS_ResizeMemory(SegValue(es),&size)) {
				reg_ax=SegValue(es);
				CALLBACK_SCF(false);
			} else {            
				reg_ax=dos.errorcode;
				reg_bx=size;
				CALLBACK_SCF(true);
			}
			break;
		}
	case 0x4b:					/* EXEC Load and/or execute program */
		{
			result_errorcode = 0;
			MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
			LOG(LOG_EXEC,LOG_ERROR)("Execute %s %d",name1,reg_al);
			if (!DOS_Execute(name1,SegPhys(es)+reg_bx,reg_al)) {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
			}
		}
		break;
//TODO Check for use of execution state AL=5
	case 0x4c:					/* EXIT Terminate with return code */
		DOS_Terminate(dos.psp(),false,reg_al);
		if (result_errorcode)
			dos.return_code = result_errorcode;
		break;

	case 0x4d: // Get return code
		// Officially read from SDA and clear when read
		reg_al = dos.return_code;
		reg_ah = enum_val(dos.return_mode);
		CALLBACK_SCF(false);
		break;

	case 0x4e:					/* FINDFIRST Find first matching file */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if (DOS_FindFirst(name1, reg_cl)) {
			CALLBACK_SCF(false);	
			reg_ax=0;			/* Undocumented */
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		};
		break;		 
	case 0x4f:					/* FINDNEXT Find next matching file */
		if (DOS_FindNext()) {
			CALLBACK_SCF(false);
			/* reg_ax=0xffff;*/			/* Undocumented */
			reg_ax=0;				/* Undocumented:Qbix Willy beamish */
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		};
		break;		
	case 0x50:					/* Set current PSP */
		dos.psp(reg_bx);
		break;
	// case 0x51: Get current PSP, co-located with case 0x62
	case 0x52: {				/* Get list of lists */
		uint8_t count=2; // floppy drives always counted
		while (count<DOS_DRIVES && Drives[count] && !Drives[count]->IsRemovable()) count++;
		dos_infoblock.SetBlockDevices(count);
		RealPt addr=dos_infoblock.GetPointer();
		SegSet16(es,RealSegment(addr));
		reg_bx=RealOffset(addr);
		LOG(LOG_DOSMISC,LOG_NORMAL)("Call is made for list of lists - let's hope for the best");
		break; }
//TODO Think hard how shit this is gonna be
//And will any game ever use this :)
	case 0x53:					/* Translate BIOS parameter block to drive parameter block */
		E_Exit("Unhandled Dos 21 call %02X",reg_ah);
		break;
	case 0x54:					/* Get verify flag */
		reg_al=dos.verify?1:0;
		break;
	case 0x55:					/* Create Child PSP*/
		DOS_ChildPSP(reg_dx,reg_si);
		dos.psp(reg_dx);
		reg_al=0xf0;	/* al destroyed */
		break;
	case 0x56:					/* RENAME Rename file */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		MEM_StrCopy(SegPhys(es)+reg_di,name2,DOSNAMEBUF);
		if (DOS_Rename(name1,name2)) {
			CALLBACK_SCF(false);			
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;		
	case 0x57:					/* Get/Set File's Date and Time */
		if (reg_al == 0x00) {
			if (DOS_GetFileDate(reg_bx, &reg_cx, &reg_dx)) {
				CALLBACK_SCF(false);
			} else {
				reg_ax = dos.errorcode;
				CALLBACK_SCF(true);
			}
		} else if (reg_al == 0x01) {
			if (DOS_SetFileDate(reg_bx, reg_cx, reg_dx)) {
				CALLBACK_SCF(false);
			} else {
				reg_ax = dos.errorcode;
				CALLBACK_SCF(true);
			}
		} else {
			LOG(LOG_DOSMISC,LOG_ERROR)("DOS:57:Unsupported subfunction %X",reg_al);
		}
		break;
	case 0x58:					/* Get/Set Memory allocation strategy */
		switch (reg_al) {
		case 0:					/* Get Strategy */
			reg_ax=DOS_GetMemAllocStrategy();
			CALLBACK_SCF(false);
			break;
		case 1:					/* Set Strategy */
			if (DOS_SetMemAllocStrategy(reg_bx)) CALLBACK_SCF(false);
			else {
				reg_ax=1;
				CALLBACK_SCF(true);
			}
			break;
		case 2:					/* Get UMB Link Status */
			reg_al=dos_infoblock.GetUMBChainState()&1;
			CALLBACK_SCF(false);
			break;
		case 3:					/* Set UMB Link Status */
			if (DOS_LinkUMBsToMemChain(reg_bx)) CALLBACK_SCF(false);
			else {
				reg_ax=1;
				CALLBACK_SCF(true);
			}
			break;
		default:
			LOG(LOG_DOSMISC,LOG_ERROR)("DOS:58:Not Supported Set//Get memory allocation call %X",reg_al);
			reg_ax=1;
			CALLBACK_SCF(true);
		}
		break;
	case 0x59:					/* Get Extended error information */
		reg_ax=dos.errorcode;
		if (dos.errorcode==DOSERR_FILE_NOT_FOUND || dos.errorcode==DOSERR_PATH_NOT_FOUND) {
			reg_bh=8;	//Not Found error class (Road Hog)
		} else {
			reg_bh=0;	//Unspecified error class
		}
		reg_bl=1;	//Retry retry retry
		reg_ch = 0;     // Unknown error locus
		CALLBACK_SCF(false); //undocumented
		break;
	case 0x5a:					/* Create temporary file */
		{
			uint16_t handle;
			MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
			if (DOS_CreateTempFile(name1,&handle)) {
				reg_ax=handle;
				MEM_BlockWrite(SegPhys(ds)+reg_dx,name1,(Bitu)(safe_strlen(name1)+1));
				CALLBACK_SCF(false);
			} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
			}
		}
		break;
	case 0x5b:					/* Create new file */
		{
			MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
			uint16_t handle;
			if (DOS_FileExists(name1)) {
				DOS_SetError(DOSERR_FILE_ALREADY_EXISTS);
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
			} else if (DOS_CreateFile(name1, reg_cl, &handle)) {
				reg_ax=handle;
				CALLBACK_SCF(false);
			} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
			}
			break;
		}
	case 0x5c:			/* FLOCK File region locking */
		{
			const uint16_t entry = reg_bx;
			const uint32_t pos = (static_cast<uint32_t>(reg_cx) << 16) | static_cast<uint32_t>(reg_dx);
			const uint32_t len = (static_cast<uint32_t>(reg_si) << 16) | static_cast<uint32_t>(reg_di);
			bool success = false;
			if (reg_al == 0) {
				success = DOS_LockFile(entry, pos, len);
			} else if (reg_al == 1) {
				success = DOS_UnlockFile(entry, pos, len);
			} else {
				DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			}
			if (success) {
				reg_ax = 0;
				CALLBACK_SCF(false);
			} else {
				reg_ax = dos.errorcode;
				CALLBACK_SCF(true);
			}
			break;
		}
	case 0x5d:					/* Network Functions */
		if(reg_al == 0x06) {
			SegSet16(ds,DOS_SDA_SEG);
			reg_si = DOS_SDA_OFS;
			reg_cx = 0x80;  // swap if in dos
			reg_dx = 0x1a;  // swap always
			CALLBACK_SCF(false);
			LOG(LOG_DOSMISC,LOG_ERROR)("Get SDA, Let's hope for the best!");
		} else {
			LOG(LOG_DOSMISC,LOG_ERROR)("DOS:5D:Unsupported subfunction %X",reg_al);
		}
		break;
	case 0x5e:             /* Network and printer functions */
		if (!reg_al) { // Get machine name
			int result = gethostname(name1, DOSNAMEBUF);
			if (!result) {
				strcat(name1, "               ");
				name1[15] = 0;
				MEM_BlockWrite(SegPhys(ds) + reg_dx, name1, 16);
				reg_cx = 0x1ff;
				CALLBACK_SCF(false);
				break;
			}
		}
		reg_al = 1;
		CALLBACK_SCF(true);
		break;
	case 0x5f:               /* Network redirection */
		reg_ax = 0x0001; // Failing it
		CALLBACK_SCF(true);
		break; 
	case 0x60:					/* Canonicalize filename or path */
		MEM_StrCopy(SegPhys(ds)+reg_si,name1,DOSNAMEBUF);
		if (DOS_Canonicalize(name1,name2)) {
			MEM_BlockWrite(SegPhys(es)+reg_di,name2,(Bitu)(strlen(name2)+1));	
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;

	case 0x51: /* Get Current PSP */
	case 0x62: /* Get Current PSP Address */
		reg_bx=dos.psp();
		break;
	case 0x63:					/* DOUBLE BYTE CHARACTER SET */
		if(reg_al == 0) {
			SegSet16(ds,RealSegment(dos.tables.dbcs));
			reg_si=RealOffset(dos.tables.dbcs);		
			reg_al = 0;
			CALLBACK_SCF(false); //undocumented
		} else reg_al = 0xff; //Doesn't officially touch carry flag
		break;
	case 0x64:					/* Set device driver lookahead flag */
		LOG(LOG_DOSMISC,LOG_NORMAL)("set driver look ahead flag");
		break;
	case 0x65:					/* Get extented country information and a lot of other useless shit*/
		{ /* Todo maybe fully support this for now we set it standard for USA */ 
			LOG(LOG_DOSMISC,LOG_ERROR)("DOS:65:Extended country information call %X",reg_ax);
			if((reg_al <=  0x07) && (reg_cx < 0x05)) {
				DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
				CALLBACK_SCF(true);
				break;
			}
			Bitu len = 0; /* For 0x21 and 0x22 */
			PhysPt data=SegPhys(es)+reg_di;
			switch (reg_al) {
			case 0x01:
				mem_writeb(data + 0x00,reg_al);
				mem_writew(data + 0x01,0x26);
				mem_writew(data + 0x03,1);
				if(reg_cx > 0x06 ) mem_writew(data+0x05,dos.loaded_codepage);
				if(reg_cx > 0x08 ) {
					Bitu amount = (reg_cx>=0x29)?0x22:(reg_cx-7);
					MEM_BlockWrite(data + 0x07,dos.tables.country,amount);
					reg_cx=(reg_cx>=0x29)?0x29:reg_cx;
				}
				CALLBACK_SCF(false);
				break;
			case 0x05: // Get pointer to filename terminator table
				mem_writeb(data + 0x00, reg_al);
				mem_writed(data + 0x01, dos.tables.filenamechar);
				reg_cx = 5;
				CALLBACK_SCF(false);
				break;
			case 0x02: // Get pointer to uppercase table
				mem_writeb(data + 0x00, reg_al);
				mem_writed(data + 0x01, dos.tables.upcase);
				reg_cx = 5;
				CALLBACK_SCF(false);
				break;
			case 0x06: // Get pointer to collating sequence table
				mem_writeb(data + 0x00, reg_al);
				mem_writed(data + 0x01, dos.tables.collatingseq);
				reg_cx = 5;
				CALLBACK_SCF(false);
				break;
			case 0x03: // Get pointer to lowercase table
			case 0x04: // Get pointer to filename uppercase table
			case 0x07: // Get pointer to double byte char set table
				mem_writeb(data + 0x00, reg_al);
				mem_writed(data + 0x01, dos.tables.dbcs); //used to be 0
				reg_cx = 5;
				CALLBACK_SCF(false);
				break;
			case 0x20: /* Capitalize Character */
				{
					int in  = reg_dl;
					int out = toupper(in);
					reg_dl  = (uint8_t)out;
				}
				CALLBACK_SCF(false);
				break;
			case 0x21: /* Capitalize String (cx=length) */
			case 0x22: /* Capatilize ASCIZ string */
				data = SegPhys(ds) + reg_dx;
				if(reg_al == 0x21) len = reg_cx; 
				else len = mem_strlen(data); /* Is limited to 1024 */

				if(len > DOS_COPYBUFSIZE - 1) E_Exit("DOS:0x65 Buffer overflow");
				if(len) {
					MEM_BlockRead(data,dos_copybuf,len);
					dos_copybuf[len] = 0;
					//No upcase as String(0x21) might be multiple asciz strings
					for (Bitu count = 0; count < len;count++)
						dos_copybuf[count] = (uint8_t)toupper(*reinterpret_cast<unsigned char*>(dos_copybuf+count));
					MEM_BlockWrite(data,dos_copybuf,len);
				}
				CALLBACK_SCF(false);
				break;
			default:
				E_Exit("DOS:0x65:Unhandled country information call %2X",reg_al);	
			};
			break;
		}
	case 0x66:					/* Get/Set global code page table  */
		if (reg_al==1) {
			LOG(LOG_DOSMISC,LOG_ERROR)("Getting global code page table");
			reg_bx=reg_dx=dos.loaded_codepage;
			CALLBACK_SCF(false);
			break;
		}
		LOG(LOG_DOSMISC,LOG_NORMAL)("DOS:Setting code page table is not supported");
		break;
	case 0x67:					/* Set handle count */
		/* Weird call to increase amount of file handles needs to allocate memory if >20 */
		{
			DOS_PSP psp(dos.psp());
			psp.SetNumFiles(reg_bx);
			CALLBACK_SCF(false);
			break;
		};
	case 0x6a:					/* Same as commit file */
	case 0x68:					/* FFLUSH Commit file */
		if(DOS_FlushFile(reg_bl)) {
			reg_ah = 0x68;
			CALLBACK_SCF(false);
		} else {
			reg_ax = dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x69:					/* Get/Set disk serial number */
		{
			uint16_t old_cx=reg_cx;
			switch(reg_al)		{
			case 0x00:				/* Get */
				LOG(LOG_DOSMISC,LOG_WARN)("DOS:Get Disk serial number");
				reg_cl=0x66;// IOCTL function
				break;
			case 0x01:				/* Set */
				LOG(LOG_DOSMISC,LOG_WARN)("DOS:Set Disk serial number");
				reg_cl=0x46;// IOCTL function
				break;
			default:
				E_Exit("DOS:Illegal Get Serial Number call %2X",reg_al);
			}	
			reg_ch=0x08;	// IOCTL category: disk drive
			reg_ax=0x440d;	// Generic block device request
			DOS_21Handler();
			reg_cx=old_cx;
			break;
		} 
	case 0x6c:					/* Extended Open/Create */
		MEM_StrCopy(SegPhys(ds)+reg_si,name1,DOSNAMEBUF);
		if (DOS_OpenFileExtended(name1, reg_bx, reg_cl, reg_dx,
		                         &reg_ax, &reg_cx)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;

	case 0x71:					/* Unknown probably 4dos detection */
		reg_ax=0x7100;
		CALLBACK_SCF(true); //Check this! What needs this ? See default case
		LOG(LOG_DOSMISC,LOG_NORMAL)("DOS:Windows long file name support call %2X",reg_al);
		break;

	case 0xE0:
	case 0x18:	            	/* NULL Function for CP/M compatibility or Extended rename FCB */
	case 0x1d:	            	/* NULL Function for CP/M compatibility or Extended rename FCB */
	case 0x1e:	            	/* NULL Function for CP/M compatibility or Extended rename FCB */
	case 0x20:	            	/* NULL Function for CP/M compatibility or Extended rename FCB */
	case 0x6b:		            /* NULL Function */
	case 0x61:		            /* UNUSED */
	case 0xEF:                  /* Used in Ancient Art Of War CGA */
	default:
		if (reg_ah < 0x6d) LOG(LOG_DOSMISC,LOG_ERROR)("DOS:Unhandled call %02X al=%02X. Set al to default of 0",reg_ah,reg_al); //Less errors. above 0x6c the functions are simply always skipped, only al is zeroed, all other registers untouched
		reg_al=0x00; /* default value */
		break;
	};
	return CBRET_NONE;
}


static Bitu DOS_20Handler(void) {
	reg_ah=0x00;
	DOS_21Handler();
	return CBRET_NONE;
}

static Bitu DOS_27Handler(void) {
	// Terminate & stay resident
	uint16_t para = (reg_dx/16)+((reg_dx % 16)>0);
	uint16_t psp = dos.psp(); //mem_readw(SegPhys(ss)+reg_sp+2);
	if (DOS_ResizeMemory(psp,&para)) DOS_Terminate(psp,true,0);
	return CBRET_NONE;
}

static uint16_t DOS_SectorAccess(const bool read)
{
	const auto drive = std::dynamic_pointer_cast<fatDrive>(Drives.at(reg_al));
	assert(drive);

	auto bufferSeg = SegValue(ds);
	auto bufferOff = reg_bx;
	auto sectorCnt = reg_cx;
	auto sectorNum = static_cast<uint32_t>(reg_dx) + drive->partSectOff;
	auto sectorEnd = drive->getSectorCount() + drive->partSectOff;

	if (sectorCnt == 0xffff) { // large partition form
		bufferSeg = real_readw(SegValue(ds), reg_bx + 8);
		bufferOff = real_readw(SegValue(ds), reg_bx + 6);
		sectorCnt = real_readw(SegValue(ds), reg_bx + 4);
		sectorNum = real_readd(SegValue(ds), reg_bx + 0) + drive->partSectOff;
	} else if (sectorEnd > 0xffff)
		return 0x0207; // must use large partition form

	uint8_t sectorBuf[512];
	while (sectorCnt--) {
		if (sectorNum >= sectorEnd)
			return 0x0408; // sector not found
		if (read) {
			if (drive->readSector(sectorNum++, &sectorBuf))
				return 0x0408;
			for (const auto& sectorVal : sectorBuf)
				real_writeb(bufferSeg, bufferOff++, sectorVal);
		} else {
			for (auto &sectorVal : sectorBuf)
				sectorVal = real_readb(bufferSeg, bufferOff++);
			if (drive->writeSector(sectorNum++, &sectorBuf))
				return 0x0408;
		}
	}
	return 0;
}

static Bitu DOS_25Handler(void)
{
	if (reg_al >= DOS_DRIVES || !Drives[reg_al] || Drives[reg_al]->IsRemovable()) {
		reg_ax = 0x8002;
		SETFLAGBIT(CF,true);
	} else if (Drives[reg_al]->GetType() == DosDriveType::Fat) {
		reg_ax = DOS_SectorAccess(true);
		SETFLAGBIT(CF, reg_ax != 0);
	} else {
		if (reg_cx == 1 && reg_dx == 0) {
			if (reg_al >= 2) {
				// write some BPB data into buffer for MicroProse installers
				real_writew(SegValue(ds), reg_bx + 0x1c, 0x3f); // hidden sectors
			}
		} else {
			LOG(LOG_DOSMISC,LOG_NORMAL)("int 25 called but not as disk detection drive %u",reg_al);
		}
		SETFLAGBIT(CF,false);
		reg_ax = 0;
	}
    return CBRET_NONE;
}
static Bitu DOS_26Handler(void) {
	LOG(LOG_DOSMISC, LOG_NORMAL)("int 26 called: hope for the best!");
	if (reg_al >= DOS_DRIVES || !Drives[reg_al] || Drives[reg_al]->IsRemovable()) {	
		reg_ax = 0x8002;
		SETFLAGBIT(CF,true);
	} else if (Drives[reg_al]->GetType() == DosDriveType::Fat) {
		reg_ax = DOS_SectorAccess(false);
		SETFLAGBIT(CF, reg_ax != 0);
	} else {
		SETFLAGBIT(CF,false);
		reg_ax = 0;
	}
    return CBRET_NONE;
}

bool DOS_IsCancelRequest()
{
	if (shutdown_requested)
		return true;

	CALLBACK_Idle();
	while (!(Files[STDIN]->GetInformation() & (1 << 6))) {
		// A key is waiting, read it
		uint16_t count = 1;
		uint8_t code   = 0;
		DOS_ReadFile(STDIN, &code, &count);

		// Check if user requested to cancel
		if (shutdown_requested || count == 0 || code == 'q' ||
		    code == 'Q' || code == Ascii::CtrlC || code == Ascii::Escape) {
			return true;
		}
	}

	// Return control if no key pressed
	return shutdown_requested;
}

DOS_Version DOS_ParseVersion(const char *word, const char *args)
{
	DOS_Version new_version = {5, 0, 0}; // Default to 5.0
	assert(word != nullptr && args != nullptr);
	if (*word && !*args && (strchr(word, '.') != nullptr)) {
		// Allow usual syntax: ver set 7.1
		const char *p = strchr(word, '.');
		p++;
		int minor = -1;
		if (isdigit(*p)) {
			int len = strlen(p);
			// Get the first 2 characters as minor version if there are more
			minor = atoi(len > 2 ? std::string(p).substr(0, 2).c_str() : p);
			// If .1 as the minor version, regard it as .10
			if (len == 1)
				minor *= 10;
		}
		// Return the new DOS version, or 0.0 for invalid DOS version
		if (!isdigit(*word) || atoi(word) < 0 || atoi(word) > 30 || minor < 0 || (!atoi(word) && !minor)) {
			new_version.major = 0;
			new_version.minor = 0;
		} else {
			new_version.major = static_cast<uint8_t>(atoi(word));
			new_version.minor = static_cast<uint8_t>(minor);
        }
	} else if (*word || *args) { // Official DOSBox syntax: ver set 6 2
		// If only an integer like 7, regard it as 7.0, or take args as minor version
		int minor = *args ? (isdigit(*args) ? atoi(args) : -1) : 0;
		// Get the first 2 digits of if there are more in the number
		while (minor > 99)
			minor /= 10;
		// Return the new DOS version, or 0.0 for invalid DOS version
		if (!isdigit(*word) || atoi(word) < 0 || atoi(word) > 30 || minor < 0 || (!atoi(word) && !minor)) {
			new_version.major = 0;
			new_version.minor = 0;
		} else {
			new_version.major = static_cast<uint8_t>(atoi(word));
			new_version.minor = static_cast<uint8_t>(minor);
		}
	}
	return new_version;
}

class DOS:public Module_base{
private:
	CALLBACK_HandlerObject callback[7];
public:
	DOS(Section* configuration):Module_base(configuration){
		callback[0].Install(DOS_20Handler,CB_IRET,"DOS Int 20");
		callback[0].Set_RealVec(0x20);

		callback[1].Install(DOS_21Handler,CB_INT21,"DOS Int 21");
		callback[1].Set_RealVec(0x21);
	//Pseudo code for int 21
	// sti
	// callback 
	// iret
	// retf  <- int 21 4c jumps here to mimic a retf Cyber

		callback[2].Install(DOS_25Handler,CB_RETF_STI,"DOS Int 25");
		callback[2].Set_RealVec(0x25);

		callback[3].Install(DOS_26Handler,CB_RETF_STI,"DOS Int 26");
		callback[3].Set_RealVec(0x26);

		callback[4].Install(DOS_27Handler,CB_IRET,"DOS Int 27");
		callback[4].Set_RealVec(0x27);

		callback[5].Install(nullptr,CB_IRET,"DOS Int 28");
		callback[5].Set_RealVec(0x28);

		callback[6].Install(nullptr,CB_INT29,"CON Output Int 29");
		callback[6].Set_RealVec(0x29);
		// pseudocode for CB_INT29:
		//	push ax
		//	mov ah, 0x0e
		//	int 0x10
		//	pop ax
		//	iret

		AddMountTypeMessages();
		DOS_SetupFiles();								/* Setup system File tables */
		DOS_SetupDevices();							/* Setup dos devices */
		DOS_SetupTables();
		DOS_SetupMemory();								/* Setup first MCB */
		DOS_SetupPrograms();
		DOS_SetupMisc();							/* Some additional dos interrupts */
		DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetDrive(25); /* Else the next call gives a warning. */
		DOS_SetDefaultDrive(25);

		dos.version.major=5;
		dos.version.minor=0;
		dos.direct_output=false;
		dos.internal_output=false;

		const Section_prop* section = static_cast<Section_prop*>(configuration);
		std::string args = section->Get_string("ver");
		std::string word = strip_word(args);
		const auto new_version = DOS_ParseVersion(word.c_str(), args.c_str());
		if (new_version.major || new_version.minor) {
			dos.version.major = new_version.major;
			dos.version.minor = new_version.minor;
		}
	}

	// Shutdown the DOS OS constructs leaving only the BIOS and hardware
	// layers.
	~DOS()
	{
		// Clear the drive pointers and file objects. The actual drive
		// objects are managed by the drive manager class. The 'Files'
		// destructors depend on the DOS API (file flush, close, and
		// date and time lookup), so it's import to shut these down when
		// DOS is still available.
		DOS_ClearDrivesAndFiles();

		// de-init devices, this allows DOSBox to cleanly re-initialize
		// without throwing an inevitable `DOS: Too many devices added`
		// exception
		DOS_ShutDownDevices();

		DOS_FreeTableMemory();
	}
};

static DOS* test;

void DOS_ShutDown(Section* /*sec*/) {
	delete test;
}

void DOS_Init(Section* sec)
{
	assert(sec);
	test = new DOS(sec);

	sec->AddDestroyFunction(&DOS_ShutDown);
}
