libcapture_sources = files(
    'capture.cpp',
    'capture_audio.cpp',
    'capture_midi.cpp',
    'capture_video.cpp',
    'image_capturer.cpp',
    'image_decoder.cpp',
    'image_saver.cpp',
    'image_scaler.cpp',
    'png_writer.cpp',
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
