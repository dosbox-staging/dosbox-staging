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

#ifndef __PROGRAM_H
#define __PROGRAM_H
#include <dosbox.h>
#include <dos_inc.h>


char * MSG_Get(char * msg);

struct PROGRAM_Info {
	Bit16u psp_seg;
	sPSP psp_copy;
	char full_name[32];						//Enough space for programs only on the z:\ drive
	char * cmd_line;
};

typedef void (PROGRAMS_Main)(PROGRAM_Info * info);
void PROGRAMS_MakeFile(char * name,PROGRAMS_Main * main);

class Program {
public:
	Program(PROGRAM_Info * program_info);
	virtual void Run(void)=0;
	char * GetEnvStr(char * env_entry);
	char * GetEnvNum(Bit32u num);
	Bit32u GetEnvCount(void);
	bool SetEnv(char * env_entry,char * new_string);
	void WriteOut(char * format,...);					/* Write to standard output */
	PROGRAM_Info * prog_info;

};

void SHELL_AddAutoexec(char * line,...);






#endif

