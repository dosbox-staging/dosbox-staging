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

#include "programs.h"

#include <string.h>
#include <list>

#include "dosbox.h"
#if C_DEBUG
#include "debug.h"
#include "cpu.h"
#include "video.h"
#include "pic.h"
#include "keyboard.h"
#include "cpu.h"
#include "callback.h"
#include "inout.h"
#include "mixer.h"
#include "debug_inc.h"
#include "timer.h"
#include "../shell/shell_inc.h"

#ifdef WIN32
void WIN32_Console();
#endif

// Forwards
static void DrawCode(void);
static bool DEBUG_Log_Loop(int count);
static void DEBUG_RaiseTimerIrq(void);
class DEBUG;

DEBUG*	pDebugcom	= 0;
bool	exitLoop	= false;

static struct  {
	Bit32u eax,ebx,ecx,edx,esi,edi,ebp,esp,eip;
} oldregs;



static Segment oldsegs[6];
static Flag_Info oldflags;
DBGBlock dbg;
static char input_line[256];
static Bitu input_count;
Bitu cycle_count;
static bool debugging;
static void SetColor(bool test) {
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

static Bit16u dataSeg,dataOfs;

/********************/
/* Breakpoint stuff */
/********************/

bool skipFirstInstruction = false;

enum EBreakpoint { BKPNT_UNKNOWN, BKPNT_PHYSICAL, BKPNT_INTERRUPT };

#define BPINT_ALL 0x100

class CBreakpoint
{
public:
	CBreakpoint								(void)						{ location = 0; active = once = false; segment = 0; offset = 0; intNr = 0; ahValue = 0; type = BKPNT_UNKNOWN; };

	void					SetAddress		(Bit16u seg, Bit32u off)	{ location = PhysMake(seg,off);	type = BKPNT_PHYSICAL; segment = seg; offset = off; };
	void					SetAddress		(PhysPt adr)				{ location = adr;				type = BKPNT_PHYSICAL; };
	void					SetInt			(Bit8u _intNr, Bit16u ah)	{ intNr = _intNr, ahValue = ah; type = BKPNT_INTERRUPT; };
	void					SetOnce			(bool _once)				{ once = _once; };
	
	bool					IsActive		(void)						{ return active; };
	void					Activate		(bool _active);

	EBreakpoint				GetType			(void)						{ return type; };
	bool					GetOnce			(void)						{ return once; };
	PhysPt					GetLocation		(void)						{ if (GetType()==BKPNT_PHYSICAL)  return location;	else return 0; };
	Bit16u					GetSegment		(void)						{ return segment; };
	Bit32u					GetOffset		(void)						{ return offset; };
	Bit8u					GetIntNr		(void)						{ if (GetType()==BKPNT_INTERRUPT) return intNr;		else return 0; };
	Bit16u					GetValue		(void)						{ if (GetType()==BKPNT_INTERRUPT) return ahValue;	else return 0; };

	// statics
	static CBreakpoint*		AddBreakpoint		(Bit16u seg, Bit32u off, bool once);
	static CBreakpoint*		AddIntBreakpoint	(Bit8u intNum, Bit16u ah, bool once);
	static void				ActivateBreakpoints	(PhysPt adr, bool activate);
	static bool				CheckBreakpoint		(PhysPt adr);
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

bool CBreakpoint::CheckBreakpoint(PhysPt adr)
// Checks if breakpoint is valid an should stop execution
{
	// Search matching breakpoint
	std::list<CBreakpoint*>::iterator i;
	CBreakpoint* bp;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		bp = static_cast<CBreakpoint*>(*i);
		if ((bp->GetType()==BKPNT_PHYSICAL) && bp->IsActive() && (bp->GetLocation()==adr)) {
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
		};
	};
	return false;
};

bool CBreakpoint::CheckIntBreakpoint(PhysPt adr, Bit8u intNr, Bit16u ahValue)
// Checks if interrupt breakpoint is valid an should stop execution
{
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
	};
	return false;
};

bool CBreakpoint::DeleteBreakpoint(PhysPt where) 
{
	// Search matching breakpoint
	int nr = 0;
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
			PhysPt adr = bp->GetLocation();

			wprintw(dbg.win_out,"%02X. BP %04X:%04X\n",nr,bp->GetSegment(),bp->GetOffset());
			nr++;
		} else if (bp->GetType()==BKPNT_INTERRUPT) {
			if (bp->GetValue()==BPINT_ALL)	wprintw(dbg.win_out,"%02X. BPINT %02X\n",nr,bp->GetIntNr());					
			else							wprintw(dbg.win_out,"%02X. BPINT %02X AH=%02X\n",nr,bp->GetIntNr(),bp->GetValue());
			nr++;
		};
	}
	wrefresh(dbg.win_out);
};

bool DEBUG_Breakpoint(void)
{
	/* First get the phyiscal address and check for a set Breakpoint */
	PhysPt where=SegPhys(cs)+reg_eip-1;
	if (!CBreakpoint::CheckBreakpoint(where)) return false;
	// Found. Breakpoint is valid
	reg_eip -= 1;
	CBreakpoint::ActivateBreakpoints(SegPhys(cs)+reg_eip,false);	// Deactivate all breakpoints
	exitLoop = true;
	DEBUG_Enable();
	return true;
};

bool DEBUG_IntBreakpoint(Bit8u intNum)
{
	/* First get the phyiscal address and check for a set Breakpoint */
	PhysPt where=SegPhys(cs)+reg_eip-2;
	if (!CBreakpoint::CheckIntBreakpoint(where,intNum,reg_ah)) return false;
	// Found. Breakpoint is valid
	reg_eip -= 2;
	CBreakpoint::ActivateBreakpoints(SegPhys(cs)+reg_eip,false);	// Deactivate all breakpoints
	exitLoop = true;
	DEBUG_Enable();
	return true;
};

static bool StepOver()
{
	PhysPt start=Segs[cs].phys+reg_eip;
	char dline[200];Bitu size;
	size=DasmI386(dline, start, reg_eip, false);

	if (strstr(dline,"call") || strstr(dline,"int") || strstr(dline,"loop") || strstr(dline,"rep")) {
		CBreakpoint::AddBreakpoint		(SegValue(cs),reg_eip+size, true);
		CBreakpoint::ActivateBreakpoints(SegPhys(cs)+reg_eip, true);
		debugging=false;
		DrawCode();
		DOSBOX_SetNormalLoop();
		return true;
	} 
	return false;
};

bool DEBUG_ExitLoop(void)
{
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
	
	Bit16u add = dataOfs;
	/* Data win */	
	for (int y=0; y<8; y++) {
		// Adress
		mvwprintw (dbg.win_data,1+y,0,"%04X:%04X ",dataSeg,add);
		for (int x=0; x<16; x++) {
			Bit8u ch = real_readb(dataSeg,add);
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

	flags.cf=get_CF();SetColor(flags.cf!=oldflags.cf);oldflags.cf=flags.cf;mvwprintw (dbg.win_reg,1,53,"%01X",flags.cf);
	flags.zf=get_ZF();SetColor(flags.zf!=oldflags.zf);oldflags.zf=flags.zf;mvwprintw (dbg.win_reg,1,56,"%01X",flags.zf);
	flags.sf=get_SF();SetColor(flags.sf!=oldflags.sf);oldflags.sf=flags.sf;mvwprintw (dbg.win_reg,1,59,"%01X",flags.sf);
	flags.of=get_OF();SetColor(flags.of!=oldflags.of);oldflags.of=flags.of;mvwprintw (dbg.win_reg,1,62,"%01X",flags.of);
	flags.af=get_AF();SetColor(flags.af!=oldflags.af);oldflags.af=flags.af;mvwprintw (dbg.win_reg,1,65,"%01X",flags.af);
	flags.pf=get_PF();SetColor(flags.pf!=oldflags.pf);oldflags.pf=flags.pf;mvwprintw (dbg.win_reg,1,68,"%01X",flags.pf);


	SetColor(flags.df!=oldflags.df);oldflags.df=flags.df;mvwprintw (dbg.win_reg,1,71,"%01X",flags.df);
	SetColor(flags.intf!=oldflags.intf);oldflags.intf=flags.intf;mvwprintw (dbg.win_reg,1,74,"%01X",flags.intf);
	SetColor(flags.tf!=oldflags.tf);oldflags.tf=flags.tf;mvwprintw (dbg.win_reg,1,77,"%01X",flags.tf);

	wattrset(dbg.win_reg,0);
	mvwprintw(dbg.win_reg,3,60,"%d       ",cycle_count);
	wrefresh(dbg.win_reg);
};

static void DrawCode(void) 
{
	Bit32u disEIP = codeViewData.useEIP;
	PhysPt start  = codeViewData.useCS*16 + codeViewData.useEIP;
	char dline[200];Bitu size;Bitu c;
	
	for (Bit32u i=0;i<10;i++) {
		if (has_colors()) {
			if ((codeViewData.useCS==SegValue(cs)) && (disEIP == reg_eip)) {
				wattrset(dbg.win_code,COLOR_PAIR(PAIR_GREEN_BLACK));			
				if (codeViewData.cursorPos==-1) {
					codeViewData.cursorPos = i; // Set Cursor 
					codeViewData.cursorSeg = SegValue(cs);
					codeViewData.cursorOfs = disEIP;
				}
			} else if (i == codeViewData.cursorPos) {
				wattrset(dbg.win_code,COLOR_PAIR(PAIR_BLACK_GREY));			
				codeViewData.cursorSeg = codeViewData.useCS;
				codeViewData.cursorOfs = disEIP;
			} else if (CBreakpoint::IsBreakpointDrawn(start)) {
				wattrset(dbg.win_code,COLOR_PAIR(PAIR_GREY_RED));			
			} else {
				wattrset(dbg.win_code,0);			
			}
		}

		size=DasmI386(dline, start, disEIP, false);
		mvwprintw(dbg.win_code,i,0,"%04X:%04X  ",codeViewData.useCS,disEIP);
		for (c=0;c<size;c++) wprintw(dbg.win_code,"%02X",mem_readb(start+c));
		for (c=20;c>=size*2;c--) waddch(dbg.win_code,' ');
		waddstr(dbg.win_code,dline);
		for (c=30-strlen(dline);c>0;c--) waddch(dbg.win_code,' ');
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
	if (strstr(hex,"EAX")==hex) { hex+=3; return reg_eax; };
	if (strstr(hex,"EBX")==hex) { hex+=3; return reg_ebx; };
	if (strstr(hex,"ECX")==hex) { hex+=3; return reg_ecx; };
	if (strstr(hex,"EDX")==hex) { hex+=3; return reg_edx; };
	if (strstr(hex,"ESI")==hex) { hex+=3; return reg_esi; };
	if (strstr(hex,"EDI")==hex) { hex+=3; return reg_edi; };
	if (strstr(hex,"EBP")==hex) { hex+=3; return reg_ebp; };
	if (strstr(hex,"ESP")==hex) { hex+=3; return reg_esp; };
	if (strstr(hex,"EIP")==hex) { hex+=3; return reg_eip; };

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
	Bit32u	value = 0;

	char* hex = str;
	while (*hex==' ') hex++;
	if (strstr(hex,"AX")==hex) { hex+=2; reg_ax = GetHexValue(hex,hex); } else
	if (strstr(hex,"BX")==hex) { hex+=2; reg_bx = GetHexValue(hex,hex); } else
	if (strstr(hex,"CX")==hex) { hex+=2; reg_cx = GetHexValue(hex,hex); } else
	if (strstr(hex,"DX")==hex) { hex+=2; reg_dx = GetHexValue(hex,hex); } else
	if (strstr(hex,"SI")==hex) { hex+=2; reg_si = GetHexValue(hex,hex); } else
	if (strstr(hex,"DI")==hex) { hex+=2; reg_di = GetHexValue(hex,hex); } else
	if (strstr(hex,"BP")==hex) { hex+=2; reg_bp = GetHexValue(hex,hex); } else
	if (strstr(hex,"SP")==hex) { hex+=2; reg_sp = GetHexValue(hex,hex); } else
	if (strstr(hex,"IP")==hex) { hex+=2; reg_ip = GetHexValue(hex,hex); } else
	if (strstr(hex,"CS")==hex) { hex+=2; SegSet16(cs,GetHexValue(hex,hex)); } else
	if (strstr(hex,"DS")==hex) { hex+=2; SegSet16(ds,GetHexValue(hex,hex)); } else
	if (strstr(hex,"ES")==hex) { hex+=2; SegSet16(es,GetHexValue(hex,hex)); } else
	if (strstr(hex,"FS")==hex) { hex+=2; SegSet16(fs,GetHexValue(hex,hex)); } else
	if (strstr(hex,"GS")==hex) { hex+=2; SegSet16(gs,GetHexValue(hex,hex)); } else
	if (strstr(hex,"SS")==hex) { hex+=2; SegSet16(ss,GetHexValue(hex,hex)); } else
	if (strstr(hex,"EAX")==hex) { hex+=3; reg_eax = GetHexValue(hex,hex); } else
	if (strstr(hex,"EBX")==hex) { hex+=3; reg_ebx = GetHexValue(hex,hex); } else
	if (strstr(hex,"ECX")==hex) { hex+=3; reg_ecx = GetHexValue(hex,hex); } else
	if (strstr(hex,"EDX")==hex) { hex+=3; reg_edx = GetHexValue(hex,hex); } else
	if (strstr(hex,"ESI")==hex) { hex+=3; reg_esi = GetHexValue(hex,hex); } else
	if (strstr(hex,"EDI")==hex) { hex+=3; reg_edi = GetHexValue(hex,hex); } else
	if (strstr(hex,"EBP")==hex) { hex+=3; reg_ebp = GetHexValue(hex,hex); } else
	if (strstr(hex,"ESP")==hex) { hex+=3; reg_esp = GetHexValue(hex,hex); } else
	if (strstr(hex,"EIP")==hex) { hex+=3; reg_eip = GetHexValue(hex,hex); } else
	if (strstr(hex,"AF")==hex) { hex+=2; flags.af = (GetHexValue(hex,hex)!=0); } else
	if (strstr(hex,"CF")==hex) { hex+=2; flags.cf = (GetHexValue(hex,hex)!=0); } else
	if (strstr(hex,"DF")==hex) { hex+=2; flags.df = (GetHexValue(hex,hex)!=0); } else
	if (strstr(hex,"IF")==hex) { hex+=2; flags.intf = (GetHexValue(hex,hex)!=0); } else
	if (strstr(hex,"OF")==hex) { hex+=3; flags.of = (GetHexValue(hex,hex)!=0); } else
	if (strstr(hex,"PF")==hex) { hex+=3; flags.pf = (GetHexValue(hex,hex)!=0); } else
	if (strstr(hex,"ZF")==hex) { hex+=3; flags.zf = (GetHexValue(hex,hex)!=0); } else
								{ return false; };
	return true;
};

bool ParseCommand(char* str)
{
	char* found = str;
	while (*found) *found++=toupper(*found);
	found = strstr(str,"BP ");
	if (found) { // Add new breakpoint
		found+=3;
		Bit16u seg = GetHexValue(found,found);found++; // skip ":"
		Bit32u ofs = GetHexValue(found,found);
		CBreakpoint::AddBreakpoint(seg,ofs,false);
		LOG_DEBUG("DEBUG: Set breakpoint at %04X:%04X",seg,ofs);
		return true;
	}
	found = strstr(str,"BPINT");
	if (found) { // Add Interrupt Breakpoint
		found+=5;
		Bit8u intNr	= GetHexValue(found,found); found++;
		Bit8u valAH	= GetHexValue(found,found);
		if ((valAH==0x00) && (*found=='*')) {
			CBreakpoint::AddIntBreakpoint(intNr,BPINT_ALL,false);
			LOG_DEBUG("DEBUG: Set interrupt breakpoint at INT %02X",intNr);
		} else {
			CBreakpoint::AddIntBreakpoint(intNr,valAH,false);
			LOG_DEBUG("DEBUG: Set interrupt breakpoint at INT %02X AH=%02X",intNr,valAH);
		}
		return true;
	}
	found = strstr(str,"BPLIST");
	if (found) {
		wprintw(dbg.win_out,"Breakpoint list:\n");
		wprintw(dbg.win_out,"-------------------------------------------------------------------------\n");
		Bit32u nr = 0;
		CBreakpoint::ShowList();
		return true;
	};

	found = strstr(str,"BPDEL");
	if (found) { // Delete Breakpoints
		found+=5;
		Bit8u bpNr	= GetHexValue(found,found); 
		if ((bpNr==0x00) && (*found=='*')) { // Delete all
			CBreakpoint::DeleteAll();		
			LOG_DEBUG("DEBUG: Breakpoints deleted.");
		} else {		
			// delete single breakpoint
			CBreakpoint::DeleteByIndex(bpNr);
		}
		return true;
	}
	found = strstr(str,"C ");
	if (found) { // Set code overview
		found++;
		Bit16u codeSeg = GetHexValue(found,found); found++;
		Bit32u codeOfs = GetHexValue(found,found);
		LOG_DEBUG("DEBUG: Set code overview to %04X:%04X",codeSeg,codeOfs);
		codeViewData.useCS	= codeSeg;
		codeViewData.useEIP = codeOfs;
		return true;
	}
	found = strstr(str,"D ");
	if (found) { // Set data overview
		found++;
		dataSeg = GetHexValue(found,found); found++;
		dataOfs = GetHexValue(found,found);
		LOG_DEBUG("DEBUG: Set data overview to %04X:%04X",dataSeg,dataOfs);
		return true;
	}
	found = strstr(str,"LOG ");
	if (found) { // Create Cpu log file
		found+=4;
		LOG_DEBUG("DEBUG: Starting log");
		DEBUG_Log_Loop(GetHexValue(found,found));
		LOG_DEBUG("DEBUG: Logfile LOGCPU.TXT created.");
		return true;
	}
	found = strstr(str,"SR ");
	if (found) { // Set register value
		found+=2;
		if (ChangeRegister(found))	LOG_DEBUG("DEBUG: Set Register success.");
		else						LOG_DEBUG("DEBUG: Set Register failure.");
		return true;
	}	
	found = strstr(str,"SM ");
	if (found) { // Set memory with following values
		found+=3;
		Bit16u seg = GetHexValue(found,found); found++;
		Bit32u ofs = GetHexValue(found,found); found++;
		Bit16u count = 0;
		while (*found) {
			while (*found==' ') found++;
			if (*found) {
				Bit8u value = GetHexValue(found,found); found++;
				mem_writeb(PhysMake(seg,ofs+count),value);
				count++;
			}
		};
		LOG_DEBUG("DEBUG: Memory changed.");
		return true;
	}
	found = strstr(str,"INTT ");
	if (found) { // Create Cpu log file
		found+=4;
		Bit8u intNr = GetHexValue(found,found);
		LOG_DEBUG("DEBUG: Tracing INT %02X",intNr);
		Interrupt(intNr);
		SetCodeWinStart();
		return true;
	}
	found = strstr(str,"INT ");
	if (found) { // Create Cpu log file
		found+=4;
		Bit8u intNr = GetHexValue(found,found);
		LOG_DEBUG("DEBUG: Starting INT %02X",intNr);
		CBreakpoint::AddBreakpoint		(SegValue(cs),reg_eip, true);
		CBreakpoint::ActivateBreakpoints(SegPhys(cs)+reg_eip-1,true);
		debugging=false;
		DrawCode();
		DOSBOX_SetNormalLoop();
		Interrupt(intNr);
		return true;
	}
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
		wprintw(dbg.win_out,"Debugger commands (enter all values in hex or as register):\n");
		wprintw(dbg.win_out,"--------------------------------------------------------------------------\n");
		wprintw(dbg.win_out,"BP     [segment]:[offset] - Set breakpoint\n");
		wprintw(dbg.win_out,"BPINT  [intNr] *          - Set interrupt breakpoint\n");
		wprintw(dbg.win_out,"BPINT  [intNr] [ah]       - Set interrupt breakpoint with ah\n");
		wprintw(dbg.win_out,"BPLIST                    - List breakpoints\n");		
		wprintw(dbg.win_out,"BPDEL  [bpNr] / *         - Delete breakpoint nr / all\n");
		wprintw(dbg.win_out,"C / D  [segment]:[offset] - Set code / data view address\n");
		wprintw(dbg.win_out,"INT [nr] / INTT [nr]      - Execute / Trace into Iinterrupt\n");
		wprintw(dbg.win_out,"LOG [num]                 - Write cpu log file\n");
		wprintw(dbg.win_out,"SR [reg] [value]          - Set register value\n");
		wprintw(dbg.win_out,"SM [seg]:[off] [val] [.]..- Set memory with following values\n");	
		wprintw(dbg.win_out,"H                         - Help\n");
		wrefresh(dbg.win_out);
		return TRUE;
	}
	return false;
};

Bit32u DEBUG_CheckKeys(void) {

	if (codeViewData.inputMode) {
		int key = getch();
		if (key>0) {
			switch (key) {
				case 0x0A:	codeViewData.inputMode = FALSE;
							ParseCommand(codeViewData.inputStr);
							break;
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
	Bit32u ret=0;
	if (key>0) {
		switch (toupper(key)) {
		case '1':
			ret=(*cpudecoder)(100);
			break;
		case '2':
			ret=(*cpudecoder)(500);
			break;
		case '3':
			ret=(*cpudecoder)(1000);
			break;
		case '4':
			ret=(*cpudecoder)(5000);
			break;
		case '5':
			ret=(*cpudecoder)(10000);
			break;
		case 'q':
			ret=(*cpudecoder)(5);
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
					LOG_DEBUG("Debug: Timer Int started.");
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
						DOSBOX_SetNormalLoop();	
						break;
		case KEY_F(9):	// Set/Remove TBreakpoint
						{	PhysPt ptr = PhysMake(codeViewData.cursorSeg,codeViewData.cursorOfs);
							if (CBreakpoint::IsBreakpoint(ptr)) CBreakpoint::DeleteBreakpoint(ptr);
							else								CBreakpoint::AddBreakpoint(codeViewData.cursorSeg, codeViewData.cursorOfs, false);
						}
						break;
		case KEY_F(10):	// Step over inst
						if (StepOver()) return 0;
						else {
							skipFirstInstruction = true; // for heavy debugger
							Bitu ret=(*cpudecoder)(1);
							SetCodeWinStart();
							CBreakpoint::ignoreOnce = 0;
						}
						break;
		case KEY_F(11):	// trace into
						skipFirstInstruction = true; // for heavy debugger
						ret = (*cpudecoder)(1);
						SetCodeWinStart();
						CBreakpoint::ignoreOnce = 0;
						break;

//		default:
//			// FIXME : Is this correct ?
//			if (key<0x200) ret=(*cpudecoder)(1);
//			break;
		};
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
	DrawRegisters();
	DrawData();
	DrawCode();
}
static void DEBUG_RaiseTimerIrq(void) {
	PIC_ActivateIRQ(0);
}

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
			PhysPt start=Segs[cs].phys+reg_eip;
			char dline[200];Bitu size;
			size = DasmI386(dline, start, reg_eip, false);
			Bitu len = strlen(dline);
			if (len<30) for (Bitu i=0; i<30-len; i++) strcat(dline," ");
			
			PIC_IRQAgain=false;
			ret=(*cpudecoder)(1);
		
			// Get register values
			char buffer[512];
			sprintf(buffer,"%04X:%08X   %s EAX:%08X EBX:%08X ECX:%08X EDX:%08X ESI:%08X EDI:%08X EBP:%08X ESP:%08X DS:%04X ES:%04X FS:%04X GS:%04X SS:%04X CF:%01X ZF:%01X SF:%01X OF:%01X AF:%01X PF:%01X\n",csValue,eipValue,dline,reg_eax,reg_ebx,reg_ecx,reg_edx,reg_esi,reg_edi,reg_ebp,reg_esp,SegValue(ds),SegValue(ds),SegValue(es),SegValue(fs),SegValue(gs),SegValue(ss),get_CF(),get_ZF(),get_SF(),get_OF(),get_AF(),get_PF());
			fprintf(f,"%s",buffer);
			count--;
			if (count==0) break;

		} while (!ret && PIC_IRQAgain);
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

static void DEBUG_ProgramStart(Program * * make) {
	*make=new DEBUG;
}

// INIT 

void DEBUG_SetupConsole(void)
{
	#ifdef WIN32
	WIN32_Console();
    #else
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
};

void DEBUG_Init(Section* sec) {

	MSG_Add("DEBUG_CONFIGFILE_HELP","Nothing to setup yet!\n");
	DEBUG_DrawScreen();
	/* Add some keyhandlers */
	KEYBOARD_AddEvent(KBD_kpminus,0,DEBUG_Enable);
	KEYBOARD_AddEvent(KBD_kpplus,0,DEBUG_RaiseTimerIrq);
	/* Clear the TBreakpoint list */
	memset((void*)&codeViewData,0,sizeof(codeViewData));
	/* setup debug.com */
	PROGRAMS_MakeFile("DEBUG.COM",DEBUG_ProgramStart);
	/* shutdown function */
	sec->AddDestroyFunction(&DEBUG_ShutDown);
}

// HEAVY DEBUGGING STUFF

#if C_HEAVY_DEBUG

#define LOGCPUMAX 200

static Bit16u logCpuCS [LOGCPUMAX];
static Bit32u logCpuEIP[LOGCPUMAX];
static Bit32u logCount = 0;

typedef struct SLogInst {
	char buffer[256];
} TLogInst;

TLogInst logInst[LOGCPUMAX];

void DEBUG_HeavyLogInstruction(void)
{
	LogInstruction(SegValue(cs),reg_eip,logInst[logCount++].buffer);
	if (logCount>=LOGCPUMAX) logCount = 0;
};

void DEBUG_HeavyWriteLogInstruction(void)
{
	LOG_DEBUG("DEBUG: Creating cpu log LOGCPU_INT_CD.TXT");

	FILE* f = fopen("LOGCPU_INT_CD.TXT","wt");
	if (!f) {
		LOG_DEBUG("DEBUG: Failed.");	
		return;
	}

	Bit16u startLog = logCount;
	do {
		// Write Intructions
		fprintf(f,"%s",logInst[startLog++].buffer);
		if (startLog>=LOGCPUMAX) logCount = 0;
	} while (startLog!=logCount);
	
	fclose(f);

	LOG_DEBUG("DEBUG: Done.");	
};

bool DEBUG_HeavyIsBreakpoint(void)
{
	if (skipFirstInstruction) {
		skipFirstInstruction = false;
		return false;
	}
	
	PhysPt where = SegPhys(cs)+reg_eip;
	if (CBreakpoint::CheckBreakpoint(where)) {
		exitLoop = true;
		DEBUG_Enable();
		return true;
	};
	return false;
};

#endif // HEAVY DEBUG

#endif // DEBUG

