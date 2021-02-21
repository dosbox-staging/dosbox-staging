libdos_sources = files([
  'cdrom.cpp',
  'cdrom_image.cpp',
  'dos_classes.cpp',
  'dos.cpp',
  'dos_devices.cpp',
  'dos_execute.cpp',
  'dos_files.cpp',
  'dos_ioctl.cpp',
  'dos_keyboard_layout.cpp',
  'dos_memory.cpp',
  'dos_misc.cpp',
  'dos_mscdex.cpp',
  'dos_programs.cpp',
  'dos_tables.cpp',
  'drive_cache.cpp',
  'drive_fat.cpp',
  'drive_iso.cpp',
  'drive_local.cpp',
  'drive_overlay.cpp',
  'drives.cpp',
  'drive_virtual.cpp',
  'program_autotype.cpp',
  'program_ls.cpp',
])

libdos = static_library('dos', libdos_sources,
                        include_directories : incdir,
                        dependencies : [sdl2_dep, libdecoders_dep])

libdos_dep = declare_dependency(link_with : libdos)

internal_deps += libdos_dep
