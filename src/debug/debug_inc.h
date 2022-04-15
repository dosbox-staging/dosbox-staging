/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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

/* Local Debug Function */


#include <curses.h>
#include "mem.h"

enum NCURSES_COLOR_PAIRS {
	PAIR_BLACK_BLUE = 1,
	PAIR_BYELLOW_BLACK = 2,
	PAIR_GREEN_BLACK = 3,
	PAIR_BLACK_GREY = 4,
	PAIR_GREY_RED = 5,
};

void DBGUI_StartUp();

struct DBGBlock {
	WINDOW *win_main = nullptr; /* The Main Window */
	WINDOW *win_reg = nullptr;  /* Register Window */
	WINDOW *win_data = nullptr; /* Data Output window */
	WINDOW *win_code = nullptr; /* Disassembly/Debug point Window */
	WINDOW *win_var = nullptr;  /* Variable Window */
	WINDOW *win_out = nullptr;  /* Text Output Window */
	uint32_t active_win = 0;    /* Current active window */
	uint32_t input_y = 0;
	uint32_t global_mask = 0; /* Current msgmask */
};

struct DASMLine {
	uint32_t pc = 0;
	char dasm[80] = {0};
	PhysPt ea = 0;
	uint16_t easeg = 0;
	uint32_t eaoff = 0;
};

extern DBGBlock dbg;

/* Local Debug Stuff */
Bitu DasmI386(char* buffer, PhysPt pc, Bitu cur_ip, bool bit32);
int DasmLastOperandSize();
