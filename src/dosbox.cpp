/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dosbox.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unistd.h>

#include <chrono>
#include <thread>

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
#include "shell.h"
#include "control.h"
#include "cross.h"
#include "programs.h"
#include "support.h"
#include "mapper.h"
#include "ints/int10.h"
#include "render.h"
#include "pci_bus.h"
#include "midi.h"
#include "hardware.h"
#include "ne2000.h"

Config * control;
bool shutdown_requested = false;
MachineType machine;
SVGACards svgaCard;

/* The whole load of startups for all the subfunctions */
void MSG_Init(Section_prop *);
void LOG_StartUp();
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
void HARDWARE_Init(Section*);

#if defined(PCI_FUNCTIONALITY_ENABLED)
void PCI_Init(Section*);
#endif


void KEYBOARD_Init(Section*);	//TODO This should setup INT 16 too but ok ;)
void JOYSTICK_Init(Section*);
void MOUSE_Init(Section*);
void SBLASTER_Init(Section*);
void MPU401_Init(Section*);
void PCSPEAKER_Init(Section*);
void TANDYSOUND_Init(Section*);
void DISNEY_Init(Section*);
void PS1AUDIO_Init(Section *);
void INNOVA_Init(Section*);
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
void SHELL_Init();

void INT10_Init(Section*);

static LoopHandler * loop;

static int ticksRemain;
static int64_t ticksLast;
static int ticksAdded;
int ticksDone;
int ticksScheduled;
bool ticksLocked;
void increaseticks();

bool mono_cga=false;

void Null_Init([[maybe_unused]] Section *sec) {
	// do nothing
}

static Bitu Normal_Loop() {
	Bits ret;
	while (1) {
		if (PIC_RunQueue()) {
			ret = (*cpudecoder)();
			if (GCC_UNLIKELY(ret<0)) return 1;
			if (ret>0) {
				if (GCC_UNLIKELY(ret >= CB_MAX)) return 0;
				Bitu blah = (*CallBack_Handlers[ret])();
				if (GCC_UNLIKELY(blah)) return blah;
			}
#if C_DEBUG
			if (DEBUG_ExitLoop()) return 0;
#endif
		} else {
			if (!GFX_Events())
				return 0;
			if (ticksRemain > 0) {
				TIMER_AddTick();
				ticksRemain--;
			} else {increaseticks();return 0;}
		}
	}
}

void increaseticks() { //Make it return ticksRemain and set it in the function above to remove the global variable.
	if (GCC_UNLIKELY(ticksLocked)) { // For Fast Forward Mode
		ticksRemain=5;
		/* Reset any auto cycle guessing for this frame */
		ticksLast = GetTicks();
		ticksAdded = 0;
		ticksDone = 0;
		ticksScheduled = 0;
		return;
	}

	const auto ticksNew = GetTicks();
	ticksScheduled += ticksAdded;
	if (ticksNew <= ticksLast) { //lower should not be possible, only equal.
		ticksAdded = 0;

		constexpr auto duration = std::chrono::milliseconds(1);
		std::this_thread::sleep_for(duration);

		const auto timeslept = GetTicksSince(ticksNew);

		// Update ticksDone with the time spent sleeping
		ticksDone -= timeslept;
		if (ticksDone < 0)
			ticksDone = 0;
		return; //0

		// If we do work this tick and sleep till the next tick, then ticksDone is decreased,
		// despite the fact that work was done as well in this tick. Maybe make it depend on an extra parameter.
		// What do we know: ticksRemain = 0 (condition to enter this function)
		// ticksNew = time before sleeping

		// maybe keep track of sleeped time in this frame, and use sleeped and done as indicators. (and take care of the fact there
		// are frames that have both.
	}

	//TicksNew > ticksLast
	ticksRemain = GetTicksDiff(ticksNew, ticksLast);
	ticksLast = ticksNew;
	ticksDone += ticksRemain;
	if ( ticksRemain > 20 ) {
//		LOG(LOG_MISC,LOG_ERROR)("large remain %d",ticksRemain);
		ticksRemain = 20;
	}
	ticksAdded = ticksRemain;

	// Is the system in auto cycle mode guessing ? If not just exit. (It can be temporary disabled)
	if (!CPU_CycleAutoAdjust || CPU_SkipCycleAutoAdjust) return;

	if (ticksScheduled >= 250 || ticksDone >= 250 || (ticksAdded > 15 && ticksScheduled >= 5) ) {
		if(ticksDone < 1) ticksDone = 1; // Protect against div by zero
		/* ratio we are aiming for is around 90% usage*/
		Bit32s ratio = (ticksScheduled * (CPU_CyclePercUsed*90*1024/100/100)) / ticksDone;
		Bit32s new_cmax = CPU_CycleMax;
		Bit64s cproc = (Bit64s)CPU_CycleMax * (Bit64s)ticksScheduled;
		double ratioremoved = 0.0; //increase scope for logging
		if (cproc > 0) {
			/* ignore the cycles added due to the IO delay code in order
			   to have smoother auto cycle adjustments */
			ratioremoved = (double) CPU_IODelayRemoved / (double) cproc;
			if (ratioremoved < 1.0) {
				double ratio_not_removed = 1 - ratioremoved;
				ratio = (Bit32s)((double)ratio * ratio_not_removed);

				/* Don't allow very high ratio which can cause us to lock as we don't scale down
				 * for very low ratios. High ratio might result because of timing resolution */
				if (ticksScheduled >= 250 && ticksDone < 10 && ratio > 16384)
					ratio = 16384;

				// Limit the ratio even more when the cycles are already way above the realmode default.
				if (ticksScheduled >= 250 && ticksDone < 10 && ratio > 5120 && CPU_CycleMax > 50000)
					ratio = 5120;

				// When downscaling multiple times in a row, ensure a minimum amount of downscaling
				if (ticksAdded > 15 && ticksScheduled >= 5 && ticksScheduled <= 20 && ratio > 800)
					ratio = 800;

				if (ratio <= 1024) {
					// ratio_not_removed = 1.0; //enabling this restores the old formula
					double r = (1.0 + ratio_not_removed) /(ratio_not_removed + 1024.0/(static_cast<double>(ratio)));
					new_cmax = 1 + static_cast<Bit32s>(CPU_CycleMax * r);
				} else {
					Bit64s ratio_with_removed = (Bit64s) ((((double)ratio - 1024.0) * ratio_not_removed) + 1024.0);
					Bit64s cmax_scaled = (Bit64s)CPU_CycleMax * ratio_with_removed;
					new_cmax = (Bit32s)(1 + (CPU_CycleMax >> 1) + cmax_scaled / (Bit64s)2048);
				}
			}
		}

		if (new_cmax < CPU_CYCLES_LOWER_LIMIT)
			new_cmax = CPU_CYCLES_LOWER_LIMIT;
		/*
		LOG(LOG_MISC,LOG_ERROR)("cyclelog: current %06d   cmax %06d   ratio  %05d  done %03d   sched %03d Add %d rr %4.2f",
			CPU_CycleMax,
			new_cmax,
			ratio,
			ticksDone,
			ticksScheduled,
			ticksAdded,
			ratioremoved);
		*/

		/* ratios below 1% are considered to be dropouts due to
		   temporary load imbalance, the cycles adjusting is skipped */
		if (ratio > 10) {
			/* ratios below 12% along with a large time since the last update
			   has taken place are most likely caused by heavy load through a
			   different application, the cycles adjusting is skipped as well */
			if ((ratio > 120) || (ticksDone < 700)) {
				CPU_CycleMax = new_cmax;
				if (CPU_CycleLimit > 0) {
					if (CPU_CycleMax > CPU_CycleLimit) CPU_CycleMax = CPU_CycleLimit;
				} else if (CPU_CycleMax > 2000000) CPU_CycleMax = 2000000; //Hardcoded limit, if no limit was specified.
			}
		}

		//Reset cycleguessing parameters.
		CPU_IODelayRemoved = 0;
		ticksDone = 0;
		ticksScheduled = 0;
	} else if (ticksAdded > 15) {
		/* ticksAdded > 15 but ticksScheduled < 5, lower the cycles
		   but do not reset the scheduled/done ticks to take them into
		   account during the next auto cycle adjustment */
		CPU_CycleMax /= 3;
		if (CPU_CycleMax < CPU_CYCLES_LOWER_LIMIT)
			CPU_CycleMax = CPU_CYCLES_LOWER_LIMIT;
	} //if (ticksScheduled >= 250 || ticksDone >= 250 || (ticksAdded > 15 && ticksScheduled >= 5) )
}

void DOSBOX_SetLoop(LoopHandler * handler) {
	loop=handler;
}

void DOSBOX_SetNormalLoop() {
	loop=Normal_Loop;
}

void DOSBOX_RunMachine()
{
	while ((*loop)() == 0 && !shutdown_requested)
		;
}

static void DOSBOX_UnlockSpeed( bool pressed ) {
	static bool autoadjust = false;
	if (pressed) {
		LOG_MSG("Fast Forward ON");
		ticksLocked = true;
		if (CPU_CycleAutoAdjust) {
			autoadjust = true;
			CPU_CycleAutoAdjust = false;
			CPU_CycleMax /= 3;
			if (CPU_CycleMax<1000) CPU_CycleMax=1000;
		}
	} else {
		LOG_MSG("Fast Forward OFF");
		ticksLocked = false;
		if (autoadjust) {
			autoadjust = false;
			CPU_CycleAutoAdjust = true;
		}
	}
}

static void DOSBOX_RealInit(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	/* Initialize some dosbox internals */
	ticksRemain=0;
	ticksLast=GetTicks();
	ticksLocked = false;
	DOSBOX_SetLoop(&Normal_Loop);
	MSG_Init(section);

	MAPPER_AddHandler(DOSBOX_UnlockSpeed, SDL_SCANCODE_F12, MMOD2,
	                  "speedlock", "Speedlock");

	std::string cmd_machine;
	if (control->cmdline->FindString("-machine",cmd_machine,true)){
		//update value in config (else no matching against suggested values
		section->HandleInputline(std::string("machine=") + cmd_machine);
	}

	std::string mtype(section->Get_string("machine"));
	svgaCard = SVGA_None;
	machine = MCH_VGA;
	int10.vesa_nolfb = false;
	int10.vesa_oldvbe = false;
	if      (mtype == "cga")      { machine = MCH_CGA; mono_cga = false; }
	else if (mtype == "cga_mono") { machine = MCH_CGA; mono_cga = true; }
	else if (mtype == "tandy")    { machine = MCH_TANDY; }
	else if (mtype == "pcjr")     { machine = MCH_PCJR;
	} else if (mtype == "hercules") {
		machine = MCH_HERC;
	} else if (mtype == "ega") {
		machine = MCH_EGA;
	} else if (mtype == "svga_s3") {
		svgaCard = SVGA_S3Trio;
	} else if (mtype == "vesa_nolfb") {
		svgaCard = SVGA_S3Trio;
		int10.vesa_nolfb = true;
	} else if (mtype == "vesa_oldvbe") {
		svgaCard = SVGA_S3Trio;
		int10.vesa_oldvbe = true;
	} else if (mtype == "svga_et4000") {
		svgaCard = SVGA_TsengET4K;
	} else if (mtype == "svga_et3000") {
		svgaCard = SVGA_TsengET3K;
	} else if (mtype == "svga_paradise") {
		svgaCard = SVGA_ParadisePVGA1A;
	} else if (mtype == "vgaonly") {
		svgaCard = SVGA_None;
	} else
		E_Exit("DOSBOX:Unknown machine type %s", mtype.c_str());

	// Convert the users video memory in either MB or KiB to bytes
	const auto vmemsize_string = std::string(section->Get_string("vmemsize"));

	// If auto, then default to 0 and let the adapter's setup rountine set
	// the size
	auto vmemsize = vmemsize_string == "auto" ? 0 : std::stoi(vmemsize_string);

	// Scale up from MB-to-bytes or from KB-to-bytes
	vmemsize *= (vmemsize <= 8) ? 1024 * 1024 : 1024;
	vga.vmemsize = check_cast<uint32_t>(vmemsize);

	if (std::string(section->Get_string("vesa_modes")) == "all")
		int10.vesa_mode_preference = VESA_MODE_PREF::ALL;
	else
		int10.vesa_mode_preference = VESA_MODE_PREF::COMPATIBLE;
}

void DOSBOX_Init() {
	Section_prop * secprop;
	Prop_int* Pint;
	Prop_int *pint = nullptr;
	Prop_hex* Phex;
	Prop_string* Pstring; // use pstring when touching properties
	Prop_string *pstring;
	Prop_bool* Pbool;
	Prop_multival *pmulti;
	Prop_multival_remain* Pmulti_remain;

	// Specifies if and when a setting can be changed
	constexpr auto always = Property::Changeable::Always;
	constexpr auto deprecated = Property::Changeable::Deprecated;
	constexpr auto only_at_start = Property::Changeable::OnlyAtStart;
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	// Some frequently used option sets
	const char *rates[] = {"44100", "48000", "32000", "22050", "16000",
	                       "11025", "8000",  "49716", 0};

	/* Setup all the different modules making up DOSBox */
	const char *machines[] = {"hercules",      "cga",
	                          "cga_mono",      "tandy",
	                          "pcjr",          "ega",
	                          "vgaonly",       "svga_s3",
	                          "svga_et3000",   "svga_et4000",
	                          "svga_paradise", "vesa_nolfb",
	                          "vesa_oldvbe",   0};

	secprop = control->AddSection_prop("dosbox", &DOSBOX_RealInit);
	pstring = secprop->Add_string("language", always, "");
	pstring->Set_help(
	        "Select a language to use: de, en, es, fr, it, pl, and ru\n"
	        "Notes: This setting will override the 'LANG' environment, if set.\n"
	        "       The 'translations' directory bundled with the executable holds these data\n"
	        "       files. Please keep it along-side the executable to support this feature.");

	pstring = secprop->Add_string("machine", only_at_start, "svga_s3");
	pstring->Set_values(machines);
	pstring->Set_help("The type of machine DOSBox tries to emulate.");

	pstring = secprop->Add_path("captures", always, "capture");
	pstring->Set_help(
	        "Directory where things like wave, midi, screenshot get captured.");

#if C_DEBUG
	LOG_StartUp();
#endif

	secprop->AddInitFunction(&IO_Init);//done
	secprop->AddInitFunction(&PAGING_Init);//done
	secprop->AddInitFunction(&MEM_Init);//done
	secprop->AddInitFunction(&HARDWARE_Init);//done
	pint = secprop->Add_int("memsize", when_idle, 16);
	pint->SetMinMax(1, 63);
	pint->Set_help(
	        "Amount of memory DOSBox has in megabytes.\n"
	        "This value is best left at its default to avoid problems with some games,\n"
	        "though few games might require a higher value.\n"
	        "There is generally no speed advantage when raising this value.");

	const char *vmemsize_choices[] = {
	        "auto",
	        "1",    "2",    "4",    "8", // MiB
	        "256", "512",  "1024", "2048", "4096", "8192", 0, // KiB
	};
	pstring = secprop->Add_string("vmemsize", only_at_start, "auto");
	pstring->Set_values(vmemsize_choices);
	pstring->Set_help(
	        "Video memory in MiB (1-8) or KiB (256 to 8192). 'auto' uses the default per video adapter.");

	const char *vesa_modes_choices[] = {"compatible", "all", 0};
	Pstring = secprop->Add_string("vesa_modes", only_at_start, "compatible");
	Pstring->Set_values(vesa_modes_choices);
	Pstring->Set_help(
	        "Controls the selection of VESA 1.2 and 2.0 modes offered:\n"
	        "  compatible   A tailored selection that maximizes game compatibility.\n"
	        "               This is recommended along with 4 or 8 MB of video memory.\n"
	        "  all          Offers all modes for a given video memory size, however\n"
	        "               some games may not use them properly (flickering) or may need\n"
	        "               more system memory (mem = ) to use them.");

	secprop->AddInitFunction(&CALLBACK_Init);
	secprop->AddInitFunction(&PIC_Init);//done
	secprop->AddInitFunction(&PROGRAMS_Init);
	secprop->AddInitFunction(&TIMER_Init);//done
	secprop->AddInitFunction(&CMOS_Init);//done

	const char *autoexec_section_choices[] = {
	        "join",
	        "overwrite",
	        0,
	};
	Pstring = secprop->Add_string("autoexec_section", only_at_start, "join");
	Pstring->Set_values(autoexec_section_choices);
	Pstring->Set_help(
	        "How autoexec sections are handled from multiple config files.\n"
	        "join      : combines them into one big section (legacy behavior).\n"
	        "overwrite : use the last one encountered, like other conf settings.");

	const char *verbosity_choices[] = {
	        "auto", "high", "medium", "low", "splash_only", "quiet", 0,
	};
	Pstring = secprop->Add_string("startup_verbosity", only_at_start, "auto");
	Pstring->Set_values(verbosity_choices);
	Pstring->Set_help(
	        "Controls verbosity prior to displaying the program:\n"
	        "Verbosity   | Splash | Welcome | Early stdout\n"
	        "high        |  yes   |   yes   |    yes\n"
	        "medium      |  no    |   yes   |    yes\n"
	        "low         |  no    |   no    |    yes\n"
	        "quiet       |  no    |   no    |    no\n"
	        "splash_only |  yes   |   no    |    no\n"
	        "auto        | 'low' if exec or dir is passed, otherwise 'high'");

	secprop = control->AddSection_prop("render", &RENDER_Init, true);
	pint = secprop->Add_int("frameskip", always, 0);
	pint->SetMinMax(0, 10);
	pint->Set_help("How many frames DOSBox skips before drawing one.");

	Pbool = secprop->Add_bool("aspect", always, true);
	Pbool->Set_help("Scales the vertical resolution to produce a 4:3 display aspect\n"
	                "ratio, matching that of the original standard-definition monitors\n"
	                "for which the majority of DOS games were designed. This setting\n"
	                "only affects video modes that use non-square pixels, such as\n"
	                "320x200 or 640x400; where as square-pixel modes, such as 640x480\n"
	                "and 800x600, will be displayed as-is.");

	pstring = secprop->Add_string("monochrome_palette", always, "white");
	pstring->Set_help("Select default palette for monochrome display.\n"
	                  "Works only when emulating hercules or cga_mono.\n"
	                  "You can also cycle through available colours using F11.");
	const char *mono_pal[] = {"white", "paperwhite", "green", "amber", 0};
	pstring->Set_values(mono_pal);

	pmulti = secprop->Add_multi("scaler", always, " ");
	pmulti->SetValue("none");
	pmulti->Set_help("Scaler used to enlarge/enhance low resolution modes.\n"
	                 "If 'forced' is appended, then the scaler will be used even if\n"
	                 "the result might not be desired.\n"
	                 "Note that some scalers may use black borders to fit the image\n"
	                 "within your configured display resolution. If this is\n"
	                 "undesirable, try either a different scaler or enabling\n"
	                 "fullresolution output.");

	pstring = pmulti->GetSection()->Add_string("type", always, "none");

	const char *scalers[] = {
		"none", "normal2x", "normal3x",
#if RENDER_USE_ADVANCED_SCALERS>2
		"advmame2x", "advmame3x", "advinterp2x", "advinterp3x", "hq2x", "hq3x", "2xsai", "super2xsai", "supereagle",
#endif
#if RENDER_USE_ADVANCED_SCALERS>0
		"tv2x", "tv3x", "rgb2x", "rgb3x", "scan2x", "scan3x",
#endif
		0 };
	pstring->Set_values(scalers);

	const char *force[] = {"", "forced", 0};
	pstring = pmulti->GetSection()->Add_string("force", always, "");
	pstring->Set_values(force);

#if C_OPENGL
	pstring = secprop->Add_path("glshader", always, "default");
	pstring->Set_help("Either 'none' or a GLSL shader name. Works only with\n"
	                  "OpenGL output.  Can be either an absolute path, a file\n"
	                  "in the 'glshaders' subdirectory of the DOSBox\n"
	                  "configuration directory, or one of the built-in shaders:\n"
	                  "advinterp2x, advinterp3x, advmame2x, advmame3x,\n"
	                  "crt-easymode-flat, crt-fakelottes-flat, rgb2x, rgb3x,\n"
	                  "scan2x, scan3x, tv2x, tv3x, sharp (default).");
#endif

	// Add the [composite] conf block after [render]
	assert(control);
	VGA_AddCompositeSettings(*control);

	secprop=control->AddSection_prop("cpu",&CPU_Init,true);//done
	const char* cores[] = { "auto",
#if (C_DYNAMIC_X86) || (C_DYNREC)
		"dynamic",
#endif
		"normal", "simple",0 };
	Pstring = secprop->Add_string("core", when_idle, "auto");
	Pstring->Set_values(cores);
	Pstring->Set_help("CPU Core used in emulation. auto will switch to dynamic if available and\n"
		"appropriate.");

	const char* cputype_values[] = { "auto", "386", "386_slow", "486_slow", "pentium_slow", "386_prefetch", 0};
	Pstring = secprop->Add_string("cputype", always, "auto");
	Pstring->Set_values(cputype_values);
	Pstring->Set_help("CPU Type used in emulation. auto is the fastest choice.");


	Pmulti_remain = secprop->Add_multiremain("cycles", always, " ");
	Pmulti_remain->Set_help(
		"Number of instructions DOSBox tries to emulate each millisecond.\n"
		"Setting this value too high results in sound dropouts and lags.\n"
		"Cycles can be set in 3 ways:\n"
		"  'auto'          tries to guess what a game needs.\n"
		"                  It usually works, but can fail for certain games.\n"
		"  'fixed #number' will set a fixed number of cycles. This is what you usually\n"
		"                  need if 'auto' fails (Example: fixed 4000).\n"
		"  'max'           will allocate as much cycles as your computer is able to\n"
		"                  handle.");

	const char* cyclest[] = { "auto","fixed","max","%u",0 };
	Pstring = Pmulti_remain->GetSection()->Add_string("type", always, "auto");
	Pmulti_remain->SetValue("auto");
	Pstring->Set_values(cyclest);

	Pmulti_remain->GetSection()->Add_string("parameters", always, "");

	Pint = secprop->Add_int("cycleup", always, 10);
	Pint->SetMinMax(1,1000000);
	Pint->Set_help("Number of cycles added or subtracted with speed control hotkeys.\n"
	               "Run INTRO and see Special Keys for list of hotkeys.");

	Pint = secprop->Add_int("cycledown", always, 20);
	Pint->SetMinMax(1,1000000);
	Pint->Set_help("Setting it lower than 100 will be a percentage.");

#if C_FPU
	secprop->AddInitFunction(&FPU_Init);
#endif
	secprop->AddInitFunction(&DMA_Init);//done
	secprop->AddInitFunction(&VGA_Init);
	secprop->AddInitFunction(&KEYBOARD_Init);


#if defined(PCI_FUNCTIONALITY_ENABLED)
	secprop=control->AddSection_prop("pci",&PCI_Init,false); //PCI bus
#endif


	secprop=control->AddSection_prop("mixer",&MIXER_Init);
	Pbool = secprop->Add_bool("nosound", only_at_start, false);
	Pbool->Set_help("Enable silent mode, sound is still emulated though.");

	Pint = secprop->Add_int("rate", only_at_start, 48000);
	Pint->Set_values(rates);
	Pint->Set_help("Mixer sample rate, setting any device's rate higher than this will probably lower their sound quality.");

	const char *blocksizes[] = {
		 "1024", "2048", "4096", "8192", "512", "256", "128", 0};
	Pint = secprop->Add_int("blocksize", only_at_start, 512);
	Pint->Set_values(blocksizes);
	Pint->Set_help("Mixer block size, larger blocks might help sound stuttering but sound will also be more lagged.");

	Pint = secprop->Add_int("prebuffer",only_at_start,20);
	Pint->SetMinMax(0,100);
	Pint->Set_help("How many milliseconds of data to keep on top of the blocksize.");

	Pbool = secprop->Add_bool("negotiate", only_at_start, true);
	Pbool->Set_help("Allow system audio driver to negotiate optimal rate and blocksize\n"
	                "as close to the specified values as possible.");

	secprop = control->AddSection_prop("midi", &MIDI_Init, true);
	secprop->AddInitFunction(&MPU401_Init, true);

	pstring = secprop->Add_string("mididevice", when_idle, "auto");
	const char *midi_devices[] = {
		"auto",
#if defined(MACOSX)
#if C_COREMIDI
		"coremidi",
#endif
#if C_COREAUDIO
		"coreaudio",
#endif
#elif defined(WIN32)
		"win32",
#else
		"oss",
#endif
#if C_ALSA
		"alsa",
#endif
#if C_FLUIDSYNTH
		"fluidsynth",
#endif
#if C_MT32EMU
		"mt32",
#endif
		"none",
		0 };

	pstring->Set_values(midi_devices);
	pstring->Set_help(
	        "Device that will receive the MIDI data (from the emulated MIDI\n"
	        "interface - MPU-401). Choose one of the following:\n"
#if C_FLUIDSYNTH
	        "'fluidsynth', to use the built-in MIDI synthesizer. See the\n"
	        "       [fluidsynth] section for detailed configuration.\n"
#endif
#if C_MT32EMU
	        "'mt32', to use the built-in Roland MT-32 synthesizer.\n"
	        "       See the [mt32] section for detailed configuration.\n"
#endif
	        "'auto', to use the first working external MIDI player. This\n"
	        "       might be a software synthesizer or physical device.");

	pstring = secprop->Add_string("midiconfig", when_idle, "");
	pstring->Set_help(
	        "Configuration options for the selected MIDI interface.\n"
	        "This is usually the id or name of the MIDI synthesizer you want\n"
	        "to use (find the id/name with DOS command 'mixer /listmidi').\n"
#if (C_FLUIDSYNTH == 1 || C_MT32EMU == 1)
	        "- This option has no effect when using the built-in synthesizers\n"
	        "  (mididevice = fluidsynth or mt32).\n"
#endif
#if C_COREAUDIO
	        "- When using CoreAudio, you can specify a soundfont here.\n"
#endif
#if C_ALSA
	        "- When using ALSA, use Linux command 'aconnect -l' to list open\n"
	        "  MIDI ports, and select one (for example 'midiconfig=14:0'\n"
	        "  for sequencer client 14, port 0).\n"
#endif
	        "- If you're using a physical Roland MT-32 with revision 0 PCB,\n"
	        "  the hardware may require a delay in order to prevent its\n"
	        "  buffer from overflowing. In that case, add 'delaysysex',\n"
	        "  for example: 'midiconfig=2 delaysysex'.\n"
	        "See the README/Manual for more details.");

	pstring = secprop->Add_string("mpu401", when_idle, "intelligent");
	const char *mputypes[] = {"intelligent", "uart", "none", 0};
	pstring->Set_values(mputypes);
	pstring->Set_help("Type of MPU-401 to emulate.");

#if C_FLUIDSYNTH
	FLUID_AddConfigSection(control);
#endif

#if C_MT32EMU
	MT32_AddConfigSection(control);
#endif

#if C_DEBUG
	secprop=control->AddSection_prop("debug",&DEBUG_Init);
#endif

	secprop = control->AddSection_prop("sblaster", &SBLASTER_Init, true);

	const char* sbtypes[] = {"sb1", "sb2", "sbpro1", "sbpro2", "sb16", "gb", "none", 0};
	Pstring = secprop->Add_string("sbtype", when_idle, "sb16");
	Pstring->Set_values(sbtypes);
	Pstring->Set_help("Type of Sound Blaster to emulate. 'gb' is Game Blaster.");

	const char *ios[] = {"220", "240", "260", "280", "2a0", "2c0", "2e0", "300", 0};
	Phex = secprop->Add_hex("sbbase", when_idle, 0x220);
	Phex->Set_values(ios);
	Phex->Set_help("The IO address of the Sound Blaster.");

	const char *irqssb[] = {"7", "5", "3", "9", "10", "11", "12", 0};
	Pint = secprop->Add_int("irq", when_idle, 7);
	Pint->Set_values(irqssb);
	Pint->Set_help("The IRQ number of the Sound Blaster.");

	const char *dmassb[] = {"1", "5", "0", "3", "6", "7", 0};
	Pint = secprop->Add_int("dma", when_idle, 1);
	Pint->Set_values(dmassb);
	Pint->Set_help("The DMA number of the Sound Blaster.");

	Pint = secprop->Add_int("hdma", when_idle, 5);
	Pint->Set_values(dmassb);
	Pint->Set_help("The High DMA number of the Sound Blaster.");

	Pbool = secprop->Add_bool("sbmixer", when_idle, true);
	Pbool->Set_help("Allow the Sound Blaster mixer to modify the DOSBox mixer.");

	pint = secprop->Add_int("oplrate", deprecated, false);
	pint->Set_help("oplrate is deprecated. The OPL waveform is now sampled\n"
	               "        at the mixer's playback rate to avoid resampling.");

	const char* oplmodes[] = {"auto", "cms", "opl2", "dualopl2", "opl3", "opl3gold", "none", 0};
	Pstring = secprop->Add_string("oplmode", when_idle, "auto");
	Pstring->Set_values(oplmodes);
	Pstring->Set_help("Type of OPL emulation. On 'auto' the mode is determined by 'sbtype'.\n"
	                  "All OPL modes are AdLib-compatible, except for 'cms'.");

	const char* oplemus[] = {"default", "compat", "fast", "mame", "nuked", 0};
	Pstring = secprop->Add_string("oplemu", when_idle, "default");
	Pstring->Set_values(oplemus);
	Pstring->Set_help(
	        "Provider for the OPL emulation. 'compat' provides better quality,\n"
	        "'nuked' is the default and most accurate (but the most CPU-intensive).");

	// Configure Gravis UltraSound emulation
	GUS_AddConfigSection(control);

	// Configure Innovation SSI-2001 emulation
	INNOVATION_AddConfigSection(control);

	secprop = control->AddSection_prop("speaker",&PCSPEAKER_Init,true);//done
	Pbool = secprop->Add_bool("pcspeaker", when_idle, true);
	Pbool->Set_help("Enable PC-Speaker emulation.");

	// Basis for the default PC-Speaker sample generation rate:
	//   "With the PC speaker, typically a 6-bit DAC with a maximum value of
	//   63
	//    is used at a sample rate of 18,939.4 Hz."
	// PC Speaker. (2020, June 8). In Wikipedia. Retrieved from
	// https://en.wikipedia.org/w/index.php?title=PC_speaker&oldid=961464485
	//
	// As this is the frequency range that game authors in the 1980s would
	// have worked with when tuning their game audio, we therefore use this
	// same value given it's the most likely to produce audio as intended by
	// the authors.
	pint = secprop->Add_int("pcrate", when_idle, 18939);
	pint->SetMinMax(8000, 48000);
	pint->Set_help("Sample rate of the PC-Speaker sound generation.");

	const char *zero_offset_opts[] = {"auto", "true", "false", 0};
	pstring = secprop->Add_string("zero_offset", when_idle, zero_offset_opts[0]);
	pstring->Set_values(zero_offset_opts);
	pstring->Set_help(
	        "Neutralizes and prevents the PC speaker's DC-offset from harming other sources.\n"
	        "'auto' enables this for non-Windows systems and disables it on Windows.\n"
	        "If your OS performs its own DC-offset correction, then set this to 'false'.");

	secprop->AddInitFunction(&TANDYSOUND_Init, true);
	const char *tandys[] = {"auto", "on", "off", 0};
	Pstring = secprop->Add_string("tandy", when_idle, "auto");
	Pstring->Set_values(tandys);
	Pstring->Set_help("Enable Tandy Sound System emulation. For 'auto', emulation is present only if machine is set to 'tandy'.");

	Pint = secprop->Add_int("tandyrate", when_idle, 44100);
	Pint->Set_values(rates);
	Pint->Set_help("Sample rate of the Tandy 3-Voice generation.");

	secprop->AddInitFunction(&DISNEY_Init,true);//done

	Pbool = secprop->Add_bool("disney", when_idle, true);
	Pbool->Set_help("Enable Disney Sound Source emulation. (Covox Voice Master and Speech Thing compatible).");

	// IBM PS/1 Audio emulation
	secprop->AddInitFunction(&PS1AUDIO_Init, true);
	Pbool = secprop->Add_bool("ps1audio", when_idle, false);
	Pbool->Set_help("Enable IBM PS/1 Audio emulation.");

	secprop=control->AddSection_prop("joystick",&BIOS_Init,false);//done
	secprop->AddInitFunction(&INT10_Init);
	secprop->AddInitFunction(&MOUSE_Init); //Must be after int10 as it uses CurMode
	secprop->AddInitFunction(&JOYSTICK_Init,true);
	const char *joytypes[] = {"auto", "2axis", "4axis",    "4axis_2", "fcs",
	                          "ch",   "hidden",  "disabled", 0};
	Pstring = secprop->Add_string("joysticktype", when_idle, "auto");
	Pstring->Set_values(joytypes);
	Pstring->Set_help(
	        "Type of joystick to emulate: auto (default),\n"
	        "auto     : Detect and use any joystick(s), if possible.,\n"
	        "2axis    : Support up to two joysticks.\n"
	        "4axis    : Support the first joystick only.\n"
	        "4axis_2  : Support the second joystick only.\n"
	        "fcs      : Support a Thrustmaster-type joystick.\n"
	        "ch       : Support a CH Flightstick-type joystick.\n"
	        "hidden   : Prevent DOS from seeing the joystick(s), but enable them for mapping.\n"
	        "disabled : Fully disable joysticks: won't be polled, mapped, or visible in DOS.\n"
	        "(Remember to reset DOSBox's mapperfile if you saved it earlier)");

	Pbool = secprop->Add_bool("timed", when_idle, true);
	Pbool->Set_help("enable timed intervals for axis. Experiment with this option, if your joystick drifts (away).");

	Pbool = secprop->Add_bool("autofire", when_idle, false);
	Pbool->Set_help("continuously fires as long as you keep the button pressed.");

	Pbool = secprop->Add_bool("swap34", when_idle, false);
	Pbool->Set_help("swap the 3rd and the 4th axis. Can be useful for certain joysticks.");

	Pbool = secprop->Add_bool("buttonwrap", when_idle, false);
	Pbool->Set_help("enable button wrapping at the number of emulated buttons.");

	Pbool = secprop->Add_bool("circularinput", when_idle, false);
	Pbool->Set_help("enable translation of circular input to square output.\n"
	                "Try enabling this if your left analog stick can only move in a circle.");

	Pint = secprop->Add_int("deadzone", when_idle, 10);
	Pint->SetMinMax(0,100);
	Pint->Set_help("the percentage of motion to ignore. 100 turns the stick into a digital one.");

	secprop=control->AddSection_prop("serial",&SERIAL_Init,true);
	const char* serials[] = { "dummy", "disabled", "modem", "nullmodem",
	                          "directserial",0 };

	Pmulti_remain = secprop->Add_multiremain("serial1", when_idle, " ");
	Pstring = Pmulti_remain->GetSection()->Add_string("type", when_idle, "dummy");
	Pmulti_remain->SetValue("dummy");
	Pstring->Set_values(serials);
	Pmulti_remain->GetSection()->Add_string("parameters", when_idle, "");
	Pmulti_remain->Set_help(
		"set type of device connected to com port.\n"
		"Can be disabled, dummy, modem, nullmodem, directserial.\n"
		"Additional parameters must be in the same line in the form of\n"
		"parameter:value. Parameter for all types is irq (optional).\n"
		"for directserial: realport (required), rxdelay (optional).\n"
		"                 (realport:COM1 realport:ttyS0).\n"
		"for modem: listenport enet (all optional).\n"
		"for nullmodem: server, rxdelay, txdelay, telnet, usedtr,\n"
		"               transparent, port, inhsocket, enet (all optional).\n"
		"Example: serial1=modem listenport:5000");

	Pmulti_remain = secprop->Add_multiremain("serial2", when_idle, " ");
	Pstring = Pmulti_remain->GetSection()->Add_string("type", when_idle, "dummy");
	Pmulti_remain->SetValue("dummy");
	Pstring->Set_values(serials);
	Pmulti_remain->GetSection()->Add_string("parameters", when_idle, "");
	Pmulti_remain->Set_help("see serial1");

	Pmulti_remain = secprop->Add_multiremain("serial3", when_idle, " ");
	Pstring = Pmulti_remain->GetSection()->Add_string("type", when_idle, "disabled");
	Pmulti_remain->SetValue("disabled");
	Pstring->Set_values(serials);
	Pmulti_remain->GetSection()->Add_string("parameters", when_idle, "");
	Pmulti_remain->Set_help("see serial1");

	Pmulti_remain = secprop->Add_multiremain("serial4", when_idle, " ");
	Pstring = Pmulti_remain->GetSection()->Add_string("type", when_idle, "disabled");
	Pmulti_remain->SetValue("disabled");
	Pstring->Set_values(serials);
	Pmulti_remain->GetSection()->Add_string("parameters", when_idle, "");
	Pmulti_remain->Set_help("see serial1");

	pstring = secprop->Add_path("phonebookfile", only_at_start, "phonebook.txt");
	pstring->Set_help("File used to map fake phone numbers to addresses.");

	/* All the DOS Related stuff, which will eventually start up in the shell */
	secprop=control->AddSection_prop("dos",&DOS_Init,false);//done
	secprop->AddInitFunction(&XMS_Init,true);//done
	Pbool = secprop->Add_bool("xms", when_idle, true);
	Pbool->Set_help("Enable XMS support.");

	secprop->AddInitFunction(&EMS_Init,true);//done
	const char* ems_settings[] = { "true", "emsboard", "emm386", "false", 0};
	Pstring = secprop->Add_string("ems", when_idle, "true");
	Pstring->Set_values(ems_settings);
	Pstring->Set_help("Enable EMS support. The default (=true) provides the best\n"
		"compatibility but certain applications may run better with\n"
		"other choices, or require EMS support to be disabled (=false)\n"
		"to work at all.");

	Pbool = secprop->Add_bool("umb", when_idle, true);
	Pbool->Set_help("Enable UMB support.");

	pstring = secprop->Add_string("ver", when_idle, "5.0");
	pstring->Set_help("Set DOS version (5.0 by default). Specify as major.minor format.\n"
	                  "A single number is treated as the major version.\n"
	                  "Common settings are 3.3, 5.0, 6.22, and 7.1.");

	secprop->AddInitFunction(&DOS_KeyboardLayout_Init,true);
	Pstring = secprop->Add_string("keyboardlayout", when_idle,  "auto");
	Pstring->Set_help("Language code of the keyboard layout (or none).");

	// Mscdex
	secprop->AddInitFunction(&MSCDEX_Init);
	secprop->AddInitFunction(&DRIVES_Init);
	secprop->AddInitFunction(&CDROM_Image_Init);
#if C_IPX
	secprop=control->AddSection_prop("ipx",&IPX_Init,true);
	Pbool = secprop->Add_bool("ipx", when_idle,  false);
	Pbool->Set_help("Enable ipx over UDP/IP emulation.");
#endif

#if C_SLIRP
	secprop = control->AddSection_prop("ethernet", &NE2K_Init, true);

	Pbool = secprop->Add_bool("ne2000", when_idle,  true);
	Pbool->Set_help(
	        "Enable emulation of a Novell NE2000 network card on a software-based\n"
	        "network (using libslirp) with properties as follows:\n"
	        " - 255.255.255.0 : Subnet mask of the 10.0.2.0 virtual LAN.\n"
	        " - 10.0.2.2      : IP of the gateway and DHCP service.\n"
	        " - 10.0.2.3      : IP of the virtual DNS server.\n"
	        " - 10.0.2.15     : First IP provided by DHCP, your IP!\n"
	        "Note: Inside DOS, setting this up requires an NE2000 packet driver,\n"
	        "      DHCP client, and TCP/IP stack. You might need port-forwarding\n"
	        "      from the host into the DOS guest, and from your router to your\n"
	        "      host when acting as the server for multiplayer games.");

	const char *nic_addresses[] = {"200", "220", "240", "260", "280", "2c0",
	                               "300", "320", "340", "360", 0};
	Phex = secprop->Add_hex("nicbase", when_idle, 0x300);
	Phex->Set_values(nic_addresses);
	Phex->Set_help(
	        "The base address of the NE2000 card.\n"
	        "Note: Addresses 220 and 240 might not be available as they're assigned\n"
	        "      to the Sound Blaster and Gravis UltraSound by default.");

	const char *nic_irqs[] = {"3",  "4",  "5",  "9",  "10",
	                          "11", "12", "14", "15", 0};
	Pint = secprop->Add_int("nicirq", when_idle, 3);
	Pint->Set_values(nic_irqs);
	Pint->Set_help("The interrupt used by the NE2000 card.\n"
	               "Note: IRQs 3 and 5 might not be available as they're assigned\n"
	               "      to 'serial2' and the Gravis UltraSound by default.");

	Pstring = secprop->Add_string("macaddr", when_idle, "AC:DE:48:88:99:AA");
	Pstring->Set_help("The MAC address of the NE2000 card.");

	Pstring = secprop->Add_string("tcp_port_forwards", when_idle, "");
	Pstring->Set_help("Forwards one or more TCP ports from the host into the DOS guest.\n"
	                  "The format is:\n"
	                  "  port1  port2  port3 ... (e.g., 21 80 443)\n"
	                  "  This will forward FTP, HTTP, and HTTPS into the DOS guest.\n"
	                  "If the ports are privilidged on the host, a mapping can be used\n"
	                  "  host:guest  ..., (e.g., 8021:21 8080:80)\n"
	                  "  This will forward ports 8021 and 8080 to FTP and HTTP in the guest\n"
	                  "A range of adjacent ports can be abbreviated with a dash:\n"
	                  "  start-end ... (e.g., 27910-27960)\n"
	                  "  This will forward ports 27910 to 27960 into the DOS guest.\n"
	                  "Mappings and ranges can be combined, too:\n"
	                  "  hstart-hend:gstart-gend ..., (e.g, 8040-8080:20-60)\n"
	                  "  This forwards ports 8040 to 8080 into 20 to 60 in the guest\n"
	                  ""
	                  "Notes:\n"
	                  "  - If mapped ranges differ, the shorter range is extended to fit.\n"
	                  "  - If conflicting host ports are given, only the first one is setup.\n"
	                  "  - If conflicting guest ports are given, the latter rule takes predecent.");

	Pstring = secprop->Add_string("udp_port_forwards", when_idle, "");
	Pstring->Set_help("Forwards one or more UDP ports from the host into the DOS guest.\n"
	                  "The format is the same as for TCP port forwards.");
#endif

	//	secprop->AddInitFunction(&CREDITS_Init);

	//TODO ?
	control->AddSection_line("autoexec", &AUTOEXEC_Init);
	MSG_Add("AUTOEXEC_CONFIGFILE_HELP",
		"Lines in this section will be run at startup.\n"
		"You can put your MOUNT lines here.\n"
	);
	MSG_Add("CONFIGFILE_INTRO",
	        "# This is the configuration file for dosbox-staging (%s).\n"
	        "# Lines starting with a '#' character are comments.\n");
	MSG_Add("CONFIG_SUGGESTED_VALUES", "Possible values");

	control->SetStartUp(&SHELL_Init);
}
