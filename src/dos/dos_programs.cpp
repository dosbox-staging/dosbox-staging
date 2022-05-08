/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
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

#include "program_attrib.h"
#include "program_autotype.h"
#include "program_boot.h"
#include "program_choice.h"
#include "program_help.h"
#include "program_imgmount.h"
#include "program_intro.h"
#include "program_keyb.h"
#include "program_loadfix.h"
#include "program_loadrom.h"
#include "program_ls.h"
#include "program_mem.h"
#include "program_mount.h"
#include "program_placeholder.h"
#include "program_rescan.h"
#include "program_serial.h"

#if C_DEBUG
#include "program_biostest.h"
#endif

#include "dos_resources.h"

extern uint32_t floppytype;

extern char autoexec_data[autoexec_maxsize];
std::unique_ptr<Program> CONFIG_ProgramStart();
std::unique_ptr<Program> MIXER_ProgramStart();
std::unique_ptr<Program> SHELL_ProgramStart();
void z_drive_getpath(std::string &path, const std::string &dirname);
void z_drive_register(const std::string &path, const std::string &dir);

void Add_VFiles(const bool add_autoexec)
{
	const std::string dirname = "drivez";
	std::string path = ".";
	path += CROSS_FILESPLIT;
	path += dirname;
	z_drive_getpath(path, dirname);
	z_drive_register(path, "/");

	PROGRAMS_MakeFile("ATTRIB.COM", ProgramStart<ATTRIB>);
	PROGRAMS_MakeFile("AUTOTYPE.COM", ProgramStart<AUTOTYPE>);
#if C_DEBUG
	PROGRAMS_MakeFile("BIOSTEST.COM", ProgramStart<AUTOTYPE>);
#endif
	PROGRAMS_MakeFile("BOOT.COM", ProgramStart<AUTOTYPE>);
	PROGRAMS_MakeFile("CHOICE.COM", ProgramStart<CHOICE>);
	PROGRAMS_MakeFile("HELP.COM", ProgramStart<HELP>);
	PROGRAMS_MakeFile("IMGMOUNT.COM", ProgramStart<IMGMOUNT>);
	PROGRAMS_MakeFile("INTRO.COM", ProgramStart<INTRO>);
	PROGRAMS_MakeFile("KEYB.COM", ProgramStart<KEYB>);
	PROGRAMS_MakeFile("LOADFIX.COM", ProgramStart<LOADFIX>);
	PROGRAMS_MakeFile("LOADROM.COM", ProgramStart<LOADROM>);
	PROGRAMS_MakeFile("LS.COM", ProgramStart<LS>);
	PROGRAMS_MakeFile("MEM.COM", ProgramStart<MEM>);
	PROGRAMS_MakeFile("MOUNT.COM", ProgramStart<MOUNT>);
	PROGRAMS_MakeFile("RESCAN.COM", ProgramStart<RESCAN>);
	PROGRAMS_MakeFile("MIXER.COM", MIXER_ProgramStart);
	PROGRAMS_MakeFile("CONFIG.COM", CONFIG_ProgramStart);
	PROGRAMS_MakeFile("SERIAL.COM", ProgramStart<SERIAL>);
	PROGRAMS_MakeFile("COMMAND.COM", SHELL_ProgramStart);
	if (add_autoexec)
		VFILE_Register("AUTOEXEC.BAT", (uint8_t *)autoexec_data,
		               (uint32_t)strlen(autoexec_data));
	VFILE_Register("CPI", 0, 0, "/");

	for (size_t i = 0; i < FILE_EGA_CPX.size(); i++)
		VFILE_Register(FILE_EGA_CPX[i], BLOB_EGA_CPX[i], "/CPI/");
	for (size_t i = 0; i < FILE_KEYBOARD_SYS.size(); i++)
		VFILE_Register(FILE_KEYBOARD_SYS[i], BLOB_KEYBOARD_SYS[i], "");
}

void DOS_SetupPrograms(void)
{
	/*Add misc messages */
	MSG_Add("WIKI_ADD_UTILITIES_ARTICLE", WIKI_ADD_UTILITIES_ARTICLE);
	MSG_Add("WIKI_URL", WIKI_URL);

	const auto add_autoexec = false;
	Add_VFiles(add_autoexec);
}
