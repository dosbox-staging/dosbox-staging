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
#include <ctype.h>
#include <stdio.h>
#include "dosbox.h"
#include "mem.h"
#include "programs.h"
#include "dos_inc.h"
#include "regs.h"
#include "support.h"
#include "callback.h"
#include "setup.h"

#define CMD_MAXLINE 4096
#define CMD_MAXCMDS 20
#define CMD_OLDSIZE 4096
extern Bitu call_shellstop;
class DOS_Shell;


class BatchFile {
public:
	BatchFile(DOS_Shell * host,char * name, char * cmd_line);
	~BatchFile();
	bool ReadLine(char * line);
	bool Goto(char * where);
	Bit16u file_handle;
	Bit32u line_count;	
	char * cmd_words[CMD_MAXCMDS];
	char cmd_buffer[128];							//Command line can only be 128 chars
	Bit32u cmd_count;
	bool echo;
	DOS_Shell * shell;
	BatchFile * prev;
	CommandLine * cmd;
};



class DOS_Shell : public Program {
public:
	DOS_Shell();
	void Run(void);
/* A load of subfunctions */
	void ParseLine(char * line);
	Bit32u GetRedirection(char *s, char **ifn, char **ofn);
	void InputCommand(char * line);
	void ShowPrompt();
	void DoCommand(char * cmd);
	void Execute(char * name,char * args);
/* Some internal used functions */
	char * Which(char * name);
/* Some supported commands */
	void CMD_HELP(char * args);
	void CMD_CLS(char * args);
	void CMD_COPY(char * args);
	void CMD_DIR(char * args);
	void CMD_ECHO(char * args);
	void CMD_EXIT(char * args);
	void CMD_MKDIR(char * args);
	void CMD_CHDIR(char * args);
	void CMD_RMDIR(char * args);
	void CMD_SET(char * args);
	void CMD_IF(char * args);
	void CMD_GOTO(char * args);
	void CMD_TYPE(char * args);
	void CMD_REM(char * args);
	void SyntaxError(void);

	/* The shell's variables */
	Bit16u input_handle;
	BatchFile * bf;
	bool echo;
	bool exit;
	struct {
		char buffer[CMD_OLDSIZE];
		Bitu index;
		Bitu size;
	} old;

};

struct SHELL_Cmd {
    const char * name;								/* Command name*/
	Bit32u flags;									/* Flags about the command */
    void (DOS_Shell::*handler)(char * args);		/* Handler for this command */
    const char * help;								/* String with command help */
};


