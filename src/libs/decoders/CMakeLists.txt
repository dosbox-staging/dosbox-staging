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

find_package(OpusFile CONFIG REQUIRED)
target_link_libraries(libdecoders
		PRIVATE
		OpusFile::opusfile
		$<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
