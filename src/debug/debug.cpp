/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: debug.cpp,v 1.58 2004-08-28 12:51:35 qbix79 Exp $ */

#include "programs.h"

#include <string.h>
#include <list>

#include "dosbox.h"
#if C_DEBUG
#include "debug.h"
#include "cross.h" //snprintf
#include "cpu.h"
#include "video.h"
#include "pic.h"
#include "mapper.h"
#include "cpu.h"
#include "callback.h"
#include "inout.h"
#include "mixer.h"
#include "debug_inc.h"
#include "timer.h"
#include "paging.h"
#include "../ints/xms.h"
#include "shell.h"

#ifdef WIN32
void WIN32_Console();
#else
#include <termios.h>
#include <unistd.h>
static struct termios consolesettings;
int old_cursor_state;
#endif

// Forwards
static void DrawCode(void);
static bool DEBUG_Log_Loop(int count);
static void DEBUG_RaiseTimerIrq(void);
static void SaveMemory(Bitu seg, Bitu ofs1, Bit32s num);
static void LogGDT(void);
static void LogLDT(void);
static void LogIDT(void);
static void OutputVecTable(char* filename);
static void DrawVariables(void);

char* AnalyzeInstruction(char* inst, bool saveSelector);
Bit32u GetHexValue(char* str, char*& hex);



class DEBUG;

DEBUG*	pDebugcom	= 0;
bool	exitLoop	= false;
bool	logHeavy	= false;


// Heavy Debugging Vars for logging
#if C_HEAVY_DEBUG
static FILE*	cpuLogFile		= 0;
static bool		cpuLog			= false;
static int		cpuLogCounter	= 0;
#endif



static struct  {
	Bit32u eax,ebx,ecx,edx,esi,edi,ebp,esp,eip;
} oldregs;

static char curSelectorName[3] = { 0,0,0 };

static Segment oldsegs[6];
static Bitu oldflags;
DBGBlock dbg;
static Bitu input_count;
Bitu cycle_count;
static bool debugging;


static void SetColor(Bitu test) {
	if (test) {
		if (has_colors()) { wattrset(dbg.win_reg,COLOR_PAIR(PAIR_BYELLOW_BLACK));}
	} else {
		if (has_colors()) { wattrset(dbg.win_reg,0);}
	}
}

struct SCodeViewData {	
	int		cursorPos;
	Bit16u  firstInstSize;
	Bit16u	useCS;
	Bit32u	useEIPlast, useEIPmid;
	Bit32u	useEIP;
	Bit16u  cursorSeg;
	Bit32u	cursorOfs;
	bool	inputMode;
	char	inputStr[255];

} codeViewData;

static Bit16u	dataSeg;
static Bit32u	dataOfs;
static bool		showExtend = true;

/***********/
/* Helpers */
/***********/

Bit32u PhysMakeProt(Bit16u selector, Bit32u offset)
{
	Descriptor desc;
	if (cpu.gdt.GetDescriptor(selector,desc)) return desc.GetBase()+offset;
	return 0;
};

Bit32u GetAddress(Bit16u seg, Bit32u offset)
{
	if (seg==SegValue(cs)) return SegPhys(cs)+offset;
	if (cpu.pmode) {
		Descriptor desc;
		if (cpu.gdt.GetDescriptor(seg,desc)) return PhysMakeProt(seg,offset);
	}
	return (seg<<4)+offset;
}


bool GetDescriptorInfo(char* selname, char* out1, char* out2)
{
	Bitu sel;
	Descriptor desc;

	if (strstr(selname,"cs") || strstr(selname,"CS")) sel = SegValue(cs); else
	if (strstr(selname,"ds") || strstr(selname,"DS")) sel = SegValue(ds); else 
	if (strstr(selname,"es") || strstr(selname,"ES")) sel = SegValue(es); else
	if (strstr(selname,"fs") || strstr(selname,"FS")) sel = SegValue(fs); else
	if (strstr(selname,"gs") || strstr(selname,"GS")) sel = SegValue(gs); else
	if (strstr(selname,"ss") || strstr(selname,"SS")) sel = SegValue(ss); else
	sel = GetHexValue(selname,selname);
	// FIXME: Call Gate Descriptors
	if (cpu.gdt.GetDescriptor(sel,desc)) {
		sprintf(out1,"%s: b:%08X type:%02X parbg",selname,desc.GetBase(),desc.saved.seg.type);
		sprintf(out2,"    l:%08X dpl : %01X %1X%1X%1X%1X%1X",desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
		return true;
	} else {
		strcpy(out1,"                                     ");
		strcpy(out2,"                                     ");
	}
	//out1[0] = out2[0] = 0;
	return false;
};

/********************/
/* DebugVar   stuff */
/********************/

class CDebugVar
{
public:
	CDebugVar(char* _name, PhysPt _adr) { adr=_adr; (strlen(name)<15)?strcpy(name,_name):strncpy(name,_name,15); name[15]=0; };
	
	char*	GetName(void) { return name; };
	PhysPt	GetAdr (void) { return adr;  };

private:
	PhysPt  adr;
	char	name[16];

public: 
	static void			InsertVariable	(char* name, PhysPt adr);
	static CDebugVar*	FindVar			(PhysPt adr);
	static void			DeleteAll		();
	static bool			SaveVars		(char* name);
	static bool			LoadVars		(char* name);

	static std::list<CDebugVar*>	varList;
};

std::list<CDebugVar*> CDebugVar::varList;


/********************/
/* Breakpoint stuff */
/********************/

bool skipFirstInstruction = false;

enum EBreakpoint { BKPNT_UNKNOWN, BKPNT_PHYSICAL, BKPNT_INTERRUPT, BKPNT_MEMORY, BKPNT_MEMORY_PROT, BKPNT_MEMORY_LINEAR };

#define BPINT_ALL 0x100

class CBreakpoint
{
public:
	CBreakpoint								(void)						{ location = 0; active = once = false; segment = 0; offset = 0; intNr = 0; ahValue = 0; type = BKPNT_UNKNOWN; };

	void					SetAddress		(Bit16u seg, Bit32u off)	{ location = GetAddress(seg,off);	type = BKPNT_PHYSICAL; segment = seg; offset = off; };
	void					SetAddress		(PhysPt adr)				{ location = adr;				type = BKPNT_PHYSICAL; };
	void					SetInt			(Bit8u _intNr, Bit16u ah)	{ intNr = _intNr, ahValue = ah; type = BKPNT_INTERRUPT; };
	void					SetOnce			(bool _once)				{ once = _once; };
	void					SetType			(EBreakpoint _type)			{ type = _type; };
	void					SetValue		(Bit8u value)				{ ahValue = value; };

	bool					IsActive		(void)						{ return active; };
	void					Activate		(bool _active);

	EBreakpoint				GetType			(void)						{ return type; };
	bool					GetOnce			(void)						{ return once; };
	PhysPt					GetLocation		(void)						{ if (GetType()!=BKPNT_INTERRUPT)	return location;	else return 0; };
	Bit16u					GetSegment		(void)						{ return segment; };
	Bit32u					GetOffset		(void)						{ return offset; };
	Bit8u					GetIntNr		(void)						{ if (GetType()==BKPNT_INTERRUPT)	return intNr;		else return 0; };
	Bit16u					GetValue		(void)						{ if (GetType()!=BKPNT_PHYSICAL)	return ahValue;		else return 0; };

	// statics
	static CBreakpoint*		AddBreakpoint		(Bit16u seg, Bit32u off, bool once);
	static CBreakpoint*		AddIntBreakpoint	(Bit8u intNum, Bit16u ah, bool once);
	static CBreakpoint*		AddMemBreakpoint	(Bit16u seg, Bit32u off);
	static void				ActivateBreakpoints	(PhysPt adr, bool activate);
	static bool				CheckBreakpoint		(PhysPt adr);
	static bool				CheckBreakpoint		(Bitu seg, Bitu off);
	static bool				CheckIntBreakpoint	(PhysPt adr, Bit8u intNr, Bit16u ahValue);
	static bool				IsBreakpoint		(PhysPt where);
	static bool				IsBreakpointDrawn	(PhysPt where);
	static bool				DeleteBreakpoint	(PhysPt where);
	static bool				DeleteByIndex		(Bit16u index);
	static void				DeleteAll			(void);
	static void				ShowList			(void);


private:
	EBreakpoint	type;
	// Physical
	PhysPt		location;
	Bit8u		oldData;
	Bit16u		segment;
	Bit32u		offset;
	// Int
	Bit8u		intNr;
	Bit16u		ahValue;
	// Shared
	bool		active;
	bool		once;

	static std::list<CBreakpoint*>	BPoints;
public:
	static CBreakpoint*				ignoreOnce;
};

void CBreakpoint::Activate(bool _active)
{
	if (GetType()==BKPNT_PHYSICAL) {
#if !C_HEAVY_DEBUG
		if (_active) {
			// Set 0xCC and save old value
			Bit8u data = mem_readb(location);
			if (data!=0xCC) {
				oldData = data;
				mem_writeb(location,0xCC);
			};
		} else {
			// Remove 0xCC and set old value
			if (mem_readb (location)==0xCC) {
				mem_writeb(location,oldData);
			};
		}
#endif
	}
	active = _active;
};

// Statics
std::list<CBreakpoint*> CBreakpoint::BPoints;
CBreakpoint*			CBreakpoint::ignoreOnce = 0;
Bitu					ignoreAddressOnce = 0;

CBreakpoint* CBreakpoint::AddBreakpoint(Bit16u seg, Bit32u off, bool once)
{
	CBreakpoint* bp = new CBreakpoint();
	bp->SetAddress		(seg,off);
	bp->SetOnce			(once);
	BPoints.push_front	(bp);
	return bp;
};

CBreakpoint* CBreakpoint::AddIntBreakpoint(Bit8u intNum, Bit16u ah, bool once)
{
	CBreakpoint* bp = new CBreakpoint();
	bp->SetInt			(intNum,ah);
	bp->SetOnce			(once);
	BPoints.push_front	(bp);
	return bp;
};

CBreakpoint* CBreakpoint::AddMemBreakpoint(Bit16u seg, Bit32u off)
{
	CBreakpoint* bp = new CBreakpoint();
	bp->SetAddress		(seg,off);
	bp->SetOnce			(false);
	bp->SetType			(BKPNT_MEMORY);
	BPoints.push_front	(bp);
	return bp;
};

void CBreakpoint::ActivateBreakpoints(PhysPt adr, bool activate)
{
	// activate all breakpoints
	std::list<CBreakpoint*>::iterator i;
	CBreakpoint* bp;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		bp = static_cast<CBreakpoint*>(*i);
		// Do not activate, when bp is an actual adress
		if (activate && (bp->GetType()==BKPNT_PHYSICAL) && (bp->GetLocation()==adr)) {
			// Do not activate :)
			continue;
		}
		bp->Activate(activate);	
	};
};

bool CBreakpoint::CheckBreakpoint(Bitu seg, Bitu off)
// Checks if breakpoint is valid an should stop execution
{
	if ((ignoreAddressOnce!=0) && (GetAddress(seg,off)==ignoreAddressOnce)) {
		ignoreAddressOnce = 0;
		return false;
	} else
		ignoreAddressOnce = 0;

	// Search matching breakpoint
	std::list<CBreakpoint*>::iterator i;
	CBreakpoint* bp;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		bp = static_cast<CBreakpoint*>(*i);
		if ((bp->GetType()==BKPNT_PHYSICAL) && bp->IsActive() && (bp->GetSegment()==seg) && (bp->GetOffset()==off)) {
			// Ignore Once ?
			if (ignoreOnce==bp) {
				ignoreOnce=0;
				bp->Activate(true);
				return false;
			};
			// Found, 
			if (bp->GetOnce()) {
				// delete it, if it should only be used once
				(BPoints.erase)(i);
				bp->Activate(false);
				delete bp;
			} else {
				ignoreOnce = bp;
			};
			return true;
		} 
#if C_HEAVY_DEBUG
		// Memory breakpoint support
		else if (bp->IsActive()) {
			if ((bp->GetType()==BKPNT_MEMORY) || (bp->GetType()==BKPNT_MEMORY_PROT) || (bp->GetType()==BKPNT_MEMORY_LINEAR)) {
				// Watch Protected Mode Memoryonly in pmode
				if (bp->GetType()==BKPNT_MEMORY_PROT) {
					// Check if pmode is active
					if (!cpu.pmode) return false;
					// Check if descriptor is valid
					Descriptor desc;
					if (!cpu.gdt.GetDescriptor(bp->GetSegment(),desc)) return false;
					if (desc.GetLimit()==0) return false;
				}

				Bitu address; 
				if (bp->GetType()==BKPNT_MEMORY_LINEAR) address = bp->GetOffset();
				else address = GetAddress(bp->GetSegment(),bp->GetOffset());
				Bit8u value = mem_readb(address);
				if (bp->GetValue() != value) {
					// Yup, memory value changed
					DEBUG_ShowMsg("DEBUG: Memory breakpoint %s: %04X:%04X - %02X -> %02X",(bp->GetType()==BKPNT_MEMORY_PROT)?"(Prot)":"",bp->GetSegment(),bp->GetOffset(),bp->GetValue(),value);
					bp->SetValue(value);
					return true;
				};		
			} 		
		};
#endif
	};
	return false;
};

bool CBreakpoint::CheckIntBreakpoint(PhysPt adr, Bit8u intNr, Bit16u ahValue)
// Checks if interrupt breakpoint is valid an should stop execution
{
	if ((ignoreAddressOnce!=0) && (adr==ignoreAddressOnce)) {
		ignoreAddressOnce = 0;
		return false;
	} else
		ignoreAddressOnce = 0;

	// Search matching breakpoint
	std::list<CBreakpoint*>::iterator i;
	CBreakpoint* bp;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		bp = static_cast<CBreakpoint*>(*i);
		if ((bp->GetType()==BKPNT_INTERRUPT) && bp->IsActive() && (bp->GetIntNr()==intNr)) {
			if ((bp->GetValue()==BPINT_ALL) || (bp->GetValue()==ahValue)) {
				// Ignoie it once ?
				if (ignoreOnce==bp) {
					ignoreOnce=0;
					bp->Activate(true);
					return false;
				};
				// Found
				if (bp->GetOnce()) {
					// delete it, if it should only be used once
					(BPoints.erase)(i);
					bp->Activate(false);
					delete bp;
				} else {
					ignoreOnce = bp;
				}
				return true;
			}
		};
	};
	return false;
};

void CBreakpoint::DeleteAll() 
{
	std::list<CBreakpoint*>::iterator i;
	CBreakpoint* bp;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		bp = static_cast<CBreakpoint*>(*i);
		bp->Activate(false);
		delete bp;
	};
	(BPoints.clear)();
};


bool CBreakpoint::DeleteByIndex(Bit16u index) 
{
	// Search matching breakpoint
	int nr = 0;
	std::list<CBreakpoint*>::iterator i;
	CBreakpoint* bp;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		if (nr==index) {
			bp = static_cast<CBreakpoint*>(*i);
			(BPoints.erase)(i);
			bp->Activate(false);
			delete bp;
			return true;
		}
		nr++;
	};
	return false;
};

bool CBreakpoint::DeleteBreakpoint(PhysPt where) 
{
	// Search matching breakpoint
	std::list<CBreakpoint*>::iterator i;
	CBreakpoint* bp;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		bp = static_cast<CBreakpoint*>(*i);
		if ((bp->GetType()==BKPNT_PHYSICAL) && (bp->GetLocation()==where)) {
			(BPoints.erase)(i);
			bp->Activate(false);
			delete bp;
			return true;
		}
	};
	return false;
};

bool CBreakpoint::IsBreakpoint(PhysPt adr) 
// is there a breakpoint at address ?
{
	// Search matching breakpoint
	std::list<CBreakpoint*>::iterator i;
	CBreakpoint* bp;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		bp = static_cast<CBreakpoint*>(*i);
		if ((bp->GetType()==BKPNT_PHYSICAL) && (bp->GetSegment()==adr)) {
			return true;
		};
		if ((bp->GetType()==BKPNT_PHYSICAL) && (bp->GetLocation()==adr)) {
			return true;
		};
	};
	return false;
};

bool CBreakpoint::IsBreakpointDrawn(PhysPt adr) 
// valid breakpoint, that should be drawn ?
{
	// Search matching breakpoint
	std::list<CBreakpoint*>::iterator i;
	CBreakpoint* bp;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		bp = static_cast<CBreakpoint*>(*i);
		if ((bp->GetType()==BKPNT_PHYSICAL) && (bp->GetLocation()==adr)) {
			// Only draw, if breakpoint is not only once, 
			return !bp->GetOnce();
		};
	};
	return false;
};

void CBreakpoint::ShowList(void)
{
	// iterate list 
	int nr = 0;
	std::list<CBreakpoint*>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		CBreakpoint* bp = static_cast<CBreakpoint*>(*i);
		if (bp->GetType()==BKPNT_PHYSICAL) {
			wprintw(dbg.win_out,"%02X. BP %04X:%04X\n",nr,bp->GetSegment(),bp->GetOffset());
			nr++;
		} else if (bp->GetType()==BKPNT_INTERRUPT) {
			if (bp->GetValue()==BPINT_ALL)	wprintw(dbg.win_out,"%02X. BPINT %02X\n",nr,bp->GetIntNr());					
			else							wprintw(dbg.win_out,"%02X. BPINT %02X AH=%02X\n",nr,bp->GetIntNr(),bp->GetValue());
			nr++;
		} else if (bp->GetType()==BKPNT_MEMORY) {
			wprintw(dbg.win_out,"%02X. BPMEM %04X:%04X (%02X)\n",nr,bp->GetSegment(),bp->GetOffset(),bp->GetValue());
			nr++;
		};
	}
	wrefresh(dbg.win_out);
};

bool DEBUG_Breakpoint(void)
{
	/* First get the phyiscal address and check for a set Breakpoint */
	PhysPt where=GetAddress(SegValue(cs),reg_eip);
	if (!CBreakpoint::CheckBreakpoint(SegValue(cs),reg_eip)) return false;
	// Found. Breakpoint is valid
	CBreakpoint::ActivateBreakpoints(where,false);	// Deactivate all breakpoints
	return true;
};

bool DEBUG_IntBreakpoint(Bit8u intNum)
{
	/* First get the phyiscal address and check for a set Breakpoint */
	PhysPt where=GetAddress(SegValue(cs),reg_eip);
	if (!CBreakpoint::CheckIntBreakpoint(where,intNum,reg_ah)) return false;
	// Found. Breakpoint is valid
	CBreakpoint::ActivateBreakpoints(where,false);	// Deactivate all breakpoints
	return true;
};

static bool StepOver()
{
	exitLoop = false;
	PhysPt start=GetAddress(SegValue(cs),reg_eip);
	char dline[200];Bitu size;
	size=DasmI386(dline, start, reg_eip, cpu.code.big);

	if (strstr(dline,"call") || strstr(dline,"int") || strstr(dline,"loop") || strstr(dline,"rep")) {
		CBreakpoint::AddBreakpoint		(SegValue(cs),reg_eip+size, true);
		CBreakpoint::ActivateBreakpoints(start, true);
		debugging=false;
		DrawCode();
		DOSBOX_SetNormalLoop();
		return true;
	} 
	return false;
};

bool DEBUG_ExitLoop(void)
{
#if C_HEAVY_DEBUG
	DrawVariables();
#endif

	if (exitLoop) {
		exitLoop = false;
		return true;
	}
	return false;
};

/********************/
/*   Draw windows   */
/********************/

static void DrawData(void) {
	
	Bit8u ch;
	Bit32u add = dataOfs;
	Bit32u address;
	/* Data win */	
	for (int y=0; y<8; y++) {
		// Adress
		mvwprintw (dbg.win_data,1+y,0,"%04X:%04X ",dataSeg,add);
		for (int x=0; x<16; x++) {
			address = GetAddress(dataSeg,add);
			if (!(paging.tlb.handler[address >> 12]->flags & PFLAG_INIT)) {
				ch = mem_readb(address);
			} else ch = 0;
			mvwprintw (dbg.win_data,1+y,11+3*x,"%02X",ch);
			if (ch<32) ch='.';
			mvwprintw (dbg.win_data,1+y,60+x,"%c",ch);			
			add++;
		};
	}	
	wrefresh(dbg.win_data);
};

static void DrawRegisters(void) {
	/* Main Registers */
	SetColor(reg_eax!=oldregs.eax);oldregs.eax=reg_eax;mvwprintw (dbg.win_reg,0,4,"%08X",reg_eax);
	SetColor(reg_ebx!=oldregs.ebx);oldregs.ebx=reg_ebx;mvwprintw (dbg.win_reg,1,4,"%08X",reg_ebx);
	SetColor(reg_ecx!=oldregs.ecx);oldregs.ecx=reg_ecx;mvwprintw (dbg.win_reg,2,4,"%08X",reg_ecx);
	SetColor(reg_edx!=oldregs.edx);oldregs.edx=reg_edx;mvwprintw (dbg.win_reg,3,4,"%08X",reg_edx);

	SetColor(reg_esi!=oldregs.esi);oldregs.esi=reg_esi;mvwprintw (dbg.win_reg,0,18,"%08X",reg_esi);
	SetColor(reg_edi!=oldregs.edi);oldregs.edi=reg_edi;mvwprintw (dbg.win_reg,1,18,"%08X",reg_edi);
	SetColor(reg_ebp!=oldregs.ebp);oldregs.ebp=reg_ebp;mvwprintw (dbg.win_reg,2,18,"%08X",reg_ebp);
	SetColor(reg_esp!=oldregs.esp);oldregs.esp=reg_esp;mvwprintw (dbg.win_reg,3,18,"%08X",reg_esp);
	SetColor(reg_eip!=oldregs.eip);oldregs.eip=reg_eip;mvwprintw (dbg.win_reg,1,42,"%08X",reg_eip);
	
	SetColor(SegValue(ds)!=oldsegs[ds].val);oldsegs[ds].val=SegValue(ds);mvwprintw (dbg.win_reg,0,31,"%04X",SegValue(ds));
	SetColor(SegValue(es)!=oldsegs[es].val);oldsegs[es].val=SegValue(es);mvwprintw (dbg.win_reg,0,41,"%04X",SegValue(es));
	SetColor(SegValue(fs)!=oldsegs[fs].val);oldsegs[fs].val=SegValue(fs);mvwprintw (dbg.win_reg,0,51,"%04X",SegValue(fs));
	SetColor(SegValue(gs)!=oldsegs[gs].val);oldsegs[gs].val=SegValue(gs);mvwprintw (dbg.win_reg,0,61,"%04X",SegValue(gs));
	SetColor(SegValue(ss)!=oldsegs[ss].val);oldsegs[ss].val=SegValue(ss);mvwprintw (dbg.win_reg,0,71,"%04X",SegValue(ss));
	SetColor(SegValue(cs)!=oldsegs[cs].val);oldsegs[cs].val=SegValue(cs);mvwprintw (dbg.win_reg,1,31,"%04X",SegValue(cs));

	/*Individual flags*/

	SetColor((reg_flags ^ oldflags)&FLAG_CF);
	mvwprintw (dbg.win_reg,1,53,"%01X",GETFLAG(CF) ? 1:0);
	SetColor((reg_flags ^ oldflags)&FLAG_ZF);
	mvwprintw (dbg.win_reg,1,56,"%01X",GETFLAG(ZF) ? 1:0);
	SetColor((reg_flags ^ oldflags)&FLAG_SF);
	mvwprintw (dbg.win_reg,1,59,"%01X",GETFLAG(SF) ? 1:0);
	SetColor((reg_flags ^ oldflags)&FLAG_OF);
	mvwprintw (dbg.win_reg,1,62,"%01X",GETFLAG(OF) ? 1:0);
	SetColor((reg_flags ^ oldflags)&FLAG_AF);
	mvwprintw (dbg.win_reg,1,65,"%01X",GETFLAG(AF) ? 1:0);
	SetColor((reg_flags ^ oldflags)&FLAG_PF);
	mvwprintw (dbg.win_reg,1,68,"%01X",GETFLAG(PF) ? 1:0);


	SetColor((reg_flags ^ oldflags)&FLAG_DF);
	mvwprintw (dbg.win_reg,1,71,"%01X",GETFLAG(DF) ? 1:0);
	SetColor((reg_flags ^ oldflags)&FLAG_IF);
	mvwprintw (dbg.win_reg,1,74,"%01X",GETFLAG(IF) ? 1:0);
	SetColor((reg_flags ^ oldflags)&FLAG_TF);
	mvwprintw (dbg.win_reg,1,77,"%01X",GETFLAG(TF) ? 1:0);

	oldflags=reg_flags;

	if (cpu.pmode) {
		if (reg_flags & FLAG_VM) mvwprintw(dbg.win_reg,0,76,"VM86");
		else if (cpu.code.big) mvwprintw(dbg.win_reg,0,76,"Pr32");
		else mvwprintw(dbg.win_reg,0,76,"Pr16");
	} else	
		mvwprintw(dbg.win_reg,0,76,"Real");

	// Selector info, if available
	if ((cpu.pmode) && curSelectorName[0]) {		
		char out1[200], out2[200];
		GetDescriptorInfo(curSelectorName,out1,out2);
		mvwprintw(dbg.win_reg,2,28,out1);
		mvwprintw(dbg.win_reg,3,28,out2);
	}

	wattrset(dbg.win_reg,0);
	mvwprintw(dbg.win_reg,3,60,"%d       ",cycle_count);
	wrefresh(dbg.win_reg);
};

static void DrawCode(void) 
{
	bool saveSel; 
	Bit32u disEIP = codeViewData.useEIP;
	PhysPt start  = GetAddress(codeViewData.useCS,codeViewData.useEIP);
	char dline[200];Bitu size;Bitu c;
	
	for (int i=0;i<10;i++) {
		saveSel = false;
		if (has_colors()) {
			if ((codeViewData.useCS==SegValue(cs)) && (disEIP == reg_eip)) {
				wattrset(dbg.win_code,COLOR_PAIR(PAIR_GREEN_BLACK));			
				if (codeViewData.cursorPos==-1) {
					codeViewData.cursorPos = i; // Set Cursor 
					codeViewData.cursorSeg = SegValue(cs);
					codeViewData.cursorOfs = disEIP;
				}
				saveSel = (i == codeViewData.cursorPos);
			} else if (i == codeViewData.cursorPos) {
				wattrset(dbg.win_code,COLOR_PAIR(PAIR_BLACK_GREY));			
				codeViewData.cursorSeg = codeViewData.useCS;
				codeViewData.cursorOfs = disEIP;
				saveSel = true;
			} else if (CBreakpoint::IsBreakpointDrawn(start)) {
				wattrset(dbg.win_code,COLOR_PAIR(PAIR_GREY_RED));			
			} else {
				wattrset(dbg.win_code,0);			
			}
		}


		Bitu drawsize=size=DasmI386(dline, start, disEIP, cpu.code.big);
		bool toolarge = false;
		mvwprintw(dbg.win_code,i,0,"%04X:%04X  ",codeViewData.useCS,disEIP);
		
		if (drawsize>10) { toolarge = true; drawsize = 9; };
		for (c=0;c<drawsize;c++) wprintw(dbg.win_code,"%02X",mem_readb(start+c));
		if (toolarge) { wprintw(dbg.win_code,".."); drawsize++; };
		for (c=20;c>=drawsize*2;c--) waddch(dbg.win_code,' ');
		
		char* res = 0;
		if (showExtend) res = AnalyzeInstruction(dline, saveSel);
		waddstr(dbg.win_code,dline);
		if (strlen(dline)<28) for (c=28-strlen(dline);c>0;c--) waddch(dbg.win_code,' ');
		if (showExtend) {
			waddstr(dbg.win_code,res);
			for (c=strlen(res);c<20;c++) waddch(dbg.win_code,' ');
		} else {
			for (c=0;c<20;c++) waddch(dbg.win_code,' ');
		}
		start+=size;
		disEIP+=size;

		if (i==0) codeViewData.firstInstSize = size;
		if (i==4) codeViewData.useEIPmid	 = disEIP;
	}

	codeViewData.useEIPlast = disEIP;
	
	wattrset(dbg.win_code,0);			
	if (!debugging) {
		mvwprintw(dbg.win_code,10,0,"(Running)",codeViewData.inputStr);		
	} else if (codeViewData.inputMode) {
		mvwprintw(dbg.win_code,10,0,"-> %s_  ",codeViewData.inputStr);		
	} else {
		mvwprintw(dbg.win_code,10,0," ");
		for (c=0;c<50;c++) waddch(dbg.win_code,' ');
	};
	
	wrefresh(dbg.win_code);
}

static void SetCodeWinStart()
{
	if ((SegValue(cs)==codeViewData.useCS) && (reg_eip>=codeViewData.useEIP) && (reg_eip<=codeViewData.useEIPlast)) {
		// in valid window - scroll ?
		if (reg_eip>=codeViewData.useEIPmid) codeViewData.useEIP += codeViewData.firstInstSize;
		
	} else {
		// totally out of range.
		codeViewData.useCS	= SegValue(cs);
		codeViewData.useEIP	= reg_eip;
	}
	codeViewData.cursorPos = -1;	// Recalc Cursor position
};

/********************/
/*    User input    */
/********************/

Bit32u GetHexValue(char* str, char*& hex)
{
	Bit32u	value = 0;

	hex = str;
	while (*hex==' ') hex++;
	if (strstr(hex,"EAX")==hex) { hex+=3; return reg_eax; };
	if (strstr(hex,"EBX")==hex) { hex+=3; return reg_ebx; };
	if (strstr(hex,"ECX")==hex) { hex+=3; return reg_ecx; };
	if (strstr(hex,"EDX")==hex) { hex+=3; return reg_edx; };
	if (strstr(hex,"ESI")==hex) { hex+=3; return reg_esi; };
	if (strstr(hex,"EDI")==hex) { hex+=3; return reg_edi; };
	if (strstr(hex,"EBP")==hex) { hex+=3; return reg_ebp; };
	if (strstr(hex,"ESP")==hex) { hex+=3; return reg_esp; };
	if (strstr(hex,"EIP")==hex) { hex+=3; return reg_eip; };
	if (strstr(hex,"AX")==hex) { hex+=2; return reg_ax; };
	if (strstr(hex,"BX")==hex) { hex+=2; return reg_bx; };
	if (strstr(hex,"CX")==hex) { hex+=2; return reg_cx; };
	if (strstr(hex,"DX")==hex) { hex+=2; return reg_dx; };
	if (strstr(hex,"SI")==hex) { hex+=2; return reg_si; };
	if (strstr(hex,"DI")==hex) { hex+=2; return reg_di; };
	if (strstr(hex,"BP")==hex) { hex+=2; return reg_bp; };
	if (strstr(hex,"SP")==hex) { hex+=2; return reg_sp; };
	if (strstr(hex,"IP")==hex) { hex+=2; return reg_ip; };
	if (strstr(hex,"CS")==hex) { hex+=2; return SegValue(cs); };
	if (strstr(hex,"DS")==hex) { hex+=2; return SegValue(ds); };
	if (strstr(hex,"ES")==hex) { hex+=2; return SegValue(es); };
	if (strstr(hex,"FS")==hex) { hex+=2; return SegValue(fs); };
	if (strstr(hex,"GS")==hex) { hex+=2; return SegValue(gs); };
	if (strstr(hex,"SS")==hex) { hex+=2; return SegValue(ss); };

	while (*hex) {
		if ((*hex>='0') && (*hex<='9')) value = (value<<4)+*hex-'0';	  else
		if ((*hex>='A') && (*hex<='F')) value = (value<<4)+*hex-'A'+10; 
		else break; // No valid char
		hex++;
	};
	return value;
};

bool ChangeRegister(char* str)
{
	char* hex = str;
	while (*hex==' ') hex++;
	if (strstr(hex,"EAX")==hex) { hex+=3; reg_eax = GetHexValue(hex,hex); } else
	if (strstr(hex,"EBX")==hex) { hex+=3; reg_ebx = GetHexValue(hex,hex); } else
	if (strstr(hex,"ECX")==hex) { hex+=3; reg_ecx = GetHexValue(hex,hex); } else
	if (strstr(hex,"EDX")==hex) { hex+=3; reg_edx = GetHexValue(hex,hex); } else
	if (strstr(hex,"ESI")==hex) { hex+=3; reg_esi = GetHexValue(hex,hex); } else
	if (strstr(hex,"EDI")==hex) { hex+=3; reg_edi = GetHexValue(hex,hex); } else
	if (strstr(hex,"EBP")==hex) { hex+=3; reg_ebp = GetHexValue(hex,hex); } else
	if (strstr(hex,"ESP")==hex) { hex+=3; reg_esp = GetHexValue(hex,hex); } else
	if (strstr(hex,"EIP")==hex) { hex+=3; reg_eip = GetHexValue(hex,hex); } else
	if (strstr(hex,"AX")==hex) { hex+=2; reg_ax = (Bit16u)GetHexValue(hex,hex); } else
	if (strstr(hex,"BX")==hex) { hex+=2; reg_bx = (Bit16u)GetHexValue(hex,hex); } else
	if (strstr(hex,"CX")==hex) { hex+=2; reg_cx = (Bit16u)GetHexValue(hex,hex); } else
	if (strstr(hex,"DX")==hex) { hex+=2; reg_dx = (Bit16u)GetHexValue(hex,hex); } else
	if (strstr(hex,"SI")==hex) { hex+=2; reg_si = (Bit16u)GetHexValue(hex,hex); } else
	if (strstr(hex,"DI")==hex) { hex+=2; reg_di = (Bit16u)GetHexValue(hex,hex); } else
	if (strstr(hex,"BP")==hex) { hex+=2; reg_bp = (Bit16u)GetHexValue(hex,hex); } else
	if (strstr(hex,"SP")==hex) { hex+=2; reg_sp = (Bit16u)GetHexValue(hex,hex); } else
	if (strstr(hex,"IP")==hex) { hex+=2; reg_ip = (Bit16u)GetHexValue(hex,hex); } else
	if (strstr(hex,"CS")==hex) { hex+=2; SegSet16(cs,(Bit16u)GetHexValue(hex,hex)); } else
	if (strstr(hex,"DS")==hex) { hex+=2; SegSet16(ds,(Bit16u)GetHexValue(hex,hex)); } else
	if (strstr(hex,"ES")==hex) { hex+=2; SegSet16(es,(Bit16u)GetHexValue(hex,hex)); } else
	if (strstr(hex,"FS")==hex) { hex+=2; SegSet16(fs,(Bit16u)GetHexValue(hex,hex)); } else
	if (strstr(hex,"GS")==hex) { hex+=2; SegSet16(gs,(Bit16u)GetHexValue(hex,hex)); } else
	if (strstr(hex,"SS")==hex) { hex+=2; SegSet16(ss,(Bit16u)GetHexValue(hex,hex)); } else
	if (strstr(hex,"AF")==hex) { hex+=2; SETFLAGBIT(AF,GetHexValue(hex,hex)); } else
	if (strstr(hex,"CF")==hex) { hex+=2; SETFLAGBIT(CF,GetHexValue(hex,hex)); } else
	if (strstr(hex,"DF")==hex) { hex+=2; SETFLAGBIT(PF,GetHexValue(hex,hex)); } else
	if (strstr(hex,"IF")==hex) { hex+=2; SETFLAGBIT(IF,GetHexValue(hex,hex)); } else
	if (strstr(hex,"OF")==hex) { hex+=3; SETFLAGBIT(OF,GetHexValue(hex,hex)); } else
	if (strstr(hex,"ZF")==hex) { hex+=3; SETFLAGBIT(ZF,GetHexValue(hex,hex)); } else
	if (strstr(hex,"PF")==hex) { hex+=3; SETFLAGBIT(PF,GetHexValue(hex,hex)); } else
								{ return false; };
	return true;
};

bool ParseCommand(char* str)
{
	char* found = str;
	for(char* idx = found;*idx != 0; idx++)
		*idx = toupper(*idx);

	found = trim(found);

	found = strstr(str,"MEMDUMP ");
	if (found) { // Insert variable
		found+=8;
		Bit16u seg = (Bit16u)GetHexValue(found,found); found++;
		Bit32u ofs = GetHexValue(found,found); found++;
		Bit32u num = GetHexValue(found,found); found++;
		SaveMemory(seg,ofs,num);
		return true;
	};

	found = strstr(str,"IV ");
	if (found) { // Insert variable
		found+=3;
		Bit16u seg = (Bit16u)GetHexValue(found,found); found++;
		Bit32u ofs = (Bit16u)GetHexValue(found,found); found++;
		char name[16];
		for (int i=0; i<16; i++) {
			if ((found[i]!=' ') && (found[i]!=0)) name[i] = found[i]; 
			else { name[i] = 0; break; };
		};
		name[15] = 0;
		
		DEBUG_ShowMsg("DEBUG: Created debug var %s at %04X:%04X",name,seg,ofs);
		CDebugVar::InsertVariable(name,GetAddress(seg,ofs));
		return true;
	}
	
	found = strstr(str,"SV ");
	if (found) { // Save variables
		found+=3;
		char name[13];
		for (int i=0; i<12; i++) {
			if ((found[i]!=' ') && (found[i]!=0)) name[i] = found[i]; 
			else { name[i] = 0; break; };
		};
		name[12] = 0;		
		if (CDebugVar::SaveVars(name))	DEBUG_ShowMsg("DEBUG: Variable list save (%s) : ok.",name);
		else							DEBUG_ShowMsg("DEBUG: Variable list save (%s) : failure",name);
		return true;
	}

	found = strstr(str,"LV ");
	if (found) { // load variables
		found+=3;
		char name[13];
		for (int i=0; i<12; i++) {
			if ((found[i]!=' ') && (found[i]!=0)) name[i] = found[i]; 
			else { name[i] = 0; break; };
		};
		name[12] = 0;		
		if (CDebugVar::LoadVars(name))	DEBUG_ShowMsg("DEBUG: Variable list load (%s) : ok.",name);
		else							DEBUG_ShowMsg("DEBUG: Variable list load (%s) : failure",name);
		return true;
	}
	found = strstr(str,"SR ");
	if (found) { // Set register value
		found+=2;
		if (ChangeRegister(found))	DEBUG_ShowMsg("DEBUG: Set Register success.");
		else						DEBUG_ShowMsg("DEBUG: Set Register failure.");
		return true;
	}	
	found = strstr(str,"SM ");
	if (found) { // Set memory with following values
		found+=3;
		Bit16u seg = (Bit16u)GetHexValue(found,found); found++;
		Bit32u ofs = GetHexValue(found,found); found++;
		Bit16u count = 0;
		while (*found) {
			while (*found==' ') found++;
			if (*found) {
				Bit8u value = (Bit8u)GetHexValue(found,found); found++;
				mem_writeb(GetAddress(seg,ofs+count),value);
				count++;
			}
		};
		DEBUG_ShowMsg("DEBUG: Memory changed.");
		return true;
	}

	found = strstr(str,"BP ");
	if (found) { // Add new breakpoint
		found+=3;
		Bit16u seg = (Bit16u)GetHexValue(found,found);found++; // skip ":"
		Bit32u ofs = GetHexValue(found,found);
		CBreakpoint::AddBreakpoint(seg,ofs,false);
		DEBUG_ShowMsg("DEBUG: Set breakpoint at %04X:%04X",seg,ofs);
		return true;
	}
#if C_HEAVY_DEBUG
	found = strstr(str,"BPM ");
	if (found) { // Add new breakpoint
		found+=3;
		Bit16u seg = (Bit16u)GetHexValue(found,found);found++; // skip ":"
		Bit32u ofs = GetHexValue(found,found);
		CBreakpoint::AddMemBreakpoint(seg,ofs);
		DEBUG_ShowMsg("DEBUG: Set memory breakpoint at %04X:%04X",seg,ofs);
		return true;
	}
	found = strstr(str,"BPPM ");
	if (found) { // Add new breakpoint
		found+=4;
		Bit16u seg = (Bit16u)GetHexValue(found,found);found++; // skip ":"
		Bit32u ofs = GetHexValue(found,found);
		CBreakpoint* bp = CBreakpoint::AddMemBreakpoint(seg,ofs);
		if (bp) bp->SetType(BKPNT_MEMORY_PROT);
		DEBUG_ShowMsg("DEBUG: Set prot-mode memory breakpoint at %04X:%08X",seg,ofs);
		return true;
	}
	found = strstr(str,"BPLM ");
	if (found) { // Add new breakpoint
		found+=4;
		Bitu ofs		= GetHexValue(found,found);
		CBreakpoint* bp = CBreakpoint::AddMemBreakpoint(0,ofs);
		if (bp) bp->SetType(BKPNT_MEMORY_LINEAR);
		DEBUG_ShowMsg("DEBUG: Set linear memory breakpoint at %08X",ofs);
		return true;
	}
#endif
	found = strstr(str,"BPINT");
	if (found) { // Add Interrupt Breakpoint
		found+=5;
		Bit8u intNr	= (Bit8u)GetHexValue(found,found); found++;
		Bit8u valAH	= (Bit8u)GetHexValue(found,found);
		if ((valAH==0x00) && (*found=='*')) {
			CBreakpoint::AddIntBreakpoint(intNr,BPINT_ALL,false);
			DEBUG_ShowMsg("DEBUG: Set interrupt breakpoint at INT %02X",intNr);
		} else {
			CBreakpoint::AddIntBreakpoint(intNr,valAH,false);
			DEBUG_ShowMsg("DEBUG: Set interrupt breakpoint at INT %02X AH=%02X",intNr,valAH);
		}
		return true;
	}
	found = strstr(str,"BPLIST");
	if (found) {
		wprintw(dbg.win_out,"Breakpoint list:\n");
		wprintw(dbg.win_out,"-------------------------------------------------------------------------\n");
		CBreakpoint::ShowList();
		return true;
	};

	found = strstr(str,"BPDEL");
	if (found) { // Delete Breakpoints
		found+=5;
		Bit8u bpNr	= (Bit8u)GetHexValue(found,found); 
		if ((bpNr==0x00) && (*found=='*')) { // Delete all
			CBreakpoint::DeleteAll();		
			DEBUG_ShowMsg("DEBUG: Breakpoints deleted.");
		} else {		
			// delete single breakpoint
			CBreakpoint::DeleteByIndex(bpNr);
		}
		return true;
	}
	found = strstr(str,"C ");
	if (found==(char*)str) { // Set code overview
		found++;
		Bit16u codeSeg = (Bit16u)GetHexValue(found,found); found++;
		Bit32u codeOfs = GetHexValue(found,found);
		DEBUG_ShowMsg("DEBUG: Set code overview to %04X:%04X",codeSeg,codeOfs);
		codeViewData.useCS	= codeSeg;
		codeViewData.useEIP = codeOfs;
		return true;
	}
	found = strstr(str,"D ");
	if (found==(char*)str) { // Set data overview
		found++;
		dataSeg = (Bit16u)GetHexValue(found,found); found++;
		dataOfs = GetHexValue(found,found);
		DEBUG_ShowMsg("DEBUG: Set data overview to %04X:%04X",dataSeg,dataOfs);
		return true;
	}
#if C_HEAVY_DEBUG
	found = strstr(str,"LOG ");
	if (found) { // Create Cpu log file
		found+=4;
		DEBUG_ShowMsg("DEBUG: Starting log");
//		DEBUG_Log_Loop(GetHexValue(found,found));
		cpuLogFile = fopen("LOGCPU.TXT","wt");
		if (!cpuLogFile) {
			DEBUG_ShowMsg("DEBUG: Logfile couldnt be created.");
			return false;
		}
		cpuLog = true;
		cpuLogCounter = GetHexValue(found,found);

		debugging=false;
		CBreakpoint::ActivateBreakpoints(SegPhys(cs)+reg_eip,true);						
		ignoreAddressOnce = SegPhys(cs)+reg_eip;
		DOSBOX_SetNormalLoop();	
		return true;
	}
#endif
	found = strstr(str,"INTT ");
	if (found) { // Create Cpu log file
		found+=4;
		Bit8u intNr = (Bit8u)GetHexValue(found,found);
		DEBUG_ShowMsg("DEBUG: Tracing INT %02X",intNr);
		CPU_HW_Interrupt(intNr);
		SetCodeWinStart();
		return true;
	}
	found = strstr(str,"INT ");
	if (found) { // Create Cpu log file
		found+=4;
		Bit8u intNr = (Bit8u)GetHexValue(found,found);
		DEBUG_ShowMsg("DEBUG: Starting INT %02X",intNr);
		CBreakpoint::AddBreakpoint		(SegValue(cs),reg_eip, true);
		CBreakpoint::ActivateBreakpoints(SegPhys(cs)+reg_eip-1,true);
		debugging=false;
		DrawCode();
		DOSBOX_SetNormalLoop();
		CPU_HW_Interrupt(intNr);
		return true;
	}	
	found = strstr(str,"SELINFO ");
	if (found) {
		found += 8;
		while (found[0]==' ') found++;
		char out1[200],out2[200];
		GetDescriptorInfo(found,out1,out2);
		DEBUG_ShowMsg("SelectorInfo %s:",found);
		DEBUG_ShowMsg("%s",out1);
		DEBUG_ShowMsg("%s",out2);
	};

	found = strstr(str,"GDT");
	if (found) {
		LogGDT();
	}

	found = strstr(str,"LDT");
	if (found) {
		LogLDT();
	}

	found = strstr(str,"IDT");
	if (found) {
		LogIDT();
	}

	found = strstr(str,"INTVEC ");
	if (found)
	{
		found += 7;
		while (found[0]==' ') found++;
		if (found[0] != 0)
			OutputVecTable(found);
	}

	found = strstr(str,"INTHAND ");
	if (found)
	{
		found += 8;
		while (found[0]==' ') found++;
		if (found[0] != 0)
		{
			Bit8u intNr = (Bit8u)GetHexValue(found,found);
			DEBUG_ShowMsg("DEBUG: Set code overview to interrupt handler %X",intNr);
			codeViewData.useCS	= mem_readw(intNr*4+2);
			codeViewData.useEIP = mem_readw(intNr*4);
			return true;
		}
	}

	/*	found = strstr(str,"EXCEPTION ");
	if (found) {
		found += 9;
		Bit8u num = GetHexValue(found,found);		
		DPMI_CreateException(num,0xDD);
		DEBUG_ShowMsg("Exception %04X",num);
	};
*/	

#if C_HEAVY_DEBUG
	found = strstr(str,"HEAVYLOG");
	if (found) { // Create Cpu log file
		logHeavy = !logHeavy;
		if (logHeavy)	DEBUG_ShowMsg("DEBUG: Heavy cpu logging on.");
		else			DEBUG_ShowMsg("DEBUG: Heavy cpu logging off.");
		return true;
	}
#endif
	if ((*str=='H') || (*str=='?')) {
		wprintw(dbg.win_out,"Debugger keys:\n");
		wprintw(dbg.win_out,"--------------------------------------------------------------------------\n");
		wprintw(dbg.win_out,"F5                        - Run\n");
		wprintw(dbg.win_out,"F9                        - Set/Remove breakpoint\n");
		wprintw(dbg.win_out,"F10/F11                   - Step over / trace into instruction\n");
		wprintw(dbg.win_out,"Up/Down                   - Move code view cursor\n");
		wprintw(dbg.win_out,"Return                    - Enable command line input\n");
		wprintw(dbg.win_out,"D/E/S/X/B                 - Set data view to DS:SI/ES:DI/SS:SP/DS:DX/ES:BX\n");
		wprintw(dbg.win_out,"R/F                       - Scroll data view\n");
		wprintw(dbg.win_out,"V                         - Toggle additional info\n");
		wprintw(dbg.win_out,"Debugger commands (enter all values in hex or as register):\n");
		wprintw(dbg.win_out,"--------------------------------------------------------------------------\n");
		wprintw(dbg.win_out,"BP     [segment]:[offset] - Set breakpoint\n");
		wprintw(dbg.win_out,"BPINT  [intNr] *          - Set interrupt breakpoint\n");
		wprintw(dbg.win_out,"BPINT  [intNr] [ah]       - Set interrupt breakpoint with ah\n");
#if C_HEAVY_DEBUG
		wprintw(dbg.win_out,"BPM    [segment]:[offset] - Set memory breakpoint (memory change)\n");
		wprintw(dbg.win_out,"BPPM   [selector]:[offset]- Set pmode-memory breakpoint (memory change)\n");
		wprintw(dbg.win_out,"BPLM   [linear address]   - Set linear memory breakpoint (memory change)\n");
#endif
		wprintw(dbg.win_out,"BPLIST                    - List breakpoints\n");		
		wprintw(dbg.win_out,"BPDEL  [bpNr] / *         - Delete breakpoint nr / all\n");
		wprintw(dbg.win_out,"C / D  [segment]:[offset] - Set code / data view address\n");
		wprintw(dbg.win_out,"INT [nr] / INTT [nr]      - Execute / Trace into Iinterrupt\n");
		wprintw(dbg.win_out,"LOG [num]                 - Write cpu log file\n");
#if C_HEAVY_DEBUG
		wprintw(dbg.win_out,"HEAVYLOG                  - Enable/Disable automatic cpu log for INT CD\n");
#endif
		wprintw(dbg.win_out,"SR [reg] [value]          - Set register value\n");
		wprintw(dbg.win_out,"SM [seg]:[off] [val] [.]..- Set memory with following values\n");	
	
		wprintw(dbg.win_out,"IV [seg]:[off] [name]     - Create var name for memory address\n");
		wprintw(dbg.win_out,"SV [filename]             - Save var list in file\n");
		wprintw(dbg.win_out,"LV [seg]:[off] [name]     - Load var list from file\n");

		wprintw(dbg.win_out,"MEMDUMP [seg]:[off] [len] - Write memory to file memdump.txt\n");
		wprintw(dbg.win_out,"SELINFO [segName]         - Show selector info\n");

		wprintw(dbg.win_out,"INTVEC [filename]         - Writes interrupt vector table to file\n");
		wprintw(dbg.win_out,"INTHAND [intNum]          - Set code view to interrupt handler\n");

		wprintw(dbg.win_out,"H                         - Help\n");
		
		wrefresh(dbg.win_out);
		return TRUE;
	}
	return false;
};

char* AnalyzeInstruction(char* inst, bool saveSelector)
{
	static char result[256];
	
	char instu[256];
	char prefix[3];
	Bit16u seg;

	strcpy(instu,inst);
	upcase(instu);

	result[0] = 0;
	char* pos = strchr(instu,'[');
	if (pos) {
		// Segment prefix ?
		if (*(pos-1)==':') {
			char* segpos = pos-3;
			prefix[0] = tolower(*segpos);
			prefix[1] = tolower(*(segpos+1));
			prefix[2] = 0;
			seg = (Bit16u)GetHexValue(segpos,segpos);
		} else {
			if (strstr(pos,"SP") || strstr(pos,"BP")) {
				seg = SegValue(ss);
				strcpy(prefix,"ss");
			} else {
				seg = SegValue(ds);
				strcpy(prefix,"ds");
			};
		};

		pos++;
		Bit32u adr = GetHexValue(pos,pos);
		while (*pos!=']') {
			if (*pos=='+') {
				pos++;
				adr += GetHexValue(pos,pos);
			} else if (*pos=='-') {
				pos++;
				adr -= GetHexValue(pos,pos); 
			} else 
				pos++;
		};
		Bit32u address = GetAddress(seg,adr);
		if (!(paging.tlb.handler[address >> 12]->flags & PFLAG_INIT)) {
			static char outmask[] = "%s:[%04X]=%02X";
			
			if (cpu.pmode) outmask[6] = '8';
				switch (DasmLastOperandSize()) {
				case 8 : {	Bit8u val = mem_readb(address);
							outmask[12] = '2';
							sprintf(result,outmask,prefix,adr,val);
						}	break;
				case 16: {	Bit16u val = mem_readw(address);
							outmask[12] = '4';
							sprintf(result,outmask,prefix,adr,val);
						}	break;
				case 32: {	Bit32u val = mem_readd(address);
							outmask[12] = '8';
							sprintf(result,outmask,prefix,adr,val);
						}	break;
			}
		} else {
			sprintf(result,"[illegal]");
		}
		// Variable found ?
		CDebugVar* var = CDebugVar::FindVar(address);
		if (var) {
			// Replace occurance
			char* pos1 = strchr(inst,'[');
			char* pos2 = strchr(inst,']');
			if (pos1 && pos2) {
				char temp[256];
				strcpy(temp,pos2);				// save end
				pos1++; *pos1 = 0;				// cut after '['
				strcat(inst,var->GetName());	// add var name
				strcat(inst,temp);				// add end
			};
		};
		// show descriptor info, if available
		if ((cpu.pmode) && saveSelector) {
			strcpy(curSelectorName,prefix);
		};
	};
	// If it is a callback add additional info
	pos = strstr(inst,"callback");
	if (pos) {
		pos += 9;
		Bitu nr = GetHexValue(pos,pos);
		const char* descr = CALLBACK_GetDescription(nr);
		if (descr) {
			strcat(inst,"  ("); strcat(inst,descr); strcat(inst,")");
		}
	};
	return result;
};


Bit32u DEBUG_CheckKeys(void) {

	if (codeViewData.inputMode) {
		int key = getch();
		if (key>0) {
			switch (key) {
				case 0x0A:	codeViewData.inputMode = FALSE;
							ParseCommand(codeViewData.inputStr);
							break;
				case 0x107:     //backspace (linux)
				case 0x08:	// delete
							if (strlen(codeViewData.inputStr)>0) codeViewData.inputStr[strlen(codeViewData.inputStr)-1] = 0;
							break;
				default	:	if ((key>=32) && (key<=128) && (strlen(codeViewData.inputStr)<253)) {
								Bit32u len = strlen(codeViewData.inputStr);
								codeViewData.inputStr[len]	= char(key);
								codeViewData.inputStr[len+1] = 0;
							}
							break;
			}
			DEBUG_DrawScreen();
		}
		return 0;
	};	
	
	int key=getch();
	Bits ret=0;
	if (key>0) {
		switch (toupper(key)) {
		case '1':
			CPU_Cycles = 100;
			ret=(*cpudecoder)();
			break;
		case '2':
			CPU_Cycles = 500;
			ret=(*cpudecoder)();
			break;
		case '3':
			CPU_Cycles = 1000;
			ret=(*cpudecoder)();
			break;
		case '4':
			CPU_Cycles = 5000;
			ret=(*cpudecoder)();
			break;
		case '5':
			CPU_Cycles = 10000;
			ret=(*cpudecoder)();
			break;
		case 'q':
			CPU_Cycles = 5;
			ret=(*cpudecoder)();
			break;
		case 'D':	dataSeg = SegValue(ds);
					dataOfs = reg_si;
					break;
		case 'E':	dataSeg = SegValue(es);
					dataOfs = reg_di;
					break;
		case 'X':	dataSeg = SegValue(ds);
					dataOfs = reg_dx;
					break;
		case 'B':	dataSeg = SegValue(es);
					dataOfs = reg_bx;
					break;
		case 'S':	dataSeg	= SegValue(ss);
					dataOfs	= reg_sp;
					break;

		case 'R' :	dataOfs -= 16;	break;
		case 'F' :	dataOfs += 16;	break;
		case 'H' :  strcpy(codeViewData.inputStr,"H ");
					ParseCommand(codeViewData.inputStr);
					break;
		case 'T'  :	DEBUG_RaiseTimerIrq(); 
					DEBUG_ShowMsg("Debug: Timer Int started.");
					break;
		case 'V'  : showExtend = !showExtend;	
					break;

		case 0x0A	:	// Return : input
						codeViewData.inputMode = true;
						codeViewData.inputStr[0] = 0;
						break;
		case KEY_DOWN:	// down 
						if (codeViewData.cursorPos<9) codeViewData.cursorPos++;
						else codeViewData.useEIP += codeViewData.firstInstSize;	
						break;
		case KEY_UP:	// up 
						if (codeViewData.cursorPos>0) codeViewData.cursorPos--;
						else codeViewData.useEIP -= 1;
						break;
		case KEY_F(5):	// Run Programm
						debugging=false;
						CBreakpoint::ActivateBreakpoints(SegPhys(cs)+reg_eip,true);						
						ignoreAddressOnce = SegPhys(cs)+reg_eip;
						DOSBOX_SetNormalLoop();	
						break;
		case KEY_F(9):	// Set/Remove TBreakpoint
						{	PhysPt ptr = GetAddress(codeViewData.cursorSeg,codeViewData.cursorOfs);
							if (CBreakpoint::IsBreakpoint(ptr)) CBreakpoint::DeleteBreakpoint(ptr);
							else								CBreakpoint::AddBreakpoint(codeViewData.cursorSeg, codeViewData.cursorOfs, false);
						}
						break;
		case KEY_F(10):	// Step over inst
						if (StepOver()) return 0;
						else {
							exitLoop = false;
							skipFirstInstruction = true; // for heavy debugger
							CPU_Cycles = 1;
							Bitu ret=(*cpudecoder)();
							SetCodeWinStart();
							CBreakpoint::ignoreOnce = 0;
						}
						break;
		case KEY_F(11):	// trace into
						exitLoop = false;
						skipFirstInstruction = true; // for heavy debugger
						CPU_Cycles = 1;
						ret = (*cpudecoder)();
						SetCodeWinStart();
						CBreakpoint::ignoreOnce = 0;
						break;
		}
		if (ret<0) return ret;
		if (ret>0){
			ret=(*CallBack_Handlers[ret])();
			if (ret) {
				exitLoop=true;
				CPU_Cycles=CPU_CycleLeft=0;
				return ret;
			}
		}
		ret=0;
		DEBUG_DrawScreen();
	}
	return ret;
};

Bitu DEBUG_Loop(void) {
//TODO Disable sound
	GFX_Events();
	// Interrupt started ? - then skip it
	Bit16u oldCS	= SegValue(cs);
	Bit32u oldEIP	= reg_eip;
	PIC_runIRQs();
	if ((oldCS!=SegValue(cs)) || (oldEIP!=reg_eip)) {
		CBreakpoint::AddBreakpoint(oldCS,oldEIP,true);
		CBreakpoint::ActivateBreakpoints(SegPhys(cs)+reg_eip,true);
		debugging=false;
		DOSBOX_SetNormalLoop();
		return 0;
	}
	return DEBUG_CheckKeys();
}

void DEBUG_Enable(void) {

	debugging=true;
	SetCodeWinStart();
	DEBUG_DrawScreen();
	DOSBOX_SetLoop(&DEBUG_Loop);
}

void DEBUG_DrawScreen(void) {
	DrawData();
	DrawCode();
	DrawRegisters();
	DrawVariables();
}

static void DEBUG_RaiseTimerIrq(void) {
	PIC_ActivateIRQ(0);
}

static void LogGDT(void)
{
	char out1[512];
	Descriptor desc;
	Bitu length = cpu.gdt.GetLimit();
	PhysPt address = cpu.gdt.GetBase();
	PhysPt max	   = address + length;
	Bitu i = 0;
	LOG(LOG_MISC,LOG_ERROR)("GDT Base:%08X Limit:%08X",address,length);
	while (address<max) {
		desc.Load(address);
		sprintf(out1,"%04X: b:%08X type: %02X parbg",(i<<3),desc.GetBase(),desc.saved.seg.type);
		LOG(LOG_MISC,LOG_ERROR)(out1);
		sprintf(out1,"      l:%08X dpl : %01X  %1X%1X%1X%1X%1X",desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
		LOG(LOG_MISC,LOG_ERROR)(out1);
		address+=8; i++;
	};
};

static void LogLDT(void)
{
	char out1[512];
	Descriptor desc;
	Bitu ldtSelector = cpu.gdt.SLDT();
	cpu.gdt.GetDescriptor(ldtSelector,desc);
	Bitu length = desc.GetLimit();
	PhysPt address = desc.GetBase();
	PhysPt max	   = address + length;
	Bitu i = 0;
	LOG(LOG_MISC,LOG_ERROR)("LDT Base:%08X Limit:%08X",address,length);
	while (address<max) {
		desc.Load(address);
		sprintf(out1,"%04X: b:%08X type: %02X parbg",(i<<3)|4,desc.GetBase(),desc.saved.seg.type);
		LOG(LOG_MISC,LOG_ERROR)(out1);
		sprintf(out1,"      l:%08X dpl : %01X  %1X%1X%1X%1X%1X",desc.GetLimit(),desc.saved.seg.dpl,desc.saved.seg.p,desc.saved.seg.avl,desc.saved.seg.r,desc.saved.seg.big,desc.saved.seg.g);
		LOG(LOG_MISC,LOG_ERROR)(out1);
		address+=8; i++;
	};
};

static void LogIDT(void)
{
	char out1[512];
	Descriptor desc;
	Bitu address = 0;
	while (address<256*8) {
		cpu.idt.GetDescriptor(address,desc);
		sprintf(out1,"%04X: sel:%04X off:%02X",address/8,desc.GetSelector(),desc.GetOffset());
		LOG(LOG_MISC,LOG_ERROR)(out1);
		address+=8;
	};
};

static void LogInstruction(Bit16u segValue, Bit32u eipValue, char* buffer) 
{
	static char empty[15] = { 32,32,32,32,32,32,32,32,32,32,32,32,32,32,0 };

	PhysPt start = GetAddress(segValue,eipValue);
	char dline[200];Bitu size;
	size = DasmI386(dline, start, reg_eip, cpu.code.big);
	Bitu len = strlen(dline);
	char* res = empty;
	if (showExtend) {
		res = AnalyzeInstruction(dline,false);
		if (!res || (strlen(res)==0)) res = empty;
	};
	
	if (len<30) for (Bitu i=0; i<30-len; i++) strcat(dline," ");	
	// Get register values
	sprintf(buffer,"%04X:%08X   %s  %s  EAX:%08X EBX:%08X ECX:%08X EDX:%08X ESI:%08X EDI:%08X EBP:%08X ESP:%08X DS:%04X ES:%04X FS:%04X GS:%04X SS:%04X CF:%01X ZF:%01X SF:%01X OF:%01X AF:%01X PF:%01X IF:%01X\n",segValue,eipValue,dline,res,reg_eax,reg_ebx,reg_ecx,reg_edx,reg_esi,reg_edi,reg_ebp,reg_esp,SegValue(ds),SegValue(es),SegValue(fs),SegValue(gs),SegValue(ss),
		GETFLAGBOOL(CF),GETFLAGBOOL(ZF),GETFLAGBOOL(SF),GETFLAGBOOL(OF),GETFLAGBOOL(AF),GETFLAGBOOL(PF),GETFLAGBOOL(IF));
};

static bool DEBUG_Log_Loop(int count) {

	char buffer[512];	
	
	FILE* f = fopen("LOGCPU.TXT","wt");
	if (!f) return false;

	do {
//		PIC_runIRQs();
				
		Bitu ret;
		do {
			// Get disasm
			Bit16u csValue	= SegValue(cs);
			Bit32u eipValue = reg_eip;
			
			LogInstruction(csValue,eipValue,buffer);
			fprintf(f,"%s",buffer);
						
			CPU_Cycles = 1;
			ret=(*cpudecoder)();
			if (ret>0) ret=(*CallBack_Handlers[ret])();

			count--;
			if (count==0) break;

		} while (!ret);
		if (ret) break;
	} while (count>0);
	
	fclose(f);
	return true;
}

// DEBUG.COM stuff

class DEBUG : public Program {
public:
	DEBUG()		{ pDebugcom	= this;	active = false; };
	~DEBUG()	{ pDebugcom	= 0; };

	bool IsActive() { return active; };

	void Run(void)
	{
		char filename[128];
		char args[256];
		cmd->FindCommand(1,temp_line);
		strncpy(filename,temp_line.c_str(),128);
		// Read commandline
		Bit16u i	=2;
		bool ok		= false; 
		args[0]		= 0;
		do {
			ok = cmd->FindCommand(i++,temp_line);
			strncat(args,temp_line.c_str(),256);
			strncat(args," ",256);
		} while (ok);
		// Start new shell and execute prog		
		active = true;
		// Save cpu state....
		Bit16u oldcs	= SegValue(cs);
		Bit32u oldeip	= reg_eip;	
		Bit16u oldss	= SegValue(ss);
		Bit32u oldesp	= reg_esp;

		// Workaround : Allocate Stack Space
		Bit16u segment;
		Bit16u size = 0x200 / 0x10;
		if (DOS_AllocateMemory(&segment,&size)) {
			SegSet16(ss,segment);
			reg_sp = 0x200;
			// Start shell
			DOS_Shell shell;
			shell.Execute(filename,args);
			DOS_FreeMemory(segment);
		}
		// set old reg values
		SegSet16(ss,oldss);
		reg_esp = oldesp;
		SegSet16(cs,oldcs);
		reg_eip = oldeip;
	};

private:
	bool	active;
};

void DEBUG_CheckExecuteBreakpoint(Bit16u seg, Bit32u off)
{
	if (pDebugcom && pDebugcom->IsActive()) {
		CBreakpoint::AddBreakpoint(seg,off,true);		
		CBreakpoint::ActivateBreakpoints(SegPhys(cs)+reg_eip,true);	
		pDebugcom = 0;
	};
};

Bitu DEBUG_EnableDebugger(void)
{
	exitLoop = true;
	DEBUG_Enable();
	CPU_Cycles=CPU_CycleLeft=0;
	return 0;
};

static void DEBUG_ProgramStart(Program * * make) {
	*make=new DEBUG;
}

// INIT 

void DEBUG_SetupConsole(void)
{
	#ifdef WIN32
	WIN32_Console();
	#else
	tcgetattr(0,&consolesettings);
	printf("\e[8;50;80t"); //resize terminal
	fflush(NULL);
	#endif	
	memset((void *)&dbg,0,sizeof(dbg));
	debugging=false;
	dbg.active_win=3;
	input_count=0;
	/* Start the Debug Gui */
	DBGUI_StartUp();
};

static void DEBUG_ShutDown(Section * sec) 
{
	CBreakpoint::DeleteAll();
	CDebugVar::DeleteAll();
	#ifndef WIN32
	curs_set(old_cursor_state);
	tcsetattr(0, TCSANOW,&consolesettings);
	printf("\e[0m\e[2J");
	fflush(NULL);
	#endif
};

Bitu debugCallback;

void DEBUG_Init(Section* sec) {

	MSG_Add("DEBUG_CONFIGFILE_HELP","Nothing to setup yet!\n");
	DEBUG_DrawScreen();
	/* Add some keyhandlers */
	MAPPER_AddHandler(DEBUG_Enable,MK_pause,0,"debugger","Debugger");
	/* Clear the TBreakpoint list */
	memset((void*)&codeViewData,0,sizeof(codeViewData));
	/* setup debug.com */
	PROGRAMS_MakeFile("DEBUG.COM",DEBUG_ProgramStart);
	/* Setup callback */
	debugCallback=CALLBACK_Allocate();
	CALLBACK_Setup(debugCallback,DEBUG_EnableDebugger,CB_RETF);
	/* shutdown function */
	sec->AddDestroyFunction(&DEBUG_ShutDown);
}

// DEBUGGING VAR STUFF

void CDebugVar::InsertVariable(char* name, PhysPt adr)
{
	varList.push_back(new CDebugVar(name,adr));
};

void CDebugVar::DeleteAll(void) 
{
	std::list<CDebugVar*>::iterator i;
	CDebugVar* bp;
	for(i=varList.begin(); i != varList.end(); i++) {
		bp = static_cast<CDebugVar*>(*i);
		delete bp;
	};
	(varList.clear)();
};

CDebugVar* CDebugVar::FindVar(PhysPt pt)
{
	std::list<CDebugVar*>::iterator i;
	CDebugVar* bp;
	for(i=varList.begin(); i != varList.end(); i++) {
		bp = static_cast<CDebugVar*>(*i);
		if (bp->GetAdr()==pt) return bp;
	};
	return 0;
};

bool CDebugVar::SaveVars(char* name)
{
	FILE* f = fopen(name,"wb+");
	if (!f) return false;

	// write number of vars
	Bit16u num = varList.size();
	fwrite(&num,1,sizeof(num),f);

	std::list<CDebugVar*>::iterator i;
	CDebugVar* bp;
	for(i=varList.begin(); i != varList.end(); i++) {
		bp = static_cast<CDebugVar*>(*i);
		// name
		fwrite(bp->GetName(),1,16,f);
		// adr
		PhysPt adr = bp->GetAdr();
		fwrite(&adr,1,sizeof(adr),f);
	};
	fclose(f);
	return true;
};

bool CDebugVar::LoadVars(char* name)
{
	FILE* f = fopen(name,"rb");
	if (!f) return false;

	// read number of vars
	Bit16u num;
	fread(&num,1,sizeof(num),f);

	for (Bit16u i=0; i<num; i++) {
		char name[16];
		// name
		fread(name,1,16,f);
		// adr
		PhysPt adr;
		fread(&adr,1,sizeof(adr),f);
		// insert
		InsertVariable(name,adr);
	};
	fclose(f);
	return true;
};

static void SaveMemory(Bitu seg, Bitu ofs1, Bit32s num)
{
	FILE* f = fopen("MEMDUMP.TXT","wt");
	if (!f) {
		DEBUG_ShowMsg("DEBUG: Memory dump failed.");
		return;
	}
	
	char buffer[128];
	char temp[16];

	while(num>0) {

		sprintf(buffer,"%04X:%04X   ",seg,ofs1);
		for (Bit16u x=0; x<16; x++) {
			sprintf	(temp,"%02X ",mem_readb(GetAddress(seg,ofs1+x)));
			strcat	(buffer,temp);
		};
		ofs1+=16;
		num-=16;

		fprintf(f,"%s\n",buffer);
	};
	fclose(f);
	DEBUG_ShowMsg("DEBUG: Memory dump success.");
};

static void OutputVecTable(char* filename)
{
	FILE* f = fopen(filename, "wt");
	if (!f)
	{
		DEBUG_ShowMsg("DEBUG: Output of interrupt vector table failed.");
		return;
	}

	for (int i=0; i<256; i++)
		fprintf(f,"INT %02X:  %04X:%04X\n", i, mem_readw(i*4+2), mem_readw(i*4));

	fclose(f);
	DEBUG_ShowMsg("DEBUG: Interrupt vector table written to %s.", filename);
}

#define DEBUG_VAR_BUF_LEN 16
static void DrawVariables(void)
{
	std::list<CDebugVar*>::iterator i;
	CDebugVar *dv;
	char buffer[DEBUG_VAR_BUF_LEN];

	int idx = 0;
	for(i=CDebugVar::varList.begin(); i != CDebugVar::varList.end(); i++, idx++) {

		if (idx == 4*3) {
			/* too many variables */
			break;
		}

		dv = static_cast<CDebugVar*>(*i);

		Bit16u value = mem_readw(dv->GetAdr());
		snprintf(buffer,DEBUG_VAR_BUF_LEN, "0x%04x", value);

		int y = idx / 3;
		int x = (idx % 3) * 26;
		mvwprintw(dbg.win_var, y, x, dv->GetName());
		mvwprintw(dbg.win_var, y,  (x + DEBUG_VAR_BUF_LEN + 1) , buffer);
	}

	wrefresh(dbg.win_var);
}
#undef DEBUG_VAR_BUF_LEN
// HEAVY DEBUGGING STUFF

#if C_HEAVY_DEBUG

const Bit32u LOGCPUMAX = 20000;

static Bit16u logCpuCS [LOGCPUMAX];
static Bit32u logCpuEIP[LOGCPUMAX];
static Bit32u logCount = 0;

typedef struct SLogInst {
	char buffer[512];
} TLogInst;

TLogInst logInst[LOGCPUMAX];

void DEBUG_HeavyLogInstruction(void)
{
	LogInstruction(SegValue(cs),reg_eip,logInst[logCount++].buffer);
	if (logCount>=LOGCPUMAX) {
		logCount = 0;
	};
};

void DEBUG_HeavyWriteLogInstruction(void)
{
	if (!logHeavy) return;

	logHeavy = false;
	
	DEBUG_ShowMsg("DEBUG: Creating cpu log LOGCPU_INT_CD.TXT");

	FILE* f = fopen("LOGCPU_INT_CD.TXT","wt");
	if (!f) {
		DEBUG_ShowMsg("DEBUG: Failed.");	
		return;
	}

	Bit32u startLog = logCount;
	do {
		// Write Intructions
		fprintf(f,"%s",logInst[startLog++].buffer);
		if (startLog>=LOGCPUMAX) startLog = 0;
	} while (startLog!=logCount);
	
	fclose(f);

	DEBUG_ShowMsg("DEBUG: Done.");	
};

bool DEBUG_HeavyIsBreakpoint(void)
{
	if (cpuLog) {
		if (cpuLogCounter>0) {
			static char buffer[4096];
			LogInstruction(SegValue(cs),reg_eip,buffer);
			fprintf(cpuLogFile,"%s",buffer);
			cpuLogCounter--;
		}
		if (cpuLogCounter<=0) {
			fclose(cpuLogFile);
			DEBUG_ShowMsg("DEBUG: cpu log LOGCPU.TXT created");
			cpuLog = false;
			DEBUG_EnableDebugger();
			return true;
		}
	}
	// LogInstruction
	if (logHeavy) DEBUG_HeavyLogInstruction();

	if (skipFirstInstruction) {
		skipFirstInstruction = false;
		return false;
	}
	PhysPt where = SegPhys(cs)+reg_eip;
	if (CBreakpoint::CheckBreakpoint(SegValue(cs),reg_eip)) {
		return true;	
	}
	return false;
};
#endif // HEAVY DEBUG


#endif // DEBUG


