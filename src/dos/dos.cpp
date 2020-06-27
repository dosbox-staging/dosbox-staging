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
 *
 *  Wengier: LFN support
 */


#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "dosbox.h"
#include "bios.h"
#include "mem.h"
#include "callback.h"
#include "regs.h"
#include "dos_inc.h"
#include "setup.h"
#include "support.h"
#include "serialport.h"
#include "drives.h"
#include "shell.h"
#include "time.h"

DOS_Block dos;
DOS_InfoBlock dos_infoblock;
int enablelfn=-1;
bool uselfn;
extern bool force_sfn;
extern int lfn_filefind_handle;

#define DOS_COPYBUFSIZE 0x10000
Bit8u dos_copybuf[DOS_COPYBUFSIZE];

void DOS_SetError(Bit16u code) {
	dos.errorcode=code;
}

const Bit8u DOS_DATE_months[] = {
	0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static void DOS_AddDays(Bitu days) {
	dos.date.day += days;
	Bit8u monthlimit = DOS_DATE_months[dos.date.month];

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

void DOS_Int21_7139(char *name1, const char *name2);
void DOS_Int21_713a(char *name1, const char *name2);
void DOS_Int21_713b(char *name1, const char *name2);
void DOS_Int21_7141(char *name1, const char *name2);
void DOS_Int21_7143(char *name1, const char *name2);
void DOS_Int21_7147(char *name1, const char *name2);
void DOS_Int21_714e(char *name1, char *name2);
void DOS_Int21_714f(const char *name1, const char *name2);
void DOS_Int21_7156(char *name1, char *name2);
void DOS_Int21_7160(char *name1, char *name2);
void DOS_Int21_716c(char *name1, const char *name2);
void DOS_Int21_71a0(char *name1, char *name2);
void DOS_Int21_71a1(const char *name1, const char *name2);
void DOS_Int21_71a6(const char *name1, const char *name2);
void DOS_Int21_71a7(const char *name1, const char *name2);
void DOS_Int21_71a8(char* name1, const char* name2);
void DOS_Int21_71aa(char* name1, const char* name2);

typedef struct {
	UINT16 size_of_structure;
	UINT16 structure_version;
	UINT32 sectors_per_cluster;
	UINT32 bytes_per_sector;
	UINT32 available_clusters_on_drive;
	UINT32 total_clusters_on_drive;
	UINT32 available_sectors_on_drive;
	UINT32 total_sectors_on_drive;
	UINT32 available_allocation_units;
	UINT32 total_allocation_units;
	UINT8 reserved[8];
} ext_space_info_t;

#define DOSNAMEBUF 256
static Bitu DOS_21Handler(void) {
	if (((reg_ah != 0x50) && (reg_ah != 0x51) && (reg_ah != 0x62) && (reg_ah != 0x64)) && (reg_ah<0x6c)) {
		DOS_PSP psp(dos.psp());
		psp.SetStack(RealMake(SegValue(ss),reg_sp-18));
	}

	char name1[DOSNAMEBUF+2+DOS_NAMELENGTH_ASCII];
	char name2[DOSNAMEBUF+2+DOS_NAMELENGTH_ASCII];
	
	static Bitu time_start = 0; //For emulating temporary time changes.

	switch (reg_ah) {
	case 0x00:		/* Terminate Program */
		DOS_Terminate(mem_readw(SegPhys(ss)+reg_sp+2),false,0);
		break;
	case 0x01:		/* Read character from STDIN, with echo */
		{	
			Bit8u c;Bit16u n=1;
			dos.echo=true;
			DOS_ReadFile(STDIN,&c,&n);
			reg_al=c;
			dos.echo=false;
		}
		break;
	case 0x02:		/* Write character to STDOUT */
		{
			Bit8u c=reg_dl;Bit16u n=1;
			DOS_WriteFile(STDOUT,&c,&n);
			//Not in the official specs, but happens nonetheless. (last written character)
			reg_al=(c==9)?0x20:c; //strangely, tab conversion to spaces is reflected here
		}
		break;
	case 0x03:		/* Read character from STDAUX */
		{
			Bit16u port = real_readw(0x40,0);
			if(port!=0 && serialports[0]) {
				Bit8u status;
				// RTS/DTR on
				IO_WriteB(port+4,0x3);
				serialports[0]->Getchar(&reg_al, &status, true, 0xFFFFFFFF);
			}
		}
		break;
	case 0x04:		/* Write Character to STDAUX */
		{
			Bit16u port = real_readw(0x40,0);
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
				Bit8u c;Bit16u n=1;
				DOS_ReadFile(STDIN,&c,&n);
				reg_al=c;
				CALLBACK_SZF(false);
				break;
			}
		default:
			{
				Bit8u c = reg_dl;Bit16u n = 1;
				dos.direct_output=true;
				DOS_WriteFile(STDOUT,&c,&n);
				dos.direct_output=false;
				reg_al=c;
			}
			break;
		};
		break;
	case 0x07:		/* Character Input, without echo */
		{
				Bit8u c;Bit16u n=1;
				DOS_ReadFile (STDIN,&c,&n);
				reg_al=c;
				break;
		};
	case 0x08:		/* Direct Character Input, without echo (checks for breaks officially :)*/
		{
				Bit8u c;Bit16u n=1;
				DOS_ReadFile (STDIN,&c,&n);
				reg_al=c;
				break;
		};
	case 0x09:		/* Write string to STDOUT */
		{	
			Bit8u c;Bit16u n=1;
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
			Bit8u free=mem_readb(data);
			Bit8u read=0;Bit8u c;Bit16u n=1;
			if (!free) break;
			free--;
			for(;;) {
				DOS_ReadFile(STDIN,&c,&n);
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
					Bit8u bell = 7;
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
			Bit8u handle=RealHandle(STDIN);
			if (handle!=0xFF && Files[handle] && Files[handle]->IsName("CON")) {
				Bit8u c;Bit16u n;
				while (DOS_GetSTDINStatus()) {
					n=1;	DOS_ReadFile(STDIN,&c,&n);
				}
			}
			switch (reg_al) {
			case 0x1:
			case 0x6:
			case 0x7:
			case 0x8:
			case 0xa:
				{ 
					Bit8u oldah=reg_ah;
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
			Bit16u toread=1;
			reg_al = DOS_FCBRandomRead(SegValue(ds),reg_dx,&toread,true);
		}
		LOG(LOG_FCB,LOG_NORMAL)("DOS:0x21 FCB-Random read used, result:al=%d",reg_al);
		break;
	case 0x22:		/* Write random record to FCB */
		{
			Bit16u towrite=1;
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
			Bit8u difference;
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
		if (reg_cx<1980) { reg_al=0xff;break;}
		if ((reg_dh>12) || (reg_dh==0))	{ reg_al=0xff;break;}
		if (reg_dl==0) { reg_al=0xff;break;}
 		if (reg_dl>DOS_DATE_months[reg_dh]) {
			if(!((reg_dh==2)&&(reg_cx%4 == 0)&&(reg_dl==29))) // february pass
			{ reg_al=0xff;break; }
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

		reg_dl=(Bit8u)((Bitu)time % 100); // 1/100 seconds
		time/=100;
		reg_dh=(Bit8u)((Bitu)time % 60); // seconds
		time/=60;
		reg_cl=(Bit8u)((Bitu)time % 60); // minutes
		time/=60;
		reg_ch=(Bit8u)((Bitu)time % 24); // hours

		//Simulate DOS overhead for timing-sensitive games
        //Robomaze 2
		overhead();
		break;
	}
	case 0x2d:		/* Set System Time */
		LOG(LOG_DOSMISC,LOG_ERROR)("DOS:Set System Time not supported");
		//Check input parameters nonetheless
		if( reg_ch > 23 || reg_cl > 59 || reg_dh > 59 || reg_dl > 99 )
			reg_al = 0xff; 
		else { //Allow time to be set to zero. Restore the orginal time for all other parameters. (QuickBasic)
			if (reg_cx == 0 && reg_dx == 0) {time_start = mem_readd(BIOS_TIMER);LOG_MSG("Warning: game messes with DOS time!");}
			else time_start = 0;
			reg_al = 0;
		}
		break;
	case 0x2e:		/* Set Verify flag */
		dos.verify=(reg_al==1);
		break;
	case 0x2f:		/* Get Disk Transfer Area */
		SegSet16(es,RealSeg(dos.dta()));
		reg_bx=RealOff(dos.dta());
		break;
	case 0x30:		/* Get DOS Version */
		if (reg_al==0) reg_bh=0xFF;		/* Fake Microsoft DOS */
		if (reg_al==1) reg_bh=0x10;		/* DOS is in HMA */
		reg_al=dos.version.major;
		reg_ah=dos.version.minor;
		/* Serialnumber */
		reg_bl=0x00;
		reg_cx=0x0000;
		break;
	case 0x31:		/* Terminate and stay resident */
		// Important: This service does not set the carry flag!
		DOS_ResizeMemory(dos.psp(),&reg_dx);
		DOS_Terminate(dos.psp(),true,reg_al);
		break;
	case 0x1f: /* Get drive parameter block for default drive */
	case 0x32: /* Get drive parameter block for specific drive */
		{	/* Officially a dpb should be returned as well. The disk detection part is implemented */
			Bit8u drive=reg_dl;
			if (!drive || reg_ah==0x1f) drive = DOS_GetDefaultDrive();
			else drive--;
			if (drive < DOS_DRIVES && Drives[drive] && !Drives[drive]->isRemovable()) {
				reg_al = 0x00;
				SegSet16(ds,dos.tables.dpb);
				reg_bx = drive*9;
				LOG(LOG_DOSMISC,LOG_ERROR)("Get drive parameter block.");
			} else {
				reg_al=0xff;
			}
		}
		break;
	case 0x33:		/* Extended Break Checking */
		switch (reg_al) {
			case 0:reg_dl=dos.breakcheck;break;			/* Get the breakcheck flag */
			case 1:dos.breakcheck=(reg_dl>0);break;		/* Set the breakcheck flag */
			case 2:{bool old=dos.breakcheck;dos.breakcheck=(reg_dl>0);reg_dl=old;}break;
			case 3: /* Get cpsw */
				/* Fallthrough */
			case 4: /* Set cpsw */
				LOG(LOG_DOSMISC,LOG_ERROR)("Someone playing with cpsw %x",reg_ax);
				break;
			case 5:reg_dl=3;break;//TODO should be z						/* Always boot from c: :) */
			case 6:											/* Get true version number */
				reg_bl=dos.version.major;
				reg_bh=dos.version.minor;
				reg_dl=dos.version.revision;
				reg_dh=0x10;								/* Dos in HMA */
				break;
			default:
				LOG(LOG_DOSMISC,LOG_ERROR)("Weird 0x33 call %2X",reg_al);
				reg_al =0xff;
				break;
		}
		break;
	case 0x34:		/* Get INDos Flag */
		SegSet16(es,DOS_SDA_SEG);
		reg_bx=DOS_SDA_OFS + 0x01;
		break;
	case 0x35:		/* Get interrupt vector */
		reg_bx=real_readw(0,((Bit16u)reg_al)*4);
		SegSet16(es,real_readw(0,((Bit16u)reg_al)*4+2));
		break;
	case 0x36:		/* Get Free Disk Space */
		{
			Bit16u bytes,clusters,free;
			Bit8u sectors;
			if (DOS_GetFreeDiskSpace(reg_dl,&bytes,&sectors,&clusters,&free)) {
				reg_ax=sectors;
				reg_bx=free;
				reg_cx=bytes;
				reg_dx=clusters;
			} else {
				Bit8u drive=reg_dl;
				if (drive==0) drive=DOS_GetDefaultDrive();
				else drive--;
				if (drive<2) {
					// floppy drive, non-present drivesdisks issue floppy check through int24
					// (critical error handler); needed for Mixed up Mother Goose (hook)
//					CALLBACK_RunRealInt(0x24);
				}
				reg_ax=0xffff;	// invalid drive specified
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
	case 0x38:					/* Set Country Code */	
		if (reg_al==0) {		/* Get country specidic information */
			PhysPt dest = SegPhys(ds)+reg_dx;
			MEM_BlockWrite(dest,dos.tables.country,0x18);
			reg_ax = reg_bx = 0x01;
			CALLBACK_SCF(false);
			break;
		} else {				/* Set country code */
			LOG(LOG_MISC,LOG_ERROR)("DOS:Setting country code not supported");
		}
		CALLBACK_SCF(true);
		break;
	case 0x39:		/* MKDIR Create directory */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		force_sfn = true;
		if (DOS_MakeDir(name1)) {
			reg_ax=0x05;	/* ax destroyed */
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		force_sfn = false;
		break;
	case 0x3a:		/* RMDIR Remove directory */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		force_sfn = true;
		if  (DOS_RemoveDir(name1)) {
			reg_ax=0x05;	/* ax destroyed */
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
			LOG(LOG_MISC,LOG_NORMAL)("Remove dir failed on %s with error %X",name1,dos.errorcode);
		}
		force_sfn = false;
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
		force_sfn = true;
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if (DOS_CreateFile(name1,reg_cx,&reg_ax)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		force_sfn = false;
		break;
	case 0x3d:		/* OPEN Open existing file */
	{
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		force_sfn = true;
		Bit8u oldal=reg_al;
		if (DOS_OpenFile(name1,reg_al,&reg_ax)) {
			force_sfn = false;
			CALLBACK_SCF(false);
		} else {
			force_sfn = false;
			if (uselfn&&DOS_OpenFile(name1,oldal,&reg_ax)) {
				CALLBACK_SCF(false);
				break;
			}
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		force_sfn = false;
		break;
	}
	case 0x3e:		/* CLOSE Close file */
		if (DOS_CloseFile(reg_bx)) {
//			reg_al=0x01;	/* al destroyed. Refcount */
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x3f:		/* READ Read from file or device */
		{ 
			Bit16u toread=reg_cx;
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
			Bit16u towrite=reg_cx;
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
		force_sfn = true;
		if (DOS_UnlinkFile(name1)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		force_sfn = false;
		break;
	case 0x42:					/* LSEEK Set current file position */
		{
			Bit32u pos=(reg_cx<<16) + reg_dx;
			if (DOS_SeekFile(reg_bx,&pos,reg_al)) {
				reg_dx=(Bit16u)(pos >> 16);
				reg_ax=(Bit16u)(pos & 0xFFFF);
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
				Bit16u attr_val=reg_cx;
				if (DOS_GetFileAttr(name1,&attr_val)) {
					reg_cx=attr_val;
					reg_ax=attr_val; /* Undocumented */   
					CALLBACK_SCF(false);
				} else {
					CALLBACK_SCF(true);
					reg_ax=dos.errorcode;
				}
				break;
			};
		case 0x01:				/* Set */
			LOG(LOG_MISC,LOG_ERROR)("DOS:Set File Attributes for %s not supported",name1);
			if (DOS_SetFileAttr(name1,reg_cx)) {
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
		if (DOS_GetCurrentDir(reg_dl,name1,false)) {
			MEM_BlockWrite(SegPhys(ds)+reg_si,name1,(Bitu)(strlen(name1)+1));	
			reg_ax=0x0100;
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x48:					/* Allocate memory */
		{
			Bit16u size=reg_bx;Bit16u seg;
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
			Bit16u size=reg_bx;
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
		break;
	case 0x4d:					/* Get Return code */
		reg_al=dos.return_code;/* Officially read from SDA and clear when read */
		reg_ah=dos.return_mode;
		break;
	case 0x4e:					/* FINDFIRST Find first matching file */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if (DOS_FindFirst(name1,reg_cx)) {
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
	case 0x51:					/* Get current PSP */
		reg_bx=dos.psp();
		break;
	case 0x52: {				/* Get list of lists */
		Bit8u count=2; // floppy drives always counted
		while (count<DOS_DRIVES && Drives[count] && !Drives[count]->isRemovable()) count++;
		dos_infoblock.SetBlockDevices(count);
		RealPt addr=dos_infoblock.GetPointer();
		SegSet16(es,RealSeg(addr));
		reg_bx=RealOff(addr);
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
		force_sfn = true;
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		MEM_StrCopy(SegPhys(es)+reg_di,name2,DOSNAMEBUF);
		if (DOS_Rename(name1,name2)) {
			CALLBACK_SCF(false);			
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		force_sfn = false;
		break;		
	case 0x57:					/* Get/Set File's Date and Time */
		if (reg_al==0x00) {
			if (DOS_GetFileDate(reg_bx,&reg_cx,&reg_dx)) {
				CALLBACK_SCF(false);
			} else {
				CALLBACK_SCF(true);
			}
		} else if (reg_al==0x01) {
			LOG(LOG_DOSMISC,LOG_ERROR)("DOS:57:Set File Date Time Faked");
			CALLBACK_SCF(false);		
		} else {
			LOG(LOG_DOSMISC,LOG_ERROR)("DOS:57:Unsupported subtion %X",reg_al);
		}
		break;
	case 0x58:					/* Get/Set Memory allocation strategy */
		switch (reg_al) {
		case 0:					/* Get Strategy */
			reg_ax=DOS_GetMemAllocStrategy();
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
		reg_ch=0;	//Unkown error locus
		break;
	case 0x5a:					/* Create temporary file */
		{
			Bit16u handle;
			MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
			if (DOS_CreateTempFile(name1,&handle)) {
				reg_ax=handle;
				MEM_BlockWrite(SegPhys(ds)+reg_dx,name1,(Bitu)(strlen(name1)+1));
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
			Bit16u handle;
			if (DOS_OpenFile(name1,0,&handle)) {
				DOS_CloseFile(handle);
				DOS_SetError(DOSERR_FILE_ALREADY_EXISTS);
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
				break;
			}
			if (DOS_CreateFile(name1,reg_cx,&handle)) {
				reg_ax=handle;
				CALLBACK_SCF(false);
			} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
			}
			break;
		}
	case 0x5c:			/* FLOCK File region locking */
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		reg_ax = dos.errorcode;
		CALLBACK_SCF(true);
		break;
	case 0x5d:					/* Network Functions */
		if(reg_al == 0x06) {
			SegSet16(ds,DOS_SDA_SEG);
			reg_si = DOS_SDA_OFS;
			reg_cx = 0x80;  // swap if in dos
			reg_dx = 0x1a;  // swap always
			LOG(LOG_DOSMISC,LOG_ERROR)("Get SDA, Let's hope for the best!");
		}
		break;
	case 0x5f:					/* Network redirection */
		reg_ax=0x0001;		//Failing it
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
	case 0x62:					/* Get Current PSP Address */
		reg_bx=dos.psp();
		break;
	case 0x63:					/* DOUBLE BYTE CHARACTER SET */
		if(reg_al == 0) {
			SegSet16(ds,RealSeg(dos.tables.dbcs));
			reg_si=RealOff(dos.tables.dbcs);		
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
					reg_dl  = (Bit8u)out;
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
						dos_copybuf[count] = (Bit8u)toupper(*reinterpret_cast<unsigned char*>(dos_copybuf+count));
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
	case 0x68:                  /* FFLUSH Commit file */
		if(DOS_FlushFile(reg_bl)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax = dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x69:					/* Get/Set disk serial number */
		{
			Bit16u old_cx=reg_cx;
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
		if (DOS_OpenFileExtended(name1,reg_bx,reg_cx,reg_dx,&reg_ax,&reg_cx)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;

	case 0x71:					/* Unknown probably 4dos detection */
		printf("DOS:Windows long file name support call %2X\n",reg_al);
		if (!uselfn) {
				reg_ax=0x7100;
				CALLBACK_SCF(true); //Check this! What needs this ? See default case
				break;
		}
		switch(reg_al) {
			case 0x39:              /* LFN MKDIR */
					DOS_Int21_7139(name1, name2);
					break;
			case 0x3a:              /* LFN RMDIR */
					DOS_Int21_713a(name1, name2);
					break;
			case 0x3b:              /* LFN CHDIR */
					DOS_Int21_713b(name1, name2);
					break;
			case 0x41:              /* LFN UNLINK */
					DOS_Int21_7141(name1, name2);
					break;
			case 0x43:              /* LFN ATTR */
					DOS_Int21_7143(name1, name2);
					break;
			case 0x47:              /* LFN PWD */
					DOS_Int21_7147(name1, name2);
					break;
			case 0x4e:              /* LFN FindFirst */
					DOS_Int21_714e(name1, name2);
					break;
			case 0x4f:              /* LFN FindNext */
					DOS_Int21_714f(name1, name2);
					break;
			case 0x56:              /* LFN Rename */
					DOS_Int21_7156(name1, name2);
					break;
			case 0x60:              /* LFN GetName */
					DOS_Int21_7160(name1, name2);
					break;
			case 0x6c:              /* LFN Create */
					DOS_Int21_716c(name1, name2);
					break;
			case 0xa0:              /* LFN VolInfo */
					DOS_Int21_71a0(name1, name2);
					break;
			case 0xa1:              /* LFN FileClose */
					DOS_Int21_71a1(name1, name2);
					break;
			case 0xa6:              /* LFN GetFileInfoByHandle */
					DOS_Int21_71a6(name1, name2);
					break;
			case 0xa7:              /* LFN TimeConv */
					DOS_Int21_71a7(name1, name2);
					break;
			case 0xa8:              /* LFN GenSFN */
					DOS_Int21_71a8(name1, name2);
					break;
			case 0xaa:              /* LFN Subst */
					DOS_Int21_71aa(name1, name2);
					break;
			case 0xa9:              /* LFN Server Create */
					reg_ax=0x7100; // not implemented yet
			default:
					reg_ax=0x7100;
					CALLBACK_SCF(true); //Check this! What needs this ? See default case
		}
		break;

	case 0x73:
		if (dos.version.major < 7) {
			CALLBACK_SCF(true);
			reg_ax=0x7300;
		} else if (reg_al==3) {
			MEM_StrCopy(SegPhys(ds)+reg_dx,name1,reg_cx);
			if (name1[1]==':'&&name1[2]=='\\')
				reg_dl=name1[0]-'A'+1;
			else {
				reg_ax=0xffff;
				CALLBACK_SCF(true);
				break;
			}
			Bit16u bytes_per_sector,total_clusters,free_clusters;
			Bit8u sectors_per_cluster;
			if (DOS_GetFreeDiskSpace(reg_dl,&bytes_per_sector,&sectors_per_cluster,&total_clusters,&free_clusters)) {
				ext_space_info_t *info = new ext_space_info_t;
				info->size_of_structure = sizeof(ext_space_info_t);
				info->structure_version = 0;
				info->sectors_per_cluster = sectors_per_cluster;
				info->bytes_per_sector = bytes_per_sector;
				info->available_clusters_on_drive = free_clusters;
				info->total_clusters_on_drive = total_clusters;
				info->available_sectors_on_drive = sectors_per_cluster * free_clusters;
				info->total_sectors_on_drive = sectors_per_cluster * total_clusters;
				info->available_allocation_units = free_clusters;
				info->total_allocation_units = total_clusters;
				MEM_BlockWrite(SegPhys(es)+reg_di,info,sizeof(ext_space_info_t));
				delete(info);
				reg_ax=0;
				CALLBACK_SCF(false);
				}
			else
				{
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
				}
			break;
		} else {
			CALLBACK_SCF(true);
			reg_ax=0x7300;
			break;
		}
	case 0xE0:
	case 0x18:	            	/* NULL Function for CP/M compatibility or Extended rename FCB */
	case 0x1d:	            	/* NULL Function for CP/M compatibility or Extended rename FCB */
	case 0x1e:	            	/* NULL Function for CP/M compatibility or Extended rename FCB */
	case 0x20:	            	/* NULL Function for CP/M compatibility or Extended rename FCB */
	case 0x6b:		            /* NULL Function */
	case 0x61:		            /* UNUSED */
	case 0xEF:                  /* Used in Ancient Art Of War CGA */
	case 0x5e:					/* More Network Functions */
	default:
		if (reg_ah < 0x6d) LOG(LOG_DOSMISC,LOG_ERROR)("DOS:Unhandled call %02X al=%02X. Set al to default of 0",reg_ah,reg_al); //Less errors. above 0x6c the functions are simply always skipped, only al is zeroed, all other registers untouched
		reg_al=0x00; /* default value */
		break;
	};
	return CBRET_NONE;
}

void DOS_Int21_7139(char *name1, const char *name2) {
    (void)name2;
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if (DOS_MakeDir(name1)) {
				reg_ax=0;
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_713a(char *name1, const char *name2) {
    (void)name2;
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if  (DOS_RemoveDir(name1)) {
				reg_ax=0;
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
				LOG(LOG_MISC,LOG_NORMAL)("Remove dir failed on %s with error %X",name1,dos.errorcode);
		}
}

void DOS_Int21_713b(char *name1, const char *name2) {
    (void)name2;
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if  (DOS_ChangeDir(name1)) {
				reg_ax=0;
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_7141(char *name1, const char *name2) {
    (void)name2;
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if (DOS_UnlinkFile(name1)) {
				reg_ax=0;
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_7143(char *name1, const char *name2) {
    (void)name2;
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		switch (reg_bl) {
				case 0x00:                              /* Get */
				{
					Bit16u attr_val=reg_cx;
					if (DOS_GetFileAttr(name1,&attr_val)) {
							reg_cx=attr_val;
							reg_ax=0;
							CALLBACK_SCF(false);
					} else {
							CALLBACK_SCF(true);
							reg_ax=dos.errorcode;
					}
					break;
				};
				case 0x01:                              /* Set */
					if (DOS_SetFileAttr(name1,reg_cx)) {
							reg_ax=0;
							CALLBACK_SCF(false);
					} else {
							CALLBACK_SCF(true);
							reg_ax=dos.errorcode;
					}
					break;
				case 0x02:				/* Get compressed file size */
				{
					reg_ax=0;
					reg_dx=0;
					unsigned long size = DOS_GetCompressedFileSize(name1);
					if (size != (unsigned long)(-1l)) {
#if defined (WIN32)
						reg_ax = LOWORD(size);
						reg_dx = HIWORD(size);
#endif
						CALLBACK_SCF(false);
					} else {
						CALLBACK_SCF(true);
						reg_ax=dos.errorcode;
					}
					break;
				}
				case 0x03:
				case 0x05:
				case 0x07:
				{
#if defined (WIN32) && !defined(HX_DOS)
					HANDLE hFile = DOS_CreateOpenFile(name1);
					if (hFile != INVALID_HANDLE_VALUE) {
						time_t clock = time(NULL), ttime;
						struct tm *t = localtime(&clock);
						FILETIME time;
						t->tm_isdst = -1;
						t->tm_sec  = (((int)reg_cx) << 1) & 0x3e;
						t->tm_min  = (((int)reg_cx) >> 5) & 0x3f;
						t->tm_hour = (((int)reg_cx) >> 11) & 0x1f;
						t->tm_mday = (int)(reg_di) & 0x1f;
						t->tm_mon  = ((int)(reg_di >> 5) & 0x0f) - 1;
						t->tm_year = ((int)(reg_di >> 9) & 0x7f) + 80;
						ttime=mktime(t);
						LONGLONG ll = Int32x32To64(ttime, 10000000) + 116444736000000000 + (reg_bl==0x07?reg_si*100000:0);
						time.dwLowDateTime = (DWORD) ll;
						time.dwHighDateTime = (DWORD) (ll >> 32);
						if (!SetFileTime(hFile, reg_bl==0x07?&time:NULL,reg_bl==0x05?&time:NULL,reg_bl==0x03?&time:NULL)) {
							CloseHandle(hFile);
							CALLBACK_SCF(true);
							reg_ax=dos.errorcode;
							break;
						}
						CloseHandle(hFile);
						reg_ax=0;
						CALLBACK_SCF(false);
					} else
#endif
					{
						CALLBACK_SCF(true);
						reg_ax=dos.errorcode;
					}
					break;
				}
				case 0x04:
				case 0x06:
				case 0x08:
#if !defined(HX_DOS)
					struct stat status;
					if (DOS_GetFileAttrEx(name1, &status)) {
						const struct tm * ltime;
						time_t ttime=reg_bl==0x04?status.st_mtime:reg_bl==0x06?status.st_atime:status.st_ctime;
						if ((ltime=localtime(&ttime))!=0) {
							reg_cx=DOS_PackTime((Bit16u)ltime->tm_hour,(Bit16u)ltime->tm_min,(Bit16u)ltime->tm_sec);
							reg_di=DOS_PackDate((Bit16u)(ltime->tm_year+1900),(Bit16u)(ltime->tm_mon+1),(Bit16u)ltime->tm_mday);
						}
						if (reg_bl==0x08)
							reg_si = 0;
						reg_ax=0;
						CALLBACK_SCF(false);
					} else
#endif
					{
						CALLBACK_SCF(true);
						reg_ax=dos.errorcode;
					}
					break;
				default:
						E_Exit("DOS:Illegal LFN Attr call %2X",reg_bl);
		}
}

void DOS_Int21_7147(char *name1, const char *name2) {
    (void)name2;
		DOS_PSP psp(dos.psp());
		psp.StoreCommandTail();
		if (DOS_GetCurrentDir(reg_dl,name1,true)) {
				MEM_BlockWrite(SegPhys(ds)+reg_si,name1,(Bitu)(strlen(name1)+1));
				psp.RestoreCommandTail();
				reg_ax=0;
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_714e(char *name1, char *name2) {
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if (!DOS_GetSFNPath(name1,name2,false)) {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
			return;
		}
		Bit16u entry;
		Bit8u i,handle=(Bit8u)DOS_FILES;
		for (i=1;i<DOS_FILES;i++) {
			if (!Files[i]) {
				handle=i;
				break;
			}
		}
		if (handle==DOS_FILES) {
			reg_ax=DOSERR_TOO_MANY_OPEN_FILES;
			CALLBACK_SCF(true);
			return;
		}
		if (strlen(name2)>2&&name2[strlen(name2)-2]=='\\'&&name2[strlen(name2)-1]=='*')
			strcat(name2, ".*");
		lfn_filefind_handle=handle;
		bool b=DOS_FindFirst(name2,reg_cx,false);
		lfn_filefind_handle=LFN_FILEFIND_NONE;
		int error=dos.errorcode;
		Bit16u attribute = 0;
		if (!b&&!(strlen(name2)==3&&*(name2+1)==':'&&*(name2+2)=='\\')&&DOS_GetFileAttr(name2, &attribute) && (attribute&DOS_ATTR_DIRECTORY)) {
			strcat(name2,"\\*.*");
			lfn_filefind_handle=handle;
			b=DOS_FindFirst(name2,reg_cx,false);
			lfn_filefind_handle=LFN_FILEFIND_NONE;
			error=dos.errorcode;
		}
		if (b) {
				DOS_PSP psp(dos.psp());
				entry = psp.FindFreeFileEntry();
				if (entry==0xff) {
					reg_ax=DOSERR_TOO_MANY_OPEN_FILES;
					CALLBACK_SCF(true);
					return;
				}
				if (handle>=DOS_DEVICES||!Devices[handle])
					{
					int m=0;
					for (int i=1;i<DOS_DEVICES;i++)
						if (Devices[i]) m=i;
					Files[handle]=new DOS_Device(*Devices[m]);
					}
				else
					Files[handle]=new DOS_Device(*Devices[handle]);
				Files[handle]->AddRef();
				psp.SetFileHandle(entry,handle);
				reg_ax=handle;
				DOS_DTA dta(dos.dta());
				char finddata[CROSS_LEN];
				int c=0;
				MEM_BlockWrite(SegPhys(es)+reg_di,finddata,dta.GetFindData((int)reg_si,finddata,&c));
				reg_cx=c;
				CALLBACK_SCF(false);
		} else {
				dos.errorcode=error;
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_714f(const char *name1, const char *name2) {
    (void)name1;
    (void)name2;
		Bit8u handle=(Bit8u)reg_bx;
		if (!handle || handle>=DOS_FILES || !Files[handle]) {
			reg_ax=DOSERR_INVALID_HANDLE;
			CALLBACK_SCF(true);
			return;
		}
		lfn_filefind_handle=handle;
		if (DOS_FindNext()) {
				DOS_DTA dta(dos.dta());
				char finddata[CROSS_LEN];
				int c=0;
				MEM_BlockWrite(SegPhys(es)+reg_di,finddata,dta.GetFindData((int)reg_si,finddata,&c));
				reg_cx=c;
				CALLBACK_SCF(false);
				reg_ax=0x4f00+handle;
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
		lfn_filefind_handle=LFN_FILEFIND_NONE;
}

void DOS_Int21_7156(char *name1, char *name2) {
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		MEM_StrCopy(SegPhys(es)+reg_di,name2+1,DOSNAMEBUF);
		*name2='\"';
		p=name2+strlen(name2);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if (DOS_Rename(name1,name2)) {
				reg_ax=0;
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_7160(char *name1, char *name2) {
		MEM_StrCopy(SegPhys(ds)+reg_si,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if (DOS_Canonicalize(name1,name2)) {
				strcpy(name1,"\"");
				strcat(name1,name2);
				strcat(name1,"\"");
				switch(reg_cl)          {
						case 0:         // Canonoical path name
								strcpy(name2,name1);
								MEM_BlockWrite(SegPhys(es)+reg_di,name2,(Bitu)(strlen(name2)+1));
								reg_ax=0;
								CALLBACK_SCF(false);
								break;
						case 1:         // SFN path name
								if (DOS_GetSFNPath(name1,name2,false)) {
									MEM_BlockWrite(SegPhys(es)+reg_di,name2,(Bitu)(strlen(name2)+1));
									reg_ax=0;
									CALLBACK_SCF(false);
								} else {
									reg_ax=2;
									CALLBACK_SCF(true);
								}
								break;
						case 2:         // LFN path name
								if (DOS_GetSFNPath(name1,name2,true)) {
									MEM_BlockWrite(SegPhys(es)+reg_di,name2,(Bitu)(strlen(name2)+1));
									reg_ax=0;
									CALLBACK_SCF(false);
								} else {
									reg_ax=2;
									CALLBACK_SCF(true);
								}
								break;
						default:
								E_Exit("DOS:Illegal LFN GetName call %2X",reg_cl);
				}
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_716c(char *name1, const char *name2) {
    (void)name2;
		MEM_StrCopy(SegPhys(ds)+reg_si,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if (DOS_OpenFileExtended(name1,reg_bx,reg_cx,reg_dx,&reg_ax,&reg_cx)) {
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_71a0(char *name1, char *name2) {
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if (DOS_Canonicalize(name1,name2)) {
				if (reg_cx > 3)
						MEM_BlockWrite(SegPhys(es)+reg_di,"FAT",4);
				reg_ax=0;
				reg_bx=0x4006;
				reg_cx=0xff;
				reg_dx=0x104;
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_71a1(const char *name1, const char *name2) {
    (void)name1;
    (void)name2;
		Bit8u handle=(Bit8u)reg_bx;
		if (!handle || handle>=DOS_FILES || !Files[handle]) {
			reg_ax=DOSERR_INVALID_HANDLE;
			CALLBACK_SCF(true);
			return;
		}
		DOS_PSP psp(dos.psp());
		Bit16u entry=psp.FindEntryByHandle(handle);
		if (entry>0&&entry!=0xff) psp.SetFileHandle(entry,0xff);
		if (entry>0&&Files[handle]->RemoveRef()<=0) {
			delete Files[handle];
			Files[handle]=0;
		}
		reg_ax=0;
		CALLBACK_SCF(false);
}

void DOS_Int21_71a6(const char *name1, const char *name2) {
    (void)name1;
    (void)name2;
	char buf[64];
	unsigned long serial_number=0x1234,st=0,cdate=0,ctime=0,adate=0,atime=0,mdate=0,mtime=0;
	Bit8u entry=(Bit8u)reg_bx, handle;
	if (entry>=DOS_FILES) {
		reg_ax=DOSERR_INVALID_HANDLE;
		CALLBACK_SCF(true);
		return;
	}
	DOS_PSP psp(dos.psp());
	for (unsigned int i=0;i<=DOS_FILES;i++)
		if (Files[i] && psp.FindEntryByHandle(i)==entry)
			handle=i;
	if (handle < DOS_FILES && Files[handle] && Files[handle]->name!="") {
		struct stat status;
		if (DOS_GetFileAttrEx(Files[handle]->name.c_str(), &status, Files[handle]->GetDrive())) {
#if !defined(HX_DOS)
			time_t ttime;
			const struct tm * ltime;
			ttime=status.st_ctime;
			if ((ltime=localtime(&ttime))!=0) {
				ctime=DOS_PackTime((Bit16u)ltime->tm_hour,(Bit16u)ltime->tm_min,(Bit16u)ltime->tm_sec);
				cdate=DOS_PackDate((Bit16u)(ltime->tm_year+1900),(Bit16u)(ltime->tm_mon+1),(Bit16u)ltime->tm_mday);
			}
			ttime=status.st_atime;
			if ((ltime=localtime(&ttime))!=0) {
				atime=DOS_PackTime((Bit16u)ltime->tm_hour,(Bit16u)ltime->tm_min,(Bit16u)ltime->tm_sec);
				adate=DOS_PackDate((Bit16u)(ltime->tm_year+1900),(Bit16u)(ltime->tm_mon+1),(Bit16u)ltime->tm_mday);
			}
			ttime=status.st_mtime;
			if ((ltime=localtime(&ttime))!=0) {
				mtime=DOS_PackTime((Bit16u)ltime->tm_hour,(Bit16u)ltime->tm_min,(Bit16u)ltime->tm_sec);
				mdate=DOS_PackDate((Bit16u)(ltime->tm_year+1900),(Bit16u)(ltime->tm_mon+1),(Bit16u)ltime->tm_mday);
			}
#endif
			sprintf(buf,"%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s",(char*)&st,(char*)&ctime,(char*)&cdate,(char*)&atime,(char*)&adate,(char*)&mtime,(char*)&mdate,(char*)&serial_number,(char*)&st,(char*)&st,(char*)&st,(char*)&st,(char*)&handle);
			for (int i=32;i<36;i++) buf[i]=0;
			buf[36]=(char)((Bit32u)status.st_size%256);
			buf[37]=(char)(((Bit32u)status.st_size%65536)/256);
			buf[38]=(char)(((Bit32u)status.st_size%16777216)/65536);
			buf[39]=(char)((Bit32u)status.st_size/16777216);
			buf[40]=(char)status.st_nlink;
			for (int i=41;i<47;i++) buf[i]=0;
			buf[52]=0;
			MEM_BlockWrite(SegPhys(ds)+reg_dx,buf,53);
			reg_ax=0;
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
	} else {
		reg_ax=dos.errorcode;
		CALLBACK_SCF(true);
	}
}

void DOS_Int21_71a7(const char *name1, const char *name2) {
    (void)name1;
    (void)name2;
	switch (reg_bl) {
			case 0x00:
					reg_cl=mem_readb(SegPhys(ds)+reg_si);	//not yet a proper implementation,
					reg_ch=mem_readb(SegPhys(ds)+reg_si+1);	//but MS-DOS 7 and 4DOS DIR should
					reg_dl=mem_readb(SegPhys(ds)+reg_si+4);	//show date/time correctly now
					reg_dh=mem_readb(SegPhys(ds)+reg_si+5);
					reg_bh=0;
					reg_ax=0;
					CALLBACK_SCF(false);
					break;
			case 0x01:
					mem_writeb(SegPhys(es)+reg_di,reg_cl);
					mem_writeb(SegPhys(es)+reg_di+1,reg_ch);
					mem_writeb(SegPhys(es)+reg_di+4,reg_dl);
					mem_writeb(SegPhys(es)+reg_di+5,reg_dh);
					reg_ax=0;
					CALLBACK_SCF(false);
					break;
			default:
					E_Exit("DOS:Illegal LFN TimeConv call %2X",reg_bl);
	}
}

void DOS_Int21_71a8(char* name1, const char* name2) {
    (void)name2;
	if (reg_dh == 0 || reg_dh == 1) {
			MEM_StrCopy(SegPhys(ds)+reg_si,name1,DOSNAMEBUF);
			int i,j=0,o=0;
            char c[13];
            const char* s = strrchr(name1, '.');
			for (i=0;i<8;j++) {
					if (name1[j] == 0 || s-name1 <= j) break;
					if (name1[j] == '.') continue;
					c[o++] = toupper(name1[j]);
					i++;
			}
			if (s != NULL) {
					s++;
					if (s != 0 && reg_dh == 1) c[o++] = '.';
					for (i=0;i<3;i++) {
							if (*(s+i) == 0) break;
							c[o++] = toupper(*(s+i));
					}
			}
			assert(o <= 12);
			c[o] = 0;
			MEM_BlockWrite(SegPhys(es)+reg_di,c,strlen(c)+1);
			reg_ax=0;
			CALLBACK_SCF(false);
	} else {
			reg_ax=1;
			CALLBACK_SCF(true);
	}
}

void DOS_Int21_71aa(char* name1, const char* name2) {
    (void)name2;
	if (reg_bh<3 && (reg_bl<1 || reg_bl>26)) {
			reg_ax = DOSERR_INVALID_DRIVE;
			CALLBACK_SCF(true);
			return;
	}
	switch (reg_bh) {
		case 0:
		{
			Bit8u drive=reg_bl-1;
			if (drive==DOS_GetDefaultDrive() || Drives[drive] || drive==25) {
				reg_ax = DOSERR_INVALID_DRIVE;
				CALLBACK_SCF(true);
			} else {
				MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
				char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
				strcpy(mountstring,"MOUNT ");
				char temp_str[3] = { 0,0,0 };
				temp_str[0]=(char)('A'+reg_bl-1);
				temp_str[1]=' ';
				strcat(mountstring,temp_str);
				strcat(mountstring,name1);
				strcat(mountstring," >nul");
				DOS_Shell temp;
				temp.ParseLine(mountstring);
				if (Drives[drive]) {
					reg_ax=0;
					CALLBACK_SCF(false);
				} else {
					reg_ax=DOSERR_PATH_NOT_FOUND;
					CALLBACK_SCF(true);
				}
			}
			break;
		}
		case 1:
		{
			Bit8u drive=reg_bl-1;
			if (drive==DOS_GetDefaultDrive() || !Drives[drive] || drive==25) {
				reg_ax = DOSERR_INVALID_DRIVE;
				CALLBACK_SCF(true);
			} else {
				char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
				strcpy(mountstring,"MOUNT -u ");
				char temp_str[2] = { 0,0 };
				temp_str[0]=(char)('A'+reg_bl-1);
				strcat(mountstring,temp_str);
				strcat(mountstring," >nul");
				DOS_Shell temp;
				temp.ParseLine(mountstring);
				if (!Drives[drive]) {
					reg_ax =0;
					CALLBACK_SCF(false);
				} else {
					reg_ax=5;
					CALLBACK_SCF(true);
				}
			}
			break;
		}
		case 2:
		{
			Bit8u drive=reg_bl>0?reg_bl-1:DOS_GetDefaultDrive();
			if (Drives[drive]&&!strncmp(Drives[drive]->GetInfo(),"local directory ",16)) {
				strcpy(name1,Drives[drive]->GetInfo()+16);
				MEM_BlockWrite(SegPhys(ds)+reg_dx,name1,(Bitu)(strlen(name1)+1));
				reg_ax=0;
				CALLBACK_SCF(false);
			} else {
				reg_ax=3;
				CALLBACK_SCF(true);
			}
			break;
		}
		default:
			E_Exit("DOS:Illegal LFN Subst call %2X",reg_bh);
	}
}

static Bitu DOS_20Handler(void) {
	reg_ah=0x00;
	DOS_21Handler();
	return CBRET_NONE;
}

static Bitu DOS_27Handler(void) {
	// Terminate & stay resident
	Bit16u para = (reg_dx/16)+((reg_dx % 16)>0);
	Bit16u psp = dos.psp(); //mem_readw(SegPhys(ss)+reg_sp+2);
	if (DOS_ResizeMemory(psp,&para)) DOS_Terminate(psp,true,0);
	return CBRET_NONE;
}

static Bitu DOS_25Handler(void) {
	if (reg_al >= DOS_DRIVES || !Drives[reg_al] || Drives[reg_al]->isRemovable()) {
		reg_ax = 0x8002;
		SETFLAGBIT(CF,true);
	} else {
		if (reg_cx == 1 && reg_dx == 0) {
			if (reg_al >= 2) {
				PhysPt ptr = PhysMake(SegValue(ds),reg_bx);
				// write some BPB data into buffer for MicroProse installers
				mem_writew(ptr+0x1c,0x3f); // hidden sectors
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
	LOG(LOG_DOSMISC,LOG_NORMAL)("int 26 called: hope for the best!");
	if (reg_al >= DOS_DRIVES || !Drives[reg_al] || Drives[reg_al]->isRemovable()) {	
		reg_ax = 0x8002;
		SETFLAGBIT(CF,true);
	} else {
		SETFLAGBIT(CF,false);
		reg_ax = 0;
	}
    return CBRET_NONE;
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

		callback[5].Install(NULL,CB_IRET,"DOS Int 28");
		callback[5].Set_RealVec(0x28);

		callback[6].Install(NULL,CB_INT29,"CON Output Int 29");
		callback[6].Set_RealVec(0x29);
		// pseudocode for CB_INT29:
		//	push ax
		//	mov ah, 0x0e
		//	int 0x10
		//	pop ax
		//	iret

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
		if (!strcmp(section->Get_string("lfn"), "true")) enablelfn=1;
		else if (!strcmp(section->Get_string("lfn"), "false")) enablelfn=0;
		else if (!strcmp(section->Get_string("lfn"), "autostart")) enablelfn=-2;
		else enablelfn=-1;

		std::string ver = section->Get_string("ver");
		if (!ver.empty()) {
			const char *s = ver.c_str();

			if (isdigit(*s)) {
				dos.version.minor=0;
				dos.version.major=(int)strtoul(s,(char**)(&s),10);
				if (*s == '.' || *s == ' ') {
					s++;
					if (isdigit(*s))
						dos.version.minor=(*(s-1)=='.'&&strlen(s)==1?10:1)*(int)strtoul(s,(char**)(&s),10);
				}
			}
		}
		uselfn = enablelfn==1 || ((enablelfn == -1 || enablelfn == -2) && dos.version.major>6);
	}
	~DOS(){
		for (Bit16u i=0;i<DOS_DRIVES;i++) delete Drives[i];
	}
};

static DOS* test;

void DOS_ShutDown(Section* /*sec*/) {
	delete test;
}

void DOS_Init(Section* sec) {
	test = new DOS(sec);
	/* shutdown function */
	sec->AddDestroyFunction(&DOS_ShutDown,false);
}
