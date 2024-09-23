/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
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
#include "capture/capture.h"
#include "control.h"
#include "cpu.h"
#include "cross.h"
#include "debug.h"
#include "dos/dos_locale.h"
#include "dos_inc.h"
#include "hardware.h"
#include "inout.h"
#include "ints/int10.h"
#include "mapper.h"
#include "math_utils.h"
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

void LOG_StartUp();
void MEM_Init(Section *);
void PAGING_Init(Section *);
void IO_Init(Section * );
void CALLBACK_Init(Section*);
void PROGRAMS_Init(Section*);
//void CREDITS_Init(Section*);
void VGA_Init(Section*);

void DOS_Init(Section*);


void CPU_Init(Section*);

#if C_FPU
void FPU_Init(Section*);
#endif

void DMA_Init(Section*);

void PCI_Init(Section*);
void VOODOO_Init(Section*);
void VIRTUALBOX_Init(Section*);
void VMWARE_Init(Section*);

//TODO This should setup INT 16 too but ok ;)
void KEYBOARD_Init(Section*);

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

// DOS internals mostly
void EMS_Init(Section*);
void XMS_Init(Section*);

void AUTOEXEC_Init(Section*);
void SHELL_Init();

void INT10_Init(Section*);

static LoopHandler * loop;

static struct {
	int64_t remain    = {};
	int64_t last      = {};
	int64_t added     = {};
	int64_t done      = {};
	int64_t scheduled = {};
	bool locked       = {};
} ticks = {};

int64_t DOSBOX_GetTicksDone()
{
	return ticks.done;
}

void DOSBOX_SetTicksDone(const int64_t ticks_done)
{
	ticks.done = ticks_done;
}

void DOSBOX_SetTicksScheduled(const int64_t ticks_scheduled)
{
	ticks.scheduled = ticks_scheduled;
}

bool mono_cga = false;

void Null_Init([[maybe_unused]] Section *sec) {
	// do nothing
}

// forward declaration
static void increase_ticks();

static Bitu Normal_Loop()
{
	Bits ret;

	while (true) {
		if (PIC_RunQueue()) {
			ret = (*cpudecoder)();
			if (ret < 0) {
				return 1;
			}
			if (ret > 0) {
				if (ret >= CB_MAX) {
					return 0;
				}
				Bitu blah = (*CallBack_Handlers[ret])();
				if (blah) {
					return blah;
				}
			}
#if C_DEBUG
			if (DEBUG_ExitLoop()) {
				return 0;
			}
#endif
		} else {
			if (!GFX_Events()) {
				return 0;
			}
			if (ticks.remain > 0) {
				TIMER_AddTick();
				--ticks.remain;
			} else {
				increase_ticks();
				return 0;
			}
		}
	}
}

constexpr auto auto_cpu_cycles_min = 200;

static void increase_ticks()
{
	// Make it return ticks.remain and set it in the function above to
	// remove the global variable.
	ZoneScoped;

	// For fast-forward mode
	if (ticks.locked) {
		ticks.remain = 5;

		// Reset any auto cycle guessing for this frame
		ticks.last      = GetTicks();
		ticks.added     = 0;
		ticks.done      = 0;
		ticks.scheduled = 0;
		return;
	}

	constexpr auto MicrosInMillisecond = 1000;

	const auto ticks_new_us = GetTicksUs();
	const auto ticks_new    = ticks_new_us / MicrosInMillisecond;

	ticks.scheduled += ticks.added;

	// Lower should not be possible, only equal
	if (ticks_new <= ticks.last) {
		ticks.added = 0;

		static int64_t cumulative_time_slept_us = 0;

		constexpr auto sleep_duration = std::chrono::microseconds(1000);
		std::this_thread::sleep_for(sleep_duration);

		const auto time_slept_us = GetTicksUsSince(ticks_new_us);
		cumulative_time_slept_us += time_slept_us;

		// Update ticks.done with the total time spent sleeping
		if (cumulative_time_slept_us >= MicrosInMillisecond) {
			// 1 tick == 1 millisecond
			const auto cumulative_ticks_slept = cumulative_time_slept_us /
			                                    MicrosInMillisecond;
			ticks.done -= cumulative_ticks_slept;

			// Keep the fractional microseconds part
			cumulative_time_slept_us %= MicrosInMillisecond;
		}

		if (ticks.done < 0) {
			ticks.done = 0;
		}

		// If we do work this tick and sleep till the next tick, then
		// ticks.done is decreased, despite the fact that work was done
		// as well in this tick. Maybe make it depend on an extra
		// parameter. What do we know: ticks.remain = 0 (condition to
		// enter this function) ticks_new = time before sleeping

		// Maybe keep track of slept time in this frame, and use slept
		// and done as indicators, and take care of the fact there are
		// frames that have both.

		return;
	}

	// ticks_new > ticks.last
	ticks.remain = GetTicksDiff(ticks_new, ticks.last);
	ticks.last   = ticks_new;
	ticks.done += ticks.remain;

	if (ticks.remain > 20) {
#if 0
		LOG(LOG_MISC,LOG_ERROR)("large remain %d", ticks.remain);
#endif
		ticks.remain = 20;
	}

	ticks.added = ticks.remain;

	// Is the system in auto cycle guessing mode? If not, do nothing.
	if (!CPU_CycleAutoAdjust) {
		return;
	}

	if (ticks.scheduled >= 100 || ticks.done >= 100 ||
	    (ticks.added > 15 && ticks.scheduled >= 5)) {

		if (ticks.done < 1) {
			// Protect against division by zero.
			ticks.done = 1;
		}

		// Ratio we are aiming for is 100% usage
		auto ratio = (ticks.scheduled * (CPU_CyclePercUsed * 1024 / 100)) /
		             ticks.done;

		auto new_cycle_max = CPU_CycleMax.load();

		auto cproc = static_cast<int64_t>(CPU_CycleMax) *
		             static_cast<int64_t>(ticks.scheduled);

		// Increase scope for logging
		auto ratio_removed = 0.0;

		// TODO This is full of magic constants with zero explanation.
		// Where are they coming from, what do the signify, why these
		// numbers and not some other numbers, etc.--there's lots of
		// explaining to do here.

		if (cproc > 0) {
			// Ignore the cycles added due to the IO delay code in
			// order to have smoother auto cycle adjustments.
			ratio_removed = static_cast<double>(CPU_IODelayRemoved) /
			                static_cast<double>(cproc);

			if (ratio_removed < 1.0) {
				double ratio_not_removed = 1 - ratio_removed;
				ratio = ifloor(static_cast<double>(ratio) *
				               ratio_not_removed);

				// Don't allow very high ratios; they can cause
				// locking as we don't scale down for very low
				// ratios. High ratios might be the result of
				// timing resolution.
				if (ticks.scheduled >= 100 && ticks.done < 10 &&
				    ratio > 16384) {
					ratio = 16384;
				}

				// Limit the ratio even more when the cycles are
				// already way above the realmode default.
				if (ticks.scheduled >= 100 && ticks.done < 10 &&
				    ratio > 5120 && CPU_CycleMax > 50000) {
					ratio = 5120;
				}

				// When downscaling multiple times in a row,
				// ensure a minimum amount of downscaling
				if (ticks.added > 15 && ticks.scheduled >= 5 &&
				    ticks.scheduled <= 20 && ratio > 800) {
					ratio = 800;
				}

				if (ratio <= 1024) {
					// Uncommenting the below line restores
					// the old formula: ratio_not_removed
					// = 1.0;

					auto r = (1.0 + ratio_not_removed) /
					         (ratio_not_removed +
					          1024.0 / (static_cast<double>(ratio)));

					new_cycle_max = static_cast<int>(CPU_CycleMax * r + 1);
				} else {
					auto ratio_with_removed = static_cast<int64_t>(
					        ((static_cast<double>(ratio) - 1024.0) *
					         ratio_not_removed) +
					        1024.0);

					auto cmax_scaled = check_cast<int64_t>(
					        CPU_CycleMax * ratio_with_removed);

					new_cycle_max = static_cast<int>(
					        1 + (CPU_CycleMax / 2) +
					        cmax_scaled / 2048LL);
				}
			}
		}

		if (new_cycle_max < auto_cpu_cycles_min) {
			new_cycle_max = auto_cpu_cycles_min;
		}

#if 0
		LOG_INFO("CPU: cycles: curr %7d | new %7d | ratio %5d | done %3d | sched %3d | add %3d | rem %4.2f",
		         CPU_CycleMax,
		         new_cycle_max,
		         ratio,
		         ticks.done,
		         ticks.scheduled,
		         ticks.added,
		         ratio_removed);
#endif

		// Ratios below 1% are considered to be dropouts due to
		// temporary load imbalance, the cycles adjusting is skipped.
		if (ratio > 10) {

			// Ratios below 12% along with a large time since the
			// last update has taken place are most likely caused by
			// heavy load through a different application, the
			// cycles adjusting is skipped as well.
			if ((ratio > 120) || (ticks.done < 700)) {
				CPU_CycleMax = new_cycle_max;

				if (CPU_CycleLimit > 0) {
					if (CPU_CycleMax > CPU_CycleLimit) {
						CPU_CycleMax = CPU_CycleLimit;
					}
				} else if (CPU_CycleMax > CpuCyclesMax) {
					// Hardcoded limit if the limit wasn't
					// explicitly specified.
					CPU_CycleMax = CpuCyclesMax;
				}
			}
		}

		// Reset cycle guessing parameters.
		CPU_IODelayRemoved = 0;
		ticks.done         = 0;
		ticks.scheduled    = 0;

	} else if (ticks.added > 15) {
		// ticks.added > 15 but ticks.scheduled < 5, lower the cycles
		// but do not reset the scheduled/done ticks to take them into
		// account during the next auto cycle adjustment.
		CPU_CycleMax = CPU_CycleMax / 3;

		if (CPU_CycleMax < auto_cpu_cycles_min) {
			CPU_CycleMax = auto_cpu_cycles_min;
		}
	}
}

const char* DOSBOX_GetVersion() noexcept
{
	static constexpr char version[] = DOSBOX_VERSION;
	return version;
}

const char* DOSBOX_GetDetailedVersion() noexcept
{
	static constexpr char version[] = DOSBOX_VERSION " (" BUILD_GIT_HASH ")";
	return version;
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
		ticks.locked = true;
		MIXER_EnableFastForwardMode();

		if (CPU_CycleAutoAdjust) {
			autoadjust = true;
			CPU_CycleAutoAdjust = false;
			CPU_CycleMax = CPU_CycleMax / 3;
			if (CPU_CycleMax<1000) CPU_CycleMax=1000;
		}
	} else {
		LOG_MSG("Fast Forward OFF");
		ticks.locked = false;
		MIXER_DisableFastForwardMode();

		if (autoadjust) {
			autoadjust = false;
			CPU_CycleAutoAdjust = true;
		}
	}
}

void DOSBOX_SetMachineTypeFromConfig(Section_prop* section)
{
	const auto arguments = &control->arguments;
	if (!arguments->machine.empty()) {
		//update value in config (else no matching against suggested values
		section->HandleInputline(std::string("machine=") +
		                         arguments->machine);
	}

	std::string mtype = section->Get_string("machine");
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
	} else {
		E_Exit("DOSBOX: Invalid machine type '%s'", mtype.c_str());
	}

	// VGA-type machine needs an valid SVGA card and vice-versa
	assert((machine == MCH_VGA && svgaCard != SVGA_None) ||
	       (machine != MCH_VGA && svgaCard == SVGA_None));
}

static void DOSBOX_RealInit(Section* sec)
{
	Section_prop* section = static_cast<Section_prop*>(sec);

	// Initialize some dosbox internals
	ticks.remain = 0;
	ticks.last   = GetTicks();
	ticks.locked = false;

	DOSBOX_SetLoop(&Normal_Loop);

	MAPPER_AddHandler(DOSBOX_UnlockSpeed, SDL_SCANCODE_F12, MMOD2, "speedlock", "Speedlock");

	DOSBOX_SetMachineTypeFromConfig(section);

	// Set the user's prefered MCB fault handling strategy
	DOS_SetMcbFaultStrategy(section->Get_string("mcb_fault_strategy").c_str());

	// Convert the users video memory in either MB or KB to bytes
	const std::string vmemsize_string = section->Get_string("vmemsize");

	// If auto, then default to 0 and let the adapter's setup rountine set
	// the size
	auto vmemsize = vmemsize_string == "auto" ? 0 : std::stoi(vmemsize_string);

	// Scale up from MB-to-bytes or from KB-to-bytes
	vmemsize *= (vmemsize <= 8) ? 1024 * 1024 : 1024;
	vga.vmemsize = check_cast<uint32_t>(vmemsize);

	const std::string pref = section->Get_string("vesa_modes");
	if (pref == "compatible") {
		int10.vesa_modes = VesaModes::Compatible;
	} else if (pref == "halfline") {
		int10.vesa_modes = VesaModes::Halfline;
	} else {
		int10.vesa_modes = VesaModes::All;
	}

	VGA_SetRatePreference(section->Get_string("dos_rate"));
}

// Returns decimal seconds of elapsed uptime.
// The first call starts the uptime counter (and returns 0.0 seconds of uptime).
double DOSBOX_GetUptime()
{
	static auto start_ms = GetTicks();
	return GetTicksSince(start_ms) / MillisInSecond;
}

void DOSBOX_Init()
{
	Section_prop* secprop             = nullptr;
	Prop_bool* pbool                  = nullptr;
	Prop_int* pint                    = nullptr;
	Prop_hex* phex                    = nullptr;
	Prop_string* pstring              = nullptr;
	PropMultiValRemain* pmulti_remain = nullptr;

	// Specifies if and when a setting can be changed
	// constexpr auto always     = Property::Changeable::Always;
	constexpr auto deprecated    = Property::Changeable::Deprecated;
	constexpr auto only_at_start = Property::Changeable::OnlyAtStart;
	constexpr auto when_idle     = Property::Changeable::WhenIdle;

	constexpr auto changeable_at_runtime = true;

	/* Setup all the different modules making up DOSBox */

	secprop = control->AddSection_prop("dosbox", &DOSBOX_RealInit);
	pstring = secprop->Add_string("language", only_at_start, "");
	pstring->Set_help(
	        "Select the DOS messages language (unset by default; this results in 'auto'):\n"
	        "  auto:     Tries to detect the language from the host OS (default).\n"
	        "  <value>:  Loads a translation from the given file.\n"
	        "Notes:\n"
	        "  - Currently the following language files are available:\n"
	        "    'br', 'de', 'en', 'es', 'fr', 'it', 'nl', 'pl' and 'ru'.\n"
	        "  - The English is built-in, remaining ones are stored in the bundled\n"
	        "    'resources/translations' directory.");

	pstring = secprop->Add_string("machine", only_at_start, "svga_s3");
	pstring->Set_values({"hercules",
	                     "cga_mono",
	                     "cga",
	                     "pcjr",
	                     "tandy",
	                     "ega",
	                     "svga_s3",
	                     "svga_et3000",
	                     "svga_et4000",
	                     "svga_paradise",
	                     "vesa_nolfb",
	                     "vesa_oldvbe"});

	pstring->SetDeprecatedWithAlternateValue("vgaonly", "svga_paradise");
	pstring->Set_help(
	        "Set the video adapter or machine to emulate:\n"
	        "  hercules:       Hercules Graphics Card (HGC) (see 'monochrome_palette').\n"
	        "  cga_mono:       CGA adapter connected to a monochrome monitor (see\n"
	        "                  'monochrome_palette').\n"
	        "  cga:            IBM Color Graphics Adapter (CGA). Also enables composite\n"
	        "                  video emulation (see [composite] section).\n"
	        "  pcjr:           An IBM PCjr machine. Also enables PCjr sound and composite\n"
	        "                  video emulation (see [composite] section).\n"
	        "  tandy:          A Tandy 1000 machine with TGA graphics. Also enables Tandy\n"
	        "                  sound and composite video emulation (see [composite]\n"
	        "                  section).\n"
	        "  ega:            IBM Enhanced Graphics Adapter (EGA).\n"
	        "  svga_paradise:  Paradise PVGA1A SVGA card (no VESA VBE; 512K vmem by default,\n"
	        "                  can be set to 256K or 1MB with 'vmemsize'). This is the\n"
	        "                  closest to IBM's original VGA adapter.\n"
	        "  svga_et3000:    Tseng Labs ET3000 SVGA card (no VESA VBE; fixed 512K vmem).\n"
	        "  svga_et4000:    Tseng Labs ET4000 SVGA card (no VESA VBE; 1MB vmem by\n"
	        "                  default, can be set to 256K or 512K with 'vmemsize').\n"
	        "  svga_s3:        S3 Trio64 (VESA VBE 2.0; 4MB vmem by default, can be set to\n"
	        "                  512K, 1MB, 2MB, or 8MB with 'vmemsize') (default)\n"
	        "  vesa_oldvbe:    Same as 'svga_s3' but limited to VESA VBE 1.2.\n"
	        "  vesa_nolfb:     Same as 'svga_s3' (VESA VBE 2.0), plus the \"no linear\n"
	        "                  framebuffer\" hack (needed only by a few games).");

	pstring = secprop->Add_path("captures", deprecated, "capture");
	pstring->Set_help("Moved to [capture] section and renamed to 'capture_dir'.");

#if C_DEBUG
	LOG_StartUp();
#endif

	secprop->AddInitFunction(&IO_Init);
	secprop->AddInitFunction(&PAGING_Init);
	secprop->AddInitFunction(&MEM_Init);

	pint = secprop->Add_int("memsize", when_idle, 16);
	pint->SetMinMax(MEM_GetMinMegabytes(), MEM_GetMaxMegabytes());
	pint->Set_help(
	        "Amount of memory of the emulated machine has in MB (16 by default).\n"
	        "Best leave at the default setting to avoid problems with some games,\n"
	        "though a few games might require a higher value.\n"
	        "There is generally no speed advantage when raising this value.");

	pstring = secprop->Add_string("mcb_fault_strategy", only_at_start, "repair");
	pstring->Set_help(
	        "How software-corrupted memory chain blocks should be handled:\n"
	        "  repair:  Repair (and report) faults using adjacent blocks (default).\n"
	        "  report:  Report faults but otherwise proceed as-is.\n"
	        "  allow:   Allow faults to go unreported (hardware behavior).\n"
	        "  deny:    Quit (and report) when faults are detected.");

	pstring->Set_values({"repair", "report", "allow", "deny"});

	static_assert(8192 * 1024 <= PciGfxLfbLimit - PciGfxLfbBase);
	pstring = secprop->Add_string("vmemsize", only_at_start, "auto");

	pstring->Set_values({"auto",
	                     // values in MB
	                     "1",
	                     "2",
	                     "4",
	                     "8",
	                     // values in KB
	                     "256",
	                     "512",
	                     "1024",
	                     "2048",
	                     "4096",
	                     "8192"});
	pstring->Set_help(
	        "Video memory in MB (1-8) or KB (256 to 8192). 'auto' uses the default for\n"
	        "the selected video adapter ('auto' by default). See the 'machine' setting for\n"
	        "the list of valid options and defaults per adapter.");

	pstring = secprop->Add_string("vmem_delay", only_at_start, "off");
	pstring->Set_help(
	        "Set video memory access delay emulation ('off' by default).\n"
	        "  off:      Disable video memory access delay emulation (default).\n"
	        "            This is preferable for most games to avoid slowdowns.\n"
	        "  on:       Enable video memory access delay emulation (3000 ns).\n"
	        "            This can help reduce or eliminate flicker in Hercules,\n"
	        "            CGA, EGA, and early VGA games.\n"
	        "  <value>:  Set access delay in nanoseconds. Valid range is 0 to 20000 ns;\n"
	        "            500 to 5000 ns is the most useful range.\n"
	        "Note: Only set this on a per-game basis when necessary as it slows down\n"
	        "      the whole emulator.");

	pstring = secprop->Add_string("dos_rate", when_idle, "default");
	pstring->Set_help(
	        "Customize the emulated video mode's frame rate.\n"
	        "  default:  The DOS video mode determines the rate (default).\n"
	        "  host:     Match the DOS rate to the host rate (see 'host_rate' setting).\n"
	        "  <value>:  Sets the rate to an exact value in between 24.000 and 1000.000 Hz.\n"
	        "Note: We recommend the 'default' rate, otherwise test and set on a per-game\n"
	        "      basis.");

	pstring = secprop->Add_string("vesa_modes", only_at_start, "compatible");
	pstring->Set_values({"compatible", "all", "halfline"});
	pstring->Set_help(
	        "Controls which VESA video modes are available:\n"
	        "  compatible:  Only the most compatible VESA modes for the configured video\n"
	        "               memory size (default). Recommended with 4 or 8 MB of video\n"
	        "               memory ('vmemsize') for the widest compatiblity with games.\n"
	        "               320x200 high colour modes are excluded as they were not\n"
	        "               properly supported until the late '90s. The 256-colour linear\n"
	        "               framebuffer 320x240, 400x300, and 512x384 modes are also\n"
	        "               excluded as they cause timing problems in Build Engine games.\n"
	        "  halfline:    Same as 'compatible', but the 120h VESA mode is replaced with\n"
	        "               a special halfline mode used by Extreme Assault. Use only if\n"
	        "               needed.\n"
	        "  all:         All modes are available, including extra DOSBox-specific VESA\n"
	        "               modes. Use 8 MB of video memory for the best results. Some\n"
	        "               games misbehave in the presence of certain VESA modes; try\n"
	        "               'compatible' mode if this happens. The 320x200 high colour\n"
	        "               modes available in this mode are often required by late '90s\n"
	        "               demoscene productions.");

	pbool = secprop->Add_bool("vga_8dot_font", only_at_start, false);
	pbool->Set_help("Use 8-pixel-wide fonts on VGA adapters (disabled by default).");

	pbool = secprop->Add_bool("vga_render_per_scanline", only_at_start, true);
	pbool->Set_help(
	        "Emulate accurate per-scanline VGA rendering (enabled by default).\n"
	        "Currently, you need to disable this for a few games, otherwise they will crash\n"
	        "at startup (e.g., Deus, Ishar 3, Robinson's Requiem, Time Warriors).");

	pbool = secprop->Add_bool("speed_mods", only_at_start, true);
	pbool->Set_help(
	        "Permit changes known to improve performance (enabled by default).\n"
	        "Currently, no games are known to be negatively affected by this.\n"
	        "Please file a bug with the project if you find a game that fails\n"
	        "when this is enabled so we will list them here.");

	secprop->AddInitFunction(&CALLBACK_Init);
	secprop->AddInitFunction(&PIC_Init);
	secprop->AddInitFunction(&PROGRAMS_Init);
	secprop->AddInitFunction(&TIMER_Init);
	secprop->AddInitFunction(&CMOS_Init);

	pstring = secprop->Add_string("autoexec_section", only_at_start, "join");
	pstring->Set_values({"join", "overwrite"});
	pstring->Set_help(
	        "How autoexec sections are handled from multiple config files:\n"
	        "  join:       Combine them into one big section (legacy behavior; default).\n"
	        "  overwrite:  Use the last one encountered, like other config settings.");

	pbool = secprop->Add_bool("automount", only_at_start, true);
	pbool->Set_help(
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
	        "  verbose = true or false\n"
	        "  readonly = true or false");

	pstring = secprop->Add_string("startup_verbosity", only_at_start, "auto");
	pstring->Set_values({"auto", "high", "low", "quiet"});
	pstring->Set_help(
	        "Controls verbosity prior to displaying the program ('auto' by default):\n"
	        "  Verbosity   | Welcome | Early stdout\n"
	        "  high        |   yes   |    yes\n"
	        "  low         |   no    |    yes\n"
	        "  quiet       |   no    |    no\n"
	        "  auto        | 'low' if exec or dir is passed, otherwise 'high'");

	pbool = secprop->Add_bool("allow_write_protected_files", only_at_start, true);
	pbool->Set_help(
	        "Many games open all their files with writable permissions; even files that they\n"
	        "never modify. This setting lets you write-protect those files while still\n"
	        "allowing the game to read them (enabled by default). A second use-case: if\n"
	        "you're using a copy-on-write or network-based filesystem, this setting avoids\n"
	        "triggering write operations for these write-protected files.");

	pbool = secprop->Add_bool("shell_config_shortcuts", when_idle, true);
	pbool->Set_help(
	        "Allow shortcuts for simpler configuration management (enabled by default).\n"
	        "E.g., instead of 'config -set sbtype sb16', it is enough to execute\n"
	        "'sbtype sb16', and instead of 'config -get sbtype', you can just execute\n"
	        "the 'sbtype' command.");

	// Configure render settings
	RENDER_AddConfigSection(control);

	// Configure composite video settings
	VGA_AddCompositeSettings(*control);

	// Configure CPU settings
	CPU_AddConfigSection(control);

#if C_FPU
	secprop->AddInitFunction(&FPU_Init);
#endif
	secprop->AddInitFunction(&DMA_Init);
	secprop->AddInitFunction(&VGA_Init);
	secprop->AddInitFunction(&KEYBOARD_Init);
	secprop->AddInitFunction(&PCI_Init); // PCI bus

	secprop = control->AddSection_prop("voodoo", &VOODOO_Init);

	pbool = secprop->Add_bool("voodoo", when_idle, true);
	pbool->Set_help("Enable 3dfx Voodoo emulation (enabled by default).");

	pstring = secprop->Add_string("voodoo_memsize", only_at_start, "4");
	pstring->Set_values({"4", "12"});
	pstring->Set_help(
	        "Set the amount of video memory for 3dfx Voodoo graphics, either 4 or 12 MB.\n"
	        "The memory is used by the Frame Buffer Interface (FBI) and Texture Mapping Unit\n"
	        "(TMU) as follows:\n"
	        "   4: 2 MB for the FBI and one TMU with 2 MB (default).\n"
	        "  12: 4 MB for the FBI and two TMUs, each with 4 MB.");

	// Deprecate the boolean Voodoo multithreading setting
	pbool = secprop->Add_bool("voodoo_multithreading", deprecated, false);
	pbool->Set_help("Renamed to 'voodoo_threads'");

	pstring = secprop->Add_string("voodoo_threads", only_at_start, "auto");
	pstring->Set_help(
	        "Use threads to improve 3dfx Voodoo performance:\n"
	        "  auto:     Use up to 8 threads based on available CPU cores (default).\n"
	        "  <value>:  Set a specific number of threads between 1 and 16.\n"
	        "Note: Tests show that frame rates increase up to 8 threads after which\n"
	        "      they level off or decrease, in general.");

	pbool = secprop->Add_bool("voodoo_bilinear_filtering", only_at_start, false);
	pbool->Set_help(
	        "Use bilinear filtering to emulate the 3dfx Voodoo's texture smoothing effect\n"
	        "(disabled by default). Only suggested if you have a fast desktop-class CPU, as\n"
	        "it can impact frame rates on slower systems.");

	// Configure capture
	CAPTURE_AddConfigSection(control);

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

	// Configure Sound Blaster and ESS
	SB_AddConfigSection(control);

	// Configure CMS/Game Blaster, OPL and ESFM
	// Must be called after SB_AddConfigSection
	OPL_AddConfigSettings(control);

	// Configure Gravis UltraSound emulation
	GUS_AddConfigSection(control);

	// Configure the IBM Music Feature emulation
	IMFC_AddConfigSection(control);

	// Configure Innovation SSI-2001 emulation
	INNOVATION_AddConfigSection(control);

	// PC speaker emulation
	secprop = control->AddSection_prop("speaker",
	                                   &PCSPEAKER_Init,
	                                   changeable_at_runtime);

	pstring = secprop->Add_string("pcspeaker", when_idle, "impulse");
	pstring->Set_help(
	        "PC speaker emulation model:\n"
	        "  impulse:   A very faithful emulation of the PC speaker's output (default).\n"
	        "             Works with most games, but may result in garbled sound or silence\n"
	        "             in a small number of programs.\n"
	        "  discrete:  Legacy simplified PC speaker emulation; only use this on specific\n"
	        "             titles that give you problems with the 'impulse' model.\n"
	        "  none/off:  Don't emulate the PC speaker.");
	pstring->Set_values({"impulse", "discrete", "none", "off"});

	pstring = secprop->Add_string("pcspeaker_filter", when_idle, "on");
	pstring->Set_help(
	        "Filter for the PC speaker output:\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	pstring = secprop->Add_string("zero_offset", deprecated, "");
	pstring->Set_help(
	        "DC-offset is now eliminated globally from the master mixer output.");

	// Tandy audio emulation
	secprop->AddInitFunction(&TANDYSOUND_Init, changeable_at_runtime);

	pstring = secprop->Add_string("tandy", when_idle, "auto");
	pstring->Set_values({"auto", "on", "psg", "off"});
	pstring->Set_help(
	        "Set the Tandy/PCjr 3 Voice sound emulation:\n"
	        "  auto:  Automatically enable Tandy/PCjr sound for the 'tandy' and 'pcjr'\n"
	        "         machine types only (default).\n"
	        "  on:    Enable Tandy/PCjr sound with DAC support, when possible.\n"
	        "         Most games also need the machine set to 'tandy' or 'pcjr' to work.\n"
	        "  psg:   Only enable the card's three-voice programmable sound generator\n"
	        "         without DAC to avoid conflicts with other cards using DMA 1.\n"
	        "  off:   Disable Tandy/PCjr sound.");

	pstring = secprop->Add_string("tandy_fadeout", when_idle, "off");
	pstring->Set_help(
	        "Fade out the Tandy synth output after the last IO port write:\n"
	        "  off:       Don't fade out; residual output will play forever (default).\n"
	        "  on:        Wait 0.5s before fading out over a 0.5s period.\n"
	        "  <custom>:  Custom fade out definition; see 'opl_fadeout' for details.");

	pstring = secprop->Add_string("tandy_filter", when_idle, "on");
	pstring->Set_help(
	        "Filter for the Tandy synth output:\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	pstring = secprop->Add_string("tandy_dac_filter", when_idle, "on");
	pstring->Set_help(
	        "Filter for the Tandy DAC output:\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	// LPT DAC device emulation
	secprop->AddInitFunction(&LPT_DAC_Init, changeable_at_runtime);
	pstring = secprop->Add_string("lpt_dac", when_idle, "none");
	pstring->Set_help(
	        "Type of DAC plugged into the parallel port:\n"
	        "  disney:    Disney Sound Source.\n"
	        "  covox:     Covox Speech Thing.\n"
	        "  ston1:     Stereo-on-1 DAC, in stereo up to 30 kHz.\n"
	        "  none/off:  Don't use a parallel port DAC (default).");
	pstring->Set_values({"none", "disney", "covox", "ston1", "off"});

	pstring = secprop->Add_string("lpt_dac_filter", when_idle, "on");
	pstring->Set_help(
	        "Filter for the LPT DAC audio device(s):\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	// Deprecate the overloaded Disney setting
	pbool = secprop->Add_bool("disney", deprecated, false);
	pbool->Set_help("Use 'lpt_dac = disney' to enable the Disney Sound Source.");

	// IBM PS/1 Audio emulation
	secprop->AddInitFunction(&PS1AUDIO_Init, changeable_at_runtime);

	pbool = secprop->Add_bool("ps1audio", when_idle, false);
	pbool->Set_help("Enable IBM PS/1 Audio emulation (disabled by default).");

	pstring = secprop->Add_string("ps1audio_filter", when_idle, "on");
	pstring->Set_help(
	        "Filter for the PS/1 Audio synth output:\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	pstring = secprop->Add_string("ps1audio_dac_filter", when_idle, "on");
	pstring->Set_help(
	        "Filter for the PS/1 Audio DAC output:\n"
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
	        "  on:        Initialize the card and load the FMPDRV.EXE on startup.");

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
	secprop = control->AddSection_prop("joystick", &BIOS_Init);

	secprop->AddInitFunction(&INT10_Init);
	secprop->AddInitFunction(&MOUSE_Init); // Must be after int10 as it uses
	                                       // CurMode
	secprop->AddInitFunction(&JOYSTICK_Init, changeable_at_runtime);
	pstring = secprop->Add_string("joysticktype", when_idle, "auto");

	pstring->Set_values(
	        {"auto", "2axis", "4axis", "4axis_2", "fcs", "ch", "hidden", "disabled"});

	pstring->Set_help(
	        "Type of joystick to emulate:\n"
	        "  auto:      Detect and use any joystick(s), if possible (default).\n"
	        "             Joystick emulation is disabled if no joystick is found.\n"
	        "  2axis:     Support up to two joysticks, each with 2 axis.\n"
	        "  4axis:     Support the first joystick only, as a 4-axis type.\n"
	        "  4axis_2:   Support the second joystick only, as a 4-axis type.\n"
	        "  fcs:       Emulate joystick as an original Thrustmaster FCS.\n"
	        "  ch:        Emulate joystick as an original CH Flightstick.\n"
	        "  hidden:    Prevent DOS from seeing the joystick(s), but enable them\n"
	        "             for mapping.\n"
	        "  disabled:  Fully disable joysticks: won't be polled, mapped,\n"
	        "             or visible in DOS.\n"
	        "Remember to reset DOSBox's mapperfile if you saved it earlier.");

	pbool = secprop->Add_bool("timed", when_idle, true);
	pbool->Set_help(
	        "Enable timed intervals for axis (enabled by default).\n"
	        "Experiment with this option, if your joystick drifts away.");

	pbool = secprop->Add_bool("autofire", when_idle, false);
	pbool->Set_help(
	        "Fire continuously as long as the button is pressed\n"
	        "(disabled by default).");

	pbool = secprop->Add_bool("swap34", when_idle, false);
	pbool->Set_help(
	        "Swap the 3rd and the 4th axis (disabled by default).\n"
	        "Can be useful for certain joysticks.");

	pbool = secprop->Add_bool("buttonwrap", when_idle, false);
	pbool->Set_help("Enable button wrapping at the number of emulated buttons (disabled by default).");

	pbool = secprop->Add_bool("circularinput", when_idle, false);
	pbool->Set_help(
	        "Enable translation of circular input to square output (disabled by default).\n"
	        "Try enabling this if your left analog stick can only move in a circle.");

	pint = secprop->Add_int("deadzone", when_idle, 10);
	pint->SetMinMax(0, 100);
	pint->Set_help(
	        "Percentage of motion to ignore (10 by default).\n"
	        "100 turns the stick into a digital one.");

	pbool = secprop->Add_bool("use_joy_calibration_hotkeys", when_idle, false);
	pbool->Set_help(
	        "Enable hotkeys to allow realtime calibration of the joystick's X and Y axes\n"
	        "(disabled by default). Only consider this if in-game calibration fails and\n"
	        "other settings have been tried.\n"
	        "  - Ctrl/Cmd+Arrow-keys adjust the axis' scalar value:\n"
	        "      - Left and Right diminish or magnify the x-axis scalar, respectively.\n"
	        "      - Down and Up diminish or magnify the y-axis scalar, respectively.\n"
	        "  - Alt+Arrow-keys adjust the axis' offset position:\n"
	        "      - Left and Right shift X-axis offset in the given direction.\n"
	        "      - Down and Up shift the Y-axis offset in the given direction.\n"
	        "  - Reset the X and Y calibration using Ctrl+Delete and Ctrl+Home,\n"
	        "    respectively.\n"
	        "Each tap will report X or Y calibration values you can set below. When you find\n"
	        "parameters that work, quit the game, switch this setting back to disabled, and\n"
	        "populate the reported calibration parameters.");

	pstring = secprop->Add_string("joy_x_calibration", when_idle, "auto");
	pstring->Set_help(
	        "Apply X-axis calibration parameters from the hotkeys ('auto' by default).");

	pstring = secprop->Add_string("joy_y_calibration", when_idle, "auto");
	pstring->Set_help(
	        "Apply Y-axis calibration parameters from the hotkeys ('auto' by default).");

	secprop = control->AddSection_prop("serial", &SERIAL_Init, changeable_at_runtime);
	const std::vector<std::string> serials = {
	        "dummy", "disabled", "mouse", "modem", "nullmodem", "direct"};

	pmulti_remain = secprop->AddMultiValRemain("serial1", when_idle, " ");
	pstring = pmulti_remain->GetSection()->Add_string("type", when_idle, "dummy");
	pmulti_remain->SetValue("dummy");
	pstring->Set_values(serials);
	pmulti_remain->GetSection()->Add_string("parameters", when_idle, "");
	pmulti_remain->Set_help(
	        "Set type of device connected to the COM1 port.\n"
	        "Can be disabled, dummy, mouse, modem, nullmodem, direct ('dummy' by default).\n"
	        "Additional parameters must be on the same line in the form of\n"
	        "parameter:value. The optional 'irq' parameter is common for all types.\n"
	        "  - for 'mouse':      model (overrides the setting from the [mouse] section)\n"
	        "  - for 'direct':     realport (required), rxdelay (optional).\n"
	        "                      (e.g., realport:COM1, realport:ttyS0).\n"
	        "  - for 'modem':      listenport, sock, bps (all optional).\n"
	        "  - for 'nullmodem':  server, rxdelay, txdelay, telnet, usedtr,\n"
	        "                      transparent, port, inhsocket, sock (all optional).\n"
	        "The 'sock' parameter specifies the protocol to use at both sides of the\n"
	        "connection. Valid values are 0 for TCP, and 1 for ENet reliable UDP.\n"
	        "Example: serial1=modem listenport:5000 sock:1");

	pmulti_remain = secprop->AddMultiValRemain("serial2", when_idle, " ");
	pstring = pmulti_remain->GetSection()->Add_string("type", when_idle, "dummy");
	pmulti_remain->SetValue("dummy");
	pstring->Set_values(serials);
	pmulti_remain->GetSection()->Add_string("parameters", when_idle, "");
	pmulti_remain->Set_help("See 'serial1' ('dummy' by default).");

	pmulti_remain = secprop->AddMultiValRemain("serial3", when_idle, " ");
	pstring = pmulti_remain->GetSection()->Add_string("type", when_idle, "disabled");
	pmulti_remain->SetValue("disabled");
	pstring->Set_values(serials);
	pmulti_remain->GetSection()->Add_string("parameters", when_idle, "");
	pmulti_remain->Set_help("See 'serial1' ('disabled' by default).");

	pmulti_remain = secprop->AddMultiValRemain("serial4", when_idle, " ");
	pstring = pmulti_remain->GetSection()->Add_string("type", when_idle, "disabled");
	pmulti_remain->SetValue("disabled");
	pstring->Set_values(serials);
	pmulti_remain->GetSection()->Add_string("parameters", when_idle, "");
	pmulti_remain->Set_help("See 'serial1' ('disabled' by default).");

	pstring = secprop->Add_path("phonebookfile", only_at_start, "phonebook.txt");
	pstring->Set_help(
	        "File used to map fake phone numbers to addresses\n"
	        "('phonebook.txt' by default).");

	// All the general DOS Related stuff, on real machines mostly located in
	// CONFIG.SYS

	secprop = control->AddSection_prop("dos", &DOS_Init);
	secprop->AddInitFunction(&XMS_Init, changeable_at_runtime);
	pbool = secprop->Add_bool("xms", when_idle, true);
	pbool->Set_help("Enable XMS support (enabled by default).");

	secprop->AddInitFunction(&EMS_Init, changeable_at_runtime);
	pstring = secprop->Add_string("ems", when_idle, "true");
	pstring->Set_values({"true", "emsboard", "emm386", "false"});
	pstring->Set_help(
	        "Enable EMS support (enabled by default). Enabled provides the best\n"
	        "compatibility but certain applications may run better with other choices,\n"
	        "or require EMS support to be disabled to work at all.");

	pbool = secprop->Add_bool("umb", when_idle, true);
	pbool->Set_help("Enable UMB support (enabled by default).");

	pstring = secprop->Add_string("pcjr_memory_config", only_at_start, "expanded");
	pstring->Set_values({"expanded", "standard"});
	pstring->Set_help(
	        "PCjr memory layout ('expanded' by default).\n"
	        "  expanded:  640 KB total memory with applications residing above 128 KB.\n"
	        "             Compatible with most games.\n"
	        "  standard:  128 KB total memory with applications residing below 96 KB.\n"
	        "             Required for some older games (e.g., Jumpman, Troll).");

	pstring = secprop->Add_string("ver", when_idle, "5.0");
	pstring->Set_help(
	        "Set DOS version (5.0 by default). Specify in major.minor format.\n"
	        "A single number is treated as the major version.\n"
	        "Common settings are 3.3, 5.0, 6.22, and 7.1.");

	// DOS locale settings

	secprop->AddInitFunction(&DOS_Locale_Init, changeable_at_runtime);

	pstring = secprop->Add_string("locale_period", when_idle, "modern");
	pstring->Set_help(
	        "Set locale epoch ('native' by default).\n"
	        "  historic:  if data is available for the given country, tries to mimic old DOS\n"
	        "             behavior when displaying time, dates, or numbers\n"
	        "  modern:    follow current day practices for user experience more consistent\n"
	        "             with typical host systems\n"
	        "  native:    tries to re-use current host OS settings, regardless of the country\n"
	        "             set; uses 'modern' data to fill-in the gaps, like when the DOS\n"
	        "             locale system is too limited to follow the desktop settings");
	pstring->Set_values({"historic", "modern", "native"});

	pstring = secprop->Add_string("country", when_idle, "auto");
	pstring->Set_help(
	        "Set DOS country code ('auto' by default).\n"
	        "This affects country-specific information such as date, time, and decimal\n"
	        "formats. If set to 'auto', it tries to set the country code reflecting\n"
	        "the host OS settings.\n"
	        "The list of country codes can be displayed using '--list-countries'\n"
	        "command-line argument.");

	pstring = secprop->Add_string("keyboardlayout", deprecated, "");
	pstring->Set_help("Renamed to 'keyboard_layout'.");

	pstring = secprop->Add_string("keyboard_layout", only_at_start, "");
	pstring->Set_help(
	        "Keyboard layout code (unset by default; this results in 'auto').\n"
	        "The list of supported keyboard layout codes can be displayed using\n"
	        "'--list-layouts' command-line argument, i. e. 'uk' is the British English\n"
	        "layout. The layout can be followed by the code page number, i.e. 'uk 850'\n"
	        "selects a Western-European screen font.\n"
	        "After startup, use DOS command 'KEYB' to manipulate or get information about\n"
	        "keyboard layout and code page - type 'HELP KEYB' for details.");

	// COMMAND.COM settings

	pstring = secprop->Add_string("expand_shell_variable", when_idle, "auto");
	pstring->Set_values({"auto", "true", "false"});
	pstring->Set_help(
	        "Enable expanding environment variables such as %%PATH%% in the DOS command shell\n"
	        "(auto by default, enabled if DOS version >= 7.0).\n"
	        "FreeDOS and MS-DOS 7/8 COMMAND.COM supports this behavior.");

	pstring = secprop->Add_path("shell_history_file",
	                            only_at_start,
	                            "shell_history.txt");

	pstring->Set_help(
	        "File containing persistent command line history ('shell_history.txt'\n"
	        "by default). Setting it to empty disables persistent shell history.");

	// Misc DOS command settings

	pstring = secprop->Add_path("setver_table_file", only_at_start, "");
	pstring->Set_help(
	        "File containing the list of applications and assigned DOS versions, in a\n"
	        "tab-separated format, used by SETVER.EXE as a persistent storage\n"
	        "(empty by default).");

	// Mscdex
	secprop->AddInitFunction(&MSCDEX_Init);
	secprop->AddInitFunction(&DRIVES_Init);
	secprop->AddInitFunction(&CDROM_Image_Init);

#if C_IPX
	secprop = control->AddSection_prop("ipx", &IPX_Init, changeable_at_runtime);
#else
	secprop = control->AddInactiveSectionProp("ipx");
#endif
	pbool = secprop->Add_bool("ipx", when_idle, false);
	pbool->SetOptionHelp("Enable IPX over UDP/IP emulation (disabled by default).");
#if C_IPX
	pbool->SetEnabledOptions({"ipx"});
#endif

#if C_SLIRP
	secprop = control->AddSection_prop("ethernet", &NE2K_Init, changeable_at_runtime);
#else
	secprop = control->AddInactiveSectionProp("ethernet");
#endif

	pbool = secprop->Add_bool("ne2000", when_idle, true);
	pbool->SetOptionHelp(
	        "SLIRP",
	        "Enable emulation of a Novell NE2000 network card on a software-based\n"
	        "network (using libslirp) with properties as follows (enabled by default):\n"
	        "  - 255.255.255.0:  Subnet mask of the 10.0.2.0 virtual LAN.\n"
	        "  - 10.0.2.2:       IP of the gateway and DHCP service.\n"
	        "  - 10.0.2.3:       IP of the virtual DNS server.\n"
	        "  - 10.0.2.15:      First IP provided by DHCP, your IP!\n"
	        "Note: Inside DOS, setting this up requires an NE2000 packet driver, DHCP\n"
	        "      client, and TCP/IP stack. You might need port-forwarding from the host\n"
	        "      into the DOS guest, and from your router to your host when acting as the\n"
	        "      server for multiplayer games.");
#if C_SLIRP
	pbool->SetEnabledOptions({"SLIRP"});
#endif

	phex = secprop->Add_hex("nicbase", when_idle, 0x300);
	phex->Set_values(
	        {"200", "220", "240", "260", "280", "2c0", "300", "320", "340", "360"});
	phex->SetOptionHelp("SLIRP",
	                    "Base address of the NE2000 card (300 by default).\n"
	                    "Note: Addresses 220 and 240 might not be available as they're assigned to the\n"
	                    "      Sound Blaster and Gravis UltraSound by default.");
#if C_SLIRP
	phex->SetEnabledOptions({"SLIRP"});
#endif

	pint = secprop->Add_int("nicirq", when_idle, 3);
	pint->Set_values({"3", "4", "5", "9", "10", "11", "12", "14", "15"});
	pint->SetOptionHelp("SLIRP",
	                    "The interrupt used by the NE2000 card (3 by default).\n"
	                    "Note: IRQs 3 and 5 might not be available as they're assigned to\n"
	                    "      'serial2' and the Gravis UltraSound by default.");
#if C_SLIRP
	pint->SetEnabledOptions({"SLIRP"});
#endif

	pstring = secprop->Add_string("macaddr", when_idle, "AC:DE:48:88:99:AA");
	pstring->SetOptionHelp("SLIRP",
	                       "The MAC address of the NE2000 card ('AC:DE:48:88:99:AA' by default).");
#if C_SLIRP
	pstring->SetEnabledOptions({"SLIRP"});
#endif

	pstring = secprop->Add_string("tcp_port_forwards", when_idle, "");
	pstring->SetOptionHelp(
	        "SLIRP",
	        "Forward one or more TCP ports from the host into the DOS guest\n"
	        "(unset by default).\n"
	        "The format is:\n"
	        "  port1  port2  port3 ... (e.g., 21 80 443)\n"
	        "  This will forward FTP, HTTP, and HTTPS into the DOS guest.\n"
	        "If the ports are privileged on the host, a mapping can be used\n"
	        "  host:guest  ..., (e.g., 8021:21 8080:80)\n"
	        "  This will forward ports 8021 and 8080 to FTP and HTTP in the guest.\n"
	        "A range of adjacent ports can be abbreviated with a dash:\n"
	        "  start-end ... (e.g., 27910-27960)\n"
	        "  This will forward ports 27910 to 27960 into the DOS guest.\n"
	        "Mappings and ranges can be combined, too:\n"
	        "  hstart-hend:gstart-gend ..., (e.g, 8040-8080:20-60)\n"
	        "  This forwards ports 8040 to 8080 into 20 to 60 in the guest.\n"
	        "Notes:\n"
	        "  - If mapped ranges differ, the shorter range is extended to fit.\n"
	        "  - If conflicting host ports are given, only the first one is setup.\n"
	        "  - If conflicting guest ports are given, the latter rule takes precedent.");
#if C_SLIRP
	pstring->SetEnabledOptions({"SLIRP"});
#endif

	pstring = secprop->Add_string("udp_port_forwards", when_idle, "");
	pstring->SetOptionHelp(
	        "SLIRP",
	        "Forward one or more UDP ports from the host into the DOS guest\n"
	        "(unset by default). The format is the same as for TCP port forwards.");
#if C_SLIRP
	pstring->SetEnabledOptions({"SLIRP"});
#endif

	//	secprop->AddInitFunction(&CREDITS_Init);

	// VMM interfaces
	secprop->AddInitFunction(&VIRTUALBOX_Init);
	secprop->AddInitFunction(&VMWARE_Init);

	// TODO ?
	control->AddSection_line("autoexec", &AUTOEXEC_Init);

	MSG_Add("AUTOEXEC_CONFIGFILE_HELP",
	        "Each line in this section is executed at startup as a DOS command.\n"
	        "Important: The [autoexec] section must be the last section in the config!");

	MSG_Add("CONFIGFILE_INTRO",
	        "# This is the configuration file for " DOSBOX_PROJECT_NAME
	        " (%s).\n"
	        "# Lines starting with a '#' character are comments.\n");

	MSG_Add("CONFIG_VALID_VALUES", "Possible values");
	MSG_Add("CONFIG_DEPRECATED_VALUES", "Deprecated values");

	// Initialize the uptime counter when launching the first shell. This
	// ensures that slow-performing configurable tasks (like loading MIDI
	// SF2 files) have already been performed and won't affect this time.
	DOSBOX_GetUptime();

	control->SetStartUp(&SHELL_Init);
}
