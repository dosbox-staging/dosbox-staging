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

#include "program_intro.h"

#include "mapper.h"

void INTRO::WriteOutProgramIntroSpecial()
{
	WriteOut(MSG_Get("PROGRAM_INTRO_SPECIAL"),
	         MMOD2_NAME,       // Alt, for fullscreen toggle
	         MMOD2_NAME,       // Alt, for pause/unpause
	         PRIMARY_MOD_NAME, // Ctrl/Cmd, for keymapper
	         PRIMARY_MOD_PAD,
	         PRIMARY_MOD_NAME, // Ctrl/Cmd, for swap disk image
	         PRIMARY_MOD_PAD,
	         PRIMARY_MOD_NAME, // Ctrl/Cmd, for screenshot
	         PRIMARY_MOD_PAD,
	         PRIMARY_MOD_NAME, // Ctrl/Cmd, for sound recording
	         PRIMARY_MOD_PAD,
	         PRIMARY_MOD_NAME, // Ctrl/Cmd, for video recording
	         PRIMARY_MOD_PAD,
	         PRIMARY_MOD_NAME, // Ctrl/Cmd, for mute/unmute
	         PRIMARY_MOD_PAD,
	         PRIMARY_MOD_NAME, // Ctrl/Cmd, for shutdown
	         PRIMARY_MOD_PAD,
	         PRIMARY_MOD_NAME, // Ctrl/Cmd, for mouse capture
	         PRIMARY_MOD_PAD,
	         PRIMARY_MOD_NAME, // Ctrl/Cmd, for slow down
	         PRIMARY_MOD_PAD,
	         PRIMARY_MOD_NAME, // Ctrl/Cmd, for speed up
	         PRIMARY_MOD_PAD,
	         MMOD2_NAME); // Alt, for turbo
}

void INTRO::DisplayMount(void) {
    /* Basic mounting has a version for each operating system.
        * This is done this way so both messages appear in the language file*/
    WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_START"));
#if (WIN32)
    WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_WINDOWS"));
#else
    WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_OTHER"));
#endif
    WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_END"));
}

void INTRO::Run(void) {
	// Usage
	if (HelpRequested()) {
		WriteOut(MSG_Get("SHELL_CMD_INTRO_HELP_LONG"));
		return;
	}
    /* Only run if called from the first shell (Xcom TFTD runs any intro file in the path) */
    if (DOS_PSP(dos.psp()).GetParent() != DOS_PSP(DOS_PSP(dos.psp()).GetParent()).GetParent()) return;
    if (cmd->FindExist("cdrom",false)) {
#ifdef WIN32
        WriteOut(MSG_Get("PROGRAM_INTRO_CDROM_WINDOWS"));
#else
        WriteOut(MSG_Get("PROGRAM_INTRO_CDROM_OTHER"));
#endif
        return;
    }
    if (cmd->FindExist("mount",false)) {
        WriteOut("\033[2J");//Clear screen before printing
        DisplayMount();
        return;
    }
    if (cmd->FindExist("special",false)) {
        WriteOutProgramIntroSpecial();
        return;
    }
    /* Default action is to show all pages */
    WriteOut(MSG_Get("PROGRAM_INTRO"));
    uint8_t c;uint16_t n=1;
    DOS_ReadFile (STDIN,&c,&n);
    DisplayMount();
    DOS_ReadFile (STDIN,&c,&n);
#ifdef WIN32
    WriteOut(MSG_Get("PROGRAM_INTRO_CDROM_WINDOWS"));
#else
    WriteOut(MSG_Get("PROGRAM_INTRO_CDROM_OTHER"));
#endif
    DOS_ReadFile(STDIN, &c, &n);
    WriteOutProgramIntroSpecial();
}

void INTRO::AddMessages() {
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
	        "\033[33;1m%s+F8\033[0m   %s Mute/Unmute the audio.\n"
	        "\033[33;1m%s+F9\033[0m   %s Shutdown emulator.\n"
	        "\033[33;1m%s+F10\033[0m  %s Capture/Release the mouse.\n"
	        "\033[33;1m%s+F11\033[0m  %s Slow down emulation.\n"
	        "\033[33;1m%s+F12\033[0m  %s Speed up emulation.\n"
	        "\033[33;1m%s+F12\033[0m    Unlock speed (turbo button/fast forward).\n");
}
