// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/programs.h"

#include "shell/autoexec.h"
#include "program/program_attrib.h"
#include "program/program_autotype.h"
#include "program/program_boot.h"
#include "program/program_choice.h"
#include "program/program_clip.h"
#include "program/program_help.h"
#include "program/program_imgmount.h"
#include "program/program_intro.h"
#include "program/program_keyb.h"
#include "program/program_loadfix.h"
#include "program/program_loadrom.h"
#include "program/program_ls.h"
#include "program/program_mem.h"
#include "program/program_mixer.h"
#include "program/program_mode.h"
#include "program/program_more.h"
#include "program/program_mount.h"
#include "program/program_mouse.h"
#include "program/program_mousectl.h"
#include "program/program_move.h"
#include "program/program_placeholder.h"
#include "program/program_rescan.h"
#include "program/program_serial.h"
#include "program/program_setver.h"
#include "program/program_subst.h"
#include "program/program_tree.h"

#if C_DEBUGGER
#include "program/program_biostest.h"
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
#if C_DEBUGGER
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
	PROGRAMS_MakeFile("MOUSE.COM", ProgramCreate<MOUSE>);
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
