libdecoders_sources = files(
    'SDL_sound.c',
    'flac.c',
    'mp3.cpp',
    'mp3_seek_table.cpp',
    'opus.cpp',
    'vorbis.c',
    'wav.c',
)

libdecoders = static_library(
    'decoders',
    libdecoders_sources,
    include_directories: incdir,
    dependencies: [
        sdl2_dep,
        opus_dep,
        libloguru_dep,
    ],
)

libdecoders_dep = declare_dependency(link_with: libdecoders)
