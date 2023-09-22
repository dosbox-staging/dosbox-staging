/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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


#include "dosbox.h"
#include "shell.h"
#include "callback.h"
#include "regs.h"
#include "pic.h"
#include "keyboard.h"
#include "timer.h"
#include "../src/ints/int10.h"
#include <time.h>
#include <assert.h>
#include "bios.h"
#include "../dos/drives.h"
#include "support.h"
#include "control.h"
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <vector>
#include <string>
#include <time.h>

static SHELL_Cmd cmd_list[]={
{	"DIR",		0,			&DOS_Shell::CMD_DIR,		"SHELL_CMD_DIR_HELP"},
{	"CHDIR",	1,			&DOS_Shell::CMD_CHDIR,		"SHELL_CMD_CHDIR_HELP"},
{	"ATTRIB",	1,			&DOS_Shell::CMD_ATTRIB,		"SHELL_CMD_ATTRIB_HELP"},
{	"CALL",		1,			&DOS_Shell::CMD_CALL,		"SHELL_CMD_CALL_HELP"},
{	"CD",		0,			&DOS_Shell::CMD_CHDIR,		"SHELL_CMD_CHDIR_HELP"},
{	"CHOICE",	1,			&DOS_Shell::CMD_CHOICE,		"SHELL_CMD_CHOICE_HELP"},
{	"CLS",		0,			&DOS_Shell::CMD_CLS,		"SHELL_CMD_CLS_HELP"},
{	"COPY",		0,			&DOS_Shell::CMD_COPY,		"SHELL_CMD_COPY_HELP"},
{	"DATE",		0,			&DOS_Shell::CMD_DATE,		"SHELL_CMD_DATE_HELP"},
{	"DEL",		0,			&DOS_Shell::CMD_DELETE,		"SHELL_CMD_DELETE_HELP"},
{	"DELETE",	1,			&DOS_Shell::CMD_DELETE,		"SHELL_CMD_DELETE_HELP"},
{	"ERASE",	1,			&DOS_Shell::CMD_DELETE,		"SHELL_CMD_DELETE_HELP"},
{	"ECHO",		1,			&DOS_Shell::CMD_ECHO,		"SHELL_CMD_ECHO_HELP"},
{	"EXIT",		0,			&DOS_Shell::CMD_EXIT,		"SHELL_CMD_EXIT_HELP"},	
{	"GOTO",		1,			&DOS_Shell::CMD_GOTO,		"SHELL_CMD_GOTO_HELP"},
{	"HELP",		1,			&DOS_Shell::CMD_HELP,		"SHELL_CMD_HELP_HELP"},
{	"IF",		1,			&DOS_Shell::CMD_IF,			"SHELL_CMD_IF_HELP"},
{	"LOADHIGH",	1,			&DOS_Shell::CMD_LOADHIGH, 	"SHELL_CMD_LOADHIGH_HELP"},
{	"LH",		1,			&DOS_Shell::CMD_LOADHIGH,	"SHELL_CMD_LOADHIGH_HELP"},
{	"MKDIR",	1,			&DOS_Shell::CMD_MKDIR,		"SHELL_CMD_MKDIR_HELP"},
{	"MD",		0,			&DOS_Shell::CMD_MKDIR,		"SHELL_CMD_MKDIR_HELP"},
{	"PATH",		1,			&DOS_Shell::CMD_PATH,		"SHELL_CMD_PATH_HELP"},
{	"PAUSE",	1,			&DOS_Shell::CMD_PAUSE,		"SHELL_CMD_PAUSE_HELP"},
{	"RMDIR",	1,			&DOS_Shell::CMD_RMDIR,		"SHELL_CMD_RMDIR_HELP"},
{	"RD",		0,			&DOS_Shell::CMD_RMDIR,		"SHELL_CMD_RMDIR_HELP"},
{	"REM",		1,			&DOS_Shell::CMD_REM,		"SHELL_CMD_REM_HELP"},
{	"RENAME",	1,			&DOS_Shell::CMD_RENAME,		"SHELL_CMD_RENAME_HELP"},
{	"REN",		0,			&DOS_Shell::CMD_RENAME,		"SHELL_CMD_RENAME_HELP"},
{	"SET",		1,			&DOS_Shell::CMD_SET,		"SHELL_CMD_SET_HELP"},
{	"SHIFT",	1,			&DOS_Shell::CMD_SHIFT,		"SHELL_CMD_SHIFT_HELP"},
{	"SUBST",	1,			&DOS_Shell::CMD_SUBST,		"SHELL_CMD_SUBST_HELP"},
{	"TIME",		0,			&DOS_Shell::CMD_TIME,		"SHELL_CMD_TIME_HELP"},
{	"TYPE",		0,			&DOS_Shell::CMD_TYPE,		"SHELL_CMD_TYPE_HELP"},
{	"VER",		0,			&DOS_Shell::CMD_VER,		"SHELL_CMD_VER_HELP"},
{	"ADDKEY",	1,			&DOS_Shell::CMD_ADDKEY,		"SHELL_CMD_ADDKEY_HELP"},
{	"VOL",	0,			&DOS_Shell::CMD_VOL,		"SHELL_CMD_VOL_HELP"},
{	"PROMPT",	0,			&DOS_Shell::CMD_PROMPT,		"SHELL_CMD_PROMPT_HELP"},
{	"LABEL",	0,			&DOS_Shell::CMD_LABEL,		"SHELL_CMD_LABEL_HELP"},
//{	"MORE",	1,			&DOS_Shell::CMD_MORE,		"SHELL_CMD_MORE_HELP"},
{	"FOR",	1,			&DOS_Shell::CMD_FOR,		"SHELL_CMD_FOR_HELP"},
{	"INT2FDBG",	1,			&DOS_Shell::CMD_INT2FDBG,	"Hook INT 2Fh for debugging purposes"},
{0,0,0,0}
}; 

/* support functions */
static char empty_char = 0;
static char* empty_string = &empty_char;
static void StripSpaces(char*&args) {
	while(args && *args && isspace(*reinterpret_cast<unsigned char*>(args)))
		args++;
}

static void StripSpaces(char*&args,char also) {
	while(args && *args && (isspace(*reinterpret_cast<unsigned char*>(args)) || (*args == also)))
		args++;
}

static char* ExpandDot(char*args, char* buffer) {
	if(*args == '.') {
		if(*(args+1) == 0){
			strcpy(buffer,"*.*");
			return buffer;
		}
		if( (*(args+1) != '.') && (*(args+1) != '\\') ) {
			buffer[0] = '*';
			buffer[1] = 0;
			strcat(buffer,args);
			return buffer;
		} else
			strcpy (buffer, args);
	}
	else strcpy(buffer,args);
	return buffer;
}



bool DOS_Shell::CheckConfig(char* cmd_in,char*line) {
	Section* test = control->GetSectionFromProperty(cmd_in);
	if(!test) return false;
	if(line && !line[0]) {
		std::string val = test->GetPropValue(cmd_in);
		if(val != NO_SUCH_PROPERTY) WriteOut("%s\n",val.c_str());
		return true;
	}
	char newcom[1024]; newcom[0] = 0; strcpy(newcom,"z:\\config -set ");
	strcat(newcom,test->GetName());	strcat(newcom," ");
	strcat(newcom,cmd_in);strcat(newcom,line);
	DoCommand(newcom);
	return true;
}

void DOS_Shell::DoCommand(char * line) {
/* First split the line into command and arguments */
	line=trim(line);
	char cmd_buffer[CMD_MAXLINE];
	char * cmd_write=cmd_buffer;
	while (*line) {
		if (*line == 32) break;
		if (*line == '/') break;
		if (*line == '\t') break;
		if (*line == '=') break;
//		if (*line == ':') break; //This breaks drive switching as that is handled at a later stage. 
		if ((*line == '.') ||(*line == '\\')) {  //allow stuff like cd.. and dir.exe cd\kees
			*cmd_write=0;
			Bit32u cmd_index=0;
			while (cmd_list[cmd_index].name) {
				if (strcasecmp(cmd_list[cmd_index].name,cmd_buffer)==0) {
					(this->*(cmd_list[cmd_index].handler))(line);
			 		return;
				}
				cmd_index++;
			}
		}
		*cmd_write++=*line++;
	}
	*cmd_write=0;
	if (strlen(cmd_buffer)==0) return;
/* Check the internal list */
	Bit32u cmd_index=0;
	while (cmd_list[cmd_index].name) {
		if (strcasecmp(cmd_list[cmd_index].name,cmd_buffer)==0) {
			(this->*(cmd_list[cmd_index].handler))(line);
			return;
		}
		cmd_index++;
	}
/* This isn't an internal command execute it */
	if(Execute(cmd_buffer,line)) return;
	if(CheckConfig(cmd_buffer,line)) return;
	WriteOut(MSG_Get("SHELL_EXECUTE_ILLEGAL_COMMAND"),cmd_buffer);
}

#define HELP(command) \
	if (ScanCMDBool(args,"?")) { \
		WriteOut(MSG_Get("SHELL_CMD_" command "_HELP")); \
		const char* long_m = MSG_Get("SHELL_CMD_" command "_HELP_LONG"); \
		WriteOut("\n"); \
		if(strcmp("Message not Found!\n",long_m)) WriteOut(long_m); \
		else WriteOut(command "\n"); \
		return; \
	}

Bitu int2fdbg_hook_callback = 0;

static Bitu INT2FDBG_Handler(void) {
	if (reg_ax == 0x1605) { /* Windows init broadcast */
		int patience = 500;
		Bitu st_seg,st_ofs;

		LOG_MSG("INT 2Fh debug hook: Caught Windows init broadcast results (ES:BX=%04x:%04x DS:SI=%04x:%04x CX=%04x DX=%04x DI=%04x)\n",
			SegValue(es),reg_bx,
			SegValue(ds),reg_si,
			reg_cx,reg_dx,reg_di);

		st_seg = SegValue(es);
		st_ofs = reg_bx;
		while (st_seg != 0 || st_ofs != 0) {
			unsigned char v_major,v_minor;
			Bitu st_seg_next,st_ofs_next;
			Bitu idrc_seg,idrc_ofs;
			Bitu vdev_seg,vdev_ofs;
			Bitu name_seg,name_ofs;
			char devname[64];
			PhysPt st_o;

			if (--patience <= 0) {
				LOG_MSG("**WARNING: Chain is too long. Something might have gotten corrupted\n");
				break;
			}

			st_o = PhysMake(st_seg,st_ofs);
			/* +0x00: Major, minor version of info structure
			 * +0x02: pointer to next startup info structure or 0000:0000
			 * +0x06: pointer to ASCIIZ name of virtual device or 0000:0000
			 * +0x0A: virtual device ref data (pointer to?? or actual data??) or 0000:0000
			 * +0x0E: pointer to instance data records or 0000:0000
			 * Windows 95 or later (v4.0+):
			 * +0x12: pointer to optionally-instanced data records or 0000:0000 */
			v_major = mem_readb(st_o+0x00);
			v_minor = mem_readb(st_o+0x01);
			st_seg_next = mem_readw(st_o+0x02+2);
			st_ofs_next = mem_readw(st_o+0x02+0);
			name_ofs = mem_readw(st_o+0x06+0);
			name_seg = mem_readw(st_o+0x06+2);
			vdev_ofs = mem_readw(st_o+0x0A+0);
			vdev_seg = mem_readw(st_o+0x0A+2);
			idrc_ofs = mem_readw(st_o+0x0A+4);	/* FIXME: 0x0E+0 and 0x0E+2 generates weird compiler error WTF?? */
			idrc_seg = mem_readw(st_o+0x0A+6);

			{
				devname[0] = 0;
				if (name_seg != 0 || name_ofs != 0) {
					unsigned int i;
					PhysPt scan;
					char c;

					scan = PhysMake(name_seg,name_ofs);
					for (i=0;i < 63 && (c=mem_readb(scan++)) != 0;) devname[i++] = c;
					devname[i] = 0;
				}
			}

			LOG_MSG(" >> Version %u.%u\n",v_major,v_minor);
			LOG_MSG("    Next entry at %04x:%04x\n",st_seg_next,st_ofs_next);
			LOG_MSG("    Virtual device name: %04x:%04x '%s'\n",name_seg,name_ofs,devname);
			LOG_MSG("    Virtual dev ref data: %04x:%04x\n",vdev_seg,vdev_ofs);
			LOG_MSG("    Instance data records: %04x:%04x\n",idrc_seg,idrc_ofs);

			st_seg = st_seg_next;
			st_ofs = st_ofs_next;
		}

		LOG_MSG("----END CHAIN\n");
	}

	return CBRET_NONE;
}

/* NTS: I know I could just modify the DOS kernel's INT 2Fh code to receive the init call,
 *      the problem is that at that point, the registers do not yet contain anything interesting.
 *      all the interesting results of the call are added by TSRs on the way back UP the call
 *      chain. The purpose of this program therefore is to hook INT 2Fh on the other end
 *      of the call chain so that we can see the results just before returning INT 2Fh back
 *      to WIN.COM */
void DOS_Shell::CMD_INT2FDBG(char * args) {
	/* TODO: Allow /U to remove INT 2Fh hook */

	if (ScanCMDBool(args,"I")) {
		if (int2fdbg_hook_callback == 0) {
			Bit32u old_int2Fh;
			PhysPt w;

			int2fdbg_hook_callback = CALLBACK_Allocate();
			CALLBACK_Setup(int2fdbg_hook_callback,&INT2FDBG_Handler,CB_IRET,"INT 2Fh DBG callback");

			/* record old vector, set our new vector */
			old_int2Fh = RealGetVec(0x2f);
			w = CALLBACK_PhysPointer(int2fdbg_hook_callback);
			RealSetVec(0x2f,CALLBACK_RealPointer(int2fdbg_hook_callback));

			/* overwrite the callback with code to chain the call down, then invoke our callback on the way back up: */

			/* first, chain to the previous INT 15h handler */
			phys_writeb(w++,(Bit8u)0x9C);					//PUSHF
			phys_writeb(w++,(Bit8u)0x9A);					//CALL FAR <address>
			phys_writew(w,(Bit16u)(old_int2Fh&0xFFFF)); w += 2;		//offset
			phys_writew(w,(Bit16u)((old_int2Fh>>16)&0xFFFF)); w += 2;	//seg

			/* then, having returned from it, invoke our callback */
			phys_writeb(w++,(Bit8u)0xFE);					//GRP 4
			phys_writeb(w++,(Bit8u)0x38);					//Extra Callback instruction
			phys_writew(w,(Bit16u)int2fdbg_hook_callback); w += 2;		//The immediate word

			/* return */
			phys_writeb(w++,(Bit8u)0xCF);					//IRET

			LOG_MSG("INT 2Fh debugging hook set\n");
			WriteOut("INT 2Fh hook set\n");
		}
		else {
			WriteOut("INT 2Fh hook already setup\n");
		}
	}
	else {
		WriteOut("INT2FDBG [switches]\n");
		WriteOut("INT2FDBG.COM Int 2Fh debugging hook.\n");
		WriteOut("  /I      Install hook\n");
		WriteOut("\n");
		WriteOut("Hooks INT 2Fh at the top of the call chain for debugging information.\n");
	}
}

void DOS_Shell::CMD_CLS(char * args) {
	HELP("CLS");
   if (CurMode->type==M_TEXT) WriteOut("[2J"); 
   else { 
      reg_ax=(Bit16u)CurMode->mode; 
      CALLBACK_RunRealInt(0x10); 
   } 
}

void DOS_Shell::CMD_DELETE(char * args) {
	HELP("DELETE");
	bool optQ1=ScanCMDBool(args,"Q");
	bool optP=ScanCMDBool(args,"P");

	// ignore /p /f, /s, /ar, /as, /ah and /aa switches for compatibility
	ScanCMDBool(args,"P");
	ScanCMDBool(args,"F");
	ScanCMDBool(args,"S");
	ScanCMDBool(args,"AR");
	ScanCMDBool(args,"AS");
	ScanCMDBool(args,"AH");
	ScanCMDBool(args,"AA");

	StripSpaces(args);
	if (!strcasecmp(args,"*")) args=(char*)("*.*"); // 'del *' should be 'del *.*'?
	if (!strcasecmp(args,"*.*")) {
		if (!optQ1) {
first_1:
			WriteOut(MSG_Get("SHELL_CMD_DEL_SURE"));
first_2:
			Bit8u c;Bit16u n=1;
			DOS_ReadFile (STDIN,&c,&n);
			do switch (c) {
			case 'n':			case 'N':
			{
				DOS_WriteFile (STDOUT,&c, &n);
				DOS_ReadFile (STDIN,&c,&n);
				do switch (c) {
					case 0xD: WriteOut("\n"); return;
					case 0x08: WriteOut("\b \b"); goto first_2;
				} while (DOS_ReadFile (STDIN,&c,&n));
			}
			case 'y':			case 'Y':
			{
				DOS_WriteFile (STDOUT,&c, &n);
				DOS_ReadFile (STDIN,&c,&n);
				do switch (c) {
					case 0xD: WriteOut("\n"); goto continue_1;
					case 0x08: WriteOut("\b \b"); goto first_2;
				} while (DOS_ReadFile (STDIN,&c,&n));
			}
			case 0xD: WriteOut("\n"); goto first_1;
			case '\t':
			case 0x08:
				goto first_2;
			default:
			{
				DOS_WriteFile (STDOUT,&c, &n);
				DOS_ReadFile (STDIN,&c,&n);
				do switch (c) {
					case 0xD: WriteOut("\n"); goto first_1;
					case 0x08: WriteOut("\b \b"); goto first_2;
				} while (DOS_ReadFile (STDIN,&c,&n));
				goto first_2;
			}
		} while (DOS_ReadFile (STDIN,&c,&n));
	}
}

continue_1:
	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);

	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	/* If delete accept switches mind the space infront of them. See the dir /p code */ 

	char full[DOS_PATHLENGTH];
	char buffer[CROSS_LEN];
	args = ExpandDot(args,buffer);
	StripSpaces(args);
	if (!DOS_Canonicalize(args,full)) { WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));return; }
	bool res=DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME);
	if (!res) {
		WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),args);
		dos.dta(save_dta);
		return;
	}
	//end can't be 0, but if it is we'll get a nice crash, who cares :)
	char * end=strrchr(full,'\\')+1;*end=0;
	char name[DOS_NAMELENGTH_ASCII];Bit32u size;Bit16u time,date;Bit8u attr;
	DOS_DTA dta(dos.dta());
	while (res) {
		dta.GetResult(name,size,date,time,attr);	
		if (!(attr & (DOS_ATTR_DIRECTORY|DOS_ATTR_READ_ONLY))) {
			strcpy(end,name);
			if (!DOS_UnlinkFile(full)) WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),full);
		}
		res=DOS_FindNext();
	}
	dos.dta(save_dta);
}

void DOS_Shell::CMD_HELP(char * args){
	HELP("HELP");
	bool optall=ScanCMDBool(args,"ALL");
	/* Print the help */
	if(!optall) WriteOut(MSG_Get("SHELL_CMD_HELP"));
	Bit32u cmd_index=0,write_count=0;
	while (cmd_list[cmd_index].name) {
		if (optall || !cmd_list[cmd_index].flags) {
			WriteOut("<\033[34;1m%-8s\033[0m> %s",cmd_list[cmd_index].name,MSG_Get(cmd_list[cmd_index].help));
			if(!(++write_count%22)) CMD_PAUSE(empty_string);
		}
		cmd_index++;
	}
}

void DOS_Shell::CMD_RENAME(char * args){
	HELP("RENAME");
	StripSpaces(args);
	if(!*args) {SyntaxError();return;}
	if((strchr(args,'*')!=NULL) || (strchr(args,'?')!=NULL) ) { WriteOut(MSG_Get("SHELL_CMD_NO_WILD"));return;}
	char * arg1=StripWord(args);
	char* slash = strrchr(arg1,'\\');
	if(slash) { 
		slash++;
		/* If directory specified (crystal caves installer)
		 * rename from c:\X : rename c:\abc.exe abc.shr. 
		 * File must appear in C:\ */ 
		
		char dir_source[DOS_PATHLENGTH]={0};
		//Copy first and then modify, makes GCC happy
		strcpy(dir_source,arg1);
		char* dummy = strrchr(dir_source,'\\');
		*dummy=0;

		if((strlen(dir_source) == 2) && (dir_source[1] == ':')) 
			strcat(dir_source,"\\"); //X: add slash

		char dir_current[DOS_PATHLENGTH + 1];
		dir_current[0] = '\\'; //Absolute addressing so we can return properly
		DOS_GetCurrentDir(0,dir_current + 1);
		if(!DOS_ChangeDir(dir_source)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			return;
		}
		DOS_Rename(slash,args);
		DOS_ChangeDir(dir_current);
	} else {
		DOS_Rename(arg1,args);
	}
}

void DOS_Shell::CMD_ECHO(char * args){
	if (!*args) {
		if (echo) { WriteOut(MSG_Get("SHELL_CMD_ECHO_ON"));}
		else { WriteOut(MSG_Get("SHELL_CMD_ECHO_OFF"));}
		return;
	}
	char buffer[512];
	char* pbuffer = buffer;
	safe_strncpy(buffer,args,512);
	StripSpaces(pbuffer);
	if (strcasecmp(pbuffer,"OFF")==0) {
		echo=false;		
		return;
	}
	if (strcasecmp(pbuffer,"ON")==0) {
		echo=true;		
		return;
	}
	if(strcasecmp(pbuffer,"/?")==0) { HELP("ECHO"); }

	args++;//skip first character. either a slash or dot or space
	size_t len = strlen(args); //TODO check input of else ook nodig is.
	if(len && args[len - 1] == '\r') {
		LOG(LOG_MISC,LOG_WARN)("Hu ? carriage return already present. Is this possible?");
		WriteOut("%s\n",args);
	} else WriteOut("%s\r\n",args);
}


void DOS_Shell::CMD_EXIT(char * args) {
	HELP("EXIT");
	exit = true;
}

void DOS_Shell::CMD_CHDIR(char * args) {
	HELP("CHDIR");
	StripSpaces(args);
	Bit8u drive = DOS_GetDefaultDrive()+'A';
	char dir[DOS_PATHLENGTH];
	if (!*args) {
		DOS_GetCurrentDir(0,dir);
		WriteOut("%c:\\%s\n",drive,dir);
	} else if(strlen(args) == 2 && args[1]==':') {
		Bit8u targetdrive = (args[0] | 0x20)-'a' + 1;
		unsigned char targetdisplay = *reinterpret_cast<unsigned char*>(&args[0]);
		if(!DOS_GetCurrentDir(targetdrive,dir)) {
			if(drive == 'Z') {
				WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(targetdisplay));
			} else {
				WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			}
			return;
		}
		WriteOut("%c:\\%s\n",toupper(targetdisplay),dir);
		if(drive == 'Z')
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT"),toupper(targetdisplay));
	} else 	if (!DOS_ChangeDir(args)) {
		/* Changedir failed. Check if the filename is longer then 8 and/or contains spaces */
	   
		std::string temps(args),slashpart;
		std::string::size_type separator = temps.find_first_of("\\/");
		if(!separator) {
			slashpart = temps.substr(0,1);
			temps.erase(0,1);
		}
		separator = temps.find_first_of("\\/");
		if(separator != std::string::npos) temps.erase(separator);
		separator = temps.rfind('.');
		if(separator != std::string::npos) temps.erase(separator);
		separator = temps.find(' ');
		if(separator != std::string::npos) {/* Contains spaces */
			temps.erase(separator);
			if(temps.size() >6) temps.erase(6);
			temps += "~1";
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT_2"),temps.insert(0,slashpart).c_str());
		} else if (temps.size()>8) {
			temps.erase(6);
			temps += "~1";
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT_2"),temps.insert(0,slashpart).c_str());
		} else {
			if (drive == 'Z') {
				WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT_3"));
			} else {
				WriteOut(MSG_Get("SHELL_CMD_CHDIR_ERROR"),args);
			}
		}
	}
}

void DOS_Shell::CMD_MKDIR(char * args) {
	HELP("MKDIR");
	StripSpaces(args);
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!DOS_MakeDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_MKDIR_ERROR"),args);
	}
}

void DOS_Shell::CMD_RMDIR(char * args) {
	HELP("RMDIR");
	// ignore /s,and /q switches for compatibility
	ScanCMDBool(args,"S");
	ScanCMDBool(args,"Q");
	StripSpaces(args);
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!DOS_RemoveDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_RMDIR_ERROR"),args);
	}
}

static void FormatNumber(Bit32u num,char * buf) {
	Bit32u numm,numk,numb,numg;
	numb=num % 1000;
	num/=1000;
	numk=num % 1000;
	num/=1000;
	numm=num % 1000;
	num/=1000;
	numg=num;
	if (numg) {
		sprintf(buf,"%d,%03d,%03d,%03d",numg,numm,numk,numb);
		return;
	};
	if (numm) {
		sprintf(buf,"%d,%03d,%03d",numm,numk,numb);
		return;
	};
	if (numk) {
		sprintf(buf,"%d,%03d",numk,numb);
		return;
	};
	sprintf(buf,"%d",numb);
}	

void DOS_Shell::CMD_DIR(char * args) {
	HELP("DIR");
	char numformat[16];
	char path[DOS_PATHLENGTH];

	std::string line;
	if(GetEnvStr("DIRCMD",line)){
		std::string::size_type idx = line.find('=');
		std::string value=line.substr(idx +1 , std::string::npos);
		line = std::string(args) + " " + value;
		args=const_cast<char*>(line.c_str());
	}
   
	bool optW=ScanCMDBool(args,"W");
	ScanCMDBool(args,"S");
	bool optP=ScanCMDBool(args,"P");
	if (ScanCMDBool(args,"WP") || ScanCMDBool(args,"PW")) {
		optW=optP=true;
	}
	bool optB=ScanCMDBool(args,"B");
	bool optAD=ScanCMDBool(args,"AD");
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	Bit32u byte_count,file_count,dir_count;
	Bitu w_count=0;
	Bitu p_count=0;
	Bitu w_size = optW?5:1;
	byte_count=file_count=dir_count=0;

	char buffer[CROSS_LEN];
	args = trim(args);
	size_t argLen = strlen(args);
	if (argLen == 0) {
		strcpy(args,"*.*"); //no arguments.
	} else {
		switch (args[argLen-1])
		{
		case '\\':	// handle \, C:\, etc.
		case ':' :	// handle C:, etc.
			strcat(args,"*.*");
			break;
		default:
			break;
		}
	}
	args = ExpandDot(args,buffer);

	bool con_temp = false, null_temp = false;

	if (!strcasecmp(args,"con"))
		con_temp=true;
	else if (!strcasecmp(args,"nul"))
		null_temp=true;

	if (DOS_FindDevice(args) != DOS_DEVICES) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
		return;
	}
	if (!strrchr(args,'*') && !strrchr(args,'?')) {
		Bit16u attribute=0;
		if(DOS_GetFileAttr(args,&attribute) && (attribute&DOS_ATTR_DIRECTORY) ) {
			strcat(args,"\\*.*");	// if no wildcard and a directory, get its files
		}
	}
	if (!strrchr(args,'.')) {
		strcat(args,".*");	// if no extension, get them all
	}

	/* Make a full path in the args */
	if (!DOS_Canonicalize(args,path)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}
	*(strrchr(path,'\\')+1)=0;
	if (!optB) {
		CMD_VOL(empty_string);
		WriteOut(MSG_Get("SHELL_CMD_DIR_INTRO"),path);
	}

	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	bool ret=DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME);
	if (!ret) {
		if (!optB) WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
		dos.dta(save_dta);
		return;
	}
 
	if(con_temp) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),"con.*");
		return;
	} else if (null_temp) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),"nul.*");
		return;
	}

	do {    /* File name and extension */
		char name[DOS_NAMELENGTH_ASCII];Bit32u size;Bit16u date;Bit16u time;Bit8u attr;
		dta.GetResult(name,size,date,time,attr);

		/* Skip non-directories if option AD is present */
		if(optAD && !(attr&DOS_ATTR_DIRECTORY) ) continue;
		
		/* output the file */
		if (optB) {
			// this overrides pretty much everything
			if (strcmp(".",name) && strcmp("..",name)) {
				WriteOut("%s\n",name);
			}
		} else {
			char * ext = empty_string;
			if (!optW && (name[0] != '.')) {
				ext = strrchr(name, '.');
				if (!ext) ext = empty_string;
				else *ext++ = 0;
			}
			Bit8u day	= (Bit8u)(date & 0x001f);
			Bit8u month	= (Bit8u)((date >> 5) & 0x000f);
			Bit16u year = (Bit16u)((date >> 9) + 1980);
			Bit8u hour	= (Bit8u)((time >> 5 ) >> 6);
			Bit8u minute = (Bit8u)((time >> 5) & 0x003f);

			if (attr & DOS_ATTR_DIRECTORY) {
				if (optW) {
					WriteOut("[%s]",name);
					size_t namelen = strlen(name);
					if (namelen <= 14) {
						for (size_t i=14-namelen;i>0;i--) WriteOut(" ");
					}
				} else {
					WriteOut("%-8s %-3s   %-16s %02d-%02d-%04d %2d:%02d\n",name,ext,"<DIR>",day,month,year,hour,minute);
				}
				dir_count++;
			} else {
				if (optW) {
					WriteOut("%-16s",name);
				} else {
					FormatNumber(size,numformat);
					WriteOut("%-8s %-3s   %16s %02d-%02d-%04d %2d:%02d\n",name,ext,numformat,day,month,year,hour,minute);
				}
				file_count++;
				byte_count+=size;
			}
			if (optW) {
				w_count++;
			}
		}
		if (optP && !(++p_count%(22*w_size))) {
			CMD_PAUSE(empty_string);
		}
	} while ( (ret=DOS_FindNext()) );
	if (optW) {
		if (w_count%5)	WriteOut("\n");
	}
	if (!optB) {
		/* Show the summary of results */
		FormatNumber(byte_count,numformat);
		WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_USED"),file_count,numformat);
		Bit8u drive=dta.GetSearchDrive();
		//TODO Free Space
		Bitu free_space=1024*1024*100;
		if (Drives[drive]) {
			Bit16u bytes_sector;Bit8u sectors_cluster;Bit16u total_clusters;Bit16u free_clusters;
			Drives[drive]->AllocationInfo(&bytes_sector,&sectors_cluster,&total_clusters,&free_clusters);
			free_space=bytes_sector*sectors_cluster*free_clusters;
		}
		FormatNumber(free_space,numformat);
		WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_FREE"),dir_count,numformat);
	}
	dos.dta(save_dta);
}

struct copysource {
	std::string filename;
	bool concat;
	copysource(std::string filein,bool concatin):
		filename(filein),concat(concatin){ };
	copysource():filename(""),concat(false){ };
};


void DOS_Shell::CMD_COPY(char * args) {
	extern Bitu ZDRIVE_NUM;
	const char root[4] = {(char)('A'+ZDRIVE_NUM),':','\\',0};
	char cmd[20];
	strcpy(cmd,root);
	strcat(cmd,"COPY.EXE");
	if (DOS_FindFirst(cmd,0xffff & ~DOS_ATTR_VOLUME)) {
		StripSpaces(args);
		while(ScanCMDBool(args,"T")) ; //Shouldn't this be A ?
		ScanCMDBool(args,"Y");
		ScanCMDBool(args,"-Y");
		Execute(cmd,args);
		return;
	}

	HELP("COPY");
	static char defaulttarget[] = ".";
	StripSpaces(args);
	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	Bit32u size;Bit16u date;Bit16u time;Bit8u attr;
	char name[DOS_NAMELENGTH_ASCII];
	std::vector<copysource> sources;
	// ignore /b and /t switches: always copy binary
	while(ScanCMDBool(args,"B")) ;
	while(ScanCMDBool(args,"T")) ; //Shouldn't this be A ?
	while(ScanCMDBool(args,"A")) ;
	ScanCMDBool(args,"Y");
	ScanCMDBool(args,"-Y");
	ScanCMDBool(args,"V");

	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		dos.dta(save_dta);
		return;
	}
	// Gather all sources (extension to copy more then 1 file specified at commandline)
	// Concatating files go as follows: All parts except for the last bear the concat flag.
	// This construction allows them to be counted (only the non concat set)
	char* source_p = NULL;
	char source_x[DOS_PATHLENGTH+CROSS_LEN];
	while ( (source_p = StripWord(args)) && *source_p ) {
		do {
			char* plus = strchr(source_p,'+');
			// If StripWord() previously cut at a space before a plus then
			// set concatenate flag on last source and remove leading plus.
			if (plus == source_p && sources.size()) {
				sources[sources.size()-1].concat = true;
				// If spaces also followed plus then item is only a plus.
				if (strlen(++source_p)==0) break;
				plus = strchr(source_p,'+');
			}
			if (plus) *plus++ = 0;
			safe_strncpy(source_x,source_p,CROSS_LEN);
			bool has_drive_spec = false;
			size_t source_x_len = strlen(source_x);
			if (source_x_len>0) {
				if (source_x[source_x_len-1]==':') has_drive_spec = true;
			}
			if (!has_drive_spec  && !strpbrk(source_p,"*?") ) { //doubt that fu*\*.* is valid
				if (DOS_FindFirst(source_p,0xffff & ~DOS_ATTR_VOLUME)) {
					dta.GetResult(name,size,date,time,attr);
					if (attr & DOS_ATTR_DIRECTORY)
						strcat(source_x,"\\*.*");
				}
			}
			sources.push_back(copysource(source_x,(plus)?true:false));
			source_p = plus;
		} while(source_p && *source_p);
	}
	// At least one source has to be there
	if (!sources.size() || !sources[0].filename.size()) {
		WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
		dos.dta(save_dta);
		return;
	};

	copysource target;
	// If more then one object exists and last target is not part of a 
	// concat sequence then make it the target.
	if(sources.size()>1 && !sources[sources.size()-2].concat){
		target = sources.back();
		sources.pop_back();
	}
	//If no target => default target with concat flag true to detect a+b+c
	if(target.filename.size() == 0) target = copysource(defaulttarget,true);

	copysource oldsource;
	copysource source;
	Bit32u count = 0;
	while(sources.size()) {
		/* Get next source item and keep track of old source for concat start end */
		oldsource = source;
		source = sources[0];
		sources.erase(sources.begin());

		//Skip first file if doing a+b+c. Set target to first file
		if(!oldsource.concat && source.concat && target.concat) {
			target = source;
			continue;
		}

		/* Make a full path in the args */
		char pathSource[DOS_PATHLENGTH];
		char pathTarget[DOS_PATHLENGTH];

		if (!DOS_Canonicalize(const_cast<char*>(source.filename.c_str()),pathSource)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			dos.dta(save_dta);
			return;
		}
		// cut search pattern
		char* pos = strrchr(pathSource,'\\');
		if (pos) *(pos+1) = 0;

		if (!DOS_Canonicalize(const_cast<char*>(target.filename.c_str()),pathTarget)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			dos.dta(save_dta);
			return;
		}
		char* temp = strstr(pathTarget,"*.*");
		if(temp && (temp == pathTarget || temp[-1] == '\\')) *temp = 0;//strip off *.* from target
	
		// add '\\' if target is a directoy
		if (pathTarget[strlen(pathTarget)-1]!='\\') {
			if (DOS_FindFirst(pathTarget,0xffff & ~DOS_ATTR_VOLUME)) {
				dta.GetResult(name,size,date,time,attr);
				if (attr & DOS_ATTR_DIRECTORY)	
					strcat(pathTarget,"\\");
			}
		};

		//Find first sourcefile
		bool ret = DOS_FindFirst(const_cast<char*>(source.filename.c_str()),0xffff & ~DOS_ATTR_VOLUME);
		if (!ret) {
			WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),const_cast<char*>(source.filename.c_str()));
			dos.dta(save_dta);
			return;
		}

		Bit16u sourceHandle,targetHandle;
		char nameTarget[DOS_PATHLENGTH];
		char nameSource[DOS_PATHLENGTH];
		
		// Cache so we don't have to recalculate
		size_t pathTargetLen = strlen(pathTarget);
		
		// See if we have to substitute filename or extension
		char *ext = 0;
		size_t replacementOffset = 0;
		if (pathTarget[pathTargetLen-1]!='\\') { 
				// only if it's not a directory
			ext = strchr(pathTarget, '.');
			if (ext > pathTarget) { // no possible substitution
				if (ext[-1] == '*') {
					// we substitute extension, save it, hide the name
					ext[-1] = 0;
					assert(ext > pathTarget + 1); // pathTarget is fully qualified
					if (ext[-2] != '\\') {
						// there is something before the asterisk
						// save the offset in the source names
						replacementOffset = source.filename.find('*');
						size_t lastSlash = source.filename.rfind('\\');
						if (std::string::npos == lastSlash)
							lastSlash = 0;
						else
							lastSlash++;
						if (std::string::npos == replacementOffset
							  || replacementOffset < lastSlash) {
							// no asterisk found or in wrong place, error
							WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
							dos.dta(save_dta);
							return;
						}
						replacementOffset -= lastSlash;
//						WriteOut("[II] replacement offset is %d\n", replacementOffset);
					}
				}
				if (ext[1] == '*') {
					// we substitute name, clear the extension
					*ext = 0;
				} else if (ext[-1]) {
					// we don't substitute anything, clear up
					ext = 0;
				}
			}
		}

		while (ret) {
			dta.GetResult(name,size,date,time,attr);

			if ((attr & DOS_ATTR_DIRECTORY)==0) {
				strcpy(nameSource,pathSource);
				strcat(nameSource,name);
				// Open Source
				if (DOS_OpenFile(nameSource,0,&sourceHandle)) {
					// Create Target or open it if in concat mode
					strcpy(nameTarget,pathTarget);
					
					if (ext) { // substitute parts if necessary
						if (!ext[-1]) { // substitute extension
							strcat(nameTarget, name + replacementOffset);
							strcpy(strchr(nameTarget, '.'), ext);
						}
						if (ext[1] == '*') { // substitute name (so just add the extension)
							strcat(nameTarget, strchr(name, '.'));
						}
					}
					
					if (nameTarget[pathTargetLen-1]=='\\') strcat(nameTarget,name);
					//Don't create a newfile when in concat mode
					if (oldsource.concat || DOS_CreateFile(nameTarget,0,&targetHandle)) {
						Bit32u dummy=0;
						//In concat mode. Open the target and seek to the eof
						if (!oldsource.concat || (DOS_OpenFile(nameTarget,OPEN_READWRITE,&targetHandle) && 
					        	                  DOS_SeekFile(targetHandle,&dummy,DOS_SEEK_END))) {
							// Copy 
							static Bit8u buffer[0x8000]; // static, otherwise stack overflow possible.
							bool	failed = false;
							Bit16u	toread = 0x8000;
							do {
								failed |= DOS_ReadFile(sourceHandle,buffer,&toread);
								failed |= DOS_WriteFile(targetHandle,buffer,&toread);
							} while (toread==0x8000);
							if (!oldsource.concat) { // credits to FeedingDragon
								DOS_GetFileDate(sourceHandle,&time,&date);
								DOS_SetFileDate(targetHandle,time,date);
							}
							failed |= DOS_CloseFile(sourceHandle);
							failed |= DOS_CloseFile(targetHandle);
							WriteOut(" %s\n",name);
							if(!source.concat) count++; //Only count concat files once
						} else {
							DOS_CloseFile(sourceHandle);
							WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(target.filename.c_str()));
						}
					} else {
						DOS_CloseFile(sourceHandle);
						WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(target.filename.c_str()));
					}
				} else WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(source.filename.c_str()));
			};
			//On to the next file if the previous one wasn't a device
			if ((attr&DOS_ATTR_DEVICE) == 0) ret = DOS_FindNext();
			else ret = false;
		};
	}

	WriteOut(MSG_Get("SHELL_CMD_COPY_SUCCESS"),count);
	dos.dta(save_dta);
}

/* NTS: WARNING, this function modifies the buffer pointed to by char *args */
void DOS_Shell::CMD_SET(char * args) {
	std::string line;

	HELP("SET");
	StripSpaces(args);

	if (*args == 0) { /* "SET" by itself means to show the environment block */
		Bitu count = GetEnvCount();

		for (Bitu a = 0;a < count;a++) {
			if (GetEnvNum(a,line))
				WriteOut("%s\n",line.c_str());			
		}

	}
	else {
		char *p;

		{ /* parse arguments at the start */
			char *pcheck = args;

			while (*pcheck != 0 && (*pcheck == ' ' || *pcheck == '\t')) pcheck++;
			if (*pcheck != 0 && strlen(pcheck) > 3 && (strncasecmp(pcheck,"/p ",3) == 0))
				E_Exit("Set /P is not supported. Use Choice!"); /* TODO: What is SET /P supposed to do? */
		}

		/* Most SET commands take the form NAME=VALUE */
		p = strchr(args,'=');
		if (p == NULL) {
			/* SET <variable> without assignment prints the variable instead */
			if (!GetEnvStr(args,line)) WriteOut(MSG_Get("SHELL_CMD_SET_NOT_SET"),args);
			WriteOut("%s\n",line.c_str());
		} else {
			/* ASCIIZ snip the args string in two, so that args is C-string name of the variable,
			 * and "p" is C-string value of the variable */
			*p++ = 0;

			/* No parsing is needed. The command interpreter does the variable substitution for us */
			if (!SetEnv(args,p)) {
				/* NTS: If Win95 is any example, the command interpreter expands the variables for us */
				WriteOut(MSG_Get("SHELL_CMD_SET_OUT_OF_SPACE"));
			}
		}
	}
}

void DOS_Shell::CMD_IF(char * args) {
	HELP("IF");
	StripSpaces(args,'=');
	bool has_not=false;

	while (strncasecmp(args,"NOT",3) == 0) {
		if (!isspace(*reinterpret_cast<unsigned char*>(&args[3])) && (args[3] != '=')) break;
		args += 3;	//skip text
		//skip more spaces
		StripSpaces(args,'=');
		has_not = !has_not;
	}

	if(strncasecmp(args,"ERRORLEVEL",10) == 0) {
		args += 10;	//skip text
		//Strip spaces and ==
		StripSpaces(args,'=');
		char* word = StripWord(args);
		if(!isdigit(*word)) {
			WriteOut(MSG_Get("SHELL_CMD_IF_ERRORLEVEL_MISSING_NUMBER"));
			return;
		}

		Bit8u n = 0;
		do n = n * 10 + (*word - '0');
		while (isdigit(*++word));
		if(*word && !isspace(*word)) {
			WriteOut(MSG_Get("SHELL_CMD_IF_ERRORLEVEL_INVALID_NUMBER"));
			return;
		}
		/* Read the error code from DOS */
		if ((dos.return_code>=n) ==(!has_not)) DoCommand(args);
		return;
	}

	if(strncasecmp(args,"EXIST ",6) == 0) {
		args += 6; //Skip text
		StripSpaces(args);
		char* word = StripWord(args);
		if (!*word) {
			WriteOut(MSG_Get("SHELL_CMD_IF_EXIST_MISSING_FILENAME"));
			return;
		}

		{	/* DOS_FindFirst uses dta so set it to our internal dta */
			RealPt save_dta=dos.dta();
			dos.dta(dos.tables.tempdta);
			bool ret=DOS_FindFirst(word,0xffff & ~DOS_ATTR_VOLUME);
			dos.dta(save_dta);
			if (ret==(!has_not)) DoCommand(args);
		}
		return;
	}

	/* Normal if string compare */

	char* word1 = args;
	// first word is until space or =
	while (*args && !isspace(*reinterpret_cast<unsigned char*>(args)) && (*args != '='))
		args++;
	char* end_word1 = args;

	// scan for =
	while (*args && (*args != '='))
		args++;
	// check for ==
	if ((*args==0) || (args[1] != '=')) {
		SyntaxError();
		return;
	}
	args += 2;
	StripSpaces(args,'=');

	char* word2 = args;
	// second word is until space or =
	while (*args && !isspace(*reinterpret_cast<unsigned char*>(args)) && (*args != '='))
		args++;

	if (*args) {
		*end_word1 = 0;		// mark end of first word
		*args++ = 0;		// mark end of second word
		StripSpaces(args,'=');

		if ((strcmp(word1,word2)==0)==(!has_not)) DoCommand(args);
	}
}

void DOS_Shell::CMD_GOTO(char * args) {
	HELP("GOTO");
	StripSpaces(args);
	if (!bf) return;
	if (*args &&(*args==':')) args++;
	//label ends at the first space
	char* non_space = args;
	while (*non_space) {
		if((*non_space == ' ') || (*non_space == '\t')) 
			*non_space = 0; 
		else non_space++;
	}
	if (!*args) {
		WriteOut(MSG_Get("SHELL_CMD_GOTO_MISSING_LABEL"));
		return;
	}
	if (!bf->Goto(args)) {
		WriteOut(MSG_Get("SHELL_CMD_GOTO_LABEL_NOT_FOUND"),args);
		return;
	}
}

void DOS_Shell::CMD_SHIFT(char * args ) {
	HELP("SHIFT");
	if(bf) bf->Shift();
}

void DOS_Shell::CMD_TYPE(char * args) {
	HELP("TYPE");
	bool optP=ScanCMDBool(args,"P");
	// ignore /p /h and /t for compatibility
	ScanCMDBool(args,"P");
	ScanCMDBool(args,"H");
	ScanCMDBool(args,"T");
	StripSpaces(args);
	if (strcasecmp(args,"nul")==0) return;
	if (!*args) {
		WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
		return;
	}
	Bit16u handle;
	char * word;
nextfile:
	word=StripWord(args);
	if (!DOS_OpenFile(word,0,&handle)) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),word);
		return;
	}
	Bit8u c;Bit16u n=1;
	while (n) {
		DOS_ReadFile(handle,&c,&n);
		if (n==0 || c==0x1a) break; // stop at EOF
		DOS_WriteFile(STDOUT,&c,&n);
	}
	DOS_CloseFile(handle);
	if (*args) goto nextfile;
}

void DOS_Shell::CMD_REM(char * args) {
	HELP("REM");
}

/*static void PAUSED(void) {
	Bit8u c; Bit16u n=1;
	DOS_ReadFile (STDIN,&c,&n);
}

void DOS_Shell::CMD_MORE(char * args) {
	HELP("MORE");
	//ScanCMDBool(args,">");
	if(!*args) {
		//char defaultcon[DOS_PATHLENGTH+CROSS_LEN+20]={ 0 };
		//strcpy(defaultcon,"copy con >nul");
		//this->ParseLine(defaultcon);
		return;
	}
	int nchars = 0, nlines = 0, linecount = 0, LINES = (Bit16u)mem_readb(BIOS_ROWS_ON_SCREEN_MINUS_1)-3, COLS = mem_readw(BIOS_SCREEN_COLUMNS), TABSIZE = 8;
	StripSpaces(args);
	if (strcasecmp(args,"nul")==0) return;
	if (!*args) {
		WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
		return;
	}
	Bit16u handle;
	char * word;
nextfile:
	word=StripWord(args);
	if (!DOS_OpenFile(word,0,&handle)) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),word);
		return;
	}
	Bit16u n; Bit8u c;
	do {
		n=1;
		DOS_ReadFile(handle,&c,&n);
		DOS_WriteFile(STDOUT,&c,&n);
		if (c != '\t') nchars++;
		else do {
			WriteOut(" ");
			nchars++;
		} while ( nchars < COLS && nchars % TABSIZE );

		if (c == '\n') linecount++;
		if ((c == '\n') || (nchars >= COLS)) {
			nlines++;
			nchars = 0;
			if (nlines == (LINES-1)) {
				WriteOut("\n-- More -- %s (%u) --\n",word,linecount);
				PAUSED();
				nlines=0;
			}
		}
	} while (n);
	DOS_CloseFile(handle);
	if (*args) {
		WriteOut("\n");
		PAUSED();
		goto nextfile;
	}
}
*/
void DOS_Shell::CMD_PAUSE(char * args){
	HELP("PAUSE");
	if(args && *args) {
		args++;
		WriteOut("%s\n",args);	// optional specified message
	} else
	WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
	Bit8u c;Bit16u n=1;
	DOS_ReadFile(STDIN,&c,&n);
	if (c==0) DOS_ReadFile(STDIN,&c,&n); // read extended key
}

void DOS_Shell::CMD_CALL(char * args){
	HELP("CALL");
	this->call=true; /* else the old batchfile will be closed first */
	this->ParseLine(args);
	this->call=false;
}

extern bool date_host_forced;
void DOS_Shell::CMD_DATE(char * args) {
	HELP("DATE");	
	if(ScanCMDBool(args,"H")) {
		// synchronize date with host parameter
		time_t curtime;
		struct tm *loctime;
		curtime = time (NULL);
		loctime = localtime (&curtime);
		
		reg_cx = loctime->tm_year+1900;
		reg_dh = loctime->tm_mon+1;
		reg_dl = loctime->tm_mday;

		reg_ah=0x2b; // set system date
		CALLBACK_RunRealInt(0x21);
		return;
	}
	// check if a date was passed in command line
	Bit32u newday,newmonth,newyear;
	if(sscanf(args,"%u-%u-%u",&newmonth,&newday,&newyear)==3) {
		reg_cx = static_cast<Bit16u>(newyear);
		reg_dh = static_cast<Bit8u>(newmonth);
		reg_dl = static_cast<Bit8u>(newday);

		reg_ah=0x2b; // set system date
		CALLBACK_RunRealInt(0x21);
		if(reg_al==0xff) WriteOut(MSG_Get("SHELL_CMD_DATE_ERROR"));
		return;
	}
	// display the current date
	reg_ah=0x2a; // get system date
	CALLBACK_RunRealInt(0x21);

	const char* datestring = MSG_Get("SHELL_CMD_DATE_DAYS");
	Bit32u length;
	char day[6] = {0};
	if(sscanf(datestring,"%u",&length) && (length<5) && (strlen(datestring)==(length*7+1))) {
		// date string appears valid
		for(Bit32u i = 0; i < length; i++) day[i] = datestring[reg_al*length+1+i];
	}
	bool dateonly = ScanCMDBool(args,"T");
	if(!dateonly) WriteOut(MSG_Get("SHELL_CMD_DATE_NOW"));

	const char* formatstring = MSG_Get("SHELL_CMD_DATE_FORMAT");
	if(strlen(formatstring)!=5) return;
	char buffer[15] = {0};
	Bitu bufferptr=0;
	for(Bitu i = 0; i < 5; i++) {
		if(i==1 || i==3) {
			buffer[bufferptr] = formatstring[i];
			bufferptr++;
		} else {
			if(formatstring[i]=='M') bufferptr += sprintf(buffer+bufferptr,"%02u",(Bit8u) reg_dh);
			if(formatstring[i]=='D') bufferptr += sprintf(buffer+bufferptr,"%02u",(Bit8u) reg_dl);
			if(formatstring[i]=='Y') bufferptr += sprintf(buffer+bufferptr,"%04u",(Bit16u) reg_cx);
		}
	}
	if(date_host_forced) {
		time_t curtime;

		struct tm *loctime;
		curtime = time (NULL);

		loctime = localtime (&curtime);
		int hosty=loctime->tm_year+1900;
		int hostm=loctime->tm_mon+1;
		int hostd=loctime->tm_mday;
		if (hostm == 1 || hostm == 2) hosty--;
		hostm = (hostm + 9) % 12 + 1;
		int y = hosty % 100;
		int century = hosty / 100;
		int week = ((13 * hostm - 1) / 5 + hostd + y + y/4 + century/4 - 2*century) % 7;
		if (week < 0) week = (week + 7) % 7;

		const char* my_week[7]={"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
		WriteOut("%s %s\n",my_week[week],buffer);
	} else
	WriteOut("%s %s\n",day, buffer);
	if(!dateonly) WriteOut(MSG_Get("SHELL_CMD_DATE_SETHLP"));
};

void DOS_Shell::CMD_TIME(char * args) {
	HELP("TIME");
	if(ScanCMDBool(args,"H")) {
		// synchronize time with host parameter
		time_t curtime;
		struct tm *loctime;
		curtime = time (NULL);
		loctime = localtime (&curtime);
		
		//reg_cx = loctime->;
		//reg_dh = loctime->;
		//reg_dl = loctime->;

		// reg_ah=0x2d; // set system time TODO
		// CALLBACK_RunRealInt(0x21);
		
		Bit32u ticks=(Bit32u)(((double)(loctime->tm_hour*3600+
										loctime->tm_min*60+
										loctime->tm_sec))*18.206481481);
		mem_writed(BIOS_TIMER,ticks);
		return;
	}
	bool timeonly = ScanCMDBool(args,"T");

	reg_ah=0x2c; // get system time
	CALLBACK_RunRealInt(0x21);
/*
		reg_dl= // 1/100 seconds
		reg_dh= // seconds
		reg_cl= // minutes
		reg_ch= // hours
*/
	if(timeonly) {
		WriteOut("%2u:%02u\n",reg_ch,reg_cl);
	} else {
		WriteOut(MSG_Get("SHELL_CMD_TIME_NOW"));
		WriteOut("%2u:%02u:%02u,%02u\n",reg_ch,reg_cl,reg_dh,reg_dl);
	}
};

void DOS_Shell::CMD_SUBST (char * args) {
/* If more that one type can be substed think of something else 
 * E.g. make basedir member dos_drive instead of localdrive
 */
	HELP("SUBST");
	localDrive* ldp=0;
	char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
	char temp_str[2] = { 0,0 };
	try {
		strcpy(mountstring,"MOUNT ");
		StripSpaces(args);
		std::string arg;
		CommandLine command(0,args);

		if (command.GetCount() != 2) throw 0 ;
  
		command.FindCommand(1,arg);
		if( (arg.size()>1) && arg[1] !=':')  throw(0);
		temp_str[0]=(char)toupper(args[0]);
		command.FindCommand(2,arg);
		if((arg=="/D") || (arg=="/d")) {
			if(!Drives[temp_str[0]-'A'] ) throw 1; //targetdrive not in use
			strcat(mountstring,"-u ");
			strcat(mountstring,temp_str);
			this->ParseLine(mountstring);
			return;
		}
		if(Drives[temp_str[0]-'A'] ) throw 0; //targetdrive in use
		strcat(mountstring,temp_str);
		strcat(mountstring," ");

   		Bit8u drive;char fulldir[DOS_PATHLENGTH];
		if (!DOS_MakeName(const_cast<char*>(arg.c_str()),fulldir,&drive)) throw 0;
	
		if( ( ldp=dynamic_cast<localDrive*>(Drives[drive])) == 0 ) throw 0;
		char newname[CROSS_LEN];   
		strcpy(newname, ldp->basedir);	   
		strcat(newname,fulldir);
		CROSS_FILENAME(newname);
		ldp->dirCache.ExpandName(newname);
		strcat(mountstring,"\"");	   
		strcat(mountstring, newname);
		strcat(mountstring,"\"");	   
		this->ParseLine(mountstring);
	}
	catch(int a){
		if(a == 0) {
			WriteOut(MSG_Get("SHELL_CMD_SUBST_FAILURE"));
		} else {
		       	WriteOut(MSG_Get("SHELL_CMD_SUBST_NO_REMOVE"));
		}
		return;
	}
	catch(...) {		//dynamic cast failed =>so no localdrive
		WriteOut(MSG_Get("SHELL_CMD_SUBST_FAILURE"));
		return;
	}
   
	return;
}

void DOS_Shell::CMD_LOADHIGH(char *args){
	HELP("LOADHIGH");
	Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
	Bit8u umb_flag=dos_infoblock.GetUMBChainState();
	Bit8u old_memstrat=(Bit8u)(DOS_GetMemAllocStrategy()&0xff);
	if (umb_start==0x9fff) {
		if ((umb_flag&1)==0) DOS_LinkUMBsToMemChain(1);
		DOS_SetMemAllocStrategy(0x80);	// search in UMBs first
		this->ParseLine(args);
		Bit8u current_umb_flag=dos_infoblock.GetUMBChainState();
		if ((current_umb_flag&1)!=(umb_flag&1)) DOS_LinkUMBsToMemChain(umb_flag);
		DOS_SetMemAllocStrategy(old_memstrat);	// restore strategy
	} else this->ParseLine(args);
}

void DOS_Shell::CMD_CHOICE(char * args){
	HELP("CHOICE");
	static char defchoice[3] = {'y','n',0};
	char *rem = NULL, *ptr;
	bool optN = ScanCMDBool(args,"N");
	bool optS = ScanCMDBool(args,"S"); //Case-sensitive matching
	// ignore /b and /m switches for compatibility
	ScanCMDBool(args,"B");
	ScanCMDBool(args,"M"); // Text
	ScanCMDBool(args,"T"); //Default Choice after timeout
	if (args) {
		char *last = strchr(args,0);
		StripSpaces(args);
		rem = ScanCMDRemain(args);
		if (rem && *rem && (tolower(rem[1]) != 'c')) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
			return;
		}
		if (args == rem) args = strchr(rem,0)+1;
		if (rem) rem += 2;
		if(rem && rem[0]==':') rem++; /* optional : after /c */
		if (args > last) args = NULL;
	}
	if (!rem || !*rem) rem = defchoice; /* No choices specified use YN */
	ptr = rem;
	Bit8u c;
	if(!optS) while ((c = *ptr)) *ptr++ = (char)toupper(c); /* When in no case-sensitive mode. make everything upcase */
	if(args && *args ) {
		StripSpaces(args);
		size_t argslen = strlen(args);
		if(argslen>1 && args[0] == '"' && args[argslen-1] =='"') {
			args[argslen-1] = 0; //Remove quotes
			args++;
		}
		WriteOut(args);
	}
	/* Show question prompt of the form [a,b]? where a b are the choice values */
	if (!optN) {
		if(args && *args) WriteOut(" ");
		WriteOut("[");
		size_t len = strlen(rem);
		for(size_t t = 1; t < len; t++) {
			WriteOut("%c,",rem[t-1]);
		}
		WriteOut("%c]?",rem[len-1]);
	}

	Bit16u n=1;
	do {
		DOS_ReadFile (STDIN,&c,&n);
	} while (!c || !(ptr = strchr(rem,(optS?c:toupper(c)))));
	c = optS?c:(Bit8u)toupper(c);
	DOS_WriteFile (STDOUT,&c, &n);
	c = '\n'; DOS_WriteFile (STDOUT,&c, &n);
	dos.return_code = (Bit8u)(ptr-rem+1);
}

void DOS_Shell::CMD_ATTRIB(char *args){
	HELP("ATTRIB");
	// No-Op for now.
}

void DOS_Shell::CMD_PROMPT(char *args){
	HELP("PROMPT");
	if(args && *args && strlen(args)) {
		args++;
		SetEnv("PROMPT",args);
	} else
		SetEnv("PROMPT","$P$G");
	return;
}

void DOS_Shell::CMD_LABEL(char *args){
	HELP("LABEL");
	Bit8u drive = DOS_GetDefaultDrive();
	if(args && *args) {
		std::string label;
		args++;
		label = args;
		Drives[drive]->SetLabel(label.c_str(),false,true);
		return;
	}
	WriteOut(MSG_Get("SHELL_CMD_LABEL_HELP")); WriteOut("\n");
	WriteOut(MSG_Get("SHELL_CMD_LABEL_HELP_LONG"));
	return;
}

void DOS_Shell::CMD_PATH(char *args){
	HELP("PATH");
	if(args && *args && strlen(args)){
		char pathstring[DOS_PATHLENGTH+CROSS_LEN+20]={ 0 };
		strcpy(pathstring,"set PATH=");
		while(args && *args && (*args=='='|| *args==' ')) 
		     args++;
		strcat(pathstring,args);
		this->ParseLine(pathstring);
		return;
	} else {
		std::string line;
		if(GetEnvStr("PATH",line)) {
			WriteOut("%s\n",line.c_str());
		} else {
			WriteOut("PATH=(null)\n");
		}
	}
}

void DOS_Shell::CMD_VER(char *args) {
	HELP("VER");
	if(args && *args) {
		char* word = StripWord(args);
		if(strcasecmp(word,"set")) return;
		word = StripWord(args);
		dos.version.major = (Bit8u)(atoi(word));
		dos.version.minor = (Bit8u)(atoi(args));
	} else WriteOut(MSG_Get("SHELL_CMD_VER_VER"),VERSION,dos.version.major,dos.version.minor);
}

void DOS_Shell::CMD_VOL(char *args){
	HELP("VOL");
	Bit8u drive=DOS_GetDefaultDrive();
	if(args && *args && strlen(args)){
		args++;
		Bit32u argLen = strlen(args);
		switch (args[argLen-1]) {
		case ':' :
			if(!strcasecmp(args,":")) return;
			int drive2; drive2= toupper(*reinterpret_cast<unsigned char*>(&args[0]));
			char * c; c = strchr(args,':'); *c = '\0';
			if (Drives[drive2-'A']) drive = drive2 - 'A';
			else {
				WriteOut(MSG_Get("SHELL_CMD_VOL_DRIVEERROR"));
				return;
			}
			break;
		default:
			return;
		}
	}
	char const* bufin = Drives[drive]->GetLabel();
	WriteOut(MSG_Get("SHELL_CMD_VOL_DRIVE"),drive+'A');

	//if((drive+'A')=='Z') bufin="DOSBOX";
	if(strcasecmp(bufin,"")==0)
		WriteOut(MSG_Get("SHELL_CMD_VOL_SERIAL_NOLABEL"));
	else
		WriteOut(MSG_Get("SHELL_CMD_VOL_SERIAL_LABEL"),bufin);

	WriteOut(MSG_Get("SHELL_CMD_VOL_SERIAL"));
	WriteOut("0000-1234\n"); // fake serial number
	return;
}

void SetVal(const std::string secname, std::string preval, const std::string val);
static void delayed_press(Bitu key) { KEYBOARD_AddKey((KBD_KEYS)key,true); }
static void delayed_release(Bitu key) { KEYBOARD_AddKey((KBD_KEYS)key,false); }
static void delayed_sdlpress(Bitu core) {
	if(core==1) SetVal("cpu","core","normal");
	else if(core==2) SetVal("cpu","core","simple");
	else if(core==3) SetVal("cpu","core","dynamic");
	else if(core==3) SetVal("cpu","core","full");
}
// ADDKEY patch was created by Moe
void DOS_Shell::CMD_ADDKEY(char * args){
	HELP("ADDKEY");
	StripSpaces(args);
	if (!*args) {
		WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
		return;
	}
	char * word;
	int delay = 0, duration = 0, core=0;

	while (*args) {
		word=StripWord(args);
		KBD_KEYS scankey = (KBD_KEYS)0;
		char *tail;
		bool alt = false, control = false, shift = false;
		while (word[1] == '-') {
			switch (word[0]) {
				case 'c':
					control = true;
					word += 2;
					break;
				case 's':
					shift = true;
					word += 2;
					break;
				case 'a':
					alt = true;
					word += 2;
					break;
				default:
					WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
					return;
			}
		}
		if (!strcasecmp(word,"enter")) {
			word[0] = 10;
			word[1] = 0;
		} else if (!strcasecmp(word,"space")) {
			word[0] = 32;
			word[1] = 0;
		} else if (!strcasecmp(word,"bs")) {
			word[0] = 8;
			word[1] = 0;
		} else if (!strcasecmp(word,"tab")) {
			word[0] = 9;
			word[1] = 0;
		} else if (!strcasecmp(word,"escape")) {
			word[0] = 27;
			word[1] = 0;
		} else if (!strcasecmp(word,"up")) {
			word[0] = 141;
			word[1] = 0;
		} else if (!strcasecmp(word,"down")) {
			word[0] = 142;
			word[1] = 0;
		} else if (!strcasecmp(word,"left")) {
			word[0] = 143;
			word[1] = 0;
		} else if (!strcasecmp(word,"right")) {
			word[0] = 144;
			word[1] = 0;
		} else if (!strcasecmp(word,"ins")) {
			word[0] = 145;
			word[1] = 0;
		} else if (!strcasecmp(word,"del")) {
			word[0] = 146;
			word[1] = 0;
		} else if (!strcasecmp(word,"home")) {
			word[0] = 147;
			word[1] = 0;
		} else if (!strcasecmp(word,"end")) {
			word[0] = 148;
			word[1] = 0;
		} else if (!strcasecmp(word,"pgup")) {
			word[0] = 149;
			word[1] = 0;
		} else if (!strcasecmp(word,"pgdown")) {
			word[0] = 150;
			word[1] = 0;
		} else if (!strcasecmp(word,"normal")) {
			core = 1;
		} else if (!strcasecmp(word,"simple")) {
			core = 2;
		} else if (!strcasecmp(word,"dynamic")) {
			core = 3;
		} else if (!strcasecmp(word,"full")) {
			core = 4;
		} else if (word[0] == 'k' && word[1] == 'p' && word[2] & !word[3]) {
			word[0] = 151+word[2]-'0';
			word[1] = 0;
		} else if (word[0] == 'f' && word[1]) {
			word[0] = 128+word[1]-'0';
			if (word[1] == '1' && word[2]) word[0] = 128+word[2]-'0'+10;
			word[1] = 0;
		}
		if (!word[1]) {
			const int shiftflag = 0x1000000;
			const int map[256] = {
				0,0,0,0,0,0,0,0,
				KBD_backspace,
				KBD_tab,
				KBD_enter,
				0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
				KBD_esc,
				0,0,0,0,
				KBD_space, KBD_1|shiftflag, KBD_quote|shiftflag, KBD_3|shiftflag, KBD_4|shiftflag, KBD_5|shiftflag, KBD_7|shiftflag, KBD_quote,
				KBD_9|shiftflag, KBD_0|shiftflag, KBD_8|shiftflag, KBD_equals|shiftflag, KBD_comma, KBD_minus, KBD_period, KBD_slash,
				KBD_0, KBD_1, KBD_2, KBD_3, KBD_4, KBD_5, KBD_6, KBD_7,
				KBD_8, KBD_9, KBD_semicolon|shiftflag, KBD_semicolon, KBD_comma|shiftflag, KBD_equals, KBD_period|shiftflag, KBD_slash|shiftflag,
				KBD_2|shiftflag, KBD_a|shiftflag, KBD_b|shiftflag, KBD_c|shiftflag, KBD_d|shiftflag, KBD_e|shiftflag, KBD_f|shiftflag, KBD_g|shiftflag,
				KBD_h|shiftflag, KBD_i|shiftflag, KBD_j|shiftflag, KBD_k|shiftflag, KBD_l|shiftflag, KBD_m|shiftflag, KBD_n|shiftflag, KBD_o|shiftflag,
				KBD_p|shiftflag, KBD_q|shiftflag, KBD_r|shiftflag, KBD_s|shiftflag, KBD_t|shiftflag, KBD_u|shiftflag, KBD_v|shiftflag, KBD_w|shiftflag,
				KBD_x|shiftflag, KBD_y|shiftflag, KBD_z|shiftflag, KBD_leftbracket, KBD_backslash, KBD_rightbracket, KBD_6|shiftflag, KBD_minus|shiftflag,
				KBD_grave, KBD_a, KBD_b, KBD_c, KBD_d, KBD_e, KBD_f, KBD_g,
				KBD_h, KBD_i, KBD_j, KBD_k, KBD_l, KBD_m, KBD_n, KBD_o,
				KBD_p, KBD_q, KBD_r, KBD_s, KBD_t, KBD_u, KBD_v, KBD_w,
				KBD_x, KBD_y, KBD_z, KBD_leftbracket|shiftflag, KBD_backslash|shiftflag, KBD_rightbracket|shiftflag, KBD_grave|shiftflag, 0,
				0, KBD_f1, KBD_f2, KBD_f3, KBD_f4, KBD_f5, KBD_f6, KBD_f7, KBD_f8, KBD_f9, KBD_f10, KBD_f11, KBD_f12,
				KBD_up, KBD_down, KBD_left, KBD_right, KBD_insert, KBD_delete, KBD_home, KBD_end, KBD_pageup, KBD_pagedown,
				KBD_kp0, KBD_kp1, KBD_kp2, KBD_kp3, KBD_kp4, KBD_kp5, KBD_kp6, KBD_kp7, KBD_kp8, KBD_kp9, 
			};
			scankey = (KBD_KEYS)(map[(unsigned char)word[0]] & ~shiftflag);
			if (map[(unsigned char)word[0]] & shiftflag) shift = true;
			if (!scankey && core == 0) {
				WriteOut(MSG_Get("SHELL_SYNTAXERROR"),word);
				return;
			}
			if (core == 0) word[0] = 0;
		}
		if (word[0] == 'p') {
			delay += strtol(word+1,&tail,0);
			if (tail && *tail) {
				WriteOut(MSG_Get("SHELL_SYNTAXERROR"),word);
				return;
			}
		} else if (word[0] == 'l') {
			duration = strtol(word+1,&tail,0);
			if (tail && *tail) {
				WriteOut(MSG_Get("SHELL_SYNTAXERROR"),word);
				return;
			}
		} else if (!word[0] || ((scankey = (KBD_KEYS)strtol(word,NULL,0)) > KBD_NONE && scankey < KBD_LAST)) {
			if (shift) {
				if (delay == 0) KEYBOARD_AddKey(KBD_leftshift,true);
				else PIC_AddEvent(&delayed_press,delay++,KBD_leftshift);
			}
			if (control) {
				if (delay == 0) KEYBOARD_AddKey(KBD_leftctrl,true);
				else PIC_AddEvent(&delayed_press,delay++,KBD_leftctrl);
			}
			if (alt) {
				if (delay == 0) KEYBOARD_AddKey(KBD_leftalt,true);
			else PIC_AddEvent(&delayed_press,delay++,KBD_leftalt);
			}
			if (delay == 0) KEYBOARD_AddKey(scankey,true);
			else PIC_AddEvent(&delayed_press,delay++,scankey);

			if (delay+duration == 0) KEYBOARD_AddKey(scankey,false);
			else PIC_AddEvent(&delayed_release,delay+++duration,scankey);
			if (alt) {
				if (delay+duration == 0) KEYBOARD_AddKey(KBD_leftalt,false);
				else PIC_AddEvent(&delayed_release,delay+++duration,KBD_leftalt);
			}
			if (control) {
				if (delay+duration == 0) KEYBOARD_AddKey(KBD_leftctrl,false);
				else PIC_AddEvent(&delayed_release,delay+++duration,KBD_leftctrl);
			}
			if (shift) {
				if (delay+duration == 0) KEYBOARD_AddKey(KBD_leftshift,false);
				else PIC_AddEvent(&delayed_release,delay+++duration,KBD_leftshift);
			}
		} else if (core != 0) {
			if (core == 1) {
				if (delay == 0) SetVal("cpu","core","normal");
				else PIC_AddEvent(&delayed_sdlpress,delay++,1);
			} else if (core == 2) {
				if (delay == 0) SetVal("cpu","core","simple");
				else PIC_AddEvent(&delayed_sdlpress,delay++,2);
			} else if (core == 3) {
				if (delay == 0) SetVal("cpu","core","dynamic");
				else PIC_AddEvent(&delayed_sdlpress,delay++,3);
			} else if (core == 4) {
				if (delay == 0) SetVal("cpu","core","full");
				else PIC_AddEvent(&delayed_sdlpress,delay++,4);
			}
		} else {
			WriteOut(MSG_Get("SHELL_SYNTAXERROR"),word);
			return;
		}
	}
 }

void DOS_Shell::CMD_FOR(char *args){
}


// save state support
void *delayed_press_PIC_Event = (void*)delayed_press;
void *delayed_release_PIC_Event = (void*)delayed_release;
