/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
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

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>
#include <unistd.h>

#include "callback.h"
#include "control.h"
#include "cpu.h"
#include "cross.h"
#include "debug.h"
#include "dos_inc.h"
#include "hardware.h"
#include "inout.h"
#include "ints/int10.h"
#include "mapper.h"
#include "memory.h"
#include "midi.h"
#include "mixer.h"
#include "mouse.h"
#include "ne2000.h"
#include "pci_bus.h"
#include "pic.h"
#include "programs.h"
#include "reelmagic.h"
#include "render.h"
#include "setup.h"
#include "shell.h"
#include "support.h"
#include "timer.h"
#include "tracy.h"
#include "video.h"

bool shutdown_requested = false;
MachineType machine;
SVGACards svgaCard;

/* The whole load of startups for all the subfunctions */
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

void HARDWARE_Init(Section*);

#if defined(PCI_FUNCTIONALITY_ENABLED)
void PCI_Init(Section*);
#endif

void KEYBOARD_Init(Section*);	//TODO This should setup INT 16 too but ok ;)
void JOYSTICK_Init(Section*);
void SBLASTER_Init(Section*);
void PCSPEAKER_Init(Section*);
void TANDYSOUND_Init(Section*);
void LPT_DAC_Init(Section *);
void PS1AUDIO_Init(Section *);
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
	ZoneScoped;
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
		int32_t ratio = (ticksScheduled * (CPU_CyclePercUsed*90*1024/100/100)) / ticksDone;
		int32_t new_cmax = CPU_CycleMax;
		int64_t cproc = (int64_t)CPU_CycleMax * (int64_t)ticksScheduled;
		double ratioremoved = 0.0; //increase scope for logging
		if (cproc > 0) {
			/* ignore the cycles added due to the IO delay code in order
			   to have smoother auto cycle adjustments */
			ratioremoved = (double) CPU_IODelayRemoved / (double) cproc;
			if (ratioremoved < 1.0) {
				double ratio_not_removed = 1 - ratioremoved;
				ratio = (int32_t)((double)ratio * ratio_not_removed);

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
					new_cmax = 1 + static_cast<int32_t>(CPU_CycleMax * r);
				} else {
					int64_t ratio_with_removed = (int64_t) ((((double)ratio - 1024.0) * ratio_not_removed) + 1024.0);
					int64_t cmax_scaled = (int64_t)CPU_CycleMax * ratio_with_removed;
					new_cmax = (int32_t)(1 + (CPU_CycleMax >> 1) + cmax_scaled / (int64_t)2048);
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

	// Set the user's prefered MCB fault handling strategy
	DOS_SetMcbFaultStrategy(section->Get_string("mcb_fault_strategy"));

	// Convert the users video memory in either MB or KB to bytes
	const auto vmemsize_string = std::string(section->Get_string("vmemsize"));

	// If auto, then default to 0 and let the adapter's setup rountine set
	// the size
	auto vmemsize = vmemsize_string == "auto" ? 0 : std::stoi(vmemsize_string);

	// Scale up from MB-to-bytes or from KB-to-bytes
	vmemsize *= (vmemsize <= 8) ? 1024 * 1024 : 1024;
	vga.vmemsize = check_cast<uint32_t>(vmemsize);

	if (const auto pref = std::string_view(section->Get_string("vesa_modes")); pref == "compatible") {
		int10.vesa_mode_preference = VesaModePref::Compatible;
	} else if (pref == "halfline") {
		int10.vesa_mode_preference = VesaModePref::Halfline;
	} else {
		int10.vesa_mode_preference = VesaModePref::All;
	}

	VGA_SetRatePreference(section->Get_string("dos_rate"));
}

// Returns decimal seconds of elapsed uptime.
// The first call starts the uptime counter (and returns 0.0 seconds of uptime).
double DOSBOX_GetUptime()
{
	static auto start_ms = GetTicks();
	return GetTicksSince(start_ms) / millis_in_second;
}

void DOSBOX_Init()
{
	Section_prop *secprop;
	Prop_int *Pint;
	Prop_int *pint = nullptr;
	Prop_hex* Phex;
	Prop_string* Pstring; // use pstring when touching properties
	Prop_string *pstring;
	Prop_bool* Pbool;
	PropMultiValRemain* pmulti_remain;

	// Specifies if and when a setting can be changed
	constexpr auto always = Property::Changeable::Always;
	constexpr auto deprecated = Property::Changeable::Deprecated;
	constexpr auto only_at_start = Property::Changeable::OnlyAtStart;
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	constexpr auto changeable_at_runtime = true;

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
	        "Select a language to use: de, en, es, fr, it, nl, pl, and ru (unset by default)\n"
	        "Notes: This setting will override the 'LANG' environment, if set.\n"
	        "       The 'resources/translations' directory bundled with the executable holds\n"
	        "       these files. Please keep it along-side the executable to support this\n"
	        "       feature.");
	pstring = secprop->Add_string("machine", only_at_start, "svga_s3");
	pstring->Set_values(machines);
	pstring->Set_help(
	        "The type of machine DOSBox tries to emulate ('svga_s3' by default).");

	pstring = secprop->Add_path("captures", always, "capture");
	pstring->Set_help("Directory where audio, video, MIDI, and screenshot captures get saved\n"
	                  "('capture' by default).");

#if C_DEBUG
	LOG_StartUp();
#endif

	secprop->AddInitFunction(&IO_Init);
	secprop->AddInitFunction(&PAGING_Init);
	secprop->AddInitFunction(&MEM_Init);
	secprop->AddInitFunction(&HARDWARE_Init);
	pint = secprop->Add_int("memsize", when_idle, 16);
	pint->SetMinMax(MEM_GetMinMegabytes(), MEM_GetMaxMegabytes());
	pint->Set_help(
	        "Amount of memory of the emulated machine has in MB (16 by default).\n"
	        "Best leave at the default setting to avoid problems with some games,\n"
	        "though a few games might require a higher value.\n"
	        "There is generally no speed advantage when raising this value.");

	const char *mcb_fault_strategies[] = {"deny", "repair", "report", "allow", nullptr};
	pstring = secprop->Add_string("mcb_fault_strategy",
	                              only_at_start,
	                              mcb_fault_strategies[0]);
	pstring->Set_help(
	        "How software-corrupted memory chain blocks should be handled:\n"
	        "  deny:    Quit (and report) when faults are detected (default).\n"
	        "  repair:  Repair (and report) faults using adjacent chain blocks.\n"
	        "  report:  Report faults but otherwise proceed as-is.\n"
	        "  allow:   Allow faults to go unreported (hardware behavior).\n"
	        "The default ('deny') is recommended unless a game is failing with MCB\n"
	        "corruption errors.");
	pstring->Set_values(mcb_fault_strategies);

	const char *vmemsize_choices[] = {
	        "auto",
	        "1",
	        "2",
	        "4",
	        "8", // MB
	        "256",
	        "512",
	        "1024",
	        "2048",
	        "4096",
	        "8192",
	        0, // KB
	};
	pstring = secprop->Add_string("vmemsize", only_at_start, "auto");
	pstring->Set_values(vmemsize_choices);
	pstring->Set_help("Video memory in MB (1-8) or KB (256 to 8192). 'auto' uses the default per\n"
	                  "video adapter ('auto' by default).");

	pstring = secprop->Add_string("dos_rate", when_idle, "default");
	pstring->Set_help(
	        "Customize the emulated video mode's frame rate, in Hz:\n"
	        "  default:  The DOS video mode determines the rate (recommended; default).\n"
	        "  host:     Match the DOS rate to the host rate (see 'host_rate' setting).\n"
	        "  <value>:  Sets the rate to an exact value, between 24.000 and 1000.000 (Hz).\n"
	        "We recommend the 'default' rate; otherwise test and set on a per-game basis.");

	const char *vesa_modes_choices[] = {"compatible", "all", "halfline", 0};
	Pstring = secprop->Add_string("vesa_modes", only_at_start, "compatible");
	Pstring->Set_values(vesa_modes_choices);
	Pstring->Set_help(
	        "Controls the selection of VESA 1.2 and 2.0 modes offered:\n"
	        "  compatible:  A tailored selection that maximizes game compatibility.\n"
	        "               This is recommended along with 4 or 8 MB of video memory\n"
	        "               (default).\n"
	        "  halfline:    Supports the low-resolution halfline VESA 2.0 mode used by\n"
	        "               Extreme Assault. Use only if needed, as it's not S3 compatible.\n"
	        "  all:         Offers all modes for a given video memory size, however\n"
	        "               some games may not use them properly (flickering) or may need\n"
	        "               more system memory to use them.");

	Pbool = secprop->Add_bool("speed_mods", only_at_start, true);
	Pbool->Set_help(
	        "Permit changes known to improve performance (enabled by default).\n"
	        "Currently, no games are known to be negatively affected by this.\n"
	        "Please file a bug with the project if you find a game that fails\n"
	        "when this is enabled so we will list them here.");

	secprop->AddInitFunction(&CALLBACK_Init);
	secprop->AddInitFunction(&PIC_Init);
	secprop->AddInitFunction(&PROGRAMS_Init);
	secprop->AddInitFunction(&TIMER_Init);
	secprop->AddInitFunction(&CMOS_Init);

	const char *autoexec_section_choices[] = {
	        "join",
	        "overwrite",
	        0,
	};
	Pstring = secprop->Add_string("autoexec_section", only_at_start, "join");
	Pstring->Set_values(autoexec_section_choices);
	Pstring->Set_help(
	        "How autoexec sections are handled from multiple config files:\n"
	        "  join:       Combine them into one big section (legacy behavior; default).\n"
	        "  overwrite:  Use the last one encountered, like other config settings.");

	Pbool = secprop->Add_bool("automount", only_at_start, true);
	Pbool->Set_help(
	        "Mount 'drives/[c]' directories as drives on startup, where [c] is a lower-case\n"
	        "drive letter from 'a' to 'y' (enabled by default). The 'drives' folder can be\n"
	        "provided relative to the current directory or via built-in resources.\n"
	        "Mount settings can be optionally provided using a [c].conf file along-side\n"
	        "the drive's directory, with content as follows:\n"
	        "  [drive]\n"
	        "  type    = dir, overlay, floppy, or cdrom\n"
	        "  label   = custom_label\n"
	        "  path    = path-specification, ie: path = %%path%%;c:\\tools\n"
	        "  override_drive = mount the directory to this drive instead (default empty)\n"
	        "  verbose = true or false");

	const char *verbosity_choices[] = {
	        "auto", "high", "low", "quiet", 0,
	};
	Pstring = secprop->Add_string("startup_verbosity", only_at_start, "auto");
	Pstring->Set_values(verbosity_choices);
	Pstring->Set_help(
	        "Controls verbosity prior to displaying the program ('auto' by default):\n"
	        "  Verbosity   | Welcome | Early stdout\n"
	        "  high        |   yes   |    yes\n"
	        "  low         |   no    |    yes\n"
	        "  quiet       |   no    |    no\n"
	        "  auto        | 'low' if exec or dir is passed, otherwise 'high'");

	Pbool = secprop->Add_bool("allow_write_protected_files", only_at_start, true);
	Pbool->Set_help(
	        "Many games open all their files with writable permissions; even files that they\n"
	        "never modify. This setting lets you write-protect those files while still\n"
	        "allowing the game to read them. A second use-case: if you're using a copy-on-write\n"
	        "or network-based filesystem, this setting avoids triggering write-operations for\n"
	        "these write-protected files.");

	secprop = control->AddSection_prop("render", &RENDER_Init, changeable_at_runtime);
	secprop->AddEarlyInitFunction(&RENDER_InitShaderSource, changeable_at_runtime);

	pint = secprop->Add_int("frameskip", deprecated, 0);
	pint->Set_help("Consider capping frame-rates using the '[sdl] host_rate' setting.");

	Pbool = secprop->Add_bool("aspect", always, true);
	Pbool->Set_help(
	        "Scale the vertical resolution to produce a 4:3 display aspect ratio, matching\n"
	        "that of the original monitors the majority of DOS games were designed for\n"
	        "(enabled by default).\n"
	        "This setting only affects video modes that use non-square pixels, such as\n"
	        "320x200 or 640x400; whereas square-pixel modes, such as 640x480\n"
	        "and 800x600, are displayed as-is.");

	pstring = secprop->Add_string("monochrome_palette", always, "white");
	pstring->Set_help(
	        "Select default palette for monochrome display ('white' by default).\n"
	        "Works only when emulating 'hercules' or 'cga_mono'.\n"
	        "You can also cycle through available colours using F11.");
	const char* mono_pal[] = {"white", "paperwhite", "green", "amber", 0};
	pstring->Set_values(mono_pal);

	pstring = secprop->Add_string("cga_colors", only_at_start, "default");
	pstring->Set_help(
	        "Set the interpretation of CGA RGBI colours. Affects all machine types capable\n"
	        "of displaying CGA or better graphics. Built-in presets:\n"
	        "  default:       The canonical CGA palette, as emulated by VGA adapters\n"
	        "                 (default).\n"
	        "  tandy <bl>:    Emulation of an idealised Tandy monitor with adjustable brown\n"
	        "                 level. The brown level can be provided as an optional second\n"
	        "                 parameter (0 - red, 50 - brown, 100 - dark yellow;\n"
	        "                 defaults to 50). E.g. tandy 100\n"
	        "  tandy-warm:    Emulation of the actual colour output of an unknown Tandy\n"
	        "                 monitor.\n"
	        "  ibm5153 <c>:   Emulation of the actual colour output of an IBM 5153 monitor\n"
	        "                 with a unique contrast control that dims non-bright colours\n"
	        "                 only. The contrast can be optionally provided as a second\n"
	        "                 parameter (0 to 100; defaults to 100), e.g. ibm5153 60\n"
	        "  agi-amiga-v1, agi-amiga-v2, agi-amiga-v3:\n"
	        "                 Palettes used by the Amiga ports of Sierra AGI games.\n"
	        "  agi-amigaish:  A mix of EGA and Amiga colours used by the Sarien\n"
	        "                 AGI-interpreter.\n"
	        "  scumm-amiga:   Palette used by the Amiga ports of LucasArts EGA games.\n"
	        "  colodore:      Commodore 64 inspired colours based on the Colodore palette.\n"
	        "  colodore-sat:  Colodore palette with 20% more saturation.\n"
	        "  dga16:         A modern take on the canonical CGA palette with dialed back\n"
	        "                 contrast.\n"
	        "You can also set custom colours by specifying 16 space or comma separated\n"
	        "colour values, either as 3 or 6-digit hex codes (e.g. #f00 or #ff0000 for full red),\n"
	        "or decimal RGB triplets (e.g. (255, 0, 255) for magenta). The 16 colours are\n"
	        "ordered as follows:\n"
	        "  black, blue, green, cyan, red, magenta, brown, light-grey, dark-grey,\n"
	        "  light-blue, light-green, light-cyan, light-red, light-magenta, yellow, white.\n"
	        "Their default values, shown here in 6-digit hex code format, are:\n"
	        "  #000000 #0000aa #00aa00 #00aaaa #aa0000 #aa00aa #aa5500 #aaaaaa\n"
	        "  #555555 #5555ff #55ff55 #55ffff #ff5555 #ff55ff #ffff55 #ffffff");

	pstring = secprop->Add_string("scaler", deprecated, "none");
	pstring->Set_help(
	        "Software scalers are deprecated in favour of hardware-accelerated options:\n"
	        "  - If you used the normal2x/3x scalers, set a desired 'windowresolution'\n"
	        "    instead.\n"
	        "  - If you used an advanced scaler, consider one of the 'glshader'\n"
	        "    options instead.");

#if C_OPENGL
	pstring = secprop->Add_path("glshader", always, "default");
	pstring->Set_help(
	        "Set the GLSL shader to use in OpenGL output modes.\n"
			"Options include 'default', 'none', a shader listed using the --list-glshaders\n"
	        "command-line argument, or an absolute or relative path to a file.\n"
			"'default' sets the `interpolation/sharp.glsl` shader.\n"
	        "In all cases, you may omit the shader's '.glsl' file extension.");
#endif

	// Add the [composite] conf block after [render]
	assert(control);
	VGA_AddCompositeSettings(*control);

	secprop = control->AddSection_prop("cpu", &CPU_Init, changeable_at_runtime);
	const char* cores[] =
	{ "auto",
#if (C_DYNAMIC_X86) || (C_DYNREC)
	  "dynamic",
#endif
	  "normal",
	  "simple",
	  0 };
	Pstring = secprop->Add_string("core", when_idle, "auto");
	Pstring->Set_values(cores);
	Pstring->Set_help("CPU core used in emulation ('auto' by default). 'auto' will switch to dynamic\n"
	                  "if available and appropriate.");

	const char* cputype_values[] = { "auto", "386", "386_slow", "486_slow", "pentium_slow", "386_prefetch", 0};
	Pstring = secprop->Add_string("cputype", always, "auto");
	Pstring->Set_values(cputype_values);
	Pstring->Set_help("CPU type used in emulation ('auto' by default). 'auto' is the fastest choice.");


	pmulti_remain = secprop->AddMultiValRemain("cycles", always, " ");
	pmulti_remain->Set_help(
	        "Number of instructions DOSBox tries to emulate per millisecond\n"
	        "('auto' by default). Setting this value too high may result in sound drop-outs\n"
			"and lags.\n"
	        "Possible settings:\n"
	        "  auto:            Try to guess what a game needs. It usually works, but can\n"
	        "                   fail with certain games.\n"
	        "  fixed <number>:  Set a fixed number of cycles. This is what you usually\n"
	        "                   need if 'auto' fails (e.g. 'fixed 4000').\n"
	        "  max:             Allocate as much cycles as your computer is able to handle.");

	const char* cyclest[] = { "auto", "fixed", "max", "%u", 0 };
	Pstring = pmulti_remain->GetSection()->Add_string("type", always, "auto");
	pmulti_remain->SetValue("auto");
	Pstring->Set_values(cyclest);

	pmulti_remain->GetSection()->Add_string("parameters", always, "");

	Pint = secprop->Add_int("cycleup", always, 10);
	Pint->SetMinMax(1, 1000000);
	Pint->Set_help("Number of cycles added or subtracted with speed control hotkeys\n"
	               "(10 by default).");

	Pint = secprop->Add_int("cycledown", always, 20);
	Pint->SetMinMax(1, 1000000);
	Pint->Set_help("Setting it lower than 100 will be a percentage (20 by default).");

#if C_FPU
	secprop->AddInitFunction(&FPU_Init);
#endif
	secprop->AddInitFunction(&DMA_Init);
	secprop->AddInitFunction(&VGA_Init);
	secprop->AddInitFunction(&KEYBOARD_Init);


#if defined(PCI_FUNCTIONALITY_ENABLED)
	secprop=control->AddSection_prop("pci", &PCI_Init); // PCI bus
#endif

	// Configure mouse
	MOUSE_AddConfigSection(control);

	// Configure mixer
	MIXER_AddConfigSection(control);

	// Configure MIDI
	MIDI_AddConfigSection(control);

#if C_FLUIDSYNTH
	FLUID_AddConfigSection(control);
#endif

#if C_MT32EMU
	MT32_AddConfigSection(control);
#endif

#if C_DEBUG
	secprop = control->AddSection_prop("debug", &DEBUG_Init);
#endif

	secprop = control->AddSection_prop("sblaster",
	                                   &SBLASTER_Init,
	                                   changeable_at_runtime);

	const char* sbtypes[] = {"sb1", "sb2", "sbpro1", "sbpro2", "sb16", "gb", "none", 0};
	Pstring = secprop->Add_string("sbtype", when_idle, "sb16");
	Pstring->Set_values(sbtypes);
	Pstring->Set_help("Type of Sound Blaster to emulate ('sb16' by default).\n"
	                  "'gb' is Game Blaster.");

	const char *ios[] = {"220", "240", "260", "280", "2a0", "2c0", "2e0", "300", 0};
	Phex = secprop->Add_hex("sbbase", when_idle, 0x220);
	Phex->Set_values(ios);
	Phex->Set_help("The IO address of the Sound Blaster (220 by default).");

	const char *irqssb[] = {"3", "5", "7", "9", "10", "11", "12", 0};
	Pint = secprop->Add_int("irq", when_idle, 7);
	Pint->Set_values(irqssb);
	Pint->Set_help("The IRQ number of the Sound Blaster (7 by default).");

	const char *dmassb[] = {"0", "1", "3", "5", "6", "7", 0};
	Pint = secprop->Add_int("dma", when_idle, 1);
	Pint->Set_values(dmassb);
	Pint->Set_help("The DMA number of the Sound Blaster (1 by default).");

	Pint = secprop->Add_int("hdma", when_idle, 5);
	Pint->Set_values(dmassb);
	Pint->Set_help("The High DMA number of the Sound Blaster (5 by default).");

	Pbool = secprop->Add_bool("sbmixer", when_idle, true);
	Pbool->Set_help("Allow the Sound Blaster mixer to modify the DOSBox mixer (enabled by default).");

	pint = secprop->Add_int("sbwarmup", when_idle, 100);
	pint->Set_help(
	        "Silence initial DMA audio after card power-on, in milliseconds\n"
	        "(100 by default). This mitigates pops heard when starting many SB-based games.\n"
	        "Reduce this if you notice intial playback is missing audio.");
	pint->SetMinMax(0, 100);

	pint = secprop->Add_int("oplrate", deprecated, false);
	pint->Set_help("The OPL waveform is now sampled at the mixer's playback rate to avoid\n"
	               "resampling.");

	const char* oplmodes[] = {"auto", "cms", "opl2", "dualopl2", "opl3", "opl3gold", "none", 0};
	Pstring = secprop->Add_string("oplmode", when_idle, "auto");
	Pstring->Set_values(oplmodes);
	Pstring->Set_help("Type of OPL emulation ('auto' by default).\n"
	                  "On 'auto' the mode is determined by 'sbtype'.\n"
	                  "All OPL modes are AdLib-compatible, except for 'cms'.");

	Pstring = secprop->Add_string("oplemu", deprecated, "");
	Pstring->Set_help("Only 'nuked' OPL emulation is supported now.");

	Pstring = secprop->Add_string("sb_filter", when_idle, "modern");
	Pstring->Set_help(
	        "Type of filter to emulate for the Sound Blaster digital sound output:\n"
	        "  auto:      Use the appropriate filter determined by 'sbtype'.\n"
	        "  sb1, sb2, sbpro1, sbpro2, sb16:\n"
	        "             Use the filter of this Sound Blaster model.\n"
	        "  modern:    Use linear interpolation upsampling that acts as a low-pass\n"
	        "             filter; this is the legacy DOSBox behaviour (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  One or two custom filters in the following format:\n"
	        "               TYPE ORDER FREQ\n"
	        "             Where TYPE can be 'hpf' (high-pass) or 'lpf' (low-pass),\n"
	        "             ORDER is the order of the filter from 1 to 16\n"
	        "             (1st order = 6dB/oct slope, 2nd order = 12dB/oct, etc.),\n"
	        "             and FREQ is the cutoff frequency in Hz. Examples:\n"
	        "                lpf 2 12000\n"
	        "                hpf 3 120 lfp 1 6500");

	Pbool = secprop->Add_bool("sb_filter_always_on", when_idle, false);
	Pbool->Set_help("Force the Sound Blaster filter to be always on\n"
					"(disallow programs from turning the filter off; disabled by default).");

	Pstring = secprop->Add_string("opl_filter", when_idle, "auto");
	Pstring->Set_help(
	        "Type of filter to emulate for the Sound Blaster OPL output:\n"
	        "  auto:      Use the appropriate filter determined by 'sbtype' (default).\n"
	        "  sb1, sb2, sbpro1, sbpro2, sb16:\n"
	        "             Use the filter of this Sound Blaster model.\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	pstring = secprop->Add_string("cms_filter", when_idle, "on");
	pstring->Set_help(
	        "Filter for the Sound Blaster CMS output:\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	// Configure Gravis UltraSound emulation
	GUS_AddConfigSection(control);

	// Configure Innovation SSI-2001 emulation
	INNOVATION_AddConfigSection(control);

	// PC speaker emulation
	secprop = control->AddSection_prop("speaker",
	                                   &PCSPEAKER_Init,
	                                   changeable_at_runtime);

	const char *pcspeaker_models[] = {"discrete", "impulse", "none", "off", 0};
	pstring = secprop->Add_string("pcspeaker", when_idle, pcspeaker_models[0]);
	pstring->Set_help(
	        "PC speaker emulation model:\n"
	        "  discrete:  Waveform is created using discrete steps (default).\n"
	        "             Works well for games that use RealSound-type effects.\n"
	        "  impulse:   Waveform is created using sinc impulses.\n"
	        "             Recommended for square-wave games, like Commander Keen.\n"
	        "             While improving accuracy, it is more CPU intensive.\n"
	        "  none/off:  Don't use the PC Speaker.");
	pstring->Set_values(pcspeaker_models);

	pstring = secprop->Add_string("pcspeaker_filter", when_idle, "on");
	pstring->Set_help(
	        "Filter for the PC Speaker output:\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	pstring = secprop->Add_string("zero_offset", deprecated, "");
	pstring->Set_help(
	        "DC-offset is now eliminated globally from the master mixer output.");

	// Tandy audio emulation
	secprop->AddInitFunction(&TANDYSOUND_Init, changeable_at_runtime);

	const char *tandys[] = {"auto", "on", "off", 0};

	Pstring = secprop->Add_string("tandy", when_idle, "auto");
	Pstring->Set_values(tandys);
	Pstring->Set_help(
	        "Enable Tandy Sound System emulation ('auto' by default).\n"
	        "For 'auto', emulation is present only if machine is set to 'tandy'.");

	Pstring = secprop->Add_string("tandy_filter", when_idle, "on");
	Pstring->Set_help(
	        "Filter for the Tandy synth output:\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	Pstring = secprop->Add_string("tandy_dac_filter", when_idle, "on");
	Pstring->Set_help(
	        "Filter for the Tandy DAC output:\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	// LPT DAC device emulation
	secprop->AddInitFunction(&LPT_DAC_Init, changeable_at_runtime);
	const char *lpt_dac_types[] = {"none", "disney", "covox", "ston1", "off", 0};
	pstring = secprop->Add_string("lpt_dac", when_idle, lpt_dac_types[0]);
	pstring->Set_help("Type of DAC plugged into the parallel port:\n"
	                  "  disney:    Disney Sound Source.\n"
	                  "  covox:     Covox Speech Thing.\n"
	                  "  ston1:     Stereo-on-1 DAC, in stereo up to 30 kHz.\n"
	                  "  none/off:  Don't use a parallel port DAC (default).");
	pstring->Set_values(lpt_dac_types);

	pstring = secprop->Add_string("lpt_dac_filter", when_idle, "on");
	pstring->Set_help(
	        "Filter for the LPT DAC audio device(s):\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	// Deprecate the overloaded Disney setting
	Pbool = secprop->Add_bool("disney", deprecated, false);
	Pbool->Set_help("Use 'lpt_dac=disney' to enable the Disney Sound Source.");

	// IBM PS/1 Audio emulation
	secprop->AddInitFunction(&PS1AUDIO_Init, changeable_at_runtime);

	Pbool = secprop->Add_bool("ps1audio", when_idle, false);
	Pbool->Set_help("Enable IBM PS/1 Audio emulation (disabled by default).");

	Pstring = secprop->Add_string("ps1audio_filter", when_idle, "on");
	Pstring->Set_help(
	        "Filter for the PS/1 Audio synth output:\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	Pstring = secprop->Add_string("ps1audio_dac_filter", when_idle, "on");
	Pstring->Set_help("Filter for the PS/1 Audio DAC output:\n"
	                  "  on:        Filter the output (default).\n"
	                  "  off:       Don't filter the output.\n"
	                  "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	// ReelMagic Emulator
	secprop = control->AddSection_prop("reelmagic",
	                                   &ReelMagic_Init,
	                                   changeable_at_runtime);

	pstring = secprop->Add_string("reelmagic", when_idle, "off");
	pstring->Set_help(
	        "ReelMagic (aka REALmagic) MPEG playback support:\n"
	        "  off:       Disable support (default).\n"
	        "  cardonly:  Initialize the card without loading the FMPDRV.EXE driver.\n"
	        "  on:        Initialize the card and load the FMPDRV.EXE on start-up.");

	pstring = secprop->Add_string("reelmagic_key", when_idle, "auto");
	pstring->Set_help(
	        "Set the 32-bit magic key used to decode the game's videos:\n"
	        "  auto:      Use the built-in routines to determine the key (default).\n"
	        "  common:    Use the most commonly found key, which is 0x40044041.\n"
	        "  thehorde:  Use The Horde's key, which is 0xC39D7088.\n"
	        "  <custom>:  Set a custom key in hex format (e.g., 0x12345678).");

	pint = secprop->Add_int("reelmagic_fcode", when_idle, 0);
	pint->Set_help(
	        "Override the frame rate code used during video playback:\n"
	        "  0:       No override: attempt automatic rate discovery (default).\n"
	        "  1 to 7:  Override the frame rate to one the following (use 1 through 7):\n"
	        "           1=23.976, 2=24, 3=25, 4=29.97, 5=30, 6=50, or 7=59.94 FPS.");

	// Joystick emulation
	secprop=control->AddSection_prop("joystick", &BIOS_Init);

	secprop->AddInitFunction(&INT10_Init);
	secprop->AddInitFunction(&MOUSE_Init); // Must be after int10 as it uses CurMode
	secprop->AddInitFunction(&JOYSTICK_Init, changeable_at_runtime);
	const char *joytypes[] = {"auto", "2axis", "4axis",    "4axis_2", "fcs",
	                          "ch",   "hidden",  "disabled", 0};
	Pstring = secprop->Add_string("joysticktype", when_idle, "auto");
	Pstring->Set_values(joytypes);
	Pstring->Set_help(
	        "Type of joystick to emulate:\n"
	        "  auto:      Detect and use any joystick(s), if possible (default).\n"
	        "  2axis:     Support up to two joysticks, each with 2 axis\n"
	        "  4axis:     Support the first joystick only, as a 4-axis type.\n"
	        "  4axis_2:   Support the second joystick only, as a 4-axis type.\n"
	        "  fcs:       Emulate joystick as an original Thrustmaster FCS.\n"
	        "  ch:        Emulate joystick as an original CH Flightstick.\n"
	        "  hidden:    Prevent DOS from seeing the joystick(s), but enable them\n"
	        "             for mapping.\n"
	        "  disabled:  Fully disable joysticks: won't be polled, mapped,\n"
			"             or visible in DOS.\n"
	        "Remember to reset DOSBox's mapperfile if you saved it earlier.");

	Pbool = secprop->Add_bool("timed", when_idle, true);
	Pbool->Set_help("Enable timed intervals for axis (disabled by default).\n"
	                "Experiment with this option, if your joystick drifts away.");

	Pbool = secprop->Add_bool("autofire", when_idle, false);
	Pbool->Set_help("Fire continuously as long as the button is pressed\n"
	                "(disabled by default).");

	Pbool = secprop->Add_bool("swap34", when_idle, false);
	Pbool->Set_help("Swap the 3rd and the 4th axis (disabled by default).\n"
	                "Can be useful for certain joysticks.");

	Pbool = secprop->Add_bool("buttonwrap", when_idle, false);
	Pbool->Set_help("Enable button wrapping at the number of emulated buttons (disabled by default).");

	Pbool = secprop->Add_bool("circularinput", when_idle, false);
	Pbool->Set_help(
	        "Enable translation of circular input to square output (disabled by default).\n"
	        "Try enabling this if your left analog stick can only move in a circle.");

	Pint = secprop->Add_int("deadzone", when_idle, 10);
	Pint->SetMinMax(0, 100);
	Pint->Set_help("Percentage of motion to ignore (10 by default).\n"
	               "100 turns the stick into a digital one.");

	Pbool = secprop->Add_bool("use_joy_calibration_hotkeys", when_idle, false);
	Pbool->Set_help(
	        "Enable hotkeys to allow realtime calibration of the joystick's x and y axis\n"
	        "(disabled by default). Only consider this if in-game calibration fails and\n"
	        "other settings have been tried.\n"
	        "  - Ctrl/Cmd+Arrow-keys adjusts the axis' scalar value:\n"
	        "      - left and right diminish or magnify the x-axis scalar, respectively.\n"
	        "      - down and up diminish or magnify the y-axis scalar, respectively.\n"
	        "  - Alt+Arrow-keys adjusts the axis' offset position:\n"
	        "      - left and right shift x-axis offset in the given direction.\n"
	        "      - down and up shift the y-axis offset in the given direction.\n"
	        "  - Reset the X and Y calibration using Ctrl+Delete and Ctrl+Home,\n"
			"    respectively.\n"
	        "Each tap will report X or Y calibration values you can set below. When you find\n"
	        "parameters that work, quit the game, switch this setting back to false, and\n"
	        "populate the reported calibration parameters.");

	pstring = secprop->Add_string("joy_x_calibration", when_idle, "auto");
	pstring->Set_help(
	        "Apply x-axis calibration parameters from the hotkeys ('auto' by default).");

	pstring = secprop->Add_string("joy_y_calibration", when_idle, "auto");
	pstring->Set_help(
	        "Apply Y-axis calibration parameters from the hotkeys ('auto' by default).");

	secprop = control->AddSection_prop("serial", &SERIAL_Init, changeable_at_runtime);
	const char* serials[] = {
	        "dummy", "disabled", "mouse", "modem", "nullmodem", "direct", 0};

	pmulti_remain = secprop->AddMultiValRemain("serial1", when_idle, " ");
	Pstring = pmulti_remain->GetSection()->Add_string("type", when_idle, "dummy");
	pmulti_remain->SetValue("dummy");
	Pstring->Set_values(serials);
	pmulti_remain->GetSection()->Add_string("parameters", when_idle, "");
	pmulti_remain->Set_help(
	        "Set type of device connected to COM port.\n"
	        "Can be disabled, dummy, mouse, modem, nullmodem, direct ('dummy' by default).\n"
	        "Additional parameters must be on the same line in the form of\n"
	        "parameter:value. Parameter for all types is irq (optional).\n"
	        "  - for 'mouse':      model, overrides setting from [mouse] section\n"
	        "  - for 'direct':     realport (required), rxdelay (optional).\n"
	        "                      (realport:COM1 realport:ttyS0).\n"
	        "  - for 'modem':      listenport, sock, baudrate (all optional).\n"
	        "  - for 'nullmodem':  server, rxdelay, txdelay, telnet, usedtr,\n"
	        "                      transparent, port, inhsocket, sock (all optional).\n"
	        "SOCK parameter specifies the protocol to be used by both sides\n"
	        "of the conection. 0 for TCP and 1 for ENet reliable UDP.\n"
	        "Example: serial1=modem listenport:5000 sock:1");

	pmulti_remain = secprop->AddMultiValRemain("serial2", when_idle, " ");
	Pstring = pmulti_remain->GetSection()->Add_string("type", when_idle, "dummy");
	pmulti_remain->SetValue("dummy");
	Pstring->Set_values(serials);
	pmulti_remain->GetSection()->Add_string("parameters", when_idle, "");
	pmulti_remain->Set_help("See 'serial1'");

	pmulti_remain = secprop->AddMultiValRemain("serial3", when_idle, " ");
	Pstring = pmulti_remain->GetSection()->Add_string("type", when_idle, "disabled");
	pmulti_remain->SetValue("disabled");
	Pstring->Set_values(serials);
	pmulti_remain->GetSection()->Add_string("parameters", when_idle, "");
	pmulti_remain->Set_help("See 'serial1'");

	pmulti_remain = secprop->AddMultiValRemain("serial4", when_idle, " ");
	Pstring = pmulti_remain->GetSection()->Add_string("type", when_idle, "disabled");
	pmulti_remain->SetValue("disabled");
	Pstring->Set_values(serials);
	pmulti_remain->GetSection()->Add_string("parameters", when_idle, "");
	pmulti_remain->Set_help("See 'serial1'");

	pstring = secprop->Add_path("phonebookfile", only_at_start, "phonebook.txt");
	pstring->Set_help("File used to map fake phone numbers to addresses\n"
	                  "('phonebook.txt' by default).");

	/* All the DOS Related stuff, which will eventually
	 * start up in the shell */
	secprop = control->AddSection_prop("dos", &DOS_Init);
	secprop->AddInitFunction(&XMS_Init, changeable_at_runtime);
	Pbool = secprop->Add_bool("xms", when_idle, true);
	Pbool->Set_help("Enable XMS support (enabled by default).");

	secprop->AddInitFunction(&EMS_Init, changeable_at_runtime);
	const char* ems_settings[] = {"true", "emsboard", "emm386", "false", 0};
	Pstring = secprop->Add_string("ems", when_idle, "true");
	Pstring->Set_values(ems_settings);
	Pstring->Set_help(
	        "Enable EMS support (enabled by default). Enabled provides the best\n"
	        "compatibility but certain applications may run better with other choices,\n"
	        "or require EMS support to be disabled to work at all.");

	Pbool = secprop->Add_bool("umb", when_idle, true);
	Pbool->Set_help("Enable UMB support (enabled by default).");

	pstring = secprop->Add_string("ver", when_idle, "5.0");
	pstring->Set_help("Set DOS version (5.0 by default). Specify in major.minor format.\n"
	                  "A single number is treated as the major version.\n"
	                  "Common settings are 3.3, 5.0, 6.22, and 7.1.");

	pint = secprop->Add_int("country", when_idle, 0);
	pint->Set_help("Set DOS country code (0 by default).\n"
	               "This affects country-specific information such as date, time, and decimal\n"
	               "formats. If set to 0, the country code corresponding to the selected keyboard\n"
	               "layout will be used.");

	Pbool = secprop->Add_bool("expand_shell_variable", when_idle, false);
	Pbool->Set_help("Enable expanding environment variables such as %PATH% in the DOS command shell\n"
	                "(disabled by default).\n"
	                "FreeDOS and MS-DOS 7/8 COMMAND.COM supports this behavior.");

	secprop->AddInitFunction(&DOS_KeyboardLayout_Init, changeable_at_runtime);
	Pstring = secprop->Add_string("keyboardlayout", when_idle, "auto");
	Pstring->Set_help("Language code of the keyboard layout, or 'auto' ('auto' by default).");

	// Mscdex
	secprop->AddInitFunction(&MSCDEX_Init);
	secprop->AddInitFunction(&DRIVES_Init);
	secprop->AddInitFunction(&CDROM_Image_Init);
#if C_IPX
	secprop = control->AddSection_prop("ipx", &IPX_Init, changeable_at_runtime);
	Pbool   = secprop->Add_bool("ipx", when_idle, false);
	Pbool->Set_help("Enable IPX over UDP/IP emulation (enabled by default).");
#endif

#if C_SLIRP
	secprop = control->AddSection_prop("ethernet", &NE2K_Init, changeable_at_runtime);

	Pbool = secprop->Add_bool("ne2000", when_idle,  true);
	Pbool->Set_help(
	        "Enable emulation of a Novell NE2000 network card on a software-based\n"
	        "network (using libslirp) with properties as follows (enabled by default):\n"
	        "  - 255.255.255.0:  Subnet mask of the 10.0.2.0 virtual LAN.\n"
	        "  - 10.0.2.2:       IP of the gateway and DHCP service.\n"
	        "  - 10.0.2.3:       IP of the virtual DNS server.\n"
	        "  - 10.0.2.15:      First IP provided by DHCP, your IP!\n"
	        "Notes: Inside DOS, setting this up requires an NE2000 packet driver, DHCP\n"
	        "       client, and TCP/IP stack. You might need port-forwarding from the host\n"
	        "       into the DOS guest, and from your router to your host when acting as the\n"
	        "       server for multiplayer games.");

	const char *nic_addresses[] = {"200", "220", "240", "260", "280", "2c0",
	                               "300", "320", "340", "360", 0};
	Phex = secprop->Add_hex("nicbase", when_idle, 0x300);
	Phex->Set_values(nic_addresses);
	Phex->Set_help(
	        "Base address of the NE2000 card (300 by default).\n"
	        "Notes: Addresses 220 and 240 might not be available as they're assigned to the\n"
	        "       Sound Blaster and Gravis UltraSound by default.");

	const char *nic_irqs[] = {"3",  "4",  "5",  "9",  "10",
	                          "11", "12", "14", "15", 0};
	Pint = secprop->Add_int("nicirq", when_idle, 3);
	Pint->Set_values(nic_irqs);
	Pint->Set_help("The interrupt used by the NE2000 card (3 by default).\n"
	               "Notes: IRQs 3 and 5 might not be available as they're assigned to\n"
	               "       'serial2' and the Gravis UltraSound by default.");

	Pstring = secprop->Add_string("macaddr", when_idle, "AC:DE:48:88:99:AA");
	Pstring->Set_help("The MAC address of the NE2000 card ('AC:DE:48:88:99:AA' by default).");

	Pstring = secprop->Add_string("tcp_port_forwards", when_idle, "");
	Pstring->Set_help(
	        "Forward one or more TCP ports from the host into the DOS guest\n"
			"(unset by default).\n"
	        "The format is:\n"
	        "  port1  port2  port3 ... (e.g., 21 80 443)\n"
	        "  This will forward FTP, HTTP, and HTTPS into the DOS guest.\n"
	        "If the ports are privileged on the host, a mapping can be used\n"
	        "  host:guest  ..., (e.g., 8021:21 8080:80)\n"
	        "  This will forward ports 8021 and 8080 to FTP and HTTP in the guest\n"
	        "A range of adjacent ports can be abbreviated with a dash:\n"
	        "  start-end ... (e.g., 27910-27960)\n"
	        "  This will forward ports 27910 to 27960 into the DOS guest.\n"
	        "Mappings and ranges can be combined, too:\n"
	        "  hstart-hend:gstart-gend ..., (e.g, 8040-8080:20-60)\n"
	        "  This forwards ports 8040 to 8080 into 20 to 60 in the guest\n"
	        "Notes: If mapped ranges differ, the shorter range is extended to fit.\n"
	        "       If conflicting host ports are given, only the first one is setup.\n"
	        "       If conflicting guest ports are given, the latter rule takes precedent.");

	Pstring = secprop->Add_string("udp_port_forwards", when_idle, "");
	Pstring->Set_help("Forward one or more UDP ports from the host into the DOS guest\n"
	                  "(unset by default). The format is the same as for TCP port forwards.");
#endif

	//	secprop->AddInitFunction(&CREDITS_Init);

	//TODO ?
	control->AddSection_line("autoexec", &AUTOEXEC_Init);
	MSG_Add("AUTOEXEC_CONFIGFILE_HELP",
		"Lines in this section will be run at startup.\n"
		"You can put your MOUNT lines here.\n"
	);
	MSG_Add("CONFIGFILE_INTRO",
	        "# This is the configuration file for " CANONICAL_PROJECT_NAME " (%s).\n"
	        "# Lines starting with a '#' character are comments.\n");
	MSG_Add("CONFIG_VALID_VALUES", "Possible values");

	// Initialize the uptime counter when launching the first shell. This
	// ensures that slow-performing configurable tasks (like loading MIDI
	// SF2 files) have already been performed and won't affect this time.
	(void)DOSBOX_GetUptime();

	control->SetStartUp(&SHELL_Init);
}
