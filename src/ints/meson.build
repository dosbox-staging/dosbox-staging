libints_sources = files([
  'bios.cpp',
  'bios_disk.cpp',
  'bios_keyboard.cpp',
  'ems.cpp',
  'int10_char.cpp',
  'int10.cpp',
  'int10_memory.cpp',
  'int10_misc.cpp',
  'int10_modes.cpp',
  'int10_pal.cpp',
  'int10_put_pixel.cpp',
  'int10_vesa.cpp',
  'int10_video_state.cpp',
  'int10_vptable.cpp',
  'mouse.cpp',
  'mouse_serial.cpp',
  'xms.cpp',
])

libints = static_library('ints', libints_sources,
                         include_directories : incdir,
                         dependencies : [
                           sdl2_dep,
                           libghc_dep,
                           libloguru_dep,
                         ])

libints_dep = declare_dependency(link_with : libints)

internal_deps += libints_dep
