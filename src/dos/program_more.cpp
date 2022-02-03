/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
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

constexpr char CHAR_BR = 3, CHAR_LF = 10, CHAR_CR = 13, CHAR_EOL = 26, TAB_SIZE = 8;
static char paused_char(bool out_con)
{
	uint8_t c;
	uint16_t n = 1, handle;
	if (out_con && DOS_OpenFile("con", OPEN_READWRITE, &handle)) {
		DOS_ReadFile(handle, &c, &n);
		DOS_CloseFile(handle);
	} else
		DOS_ReadFile(STDIN, &c, &n);
	return c;
}

void MORE::show_tabs(int &nchars)
{
	const auto COLS = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	do {
		WriteOut(" ");
		nchars++;
	} while (nchars < COLS && nchars % TAB_SIZE);
}

bool MORE::show_lines(
        const uint8_t c, int &nchars, int &nlines, int line_count, const char *word)
{
	const auto COLS = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	const auto LINES = IS_EGAVGA_ARCH ? real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS)
	                                  : 24;
	if ((c == '\n') || (nchars >= COLS)) {
		nlines++;
		nchars = 0;
		if (*word ? !output_con : false) {
			if (c != '\n')
				WriteOut("\n");
		} else if (nlines == LINES) {
			WriteOut("-- More -- %s (%u) --", word, line_count);
			if (paused_char(true) == CHAR_BR) {
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
	char *word = NULL;
	int nchars = 0;
	int nlines = 0;
	int line_count = 0;
	uint8_t c = 0;
	uint8_t last = 0;
	uint16_t n = 1;
	ltrim(args);
	if (!*args || !strcasecmp(args, "con")) {
		while (true) {
			DOS_ReadFile(STDIN, &c, &n);
			if (c == CHAR_BR) {
				dos.echo = first_shell->echo;
				WriteOut("^C\n");
				return;
			}
			if (!n) {
				if (last != CHAR_LF)
					WriteOut("\r\n");
				return;
			} else if (c == CHAR_CR && last == CHAR_EOL)
				return;
			else {
				if (c == CHAR_CR) {
					line_count++;
					WriteOut("\r\n");
				} else if (c == '\t') {
					show_tabs(nchars);
				} else if (c != CHAR_LF) {
					nchars++;
					WriteOut("%c", c);
				}
				if (!show_lines(c, nchars, nlines, line_count, ""))
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
			line_count++;
		if (!show_lines(c, nchars, nlines, line_count, word))
			return;
	} while (n);
	DOS_CloseFile(handle);
	if (*args) {
		WriteOut("\n");
		if (paused_char(!output_con) == CHAR_BR)
			return;
		goto nextfile;
	}
}

void MORE_ProgramStart(Program **make)
{
	*make = new MORE;
}
