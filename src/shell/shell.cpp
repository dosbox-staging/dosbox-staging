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

/* $Id: shell.cpp,v 1.54 2005-01-14 19:38:19 qbix79 Exp $ */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "setup.h"
#include "shell.h"

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
		E_Exit("SYSTEM:Autoexec.bat file overflow");
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

	while ( (ch=*lr++) ) {
		switch (ch) {
		case '>':
			*append=((*lr)=='>');
			if (*append) lr++;
			lr=ltrim(lr);
			if (*ofn) free(*ofn);
			*ofn=lr;
			while (*lr && *lr!=' ') lr++;
			if(*lr && *(lr+1)) 
				*lr++=0; 
			else 
				*lr=0;
			*ofn=strdup(*ofn);
			continue;
		case '<':
			if (*ifn) free(*ifn);
			lr=ltrim(lr);
			*ifn=lr;
			while (*lr && *lr!=' ') lr++;
			if(*lr && *(lr+1)) 
				*lr++=0; 
			else 
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
	LOG(LOG_EXEC,LOG_ERROR)("Parsing command line: %s",line);
	/* Check for a leading @ */
 	if (line[0]=='@') line[0]=' ';
	line=trim(line);

	/* Do redirection and pipe checks */
	
	char * in=0;
	char * out=0;

	Bit16u old_in,old_out;
	Bit16u dummy,dummy2;
	Bitu num=0;		/* Number of commands in this line */
	bool append;

	num = GetRedirection(line,&in, &out,&append);
	if (num>1) LOG_MSG("SHELL:Multiple command on 1 line not supported");
//	if (in || num>1) DOS_DuplicateEntry(0,&old_in);

	if (in) {
		if(DOS_OpenFile(in,0,&dummy)) { //Test if file exists
			DOS_CloseFile(dummy);
			LOG_MSG("SHELL:Redirect input from %s",in);
			DOS_CloseFile(0); //Close stdin
			DOS_OpenFile(in,0,&dummy);//Open new stdin
		}
	}
	if (out){
		LOG_MSG("SHELL:Redirect output to %s",out);
		DOS_CloseFile(1);
		/* Create if not exist. Open if exist. Both in read/write mode */
		if(!DOS_OpenFileExtended(out,2,2,0x11,&dummy,&dummy2))
			DOS_OpenFile("con",2,&dummy); //Read only file, open con again
	}
	/* Run the actual command */
	DoCommand(line);
	/* Restore handles */
	if(in) {
		DOS_CloseFile(0);
		DOS_OpenFile("con",2,&dummy);
		free(in);
	}
	if(out) {   
		DOS_CloseFile(1);
		DOS_OpenFile("con",2,&dummy);
		free(out);
	}
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
			upcase(name);
			if(strstr(name,".BAT")==0) {
				SHELL_AddAutoexec(name);
			} else {
				char call[CROSS_LEN] = { 0 };
				strcpy(call,"CALL ");
				strcat(call,name);
				SHELL_AddAutoexec(call);
			}
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
	MSG_Add("SHELL_ILLEGAL_PATH","Illegal Path.\n");
	MSG_Add("SHELL_CMD_HELP","supported commands are:\n");
	MSG_Add("SHELL_CMD_ECHO_ON","ECHO is on.\n");
	MSG_Add("SHELL_CMD_ECHO_OFF","ECHO is off.\n");
	MSG_Add("SHELL_ILLEGAL_SWITCH","Illegal switch: %s.\n");
	MSG_Add("SHELL_CMD_CHDIR_ERROR","Unable to change to: %s.\n");
	MSG_Add("SHELL_CMD_MKDIR_ERROR","Unable to make: %s.\n");
	MSG_Add("SHELL_CMD_RMDIR_ERROR","Unable to remove: %s.\n");
	MSG_Add("SHELL_CMD_DEL_ERROR","Unable to delete: %s.\n");
	MSG_Add("SHELL_SYNTAXERROR","The syntax of the command is incorrect.\n");
	MSG_Add("SHELL_CMD_SET_NOT_SET","Environment variable %s not defined.\n");
	MSG_Add("SHELL_CMD_SET_OUT_OF_SPACE","Not enough environment space left.\n");
	MSG_Add("SHELL_CMD_IF_EXIST_MISSING_FILENAME","IF EXIST: Missing filename.\n");
	MSG_Add("SHELL_CMD_IF_ERRORLEVEL_MISSING_NUMBER","IF ERRORLEVEL: Missing number.\n");
	MSG_Add("SHELL_CMD_IF_ERRORLEVEL_INVALID_NUMBER","IF ERRORLEVEL: Invalid number.\n");
	MSG_Add("SHELL_CMD_GOTO_MISSING_LABEL","No label supplied to GOTO command.\n");
	MSG_Add("SHELL_CMD_GOTO_LABEL_NOT_FOUND","GOTO: Label %s not found.\n");
	MSG_Add("SHELL_CMD_FILE_NOT_FOUND","File %s not found.\n");
	MSG_Add("SHELL_CMD_FILE_EXISTS","File %s already exists.\n");
	MSG_Add("SHELL_CMD_DIR_INTRO","Directory of %s.\n");
	MSG_Add("SHELL_CMD_DIR_BYTES_USED","%5d File(s) %17s Bytes.\n");
	MSG_Add("SHELL_CMD_DIR_BYTES_FREE","%5d Dir(s)  %17s Bytes free.\n");
	MSG_Add("SHELL_EXECUTE_DRIVE_NOT_FOUND","Drive %c does not exist!\nYou must \033[31mmount\033[0m it first. Type \033[1;33mintro\033[0m for more information.\n");
	MSG_Add("SHELL_EXECUTE_ILLEGAL_COMMAND","Illegal command: %s.\n");
	MSG_Add("SHELL_CMD_PAUSE","Press any key to continue.\n");
	MSG_Add("SHELL_CMD_PAUSE_HELP","Waits for 1 keystroke to continue.\n");
	MSG_Add("SHELL_CMD_COPY_FAILURE","Copy failure : %s.\n");
	MSG_Add("SHELL_CMD_COPY_SUCCESS","   %d File(s) copied.\n");
	MSG_Add("SHELL_CMD_SUBST_NO_REMOVE","Removing drive not supported. Doing nothing.\n");
	MSG_Add("SHELL_CMD_SUBST_FAILURE","SUBST failed. You either made an error in your commandline or the target drive is already used.\nIt's only possible to use SUBST on Local drives");

	MSG_Add("SHELL_STARTUP",
		"\033[44;1m\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n"
		"\xBA \033[32mDOSBox Shell v" VERSION "\033[37m                                                 \xBA\n"
		"\xBA DOSBox runs real and protected mode games.                         \xBA\n"
		"\xBA For supported shell commands type: \033[33mHELP\033[37m                            \xBA\n"
		"\xBA For a short introduction type: \033[33mINTRO\033[37m                               \xBA\n"
		"\xBA                                                                    \xBA\n"
		"\xBA If you want more speed, try \033[31mctrl-F8\033[37m and \033[31mctrl-F12\033[37m.                  \xBA\n"
		"\xBA To activate the keymapper \033[31mctrl-F1\033[37m.                                 \xBA\n"
		"\xBA For more information read the \033[36mREADME\033[37m file in the DOSBox directory. \xBA\n"
		"\xBA                                                                    \xBA\n"
#if C_DEBUG
		"\xBA Press \033[31mPause\033[37m to enter the debugger or start the exe with \033[33mDEBUG\033[37m.     \xBA\n"
		"\xBA                                                                    \xBA\n"
#endif
		"\xBA \033[32mHAVE FUN!\033[37m                                                          \xBA\n"
		"\xBA \033[32mThe DOSBox Team\033[37m                                                    \xBA\n"
		"\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\033[0m\n"
		//"\n" //Breaks the startup message if you type a mount and a drive change.
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
	MSG_Add("SHELL_CMD_SUBST_HELP","Assign an internal directory to a drive.\n");
	MSG_Add("SHELL_CMD_LOADHIGH_HELP","Run a program. For batch file compatibility only.\n");
	MSG_Add("SHELL_CMD_CHOICE_HELP","Waits for a keypress and sets ERRORLEVEL.\n");
	MSG_Add("SHELL_CMD_ATTRIB_HELP","Does nothing. Provided for compatibility.\n");
	MSG_Add("SHELL_CMD_PATH_HELP","Provided for compatibility.\n");

	/* Regular startup */
	call_shellstop=CALLBACK_Allocate();
	/* Setup the startup CS:IP to kill the last running machine when exitted */
	RealPt newcsip=CALLBACK_RealPointer(call_shellstop);
	SegSet16(cs,RealSeg(newcsip));
	reg_ip=RealOff(newcsip);

	CALLBACK_Setup(call_shellstop,shellstop_handler,CB_IRET,"shell stop");
	PROGRAMS_MakeFile("COMMAND.COM",SHELL_ProgramStart);

	/* Now call up the shell for the first time */
	Bit16u psp_seg=DOS_GetMemory(16+3)+1;
	Bit16u env_seg=DOS_GetMemory(1+(4096/16))+1;
	Bit16u stack_seg=DOS_GetMemory(2048/16);
	SegSet16(ss,stack_seg);
	reg_sp=2046;

	/* Set up int 24 and psp (Telarium games) */
	real_writeb(psp_seg+16+1,0,0xea);		/* far jmp */
	real_writed(psp_seg+16+1,1,real_readd(0,0x24*4));
	real_writed(0,0x24*4,((Bit32u)psp_seg<<16) | ((16+1)<<4));

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
	dos.psp(psp_seg);
	Bit16u dummy=0;
	DOS_OpenFile("CON",2,&dummy);/* STDIN  */
	DOS_OpenFile("CON",2,&dummy);/* STDOUT */
	DOS_OpenFile("CON",2,&dummy);/* STDERR */
	DOS_OpenFile("CON",2,&dummy);/* STDAUX */
	DOS_OpenFile("CON",2,&dummy);/* STDPRN */
	psp.SetParent(psp_seg);
	/* Set the environment */
	psp.SetEnvironment(env_seg);
	/* Set the command line for the shell start up */
	CommandTail tail;
	tail.count=strlen(init_line);
	strcpy(tail.buffer,init_line);
	MEM_BlockWrite(PhysMake(psp_seg,128),&tail,128);
	/* Setup internal DOS Variables */

	dos.dta(RealMake(psp_seg,0x80));
	dos.psp(psp_seg);

	Program * new_program;
	SHELL_ProgramStart(&new_program);
	new_program->Run();
	delete new_program;
}
