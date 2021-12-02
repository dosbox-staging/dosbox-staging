libmisc_sources = [
  'cross.cpp',
  'ethernet.cpp',
  'ethernet_slirp.cpp',
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
                           corefoundation_dep,
                           sdl2_dep,
                           slirp_dep,
                           stdcppfs_dep,
                           libloguru_dep,
                           libwhereami_dep,
                           winsock2_dep,
                         ])

libmisc_dep = declare_dependency(link_with : libmisc)

internal_deps += libmisc_dep
