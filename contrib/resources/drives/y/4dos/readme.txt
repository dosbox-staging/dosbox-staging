				4DOS v8.00

			README.TXT -- February 2009


Greetings, and thanks for using 4DOS!

This file contains a variety of information you should read before using
4DOS, including:

      Installation Notes
      Files Included with this Version
      Technical Support and Downloads
      Legal Notices
      Build History


INSTALLATION NOTES
------------------

   There are just two steps you need to take to install a downloaded copy
   of 4DOS:

      >> Extract all the files to a new directory (do not use a directory
	 you're using for other products, or for a previous version of
	 4DOS).

      >> Start 4DOS.  It will "install itself".

   Additional details on these steps are below.  For manual installation
   instructions see the sections with those headings below.

   When installing 4DOS under Windows 95/98 we recommend that you do NOT
   use a "long" directory name.  If you do, you will have to know and use
   the equivalent short name to load 4DOS in CONFIG.SYS; it's easier to
   simply use a short name from the beginning.	(This is not a 4DOS
   limitation.	It's because DOS, which starts before Windows 95/98 and
   loads the primary command processor, cannot handle long file names.
   While 4DOS fully supports long file names, to make DOS work well it is
   best to install 4DOS in a directory with a short name.)

   You can view any of the documentation files on-line, or copy them to
   your printer.  The Introduction and Installation Guide in INTRO.TXT has
   some page breaks; all other documentation files are unpaginated ASCII
   text.


"Self-Installation"

   To begin the installation process, simply run the program file,
   4DOS.COM, from a prompt.  This will automatically start the
   self-installation file _4INST.BTM, which will finish installing 4DOS on
   your system.

   _4INST will prompt you for permission before each installation step,
   and no system files will be modified without asking you first.

   When the installation is finished, _4INST renames itself to _4INST.BTX,
   so that it will not be run again.  (If installation terminates
   abnormally, _4INST may not be renamed, and will then run again the next
   time you start the program.)


Manual Installation

   For complete manual installation instructions, see the Introduction and
   Installation Guide in the file INTRO.TXT.  The detailed instructions
   for 4DOS are in Chapter 5.

   If you choose to perform a manual installation you should rename the
   automated installation file, _4INST.BTM (we recommend renaming it to
   _4INST.BTX, which is what the file itself does when it's finished). If
   you don't, it will start automatically the first time you run 4DOS.


FILES INCLUDED
--------------

   The following files are included with 4DOS 8.00:

	 4DOS.COM	   4DOS program file
	 4HELP.EXE	   4DOS help program
	 4DOS.HLP	   4DOS online help text
	 _4INST.BTM	   Self-installation batch file
	 4DOS.ICO	   Icon file for 4DOS under Windows
	 BATCOMP.EXE	   Batch file compression utility
	 EXAMPLES.BTM	   Sample batch file demonstrating the use of
			      variable functions and internal variables
	 HELPCFG.EXE	   Color configuration program for the help
			      system
	 INTRO.TXT	   ASCII copy of the 4DOS Introduction and
			      Installation Guide
	 INSTHELP.EXE	   Installation helper used when installing 4DOS
			      under DOS and Windows 95/98
	 JP4DOS.INF	   Setup file to install 4DOS registry extensions
			      under Windows 95/98
	 JPOS2INS.CMD	   Installation helper used when installing 4DOS
			      under OS/2
	 JP4DOSSC.INF	   Setup file to install JP Software program
			      group and shortcuts under Windows 95/98
	 KSTACK.COM	   Memory-resident program used by the
			      KEYSTACK command
	 LICENSE.TXT	   License agreement
	 OPTION.EXE	   Program used by the OPTION command
	 README.TXT	   This file


TECHNICAL SUPPORT AND DOWNLOADS
-------------------------------

   This version of 4DOS is free and unsupported.

   To download JP Software files, including trial versions of the Windows
   products (4NT and Take Command), visit:

      >> JP Software web site at http://jpsoft.com
      >> JP Software FTP site at  ftp://jpsoft.com


LEGAL STUFF
-----------

Copyright © 1988-2004  Rex Conn & JP Software Inc., Worton, MD, USA
4DOS is a registered trademark of JP Software Inc.  JP Software, jpsoft.com
and all JP Software designs and logos are also trademarks of JP Software Inc.
Other product and company names are trademarks of their respective owners.


BUILD HISTORY SINCE VERSION 7.50 BUILD 132 IN REVERSE ORDER
-----------------------------------------------------------

Builds newer than 132 are developed independently of JP Software by Luchezar
Georgiev in Bulgaria. JP Software is not responsible for any bugs that these
builds may introduce. The user must assume all risks of using them.

Build 200 (Version 8.00) - 27 February 2009:
	Fixed my v7.51 bug: swapping to hard disk would crash
	SET <the switch character><any character>=<value string> now works
	REN /N now returns an error if the target filename already exists
	Added SETERROR command
	Added ATTRIB /N option
	Added @ISLOWER and @ISUPPER functions
	Added _LASTDIR, _VERMAJOR, _VERMINOR and _VERSION variables
	Updated help file for the above changes and updated "Keywords" page

Build 199 (Version 7.99) - 29 December 2008:
	No _CPUSPEED crash on early 586 if RDTSC not emulated by EMM386
	_CPUSPEED now returns a much more consistent value under W9x/ME
	_CPUSPEED now works even if the CPU has no TSC (i.e. 8086-486), and
	_CPU and _NDP now return much more detailed info if no brand string
	  thanks to the CPU/FPU detection library by Vladimir M. Zakharychev
	Updated help file for the above changes

Build 198 (Version 7.98) - 19 December 2008:
	Fixed my 7.92 bug: if the Int 67h vector was 0, _VCPI would crash
	Mouse functions would no longer crash on old PCs if no mouse driver
	Added @COM function and _SBDSP internal variable
	_VIDEO now returns "svga" if a VESA SuperVGA BIOS is present
	Updated help file for the above changes and updated "Keywords" page

Build 197 (Version 7.97) - 12 December 2008:
	_DRIVES and _LASTDISK now return correct results in PTS-DOS
	4DOS now honours the NO_SEP environment variable
	Added _VDS internal variable
	Updated help file for the above changes and updated "Keywords" page

Build 196 (Version 7.96) - 1 December 2008:
	Added REBOOT /M(onitor off) and /S(uspend) options
	Added @CODEPAGE function and _POWER variable
	Updated help file for the above changes and updated "Keywords" page

Build 195 (Version 7.95) - 24 November 2008:
	Now LIST /X /T"xy zt..." searches for the hex sequence "xy zt..."
	Added @CLUSTSIZE and @HDDSIZE functions
	Added _APPEND,_ASSIGN,_DISPLAY,_DRIVER,_EGA,_GRAFTABL,_GRAPHICS,
	  _MSCDEX,_PRINT,_SMARTDRV,_TASKMAX and _TASKSWITCHER variables
	Updated help file for the above changes and updated "Keywords" page
	Slightly edited _4INST.BTM and INTRO.TXT

Build 194 (Version 7.94) - 18 November 2008:
	Changed box shadow foreground from low intensity white to dark grey
	Added @DDCSTR function
	Added _MACHINE, _NETWORK, _NLSFUNC and _SHARE variables
	Updated help file for the above changes and updated "Keywords" page

Build 193 (Version 7.93) - 7 November 2008:
	Revised and somewhat shortened the INTRO.TXT file (also by Klaus)
	_4INST.BTM heavily modified; now searched for in the 4DOS directory
	Jaelani fixed KSTACK 7.9 conditional load (allocation strategy) bug
	Fixed all warnings but 1, muted OW source warnings, quietened build
	Updated BATCOMP to version 7.5 which has /E(ncryption) switch added
	Added _FONTPAGE internal variable
	Updated help file for the above changes and updated "Keywords" page

Build 192 (Version 7.92) - 31 October 2008:
	The installer batch file cleaned up and greatly simplified by Klaus
	ALIAS, FUNCTION and SET now accept wildcards in display mode
	Added /U(ninstall) option to KSTACK (plus optimisation) by Jaelani
	Added @FSTYPE function and _KEYSTACKED and _VCPI variables
	Updated help file for the above changes, incl. Glossary and Keywords

Build 191 (Version 7.91) - 27 October 2008:
	Fixed my v7.86 bug: cursor was lost after running LIST in batch file
	Added COUNTRY command
	Updated help file for the above change and updated "Keywords" page

Build 190 (Version 7.90) - 20 October 2008:
	Fixed my v7.89 bug: Shift-TAB didn't act as F8 on auto-completion
	Fixed my v7.89 bug: LIST search highlighted text past right margin
	_READY now works under DR-DOS for CD-/DVD-drives
	HELPCFG can now be built from source thanks to Jaelani's EDISCRN.INC
	Added IDLE command (DR-DOS-only)
	Updated help file for the above change and updated "Keywords" page

Build 189 (Version 7.89) - 16 October 2008:
	Fixed my bug where if no TZ was set, time zone info was garbage
	TYPE without /L, /P or redirection can now show Mac OS 9 text files
	LIST hex search result now highlighted across byte 8 & 9 of the row
	LIST hex search in hex mode highlights no text results & vice-versa

Build 188 (Version 7.88) - 12 October 2008:
	Added hex input format for SETDOS /C, /E and /P, @CHAR and
	  @FILEWRITEB (others accept it too but it's unusual for them)
	Added =X hex output format with a leading 0x for @EVAL
	Added TRANSIENT command
	Updated help file for the above changes and updated "Keywords" page

Build 187 (Version 7.87) - 10 October 2008:
	Fixed my bug causing DELAY /M and _CPUSPEED sporadic hang-up
	_WINTICKS now works on 8086-286 too
	TIMER accuracy now 10 ms on 8086-286 too (but still not under OS/2)
	TITLEPROMPT now works under OS/2 too
	Added TITLE command
	Updated help file for the above changes and updated "Keywords" page

Build 186 (Version 7.86) - 6 October 2008:
	Hid LIST cursor
	Added _DST, _MJD, _STZN, _STZO, _TZN, _TZO, _UNIXTIME, _UTCDATE,
	  _UTCDATETIME,_UTCHOUR,_UTCISODATE,_UTCMINUTE,_UTCSECOND,_UTCTIME
	Updated help file for the above change and updated "Keywords" page

Build 185 (Version 7.85) - 26 September 2008:
	All date options, arguments and date input can be ISO ordinal date
	_WINTITLE now works also under W9x/ME
	Added _STARTPATH internal variable
	Updated help file for the above changes and updated "Keywords" page

Build 184 (Version 7.84) - 24 September 2008:
	Added stack size monitoring to batch debugger window title
	Added ISO ordinal date format 6 to @AGEDATE, @DATECONV,
	  @FILEDATE and @MAKEDATE
	Added _ISORDATE and _WINTICKS internal variables
	Added @DIRSTACK and @SIMILAR functions
	Updated help file for the above changes and corrected DIRS direction

Build 183 (Version 7.83) - 22 September 2008:
	Avoid diskette swap prompt for _READY if B: is a "phantom floppy"
	Added @COMPARE, @LCS and @REVERSE functions
	Updated help file for the above change

Build 182 (Version 7.82) - 19 September 2008:
	Fixed my v7.81 @DATE bug: wrongly used ISO week year to sum days
	SETDOS /C, /E and /P now accept ASCII code as numeric argument
	Added PROMPT $A to show ampersand and $K to show ISO week date
	Added _HDRIVES internal variable
	Added @CEILING, @DRIVETYPE and @FLOOR functions
	Updated help file for the above changes

Build 181 (Version 7.81) - 17 September 2008:
	Prevented buffer overflow if @INSERT strings too long
	Made stricter and united all day-of-year/days-since-1980 code
	DATE/TIME prompt date/time separators made country-specific
	Added _CDROMS, _DRIVES, _ISOWYEAR and _READY variables
	Added @DATECONV,@HISTORY,@ISOWYEAR,@SUBST,@UNQUOTES functions
	Updated help file for the above changes

Build 180 (Version 7.80) - 14 September 2008:
	INKEY /W no longer loads the CPU in W9x/ME and DesqView
	Added _ININAME and _TICK variables
	Added @COUNT,@ISALNUM,@ISALPHA,@ISASCII,@ISCNTRL,@ISDIGIT,
	  @ISPRINT,@ISPUNCT,@ISSPACE and @ISXDIGIT functions
	Updated help file for the above changes

Build 179 (Version 7.79) - 11 September 2008:
	Last LIST search text is now default for new search in same mode
	Added _BATCHTYPE, _BDEBUGGER, _CMDSPEC and _V86 variables
	Added @QUOTE and @UNQUOTE functions
	Updated help file for the above changes

Build 178 (Version 7.78) - 8 September 2008:
	Fixed my year increment/decrement bug on ISO week date output
	The keystroke terminating DELAY /B is no longer shown
	Non-repeated seed value period increased from 1 month to 1 year
	Added _TSC and _CPUSPEED variables
	Added TIMER /Q(uiet) option
	TIMER accuracy improved from 55 to 10 ms but on 8086-286 or OS/2
	Accuracy of DELAY /M improved from 55 to 1 ms (32 ms under OS/2)
	Updated and amended help file for DELAY, TIMER, _TSC, _CPUSPEED

Build 177 (Version 7.77) - 31 August 2008:
	All date options, arguments and date input can now be ISO week date
	Added ISO week date format 5 to @AGEDATE, @FILEDATE, @MAKEDATE
	Added _ISOWDATE internal variable
	Updated help file for the above changes

Build 176 (Version 7.76) - 29 August 2008:
	Added MOD as an equivalent to the %% operator of @EVAL
	Added @ISODOWI,@ISOWEEK functions and _ISODOWI,_ISOWEEK variables
	Updated help file for the above changes

Build 175 (Version 7.75) - 24 August 2008:
	GOSUB variables containing the switch character are now accepted
	Prompt is now on the first, not the second row after screen cleared
	Added SET /E option (set local environment too if /M also specified)
	Added @AVERAGE function and _SYSREQ variable
	Updated help file for the above changes

Build 174 (Version 7.74) - 12 August 2008:
	History log no longer contains the line "AUTOEXEC"
	Added @FILEREADB function
	If length = -1, @FILEWRITEB input data is series of ASCII values
	Updated help file for the above changes

Build 173 (Version 7.73) - 4 August 2008:
	Correct extended memory size even if > 64M in MEMORY and @EXTENDED
	(NOTE: It's size of all extended memory regardless of XMS managers.)
	Added @SMBSTR function
	Updated help file for the above changes

_4INST Change on 23 July 2008
	Allowed installation in 4DOS 7.65+ where _DOS is not always "DOS".

4HELP Change on 16 July 2008
	Maximum number of cross-references per topic set to 512 (was 128).

Build 172 (Version 7.72) - 15 July 2008:
	SETDOS /W now works under MS-DOS/PC DOS 5.0+ and W9x/ME
	Replaced the @RANDOM LCG with Xorshift7 (period = 2^256-1)
	Added @LTRIM, @RTRIM and @TRUNCATE functions
	Updated help file for the above changes

Build 171 (Version 7.71) - 10 July 2008:
	Fixed a bug in LIST search results highlighting, obvious in hex mode
	Enabled SETDOS /X[+|-]9 to enable / disable user-defined functions
	Added _EDITMODE and _EXPANSION variables
	Updated help file for the above changes

Build 170 (Version 7.70) - 9 July 2008:
	Fixed a bug in LIST where bottom line repeats on 1-line scroll down
	  if a higher line ended at the right margin (DOS text files only)
	Fixed a LIST header percentage display overflow bug for big files
	Holding Shift with F or Ctrl-F in LIST now matches search case
	Added a /N (line numbers) option to LIST
	Updated help file for the above changes

Build 169 (Version 7.69) - 4 July 2008:
	Fixed the title of the reverse find (Ctrl-F) window in LIST
	Added @SHA1 function and _STDIN, _STDOUT and _STDERR variables
	Updated help file for the above change

Build 168 (Version 7.68) - 1 July 2008:
	_CPU now set to processor brand string if supported by the CPU
	REBOOT /P now works in W9x/ME even if PM APM interface engaged
	Update help file for the above changes, added a W9x/ME warning

Build 167 (Version 7.67) - 29 June 2008:
	TYPE without /L, /P or redirection now shows UNIX text files right
	REBOOT now works under OS/2, Windows 9x/ME and QEMM Stealth ROM
	Added _EXECSTR internal variable
	Updated help file for the above changes

Build 166 (Version 7.66) - 23 June 2008:
	Warm REBOOT now honours EMM's Int 19h "hook" or jumps to F000:E05B
	SHIFT switch character no longer hard-coded to '/'
	_DOS now available also with 4DOS /C

Build 165 (Version 7.65) - 11 June 2008:
	The Shell sort of directories replaced with the much faster heapsort
	Internal variable _DOS now reflects actual running operating system
	Added _LALT, _LCTRL, _RALT, _RCTRL internal variables
	Updated help file for the above changes

4HELP Changes on 31 May 2008
	In VESA text video-modes with over 80 columns, BP7 no longer resets
	the video-mode on start or blanks the screen on exit (invoked by F1).

Build 164 (Version 7.64) - 26 May 2008:
	If the string has a leading separator, the word or field index is
	   negative and its absolute value is equal to the number of words
	   (fields), @WORD and @FIELD no longer return the whole string
	@WORD[S], @FIELD[S] no longer ignore leading space(s) in string
	TOUCH /R or /T now set file seconds properly without halving them
	LIST now shows also seconds and 4-digit year in its file info box
	Added EQC (case-sensitive) comparison operator
	Updated help file for the above change

Build 163 (Version 7.63) - 3 May 2008:
	If EditMode is not Init*, cursor shape is now reset on hitting Enter
	"Warm" REBOOT in DOS now also pulses RESET pin on ATs (still "warm")
	If InstallPath is not set, OPTION can now invoke 4HELP

Build 162 (Version 7.62) - 23 April 2008:
	The prompt after a CLS now goes to the first, not the second line
	As in COMMAND.COM, /K now suppresses the signon messages like /C
	(NOTE: As IO.SYS 7.x appends "/D /K AUTOEXEC" or "/K NETSTART" to
	the SHELL= line, add a colon after the "K" to "mute" the signon.)

Build 161 (Version 7.61) - 28 January 2008:
	Correctly show the minor version number of OS/2 Warp 4.x
	Correctly show X-DOS version and avoid NLS separator corruption in it
	Recognise Wendin-DOS (but unfortunately 4DOS crashes in it)

Build 160 (Version 7.60) - 28 December 2007:
	Auto-completion and SELECT now support filenames containing backquotes
	HEAD/TAIL /V option header now always starts on new line if /C used
	Return value of @FILEWRITE now equals the number of bytes written

Build 159 (Version 7.59) - 27 October 2007:
	Wildcard matches include all LFNs containing bracket characters ([])
	Added @MD5 function for files using the RSA algorithm from RFC 1321
	Updated help file for the above change

Build 158 (Version 7.58) - 15 September 2007:
	Direct screen output now works in monochrome video modes too
	Amended sign-on message with information about 4DOS patches
	Re-enabled the MOVE /W(ipe) option

Build 157 (Version 7.57) - 27 July 2007:
	Auto-completion of file names with many dots works in DOSLFN as in W9x
	Creation / access times can now be TOUCHed in DOSLFN, not only in W9x
	SELECT /X now works in DOSLFN too, not only in W9x
	Fixed possible pointer underflow if @WORD and @FIELD count backwards
	@WORD and @FIELD no longer return whole string if it has leading space
	Added @FIELDS function
	Updated help file for the above change

Build 156 (Version 7.56) - 25 June 2007:
	Show zero creation or access times in DOSLFN the same way as in W9x
	Added _DATETIME and _MONTHF variables
	Added @AGEDATE and @MONTHF functions
	Updated help file for the above changes

Build 155 (Version 7.55) - 14 May 2007:
	Show correct CD/DVD-ROM disk space in raw DOS; fixes DIR in PC DOS 7.1
	Avoid false detection of MS-DOS 7 (MSDOS7 variable) in case of FreeDOS
	DIR /2 /X in Windows now looks like in DOS and doesn't reach column 80
	VER /R now displays also build date

Build 154 (Version 7.54) - 20 April 2007:
	Added message.* to global header dependency
	Added high-level @EMS function code (was missing in original sources)
	Added _ALT,_CAPSLOCK,_CTRL,_LSHIFT,_NUMLOCK,_RSHIFT,_SCROLLLOCK,_SHIFT
	Updated help file for the above change

Build 153 (Version 7.53) - 15 February 2007:
	Recognise LZ-DOS, RXDOS, DOS-ROM and S/DOS
	The "Marked" message of SELECT no longer shifts even on largest files
	8.3 file size over 1 GB no longer misaligns DIR, TREE or SELECT output
	Updated help for the above (maximal non-wrap description size now 39)

Build 152 (Version 7.52) - 4 February 2007:
	Correctly process the "invalid disk change" code during critical error
	Irrelevant error codes in critical error converted to "general failure"
	Added a /L(ine offset) option to LIST
	Updated help file for the above change

Build 151 (Version 7.51) - 31 January 2007:
	For Novell DOS and DR-DOS 7.x-8.0, show the right DOS version
	For hard errors, show operation and drive; accept only allowed actions
	DOS error messages amended and edited to clarify and remove duplicates
	Updated help file for the above change

Build 150 - 25 January 2007:
	Fixed the "Lock violation on COPY from remote drive without SHARE" bug
	Worked around a bug (in MSVCRT?!) leaving high byte of _doserrno != 0
	VER_MINOR = VER_BUILD - 100 from now on (version 7.51 = build 151)

Build 149 - 18 January 2007:
	Fixed the "DIR [PATH] finds no files in DOS LFN volumes" bug
	An * instead of *.* default wildcard is now used for DOS LFN volumes
	FFIND /U (summary only) option can now be evaluated by @EXECSTR
	FFIND and LIST search can now be interrupted with Ctrl-C

4HELP Changes on 15 January 2007
	4HELP now supports mouse wheel (As for 4DOS, a driver that supports
	wheel is needed, e.g. Cute Mouse 2.x; the W9x driver does NOT support
	wheel for the DOS mouse API.)

Build 148 - 10 January 2007:
	LIST, SELECT and command history window now support mouse wheel
	(NOTE: A driver that supports wheel is needed, e.g. Cute Mouse 2.x+)
	The right mouse button now exits LIST and SELECT

Build 147 - 8 January 2007:
	Fixed command line buffer overflow when repeatedly pressing F12
	Added hexadecimal output option to @EVAL
	Updated help file for the above change

Build 146 - 7 January 2007:
	Fixed command line buffer overflow and path search name underflow bug
	Added new @CWD and @CWDS functions
	Updated help file for the above change

Build 145 - 6 January 2007:
	@READY now equals the inverted "no disk in drive" bit for CD-ROMs
	Added new EJECTMEDIA and CLOSETRAY commands
	Updated help file for the above change

Build 144 - 30 December 2006:
	Added a /V(erbose) option to TYPE to show a header for each file
	Added a /P(ower off) option to REBOOT to shut the system down
	REBOOT now flushes SMARTDRV cache before reboot or power off
	Updated help file for the above changes

Build 143 - 29 December 2006:
	/P page prompts are now overwritten like DR-/PTS-DOS COMMAND.COM
	Restored DOS version 2.x compatibility of 4DOS.COM, lost in v6
	4DOS.COM now aborts on attempt to run it in DOS version 1.x

Build 142 - 26 December 2006:
	Restored 8086/8088 (PC/XT, Pravetz-16, etc.) compatibility, lost in v6
	Added compatibility information about CADStar PCB in the help file
	Added @SERIAL function; help file updated for it too

Build 141 - 24 December 2006:
	Added GB units to file size ranges, @DISK* and @FILESIZE functions
	DIR /4 now shows size of files over 1 GB with precision of 0.1 GB
	Added information about the above features in the help file

Build 140 - 22 December 2006:
	For Pentium 4 and up, _CPU now returns 786
	Removed the @READY warning from 4DOS.HLP and added 786 info to it
	Added build.h dependency for expand, removed shareware dependencies

Build 139 - 21 December 2006:
	Recognise Datalight ROM-DOS
	Can now load in upper memory in PTS-DOS
	@READY now works properly in PTS-DOS

Build 138 - 19 December 2006:
	@READY now works more reliably with CD-ROM drives
	Removed unused modules "batcomp" and "parspath" from 4DOS.LNK
	INTVER = VER_BUILD again, now OPTION.EXE patched on each build

Build 137 - 17 December 2006:
	Re-enabled _4INST.BTM (self-installer) support
	Added the installer files from version 6.02 (1999) to the archive
	4DOS.HLP replaced by extended and updated version by Charles Dye

Build 136 - 15 December 2006:
	Process the DR-DOS F5/F8 startup keystrokes too
	Fixed the "%* affected by the SHIFT command" bug
	Removed the unused (non-4DOS and shareware) error messages

Build 135 - 13 December 2006:
	No longer mistake DR-DOS 7.0x for Novell DOS
	Recognise PC DOS, FreeDOS, PTS-DOS and DR-DOS 7.03
	Fixed the "DIR /F and DIR /B /S disable colourisation" bug

Build 134 - 9 December 2006:
	Batch debugger now refuses to load batch files over the 64 KB size limit
	Fixed the "total @FILESIZE roll-over at 4 GB" bug
	Fixed the "DESCRIBE loses third-party info" bug

Build 133 - 7 December 2006:
	Properly show FAT32 drive space in MS/PC/LZ/EDR/PTS/ROM/FreeDOS
	Properly show free XMS memory in the MEMORY command if > 64 MB
	Re-enabled the /Y single-stepping option that was disabled in 2003
	Fixed the "SET /M not upper-casing master environment variables" bug
	Fixed the "RD not removing hidden and system sub-directories" bug
	Fixed the C4018 "signed/unsigned mismatch" C compiler warnings
	Reverted INTVER from 131 to 130 to make OPTION compatible
