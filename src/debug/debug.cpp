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
#ifdef C_DEBUG
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

enum { BKPNT_REALMODE,BKPNT_PHYSICAL };

struct BreakPoint {
	PhysPt location;
	Bit8u olddata;
	Bit16u seg;
	Bit16u off_16;
	Bit32u off_32;
	Bit8u type;
	bool enabled;
	bool active;
};

static std::list<struct BreakPoint> BPoints;

static Bit16u dataSeg,dataOfs;

static void DrawData() {
	
	Bit16u add = 0;
	/* Data win */	
	for (int y=0; y<8; y++) {
		// Adress
		mvwprintw (dbg.win_data,1+y,0,"%04X:%04X",dataSeg,dataOfs+add);
		for (int x=0; x<16; x++) {
			Bit8u ch = real_readb(dataSeg,dataOfs+add);
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
	
	SetColor(Segs[ds].value!=oldsegs[ds].value);oldsegs[ds].value=Segs[ds].value;mvwprintw (dbg.win_reg,0,31,"%04X",Segs[ds].value);
	SetColor(Segs[es].value!=oldsegs[es].value);oldsegs[es].value=Segs[es].value;mvwprintw (dbg.win_reg,0,41,"%04X",Segs[es].value);
	SetColor(Segs[fs].value!=oldsegs[fs].value);oldsegs[fs].value=Segs[fs].value;mvwprintw (dbg.win_reg,0,51,"%04X",Segs[fs].value);
	SetColor(Segs[gs].value!=oldsegs[gs].value);oldsegs[gs].value=Segs[gs].value;mvwprintw (dbg.win_reg,0,61,"%04X",Segs[gs].value);
	SetColor(Segs[ss].value!=oldsegs[ss].value);oldsegs[ss].value=Segs[ss].value;mvwprintw (dbg.win_reg,0,71,"%04X",Segs[ss].value);
	SetColor(Segs[cs].value!=oldsegs[cs].value);oldsegs[cs].value=Segs[cs].value;mvwprintw (dbg.win_reg,1,31,"%04X",Segs[cs].value);

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


static void DrawCode(void) {
	PhysPt start=Segs[cs].phys+reg_eip;
	char dline[200];Bitu size;Bitu c;
	for (Bit32u i=0;i<10;i++) {
		size=DasmI386(dline, start, reg_eip, false);
		mvwprintw(dbg.win_code,i,0,"%02X:%04X  ",Segs[cs].value,(start-Segs[cs].phys));
		for (c=0;c<size;c++) wprintw(dbg.win_code,"%02X",mem_readb(start+c));
		for (c=20;c>=size*2;c--) waddch(dbg.win_code,' ');
		waddstr(dbg.win_code,dline);
		for (c=30-strlen(dline);c>0;c--) waddch(dbg.win_code,' ');
		start+=size;
	}
	wrefresh(dbg.win_code);
}
/* This clears all breakpoints by replacing their 0xcc by the original byte */
static void ClearBreakPoints(void) {

//	for (Bit32u i=0;i<MAX_BREAKPOINT;i++) {
//		if (bpoints[i].active && bpoints[i].enabled) {
//			mem_writeb(bpoints[i].location,bpoints[i].olddata);
//		}
//	}
}

static void SetBreakPoints(void) {
//	for (Bit32u i=0;i<MAX_BREAKPOINT;i++) {
//		if (bpoints[i].active && bpoints[i].enabled) {
//			bpoints[i].olddata=mem_readb(bpoints[i].location);
//			mem_writeb(bpoints[i].location,0xcc);
//		}
//	}
}



static void AddBreakPoint(PhysPt off) {
	
}



bool DEBUG_BreakPoint(void) {
	/* First get the phyiscal address and check for a set breakpoint */
	PhysPt where=real_phys(Segs[cs].value,reg_ip-1);
	bool found=false;
	std::list<BreakPoint>::iterator i;
	for(i=BPoints.begin(); i != BPoints.end(); ++i) {
		if ((*i).active && (*i).enabled) {
			if ((*i).location==where) {
				found=true;
				break;
			}
		}
	}
	if (!found) return false;
	ClearBreakPoints();
	DEBUG_Enable();

	SetBreakPoints();
	return false;
}


Bit32u DEBUG_CheckKeys(void) {
	int key=getch();
	Bit32u ret=0;
	if (key>0) {
		switch (key) {
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

		case 'd':	dataSeg = Segs[ds].value;
					dataOfs = reg_si;
					break;
		case 'e':	dataSeg = Segs[es].value;
					dataOfs = reg_di;
					break;
		case 'x':	dataSeg = Segs[ds].value;
					dataOfs = reg_dx;
					break;
		case 'b':	dataSeg = Segs[es].value;
					dataOfs = reg_bx;
					break;

		case 'r' :	dataOfs -= 16;	break;
		case 'f' :	dataOfs += 16;	break;

		default:
			ret=(*cpudecoder)(1);
		};
		DEBUG_DrawScreen();
	}
	return ret;
};

Bitu DEBUG_Loop(void) {
//TODO Disable sound
	GFX_Events();
	PIC_runIRQs();
	return DEBUG_CheckKeys();
}

void DEBUG_Enable(void) {
	DEBUG_DrawScreen();
	debugging=true;
	DOSBOX_SetLoop(&DEBUG_Loop);
}


void DEBUG_DrawScreen(void) {
	DrawRegisters();
	DrawCode();
	DrawData();
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
	/* Clear the breakpoint list */

}


#endif
