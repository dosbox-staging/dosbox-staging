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

#include "shell_inc.h"

Bitu call_shellstop;

static Bitu shellstop_handler(void) {
	return CBRET_STOP;
}

static void SHELL_ProgramStart(PROGRAM_Info * info) {
	DOS_Shell * tempshell=new DOS_Shell(info);
	tempshell->Run();
	delete tempshell;
}

#define AUTOEXEC_SIZE 4096
static char autoexec_data[AUTOEXEC_SIZE]={0};

void SHELL_AddAutoexec(char * line,...) {
	char buf[2048];
	va_list msg;
	
	va_start(msg,line);
	vsprintf(buf,line,msg);
	va_end(msg);

	size_t auto_len=strlen(autoexec_data);
	if ((auto_len+strlen(line)+3)>AUTOEXEC_SIZE) {
		E_Exit("SYSTEM:Autoexec.bat file overlow");
	}
	sprintf((autoexec_data+auto_len),"%s\r\n",buf);
}


DOS_Shell::DOS_Shell(PROGRAM_Info * info):Program(info) {
	input_handle=STDIN;
	echo=true;
	exit=false;
	bf=0;
	memset(&old.buffer,0,CMD_OLDSIZE);
	old.size=0;
}




Bit32u DOS_Shell::GetRedirection(char *s, char **ifn, char **ofn) {


	return 1;
}
	


void DOS_Shell::ParseLine(char * line) {

	char * in=0;
	char * out=0;
	char * fname0=0;
	char * fname1=0;

	/* Check for a leading @ */
	if (line[0]=='@') line[0]=' ';
	line=trim(line);
	Bit32u num=0;		/* Number of commands in this line */

	num = GetRedirection(line,&in, &out);

/* TODO in and out redirection */
        
	DoCommand(line);
	
}



void DOS_Shell::Run(void) {
	/* Check for a direct Command */
	if (strncasecmp(prog_info->cmd_line,"/C ",3)==0) {
		ParseLine(prog_info->cmd_line+3);
		return;
	}
	/* Start a normal shell and check for a first command init */
	WriteOut(MSG_Get("SHELL_STARTUP"));
	char input_line[CMD_MAXLINE];
	if (strncasecmp(prog_info->cmd_line,"/INIT ",6)==0) {
		ParseLine(prog_info->cmd_line+6);
	}
	do {
		if (bf && bf->ReadLine(input_line)) {
			if (echo) {
				if (input_line[0]!='@') {
					ShowPrompt();
					WriteOut(input_line);
					WriteOut("\n");
				};
			};
		} else {
			if (echo) ShowPrompt();
			InputCommand(input_line);

		}
		ParseLine(input_line);
		if (echo) WriteOut("\n");
	} while (!exit);
}

void DOS_Shell::SyntaxError(void) {
	WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
}




void SHELL_Init() {
	call_shellstop=CALLBACK_Allocate();
	CALLBACK_Setup(call_shellstop,shellstop_handler,CB_IRET);
	PROGRAMS_MakeFile("COMMAND.COM",SHELL_ProgramStart);
	/* Now call up the shell for the first time */
	Bit16u psp_seg=DOS_GetMemory(16);
	Bit16u env_seg=DOS_GetMemory(1+(4096/16));
	Bit16u stack_seg=DOS_GetMemory(2048/16);
	SegSet16(ss,stack_seg);
	reg_sp=2046;
	/* Setup a fake MCB for the environment */
	MCB * env_mcb=(MCB *)HostMake(env_seg,0);
	env_mcb->psp_segment=psp_seg;
	env_mcb->size=4096/16;
	real_writed(env_seg+1,0,0);

	PSP * psp=(PSP *)HostMake(psp_seg,0);
	Bit32u i;
	for (i=0;i<20;i++) psp->files[i]=0xff;
	psp->files[STDIN]=DOS_FindDevice("CON");
	psp->files[STDOUT]=DOS_FindDevice("CON");
	psp->files[STDERR]=DOS_FindDevice("CON");
	psp->files[STDAUX]=DOS_FindDevice("CON");
	psp->files[STDNUL]=DOS_FindDevice("CON");
	psp->files[STDPRN]=DOS_FindDevice("CON");
	psp->max_files=20;
	psp->file_table=RealMake(psp_seg,offsetof(PSP,files));
	/* Save old DTA in psp */
	psp->dta=dos.dta;
	/* Set the environment and clear it */
	psp->environment=env_seg+1;
	mem_writew(Real2Phys(RealMake(env_seg+1,0)),0);
	/* Setup internal DOS Variables */
	dos.dta=RealMake(psp_seg,0x80);
	dos.psp=psp_seg;
	PROGRAM_Info info;
	strcpy(info.full_name,"Z:\\COMMAND.COM");
	info.psp_seg=psp_seg;
	MEM_BlockRead(PhysMake(dos.psp,0),&info.psp_copy,sizeof(PSP));
	char line[256];
	strcpy(line,"/INIT Z:\\AUTOEXEC.BAT");
	info.cmd_line=line;

/* Handle the last AUTOEXEC.BAT Setup stuff */
	VFILE_Register("AUTOEXEC.BAT",(Bit8u *)autoexec_data,strlen(autoexec_data));
	SHELL_ProgramStart(&info);
}
