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
#include <unistd.h>
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

//The whole load of startups for all the subfunctions
void MSG_Init(void);
void MEM_Init(void);
void VGA_Init(void);
void CALLBACK_Init();
void DOS_Init();
void RENDER_Init(void);

void CPU_Init();
//void FPU_Init();
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
	GFX_Events();
	TIMER_CheckPIT();
	PIC_runIRQs();
	Bitu ret;
	do {
		ret=(*cpudecoder)(cpu_cycles);
	} while (!ret && TimerAgain);
	return ret;
};


void DOSBOX_SetLoop(LoopHandler * handler) {
	loop=handler;
}

void DOSBOX_SetNormalLoop() {
	loop=Normal_Loop;
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
#if C_FPU
	FPU_Init();
#endif
	MIXER_Init();
#if C_DEBUG
	DEBUG_Init();
#endif

	LastTicks=GetTicks();
	DOSBOX_SetLoop(&Normal_Loop);

	//Start up individual hardware
	DMA_Init();
	PIC_Init();
	VGA_Init();
	KEYBOARD_Init();
	MOUSE_Init();
	JOYSTICK_Init();
	SBLASTER_Init();
//	TANDY_Init();
//	PCSPEAKER_Init();
	ADLIB_Init();
//	CMS_Init();

	PLUGIN_Init();
/* Most of the interrupt handlers */
	BIOS_Init();
	DOS_Init();
	EMS_Init();		//Needs dos first
	XMS_Init();		//Needs dos first
}


void DOSBOX_Init(int argc, char* argv[]) {
/* Find the base directory */
	SHELL_AddAutoexec("SET PATH=Z:\\");
	SHELL_AddAutoexec("SET COMSPEC=Z:\\COMMAND.COM");
    strcpy(dosbox_basedir,argv[0]);
	char * last=strrchr(dosbox_basedir,CROSS_FILESPLIT); //if windowsversion fails: 
    if (!last){     
            getcwd(dosbox_basedir,CROSS_LEN);
            char a[2];
            a[0]=CROSS_FILESPLIT;
            a[1]='\0';
            strcat(dosbox_basedir,a);
    } else {
	*++last=0;
    }
	/* Parse the command line with a setup function */
	int argl=1;
	if (argc>1) {
		if (*argv[1]!='-') { 
                        char mount_buffer[CROSS_LEN];
                        if( (*argv[1]=='/') || 
                            (*argv[1]=='.') || 
                            (*argv[1]=='~') ||
                            (*(argv[1]+1) ==':') ) { 
	                     strcpy(mount_buffer,argv[1]); 
		        } else {
	                    getcwd(mount_buffer,CROSS_LEN);
			 strcat(mount_buffer,argv[1]);
	                };
	   
		
			struct stat test;
			if (stat(mount_buffer,&test)) {
				E_Exit("%s Doesn't exist",mount_buffer);
			}
			/* Not a switch so a normal directory/file */
			if (test.st_mode & S_IFDIR) { 
				SHELL_AddAutoexec("MOUNT C %s",mount_buffer);
				SHELL_AddAutoexec("C:");
			} else {
				char * name=strrchr(argv[1],CROSS_FILESPLIT);
				if (!name) E_Exit("This is weird %s",mount_buffer);
				*name++=0;
				if (access(argv[1],F_OK)) E_Exit("Illegal Directory %s",mount_buffer);
				SHELL_AddAutoexec("MOUNT C %s",mount_buffer);
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
	SHELL_Init();
};
