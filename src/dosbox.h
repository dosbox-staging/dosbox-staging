// SPDX-FileCopyrightText:  2019-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DOSBOX_H
#define DOSBOX_DOSBOX_H

#include "dosbox_config.h"
#include "misc/compiler.h"
#include "misc/messages.h"
#include "misc/types.h"

#include <memory>

// Project name, lower-case and without spaces
#define DOSBOX_PROJECT_NAME "dosbox-staging"

// Name of the emulator
#define DOSBOX_NAME "DOSBox Staging"

// Development team name
#define DOSBOX_TEAM "The " DOSBOX_NAME " Team"

// Copyright string
#define DOSBOX_COPYRIGHT "(C) " DOSBOX_TEAM

// Address to report bugs
#define DOSBOX_BUGS_TO "https://github.com/dosbox-staging/dosbox-staging/issues"

// Address of the translation manual
#define DOSBOX_MANUAL_TRANSLATION "https://github.com/dosbox-staging/dosbox-staging/blob/main/docs/TRANSLATING.md"

// Fully qualified application ID for the emulator; see
// https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names
// for more details
#define DOSBOX_APP_ID "org.dosbox_staging.dosbox_staging"


int sdl_main(int argc, char *argv[]);

// The shutdown_requested bool is a conditional break in the parse-loop and
// machine-loop. Set it to true to gracefully quit in expected circumstances.
extern bool shutdown_requested;

// The E_Exit function throws an exception to quit. Call it in unexpected
// circumstances.
[[noreturn]] void E_Exit(const char *message, ...)
        GCC_ATTRIBUTE(__format__(__printf__, 1, 2));

class Section;
class SectionProp;

typedef Bitu (LoopHandler)(void);

const char* DOSBOX_GetVersion() noexcept;
const char* DOSBOX_GetDetailedVersion() noexcept;

double DOSBOX_GetUptime();

void DOSBOX_RunMachine();
void DOSBOX_SetLoop(LoopHandler * handler);
void DOSBOX_SetNormalLoop();

void DOSBOX_InitAllModuleConfigsAndMessages(void);

void DOSBOX_SetMachineTypeFromConfig(SectionProp* section);

int64_t DOSBOX_GetTicksDone();
void DOSBOX_SetTicksDone(const int64_t ticks_done);
void DOSBOX_SetTicksScheduled(const int64_t ticks_scheduled);

enum class MachineType {
	// Value not set yet
	None,

	// PC XT with Hercules
	Hercules,

	// PC XT with CGA and monochrome monitor
	CgaMono,
	// PC XT with CGA and color monitor
	CgaColor,
	// IBM PCjr
	Pcjr,
	// Tandy 1000
	Tandy,

	// PC AT with EGA
	Ega,
	// PC AT with VGA or SVGA
	Vga
};

enum class SvgaType {
	// Non-SVGA card
	None,
	// S3 Graphics series (emulated card is mostly Trio64 compatible)
	S3,
	// Tseng Labs ET3000
	TsengEt3k,
	// Tseng Labs ET4000
	TsengEt4k,
	// Paradise Systems PVGA1A
	Paradise
}; 

extern MachineType machine;
extern SvgaType    svga_type;

inline bool is_machine_svga() {
	return svga_type != SvgaType::None;
}

inline bool is_machine_vga_or_better() {
	return machine == MachineType::Vga;
}

inline bool is_machine_ega() {
	return machine == MachineType::Ega;	
}

inline bool is_machine_ega_or_better() {
	return is_machine_ega() || is_machine_vga_or_better();
}

inline bool is_machine_tandy() {
	return machine == MachineType::Tandy;
}

inline bool is_machine_pcjr() {
	return machine == MachineType::Pcjr;
}

inline bool is_machine_pcjr_or_tandy() {
	return is_machine_pcjr() || is_machine_tandy();
}

inline bool is_machine_cga_mono() {
	return machine == MachineType::CgaMono;
}

inline bool is_machine_cga_color() {
	return machine == MachineType::CgaColor;
}

inline bool is_machine_cga()
{
	return is_machine_cga_mono() || is_machine_cga_color();
}

inline bool is_machine_cga_or_better() {
	return is_machine_cga() ||
               is_machine_pcjr_or_tandy() ||
               is_machine_ega_or_better();
}

inline bool is_machine_hercules() {
	return machine == MachineType::Hercules;
}

#ifndef DOSBOX_LOGGING_H
#include "misc/logging.h"
#endif // the logging system.

constexpr auto DefaultMt32RomsDir   = "mt32-roms";
constexpr auto DefaultSoundfontsDir = "soundfonts";
constexpr auto GlShadersDir         = "glshaders";
constexpr auto DiskNoiseDir         = "disknoises";
constexpr auto PluginsDir           = "plugins";

constexpr auto MicrosInMillisecond = 1000;
constexpr auto BytesPerKilobyte    = 1024;

enum class DiskSpeed { Maximum, Fast, Medium, Slow };

#endif /* DOSBOX_DOSBOX_H */
