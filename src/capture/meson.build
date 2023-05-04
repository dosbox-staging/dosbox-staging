libcapture_sources = files(
    'capture.cpp',
    'capture_image.cpp',
    'capture_midi.cpp',
    'capture_video.cpp',
)

libcapture = static_library(
    'capture',
    libcapture_sources,
    include_directories: incdir,
    dependencies: [
        libloguru_dep,
        libzmbv_dep,
        png_dep,
        sdl2_dep,
    ],
)

libcapture_dep = declare_dependency(link_with: libcapture)

internal_deps += libcapture_dep
