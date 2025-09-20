// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <thread>
#include <unistd.h>

#include "audio/disk_noise.h"
#include "audio/mixer.h"
#include "capture/capture.h"
#include "config/config.h"
#include "config/setup.h"
#include "cpu/callback.h"
#include "cpu/cpu.h"
#include "cpu/paging.h"
#include "debugger/debugger.h"
#include "dos/dos.h"
#include "dos/dos_locale.h"
#include "dos/programs.h"
#include "gui/common.h"
#include "gui/mapper.h"
#include "gui/render.h"
#include "hardware/audio/gus.h"
#include "hardware/audio/opl.h"
#include "hardware/audio/sblaster.h"
#include "hardware/audio/speaker.h"
#include "hardware/cmos.h"
#include "hardware/hardware.h"
#include "hardware/input/joystick.h"
#include "hardware/input/joystick.h"
#include "hardware/input/joystick.h"
#include "hardware/input/mouse.h"
#include "hardware/memory.h"
#include "hardware/network/ipx.h"
#include "hardware/network/ne2000.h"
#include "hardware/pci_bus.h"
#include "hardware/pic.h"
#include "hardware/port.h"
#include "hardware/serialport/serialport.h"
#include "hardware/timer.h"
#include "hardware/video/reelmagic/reelmagic.h"
#include "hardware/video/voodoo.h"
#include "ints/int10.h"
#include "midi/midi.h"
#include "misc/cross.h"
#include "misc/support.h"
#include "misc/tracy.h"
#include "misc/video.h"
#include "network/ethernet.h"
#include "shell/shell.h"
#include "utils/math_utils.h"

MachineType machine   = MachineType::None;
SvgaType    svga_type = SvgaType::None;

void LOG_StartUp();

void DEBUG_Init(Section*);

void AUTOEXEC_Init(Section*);
void SHELL_Init();

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

void Null_Init([[maybe_unused]] Section *sec) {
	// do nothing
}

// forward declaration
static void increase_ticks();

static Bitu normal_loop()
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
				Bitu result = (*Callback_Handlers[ret])();
				if (result) {
					return result;
				}
			}
#if C_DEBUGGER
			if (DEBUG_ExitLoop()) {
				return 0;
			}
#endif
		} else {
			// In 'host-rate' presentation mode, this effectively
			// accomplishes polling at the sub-millisecond level for
			// presenting the frame.
			//
			// We're effectively implementing cooperative
			// multitasking here to present the frame at roughly the
			// right time as `GFX_MaybePresentFrame()` is called
			// around 2-5 times per tick (1 ms) depending on the
			// cycles setting.
			//
			// This is a good-enough alternative to moving the
			// entire emulation off the main thread and then
			// presenting the last-rendered frame at regular
			// intervals from the main thread.
			//
			if (GFX_GetPresentationMode() == PresentationMode::HostRate) {
				GFX_MaybePresentFrame();
			}

			if (!DOSBOX_PollAndHandleEvents()) {
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

		auto new_cycle_max = CPU_CycleMax;

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
		CPU_CycleMax /= 3;

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

void DOSBOX_SetLoop(LoopHandler* handler)
{
	loop = handler;
}

void DOSBOX_SetNormalLoop()
{
	loop = normal_loop;
}

static bool is_shutdown_requested = false;

void DOSBOX_RunMachine()
{
	while ((*loop)() == 0 && !is_shutdown_requested)
		;
}

void DOSBOX_RequestShutdown()
{
	is_shutdown_requested = true;
}

bool DOSBOX_IsShutdownRequested()
{
	return is_shutdown_requested;
}

static void DOSBOX_UnlockSpeed(bool pressed)
{
	static bool autoadjust = false;

	if (pressed) {
		LOG_MSG("Fast Forward ON");
		ticks.locked = true;
		MIXER_EnableFastForwardMode();

		if (CPU_CycleAutoAdjust) {
			autoadjust = true;
			CPU_CycleAutoAdjust = false;
			CPU_CycleMax /= 3;
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

void DOSBOX_SetMachineTypeFromConfig(SectionProp* section)
{
	const auto arguments = &control->arguments;
	if (!arguments->machine.empty()) {
		//update value in config (else no matching against suggested values
		section->HandleInputline(std::string("machine=") +
		                         arguments->machine);
	}

	const auto machine_str = section->GetString("machine");

	svga_type = SvgaType::None;
	machine   = MachineType::Vga;

	int10.vesa_nolfb  = false;
	int10.vesa_oldvbe = false;

	if (machine_str == "cga") {
		machine = MachineType::CgaColor;
	} else if (machine_str == "cga_mono") {
		machine = MachineType::CgaMono;
	} else if (machine_str == "tandy") {
		machine = MachineType::Tandy;
	} else if (machine_str == "pcjr") {
		machine = MachineType::Pcjr;
	} else if (machine_str == "hercules") {
		machine = MachineType::Hercules;
	} else if (machine_str == "ega") {
		machine = MachineType::Ega;
	} else if (machine_str == "svga_s3") {
		svga_type = SvgaType::S3;
	} else if (machine_str == "vesa_nolfb") {
		svga_type = SvgaType::S3;
		int10.vesa_nolfb = true;
	} else if (machine_str == "vesa_oldvbe") {
		svga_type = SvgaType::S3;
		int10.vesa_oldvbe = true;
	} else if (machine_str == "svga_et3000") {
		svga_type = SvgaType::TsengEt3k;
	} else if (machine_str == "svga_et4000") {
		svga_type = SvgaType::TsengEt4k;
	} else if (machine_str == "svga_paradise") {
		svga_type = SvgaType::Paradise;
	} else {
		E_Exit("DOSBOX: Invalid machine type '%s'", machine_str.c_str());
	}

	// VGA-type machine needs an valid SVGA card and vice-versa
	assert((machine == MachineType::Vga && svga_type != SvgaType::None) ||
	       (machine != MachineType::Vga && svga_type == SvgaType::None));
}

static void DOSBOX_RealInit(Section* sec)
{
	SectionProp* section = static_cast<SectionProp*>(sec);

	// Initialize some dosbox internals
	ticks.remain = 0;
	ticks.last   = GetTicks();
	ticks.locked = false;

	DOSBOX_SetNormalLoop();

	MAPPER_AddHandler(DOSBOX_UnlockSpeed, SDL_SCANCODE_F12, MMOD2, "speedlock", "Speedlock");

	DOSBOX_SetMachineTypeFromConfig(section);

	// Set the user's prefered MCB fault handling strategy
	DOS_SetMcbFaultStrategy(section->GetString("mcb_fault_strategy").c_str());

	// Convert the users video memory in either MB or KB to bytes
	const std::string vmemsize_string = section->GetString("vmemsize");

	// If auto, then default to 0 and let the adapter's setup rountine set
	// the size
	auto vmemsize = vmemsize_string == "auto" ? 0 : std::stoi(vmemsize_string);

	// Scale up from MB-to-bytes or from KB-to-bytes
	vmemsize *= (vmemsize <= 8) ? 1024 * 1024 : 1024;
	vga.vmemsize = check_cast<uint32_t>(vmemsize);

	const std::string pref = section->GetString("vesa_modes");
	if (pref == "compatible") {
		int10.vesa_modes = VesaModes::Compatible;
	} else if (pref == "halfline") {
		int10.vesa_modes = VesaModes::Halfline;
	} else {
		int10.vesa_modes = VesaModes::All;
	}

	VGA_SetRefreshRateMode(section->GetString("dos_rate"));

	// Set the disk IO data rate
	const auto hdd_io_speed = section->GetString("hard_disk_speed");
	if (hdd_io_speed == "fast") {
		DOS_SetDiskSpeed(DiskSpeed::Fast, DiskType::HardDisk);
	} else if (hdd_io_speed == "medium") {
		DOS_SetDiskSpeed(DiskSpeed::Medium, DiskType::HardDisk);
	} else if (hdd_io_speed == "slow") {
		DOS_SetDiskSpeed(DiskSpeed::Slow, DiskType::HardDisk);
	} else {
		DOS_SetDiskSpeed(DiskSpeed::Maximum, DiskType::HardDisk);
	}

	// Set the floppy disk IO data rate
	const auto floppy_io_speed = section->GetString("floppy_disk_speed");
	if (floppy_io_speed == "fast") {
		DOS_SetDiskSpeed(DiskSpeed::Fast, DiskType::Floppy);
	} else if (floppy_io_speed == "medium") {
		DOS_SetDiskSpeed(DiskSpeed::Medium, DiskType::Floppy);
	} else if (floppy_io_speed == "slow") {
		DOS_SetDiskSpeed(DiskSpeed::Slow, DiskType::Floppy);
	} else {
		DOS_SetDiskSpeed(DiskSpeed::Maximum, DiskType::Floppy);
	}
}

static void dosbox_init(Section* sec)
{
	DOSBOX_RealInit(sec);
	MSG_LoadMessages();
}

static void notify_dosbox_setting_updated([[maybe_unused]] SectionProp* section,
                                          const std::string prop_name)
{
	if (prop_name == "language") {
		MSG_LoadMessages();

	} else if (prop_name == "dos_rate") {
		DOSBOX_RealInit(section);

	} else if (prop_name == "shell_config_shortcuts") {
		// No need to re-init anything; the setting is always queried when
		// executing a command.
	}
}

// Returns decimal seconds of elapsed uptime.
// The first call starts the uptime counter (and returns 0.0 seconds of uptime).
double DOSBOX_GetUptime()
{
	static auto start_ms = GetTicks();
	return GetTicksSince(start_ms) / MillisInSecond;
}

static void add_dosbox_config_section(const ConfigPtr& conf)
{
	assert(conf);

	using enum Property::Changeable::Value;

	auto section = conf->AddSection("dosbox", dosbox_init);

	auto pstring = section->AddString("language", Always, "auto");
	section->AddUpdateHandler(notify_dosbox_setting_updated);

	pstring->SetHelp(
	        "Select the DOS messages language:\n"
	        "  auto:     Detects the language from the host OS (default).\n"
	        "  <value>:  Loads a translation from the given file.\n"
	        "Notes:\n"
	        "  - The following language files are available:\n"
	        "    'br', 'de', 'en', 'es', 'fr', 'it', 'nl', 'pl' and 'ru'.\n"
	        "  - English is built-in, the rest is stored in the bundled\n"
	        "    'resources/translations' directory.");

	pstring = section->AddString("machine", OnlyAtStart, "svga_s3");
	pstring->SetValues({"hercules",
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
	pstring->SetHelp(
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

	pstring = section->AddPath("captures", Deprecated, "capture");
	pstring->SetHelp("Moved to [capture] section and renamed to 'capture_dir'.");

#if C_DEBUGGER
	LOG_StartUp();
#endif

	section->AddInitHandler(IO_Init);
	section->AddInitHandler(PAGING_Init);
	section->AddInitHandler(MEM_Init);

	auto pint = section->AddInt("memsize", OnlyAtStart, 16);
	pint->SetMinMax(MEM_GetMinMegabytes(), MEM_GetMaxMegabytes());
	pint->SetHelp(
	        "Amount of memory of the emulated machine has in MB (16 by default).\n"
	        "Best leave at the default setting to avoid problems with some games,\n"
	        "though a few games might require a higher value.\n"
	        "There is generally no speed advantage when raising this value.");

	pstring = section->AddString("mcb_fault_strategy", OnlyAtStart, "repair");
	pstring->SetHelp(
	        "How software-corrupted memory chain blocks should be handled:\n"
	        "  repair:  Repair (and report) faults using adjacent blocks (default).\n"
	        "  report:  Report faults but otherwise proceed as-is.\n"
	        "  allow:   Allow faults to go unreported (hardware behavior).\n"
	        "  deny:    Quit (and report) when faults are detected.");

	pstring->SetValues({"repair", "report", "allow", "deny"});

	static_assert(8192 * 1024 <= PciGfxLfbLimit - PciGfxLfbBase);
	pstring = section->AddString("vmemsize", OnlyAtStart, "auto");

	pstring->SetValues({"auto",
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
	pstring->SetHelp(
	        "Video memory in MB (1-8) or KB (256 to 8192). 'auto' uses the default for\n"
	        "the selected video adapter ('auto' by default). See the 'machine' setting for\n"
	        "the list of valid options and defaults per adapter.");

	pstring = section->AddString("vmem_delay", OnlyAtStart, "off");
	pstring->SetHelp(
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

	pstring = section->AddString("dos_rate", WhenIdle, "default");
	pstring->SetHelp(
	        "Override the emulated DOS video mode's refresh rate with a custom rate.\n"
	        "  default:  Don't override; use the emulated DOS video mode's refresh rate\n"
	        "            (default).\n"
	        "  host:     Override the refresh rate of all DOS video modes with the refresh\n"
	        "            rate of your monitor. This might allow you to play some 70 Hz VGA\n"
	        "            games with perfect vsync on a 60 Hz fixed refresh rate monitor (see\n"
	        "            'vsync' for further details).\n"
	        "  <value>:  Override the refresh rate of all DOS video modes with a fixed rate\n"
	        "            specified in Hz (valid range is from 24.000 to 1000.000). This is a\n"
	        "            niche option for a select few fast-paced mid to late 1990s 3D games\n"
	        "            for high refresh rate gaming.\n"
	        "\n"
	        "Note: Many games will misbehave when overriding the DOS video mode's refresh\n"
	        "      rate with non-standard values. This can manifest in glitchy video,\n"
	        "      sped-up or slowed-down audio, jerky mouse movement, mouse button presses\n"
	        "      not being registered, and even gameplay bugs. Overriding the DOS refresh\n"
	        "      rate is a hack that only works acceptably with a small subset of all DOS\n"
	        "      games (typically mid to late 1990s games).");

	pstring = section->AddString("vesa_modes", OnlyAtStart, "compatible");
	pstring->SetValues({"compatible", "all", "halfline"});
	pstring->SetHelp(
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

	auto pbool = section->AddBool("vga_8dot_font", OnlyAtStart, false);
	pbool->SetHelp("Use 8-pixel-wide fonts on VGA adapters ('off' by default).");

	pbool = section->AddBool("vga_render_per_scanline", OnlyAtStart, true);
	pbool->SetHelp(
	        "Emulate accurate per-scanline VGA rendering ('on' by default).\n"
	        "Currently, you need to disable this for a few games, otherwise they will crash\n"
	        "at startup (e.g., Deus, Ishar 3, Robinson's Requiem, Time Warriors).");

	pbool = section->AddBool("speed_mods", OnlyAtStart, true);
	pbool->SetHelp(
	        "Permit changes known to improve performance ('on' by default).\n"
	        "Currently, no games are known to be negatively affected by this.\n"
	        "Please file a bug with the project if you find a game that fails\n"
	        "when this is enabled so we will list them here.");

	section->AddInitHandler(CALLBACK_Init);
	section->AddInitHandler(PIC_Init);
	section->AddInitHandler(PROGRAMS_Init);
	section->AddInitHandler(TIMER_Init);
	section->AddInitHandler(CMOS_Init);

	pstring = section->AddString("autoexec_section", OnlyAtStart, "join");
	pstring->SetValues({"join", "overwrite"});
	pstring->SetHelp(
	        "How autoexec sections are handled from multiple config files:\n"
	        "  join:       Combine them into one big section (legacy behavior; default).\n"
	        "  overwrite:  Use the last one encountered, like other config settings.");

	pbool = section->AddBool("automount", OnlyAtStart, true);
	pbool->SetHelp(
	        "Mount 'drives/[c]' directories as drives on startup, where [c] is a lower-case\n"
	        "drive letter from 'a' to 'y' ('on' by default). The 'drives' folder can be\n"
	        "provided relative to the current directory or via built-in resources.\n"
	        "Mount settings can be optionally provided using a [c].conf file along-side\n"
	        "the drive's directory, with content as follows:\n"
	        "  [drive]\n"
	        "  type     = dir, overlay, floppy, or cdrom\n"
	        "  label    = custom_label\n"
	        "  path     = path-specification (e.g., path = %%path%%;c:\\tools)\n"
	        "  override_drive = mount the directory to this drive instead (default empty)\n"
	        "  verbose  = on or off\n"
	        "  readonly = on or off");

	pstring = section->AddString("startup_verbosity", OnlyAtStart, "auto");
	pstring->SetValues({"auto", "high", "low", "quiet"});
	pstring->SetHelp(
	        "Controls verbosity prior to displaying the program ('auto' by default):\n"
	        "  Verbosity   | Welcome | Early stdout\n"
	        "  high        |   yes   |    yes\n"
	        "  low         |   no    |    yes\n"
	        "  quiet       |   no    |    no\n"
	        "  auto        | 'low' if exec or dir is passed, otherwise 'high'");

	pbool = section->AddBool("allow_write_protected_files", OnlyAtStart, true);
	pbool->SetHelp(
	        "Many games open all their files with writable permissions; even files that they\n"
	        "never modify. This setting lets you write-protect those files while still\n"
	        "allowing the game to read them ('on' by default). A second use-case: if you're\n"
	        "using a copy-on-write or network-based filesystem, this setting avoids\n"
	        "triggering write operations for these write-protected files.");

	pbool = section->AddBool("shell_config_shortcuts", WhenIdle, true);
	pbool->SetHelp(
	        "Allow shortcuts for simpler configuration management ('on' by default).\n"
	        "E.g., instead of 'config -set sbtype sb16', it is enough to execute\n"
	        "'sbtype sb16', and instead of 'config -get sbtype', you can just execute\n"
	        "the 'sbtype' command.");

	pstring = section->AddString("hard_disk_speed", OnlyAtStart, "maximum");
	pstring->SetValues({"maximum", "fast", "medium", "slow"});
	pstring->SetHelp(
	        "Set the emulated hard disk speed ('maximum' by default).\n"
	        "  maximum:  As fast as possible, no slowdown (default)\n"
	        "  fast:     Typical mid-1990s hard disk speed (~15 MB/s)\n"
	        "  medium:   Typical early 1990s hard disk speed (~2.5 MB/s)\n"
	        "  slow:     Typical 1980s hard disk speed (~600 kB/s)");

	pstring = section->AddString("floppy_disk_speed", OnlyAtStart, "maximum");
	pstring->SetValues({"maximum", "fast", "medium", "slow"});
	pstring->SetHelp(
	        "Set the emulated floppy disk speed ('maximum' by default).\n"
	        "  maximum:  As fast as possible, no slowdown (default)\n"
	        "  fast:     Extra-high density (ED) floppy speed (~120 kB/s)\n"
	        "  medium:   High density (HD) floppy speed (~60 kB/s)\n"
	        "  slow:     Double density (DD) floppy speed (~30 kB/s)");
}

void DOSBOX_InitAllModuleConfigsAndMessages()
{
	// The [sdl] section gets initialised first in `sdlmain.cpp`, then
	// this init method gets called.

	add_dosbox_config_section(control);

	RENDER_AddConfigSection(control);

	VGA_AddCompositeSettings(*control);

	CPU_AddConfigSection(control);

	VOODOO_AddConfigSection(control);

	CAPTURE_AddConfigSection(control);

	MOUSE_AddConfigSection(control);

	MIXER_AddConfigSection(control);

	FSYNTH_AddConfigSection(control);

#if C_MT32EMU
	MT32_AddConfigSection(control);
#endif

	SOUNDCANVAS_AddConfigSection(control);

	// The MIDI section must be added *after* the FluidSynth, MT-32 and
	// SoundCanvas MIDI device sections. If the MIDI section is intialised
	// before these, these devices would be double-initialised if selected
	// at startup time (e.g., by having `mididevice = mt32` in the config).
	MIDI_AddConfigSection(control);

#if C_DEBUGGER
	control->AddSection("debug", DEBUG_Init);
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

	DISKNOISE_AddConfigSection(control);

	// Configure PS/1, Tandy/PCjr , LPT DAC, and PC speaker audio emulation
	SPEAKER_AddConfigSection(control);

	REELMAGIC_AddConfigSection(control);

	JOYSTICK_AddConfigSection(control);

	SERIAL_AddConfigSection(control);

	DOS_AddConfigSection(control);

#if C_IPX
	IPX_AddConfigSection(control);
#endif

	ETHERNET_AddConfigSection(control);

	control->AddAutoexecSection(AUTOEXEC_Init);

	MSG_Add("AUTOEXEC_CONFIGFILE_HELP",
	        "Each line in this section is executed at startup as a DOS command.\n"
	        "Important: The [autoexec] section must be the last section in the config!");

	MSG_Add("CONFIGFILE_INTRO",
	        "# This is the configuration file for " DOSBOX_NAME
	        " (%s).\n"
	        "# Lines starting with a '#' character are comments.\n");

	// Needs to be initialised early before the config settings get applied
	PROGRAMS_AddMessages();

	// Initialize the uptime counter when launching the first shell. This
	// ensures that slow-performing configurable tasks (like loading MIDI
	// SF2 files) have already been performed and won't affect this time.
	DOSBOX_GetUptime();

	control->SetStartUp(&SHELL_Init);
}
