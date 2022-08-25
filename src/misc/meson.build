libmisc_sources = [
    'ansi_code_markup.cpp',
    'cross.cpp',
    'ethernet.cpp',
    'ethernet_slirp.cpp',
    'fs_utils.cpp',
    'fs_utils_posix.cpp',
    'fs_utils_win32.cpp',
    'help_util.cpp',
    'messages.cpp',
    'pacer.cpp',
    'programs.cpp',
    'rwqueue.cpp',
    'setup.cpp',
    'string_utils.cpp',
    'support.cpp',
    'unicode.cpp',
]

libmisc = static_library(
    'misc',
    libmisc_sources,
    include_directories: incdir,
    dependencies: [
        corefoundation_dep,
        ghc_dep,
        libloguru_dep,
        libslirp_dep,
        libwhereami_dep,
        sdl2_dep,
        stdcppfs_dep,
        winsock2_dep,
    ],
)

libmisc_dep = declare_dependency(link_with: libmisc)

internal_deps += libmisc_dep
