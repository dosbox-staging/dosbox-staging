/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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

/* $Id: shell_misc.cpp,v 1.22 2003-08-19 18:01:57 qbix79 Exp $ */

#include <stdlib.h>
#include <string.h>
#include "shell_inc.h"
#include "regs.h"


void DOS_Shell::ShowPrompt(void) {
	Bit8u drive=DOS_GetDefaultDrive()+'A';
	char dir[DOS_PATHLENGTH];
	DOS_GetCurrentDir(0,dir);
	WriteOut("%c:\\%s>",drive,dir);
}

static void outc(Bit8u c) {
	Bit16u n=1;
	DOS_WriteFile(STDOUT,&c,&n);
}

static void outs(char * str) {
	Bit16u n=strlen(str);
	DOS_WriteFile(STDOUT,(Bit8u *)str,&n);
}

void DOS_Shell::InputCommand(char * line) {
	Bitu size=CMD_MAXLINE-1;
	Bit8u c;Bit16u n=1;
	Bitu str_len=0;Bitu str_index=0;
	Bit16u len;

	line[0] = '\0';

	std::list<std::string>::iterator it_history = l_history.begin(), it_completion = l_completion.begin();

	while (size) {
        dos.echo=false;
		DOS_ReadFile(input_handle,&c,&n);
		if (!n) {
			size=0;			//Kill the while loop
			continue;
		}
		switch (c) {
		case 0x00:				/* Extended Keys */
			{
				DOS_ReadFile(input_handle,&c,&n);
				switch (c) {

				case 0x3d:		/* F3 */
					if (!l_history.size()) break;
					it_history = l_history.begin();
					if (it_history != l_history.end() && it_history->length() > str_len) {
						const char *reader = &(it_history->c_str())[str_len];
						while ((c = *reader++)) {
							line[str_index ++] = c;
							DOS_WriteFile(STDOUT,&c,&n);
						}
						str_len = str_index = it_history->length();
						size = CMD_MAXLINE - str_index - 1;
					}
					break;

				case 0x4B:	/* LEFT */
					if (str_index) {
						outc(8);
						str_index --;
					}
					break;

				case 0x4D:	/* RIGHT */
					if (str_index < str_len) {
						outc(line[str_index++]);
					}
					break;

				case 0x48:	/* UP */
					if (l_history.empty() || it_history == l_history.end()) break;

					for (;str_index>0; str_index--) {
						// removes all characters
						outc(8); outc(' '); outc(8);
					}
					strcpy(line, it_history->c_str());
					len = it_history->length();
					str_len = str_index = len;
					size = CMD_MAXLINE - str_index - 1;
					DOS_WriteFile(STDOUT, (Bit8u *)line, &len);
					it_history ++;

					break;

				case 0x50:	/* DOWN */
					if (l_history.empty() || it_history == l_history.begin()) break;

					// not very nice but works ..
					it_history --;
					if (it_history == l_history.begin()) {
						// no previous commands in history
						it_history ++;
						break;
					} else it_history --;

					for (;str_index>0; str_index--) {
						// removes all characters
						outc(8); outc(' '); outc(8);
					}
					strcpy(line, it_history->c_str());
					len = it_history->length();
					str_len = str_index = len;
					size = CMD_MAXLINE - str_index - 1;
					DOS_WriteFile(STDOUT, (Bit8u *)line, &len);
					it_history ++;

					break;

				default:
					break;
				}
			};
			break;
		case 0x08:				/* BackSpace */
			if (str_index) {
				outc(8);
				Bit32u str_remain=str_len - str_index;
				if (str_remain) {
					memmove(&line[str_index-1],&line[str_index],str_remain);
					line[--str_len]=0;
					str_index --;
					/* Go back to redraw */
					for (Bit16u i=str_index; i < str_len; i++)
						outc(line[i]);
				} else {
					line[--str_index] = '\0';
					str_len--;
				}
				outc(' ');	outc(8);
				// moves the cursor left
				while (str_remain--) outc(8);
			}
			if (strlen(line) == 0 && l_completion.size()) l_completion.clear();
			break;
		case 0x0a:				/* New Line not handled */
			/* Don't care */
			break;
		case 0x0d:				/* Return */
			outc('\n');
			size=0;			//Kill the while loop
			break;
		case'\t':
			{
				if (l_completion.size()) {
					it_completion ++;
					if (it_completion == l_completion.end()) it_completion = l_completion.begin();
				} else {
					// build new completion list

					// get completion mask
					char *completion_start = strrchr(line, ' ');

					if (completion_start) {
						completion_start ++;
						completion_index = str_index - strlen(completion_start);
					} else {
						completion_start = line;
						completion_index = 0;
					}

					// build the completion list
					char mask[DOS_PATHLENGTH];
					if (completion_start) {
						strcpy(mask, completion_start);
						// not perfect when line already contains wildcards, but works
						strcat(mask, "*.*");
					} else {
						strcpy(mask, "*.*");
					}

					bool res = DOS_FindFirst(mask, 0xffff & ~DOS_ATTR_VOLUME);
					if (!res) break;	// TODO: beep

					DOS_DTA dta(dos.dta);
					char name[DOS_NAMELENGTH_ASCII];Bit32u size;Bit16u date;Bit16u time;Bit8u attr;

					while (res) {
						dta.GetResult(name,size,date,time,attr);
						// add result to completion list

						char *ext;	// file extension
						if (strcmp(name, ".") && strcmp(name, "..")) {
							ext = strrchr(name, '.');
							if (ext && (strcmp(ext, ".BAT") == 0 || strcmp(ext, ".COM") == 0 || strcmp(ext, ".EXE") == 0))
								// we add executables to the start of the list
								l_completion.push_front(name);
							else
								l_completion.push_back(name);
						}

						res=DOS_FindNext();
					}

					it_completion = l_completion.begin();
				}

				if (l_completion.size() && it_completion->length()) {
					for (;str_index > completion_index; str_index--) {
						// removes all characters
						outc(8); outc(' '); outc(8);
					}

					strcpy(&line[completion_index], it_completion->c_str());
					len = it_completion->length();
					str_len = str_index = completion_index + len;
					size = CMD_MAXLINE - str_index - 1;
					DOS_WriteFile(STDOUT, (Bit8u *)it_completion->c_str(), &len);
				}
			}
			break;
		case 0x1b:   /* ESC */
			//write a backslash and return to the next line
			outc('\\');
			outc('\n');
			*line = 0;      // reset the line.
			if (l_completion.size()) l_completion.clear(); //reset the completion list.
			this->InputCommand(line);	//Get the NEW line.
			size = 0;       // stop the next loop
			str_len = 0;    // prevent multiple adds of the same line
			break;
		default:
			if (l_completion.size()) l_completion.clear();
			line[str_index]=c;
			str_index ++;
			if (str_index > str_len) line[str_index] = '\0';
			str_len++;//This should depend on insert being active
			size--;
			DOS_WriteFile(STDOUT,&c,&n);
			break;
		}
	}

	if (!str_len) return;
	str_len++;

	// add command line to history
	l_history.push_front(line); it_history = l_history.begin();
	if (l_completion.size()) l_completion.clear();
}

void DOS_Shell::Execute(char * name,char * args) {
	char * fullname;
	char line[255];
	if(strlen(args)!= 0){
		if(*args != ' '){ //put a space in front
			line[0]=' ';line[1]=0;
			strcat(line,args);
		}
		else
		{
			strcpy(line,args);
		}
	}else{
		line[0]=0;
	};

	/* check for a drive change */
	if ((strcmp(name + 1, ":") == 0) && isalpha(*name))
	{
		if (!DOS_SetDrive(toupper(name[0])-'A')) {
			WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(name[0]));
		}
		return;
	}
	/* Check for a full name */
	fullname=Which(name);
	if (!fullname) {
		WriteOut(MSG_Get("SHELL_EXECUTE_ILLEGAL_COMMAND"),name);
		return;
	}
	if (strcasecmp(strrchr(fullname, '.'), ".bat") == 0) {
	/* Run the .bat file */
		bf=new BatchFile(this,fullname,line);
	} else {
		if(strcasecmp(strrchr(fullname, '.'), ".com") !=0) {
			if(strcasecmp(strrchr(fullname, '.'), ".exe") !=0){
		  		WriteOut(MSG_Get("SHELL_EXECUTE_ILLEGAL_COMMAND"),fullname);
				return;
			}
	  	}
		/* Run the .exe or .com file from the shell */
		/* Allocate some stack space for tables in physical memory */
		reg_sp-=0x200;
		//Add Parameter block
		DOS_ParamBlock block(SegPhys(ss)+reg_sp);
		block.Clear();
		//Add a filename
		RealPt file_name=RealMakeSeg(ss,reg_sp+0x20);
		MEM_BlockWrite(Real2Phys(file_name),fullname,strlen(fullname)+1);
		/* Fill the command line */
		CommandTail cmd;
		if (strlen(line)>126) line[126]=0;
		cmd.count=strlen(line);
		memcpy(cmd.buffer,line,strlen(line));
		cmd.buffer[strlen(line)]=0xd;
		/* Copy command line in stack block too */
		MEM_BlockWrite(SegPhys(ss)+reg_sp+0x100,&cmd,128);
		/* Parse FCB (first two parameters) and put them into the current DOS_PSP */
		Bit8u add;
		FCB_Parsename(dos.psp,0x5C,0x00,cmd.buffer,&add);
		FCB_Parsename(dos.psp,0x6C,0x00,&cmd.buffer[add],&add);
		block.exec.fcb1=RealMake(dos.psp,0x5C);
		block.exec.fcb2=RealMake(dos.psp,0x6C);
		/* Set the command line in the block and save it */
		block.exec.cmdtail=RealMakeSeg(ss,reg_sp+0x100);
		block.SaveData();
#if 0
		/* Save CS:IP to some point where i can return them from */
		Bit32u oldeip=reg_eip;
		Bit16u oldcs=SegValue(cs);
		RealPt newcsip=CALLBACK_RealPointer(call_shellstop);
		SegSet16(cs,RealSeg(newcsip));
		reg_ip=RealOff(newcsip);
#endif
		/* Start up a dos execute interrupt */
		reg_ax=0x4b00;
		//Filename pointer
		SegSet16(ds,SegValue(ss));
		reg_dx=RealOff(file_name);
		//Paramblock
		SegSet16(es,SegValue(ss));
		reg_bx=reg_sp;
		SETFLAGBIT(IF,false);
		CALLBACK_RunRealInt(0x21);
		/* Restore CS:IP and the stack */
		reg_sp+=0x200;
#if 0
		reg_eip=oldeip;
		SegSet16(cs,oldcs);
#endif
	}
}




static char * bat_ext=".BAT";
static char * com_ext=".COM";
static char * exe_ext=".EXE";
static char which_ret[DOS_PATHLENGTH];

char * DOS_Shell::Which(char * name) {
	/* Parse through the Path to find the correct entry */
	/* Check if name is already ok but just misses an extension */
	if (DOS_FileExists(name)) return name;
	/* try to find .com .exe .bat */
	strcpy(which_ret,name);
	strcat(which_ret,com_ext);
	if (DOS_FileExists(which_ret)) return which_ret;
	strcpy(which_ret,name);
	strcat(which_ret,exe_ext);
	if (DOS_FileExists(which_ret)) return which_ret;
	strcpy(which_ret,name);
	strcat(which_ret,bat_ext);
	if (DOS_FileExists(which_ret)) return which_ret;


	/* No Path in filename look through path environment string */
	static char path[DOS_PATHLENGTH];std::string temp;
	if (!GetEnvStr("PATH",temp)) return 0;
	const char * pathenv=temp.c_str();
	if (!pathenv) return 0;
	pathenv=strchr(pathenv,'=');
	if (!pathenv) return 0;
	pathenv++;
	char * path_write=path;
	while (*pathenv) {
		if (*pathenv!=';') {
			*path_write++=*pathenv++;
		}
		if (*pathenv==';' || *(pathenv)==0) {
			if (*path_write!='\\') *path_write++='\\';
			*path_write++=0;
			strcat(path,name);
			strcpy(which_ret,path);
			if (DOS_FileExists(which_ret)) return which_ret;
			strcpy(which_ret,path);
			strcat(which_ret,com_ext);
			if (DOS_FileExists(which_ret)) return which_ret;
			strcpy(which_ret,path);
			strcat(which_ret,exe_ext);
			if (DOS_FileExists(which_ret)) return which_ret;
			strcpy(which_ret,path);
			strcat(which_ret,bat_ext);
			if (DOS_FileExists(which_ret)) return which_ret;
		
			path_write=path;
			if (*pathenv) pathenv++;
		}
	}
	return 0;
}

