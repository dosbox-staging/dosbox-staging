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
#include <ctype.h>
#include "programs.h"
#include "support.h"
#include "drives.h"
#include "cross.h"
#include "regs.h"
#include "callback.h"

class MOUNT : public Program {
public:
	MOUNT(PROGRAM_Info * program_info):Program(program_info){};
	void Run(void){
		/* Parse the command line */
		/* if the command line is empty show current mounts */
		if (!*prog_info->cmd_line) {
			WriteOut("Current mounted drives are\n");
			for (int d=0;d<DOS_DRIVES;d++) {
				if (Drives[d]) {
					WriteOut("Drive %c is mounted as %s\n",d+'A',Drives[d]->GetInfo());
				}
			}
			return;
		}

		char drive;	drive=toupper(*prog_info->cmd_line);

		char * dir=strchr(prog_info->cmd_line,' ');	if (dir) {
			if (!*dir) dir=0;
			else dir=trim(dir);
		}
		if (!isalpha(drive) || !dir) {
			WriteOut("Usage MOUNT Drive-Letter Local-Directory\nSo a MOUNT c c:\\windows mounts windows directory as the c: drive in DOSBox\n");
			return;
		};
		struct stat test;
		if (stat(dir,&test)) {
			WriteOut("Directory %s Doesn't exist",dir);
			return;
		}
		/* Not a switch so a normal directory/file */
		if (!(test.st_mode & S_IFDIR)) {
			WriteOut("%s isn't a directory",dir);
			return;
		}
		if (Drives[drive-'A']) {
			WriteOut("Drive %c already mounted with %s\n",drive,Drives[drive-'A']->GetInfo());
			return;
		}
		char fulldir[DOS_PATHLENGTH];
		strcpy(fulldir,dir);
		static char theend[2]={CROSS_FILESPLIT,0};
		char * last=strrchr(fulldir,CROSS_FILESPLIT);
		if (!last || *(++last))  strcat(fulldir,theend);
		Drives[drive-'A']=new localDrive(fulldir);
		WriteOut("Mounting drive %c as %s\n",drive,fulldir);
	}
};

static void MOUNT_ProgramStart(PROGRAM_Info * info) {
	MOUNT * tempmount=new MOUNT(info);
	tempmount->Run() ;
	delete tempmount;
}

class MEM : public Program {
public:
	MEM(PROGRAM_Info * program_info):Program(program_info){};
	void Run(void) {
		/* Show conventional Memory */
		WriteOut("\n");
		Bit16u seg,blocks;blocks=0xffff;
		DOS_AllocateMemory(&seg,&blocks);
		WriteOut("%10d Kb free conventional memory\n",blocks*16/1024);
		/* Test for and show free XMS */
		reg_ax=0x4300;CALLBACK_RunRealInt(0x2f);
		if (reg_al==0x80) {
			reg_ax=0x4310;CALLBACK_RunRealInt(0x2f);
			Bit16u xms_seg=SegValue(es);Bit16u xms_off=reg_bx;
			reg_ah=8;
			CALLBACK_RunRealFar(xms_seg,xms_off);
			if (!reg_bl) {
				WriteOut("%10d Kb free extended memory\n",reg_dx);
			}
		}	
		/* Test for and show free EMS */
		Bit16u handle;
		if (DOS_OpenFile("EMMXXXX0",0,&handle)) {
			DOS_CloseFile(handle);
			reg_ah=0x42;
			CALLBACK_RunRealInt(0x67);
			WriteOut("%10d Kb free expanded memory\n",reg_bx*16);
		}
	}
};



static void MEM_ProgramStart(PROGRAM_Info * info) {
	MEM mem(info);
	mem.Run();
}


#if !defined (WIN32)						/* Unix */
class UPCASE : public Program {
public:
	UPCASE(PROGRAM_Info * program_info);
	void Run(void);
	void upcasedir(char * directory);
};


UPCASE::UPCASE(PROGRAM_Info * info):Program(info) {

}


void UPCASE::upcasedir(char * directory) {
	DIR * sdir;
	char fullname[512];
	char newname[512];
	struct dirent *tempdata;
	struct stat finfo;

	if(!(sdir=opendir(directory)))	{
		WriteOut("Failed to open directory %s\n",directory);
		return;
	}
	WriteOut("Scanning directory %s\n",fullname);
	while (tempdata=readdir(sdir)) {
		if (strcmp(tempdata->d_name,".")==0) continue;
		if (strcmp(tempdata->d_name,"..")==0) continue;
		strcpy(fullname,directory);
		strcat(fullname,"/");
		strcat(fullname,tempdata->d_name);
		strcpy(newname,directory);
		strcat(newname,"/");
		upcase(tempdata->d_name);
		strcat(newname,tempdata->d_name);
		WriteOut("Renaming %s to %s\n",fullname,newname);
		rename(fullname,newname);
		stat(fullname,&finfo);
		if(S_ISDIR(finfo.st_mode)) {
			upcasedir(fullname);
		}
	}
	closedir(sdir);
}


void UPCASE::Run(void) {
	/* First check if the directory exists */
	struct stat info;
	WriteOut("UPCASE 0.1 Directory case convertor.\n");
	if (!strlen(prog_info->cmd_line)) {
		WriteOut("Usage UPCASE [local directory]\n");
		WriteOut("This tool will convert all files and subdirectories in a directory.\n");
		WriteOut("Be VERY sure this directory contains only dos related material.\n");
		WriteOut("Otherwise you might horribly screw up your filesystem.\n");
		return;
	}
	if (stat(prog_info->cmd_line,&info)) {
		WriteOut("%s doesn't exist\n",prog_info->cmd_line);
		return;
	}
	if(!S_ISDIR(info.st_mode)) {
		WriteOut("%s isn't a directory\n",prog_info->cmd_line);
		return;
	}
	WriteOut("Converting the wrong directories can be very harmfull, please be carefull.\n");
	WriteOut("Are you really really sure you want to convert %s to upcase?Y/N\n",prog_info->cmd_line);
	Bit8u key;Bit16u n=1;
	DOS_ReadFile(STDIN,&key,&n);
	if (toupper(key)=='Y') {
		upcasedir(prog_info->cmd_line);	
	} else {
		WriteOut("Okay better not do it.\n");
	}
}

static void UPCASE_ProgramStart(PROGRAM_Info * info) {
	UPCASE * tempUPCASE=new UPCASE(info);
	tempUPCASE->Run();
	delete tempUPCASE;
}

#endif

void DOS_SetupPrograms(void) {
	PROGRAMS_MakeFile("MOUNT.COM",MOUNT_ProgramStart);
	PROGRAMS_MakeFile("MEM.COM",MEM_ProgramStart);
#if !defined (WIN32)						/* Unix */
	PROGRAMS_MakeFile("UPCASE.COM",UPCASE_ProgramStart);
#endif
}
