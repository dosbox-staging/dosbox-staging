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
#include "dosbox.h"
#include "dos_inc.h"
#include "setup.h"



class Program;

typedef void (PROGRAMS_Main)(Program * * make);
void PROGRAMS_MakeFile(char * name,PROGRAMS_Main * main);

class Program {
public:
	Program();
	virtual ~Program(){
		delete cmd;
		delete psp;
	}
	std::string temp_line;
	CommandLine * cmd;
	DOS_PSP * psp;
	virtual void Run(void)=0;
	bool Program::GetEnvStr(const char * entry,std::string & result);
	bool GetEnvNum(Bitu num,std::string & result);
	Bitu GetEnvCount(void);
	bool SetEnv(const char * entry,const char * new_string);
	void WriteOut(const char * format,...);					/* Write to standard output */

};

void SHELL_AddAutoexec(char * line,...);






#endif

