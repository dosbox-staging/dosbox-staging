# Microsoft Developer Studio Project File - Name="dosbox" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=dosbox - Win32 Debug
!MESSAGE Dies ist kein gültiges Makefile. Zum Erstellen dieses Projekts mit NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und führen Sie den Befehl
!MESSAGE 
!MESSAGE NMAKE /f "dosbox.mak".
!MESSAGE 
!MESSAGE Sie können beim Ausführen von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "dosbox.mak" CFG="dosbox - Win32 Debug"
!MESSAGE 
!MESSAGE Für die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "dosbox - Win32 Release" (basierend auf  "Win32 (x86) Console Application")
!MESSAGE "dosbox - Win32 Debug" (basierend auf  "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "dosbox - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /MD /W3 /GX /O1 /Op /Ob2 /I "../include" /I "../src/platform/visualc" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /FAs /FR /FD /QxMi /bQipo /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 zlib.lib libpng.lib sdlmain.lib sdl.lib curses.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "dosbox - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../include" /I "../src/platform/visualc" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /FD /GZ /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 zlib.lib libpng.lib sdlmain.lib sdl.lib curses.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "dosbox - Win32 Release"
# Name "dosbox - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "cpu"

# PROP Default_Filter ""
# Begin Group "core_16"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\cpu\core_16\helpers.h
# End Source File
# Begin Source File

SOURCE=..\src\cpu\core_16\main.h
# End Source File
# Begin Source File

SOURCE=..\src\cpu\core_16\prefix_66.h
# End Source File
# Begin Source File

SOURCE=..\src\cpu\core_16\prefix_66_of.h
# End Source File
# Begin Source File

SOURCE=..\src\cpu\core_16\prefix_of.h
# End Source File
# Begin Source File

SOURCE=..\src\cpu\core_16\start.h
# End Source File
# Begin Source File

SOURCE=..\src\cpu\core_16\stop.h
# End Source File
# Begin Source File

SOURCE=..\src\cpu\core_16\support.h
# End Source File
# Begin Source File

SOURCE=..\src\cpu\core_16\table_ea.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\src\cpu\callback.cpp
# End Source File
# Begin Source File

SOURCE=..\src\cpu\cpu.cpp
# End Source File
# Begin Source File

SOURCE=..\src\cpu\flags.cpp
# End Source File
# Begin Source File

SOURCE=..\src\cpu\instructions.h
# End Source File
# Begin Source File

SOURCE=..\src\cpu\modrm.cpp
# End Source File
# Begin Source File

SOURCE=..\src\cpu\modrm.h
# End Source File
# Begin Source File

SOURCE=..\src\cpu\slow_16.cpp
# End Source File
# End Group
# Begin Group "debug"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\debug\debug.cpp
# End Source File
# Begin Source File

SOURCE=..\src\debug\debug_disasm.cpp
# End Source File
# Begin Source File

SOURCE=..\src\debug\debug_gui.cpp
# End Source File
# Begin Source File

SOURCE=..\src\debug\debug_inc.h
# End Source File
# Begin Source File

SOURCE=..\src\debug\debug_win32.cpp
# End Source File
# Begin Source File

SOURCE=..\src\debug\disasm_tables.h
# End Source File
# End Group
# Begin Group "dos"

# PROP Default_Filter ""
# Begin Group "devices"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\dos\dev_con.h
# End Source File
# End Group
# Begin Group "drives"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\dos\drive_cache.cpp
# End Source File
# Begin Source File

SOURCE=..\src\dos\drive_local.cpp
# End Source File
# Begin Source File

SOURCE=..\src\dos\drive_virtual.cpp
# End Source File
# Begin Source File

SOURCE=..\src\dos\drives.cpp
# End Source File
# Begin Source File

SOURCE=..\src\dos\drives.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\src\dos\dos.cpp
# End Source File
# Begin Source File

SOURCE=..\src\dos\dos_classes.cpp
# End Source File
# Begin Source File

SOURCE=..\src\dos\dos_devices.cpp
# End Source File
# Begin Source File

SOURCE=..\src\dos\dos_execute.cpp
# End Source File
# Begin Source File

SOURCE=..\src\dos\dos_files.cpp
# End Source File
# Begin Source File

SOURCE=..\src\dos\dos_ioctl.cpp
# End Source File
# Begin Source File

SOURCE=..\src\dos\dos_memory.cpp
# End Source File
# Begin Source File

SOURCE=..\src\dos\dos_misc.cpp
# End Source File
# Begin Source File

SOURCE=..\src\dos\dos_programs.cpp
# End Source File
# Begin Source File

SOURCE=..\src\dos\dos_tables.cpp
# End Source File
# End Group
# Begin Group "gui"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\gui\render.cpp
# End Source File
# Begin Source File

SOURCE=..\include\render.h
# End Source File
# Begin Source File

SOURCE=..\src\gui\render_support.h
# End Source File
# Begin Source File

SOURCE=..\src\gui\sdlmain.cpp
# End Source File
# Begin Source File

SOURCE=..\include\video.h
# End Source File
# End Group
# Begin Group "hardware"

# PROP Default_Filter ""
# Begin Group "vga"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\hardware\vga.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\vga.h
# End Source File
# Begin Source File

SOURCE=..\src\hardware\vga_attr.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\vga_crtc.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\vga_dac.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\vga_draw.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\vga_fonts.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\vga_gfx.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\vga_memory.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\vga_misc.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\vga_seq.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\src\hardware\adlib.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\cmos.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\disney.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\dma.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\gameblaster.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\gus.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\hardware.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\iohandler.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\joystick.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\keyboard.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\memory.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\mixer.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\pcspeaker.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\pic.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\sblaster.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\tandy_sound.cpp
# End Source File
# Begin Source File

SOURCE=..\src\hardware\timer.cpp
# End Source File
# End Group
# Begin Group "ints"

# PROP Default_Filter ""
# Begin Group "int10"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\ints\int10.cpp
# End Source File
# Begin Source File

SOURCE=..\src\ints\int10.h
# End Source File
# Begin Source File

SOURCE=..\src\ints\int10_char.cpp
# End Source File
# Begin Source File

SOURCE=..\src\ints\int10_memory.cpp
# End Source File
# Begin Source File

SOURCE=..\src\ints\int10_misc.cpp
# End Source File
# Begin Source File

SOURCE=..\src\ints\int10_modes.cpp
# End Source File
# Begin Source File

SOURCE=..\src\ints\int10_pal.cpp
# End Source File
# Begin Source File

SOURCE=..\src\ints\int10_put_pixel.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\src\ints\bios.cpp
# End Source File
# Begin Source File

SOURCE=..\src\ints\bios_disk.cpp
# End Source File
# Begin Source File

SOURCE=..\src\ints\bios_keyboard.cpp
# End Source File
# Begin Source File

SOURCE=..\src\ints\ems.cpp
# End Source File
# Begin Source File

SOURCE=..\src\ints\mouse.cpp
# End Source File
# Begin Source File

SOURCE=..\src\ints\xms.cpp
# End Source File
# End Group
# Begin Group "misc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\misc\messages.cpp
# End Source File
# Begin Source File

SOURCE=..\src\misc\programs.cpp
# End Source File
# Begin Source File

SOURCE=..\src\misc\setup.cpp
# End Source File
# Begin Source File

SOURCE=..\src\misc\support.cpp
# End Source File
# End Group
# Begin Group "shell"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\shell\shell.cpp
# End Source File
# Begin Source File

SOURCE=..\src\shell\shell_batch.cpp
# End Source File
# Begin Source File

SOURCE=..\src\shell\shell_cmds.cpp
# End Source File
# Begin Source File

SOURCE=..\src\shell\shell_inc.h
# End Source File
# Begin Source File

SOURCE=..\src\shell\shell_misc.cpp
# End Source File
# End Group
# Begin Group "fpu"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\fpu\fpu.cpp
# End Source File
# Begin Source File

SOURCE=..\src\fpu\fpu_instructions.h
# End Source File
# Begin Source File

SOURCE=..\src\fpu\fpu_types.h
# End Source File
# End Group
# Begin Group "visualc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\platform\visualc\config.h
# End Source File
# Begin Source File

SOURCE=..\src\platform\visualc\dirent.c
# End Source File
# Begin Source File

SOURCE=..\src\platform\visualc\dirent.h
# End Source File
# Begin Source File

SOURCE=..\src\platform\visualc\unistd.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\src\dosbox.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\include\bios.h
# End Source File
# Begin Source File

SOURCE=..\include\callback.h
# End Source File
# Begin Source File

SOURCE=..\include\cpu.h
# End Source File
# Begin Source File

SOURCE=..\include\cross.h
# End Source File
# Begin Source File

SOURCE=..\include\debug.h
# End Source File
# Begin Source File

SOURCE=..\include\dma.h
# End Source File
# Begin Source File

SOURCE=..\include\dos_inc.h
# End Source File
# Begin Source File

SOURCE=..\include\dos_system.h
# End Source File
# Begin Source File

SOURCE=..\include\dosbox.h
# End Source File
# Begin Source File

SOURCE=..\include\fpu.h
# End Source File
# Begin Source File

SOURCE=..\include\hardware.h
# End Source File
# Begin Source File

SOURCE=..\include\inout.h
# End Source File
# Begin Source File

SOURCE=..\include\joystick.h
# End Source File
# Begin Source File

SOURCE=..\include\keyboard.h
# End Source File
# Begin Source File

SOURCE=..\include\mem.h
# End Source File
# Begin Source File

SOURCE=..\include\mixer.h
# End Source File
# Begin Source File

SOURCE=..\include\mouse.h
# End Source File
# Begin Source File

SOURCE=..\include\pic.h
# End Source File
# Begin Source File

SOURCE=..\include\programs.h
# End Source File
# Begin Source File

SOURCE=..\include\regs.h
# End Source File
# Begin Source File

SOURCE=..\settings.h
# End Source File
# Begin Source File

SOURCE=..\include\setup.h
# End Source File
# Begin Source File

SOURCE=..\include\support.h
# End Source File
# Begin Source File

SOURCE=..\include\timer.h
# End Source File
# End Group
# End Target
# End Project
