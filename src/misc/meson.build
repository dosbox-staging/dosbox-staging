# Sources without messages.cpp or messages_stubs.cpp
libmisc_nomsg_sources = [
    'ansi_code_markup.cpp',
    'cross.cpp',
    'ethernet.cpp',
    'ethernet_slirp.cpp',
    'fs_utils.cpp',
    'fs_utils_posix.cpp',
    'fs_utils_win32.cpp',
    'help_util.cpp',
    'host_locale.cpp',
    'host_locale_macos.cpp',
    'host_locale_posix.cpp',
    'host_locale_win32.cpp',
    'pacer.cpp',
    'programs.cpp',
    'rwqueue.cpp',
    'setup.cpp',
    'string_utils.cpp',
    'support.cpp',
    'unicode.cpp',
]

# Full sources
libmisc_sources = libmisc_nomsg_sources + ['messages.cpp']

# Stubbed-out messages version for unit testing.
libmisc_stubs_sources = libmisc_nomsg_sources + ['messages_stubs.cpp']

libmisc_dependencies = [
    corefoundation_dep,
    ghc_dep,
    libiir_dep,
    libloguru_dep,
    libslirp_dep,
    libwhereami_dep,
    sdl2_dep,
    stdcppfs_dep,
    winsock2_dep,
]

libmisc = static_library(
    'misc',
    libmisc_sources,
    include_directories: incdir,
    dependencies: libmisc_dependencies,
    cpp_args: warnings,
)

libmisc_stubs = static_library(
    'misc_stubs',
    libmisc_stubs_sources,
    include_directories: incdir,
    dependencies: libmisc_dependencies,
    cpp_args: warnings,
)

libmisc_dep = declare_dependency(link_with: libmisc)

libmisc_stubs_dep = declare_dependency(link_with: libmisc_stubs)

internal_deps += libmisc_dep
