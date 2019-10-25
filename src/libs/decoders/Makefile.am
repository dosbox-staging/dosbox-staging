noinst_LIBRARIES = libdecoders.a

libdecoders_a_SOURCES = \
	SDL_sound.c          \
	SDL_sound.h          \
	SDL_sound_internal.h \
	audio_convert.c      \
	wav.c                \
	dr_wav.h             \
	flac.c               \
	dr_flac.h            \
	opus.c               \
	vorbis.c             \
	stb_vorbis.h         \
	mp3.cpp              \
	mp3_seek_table.cpp   \
	mp3_seek_table.h     \
	dr_mp3.h             \
	archive.h            \
	xxhash.c             \
	xxhash.h

libdecoders_a_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPFLAGS)

libdecoders_a_CXXFLAGS = \
	$(AM_CXXFLAGS) \
	$(CXXFLAGS) \
	-std=c++11 \
	-Wpedantic \
	-Wall

libdecoders_a_CFLAGS = \
	$(AM_CFLAGS) \
	$(CFLAGS) \
	-std=c++11 \
	-Wpedantic \
	-Wall
