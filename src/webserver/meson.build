libwebserver_sources = [
    'bridge.cpp',
    'cpu.cpp',
    'dos.cpp',
    'memory.cpp',
    'video.cpp',
    'webserver.cpp',
]

jpeg_dep = dependency('libjpeg', required: true)

libwebserver = static_library(
    'webserver',
    libwebserver_sources,
    include_directories: incdir,
    dependencies: [png_dep, jpeg_dep],
    cpp_args: warnings,
)

libwebserver_dep = declare_dependency(link_with: libwebserver)

internal_deps += libwebserver_dep