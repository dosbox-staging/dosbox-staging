libcapture_sources = files(
    'capture.cpp',
    'capture_audio.cpp',
    'capture_midi.cpp',
    'capture_video.cpp',
    'image/image_capturer.cpp',
    'image/image_saver.cpp',
    'image/image_scaler.cpp',
    'image/png_writer.cpp',
    'video/zmbv.cpp',
)

zlib_or_ng_dep = system_zlib_ng_dep.found() ? system_zlib_ng_dep : zlib_dep

libcapture = static_library(
    'capture',
    libcapture_sources,
    include_directories: incdir,
    dependencies: [
        libloguru_dep,
        zlib_or_ng_dep,
        png_dep,
        sdl2_dep,
    ],
    cpp_args: warnings,
)

libcapture_dep = declare_dependency(link_with: libcapture)

internal_deps += libcapture_dep
