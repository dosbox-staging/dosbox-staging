/*
 *  Copyright (C) 2002 - 2003  The DOSBox Team
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



#include "dosbox.h"
#if C_DEBUG

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <curses.h>
#include <string.h>

#include "regs.h"
#include "debug.h"
#include "debug_inc.h"

extern int old_cursor_state;
void DEBUG_ShowMsg(char * msg) {
	char buf[1024];
	strcpy(buf,msg);

	waddstr(dbg.win_out,buf);
	wprintw(dbg.win_out," %d\n",cycle_count);
	wrefresh(dbg.win_out);
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


	mvwaddstr(dbg.win_reg,1,52,"C  Z  S  O  A  P  D  I  T ");
}


static void DrawBars(void) {
	if (has_colors()) {
		attrset(COLOR_PAIR(PAIR_BLACK_BLUE));
	}
	/* Show the Register bar */
	mvaddstr(dbg.win_reg->_begy-1,0,"---[F1](Register Overview)---");
	/* Show the Data Overview bar perhaps with more special stuff in the end */
	mvaddstr(dbg.win_data->_begy-1,0,"---[F2](Data Overview)---");
	/* Show the Code Overview perhaps with special stuff in bar too */
	mvaddstr(dbg.win_code->_begy-1,0,"---[F3](Code Overview)---");
	/* Show the Output OverView */
	mvaddstr(dbg.win_out->_begy-1,0,"---[F4](OutPut/Input)---");
	attrset(0);
}



static void MakeSubWindows(void) {
	/* The Std output win should go in bottem */
	/* Make all the subwindows */
	int outy=1;
	/* The Register window  */
	dbg.win_reg=subwin(dbg.win_main,4,dbg.win_main->_maxx,outy,0);
	outy+=5;
	/* The Data Window */
	dbg.win_data=subwin(dbg.win_main,10,dbg.win_main->_maxx,outy,0);
	outy+=11;
	/* The Code Window */
	dbg.win_code=subwin(dbg.win_main,11,dbg.win_main->_maxx,outy,0);
	outy+=12;
	/* The output Window */	
	dbg.win_out=subwin(dbg.win_main,dbg.win_main->_maxy-outy-1,dbg.win_main->_maxx,outy,0);
	dbg.input_y=dbg.win_main->_maxy-1;
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


void DBGUI_StartUp(void) {
	/* Start the main window */
	dbg.win_main=initscr();
	cbreak();       /* take input chars one at a time, no wait for \n */
	noecho();       /* don't echo input */
	nodelay(dbg.win_main,true);
	keypad(dbg.win_main,true);
	#ifndef WIN32
	resizeterm(50,80);
	touchwin(dbg.win_main);
	old_cursor_state = curs_set(0);
	#endif
	start_color();
	cycle_count=0;
	MakePairs();
	MakeSubWindows();


}

#endif
