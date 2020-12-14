libdebug_sources = files([
  'debug.cpp',
  'debug_disasm.cpp',
  'debug_gui.cpp',
  'debug_win32.cpp',
])

libdebug = static_library('debug', libdebug_sources,
                          include_directories : incdir)

libdebug_dep = declare_dependency(link_with : libdebug)

internal_deps += libdebug_dep
