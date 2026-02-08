libshell_sources = files(
    'autoexec.cpp',
    'command_line.cpp',
    'file_reader.cpp',
    'shell.cpp',
    'shell_batch.cpp',
    'shell_cmds.cpp',
    'shell_history.cpp',
    'shell_misc.cpp',
)

libshell = static_library(
    'shell',
    libshell_sources,
    include_directories: incdir,
    dependencies: [
        sdl2_dep,
        libloguru_dep,
    ],
    cpp_args: warnings,
)

libshell_dep = declare_dependency(link_with: libshell)

libshell_stubs = static_library(
    'shell_stubs',
    ['command_line.cpp'],
    include_directories: incdir,
)

libshell_stubs_dep = declare_dependency(link_with: libshell_stubs)

internal_deps += libshell_dep
