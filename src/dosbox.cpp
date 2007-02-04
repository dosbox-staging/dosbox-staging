/*
 *  Copyright (C) 2002-2007  The DOSBox Team
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

/* $Id: dosbox.cpp,v 1.114 2007-02-04 11:10:22 qbix79 Exp $ */

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
#include "mapper.h"

Config * control;
MachineType machine;
SVGACards svgaCard;

/* The whole load of startups for all the subfunctions */
void MSG_Init(Section_prop *);
void LOG_StartUp(void);
void MEM_Init(Section *);
void PAGING_Init(Section *);
void IO_Init(Section * );
void CALLBACK_Init(Section*);
void PROGRAMS_Init(Section*);
//void CREDITS_Init(Section*);
void RENDER_Init(Section*);
void VGA_Init(Section*);

void DOS_Init(Section*);


void CPU_Init(Section*);

#if C_FPU
void FPU_Init(Section*);
#endif

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
void PCSPEAKER_Init(Section*);
void TANDYSOUND_Init(Section*);
void DISNEY_Init(Section*);
void SERIAL_Init(Section*); 


#if C_IPX
void IPX_Init(Section*);
#endif

void SID_Init(Section* sec);

void PIC_Init(Section*);
void TIMER_Init(Section*);
void BIOS_Init(Section*);
void DEBUG_Init(Section*);
void CMOS_Init(Section*);

void MSCDEX_Init(Section*);
void DRIVES_Init(Section*);
void CDROM_Image_Init(Section*);

/* Dos Internal mostly */
void EMS_Init(Section*);
void XMS_Init(Section*);

void DOS_KeyboardLayout_Init(Section*);

void AUTOEXEC_Init(Section*);
void SHELL_Init(void);

void INT10_Init(Section*);

static LoopHandler * loop;

bool SDLNetInited;

static Bit32u ticksRemain;
static Bit32u ticksLast;
static Bit32u ticksAdded;
static Bit32s ticksDone;
static Bit32u ticksScheduled;
bool ticksLocked;

static Bitu Normal_Loop(void) {
	Bits ret;
	while (1) {
		if (PIC_RunQueue()) {
			ret=(*cpudecoder)();
			if (ret<0) return 1;
			if (ret>0) {
				Bitu blah=(*CallBack_Handlers[ret])();
				if (blah) return blah;
			}
#if C_DEBUG
			if (DEBUG_ExitLoop()) return 0;
#endif
		} else {
			GFX_Events();
			if (ticksRemain>0) {
				TIMER_AddTick();
				ticksRemain--;
			} else goto increaseticks;
		}
	}
increaseticks:
	if (GCC_UNLIKELY(ticksLocked)) {
		ticksRemain=5;
		/* Reset any auto cycle guessing for this frame */
		ticksLast = GetTicks();
		ticksAdded = 0;
		ticksDone = 0;
		ticksScheduled = 0;
	} else {
		Bit32u ticksNew;
		ticksNew=GetTicks();
		ticksScheduled += ticksAdded;
		if (ticksNew > ticksLast) {
			ticksRemain = ticksNew-ticksLast;
			ticksLast = ticksNew;
			ticksDone += ticksRemain;
			if ( ticksRemain > 20 ) {
				ticksRemain = 20;
			}
			ticksAdded = ticksRemain;
			if (CPU_CycleAutoAdjust) {
				if(ticksScheduled >= 250 || ticksDone >= 250 || (ticksAdded > 15 && ticksScheduled >= 5) ) {
					/* ratio we are aiming for is around 90% usage*/
					Bits ratio = (ticksScheduled * (CPU_CyclePercUsed*90*1024/100/100)) / ticksDone;
					Bit32s new_cmax = CPU_CycleMax;
					Bit64s cproc = (Bit64s)CPU_CycleMax * (Bit64s)ticksScheduled;
					if(cproc > 0) {
						double ratioremoved = (double) CPU_IODelayRemoved / (double) cproc;
						if(ratioremoved < 1.0) {
							ratio = (Bits)((double)ratio * (1 - ratioremoved));
							if (ratio <= 1024) 
								new_cmax = (CPU_CycleMax * ratio) / 1024;
							else 
								new_cmax = 1 + (CPU_CycleMax >> 1) + (CPU_CycleMax * ratio) / 2048;
						}
					}

					// maybe care about not going negative
					if (new_cmax > 0) 
						CPU_CycleMax = new_cmax;
					if (CPU_CycleLimit > 0) {
						if (CPU_CycleMax>CPU_CycleLimit) CPU_CycleMax = CPU_CycleLimit;
					}
					CPU_IODelayRemoved = 0;
					ticksDone = 0;
					ticksScheduled = 0;
				} else if (ticksAdded > 15) {
					CPU_CycleMax /= 3;
					if (CPU_CycleMax < 100)
					CPU_CycleMax = 100;
				}
			}
		} else {
			ticksAdded = 0;
			SDL_Delay(1);
			ticksDone -= GetTicks() - ticksNew;
			if (ticksDone < 0)
				ticksDone = 0;
		}
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

static void DOSBOX_UnlockSpeed( bool pressed ) {
	if (pressed)
		ticksLocked = true;
	else 
		ticksLocked = false;
}

static void DOSBOX_RealInit(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	/* Initialize some dosbox internals */

	ticksRemain=0;
	ticksLast=GetTicks();
	ticksLocked = false;
	DOSBOX_SetLoop(&Normal_Loop);
	MSG_Init(section);

	MAPPER_AddHandler(DOSBOX_UnlockSpeed, MK_f12, MMOD2,"speedlock","Speedlock");
	svgaCard = SVGA_S3Trio; 
	machine=MCH_VGA;
	std::string cmd_machine;
	const char * mtype;
	if (control->cmdline->FindString("-machine",cmd_machine,true)) mtype=cmd_machine.c_str();
	else mtype=section->Get_string("machine");
	if (strcasecmp(mtype,"cga")==0) machine=MCH_CGA;
	else if (strcasecmp(mtype,"tandy")==0) machine=MCH_TANDY;
	else if (strcasecmp(mtype,"pcjr")==0) machine=MCH_PCJR;
	else if (strcasecmp(mtype,"hercules")==0) machine=MCH_HERC;
	else if (strcasecmp(mtype,"vga")==0) machine=MCH_VGA;
	else LOG_MSG("DOSBOX:Unknown machine type %s",mtype);
}


void DOSBOX_Init(void) {
	Section_prop * secprop;
	Section_line * secline;

	SDLNetInited = false;

	/* Setup all the different modules making up DOSBox */
	
	secprop=control->AddSection_prop("dosbox",&DOSBOX_RealInit);
	secprop->Add_string("language","");
	secprop->Add_string("machine","vga");
	secprop->Add_string("captures","capture");

#if C_DEBUG	
	LOG_StartUp();
#endif
	
	secprop->AddInitFunction(&IO_Init);//done
	secprop->AddInitFunction(&PAGING_Init);//done
	secprop->AddInitFunction(&MEM_Init);//done
	secprop->AddInitFunction(&HARDWARE_Init);//done
	secprop->Add_int("memsize",16);
	secprop->AddInitFunction(&CALLBACK_Init);
	secprop->AddInitFunction(&PIC_Init);//done
	secprop->AddInitFunction(&PROGRAMS_Init);
	secprop->AddInitFunction(&TIMER_Init);//done
	secprop->AddInitFunction(&CMOS_Init);//done
		
	MSG_Add("DOSBOX_CONFIGFILE_HELP",
		"language -- Select another language file.\n"
		"memsize -- Amount of memory dosbox has in megabytes.\n"
		"machine -- The type of machine tries to emulate:hercules,cga,tandy,pcjr,vga.\n"
		"captures -- Directory where things like wave,midi,screenshot get captured.\n"
	);

	secprop=control->AddSection_prop("render",&RENDER_Init,true);
	secprop->Add_int("frameskip",0);
	secprop->Add_bool("aspect",false);
	secprop->Add_string("scaler","normal2x");
	MSG_Add("RENDER_CONFIGFILE_HELP",
		"frameskip -- How many frames dosbox skips before drawing one.\n"
		"aspect -- Do aspect correction, if your output method doesn't support scaling this can slow things down!.\n"
		"scaler -- Scaler used to enlarge/enhance low resolution modes.\n"
		"          Supported are none,normal2x,normal3x,advmame2x,advmame3x,hq2x,hq3x,\n"
		"                        2xsai,super2xsai,supereagle,advinterp2x,advinterp3x,\n"
		"                        tv2x,tv3x,rgb2x,rgb3x,scan2x,scan3x.\n"
	);

	secprop=control->AddSection_prop("cpu",&CPU_Init,true);//done
#if (C_DYNAMIC_X86)
	secprop->Add_string("core","auto");
#else
	secprop->Add_string("core","normal");
#endif
	secprop->Add_string("cycles","auto");
	secprop->Add_int("cycleup",500);
	secprop->Add_int("cycledown",20);
	MSG_Add("CPU_CONFIGFILE_HELP",
		"core -- CPU Core used in emulation: normal"
#if (C_DYNAMIC_X86)
		",dynamic,auto.\n"
		"        auto switches from normal to dynamic if appropriate"
#endif
		".\n"
		"cycles -- Amount of instructions dosbox tries to emulate each millisecond.\n"
		"          Setting this higher than your machine can handle is bad!\n"
		"          You can also let DOSBox guess the correct value by setting it to max.\n"
		"          The default setting (auto) switches to max if appropriate.\n"
		"cycleup   -- Amount of cycles to increase/decrease with keycombo.\n"
		"cycledown    Setting it lower than 100 will be a percentage.\n"
	);
#if C_FPU
	secprop->AddInitFunction(&FPU_Init);
#endif
	secprop->AddInitFunction(&DMA_Init);//done
	secprop->AddInitFunction(&VGA_Init);
	secprop->AddInitFunction(&KEYBOARD_Init);

	secprop=control->AddSection_prop("mixer",&MIXER_Init);
	secprop->Add_bool("nosound",false);
	secprop->Add_int("rate",22050);
	secprop->Add_int("blocksize",2048);
	secprop->Add_int("prebuffer",10);

	MSG_Add("MIXER_CONFIGFILE_HELP",
		"nosound -- Enable silent mode, sound is still emulated though.\n"
		"rate -- Mixer sample rate, setting any devices higher than this will\n"
		"        probably lower their sound quality.\n"
		"blocksize -- Mixer block size, larger blocks might help sound stuttering\n"
		"             but sound will also be more lagged.\n"
		"prebuffer -- How many milliseconds of data to keep on top of the blocksize.\n"
	);
	
	secprop=control->AddSection_prop("midi",&MIDI_Init,true);//done
	secprop->AddInitFunction(&MPU401_Init,true);//done
	secprop->Add_string("mpu401","intelligent");
	secprop->Add_string("device","default");
	secprop->Add_string("config","");
	
	MSG_Add("MIDI_CONFIGFILE_HELP",
		"mpu401      -- Type of MPU-401 to emulate: none, uart or intelligent.\n"
		"device      -- Device that will receive the MIDI data from MPU-401.\n"
		"               This can be default,alsa,oss,win32,coreaudio,none.\n"
		"config      -- Special configuration options for the device. In Windows put\n" 
		"               the id of the device you want to use. See README for details.\n"
	);

#if C_DEBUG
	secprop=control->AddSection_prop("debug",&DEBUG_Init);
#endif

	secprop=control->AddSection_prop("sblaster",&SBLASTER_Init,true);//done
	secprop->Add_string("sbtype","sb16");
	secprop->Add_hex("sbbase",0x220);
	secprop->Add_int("irq",7);
	secprop->Add_int("dma",1);
	secprop->Add_int("hdma",5);
	secprop->Add_bool("mixer",true);
	secprop->Add_string("oplmode","auto");
	secprop->Add_int("oplrate",22050);

	MSG_Add("SBLASTER_CONFIGFILE_HELP",
		"sbtype -- Type of sblaster to emulate:none,sb1,sb2,sbpro1,sbpro2,sb16.\n"
		"sbbase,irq,dma,hdma -- The IO/IRQ/DMA/High DMA address of the soundblaster.\n"
		"mixer -- Allow the soundblaster mixer to modify the dosbox mixer.\n"
		"oplmode -- Type of OPL emulation: auto,cms,opl2,dualopl2,opl3.\n"
		"           On auto the mode is determined by sblaster type.\n"
		"           All OPL modes are 'Adlib', except for CMS.\n"
		"oplrate -- Sample rate of OPL music emulation.\n"
		);

	secprop=control->AddSection_prop("gus",&GUS_Init,true); //done
	secprop->Add_bool("gus",true); 	
	secprop->Add_int("gusrate",22050);
	secprop->Add_hex("gusbase",0x240);
	secprop->Add_int("irq1",5);
	secprop->Add_int("irq2",5);
	secprop->Add_int("dma1",3);
	secprop->Add_int("dma2",3);
	secprop->Add_string("ultradir","C:\\ULTRASND");

	MSG_Add("GUS_CONFIGFILE_HELP",
		"gus -- Enable the Gravis Ultrasound emulation.\n"
		"gusbase,irq1,irq2,dma1,dma2 -- The IO/IRQ/DMA addresses of the \n"
		"           Gravis Ultrasound. (Same IRQ's and DMA's are OK.)\n"
		"gusrate -- Sample rate of Ultrasound emulation.\n"
		"ultradir -- Path to Ultrasound directory.  In this directory\n"
		"            there should be a MIDI directory that contains\n"
		"            the patch files for GUS playback.  Patch sets used\n"
		"            with Timidity should work fine.\n"
	);

	secprop=control->AddSection_prop("speaker",&PCSPEAKER_Init,true);//done
	secprop->Add_bool("pcspeaker",true);
	secprop->Add_int("pcrate",22050);
	secprop->AddInitFunction(&TANDYSOUND_Init,true);//done
	secprop->Add_string("tandy","auto");
	secprop->Add_int("tandyrate",22050);
	secprop->AddInitFunction(&DISNEY_Init,true);//done
	secprop->Add_bool("disney",true);

	MSG_Add("SPEAKER_CONFIGFILE_HELP",
		"pcspeaker -- Enable PC-Speaker emulation.\n"
		"pcrate -- Sample rate of the PC-Speaker sound generation.\n"
		"tandy -- Enable Tandy Sound System emulation (off,on,auto).\n"
		"         For auto Tandysound emulation is present only if machine is set to tandy.\n"
		"tandyrate -- Sample rate of the Tandy 3-Voice generation.\n"
		"disney -- Enable Disney Sound Source emulation.\n"
	);

	secprop=control->AddSection_prop("joystick",&BIOS_Init,false);//done
	MSG_Add("JOYSTICK_CONFIGFILE_HELP",
	        "joysticktype -- Type of joystick to emulate: auto (default), none,\n"
	        "                2axis (supports two joysticks), 4axis,\n"
	        "                fcs (Thrustmaster), ch (CH Flightstick).\n"
	        "                none disables joystick emulation.\n"
	        "                auto chooses emulation depending on real joystick(s).\n"
	        "timed -- enable timed intervals for axis. (false is old style behaviour).\n"
	        "autofire -- continuously fires as long as you keep the button pressed.\n"
	        "swap34 -- swap the 3rd and the 4th axis. can be useful for certain joysticks.\n"
	);

	secprop->AddInitFunction(&INT10_Init);
	secprop->AddInitFunction(&MOUSE_Init); //Must be after int10 as it uses CurMode
	secprop->AddInitFunction(&JOYSTICK_Init);
	secprop->Add_string("joysticktype","auto");
	secprop->Add_bool("timed",true);
	secprop->Add_bool("autofire",false);
	secprop->Add_bool("swap34",false);

	// had to rename these to serial due to conflicts in config
	secprop=control->AddSection_prop("serial",&SERIAL_Init,true);
	secprop->Add_string("serial1","dummy");
	secprop->Add_string("serial2","dummy");
	secprop->Add_string("serial3","disabled");
	secprop->Add_string("serial4","disabled");
	MSG_Add("SERIAL_CONFIGFILE_HELP",
	        "serial1-4 -- set type of device connected to com port.\n"
	        "             Can be disabled, dummy, modem, nullmodem, directserial.\n"
	        "             Additional parameters must be in the same line in the form of\n"
	        "             parameter:value. Parameter for all types is irq.\n"
	        "             for directserial: realport (required), rxdelay (optional).\n"
	        "             for modem: listenport (optional).\n"
	        "             for nullmodem: server, rxdelay, txdelay, telnet, usedtr,\n"
	        "                            transparent, port, inhsocket (all optional).\n"
	        "             Example: serial1=modem listenport:5000\n"
	);

	/* All the DOS Related stuff, which will eventually start up in the shell */
	//TODO Maybe combine most of the dos stuff in one section like ems,xms
	secprop=control->AddSection_prop("dos",&DOS_Init,false);//done
	secprop->AddInitFunction(&XMS_Init,true);//done
	secprop->Add_bool("xms",true);
	secprop->AddInitFunction(&EMS_Init,true);//done
	secprop->Add_bool("ems",true);
	secprop->Add_bool("umb",true);
	secprop->AddInitFunction(&DOS_KeyboardLayout_Init,true);
	secprop->Add_string("keyboardlayout", "none");
	MSG_Add("DOS_CONFIGFILE_HELP",
		"xms -- Enable XMS support.\n"
		"ems -- Enable EMS support.\n"
		"umb -- Enable UMB support.\n"
		"keyboardlayout -- Language code of the keyboard layout (or none).\n"
	);
	// Mscdex
	secprop->AddInitFunction(&MSCDEX_Init);
	secprop->AddInitFunction(&DRIVES_Init);
	secprop->AddInitFunction(&CDROM_Image_Init);
#if C_IPX
	secprop=control->AddSection_prop("ipx",&IPX_Init,true);
	secprop->Add_bool("ipx", false);
	MSG_Add("IPX_CONFIGFILE_HELP",
		"ipx -- Enable ipx over UDP/IP emulation.\n"
	);
#endif
//	secprop->AddInitFunction(&CREDITS_Init);

	secline=control->AddSection_line("autoexec",&AUTOEXEC_Init);
	MSG_Add("AUTOEXEC_CONFIGFILE_HELP",
		"Lines in this section will be run at startup.\n"
	);
	control->SetStartUp(&SHELL_Init);
}
