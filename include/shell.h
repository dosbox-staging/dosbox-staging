/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: shell.h,v 1.8 2004-10-26 18:28:51 qbix79 Exp $ */

#ifndef SHELL_H_
#define SHELL_H_


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

#include <string>
#include <list>

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
	bool echo;
	DOS_Shell * shell;
	BatchFile * prev;
	CommandLine * cmd;
};

class DOS_Shell : public Program {
private:
	std::list<std::string> l_history, l_completion;

	char *completion_start;
	Bit16u completion_index;
	
public:

	DOS_Shell();

	void Run(void);
	void RunInternal(void); //for command /C
/* A load of subfunctions */
	void ParseLine(char * line);
	Bitu GetRedirection(char *s, char **ifn, char **ofn,bool * append);
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
	void CMD_DELETE(char * args);
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
	void CMD_RENAME(char * args);
	void CMD_CALL(char * args);
	void SyntaxError(void);
	void CMD_PAUSE(char * args);
	void CMD_SUBST(char* args);
	void CMD_LOADHIGH(char* args);
	void CMD_CHOICE(char * args);
	void CMD_ATTRIB(char * args);
	/* The shell's variables */
	Bit16u input_handle;
	BatchFile * bf;
	bool echo;
	bool exit;
	bool call;
};

struct SHELL_Cmd {
    const char * name;								/* Command name*/
	Bit32u flags;									/* Flags about the command */
    void (DOS_Shell::*handler)(char * args);		/* Handler for this command */
    const char * help;								/* String with command help */
};

static inline void StripSpaces(char*&args) {
	while(args && *args && isspace(*reinterpret_cast<unsigned char*>(args)))
		args++;
}

static inline char* ExpandDot(char*args, char* buffer) {
	if(*args=='.')
	{
		if(*(args+1)==0)
		{
			strcpy(buffer,"*.*");
			return buffer;
		}
		if( (*(args+1)!='.') && (*(args+1)!='\\') )
		{
			buffer[0]='*';
			buffer[1]=0;
			strcat(buffer,args);
			return buffer;
		} else 
			strcpy (buffer, args); 
	}
	else strcpy(buffer,args);
	return buffer;
}

#endif
