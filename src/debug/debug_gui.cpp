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
 */


#include "dosbox.h"

#if C_DEBUG
#include "control.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <curses.h>
#include <string.h>

#include "cross.h"
#include "support.h"
#include "regs.h"
#include "debug.h"
#include "debug_inc.h"

struct _LogGroup {
	char const* front;
	bool enabled;
};
#include <list>
#include <string>
using namespace std;

#define MAX_LOG_BUFFER 500
static list<string> logBuff;
static list<string>::iterator logBuffPos = logBuff.end();

static _LogGroup loggrp[LOG_MAX]={{"",true},{0,false}};
static FILE* debuglog;

extern int old_cursor_state;



void DEBUG_ShowMsg(char const* format,...) {
	
	char buf[512];
	va_list msg;
	va_start(msg,format);
	vsnprintf(buf,sizeof(buf),format,msg);
	va_end(msg);

	buf[sizeof(buf) - 1] = '\0';

	/* Add newline if not present */
	size_t len = strlen(buf);
	if(buf[len - 1] != '\n' && len + 1 < sizeof(buf) ) strcat(buf,"\n");

	if (debuglog) {
		fprintf(debuglog,"%s",buf);
		fflush(debuglog);
	}

	if (logBuffPos != logBuff.end()) {
		logBuffPos=logBuff.end();
		DEBUG_RefreshPage(0);
//		mvwprintw(dbg.win_out,dbg.win_out->_maxy-1, 0, "");
	}
	logBuff.push_back(buf);
	if (logBuff.size() > MAX_LOG_BUFFER)
		logBuff.pop_front();

	logBuffPos = logBuff.end();
	wprintw(dbg.win_out,"%s",buf);
	wrefresh(dbg.win_out);
}

void DEBUG_RefreshPage(char scroll) {
	if (scroll==-1 && logBuffPos!=logBuff.begin()) logBuffPos--;
	else if (scroll==1 && logBuffPos!=logBuff.end()) logBuffPos++;

	list<string>::iterator i = logBuffPos;
	int maxy, maxx; getmaxyx(dbg.win_out,maxy,maxx);
	int rem_lines = maxy;
	if(rem_lines == -1) return;

	wclear(dbg.win_out);

	while (rem_lines > 0 && i!=logBuff.begin()) {
		--i;
		for (string::size_type posf=0, posl; (posl=(*i).find('\n',posf)) != string::npos ;posf=posl+1)
			rem_lines -= (int) ((posl-posf) / maxx) + 1; // len=(posl+1)-posf-1
		/* Const cast is needed for pdcurses which has no const char in mvwprintw (bug maybe) */
		mvwprintw(dbg.win_out,rem_lines-1, 0, const_cast<char*>((*i).c_str()));
	}
	mvwprintw(dbg.win_out,maxy-1, 0, "");
	wrefresh(dbg.win_out);
}

void LOG::operator() (char const* format, ...){
	char buf[512];
	va_list msg;
	va_start(msg,format);
	vsprintf(buf,format,msg);
	va_end(msg);

	if (d_type>=LOG_MAX) return;
	if ((d_severity!=LOG_ERROR) && (!loggrp[d_type].enabled)) return;
	DEBUG_ShowMsg("%10u: %s:%s\n",static_cast<Bit32u>(cycle_count),loggrp[d_type].front,buf);
}


static void Draw_RegisterLayout(void) {
	mvwaddstr(dbg.win_reg,0,0,"EAX=");
	mvwaddstr(dbg.win_reg,1,0,"EBX=");
	mvwaddstr(dbg.win_reg,2,0,"ECX=");
	mvwaddstr(dbg.win_reg,3,0,"EDX=");

	mvwaddstr(dbg.win_reg,0,14,"ESI=");
	mvwaddstr(dbg.win_reg,1,14,"EDI=");
	mvwaddstr(dbg.win_reg,2,14,"EBP=");
	mvwaddstr(dbg.win_reg,3,14,"ESP=");

	mvwaddstr(dbg.win_reg,0,28,"DS=");
	mvwaddstr(dbg.win_reg,0,38,"ES=");
	mvwaddstr(dbg.win_reg,0,48,"FS=");
	mvwaddstr(dbg.win_reg,0,58,"GS=");
	mvwaddstr(dbg.win_reg,0,68,"SS=");

	mvwaddstr(dbg.win_reg,1,28,"CS=");
	mvwaddstr(dbg.win_reg,1,38,"EIP=");

	mvwaddstr(dbg.win_reg,2,75,"CPL");
	mvwaddstr(dbg.win_reg,2,68,"IOPL");

	mvwaddstr(dbg.win_reg,1,52,"C  Z  S  O  A  P  D  I  T ");
}


static void DrawBars(void) {
	if (has_colors()) {
		attrset(COLOR_PAIR(PAIR_BLACK_BLUE));
	}
	/* Show the Register bar */
	mvaddstr(1-1,0, "-----(Register Overview                   )-----                                ");
	/* Show the Data Overview bar perhaps with more special stuff in the end */
	mvaddstr(6-1,0, "-----(Data Overview   Scroll: page up/down)-----                                ");
	/* Show the Code Overview perhaps with special stuff in bar too */  
	mvaddstr(15-1,0,"-----(Code Overview   Scroll: up/down     )-----                                ");
	/* Show the Variable Overview bar */
	mvaddstr(27-1,0,"-----(Variable Overview                   )-----                                ");
	/* Show the Output OverView */
	mvaddstr(32-1,0,"-----(Output          Scroll: home/end    )-----                                ");
	attrset(0);
	//Match values with below. So we don't need to touch the internal window structures
}



static void MakeSubWindows(void) {
	/* The Std output win should go at the bottom */
	/* Make all the subwindows */
	int win_main_maxy, win_main_maxx; getmaxyx(dbg.win_main,win_main_maxy,win_main_maxx);
	int outy=1; //Match values with above
	/* The Register window  */
	dbg.win_reg=subwin(dbg.win_main,4,win_main_maxx,outy,0);
	outy+=5; // 6
	/* The Data Window */
	dbg.win_data=subwin(dbg.win_main,8,win_main_maxx,outy,0);
	outy+=9; // 15
	/* The Code Window */
	dbg.win_code=subwin(dbg.win_main,11,win_main_maxx,outy,0);
	outy+=12; // 27
	/* The Variable Window */
	dbg.win_var=subwin(dbg.win_main,4,win_main_maxx,outy,0);
	outy+=5; // 32
	/* The Output Window */	
	dbg.win_out=subwin(dbg.win_main,win_main_maxy-outy,win_main_maxx,outy,0);
	if(!dbg.win_reg ||!dbg.win_data || !dbg.win_code || !dbg.win_var || !dbg.win_out) E_Exit("Setting up windows failed");
//	dbg.input_y=win_main_maxy-1;
	scrollok(dbg.win_out,TRUE);
	DrawBars();
	Draw_RegisterLayout();
	refresh();
}

static void MakePairs(void) {
	init_pair(PAIR_BLACK_BLUE, COLOR_BLACK, COLOR_CYAN);
	init_pair(PAIR_BYELLOW_BLACK, COLOR_YELLOW /*| FOREGROUND_INTENSITY */, COLOR_BLACK);
	init_pair(PAIR_GREEN_BLACK, COLOR_GREEN /*| FOREGROUND_INTENSITY */, COLOR_BLACK);
	init_pair(PAIR_BLACK_GREY, COLOR_BLACK /*| FOREGROUND_INTENSITY */, COLOR_WHITE);
	init_pair(PAIR_GREY_RED, COLOR_WHITE/*| FOREGROUND_INTENSITY */, COLOR_RED);
}
static void LOG_Destroy(Section*) {
	if(debuglog) fclose(debuglog);
	debuglog = 0;
}

static void LOG_Init(Section * sec) {
	Section_prop * sect = static_cast<Section_prop *>(sec);
	const char * blah = sect->Get_string("logfile");
	if(blah && blah[0] && (debuglog = fopen(blah,"wt+"))){
		;
	} else {
		debuglog = 0;
	}
	sect->AddDestroyFunction(&LOG_Destroy);
	char buf[64];
	for (Bitu i = LOG_ALL + 1;i < LOG_MAX;i++) { //Skip LOG_ALL, it is always enabled
		safe_strncpy(buf,loggrp[i].front,sizeof(buf));
		lowcase(buf);
		loggrp[i].enabled=sect->Get_bool(buf);
	}
}


void LOG_StartUp(void) {
	/* Setup logging groups */
	loggrp[LOG_ALL].front="ALL";
	loggrp[LOG_VGA].front="VGA";
	loggrp[LOG_VGAGFX].front="VGAGFX";
	loggrp[LOG_VGAMISC].front="VGAMISC";
	loggrp[LOG_INT10].front="INT10";
	loggrp[LOG_SB].front="SBLASTER";
	loggrp[LOG_DMACONTROL].front="DMA_CONTROL";
	
	loggrp[LOG_FPU].front="FPU";
	loggrp[LOG_CPU].front="CPU";
	loggrp[LOG_PAGING].front="PAGING";

	loggrp[LOG_FCB].front="FCB";
	loggrp[LOG_FILES].front="FILES";
	loggrp[LOG_IOCTL].front="IOCTL";
	loggrp[LOG_EXEC].front="EXEC";
	loggrp[LOG_DOSMISC].front="DOSMISC";

	loggrp[LOG_PIT].front="PIT";
	loggrp[LOG_KEYBOARD].front="KEYBOARD";
	loggrp[LOG_PIC].front="PIC";

	loggrp[LOG_MOUSE].front="MOUSE";
	loggrp[LOG_BIOS].front="BIOS";
	loggrp[LOG_GUI].front="GUI";
	loggrp[LOG_MISC].front="MISC";

	loggrp[LOG_IO].front="IO";
	loggrp[LOG_PCI].front="PCI";
	
	/* Register the log section */
	Section_prop * sect=control->AddSection_prop("log",LOG_Init);
	Prop_string* Pstring = sect->Add_string("logfile",Property::Changeable::Always,"");
	Pstring->Set_help("file where the log messages will be saved to");
	char buf[64];
	for (Bitu i = LOG_ALL + 1;i < LOG_MAX;i++) {
		safe_strncpy(buf,loggrp[i].front, sizeof(buf));
		lowcase(buf);
		Prop_bool* Pbool = sect->Add_bool(buf,Property::Changeable::Always,true);
		Pbool->Set_help("Enable/Disable logging of this type.");
	}
//	MSG_Add("LOG_CONFIGFILE_HELP","Logging related options for the debugger.\n");
}




void DBGUI_StartUp(void) {
	/* Start the main window */
	dbg.win_main=initscr();
	cbreak();       /* take input chars one at a time, no wait for \n */
	noecho();       /* don't echo input */
	nodelay(dbg.win_main,true);
	keypad(dbg.win_main,true);
	#ifndef WIN32
	printf("\e[8;50;80t");
	fflush(NULL);
	resizeterm(50,80);
	touchwin(dbg.win_main);
	#endif
	old_cursor_state = curs_set(0);
	start_color();
	cycle_count=0;
	MakePairs();
	MakeSubWindows();

}

#endif
