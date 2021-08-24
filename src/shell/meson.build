libshell_sources = files([
  'shell.cpp',
  'shell_batch.cpp',
  'shell_cmds.cpp',
  'shell_misc.cpp',
])

libshell = static_library('shell', libshell_sources,
                          include_directories : incdir,
                          dependencies : [
                            sdl2_dep,
                            libloguru_dep,
                          ])

libshell_dep = declare_dependency(link_with : libshell)

internal_deps += libshell_dep
