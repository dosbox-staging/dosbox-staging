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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "programs.h"
#include "callback.h"
#include "regs.h"
#include "support.h"
#include "cross.h"

Bitu call_program;

/* This registers a file on the virtual drive and creates the correct structure for it*/

static Bit8u exe_block[]={
	0xbc,0x00,0x03,					//MOV SP,0x300 decrease stack size
	0xbb,0x30,0x00,					//MOV BX,0x030 for memory resize
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
	/* First get the current psp */
	PROGRAM_Info * info=new PROGRAM_Info;
	info->psp_seg=dos.psp;
	MEM_BlockRead(PhysMake(dos.psp,0),&info->psp_copy,sizeof(sPSP));
	/* Get the file name cmd_line 0 */
	PhysPt envblock=PhysMake(info->psp_copy.environment,0);
	do {} while (mem_readw(envblock++));
	envblock+=3;
	MEM_StrCopy(envblock,info->full_name,32);
	info->psp_copy.cmdtail.buffer[info->psp_copy.cmdtail.count]=0;
	info->cmd_line=info->psp_copy.cmdtail.buffer;
	/* Find the program handler somewhere reference in the file */
	Bit16u handle;
	DOS_OpenFile(info->full_name,0,&handle);
	Bit32u pos=sizeof(PROGRAMS_Main *);
	DOS_SeekFile(handle,&pos,DOS_SEEK_END);
	PROGRAMS_Main * handler;
	Bit16u size=sizeof(PROGRAMS_Main *);
	DOS_ReadFile(handle,(Bit8u *)&handler,&size);
	DOS_CloseFile(handle);
	(*handler)(info);
	free(info);
	return CBRET_NONE;
};


/* Main functions used in all program */


Program::Program(PROGRAM_Info * program_info) {
	prog_info=program_info;
}

void Program::WriteOut(char * format,...) {
	char buf[1024];
	va_list msg;
	
	va_start(msg,format);
	vsprintf(buf,format,msg);
	va_end(msg);

	Bit16u size=strlen(buf);
	DOS_WriteFile(STDOUT,(Bit8u *)buf,&size);
}


char * Program::GetEnvStr(char * env_str) {
	/* Walk through the internal environment and see for a match */
/* Taking some short cuts here to not fuck around with memory structure */

	char * envstart=(char *)HostMake(prog_info->psp_copy.environment,0);
	size_t len=strlen(env_str);
	while (*envstart) {
		if (strncasecmp(env_str,envstart,len)==0 && envstart[len]=='=') {
				return envstart;
		}
		envstart+=strlen(envstart)+1;	
	}
	return 0;
};

char * Program::GetEnvNum(Bit32u num) {
	char * envstart=(char *)HostMake(prog_info->psp_copy.environment,0);
	while (*envstart) {
		if (!num) return envstart;
		envstart+=strlen(envstart)+1;	
		num--;
	}
	return 0;
}

Bit32u Program::GetEnvCount(void) {
	char * envstart=(char *)HostMake(prog_info->psp_copy.environment,0);
	Bit32u num=0;
	while (*envstart) {
		envstart+=strlen(envstart)+1;	
		num++;
	}
	return num;
}

bool Program::SetEnv(char * env_entry,char * new_string) {
	MCB * env_mcb=(MCB *)HostMake(prog_info->psp_copy.environment-1,0);
	upcase(env_entry);
	Bit32u env_size=env_mcb->size*16;
	if (!env_size) E_Exit("SHELL:Illegal environment size");
	/* First try to find the old entry */
	size_t len=strlen(env_entry);
	char * envstart=(char *)HostMake(prog_info->psp_copy.environment,0);
	while (*envstart) {
		if (strncasecmp(env_entry,envstart,len)==0 && envstart[len]=='=') {
			/* Now remove this entry */
			memmove(envstart,envstart+strlen(envstart)+1,env_size);
		} else {
			envstart+=strlen(envstart)+1;
			env_size-=(strlen(envstart)+1);
		}
	}
	/* Now add the string if there is space available */
	if (env_size<(strlen(env_entry)+strlen(new_string)+2)) return false;
	if (!*new_string) return true;
	sprintf(envstart,"%s=%s",env_entry,new_string);
	envstart+=strlen(envstart)+1;
	*envstart++=0;*envstart++=0;*envstart++=0;
	return true;
}

//TODO Hash table :)


void PROGRAMS_Init(Section* sec) {
	/* Setup a special callback to start virtual programs */
	call_program=CALLBACK_Allocate();
	CALLBACK_Setup(call_program,&PROGRAMS_Handler,CB_RETF);
}
