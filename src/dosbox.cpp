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
#include "support.h"

 /* NEEDS A CLEANUP */
char dosbox_basedir[CROSS_LEN];
Config * control;

//The whole load of startups for all the subfunctions

void MSG_Init(Section_prop *);
void MEM_Init(Section *);
void IO_Init(Section * );
void CALLBACK_Init(Section*);
void PROGRAMS_Init(Section*);


void VGA_Init(Section*);

void DOS_Init(Section*);


void CPU_Init(Section*);
//void FPU_Init();
void DMA_Init(Section*);
void MIXER_Init(Section*);
void HARDWARE_Init(Section*);


void KEYBOARD_Init(Section*);	//TODO This should setup INT 16 too but ok ;)
void JOYSTICK_Init(Section*);
void MOUSE_Init(Section*);
void SBLASTER_Init(Section*);
void ADLIB_Init(Section*);
void PCSPEAKER_Init(Section*);
void TANDYSOUND_Init(Section*);
void CMS_Init(Section*);

void PIC_Init(Section*);
void TIMER_Init(Section*);
void BIOS_Init(Section*);
void DEBUG_Init(Section*);




/* Dos Internal mostly */
void EMS_Init(Section*);
void XMS_Init(Section*);
void AUTOEXEC_Init(Section*);
void SHELL_Init(void);

void INT10_Init(Section*);




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
		PIC_IRQAgain=false;
		ret=(*cpudecoder)(cpu_cycles);
	} while (!ret && PIC_IRQAgain);
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


static void DOSBOX_RealInit(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	/* Initialize some dosbox internals */
	LastTicks=GetTicks();
	DOSBOX_SetLoop(&Normal_Loop);
	MSG_Init(section);
}


void DOSBOX_Init(void) {
//	Section * sec;
	Section_prop * secprop;
	Section_line * secline;

	/* Setup all the different modules making up DOSBox */
	
	secprop=control->AddSection_prop("DOSBOX",&DOSBOX_RealInit);
	
	secprop->Add_string("LANGUAGE","dosbox.lang");
	secprop->Add_int("WARNINGS",0);
	
	control->AddSection		("MEMORY",&MEM_Init);
	control->AddSection		("IO",&IO_Init);
	control->AddSection		("CALLBACK",&CALLBACK_Init);
    control->AddSection		("PIC",&PIC_Init);
	control->AddSection		("PROGRAMS",&PROGRAMS_Init);
	control->AddSection_prop("TIMER",&TIMER_Init);
	secprop=control->AddSection_prop("CPU",&CPU_Init);
	secprop->Add_int("CYCLES",3000);
	secprop=control->AddSection_prop("MIXER",&MIXER_Init);
#if C_DEBUG
	secprop=control->AddSection_prop("'DEBUG",&DEBUG_Init);
#endif
	control->AddSection		("DMA",&DMA_Init);
	control->AddSection		("VGA",&VGA_Init);
	control->AddSection		("KEYBOARD",&KEYBOARD_Init);
	secprop=control->AddSection_prop("MOUSE",&MOUSE_Init);
	control->AddSection		("JOYSTICK",&JOYSTICK_Init);
	
	secprop=control->AddSection_prop("SBLASTER",&SBLASTER_Init);
    secprop->Add_hex("BASE",0x220);
	secprop->Add_int("IRQ",5);
	secprop->Add_int("DMA",1);
	secprop->Add_bool("STATUS",true);

	secprop=control->AddSection_prop("TANDYSOUND",&TANDYSOUND_Init);
    secprop->Add_bool("STATUS",false);
	
	secprop=control->AddSection_prop("PCSPEAKER",&PCSPEAKER_Init);

	secprop=control->AddSection_prop("ADLIB",&ADLIB_Init);
    secprop->Add_bool("STATUS",true);
	
	secprop=control->AddSection_prop("CMS",&CMS_Init);
    secprop->Add_bool("STATUS",false);

	secprop=control->AddSection_prop("BIOS",&BIOS_Init);
	secprop=control->AddSection_prop("VIDBIOS",&INT10_Init);

	/* All the DOS Related stuff, which will eventually start up in the shell */
	
	secprop=control->AddSection_prop("DOS",&DOS_Init);
	secprop=control->AddSection_prop("EMS",&EMS_Init);
	secprop->Add_bool("STATUS",true);
	secprop->Add_int("SIZE",4);
	secprop=control->AddSection_prop("XMS",&XMS_Init);
	secprop->Add_bool("STATUS",true);
	secprop->Add_int("SIZE",8);

	secline=control->AddSection_line("AUTOEXEC",&AUTOEXEC_Init);

	control->SetStartUp(&SHELL_Init);	

//	HARDWARE_Init();
#if C_FPU
	FPU_Init();
#endif



}


