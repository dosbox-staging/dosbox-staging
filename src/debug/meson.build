libdebug_sources = files([
  'debug.cpp',
  'debug_disasm.cpp',
  'debug_gui.cpp',
])

libdebug = static_library('debug', libdebug_sources,
                          include_directories : incdir,
                          dependencies: [
                            sdl2_dep,
                            libghc_dep,
                            libloguru_dep,
                            libpdcurses_dep,
                         ])

libdebug_dep = declare_dependency(link_with : libdebug)

internal_deps += libdebug_dep
