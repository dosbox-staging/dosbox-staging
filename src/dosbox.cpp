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

Config * control;
Bitu errorlevel=1;

/* The whole load of startups for all the subfunctions */
void MSG_Init(Section_prop *);
void LOG_StartUp(void);
void MEM_Init(Section *);
void IO_Init(Section * );
void CALLBACK_Init(Section*);
void PROGRAMS_Init(Section*);

void RENDER_Init(Section*);
void VGA_Init(Section*);

void DOS_Init(Section*);


void CPU_Init(Section*);
//void FPU_Init();
void DMA_Init(Section*);
void MIXER_Init(Section*);
void MIDI_Init(Section*);
void HARDWARE_Init(Section*);


void KEYBOARD_Init(Section*);	//TODO This should setup INT 16 too but ok ;)
void JOYSTICK_Init(Section*);
void MOUSE_Init(Section*);
void SBLASTER_Init(Section*);
void GUS_Init(Section*);
void MPU401_Init(Section*);
void ADLIB_Init(Section*);
void PCSPEAKER_Init(Section*);
void TANDYSOUND_Init(Section*);
void CMS_Init(Section*);
void DISNEY_Init(Section*);

void PIC_Init(Section*);
void TIMER_Init(Section*);
void BIOS_Init(Section*);
void DEBUG_Init(Section*);
void CMOS_Init(Section*);

void MSCDEX_Init(Section*);

/* Dos Internal mostly */
void EMS_Init(Section*);
void XMS_Init(Section*);
void AUTOEXEC_Init(Section*);
void SHELL_Init(void);

void INT10_Init(Section*);

static LoopHandler * loop;

Bitu RemainTicks;;
Bitu LastTicks;

static Bitu Normal_Loop(void) {
	Bitu ret,NewTicks;
	while (RemainTicks) {
		ret=PIC_RunQueue();
	#if C_DEBUG
		if (DEBUG_ExitLoop()) return 0;	
	#endif
		if (ret) return ret;
		RemainTicks--;
		TIMER_AddTick();
		GFX_Events();
	}
	NewTicks=GetTicks();
	if (NewTicks>LastTicks) {
		RemainTicks+=NewTicks-LastTicks;
		if (RemainTicks>20) {
//			LOG_DEBUG("Ticks to handle overflow %d",RemainTicks);
			RemainTicks=20;
		}
		LastTicks=NewTicks;
	}
	//TODO Make this selectable in the config file, since it gives some lag */
	if (!RemainTicks) {
		SDL_Delay(1);
		return 0;
	}
	return 0;
}

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
	errorlevel=section->Get_int("warnings");
	MSG_Add("DOSBOX_CONFIGFILE_HELP","General Dosbox settings\n");
	RemainTicks=0;LastTicks=GetTicks();
	DOSBOX_SetLoop(&Normal_Loop);
	MSG_Init(section);
}


void DOSBOX_Init(void) {
//	Section * sec;
	Section_prop * secprop;
	Section_line * secline;

	/* Setup all the different modules making up DOSBox */
	
	secprop=control->AddSection_prop("dosbox",&DOSBOX_RealInit);
    secprop->Add_string("language","");
#if C_DEBUG	
	secprop->Add_int("warnings",4);
	LOG_StartUp();
#else 
	secprop->Add_int("warnings",0);
#endif
	
	secprop->AddInitFunction(&MEM_Init);
	secprop->AddInitFunction(&IO_Init);
	secprop->AddInitFunction(&CALLBACK_Init);
	secprop->AddInitFunction(&PIC_Init);
	secprop->AddInitFunction(&PROGRAMS_Init);
	secprop->AddInitFunction(&TIMER_Init);
	secprop->AddInitFunction(&CMOS_Init);
	secprop=control->AddSection_prop("render",&RENDER_Init);
	secprop->Add_int("frameskip",0);
	secprop->Add_bool("keepsmall",false);
	secprop->Add_string("snapshots","snapshots");
	secprop->Add_string("scaler","none");

	secprop=control->AddSection_prop("cpu",&CPU_Init);
	secprop->Add_int("cycles",1800);

	secprop->AddInitFunction(&DMA_Init);
	secprop->AddInitFunction(&VGA_Init);
	secprop->AddInitFunction(&KEYBOARD_Init);
	secprop->AddInitFunction(&MOUSE_Init);
	secprop->AddInitFunction(&JOYSTICK_Init);

	secprop=control->AddSection_prop("mixer",&MIXER_Init);
	secprop=control->AddSection_prop("midi",&MIDI_Init);
#if C_DEBUG
	secprop=control->AddSection_prop("debug",&DEBUG_Init);
#endif
	secprop=control->AddSection_prop("sblaster",&SBLASTER_Init);
    secprop->Add_hex("base",0x220);
	secprop->Add_int("irq",7);
	secprop->Add_int("dma",1);
	secprop->Add_int("hdma",5);
	secprop->Add_int("sbrate",22050);
	secprop->Add_bool("enabled",true);

	secprop->AddInitFunction(&ADLIB_Init);
	secprop->Add_bool("adlib",true);
	secprop->AddInitFunction(&CMS_Init);
    secprop->Add_bool("cms",false);
	secprop->AddInitFunction(&MPU401_Init);
	secprop->Add_bool("mpu401",true);

	secprop=control->AddSection_prop("gus",&GUS_Init);

	secprop=control->AddSection_prop("disney",&DISNEY_Init);
	secprop->Add_bool("enabled",true);

	secprop=control->AddSection_prop("speaker",&PCSPEAKER_Init);
	secprop->Add_bool("enabled",true);
	secprop->Add_bool("sinewave",false);
	secprop->AddInitFunction(&TANDYSOUND_Init);
	secprop->Add_bool("tandy",false);

	secprop=control->AddSection_prop("bios",&BIOS_Init);
	secprop->AddInitFunction(&INT10_Init);

	/* All the DOS Related stuff, which will eventually start up in the shell */
	//TODO Maybe combine most of the dos stuff in one section like ems,xms
	secprop=control->AddSection_prop("dos",&DOS_Init);
	secprop->AddInitFunction(&EMS_Init);
	secprop->Add_int("emssize",4);
	secprop->AddInitFunction(&XMS_Init);
	secprop->Add_int("xmssize",8);

	// Mscdex
	secprop->AddInitFunction(&MSCDEX_Init);

	secline=control->AddSection_line("autoexec",&AUTOEXEC_Init);

	control->SetStartUp(&SHELL_Init);	

#if C_FPU
	FPU_Init();
#endif
}


