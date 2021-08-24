libdecoders_sources = files([
  'flac.c',
  'mp3.cpp',
  'mp3_seek_table.cpp',
  'opus.cpp',
  'SDL_sound.c',
  'vorbis.c',
  'wav.c',
])

libdecoders = static_library('decoders', libdecoders_sources,
                             include_directories : incdir,
                             dependencies : [
                               sdl2_dep,
                               opus_dep,
                               libloguru_dep,
                             ])
