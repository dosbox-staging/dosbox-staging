add_library(libdecoders STATIC
  flac.c
  mp3.cpp
  mp3_seek_table.cpp
  opus.cpp
  SDL_sound.c
  vorbis.c
  wav.c
)

target_include_directories(libdecoders PUBLIC ..)

if(USE_SYSTEM_LIBS)
  pkg_check_modules(OPUSFILE REQUIRED IMPORTED_TARGET opusfile)

  target_include_directories(
    libdecoders PRIVATE ${OPUSFILE_INCLUDE_DIRS}
  )
  target_compile_options(
    libdecoders PRIVATE ${OPUSFILE_CFLAGS_OTHER}
  )
  target_link_libraries(
    libdecoders PRIVATE ${OPUSFILE_LIBRARIES}
  )
else()
  find_package(OpusFile REQUIRED)
  target_link_libraries(
    libdecoders PRIVATE OpusFile::opusfile
  )
endif()

target_link_libraries(libdecoders
  PRIVATE
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
