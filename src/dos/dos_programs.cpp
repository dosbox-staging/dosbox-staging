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
#include "../shell/shell_inc.h"

class MOUNT : public Program {
public:
	void Run(void)
	{
		DOS_Drive * newdrive;char drive;
		
		/* Parse the command line */
		/* if the command line is empty show current mounts */
		if (!cmd->GetCount()) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_1"));
			for (int d=0;d<DOS_DRIVES;d++) {
				if (Drives[d]) {
					WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),d+'A',Drives[d]->GetInfo());
				}
			}
			return;
		}
		std::string type="dir";
		cmd->FindString("-t",type,true);
		if (type=="floppy" || type=="dir") {
			Bit16u sizes[4];
			Bit8u mediaid;
			std::string str_size;
			if (type=="floppy") {
				str_size="512,1,2847,2847";/* All space free */
				mediaid=0xF0;		/* Floppy 1.44 media */
			}
			if (type=="dir") {
				str_size="512,127,16513,1700";
				mediaid=0xF8;		/* Hard Disk */
			}
			cmd->FindString("-size",str_size,true);
			char number[20];const char * scan=str_size.c_str();
			Bitu index=0;Bitu count=0;
			/* Parse the str_size string */
			while (*scan) {
				if (*scan==',') {
					number[index]=0;sizes[count++]=atoi(number);
					index=0;
				} else number[index++]=*scan;
				scan++;
			}
			number[index]=0;sizes[count++]=atoi(number);


			if (!cmd->FindCommand(2,temp_line)) goto showusage;
			if (!temp_line.size()) goto showusage;
			struct stat test;
			if (stat(temp_line.c_str(),&test)) {
				WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_1"),temp_line.c_str());
				return;
			}
			/* Not a switch so a normal directory/file */
			if (!(test.st_mode & S_IFDIR)) {
				WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_2"),temp_line.c_str());
				return;
			}
			if (temp_line[temp_line.size()-1]!=CROSS_FILESPLIT) temp_line+=CROSS_FILESPLIT;
            Bit8u bit8size=(Bit8u) sizes[1];
			newdrive=new localDrive(temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid);
		}
		cmd->FindCommand(1,temp_line);
		if (temp_line.size()>1) goto showusage;
		drive=toupper(temp_line[0]);
		if (!isalpha(drive)) goto showusage;
		if (Drives[drive-'A']) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_ALLREADY_MOUNDTED"),drive,Drives[drive-'A']->GetInfo());
			if (newdrive) delete newdrive;
			return;
		}
		if (!newdrive) E_Exit("DOS:Can't create drive");
		Drives[drive-'A']=newdrive;
		/* Set the correct media byte in the table */
		mem_writeb(Real2Phys(dos.tables.mediaid)+drive-'A',newdrive->GetMediaByte());
		WriteOut("Drive %c mounted as %s\n",drive,newdrive->GetInfo());
		return;
showusage:
		WriteOut(MSG_Get("PROGRAM_MOUNT_USAGE"));
		return;
	}
};

static void MOUNT_ProgramStart(Program * * make) {
	*make=new MOUNT;
}

class MEM : public Program {
public:
	void Run(void) {
		/* Show conventional Memory */
		WriteOut("\n");
		Bit16u seg,blocks;blocks=0xffff;
		DOS_AllocateMemory(&seg,&blocks);
		WriteOut(MSG_Get("PROGRAM_MEM_CONVEN"),blocks*16/1024);
		/* Test for and show free XMS */
		reg_ax=0x4300;CALLBACK_RunRealInt(0x2f);
		if (reg_al==0x80) {
			reg_ax=0x4310;CALLBACK_RunRealInt(0x2f);
			Bit16u xms_seg=SegValue(es);Bit16u xms_off=reg_bx;
			reg_ah=8;
			CALLBACK_RunRealFar(xms_seg,xms_off);
			if (!reg_bl) {
				WriteOut(MSG_Get("PROGRAM_MEM_EXTEND"),reg_dx);
			}
		}	
		/* Test for and show free EMS */
		Bit16u handle;
		if (DOS_OpenFile("EMMXXXX0",0,&handle)) {
			DOS_CloseFile(handle);
			reg_ah=0x42;
			CALLBACK_RunRealInt(0x67);
			WriteOut(MSG_Get("PROGRAM_MEM_EXPAND"),reg_bx*16);
		}
	}
};


static void MEM_ProgramStart(Program * * make) {
	*make=new MEM;
}


#if !defined (WIN32)						/* Unix */

class UPCASE : public Program {
public:
	void Run(void);
	void upcasedir(const char * directory);
};

void UPCASE::upcasedir(const char * directory) {
	DIR * sdir;
	char fullname[512];
	char newname[512];
	struct dirent *tempdata;
	struct stat finfo;

	if(!(sdir=opendir(directory)))	{
		WriteOut(MSG_Get("PROGRAM_UPCASE_ERROR_DIR"),directory);
		return;
	}
	WriteOut(MSG_Get("PROGRAM_UPCASE_SCANNING_DIR"),fullname);
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
		WriteOut(MSG_Get("PROGRAM_UPCASE_RENAME"),fullname,newname);
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
	WriteOut(MSG_Get("PROGRAM_UPCASE_RUN_1"));
	if (!cmd->GetCount()) {
		WriteOut(MSG_Get("PROGRAM_UPCASE_USAGE"));
		return;
	}
	cmd->FindCommand(1,temp_line);
	if (stat(temp_line.c_str(),&info)) {
		WriteOut(MSG_Get("PROGRAM_UPCASE_RUN_ERROR_1"),temp_line.c_str());
		return;
	}
	if(!S_ISDIR(info.st_mode)) {
		WriteOut(MSG_Get("PROGRAM_UPCASE_RUN_ERROR_2"),temp_line.c_str());
		return;
	}
	WriteOut(MSG_Get("PROGRAM_UPCASE_RUN_CHOICE"),temp_line.c_str());
	Bit8u key;Bit16u n=1;
	DOS_ReadFile(STDIN,&key,&n);
	if (toupper(key)=='Y') {
		upcasedir(temp_line.c_str());	
	} else {
		WriteOut(MSG_Get("PROGRAM_UPCASE_RUN_NO"));
	}
}

static void UPCASE_ProgramStart(Program * * make) {
	*make=new UPCASE;
}
#endif

// LOADFIX

class LOADFIX : public Program {
public:
	void Run(void);
};

void LOADFIX::Run(void) 
{
	Bit16u commandNr	= 1;
	Bit16u kb			= 64;
	if (cmd->FindCommand(commandNr,temp_line)) {
		if (temp_line[0]=='-') {
			char ch = temp_line[1];
			if ((*upcase(&ch)=='D') || (*upcase(&ch)=='F')) {
				// Deallocate all
				DOS_FreeProcessMemory(0x40);
				WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOCALL"),kb);
				return;
			} else {
				// Set mem amount to allocate
				kb = atoi(temp_line.c_str()+1);
				if (kb==0) kb=64;
				commandNr++;
			}
		}
	}
	// Allocate Memory
	Bit16u segment;
	Bit16u blocks = kb*1024/16;
	if (DOS_AllocateMemory(&segment,&blocks)) {
		MCB* pmcb = (MCB*)HostMake(segment-1,0);
		pmcb->psp_segment = 0x40;	// use fake segment
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ALLOC"),kb);
		// Prepare commandline...
		if (cmd->FindCommand(commandNr++,temp_line)) {
			// get Filename
			char filename[128];
			strncpy(filename,temp_line.c_str(),128);
			// Setup commandline
			bool ok;
			char args[256];
			args[0] = 0;
			do {
				ok = cmd->FindCommand(commandNr++,temp_line);
				strncat(args,temp_line.c_str(),256);
				strncat(args," ",256);
			} while (ok);			
			// Use shell to start program
			DOS_Shell shell;
			shell.Execute(filename,args);
			DOS_FreeMemory(segment);		
			WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOC"),kb);
		}
	} else {
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ERROR"),kb);	
	}
};

static void LOADFIX_ProgramStart(Program * * make) {
	*make=new LOADFIX;
}

void DOS_SetupPrograms(void) {
    /*Add Messages */
	MSG_Add("PROGRAM_MOUNT_STATUS_2","Drive %c is mounted as %s\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_1","Current mounted drives are:\n");
    MSG_Add("PROGRAM_MOUNT_ERROR_1","Directory %s doesn't exist.\n");
    MSG_Add("PROGRAM_MOUNT_ERROR_2","%s isn't a directory\n");
    MSG_Add("PROGRAM_MOUNT_ALLREADY_MOUNTED","Drive %c already mounted with %s\n");
    MSG_Add("PROGRAM_MOUNT_USAGE","Usage MOUNT Drive-Letter Local-Directory\nSo a MOUNT c c:\\windows mounts windows directory as the c: drive in DOSBox\n");

    MSG_Add("PROGRAM_MEM_CONVEN","%10d Kb free conventional memory\n");
    MSG_Add("PROGRAM_MEM_EXTEND","%10d Kb free extended memory\n");
    MSG_Add("PROGRAM_MEM_EXPAND","%10d Kb free expanded memory\n");

	MSG_Add("PROGRAM_LOADFIX_ALLOC","%d kb allocated.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOC","%d kb freed.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOCALL","Used memory freed.\n");
	MSG_Add("PROGRAM_LOADFIX_ERROR","Memory allocation error.\n");

#if !defined (WIN32)                        /* Unix */
    MSG_Add("PROGRAM_UPCASE_ERROR_DIR","Failed to open directory %s\n");
    MSG_Add("PROGRAM_UPCASE_SCANNING_DIR","Scanning directory %s\n");
    MSG_Add("PROGRAM_UPCASE_RENAME","Renaming %s to %s\n");
    MSG_Add("PROGRAM_UPCASE_RUN_1","UPCASE 0.1 Directory case convertor.\n");
    MSG_Add("PROGRAM_UPCASE_USAGE","Usage UPCASE [local directory]\nThis tool will convert all files and subdirectories in a directory.\nBe VERY sure this directory contains only dos related material.\nOtherwise you might horribly screw up your filesystem.\n");
    MSG_Add("PROGRAM_UPCASE_RUN_ERROR_1","%s doesn't exist\n");
    MSG_Add("PROGRAM_UPCASE_RUN_ERROR_2","%s isn't a directory\n");
    MSG_Add("PROGRAM_UPCASE_RUN_CHOICE","Converting the wrong directories can be very harmfull, please be carefull.\nAre you really really sure you want to convert %s to upcase?Y/N\n");
    MSG_Add("PROGRAM_UPCASE_RUN_NO","Okay better not do it.\n");
#endif
    /*regular setup*/
	PROGRAMS_MakeFile("MOUNT.COM",MOUNT_ProgramStart);
	PROGRAMS_MakeFile("MEM.COM",MEM_ProgramStart);
	PROGRAMS_MakeFile("LOADFIX.COM",LOADFIX_ProgramStart);
#if !defined (WIN32)						/* Unix */
	PROGRAMS_MakeFile("UPCASE.COM",UPCASE_ProgramStart);
#endif
}
