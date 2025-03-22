/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2024  The DOSBox Staging Team
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
#include "program_more_output.h"

void INTRO::WriteOutProgramIntroSpecial()
{
	WriteOut(MSG_Get("PROGRAM_INTRO_SPECIAL"),
	         MMOD2_NAME,       // Alt, for fullscreen toggle
	         MMOD2_NAME,       // Alt, for pause/unpause
	         PRIMARY_MOD_NAME, // Ctrl/Cmd, for keymapper
	         PRIMARY_MOD_PAD,
	         PRIMARY_MOD_NAME, // Ctrl/Cmd, for swap disk image
	         PRIMARY_MOD_PAD,
	         MMOD2_NAME, // Alt,      to screenshot the rendered output
	         PRIMARY_MOD_NAME, // Ctrl/Cmd, to screenshot the image source
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

void INTRO::DisplayMount(void)
{
	/* Basic mounting has a version for each operating system.
	 * This is done this way so both messages appear in the language file*/
	WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_START"));
#ifdef WIN32
	WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_WINDOWS"));
#else
	WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_OTHER"));
#endif
	WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_END"));
}

void INTRO::Run(void)
{
	// Usage
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_INTRO_HELP"));
		output.AddString("\n");
		output.AddString(MSG_Get("PROGRAM_INTRO_HELP_LONG"));
		output.Display();
		return;
	}
	if (cmd->FindExist("cdrom", false)) {
		WriteOut(MSG_Get("PROGRAM_INTRO_CDROM"), PRIMARY_MOD_NAME);
		return;
	}
	if (cmd->FindExist("mount", false)) {
		WriteOut("\033[2J"); // Clear screen before printing
		DisplayMount();
		return;
	}
	if (cmd->FindExist("special", false)) {
		WriteOutProgramIntroSpecial();
		return;
	}
	/* Default action is to show all pages */
	WriteOut(MSG_Get("PROGRAM_INTRO"));
	uint8_t c;
	uint16_t n = 1;
	DOS_ReadFile(STDIN, &c, &n);
	DisplayMount();
	DOS_ReadFile(STDIN, &c, &n);
	WriteOut(MSG_Get("PROGRAM_INTRO_CDROM"));
	DOS_ReadFile(STDIN, &c, &n);
	WriteOutProgramIntroSpecial();
}

void INTRO::AddMessages()
{
	MSG_Add("PROGRAM_INTRO_HELP",
	        "Display a full-screen introduction to DOSBox Staging.\n\n");

	MSG_Add("PROGRAM_INTRO_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]intro[reset]\n"
	        "  [color=light-green]intro[reset] [color=white]PAGE[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]PAGE[reset]  page name to display, including [color=white]cdrom[reset], [color=white]mount[reset], and [color=white]special[reset]\n"
	        "\n"
	        "Notes:\n"
	        "  Running [color=light-green]intro[reset] without an argument displays one information page at a time;\n"
	        "  press any key to move to the next page. If a page name is provided, then the\n"
	        "  specified page will be displayed directly.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]intro[reset]\n"
	        "  [color=light-green]intro[reset] [color=white]cdrom[reset]\n"
	        "\n");

	MSG_Add("PROGRAM_INTRO",
	        "[erases=entire][color=light-green]Welcome to DOSBox Staging[reset], an x86 PC emulator to run old DOS games.\n"
	        "\n"
	        "DOSBox Staging includes its own DOS-like shell, so you don't need real\n"
		    "MS-DOS to use it.\n"
	        "\n"
	        "For basic drive mounting info, run [color=light-blue]intro mount[reset]\n"
	        "For basic CD-ROM mounting info, run [color=light-blue]intro cdrom[reset]\n"
	        "For the list of special keys, run [color=light-blue]intro special[reset]\n"
	        "\n"
	        "To get the most out of DOSBox Staging, read our [color=white]Getting Started guide[reset]:\n"
	        "[color=yellow]https://www.dosbox-staging.org/getting-started/[reset]\n"
	        "\n"
	        "The [color=white]DOSBox Staging wiki[reset] contains many helpful guides and game-specific tips:\n"
	        "[color=yellow]https://github.com/dosbox-staging/dosbox-staging/wiki[reset]\n"
	        "\n");

	MSG_Add("PROGRAM_INTRO_MOUNT_START",
	        "[erases=entire][color=light-green]Here are some commands to get you started:[reset]\n"
	        "\n"
	        "Before you can use the files located on your own filesystem,\n"
	        "you have to mount the directory containing the files.\n"
	        "\n");

	MSG_Add("PROGRAM_INTRO_MOUNT_WINDOWS",
	        "[bgcolor=blue][color=white]╔═════════════════════════════════════════════════════════════════════════╗\n"
	        "║ [color=light-green]mount c c:\\dosgames\\ [color=white]will create a C drive with c:\\dosgames as contents.║\n"
	        "║                                                                         ║\n"
	        "║ [color=light-green]c:\\dosgames\\ [color=white]is an example. Replace it with your own games directory.   ║\n"
	        "╚═════════════════════════════════════════════════════════════════════════╝[reset]\n\n");

	MSG_Add("PROGRAM_INTRO_MOUNT_OTHER",
	        "[bgcolor=blue][color=white]╔══════════════════════════════════════════════════════════════════════╗\n"
	        "║ [color=light-green]mount c ~/dosgames[color=white] will create a C drive with ~/dosgames as contents.║\n"
	        "║                                                                      ║\n"
	        "║ [color=light-green]~/dosgames[color=white] is an example. Replace it with your own games directory.  ║\n"
	        "╚══════════════════════════════════════════════════════════════════════╝[reset]\n");

	MSG_Add("PROGRAM_INTRO_MOUNT_END",
	        "After successfully mounting the disk, you can type [color=light-blue]c:[reset] to go to your freshly\n"
	        "mounted C drive. Typing [color=light-blue]dir[reset] there will show its contents."
	        " [color=light-blue]cd[reset] will allow you\n"
	        "to enter a directory (recognised by the [color=yellow][][reset] in a directory listing).\n"
	        "\n"
	        "You can run programs/files with extensions [color=red].exe .bat[reset] and [color=red].com[reset].\n");

	MSG_Add("PROGRAM_INTRO_CDROM",
	        "[erases=entire][color=yellow]Mounting CD-ROM drives and images[reset]\n"
	        "\n"
	        "The [color=light-green]mount[reset] command works on all normal directories. It installs MSCDEX\n"
	        "and marks the files as read-only.\n"
	        "\n"
	        "To mount the directory [color=light-cyan]cd-dir[reset] relative to the startup directory as the [color=white]D[reset] drive\n"
		    "(you can use absolute paths as well):\n"
	        "\n"
	        "  [color=light-green]mount [color=white]D [color=light-cyan]cd-dir [reset]-t cdrom\n"
	        "\n"
	        "For some programs, you'll need to specify the label of the CD-ROM as well:\n"
	        "\n"
	        "  [color=light-green]mount [color=white]D [color=light-cyan]cd-dir [reset]-t cdrom -label CDLABEL\n"
	        "\n"
	        "Use [color=light-green]imgmount[reset] to mount one or more ISO or CUE/BIN images to a drive letter:\n"
	        "\n"
	        "  [color=light-green]imgmount [color=white]D [color=light-cyan]cd/disc1.iso [reset]-t iso\n"
	        "  [color=light-green]imgmount [color=white]D [color=light-cyan]cd/disc1.cue [reset]-t iso\n"
	        "  [color=light-green]imgmount [color=white]D [color=light-cyan]cd/disc1.cue cd/disc2.cue [reset]-t iso\n"
	        "  [color=light-green]imgmount [color=white]D [color=light-cyan]cd/*.cue [reset]-t iso\n"
	        "\n"
	        "Press [color=yellow]%s+F4[reset] to switch between multiple CD-ROM images mounted to the same\n"
	        "drive letter.\n"
	        "\n"
	        "Run [color=light-green]imgmount /?[reset] for further details.\n"
	        "\n");

	MSG_Add("PROGRAM_INTRO_SPECIAL",
	        "[erases=entire][color=light-green]Special keys[reset]\n"
	        "\n"
	        "This is a list of the most important special keys.\n"
	        "You can change these key bindings in the [color=brown]keymapper[reset].\n"
	        "\n"
	        "[color=yellow]%s+Enter[reset]  Switch between fullscreen and window mode.\n"
	        "[color=yellow]%s+Pause[reset]  Pause/unpause the emulation.\n"
	        "[color=yellow]%s+F1[reset]   %s Open the [color=brown]keymapper[reset].\n"
	        "[color=yellow]%s+F4[reset]   %s Swap mounted disk image, and scan for changes on all mounted drives.\n"
	        "[color=yellow]%s+F5[reset]     Save screenshot.\n"
	        "[color=yellow]%s+F5[reset]   %s Save raw screenshot.\n"
	        "[color=yellow]%s+F6[reset]   %s Start/stop recording sound output to a WAV file.\n"
	        "[color=yellow]%s+F7[reset]   %s Start/Stop recording video output to a ZMBV file.\n"
	        "[color=yellow]%s+F8[reset]   %s Mute/unmute audio.\n"
	        "[color=yellow]%s+F9[reset]   %s Shut down the emulator.\n"
	        "[color=yellow]%s+F10[reset]  %s Capture/release the mouse.\n"
	        "[color=yellow]%s+F11[reset]  %s Increase CPU cycles (slow down the emulation).\n"
	        "[color=yellow]%s+F12[reset]  %s Decrease CPU cycles (speed up the emulation).\n"
	        "[color=yellow]%s+F12[reset]    Toggle fast-forward mode.\n"
	        "\n");
}
