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
	char * prev=old.buffer;
	char * reader;
	Bitu size=CMD_MAXLINE-1;
	Bit8u c;Bit16u n=1;
	Bitu str_len=0;Bitu str_index=0;

	while (size) {
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
					if (strlen(old.buffer)>str_len) {
						reader=&old.buffer[str_len];
						while (c=*reader++) {
							line[str_index]=c;
							str_len++;
							str_index++;
							size--;
							DOS_WriteFile(STDOUT,&c,&n);
						}
					}
					break;
				default:
					break;


				}
			};
			break;
		case 0x08:				/* BackSpace */
			if (str_index>0) {
				Bit32u str_remain=str_len-str_index;
				if (str_remain) {
					memcpy(&line[str_index-1],&line[str_index],str_remain);
					line[str_len]=0;
					/* Go back to redraw */
					for (;str_remain>0;str_remain--) {
						outc(8);
					}
				}
				str_index--;str_len--;
				outc(8);
				outc(' ');				
				outc(8);
				
			}
			break;
		case 0x0a:				/* New Line not handled */
			/* Don't care */
			break;
		case 0x0d:				/* Return */
			outc('\n');
			size=0;			//Kill the while loop
			break;
		default:
			line[str_index]=c;
			str_len++;//This should depend on insert being active
			str_index++;
			size--;
			DOS_WriteFile(STDOUT,&c,&n);
			break;
		}
	}
/* String is inputted now save it in the buffer */
	line[str_len]=0;
	if (!str_len) return;
	str_len++;
	//Not quite perfect last entries can get screwed :)
	size_t first_len=strlen(old.buffer)+1;
	memmove(&old.buffer[first_len],&old.buffer[0],CMD_OLDSIZE-first_len);
	strcpy(old.buffer,line);		



}

void DOS_Shell::Execute(char * name,char * args) {
	char * fullname;

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
		bf=new BatchFile(this,fullname,args);
	} else {
		/* Run the .exe or .com file from the shell */
		/* Allocate some stack space for tables in physical memory */
		reg_sp-=0x200;
		//Add Parameter block
		DOS_ParamBlock block(SegPhys(ss)+reg_sp);
		//Add a filename
		RealPt file_name=RealMakeSeg(ss,reg_sp+0x20);
		MEM_BlockWrite(Real2Phys(file_name),fullname,strlen(fullname)+1);
		/* Fill the command line */
		CommandTail cmd;
		if (strlen(args)>126) args[126]=0;
		cmd.count=strlen(args);
		memcpy(cmd.buffer,args,strlen(args));
		cmd.buffer[strlen(args)]=0xd;
		MEM_BlockWrite(PhysMake(prog_info->psp_seg,128),&cmd,128);

		block.InitExec(RealMake(prog_info->psp_seg,128));
		/* Save CS:IP to some point where i can return them from */
		RealPt newcsip;
		newcsip=CALLBACK_RealPointer(call_shellstop);
		SegSet16(cs,RealSeg(newcsip));
		reg_ip=RealOff(newcsip);
		/* Start up a dos execute interrupt */
		reg_ax=0x4b00;
		//Filename pointer
		SegSet16(ds,SegValue(ss));
		reg_dx=RealOff(file_name);
		//Paramblock
		SegSet16(es,SegValue(ss));
		reg_bx=reg_sp;
		flags.intf=false;
		CALLBACK_RunRealInt(0x21);
		reg_sp+=0x200;
	}


}




static char * bat_ext=".BAT";
static char * com_ext=".COM";
static char * exe_ext=".EXE";
static char which_ret[DOS_PATHLENGTH];

char * DOS_Shell::Which(char * name) {
	/* Parse through the Path to find the correct entry */
	/* Check if name is already ok but just misses an extension */
	char * ext=strrchr(name,'.');
	if (ext) if (strlen(ext)>4) ext=0;
	if (ext) {
		if (DOS_FileExists(name)) return name;
	} else {
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
	}

	/* No Path in filename look through %path% */
	static char path[DOS_PATHLENGTH];
	char * pathenv=GetEnvStr("PATH");
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
			if (ext) {
				if (DOS_FileExists(which_ret)) return which_ret;
			} else {
				strcpy(which_ret,path);
				strcat(which_ret,com_ext);
				if (DOS_FileExists(which_ret)) return which_ret;
				strcpy(which_ret,path);
				strcat(which_ret,exe_ext);
				if (DOS_FileExists(which_ret)) return which_ret;
				strcpy(which_ret,path);
				strcat(which_ret,bat_ext);
				if (DOS_FileExists(which_ret)) return which_ret;
			}
			path_write=path;
			if (*pathenv) pathenv++;
		}
	}
	return 0;
}

