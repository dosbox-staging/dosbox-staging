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
#include <list>
#include <string>
using namespace std;



#define LINE_IN_MAXLEN 1024

struct MessageBlock
{
   string name;
   string val;
   MessageBlock(const char* _name, const char* _val):
   name(_name),val(_val)
     {}
};

static list<MessageBlock> Lang;
typedef list<MessageBlock>::iterator itmb;
void AddMessage(const char * _name, const char* _val)
{
   Lang.push_back(MessageBlock(_name,_val));
}

void ReplaceMessage(const char * _name, const char* _val)
{
   //find the message
   for(itmb tel=Lang.begin();tel!=Lang.end();tel++)
     {
	if((*tel).name==_name)
	     {  itmb teln=tel;
		teln++;
		Lang.erase(tel,teln);
		break;
	     }
     }
   //even if the message doesn't exist add it 
   AddMessage(_name,_val);
}

static void LoadMessageFile(const char * fname) {
	if(*fname=='\0') return;//empty string=no languagefile
        FILE * mfile=fopen(fname,"rb");
/* This should never happen and since other modules depend on this use a normal printf */
	if (!mfile) {
		E_Exit("MSG:Can't load messages: %s",fname);
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
		/* Replace/Add the string to the internal langaugefile */
		   ReplaceMessage(name,string);
		} else {
		/* Normal string to be added */
			strcat(string,linein);
			strcat(string,"\n");
		}
	}
	fclose(mfile);
}
const char * MSG_Get(char const * msg) {
      for(itmb tel=Lang.begin();tel!=Lang.end();tel++)
      {
	
	        if((*tel).name==msg)
	  {
	     return  (*tel).val.c_str();
	  }
     }
   return "Message not Found!";
}

void MSG_Init(Section_prop * section) {
	/* Load the messages from "dosbox.lang file" */
	std::string file_name;
	if (control->cmdline->FindString("-lang",file_name)) {
		LoadMessageFile(file_name.c_str());
	} else LoadMessageFile(section->Get_string("LANGUAGE"));
}
