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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: programs.cpp,v 1.13 2004-05-04 18:34:08 qbix79 Exp $ */

#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "programs.h"
#include "callback.h"
#include "regs.h"
#include "support.h"
#include "cross.h"
#include "setup.h"

Bitu call_program;

/* This registers a file on the virtual drive and creates the correct structure for it*/

static Bit8u exe_block[]={
	0xbc,0x00,0x04,					//MOV SP,0x400 decrease stack size
	0xbb,0x40,0x00,					//MOV BX,0x040 for memory resize
	0xb4,0x4a,						//MOV AH,0x4A	Resize memory block
	0xcd,0x21,						//INT 0x21
//pos 12 is callback number
	0xFE,0x38,0x00,0x00,			//CALLBack number
	0xb8,0x00,0x4c,					//Mov ax,4c00
	0xcd,0x21,						//INT 0x21
};

#define CB_POS 12

void PROGRAMS_MakeFile(char * name,PROGRAMS_Main * main) {
	Bit8u * comdata=(Bit8u *)malloc(128);
	memcpy(comdata,&exe_block,sizeof(exe_block));
	comdata[CB_POS]=call_program&0xff;
	comdata[CB_POS+1]=(call_program>>8)&0xff;
/* Copy the pointer this should preserve endianes */
	memcpy(&comdata[sizeof(exe_block)],&main,sizeof(main));
	Bit32u size=sizeof(exe_block)+sizeof(main);	
	VFILE_Register(name,comdata,size);	
}



static Bitu PROGRAMS_Handler(void) {
	/* This sets up everything for a program start up call */
	PROGRAMS_Main * handler=0;			//It will get sneakily itinialized
	Bitu size=sizeof(PROGRAMS_Main *);
	/* Read the handler from program code in memory */
	PhysPt reader=PhysMake(dos.psp(),256+sizeof(exe_block));
	HostPt writer=(HostPt)&handler;
	for (;size>0;size--) *writer++=mem_readb(reader++);
	Program * new_program;
	(*handler)(&new_program);
	new_program->Run();
	delete new_program;
	return CBRET_NONE;
}


/* Main functions used in all program */


Program::Program() {
	/* Find the command line and setup the PSP */
	psp = new DOS_PSP(dos.psp());
	/* Scan environment for filename */
	PhysPt envscan=PhysMake(psp->GetEnvironment(),0);
	while (mem_readb(envscan)) envscan+=mem_strlen(envscan)+1;	
	envscan+=3;
	CommandTail tail;
	MEM_BlockRead(PhysMake(dos.psp(),128),&tail,128);
	if (tail.count<127) tail.buffer[tail.count]=0;
	else tail.buffer[126]=0;
	char filename[256];
	MEM_StrCopy(envscan,filename,256);
	cmd = new CommandLine(filename,tail.buffer);
}

void Program::WriteOut(const char * format,...) {
	char buf[1024];
	va_list msg;
	
	va_start(msg,format);
	vsprintf(buf,format,msg);
	va_end(msg);

	Bit16u size=strlen(buf);
	DOS_WriteFile(STDOUT,(Bit8u *)buf,&size);
}


bool Program::GetEnvStr(const char * entry,std::string & result) {
	/* Walk through the internal environment and see for a match */
	PhysPt env_read=PhysMake(psp->GetEnvironment(),0);
	char env_string[1024];
	result.erase();
	if (!entry[0]) return false;
	do 	{
		MEM_StrCopy(env_read,env_string,1024);
		if (!env_string[0]) return false;
		env_read+=strlen(env_string)+1;
		if (!strchr(env_string,'=')) continue;
		if (strncasecmp(entry,env_string,strlen(entry))!=0) continue;
		result=env_string;
		return true;
	} while (1);
	return false;
};

bool Program::GetEnvNum(Bitu num,std::string & result) {
	char env_string[1024];
	PhysPt env_read=PhysMake(psp->GetEnvironment(),0);
	do 	{
		MEM_StrCopy(env_read,env_string,1024);
		if (!env_string[0]) break;
		if (!num) { result=env_string;return true;}
		env_read+=strlen(env_string)+1;
		num--;
	} while (1);
	return false;
}

Bitu Program::GetEnvCount(void) {
	PhysPt env_read=PhysMake(psp->GetEnvironment(),0);
	Bitu num=0;
	while (mem_readb(env_read)!=0) {
		for (;mem_readb(env_read);env_read++) {};
		env_read++;
		num++;
	}
	return num;
}

bool Program::SetEnv(const char * entry,const char * new_string) {
	PhysPt env_read=PhysMake(psp->GetEnvironment(),0);
	PhysPt env_write=env_read;
	char env_string[1024];
	do 	{
		MEM_StrCopy(env_read,env_string,1024);
		if (!env_string[0]) break;
		env_read+=strlen(env_string)+1;
		if (!strchr(env_string,'=')) continue;		/* Remove corrupt entry? */
		if ((strncasecmp(entry,env_string,strlen(entry))==0) && 
			env_string[strlen(entry)]=='=') continue;
		MEM_BlockWrite(env_write,env_string,strlen(env_string)+1);
		env_write+=strlen(env_string)+1;
	} while (1);
/* TODO Maybe save the program name sometime. not really needed though */
	/* Save the new entry */
	if (new_string[0]) {
		std::string bigentry(entry);
		for (std::string::iterator it = bigentry.begin(); it != bigentry.end(); ++it) *it = toupper(*it);
		sprintf(env_string,"%s=%s",bigentry.c_str(),new_string); 
//		sprintf(env_string,"%s=%s",entry,new_string); //oldcode
		MEM_BlockWrite(env_write,env_string,strlen(env_string)+1);
		env_write+=strlen(env_string)+1;
	}
	/* Clear out the final piece of the environment */
	mem_writed(env_write,0);
	return true;
}

class CONFIG : public Program {
public:
	void Run(void);
};

void MSG_Write(const char *);

void CONFIG::Run(void) {
	FILE * f;
	if (cmd->FindString("-writeconf",temp_line,true)) {
		f=fopen(temp_line.c_str(),"wb+");
		if (!f) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_ERROR"),temp_line.c_str());
			return;
		}
		fclose(f);
		control->PrintConfig(temp_line.c_str());
		return;
	}
	if (cmd->FindString("-writelang",temp_line,true)) {
		f=fopen(temp_line.c_str(),"wb+");
		if (!f) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_ERROR"),temp_line.c_str());
			return;
		}
		fclose(f);
		MSG_Write(temp_line.c_str());
		return;
	}
	WriteOut(MSG_Get("PROGRAM_CONFIG_USAGE"));
}


static void CONFIG_ProgramStart(Program * * make) {
	*make=new CONFIG;
}


void PROGRAMS_Init(Section* sec) {
	/* Setup a special callback to start virtual programs */
	call_program=CALLBACK_Allocate();
	CALLBACK_Setup(call_program,&PROGRAMS_Handler,CB_RETF);
	PROGRAMS_MakeFile("CONFIG.COM",CONFIG_ProgramStart);

	MSG_Add("PROGRAM_CONFIG_FILE_ERROR","Can't open file %s\n");
	MSG_Add("PROGRAM_CONFIG_USAGE","Config tool:\nUse -writeconf filename to write the current config.\nUse -writelang filename to write the current language strings.\n");
}
