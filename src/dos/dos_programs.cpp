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

extern Bit32u floppytype;

#define WIKI_URL                   "https://github.com/dosbox-staging/dosbox-staging/wiki"
#define WIKI_ADD_UTILITIES_ARTICLE WIKI_URL "/Add-Utilities"

extern char autoexec_data[autoexec_maxsize];
void CONFIG_ProgramStart(Program **make);
void MIXER_ProgramStart(Program **make);
void SHELL_ProgramStart(Program **make);
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

	PROGRAMS_MakeFile("ATTRIB.COM", ATTRIB_ProgramStart);
	PROGRAMS_MakeFile("AUTOTYPE.COM", AUTOTYPE_ProgramStart);
#if C_DEBUG
	PROGRAMS_MakeFile("BIOSTEST.COM", BIOSTEST_ProgramStart);
#endif
	PROGRAMS_MakeFile("BOOT.COM", BOOT_ProgramStart);
	PROGRAMS_MakeFile("CHOICE.COM", CHOICE_ProgramStart);
	PROGRAMS_MakeFile("HELP.COM", HELP_ProgramStart);
	PROGRAMS_MakeFile("IMGMOUNT.COM", IMGMOUNT_ProgramStart);
	PROGRAMS_MakeFile("INTRO.COM", INTRO_ProgramStart);
	PROGRAMS_MakeFile("KEYB.COM", KEYB_ProgramStart);
	PROGRAMS_MakeFile("LOADFIX.COM", LOADFIX_ProgramStart);
	PROGRAMS_MakeFile("LOADROM.COM", LOADROM_ProgramStart);
	PROGRAMS_MakeFile("LS.COM", LS_ProgramStart);
	PROGRAMS_MakeFile("MEM.COM", MEM_ProgramStart);
	PROGRAMS_MakeFile("MOUNT.COM", MOUNT_ProgramStart);
	PROGRAMS_MakeFile("RESCAN.COM", RESCAN_ProgramStart);
	PROGRAMS_MakeFile("MIXER.COM", MIXER_ProgramStart);
	PROGRAMS_MakeFile("CONFIG.COM", CONFIG_ProgramStart);
	PROGRAMS_MakeFile("SERIAL.COM", SERIAL_ProgramStart);
	PROGRAMS_MakeFile("COMMAND.COM", SHELL_ProgramStart);
	if (add_autoexec)
		VFILE_Register("AUTOEXEC.BAT", (uint8_t *)autoexec_data,
		               (uint32_t)strlen(autoexec_data));
}

void DOS_SetupPrograms(void)
{
	/*Add Messages */
	MSG_Add("PROGRAM_MOUNT_CDROMS_FOUND","CDROMs found: %d\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_DRIVE", "Drive");
	MSG_Add("PROGRAM_MOUNT_STATUS_TYPE", "Type");
	MSG_Add("PROGRAM_MOUNT_STATUS_LABEL", "Label");
	MSG_Add("PROGRAM_MOUNT_STATUS_NAME", "Image name");
	MSG_Add("PROGRAM_MOUNT_STATUS_SLOT", "Swap slot");
	MSG_Add("PROGRAM_MOUNT_STATUS_2","Drive %c is mounted as %s\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_1", "The currently mounted drives are:\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_1","Directory %s doesn't exist.\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_2","%s isn't a directory\n");
	MSG_Add("PROGRAM_MOUNT_ILL_TYPE","Illegal type %s\n");
	MSG_Add("PROGRAM_MOUNT_ALREADY_MOUNTED","Drive %c already mounted with %s\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED","Drive %c isn't mounted.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_SUCCESS","Drive %c has successfully been removed.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL","Virtual Drives can not be unMOUNTed.\n");
	MSG_Add("PROGRAM_MOUNT_DRIVEID_ERROR", "'%c' is not a valid drive identifier.\n");
	MSG_Add("PROGRAM_MOUNT_WARNING_WIN","\033[31;1mMounting c:\\ is NOT recommended. Please mount a (sub)directory next time.\033[0m\n");
	MSG_Add("PROGRAM_MOUNT_WARNING_OTHER","\033[31;1mMounting / is NOT recommended. Please mount a (sub)directory next time.\033[0m\n");
	MSG_Add("PROGRAM_MOUNT_NO_OPTION", "Warning: Ignoring unsupported option '%s'.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_NO_BASE","A normal directory needs to be MOUNTed first before an overlay can be added on top.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_INCOMPAT_BASE","The overlay is NOT compatible with the drive that is specified.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_MIXED_BASE","The overlay needs to be specified using the same addressing as the underlying drive. No mixing of relative and absolute paths.");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_SAME_AS_BASE","The overlay directory can not be the same as underlying drive.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_GENERIC_ERROR","Something went wrong.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_STATUS","Overlay %s on drive %c mounted.\n");
	MSG_Add("PROGRAM_MOUNT_MOVE_Z_ERROR_1", "Can't move drive Z. Drive %c is mounted already.\n");

	MSG_Add("SHELL_CMD_MEM_HELP_LONG",
	        "Displays the DOS memory information.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]mem[reset]\n"
	        "\n"
	        "Where:\n"
	        "  This command has no parameters.\n"
	        "\n"
	        "Notes:\n"
	        "  This command shows the DOS memory status, including the free conventional\n"
	        "  memory, UMB (upper) memory, XMS (extended) memory, and EMS (expanded) memory.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]mem[reset]\n");
	MSG_Add("PROGRAM_MEM_CONVEN", "%10d kB free conventional memory\n");
	MSG_Add("PROGRAM_MEM_EXTEND", "%10d kB free extended memory\n");
	MSG_Add("PROGRAM_MEM_EXPAND", "%10d kB free expanded memory\n");
	MSG_Add("PROGRAM_MEM_UPPER",
	        "%10d kB free upper memory in %d blocks (largest UMB %d kB)\n");

	MSG_Add("SHELL_CMD_LOADFIX_HELP_LONG",
	        "Loads a program in the specific memory region and then runs it.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]loadfix[reset] [color=cyan]GAME[reset] [color=white][PARAMETERS][reset]\n"
	        "  [color=green]loadfix[reset] [/d] (or [/f])[reset]\n"
	        "\n"
	        "Where:\n"
	        "  [color=cyan]GAME[reset] is a game or program to be loaded, optionally with parameters.\n"
	        "\n"
	        "Notes:\n"
	        "  The most common use cases of this command are to fix DOS games or programs\n"
	        "  which show either the \"[color=white]Packed File Corrupt[reset]\" or \"[color=white]Not enough memory\"[reset] (e.g.,\n"
	        "  from some 1980's games such as California Games II) error message when run.\n"
	        "  Running [color=green]loadfix[reset] without an argument simply allocates memory for your game\n"
	        "  to run; you can free the memory with either /d or /f option when it finishes.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]loadfix[reset] [color=cyan]mygame[reset] [color=white]args[reset]\n"
	        "  [color=green]loadfix[reset] /d\n");
	MSG_Add("PROGRAM_LOADFIX_ALLOC", "%d kB allocated.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOC", "%d kB freed.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOCALL","Used memory freed.\n");
	MSG_Add("PROGRAM_LOADFIX_ERROR","Memory allocation error.\n");

	MSG_Add("SHELL_CMD_AUTOTYPE_HELP_LONG",
	        "Performs scripted keyboard entry into a running DOS game.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]autotype[reset] -list\n"
	        "  [color=green]autotype[reset] [-w [color=white]WAIT[reset]] [-p [color=white]PACE[reset]] [color=cyan]BUTTONS[reset]\n"
	        "\n"
	        "Where:\n"
	        "  [color=white]WAIT[reset]    is the number of seconds to wait before typing begins (max of 30).\n"
	        "  [color=white]PACE[reset]    is the number of seconds before each keystroke (max of 10).\n"
	        "  [color=cyan]BUTTONS[reset] is one or more space-separated buttons.\n"
	        "\n"
	        "Notes:\n"
	        "  The [color=cyan]BUTTONS[reset] supplied in the command will be autotyped into running DOS games\n"
	        "  after they start. Autotyping begins after [color=cyan]WAIT[reset] seconds, and each button is\n"
	        "  entered every [color=white]PACE[reset] seconds. The [color=cyan],[reset] character inserts an extra [color=white]PACE[reset] delay.\n"
	        "  [color=white]WAIT[reset] and [color=white]PACE[reset] default to 2 and 0.5 seconds respectively if not specified.\n"
	        "  A list of all available button names can be obtained using the -list option.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]autotype[reset] -list\n"
	        "  [color=green]autotype[reset] -w [color=white]1[reset] -p [color=white]0.3[reset] [color=cyan]up enter , right enter[reset]\n"
	        "  [color=green]autotype[reset] -p [color=white]0.2[reset] [color=cyan]f1 kp_8 , , enter[reset]\n"
	        "  [color=green]autotype[reset] -w [color=white]1.3[reset] [color=cyan]esc enter , p l a y e r enter\n[reset]");

	MSG_Add("SHELL_CMD_MIXER_HELP_LONG",
	        "Displays or changes the current sound mixer volumes.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]mixer[reset] [color=cyan]CHANNEL[reset] [color=white]VOLUME[reset] [/noshow]\n"
	        "  [color=green]mixer[reset] [/listmidi][reset]\n"
	        "\n"
	        "Where:\n"
	        "  [color=cyan]CHANNEL[reset] is the sound channel you want to change the volume.\n"
	        "  [color=white]VOLUME[reset]  is an integer between 0 and 100 representing the volume.\n"
	        "\n"
	        "Notes:\n"
	        "  Running [color=green]mixer[reset] without an argument shows the volumes of all sound channels.\n"
	        "  You can view available MIDI devices and user options with /listmidi option.\n"
	        "  You may change the volumes of more than one sound channels in one command.\n"
	        "  The /noshow option causes mixer not to show the volumes when making a change.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]mixer[reset]\n"
	        "  [color=green]mixer[reset] [color=cyan]master[reset] [color=white]50[reset] [color=cyan]record[reset] [color=white]60[reset] /noshow\n"
	        "  [color=green]mixer[reset] /listmidi");

	MSG_Add("MSCDEX_SUCCESS","MSCDEX installed.\n");
	MSG_Add("MSCDEX_ERROR_MULTIPLE_CDROMS","MSCDEX: Failure: Drive-letters of multiple CD-ROM drives have to be continuous.\n");
	MSG_Add("MSCDEX_ERROR_NOT_SUPPORTED","MSCDEX: Failure: Not yet supported.\n");
	MSG_Add("MSCDEX_ERROR_PATH","MSCDEX: Specified location is not a CD-ROM drive.\n");
	MSG_Add("MSCDEX_ERROR_OPEN","MSCDEX: Failure: Invalid file or unable to open.\n");
	MSG_Add("MSCDEX_TOO_MANY_DRIVES","MSCDEX: Failure: Too many CD-ROM drives (max: 5). MSCDEX Installation failed.\n");
	MSG_Add("MSCDEX_LIMITED_SUPPORT","MSCDEX: Mounted subdirectory: limited support.\n");
	MSG_Add("MSCDEX_INVALID_FILEFORMAT","MSCDEX: Failure: File is either no ISO/CUE image or contains errors.\n");
	MSG_Add("MSCDEX_UNKNOWN_ERROR","MSCDEX: Failure: Unknown error.\n");
	MSG_Add("MSCDEX_WARNING_NO_OPTION", "MSCDEX: Warning: Ignoring unsupported option '%s'.\n");

	MSG_Add("SHELL_CMD_RESCAN_HELP_LONG",
	        "Scans for changes on mounted DOS drives.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]rescan[reset] [color=cyan]DRIVE[reset]\n"
	        "  [color=green]rescan[reset] [/a]\n"
	        "\n"
	        "Where:\n"
	        "  [color=cyan]DRIVE[reset] is the drive to scan for changes.\n"
	        "\n"
	        "Notes:\n"
	        "  - Running [color=green]rescan[reset] without an argument scans for changes of the current drive.\n"
	        "  - Changes to this drive made on the host will then be reflected inside DOS.\n"
	        "  - You can also scan for changes on all mounted drives with the /a option.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]rescan[reset] [color=cyan]c:[reset]\n"
	        "  [color=green]rescan[reset] /a\n");
	MSG_Add("PROGRAM_RESCAN_SUCCESS","Drive re-scanned.\n");

	MSG_Add("PROGRAM_INTRO",
	        "\033[2J\033[32;1mWelcome to DOSBox Staging\033[0m, an x86 emulator with sound and graphics.\n"
	        "DOSBox creates a shell for you which looks like old plain DOS.\n"
	        "\n"
	        "For information about basic mount type \033[34;1mintro mount\033[0m\n"
	        "For information about CD-ROM support type \033[34;1mintro cdrom\033[0m\n"
	        "For information about special keys type \033[34;1mintro special\033[0m\n"
	        "For more information, visit DOSBox Staging wiki:\033[34;1m\n" WIKI_URL
	        "\033[0m\n"
	        "\n"
	        "\033[31;1mDOSBox will stop/exit without a warning if an error occurred!\033[0m\n");
	MSG_Add("PROGRAM_INTRO_MOUNT_START",
		"\033[2J\033[32;1mHere are some commands to get you started:\033[0m\n"
		"Before you can use the files located on your own filesystem,\n"
		"You have to mount the directory containing the files.\n"
		"\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_WINDOWS",
		"\033[44;1m\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n"
		"\xBA \033[32mmount c c:\\dosgames\\\033[37m will create a C drive with c:\\dosgames as contents.\xBA\n"
		"\xBA                                                                         \xBA\n"
		"\xBA \033[32mc:\\dosgames\\\033[37m is an example. Replace it with your own games directory.  \033[37m \xBA\n"
		"\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\033[0m\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_OTHER",
		"\033[44;1m\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n"
		"\xBA \033[32mmount c ~/dosgames\033[37m will create a C drive with ~/dosgames as contents.\xBA\n"
		"\xBA                                                                      \xBA\n"
		"\xBA \033[32m~/dosgames\033[37m is an example. Replace it with your own games directory.\033[37m  \xBA\n"
		"\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\033[0m\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_END",
		"After successfully mounting the disk you can type \033[34;1mc:\033[0m to go to your freshly\n"
		"mounted C-drive. Typing \033[34;1mdir\033[0m there will show its contents."
		" \033[34;1mcd\033[0m will allow you to\n"
		"enter a directory (recognised by the \033[33;1m[]\033[0m in a directory listing).\n"
		"You can run programs/files with extensions \033[31m.exe .bat\033[0m and \033[31m.com\033[0m.\n"
		);
	MSG_Add("PROGRAM_INTRO_CDROM_WINDOWS",
	        "\033[2J\033[32;1mHow to mount a real/virtual CD-ROM Drive in DOSBox:\033[0m\n"
	        "DOSBox provides CD-ROM emulation on two levels.\n"
	        "\n"
	        "The \033[33mbasic\033[0m level works on all normal directories, which installs MSCDEX\n"
	        "and marks the files read-only. Usually this is enough for most games:\n"
	        "\033[34;1mmount D C:\\example -t cdrom\033[0m\n"
	        "If it doesn't work you might have to tell DOSBox the label of the CD-ROM:\n"
	        "\033[34;1mmount D C:\\example -t cdrom -label CDLABEL\033[0m\n"
	        "\n"
	        "The \033[33mnext\033[0m level adds some low-level support.\n"
	        "Therefore only works on CD-ROM drives:\n"
	        "\033[34;1mmount d \033[0;31mD:\\\033[34;1m -t cdrom -usecd \033[33m0\033[0m\n"
	        "Replace \033[0;31mD:\\\033[0m with the location of your CD-ROM.\n"
	        "Replace the \033[33;1m0\033[0m in \033[34;1m-usecd \033[33m0\033[0m with the number reported for your CD-ROM if you type:\n"
	        "\033[34;1mmount -listcd\033[0m\n"
	        "\n"
	        "Additionally, you can use imgmount to mount iso or cue/bin images:\n"
	        "\033[34;1mimgmount D C:\\cd.iso -t cdrom\033[0m\n"
	        "\033[34;1mimgmount D C:\\cd.cue -t cdrom\033[0m\n");
	MSG_Add("PROGRAM_INTRO_CDROM_OTHER",
	        "\033[2J\033[32;1mHow to mount a real/virtual CD-ROM Drive in DOSBox:\033[0m\n"
	        "DOSBox provides CD-ROM emulation on two levels.\n"
	        "\n"
	        "The \033[33mbasic\033[0m level works on all normal directories, which installs MSCDEX\n"
	        "and marks the files read-only. Usually this is enough for most games:\n"
	        "\033[34;1mmount D ~/example -t cdrom\033[0m\n"
	        "If it doesn't work you might have to tell DOSBox the label of the CD-ROM:\n"
	        "\033[34;1mmount D ~/example -t cdrom -label CDLABEL\033[0m\n"
	        "\n"
	        "The \033[33mnext\033[0m level adds some low-level support.\n"
	        "Therefore only works on CD-ROM drives:\n"
	        "\033[34;1mmount d \033[0;31m~/example\033[34;1m -t cdrom -usecd \033[33m0\033[0m\n"
	        "\n"
	        "Replace \033[0;31m~/example\033[0m with the location of your CD-ROM.\n"
	        "Replace the \033[33;1m0\033[0m in \033[34;1m-usecd \033[33m0\033[0m with the number reported for your CD-ROM if you type:\n"
	        "\033[34;1mmount -listcd\033[0m\n"
	        "\n"
	        "Additionally, you can use imgmount to mount iso or cue/bin images:\n"
	        "\033[34;1mimgmount D ~/cd.iso -t cdrom\033[0m\n"
	        "\033[34;1mimgmount D ~/cd.cue -t cdrom\033[0m\n");
	MSG_Add("PROGRAM_INTRO_SPECIAL",
	        "\033[2J\033[32;1mSpecial keys:\033[0m\n"
	        "These are the default keybindings.\n"
	        "They can be changed in the \033[33mkeymapper\033[0m.\n"
	        "\n"
	        "\033[33;1m%s+Enter\033[0m  Switch between fullscreen and window mode.\n"
	        "\033[33;1m%s+Pause\033[0m  Pause/Unpause emulator.\n"
	        "\033[33;1m%s+F1\033[0m   %s Start the \033[33mkeymapper\033[0m.\n"
	        "\033[33;1m%s+F4\033[0m   %s Swap mounted disk image, scan for changes on all drives.\n"
	        "\033[33;1m%s+F5\033[0m   %s Save a screenshot.\n"
	        "\033[33;1m%s+F6\033[0m   %s Start/Stop recording sound output to a wave file.\n"
	        "\033[33;1m%s+F7\033[0m   %s Start/Stop recording video output to a zmbv file.\n"
	        "\033[33;1m%s+F9\033[0m   %s Shutdown emulator.\n"
	        "\033[33;1m%s+F10\033[0m  %s Capture/Release the mouse.\n"
	        "\033[33;1m%s+F11\033[0m  %s Slow down emulation.\n"
	        "\033[33;1m%s+F12\033[0m  %s Speed up emulation.\n"
	        "\033[33;1m%s+F12\033[0m    Unlock speed (turbo button/fast forward).\n");

	MSG_Add("SHELL_CMD_BOOT_HELP_LONG",
	        "Boots DOSBox Staging from a DOS drive or disk image.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]boot[reset] [color=white]DRIVE[reset]\n"
	        "  [color=green]boot[reset] [color=cyan]IMAGEFILE[reset]\n"
	        "\n"
	        "Where:\n"
	        "  [color=white]DRIVE[reset] is a drive to boot from, must be [color=white]A:[reset], [color=white]C:[reset], or [color=white]D:[reset].\n"
	        "  [color=cyan]IMAGEFILE[reset] is one or more floppy images, separated by spaces.\n"
	        "\n"
	        "Notes:\n"
	        "  A DOS drive letter must have been mounted previously with [color=green]imgmount[reset] command.\n"
	        "  The DOS drive or disk image must be bootable, containing DOS system files.\n"
	        "  If more than one disk images are specified, you can swap them with a hotkey.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]boot[reset] [color=white]c:[reset]\n"
	        "  [color=green]boot[reset] [color=cyan]disk1.ima disk2.ima[reset]\n");
	MSG_Add("PROGRAM_BOOT_NOT_EXIST","Bootdisk file does not exist.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_NOT_OPEN","Cannot open bootdisk file.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_WRITE_PROTECTED","Image file is read-only! Might create problems.\n");
	MSG_Add("PROGRAM_BOOT_PRINT_ERROR",
	        "This command boots DOSBox Staging from either a floppy or hard disk image.\n\n"
	        "For this command, one can specify a succession of floppy disks swappable\n"
	        "by pressing %s+F4, and -l specifies the mounted drive to boot from.  If\n"
	        "no drive letter is specified, this defaults to booting from the A drive.\n"
	        "The only bootable drive letters are A, C, and D.  For booting from a hard\n"
	        "drive (C or D), the image should have already been mounted using the\n"
	        "\033[34;1mIMGMOUNT\033[0m command.\n\n"
	        "Type \033[34;1mBOOT /?\033[0m for the syntax of this command.\033[0m\n");
	MSG_Add("PROGRAM_BOOT_UNABLE","Unable to boot off of drive %c");
	MSG_Add("PROGRAM_BOOT_IMAGE_OPEN","Opening image file: %s\n");
	MSG_Add("PROGRAM_BOOT_IMAGE_NOT_OPEN","Cannot open %s");
	MSG_Add("PROGRAM_BOOT_BOOT","Booting from drive %c...\n");
	MSG_Add("PROGRAM_BOOT_CART_WO_PCJR","PCjr cartridge found, but machine is not PCjr");
	MSG_Add("PROGRAM_BOOT_CART_LIST_CMDS", "Available PCjr cartridge commands: %s");
	MSG_Add("PROGRAM_BOOT_CART_NO_CMDS", "No PCjr cartridge commands found");
	MSG_Add("SHELL_CMD_LOADROM_HELP_LONG",
	        "Loads a ROM image of the video BIOS or IBM BASIC.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]loadrom [color=cyan]IMAGEFILE[reset]\n"
	        "\n"
	        "Where:\n"
	        "  [color=cyan]IMAGEFILE[reset] is a video BIOS or IBM BASIC ROM image.\n"
	        "\n"
	        "Notes:\n"
	        "   After loading an IBM BASIC ROM image into the emulated ROM with the command,\n"
	        "   you can run the original IBM BASIC interpreter program in DOSBox Staging.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]loadrom[reset] [color=cyan]bios.rom[reset]\n");
	MSG_Add("PROGRAM_LOADROM_SPECIFY_FILE","Must specify ROM file to load.\n");
	MSG_Add("PROGRAM_LOADROM_CANT_OPEN","ROM file not accessible.\n");
	MSG_Add("PROGRAM_LOADROM_TOO_LARGE","ROM file too large.\n");
	MSG_Add("PROGRAM_LOADROM_INCOMPATIBLE","Video BIOS not supported by machine type.\n");
	MSG_Add("PROGRAM_LOADROM_UNRECOGNIZED","ROM file not recognized.\n");
	MSG_Add("PROGRAM_LOADROM_BASIC_LOADED","BASIC ROM loaded.\n");

	MSG_Add("SHELL_CMD_IMGMOUNT_HELP",
	        "mounts compact disc image(s) or floppy disk image(s) to a given drive letter.\n");

	MSG_Add("SHELL_CMD_IMGMOUNT_HELP_LONG",
	        "Mount a CD-ROM, floppy, or disk image to a drive letter.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]imgmount[reset] [color=white]DRIVE[reset] [color=cyan]CDROM-SET[reset] [-fs iso] [-ide] -t cdrom|iso\n"
	        "  [color=green]imgmount[reset] [color=white]DRIVE[reset] [color=cyan]IMAGEFILE[reset] [IMAGEFILE2 [..]] [-fs fat] -t hdd|floppy -ro\n"
	        "  [color=green]imgmount[reset] [color=white]DRIVE[reset] [color=cyan]BOOTIMAGE[reset] [-fs fat|none] -t hdd -size GEOMETRY -ro\n"
	        "  [color=green]imgmount[reset] -u [color=white]DRIVE[reset]  (unmounts the [color=white]DRIVE[reset]'s image)\n"
	        "\n"
	        "Where:\n"
	        "  [color=white]DRIVE[reset]     is the drive letter where the image will be mounted: a, c, d, ...\n"
	        "  [color=cyan]CDROM-SET[reset] is an ISO, CUE+BIN, CUE+ISO, or CUE+ISO+FLAC/OPUS/OGG/MP3/WAV\n"
	        "  [color=cyan]IMAGEFILE[reset] is a hard drive or floppy image in FAT16 or FAT12 format\n"
	        "  [color=cyan]BOOTIMAGE[reset] is a bootable disk image with specified -size GEOMETRY:\n"
	        "            bytes-per-sector,sectors-per-head,heads,cylinders\n"
	        "Notes:\n"
	        "  - %s+F4 swaps & mounts the next [color=cyan]CDROM-SET[reset] or [color=cyan]BOOTIMAGE[reset], if provided.\n"
	        "  - The -ro flag mounts the disk image in read-only (write-protected) mode.\n"
	        "  - The -ide flag emulates an IDE controller with attached IDE CD drive, useful\n"
	        "    for CD-based games that need a real DOS environment via bootable HDD image.\n"
	        "Examples:\n"
#if defined(WIN32)
	        "  [color=green]imgmount[reset] [color=white]D[reset] [color=cyan]C:\\games\\doom.iso[reset] -t cdrom\n"
	        "  [color=green]imgmount[reset] [color=white]D[reset] [color=cyan]cd/quake1.cue[reset] -t cdrom\n"
	        "  [color=green]imgmount[reset] [color=white]A[reset] [color=cyan]floppy1.img floppy2.img floppy3.img[reset] -t floppy -ro\n"
	        "  [color=green]imgmount[reset] [color=white]C[reset] [color=cyan]bootable.img[reset] -t hdd -fs none -size 512,63,32,1023\n"
#elif defined(MACOSX)
	        "  [color=green]imgmount[reset] [color=white]D[reset] [color=cyan]/Users/USERNAME/games/doom.iso[reset] -t cdrom\n"
	        "  [color=green]imgmount[reset] [color=white]D[reset] [color=cyan]cd/quake1.cue[reset] -t cdrom\n"
	        "  [color=green]imgmount[reset] [color=white]A[reset] [color=cyan]floppy1.img floppy2.img floppy3.img[reset] -t floppy -ro\n"
	        "  [color=green]imgmount[reset] [color=white]C[reset] [color=cyan]bootable.img[reset] -t hdd -fs none -size 512,63,32,1023\n"
#else
	        "  [color=green]imgmount[reset] [color=white]D[reset] [color=cyan]/home/USERNAME/games/doom.iso[reset] -t cdrom\n"
	        "  [color=green]imgmount[reset] [color=white]D[reset] [color=cyan]cd/quake1.cue[reset] -t cdrom\n"
	        "  [color=green]imgmount[reset] [color=white]A[reset] [color=cyan]floppy1.img floppy2.img floppy3.img[reset] -t floppy -ro\n"
	        "  [color=green]imgmount[reset] [color=white]C[reset] [color=cyan]bootable.img[reset] -t hdd -fs none -size 512,63,32,1023\n"
#endif
	);

	MSG_Add("SHELL_CMD_MOUNT_HELP",
	        "maps physical folders or drives to a virtual drive letter.\n");

	MSG_Add("SHELL_CMD_MOUNT_HELP_LONG",
	        "Mount a directory from the host OS to a drive letter.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]mount[reset] [color=white]DRIVE[reset] [color=cyan]DIRECTORY[reset] [-t TYPE] [-usecd #] [-freesize SIZE] [-label LABEL]\n"
	        "  [color=green]mount[reset] -listcd / -cd (lists all detected CD-ROM drives and their numbers)\n"
	        "  [color=green]mount[reset] -u [color=white]DRIVE[reset]  (unmounts the DRIVE's directory)\n"
	        "\n"
	        "Where:\n"
	        "  [color=white]DRIVE[reset]     the drive letter where the directory will be mounted: A, C, D, ...\n"
	        "  [color=cyan]DIRECTORY[reset] is the directory on the host OS to be mounted\n"
	        "  TYPE      type of the directory to mount: dir, floppy, cdrom, or overlay\n"
	        "  SIZE      free space for the virtual drive (KiB for floppies, MiB otherwise)\n"
	        "  LABEL     drive label name to be used\n"
	        "\n"
	        "Notes:\n"
	        "  - '-t overlay' redirects writes for mounted drive to another directory.\n"
	        "  - Additional options are described in the manual (README file, chapter 4).\n"
	        "\n"
	        "Examples:\n"
#if defined(WIN32)
	        "  [color=green]mount[reset] [color=white]C[reset] [color=cyan]C:\\dosgames[reset]\n"
	        "  [color=green]mount[reset] [color=white]D[reset] [color=cyan]D:\\[reset] -t cdrom\n"
	        "  [color=green]mount[reset] [color=white]C[reset] [color=cyan]my_savegame_files[reset] -t overlay\n"
#elif defined(MACOSX)
	        "  [color=green]mount[reset] [color=white]C[reset] [color=cyan]~/dosgames[reset]\n"
	        "  [color=green]mount[reset] [color=white]D[reset] [color=cyan]\"/Volumes/Game CD\"[reset] -t cdrom\n"
	        "  [color=green]mount[reset] [color=white]C[reset] [color=cyan]my_savegame_files[reset] -t overlay\n"
#else
	        "  [color=green]mount[reset] [color=white]C[reset] [color=cyan]~/dosgames[reset]\n"
	        "  [color=green]mount[reset] [color=white]D[reset] [color=cyan]\"/media/USERNAME/Game CD\"[reset] -t cdrom\n"
	        "  [color=green]mount[reset] [color=white]C[reset] [color=cyan]my_savegame_files[reset] -t overlay\n"
#endif
	);

	MSG_Add("PROGRAM_IMGMOUNT_STATUS_1",
	        "The currently mounted disk and CD image drives are:\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_DRIVE",
	        "Must specify drive letter to mount image at.\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY2",
	        "Must specify drive number (0 or 3) to mount image at (0,1=fda,fdb;2,3=hda,hdb).\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY",
		"For \033[33mCD-ROM\033[0m images:   \033[34;1mIMGMOUNT drive-letter location-of-image -t iso\033[0m\n"
		"\n"
		"For \033[33mhardrive\033[0m images: Must specify drive geometry for hard drives:\n"
		"bytes_per_sector, sectors_per_cylinder, heads_per_cylinder, cylinder_count.\n"
		"\033[34;1mIMGMOUNT drive-letter location-of-image -size bps,spc,hpc,cyl\033[0m\n");
	MSG_Add("PROGRAM_IMGMOUNT_STATUS_NONE",
		"No drive available\n");
	MSG_Add("PROGRAM_IMGMOUNT_IDE_CONTROLLERS_UNAVAILABLE",
		"No available IDE controllers. Drive will not have IDE emulation.\n");
	MSG_Add("PROGRAM_IMGMOUNT_INVALID_IMAGE","Could not load image file.\n"
		"Check that the path is correct and the image is accessible.\n");
	MSG_Add("PROGRAM_IMGMOUNT_INVALID_GEOMETRY","Could not extract drive geometry from image.\n"
		"Use parameter -size bps,spc,hpc,cyl to specify the geometry.\n");
	MSG_Add("PROGRAM_IMGMOUNT_TYPE_UNSUPPORTED",
	        "Type '%s' is unsupported. Specify 'floppy', 'hdd', 'cdrom', or 'iso'.\n");
	MSG_Add("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED","Format \"%s\" is unsupported. Specify \"fat\" or \"iso\" or \"none\".\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_FILE","Must specify file-image to mount.\n");
	MSG_Add("PROGRAM_IMGMOUNT_FILE_NOT_FOUND","Image file not found.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT","To mount directories, use the \033[34;1mMOUNT\033[0m command, not the \033[34;1mIMGMOUNT\033[0m command.\n");
	MSG_Add("PROGRAM_IMGMOUNT_ALREADY_MOUNTED","Drive already mounted at that letter.\n");
	MSG_Add("PROGRAM_IMGMOUNT_CANT_CREATE","Can't create drive from file.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT_NUMBER","Drive number %d mounted as %s\n");
	MSG_Add("PROGRAM_IMGMOUNT_NON_LOCAL_DRIVE", "The image must be on a host or local drive.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MULTIPLE_NON_CUEISO_FILES", "Using multiple files is only supported for cue/iso images.\n");

	MSG_Add("PROGRAM_KEYB_INFO","Codepage %i has been loaded\n");
	MSG_Add("PROGRAM_KEYB_INFO_LAYOUT","Codepage %i has been loaded for layout %s\n");
	MSG_Add("PROGRAM_KEYB_HELP_LONG",
	        "Configures a keyboard for a specific language.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]keyb[reset] [color=cyan][LANGCODE][reset]\n"
	        "  [color=green]keyb[reset] [color=cyan]LANGCODE[reset] [color=white]CODEPAGE[reset] [CODEPAGEFILE]\n"
	        "\n"
	        "Where:\n"
	        "  [color=cyan]LANGCODE[reset]     is a language code or keyboard layout ID.\n"
	        "  [color=white]CODEPAGE[reset]     is a code page number, such as [color=white]437[reset] and [color=white]850[reset].\n"
	        "  CODEPAGEFILE is a file containing information for a code page.\n"
	        "\n"
	        "Notes:\n"
	        "  Running [color=green]keyb[reset] without an argument shows the currently loaded keyboard layout\n"
	        "  and code page. It will change to [color=cyan]LANGCODE[reset] if provided, optionally with a\n"
	        "  [color=white]CODEPAGE[reset] and an additional CODEPAGEFILE to load the specified code page\n"
	        "  number and code page file if provided. This command is especially useful if\n"
	        "  you use a non-US keyboard, and [color=cyan]LANGCODE[reset] can also be set in the configuration\n"
	        "  file under the [dos] section using the \"keyboardlayout = [color=cyan]LANGCODE[reset]\" setting.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]KEYB[reset]\n"
	        "  [color=green]KEYB[reset] [color=cyan]uk[reset]\n"
	        "  [color=green]KEYB[reset] [color=cyan]sp[reset] [color=white]850[reset]\n"
	        "  [color=green]KEYB[reset] [color=cyan]de[reset] [color=white]858[reset] mycp.cpi\n");
	MSG_Add("PROGRAM_KEYB_NOERROR","Keyboard layout %s loaded for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_FILENOTFOUND","Keyboard file %s not found\n\n");
	MSG_Add("PROGRAM_KEYB_INVALIDFILE","Keyboard file %s invalid\n");
	MSG_Add("PROGRAM_KEYB_LAYOUTNOTFOUND","No layout in %s for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_INVCPFILE","None or invalid codepage file for layout %s\n\n");

	MSG_Add("PROGRAM_SERIAL_HELP",
	        "Manages the serial ports.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]serial[reset] [color=white][PORT#][reset]                List all or specified serial ports.\n"
	        "  [color=green]serial[reset] [color=white]PORT#[reset] [color=cyan]TYPE[reset] [settings]  Set the specified port to the given type.\n"
	        "\n"
	        "Where:\n"
	        "  [color=white]PORT#[reset] The port number: [color=white]1[reset], [color=white]2[reset], [color=white]3[reset], or [color=white]4[reset]\n"
	        "  [color=cyan]TYPE[reset]  The port type: [color=cyan]MODEM[reset], [color=cyan]NULLMODEM[reset], [color=cyan]DIRECTSERIAL[reset], [color=cyan]DUMMY[reset], or [color=cyan]DISABLED[reset]\n"
	        "\n"
	        "Notes:\n"
	        "  Optional settings for each [color=cyan]TYPE[reset]:\n"
	        "  For [color=cyan]MODEM[reset]        : IRQ, LISTENPORT, SOCK\n"
	        "  For [color=cyan]NULLMODEM[reset]    : IRQ, SERVER, RXDELAY, TXDELAY, TELNET,\n"
	        "                     USEDTR, TRANSPARENT, PORT, INHSOCKET, SOCK\n"
	        "  For [color=cyan]DIRECTSERIAL[reset] : IRQ, REALPORT (required), RXDELAY\n"
	        "  For [color=cyan]DUMMY[reset]        : IRQ\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]SERIAL[reset]                                       : List the current serial ports\n"
	        "  [color=green]SERIAL[reset] [color=white]1[reset] [color=cyan]NULLMODEM[reset] PORT:1250                 : Listen on TCP:1250 as server\n"
	        "  [color=green]SERIAL[reset] [color=white]2[reset] [color=cyan]NULLMODEM[reset] SERVER:10.0.0.6 PORT:1250 : Connect to TCP:1250 as client\n"
	        "  [color=green]SERIAL[reset] [color=white]3[reset] [color=cyan]MODEM[reset] LISTENPORT:5000 SOCK:1        : Listen on UDP:5000 as server\n"
	        "  [color=green]SERIAL[reset] [color=white]4[reset] [color=cyan]DIRECTSERIAL[reset] REALPORT:ttyUSB0       : Use a physical port on Linux\n");
	MSG_Add("PROGRAM_SERIAL_SHOW_PORT", "COM%d: %s %s\n");
	MSG_Add("PROGRAM_SERIAL_BAD_PORT",
	        "Must specify a numeric port value between 1 and %d, inclusive.\n");
	MSG_Add("PROGRAM_SERIAL_BAD_TYPE", "Type must be one of the following:\n");
	MSG_Add("PROGRAM_SERIAL_INDENTED_LIST", "  %s\n");

	MSG_Add("PROGRAM_PLACEHOLDER_SHORT_HELP", "This program is a placeholder");
	MSG_Add("PROGRAM_PLACEHOLDER_LONG_HELP",
	        "%s is only a placeholder.\n"
	        "\nInstall a 3rd-party and give its PATH precedence.\n"
	        "\nFor example:");

	MSG_Add("UTILITY_DRIVE_EXAMPLE_NO_TRANSLATE",
	        "\n   [autoexec]\n"
#if defined(WIN32)
	        "   mount u C:\\Users\\username\\dos\\utils\n"
#else
	        "   mount u ~/dos/utils\n"
#endif
	        "   set PATH=u:\\;%PATH%\n\n");

	MSG_Add("VISIT_FOR_MORE_HELP", "Visit the following for more help:");
	MSG_Add("WIKI_ADD_UTILITIES_ARTICLE", WIKI_ADD_UTILITIES_ARTICLE);
	MSG_Add("WIKI_URL", WIKI_URL);

	const auto add_autoexec = false;
	Add_VFiles(add_autoexec);
}
