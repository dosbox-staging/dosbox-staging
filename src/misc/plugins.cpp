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

#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "dosbox.h"
#include "regs.h"
#include "mem.h"
#include "inout.h"
#include "pic.h"
#include "modules.h"
#include "programs.h"
#include "timer.h"
#include "dma.h"
#include "mixer.h"


struct PLUGIN_Function {
	char * name;
	void * function;
};

struct PLUGIN_Module {
#ifdef WIN32
	HINSTANCE handle;
#endif
	
	PLUGIN_Module * next;
};


static PLUGIN_Function functions[]={
"IO_RegisterReadHandler",		(void *)IO_RegisterReadHandler,
"IO_RegisterWriteHandler",		(void *)IO_RegisterWriteHandler,
"IO_FreeReadHandler",			(void *)IO_FreeReadHandler,
"IO_FreeWriteHandler",			(void *)IO_FreeWriteHandler,

"IRQ_RegisterEOIHandler",		(void *)PIC_RegisterIRQ,
"IRQ_FreeEOIHandler",			(void *)PIC_FreeIRQ,
"IRQ_Activate",					(void *)PIC_ActivateIRQ,
"IRQ_Deactivate",				(void *)PIC_DeActivateIRQ,

"TIMER_RegisterMicroHandler",	(void *)TIMER_RegisterMicroHandler,
"TIMER_RegisterTickHandler",	(void *)TIMER_RegisterTickHandler,

"DMA_8_Read",					(void *)DMA_8_Read,
"DMA_16_Read",					(void *)DMA_16_Read,

"DMA_8_Write",					(void *)DMA_8_Write,
"DMA_16_Write",					(void *)DMA_16_Write,

"MIXER_AddChannel",				(void *)MIXER_AddChannel,
"MIXER_SetVolume",				(void *)MIXER_SetVolume,
"MIXER_SetFreq",				(void *)MIXER_SetFreq,
"MIXER_SetMode",				(void *)MIXER_SetMode,
"MIXER_Enable",					(void *)MIXER_Enable,

0,0
};


class PLUGIN : public Program {
public:
	PLUGIN(PROGRAM_Info * program_info);
	void Run(void);
};

PLUGIN::PLUGIN(PROGRAM_Info * info):Program(info) {

}


void PLUGIN::Run(void) {

}



static void PLUGIN_ProgramStart(PROGRAM_Info * info) {
	PLUGIN * tempPLUGIN=new PLUGIN(info);
	tempPLUGIN->Run();
	delete tempPLUGIN;
}



bool PLUGIN_FindFunction(char * name,void * * function) {
/* Run through table and hope to find a match */
	Bitu index=0;
	while (functions[index].name) {
		if (strcmp(functions[index].name,name)==0) {
			*function=functions[index].function;
			return true;
		};
		index++;
	}
	return false;
}


bool PLUGIN_LoadModule(char * name) {
	MODULE_StartHandler starter;

#ifdef WIN32
	HMODULE module;
	module=LoadLibrary(name);
	if (!module) return false;
/* Look for the module start functions */
	FARPROC address;
	address=GetProcAddress(module,MODULE_START_PROC);
	starter=(MODULE_StartHandler)address;
#else 
//TODO LINUX




#endif
	starter(PLUGIN_FindFunction);
return false;

}



void PLUGIN_Init(void) {
	PROGRAMS_MakeFile("PLUGIN.COM",PLUGIN_ProgramStart);
//	PLUGIN_LoadModule("c:\\dosbox\\testmod.dll");
};










