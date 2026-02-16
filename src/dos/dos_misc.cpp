// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos.h"

#include <list>

#include "cpu/callback.h"
#include "hardware/memory.h"
#include "cpu/registers.h"

RealPt fake_sft_table = 0;

static callback_number_t call_int2f = 0;
static callback_number_t call_int2a = 0;

static std::list<MultiplexHandler*> Multiplex;

void DOS_AddMultiplexHandler(MultiplexHandler* handler) {
	Multiplex.push_front(handler);
}

void DOS_DeleteMultiplexHandler(MultiplexHandler* const handler)
{
	for (auto it = Multiplex.begin(); it != Multiplex.end(); ++it) {
		if(*it == handler) {
			Multiplex.erase(it);
			return;
		}
	}
}

static Bitu INT2F_Handler(void)
{
	for (auto handler : Multiplex) {
		if (handler()) {
			return CBRET_NONE;
		}
	}

	LOG(LOG_DOSMISC,LOG_ERROR)("DOS:Multiplex Unhandled call %4X",reg_ax);
	return CBRET_NONE;
}


static Bitu INT2A_Handler(void) {
	return CBRET_NONE;
}

static bool DOS_MultiplexFunctions(void) {
	switch (reg_ax) {
	case 0x1000:
		if (!DOS_IsFileLocking()) {
			return false;
		}
		// Report that SHARE.EXE is installed
		reg_al = 0xff;
		return true;
	case 0x1216:	/* GET ADDRESS OF SYSTEM FILE TABLE ENTRY */
		// reg_bx is a system file table entry, should coincide with
		// the file handle so just use that
		LOG(LOG_DOSMISC,LOG_ERROR)("Some BAD filetable call used bx=%X",reg_bx);
		if(reg_bx <= DOS_FILES) CALLBACK_SCF(false);
		else CALLBACK_SCF(true);
		if (reg_bx < FakeSftEntries) {
			// Initalized by DOS_SetupTables()
			assert(fake_sft_table != 0);

			RealPt sftrealpt = fake_sft_table;
			PhysPt sftptr = RealToPhysical(sftrealpt);
			Bitu sftofs = SftHeaderSize + reg_bx * SftEntrySize;

			if (Files[reg_bx]) mem_writeb(sftptr+sftofs,Files[reg_bx]->refCtr);
			else mem_writeb(sftptr+sftofs,0);

			if (!Files[reg_bx]) return true;

			uint32_t handle=RealHandle(reg_bx);
			if (handle>=DOS_FILES) {
				mem_writew(sftptr + sftofs + 0x02, 0x02); // file
				                                          // open
				                                          // mode
				mem_writeb(sftptr + sftofs + 0x04, 0x00); // file
				                                          // attribute
				mem_writew(sftptr + sftofs + 0x05,
				           Files[reg_bx]->GetInformation()); // device info word
				mem_writed(sftptr + sftofs + 0x07, 0); // device
				                                       // driver
				                                       // header
				mem_writew(sftptr + sftofs + 0x0d, 0); // packed
				                                       // time
				mem_writew(sftptr + sftofs + 0x0f, 0); // packed
				                                       // date
				mem_writew(sftptr + sftofs + 0x11, 0); // size
				mem_writew(sftptr + sftofs + 0x15, 0); // current
				                                       // position
			} else {
				uint8_t drive=Files[reg_bx]->GetDrive();

				mem_writew(sftptr + sftofs + 0x02,
				           (uint16_t)(Files[reg_bx]->flags & 3)); // file open mode
				mem_writeb(sftptr + sftofs + 0x04,
				           Files[reg_bx]->attr._data); // file
				                                       // attribute
				mem_writew(sftptr + sftofs + 0x05,
				           0x40 | drive); // device info word
				mem_writed(sftptr + sftofs + 0x07,
				           RealMake(dos.tables.dpb, drive * 9)); // dpb of the drive
				mem_writew(sftptr + sftofs + 0x0d,
				           Files[reg_bx]->time); // packed file
				                                 // time
				mem_writew(sftptr + sftofs + 0x0f,
				           Files[reg_bx]->date); // packed file
				                                 // date
				uint32_t curpos = 0;
				Files[reg_bx]->Seek(&curpos,DOS_SEEK_CUR);
				uint32_t endpos=0;
				Files[reg_bx]->Seek(&endpos,DOS_SEEK_END);
				mem_writed(sftptr + sftofs + 0x11, endpos); // size
				mem_writed(sftptr + sftofs + 0x15, curpos); // current position
				Files[reg_bx]->Seek(&curpos, DOS_SEEK_SET);
			}

			// fill in filename in fcb style
			// (space-padded name (8 chars)+space-padded extension (3 chars))
			auto filename=(const char*)Files[reg_bx]->GetName();
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

			SegSet16(es,RealSegment(sftrealpt));
			reg_di=RealOffset(sftrealpt+sftofs);
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
	case 0x1a00: // ANSI.SYS installation check (MS-DOS 4.0 or higher)
		/* Our console device emulates ANSI.SYS, so respond like it's
		   installed.
		   See: http://www.delorie.com/djgpp/doc/rbinter/id/71/46.html
		   Reported behavior was confirmed with ANSI.SYS loaded on a
		   Windows 95 MS-DOS boot disk, result AX=1AFF
		*/
		reg_al = 0xFF;
		return true;
	case 0x4a01:	/* Query free hma space */
	case 0x4a02:	/* ALLOCATE HMA SPACE */
		LOG(LOG_DOSMISC,LOG_WARN)("INT 2f:4a HMA. DOSBox reports none available.");
		reg_bx=0;	//number of bytes available in HMA or amount successfully allocated
		//ESDI=ffff:ffff Location of HMA/Allocated memory
		SegSet16(es,0xffff);
		reg_di=0xffff;
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
