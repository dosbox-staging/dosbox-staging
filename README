DOSBox v0.72


=====
NOTE: 
=====

While we are hoping that one day DOSBox will run all programs ever
made for the PC, we are not there yet. At present, DOSBox running
on a high-end machine will roughly be the equivalent of a 486 PC.
DOSBox can be configured to run a wide range of DOS games, from
CGA/Tandy/PCjr classics up to games from the Quake era.



======
INDEX:
======
1. Quickstart
2. FAQ
3. Usage
4. Internal Programs
5. Special Keys
6. Mapper
7. Keyboard Layout
8. Serial Multiplayer feature
9. How to run resource-demanding games
10. Troubleshooting
11. The config file
12. The language file
13. Building your own version of DOSBox
14. Special thanks
15. Contact



==============
1. Quickstart:
==============

Type INTRO in DOSBox for a quick tour.
It is essential that you get familiar with the idea of mounting,
DOSBox does not automatically make any drive (or parts of it)
accessible to the emulation.
See the FAQ entry "I've got a Z instead of a C at the prompt" as
well as the description of the MOUNT command (section 4).



=======
2. FAQ:
=======

Some Frequently Asked Questions:

Q: I've got a Z instead of a C at the prompt.
Q: Do I always have to type these commands? Automation?
Q: How do I change to fullscreen?
Q: My CD-ROM doesn't work.
Q: The mouse doesn't work.
Q: There is no sound.
Q: The sound stutters or sounds stretched/weird.
Q: I can't type \ or : in DOSBox.
Q: The keyboard lags.
Q: The cursor always moves into one direction!
Q: The game/application can't find its CD-ROM.
Q: The game/application runs much too slow!
Q: Can DOSBox harm my computer?
Q: I would like to change the memory size/cpu speed/ems/soundblaster IRQ.
Q: What sound hardware does DOSBox presently emulate?
Q: DOSBox crashes on startup and I'm running arts.
Q: Great README, but I still don't get it.




Q: I've got a Z instead of a C at the prompt.
A: You have to make your directories available as drives in DOSBox by using
   the "mount" command. For example, in Windows "mount C D:\GAMES" will give
   you a C drive in DOSBox which points to your Windows D:\GAMES directory.
   In Linux, "mount c /home/username" will give you a C drive in DOSBox
   which points to /home/username in Linux.
   To change to the drive mounted like above, type "C:". If everything went
   fine, DOSBox will display the prompt "C:\>".


Q: Do I always have to type these commands? Automation?
A: In the DOSBox configuration file is an [autoexec] section. The commands
   present there are run when DOSBox starts, so you can use this section
   for the mounting.


Q: How do I change to fullscreen?
A: Press alt-enter. Alternatively: Edit the configuration file of DOSBox and 
   change the option fullscreen=false to fullscreen=true. If fullscreen looks 
   wrong in your opinion: Play with the option fullresolution in the 
   configuration file of DOSBox. To get back from fullscreen mode:
   Press alt-enter again.


Q: My CD-ROM doesn't work.
A: To mount your CD-ROM in DOSBox you have to specify some additional options 
   when mounting the CD-ROM. 
   To enable CD-ROM support (includes MSCDEX):
     - mount d f:\ -t cdrom
   To enable low-level CD-ROM-support (uses ioctl if possible):
     - mount d f:\ -t cdrom -usecd 0
   To enable low-level SDL-support:
     - mount d f:\ -t cdrom -usecd 0 -noioctl
   To enable low-level aspi-support (win98 with aspi-layer installed):
     - mount d f:\ -t cdrom -usecd 0 -aspi
   
   In the commands: - d   driveletter you will get in DOSBox
                    - f:\ location of CD-ROM on your PC.
                    - 0   The number of the CD-ROM drive, reported by mount -cd
   See also the question: The game/application can't find its CD-ROM.


Q: The mouse doesn't work.
A: Usually, DOSBox detects when a game uses mouse control. When you click on 
   the screen it should get locked (confined to the DOSBox window) and work. 
   With certain games, the DOSBox mouse detection doesn't work. In that case 
   you will have to lock the mouse manually by pressing CTRL-F10.


Q: There is no sound.
A: Be sure that the sound is correctly configured in the game. This might be
   done during the installation or with a setup/setsound utility that 
   accompanies the game. First see if an autodetection option is provided. If 
   there is none try selecting soundblaster or soundblaster16 with the default
   settings being "address=220 irq=7 dma=1". You might also want to select 
   midi at address 330 as music device.
   The parameters of the emulated soundcards can be changed in the DOSBox
   configuration file.
   If you still don't get any sound set the core to normal and use some lower
   fixed cycles value (like cycles=2000). Also assure that your host operating
   sound does provide sound.


Q: The sound stutters or sounds stretched/weird.
A: You're using too much CPU power to keep DOSBox running at the current speed.
   You can lower the cycles, skip frames, reduce the sampling rate of
   the respective sound device (see the DOSBox configuration file) or
   the mixer device. You can also increase the prebuffer in the configfile.
   If you are using cycles=max or =auto, then make sure that there no
   background processes interfering! (especially if they access the harddisk)


Q: I can't type \ or : in DOSBox.
A: This is a known problem. It only occurs if your keyboard layout isn't US.
   Some possible fixes:
     1. Switch the keyboard layout of your operating system.
     2. Use / instead.
     3. Open dosbox.conf and change usescancodes=false to usescancodes=true.
     4. Add the commands you want to execute to the "configfile".
     5. Change the DOS keyboard layout (see Section 7 Keyboard Layout).
     6. Use ALT-58 for : and ALT-92 for \.
     7. for \ try the keys around "enter". For ":" try shift and the keys 
        between "enter" and "l" (US keyboard layout).
     8. Try keyb.com from FreeDOS (http://projects.freedos.net/keyb/).
        Look for keyb2.0 pre4 as older and newer versions are known to
        have a bug in the loader routines.


Q: The keyboard lags.
A: Lower the priority setting in the DOSBox configuration file
   like set "priority=normal,normal". You might also want to
   try lowering the cycles.


Q: The cursor always moves into one direction!
A: See if it still happens if you disable the joystick emulation,
   set joysticktype=none in the [joystick] section of your DOSBox
   configuration file. Maybe also try unplugging any joystick.
   If you want to use the joystick in the game, try setting timed=false
   and be sure to calibrate the joystick (both in your OS as well as
   in the game or the game's setup).
   

Q: The game/application can't find its CD-ROM.
A: Be sure to mount the CD-ROM with -t cdrom switch, this will enable the
   MSCDEX interface required by DOS games to interface with CD-ROMs.
   Also try adding the correct label (-label LABEL). To enable lower-level
   CD-ROM support, add the following switch to mount: -usecd #, where # is
   the number of your CD-ROM drive reported by mount -cd. Under Windows you
   can specify -ioctl, -aspi or -noioctl. Look at the description elsewhere
   in this document for their meaning.
   Try creating a CD-ROM image (preferably CUE/BIN pair) and use the
   DOSBox-internal IMGMOUNT tool to mount the image. This enables very
   good low-level CD-ROM support on any operating system.


Q: The game/application runs much too slow!
A: Look at the section "How to run resource-demanding games" for more 
   information.


Q: Can DOSBox harm my computer?
A: DOSBox can not harm your computer more than any other resource demanding
   program. Increasing the cycles does not overclock your real CPU.
   Setting the cycles too high has a negative performance effect on the
   software running inside DOSBox.


Q: I would like to change the memory size/cpu speed/ems/soundblaster IRQ.
A: This is possible! Just create a config file: config -writeconf configfile.
   Start your favourite editor and look through the settings. To start DOSBox
   with your new settings: dosbox -conf configfile


Q: What sound hardware does DOSBox presently emulate?
A: DOSBox emulates several legacy sound devices:
   - Internal PC speaker
     This emulation includes both the tone generator and several forms of
     digital sound output through the internal speaker.
   - Creative CMS/Gameblaster
     The is the first card released by Creative Labs(R).  The default 
     configuration places it on port 0x220.  It should be noted that enabling 
     this with the Adlib emulation may result in conflicts.
   - Tandy 3 voice
     The emulation of this sound hardware is complete with the exception of 
     the noise channel. The noise channel is not very well documented and as 
     such is only a best guess as to the sound's accuracy.
   - Tandy DAC
     Emulation of the Tandy DAC utilizes the soundblaster emulation, thus
     be sure the soundblaster is not disabled in the DOSBox configuration
     file. The Tandy DAC is only emulated at the BIOS level.
   - Adlib
     Borrowed from MAME, this emulation is almost perfect and includes the 
     Adlib's ability to almost play digitized sound.
   - SoundBlaster 16/ SoundBlaster Pro I & II /SoundBlaster I & II
     By default DOSBox provides Soundblaster 16 level 16-bit stereo sound. 
     You can select a different SoundBlaster version in the configfile of 
     DOSBox (See Internal Commands: CONFIG).
   - Disney Soundsource
     Using the printer port, this sound device outputs digital sound only.
   - Gravis Ultrasound
     The emulation of this hardware is nearly complete, though the MIDI 
     capabilities have been left out, since an MPU-401 has been 
     emulated in other code.
   - MPU-401
     A MIDI passthrough interface is also emulated.  This method of sound 
     output will only work when used with a General Midi or MT-32 device.


Q: DOSBox crashes on startup and I'm running arts.
A: This isn't really a DOSBox problem, but the solution is to set the 
   environment variable SDL_AUDIODRIVER to alsa or oss.


Q: Great README, but I still don't get it.
A: A look at "The Newbie's pictorial guide to DOSBox" located at 
   http://vogons.zetafleet.com/viewforum.php?f=39 might help you.
   Also try the wiki of DOSBox:
   http://dosbox.sourceforge.net/wiki/


For more questions read the remainder of this README and/or check 
the site/forum:
http://dosbox.sourceforge.net



=========
3. Usage:
=========

An overview of the command line options you can give to DOSBox.
Windows Users must open cmd.exe or command.com or edit the shortcut to 
DOSBox.exe for this.
The options are valid for all operating systems unless noted in the option
description:

dosbox [name] [-exit] [-c command] [-fullscreen] [-conf congfigfile] 
       [-lang languagefile] [-machine machinetype] [-noconsole]
       [-startmapper] [-noautoexec] [-scaler scaler | -forcescaler scaler]
       
dosbox -version

  name   
        If "name" is a directory it will mount that as the C: drive.
        If "name" is an executable it will mount the directory of "name" 
        as the C: drive and execute "name".
    
  -exit  
        DOSBox will close itself when the DOS application "name" ends.

  -c command
        Runs the specified command before running "name". Multiple commands
        can be specified. Each command should start with "-c", though.
        A command can be: an Internal Program, a DOS command or an executable 
        on a mounted drive.

  -fullscreen
        Starts DOSBox in fullscreen mode.

  -conf configfile
        Start DOSBox with the options specified in "configfile".
        Multiple -conf options may be present.
        See Chapter 10 for more details.

  -lang languagefile
        Start DOSBox using the language specified in "languagefile".

  -machine machinetype
        Setup DOSBox to emulate a specific type of machine. Valid choices are:
        hercules, cga, pcjr, tandy, vga (default). The machinetype affects 
        both the videocard and the available soundcards.

  -noconsole (Windows Only)
        Start DOSBox without showing the console window. Output will
        be redirected to stdout.txt and stderr.txt
	
  -startmapper
        Enter the keymapper directly on startup. Useful for people with 
        keyboard problems.

  -noautoexec
        Skips the [autoexec] section of the loaded configuration file.

  -scaler scaler
        Uses the scaler specified by "scaler". See the DOSBox configuration
        file for the available scalers.

  -forcescaler scaler
        Similar to the -scaler parameter, but tries to force usage of
        the specified scaler even if it might not fit.

  -version
        output version information and exit. Useful for frontends.

Note: If a name/command/configfile/languagefile contains a space, put
      the whole name/command/configfile/languagefile between quotes
      ("command or file name"). If you need to use quotes within quotes
      (most likely with -c and mount).
      Windows and OS/2 users can use single quotes inside the double quotes. 
      Other people should be able to use escaped double quotes inside the 
      double quotes.
      win -c "mount c 'c:\program files\'" 
      linux -c "mount c \"/tmp/name with space\""

For example:

dosbox c:\atlantis\atlantis.exe -c "MOUNT D C:\SAVES"
  This mounts c:\atlantis as c:\ and runs atlantis.exe.
  Before it does that it would first mount C:\SAVES as the D drive.

In Windows, you can also drag directories/files onto the DOSBox executable.



=====================
4. Internal Programs:
=====================

DOSBox supports most of the DOS commands found in command.com.
To get a list of the internal commands type "HELP" at the prompt.

In addition, the following commands are available: 

MOUNT "Emulated Drive letter" "Real Drive or Directory" 
      [-t type] [-aspi] [-ioctl] [-noioctl] [-usecd number] [-size drivesize] 
      [-label drivelabel] [-freesize size_in_mb]
      [-freesize size_in_kb (floppies)]
MOUNT -cd
MOUNT -u "Emulated Drive letter"

  Program to mount local directories as drives inside DOSBox.

  "Emulated Drive letter"
        The driveletter inside DOSBox (eg. C).

  "Real Drive letter (usually for CD-ROMs in Windows) or Directory"
        The local directory you want accessible inside DOSBox.

  -t type
        Type of the mounted directory. Supported are: dir (default),
        floppy, cdrom.

  -size drivesize
        Sets the size of the drive, where drivesize is of the form
        "bps,spc,tcl,fcl":
           bps: bytes per sector, by default 512 for regular drives and
                2048 for CD-ROM drives
           spc: sectors per cluster, usually between 1 and 127
           tcl: total clusters, between 1 and 65534
           fcl: total free clusters, between 1 and tcl

  -freesize size_in_mb | size_in_kb
        Sets the amount of free space available on a drive in megabytes
        (regular drives) or kilobytes (floppy drives).
        This is a simpler version of -size.	

  -label drivelabel
        Sets the name of the drive to "drivelabel". Needed on some 
        systems if the cd label isn't read correctly. Useful when a 
        program can't find its CD-ROM. If you don't specify a label and no
        lowlevel support is selected (that is omitting the -usecd # and/or
        -aspi parameters or specifying -noioctl): 
          For win32: label is extracted from "Real Drive".
          For Linux: label is set to NO_LABEL.

        If you do specify a label, this label will be kept as long as the drive
        is mounted. It will not be updated !!

  -aspi
        Forces use of the aspi layer. Only valid if mounting a CD-ROM under 
        Windows systems with an ASPI-Layer.

  -ioctl   
        Forces use of ioctl commands. Only valid if mounting a CD-ROM under 
        a Windows OS which support them (Win2000/XP/NT).

  -noioctl   
        Forces use of the SDL CD-ROM layer. Valid on all systems.

  -usecd number
        Forces use of SDL CD-ROM support for drive number.
        Number can be found by -cd. Valid on all systems.

  -cd
        Displays all detected CD-ROM drives and their numbers. Use with -usecd.

  -u
        Removes the mount. Doesn't work for Z:\.

  Note: It's possible to mount a local directory as CD-ROM drive. 
        Hardware support is then missing.

  Basically MOUNT allows you to connect real hardware to DOSBox's emulated PC.
  So MOUNT C C:\GAMES tells DOSBox to use your C:\GAMES directory as drive C:
  in DOSBox. It also allows you to change the drive's letter identification
  for programs that demand specific drive letters.
  
  For example: Touche: Adventures of The Fifth Musketeer must be run on your C:
  drive. Using DOSBox and its mount command, you can trick the game into
  believing it is on the C drive, while you can still place it where you
  like. For example, if the game is in D:\OLDGAMES\TOUCHE, the command
  MOUNT C D:\OLDGAMES will allow you to run Touche from the D drive.

  Mounting your entire C drive with MOUNT C C:\ is NOT recommended! The same
  is true for mounting the root of any other drive, except for CD-ROMs (due to
  their read-only nature). Otherwise if you or DOSBox make a mistake you may
  loose all your files.
  It is recommended to put all your applications/games into a subdirectory
  and mount that.

  General MOUNT Examples:
  1. To mount c:\DirX as a floppy : 
       mount a c:\DirX -t floppy
  2. To mount system CD-ROM drive E as CD-ROM drive D in DOSBox:
       mount d e:\ -t cdrom
  3. To mount system CD-ROM drive at mountpoint /media/cdrom as CD-ROM drive D 
     in DOSBox:
       mount d /media/cdrom -t cdrom -usecd 0
  4. To mount a drive with ~870 mb free diskspace (simple version):
       mount c d:\ -freesize 870
  5. To mount a drive with ~870 mb free diskspace (experts only, full control):
       mount c d:\ -size 512,127,16513,13500
  6. To mount /home/user/dirY as drive C in DOSBox:
       mount c /home/user/dirY
  7. To mount the directory where DOSBox was started as D in DOSBox:
       mount d .


MEM
  Program to display the amount of free memory.


CONFIG -writeconf localfile
CONFIG -writelang localfile
CONFIG -set "section property=value"
CONFIG -get "section property"

  CONFIG can be used to change or query various settings of DOSBox 
  during runtime. It can save the current settings and language strings to
  disk. Information about all possible sections and properties can 
  be found in section 11 (The Config File).

  -writeconf localfile
       Write the current configuration settings to file. "localfile" is 
       located on the local drive, not a mounted drive in DOSBox. 
       The configuration file controls various settings of DOSBox: 
       the amount of emulated memory, the emulated soundcards and many more 
       things. It allows access to AUTOEXEC.BAT as well.
       See section 11 (The Config File) for more information.

  -writelang localfile
       Write the current language settings to file. "localfile" is 
       located on the local drive, not a mounted drive in DOSBox.
       The language file controls all visible output of the internal commands
       and the internal DOS.

  -set "section property=value"
       CONFIG will attempt to set the property to new value. At this moment
       CONFIG can not report whether the command succeeded or not.

  -get "section property"
       The current value of the property is reported and stored in the 
       environment variable %CONFIG%. This can be used to store the value 
       when using batch files.

  Both "-set" and "-get" work from batch files and can be used to set up your
  own preferences for each game.
  
  Examples:
  1. To create a configfile in your current directory:
      config -writeconf dosbox.conf
  2. To set the cpu cycles to 10000:
      config -set "cpu cycles=10000"
  3. To turn ems memory emulation off:
      config -set "dos ems=off"
  4. To check which cpu core is being used.
      config -get "cpu core"


LOADFIX [-size] [program] [program-parameters]
LOADFIX -f
  Program to reduce the amount of memory available. Useful for old programs 
  which don't expect much memory to be free. 

  -size	        
        number of kilobytes to "eat up", default = 64kb
  
  -f
        frees all previously allocated memory
  
Examples:
  1. To start mm2.exe and allocate 64kb memory 
     (mm2 will have 64 kb less available) :
     loadfix mm2
  2. To start mm2.exe and allocate 32kb memory :
     loadfix -32 mm2
  3. To free previous allocated memory :
     loadfix -f


RESCAN
  Make DOSBox reread the directory structure. Useful if you changed something
  on a mounted drive outside of DOSBox. (CTRL - F4 does this as well!)
  

MIXER
  Makes DOSBox display its current volume settings. 
  Here's how you can change them:
  
  mixer channel left:right [/NOSHOW] [/LISTMIDI]
  
  channel
      Can be one of the following: MASTER, DISNEY, SPKR, GUS, SB, FM.
  
  left:right
      The volume levels in percentages. If you put a D in front it will be
      in decibel (example mixer gus d-10).
  
  /NOSHOW
      Prevents DOSBox from showing the result if you set one
      of the volume levels.

  /LISTMIDI
      Lists the available midi devices on your PC (Windows). To select a 
      device other than the Windows default midi-mapper, add a line 
      'config=id' to the [midi] section in the configuration file, where
      'id' is the number for the device as listed by LISTMIDI.


IMGMOUNT
  A utility to mount disk images and CD-ROM images in DOSBox.
  
  IMGMOUNT DRIVE [imagefile] -t [image_type] -fs [image_format] 
            -size [sectorsbytesize, sectorsperhead, heads, cylinders]

  imagefile
      Location of the image files to mount in DOSBox. The location can
      be on a mounted drive inside DOSBox, or on your real disk. It is
      possible to mount CD-ROM images (ISOs or CUE/BIN) as well, if you
      need CD swapping capabilities specify all images in succession.
      The CDs can be swapped with CTRL-F4 at any time.
   
  -t 
      The following are valid image types:
        floppy: Specifies a floppy image or images.  DOSBox will automatically 
                identify the disk geometry ( 360K, 1.2MB, 720K, 1.44MB, etc).
        iso:    Specifies a CD-ROM iso image.  The geometry is automatic and 
                set for this size. This can be an iso or a cue/bin.
        hdd:    Specifies a harddrive image. The proper CHS geometry 
                must be set for this to work.

  -fs 
      The following are valid file system formats:
        iso:  Specifies the ISO 9660 CD-ROM format.
        fat:  Specifies that the image uses the FAT file system. DOSBox will attempt
              to mount this image as a drive in DOSBox and make the files 
              available from inside DOSBox.
        none: DOSBox will make no attempt to read the file system on the disk.
              This is useful if you need to format it or if you want to boot 
              the disk using the BOOT command.  When using the "none" 
              filesystem, you must specify the drive number (2 or 3, 
              where 2 = master, 3 = slave) rather than a drive letter.  
              For example, to mount a 70MB image as the slave drive device, 
              you would type:
                "imgmount 3 d:\test.img -size 512,63,16,142 -fs none" 
                (without the quotes)  Compare this with a mount to read the 
                drive in DOSBox, which would read as: 
                "imgmount e: d:\test.img -size 512,63,16,142"

  -size 
     The Cylinders, Heads and Sectors specification of the drive.
     Required to mount hard drive images.
     
  An example how to mount CD-ROM images:
    1a. mount c /tmp
    1b. imgmount d c:\myiso.iso -t iso
  or (which also works):
    2. imgmount d /tmp/myiso.iso -t iso


BOOT
  Boot will start floppy images or hard disk images independent of the 
  operating system emulation offered by DOSBox. This will allow you to
  play booter floppies or boot other operating systems inside DOSBox.
  If the target emulated system is PCjr (machine=pcjr) the boot command
  can be used to load PCjr cartridges (.jrc). 

  BOOT [diskimg1.img diskimg2.img .. diskimgN.img] [-l driveletter]
  BOOT [cart.jrc]  (PCjr only)

  diskimgN.img 
     This can be any number of floppy disk images one wants mounted after 
     DOSBox boots the specified drive letter.
     To swap between images, hit CTRL-F4 to change from the current disk 
     to the next disk in the list. The list will loop back from the last 
     disk image to the beginning.

  [-l driveletter]
     This parameter allows you to specify the drive to boot from.  
     The default is the A drive, the floppy drive.  You can also boot  
     a hard drive image mounted as master by specifying "-l C" 
     without the quotes, or the drive as slave by specifying "-l D"
     
   cart.jrc (PCjr only)
     When emulation of a PCjr is enabled, cartridges can be loaded with
     the BOOT command. Support is still limited.


IPX

  You need to enable IPX networking in the configuration file of DOSBox.

  All of the IPX networking is managed through the internal DOSBox program 
  IPXNET. For help on the IPX networking from inside DOSBox, type 
  "IPXNET HELP" (without quotes) and the program will list the commands 
  and relevant documentation. 

  With regard to actually setting up a network, one system needs to be 
  the server. To set this up, type "IPXNET STARTSERVER" (without the quotes)
  in a DOSBox session. The server DOSBox session will 
  automatically add itself to the virtual IPX network. For every 
  additional computer that should be part of the virtual IPX network, 
  you'll need to type "IPXNET CONNECT <computer host name or IP>". 
  For example, if your server is at bob.dosbox.com, 
  you would type "IPXNET CONNECT bob.dosbox.com" on every non-server system. 
  
  To play games that need Netbios a file named NETBIOS.EXE from Novell is 
  needed. Establish the IPX connection as explained above, then run 
  "netbios.exe". 

  The following is an IPXNET command reference: 

  IPXNET CONNECT 

     IPXNET CONNECT opens a connection to an IPX tunnelling server 
     running on another DOSBox session. The "address" parameter specifies 
     the IP address or host name of the server computer. You can also 
     specify the UDP port to use. By default IPXNET uses port 213 - the 
     assigned IANA port for IPX tunnelling - for its connection. 

     The syntax for IPXNET CONNECT is: 
     IPXNET CONNECT address <port> 

  IPXNET DISCONNECT 

     IPXNET DISCONNECT closes the connection to the IPX tunnelling server. 

     The syntax for IPXNET DISCONNECT is: 
     IPXNET DISCONNECT 

  IPXNET STARTSERVER 

     IPXNET STARTSERVER starts an IPX tunnelling server on this DOSBox 
     session. By default, the server will accept connections on UDP port 
     213, though this can be changed. Once the server is started, DOSBox 
     will automatically start a client connection to the IPX tunnelling server.

     The syntax for IPXNET STARTSERVER is:
     IPXNET STARTSERVER <port>

     If the server is behind a router, UDP port <port> needs to be forwarded
     to that computer.

     On Linux/Unix-based systems port numbers smaller than 1023 can only be
     used with root privileges. Use ports greater than 1023 on those systems.

  IPXNET STOPSERVER

     IPXNET STOPSERVER stops the IPX tunnelling server running on this DOSBox
     session. Care should be taken to ensure that all other connections have 
     terminated as well, since stopping the server may cause lockups on other 
     machines that are still using the IPX tunnelling server. 

     The syntax for IPXNET STOPSERVER is: 
     IPXNET STOPSERVER 

  IPXNET PING

     IPXNET PING broadcasts a ping request through the IPX tunnelled network. 
     In response, all other connected computers will respond to the ping 
     and report the time it took to receive and send the ping message. 

     The syntax for IPXNET PING is: 
     IPXNET PING

  IPXNET STATUS

     IPXNET STATUS reports the current state of this DOSBox session's 
     IPX tunnelling network. For a list of all computers connected to the 
     network use the IPXNET PING command. 

     The syntax for IPXNET STATUS is: 
     IPXNET STATUS 


KEYB [languagecode [codepage [codepagefile]]]
  Change the keyboard layout. For detailed information about keyboard
  layouts please see Section 7.

  [languagecode] is a string consisting of two (in special cases more)
     characters, examples are GK (Greece) or IT (Italy). It specifies
     the keyboard layout to be used.

  [codepage] is the number of the codepage to be used. The keyboard layout
     has to provide support for the specified codepage, otherwise the layout
     loading will fail.
     If no codepage is specified, an appropriate codepage for the requested
     layout is chosen automatically.

  [codepagefile] can be used to load codepages that are yet not compiled
     into DOSBox. This is only needed when DOSBox does not find the codepage.


  Examples:
  1) To load the german keyboard layout (automatically uses codepage 858):
       keyb gr
  2) To load the russian keyboard layout with codepage 866:
       keyb ru 866
     In order to type russian characters press ALT+RIGHT-SHIFT.
  3) To load the french keyboard layout with codepage 850 (where the
     codepage is defined in EGACPI.DAT):
       keyb fr 850 EGACPI.DAT
  4) To load codepage 858 (without a keyboard layout):
       keyb none 858
     This can be used to change the codepage for the freedos keyb2 utility.



For more information use the /? command line switch with the programs.



================
5. Special Keys:
================

ALT-ENTER     Switch to full screen and back.
ALT-PAUSE     Pause emulation.
CTRL-F1       Start the keymapper.
CTRL-F4       Change between mounted disk-images. Update directory cache for all drives!
CTRL-ALT-F5   Start/Stop creating a movie of the screen. (avi video capturing)
CTRL-F5       Save a screenshot. (png)
CTRL-F6       Start/Stop recording sound output to a wave file.
CTRL-ALT-F7   Start/Stop recording of OPL commands.
CTRL-ALT-F8   Start/Stop the recording of raw MIDI commands.
CTRL-F7       Decrease frameskip.
CTRL-F8       Increase frameskip.
CTRL-F9       Kill DOSBox.
CTRL-F10      Capture/Release the mouse.
CTRL-F11      Slow down emulation (Decrease DOSBox Cycles).
CTRL-F12      Speed up emulation (Increase DOSBox Cycles).
ALT-F12       Unlock speed (turbo button).

These are the default keybindings. They can be changed in the keymapper.

Saved/recorded files can be found in current_directory/capture 
(can be changed in the configfile). 
The directory has to exist prior to starting DOSBox, otherwise nothing 
gets saved/recorded !


NOTE: Once you increase your DOSBox cycles beyond your computer's maximum
capacity, it will produce the same effect as slowing down the emulation.
This maximum will vary from computer to computer.



==========
6. Mapper:
==========

When you start the DOSBox mapper (either with CTRL-F1 or -startmapper as
a command line argument to the DOSBox executable) you are presented with 
a virtual keyboard and a virtual joystick.

These virtual devices correspond to the keys DOSBox will report to the
DOS applications. If you click on a key with your mouse, you can see in
the lower left corner with which event it is associated (EVENT) and to
what events it is currently bound.

Event: EVENT
BIND: BIND
                        Add   Del
mod1  hold                    Next
mod2
mod3


EVENT
    The key or joystick axis/button/hat DOSBox will report to DOS applications.
BIND
    The key on your real keyboard or the axis/button/hat on your real
    joystick(s) (as reported by SDL) which is connected to the EVENT.
mod1,2,3 
    Modfiers. These are keys you need to have to be pressed while pressing
    BIND. mod1 = CTRL and mod2 = ALT. These are generally only used when you
    want to change the special keys of DOSBox.
Add 
    Add a new BIND to this EVENT. Basically add a key from your keyboard or an
    event from the joystick (button press, axis/hat movement) which will 
    produce the EVENT in DOSBox.
Del 
    Delete the BIND to this EVENT. If an EVENT has no BINDS, then it is not
    possible to trigger this event in DOSBox (that is there's no way to type
    the key or use the respective action of the joystick).
Next
    Go through the list of bindings which map to this EVENT.


Example:
Q1. You want to have the X on your keyboard to type a Z in DOSBox.
    A. Click on the Z on the keyboard mapper. Click "Add". 
       Now press the X key on your keyboard. 

Q2. If you click "Next" a couple of times, you will notice that the Z on your 
    keyboard also produces an Z in DOSBox.
    A. Therefore select the Z again, and click "Next" until you have the Z on 
       your keyboard. Now click "Del".

Q3. If you try it out in DOSBox, you will notice that pressing X makes ZX
    appear.
     A. The X on your keyboard is still mapped to the X as well! Click on
        the X in the keyboard mapper and search with "Next" until you find the 
        mapped key X. Click "Del".


Examples about remapping the joystick:
  You have a joystick attached, it is working fine under DOSBox and you
  want to play some keyboard-only game with the joystick (it is assumed
  that the game is controlled by the arrows on the keyboard):
    1) Start the mapper, then click on one of the arrows in the middle
       of the left part of the screen (right above the Mod1/Mod2 buttons).
       EVENT should be key_left. Now click on Add and move your joystick
       in the respective direction, this should add an event to the BIND.
    2) Repeat the above for the missing three directions, additionally
       the buttons of the joystick can be remapped as well (fire/jump).
    3) Click on Save, then on Exit and test it with some game.

  You want to swap the y-axis of the joystick because some flightsim uses
  the up/down joystick movement in a way you don't like, and it is not
  configurable in the game itself:
    1) Start the mapper and click on Y- in the upper joystick field (this
       is for the first joystick if you have two joysticks attached) or the
       lower joystick field (second joystick or, if you have only one
       joystick attached, the second axes cross).
       EVENT should be jaxis_0_1- (or jaxis_1_1-).
    2) Click on Del to remove the current binding, then click Add and move
       your joystick downwards. A new bind should be created.
    3) Repeat this for Y+, save the layout and finally test it with some game.



If you change the default mapping, you can save your changes by clicking on
"Save". DOSBox will save the mapping to a location specified in the configfile
(mapperfile=mapper.txt). At startup, DOSBox will load your mapperfile, if it
is present in the configfile.



===================
7. Keyboard Layout:
===================

To switch to a different keyboard layout, either the entry "keyboardlayout"
in the [dos] section in dosbox.conf can be used, or the internal DOSBox
program keyb.com. Both accept DOS conforming language codes (see below), but
only by using keyb.com a custom codepage can be specified.

Layout switching
  DOSBox supports a number of keyboard layouts and codepages by default,
  in this case just the layout identifier needs to be specified (like
  keyboardlayout=sv in the DOSBox config file, or using "keyb sv" at
  the DOSBox command prompt).
  
  Some keyboard layouts (for example layout GK codepage 869 and layout RU
  codepage 808) have support for dual layouts that can be activated by
  pressing LEFT-ALT+RIGHT-SHIFT and deactivated by LEFT-ALT+LEFT-SHIFT.

Supported external files
  The freedos .kl files are supported (freedos keyb2 keyboard layoutfiles) as
  well as the freedos keyboard.sys/keybrd2.sys/keybrd3.sys libraries which
  consist of all available .kl files.
  See http://projects.freedos.net/keyb/ for precompiled keyboard layouts if
  the DOSBox-integrated layouts don't work for some reason, or updated or
  new layouts become available.

  Both .CPI (MSDOS/compatible codepage files) and .CPX (freedos UPX-compressed
  codepage files) can be used. Some codepages are compiled into DOSBox, so it
  is mostly not needed to care about external codepage files. If you need
  a different (or custom) codepage file, copy it into the directory of the
  DOSBox configuration file so it is accessible for DOSBox.

  Additional layouts can be added by copying the corresponding .kl file into
  the directory of dosbox.conf and using the first part of the filename as
  language code.
  Example: For the file UZ.KL (keyboard layout for Uzbekistan) specify
           "keyboardlayout=uz" in dosbox.conf.
  The integration of keyboard layout packages (like keybrd2.sys) works similar.


Note that the keyboard layout allows foreign characters to be entered, but
there is NO support for them in filenames. Try to avoid them both inside
DOSBox as well as in files on your host operating system that are accessible
by DOSBox.



==============================
8. Serial Multiplayer feature:
==============================
 
DOSBox can emulate a serial nullmodem cable over network and internet.
It can be configured through the [serialports] section in the DOSBox
configuration file.

To create a nullmodem connection, one side needs to act as the server and
one as the client.

The server needs to be set up in the DOSBox configuration file like this:
   serial1=nullmodem

The client:
   serial1=nullmodem server:<IP or name of the server>

Now start your game and choose nullmodem / serial cable / already connected
as multiplayer method on COM1. Set the same baudrate on both computers.

Furthermore, additional parameters can be specified to control the behavior
of the nullmodem connection. These are all parameters:

 * port:         - TCP port number. Default: 23
 * rxdelay:      - how long (milliseconds) to delay received data if the
                   interface is not ready. Increase this value if you encounter
                   overrun errors in the DOSBox Status Window. Default: 100
 * txdelay:      - how long to gather data before sending a packet. Default: 12
                   (reduces Network overhead)
 * server:       - This nullmodem will be a client connecting to the specified
                   server. (No server argument: be a server.)
 * transparent:1 - Only send the serial data, no RTS/DTR handshake. Use this
                   when connecting to anything other than a nullmodem.
 * telnet:1      - Interpret Telnet data from the remote site. Automatically
                   sets transparent.
 * usedtr:1      - The connection will not be established until DTR is switched
                   on by the DOS program. Useful for modem terminals.
                   Automatically sets transparent.
 * inhsocket:1   - Use a socket passed to DOSBox by command line. Automatically
                   sets transparent. (Socket Inheritance: It is used for
                   playing old DOS door games on new BBS software.)

Example: Be a server listening on TCP port 5000.
   serial1=nullmodem server:<IP or name of the server> port:5000 rxdelay:1000



=======================================
9. How to run resource-demanding games: 
=======================================

DOSBox emulates the CPU, the sound and graphic cards, and other peripherals
of a PC, all at the same time. The speed of an emulated DOS application
depends on how many instructions can be emulated, which is adjustable
(number of cycles).

CPU Cycles
  By default (cycles=auto) DOSBox tries to detect whether a game needs to
  be run with as many instructions emulated per time interval as possible.
  You can force this behaviour by setting cycles=max in the DOSBox
  configuration file. The DOSBox window will display a line "Cpu Cyles: max"
  at the top then. In this mode you can reduce the amount of cycles on a
  percentage-basis (hit CTRL-F11) or raise it again (CTRL-F12).
  
  Sometimes manually setting the number of cycles achieves better results,
  in the DOSBox configuration file specify for example cycles=30000. When
  running some DOS application you can raise the cycles with CTRL-F12 even
  more, but you will be limited by the power of your actual CPU. You can see
  how much free time your true CPU has by looking at the Task Manager in
  Windows 2000/XP and the System Monitor in Windows 95/98/ME. Once 100% of
  your real CPU time is used there is no further way to speed up DOSBox
  unless you reduce the load generated by the non-CPU parts of DOSBox. 

CPU Cores
  On x86 architectures you can try to force the usage of a dynamically
  recompiling core (set core=dynamic in the DOSBox configuration file).
  This usually gives better results if the auto detection (core=auto) fails.
  It is best accompanied by cycles=max. Note that there might be games
  that work worse with the dynamic core, or do not work at all!

Graphics emulation
  VGA emulation is a very demanding part of DOSBox in terms of actual CPU
  usage. Increase the number of frames skipped (in increments of one) by
  pressing CTRL-F8. Your CPU usage should decrease when using a fixed
  cycle setting.
  Go back one step and repeat this until the game runs fast enough for you.
  Please note that this is a trade-off: you lose in fluidity of video what
  you gain in speed.

Sound emulation
  You can also try to disable the sound through the setup utility of the game
  to reduce load on your CPU further. Setting nosound=true does NOT disable
  the emulation of sound devices, just the sound output will be disabled.

Also try to close every program but DOSBox to reserve as much resources
as possible for DOSBox.


Advanced cycles configuration:
The cycles=auto and cycles=max settings can be parameterized to have
different startup defaults. The syntax is
  cycles=auto ["realmode default"] ["protected mode default"%] 
              [limit "cycle limit"]
  cycles=max ["protected mode default"%] [limit "cycle limit"]
Example:
  cycles=auto 1000 80% limit 20000
  will use cycles=1000 for real mode games, 80% CPU throttling for 
  protected mode games along with a hard cycle limit of 20000



====================
10. Troubleshooting:
====================

DOSBox crashes right after starting it:
  - use different values for the output= entry in your DOSBox
    configuration file
  - try to update your graphics card driver and DirectX

Running a certain game closes DOSBox, crashes with some message or hangs:
  - see if it works with a default DOSBox installation
    (unmodified configuration file)
  - try it with sound disabled (use the sound configuration
    program that comes with the game, additionally you can
    set sbtype=none and gus=false in the DOSBox configuration file)
  - change some entries of the DOSBox configuration file, especially try:
      core=normal
      fixed cycles (for example cycles=10000)
      ems=false
      xms=false
    or combinations of the above settings
  - use loadfix before starting the game

The game exits to the DOSBox prompt with some error message:
  - read the error message closely and try to locate the error
  - try the hints at the above sections
  - mount differently as some games are picky about the locations,
    for example if you used "mount d d:\oldgames\game" try
    "mount c d:\oldgames\game" and "mount c d:\oldgames"
  - if the game requires a CD-ROM be sure you used "-t cdrom" when
    mounting and try different additional parameters (the ioctl,
    usecd and label switches, see the appropriate section)
  - check the file permissions of the game files (remove read-only
    attributes, add write permissions etc.)
  - try reinstalling the game within DOSBox



====================
11. The Config File:
====================

A config file can be generated by CONFIG.COM, which can be found on the 
internal DOSBox Z: drive when you start up DOSBox. Look in the internal 
programs section of the readme for usage of CONFIG.COM.
You can edit the generated configfile to customize DOSBox.

The file is divided into several sections (the names have [] around it). 
Some sections have options you can set.
# and % indicate comment-lines. 
The generated configfile contains the current settings. You can alter them and
start DOSBox with the -conf switch to load the file and use these settings.

DOSBox will first parse the settings in ~/.dosboxrc (Linux),
~\dosbox.conf (Win32) or "~/Library/Preferences/DOSBox Preferences"
(MACOSX). Afterwards DOSBox will parse all configfiles specified with the 
-conf switch. If no configfile is specified with the -conf switch, DOSBox will 
look in the current directory for dosbox.conf.



======================
12. The Language File:
======================

A language file can be generated by CONFIG.COM (CONFIG -writelang langfile).
Read it, and you will hopefully understand how to change it. 
Start DOSBox with the -lang switch to use your new language file.
Alternatively, you can setup the filename in the config file in the [dosbox]
section. There's a language= entry that can be changed with the filename.



========================================
13. Building your own version of DOSBox:
========================================

Download the source.
Check the INSTALL in the source distribution.



===================
14. Special thanks:
===================

See the THANKS file.


============
15. Contact:
============

See the site: 
http://dosbox.sourceforge.net
for an email address (The Crew-page).
