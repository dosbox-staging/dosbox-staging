/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "program_more.h"

#include <string>

#include "shell.h"
#include "string_utils.h"
#include "../ints/int10.h"

extern bool output_con;

static char PAUSED(void)
{
	uint8_t c;
	uint16_t n = 1, handle;
	if (!output_con && DOS_OpenFile("con", OPEN_READWRITE, &handle)) {
		DOS_ReadFile(handle, &c, &n);
		DOS_CloseFile(handle);
	} else
		DOS_ReadFile(STDIN, &c, &n);
	return c;
}

void MORE::show_tabs(int &nchars)
{
	int COLS = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	int TABSIZE = 8;
	do {
		WriteOut(" ");
		nchars++;
	} while (nchars < COLS && nchars % TABSIZE);
}

bool MORE::show_lines(
        const uint8_t c, int &nchars, int &nlines, int linecount, const char *word)
{
	int COLS = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	int LINES = (IS_EGAVGA_ARCH ? real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS) : 24) +
	            1;
	if ((c == '\n') || (nchars >= COLS)) {
		nlines++;
		nchars = 0;
		if (*word ? !output_con : false) {
			if (c != '\n')
				WriteOut("\n");
		} else if (nlines == LINES) {
			WriteOut("-- More -- %s (%u) --", word, linecount);
			if (PAUSED() == 3) {
				WriteOut("^C\n");
				return false;
			}
			WriteOut("\n");
			nlines = 0;
		}
	}
	return true;
}

void MORE::Run()
{
	std::string tmp = "";
	cmd->GetStringRemain(tmp);
	if (cmd->FindExist("/?", false)) {
		WriteOut(MSG_Get("PROGRAM_MORE_HELP_LONG"));
		return;
	}
	char arg[CMD_MAXLINE];
	safe_strcpy(arg, tmp.c_str());
	char *args = arg;
	int nchars = 0, nlines = 0, linecount = 0, LINES = 25, COLS = 80;
	char *word;
	uint8_t c, last = 0;
	uint16_t n = 1;
	ltrim(args);
	LINES = (IS_EGAVGA_ARCH ? real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS) : 24) + 1;
	COLS = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	LINES--;
	if (!*args || !strcasecmp(args, "con")) {
		while (true) {
			DOS_ReadFile(STDIN, &c, &n);
			if (c == 3) {
				dos.echo = first_shell->echo;
				WriteOut("^C\n");
				return;
			}
			if (n == 0) {
				if (last != 10)
					WriteOut("\r\n");
				return;
			} else if (c == 13 && last == 26)
				return;
			else {
				if (c == 10)
					;
				else if (c == 13) {
					linecount++;
					WriteOut("\r\n");
				} else if (c == '\t') {
					show_tabs(nchars);
				} else {
					nchars++;
					WriteOut("%c", c);
				}
				if (!show_lines(c, nchars, nlines, linecount, ""))
					return;
				last = c;
			}
		}
	}
	if (strcasecmp(args, "nul") == 0)
		return;
	if (!*args) {
		WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
		return;
	}
	uint16_t handle;
nextfile:
	word = StripWord(args);
	if (!DOS_OpenFile(word, 0, &handle)) {
		WriteOut(MSG_Get(dos.errorcode == DOSERR_ACCESS_DENIED
		                         ? "SHELL_CMD_FILE_ACCESS_DENIED"
		                         : (dos.errorcode == DOSERR_PATH_NOT_FOUND
		                                    ? "SHELL_ILLEGAL_PATH"
		                                    : "SHELL_CMD_FILE_NOT_FOUND")),
		         word);
		return;
	}
	do {
		n = 1;
		DOS_ReadFile(handle, &c, &n);
		DOS_WriteFile(STDOUT, &c, &n);
		if (c != '\t')
			nchars++;
		else
			show_tabs(nchars);
		if (c == '\n')
			linecount++;
		if (!show_lines(c, nchars, nlines, linecount, word))
			return;
	} while (n);
	DOS_CloseFile(handle);
	if (*args) {
		WriteOut("\n");
		if (PAUSED() == 3)
			return;
		goto nextfile;
	}
}

void MORE_ProgramStart(Program **make)
{
	*make = new MORE;
}
