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

#if C_DEBUG
	#include "program_biostest.h"
#endif

extern Bit32u floppytype;

#define WIKI_URL                   "https://github.com/dosbox-staging/dosbox-staging/wiki"
#define WIKI_ADD_UTILITIES_ARTICLE WIKI_URL "/Add-Utilities"

void DOS_SetupPrograms(void)
{
	/*Add Messages */

	MSG_Add("PROGRAM_MOUNT_CDROMS_FOUND","CDROMs found: %d\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_DRIVE", "Drive");
	MSG_Add("PROGRAM_MOUNT_STATUS_TYPE", "Type");
	MSG_Add("PROGRAM_MOUNT_STATUS_LABEL", "Label");
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

	MSG_Add("PROGRAM_MEM_CONVEN", "%10d kB free conventional memory\n");
	MSG_Add("PROGRAM_MEM_EXTEND", "%10d kB free extended memory\n");
	MSG_Add("PROGRAM_MEM_EXPAND", "%10d kB free expanded memory\n");
	MSG_Add("PROGRAM_MEM_UPPER",
	        "%10d kB free upper memory in %d blocks (largest UMB %d kB)\n");

	MSG_Add("PROGRAM_LOADFIX_ALLOC", "%d kB allocated.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOC", "%d kB freed.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOCALL","Used memory freed.\n");
	MSG_Add("PROGRAM_LOADFIX_ERROR","Memory allocation error.\n");

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

	MSG_Add("PROGRAM_RESCAN_SUCCESS","Drive cache cleared.\n");

	MSG_Add("PROGRAM_INTRO",
	        "\033[2J\033[32;1mWelcome to DOSBox Staging\033[0m, an x86 emulator with sound and graphics.\n"
	        "DOSBox creates a shell for you which looks like old plain DOS.\n"
	        "\n"
	        "For information about basic mount type \033[34;1mintro mount\033[0m\n"
	        "For information about CD-ROM support type \033[34;1mintro cdrom\033[0m\n"
	        "For information about special keys type \033[34;1mintro special\033[0m\n"
	        "For more imformation, visit DOSBox Staging wiki:\033[34;1m\n" WIKI_URL
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
	        "\033[2J\033[32;1mHow to mount a virtual CD-ROM Drive in DOSBox:\033[0m\n"
	        "DOSBox provides CD-ROM emulation on several levels.\n"
	        "\n"
	        "This works on all normal directories, installs MSCDEX and marks the files\n"
	        "read-only. Usually this is enough for most games:\n"
	        "\n"
	        "\033[34;1mmount D C:\\example -t cdrom\033[0m\n"
	        "\n"
	        "If it doesn't work you might have to tell DOSBox the label of the CD-ROM:\n"
	        "\n"
	        "\033[34;1mmount D C:\\example -t cdrom -label CDLABEL\033[0m\n"
	        "\n"
	        "Additionally, you can use imgmount to mount iso or cue/bin images:\n"
	        "\n"
	        "\033[34;1mimgmount D C:\\cd.iso -t cdrom\033[0m\n"
	        "\n"
	        "\033[34;1mimgmount D C:\\cd.cue -t cdrom\033[0m\n");
	MSG_Add("PROGRAM_INTRO_CDROM_OTHER",
	        "\033[2J\033[32;1mHow to mount a virtual CD-ROM Drive in DOSBox:\033[0m\n"
	        "DOSBox provides CD-ROM emulation on several levels.\n"
	        "\n"
	        "This works on all normal directories, installs MSCDEX and marks the files\n"
	        "read-only. Usually this is enough for most games:\n"
	        "\n"
	        "\033[34;1mmount D ~/example -t cdrom\033[0m\n"
	        "\n"
	        "If it doesn't work you might have to tell DOSBox the label of the CD-ROM:\n"
	        "\n"
	        "\033[34;1mmount D ~/example -t cdrom -label CDLABEL\033[0m\n"
	        "\n"
	        "Additionally, you can use imgmount to mount iso or cue/bin images:\n"
	        "\n"
	        "\033[34;1mimgmount D ~/cd.iso -t cdrom\033[0m\n"
	        "\n"
	        "\033[34;1mimgmount D ~/cd.cue -t cdrom\033[0m\n");
	MSG_Add("PROGRAM_INTRO_SPECIAL",
	        "\033[2J\033[32;1mSpecial keys:\033[0m\n"
	        "These are the default keybindings.\n"
	        "They can be changed in the \033[33mkeymapper\033[0m.\n"
	        "\n"
	        "\033[33;1m%s+Enter\033[0m  Switch between fullscreen and window mode.\n"
	        "\033[33;1m%s+Pause\033[0m  Pause/Unpause emulator.\n"
	        "\033[33;1m%s+F1\033[0m   %s Start the \033[33mkeymapper\033[0m.\n"
	        "\033[33;1m%s+F4\033[0m   %s Swap mounted disk image, update directory cache for all drives.\n"
	        "\033[33;1m%s+F5\033[0m   %s Save a screenshot.\n"
	        "\033[33;1m%s+F6\033[0m   %s Start/Stop recording sound output to a wave file.\n"
	        "\033[33;1m%s+F7\033[0m   %s Start/Stop recording video output to a zmbv file.\n"
	        "\033[33;1m%s+F9\033[0m   %s Shutdown emulator.\n"
	        "\033[33;1m%s+F10\033[0m  %s Capture/Release the mouse.\n"
	        "\033[33;1m%s+F11\033[0m  %s Slow down emulation.\n"
	        "\033[33;1m%s+F12\033[0m  %s Speed up emulation.\n"
	        "\033[33;1m%s+F12\033[0m    Unlock speed (turbo button/fast forward).\n");

	MSG_Add("PROGRAM_BOOT_NOT_EXIST","Bootdisk file does not exist.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_NOT_OPEN","Cannot open bootdisk file.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_WRITE_PROTECTED","Image file is read-only! Might create problems.\n");
	MSG_Add("PROGRAM_BOOT_PRINT_ERROR",
	        "This command boots DOSBox from either a floppy or hard disk image.\n\n"
	        "For this command, one can specify a succession of floppy disks swappable\n"
	        "by pressing %s+F4, and -l specifies the mounted drive to boot from.  If\n"
	        "no drive letter is specified, this defaults to booting from the A drive.\n"
	        "The only bootable drive letters are A, C, and D.  For booting from a hard\n"
	        "drive (C or D), the image should have already been mounted using the\n"
	        "\033[34;1mIMGMOUNT\033[0m command.\n\n"
	        "The syntax of this command is:\n\n"
	        "\033[34;1mBOOT [diskimg1.img diskimg2.img] [-l driveletter]\033[0m\n");
	MSG_Add("PROGRAM_BOOT_UNABLE","Unable to boot off of drive %c");
	MSG_Add("PROGRAM_BOOT_IMAGE_OPEN","Opening image file: %s\n");
	MSG_Add("PROGRAM_BOOT_IMAGE_NOT_OPEN","Cannot open %s");
	MSG_Add("PROGRAM_BOOT_BOOT","Booting from drive %c...\n");
	MSG_Add("PROGRAM_BOOT_CART_WO_PCJR","PCjr cartridge found, but machine is not PCjr");
	MSG_Add("PROGRAM_BOOT_CART_LIST_CMDS", "Available PCjr cartridge commands: %s");
	MSG_Add("PROGRAM_BOOT_CART_NO_CMDS", "No PCjr cartridge commands found");

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
	        "  \033[32;1mimgmount\033[0m \033[37;1mDRIVE\033[0m \033[36;1mCDROM-SET\033[0m [CDROM-SET2 [..]] [-fs iso] -t cdrom|iso\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mDRIVE\033[0m \033[36;1mIMAGEFILE\033[0m [IMAGEFILE2 [..]] [-fs fat] -t hdd|floppy\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mDRIVE\033[0m \033[36;1mBOOTIMAGE\033[0m [-fs fat|none] -t hdd -size GEOMETRY\n"
	        "  \033[32;1mimgmount\033[0m -u \033[37;1mDRIVE\033[0m  (unmounts the DRIVE's image)\n"
	        "\n"
	        "Where:\n"
	        "  \033[37;1mDRIVE\033[0m     is the drive letter where the image will be mounted: a, c, d, ...\n"
	        "  \033[36;1mCDROM-SET\033[0m is an ISO, CUE+BIN, CUE+ISO, or CUE+ISO+FLAC/OPUS/OGG/MP3/WAV\n"
	        "  \033[36;1mIMAGEFILE\033[0m is a hard drive or floppy image in FAT16 or FAT12 format\n"
	        "  \033[36;1mBOOTIMAGE\033[0m is a bootable disk image with specified -size GEOMETRY:\n"
	        "            bytes-per-sector,sectors-per-head,heads,cylinders\n"
	        "Notes:\n"
	        "  - %s+F4 swaps & mounts the next CDROM-SET or IMAGEFILE, if provided.\n"
	        "\n"
	        "Examples:\n"
#if defined(WIN32)
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1mC:\\games\\doom.iso\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1mcd/quake1.cue\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mA\033[0m \033[36;1mfloppy1.img floppy2.img floppy3.img\033[0m -t floppy\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1mC:\\dos\\c_drive.img\033[0m -t hdd\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1mbootable.img\033[0m -t hdd -fs none -size 512,63,32,1023\n"
#elif defined(MACOSX)
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1m/Users/USERNAME/games/doom.iso\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1mcd/quake1.cue\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mA\033[0m \033[36;1mfloppy1.img floppy2.img floppy3.img\033[0m -t floppy\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1m~/dos/c_drive.img\033[0m -t hdd\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1mbootable.img\033[0m -t hdd -fs none -size 512,63,32,1023\n"
#else
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1m/home/USERNAME/games/doom.iso\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1mcd/quake1.cue\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mA\033[0m \033[36;1mfloppy1.img floppy2.img floppy3.img\033[0m -t floppy\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1m~/dos/c_drive.img\033[0m -t hdd\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1mbootable.img\033[0m -t hdd -fs none -size 512,63,32,1023\n"
#endif
	);

	MSG_Add("SHELL_CMD_MOUNT_HELP",
	        "maps physical folders or drives to a virtual drive letter.\n");

	MSG_Add("SHELL_CMD_MOUNT_HELP_LONG",
	        "Mount a directory from the host OS to a drive letter.\n"
	        "\n"
	        "Usage:\n"
	        "  \033[32;1mmount\033[0m \033[37;1mDRIVE\033[0m \033[36;1mDIRECTORY\033[0m [-t TYPE] [-freesize SIZE] [-label LABEL]\n"
	        "  \033[32;1mmount\033[0m -u \033[37;1mDRIVE\033[0m  (unmounts the DRIVE's directory)\n"
	        "\n"
	        "Where:\n"
	        "  \033[37;1mDRIVE\033[0m     the drive letter where the directory will be mounted: A, C, D, ...\n"
	        "  \033[36;1mDIRECTORY\033[0m is the directory on the host OS to be mounted\n"
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
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1mC:\\dosgames\033[0m\n"
	        "  \033[32;1mmount\033[0m \033[37;1mD\033[0m \033[36;1mD:\\\033[0m -t cdrom\n"
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1mmy_savegame_files\033[0m -t overlay\n"
#elif defined(MACOSX)
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1m~/dosgames\033[0m\n"
	        "  \033[32;1mmount\033[0m \033[37;1mD\033[0m \033[36;1m\"/Volumes/Game CD\"\033[0m -t cdrom\n"
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1mmy_savegame_files\033[0m -t overlay\n"
#else
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1m~/dosgames\033[0m\n"
	        "  \033[32;1mmount\033[0m \033[37;1mD\033[0m \033[36;1m\"/media/USERNAME/Game CD\"\033[0m -t cdrom\n"
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1mmy_savegame_files\033[0m -t overlay\n"
#endif
	);

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
	MSG_Add("PROGRAM_KEYB_SHOWHELP",
		"\033[32;1mKEYB\033[0m [keyboard layout ID[ codepage number[ codepage file]]]\n\n"
		"Some examples:\n"
		"  \033[32;1mKEYB\033[0m: Display currently loaded codepage.\n"
		"  \033[32;1mKEYB\033[0m sp: Load the spanish (SP) layout, use an appropriate codepage.\n"
		"  \033[32;1mKEYB\033[0m sp 850: Load the spanish (SP) layout, use codepage 850.\n"
		"  \033[32;1mKEYB\033[0m sp 850 mycp.cpi: Same as above, but use file mycp.cpi.\n");
	MSG_Add("PROGRAM_KEYB_NOERROR","Keyboard layout %s loaded for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_FILENOTFOUND","Keyboard file %s not found\n\n");
	MSG_Add("PROGRAM_KEYB_INVALIDFILE","Keyboard file %s invalid\n");
	MSG_Add("PROGRAM_KEYB_LAYOUTNOTFOUND","No layout in %s for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_INVCPFILE","None or invalid codepage file for layout %s\n\n");

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

	PROGRAMS_MakeFile("ATTRIB.COM", PLACEHOLDER_ProgramStart);
	PROGRAMS_MakeFile("AUTOTYPE.COM", AUTOTYPE_ProgramStart);
#if C_DEBUG
	PROGRAMS_MakeFile("BIOSTEST.COM", BIOSTEST_ProgramStart);
#endif
	PROGRAMS_MakeFile("BOOT.COM", BOOT_ProgramStart);
	PROGRAMS_MakeFile("CHOICE.COM", CHOICE_ProgramStart);
#if !(C_DEBUG)
	PROGRAMS_MakeFile("DEBUG.COM", PLACEHOLDER_ProgramStart);
#endif
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
}
