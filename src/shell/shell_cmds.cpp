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

/* $Id: shell_cmds.cpp,v 1.46 2004-09-09 18:36:50 qbix79 Exp $ */

#include <string.h>
#include <ctype.h>

#include "shell.h"
#include "callback.h"
#include "regs.h"
#include "../dos/drives.h"

static SHELL_Cmd cmd_list[]={
{	"CHDIR",	0,			&DOS_Shell::CMD_CHDIR,		"SHELL_CMD_CHDIR_HELP"},
{	"CD",		1,			&DOS_Shell::CMD_CHDIR,		"SHELL_CMD_CHDIR_HELP"},
{	"CLS",		0,			&DOS_Shell::CMD_CLS,		"SHELL_CMD_CLS_HELP"},
{	"COPY",		0,			&DOS_Shell::CMD_COPY,		"SHELL_CMD_COPY_HELP"},
{	"DIR",		0,			&DOS_Shell::CMD_DIR,		"SHELL_CMD_DIR_HELP"},
{	"DEL",		1,			&DOS_Shell::CMD_DELETE,		"SHELL_CMD_DELETE_HELP"},
{	"DELETE",	0,			&DOS_Shell::CMD_DELETE,		"SHELL_CMD_DELETE_HELP"},
{	"ERASE",	1,			&DOS_Shell::CMD_DELETE,		"SHELL_CMD_DELETE_HELP"},
{	"ECHO",		0,			&DOS_Shell::CMD_ECHO,		"SHELL_CMD_ECHO_HELP"},
{	"EXIT",		0,			&DOS_Shell::CMD_EXIT,		"SHELL_CMD_EXIT_HELP"},	
{	"HELP",		1,			&DOS_Shell::CMD_HELP,		"SHELL_CMD_HELP_HELP"},
{	"MKDIR",	0,			&DOS_Shell::CMD_MKDIR,		"SHELL_CMD_MKDIR_HELP"},
{	"MD",		1,			&DOS_Shell::CMD_MKDIR,		"SHELL_CMD_MKDIR_HELP"},
{	"RMDIR",	0,			&DOS_Shell::CMD_RMDIR,		"SHELL_CMD_RMDIR_HELP"},
{	"RD",		1,			&DOS_Shell::CMD_RMDIR,		"SHELL_CMD_RMDIR_HELP"},
{	"SET",		0,			&DOS_Shell::CMD_SET,		"SHELL_CMD_SET_HELP"},
{	"IF",		0,			&DOS_Shell::CMD_IF,			"SHELL_CMD_IF_HELP"},
{	"GOTO",		0,			&DOS_Shell::CMD_GOTO,		"SHELL_CMD_GOTO_HELP"},
{	"TYPE",		0,			&DOS_Shell::CMD_TYPE,		"SHELL_CMD_TYPE_HELP"},
{	"REM",		0,			&DOS_Shell::CMD_REM,		"SHELL_CMD_REM_HELP"},
{	"RENAME",	0,			&DOS_Shell::CMD_RENAME,		"SHELL_CMD_RENAME_HELP"},
{	"REN",		1,			&DOS_Shell::CMD_RENAME,		"SHELL_CMD_RENAME_HELP"},
{	"PAUSE",	0,			&DOS_Shell::CMD_PAUSE,		"SHELL_CMD_PAUSE_HELP"},
{	"CALL",		0,			&DOS_Shell::CMD_CALL,		"SHELL_CMD_CALL_HELP"},
{	"SUBST",	0,			&DOS_Shell::CMD_SUBST,		"SHELL_CMD_SUBST_HELP"},
{	"LOADHIGH",	0,			&DOS_Shell::CMD_LOADHIGH, 	"SHELL_CMD_LOADHIGH_HELP"},
{	"LH",		1,			&DOS_Shell::CMD_LOADHIGH,	"SHELL_CMD_LOADHIGH_HELP"},
{	"CHOICE",	0,			&DOS_Shell::CMD_CHOICE,		"SHELL_CMD_CHOICE_HELP"},
{	"ATTRIB",	0,			&DOS_Shell::CMD_ATTRIB,		"SHELL_CMD_ATTRIB_HELP"},
{0,0,0,0}
};

void DOS_Shell::DoCommand(char * line) {
/* First split the line into command and arguments */
	line=trim(line);
	char cmd[CMD_MAXLINE];
	char * cmd_write=cmd;
	while (*line) {
		if (*line==32) break;
		if (*line=='/') break;
		if (*line=='\t') break;
		if ((*line=='.') ||(*line =='\\')) {  //allow stuff like cd.. and dir.exe cd\kees
			*cmd_write=0;
			Bit32u cmd_index=0;
			while (cmd_list[cmd_index].name) {
				if (strcasecmp(cmd_list[cmd_index].name,cmd)==0) {
					(this->*(cmd_list[cmd_index].handler))(line);
			 	 	return;
				}
				cmd_index++;
			}
      		}
		*cmd_write++=*line++;
	}
	*cmd_write=0;
	if (strlen(cmd)==0) return;
/* Check the internal list */
	Bit32u cmd_index=0;
	while (cmd_list[cmd_index].name) {
		if (strcasecmp(cmd_list[cmd_index].name,cmd)==0) {
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

void DOS_Shell::CMD_DELETE(char * args) {
		
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
//TODO Maybe support confirmation for *.* like dos does.	
	bool res=DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME);
	if (!res) {
		WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),args);return;
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
}

void DOS_Shell::CMD_HELP(char * args){
	/* Print the help */
	WriteOut(MSG_Get("SHELL_CMD_HELP"));
        Bit32u cmd_index=0;
	while (cmd_list[cmd_index].name) {
		if (!cmd_list[cmd_index].flags) WriteOut("%-8s %s",cmd_list[cmd_index].name,MSG_Get(cmd_list[cmd_index].help));
		cmd_index++;
	}

}

void DOS_Shell::CMD_RENAME(char * args){
	StripSpaces(args);
	if(!*args) {SyntaxError();return;}
	if((strchr(args,'*')!=NULL) || (strchr(args,'?')!=NULL) ) { WriteOut(MSG_Get("SHELL_CMD_NO_WILD"));return;}
	char * arg1=StripWord(args);
	DOS_Rename(arg1,args);
}

void DOS_Shell::CMD_ECHO(char * args){
	if (!*args) {
		if (echo) { WriteOut(MSG_Get("SHELL_CMD_ECHO_ON"));}
		else { WriteOut(MSG_Get("SHELL_CMD_ECHO_OFF"));}
	return;
	}
	char buffer[512];
	char* pbuffer = buffer;
	strcpy(buffer,args);
	StripSpaces(pbuffer);
	if (strcasecmp(pbuffer,"OFF")==0) {
		echo=false;		
		return;
	}
	if (strcasecmp(pbuffer,"ON")==0) {
		echo=true;		
		return;
	}
	args++;//skip first character. either a slash or dot or space
	WriteOut("%s\n",args);
};


void DOS_Shell::CMD_EXIT(char * args) {
	exit=true;
};

void DOS_Shell::CMD_CHDIR(char * args) {
	StripSpaces(args);
	if (!*args) {
		Bit8u drive=DOS_GetDefaultDrive()+'A';
		char dir[DOS_PATHLENGTH];
		DOS_GetCurrentDir(0,dir);
		WriteOut("%c:\\%s\n",drive,dir);
	} else 	if (!DOS_ChangeDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_CHDIR_ERROR"),args);
	}
};

void DOS_Shell::CMD_MKDIR(char * args) {
	StripSpaces(args);
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
	StripSpaces(args);
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!DOS_RemoveDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_RMDIR_ERROR"),args);
	}
};

static void FormatNumber(Bitu num,char * buf) {
	Bitu numm,numk,numb;
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
	Bit32u argLen = strlen(args);
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
	WriteOut(MSG_Get("SHELL_CMD_DIR_INTRO"),path);

	DOS_DTA dta(dos.dta());
	bool ret=DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME);
	if (!ret) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
		return;
	}
 
	do {    /* File name and extension */
		char name[DOS_NAMELENGTH_ASCII];Bit32u size;Bit16u date;Bit16u time;Bit8u attr;
		dta.GetResult(name,size,date,time,attr);

		/* Skip non-directories if option AD is present */
		if(optAD && !(attr&DOS_ATTR_DIRECTORY) ) continue;
   
		char * ext="";
		if (!optW && (name[0] != '.')) {
			ext = strrchr(name, '.');
			if (!ext) ext = "";
			else *ext++ = '\0';
		}
		Bit8u day	= date & 0x001f;
		Bit8u month	= (date >> 5) & 0x000f;
		Bit16u year = (date >> 9) + 1980;
		Bit8u hour	= (time >> 5 ) >> 6;
		Bit8u minute = (time >> 5) & 0x003f;

		/* output the file */
		if (attr & DOS_ATTR_DIRECTORY) {
			if (optW) {
				WriteOut("[%s]",name);
				for (Bitu i=14-strlen(name);i>0;i--) WriteOut(" ");
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
		if(optP) {
			if(!(++p_count%(22*w_size))) {
				CMD_PAUSE(args);
			}
		}
	} while ( (ret=DOS_FindNext()) );
	if (optW) {
		if (w_count%5)	WriteOut("\n");
	}

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

void DOS_Shell::CMD_COPY(char * args) {
	static char defaulttarget[] = ".";
	StripSpaces(args);
	DOS_DTA dta(dos.dta());
	Bit32u size;Bit16u date;Bit16u time;Bit8u attr;
	char name[DOS_NAMELENGTH_ASCII];

	// ignore /b and /t switches: always copy binary
	ScanCMDBool(args,"B");
	ScanCMDBool(args,"T");

	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	// source/target
	char* source = StripWord(args);
	char* target = NULL;
	if (args && *args) target = StripWord(args);
	if (!target || !*target) target = defaulttarget;
	
	// Target and Source have to be there
	if (!source || !strlen(source)) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
		return;	
	};

	/* Make a full path in the args */
	char pathSource[DOS_PATHLENGTH];
	char pathTarget[DOS_PATHLENGTH];

	if (!DOS_Canonicalize(source,pathSource)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}
	// cut search pattern
	char* pos = strrchr(pathSource,'\\');
	if (pos) *(pos+1) = 0;

	if (!DOS_Canonicalize(target,pathTarget)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}
	char* temp = strstr(pathTarget,"*.*");
	if(temp) *temp = 0;//strip off *.* from target
	
	// add '\\' if target is a directoy	
	if (pathTarget[strlen(pathTarget)-1]!='\\') {
		if (DOS_FindFirst(pathTarget,0xffff & ~DOS_ATTR_VOLUME)) {
			dta.GetResult(name,size,date,time,attr);
			if (attr & DOS_ATTR_DIRECTORY)	
				strcat(pathTarget,"\\");
		}
	};

	bool ret=DOS_FindFirst(source,0xffff & ~DOS_ATTR_VOLUME);
	if (!ret) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),args);
		return;
	}

	Bit32u count = 0;

	Bit16u sourceHandle,targetHandle;
	char nameTarget[DOS_PATHLENGTH];
	char nameSource[DOS_PATHLENGTH];

	while (ret) {
		dta.GetResult(name,size,date,time,attr);

		if ((attr & DOS_ATTR_DIRECTORY)==0) {
			strcpy(nameSource,pathSource);
			strcat(nameSource,name);
			// Open Source
			if (DOS_OpenFile(nameSource,0,&sourceHandle)) {
				// Create Target
				strcpy(nameTarget,pathTarget);
				if (nameTarget[strlen(nameTarget)-1]=='\\') strcat(nameTarget,name);
				
				if (DOS_CreateFile(nameTarget,0,&targetHandle)) {
					// Copy 
					Bit8u	buffer[0x8000];
					bool	failed = false;
					Bit16u	toread = 0x8000;
					do {
						failed |= DOS_ReadFile(sourceHandle,buffer,&toread);
						failed |= DOS_WriteFile(targetHandle,buffer,&toread);
					} while (toread==0x8000);
					failed |= DOS_CloseFile(sourceHandle);
					failed |= DOS_CloseFile(targetHandle);
					WriteOut(" %s\n",name);
					count++;
				} else {
					DOS_CloseFile(sourceHandle);
					WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),target);
				}
			} else WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),source);
		};
		ret=DOS_FindNext();
	};
	WriteOut(MSG_Get("SHELL_CMD_COPY_SUCCESS"),count);
}

void DOS_Shell::CMD_SET(char * args) {
	StripSpaces(args);
	std::string line;
	if (!*args) {
		/* No command line show all environment lines */	
		Bitu count=GetEnvCount();
		for (Bitu a=0;a<count;a++) {
			if (GetEnvNum(a,line)) WriteOut("%s\n",line.c_str());			
		}
		return;
	}
	char * p=strpbrk(args, "=");
	if (!p) {
		if (!GetEnvStr(args,line)) WriteOut(MSG_Get("SHELL_CMD_SET_NOT_SET"),args);
		WriteOut("%s\n",line.c_str());
	} else {
		*p++=0;
		if (!SetEnv(args,p)) {
			WriteOut(MSG_Get("SHELL_CMD_SET_OUT_OF_SPACE"));
		}
	}
}


void DOS_Shell::CMD_IF(char * args) {
	StripSpaces(args);
	bool has_not=false;
	char * comp=strchr(args,'=');
	if (comp) {
		if (comp[1]!='=') {SyntaxError();return;}
		*comp++=' ';
		*comp++=' ';
	};
	char * word=StripWord(args);
	if (strcasecmp(word,"NOT")==0) {
		word=StripWord(args);
		has_not=true;
	}
	if (strcasecmp(word,"EXIST")==0) {
		word=StripWord(args);
		if (!*word) {
			WriteOut(MSG_Get("SHELL_CMD_IF_EXIST_MISSING_FILENAME"));
			return;
		};
			
		if (DOS_FindFirst(word,0xffff & ~DOS_ATTR_VOLUME)==(!has_not)) DoCommand(args);
		return;
	}
	if (strcasecmp(word,"ERRORLEVEL")==0) {
		word=StripWord(args);
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
		if ((dos.return_code>=n) ==(!has_not)) DoCommand(args);
		return;
	}
	/* Normal if string compare */
	if (!*args) { SyntaxError();return;};
	char * word2=StripWord(args);
	if ((strcmp(word,word2)==0)==(!has_not)) DoCommand(args);
}

void DOS_Shell::CMD_GOTO(char * args) {
	StripSpaces(args);
	if (!bf) return;
	if (*args &&(*args==':')) args++;
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
	StripSpaces(args);
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
	Bit16u n;Bit8u c;
	do {
		n=1;
		DOS_ReadFile(handle,&c,&n);
		DOS_WriteFile(STDOUT,&c,&n);
	} while (n);
	DOS_CloseFile(handle);
	if (*args) goto nextfile;
}

void DOS_Shell::CMD_REM(char * args) {
}

void DOS_Shell::CMD_PAUSE(char * args){
	WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
	Bit8u c;Bit16u n=1;
	DOS_ReadFile (STDIN,&c,&n);
}

void DOS_Shell::CMD_CALL(char * args){
	this->call=true; /* else the old batchfile will be closed first */
	this->ParseLine(args);
	this->call=false;
}

void DOS_Shell::CMD_SUBST (char * args) {
/* If more that one type can be substed think of something else 
 * E.g. make basedir member dos_drive instead of localdrive
 */
	localDrive* ldp=0;
	char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
	char temp_str[2] = { 0,0 };
	try {
		strcpy(mountstring,"MOUNT ");
		StripSpaces(args);
		std::string arg;
		CommandLine command(0,args);

		if (command.GetCount() != 2) throw 0 ;
		command.FindCommand(2,arg);
		if((arg=="/D" ) || (arg=="/d")) throw 1; //No removal (one day)
  
		command.FindCommand(1,arg);
		if(arg[1] !=':')  throw(0);
		temp_str[0]=toupper(args[0]);
		if(Drives[temp_str[0]-'A'] ) throw 0; //targetdrive in use
		strcat(mountstring,temp_str);
		strcat(mountstring," ");

		command.FindCommand(2,arg);
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
	this->ParseLine(args);
}

void DOS_Shell::CMD_CHOICE(char * args){
	static char defargs[] = "[YN]";
	static char defchoice[] = "yn";
	char *rem = NULL, *ptr;
	bool optN = false;
	if (args) {
		char *last = strchr(args,0);
		StripSpaces(args);
		optN=ScanCMDBool(args,"N");
		rem=ScanCMDRemain(args);
		if (rem && *rem && (tolower(rem[1]) != 'c' || rem[2] != ':')) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
			return;
		}
		if (args == rem) args = strchr(rem,0)+1;
		if (rem) rem += 3;
		if (args > last) args = NULL;
	}
	if (!args || !*args) args = defargs;
	if (!rem || !*rem) rem = defchoice;
	ptr = rem;
	Bit8u c;
	while ((c = *ptr)) *ptr++ = tolower(c);

	WriteOut(args);
	if (!optN) WriteOut("\r\n");
	Bit16u n=1;
	do {
		DOS_ReadFile (STDIN,&c,&n);
	} while (!c || !(ptr = strchr(rem,tolower(c))));
	if (optN) {
		DOS_WriteFile (STDOUT,&c, &n);
		WriteOut("\r\n");
	}
	dos.return_code = ptr-rem+1;
}

void DOS_Shell::CMD_ATTRIB(char *args){
	// No-Op for now.
}

