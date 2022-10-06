libmisc_sources = [
    'ansi_code_markup.cpp',
    'cross.cpp',
    'ethernet.cpp',
    'ethernet_slirp.cpp',
    'fs_utils.cpp',
    'fs_utils_posix.cpp',
    'fs_utils_win32.cpp',
    'help_util.cpp',
    'pacer.cpp',
    'programs.cpp',
    'rwqueue.cpp',
    'setup.cpp',
    'string_utils.cpp',
    'support.cpp',
    'unicode.cpp',
]

libmisc_dependencies = [
    corefoundation_dep,
    ghc_dep,
    libloguru_dep,
    libslirp_dep,
    libwhereami_dep,
    sdl2_dep,
    stdcppfs_dep,
    winsock2_dep,
]

libmisc = static_library(
    'misc',
    libmisc_sources + 'messages.cpp',
    include_directories: incdir,
    dependencies: libmisc_dependencies,
)

libmisc_stubs = static_library(
    'misc_stubs',
    libmisc_sources + 'messages_stubs.cpp',
    include_directories: incdir,
    dependencies: libmisc_dependencies,
)

libmisc_dep = declare_dependency(link_with: libmisc)

libmisc_stubs_dep = declare_dependency(link_with: libmisc_stubs)

internal_deps += libmisc_dep
