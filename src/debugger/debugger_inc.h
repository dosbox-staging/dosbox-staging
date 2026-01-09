// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

/* Local Debug Function */


#include <curses.h>
#include "hardware/memory.h"

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
	/* Window height values in rows */
	int32_t rows_registers = 4; /* Registers window height */
	int32_t rows_data = 8;      /* Data Output window height */
	int32_t rows_code = 11;     /* Disassembly/Debug point window height */
	int32_t rows_variables = 4; /* Variable window height */
	int32_t rows_output = 0;    /* Text Output window height, calculated dynamically */
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
