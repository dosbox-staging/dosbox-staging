libdebugger_sources = files(
    'debugger.cpp',
    'debugger_disasm.cpp',
    'debugger_gui.cpp',
)

libdebugger = static_library(
    'debugger',
    libdebugger_sources,
    include_directories: incdir,
    dependencies: [
        sdl2_dep,
        libpdcurses_dep,
        libiir_dep,
        libloguru_dep,
    ],
    cpp_args: warnings,
)

libdebugger_dep = declare_dependency(link_with: libdebugger)

internal_deps += libdebugger_dep
