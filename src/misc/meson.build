libmisc_sources = [
  'cross.cpp',
  'fs_utils_posix.cpp',
  'fs_utils_win32.cpp',
  'messages.cpp',
  'pacer.cpp',
  'programs.cpp',
  'rwqueue.cpp',
  'setup.cpp',
  'soft_limiter.cpp',
  'support.cpp',
]

libmisc = static_library('misc', libmisc_sources,
                         include_directories : incdir,
                         dependencies : [
                           sdl2_dep,
                           libloguru_dep,
                         ])

libmisc_dep = declare_dependency(link_with : libmisc)

internal_deps += libmisc_dep
