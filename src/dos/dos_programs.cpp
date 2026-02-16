// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/programs.h"

#include "programs/attrib.h"
#include "programs/autotype.h"
#include "programs/boot.h"
#include "programs/choice.h"
#include "programs/clip.h"
#include "programs/help.h"
#include "programs/makeimg.h"
#include "programs/imgmount.h"
#include "programs/intro.h"
#include "programs/keyb.h"
#include "programs/loadfix.h"
#include "programs/loadrom.h"
#include "programs/ls.h"
#include "programs/mem.h"
#include "programs/mixer.h"
#include "programs/mode.h"
#include "programs/more.h"
#include "programs/mount.h"
#include "programs/mouse.h"
#include "programs/mousectl.h"
#include "programs/move.h"
#include "programs/rescan.h"
#include "programs/serial.h"
#include "programs/setver.h"
#include "programs/showpic.h"
#include "programs/subst.h"
#include "programs/tree.h"
#include "shell/autoexec.h"

#if C_DEBUGGER
#include "programs/biostest.h"
#endif

extern uint32_t floppytype;

std::unique_ptr<Program> CONFIG_ProgramCreate();
std::unique_ptr<Program> MIXER_ProgramCreate();
std::unique_ptr<Program> SHELL_ProgramCreate();

void REELMAGIC_MaybeCreateFmpdrvExecutable();

void VFILE_GetPathZDrive(std::string& path, const std::string& dirname);
void VFILE_RegisterZDrive(const std_fs::path& z_drive_path);

void DOS_SetupPrograms()
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
	PROGRAMS_MakeFile("MAKEIMG.COM", ProgramCreate<MAKEIMG>);
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
	PROGRAMS_MakeFile("SHOWPIC.EXE", ProgramCreate<SHOWPIC>);
	PROGRAMS_MakeFile("SUBST.EXE", ProgramCreate<SUBST>);
	PROGRAMS_MakeFile("TREE.COM", ProgramCreate<TREE>);

	REELMAGIC_MaybeCreateFmpdrvExecutable();

	AUTOEXEC_RefreshFile();
}
