/*
 *  Copyright (C) 2002  The DOSBox Team
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "dosbox.h"
#include "bios.h"
#include "mem.h"
#include "callback.h"
#include "regs.h"
#include "dos_inc.h"

DOS_Block dos;
DOS_InfoBlock dosInfoBlock;

static Bit8u dos_copybuf[0x10000];
static Bitu call_20,call_21,call_theend;

void DOS_SetError(Bit16u code) {
	dos.errorcode=code;
};


#define DOSNAMEBUF 256
static Bitu DOS_21Handler(void) {
//TODO KEYBOARD	Check for break
	char name1[DOSNAMEBUF+1];
	char name2[DOSNAMEBUF+1];
	switch (reg_ah) {
	case 0x00:		/* Terminate Program */
		E_Exit("DOS:Unhandled call %02X",reg_ah);
		break;
	case 0x01:		/* Read character from STDIN, with echo */
		{	
			Bit8u c;Bit16u n=1;
			DOS_ReadFile(STDIN,&c,&n);
			reg_al=c;
			DOS_WriteFile(STDOUT,&c,&n);
		}
		break;
	case 0x02:		/* Write character to STDOUT */
		{
			Bit8u c=reg_dl;Bit16u n=1;
			DOS_WriteFile(STDOUT,&c,&n);
		}
		break;
	case 0x03:		/* Read character from STDAUX */
	case 0x04:		/* Write Character to STDAUX */
	case 0x05:		/* Write Character to PRINTER */
		E_Exit("DOS:Unhandled call %02X",reg_ah);
		break;
	case 0x06:		/* Direct Console Output / Input */
		switch (reg_dl) {
		case 0xFF:	/* Input */
			{	
//TODO Make this better according to standards
				if (!DOS_GetSTDINStatus()) {
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
				Bit8u c=reg_dl;Bit16u n=1;
				DOS_WriteFile(STDOUT,&c,&n);
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
	case 0x08:		/* Direct Character Input, without echo */
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
		}
		break;
	case 0x0a:		/* Buffered Input */
		{
			//TODO ADD Break checkin in STDIN but can't care that much for it
			PhysPt data=SegPhys(ds)+reg_dx;
			Bit8u free=mem_readb(data);
			Bit8u read=0;Bit8u c;Bit16u n=1;
			if (!free) break;
			while (read<free) {
				DOS_ReadFile(STDIN,&c,&n);
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
		if (DOS_GetSTDINStatus()) reg_al=0xff;
		else reg_al=0;
		break;
	case 0x0c:		/* Flush Buffer and read STDIN call */
		{
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
				LOG_ERROR("DOS:0C:Illegal Flush STDIN Buffer call %d",reg_al);
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
		reg_al=26;
		break;
	case 0x0f:		/* Open File using FCB */
		if(DOS_FCBOpen(SegValue(ds),reg_dx)){
			reg_al=0;
		}else{
			reg_al=0xff;
		}
		LOG_DEBUG("DOS:0x0f FCB-fileopen used, result:al=%d",reg_al);
		break;

	case 0x10:		/* Close File using FCB */
		if(DOS_FCBClose(SegValue(ds),reg_dx)){
		reg_al=0;
		}else{
			reg_al=0xff;
		}
		LOG_DEBUG("DOS:0x10 FCB-fileclose used, result:al=%d",reg_al);
		break;

	case 0x11:		/* Find First Matching File using FCB */
		if(DOS_FCBFindFirst(SegValue(ds),reg_dx)){
		reg_al=0;
		}else{
			reg_al=0xff;
		}
		LOG_DEBUG("DOS:0x11 FCB-FindFirst used, result:al=%d",reg_al);
		break;

	case 0x12:		/* Find Next Matching File using FCB */
		if(DOS_FCBFindNext(SegValue(ds),reg_dx)){
		reg_al=0;
		}else{
			reg_al=0xff;
		}
		LOG_DEBUG("DOS:0x12 FCB-FindNext used, result:al=%d",reg_al);
		break;

	case 0x13:		/* Delete File using FCB */
		if (DOS_FCBDeleteFile(SegValue(ds),reg_dx)) reg_al = 0x00;
		else reg_al = 0xFF;
		break;
	case 0x14:		/* Sequential read from FCB */
		reg_al = DOS_FCBRead(SegValue(ds),reg_dx,0);
		LOG_DEBUG("DOS:0x14 FCB-Read used, result:al=%d",reg_al);
		break;
	case 0x15:		/* Sequential write to FCB */
		if (DOS_FCBWrite(SegValue(ds),reg_dx,0)) reg_al = 0x00;
		else reg_al = 0x01;
		LOG_DEBUG("DOS:0x15 FCB-Write used, result:al=%d",reg_al);
		break;
	case 0x16:		/* Create or truncate file using FCB */
		if (DOS_FCBCreate(SegValue(ds),reg_dx)) reg_al = 0x00;
		else reg_al = 0x01;
		LOG_DEBUG("DOS:0x16 FCB-Create used, result:al=%d",reg_al);
		break;
	case 0x17:		/* Rename file using FCB */		
		if (DOS_FCBRenameFile(SegValue(ds),reg_dx)) reg_al = 0x00;
		else reg_al = 0xFF;
		break;
	case 0x21:		/* Read random record from FCB */
		{	DOS_FCB fcb(SegValue(ds),reg_dx);
			Bit8u curRec = fcb.Get_current_record();
			Bit16u curBlock = fcb.Get_current_block();
			reg_al = DOS_FCBRead(SegValue(ds),reg_dx,0);
			fcb.Set_current_record(curRec);
			fcb.Set_current_block(curBlock);
		}
		LOG_DEBUG("DOS:0x21 FCB-Random read used, result:al=%d",reg_al);
		break;
	case 0x22:		/* Write random record to FCB */
		{	DOS_FCB fcb(SegValue(ds),reg_dx);
			Bit8u curRec = fcb.Get_current_record();
			Bit16u curBlock = fcb.Get_current_block();
			if (DOS_FCBRandomWrite(SegValue(ds),reg_dx,reg_cx)) reg_al = 0x00;
			else reg_al = 0x01;
			fcb.Set_current_record(curRec);
			fcb.Set_current_block(curBlock);		
		}
		LOG_DEBUG("DOS:0x28 FCB-Random write used, result:al=%d",reg_al);
		break;
	case 0x23:		/* Get file size for FCB */
		if (DOS_FCBGetFileSize(SegValue(ds),reg_dx,reg_cx)) reg_al = 0x00;
		else reg_al = 0xFF;
		break;
	case 0x24:		/* Set Random Record number for FCB */
		{	DOS_FCB fcb(SegValue(ds),reg_dx);
			fcb.Set_random_record(fcb.Get_current_block()*128+fcb.Get_current_record());
		} break;
	case 0x27:		/* Random block read from FCB */
		reg_al = DOS_FCBRandomRead(SegValue(ds),reg_dx,reg_cx);
		LOG_DEBUG("DOS:0x27 FCB-Random read used, result:al=%d",reg_al);
		break;
	case 0x28:		/* Random Block write to FCB */
		if (DOS_FCBRandomWrite(SegValue(ds),reg_dx,reg_cx)) reg_al = 0x00;
		else reg_al = 0x01;
		LOG_DEBUG("DOS:0x28 FCB-Random write used, result:al=%d",reg_al);
		break;

	case 0x29:		/* Parse filename into FCB */
        {   Bit8u difference;
            char string[1024];
            MEM_StrCopy(SegPhys(ds)+reg_si,string,1024);
            reg_al=FCB_Parsename(SegValue(es),reg_di,reg_al ,string, &difference);
            reg_di+=difference;
        }
		LOG_DEBUG("DOS:29:FCB Parse Filename, result:al=%d",reg_al);
        break;

	case 0x18:		/* NULL Function for CP/M compatibility or Extended rename FCB */
	case 0x1d:		/* NULL Function for CP/M compatibility or Extended rename FCB */
	case 0x1e:		/* NULL Function for CP/M compatibility or Extended rename FCB */
	case 0x20:		/* NULL Function for CP/M compatibility or Extended rename FCB */
	case 0x6b:		/* NULL Function */
	case 0x61:		/* UNUSED */
		reg_al=0;
		break;		
	case 0x19:		/* Get current default drive */
		reg_al=DOS_GetDefaultDrive();
		break;
	case 0x1a:		/* Set Disk Transfer Area Address */
		dos.dta=RealMakeSeg(ds,reg_dx);
		break;

	case 0x1c:		/* Get allocation info for specific drive */
		LOG_DEBUG("DOS: Allocation Info call not supported correctly");
		SegSet16(ds,0xf000);
		reg_bx=0;
		real_writeb(0xf000,0,0);
		reg_al=0x7f;
		reg_cx=0x200;
		reg_dx=0x1000;
		break;		/* TODO maybe but hardly think a game needs this */
	case 0x1b:		/* Get allocation info for default drive */	
		LOG_DEBUG("DOS: Allocation Info call not supported correctly");
		SegSet16(ds,0xf000);
		reg_bx=0;
		real_writeb(0xf000,0,0);
		reg_al=0x7f;
		reg_cx=0x200;
		reg_dx=0x1000;
		break;
	case 0x1f:		/* Get drive parameter block for default drive */
	case 0x32:		/* Get drive parameter block for specific drive */
		E_Exit("DOS:Unhandled call %02X",reg_ah);
		break;		/* TODO maybe but hardly think a game needs this */
	case 0x25:		/* Set Interrupt Vector */
		RealSetVec(reg_al,RealMakeSeg(ds,reg_dx));
		break;
	case 0x26:		/* Create new PSP */
		DOS_NewPSP(reg_dx,DOS_PSP(dos.psp).GetSize());
		break;
	case 0x2a:		/* Get System Date */
		reg_al=0;	/* It's always sunday TODO find that correct formula */
		reg_cx=dos.date.year;
		reg_dh=dos.date.month;
		reg_dl=dos.date.day;
		break;
	case 0x2b:		/* Set System Date */
//TODO Check for months with less then 31 days 
		if (reg_cx<1980) { reg_al=0xff;break;}
		if ((reg_dh>12) || (reg_dh==0))	{ reg_al=0xff;break;}
		if ((reg_dl>31) || (reg_dl==0))	{ reg_al=0xff;break;}
		dos.date.year=reg_cx;
		dos.date.month=reg_dh;
		dos.date.day=reg_dl;
		reg_al=0;
		break;
	case 0x2c:		/* Get System Time */
//TODO Get time through bios calls date is fixed
		{
			Bit32u ticks=mem_readd(BIOS_TIMER);
			Bit32u seconds=(ticks*10)/182;
			reg_ch=(Bit8u)(seconds/3600);
			reg_cl=(Bit8u)(seconds % 3600)/60;
			reg_dh=(Bit8u)(seconds % 60);
			reg_dl=(Bit8u)(ticks % 19)*5;
		}
		break;
	case 0x2d:		/* Set System Time */
		LOG_DEBUG("DOS:Set System Time not supported");
		reg_al=0;	/* Noone is changing system time */
		break;
	case 0x2e:		/* Set Verify flag */
		dos.verify=(reg_al==1);
		break;
	case 0x2f:		/* Get Disk Transfer Area */
		SegSet16(es,RealSeg(dos.dta));
		reg_bx=RealOff(dos.dta);
		break;
	case 0x30:		/* Get DOS Version */
		if (reg_al==0) reg_bh=0xFF;		/* Fake Microsoft DOS */
		if (reg_al==1) reg_bh=0x10;		/* DOS is in HMA */
		reg_al=dos.version.major;
		reg_ah=dos.version.minor;
		break;
	case 0x31:		/* Terminate and stay resident */
//TODO First get normal files executing
		DOS_ResizeMemory(dos.psp,&reg_dx);
		if (DOS_Terminate(true)) {
			dos.return_code=reg_al;
			dos.return_mode=RETURN_TSR;
			CALLBACK_SCF(false);
		} else {            
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x33:		/* Extended Break Checking */
		switch (reg_al) {
			case 0:reg_dl=dos.breakcheck;break;			/* Get the breakcheck flag */
			case 1:dos.breakcheck=(reg_dl>0);break;		/* Set the breakcheck flag */
			case 2:{bool old=dos.breakcheck;dos.breakcheck=(reg_dl>0);reg_dl=old;}break;
			case 5:reg_dl=3;break;							/* Always boot from c: :) */
			case 6:											/* Get true version number */
				reg_bl=dos.version.major;
				reg_bh=dos.version.minor;
				reg_dl=dos.version.revision;
				reg_dh=0x10;								/* Dos in HMA */
				break;
			default:
				E_Exit("DOS:Illegal 0x33 Call %2X",reg_al);					
		}
		break;
	case 0x34:		/* Get INDos Flag */
		SegSet16(es,RealSeg(dos.tables.indosflag));
		reg_bx=RealOff(dos.tables.indosflag);
		break;
	case 0x35:		/* Get interrupt vector */
		reg_bx=real_readw(0,((Bit16u)reg_al)*4);
		SegSet16(es,real_readw(0,((Bit16u)reg_al)*4+2));
		break;
	case 0x36:		/* Get Free Disk Space */
		{
			Bit16u bytes,sectors,clusters,free;
			if	(DOS_GetFreeDiskSpace(reg_dl,&bytes,&sectors,&clusters,&free)) {
				reg_ax=sectors;
				reg_bx=free;
				reg_cx=bytes;
				reg_dx=clusters;
			} else {
				reg_ax=0xffff;
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
		LOG_DEBUG("DOS:0x37:Call for not supported switchchar");
		break;
	case 0x38:					/* Set Country Code */	
		LOG_DEBUG("DOS:Setting country code not supported");
		CALLBACK_SCF(true);
		break;
		if (reg_al==0) {		/* Get country specidic information */

		} else {				/* Set country code */
			

		}
		break;
	case 0x39:		/* MKDIR Create directory */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if (DOS_MakeDir(name1)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x3a:		/* RMDIR Remove directory */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if  (DOS_RemoveDir(name1)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x3b:		/* CHDIR Set current directory */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if  (DOS_ChangeDir(name1)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x3c:		/* CREATE Create of truncate file */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if (DOS_CreateFile(name1,reg_cx,&reg_ax)) {
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
		if (DOS_CloseFile(reg_bx)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x3f:		/* READ Read from file or device */
		{ 
			Bit16u toread=reg_cx;
			if (DOS_ReadFile(reg_bx,dos_copybuf,&toread)) {
				MEM_BlockWrite(SegPhys(ds)+reg_dx,dos_copybuf,toread);
				reg_ax=toread;
				CALLBACK_SCF(false);
			} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
			}
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
//TODO FIX THIS HACK
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		switch (reg_al)
		case 0x00:				/* Get */
		{
			if (DOS_GetFileAttr(name1,&reg_cx)) {
				CALLBACK_SCF(false);
			} else {
				CALLBACK_SCF(true);
				reg_ax=dos.errorcode;
			}
			break;
		case 0x01:				/* Set */
			LOG_DEBUG("DOS:Set File Attributes for %s not supported",name1);
			CALLBACK_SCF(false);
			break;
		default:
			E_Exit("DOS:0x43:Illegal subfunction %2X",reg_al);
		}
		break;
	case 0x44:					/* IOCTL Functions */
		if (DOS_IOCTL(reg_al,reg_bx)) {
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
		if (DOS_ForceDuplicateEntry(reg_bx,reg_ax)) {
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
		break;
	case 0x47:					/* CWD Get current directory */
		//TODO Memory
		if (DOS_GetCurrentDir(reg_dl,name1)) {
			MEM_BlockWrite(SegPhys(ds)+reg_si,name1,strlen(name1)+1);	
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
			if (DOS_Execute(name1,(ParamBlock *)Phys2Host(SegPhys(es)+reg_bx),reg_al)) {
				CALLBACK_SCF(false);
			} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
			}
		}
		break;
//TODO Check for use of execution state AL=5
	case 0x4c:					/* EXIT Terminate with return code */
		{
			if (DOS_Terminate(false)) {
				dos.return_code=reg_al;
				dos.return_mode=RETURN_EXIT;
				CALLBACK_SCF(false);
			} else {            
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
			}
			break;
		}
	case 0x4d:					/* Get Return code */
		reg_al=dos.return_code;
		reg_ah=dos.return_mode;
		break;
	case 0x4e:					/* FINDFIRST Find first matching file */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if (DOS_FindFirst(name1,reg_cx)) {
			CALLBACK_SCF(false);			
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		};
		break;		 
	case 0x4f:					/* FINDNEXT Find next matching file */
		if (DOS_FindNext()) {
			CALLBACK_SCF(false);			
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		};
		break;		
	case 0x50:					/* Set current PSP */
		dos.psp=reg_bx;
		break;
	case 0x51:					/* Get current PSP */
		reg_bx=dos.psp;
		break;
	case 0x52: {				/* Get list of lists */
		Bit16u seg;
		dosInfoBlock.GetDIBPointer(seg,reg_bx);
		SegSet16(es,seg);
		LOG_DEBUG("Call is made for list of lists - let's hope for the best");
		break; }
//TODO Think hard how shit this is gonna be
//And will any game ever use this :)
	case 0x53:					/* Translate BIOS parameter block to drive parameter block */
//YEAH RIGHT
	case 0x54:					/* Get verify flag */
		E_Exit("Unhandled Dos 21 call %02X",reg_ah);
		break;
	case 0x55:					/* Create Child PSP*/
		DOS_NewPSP(reg_dx,reg_si);
		dos.psp = reg_dx;
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
		reg_cx=0;
		reg_dx=0;
		LOG_DEBUG("DOS:57:Getting/Setting File Date is faked",reg_ah);
		break;
	case 0x58:					/* Get/Set Memory allocation strategy */
		LOG_DEBUG("DOS:58:Not Supported Set//Get memory allocation");
		break;
	case 0x59:					/* Get Extended error information */
		reg_ax=dos.errorcode;
		reg_bh=0;	//Unkown error class
		reg_bl=1;	//Retry retry retry
		reg_ch=0;	//Unkown error locus
		break;
	case 0x5a:					/* Create temporary file */
		{
			Bit16u handle;
			MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
			if (DOS_CreateTempFile(name1,&handle)) {
				reg_ax=handle;
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
				DOS_SetError(DOSERR_ACCESS_DENIED);
				reg_ax=dos.errorcode;
				CALLBACK_SCF(false);
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
	case 0x5c:					/* FLOCK File region locking */
	case 0x5d:					/* Network Functions */
	case 0x5e:					/* More Network Functions */
	case 0x5f:					/* And Even More Network Functions */
		E_Exit("DOS:Unhandled call %02X",reg_ah);
		break;
	case 0x60:					/* Canonicalize filename or path */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
		if (DOS_Canonicalize(name1,name2)) {
				MEM_BlockWrite(SegPhys(es)+reg_di,name2,strlen(name2)+1);	
				CALLBACK_SCF(false);
			} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
			}
			break;
	case 0x62:					/* Get Current PSP Address */
		reg_bx=dos.psp;
		break;
	case 0x63:					/* Weirdo double byte stuff */
		reg_al=0xff;
		LOG_WARN("DOS:0x63:Doubly byte characters not supported");
		break;
	case 0x64:					/* Set device driver lookahead flag */
		E_Exit("Unhandled Dos 21 call %02X",reg_ah);
		break;
	case 0x65:					/* Get extented country information and a lot of other useless shit*/
		/* Todo maybe fully support this for now we set it standard for USA */
		{
			LOG_DEBUG("DOS:65:Extended country information call");
			PhysPt data=SegPhys(es)+reg_di;
			switch (reg_al) {
			case 1:
				mem_writeb(data,reg_al);
				mem_writew(data+1,4);
				mem_writew(data+3,1);				
				mem_writew(data+5,37);
				reg_cx=4;
				CALLBACK_SCF(false);
				break;
			 default:
				E_Exit("DOS:0x65:Unhandled country information call %2X",reg_al);	
			};
			break;
		}
	case 0x66:					/* Get/Set global code page table  */
		if (reg_al==1) {
			LOG_DEBUG("Getting global code page table");
			reg_bx=reg_dx=437;
			CALLBACK_SCF(false);
			break;
		}
		LOG_ERROR("DOS:Setting code page table is not supported");
		break;
	case 0x67:					/* Set handle countr */
		/* Weird call to increase amount of file handles needs to allocate memory if >20 */
		LOG_DEBUG("DOS:67:Set Handle Count not working");
		CALLBACK_SCF(false);
		break;
	case 0x68:					/* FFLUSH Commit file */
		E_Exit("Unhandled Dos 21 call %02X",reg_ah);
		break;
	case 0x69:					/* Get/Set disk serial number */
		{
			switch(reg_al)		{
			case 0x00:				/* Get */
				LOG_DEBUG("DOS:Get Disk serial number");
				CALLBACK_SCF(true);
				break;
			case 0x01:
				LOG_DEBUG("DOS:Set Disk serial number");
			default:
				E_Exit("DOS:Illegal Get Serial Number call %2X",reg_al);
			}	
			break;
		} 
	case 0x6c:					/* Extended Open/Create */
		E_Exit("Unhandled Dos 21 call %02X",reg_ah);
		break;
	case 0x71:					/* Unknown probably 4dos detection */
		reg_ax=0x7100;
		CALLBACK_SCF(true);
		LOG_WARN("DOS:Windows long file name support call %2X",reg_al);
		break;
	case 0xE0:
		LOG_DEBUG("DOS:E0:Unhandled, what should this call do?");
		break;
	default:
		E_Exit("DOS:Unhandled call %02X",reg_ah);
		break;
	};
	return CBRET_NONE;
/* That's it now let's get it working */
};



static Bitu DOS_20Handler(void) {

	reg_ax=0x4c00;
	DOS_21Handler();
	return CBRET_NONE;
}


void DOS_Init(void) {
	call_20=CALLBACK_Allocate();
	CALLBACK_Setup(call_20,DOS_20Handler,CB_IRET);
	RealSetVec(0x20,CALLBACK_RealPointer(call_20));

	call_21=CALLBACK_Allocate();
	CALLBACK_Setup(call_21,DOS_21Handler,CB_IRET);
	RealSetVec(0x21,CALLBACK_RealPointer(call_21));

	DOS_SetupFiles();								/* Setup system File tables */
	DOS_SetupDevices();							/* Setup dos devices */
	DOS_SetupMemory();								/* Setup first MCB */
	DOS_SetupTables();
	DOS_SetupPrograms();
	DOS_SetupMisc();							/* Some additional dos interrupts */
	DOS_SetDefaultDrive(25);
	/* Execute the file that should be */
	dos.version.major=5;
	dos.version.minor=0;
//	DOS_RunProgram(startname);
};
