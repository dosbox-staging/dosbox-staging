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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dosbox.h"
#include "cross.h"
#include "support.h"
#include "setup.h"



#define LINE_IN_MAXLEN 1024

struct MessageBlock
{
   char * name;
   char * string;
   MessageBlock * next;
};

static MessageBlock * first_message;


static void LoadMessageFile(char * fname) {
	FILE * mfile=fopen(fname,"rb");
/* This should never happen and since other modules depend on this use a normal printf */
	if (!mfile) {
		E_Exit("MSG:Can't load messages",fname);
	}
	char linein[LINE_IN_MAXLEN];
	char name[LINE_IN_MAXLEN];
	char string[LINE_IN_MAXLEN*10];
	/* Start out with empty strings */
	name[0]=0;string[0]=0;
	while(fgets(linein, LINE_IN_MAXLEN, mfile)!=0) {
		/* Parse the read line */
		/* First remove characters 10 and 13 from the line */
		char * parser=linein;
		char * writer=linein;
		while (*parser) {
			if (*parser!=10 && *parser!=13) {
				*writer++=*parser;
			}
			*parser++;
		}
		*writer=0;
		/* New string name */
		if (linein[0]==':') {
			string[0]=0;
			strcpy(name,linein+1);
		/* End of string marker */
		} else if (linein[0]=='.') {
		/* Save the string internally */
			size_t total=sizeof(MessageBlock)+strlen(name)+1+strlen(string)+1;
			MessageBlock * newblock=(MessageBlock *)malloc(total);
			newblock->name=((char *)newblock)+sizeof(MessageBlock);
			newblock->string=newblock->name+strlen(name)+1;
			strcpy(newblock->name,name);
			strcpy(newblock->string,string);
			newblock->next=first_message;
			first_message=newblock;
		} else {
		/* Normal string to be added */
			strcat(string,linein);
			strcat(string,"\n");
		}
	}
	fclose(mfile);
}


char * MSG_Get(char * msg) {
	MessageBlock * index=first_message;
	while (index) {
		if (!strcmp(msg,index->name)) return index->string;
		index=index->next;
	}
	return "Message not found";
}



void MSG_Init(void) {
	/* Load the messages from "dosbox.lang file" */
	first_message=0;
	char filein[CROSS_LEN];
	strcpy(filein,dosbox_basedir);
	strcat(filein,"dosbox.lang");
	LoadMessageFile(filein);
}
