/*
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

#ifndef DOSBOX_DOSBOX_H
#define DOSBOX_DOSBOX_H

#include "config.h"
#include "compiler.h"
#include "types.h"

#include <memory>

// Project name, lower-case and without spaces
#define DOSBOX_PROJECT_NAME "dosbox-staging"

// Name of the emulator
#define DOSBOX_NAME "DOSBox Staging"

// Development team name
#define DOSBOX_TEAM "The " DOSBOX_NAME " Team"

// Copyright string
#define DOSBOX_COPYRIGHT "(C) " DOSBOX_TEAM


int sdl_main(int argc, char *argv[]);

// The shutdown_requested bool is a conditional break in the parse-loop and
// machine-loop. Set it to true to gracefully quit in expected circumstances.
extern bool shutdown_requested;

// The E_Exit function throws an exception to quit. Call it in unexpected
// circumstances.
[[noreturn]] void E_Exit(const char *message, ...)
        GCC_ATTRIBUTE(__format__(__printf__, 1, 2));

void MSG_Add(const char*,const char*); // add messages (in UTF-8) to the language file
const char* MSG_Get(const char*); // get messages (adapted to current code page)
                                  // from the language file
const char* MSG_GetRaw(const char*); // get messages (in UTF-8, without ANSI
                                     // preprocessing) from the language file
bool MSG_Exists(const char*);

class Section;
class Section_prop;

typedef Bitu (LoopHandler)(void);

const char* DOSBOX_GetVersion() noexcept;
const char* DOSBOX_GetDetailedVersion() noexcept;

double DOSBOX_GetUptime();

void DOSBOX_RunMachine();
void DOSBOX_SetLoop(LoopHandler * handler);
void DOSBOX_SetNormalLoop();

void DOSBOX_Init(void);

void DOSBOX_SetMachineTypeFromConfig(Section_prop* section);

class Config;
using config_ptr_t = std::unique_ptr<Config>;
extern config_ptr_t control;

enum SVGACards {
	SVGA_None,
	SVGA_S3Trio,
	SVGA_TsengET4K,
	SVGA_TsengET3K,
	SVGA_ParadisePVGA1A
}; 

extern SVGACards svgaCard;
extern bool mono_cga;

enum MachineType {
	// In rough age-order: Hercules is the oldest and VGA is the newest
	// (Tandy started out as a clone of the PCjr, so PCjr came first)
	MCH_INVALID = 0,
	MCH_HERC    = 1 << 0,
	MCH_CGA     = 1 << 1,
	MCH_TANDY   = 1 << 2,
	MCH_PCJR    = 1 << 3,
	MCH_EGA     = 1 << 4,
	MCH_VGA     = 1 << 5,
};

extern MachineType machine;

inline bool is_machine(const int type) {
	return machine & type;
}
#define IS_TANDY_ARCH ((machine==MCH_TANDY) || (machine==MCH_PCJR))
#define IS_EGAVGA_ARCH ((machine==MCH_EGA) || (machine==MCH_VGA))
#define IS_VGA_ARCH (machine==MCH_VGA)

#ifndef DOSBOX_LOGGING_H
#include "logging.h"
#endif // the logging system.

constexpr auto DefaultMt32RomsDir   = "mt32-roms";
constexpr auto DefaultSoundfontsDir = "soundfonts";
constexpr auto GlShadersDir         = "glshaders";

#endif /* DOSBOX_DOSBOX_H */
