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

#include <stdlib.h>
#include <string.h>

#include "shell.h"


BatchFile::BatchFile(DOS_Shell * host,char * name, char * cmd_line) {
	prev=host->bf;
	echo=host->echo;
	shell=host;
	cmd=new CommandLine(0,cmd_line);
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
	Bit8u c;Bit16u n;
	char temp[CMD_MAXLINE];
emptyline:
	char * cmd_write=temp;
	do {
		n=1;
		DOS_ReadFile(file_handle,&c,&n);
		if (n>0) {
			if (c>31 || c==0x1b || c=='\t')
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
			if (cmd_read[0]=='%') {
				cmd_read++;
				*cmd_write++='%';
			}
			size_t len=strspn(cmd_read,"0123456789");
			if (len) {
				memcpy(env_name,cmd_read,len);
				env_name[len]=0;cmd_read+=len;
				len=atoi(env_name);
				if (cmd->GetCount()<len) continue;
				std::string word;
				if (!cmd->FindCommand(len,word)) continue;
				strcpy(cmd_write,word.c_str());
				cmd_write+=strlen(word.c_str());
				continue;
			} else {
				/* Not a command line number has to be an environment */
				char * first=strchr(cmd_read,'%');
				if (!first) continue; *first++=0;
				std::string temp;
				if (shell->GetEnvStr(cmd_read,temp)) {
					const char * equals=strchr(temp.c_str(),'=');
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
	char cmd[CMD_MAXLINE];
	char * cmd_write;
	DOS_SeekFile(file_handle,&pos,DOS_SEEK_SET);

	/* Scan till we have a match or return false */
	Bit8u c;Bit16u n;
again:
	cmd_write=cmd;
	do {
		n=1;
		DOS_ReadFile(file_handle,&c,&n);
		if (n>0) {
			if (c>31)
				*cmd_write++=c;
		}
	} while (c!='\n' && n);
	*cmd_write++=0;
	if (cmd[0]==':') {
		if (strcasecmp(cmd+1,where)==0) return true;
	}
	if (!n) {
		delete this;
		return false;	
	}
	goto again;
	return false;
};
