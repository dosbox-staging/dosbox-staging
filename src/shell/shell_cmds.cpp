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
#include <string.h>

#include "shell_inc.h"
#include "callback.h"
#include "regs.h"



static SHELL_Cmd cmd_list[]={
	"CD",		0,			&DOS_Shell::CMD_CHDIR,		"Change Directory.",
	"CLS",		0,			&DOS_Shell::CMD_CLS,		"Clear screen.",
//	"COPY",		0,			&DOS_Shell::CMD_COPY,		"Copy Files.",
	"DIR",		0,			&DOS_Shell::CMD_DIR,		"Directory View.",
	"ECHO",		0,			&DOS_Shell::CMD_ECHO,		"Display messages and enable/disable command echoing.",
	"EXIT",		0,			&DOS_Shell::CMD_EXIT,		"Exit from the shell.",	
	"HELP",		0,			&DOS_Shell::CMD_HELP,		"Show help.",
	"MD",		0,			&DOS_Shell::CMD_MKDIR,		"Make Directory.",
	"RD",		0,			&DOS_Shell::CMD_RMDIR,		"Remove Directory.",
	"SET",		0,			&DOS_Shell::CMD_SET,		"Change environment variables.",
	"IF",		0,			&DOS_Shell::CMD_IF,			"Performs conditional processing in batch programs.",
	"GOTO",		0,			&DOS_Shell::CMD_GOTO,		"Jump to a labeled line in a batch script.",
	"TYPE",		0,			&DOS_Shell::CMD_TYPE,		"Display the contents of a text-file.",
/*
	"CHDIR",	0,			&DOS_Shell::CMD_CHDIR,		"Change Directory",
	"MKDIR",	0,			&DOS_Shell::CMD_MKDIR,		"Make Directory",
	"RMDIR",	0,			&DOS_Shell::CMD_RMDIR,		"Remove Directory",
*/
	0,0,0,0
};

void DOS_Shell::DoCommand(char * line) {
/* First split the line into command and arguments */
	line=trim(line);
	char cmd[255];
	char * cmd_write=cmd;
	while (*line) {
		if (*line==32) break;
		if (*line=='/') break;
		if (*line=='.') break;
		*cmd_write++=*line++;
	}
	*cmd_write=0;
	if (strlen(cmd)==0) return;
	line=trim(line);
/* Check the internal list */
	Bit32u cmd_index=0;
	while (cmd_list[cmd_index].name) {
		if (strcasecmp(cmd_list[cmd_index].name,cmd)==0) {
//TODO CHECK Flags
			(this->*(cmd_list[cmd_index].handler))(line);
		        return;
		}
		cmd_index++;
	}
/* This isn't an internal command execute it */
	Execute(cmd,line);
}


void DOS_Shell::CMD_CLS(char * args) {
	reg_ax=0x0003;
	CALLBACK_RunRealInt(0x10);
};

void DOS_Shell::CMD_HELP(char * args){
	/* Print the help */
	WriteOut(MSG_Get("SHELL_CMD_HELP"));
        Bit32u cmd_index=0;
	while (cmd_list[cmd_index].name) {
		if (!cmd_list[cmd_index].flags) WriteOut("%-8s %s\n",cmd_list[cmd_index].name,cmd_list[cmd_index].help);
		cmd_index++;
	}

}

void DOS_Shell::CMD_ECHO(char * args) {
	if (!*args) {
		if (echo) { WriteOut(MSG_Get("SHELL_CMD_ECHO_ON"));}
		else { WriteOut(MSG_Get("SHELL_CMD_ECHO_OFF"));}
		return;
	}
	if (strcasecmp(args,"OFF")==0) {
		echo=false;		
		return;
	}
	if (strcasecmp(args,"ON")==0) {
		echo=true;		
		return;
	}
	WriteOut("%s\n",args);
};

void DOS_Shell::CMD_EXIT(char * args) {
	exit=true;
};

void DOS_Shell::CMD_CHDIR(char * args) {
	if (!*args) {
		Bit8u drive=DOS_GetDefaultDrive()+'A';
		Bit8u dir[DOS_PATHLENGTH];
		DOS_GetCurrentDir(0,dir);
		WriteOut("%c:\\%s\n",drive,dir);
	}
	if (DOS_ChangeDir(args)) {
		
	} else {
	        WriteOut(MSG_Get("SHELL_CMD_CHDIR_ERROR"),args);
	}

};

void DOS_Shell::CMD_MKDIR(char * args) {
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!DOS_MakeDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_MKDIR_ERROR"),args);
	}
};

void DOS_Shell::CMD_RMDIR(char * args) {
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!DOS_RemoveDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_RMDIR_ERROR"),args);
	}
};

static void FormatNumber(Bit32u num,char * buf) {
	Bit32u numm,numk,numb;
	numb=num % 1000;
	num/=1000;
	numk=num % 1000;
	num/=1000;
	numm=num;
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
	char numformat[16];
	char path[DOS_PATHLENGTH];

	bool optW=ScanCMDBool(args,"W");
	bool optS=ScanCMDBool(args,"S");
	bool optP=ScanCMDBool(args,"P");
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}

	Bit32u byte_count,file_count,dir_count;
	Bit32u w_count=0;
	byte_count=file_count=dir_count=0;
		
	if (strlen(args)==0) args="*.*";

	/* Make a full path in the args */
	if (!DOS_Canonicalize(args,(Bit8u*)path)) {
		WriteOut(MSG_Get("SHELL_CMD_DIR_PATH_ERROR"));
		return;
	}
	*(strrchr(path,'\\')+1)=0;
	WriteOut(MSG_Get("SHELL_CMD_DIR_INTRO"),path);

	DTA_FindBlock * dta;
	dta=(DTA_FindBlock *)Real2Host(dos.dta);
	bool ret=DOS_FindFirst(args,0xffff);
	if (!ret) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
		return;
	}

	while (ret) {

/* File name and extension */
		char * ext="";
		if (!optW && (*dta->name != '.')) {
			ext = strrchr(dta->name, '.');
			if (!ext) ext = "";
			else *ext++ = '\0';
		};
	   
		Bit8u day	= dta->date & 0x001f;
		Bit8u month	= (dta->date >> 5) & 0x000f;
		Bit8u hour	= dta->time >> 5 >> 6;
		Bit8u minute = (dta->time >> 5) & 0x003f;
		Bit16u year = (dta->date >> 9) + 1980;

		/* output the file */
		if (dta->attr & DOS_ATTR_DIRECTORY) {
			if (optW) {
				WriteOut("[%s]",dta->name);
				for (Bitu i=14-strlen(dta->name);i>0;i--) WriteOut(" ");
			} else {
				WriteOut("%-8s %-3s   %-16s %02d-%02d-%04d %2d:%02d\n",dta->name,ext,"<DIR>",day,month,year,hour,minute);
			}
			dir_count++;
		} else {
			if (optW) {
				WriteOut("%-16s",dta->name);
			} else {
				FormatNumber(dta->size,numformat);
				WriteOut("%-8s %-3s   %16s %02d-%02d-%04d %2d:%02d\n",dta->name,ext,numformat,day,month,year,hour,minute);
			}
			file_count++;
			byte_count+=dta->size;
		}
		if (optW) {
			w_count++;
		}
		ret=DOS_FindNext();
	}
	if (optW) {
		if (w_count%5)	WriteOut("\n");
	}
	/* Show the summary of results */
	FormatNumber(byte_count,numformat);
	WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_USED"),file_count,numformat);
	//TODO Free Space
	FormatNumber(1024*1024*100,numformat);
	WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_FREE"),dir_count,numformat);
}

void DOS_Shell::CMD_COPY(char * args) {
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}

}

void DOS_Shell::CMD_SET(char * args) {
	if (!*args) {
		/* No command line show all environment lines */	
		Bit32u count=GetEnvCount();
		for (Bit32u a=0;a<count;a++) {
			WriteOut("%s\n",GetEnvNum(a));			
		}
		return;
	}
	char * p=strpbrk(args, "=");
	if (!p) {
		p=GetEnvStr(args);
		if (p) WriteOut("%s\n",p);
		else WriteOut(MSG_Get("SHELL_CMD_SET_NOT_SET"),args);
	} else {
		*p++=0;
		if (!SetEnv(args,p)) {
			WriteOut(MSG_Get("SHELL_CMD_SET_OUT_OF_SPACE"));
		}
	}
}


void DOS_Shell::CMD_IF(char * args) {
	bool has_not=false;

	char * comp=strchr(args,'=');
	if (comp) {
		if (comp[1]!='=') {SyntaxError();return;}
		*comp++=' ';
		*comp++=' ';
	};
	char * word;
	word=args;
	args=StripWord(word);
	if (strcasecmp(word,"NOT")==0) {
		word=args;
		has_not=true;
		args=StripWord(word);
	}
	if (strcasecmp(word,"EXIST")==0) {
		word=args;args=StripWord(word);
		if (!*word) {
			WriteOut(MSG_Get("SHELL_CMD_IF_EXIST_MISSING_FILENAME"));
			return;
		};
			
		if (DOS_FindFirst(word,0xFFFF)==(!has_not)) DoCommand(args);
		return;
	}
	if (strcasecmp(word,"ERRORLEVEL")==0) {
		word=args;args=StripWord(word);
		if(!isdigit(*word)) {
			WriteOut(MSG_Get("SHELL_CMD_IF_ERRORLEVEL_MISSING_NUMBER"));
			return;
		}

		Bit8u n=0;
		do n = n * 10 + (*word - '0');
		while (isdigit(*++word));
		if(*word && !isspace(*word)) {
			WriteOut(MSG_Get("SHELL_CMD_IF_ERRORLEVEL_INVALID_NUMBER"));
			return;
		}
		/* Read the error code from DOS */
		reg_ah=0x4d;
		CALLBACK_RunRealInt(0x21);
		if ((reg_al>=n) ==(!has_not)) DoCommand(args);
		return;
	}
	/* Normal if string compare */
	if (!*args) { SyntaxError();return;};
	char * word2=args;
	args=StripWord(word2);
	if ((strcmp(word,word2)==0)==(!has_not)) DoCommand(args);
}

void DOS_Shell::CMD_GOTO(char * args) {
	if (!bf) return;
	if (*args==':') args++;
	if (!*args) {
		WriteOut(MSG_Get("SHELL_CMD_GOTO_MISSING_LABEL"));
		return;
	}
	if (!bf->Goto(args)) {
		WriteOut(MSG_Get("SHELL_CMD_GOTO_LABEL_NOT_FOUND"),args);
		return;
	}
}


void DOS_Shell::CMD_TYPE(char * args) {
	
	if (!*args) {
		WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
		return;
	}
	Bit16u handle;
	char * word;
nextfile:
	word=args;
	args=StripWord(word);
	if (!DOS_OpenFile(word,0,&handle)) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),word);
		return;
	}
	Bit16u n;Bit8u c;
	do {
		n=1;
		DOS_ReadFile(handle,&c,&n);
		DOS_WriteFile(STDOUT,&c,&n);
	} while (n);
	DOS_CloseFile(handle);
	if (*args) goto nextfile;
}


       
