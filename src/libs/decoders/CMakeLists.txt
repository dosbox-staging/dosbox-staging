add_library(decoders STATIC
  flac.c
  mp3.cpp
  mp3_seek_table.cpp
  opus.cpp
  SDL_sound.c
  vorbis.c
  wav.c
)

target_include_directories(decoders SYSTEM PUBLIC ..)
target_link_libraries(decoders PRIVATE project_headers)

if (USE_SYSTEM_LIBS)
  pkg_check_modules(OPUSFILE REQUIRED IMPORTED_TARGET opusfile)
  target_link_libraries(decoders PRIVATE PkgConfig::OPUSFILE)
else()
  find_package(OpusFile REQUIRED)
  target_link_libraries(decoders PRIVATE OpusFile::opusfile)
endif()

target_link_libraries(decoders
  PRIVATE
  SDL3::SDL3
)

disable_warnings(decoders)
