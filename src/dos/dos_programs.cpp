/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2025  The DOSBox Staging Team
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

#include "programs.h"

#include "autoexec.h"
#include "program_attrib.h"
#include "program_autotype.h"
#include "program_boot.h"
#include "program_choice.h"
#include "program_clip.h"
#include "program_help.h"
#include "program_imgmount.h"
#include "program_intro.h"
#include "program_keyb.h"
#include "program_loadfix.h"
#include "program_loadrom.h"
#include "program_ls.h"
#include "program_mem.h"
#include "program_mixer.h"
#include "program_mode.h"
#include "program_more.h"
#include "program_mount.h"
#include "program_mousectl.h"
#include "program_move.h"
#include "program_placeholder.h"
#include "program_rescan.h"
#include "program_serial.h"
#include "program_setver.h"
#include "program_subst.h"
#include "program_tree.h"

#if C_DEBUG
#include "program_biostest.h"
#endif

extern uint32_t floppytype;

std::unique_ptr<Program> CONFIG_ProgramCreate();
std::unique_ptr<Program> MIXER_ProgramCreate();
std::unique_ptr<Program> SHELL_ProgramCreate();

void REELMAGIC_MaybeCreateFmpdrvExecutable();

void VFILE_GetPathZDrive(std::string& path, const std::string& dirname);
void VFILE_RegisterZDrive(const std_fs::path& z_drive_path);

void Add_VFiles()
{
	const std::string dirname = "drivez";

	std::string path = ".";

	path += CROSS_FILESPLIT;
	path += dirname;
	VFILE_GetPathZDrive(path, dirname);
	VFILE_RegisterZDrive(path);

	PROGRAMS_MakeFile("ATTRIB.COM", ProgramCreate<ATTRIB>);
	PROGRAMS_MakeFile("AUTOTYPE.COM", ProgramCreate<AUTOTYPE>);
#if C_DEBUG
	PROGRAMS_MakeFile("BIOSTEST.COM", ProgramCreate<BIOSTEST>);
#endif
	PROGRAMS_MakeFile("BOOT.COM", ProgramCreate<BOOT>);
	PROGRAMS_MakeFile("CHOICE.COM", ProgramCreate<CHOICE>);
	PROGRAMS_MakeFile("CLIP.EXE", ProgramCreate<CLIP>);
	PROGRAMS_MakeFile("COMMAND.COM", SHELL_ProgramCreate);
	PROGRAMS_MakeFile("CONFIG.COM", CONFIG_ProgramCreate);
	PROGRAMS_MakeFile("HELP.COM", ProgramCreate<HELP>);
	PROGRAMS_MakeFile("IMGMOUNT.COM", ProgramCreate<IMGMOUNT>);
	PROGRAMS_MakeFile("INTRO.COM", ProgramCreate<INTRO>);
	PROGRAMS_MakeFile("KEYB.COM", ProgramCreate<KEYB>);
	PROGRAMS_MakeFile("LOADFIX.COM", ProgramCreate<LOADFIX>);
	PROGRAMS_MakeFile("LOADROM.COM", ProgramCreate<LOADROM>);
	PROGRAMS_MakeFile("LS.COM", ProgramCreate<LS>);
	PROGRAMS_MakeFile("MEM.COM", ProgramCreate<MEM>);
	PROGRAMS_MakeFile("MIXER.COM", ProgramCreate<MIXER>);
	PROGRAMS_MakeFile("MODE.COM", ProgramCreate<MODE>);
	PROGRAMS_MakeFile("MORE.COM", ProgramCreate<MORE>);
	PROGRAMS_MakeFile("MOUNT.COM", ProgramCreate<MOUNT>);
	PROGRAMS_MakeFile("MOUSECTL.COM", ProgramCreate<MOUSECTL>);
	PROGRAMS_MakeFile("MOVE.EXE", ProgramCreate<MOVE>);
	PROGRAMS_MakeFile("RESCAN.COM", ProgramCreate<RESCAN>);
	PROGRAMS_MakeFile("SERIAL.COM", ProgramCreate<SERIAL>);
	PROGRAMS_MakeFile("SETVER.EXE", ProgramCreate<SETVER>);
	PROGRAMS_MakeFile("SUBST.EXE", ProgramCreate<SUBST>);
	PROGRAMS_MakeFile("TREE.COM", ProgramCreate<TREE>);

	REELMAGIC_MaybeCreateFmpdrvExecutable();

	AUTOEXEC_RefreshFile();
}

void DOS_SetupPrograms(void)
{
	/*Add misc messages */
	MSG_Add("WIKI_ADD_UTILITIES_ARTICLE", WIKI_ADD_UTILITIES_ARTICLE);
	MSG_Add("WIKI_URL", WIKI_URL);

	Add_VFiles();
}
