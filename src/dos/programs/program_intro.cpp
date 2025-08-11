// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "program_intro.h"

#include "gui/mapper.h"
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

void INTRO::DisplayMount(void) {
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

void INTRO::Run(void) {
	// Usage
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_INTRO_HELP"));
		output.AddString("\n");
		output.AddString(MSG_Get("PROGRAM_INTRO_HELP_LONG"));
		output.Display();
		return;
	}
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
	MSG_Add("PROGRAM_INTRO_HELP",
	        "Display a full-screen introduction to DOSBox Staging.\n");
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
	        "  [color=light-green]intro[reset] [color=white]cdrom[reset]\n");
    MSG_Add("PROGRAM_INTRO",
	        "[erases=entire][color=light-green]Welcome to DOSBox Staging[reset], an x86 emulator with sound and graphics.\n"
	        "DOSBox creates a shell for you which looks like old plain DOS.\n"
	        "\n"
	        "For information about basic mount type [color=light-blue]intro mount[reset]\n"
	        "For information about CD-ROM support type [color=light-blue]intro cdrom[reset]\n"
	        "For information about special keys type [color=light-blue]intro special[reset]\n"
	        "For more information, visit DOSBox Staging wiki:[color=light-blue]\n" WIKI_URL
	        "[reset]\n"
	        "\n"
	        "[color=light-red]DOSBox will stop/exit without a warning if an error occurred![reset]\n");
	MSG_Add("PROGRAM_INTRO_MOUNT_START",
		"[erases=entire][color=light-green]Here are some commands to get you started:[reset]\n"
		"Before you can use the files located on your own filesystem,\n"
		"you have to mount the directory containing the files.\n"
		"\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_WINDOWS",
		"[bgcolor=blue][color=white]╔═════════════════════════════════════════════════════════════════════════╗\n"
		"║ [color=light-green]mount c c:\\dosgames\\ [color=white]will create a C drive with c:\\dosgames as contents.║\n"
		"║                                                                         ║\n"
		"║ [color=light-green]c:\\dosgames\\ [color=white]is an example. Replace it with your own games directory.   ║\n"
		"╚═════════════════════════════════════════════════════════════════════════╝[reset]\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_OTHER",
		"[bgcolor=blue][color=white]╔══════════════════════════════════════════════════════════════════════╗\n"
		"║ [color=light-green]mount c ~/dosgames[color=white] will create a C drive with ~/dosgames as contents.║\n"
		"║                                                                      ║\n"
		"║ [color=light-green]~/dosgames[color=white] is an example. Replace it with your own games directory.  ║\n"
		"╚══════════════════════════════════════════════════════════════════════╝[reset]\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_END",
		"After successfully mounting the disk you can type [color=light-blue]c:[reset] to go to your freshly\n"
		"mounted C-drive. Typing [color=light-blue]dir[reset] there will show its contents."
		" [color=light-blue]cd[reset] will allow you to\n"
		"enter a directory (recognised by the [color=yellow][][reset] in a directory listing).\n"
		"You can run programs/files with extensions [color=red].exe .bat[reset] and [color=red].com[reset].\n"
		);
	MSG_Add("PROGRAM_INTRO_CDROM_WINDOWS",
	        "[erases=entire][color=light-green]How to mount a real/virtual CD-ROM Drive in DOSBox:[reset]\n"
	        "The [color=light-blue]mount[reset] command works on all normal directories. It installs MSCDEX and marks\n"
	        "the files as read-only.\n"
	        "Usually this is enough for most games:\n"
	        "[color=light-blue]mount D C:\\example -t cdrom[reset]\n"
	        "If it doesn't work you might have to tell DOSBox the label of the CD-ROM:\n"
	        "[color=light-blue]mount D C:\\example -t cdrom -label CDLABEL[reset]\n"
	        "\n"
	        "Additionally, you can use [color=light-blue]imgmount[reset] to mount ISO or CUE/BIN images:\n"
	        "[color=light-blue]imgmount D C:\\cd.iso -t cdrom[reset]\n"
	        "[color=light-blue]imgmount D C:\\cd.cue -t cdrom[reset]\n");
	MSG_Add("PROGRAM_INTRO_CDROM_OTHER",
	        "[erases=entire][color=light-green]How to mount a real/virtual CD-ROM Drive in DOSBox:[reset]\n"
	        "The [color=light-blue]mount[reset] command works on all normal directories. It installs MSCDEX and marks\n"
	        "the files as read-only.\n"
	        "Usually this is enough for most games:\n"
	        "[color=light-blue]mount D ~/example -t cdrom[reset]\n"
	        "If it doesn't work you might have to tell DOSBox the label of the CD-ROM:\n"
	        "[color=light-blue]mount D ~/example -t cdrom -label CDLABEL[reset]\n"
	        "\n"
	        "Additionally, you can use [color=light-blue]imgmount[reset] to mount ISO or CUE/BIN images:\n"
	        "[color=light-blue]imgmount D ~/cd.iso -t cdrom[reset]\n"
	        "[color=light-blue]imgmount D ~/cd.cue -t cdrom[reset]\n");
	MSG_Add("PROGRAM_INTRO_SPECIAL",
	        "[erases=entire][color=light-green]Special keys:[reset]\n"
	        "These are the default keybindings.\n"
	        "They can be changed in the [color=brown]keymapper[reset].\n"
	        "\n"
	        "[color=yellow]%s+Enter[reset]  Switch between fullscreen and window mode.\n"
	        "[color=yellow]%s+Pause[reset]  Pause/Unpause emulator.\n"
	        "[color=yellow]%s+F1[reset]   %s Start the [color=brown]keymapper[reset].\n"
	        "[color=yellow]%s+F4[reset]   %s Swap mounted disk image, scan for changes on all drives.\n"
	        "[color=yellow]%s+F5[reset]     Save a screenshot of the rendered image.\n"
	        "[color=yellow]%s+F5[reset]   %s Save a screenshot of the DOS pre-rendered image.\n"
	        "[color=yellow]%s+F6[reset]   %s Start/Stop recording sound output to a wave file.\n"
	        "[color=yellow]%s+F7[reset]   %s Start/Stop recording video output to a zmbv file.\n"
	        "[color=yellow]%s+F8[reset]   %s Mute/Unmute the audio.\n"
	        "[color=yellow]%s+F9[reset]   %s Shutdown emulator.\n"
	        "[color=yellow]%s+F10[reset]  %s Capture/Release the mouse.\n"
	        "[color=yellow]%s+F11[reset]  %s Slow down emulation.\n"
	        "[color=yellow]%s+F12[reset]  %s Speed up emulation.\n"
	        "[color=yellow]%s+F12[reset]    Unlock speed (turbo button/fast forward).\n");
}
