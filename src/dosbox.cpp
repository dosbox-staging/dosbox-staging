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
#include "dosbox.h"
#include "debug.h"
#include "cpu.h"
#include "video.h"
#include "pic.h"
#include "cpu.h"
#include "callback.h"
#include "inout.h"
#include "mixer.h"
#include "timer.h"
#include "dos_inc.h"
#include "setup.h"
#include "cross.h"
#include "programs.h"

char dosbox_basedir[CROSS_LEN];


#if 0

int main(int argc, char* argv[]) {

	

	/* Strip out the dosbox startup directory */
	
	
	
	/* Handle the command line for new stuff to add to autoexec.bat */
	int argl=1;
	if (argc>1) {
		if (*argv[1]!='-') {
			struct stat test;
			if (stat(argv[1],&test)) {
				E_Exit("%s Doesn't exist",argv[1]);
			}
			/* Not a switch so a normal directory/file */
			if (test.st_mode & S_IFDIR) { 
				SHELL_AddAutoexec("MOUNT C %s",argv[1]);
				SHELL_AddAutoexec("C:");
			} else {
				char * name=strrchr(argv[1],CROSS_FILESPLIT);
				if (!name) E_Exit("This is weird %s",argv[1]);
				*name++=0;
				if (access(argv[1],F_OK)) E_Exit("Illegal Directory %s",argv[1]);
				SHELL_AddAutoexec("MOUNT C %s",argv[1]);
				SHELL_AddAutoexec("C:");
				SHELL_AddAutoexec(name);
			}
			argl++;
		}
	}
	bool sw_c=false;
	while (argl<argc) {
		if (*argv[argl]=='-') {
			sw_c=false;
			if (strcmp(argv[argl],"-c")==0) sw_c=true;
			else E_Exit("Illegal switch %s",argv[argl]);
			argl++;
			continue;
		}
			SHELL_AddAutoexec(argv[argl]);
		if (sw_c) {
		}
		argl++;
	}

	SysShutDown();

	SDL_Quit();
	return 0;
}

#endif

//The whole load of startups for all the subfunctions
void MSG_Init(void);
void MEM_Init(void);
void VGA_Init(void);
void CALLBACK_Init();
void DOS_Init();
void RENDER_Init(void);

void CPU_Init();
void FPU_Init();
void IO_Init(void);
void DMA_Init(void);
void MIXER_Init(void);
void HARDWARE_Init(void);


void KEYBOARD_Init(void);	//TODO This should setup INT 16 too but ok ;)
void JOYSTICK_Init(void);
void MOUSE_Init(void);
void SBLASTER_Init(void);
void ADLIB_Init(void);
void PCSPEAKER_Init(void);
void TANDY_Init(void);
void CMS_Init(void);

void PIC_Init(void);
void TIMER_Init(void);
void BIOS_Init(void);
void DEBUG_Init(void);


void PLUGIN_Init(void);

/* Dos Internal mostly */
void EMS_Init(void);
void XMS_Init(void);
void PROGRAMS_Init(void);
void SHELL_Init(void);


void CALLBACK_ShutDown(void);
void PIC_ShutDown(void);
void KEYBOARD_ShutDown(void);
void CPU_ShutDown(void);
void VGA_ShutDown(void);
void BIOS_ShutDown(void);
void MEMORY_ShutDown(void);




Bit32u LastTicks;
static LoopHandler * loop;

static Bitu Normal_Loop(void) {
	Bit32u new_ticks;
	new_ticks=GetTicks();
	if (new_ticks>LastTicks) {
		Bit32u ticks=new_ticks-LastTicks;
		if (ticks>20) ticks=20;
		LastTicks=new_ticks;
		TIMER_AddTicks(ticks);
	}
	TIMER_CheckPIT();
	GFX_Events();
	PIC_runIRQs();
	return (*cpudecoder)(cpu_cycles);
};


static Bitu Speed_Loop(void) {
	Bit32u new_ticks;
	new_ticks=GetTicks();
	Bitu ret=0;
	Bitu cycles=1;
	if (new_ticks>LastTicks) {
		Bit32u ticks=new_ticks-LastTicks;
		if (ticks>20) ticks=20;
//		if (ticks>3) LOG_DEBUG("Ticks %d",ticks);
		LastTicks=new_ticks;
		TIMER_AddTicks(ticks);
		cycles+=cpu_cycles*ticks;
	}
	TIMER_CheckPIT();
	GFX_Events();
	PIC_runIRQs();
	return (*cpudecoder)(cycles);
}

void DOSBOX_SetLoop(LoopHandler * handler) {
	loop=handler;
}


void DOSBOX_RunMachine(void){
	Bitu ret;
	do {
		ret=(*loop)();
	} while (!ret);
}



static void InitSystems(void) {
	MSG_Init();
	MEM_Init();
	IO_Init();
	CALLBACK_Init();
	PROGRAMS_Init();
	HARDWARE_Init();
	TIMER_Init();
	CPU_Init();
#ifdef C_FPU
	FPU_Init();
#endif
	MIXER_Init();
#ifdef C_DEBUG
	DEBUG_Init();
#endif
	//Start up individual hardware
	DMA_Init();
	PIC_Init();
	VGA_Init();
	KEYBOARD_Init();
	MOUSE_Init();
	JOYSTICK_Init();
	SBLASTER_Init();
	TANDY_Init();
	PCSPEAKER_Init();
	ADLIB_Init();
	CMS_Init();

	PLUGIN_Init();
/* Most of teh interrupt handlers */
	BIOS_Init();
	DOS_Init();
	EMS_Init();		//Needs dos first
	XMS_Init();		//Needs dos first

/* Setup the normal system loop */
	LastTicks=GetTicks();
	DOSBOX_SetLoop(&Normal_Loop);
//	DOSBOX_SetLoop(&Speed_Loop);
}


void DOSBOX_Init(int argc, char* argv[]) {
/* Find the base directory */
	strcpy(dosbox_basedir,argv[0]);
	char * last=strrchr(dosbox_basedir,CROSS_FILESPLIT); //if windowsversion fails: 
	if (!last) E_Exit("Can't find basedir %s", argv[0]);
	*++last=0;
	/* Parse the command line with a setup function */
	int argl=1;
	if (argc>1) {
		if (*argv[1]!='-') {
			struct stat test;
			if (stat(argv[1],&test)) {
				E_Exit("%s Doesn't exist",argv[1]);
			}
			/* Not a switch so a normal directory/file */
			if (test.st_mode & S_IFDIR) { 
				SHELL_AddAutoexec("MOUNT C %s",argv[1]);
				SHELL_AddAutoexec("C:");
			} else {
				char * name=strrchr(argv[1],CROSS_FILESPLIT);
				if (!name) E_Exit("This is weird %s",argv[1]);
				*name++=0;
				if (access(argv[1],F_OK)) E_Exit("Illegal Directory %s",argv[1]);
				SHELL_AddAutoexec("MOUNT C %s",argv[1]);
				SHELL_AddAutoexec("C:");
				SHELL_AddAutoexec(name);
			}
			argl++;
		}
	}
	bool sw_c=false;
	while (argl<argc) {
		if (*argv[argl]=='-') {
			sw_c=false;
			if (strcmp(argv[argl],"-c")==0) sw_c=true;
			else E_Exit("Illegal switch %s",argv[argl]);
			argl++;
			continue;
		}
			SHELL_AddAutoexec(argv[argl]);
		if (sw_c) {
		}
		argl++;
	}


	
	InitSystems();	
}


void DOSBOX_StartUp(void) {
	SHELL_AddAutoexec("SET PATH=Z:\\");
	SHELL_AddAutoexec("SET COMSPEC=Z:\\COMMAND.COM");
	SHELL_Init();
};
