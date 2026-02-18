libwebserver_sources = [
    'bridge.cpp',
    'cpu.cpp',
    'dos.cpp',
    'memory.cpp',
    'webserver.cpp',
]

libwebserver = static_library(
    'webserver',
    libwebserver_sources,
    include_directories: incdir,
    dependencies: [],
    cpp_args: warnings,
)

libwebserver_dep = declare_dependency(link_with: libwebserver)

internal_deps += libwebserver_dep