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
	void Run(void)
	{
		DOS_Drive * newdrive;char drive;
		
		/* Parse the command line */
		/* if the command line is empty show current mounts */
		if (!cmd->GetCount()) {
			WriteOut("Current mounted drives are\n");
			for (int d=0;d<DOS_DRIVES;d++) {
				if (Drives[d]) {
					WriteOut("Drive %c is mounted as %s\n",d+'A',Drives[d]->GetInfo());
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
				WriteOut("Directory %s Doesn't exist",temp_line.c_str());
				return;
			}
			/* Not a switch so a normal directory/file */
			if (!(test.st_mode & S_IFDIR)) {
				WriteOut("%s isn't a directory",temp_line.c_str());
				return;
			}
			if (temp_line[temp_line.size()-1]!=CROSS_FILESPLIT) temp_line+=CROSS_FILESPLIT;
			newdrive=new localDrive(temp_line.c_str(),sizes[0],sizes[1],sizes[2],sizes[3],mediaid);
		}
		cmd->FindCommand(1,temp_line);
		if (temp_line.size()>1) goto showusage;
		drive=toupper(temp_line[0]);
		if (!isalpha(drive)) goto showusage;
		if (Drives[drive-'A']) {
			WriteOut("Drive %c already mounted with %s\n",drive,Drives[drive-'A']->GetInfo());
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
		WriteOut("Usage MOUNT Drive-Letter Local-Directory\nSo a MOUNT c c:\\windows mounts windows directory as the c: drive in DOSBox\n");
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
	if (!cmd->GetCount()) {
		WriteOut("Usage UPCASE [local directory]\n");
		WriteOut("This tool will convert all files and subdirectories in a directory.\n");
		WriteOut("Be VERY sure this directory contains only dos related material.\n");
		WriteOut("Otherwise you might horribly screw up your filesystem.\n");
		return;
	}
	cmd->FindCommand(1,temp_line);
	if (stat(temp_line.c_str(),&info)) {
		WriteOut("%s doesn't exist\n",temp_line.c_str());
		return;
	}
	if(!S_ISDIR(info.st_mode)) {
		WriteOut("%s isn't a directory\n",temp_line.c_str());
		return;
	}
	WriteOut("Converting the wrong directories can be very harmfull, please be carefull.\n");
	WriteOut("Are you really really sure you want to convert %s to upcase?Y/N\n",temp_line.c_str());
	Bit8u key;Bit16u n=1;
	DOS_ReadFile(STDIN,&key,&n);
	if (toupper(key)=='Y') {
		upcasedir(temp_line.c_str());	
	} else {
		WriteOut("Okay better not do it.\n");
	}
}

static void UPCASE_ProgramStart(Program * * make) {
	*make=new UPCASE;
}
#endif

void DOS_SetupPrograms(void) {
	
	PROGRAMS_MakeFile("MOUNT.COM",MOUNT_ProgramStart);
	PROGRAMS_MakeFile("MEM.COM",MEM_ProgramStart);
#if !defined (WIN32)						/* Unix */
	PROGRAMS_MakeFile("UPCASE.COM",UPCASE_ProgramStart);
#endif
}
