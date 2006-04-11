/*
 *  Copyright (C) 2002-2006  The DOSBox Team
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

#ifndef DOSBOX_PROGRAMS_H
#define DOSBOX_PROGRAMS_H

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif
#ifndef DOSBOX_DOS_INC_H
#include "dos_inc.h"
#endif
#ifndef DOSBOX_SETUP_H
#include "setup.h"
#endif


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
	bool GetEnvStr(const char * entry,std::string & result);
	bool GetEnvNum(Bitu num,std::string & result);
	Bitu GetEnvCount(void);
	bool SetEnv(const char * entry,const char * new_string);
	void WriteOut(const char * format,...);				/* Write to standard output */

};

typedef void (PROGRAMS_Main)(Program * * make);
void PROGRAMS_MakeFile(char const * const name,PROGRAMS_Main * main);

#endif
