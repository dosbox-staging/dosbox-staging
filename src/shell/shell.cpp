/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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

/* $Id: shell.cpp,v 1.34 2003-09-21 12:16:02 qbix79 Exp $ */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "setup.h"
#include "shell_inc.h"

Bitu call_shellstop;

static Bitu shellstop_handler(void) {
	return CBRET_STOP;
}

static void SHELL_ProgramStart(Program * * make) {
	*make=new DOS_Shell;
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

DOS_Shell::DOS_Shell():Program(){
	input_handle=STDIN;
	echo=true;
	exit=false;
	bf=0;
	call=false;
	completion_start = NULL;
}




Bitu DOS_Shell::GetRedirection(char *s, char **ifn, char **ofn,bool * append) {

	char * lr=s;
	char * lw=s;
	char ch;
	Bitu num=0;

	while (ch=*lr++) {
		switch (ch) {
		case '>':
			*append=(*lr)=='>';
			if (append) lr++;
			lr=ltrim(lr);
			if (*ofn) free(*ofn);
			*ofn=lr;
			while (*lr && *lr!=' ') lr++;
			*lr=0;
			*ofn=strdup(*ofn);
			continue;
		case '<':
			if (*ifn) free(*ifn);
			lr=ltrim(lr);
			*ifn=lr;
			while (*lr && *lr!=' ') lr++;
			*lr=0;
			*ifn=strdup(*ifn);
			continue;
		case '|':
			ch=0;
			num++;
		}
		*lw++=ch;
	}
	*lw=0;
	return num;
}	

void DOS_Shell::ParseLine(char * line) {

	/* Check for a leading @ */
 	if (line[0]=='@') line[0]=' ';
	line=trim(line);

#if 1
	/* Do redirection and pipe checks */
	
	char * in=0;
	char * out=0;

	Bit16u old_in,old_out;

	Bitu num=0;		/* Number of commands in this line */
	bool append;

	num = GetRedirection(line,&in, &out,&append);
	if (num>1) LOG_MSG("SHELL:Multiple command on 1 line not supported");
//	if (in || num>1) DOS_DuplicateEntry(0,&old_in);

	if (in) {
		LOG_MSG("SHELL:Redirect input from %s",in);
		DOS_CloseFile(0);
		free(in);
	}
	if (out) {
		LOG_MSG("SHELL:Redirect output to %s",out);
		free(out);
	}
#endif

	DoCommand(line);
	
}



void DOS_Shell::RunInternal(void)
{
	char input_line[CMD_MAXLINE];
	std::string line;
	while(bf && bf->ReadLine(input_line)) 
	{
		if (echo) {
				if (input_line[0]!='@') {
					ShowPrompt();
					WriteOut(input_line);
					WriteOut("\n");
				};
			};
		ParseLine(input_line);
	}
	return;
}


void DOS_Shell::Run(void) {
	char input_line[CMD_MAXLINE];
	std::string line;

	if (cmd->FindStringRemain("/C",line)) {
		strcpy(input_line,line.c_str());
		DOS_Shell temp;
		temp.echo = echo;
		temp.ParseLine(input_line);		//for *.exe *.com  |*.bat creates the bf needed by runinternal;
		temp.RunInternal();				// exits when no bf is found.
		return;
	}
	/* Start a normal shell and check for a first command init */
	WriteOut(MSG_Get("SHELL_STARTUP"));
	if (cmd->FindString("/INIT",line,true)) {
		strcpy(input_line,line.c_str());
		line.erase();
		ParseLine(input_line);
	}
	do {
		if (bf){
			if(bf->ReadLine(input_line)) {
				if (echo) {
					if (input_line[0]!='@') {
                        ShowPrompt();
						WriteOut(input_line);
						WriteOut("\n");
					};
				};
			ParseLine(input_line);
			if (echo) WriteOut("\n");
			}
		} else {
			if (echo) ShowPrompt();
			InputCommand(input_line);
			ParseLine(input_line);
			if (echo) WriteOut("\n");
		}
	} while (!exit);
}

void DOS_Shell::SyntaxError(void) {
	WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
}



void AUTOEXEC_Init(Section * sec) {
    MSG_Add("AUTOEXEC_CONFIGFILE_HELP","Add here the lines you want to execute on startup.\n");
	/* Register a virtual AUOEXEC.BAT file */
	std::string line;
	Section_line * section=static_cast<Section_line *>(sec);
	char * extra=(char *)section->data.c_str();
	if (extra) SHELL_AddAutoexec("%s",extra);
	/* Check to see for extra command line options to be added (before the command specified on commandline) */
	while (control->cmdline->FindString("-c",line,true))
		SHELL_AddAutoexec((char *)line.c_str());
	
	/* Check for the -exit switch which causes dosbox to when the command on the commandline has finished */
	bool addexit = control->cmdline->FindExist("-exit",true);

	/* Check for first command being a directory or file */
	char buffer[CROSS_LEN];
	if (control->cmdline->FindCommand(1,line)) {
		struct stat test;
		strcpy(buffer,line.c_str());
		if (stat(buffer,&test)){
			getcwd(buffer,CROSS_LEN);
			strcat(buffer,line.c_str());
			if (stat(buffer,&test)) goto nomount;
		}
		if (test.st_mode & S_IFDIR) { 
			SHELL_AddAutoexec("MOUNT C \"%s\"",buffer);
			SHELL_AddAutoexec("C:");
		} else {
			char * name=strrchr(buffer,CROSS_FILESPLIT);
			if (!name) goto nomount;
			*name++=0;
			if (access(buffer,F_OK)) goto nomount;
			SHELL_AddAutoexec("MOUNT C \"%s\"",buffer);
			SHELL_AddAutoexec("C:");
			SHELL_AddAutoexec(name);
			if(addexit) SHELL_AddAutoexec("exit");
		}
	}
nomount:
	VFILE_Register("AUTOEXEC.BAT",(Bit8u *)autoexec_data,strlen(autoexec_data));
}

static char * path_string="PATH=Z:\\";
static char * comspec_string="COMSPEC=Z:\\COMMAND.COM";
static char * full_name="Z:\\COMMAND.COM";
static char * init_line="/INIT AUTOEXEC.BAT";

void SHELL_Init() {
	/* Add messages */
	MSG_Add("SHELL_ILLEGAL_PATH","Illegal Path\n");
	MSG_Add("SHELL_CMD_HELP","supported commands are:\n");
	MSG_Add("SHELL_CMD_ECHO_ON","ECHO is on\n");
	MSG_Add("SHELL_CMD_ECHO_OFF","ECHO is off\n");
	MSG_Add("SHELL_ILLEGAL_SWITCH","Illegal switch: %s\n");
	MSG_Add("SHELL_CMD_CHDIR_ERROR","Unable to change to: %s\n");
	MSG_Add("SHELL_CMD_MKDIR_ERROR","Unable to make: %s\n");
	MSG_Add("SHELL_CMD_RMDIR_ERROR","Unable to remove: %s\n");
	MSG_Add("SHELL_CMD_DEL_ERROR","Unable to delete: %s\n");
	MSG_Add("SHELL_SYNTAXERROR","The syntax of the command is incorrect.\n");
	MSG_Add("SHELL_CMD_SET_NOT_SET","Environment variable %s not defined\n");
	MSG_Add("SHELL_CMD_SET_OUT_OF_SPACE","Not enough environment space left.\n");
	MSG_Add("SHELL_CMD_IF_EXIST_MISSING_FILENAME","IF EXIST: Missing filename.\n");
	MSG_Add("SHELL_CMD_IF_ERRORLEVEL_MISSING_NUMBER","IF ERRORLEVEL: Missing number.\n");
	MSG_Add("SHELL_CMD_IF_ERRORLEVEL_INVALID_NUMBER","IF ERRORLEVEL: Invalid number.\n");
	MSG_Add("SHELL_CMD_GOTO_MISSING_LABEL","No label supplied to GOTO command.\n");
	MSG_Add("SHELL_CMD_GOTO_LABEL_NOT_FOUND","GOTO: Label %s not found.\n");
	MSG_Add("SHELL_CMD_FILE_NOT_FOUND","File %s not found.\n");
	MSG_Add("SHELL_CMD_FILE_EXISTS","File %s already exists.\n");
	MSG_Add("SHELL_CMD_DIR_INTRO","Directory of %s.\n");
	MSG_Add("SHELL_CMD_DIR_BYTES_USED","%5d File(s) %17s Bytes\n");
	MSG_Add("SHELL_CMD_DIR_BYTES_FREE","%5d Dir(s)  %17s Bytes free\n");
	MSG_Add("SHELL_EXECUTE_DRIVE_NOT_FOUND","Drive %c does not exist!\n");
	MSG_Add("SHELL_EXECUTE_ILLEGAL_COMMAND","Illegal command: %s.\n");
    MSG_Add("SHELL_CMD_PAUSE","Press any key to continue.\n");
	MSG_Add("SHELL_CMD_PAUSE_HELP","Waits for 1 keystroke to continue.\n");
	MSG_Add("SHELL_CMD_COPY_FAILURE","Copy failure : %s.\n");
	MSG_Add("SHELL_CMD_COPY_SUCCESS","   %d File(s) copied.\n");

	MSG_Add("SHELL_STARTUP","DOSBox Shell v" VERSION "\n"
	   "DOSBox does not run protected mode games!\n"
	   "For supported shell commands type: [33mHELP[0m\n"
	   "For a short introduction type: [33mINTRO[0m\n\n"
	   "For more information read the [31mREADME[0m file in DOSBox directory.\n"
	   "\nHAVE FUN!\nThe DOSBox Team\n\n"
	);

	MSG_Add("SHELL_CMD_CHDIR_HELP","Change Directory.\n");
    MSG_Add("SHELL_CMD_CLS_HELP","Clear screen.\n");
    MSG_Add("SHELL_CMD_DIR_HELP","Directory View.\n");
    MSG_Add("SHELL_CMD_ECHO_HELP","Display messages and enable/disable command echoing.\n");
    MSG_Add("SHELL_CMD_EXIT_HELP","Exit from the shell.\n");
    MSG_Add("SHELL_CMD_HELP_HELP","Show help.\n");
    MSG_Add("SHELL_CMD_MKDIR_HELP","Make Directory.\n");
    MSG_Add("SHELL_CMD_RMDIR_HELP","Remove Directory.\n");
    MSG_Add("SHELL_CMD_SET_HELP","Change environment variables.\n");
    MSG_Add("SHELL_CMD_IF_HELP","Performs conditional processing in batch programs.\n");
    MSG_Add("SHELL_CMD_GOTO_HELP","Jump to a labeled line in a batch script.\n");
    MSG_Add("SHELL_CMD_TYPE_HELP","Display the contents of a text-file.\n");
    MSG_Add("SHELL_CMD_REM_HELP","Add comments in a batch file.\n");
	MSG_Add("SHELL_CMD_NO_WILD","This is a simple version of the command, no wildcards allowed!\n");
	MSG_Add("SHELL_CMD_RENAME_HELP","Renames files.\n");
    MSG_Add("SHELL_CMD_DELETE_HELP","Removes files.\n");
	MSG_Add("SHELL_CMD_COPY_HELP","Copy files.\n");
	MSG_Add("SHELL_CMD_CALL_HELP","Start a batch file from within another batch file.\n");
    /* Regular startup */
	call_shellstop=CALLBACK_Allocate();
	/* Setup the startup CS:IP to kill the last running machine when exitted */
	RealPt newcsip=CALLBACK_RealPointer(call_shellstop);
	SegSet16(cs,RealSeg(newcsip));
	reg_ip=RealOff(newcsip);

	CALLBACK_Setup(call_shellstop,shellstop_handler,CB_IRET);
	PROGRAMS_MakeFile("COMMAND.COM",SHELL_ProgramStart);

	/* Now call up the shell for the first time */
	Bit16u psp_seg=DOS_GetMemory(16+1)+1;
	Bit16u env_seg=DOS_GetMemory(1+(4096/16))+1;
	Bit16u stack_seg=DOS_GetMemory(2048/16);
	SegSet16(ss,stack_seg);
	reg_sp=2046;
	/* Setup MCB and the environment */
	DOS_MCB envmcb((Bit16u)(env_seg-1));
	envmcb.SetPSPSeg(psp_seg);
	envmcb.SetSize(4096/16);
	
	PhysPt env_write=PhysMake(env_seg,0);
	MEM_BlockWrite(env_write,path_string,strlen(path_string)+1);
	env_write+=strlen(path_string)+1;
	MEM_BlockWrite(env_write,comspec_string,strlen(comspec_string)+1);
	env_write+=strlen(comspec_string)+1;
	mem_writeb(env_write++,0);
	mem_writew(env_write,1);
	env_write+=2;
	MEM_BlockWrite(env_write,full_name,strlen(full_name)+1);

	DOS_PSP psp(psp_seg);
	psp.MakeNew(0);
	psp.SetFileHandle(STDIN ,DOS_FindDevice("CON"));
	psp.SetFileHandle(STDOUT,DOS_FindDevice("CON"));
	psp.SetFileHandle(STDERR,DOS_FindDevice("CON"));
	psp.SetFileHandle(STDAUX,DOS_FindDevice("CON"));
	psp.SetFileHandle(STDNUL,DOS_FindDevice("CON"));
	psp.SetFileHandle(STDPRN,DOS_FindDevice("CON"));
	psp.SetParent(psp_seg);
	/* Set the environment */
	psp.SetEnvironment(env_seg);
	/* Set the command line for the shell start up */
	CommandTail tail;
	tail.count=strlen(init_line);
	strcpy(tail.buffer,init_line);
	MEM_BlockWrite(PhysMake(psp_seg,128),&tail,128);
	/* Setup internal DOS Variables */
	dos.dta=psp.GetDTA();
	dos.psp=psp_seg;

	Program * new_program;
	SHELL_ProgramStart(&new_program);
	new_program->Run();
	delete new_program;
}
