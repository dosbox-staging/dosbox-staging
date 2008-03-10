/*
 *  Copyright (C) 2002-2007  The DOSBox Team
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

/* $Id: shell_batch.cpp,v 1.27 2008-03-10 13:43:37 qbix79 Exp $ */

#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "support.h"

BatchFile::BatchFile(DOS_Shell * host,char const * const name, char const * const cmd_line) {
	prev=host->bf;
	echo=host->echo;
	shell=host;
	cmd=new CommandLine(name,cmd_line);
	if (!DOS_OpenFile(name,128,&file_handle)) {
		//TODO Come up with something better
		E_Exit("SHELL:Can't open BatchFile");
	}
};

BatchFile::~BatchFile() {
	delete cmd;
	DOS_CloseFile(file_handle);
	shell->bf=prev;
	shell->echo=echo;
}

bool BatchFile::ReadLine(char * line) {
	Bit8u c=0;Bit16u n=1;
	char temp[CMD_MAXLINE];
emptyline:
	char * cmd_write=temp;
	do {
		n=1;
		DOS_ReadFile(file_handle,&c,&n);
		if (n>0) {
			/* Why are we filtering this ?
			 * Exclusion list: tab for batch files 
			 * escape for ansi
			 * backspace for alien odyssey */
			if (c>31 || c==0x1b || c=='\t' || c==8)
				*cmd_write++=c;
		}
	} while (c!='\n' && n);
	*cmd_write=0;
	if (!n && cmd_write==temp) {
		delete this;
		return false;	
	}
	if (!strlen(temp)) goto emptyline;
	if (temp[0]==':') goto emptyline;
/* Now parse the line read from the bat file for % stuff */
	cmd_write=line;
	char * cmd_read=temp;
	char env_name[256];char * env_write;
	while (*cmd_read) {
		env_write=env_name;
		if (*cmd_read=='%') {
			cmd_read++;
			if (cmd_read[0] == '%') {
				cmd_read++;
				*cmd_write++='%';
				continue;
			}
			if (cmd_read[0] == '0') {  /* Handle %0 */
				const char *file_name = cmd->GetFileName();
				cmd_read++;
				strcpy(cmd_write,file_name);
				cmd_write+=strlen(file_name);
				continue;
			}
			char next = cmd_read[0];
			if(next > '0' && next <= '9') {  
				/* Handle %1 %2 .. %9 */
				cmd_read++; //Progress reader
				next -= '0';
				if (cmd->GetCount()<next) continue;
				std::string word;
				if (!cmd->FindCommand(next,word)) continue;
				strcpy(cmd_write,word.c_str());
				cmd_write+=strlen(word.c_str());
				continue;
			} else {
				/* Not a command line number has to be an environment */
				char * first=strchr(cmd_read,'%');
				if (!first) continue; *first++=0;
				std::string env;
				if (shell->GetEnvStr(cmd_read,env)) {
					const char * equals=strchr(env.c_str(),'=');
					if (!equals) continue;
					equals++;
					strcpy(cmd_write,equals);
					cmd_write+=strlen(equals);
				}
				cmd_read=first;
			}
		} else {
			*cmd_write++=*cmd_read++;
		}
	}
	*cmd_write=0;
	return true;	
}

bool BatchFile::Goto(char * where) {
	Bit32u pos=0;
	char cmd_buffer[CMD_MAXLINE];
	char * cmd_write;
	DOS_SeekFile(file_handle,&pos,DOS_SEEK_SET);

	/* Scan till we have a match or return false */
	Bit8u c;Bit16u n;
again:
	cmd_write=cmd_buffer;
	do {
		n=1;
		DOS_ReadFile(file_handle,&c,&n);
		if (n>0) {
			if (c>31)
				*cmd_write++=c;
		}
	} while (c!='\n' && n);
	*cmd_write++=0;
	char *nospace = trim(cmd_buffer);
	if (nospace[0] == ':') {
		char* nonospace = trim(nospace+1);
		if (strcasecmp(nonospace,where)==0) return true;
	}
	if (!n) {
		delete this;
		return false;	
	}
	goto again;
	return false;
};
void BatchFile::Shift(void) {
	cmd->Shift(1);
}
