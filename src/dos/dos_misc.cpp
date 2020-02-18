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
 *
 *  Wengier: MS-DOS 7 support
 */


#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"
#include <list>


static Bitu call_int2f,call_int2a;

static std::list<MultiplexHandler*> Multiplex;
typedef std::list<MultiplexHandler*>::iterator Multiplex_it;

void DOS_AddMultiplexHandler(MultiplexHandler * handler) {
	Multiplex.push_front(handler);
}

void DOS_DelMultiplexHandler(MultiplexHandler * handler) {
	for(Multiplex_it it =Multiplex.begin();it != Multiplex.end();it++) {
		if(*it == handler) {
			Multiplex.erase(it);
			return;
		}
	}
}

static Bitu INT2F_Handler(void) {
	for(Multiplex_it it = Multiplex.begin();it != Multiplex.end();it++)
		if( (*it)() ) return CBRET_NONE;
   
	LOG(LOG_DOSMISC,LOG_ERROR)("DOS:Multiplex Unhandled call %4X",reg_ax);
	return CBRET_NONE;
}


static Bitu INT2A_Handler(void) {
	return CBRET_NONE;
}

static bool DOS_MultiplexFunctions(void) {
	char name[256];
	switch (reg_ax) {
	case 0x1216:	/* GET ADDRESS OF SYSTEM FILE TABLE ENTRY */
		// reg_bx is a system file table entry, should coincide with
		// the file handle so just use that
		LOG(LOG_DOSMISC,LOG_ERROR)("Some BAD filetable call used bx=%X",reg_bx);
		if(reg_bx <= DOS_FILES) CALLBACK_SCF(false);
		else CALLBACK_SCF(true);
		if (reg_bx<16) {
			RealPt sftrealpt=mem_readd(Real2Phys(dos_infoblock.GetPointer())+4);
			PhysPt sftptr=Real2Phys(sftrealpt);
			Bitu sftofs=0x06+reg_bx*0x3b;

			if (Files[reg_bx]) mem_writeb(sftptr+sftofs,Files[reg_bx]->refCtr);
			else mem_writeb(sftptr+sftofs,0);

			if (!Files[reg_bx]) return true;

			Bit32u handle=RealHandle(reg_bx);
			if (handle>=DOS_FILES) {
				mem_writew(sftptr+sftofs+0x02,0x02);	// file open mode
				mem_writeb(sftptr+sftofs+0x04,0x00);	// file attribute
				mem_writew(sftptr+sftofs+0x05,Files[reg_bx]->GetInformation());	// device info word
				mem_writed(sftptr+sftofs+0x07,0);		// device driver header
				mem_writew(sftptr+sftofs+0x0d,0);		// packed time
				mem_writew(sftptr+sftofs+0x0f,0);		// packed date
				mem_writew(sftptr+sftofs+0x11,0);		// size
				mem_writew(sftptr+sftofs+0x15,0);		// current position
			} else {
				Bit8u drive=Files[reg_bx]->GetDrive();

				mem_writew(sftptr+sftofs+0x02,(Bit16u)(Files[reg_bx]->flags&3));	// file open mode
				mem_writeb(sftptr+sftofs+0x04,(Bit8u)(Files[reg_bx]->attr));		// file attribute
				mem_writew(sftptr+sftofs+0x05,0x40|drive);							// device info word
				mem_writed(sftptr+sftofs+0x07,RealMake(dos.tables.dpb,drive*9));	// dpb of the drive
				mem_writew(sftptr+sftofs+0x0d,Files[reg_bx]->time);					// packed file time
				mem_writew(sftptr+sftofs+0x0f,Files[reg_bx]->date);					// packed file date
				Bit32u curpos=0;
				Files[reg_bx]->Seek(&curpos,DOS_SEEK_CUR);
				Bit32u endpos=0;
				Files[reg_bx]->Seek(&endpos,DOS_SEEK_END);
				mem_writed(sftptr+sftofs+0x11,endpos);		// size
				mem_writed(sftptr+sftofs+0x15,curpos);		// current position
				Files[reg_bx]->Seek(&curpos,DOS_SEEK_SET);
			}

			// fill in filename in fcb style
			// (space-padded name (8 chars)+space-padded extension (3 chars))
			const char* filename=(const char*)Files[reg_bx]->GetName();
			if (strrchr(filename,'\\')) filename=strrchr(filename,'\\')+1;
			if (strrchr(filename,'/')) filename=strrchr(filename,'/')+1;
			if (!filename) return true;
			const char* dotpos=strrchr(filename,'.');
			if (dotpos) {
				dotpos++;
				size_t nlen=strlen(filename);
				size_t extlen=strlen(dotpos);
				Bits nmelen=(Bits)nlen-(Bits)extlen;
				if (nmelen<1) return true;
				nlen-=(extlen+1);

				if (nlen>8) nlen=8;
				size_t i;

				for (i=0; i<nlen; i++)
					mem_writeb((PhysPt)(sftptr+sftofs+0x20+i),filename[i]);
				for (i=nlen; i<8; i++)
					mem_writeb((PhysPt)(sftptr+sftofs+0x20+i),' ');
				
				if (extlen>3) extlen=3;
				for (i=0; i<extlen; i++)
					mem_writeb((PhysPt)(sftptr+sftofs+0x28+i),dotpos[i]);
				for (i=extlen; i<3; i++)
					mem_writeb((PhysPt)(sftptr+sftofs+0x28+i),' ');
			} else {
				size_t i;
				size_t nlen=strlen(filename);
				if (nlen>8) nlen=8;
				for (i=0; i<nlen; i++)
					mem_writeb((PhysPt)(sftptr+sftofs+0x20+i),filename[i]);
				for (i=nlen; i<11; i++)
					mem_writeb((PhysPt)(sftptr+sftofs+0x20+i),' ');
			}

			SegSet16(es,RealSeg(sftrealpt));
			reg_di=RealOff(sftrealpt+sftofs);
			reg_ax=0xc000;

		}
		return true;
	case 0x1607:
		if (reg_bx == 0x15) {
			switch (reg_cx) {
				case 0x0000:		// query instance
					reg_cx = 0x0001;
					reg_dx = 0x50;		// dos driver segment
					SegSet16(es,0x50);	// patch table seg
					reg_bx = 0x60;		// patch table ofs
					return true;
				case 0x0001:		// set patches
					reg_ax = 0xb97c;
					reg_bx = (reg_dx & 0x16);
					reg_dx = 0xa2ab;
					return true;
				case 0x0003:		// get size of data struc
					if (reg_dx==0x0001) {
						// CDS size requested
						reg_ax = 0xb97c;
						reg_dx = 0xa2ab;
						reg_cx = 0x000e;	// size
					}
					return true;
				case 0x0004:		// instanced data
					reg_dx = 0;		// none
					return true;
				case 0x0005:		// get device driver size
					reg_ax = 0;
					reg_dx = 0;
					return true;
				default:
					return false;
			}
		}
		else if (reg_bx == 0x18) return true;	// idle callout
		else return false;
	case 0x1680:	/*  RELEASE CURRENT VIRTUAL MACHINE TIME-SLICE */
		//TODO Maybe do some idling but could screw up other systems :)
		return true; //So no warning in the debugger anymore
	case 0x1689:	/*  Kernel IDLE CALL */
	case 0x168f:	/*  Close awareness crap */
	   /* Removing warning */
		return true;
#ifdef WIN32
	case 0xb800:																	// Network - installation check
		reg_al = 1;																	// Installed
		reg_bx = 8;																	// Bit 3 - redirector
		return true;
	case 0xb809:																	// Network - get version
		reg_ax = 0x0201;															// Major-minor version as returned by NTVDM-Windows XP
		return true;
	case 0x1700:
		reg_al = 1;
		reg_ah = 1;
		return true;
	case 0x1701:
		reg_ax=OpenClipboard(NULL)?1:0;
		return true;
	case 0x1702:
		reg_ax=0;
		if (OpenClipboard(NULL))
			{
			reg_ax=EmptyClipboard()?1:0;
			CloseClipboard();
			}
		return true;
	case 0x1703:
		reg_ax=0;
		if ((reg_dx==1||reg_dx==7)&&OpenClipboard(NULL))
			{
			char *text, *buffer;
			text = new char[reg_cx];
			MEM_StrCopy(SegPhys(es)+reg_bx,text,reg_cx);
			*(text+reg_cx-1)=0;
			HGLOBAL clipbuffer;
			EmptyClipboard();
			clipbuffer = GlobalAlloc(GMEM_DDESHARE, strlen(text)+1);
			buffer = (char*)GlobalLock(clipbuffer);
			strcpy(buffer, text);
			delete[] text;
			GlobalUnlock(clipbuffer);
			SetClipboardData(reg_dx==1?CF_TEXT:CF_OEMTEXT,clipbuffer);
			reg_ax++;
			CloseClipboard();
			}
		return true;
	case 0x1704:
		reg_ax=0;
		if ((reg_dx==1||reg_dx==7)&&OpenClipboard(NULL))
			{
			if (HANDLE text = GetClipboardData(reg_dx==1?CF_TEXT:CF_OEMTEXT))
				{
				reg_ax=(Bit16u)strlen((char *)text)+1;
				reg_dx=(Bit16u)((strlen((char *)text)+1)/65536);
				}
			CloseClipboard();
			}
		return true;
	case 0x1705:
		reg_ax=0;
		if ((reg_dx==1||reg_dx==7)&&OpenClipboard(NULL))
			{
			if (HANDLE text = GetClipboardData(reg_dx==1?CF_TEXT:CF_OEMTEXT))
				{
				MEM_BlockWrite(SegPhys(es)+reg_bx,text,(Bitu)(strlen((char *)text)+1));
				reg_ax++;
				}
			CloseClipboard();
			}
		return true;
	case 0x1708:
		reg_ax=CloseClipboard()?1:0;
		return true;
#endif
	case 0x4a01:	/* Query free hma space */
	case 0x4a02:	/* ALLOCATE HMA SPACE */
		LOG(LOG_DOSMISC,LOG_WARN)("INT 2f:4a HMA. DOSBox reports none available.");
		reg_bx=0;	//number of bytes available in HMA or amount successfully allocated
		//ESDI=ffff:ffff Location of HMA/Allocated memory
		SegSet16(es,0xffff);
		reg_di=0xffff;
		return true;
	case 0x1300:
	case 0x1302:
		reg_ax=0;
		return true;
	case 0x1605:
		return true;
	case 0x1612:
		reg_ax=0;
		name[0]=1;
		name[1]=0;
		MEM_BlockWrite(SegPhys(es)+reg_bx,name,0x20);
		return true;
	case 0x1613:	/* Get SYSTEM.DAT path */
		strcpy(name,"C:\\WINDOWS\\SYSTEM.DAT");
		MEM_BlockWrite(SegPhys(es)+reg_di,name,(Bitu)(strlen(name)+1));
		reg_ax=0;
		reg_cx=strlen(name);
		return true;
	case 0x4a16:	/* Open bootlog */
		return true;
	case 0x4a17:	/* Write bootlog */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name,255);
		LOG(LOG_DOSMISC,LOG_NORMAL)("BOOTLOG: %s\n",name);
		return true;
	case 0x4a33:	/* Check MS-DOS Version 7 */
		reg_ax=0;
		return true;
	}

	return false;
}

void DOS_SetupMisc(void) {
	/* Setup the dos multiplex interrupt */
	call_int2f=CALLBACK_Allocate();
	CALLBACK_Setup(call_int2f,&INT2F_Handler,CB_IRET,"DOS Int 2f");
	RealSetVec(0x2f,CALLBACK_RealPointer(call_int2f));
	DOS_AddMultiplexHandler(DOS_MultiplexFunctions);
	/* Setup the dos network interrupt */
	call_int2a=CALLBACK_Allocate();
	CALLBACK_Setup(call_int2a,&INT2A_Handler,CB_IRET,"DOS Int 2a");
	RealSetVec(0x2A,CALLBACK_RealPointer(call_int2a));
}
