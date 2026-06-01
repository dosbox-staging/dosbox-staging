# GCC 16 complains about disabled optimizations on this file
# We have max-gcse-memory=300000 set globally but this file needs more
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties(
    dos_locale_data.cpp
    TARGET_DIRECTORY dosboxcommon
    PROPERTIES
    COMPILE_OPTIONS "--param;max-gcse-memory=600000"
  )
endif()

target_sources(dosboxcommon PRIVATE
  cdrom.cpp
  cdrom_image.cpp
  dos.cpp
  dos_classes.cpp
  dos_code_page.cpp
  dos_devices.cpp
  dos_execute.cpp
  dos_ioctl.cpp
  dos_keyboard_layout.cpp
  dos_locale.cpp
  dos_locale_data.cpp
  dos_memory.cpp
  dos_misc.cpp
  dos_mscdex.cpp
  dos_programs.cpp
  dos_stacks.cpp
  dos_tables.cpp
  dos_windows.cpp
  drive_cache.cpp
  drive_fat.cpp
  drive_iso.cpp
  drive_local.cpp
  drive_overlay.cpp
  drive_virtual.cpp
  drives.cpp
  programs.cpp

  programs/attrib.cpp
  programs/autotype.cpp
  programs/boot.cpp
  programs/choice.cpp
  programs/clip.cpp
  programs/guide.cpp
  programs/help.cpp
  programs/keyb.cpp
  programs/loadfix.cpp
  programs/loadrom.cpp
  programs/ls.cpp
  programs/makeimg.cpp
  programs/manual.cpp
  programs/mem.cpp
  programs/mixer.cpp
  programs/mode.cpp
  programs/more.cpp
  programs/more_output.cpp
  programs/mount.cpp
  programs/mount_common.cpp
  programs/mouse.cpp
  programs/mousectl.cpp
  programs/move.cpp
  programs/rescan.cpp
  programs/serial.cpp
  programs/setver.cpp
  programs/showpic.cpp
  programs/subst.cpp
  programs/tree.cpp
)

# Windows-only CD-ROM ioctl backend
if (DOSBOX_PLATFORM_WINDOWS)
  target_sources(dosboxcommon PRIVATE cdrom_win32.cpp)
endif()

# Linux-only CD-ROM ioctl backend
if (DOSBOX_PLATFORM_LINUX)
  target_sources(dosboxcommon PRIVATE cdrom_ioctl_linux.cpp)
endif()

# Debugger-only DOS command (see biostest.h)
if (OPT_DEBUGGER)
  target_sources(dosboxcommon PRIVATE programs/biostest.cpp)
endif()

target_sources(dosbox PRIVATE dos_files.cpp)

target_link_libraries(dosboxcommon PRIVATE
  $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
)
