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

#include "shell_inc.h"
#include "cpu.h"


BatchFile::BatchFile(DOS_Shell * host,char * name, char * cmd_line) {
	/* Go through the command line */
	char * cmd_write=cmd_buffer;
	prev=host->bf;
	echo=host->echo;
	shell=host;
	cmd_count=0;
	while (*cmd_line || (cmd_count<CMD_MAXCMDS)) {
		cmd_words[cmd_count]=cmd_write;	
		for (;(*cmd_line!=' ' && *cmd_line!=0);*cmd_write++=*cmd_line++);
		*cmd_write++=0;
		cmd_line=trim(cmd_line);
		cmd_count++;
	}
	if (!DOS_OpenFile(name,0,&file_handle)) {
		//TODO Come up with something better
		E_Exit("SHELL:Can't open BatchFile");
	}
	line_count=0;
};

BatchFile::~BatchFile() {
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
			if (c>31)
				*cmd_write++=c;
		}
	} while (c!='\n' && n);
	*cmd_write++=0;
	if (!n) {
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
			/* Find the fullstring of this */


		continue;

		} else {
			*cmd_write++=*cmd_read++;
		}
	*cmd_write=0;
	}
	return true;
	
}


bool BatchFile::Goto(char * where) {
	Bit32u pos=0;
	char cmd[CMD_MAXLINE];
	char * cmd_write;
	DOS_SeekFile(file_handle,&pos,DOS_SEEK_SET);

	/* Scan till we have a match or return false*/
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
