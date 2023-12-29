libdebug_sources = files(
    'debug.cpp',
    'debug_disasm.cpp',
    'debug_gui.cpp',
    'debug_lua.cpp',
)

libdebug = static_library(
    'debug',
    libdebug_sources,
    include_directories: incdir,
    dependencies: [
        sdl2_dep,
        libpdcurses_dep,
        ghc_dep,
        libiir_dep,
        libloguru_dep,
        lua_dep,
    ],
    cpp_args: warnings,
)

libdebug_dep = declare_dependency(link_with: libdebug)

internal_deps += libdebug_dep
