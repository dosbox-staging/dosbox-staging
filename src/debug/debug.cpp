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

#ifdef WIN32
void WIN32_Console();
#endif

static struct  {
	Bit32u eax,ebx,ecx,edx,esi,edi,ebp,esp,eip;
} oldregs;

static void DrawCode(void);

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

enum { BKPNT_REALMODE, BKPNT_PHYSICAL, BKPNT_INTERRUPT };

#define BPINT_ALL 0x100

typedef struct SBreakPoint {
	PhysPt	location;
	Bit8u	olddata;
	Bit8u	type;
	Bit16u	ahValue;
	Bit16u  segment;
	Bit32u	offset;
	bool	once;
	bool	enabled;
	bool	active;
} TBreakpoint;

static std::list<TBreakpoint> BPoints;

static bool IsBreakpoint(PhysPt off)
{
	// iterate list and remove TBreakpoint
	std::list<TBreakpoint>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		if (((*i).type==BKPNT_PHYSICAL) && ((*i).location==off)) return true;
	}
	return false;
};

/* This clears all breakpoints by replacing their 0xcc by the original byte */
static void ClearBreakpoints(void) 
{
	// iterate list and place 0xCC 
	std::list<TBreakpoint>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		if (((*i).type==BKPNT_PHYSICAL) && (*i).active) {
			if (mem_readb((*i).location)==0xCC) mem_writeb((*i).location,(*i).olddata);
			(*i).active = false;
		};
	}
}

static void DeleteBreakpoint(PhysPt off) 
{
	// iterate list and place 0xCC 
	std::list<TBreakpoint>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		if (((*i).type==BKPNT_PHYSICAL) && (off==(*i).location)) {
			if ((*i).active && (mem_readb((*i).location)==0xCC)) mem_writeb((*i).location,(*i).olddata);
			(BPoints.erase)(i);
			break;
		};
	}
}

static void SetBreakpoints(void) 
{
	// iterate list and place 0xCC 
	std::list<TBreakpoint>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		if (((*i).type==BKPNT_PHYSICAL) && (*i).enabled) {
			Bit8u data = mem_readb((*i).location);
			if (data!=0xCC) {
				(*i).olddata	= data;
				(*i).active		= true;
				mem_writeb((*i).location,0xCC);
			}
		};
	}
}

static void AddBreakpoint(Bit16u seg, Bit32u ofs, bool once) 
{
	TBreakpoint bp;
	bp.type		= BKPNT_PHYSICAL;
	bp.enabled	= true;
	bp.active	= false;
	bp.location	= PhysMake(seg,ofs);
	bp.segment	= seg;
	bp.offset	= ofs;
	bp.once		= once;
	BPoints.push_front(bp);
}

static void AddIntBreakpoint(Bit8u intNum, Bit16u ah, bool once)
{
	TBreakpoint bp;
	bp.type		= BKPNT_INTERRUPT;
	bp.olddata	= intNum;
	bp.ahValue	= ah;
	bp.enabled	= true;
	bp.active	= true;
	bp.once		= once;
	BPoints.push_front(bp);
};

static bool RemoveBreakpoint(PhysPt off)
{
	// iterate list and remove TBreakpoint
	std::list<TBreakpoint>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); i++) {
		if (((*i).type==BKPNT_PHYSICAL) && ((*i).location==off)) {
			TBreakpoint* bp = &(*i);
			if ((*i).active && (mem_readb((*i).location)==0xCC)) mem_writeb((*i).location,(*i).olddata);
			(*i).active = false;
			if ((*i).once) (BPoints.erase)(i);
			
			return true;
		};
	}
	return false;
};

static bool StepOver()
{
	PhysPt start=Segs[cs].phys+reg_eip;
	char dline[200];Bitu size;
	size=DasmI386(dline, start, reg_eip, false);

	if (strstr(dline,"call") || strstr(dline,"int")) {
		AddBreakpoint (SegValue(cs),reg_eip+size, true);
		SetBreakpoints();
		debugging=false;
		DrawCode();
		DOSBOX_SetNormalLoop();
		return true;
	} 
	return false;
};

bool DEBUG_BreakPoint(void) {
	/* First get the phyiscal address and check for a set TBreakpoint */
	PhysPt where=SegPhys(cs)+reg_eip-1;
	bool found	  = false;
	std::list<TBreakpoint>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); ++i) {
		if ((*i).active && (*i).enabled) {
			if ((*i).location==where) {
				found=true;
				break;
			}
		}
	}
	if (!found) return false;
	RemoveBreakpoint(where);
	ClearBreakpoints();
	reg_eip -= 1;
	DEBUG_Enable();
	return true;
}

bool DEBUG_IntBreakpoint(Bit8u intNum)
{
	PhysPt where=SegPhys(cs)+reg_eip-2;
	bool found=false;
	std::list<TBreakpoint>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); ++i) {
		if ( ((*i).type==BKPNT_INTERRUPT) && (*i).enabled) {
			if (((*i).olddata==intNum) && ( ((*i).ahValue==(Bit16u)reg_ah) || ((*i).ahValue==BPINT_ALL)) ) {
				if ((*i).active) {
					found=true;
					(*i).active=false;
					//(BPoints.erase)(i);
					//break;
				} else {
					// One step over is ok -> activate for next occurence
					(*i).active=true;
					//break;
				}
			}
		}
	}
	if (!found) return false;

	// Remove normal breakpoint here, cos otherwise 0xCC wont be removed here
	RemoveBreakpoint(where);
	ClearBreakpoints();
	reg_eip -= 2;
	DEBUG_Enable();
	return true;
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
			} else if (IsBreakpoint(start)) {
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

bool ParseCommand(char* str)
{
	char* found = str;
	while (*found) *found++=toupper(*found);
	found = strstr(str,"BP ");
	if (found) { // Add new breakpoint
		found+=3;
		Bit16u seg = GetHexValue(found,found);found++; // skip ":"
		Bit32u ofs = GetHexValue(found,found);
		AddBreakpoint(seg,ofs,false);
		LOG_DEBUG("DEBUG: Set breakpoint at %04X:%04X",seg,ofs);
		return true;
	}
	found = strstr(str,"BPINT");
	if (found) { // Add Interrupt Breakpoint
		found+=5;
		Bit8u intNr	= GetHexValue(found,found); found++;
		Bit8u valAH	= GetHexValue(found,found);
		if ((valAH==0x00) && (*found=='*')) {
			AddIntBreakpoint(intNr,BPINT_ALL,false);
			LOG_DEBUG("DEBUG: Set interrupt breakpoint at INT %02X",intNr);
		} else {
			AddIntBreakpoint(intNr,valAH,false);
			LOG_DEBUG("DEBUG: Set interrupt breakpoint at INT %02X AH=%02X",intNr,valAH);
		}
		return true;
	}
	found = strstr(str,"BPLIST");
	if (found) {
		wprintw(dbg.win_out,"Breakpoint list:\n");
		wprintw(dbg.win_out,"-------------------------------------------------------------------------\n");
		Bit32u nr = 0;
		// iterate list 
		std::list<TBreakpoint>::iterator i;
		for(i=BPoints.begin(); i != BPoints.end(); i++) {
			if ((*i).type==BKPNT_PHYSICAL) {
				wprintw(dbg.win_out,"%02X. BP %04X:%04X\n",nr,(*i).segment,(*i).offset);
				nr++;
			} else if ((*i).type==BKPNT_INTERRUPT) {
				if ((*i).ahValue==BPINT_ALL) wprintw(dbg.win_out,"%02X. BPINT %02X\n",nr,(*i).olddata);					
				else					     wprintw(dbg.win_out,"%02X. BPINT %02X AH=%02X\n",nr,(*i).olddata,(*i).ahValue);
				nr++;
			};
		}
		wrefresh(dbg.win_out);
		return true;
	};

	found = strstr(str,"BPDEL");
	if (found) { // Delete Breakpoints
		found+=5;
		Bit8u bpNr	= GetHexValue(found,found); 
		if ((bpNr==0x00) && (*found=='*')) { // Delete all
			(BPoints.clear)();		
			LOG_DEBUG("DEBUG: Breakpoints deleted.");
		} else {		
			// delete single breakpoint
			Bit16u nr = 0;
			std::list<TBreakpoint>::iterator i;
			for(i=BPoints.begin(); i != BPoints.end(); i++) {
				if ((*i).type==BKPNT_PHYSICAL) {
					if (nr==bpNr) {
						DeleteBreakpoint((*i).location);
						LOG_DEBUG("DEBUG: Breakpoint %02X deleted.",nr);
						break;
					} 
					nr++;
				} else if ((*i).type==BKPNT_INTERRUPT) {
					if (nr==bpNr) {
						(BPoints.erase)(i);
						LOG_DEBUG("DEBUG: Breakpoint %02X. deleted.",nr);
						break;;
					}
					nr++;
				}
			}
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
		wprintw(dbg.win_out,"\n");
		wprintw(dbg.win_out,"Debugger commands (enter all values in hex or as register):\n");
		wprintw(dbg.win_out,"--------------------------------------------------------------------------\n");
		wprintw(dbg.win_out,"BP     [segment]:[offset] - Set breakpoint\n");
		wprintw(dbg.win_out,"BPINT  [intNr] *          - Set interrupt breakpoint\n");
		wprintw(dbg.win_out,"BPINT  [intNr] [ah]       - Set interrupt breakpoint with ah\n");
		wprintw(dbg.win_out,"BPLIST                    - List breakpoints\n");		
		wprintw(dbg.win_out,"BPDEL  [bpNr] / *         - Delete breakpoint nr / all\n");
		wprintw(dbg.win_out,"C / D  [segment]:[offset] - Set code / data view address\n");
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
						SetBreakpoints();
						DOSBOX_SetNormalLoop();	
						break;
		case KEY_F(9):	// Set/Remove TBreakpoint
						{	PhysPt ptr = PhysMake(codeViewData.cursorSeg,codeViewData.cursorOfs);
							if (IsBreakpoint(ptr)) DeleteBreakpoint(ptr);
							else AddBreakpoint(codeViewData.cursorSeg, codeViewData.cursorOfs, false);
						}
						break;
		case KEY_F(10):	// Step over inst
						if (StepOver()) return 0;
						else {
							Bitu ret=(*cpudecoder)(1);
							SetCodeWinStart();
						}
						break;
		case KEY_F(11):	// trace into
						ret = (*cpudecoder)(1);
						SetCodeWinStart();
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
		AddBreakpoint(oldCS,oldEIP,true);
		SetBreakpoints();
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

void DEBUG_Init(void) {
	#ifdef WIN32
	WIN32_Console();
	#endif
	memset((void *)&dbg,0,sizeof(dbg));
	debugging=false;
	dbg.active_win=3;
	input_count=0;
	/* Start the Debug Gui */
	DBGUI_StartUp();
	DEBUG_DrawScreen();
	/* Add some keyhandlers */
	KEYBOARD_AddEvent(KBD_kpminus,0,DEBUG_Enable);
	KEYBOARD_AddEvent(KBD_kpplus,0,DEBUG_RaiseTimerIrq);
	/* Clear the TBreakpoint list */
	memset((void*)&codeViewData,0,sizeof(codeViewData));
	(BPoints.clear)();
}

#endif

