// SPDX-FileCopyrightText:  2002-2025 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#if C_DEBUGGER
#include "config/config.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <curses.h>
#include <list>
#include <string>

#include "cpu/registers.h"
#include "debugger.h"
#include "debugger_inc.h"
#include "misc/cross.h"
#include "misc/support.h"
#include "utils/string_utils.h"

#if !PDCURSES
#error SYSTEM CURSES INCLUDED, SHOULD BE PDCURSES
#endif

struct _LogGroup {
	const char* front = nullptr;
	bool enabled      = false;
};

#define MAX_LOG_BUFFER 500
static std::list<std::string> logBuff              = {};
static std::list<std::string>::iterator logBuffPos = logBuff.end();

static _LogGroup loggrp[LOG_MAX] = {
        {     "",  true},
        {nullptr, false}
};
static FILE* debuglog = nullptr;

extern int old_cursor_state;

void DEBUG_ShowMsg(const char* format, ...)
{
	// Quit early if the window hasn't been created yet
	if (!dbg.win_out) {
		return;
	}

	char buf[512];
	va_list msg;
	va_start(msg, format);
	vsprintf(buf, format, msg);
	va_end(msg);

	buf[sizeof(buf) - 1] = '\0';

	/* Add newline if not present */
	size_t len = safe_strlen(buf);
	if (buf[len - 1] != '\n' && len + 1 < sizeof(buf)) {
		strcat(buf, "\n");
	}

	if (debuglog) {
		fprintf(debuglog, "%s", buf);
		fflush(debuglog);
	}

	if (logBuffPos != logBuff.end()) {
		logBuffPos = logBuff.end();
		DEBUG_RefreshPage(0);
		//		mvwprintw(dbg.win_out,dbg.win_out->_maxy-1, 0, "");
	}
	logBuff.emplace_back(buf);
	if (logBuff.size() > MAX_LOG_BUFFER) {
		logBuff.pop_front();
	}

	logBuffPos = logBuff.end();
	wprintw(dbg.win_out, "%s", buf);
	wrefresh(dbg.win_out);
}

void DEBUG_RefreshPage(int scroll)
{
	// Quit early if the window hasn't been created yet
	if (!dbg.win_out) {
		return;
	}

	if (scroll == -1 && logBuffPos != logBuff.begin()) {
		--logBuffPos;
	} else if (scroll == 1 && logBuffPos != logBuff.end()) {
		++logBuffPos;
	}

	std::list<std::string>::iterator i = logBuffPos;
	int maxy, maxx;
	getmaxyx(dbg.win_out, maxy, maxx);
	int rem_lines = maxy;
	if (rem_lines == -1) {
		return;
	}

	wclear(dbg.win_out);

	while (rem_lines > 0 && i != logBuff.begin()) {
		--i;
		for (std::string::size_type posf = 0, posl;
		     (posl = (*i).find('\n', posf)) != std::string::npos;
		     posf = posl + 1) {
			rem_lines -= (int)((posl - posf) / maxx) + 1; // len=(posl+1)-posf-1
		}
		/* Const cast is needed for pdcurses which has no const char in
		 * mvwprintw (bug maybe) */
		mvwprintw(dbg.win_out, rem_lines - 1, 0, const_cast<char*>((*i).c_str()));
	}
	mvwprintw(dbg.win_out, maxy - 1, 0, "");
	wrefresh(dbg.win_out);
}

void LOG::operator()(const char* format, ...)
{
	char buf[512];
	va_list msg;
	va_start(msg, format);
	vsprintf(buf, format, msg);
	va_end(msg);

	if (d_type >= LOG_MAX) {
		return;
	}
	if ((d_severity != LOG_ERROR) && (!loggrp[d_type].enabled)) {
		return;
	}
	DEBUG_ShowMsg("%10u: %s:%s\n",
	              static_cast<uint32_t>(cycle_count),
	              loggrp[d_type].front,
	              buf);
}

static void Draw_RegisterLayout(void)
{
	// Quit early if the window hasn't been created yet
	if (!dbg.win_reg) {
		return;
	}

	mvwaddstr(dbg.win_reg, 0, 0, "EAX=");
	mvwaddstr(dbg.win_reg, 1, 0, "EBX=");
	mvwaddstr(dbg.win_reg, 2, 0, "ECX=");
	mvwaddstr(dbg.win_reg, 3, 0, "EDX=");

	mvwaddstr(dbg.win_reg, 0, 14, "ESI=");
	mvwaddstr(dbg.win_reg, 1, 14, "EDI=");
	mvwaddstr(dbg.win_reg, 2, 14, "EBP=");
	mvwaddstr(dbg.win_reg, 3, 14, "ESP=");

	mvwaddstr(dbg.win_reg, 0, 28, "DS=");
	mvwaddstr(dbg.win_reg, 0, 38, "ES=");
	mvwaddstr(dbg.win_reg, 0, 48, "FS=");
	mvwaddstr(dbg.win_reg, 0, 58, "GS=");
	mvwaddstr(dbg.win_reg, 0, 68, "SS=");

	mvwaddstr(dbg.win_reg, 1, 28, "CS=");
	mvwaddstr(dbg.win_reg, 1, 38, "EIP=");

	mvwaddstr(dbg.win_reg, 2, 75, "CPL");
	mvwaddstr(dbg.win_reg, 2, 68, "IOPL");

	mvwaddstr(dbg.win_reg, 1, 52, "C  Z  S  O  A  P  D  I  T ");
}

static void DrawBars(void)
{
	if (has_colors()) {
		attrset(COLOR_PAIR(PAIR_BLACK_BLUE));
	}
	/* Show the Register bar */
	int outy = 1;
	mvaddstr(outy - 1, 0, "-----(Register Overview                   )-----                                ");
	/* Show the Data Overview bar perhaps with more special stuff in the end */
	outy += dbg.rows_registers + 1;
	mvaddstr(outy - 1,
	         0,
	         "-----(Data Overview   Scroll: page up/down)-----                                ");
	/* Show the Code Overview perhaps with special stuff in bar too */
	outy += dbg.rows_data + 1;
	mvaddstr(outy - 1,
	         0,
	         "-----(Code Overview   Scroll: up/down     )-----                                ");
	/* Show the Variable Overview bar */
	outy += dbg.rows_code + 1;
	mvaddstr(outy - 1,
	         0,
	         "-----(Variable Overview                   )-----                                ");
	/* Show the Output OverView */
	outy += dbg.rows_output + 1;
	mvaddstr(outy - 1,
	         0,
	         "-----(Output          Scroll: home/end    )-----                                ");
	attrset(0);
	// Use height values in rows. So we don't need to touch the internal
	// window structures
}

static void MakeSubWindows(void)
{
	/* The Std output win should go at the bottom */
	/* Make all the subwindows */
	int win_main_maxy, win_main_maxx;
	getmaxyx(dbg.win_main, win_main_maxy, win_main_maxx);
	int outy = 1;
	/* The Register window  */
	dbg.win_reg = subwin(dbg.win_main, dbg.rows_registers, win_main_maxx, outy, 0);
	outy += dbg.rows_registers + 1;
	/* The Data Window */
	dbg.win_data = subwin(dbg.win_main, dbg.rows_data, win_main_maxx, outy, 0);
	outy += dbg.rows_data + 1;
	/* The Code Window */
	dbg.win_code = subwin(dbg.win_main, dbg.rows_code, win_main_maxx, outy, 0);
	outy += dbg.rows_code + 1;
	/* The Variable Window */
	dbg.win_var = subwin(dbg.win_main, dbg.rows_variables, win_main_maxx, outy, 0);
	outy += dbg.rows_variables + 1;
	/* The Output Window */
	dbg.rows_output = win_main_maxy - outy; /* Use the rest of window */
	dbg.win_out = subwin(dbg.win_main, dbg.rows_output, win_main_maxx, outy, 0);
	if (!dbg.win_reg || !dbg.win_data || !dbg.win_code || !dbg.win_var ||
	    !dbg.win_out) {
		E_Exit("Setting up windows failed");
	}
	//	dbg.input_y=win_main_maxy-1;
	scrollok(dbg.win_out, TRUE);
	DrawBars();
	Draw_RegisterLayout();
	refresh();
}

static void MakePairs()
{
	init_pair(PAIR_BLACK_BLUE, COLOR_BLACK, COLOR_CYAN);
	init_pair(PAIR_BYELLOW_BLACK,
	          COLOR_YELLOW /*| FOREGROUND_INTENSITY */,
	          COLOR_BLACK);
	init_pair(PAIR_GREEN_BLACK, COLOR_GREEN /*| FOREGROUND_INTENSITY */, COLOR_BLACK);
	init_pair(PAIR_BLACK_GREY, COLOR_BLACK /*| FOREGROUND_INTENSITY */, COLOR_WHITE);
	init_pair(PAIR_GREY_RED, COLOR_WHITE /*| FOREGROUND_INTENSITY */, COLOR_RED);
}

void LOG_Init()
{
	auto section = get_section("log");
	assert(section);

	std::string logfile = section->GetString("logfile");

	if (!logfile.empty() && (debuglog = fopen(logfile.c_str(), "wt+"))) {
		;
	} else {
		debuglog = nullptr;
	}

	char buf[64];

	// Skip LOG_ALL, it is always enabled
	for (Bitu i = LOG_ALL + 1; i < LOG_MAX; i++) {
		safe_strcpy(buf, loggrp[i].front);
		lowcase(buf);
		loggrp[i].enabled = section->GetBool(buf);
	}
}

void LOG_Destroy()
{
	if (debuglog) {
		fclose(debuglog);
	}
	debuglog = nullptr;
}

void LOG_StartUp()
{
	// Setup logging groups
	loggrp[LOG_ALL].front        = "ALL";
	loggrp[LOG_VGA].front        = "VGA";
	loggrp[LOG_VGAGFX].front     = "VGAGFX";
	loggrp[LOG_VGAMISC].front    = "VGAMISC";
	loggrp[LOG_INT10].front      = "INT10";
	loggrp[LOG_SB].front         = "SBLASTER";
	loggrp[LOG_DMACONTROL].front = "DMA_CONTROL";

	loggrp[LOG_FPU].front    = "FPU";
	loggrp[LOG_CPU].front    = "CPU";
	loggrp[LOG_PAGING].front = "PAGING";

	loggrp[LOG_FCB].front     = "FCB";
	loggrp[LOG_FILES].front   = "FILES";
	loggrp[LOG_IOCTL].front   = "IOCTL";
	loggrp[LOG_EXEC].front    = "EXEC";
	loggrp[LOG_DOSMISC].front = "DOSMISC";

	loggrp[LOG_PIT].front      = "PIT";
	loggrp[LOG_KEYBOARD].front = "KEYBOARD";
	loggrp[LOG_PIC].front      = "PIC";

	loggrp[LOG_MOUSE].front = "MOUSE";
	loggrp[LOG_BIOS].front  = "BIOS";
	loggrp[LOG_GUI].front   = "GUI";
	loggrp[LOG_MISC].front  = "MISC";

	loggrp[LOG_IO].front        = "IO";
	loggrp[LOG_PCI].front       = "PCI";
	loggrp[LOG_REELMAGIC].front = "REELMAGIC";

	// Register the log section
	auto sect = control->AddSection("log");

	PropString* pstring = sect->AddString("logfile",
	                                      Property::Changeable::Always,
	                                      "");

	pstring->SetHelp("Path of the log file.");

	char buf[64];
	for (Bitu i = LOG_ALL + 1; i < LOG_MAX; i++) {
		safe_strcpy(buf, loggrp[i].front);
		lowcase(buf);
		PropBool* pbool = sect->AddBool(buf, Property::Changeable::Always, true);
		pbool->SetHelp("Enable/disable logging of this type.");
	}
	//	MSG_Add("LOG_CONFIGFILE_HELP","Logging related options for the
	// debugger.\n");
}

void DBGUI_StartUp(void)
{
	// Start the main window
	dbg.win_main = initscr();

	// Take input chars one at a time, no wait for \n
	cbreak();

	// don't echo input
	noecho();

	nodelay(dbg.win_main, true);
	keypad(dbg.win_main, true);

	resize_term(50, 80);
	touchwin(dbg.win_main);

	old_cursor_state = curs_set(0);
	start_color();
	cycle_count = 0;

	// calculate minimum required size (in rows)
	auto min_size = 1 + dbg.rows_registers
	              + 1 + dbg.rows_data
	              + 1 + dbg.rows_code
	              + 1 + dbg.rows_variables
	              + 1 + dbg.rows_output;

	if (getmaxy(dbg.win_main) - min_size <= 0) {
		LOG_ERR("DEBUG: Couldn't fit layout elements, screen size is too small");
		dbg.win_main = NULL;
		endwin();
		return;
	}

	MakePairs();
	MakeSubWindows();
}

#endif
